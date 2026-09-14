#pragma once
#include <sovereign/solution.hpp>
namespace sovereign {
// LP relaxation: variable integrality is deliberately ignored.
[[nodiscard]] SolveResult solve_lp(const Model& model, const SolverOptions& options = {});
}
