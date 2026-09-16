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
TEST_CASE("Strong branching performs real LP probes and preserves the verified optimum","[mip][branching]") {
 auto m=example({9,8,7},{{"a",0,1,VariableType::binary},{"b",0,1,VariableType::binary},{"c",0,1,VariableType::binary}},{{"capacity",-infinity,3}},{{0,0,2},{0,1,2},{0,2,2}});
 SolverOptions o;o.branching="strong";o.cuts=false;std::vector<std::string>events;o.telemetry=[&](const auto&e){events.push_back(e.type);};const auto r=run(m,o);
 REQUIRE(r.status==SolveStatus::optimal);REQUIRE(r.objective==Catch::Approx(9));REQUIRE(r.verification.passed);REQUIRE(std::find(events.begin(),events.end(),"STRONG_BRANCH_PROBE")!=events.end());
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
    o={}; o.node_limit=1; o.rounding=false; o.feasibility_pump=false; o.cuts=false; const auto r=run(m,o);
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

TEST_CASE("Single-row Chvatal-Gomory cuts preserve every bounded integer point and strengthen the relaxation","[mip][cuts]") {
 auto m=example({1,1},{{"x",0,3,VariableType::integer},{"y",0,3,VariableType::integer}},{{"fractional",-infinity,1.5}},{{0,0,.6},{0,1,.6}});
 const auto solved=run(m);
 REQUIRE(solved.status==SolveStatus::optimal);
 REQUIRE(solved.objective==Catch::Approx(2));
 REQUIRE(solved.cuts_added>=1);
 const auto stats=apply_safe_root_cuts(m,std::vector<double>{1.5,1.0});
 REQUIRE(stats.chvatal_gomory>=1);
 bool strengthened=false;
 for(Index i=1;i<m.constraints.size();++i) {
  const auto activity=m.matrix.multiply(std::vector<double>{1.5,1.0})[i];
  if(activity>m.constraints[i].upper+1e-12) strengthened=true;
 }
 REQUIRE(strengthened);
 for(int x=0;x<=3;++x)for(int y=0;y<=3;++y)if(.6*x+.6*y<=1.5) {
  const auto activity=m.matrix.multiply(std::vector<double>{double(x),double(y)});
  for(Index i=1;i<m.constraints.size();++i) REQUIRE(activity[i]<=m.constraints[i].upper+1e-12);
 }
}

TEST_CASE("Single-row Chvatal-Gomory cuts preserve shifted two-sided integer domains","[mip][cuts][regression]") {
 std::mt19937 rng(9127);
 for(Index trial=0;trial<40;++trial) {
  const int lower_x=int(rng()%3)-2,lower_y=int(rng()%3)-2;
  const double a=(int(rng()%15)-7)/4.0,b=(int(rng()%15)-7)/4.0;
  const double row_lower=(int(rng()%17)-8)/3.0,row_upper=row_lower+1.0+(rng()%20)/4.0;
  auto m=example({0,0},{{"x",double(lower_x),double(lower_x+5),VariableType::integer},{"y",double(lower_y),double(lower_y+5),VariableType::integer}},{{"row",row_lower,row_upper}},{{0,0,a},{0,1,b}});
  (void)apply_safe_root_cuts(m,std::vector<double>{lower_x+2.25,lower_y+2.75});
  for(int x=lower_x;x<=lower_x+5;++x)for(int y=lower_y;y<=lower_y+5;++y) {
   const double original_activity=a*x+b*y;
   if(original_activity<row_lower||original_activity>row_upper) continue;
   const auto activity=m.matrix.multiply(std::vector<double>{double(x),double(y)});
   INFO("trial="<<trial<<" x="<<x<<" y="<<y<<" a="<<a<<" b="<<b);
   for(Index i=1;i<m.constraints.size();++i)
    REQUIRE(activity[i]<=m.constraints[i].upper+1e-12);
  }
 }
}

TEST_CASE("Tableau GMI cuts remove a fractional integer basic point and preserve mixed feasible points","[mip][cuts][gmi]") {
 auto m=example({1,0},{{"x",0,4,VariableType::integer},{"y",0,4,VariableType::continuous}},{{"mixed",-infinity,2.5}},{{0,0,1},{0,1,1}});
 SolverOptions o;o.presolve=false;o.scaling=false;LpWarmStart warm;const auto relaxation=solve_lp(m,o,nullptr,&warm);
 REQUIRE(relaxation.status==SolveStatus::optimal);REQUIRE(relaxation.primal[0]==Catch::Approx(2.5));
 const auto tableau=extract_lp_tableau(m,o,warm);const Index added=apply_tableau_gmi_cuts(m,tableau,relaxation.primal);
 REQUIRE(added>=1);
 for(int x=0;x<=2;++x)for(int quarter=0;quarter<=10-4*x;++quarter){const double y=quarter/4.0;const auto activity=m.matrix.multiply(std::vector<double>{double(x),y});for(Index i=1;i<m.constraints.size();++i)REQUIRE(activity[i]>=m.constraints[i].lower-1e-10);}
 auto original=example({1,0},{{"x",0,4,VariableType::integer},{"y",0,4,VariableType::continuous}},{{"mixed",-infinity,2.5}},{{0,0,1},{0,1,1}});
 const auto solved=run(original);REQUIRE(solved.status==SolveStatus::optimal);REQUIRE(solved.objective==Catch::Approx(2));REQUIRE(solved.cuts_added>=1);
}

TEST_CASE("Feasibility pump finds and independently verifies an incumbent missed by one-shot rounding","[mip][heuristic]") {
 auto m=example({1,1},{{"x",0,1,VariableType::binary},{"y",0,1,VariableType::binary}},{{"capacity",-infinity,1.5}},{{0,0,1},{0,1,1}});
 SolverOptions o;o.cuts=false;o.node_limit=1;o.rounding=true;o.feasibility_pump=true;o.feasibility_pump_passes=4;
 std::vector<std::string> events;o.telemetry=[&](const auto&e){events.push_back(e.type);};
 const auto r=run(m,o);
 REQUIRE(r.status==SolveStatus::optimal);
 REQUIRE(r.incumbent_updates==1);
 REQUIRE(r.objective==Catch::Approx(1));
 REQUIRE(r.verification.passed);
 REQUIRE(std::find(events.begin(),events.end(),"FEASIBILITY_PUMP_STARTED")!=events.end());
 REQUIRE(std::find(events.begin(),events.end(),"FEASIBILITY_PUMP_INCUMBENT")!=events.end());
}
