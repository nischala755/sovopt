#include <sovereign/mip.hpp>
#include <sovereign/cuts.hpp>
#include <sovereign/lp.hpp>
#include <sovereign/basis.hpp>
#include <sovereign/verification.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <queue>
#include <set>
#include <sstream>
namespace sovereign {
namespace {
struct Node {
    Model model;
    double bound=-infinity;
    Index id=0, branch=0;
    double parent_bound=-infinity, distance=0;
    bool up=false;
    LpWarmStart warm;
};
struct Later { bool operator()(const Node& a,const Node& b) const { return a.bound!=b.bound?a.bound>b.bound:a.id>b.id; } };
double objective(const Model& m,const std::vector<double>& x) {
    double value=m.objective_offset; for(Index j=0;j<x.size();++j) value+=m.objective[j]*x[j]; return value;
}
double lattice_bound(const Model& m,double reported) {
    // If every variable is discrete and every objective coefficient is an exactly
    // represented integer, objective-offset values lie on the integer lattice.
    for(Index j=0;j<m.variables.size();++j)
        if(m.variables[j].type==VariableType::continuous || std::abs(m.objective[j])>9007199254740992.0 || m.objective[j]!=std::trunc(m.objective[j]))
            return (m.sense==ObjectiveSense::minimize?1.0:-1.0)*reported;
    const double sign=m.sense==ObjectiveSense::minimize?1:-1;
    const double shifted=std::nextafter(sign*reported-sign*m.objective_offset,-infinity);
    return sign*m.objective_offset+std::ceil(shifted);
}
std::string unused_name(const std::set<std::string>& used,const std::string& stem) {
    if(!used.contains(stem)) return stem;
    for(Index suffix=1;;++suffix) {
        const auto candidate=stem+"_"+std::to_string(suffix);
        if(!used.contains(candidate)) return candidate;
    }
}
Model distance_projection(const Model& source,std::span<const double> target,
                          std::span<const Index> discrete) {
    Model projected=source;
    projected.name=source.name+" feasibility-pump projection";
    projected.sense=ObjectiveSense::minimize;
    projected.objective_offset=0;
    projected.objective.assign(source.variables.size()+discrete.size(),0.0);
    std::set<std::string> variable_names,row_names;
    for(const auto& variable:source.variables) variable_names.insert(variable.name);
    for(const auto& row:source.constraints) row_names.insert(row.name);
    std::vector<Triplet> entries;
    entries.reserve(source.matrix.nonzeros()+4*discrete.size());
    for(Index j=0;j<source.matrix.columns();++j) {
        const auto column=source.matrix.column(j);
        for(Index k=0;k<column.rows.size();++k) entries.push_back({column.rows[k],j,column.values[k]});
    }
    for(Index k=0;k<discrete.size();++k) {
        const Index j=discrete[k],d=source.variables.size()+k;
        const auto variable_name=unused_name(variable_names,"__fp_distance_"+std::to_string(j));
        variable_names.insert(variable_name);
        projected.variables.push_back({variable_name,0,infinity,VariableType::continuous});
        projected.objective[d]=1;
        const Index positive=projected.constraints.size();
        const auto positive_name=unused_name(row_names,"__fp_positive_"+std::to_string(j));
        row_names.insert(positive_name);
        projected.constraints.push_back({positive_name,-infinity,target[j]});
        entries.push_back({positive,j,1}); entries.push_back({positive,d,-1});
        const Index negative=projected.constraints.size();
        const auto negative_name=unused_name(row_names,"__fp_negative_"+std::to_string(j));
        row_names.insert(negative_name);
        projected.constraints.push_back({negative_name,-infinity,-target[j]});
        entries.push_back({negative,j,-1}); entries.push_back({negative,d,-1});
    }
    projected.matrix=CscMatrix::from_triplets(projected.constraints.size(),projected.variables.size(),std::move(entries));
    return projected;
}
}
SolveResult solve_mip(const Model& original,const SolverOptions& options) {
    validate_options(options);
    const auto start=std::chrono::steady_clock::now(); const auto cpu=std::clock();
    const auto elapsed=[&] { return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(); };
    SolveResult r; const double sign=original.sense==ObjectiveSense::minimize?1:-1;
    double incumbent=infinity, current=-infinity; bool active=false, have_incumbent=false;
    std::priority_queue<Node,std::vector<Node>,Later> open;
    auto update_bound=[&] {
        double bound=active?current:infinity;
        if(!open.empty()) bound=std::min(bound,open.top().bound);
        if(have_incumbent) bound=std::min(bound,incumbent);
        r.best_bound=sign*bound;
        r.mip_gap=have_incumbent&&std::isfinite(bound)?std::max(0.0,incumbent-bound)/std::max(1.0,std::abs(incumbent)):infinity;
    };
    auto emit=[&](const std::string& type,const std::string& detail="") {
        update_bound(); if(options.telemetry) options.telemetry({type,elapsed(),r.iterations,r.nodes,r.objective,r.best_bound,r.mip_gap,detail});
    };
    auto finish=[&] { update_bound(); r.runtime_seconds=elapsed(); r.cpu_seconds=double(std::clock()-cpu)/CLOCKS_PER_SEC; emit("MIP_COMPLETED",r.message); return r; };
    auto limit=[&] {
        if(options.cancelled&&options.cancelled()) return SolveStatus::cancelled;
        if(elapsed()>=options.time_limit_seconds) return SolveStatus::time_limit;
        if(r.iterations>=options.iteration_limit) return SolveStatus::iteration_limit;
        return SolveStatus::optimal;
    };
    auto lp=[&](const Model& m,const LpWarmStart* input=nullptr,LpWarmStart* output=nullptr,Index cap=std::numeric_limits<Index>::max()) {
        SolverOptions o=options; o.iteration_limit=std::min(options.iteration_limit-r.iterations,cap);
        o.time_limit_seconds=std::max(0.0,options.time_limit_seconds-elapsed()); o.telemetry={};o.presolve=false;
        auto result=solve_lp(m,o,input,output); r.iterations+=result.iterations; return result;
    };
    auto accept=[&](std::vector<double> x) {
        if(x.size()!=original.variables.size()) return false;
        for(Index j=0;j<x.size();++j) if(original.variables[j].type!=VariableType::continuous) x[j]=std::round(x[j]);
        const double value=objective(original,x); auto v=verify_primal(original,x,value,options.tolerances,true);
        if(!v.passed) return false;
        if(!have_incumbent||sign*value<incumbent) { have_incumbent=true; incumbent=sign*value; r.primal=std::move(x); r.objective=value; r.verification=std::move(v); ++r.incumbent_updates; emit("INCUMBENT_UPDATED"); }
        return true;
    };
    auto feasibility_pump=[&](const Model& node_model,const std::vector<double>& relaxation) {
        std::vector<Index> discrete;
        for(Index j=0;j<original.variables.size();++j)
            if(original.variables[j].type!=VariableType::continuous) discrete.push_back(j);
        if(discrete.empty()) return false;
        emit("FEASIBILITY_PUMP_STARTED","passes="+std::to_string(options.feasibility_pump_passes));
        std::vector<double> current=relaxation,target=relaxation;
        std::set<std::string> seen;
        for(Index pass=0;pass<options.feasibility_pump_passes&&limit()==SolveStatus::optimal;++pass) {
            std::ostringstream signature;
            for(const Index j:discrete) {
                target[j]=std::clamp(std::round(current[j]),std::ceil(node_model.variables[j].lower),std::floor(node_model.variables[j].upper));
                signature<<target[j]<<',';
            }
            if(!seen.insert(signature.str()).second) {
                const Index j=discrete[pass%discrete.size()];
                double alternative=target[j]>=current[j]?std::floor(current[j]):std::ceil(current[j]);
                if(alternative==target[j]) alternative=target[j]+(target[j]<node_model.variables[j].upper?1:-1);
                target[j]=std::clamp(alternative,std::ceil(node_model.variables[j].lower),std::floor(node_model.variables[j].upper));
                emit("FEASIBILITY_PUMP_PERTURBED","variable="+std::to_string(j));
            }
            auto projection=lp(distance_projection(node_model,target,discrete));
            emit("FEASIBILITY_PUMP_PASS","pass="+std::to_string(pass+1));
            if(projection.status!=SolveStatus::optimal||!projection.verification.passed) break;
            current.assign(projection.primal.begin(),projection.primal.begin()+static_cast<std::ptrdiff_t>(original.variables.size()));
            bool integral=true;
            for(const Index j:discrete)
                if(std::abs(current[j]-std::round(current[j]))>options.tolerances.integrality) integral=false;
            if(integral&&accept(current)) { emit("FEASIBILITY_PUMP_INCUMBENT","pass="+std::to_string(pass+1)); return true; }
        }
        emit("FEASIBILITY_PUMP_STOPPED","no verified candidate");
        return false;
    };
    auto diving=[&](const Model& node_model,const std::vector<double>& relaxation) {
        std::vector<Index> discrete;for(Index j=0;j<original.variables.size();++j)if(original.variables[j].type!=VariableType::continuous)discrete.push_back(j);
        if(discrete.empty()||discrete.size()>32)return false;
        emit("DIVING_STARTED","integer_variables="+std::to_string(discrete.size()));
        Model restricted=node_model;auto point=relaxation;std::vector<bool>fixed(original.variables.size(),false);
        for(Index pass=0;pass<discrete.size()&&limit()==SolveStatus::optimal;++pass){
            Index chosen=original.variables.size();double nearest=infinity;
            for(Index j:discrete)if(!fixed[j]){const double distance=std::abs(point[j]-std::round(point[j]));if(distance<nearest){nearest=distance;chosen=j;}}
            if(chosen==original.variables.size())break;
            const auto bounds=restricted.variables[chosen];const double preferred=std::clamp(std::round(point[chosen]),std::ceil(bounds.lower),std::floor(bounds.upper));
            const double alternative=preferred<=point[chosen]?std::ceil(point[chosen]):std::floor(point[chosen]);
            bool succeeded=false;
            for(Index attempt=0;attempt<2;++attempt){
                const double value=attempt==0?preferred:alternative;
                if(attempt==1&&value==preferred)continue;
                if(value<bounds.lower||value>bounds.upper||!std::isfinite(value))continue;
                restricted.variables[chosen].lower=value;restricted.variables[chosen].upper=value;
                const auto solved=lp(restricted,nullptr,nullptr,2000);emit("DIVING_PROBE","variable="+std::to_string(chosen)+" pass="+std::to_string(pass+1));
                if(solved.status==SolveStatus::optimal&&solved.verification.passed){point=solved.primal;succeeded=true;break;}
                if(solved.status!=SolveStatus::infeasible)break;
            }
            if(!succeeded){emit("DIVING_STOPPED","fixing failed");return false;}
            fixed[chosen]=true;
        }
        if(accept(point)){emit("DIVING_INCUMBENT");return true;}
        emit("DIVING_STOPPED","candidate failed original-model verification");return false;
    };
    Model root=original;
    for(auto& v:root.variables) if(v.type==VariableType::binary) { v.lower=std::max(0.0,v.lower); v.upper=std::min(1.0,v.upper); }
    CutStatistics cut_stats;
    if(options.cuts) { cut_stats=apply_safe_root_cuts(root);r.cuts_added=cut_stats.integer_rounding+cut_stats.chvatal_gomory+cut_stats.cover+cut_stats.clique; }
    open.push({std::move(root)}); r.nodes_generated=1; emit("MIP_STARTED"); emit("NODE_CREATED","0");
    if(r.cuts_added) emit("CUT_GENERATED","rounding="+std::to_string(cut_stats.integer_rounding)+" cg="+std::to_string(cut_stats.chvatal_gomory)+" cover="+std::to_string(cut_stats.cover)+" clique="+std::to_string(cut_stats.clique));
    const Index n=original.variables.size(); std::vector<double> upsum(n),downsum(n); std::vector<Index> upcount(n),downcount(n);
    while(!open.empty()) {
        r.status=limit(); if(r.status!=SolveStatus::optimal) return finish();
        if(r.nodes>=options.node_limit) { r.status=SolveStatus::node_limit; return finish(); }
        Node node=open.top(); open.pop(); active=true; current=node.bound; ++r.nodes;
        if(have_incumbent&&current>=incumbent) { emit("NODE_PRUNED","bound"); active=false; continue; }
        bool contradiction=false;
        for(const auto& v:node.model.variables) if(v.lower>v.upper || (v.type!=VariableType::continuous&&std::ceil(v.lower)>std::floor(v.upper))) contradiction=true;
        for(const auto& row:node.model.constraints) if(row.lower>row.upper) contradiction=true;
        if(contradiction) { emit("NODE_PRUNED","inconsistent bounds"); active=false; continue; }
        LpWarmStart solved_basis;const LpWarmStart* inherited=node.warm.basis.empty()?nullptr:&node.warm;
        auto relaxation=lp(node.model,inherited,&solved_basis);
        if(relaxation.status==SolveStatus::infeasible&&relaxation.verification.passed) { emit("NODE_PRUNED","verified LP infeasibility"); active=false; continue; }
        if(relaxation.status==SolveStatus::unbounded) {
            auto x=relaxation.primal, ray=relaxation.ray;
            if(x.size()==n&&ray.size()==n) for(Index j=0;j<n;++j) if(original.variables[j].type!=VariableType::continuous) { x[j]=std::round(x[j]); ray[j]=std::round(ray[j]); }
            auto v=verify_unboundedness(original,x,ray,options.tolerances,true);
            if(v.passed) { r.status=SolveStatus::unbounded; r.primal=std::move(x); r.ray=std::move(ray); r.objective=objective(original,r.primal); r.verification=v; current=-infinity; r.message="Integer-feasible base and integral improving recession ray independently verified"; }
            else { r.status=SolveStatus::numerical_failure; r.message="LP unboundedness does not establish integer unboundedness; no verified integer base and ray"; }
            return finish();
        }
        if(relaxation.status!=SolveStatus::optimal||!relaxation.verification.passed) { r.status=relaxation.status==SolveStatus::optimal?SolveStatus::numerical_failure:relaxation.status; r.message=relaxation.message; return finish(); }
        if(options.cuts&&node.id==0) {
            const auto separated=apply_safe_root_cuts(node.model,relaxation.primal);
            if(separated.chvatal_gomory) {
                r.cuts_added+=separated.chvatal_gomory;
                emit("CUT_GENERATED","violated cg="+std::to_string(separated.chvatal_gomory));
                solved_basis={};
                relaxation=lp(node.model,nullptr,&solved_basis);
                if(relaxation.status==SolveStatus::infeasible&&relaxation.verification.passed) {
                    emit("NODE_PRUNED","verified LP infeasibility after root cuts"); active=false; continue;
                }
                if(relaxation.status!=SolveStatus::optimal||!relaxation.verification.passed) {
                    r.status=relaxation.status==SolveStatus::optimal?SolveStatus::numerical_failure:relaxation.status;
                    r.message=relaxation.message; return finish();
                }
            }
            SolverOptions tableau_options=options;tableau_options.presolve=false;Index gmi=0;
            try { const auto tableau=extract_lp_tableau(node.model,tableau_options,solved_basis);gmi=apply_tableau_gmi_cuts(node.model,tableau,relaxation.primal,1e-7,1); }
            catch(const NumericalError& error) { emit("CUT_SKIPPED",std::string("tableau unavailable: ")+error.what()); }
            if(gmi) {
                r.cuts_added+=gmi;emit("CUT_GENERATED","tableau gmi="+std::to_string(gmi));
                solved_basis={};relaxation=lp(node.model,nullptr,&solved_basis);
                if(relaxation.status==SolveStatus::infeasible&&relaxation.verification.passed) {
                    emit("NODE_PRUNED","verified LP infeasibility after GMI cuts");active=false;continue;
                }
                if(relaxation.status!=SolveStatus::optimal||!relaxation.verification.passed) {
                    r.status=relaxation.status==SolveStatus::optimal?SolveStatus::numerical_failure:relaxation.status;
                    r.message=relaxation.message;return finish();
                }
            }
        }
        current=std::max(current,lattice_bound(original,relaxation.verification.dual_bound));
        if(node.distance>0&&std::isfinite(node.parent_bound)) {
            const double gain=std::max(0.0,current-node.parent_bound)/node.distance;
            if(node.up) { upsum[node.branch]+=gain; ++upcount[node.branch]; } else { downsum[node.branch]+=gain; ++downcount[node.branch]; }
        }
        emit("NODE_SOLVED",std::to_string(node.id));
        if(have_incumbent&&current>=incumbent) { emit("NODE_PRUNED","bound"); active=false; continue; }
        bool near=true;
        for(Index j=0;j<n;++j) if(original.variables[j].type!=VariableType::continuous&&std::abs(relaxation.primal[j]-std::round(relaxation.primal[j]))>options.tolerances.integrality) near=false;
        const bool rounded_ok=near&&accept(relaxation.primal);
        if(options.rounding&&!rounded_ok&&limit()==SolveStatus::optimal) {
            Model fixed=node.model;
            for(Index j=0;j<n;++j) if(fixed.variables[j].type!=VariableType::continuous) {
                const double value=std::clamp(std::round(relaxation.primal[j]),std::ceil(fixed.variables[j].lower),std::floor(fixed.variables[j].upper));
                fixed.variables[j].lower=value; fixed.variables[j].upper=value;
            }
            auto repaired=lp(fixed); if(repaired.status==SolveStatus::optimal) accept(repaired.primal);
        }
        if(options.rounding&&node.id==0&&!have_incumbent&&limit()==SolveStatus::optimal)(void)diving(node.model,relaxation.primal);
        if(options.feasibility_pump&&node.id==0&&!have_incumbent&&limit()==SolveStatus::optimal)
            (void)feasibility_pump(node.model,relaxation.primal);
        update_bound();
        if(have_incumbent&&r.mip_gap<=options.mip_gap) { r.status=SolveStatus::optimal; r.message="Verified incumbent satisfies requested global MIP gap"; return finish(); }
        Index branch=n; double score=-1;
        std::vector<std::pair<double,Index>> fractional;
        for(Index j=0;j<n;++j) if(original.variables[j].type!=VariableType::continuous) {
            const double value=relaxation.primal[j], f=value-std::floor(value); if(!(f>0&&f<1)) continue;
            double s=std::min(f,1-f);
            if(options.branching=="pseudocost") { const double d=f*(downcount[j]?downsum[j]/double(downcount[j]):1),u=(1-f)*(upcount[j]?upsum[j]/double(upcount[j]):1); s=std::min(d,u)+0.1*std::max(d,u); }
            fractional.push_back({s,j});
            if(s>score) { score=s; branch=j; }
        }
        if(options.branching=="strong"&&!fractional.empty()) {
            std::sort(fractional.begin(),fractional.end(),[](const auto&a,const auto&b){return a.first!=b.first?a.first>b.first:a.second<b.second;});
            branch=n;score=-1;const Index probes=std::min(options.strong_branching_candidates,fractional.size());
            for(Index candidate=0;candidate<probes&&limit()==SolveStatus::optimal;++candidate) {
                const Index j=fractional[candidate].second;const double value=relaxation.primal[j];double gains[2]={0,0};bool valid=true;
                for(Index direction=0;direction<2;++direction) {
                    Model probe=node.model;auto&v=probe.variables[j];if(direction)v.lower=std::max(v.lower,std::ceil(value));else v.upper=std::min(v.upper,std::floor(value));
                    if(v.lower>v.upper || (v.type!=VariableType::continuous&&std::ceil(v.lower)>std::floor(v.upper))){
                        gains[direction]=1e12;emit("STRONG_BRANCH_PROBE",std::to_string(j)+(direction?" up":" down")+" proved empty by bounds");continue;
                    }
                    const auto tested=lp(probe,&solved_basis,nullptr);if(tested.status==SolveStatus::infeasible&&tested.verification.passed)gains[direction]=1e12;
                    else if(tested.status==SolveStatus::optimal&&tested.verification.passed)gains[direction]=std::max(0.0,sign*tested.verification.dual_bound-current);
                    else {valid=false;break;}
                    emit("STRONG_BRANCH_PROBE",std::to_string(j)+(direction?" up":" down")+" gain="+std::to_string(gains[direction]));
                }
                if(valid){const double s=std::min(gains[0],gains[1])+.1*std::max(gains[0],gains[1]);if(s>score){score=s;branch=j;}}
            }
            if(branch==n)branch=fractional.front().second;
        }
        if(branch==n) {
            if(rounded_ok) { active=false; emit("NODE_PRUNED","integer LP optimum"); continue; }
            r.status=SolveStatus::numerical_failure; r.message="Integral LP point failed independent original-model verification"; return finish();
        }
        const double value=relaxation.primal[branch], f=value-std::floor(value);
        for(bool up:{false,true}) {
            Node child{node.model,current,r.nodes_generated++,branch,current,up?1-f:f,up};
            child.warm=solved_basis;
            auto& v=child.model.variables[branch]; if(up) v.lower=std::max(v.lower,std::ceil(value)); else v.upper=std::min(v.upper,std::floor(value));
            open.push(std::move(child)); emit("NODE_CREATED",std::to_string(r.nodes_generated-1));
        }
        active=false;
    }
    r.status=have_incumbent?SolveStatus::optimal:SolveStatus::infeasible;
    r.message=have_incumbent?"Branch-and-bound exhausted; incumbent verified against original model":"All nodes proved infeasible";
    return finish();
}
}
