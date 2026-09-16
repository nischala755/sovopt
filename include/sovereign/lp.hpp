#pragma once
#include <sovereign/solution.hpp>
#include <limits>
namespace sovereign {
struct LpWarmStart { Index rows=0, columns=0; std::vector<Index> basis; };
struct LpTableauColumn {
    Index original_variable = std::numeric_limits<Index>::max();
    double restore_coefficient = 0;
    bool integer_lattice = false;
    bool artificial = false;
    bool representable = false;
    double expression_constant = 0;
    std::vector<std::pair<Index,double>> original_expression;
};
// Each row represents sum_j coefficients[j] * z_j = rhs in the solver's
// nonnegative standard-form variables. Basic columns form the identity.
struct LpTableauRow { Index basic_column=0; double rhs=0; std::vector<double> coefficients; };
struct LpTableau { std::vector<LpTableauColumn> columns; std::vector<LpTableauRow> rows; };
// LP relaxation: variable integrality is deliberately ignored.
[[nodiscard]] SolveResult solve_lp(const Model& model, const SolverOptions& options = {});
[[nodiscard]] SolveResult solve_lp(const Model&, const SolverOptions&, const LpWarmStart* input, LpWarmStart* output);
[[nodiscard]] LpTableau extract_lp_tableau(const Model&, const SolverOptions&, const LpWarmStart&);
}
