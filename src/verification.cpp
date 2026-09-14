#include <sovereign/verification.hpp>
#include <algorithm>
#include <cmath>
#include <cfenv>
#include <bit>
#include <cstdint>
namespace sovereign {
namespace {
double sign(const Model& m) { return m.sense==ObjectiveSense::minimize?1:-1; }
void fail(VerificationReport& r,const char* message) { r.violations.emplace_back(message); }
// Grow-expansion addition preserves the exact sum of binary64 inputs. Products
// use an FMA error term. Reject exponent ranges where that error could underflow.
// These predicates assume IEEE round-to-nearest and no fast-math reassociation.
struct Expansion {
    std::vector<double> parts;
    bool valid=true;
    void add(double value) {
        if(!std::isfinite(value)) { valid=false; return; }
        if(value==0) return;
        std::vector<double> next;
        for(double p:parts) {
            const double s=value+p;
            if(!std::isfinite(s)) { valid=false; return; }
            const double b=s-value;
            const double error=(value-(s-b))+(p-b);
            if(!std::isfinite(b)||!std::isfinite(error)) { valid=false; return; }
            if(error!=0) next.push_back(error);
            value=s;
        }
        if(value!=0) next.push_back(value);
        parts=std::move(next);
    }
    void product(double a,double b) {
        if(!std::isfinite(a)||!std::isfinite(b)) { valid=false; return; }
        if(a==0||b==0) return;
        int ea=0,eb=0; std::frexp(a,&ea); std::frexp(b,&eb);
        if(ea+eb < -968) { valid=false; return; }
        const double p=a*b;
        if(!std::isfinite(p)) { valid=false; return; }
        const double error=std::fma(a,b,-p);
        add(error); add(p);
    }
    int direction() const { return parts.empty()?0:(parts.back()>0?1:-1); }
    double estimate() const { double v=0; for(double p:parts) v+=p; return v; }
    double lower() const {
        double v=0;
        for(double p:parts) v=std::nextafter(v+p,-infinity);
        return v;
    }
};
bool setup(const Model& m,const Tolerances& t,VerificationReport& r) {
    if(std::fegetround()!=FE_TONEAREST) { fail(r,"Verification requires round-to-nearest arithmetic"); return false; }
    volatile double denormal=std::numeric_limits<double>::denorm_min();
    const double denormal_sum=denormal+denormal;
    if(std::bit_cast<std::uint64_t>(denormal_sum)!=2) { fail(r,"Verification requires gradual underflow"); return false; }
    SolverOptions o; o.tolerances=t;
    try { validate_options(o); } catch(...) { fail(r,"Invalid tolerances"); return false; }
    if(m.matrix.columns()!=m.variables.size()||m.matrix.rows()!=m.constraints.size()||m.objective.size()!=m.variables.size()) { fail(r,"Invalid model dimensions"); return false; }
    if(!std::isfinite(m.objective_offset)) { fail(r,"Nonfinite objective offset"); return false; }
    for(double c:m.objective) if(!std::isfinite(c)) { fail(r,"Nonfinite objective coefficient"); return false; }
    for(const auto& v:m.variables) if(std::isnan(v.lower)||std::isnan(v.upper)||v.lower==infinity||v.upper==-infinity) { fail(r,"Invalid variable bounds"); return false; }
    for(const auto& v:m.constraints) if(std::isnan(v.lower)||std::isnan(v.upper)||v.lower==infinity||v.upper==-infinity) { fail(r,"Invalid row bounds"); return false; }
    return true;
}
bool finite_vector(std::span<const double> x,Index n,VerificationReport& r) {
    if(x.size()!=n) { fail(r,"Certificate vector dimension mismatch"); return false; }
    for(double v:x) if(!std::isfinite(v)) { fail(r,"Nonfinite certificate vector"); return false; }
    return true;
}
std::vector<double> activity(const Model& m,std::span<const double> x) {
    std::vector<double> a(m.constraints.size(),0);
    for(Index j=0;j<x.size();++j) { auto col=m.matrix.column(j); for(Index k=0;k<col.rows.size();++k) a[col.rows[k]]+=col.values[k]*x[j]; }
    return a;
}
double objective(const Model& m,std::span<const double> x) {
    double result=m.objective_offset; for(Index j=0;j<x.size();++j) result+=m.objective[j]*x[j]; return result;
}
void bounds(double value,double lo,double hi,double tol,VerificationReport& r) {
    if(!std::isfinite(value)) { fail(r,"Nonfinite activity"); r.primal_residual=infinity; return; }
    const double residual=std::max({0.0,lo-value,value-hi}); r.primal_residual=std::max(r.primal_residual,residual);
    if(std::isfinite(lo)&&lo-value>tol*(1+std::abs(lo))) fail(r,"Lower bound violation");
    if(std::isfinite(hi)&&value-hi>tol*(1+std::abs(hi))) fail(r,"Upper bound violation");
}
struct Domain { double lower,upper; };
std::vector<Domain> correction_domains(const Model& m) {
    std::vector<Domain> domains;
    for(const auto& v:m.variables) domains.push_back({v.lower,v.upper});
    std::vector<Index> counts(m.constraints.size(),0), columns(m.constraints.size(),0);
    std::vector<double> coefficients(m.constraints.size(),0);
    for(Index j=0;j<m.variables.size();++j) {
        const auto col=m.matrix.column(j);
        for(Index k=0;k<col.rows.size();++k) {
            const Index i=col.rows[k]; ++counts[i]; columns[i]=j; coefficients[i]=col.values[k];
        }
    }
    for(Index i=0;i<counts.size();++i) {
        if(counts[i]!=1) continue;
        const double coefficient=coefficients[i];
        auto& domain=domains[columns[i]];
        // Binary64 division is rounded to nearest. One adjacent representable
        // value encloses the exact quotient, including overflow and underflow.
        // These bounds only restrict residual corrections, never multipliers.
        const double row_lo=m.constraints[i].lower,row_hi=m.constraints[i].upper;
        if(std::isfinite(row_lo)) {
            const double q=row_lo/coefficient;
            if(coefficient>0) domain.lower=std::max(domain.lower,std::nextafter(q,-infinity));
            else domain.upper=std::min(domain.upper,std::nextafter(q,infinity));
        }
        if(std::isfinite(row_hi)) {
            const double q=row_hi/coefficient;
            if(coefficient>0) domain.upper=std::min(domain.upper,std::nextafter(q,infinity));
            else domain.lower=std::max(domain.lower,std::nextafter(q,-infinity));
        }
    }
    return domains;
}
bool dual(const Model& m,const DualCertificate& d,const Tolerances& t,bool farkas,VerificationReport& r,std::span<const double> x={}) {
    if(!finite_vector(d.row_lower,m.constraints.size(),r)||!finite_vector(d.row_upper,m.constraints.size(),r)||!finite_vector(d.variable_lower,m.variables.size(),r)||!finite_vector(d.variable_upper,m.variables.size(),r)) return false;
    Expansion bound; bound.add(farkas?0:sign(m)*m.objective_offset);
    const auto domains=correction_domains(m);
    const auto a=x.empty()?std::vector<double>{}:activity(m,x);
    auto multiplier=[&](double p,double b,double value,bool lower) {
        if(p<0) fail(r,"Negative dual multiplier");
        if(!std::isfinite(b)) { if(p!=0) fail(r,"Multiplier on infinite bound"); return; }
        bound.product((lower?1:-1)*b,p);
        if(!farkas&&!x.empty()) {
            const double product=std::abs(p*(value-b));
            if(!std::isfinite(product)||product>t.dual*(1+std::abs(p*b))) fail(r,"Complementary slackness violation");
        }
    };
    for(Index i=0;i<m.constraints.size();++i) { multiplier(d.row_lower[i],m.constraints[i].lower,a.empty()?0:a[i],true); multiplier(d.row_upper[i],m.constraints[i].upper,a.empty()?0:a[i],false); }
    r.dual_residual=0;
    for(Index j=0;j<m.variables.size();++j) {
        multiplier(d.variable_lower[j],m.variables[j].lower,x.empty()?0:x[j],true); multiplier(d.variable_upper[j],m.variables[j].upper,x.empty()?0:x[j],false);
        Expansion residual; residual.add(farkas?0:sign(m)*m.objective[j]);
        residual.add(-d.variable_lower[j]); residual.add(d.variable_upper[j]);
        double scale=1+(farkas?0:std::abs(m.objective[j]))+std::abs(d.variable_lower[j])+std::abs(d.variable_upper[j]);
        auto col=m.matrix.column(j);
        for(Index k=0;k<col.rows.size();++k) {
            residual.product(-col.values[k],d.row_lower[col.rows[k]]);
            residual.product(col.values[k],d.row_upper[col.rows[k]]);
            scale+=std::abs(col.values[k]*d.row_lower[col.rows[k]])+std::abs(col.values[k]*d.row_upper[col.rows[k]]);
        }
        const double error=residual.estimate();
        r.dual_residual=std::max(r.dual_residual,std::abs(error));
        if(!residual.valid||!std::isfinite(scale)||std::abs(error)>t.dual*scale) fail(r,"Dual stationarity violation");
        // A nonzero residual is a linear term, even below numerical tolerance.
        // Its infimum over the original variable domain corrects the bound.
        if(residual.direction()!=0) {
            const double endpoint=residual.direction()>0?domains[j].lower:domains[j].upper;
            if(!std::isfinite(endpoint)) r.violations.emplace_back("Stationarity residual has unbounded domain correction at variable " + std::to_string(j));
            else for(double p:residual.parts) bound.product(p,endpoint);
        }
    }
    const double conservative=bound.lower();
    r.dual_bound=sign(m)*conservative;
    if(!bound.valid||!std::isfinite(conservative)) fail(r,"Uncertain or nonfinite dual bound arithmetic");
    if(farkas&&!(conservative>0)) fail(r,"Farkas bound must be strictly positive");
    return true;
}
}
VerificationReport verify_primal(const Model& m,std::span<const double> x,double claimed,const Tolerances& t,bool integer) {
    VerificationReport r;
    if(!setup(m,t,r)||!finite_vector(x,m.variables.size(),r)) return r;
    r.primal_residual=0; const auto a=activity(m,x);
    for(Index j=0;j<x.size();++j) {
        const auto& v=m.variables[j]; bounds(x[j],v.lower,v.upper,t.primal,r);
        if(integer&&v.type!=VariableType::continuous) {
            const double error=std::abs(x[j]-std::round(x[j])); r.integrality_residual=std::max(r.integrality_residual,error);
            if(error>t.integrality) fail(r,"Integrality violation");
            if(v.type==VariableType::binary) bounds(x[j],0,1,t.primal,r);
        }
    }
    for(Index i=0;i<a.size();++i) bounds(a[i],m.constraints[i].lower,m.constraints[i].upper,t.primal,r);
    r.objective=objective(m,x); r.objective_error=std::abs(r.objective-claimed);
    if(!std::isfinite(claimed)||!std::isfinite(r.objective)||r.objective_error>t.primal*(1+std::abs(r.objective))) fail(r,"Objective mismatch");
    r.passed=r.violations.empty(); return r;
}
VerificationReport verify_optimality(const Model& m,std::span<const double> x,double claimed,const DualCertificate& d,const Tolerances& t) {
    auto r=verify_primal(m,x,claimed,t,false); r.passed=false;
    if(!r.violations.empty()) return r;
    if(!dual(m,d,t,false,r,x)) return r;
    r.duality_gap=sign(m)*(r.objective-r.dual_bound);
    if(!std::isfinite(r.duality_gap)||std::abs(r.duality_gap)>t.dual*(1+std::max(std::abs(r.objective),std::abs(r.dual_bound)))) fail(r,"Duality gap violation");
    r.passed=r.violations.empty(); return r;
}
VerificationReport verify_infeasibility(const Model& m,const DualCertificate& d,const Tolerances& t) {
    VerificationReport r; if(!setup(m,t,r)) return r; dual(m,d,t,true,r); r.passed=r.violations.empty(); return r;
}
VerificationReport verify_unboundedness(const Model& m,std::span<const double> x,std::span<const double> ray,const Tolerances& t,bool integer) {
    VerificationReport r; if(!setup(m,t,r)||!finite_vector(x,m.variables.size(),r)) return r;
    r=verify_primal(m,x,objective(m,x),t,integer); r.passed=false;
    if(!r.violations.empty()||!finite_vector(ray,m.variables.size(),r)) return r;
    std::vector<Expansion> a(m.constraints.size()); Expansion improvement;
    for(Index j=0;j<ray.size();++j) {
        if((std::isfinite(m.variables[j].lower)&&ray[j]<0)||(std::isfinite(m.variables[j].upper)&&ray[j]>0)) fail(r,"Variable recession bound violation");
        if(integer&&m.variables[j].type!=VariableType::continuous&&std::abs(ray[j]-std::round(ray[j]))>t.integrality) fail(r,"Nonintegral integer recession direction");
        improvement.product(sign(m)*m.objective[j],ray[j]);
        auto col=m.matrix.column(j);
        for(Index k=0;k<col.rows.size();++k) a[col.rows[k]].product(col.values[k],ray[j]);
    }
    for(Index i=0;i<a.size();++i) {
        if(!a[i].valid) fail(r,"Uncertain recession row arithmetic");
        if((std::isfinite(m.constraints[i].lower)&&a[i].direction()<0)||(std::isfinite(m.constraints[i].upper)&&a[i].direction()>0)) fail(r,"Row recession bound violation");
    }
    if(!improvement.valid||improvement.direction()>=0) fail(r,"Ray does not strictly improve objective");
    r.passed=r.violations.empty(); return r;
}
}
