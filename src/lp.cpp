#include <sovereign/lp.hpp>
#include <sovereign/basis.hpp>
#include <sovereign/presolve.hpp>
#include <sovereign/verification.hpp>
#include <sovereign/validation.hpp>
#include <sovereign/errors.hpp>
#include "lp_standard.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <deque>
#include <numeric>
#include <set>
#include <sstream>

namespace sovereign {
namespace {
using Clock=std::chrono::steady_clock;
struct Run {
    const SolverOptions& options;
    Clock::time_point started=Clock::now();
    Index iterations=0;
    double elapsed() const { return std::chrono::duration<double>(Clock::now()-started).count(); }
    void emit(std::string type,std::string detail={}) const {
        if (options.telemetry) options.telemetry({std::move(type),elapsed(),iterations,0,infinity,-infinity,infinity,std::move(detail)});
    }
    SolveStatus limit() const {
        if (options.cancelled && options.cancelled()) return SolveStatus::cancelled;
        if (elapsed()>=options.time_limit_seconds) return SolveStatus::time_limit;
        if (iterations>=options.iteration_limit) return SolveStatus::iteration_limit;
        return SolveStatus::optimal;
    }
};
std::vector<double> dense_column(const CscMatrix& a,Index j) {
    std::vector<double> result(a.rows(),0); const auto c=a.column(j);
    for (Index k=0;k<c.rows.size();++k) result[c.rows[k]]=c.values[k];
    return result;
}
double dot_column(const CscMatrix& a,Index j,std::span<const double> x) {
    const auto c=a.column(j); double result=0;
    for (Index k=0;k<c.rows.size();++k) result=std::fma(c.values[k],x[c.rows[k]],result);
    if (!std::isfinite(result)) throw NumericalError("nonfinite reduced cost product");
    return result;
}
struct SimplexState { SolveStatus status; std::vector<double> primal,dual,ray; double objective=0; };
std::string basis_signature(std::span<const Index> columns) {
    std::ostringstream out;
    for (const auto column:columns) out<<column<<',';
    return out.str();
}
SimplexState simplex(detail::StandardForm& f,std::span<const double> cost,bool phase_one,Run& run) {
    const auto& tol=run.options.tolerances;
    double global_cost_scale=phase_one?1.0:0.0;
    for(const double coefficient:cost)global_cost_scale=std::max(global_cost_scale,std::abs(coefficient));
    std::deque<std::string> basis_history;
    std::set<std::string> known_bases;
    bool anti_cycling=false;
    UpdatedBasis basis(f.matrix,f.basis,tol.pivot,32);
    run.emit("FACTORIZATION","basis dimension="+std::to_string(f.basis.size()));
    while (true) {
        const auto limit=run.limit(); if (limit!=SolveStatus::optimal) return {limit,{},{},{},0};
        const auto signature=basis_signature(f.basis);
        if (!known_bases.insert(signature).second && !anti_cycling) {
            anti_cycling=true;
            run.emit("ANTI_CYCLING_ACTIVATED","repeated basis; objective-scaled pricing tolerance active");
        }
        basis_history.push_back(signature);
        if (basis_history.size()>256) { known_bases.erase(basis_history.front()); basis_history.pop_front(); }
        auto xb=basis.solve(f.rhs);
        std::vector<double> cb; for (auto j : f.basis) cb.push_back(cost[j]);
        auto dual=basis.solve_transpose(cb);
        std::vector<bool> basic(f.matrix.columns(),false); for (auto j : f.basis) basic[j]=true;
        std::vector<double> primal(f.matrix.columns(),0);
        for (Index i=0;i<xb.size();++i) {
            if (xb[i]<-tol.primal*(1+std::abs(f.rhs[i]))) throw NumericalError("basis lost primal feasibility");
            if (xb[i]<0) xb[i]=0;
            primal[f.basis[i]]=xb[i];
        }
        const auto activity=f.matrix.multiply(primal);
        for (Index i=0;i<activity.size();++i)
            if (!std::isfinite(activity[i]) || std::abs(activity[i]-f.rhs[i])>tol.primal*(1+std::abs(f.rhs[i]))) throw NumericalError("basis solve residual exceeds tolerance");
        Index entering=f.matrix.columns();
        for (Index j=0;j<f.matrix.columns();++j) if (!basic[j] && (phase_one || !f.artificial[j])) {
            const double product=dot_column(f.matrix,j,dual); const double reduced=cost[j]-product;
            if (!std::isfinite(reduced)) throw NumericalError("nonfinite reduced cost");
            // Scale to the objective terms, not to an arbitrary unit objective.
            const double objective_scale=std::max((phase_one||anti_cycling)?global_cost_scale:0.0,
                std::max(std::abs(cost[j])+std::abs(product),std::numeric_limits<double>::min()));
            if (reduced < -tol.dual*objective_scale) { entering=j; break; }
        }
        if (entering==f.matrix.columns()) {
            double objective=0; for (Index j=0;j<cost.size();++j) objective=std::fma(cost[j],primal[j],objective);
            return {SolveStatus::optimal,std::move(primal),std::move(dual),{},objective};
        }
        const auto direction=basis.solve(dense_column(f.matrix,entering));
        Index leaving=f.basis.size(); double step=infinity,relaxed=infinity,largest_pivot=0; bool uncertain_positive=false;
        for (Index i=0;i<direction.size();++i) if (direction[i]>tol.pivot) {
            const double candidate=(xb[i]+tol.primal*(1+std::abs(xb[i])))/direction[i];
            if (!std::isfinite(candidate)) throw NumericalError("ratio test overflow");
            relaxed=std::min(relaxed,candidate);
        } else if (direction[i]>0) uncertain_positive=true;
        if (std::isfinite(relaxed)) for (Index i=0;i<direction.size();++i) if (direction[i]>tol.pivot) {
            const double ratio=xb[i]/direction[i];
            if (!std::isfinite(ratio)) throw NumericalError("ratio test overflow");
            const double allowance=tol.primal*(1+std::abs(relaxed));
            if (ratio<=relaxed+allowance) {
                const double magnitude=std::abs(direction[i]);
                if (leaving==f.basis.size() || magnitude>largest_pivot ||
                    (magnitude==largest_pivot && f.basis[i]<f.basis[leaving])) {
                    leaving=i; step=std::max(0.0,ratio); largest_pivot=magnitude;
                }
            }
        }
        if (leaving==f.basis.size()) {
            if (uncertain_positive) throw NumericalError("positive ratio direction below pivot tolerance");
            std::vector<double> ray(f.matrix.columns(),0); ray[entering]=1;
            for (Index i=0;i<direction.size();++i) ray[f.basis[i]]=-direction[i];
            return {SolveStatus::unbounded,std::move(primal),std::move(dual),std::move(ray),0};
        }
        run.emit("RATIO_TEST","harris step="+std::to_string(step)+" pivot="+std::to_string(largest_pivot));
        const auto old=f.basis[leaving];
        const auto prior_refactorizations=basis.refactorizations();
        basis.replace(leaving,entering);
        f.basis[leaving]=entering; ++run.iterations;
        if (basis.refactorizations()!=prior_refactorizations)
            run.emit("REFACTORIZATION","product-form update limit or numerical pivot guard");
        run.emit("LP_ITERATION",std::string(phase_one ? "phase_I " : "phase_II ")+std::to_string(old)+" -> "+std::to_string(entering));
    }
}
SimplexState dual_simplex(detail::StandardForm&f,std::span<const double>cost,Run&run){
 const auto&tol=run.options.tolerances;run.emit("DUAL_SIMPLEX_STARTED");UpdatedBasis basis(f.matrix,f.basis,tol.pivot,32);run.emit("FACTORIZATION","dual-simplex basis dimension="+std::to_string(f.basis.size()));
 while(true){const auto limit=run.limit();if(limit!=SolveStatus::optimal)return {limit,{},{},{},0};auto xb=basis.solve(f.rhs);std::vector<double>cb;for(auto j:f.basis)cb.push_back(cost[j]);auto dual=basis.solve_transpose(cb);std::vector<bool>basic(f.matrix.columns(),false);for(auto j:f.basis)basic[j]=true;std::vector<double>reduced(f.matrix.columns());for(Index j=0;j<f.matrix.columns();++j)if(!basic[j]){reduced[j]=cost[j]-dot_column(f.matrix,j,dual);if(reduced[j]<-tol.dual*(1+std::abs(cost[j])))throw NumericalError("warm basis is not dual feasible");}
  Index leaving=f.basis.size();for(Index i=0;i<xb.size();++i)if(xb[i]<-tol.primal*(1+std::abs(f.rhs[i]))&&(leaving==f.basis.size()||f.basis[i]<f.basis[leaving]))leaving=i;
  if(leaving==f.basis.size()){std::vector<double>primal(f.matrix.columns());for(Index i=0;i<xb.size();++i)primal[f.basis[i]]=std::max(0.0,xb[i]);double obj=0;for(Index j=0;j<cost.size();++j)obj=std::fma(cost[j],primal[j],obj);run.emit("DUAL_SIMPLEX_COMPLETED");return {SolveStatus::optimal,std::move(primal),std::move(dual),{},obj};}
  std::vector<double>unit(f.basis.size());unit[leaving]=1;const auto row=basis.solve_transpose(unit);Index entering=f.matrix.columns();double best=infinity;
  for(Index j=0;j<f.matrix.columns();++j)if(!basic[j]&&!f.artificial[j]){const double a=dot_column(f.matrix,j,row);if(a<-tol.pivot){const double ratio=reduced[j]/(-a);if(ratio<best-tol.dual||(std::abs(ratio-best)<=tol.dual&&j<entering)){best=ratio;entering=j;}}}
  if(entering==f.matrix.columns())throw NumericalError("dual simplex detected primal infeasibility; cold Phase I required");const auto old=f.basis[leaving];const auto prior=basis.refactorizations();basis.replace(leaving,entering);f.basis[leaving]=entering;++run.iterations;if(basis.refactorizations()!=prior)run.emit("REFACTORIZATION","dual-simplex product-form update limit or numerical pivot guard");run.emit("DUAL_SIMPLEX_ITERATION",std::to_string(old)+" -> "+std::to_string(entering));
 }
}
void remove_artificials(detail::StandardForm& f,Run& run) {
    for (Index i=0;i<f.basis.size();) {
        if (!f.artificial[f.basis[i]]) { ++i; continue; }
        if (run.limit()!=SolveStatus::optimal) return;
        SparseBasis basis(f.matrix,f.basis,run.options.tolerances.pivot);
        std::vector<bool> basic(f.matrix.columns(),false); for (auto j : f.basis) basic[j]=true;
        Index entering=f.matrix.columns(); bool uncertain_dependency=false;
        for (Index j=0;j<f.matrix.columns();++j) if (!f.artificial[j] && !basic[j]) {
            const auto direction=basis.solve(dense_column(f.matrix,j));
            if (std::abs(direction[i])>run.options.tolerances.pivot) { entering=j; break; }
            if (direction[i]!=0) uncertain_dependency=true;
        }
        if (entering!=f.matrix.columns()) { f.basis[i]=entering; ++run.iterations; ++i; }
        else {
            if (uncertain_dependency) throw NumericalError("small nonzero artificial-row pivot cannot establish redundancy");
            const auto art=f.matrix.column(f.basis[i]);
            if (art.rows.size()!=1) throw NumericalError("invalid artificial basis column");
            const auto physical_row=art.rows.front();
            f.basis.erase(f.basis.begin()+static_cast<std::ptrdiff_t>(i)); f.remove_row(physical_row);
            run.emit("REDUNDANT_ROW_REMOVED");
        }
    }
}
DualCertificate certificate(const Model& original,const detail::StandardForm& f,std::span<const double> dual,
                            std::span<const Index> original_rows,bool farkas) {
    DualCertificate c;
    c.row_lower.assign(original.constraints.size(),0); c.row_upper.assign(original.constraints.size(),0);
    c.variable_lower.assign(original.variables.size(),0); c.variable_upper.assign(original.variables.size(),0);
    for (Index i=0;i<f.origins.size();++i) if (f.origins[i].original) {
        const auto row=original_rows[f.origins[i].row]; const double y=dual[i]*f.origins[i].factor;
        if (y>=0 && std::isfinite(original.constraints[row].lower)) c.row_lower[row]+=y;
        else if (y<0 && std::isfinite(original.constraints[row].upper)) c.row_upper[row]-=y;
    }
    const double sign=original.sense==ObjectiveSense::minimize ? 1.0 : -1.0;
    const auto residual=[&](Index j) {
        double value=farkas ? 0 : sign*original.objective[j]; const auto col=original.matrix.column(j);
        for (Index k=0;k<col.rows.size();++k) value-=col.values[k]*(c.row_lower[col.rows[k]]-c.row_upper[col.rows[k]]);
        return value;
    };
    const auto residual_direction=[&](Index j) {
        std::vector<double> parts;
        const auto add=[&](double value) {
            if (value==0) return;
            std::vector<double> next;
            for (double p : parts) {
                const double sum=value+p, b=sum-value;
                const double error=(value-(sum-b))+(p-b);
                if (error!=0) next.push_back(error);
                value=sum;
            }
            if (value!=0) next.push_back(value);
            parts=std::move(next);
        };
        const auto product=[&](double a,double b) { const double p=a*b; add(std::fma(a,b,-p)); add(p); };
        if (!farkas) product(sign,original.objective[j]);
        add(-c.variable_lower[j]); add(c.variable_upper[j]);
        const auto col=original.matrix.column(j);
        for (Index k=0;k<col.rows.size();++k) {
            product(-col.values[k],c.row_lower[col.rows[k]]);
            product(col.values[k],c.row_upper[col.rows[k]]);
        }
        return parts.empty() ? 0 : (parts.back()>0 ? 1 : -1);
    };
    // Binary64 cannot represent many exact rational duals. Move an already-active
    // row multiplier outward by ULPs until stationarity residuals point toward a
    // finite variable bound. The independent verifier still checks the result.
    const auto correction_domains=infer_correction_domains(original);
    for (Index pass=0;pass<64*std::max<Index>(1,original.variables.size());++pass) {
        bool changed=false;
        for (Index j=0;j<original.variables.size();++j) {
            const int direction=residual_direction(j); const auto& domain=correction_domains[j];
            const bool need_up=direction<0 && !std::isfinite(domain.upper);
            const bool need_down=direction>0 && !std::isfinite(domain.lower);
            if (!need_up && !need_down) continue;
            const auto col=original.matrix.column(j);
            for (Index k=0;k<col.rows.size();++k) {
                const auto i=col.rows[k]; const double a=col.values[k]; double* multiplier=nullptr; bool finite_side=false;
                if (need_up) { multiplier=a>0 ? &c.row_upper[i] : &c.row_lower[i]; finite_side=a>0 ? std::isfinite(original.constraints[i].upper) : std::isfinite(original.constraints[i].lower); }
                else { multiplier=a>0 ? &c.row_lower[i] : &c.row_upper[i]; finite_side=a>0 ? std::isfinite(original.constraints[i].lower) : std::isfinite(original.constraints[i].upper); }
                if (finite_side && *multiplier>0) { *multiplier=std::nextafter(*multiplier,infinity); changed=true; break; }
                double* opposite=multiplier==&c.row_lower[i] ? &c.row_upper[i] : &c.row_lower[i];
                if (*opposite>0) { *opposite=std::nextafter(*opposite,0); changed=true; break; }
            }
        }
        if (!changed) break;
    }
    for (Index j=0;j<original.variables.size();++j) {
        const double value=residual(j);
        if (value>=0 && std::isfinite(original.variables[j].lower)) c.variable_lower[j]=std::max(0.0,std::nextafter(value,-infinity));
        if (value<0 && std::isfinite(original.variables[j].upper)) c.variable_upper[j]=std::max(0.0,std::nextafter(-value,-infinity));
    }
    for (Index pass=0;pass<64;++pass) {
        bool changed=false;
        for (Index j=0;j<original.variables.size();++j) {
            const int direction=residual_direction(j); const auto& domain=correction_domains[j];
            if (direction<0 && !std::isfinite(domain.upper) && c.variable_lower[j]>0) {
                c.variable_lower[j]=std::nextafter(c.variable_lower[j],0); changed=true;
            } else if (direction>0 && !std::isfinite(domain.lower) && c.variable_upper[j]>0) {
                c.variable_upper[j]=std::nextafter(c.variable_upper[j],0); changed=true;
            }
        }
        if (!changed) break;
    }
    return c;
}
double objective(const Model& m,std::span<const double> x) {
    double value=m.objective_offset;
    for (Index j=0;j<x.size();++j) value=std::fma(m.objective[j],x[j],value);
    if (!std::isfinite(value)) throw NumericalError("objective overflow");
    return value;
}
}
SolveResult solve_lp(const Model& model,const SolverOptions& options,const LpWarmStart* warm,LpWarmStart* output) {
    validate_options(options);
    Model relaxed=model; for (auto& v : relaxed.variables) v.type=VariableType::continuous;
    const auto validation=validate(relaxed);
    for (const auto& issue : validation.issues) if (issue.code!="empty_row_infeasible") throw InvalidModelError(issue.message);
    Run run{options}; const auto cpu_start=std::clock(); SolveResult result;
    result.best_bound=model.sense==ObjectiveSense::minimize ? -infinity : infinity;
    const auto finish=[&]() {
        result.runtime_seconds=run.elapsed(); result.cpu_seconds=static_cast<double>(std::clock()-cpu_start)/CLOCKS_PER_SEC;
        result.iterations=run.iterations; run.emit("LP_COMPLETED",std::string(status_name(result.status))); return result;
    };
    try {
        run.emit("LP_STARTED");
        if (run.limit()!=SolveStatus::optimal) { result.status=run.limit(); return finish(); }
        PresolveResult p; p.reduced=relaxed; p.fixed_values.assign(model.variables.size(),std::numeric_limits<double>::quiet_NaN());
        p.original_columns.resize(model.variables.size()); p.original_rows.resize(model.constraints.size());
        std::iota(p.original_columns.begin(),p.original_columns.end(),0); std::iota(p.original_rows.begin(),p.original_rows.end(),0);
        if (options.presolve && validation.ok()) {
            run.emit("PRESOLVE_STARTED"); auto candidate=presolve(relaxed,options.tolerances,false);
            if (!candidate.infeasible) p=std::move(candidate);
            run.emit("PRESOLVE_COMPLETED",std::to_string(p.reduced.variables.size())+" variables, "+std::to_string(p.reduced.constraints.size())+" rows");
        }
        auto f=detail::standardize(p.reduced,options.scaling);
        SimplexState state;bool warm_used=false;
        if(warm&&warm->rows==f.matrix.rows()&&warm->columns==f.matrix.columns()&&warm->basis.size()==f.matrix.rows())try{f.basis=warm->basis;state=dual_simplex(f,f.cost,run);warm_used=state.status==SolveStatus::optimal;}catch(const NumericalError&){run.emit("WARM_START_REJECTED","cold Phase I fallback");f=detail::standardize(p.reduced,options.scaling);}
        std::vector<double> phase_one(f.cost.size(),0); for (Index j=0;j<f.cost.size();++j) if (f.artificial[j]) phase_one[j]=1;
        if(!warm_used)state=simplex(f,phase_one,true,run);
        if (state.status!=SolveStatus::optimal) { result.status=state.status==SolveStatus::unbounded ? SolveStatus::numerical_failure : state.status; return finish(); }
        if (!warm_used&&state.objective>options.tolerances.primal) {
            result.certificate=certificate(model,f,state.dual,p.original_rows,true);
            result.verification=verify_infeasibility(model,result.certificate,options.tolerances);
            result.status=result.verification.passed ? SolveStatus::infeasible : SolveStatus::numerical_failure;
            result.message="Phase I Farkas certificate checked in original coordinates"; return finish();
        }
        if(!warm_used){remove_artificials(f,run);state=simplex(f,f.cost,false,run);} result.status=state.status;
        if (state.status==SolveStatus::optimal || state.status==SolveStatus::unbounded) {
            result.primal=p.restore(f.restore(state.primal)); result.objective=objective(model,result.primal);
            if (state.status==SolveStatus::optimal) {
                result.certificate=certificate(model,f,state.dual,p.original_rows,false);
                result.verification=verify_optimality(model,result.primal,result.objective,result.certificate,options.tolerances);
                result.best_bound=result.verification.dual_bound; result.mip_gap=0;
                result.message="Original-model primal/dual feasibility and duality gap checked";
            } else {
                const auto reduced_ray=f.restore(state.ray,true); result.ray.assign(model.variables.size(),0);
                for (Index j=0;j<reduced_ray.size();++j) result.ray[p.original_columns[j]]=reduced_ray[j];
                result.verification=verify_unboundedness(model,result.primal,result.ray,options.tolerances,false);
                result.message="Original-model feasible point and improving recession ray checked";
            }
            if (result.status==SolveStatus::optimal&&output)*output={f.matrix.rows(),f.matrix.columns(),f.basis};
            if (!result.verification.passed) {
                result.status=SolveStatus::numerical_failure; result.mip_gap=infinity;
                for (const auto& issue : result.verification.violations) result.message+="; "+issue;
            }
        }
    } catch (const NumericalError& error) { result.status=SolveStatus::numerical_failure; result.message=error.what(); }
      catch (const std::overflow_error& error) { result.status=SolveStatus::numerical_failure; result.message=error.what(); }
    return finish();
}
SolveResult solve_lp(const Model&model,const SolverOptions&options){return solve_lp(model,options,nullptr,nullptr);}
}
