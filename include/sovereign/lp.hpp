#pragma once
#include <sovereign/solution.hpp>
namespace sovereign {
struct LpWarmStart { Index rows=0, columns=0; std::vector<Index> basis; };
// LP relaxation: variable integrality is deliberately ignored.
[[nodiscard]] SolveResult solve_lp(const Model& model, const SolverOptions& options = {});
[[nodiscard]] SolveResult solve_lp(const Model&, const SolverOptions&, const LpWarmStart* input, LpWarmStart* output);
}
