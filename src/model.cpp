#include <sovereign/model.hpp>
#include <algorithm>
#include <cmath>

namespace sovereign {
ModelStatistics statistics(const Model& m) {
    ModelStatistics s;
    s.variables = m.variables.size(); s.constraints = m.constraints.size(); s.nonzeros = m.matrix.nonzeros();
    for (const auto& v : m.variables) {
        if (v.type == VariableType::continuous) ++s.continuous_variables;
        else ++s.integer_variables;
        if (v.type == VariableType::binary) ++s.binary_variables;
    }
    for (const auto& r : m.constraints) {
        if (std::isfinite(r.lower) && r.lower == r.upper) ++s.equality_rows;
        else if (std::isfinite(r.lower) && std::isfinite(r.upper)) ++s.ranged_rows;
    }
    for (double value : m.objective) if (value != 0) ++s.objective_nonzeros;
    if (s.variables && s.constraints) s.density = static_cast<double>(s.nonzeros) / static_cast<double>(s.variables) / static_cast<double>(s.constraints);
    if (s.nonzeros) {
        s.minimum_absolute_coefficient = infinity;
        for (Index j = 0; j < m.matrix.columns(); ++j) for (double value : m.matrix.column(j).values) {
            s.minimum_absolute_coefficient = std::min(s.minimum_absolute_coefficient,std::abs(value));
            s.maximum_absolute_coefficient = std::max(s.maximum_absolute_coefficient,std::abs(value));
        }
    }
    return s;
}
}
