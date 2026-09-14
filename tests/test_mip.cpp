#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <sovereign/mip.hpp>
#include <sovereign/verification.hpp>
#include <sovereign/cuts.hpp>
#include <iostream>
#include <random>
using namespace sovereign;
namespace {
Model example(std::vector<double> c,std::vector<Variable> variables,std::vector<Constraint> rows,std::vector<Triplet> a,ObjectiveSense sense=ObjectiveSense::maximize) {
    Model m; m.objective=std::move(c); m.variables=std::move(variables); m.constraints=std::move(rows); m.sense=sense;
    m.matrix=CscMatrix::from_triplets(m.constraints.size(),m.variables.size(),std::move(a)); return m;
}
SolveResult run(const Model& m,const SolverOptions& o={}) {
    auto r=solve_mip(m,o);
    std::cout<<"MIP status="<<status_name(r.status)<<" objective="<<r.objective<<" bound="<<r.best_bound<<" gap="<<r.mip_gap<<" nodes="<<r.nodes<<" runtime="<<r.runtime_seconds<<" LPiterations="<<r.iterations<<" incumbent_updates="<<r.incumbent_updates<<'\n';
    return r;
}
}
TEST_CASE("MILP hand maximum and binary knapsack", "[mip]") {
    auto m=example({10,7},{{"x",0,infinity,VariableType::integer},{"y",0,infinity,VariableType::integer}},{{"capacity",-infinity,15}},{{0,0,5},{0,1,3}});
    for(const auto& strategy:{"most_fractional","pseudocost"}) {
        SolverOptions o; o.branching=strategy; const auto r=run(m,o); INFO(r.message);
        REQUIRE(r.status==SolveStatus::optimal); REQUIRE(r.objective==Catch::Approx(35)); REQUIRE(r.best_bound==Catch::Approx(35)); REQUIRE(verify_primal(m,r.primal,r.objective).passed);
    }
    auto k=example({6,10,12},{{"a",0,1,VariableType::binary},{"b",0,1,VariableType::binary},{"c",0,1,VariableType::binary}},{{"capacity",-infinity,5}},{{0,0,1},{0,1,2},{0,2,3}});
    const auto r=run(k); REQUIRE(r.status==SolveStatus::optimal); REQUIRE(r.objective==Catch::Approx(22));
}
TEST_CASE("MILP fractional equality is integer infeasible", "[mip]") {
    const auto m=example({1},{{"x",0,1,VariableType::integer}},{{"equality",1,1}},{{0,0,2}});
    const auto r=run(m); INFO(r.message); REQUIRE(r.status==SolveStatus::infeasible); REQUIRE(r.primal.empty()); REQUIRE(std::isinf(r.objective));
}
TEST_CASE("MILP explicit budgets cancellation and preserved bound", "[mip]") {
    const auto m=example({1,1},{{"x",0,1,VariableType::binary},{"y",0,1,VariableType::binary}},{{"capacity",-infinity,3}},{{0,0,2},{0,1,2}});
    SolverOptions o; o.node_limit=0; REQUIRE(run(m,o).status==SolveStatus::node_limit);
    o={}; o.time_limit_seconds=0; REQUIRE(run(m,o).status==SolveStatus::time_limit);
    o={}; o.iteration_limit=0; REQUIRE(run(m,o).status==SolveStatus::iteration_limit);
    o={}; o.cancelled=[] { return true; }; REQUIRE(run(m,o).status==SolveStatus::cancelled);
    o={}; o.node_limit=1; o.rounding=false; o.cuts=false; const auto r=run(m,o);
    REQUIRE(r.status==SolveStatus::node_limit); REQUIRE(r.best_bound>=1); REQUIRE(r.best_bound<=1.500001); REQUIRE(r.primal.empty());
}
TEST_CASE("MILP safe row cuts apply and continuous rows stay unchanged", "[mip]") {
    auto m=example({1},{{"x",0,5,VariableType::integer}},{{"cap",-infinity,2.75}},{{0,0,1}});
    auto r=run(m); REQUIRE(r.status==SolveStatus::optimal); REQUIRE(r.objective==Catch::Approx(2)); REQUIRE(r.cuts_added==1);
    m.variables[0].type=VariableType::continuous; r=run(m); REQUIRE(r.status==SolveStatus::optimal); REQUIRE(r.objective==Catch::Approx(2.75)); REQUIRE(r.cuts_added==0);
}
TEST_CASE("MILP bounded random models agree with exhaustive independent oracle", "[mip][benchmark]") {
    std::mt19937 rng(7103);
    for(Index test=0;test<40;++test) {
        const double a=double(1+rng()%7),b=double(1+rng()%7),cap=double(rng()%25);
        auto m=example({double(int(rng()%13)-6),double(int(rng()%13)-6)},{{"x",0,4,VariableType::integer},{"y",0,4,VariableType::integer}},{{"capacity",-infinity,cap}},{{0,0,a},{0,1,b}},test%2?ObjectiveSense::minimize:ObjectiveSense::maximize);
        m.objective_offset=-2.5; double expected=m.sense==ObjectiveSense::minimize?infinity:-infinity;
        for(int x=0;x<=4;++x) for(int y=0;y<=4;++y) if(a*x+b*y<=cap) {
            const double value=m.objective_offset+m.objective[0]*x+m.objective[1]*y;
            expected=m.sense==ObjectiveSense::minimize?std::min(expected,value):std::max(expected,value);
        }
        SolverOptions o; o.branching=test%2?"pseudocost":"most_fractional"; o.cuts=test%3!=0; o.rounding=test%3!=1;
        const auto r=run(m,o); INFO("case "<<test<<": "<<r.message);
        REQUIRE(r.status==SolveStatus::optimal); REQUIRE(r.objective==Catch::Approx(expected).margin(1e-7)); REQUIRE(verify_primal(m,r.primal,r.objective).passed);
    }
}
TEST_CASE("MILP unboundedness requires integer base and integral ray", "[mip]") {
    const auto m=example({1},{{"x",0,infinity,VariableType::integer}}, {},{});
    const auto r=run(m); REQUIRE(r.status==SolveStatus::unbounded); REQUIRE(verify_unboundedness(m,r.primal,r.ray,{},true).passed);
    auto mixed=example({0,1},{{"x",0,1,VariableType::integer},{"y",0,infinity,VariableType::continuous}},{{"half",1,1}},{{0,0,2}});
    const auto q=run(mixed); REQUIRE(q.status==SolveStatus::numerical_failure); REQUIRE(q.primal.empty());
}

TEST_CASE("Mixed-integer maximization normalizes continuous LP bounds", "[mip][regression]") {
    Model m; m.name="mixed-max"; m.sense=ObjectiveSense::maximize;
    m.variables={{"x",0,2,VariableType::integer},{"y",0,2,VariableType::continuous}};
    m.constraints={{"capacity",-infinity,2.5}};
    m.matrix=CscMatrix::from_triplets(1,2,{{0,0,1},{0,1,1}});
    m.objective={1.5,1};
    const auto result=solve_mip(m);
    REQUIRE(result.status==SolveStatus::optimal);
    REQUIRE(result.verification.passed);
    REQUIRE(result.objective==Catch::Approx(3.5));
    REQUIRE(result.primal[0]==Catch::Approx(2));
    REQUIRE(result.primal[1]==Catch::Approx(0.5));
}
TEST_CASE("Safe cover and clique cuts exclude only impossible binary combinations","[mip][cuts]") {
 auto m=example({1,1,1},{{"a",0,1,VariableType::binary},{"b",0,1,VariableType::binary},{"c",0,1,VariableType::binary}},{{"capacity",-infinity,3}},{{0,0,2},{0,1,2},{0,2,1}});
 const auto stats=apply_safe_root_cuts(m);REQUIRE(stats.cover>=1);REQUIRE(stats.clique>=1);
 const auto r=run(m);REQUIRE(r.status==SolveStatus::optimal);REQUIRE(r.objective==Catch::Approx(2));REQUIRE(r.verification.passed);
}
