#include <catch2/catch_test_macros.hpp>
#include <sovereign/model.hpp>
using namespace sovereign;

TEST_CASE("Model statistics count domains rows and coefficients independently", "[model]") {
    Model m;
    m.name = "counts";
    m.variables = {{"x",0,infinity,VariableType::continuous}, {"y",-2,4,VariableType::integer}, {"z",0,1,VariableType::binary}};
    m.constraints = {{"le",-infinity,4}, {"eq",2,2}, {"range",1,3}};
    m.matrix = CscMatrix::from_triplets(3,3,{{0,0,2},{1,0,-3},{0,1,1},{2,2,0.5}});
    m.objective = {3,0,1};
    const auto s = statistics(m);
    REQUIRE(s.variables == 3);
    REQUIRE(s.constraints == 3);
    REQUIRE(s.nonzeros == 4);
    REQUIRE(s.continuous_variables == 1);
    REQUIRE(s.integer_variables == 2);
    REQUIRE(s.binary_variables == 1);
    REQUIRE(s.equality_rows == 1);
    REQUIRE(s.ranged_rows == 1);
    REQUIRE(s.objective_nonzeros == 2);
    REQUIRE(s.minimum_absolute_coefficient == 0.5);
    REQUIRE(s.maximum_absolute_coefficient == 3);
    REQUIRE(s.density == 4.0 / 9.0);
}
TEST_CASE("Empty model statistics have finite zero density and coefficient extrema", "[model]") {
    const auto s = statistics(Model{});
    REQUIRE(s.variables == 0);
    REQUIRE(s.nonzeros == 0);
    REQUIRE(s.density == 0);
    REQUIRE(s.minimum_absolute_coefficient == 0);
    REQUIRE(s.maximum_absolute_coefficient == 0);
}
