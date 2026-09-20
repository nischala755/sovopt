#include <sovereign/presolve.hpp>
#include <sovereign/validation.hpp>
#include <sovereign/errors.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>

namespace sovereign {
namespace {
double checked(double value) {
    if (!std::isfinite(value)) throw std::overflow_error("Presolve finite arithmetic overflow");
    return value;
}
// Expand a computed finite bound by one ULP. Never turn a finite operation's
// overflow into an apparently unconstrained bound.
double outward(double value, bool lower) {
    return std::nextafter(checked(value), lower ? -infinity : infinity);
}
double shift(double bound, double coefficient, double fixed, bool lower) {
    if (!std::isfinite(bound)) return bound;
    // FMA has one rounding, so one outward ULP encloses the exact shifted bound.
    return outward(std::fma(-coefficient, fixed, bound), lower);
}
}

std::vector<double> PresolveResult::restore(std::span<const double> primal) const {
    if (infeasible) throw std::invalid_argument("Cannot restore an infeasible presolve result");
    if (primal.size()!=original_columns.size()) throw std::invalid_argument("Reduced primal dimension mismatch");
    auto result=fixed_values;
    for (Index j=0;j<primal.size();++j) {
        if (!std::isfinite(primal[j])) throw std::invalid_argument("Reduced primal must be finite");
        result.at(original_columns[j])=primal[j];
    }
    return result;
}

PresolveResult presolve(const Model& model, const Tolerances& tolerances, bool tighten_singletons) {
    for (double t : {tolerances.primal,tolerances.dual,tolerances.integrality,tolerances.pivot})
        if (!std::isfinite(t) || t<0) throw ConfigurationError("Presolve tolerances must be finite and nonnegative");
    PresolveResult result;
    result.reduced=model;
    result.fixed_values.assign(model.variables.size(),std::numeric_limits<double>::quiet_NaN());
    const auto report=validate(model);
    bool contradiction=false;
    for (const auto& issue:report.issues) {
        if (issue.code=="empty_row_infeasible" || issue.code=="empty_integer_domain") contradiction=true;
        else throw InvalidModelError(issue.code+": "+issue.message);
    }
    if (contradiction) {
        result.infeasible=true; result.message="Input contains an empty row or integer domain";
        result.journal.push_back(result.message); return result;
    }
    auto variables=model.variables;
    auto rows=model.constraints;
    std::vector<bool> active_col(variables.size(),true), active_row(rows.size(),true);
    const auto fail=[&](const std::string& message) {
        result.infeasible=true; result.message=message; result.journal.push_back(message);
    };
    bool changed=true;
    // A fixed substitution removes a column permanently. Singleton tightening
    // only needs another pass when it exposes such a substitution.
    while (changed && !result.infeasible) {
        changed=false;
        for (Index j=0;j<variables.size();++j) if (active_col[j]) {
            auto& v=variables[j];
            if (v.type!=VariableType::continuous) {
                const double lo=std::ceil(v.lower), hi=std::floor(v.upper);
                if (lo!=v.lower || hi!=v.upper) result.journal.push_back("Round integer bounds: "+v.name);
                v.lower=lo; v.upper=hi;
            }
            if (v.lower>v.upper) { fail("Empty variable domain: "+v.name); break; }
            if (v.lower!=v.upper) continue;
            const double fixed=checked(v.lower);
            result.reduced.objective_offset=checked(std::fma(model.objective[j],fixed,result.reduced.objective_offset));
            const auto column=model.matrix.column(j);
            for (Index k=0;k<column.rows.size();++k) {
                auto& row=rows[column.rows[k]];
                row.lower=shift(row.lower,column.values[k],fixed,true);
                row.upper=shift(row.upper,column.values[k],fixed,false);
            }
            active_col[j]=false; result.fixed_values[j]=fixed; changed=true;
            result.journal.push_back("Substitute fixed variable: "+v.name+" = "+std::to_string(fixed));
        }
        if (result.infeasible) break;
        std::vector<Index> count(rows.size(),0), singleton(rows.size(),0);
        std::vector<double> coefficient(rows.size(),0);
        for (Index j=0;j<variables.size();++j) if(active_col[j]) {
            const auto column=model.matrix.column(j);
            for(Index k=0;k<column.rows.size();++k) {
                const Index i=column.rows[k]; ++count[i]; singleton[i]=j; coefficient[i]=column.values[k];
            }
        }
        for(Index i=0;i<rows.size();++i) if(active_row[i]) {
            const auto& row=rows[i];
            if(count[i]==0) {
                if(row.lower>0 || row.upper<0) { fail("Constant row excludes zero: "+row.name); break; }
                active_row[i]=false; result.journal.push_back("Remove redundant constant row: "+row.name);
            } else if(count[i]==1 && tighten_singletons) {
                auto& v=variables[singleton[i]];
                const double a=coefficient[i];
                const double lb=a>0?row.lower:row.upper, ub=a>0?row.upper:row.lower;
                double lo=std::isfinite(lb)?outward(lb/a,true):-infinity;
                double hi=std::isfinite(ub)?outward(ub/a,false):infinity;
                if(v.type!=VariableType::continuous) { lo=std::ceil(lo); hi=std::floor(hi); }
                lo=std::max(lo,v.lower); hi=std::min(hi,v.upper);
                if(lo>hi) { fail("Singleton row creates empty domain: "+row.name); break; }
                if(lo!=v.lower || hi!=v.upper) {
                    v.lower=lo; v.upper=hi;
                    result.journal.push_back("Tighten bounds from singleton row: "+row.name);
                    if(lo==hi) changed=true;
                }
            }
        }
    }
    if(result.infeasible) return result;
    // Exact duplicate rows can be removed without changing the feasible set or
    // requiring dual multiplier redistribution during postsolve. Differently
    // bounded or merely near-equal rows deliberately remain active.
    using RowKey=std::tuple<std::vector<std::pair<Index,double>>,double,double>;
    std::vector<std::vector<std::pair<Index,double>>> row_entries(rows.size());
    for(Index j=0;j<variables.size();++j)if(active_col[j]){const auto column=model.matrix.column(j);for(Index k=0;k<column.rows.size();++k)if(active_row[column.rows[k]])row_entries[column.rows[k]].push_back({j,column.values[k]});}
    std::map<RowKey,Index> unique_rows;
    for(Index i=0;i<rows.size();++i)if(active_row[i]){
        RowKey key{row_entries[i],rows[i].lower,rows[i].upper};const auto [found,inserted]=unique_rows.emplace(std::move(key),i);
        if(!inserted){active_row[i]=false;result.journal.push_back("Remove exact duplicate row: "+rows[i].name+" (same as "+rows[found->second].name+")");}
    }
    result.reduced.variables.clear(); result.reduced.constraints.clear(); result.reduced.objective.clear();
    std::vector<Index> row_map(rows.size());
    for(Index i=0;i<rows.size();++i) if(active_row[i]) {
        row_map[i]=result.original_rows.size(); result.original_rows.push_back(i);
        result.reduced.constraints.push_back(rows[i]);
    }
    std::vector<Triplet> entries;
    for(Index j=0;j<variables.size();++j) if(active_col[j]) {
        const Index new_j=result.original_columns.size(); result.original_columns.push_back(j);
        result.reduced.variables.push_back(variables[j]); result.reduced.objective.push_back(model.objective[j]);
        const auto column=model.matrix.column(j);
        for(Index k=0;k<column.rows.size();++k) if(active_row[column.rows[k]])
            entries.push_back({row_map[column.rows[k]],new_j,column.values[k]});
    }
    result.reduced.matrix=CscMatrix::from_triplets(result.original_rows.size(),result.original_columns.size(),std::move(entries));
    result.message="Conservative presolve completed";
    return result;
}
}
