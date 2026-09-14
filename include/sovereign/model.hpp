#pragma once
#include <sovereign/sparse_matrix.hpp>
#include <limits>
#include <string>

namespace sovereign {
inline constexpr double infinity = std::numeric_limits<double>::infinity();
enum class VariableType { continuous, integer, binary };
enum class ObjectiveSense { minimize, maximize };
struct Variable {
    std::string name;
    double lower = 0;
    double upper = infinity;
    VariableType type = VariableType::continuous;
};
struct Constraint {
    std::string name;
    double lower = -infinity;
    double upper = infinity;
};
// Original coordinates are preserved. Future solvers consume a validated const Model.
struct Model {
    std::string name;
    CscMatrix matrix;
    std::vector<Variable> variables;
    std::vector<Constraint> constraints;
    std::vector<double> objective;
    ObjectiveSense sense = ObjectiveSense::minimize;
    double objective_offset = 0;
};
struct ModelStatistics {
    Index variables = 0, constraints = 0, nonzeros = 0;
    Index continuous_variables = 0, integer_variables = 0, binary_variables = 0;
    Index equality_rows = 0, ranged_rows = 0, objective_nonzeros = 0;
    double density = 0, minimum_absolute_coefficient = 0, maximum_absolute_coefficient = 0;
};
[[nodiscard]] ModelStatistics statistics(const Model& model);
}
