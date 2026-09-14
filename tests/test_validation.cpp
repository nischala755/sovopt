#include <catch2/catch_test_macros.hpp>
#include <sovereign/validation.hpp>
#include <sovereign/errors.hpp>
#include <algorithm>
#include <limits>
using namespace sovereign;
namespace {
Model valid_model() {
    Model m;
    m.variables = {{"x",0,10,VariableType::continuous}};
    m.constraints = {{"r",-infinity,5}};
    m.objective = {1};
    m.matrix = CscMatrix::from_triplets(1,1,{{0,0,1}});
    return m;
}
bool has_code(const ValidationReport& r, const std::string& code) {
    return std::any_of(r.issues.begin(),r.issues.end(),[&](const auto& i){ return i.code == code; });
}
}
TEST_CASE("Validation accepts valid and empty models", "[validation]") {
    REQUIRE(validate(valid_model()).ok());
    REQUIRE(validate(Model{}).ok());
    REQUIRE_NOTHROW(require_valid(valid_model()));
}
TEST_CASE("Validation reports inconsistent dimensions without unsafe access", "[validation]") {
    auto m = valid_model();
    m.objective.clear();
    m.variables.clear();
    m.constraints.clear();
    const auto r = validate(m);
    REQUIRE_FALSE(r.ok());
    REQUIRE(has_code(r,"matrix_dimensions"));
    REQUIRE_THROWS_AS(require_valid(m), InvalidModelError);
    m = valid_model(); m.objective.clear();
    REQUIRE(has_code(validate(m),"objective_dimensions"));
}
TEST_CASE("Validation rejects duplicate empty names and invalid numeric data", "[validation]") {
    auto m = valid_model();
    m.variables.push_back(m.variables.front());
    m.constraints.push_back(m.constraints.front());
    REQUIRE(has_code(validate(m),"duplicate_variable_name"));
    REQUIRE(has_code(validate(m),"duplicate_constraint_name"));
    m = valid_model(); m.variables[0].name.clear(); m.constraints[0].name.clear();
    REQUIRE(has_code(validate(m),"empty_variable_name"));
    REQUIRE(has_code(validate(m),"empty_constraint_name"));
    m = valid_model(); m.objective[0] = infinity; m.objective_offset = std::numeric_limits<double>::quiet_NaN();
    REQUIRE(has_code(validate(m),"nonfinite_objective"));
    REQUIRE(has_code(validate(m),"nonfinite_offset"));
}
TEST_CASE("Validation enforces bound orientations and discrete domains", "[validation]") {
    auto m = valid_model();
    for (auto bounds : {std::pair{2.0,1.0}, std::pair{infinity,infinity}, std::pair{-infinity,-infinity}, std::pair{std::numeric_limits<double>::quiet_NaN(),1.0}}) {
        m.variables[0].lower = bounds.first; m.variables[0].upper = bounds.second;
        REQUIRE(has_code(validate(m),"variable_bounds"));
        m.constraints[0].lower = bounds.first; m.constraints[0].upper = bounds.second;
        REQUIRE(has_code(validate(m),"constraint_bounds"));
    }
    m = valid_model(); m.variables[0] = {"x",0.2,0.8,VariableType::integer};
    REQUIRE(has_code(validate(m),"empty_integer_domain"));
    m.variables[0] = {"x",-1,1,VariableType::binary};
    REQUIRE(has_code(validate(m),"binary_bounds"));
    m.variables[0] = {"x",1,1,VariableType::binary};
    REQUIRE(validate(m).ok());
}
TEST_CASE("Validation identifies impossible constant rows but does not claim general feasibility", "[validation]") {
    auto m = valid_model();
    m.matrix = CscMatrix::from_triplets(1,1,{});
    m.constraints[0].lower = 1;
    REQUIRE(has_code(validate(m),"empty_row_infeasible"));
    m.constraints[0].lower = -1;
    REQUIRE(validate(m).ok());
    m = valid_model(); m.constraints[0].lower = 20; m.constraints[0].upper = infinity;
    REQUIRE(validate(m).ok()); // x <= 10 and x >= 20 is not a structural error.
}
TEST_CASE("Validation rejects invalid enumeration values", "[validation]") {
    auto m = valid_model(); m.sense = static_cast<ObjectiveSense>(99); m.variables[0].type = static_cast<VariableType>(99);
    REQUIRE(has_code(validate(m),"objective_sense"));
    REQUIRE(has_code(validate(m),"variable_type"));
}
