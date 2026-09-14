#pragma once
#include <sovereign/solution.hpp>
namespace sovereign {
[[nodiscard]] SolveResult solve_mip(const Model&, const SolverOptions& = {});
}
