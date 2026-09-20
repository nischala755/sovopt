#include <catch2/catch_test_macros.hpp>
#include <sovereign/presolve.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
using namespace sovereign;

TEST_CASE("Presolve substitutes fixed variables and reconstructs original order", "[presolve]") {
    Model m; m.variables={{"x",2,2},{"y",0,10}};
    m.constraints={{"r",-infinity,9}}; m.objective={3,4}; m.objective_offset=1;
    m.matrix=CscMatrix::from_triplets(1,2,{{0,0,2},{0,1,1}});
    const auto p=presolve(m);
    REQUIRE_FALSE(p.infeasible); REQUIRE(p.reduced.objective_offset==7);
    REQUIRE(p.original_columns==std::vector<Index>{1});
    REQUIRE(p.original_rows==std::vector<Index>{0});
    REQUIRE(p.reduced.variables[0].upper>=5); REQUIRE(p.reduced.variables[0].upper<5.000001);
    REQUIRE(p.restore(std::vector<double>{3})==std::vector<double>{2,3});
    REQUIRE_THROWS_AS(p.restore(std::vector<double>{}),std::invalid_argument);
    REQUIRE(m.variables.size()==2); REQUIRE_FALSE(p.journal.empty());
}
TEST_CASE("Presolve detects constant contradictions and removes redundant rows", "[presolve]") {
    Model m; m.constraints={{"empty",1,infinity}}; m.matrix=CscMatrix::from_triplets(1,0,{});
    REQUIRE(presolve(m).infeasible);
    m.constraints[0].lower=0;
    REQUIRE(presolve(m).reduced.constraints.empty());
}
TEST_CASE("Presolve rounds singleton integer bounds with negative coefficients", "[presolve]") {
    Model m; m.variables={{"x",0,10,VariableType::integer}}; m.objective={1};
    m.constraints={{"r",-7,-5}}; m.matrix=CscMatrix::from_triplets(1,1,{{0,0,-2}});
    const auto p=presolve(m); REQUIRE_FALSE(p.infeasible);
    REQUIRE(p.restore(std::vector<double>{})==std::vector<double>{3});
    m.constraints[0]={"r",-5,-5}; REQUIRE(presolve(m).infeasible);
}
TEST_CASE("Presolve rejects finite arithmetic overflow", "[presolve]") {
    Model m; m.variables={{"x",1e308,1e308}}; m.objective={2};
    m.matrix=CscMatrix::from_triplets(0,1,{});
    REQUIRE_THROWS_AS(presolve(m),std::overflow_error);
    m.objective={0}; m.constraints={{"r",0,1}};
    m.matrix=CscMatrix::from_triplets(1,1,{{0,0,2}});
    REQUIRE_THROWS_AS(presolve(m),std::overflow_error);
}
TEST_CASE("Presolve never fixes merely close bounds or drops tiny coefficients", "[presolve]") {
    Model m; m.variables={{"x",1,1+1e-10}}; m.objective={0};
    m.constraints={{"r",-infinity,2}}; m.matrix=CscMatrix::from_triplets(1,1,{{0,0,1e-20}});
    auto p=presolve(m); REQUIRE(p.original_columns.size()==1); REQUIRE(p.reduced.matrix.nonzeros()==1);
}
TEST_CASE("Presolve singleton tightening can be disabled for dual postsolve", "[presolve]") {
    Model m; m.variables={{"x",0,10}}; m.objective={1};
    m.constraints={{"r",2,4}}; m.matrix=CscMatrix::from_triplets(1,1,{{0,0,1}});
    auto p=presolve(m,{},false); REQUIRE(p.reduced.variables[0].lower==0);
    REQUIRE(p.reduced.variables[0].upper==10);
}
TEST_CASE("Presolve propagates integer singleton fixes through multiple rows", "[presolve]") {
    Model m; m.variables={{"x",0,10,VariableType::integer},{"y",0,10,VariableType::integer}};
    m.objective={2,3}; m.constraints={{"a",2,2},{"b",5,5}};
    m.matrix=CscMatrix::from_triplets(2,2,{{0,0,1},{1,0,1},{1,1,1}});
    auto p=presolve(m); REQUIRE_FALSE(p.infeasible);
    REQUIRE(p.restore(std::vector<double>{})==std::vector<double>{2,3});
    REQUIRE(p.reduced.objective_offset==13); REQUIRE(p.reduced.constraints.empty());
}
TEST_CASE("Presolve removes exact duplicate rows but preserves near and differently bounded rows", "[presolve][duplicates]") {
    Model m;m.variables={{"x",0,10},{"y",0,10}};m.objective={1,1};
    m.constraints={{"original",-infinity,5},{"duplicate",-infinity,5},{"different_bound",-infinity,6},{"near",-infinity,5}};
    m.matrix=CscMatrix::from_triplets(4,2,{{0,0,1},{0,1,2},{1,0,1},{1,1,2},{2,0,1},{2,1,2},{3,0,1},{3,1,2+1e-12}});
    const auto p=presolve(m);REQUIRE_FALSE(p.infeasible);REQUIRE(p.reduced.constraints.size()==3);REQUIRE(p.original_rows==std::vector<Index>{0,2,3});REQUIRE(p.reduced.matrix.nonzeros()==6);
    REQUIRE(std::any_of(p.journal.begin(),p.journal.end(),[](const auto& entry){return entry.find("Remove exact duplicate row: duplicate")!=std::string::npos;}));
}
