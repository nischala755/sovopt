#include <sovereign/validation.hpp>
#include <sovereign/errors.hpp>
#include <cmath>
#include <unordered_set>

namespace sovereign {
namespace {
bool valid_bounds(double lo, double up) {
    return !std::isnan(lo) && !std::isnan(up) && lo != infinity && up != -infinity && lo <= up;
}
}
ValidationReport validate(const Model& m) {
    ValidationReport report;
    const auto add = [&](const std::string& code, const std::string& message) { report.issues.push_back({code,message}); };
    const bool dimensions_ok = m.matrix.rows() == m.constraints.size() && m.matrix.columns() == m.variables.size();
    if (!dimensions_ok) add("matrix_dimensions","Matrix dimensions do not match row and variable counts");
    if (m.objective.size() != m.variables.size()) add("objective_dimensions","Objective size does not match variable count");
    if (!std::isfinite(m.objective_offset)) add("nonfinite_offset","Objective offset must be finite");
    if (m.sense != ObjectiveSense::minimize && m.sense != ObjectiveSense::maximize) add("objective_sense","Unknown objective sense");
    for (double c : m.objective) if (!std::isfinite(c)) { add("nonfinite_objective","Objective coefficients must be finite"); break; }
    std::unordered_set<std::string> names;
    for (const auto& v : m.variables) {
        if (v.name.empty()) add("empty_variable_name","Variable name cannot be empty");
        if (!names.insert(v.name).second) add("duplicate_variable_name","Duplicate variable: " + v.name);
        const bool bounds_ok = valid_bounds(v.lower,v.upper);
        if (!bounds_ok) add("variable_bounds","Invalid bounds for variable: " + v.name);
        if (v.type != VariableType::continuous && v.type != VariableType::integer && v.type != VariableType::binary)
            add("variable_type","Unknown variable domain: " + v.name);
        if (v.type == VariableType::binary && (v.lower < 0 || v.upper > 1)) add("binary_bounds","Binary bounds must lie in [0,1]: " + v.name);
        if (bounds_ok && v.type != VariableType::continuous && std::ceil(v.lower) > std::floor(v.upper))
            add("empty_integer_domain","No integer lies within bounds: " + v.name);
    }
    names.clear();
    std::vector<bool> occupied;
    if (dimensions_ok) {
        occupied.assign(m.constraints.size(),false);
        for (Index j = 0; j < m.matrix.columns(); ++j) for (auto row : m.matrix.column(j).rows) occupied[row] = true;
    }
    for (Index i = 0; i < m.constraints.size(); ++i) {
        const auto& r = m.constraints[i];
        if (r.name.empty()) add("empty_constraint_name","Constraint name cannot be empty");
        if (!names.insert(r.name).second) add("duplicate_constraint_name","Duplicate constraint: " + r.name);
        if (!valid_bounds(r.lower,r.upper)) add("constraint_bounds","Invalid bounds for constraint: " + r.name);
        else if (dimensions_ok && !occupied[i] && (r.lower > 0 || r.upper < 0))
            add("empty_row_infeasible","Constant zero row excludes zero: " + r.name);
    }
    return report;
}
void require_valid(const Model& model) {
    const auto report = validate(model);
    if (!report.ok()) {
        std::string message;
        for (const auto& issue : report.issues) message += issue.code + ": " + issue.message + "\n";
        throw InvalidModelError(message);
    }
}
}
