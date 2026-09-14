#include <sovereign/qp.hpp>
#include <sovereign/validation.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <limits>
#include <stdexcept>

namespace sovereign { namespace {
using Dense=std::vector<std::vector<double>>;
double norm_inf(const std::vector<double>& v){double r=0;for(double x:v)r=std::max(r,std::abs(x));return r;}
double dot(const std::vector<double>& a,const std::vector<double>& b){double r=0;for(Index i=0;i<a.size();++i)r=std::fma(a[i],b[i],r);return r;}
std::vector<double> matvec(const Dense& a,const std::vector<double>& x){std::vector<double> r(a.size());for(Index i=0;i<a.size();++i)for(Index j=0;j<x.size();++j)r[i]=std::fma(a[i][j],x[j],r[i]);return r;}
std::vector<double> solve(Dense a,std::vector<double> b,double tol){
 const Index n=b.size(); for(Index k=0;k<n;++k){Index p=k;for(Index i=k+1;i<n;++i)if(std::abs(a[i][k])>std::abs(a[p][k]))p=i;if(std::abs(a[p][k])<=tol)throw std::runtime_error("singular interior-point KKT system");std::swap(a[p],a[k]);std::swap(b[p],b[k]);for(Index i=k+1;i<n;++i){const double q=a[i][k]/a[k][k];a[i][k]=0;for(Index j=k+1;j<n;++j)a[i][j]-=q*a[k][j];b[i]-=q*b[k];}}
 std::vector<double>x(n);for(Index i=n;i-->0;){double v=b[i];for(Index j=i+1;j<n;++j)v-=a[i][j]*x[j];x[i]=v/a[i][i];}return x;
}
Dense dense(const CscMatrix& a){Dense d(a.rows(),std::vector<double>(a.columns()));for(Index j=0;j<a.columns();++j){auto c=a.column(j);for(Index k=0;k<c.rows.size();++k)d[c.rows[k]][j]=c.values[k];}return d;}
bool psd(Dense q,double tol){ // symmetric pivoted-free LDL test; zero pivots may not couple.
 const Index n=q.size();for(Index i=0;i<n;++i)for(Index j=0;j<n;++j)if(std::abs(q[i][j]-q[j][i])>tol*(1+std::max(std::abs(q[i][j]),std::abs(q[j][i]))))return false;
 for(Index k=0;k<n;++k){if(q[k][k]<-tol)return false;if(std::abs(q[k][k])<=tol){for(Index j=k+1;j<n;++j)if(std::abs(q[j][k])>tol)return false;continue;}for(Index i=k+1;i<n;++i)for(Index j=i;j<n;++j){q[j][i]-=q[i][k]*q[j][k]/q[k][k];q[i][j]=q[j][i];}}return true;
}
struct Canon { Dense e,g; std::vector<double>b,h; };
Canon canonical(const Model&m){Canon c;const auto a=dense(m.matrix);const Index n=m.variables.size();
 auto add=[&](std::vector<double> row,double rhs){c.g.push_back(std::move(row));c.h.push_back(rhs);};
 for(Index i=0;i<m.constraints.size();++i){const auto&r=m.constraints[i];if(std::isfinite(r.lower)&&std::isfinite(r.upper)&&r.lower==r.upper){c.e.push_back(a[i]);c.b.push_back(r.lower);}else{if(std::isfinite(r.upper))add(a[i],r.upper);if(std::isfinite(r.lower)){auto v=a[i];for(auto&x:v)x=-x;add(std::move(v),-r.lower);}}}
 for(Index j=0;j<n;++j){if(std::isfinite(m.variables[j].upper)){std::vector<double>v(n);v[j]=1;add(std::move(v),m.variables[j].upper);}if(std::isfinite(m.variables[j].lower)){std::vector<double>v(n);v[j]=-1;add(std::move(v),-m.variables[j].lower);}}
 return c;
}
double objective(const QuadraticModel&p,const Dense&q,const std::vector<double>&x){auto qx=matvec(q,x);return p.linear.objective_offset+dot(p.linear.objective,x)+.5*dot(x,qx);}
struct Direction{std::vector<double>dx,dy,ds,dz;};
Direction direction(const Dense&q,const Canon&c,const std::vector<double>&s,const std::vector<double>&z,const std::vector<double>&rd,const std::vector<double>&re,const std::vector<double>&ri,const std::vector<double>&rc,double pivot){
 const Index n=q.size(),me=c.e.size(),mi=c.g.size();Dense kkt(n+me,std::vector<double>(n+me));std::vector<double>rhs(n+me);
 for(Index i=0;i<n;++i)for(Index j=0;j<n;++j)kkt[i][j]=q[i][j];
 for(Index k=0;k<mi;++k){const double d=z[k]/s[k];for(Index i=0;i<n;++i)for(Index j=0;j<n;++j)kkt[i][j]+=c.g[k][i]*d*c.g[k][j];}
 for(Index i=0;i<n;++i){rhs[i]=-rd[i];for(Index k=0;k<mi;++k)rhs[i]+=c.g[k][i]*(rc[k]-z[k]*ri[k])/s[k];for(Index k=0;k<me;++k)kkt[i][n+k]=c.e[k][i];}
 for(Index k=0;k<me;++k){rhs[n+k]=-re[k];for(Index j=0;j<n;++j)kkt[n+k][j]=c.e[k][j];}
 auto d=solve(std::move(kkt),std::move(rhs),pivot);Direction out;out.dx.assign(d.begin(),d.begin()+static_cast<std::ptrdiff_t>(n));out.dy.assign(d.begin()+static_cast<std::ptrdiff_t>(n),d.end());out.ds.resize(mi);out.dz.resize(mi);
 for(Index k=0;k<mi;++k){out.ds[k]=-ri[k]-dot(c.g[k],out.dx);out.dz[k]=(-rc[k]-z[k]*out.ds[k])/s[k];}return out;
}
double step(const std::vector<double>&v,const std::vector<double>&d,double fraction){double a=1;for(Index i=0;i<v.size();++i)if(d[i]<0)a=std::min(a,-fraction*v[i]/d[i]);return a;}
DualCertificate original_certificate(const Model&m,const std::vector<double>&y,const std::vector<double>&z){
 DualCertificate d;d.row_lower.assign(m.constraints.size(),0);d.row_upper.assign(m.constraints.size(),0);d.variable_lower.assign(m.variables.size(),0);d.variable_upper.assign(m.variables.size(),0);Index eq=0,ineq=0;
 for(Index i=0;i<m.constraints.size();++i){const auto&r=m.constraints[i];if(std::isfinite(r.lower)&&std::isfinite(r.upper)&&r.lower==r.upper){if(y[eq]>=0)d.row_upper[i]=y[eq];else d.row_lower[i]=-y[eq];++eq;}else{if(std::isfinite(r.upper))d.row_upper[i]=z[ineq++];if(std::isfinite(r.lower))d.row_lower[i]=z[ineq++];}}
 for(Index j=0;j<m.variables.size();++j){if(std::isfinite(m.variables[j].upper))d.variable_upper[j]=z[ineq++];if(std::isfinite(m.variables[j].lower))d.variable_lower[j]=z[ineq++];}return d;
}
}

QpVerificationReport verify_qp_optimality(const QuadraticModel&p,std::span<const double>x,double claimed,const DualCertificate&d,const Tolerances&t){
 QpVerificationReport r;r.objective=claimed;const auto&m=p.linear;const Index n=m.variables.size();if(x.size()!=n||p.quadratic.rows()!=n||p.quadratic.columns()!=n||d.row_lower.size()!=m.constraints.size()||d.row_upper.size()!=m.constraints.size()||d.variable_lower.size()!=n||d.variable_upper.size()!=n){r.violations.push_back("certificate dimensions do not match QP");return r;}
 std::vector<double>vx(x.begin(),x.end());for(double v:vx)if(!std::isfinite(v)){r.violations.push_back("nonfinite primal value");return r;}const auto activity=m.matrix.multiply(vx),qx=p.quadratic.multiply(vx);const double sign=m.sense==ObjectiveSense::minimize?1:-1;double obj=m.objective_offset+.5*dot(vx,qx);for(Index j=0;j<n;++j)obj=std::fma(m.objective[j],vx[j],obj);r.objective= obj;r.primal_residual=0;r.complementarity_residual=0;std::vector<double>station(n);for(Index j=0;j<n;++j)station[j]=sign*(qx[j]+m.objective[j])-d.variable_lower[j]+d.variable_upper[j];
 const auto finite_nonnegative=[&](double v){return std::isfinite(v)&&v>=0;};for(Index i=0;i<m.constraints.size();++i){if(!finite_nonnegative(d.row_lower[i])||!finite_nonnegative(d.row_upper[i])){r.violations.push_back("invalid row multiplier");return r;}if(!std::isfinite(m.constraints[i].lower)&&d.row_lower[i]!=0)r.violations.push_back("multiplier on infinite row lower bound");if(!std::isfinite(m.constraints[i].upper)&&d.row_upper[i]!=0)r.violations.push_back("multiplier on infinite row upper bound");if(std::isfinite(m.constraints[i].lower)){const double slack=activity[i]-m.constraints[i].lower;r.primal_residual=std::max(r.primal_residual,std::max(0.0,-slack));r.complementarity_residual=std::max(r.complementarity_residual,std::abs(d.row_lower[i]*slack));}if(std::isfinite(m.constraints[i].upper)){const double slack=m.constraints[i].upper-activity[i];r.primal_residual=std::max(r.primal_residual,std::max(0.0,-slack));r.complementarity_residual=std::max(r.complementarity_residual,std::abs(d.row_upper[i]*slack));}}
 for(Index j=0;j<n;++j){const auto col=m.matrix.column(j);for(Index k=0;k<col.rows.size();++k)station[j]+=col.values[k]*(d.row_upper[col.rows[k]]-d.row_lower[col.rows[k]]);if(!finite_nonnegative(d.variable_lower[j])||!finite_nonnegative(d.variable_upper[j])){r.violations.push_back("invalid variable multiplier");return r;}if(!std::isfinite(m.variables[j].lower)&&d.variable_lower[j]!=0)r.violations.push_back("multiplier on infinite variable lower bound");if(!std::isfinite(m.variables[j].upper)&&d.variable_upper[j]!=0)r.violations.push_back("multiplier on infinite variable upper bound");if(std::isfinite(m.variables[j].lower)){const double slack=vx[j]-m.variables[j].lower;r.primal_residual=std::max(r.primal_residual,std::max(0.0,-slack));r.complementarity_residual=std::max(r.complementarity_residual,std::abs(d.variable_lower[j]*slack));}if(std::isfinite(m.variables[j].upper)){const double slack=m.variables[j].upper-vx[j];r.primal_residual=std::max(r.primal_residual,std::max(0.0,-slack));r.complementarity_residual=std::max(r.complementarity_residual,std::abs(d.variable_upper[j]*slack));}}
 r.stationarity_residual=norm_inf(station);const double objective_error=std::abs(obj-claimed);const double scale=1+std::abs(obj);if(objective_error>t.primal*scale)r.violations.push_back("claimed objective does not match primal");const double threshold=10*std::max(t.primal,t.dual)*(1+norm_inf(qx)+norm_inf(m.objective));if(r.primal_residual>threshold)r.violations.push_back("primal residual exceeds tolerance");if(r.stationarity_residual>threshold)r.violations.push_back("stationarity residual exceeds tolerance");if(r.complementarity_residual>threshold)r.violations.push_back("complementarity residual exceeds tolerance");r.passed=r.violations.empty();return r;
}

QpResult solve_qp(const QuadraticModel&p,const SolverOptions&o){
 QpResult r;const auto started=std::chrono::steady_clock::now();auto finish=[&]{r.runtime_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();return r;};
 try{validate_options(o);const auto vr=validate(p.linear);if(!vr.ok())throw std::invalid_argument("invalid linear model in QP");const Index n=p.linear.variables.size();if(p.quadratic.rows()!=n||p.quadratic.columns()!=n)throw std::invalid_argument("QP Hessian dimensions do not match variables");const auto original_q=dense(p.quadratic);auto q=original_q;if(p.linear.sense==ObjectiveSense::maximize)for(auto&row:q)for(auto&v:row)v=-v;if(!psd(q,o.tolerances.pivot*100))throw std::invalid_argument("QP Hessian must be symmetric and convex for the requested objective sense");
 auto c=canonical(p.linear);std::vector<double>cost=p.linear.objective;if(p.linear.sense==ObjectiveSense::maximize)for(auto&v:cost)v=-v;
 std::vector<double>x(n),y(c.e.size()),z(c.g.size(),1),s(c.g.size());auto gx=matvec(c.g,x);for(Index i=0;i<s.size();++i)s[i]=std::max(1.0,c.h[i]-gx[i]);
 for(Index it=0;it<o.iteration_limit;++it){r.iterations=it;auto qx=matvec(q,x);std::vector<double>rd(n);for(Index j=0;j<n;++j){rd[j]=qx[j]+cost[j];for(Index k=0;k<c.e.size();++k)rd[j]+=c.e[k][j]*y[k];for(Index k=0;k<c.g.size();++k)rd[j]+=c.g[k][j]*z[k];}auto ex=matvec(c.e,x);std::vector<double>re(c.e.size());for(Index i=0;i<re.size();++i)re[i]=ex[i]-c.b[i];gx=matvec(c.g,x);std::vector<double>ri(c.g.size());for(Index i=0;i<ri.size();++i)ri[i]=gx[i]+s[i]-c.h[i];const double mu=s.empty()?0:dot(s,z)/s.size();
  const double residual=std::max({norm_inf(rd),norm_inf(re),norm_inf(ri),mu});if(residual<=std::max(o.tolerances.primal,o.tolerances.dual)){r.status=SolveStatus::optimal;break;}if(o.cancelled&&o.cancelled()){r.status=SolveStatus::cancelled;break;}if(std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()>=o.time_limit_seconds){r.status=SolveStatus::time_limit;break;}
  std::vector<double>rc(s.size());for(Index i=0;i<s.size();++i)rc[i]=s[i]*z[i];auto affine=direction(q,c,s,z,rd,re,ri,rc,o.tolerances.pivot);const double aa=step(s,affine.ds,1),ad=step(z,affine.dz,1);double mua=0;for(Index i=0;i<s.size();++i)mua+=(s[i]+aa*affine.ds[i])*(z[i]+ad*affine.dz[i]);if(!s.empty())mua/=s.size();const double sigma=mu>0?std::pow(mua/mu,3):0;for(Index i=0;i<rc.size();++i)rc[i]=s[i]*z[i]+affine.ds[i]*affine.dz[i]-sigma*mu;auto d=direction(q,c,s,z,rd,re,ri,rc,o.tolerances.pivot);const double ap=step(s,d.ds,.995),az=step(z,d.dz,.995);for(Index i=0;i<n;++i)x[i]+=ap*d.dx[i];for(Index i=0;i<y.size();++i)y[i]+=az*d.dy[i];for(Index i=0;i<s.size();++i){s[i]+=ap*d.ds[i];z[i]+=az*d.dz[i];}
  if(o.telemetry)o.telemetry({"INTERIOR_POINT_ITERATION",std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count(),it+1,0,infinity,-infinity,infinity,"mu="+std::to_string(mu)+" residual="+std::to_string(residual)});
 }
 if(r.status!=SolveStatus::optimal){if(r.status==SolveStatus::numerical_failure&&r.iterations+1>=o.iteration_limit)r.status=SolveStatus::iteration_limit;r.message="Interior-point method did not produce a verified optimum";return finish();}
 r.primal=x;r.objective=objective(p,original_q,x);r.certificate=original_certificate(p.linear,y,z);r.verification=verify_qp_optimality(p,r.primal,r.objective,r.certificate,o.tolerances);if(!r.verification.passed){r.status=SolveStatus::numerical_failure;r.message="Candidate failed independent original-coordinate KKT verification";}else r.message="Convex QP optimum independently verified by original-coordinate primal and KKT residuals";return finish();
 }catch(const std::exception&e){r.status=SolveStatus::numerical_failure;r.message=e.what();return finish();}
}
QpResult solve_interior_point(const Model&m,const SolverOptions&o){QuadraticModel p{m,CscMatrix::from_triplets(m.variables.size(),m.variables.size(),{})};return solve_qp(p,o);}
}
