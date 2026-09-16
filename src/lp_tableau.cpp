#include <sovereign/lp.hpp>
#include <sovereign/basis.hpp>
#include <sovereign/validation.hpp>
#include "lp_standard.hpp"
#include <cmath>

namespace sovereign {
LpTableau extract_lp_tableau(const Model& model,const SolverOptions& options,
                             const LpWarmStart& warm) {
    require_valid(model);
    validate_options(options);
    auto standard=detail::standardize(model,options.scaling);
    if(warm.rows!=standard.matrix.rows()||warm.columns!=standard.matrix.columns()||
       warm.basis.size()!=standard.matrix.rows())
        throw NumericalError("LP warm basis is incompatible with the standard form");
    SparseBasis basis(standard.matrix,warm.basis,options.tolerances.pivot);
    LpTableau result;
    result.columns.resize(standard.matrix.columns());
    for(Index j=0;j<result.columns.size();++j) {
        result.columns[j].artificial=standard.artificial[j];
        const auto& expression=standard.expressions[j];
        result.columns[j].representable=expression.representable;
        result.columns[j].expression_constant=expression.constant;
        result.columns[j].original_expression=expression.terms;
        bool lattice=expression.representable&&!expression.terms.empty()&&
            expression.constant==std::trunc(expression.constant);
        for(const auto& [original,coefficient]:expression.terms)
            if(model.variables[original].type==VariableType::continuous||coefficient!=std::trunc(coefficient)) lattice=false;
        result.columns[j].integer_lattice=lattice&&!standard.artificial[j];
    }
    for(Index original=0;original<standard.variables.size();++original) {
        const auto& transform=standard.variables[original];
        const bool lattice=model.variables[original].type!=VariableType::continuous&&
            transform.shift==std::trunc(transform.shift)&&transform.terms.size()==1&&
            std::abs(transform.terms.front().second)==1.0;
        for(const auto& [column,coefficient]:transform.terms) {
            auto& metadata=result.columns[column];
            metadata.original_variable=original;
            metadata.restore_coefficient=coefficient;
            metadata.integer_lattice=metadata.integer_lattice&&lattice;
        }
    }
    const auto basic_values=basis.solve(standard.rhs);
    result.rows.reserve(warm.basis.size());
    for(Index row=0;row<warm.basis.size();++row) {
        std::vector<double> unit(warm.basis.size(),0.0);
        unit[row]=1.0;
        const auto multiplier=basis.solve_transpose(unit);
        auto coefficients=standard.matrix.transpose_multiply(multiplier);
        for(Index k=0;k<warm.basis.size();++k) {
            const double expected=row==k?1.0:0.0;
            if(!std::isfinite(coefficients[warm.basis[k]])||
               std::abs(coefficients[warm.basis[k]]-expected)>1e-9)
                throw NumericalError("Extracted tableau basis is not an identity within tolerance");
            coefficients[warm.basis[k]]=expected;
        }
        if(!std::isfinite(basic_values[row]))
            throw NumericalError("Extracted tableau right-hand side is nonfinite");
        result.rows.push_back({warm.basis[row],basic_values[row],std::move(coefficients)});
    }
    return result;
}
}
