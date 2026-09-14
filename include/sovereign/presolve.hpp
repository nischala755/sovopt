#pragma once
#include <sovereign/solution.hpp>
#include <string>

namespace sovereign {
struct PresolveResult {
    Model reduced;
    std::vector<Index> original_columns, original_rows;
    std::vector<double> fixed_values;
    bool infeasible = false;
    std::string message;
    std::vector<std::string> journal;
    [[nodiscard]] std::vector<double> restore(std::span<const double> primal) const;
};
[[nodiscard]] PresolveResult presolve(const Model& model, const Tolerances& tolerances = {}, bool tighten_singletons = true);
}
