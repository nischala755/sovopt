#include <catch2/catch_test_macros.hpp>
#include <sovereign/generators.hpp>
#include <sovereign/lp.hpp>
#include <sovereign/mip.hpp>
#include <sovereign/validation.hpp>
using namespace sovereign;
TEST_CASE("industrial generators create valid solvable mathematical models","[generators]") {
 const std::vector<Model> continuous={generate_crude_blending_model(1,4),generate_supply_chain_model(2,2,4),generate_process_model(3,4)};
 for(const auto&m:continuous){INFO(m.name);REQUIRE(validate(m).ok());const auto r=solve_lp(m);INFO(r.message);REQUIRE(r.status==SolveStatus::optimal);REQUIRE(r.verification.passed);}
 auto production=generate_production_planning_model(4,2,2);REQUIRE(validate(production).ok());SolverOptions o;o.node_limit=100;const auto r=solve_mip(production,o);INFO(r.message);REQUIRE(r.status==SolveStatus::optimal);REQUIRE(r.verification.passed);
}
TEST_CASE("industrial generators reject zero dimensions","[generators]") { REQUIRE_THROWS(generate_crude_blending_model(1,0)); REQUIRE_THROWS(generate_production_planning_model(1,1,0)); REQUIRE_THROWS(generate_supply_chain_model(1,0,1)); REQUIRE_THROWS(generate_process_model(1,0)); }
