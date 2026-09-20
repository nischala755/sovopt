#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <sovereign/lp.hpp>
#include <sovereign/basis.hpp>
#include <sovereign/verification.hpp>
#include <sovereign/mps.hpp>
#include <random>
using namespace sovereign;
using Catch::Approx;
namespace {
Model make_lp(std::vector<double> c, std::vector<Variable> vars,
              std::vector<Constraint> rows, std::vector<Triplet> a, ObjectiveSense sense=ObjectiveSense::minimize) {
    Model m; m.objective=std::move(c); m.variables=std::move(vars); m.constraints=std::move(rows); m.sense=sense;
    m.matrix=CscMatrix::from_triplets(m.constraints.size(),m.variables.size(),std::move(a)); return m;
}
void optimal(const Model& m, double objective) {
    for (bool scaling : {false,true}) {
        SolverOptions options; options.scaling=scaling;
        const auto r=solve_lp(m,options); INFO(r.message);
        REQUIRE(r.status==SolveStatus::optimal); REQUIRE(r.verification.passed);
        REQUIRE(r.objective==Approx(objective).margin(1e-7));
        REQUIRE(verify_optimality(m,r.primal,r.objective,r.certificate).passed);
        REQUIRE(r.mip_gap==0);
    }
}
}
TEST_CASE("Revised simplex solves the hand-verifiable maximum with certificate", "[lp]") {
    auto m=read_mps_file(std::string(SOVEREIGN_SOURCE_DIR)+"/examples/small_lp.mps");
    optimal(m,9);
    const auto r=solve_lp(m); REQUIRE(r.primal[0]==Approx(1)); REQUIRE(r.primal[1]==Approx(3));
}
TEST_CASE("LP relaxation certificates are independent of integer metadata", "[lp][regression]") {
    const auto m=make_lp({10,7},{{"x",0,infinity,VariableType::integer},{"y",0,infinity,VariableType::integer}},
        {{"capacity",-infinity,15}},{{0,0,5},{0,1,3}},ObjectiveSense::maximize);
    const auto r=solve_lp(m); INFO(r.message); INFO(r.certificate.row_lower[0]); INFO(r.certificate.row_upper[0]);
    INFO(r.certificate.variable_lower[0]); INFO(r.certificate.variable_lower[1]); REQUIRE(r.status==SolveStatus::optimal); REQUIRE(r.objective==Approx(35));
}
TEST_CASE("LP handles greater-than equality free upper-only and fixed variables", "[lp]") {
    optimal(make_lp({1,2},{{"x"},{"y"}},{{"a",3,infinity},{"b",4,infinity}},{{0,0,1},{0,1,1},{1,0,1},{1,1,2}}),4);
    optimal(make_lp({1},{{"x",-infinity,infinity}},{{"eq",-3,-3}},{{0,0,1}}),-3);
    optimal(make_lp({-1},{{"x",-infinity,5}}, {},{}),-5);
    optimal(make_lp({3,2},{{"x",2,2},{"y"}},{{"r",5,infinity}},{{0,0,1},{0,1,1}}),12);
    Model empty; empty.objective_offset=7; optimal(empty,7);
}
TEST_CASE("LP phase I proves infeasibility in original coordinates", "[lp]") {
    const auto m=make_lp({1},{{"x"}},{{"a",2,infinity},{"b",-infinity,1}},{{0,0,1},{1,0,1}});
    const auto r=solve_lp(m); INFO(r.message);
    REQUIRE(r.status==SolveStatus::infeasible); REQUIRE(r.verification.passed);
    REQUIRE(verify_infeasibility(m,r.certificate).passed);
}
TEST_CASE("LP reports unbounded only with a verified feasible point and recession ray", "[lp]") {
    const auto m=make_lp({-1,0},{{"x"},{"y"}},{{"r",0,0}},{{0,0,1},{0,1,-1}});
    const auto r=solve_lp(m); INFO(r.message);
    REQUIRE(r.status==SolveStatus::unbounded); REQUIRE(r.verification.passed);
    REQUIRE(verify_unboundedness(m,r.primal,r.ray).passed);
}
TEST_CASE("LP removes redundant phase I artificial basics safely", "[lp]") {
    optimal(make_lp({1,2},{{"x"},{"y"}},{{"a",1,1},{"b",2,2},{"zero",0,0}},{{0,0,1},{0,1,1},{1,0,2},{1,1,2}}),1);
}
TEST_CASE("LP exact duplicate-row presolve preserves original-coordinate certificate", "[lp][presolve][duplicates]") {
    const auto model=make_lp({-3,-2},{{"x",0,infinity},{"y",0,infinity}},
        {{"capacity",-infinity,4},{"copy",-infinity,4},{"resource",-infinity,5}},
        {{0,0,1},{0,1,1},{1,0,1},{1,1,1},{2,0,2},{2,1,1}});
    SolverOptions options;options.presolve=true;const auto result=solve_lp(model,options);INFO(result.message);
    REQUIRE(result.status==SolveStatus::optimal);REQUIRE(result.objective==Approx(-9));
    REQUIRE(result.verification.passed);REQUIRE(verify_optimality(model,result.primal,result.objective,result.certificate).passed);
}
TEST_CASE("Bland pricing terminates the classical cycling example", "[lp][regression]") {
    const auto model=make_lp({10,-57,-9,-24},{{"a"},{"b"},{"c"},{"d"}},{{"r1",-infinity,0},{"r2",-infinity,0},{"r3",-infinity,1}},
        {{0,0,.5},{0,1,-5.5},{0,2,-2.5},{0,3,9},{1,0,.5},{1,1,-1.5},{1,2,-.5},{1,3,1},{2,0,1}},ObjectiveSense::maximize);
    std::vector<std::string> events;
    SolverOptions options; options.telemetry=[&](const auto& event){events.push_back(event.type);};
    const auto result=solve_lp(model,options);
    REQUIRE(result.status==SolveStatus::optimal);
    REQUIRE(result.objective==Approx(1));
    REQUIRE(std::find(events.begin(),events.end(),"FACTORIZATION")!=events.end());
    REQUIRE(std::find(events.begin(),events.end(),"RATIO_TEST")!=events.end());
}
TEST_CASE("LP scaling resolves coefficient magnitude differences", "[lp]") {
    const auto m=make_lp({1,1},{{"x"},{"y"}},{{"a",-infinity,1e-9},{"b",-infinity,2e9}},{{0,0,1e-9},{1,1,1e9}},ObjectiveSense::maximize);
    const auto r=solve_lp(m); INFO(r.message); REQUIRE(r.status==SolveStatus::optimal); REQUIRE(r.objective==Approx(3));
}
TEST_CASE("LP limits cancellation and deterministic trajectories are explicit", "[lp]") {
    auto m=read_mps_file(std::string(SOVEREIGN_SOURCE_DIR)+"/examples/small_lp.mps");
    SolverOptions o; o.iteration_limit=1; REQUIRE(solve_lp(m,o).status==SolveStatus::iteration_limit);
    o={}; o.cancelled=[] {return true;}; REQUIRE(solve_lp(m,o).status==SolveStatus::cancelled);
    std::vector<std::string> first,second;
    o={}; o.telemetry=[&](const auto& e){first.push_back(e.type+e.detail);}; const auto a=solve_lp(m,o);
    o.telemetry=[&](const auto& e){second.push_back(e.type+e.detail);}; const auto b=solve_lp(m,o);
    REQUIRE(a.primal==b.primal); REQUIRE(a.iterations==b.iterations); REQUIRE(first==second);
}
TEST_CASE("Small LP regression bank agrees with independent vertex enumeration", "[lp][benchmark]") {
    std::mt19937 rng(7123);
    for (int sample=0;sample<30;++sample) {
        const double a=1+rng()%7, b=1+rng()%7, cap=1+rng()%20, cx=1+rng()%9, cy=1+rng()%9;
        const double ux=1+rng()%7, uy=1+rng()%7;
        auto m=make_lp({cx,cy},{{"x",0,ux},{"y",0,uy}},{{"capacity",-infinity,cap}},{{0,0,a},{0,1,b}},ObjectiveSense::maximize);
        double expected=0;
        for (auto [x,y] : std::vector<std::pair<double,double>>{{0,0},{ux,0},{0,uy},{ux,uy},{cap/a,0},{0,cap/b},{ux,(cap-a*ux)/b},{(cap-b*uy)/a,uy}})
            if (x>=0 && y>=0 && x<=ux && y<=uy && a*x+b*y<=cap+1e-10) expected=std::max(expected,cx*x+cy*y);
        const auto result=solve_lp(m); INFO(sample); INFO(result.message);
        REQUIRE(result.status==SolveStatus::optimal); REQUIRE(result.objective==Approx(expected).margin(1e-6));
    }
}
TEST_CASE("Tiny nonzero redundant-row candidates never certify false unboundedness", "[lp][regression]") {
    const auto m=make_lp({0,-1,0},{{"x"},{"y"},{"z",-infinity,infinity}},{{"a",0,0},{"b",0,0},{"c",0,0}},
        {{0,0,1},{1,0,1},{1,1,1e-13},{2,1,1},{2,2,1}});
    for (bool scale : {false,true}) {
        SolverOptions o; o.scaling=scale; const auto r=solve_lp(m,o);
        REQUIRE(r.status!=SolveStatus::unbounded); REQUIRE(r.status!=SolveStatus::infeasible);
        if (r.status==SolveStatus::optimal) REQUIRE(r.objective==Approx(0).margin(1e-10));
    }
}
TEST_CASE("Small objective scale does not turn an unbounded LP into an optimum", "[lp][regression]") {
    const auto m=make_lp({-1e-9},{{"x"}}, {},{});
    const auto r=solve_lp(m); INFO(r.message); REQUIRE(r.status==SolveStatus::unbounded);
}
TEST_CASE("Phase I does not claim infeasibility from small free-variable residuals", "[lp][regression]") {
    const auto m=make_lp({0,0,0},{{"x",-infinity,infinity},{"y"},{"z",-infinity,infinity}},
        {{"a",1,infinity},{"b",-infinity,0},{"c",0,0}},{{0,1,1},{0,0,1e-9},{1,1,1},{2,0,1},{2,2,1}});
    const auto r=solve_lp(m); INFO(r.message); REQUIRE(r.status!=SolveStatus::infeasible);
}
TEST_CASE("Netlib SC50 certificates verify in original coordinates", "[lp][regression][netlib]") {
    for(const auto& [name,expected]:std::vector<std::pair<std::string,double>>{{"sc50a",-64.575077059},{"sc50b",-70.0}}) {
        MpsOptions mps; mps.format=MpsFormat::fixed;
        const auto model=read_mps_file(std::string(SOVEREIGN_SOURCE_DIR)+"/tests/data/"+name+".mps",mps);
        const auto result=solve_lp(model);
        INFO(name); INFO(result.message);
        REQUIRE(result.status==SolveStatus::optimal);
        REQUIRE(result.verification.passed);
        REQUIRE(result.objective==Approx(expected).margin(1e-6));
        REQUIRE(result.iterations<1000);
    }
}
TEST_CASE("Netlib BLEND Phase I produces a primal candidate without a false unbounded claim", "[lp][regression][netlib]") {
    MpsOptions mps; mps.format=MpsFormat::fixed;
    const auto model=read_mps_file(std::string(SOVEREIGN_SOURCE_DIR)+"/tests/data/blend.mps",mps);
    const auto result=solve_lp(model);
    INFO(result.message);
    REQUIRE(result.status!=SolveStatus::unbounded);
    REQUIRE(result.iterations>81);
    REQUIRE_FALSE(result.primal.empty());
    REQUIRE(verify_primal(model,result.primal,result.objective).passed);
    REQUIRE(result.objective==Approx(-30.8121498458).margin(1e-7));
    REQUIRE(result.message.find("variable 78")==std::string::npos);
}
TEST_CASE("Netlib ADLITTLE terminates with a verified optimum", "[lp][regression][netlib]") {
    MpsOptions mps; mps.format=MpsFormat::fixed;
    const auto model=read_mps_file(std::string(SOVEREIGN_SOURCE_DIR)+"/tests/data/adlittle.mps",mps);
    SolverOptions options; options.iteration_limit=1000;
    const auto result=solve_lp(model,options);
    INFO(result.message);
    REQUIRE(result.status==SolveStatus::optimal);
    REQUIRE(result.verification.passed);
    REQUIRE(result.objective==Approx(225494.9631623803).margin(1e-5));
}
TEST_CASE("Medium Netlib LP certificates verify in original coordinates", "[lp][regression][netlib]") {
    MpsOptions mps; mps.format=MpsFormat::fixed;
    for(const auto& [name,expected]:std::vector<std::pair<std::string,double>>{{"israel",-896644.82186},{"sc205",-52.202061212}}) {
        const auto model=read_mps_file(std::string(SOVEREIGN_SOURCE_DIR)+"/tests/data/"+name+".mps",mps);
        SolverOptions options;options.iteration_limit=2000;
        const auto result=solve_lp(model,options);
        INFO(name);INFO(result.message);
        REQUIRE(result.status==SolveStatus::optimal);
        REQUIRE(result.verification.passed);
        REQUIRE(result.objective==Approx(expected).margin(1e-5));
    }
}
TEST_CASE("LP warm start uses dual simplex to repair a tightened bound","[lp][warmstart]") {
 auto m=make_lp({-3,-2},{{"x",0,4},{"y",0,4}},{{"cap",-infinity,5}},{{0,0,1},{0,1,1}});LpWarmStart warm;
 const auto root=solve_lp(m,{},nullptr,&warm);REQUIRE(root.status==SolveStatus::optimal);REQUIRE_FALSE(warm.basis.empty());
 m.variables[0].upper=2;std::vector<std::string>events;SolverOptions o;o.presolve=false;o.telemetry=[&](const auto&e){events.push_back(e.type);};LpWarmStart child;
 const auto result=solve_lp(m,o,&warm,&child);INFO(result.message);REQUIRE(result.status==SolveStatus::optimal);REQUIRE(result.objective==Catch::Approx(-12));REQUIRE(result.verification.passed);REQUIRE(std::find(events.begin(),events.end(),"DUAL_SIMPLEX_STARTED")!=events.end());
}

TEST_CASE("LP tableau extraction returns a verified identity basis and integrality metadata","[lp][tableau]") {
 auto m=make_lp({1,1},{{"x",0,infinity,VariableType::integer},{"y",0,infinity,VariableType::continuous}},{{"cap",-infinity,2.5}},{{0,0,1},{0,1,1}},ObjectiveSense::maximize);
 SolverOptions o;o.presolve=false;o.scaling=false;LpWarmStart warm;
 const auto solved=solve_lp(m,o,nullptr,&warm);REQUIRE(solved.status==SolveStatus::optimal);
 const auto tableau=extract_lp_tableau(m,o,warm);
 REQUIRE(tableau.rows.size()==warm.rows);REQUIRE(tableau.columns.size()==warm.columns);
 REQUIRE(tableau.columns[0].original_variable==0);REQUIRE(tableau.columns[0].integer_lattice);
 REQUIRE(tableau.columns[1].original_variable==1);REQUIRE_FALSE(tableau.columns[1].integer_lattice);
 for(Index i=0;i<tableau.rows.size();++i) {
  REQUIRE(tableau.rows[i].basic_column==warm.basis[i]);
  REQUIRE(std::isfinite(tableau.rows[i].rhs));
  for(Index k=0;k<warm.basis.size();++k)
   REQUIRE(tableau.rows[i].coefficients[warm.basis[k]]==Catch::Approx(i==k?1.0:0.0).margin(1e-11));
 }
 auto invalid=warm;invalid.columns++;
 REQUIRE_THROWS_AS(extract_lp_tableau(m,o,invalid),NumericalError);
}

TEST_CASE("LP tableau metadata refuses unsafe scaled and split integer lattices","[lp][tableau][numerical]") {
 auto scaled=make_lp({-1,0},{{"x",0,infinity,VariableType::integer},{"y",0,infinity,VariableType::continuous}},{{"cap",-infinity,4}},{{0,0,2},{0,1,4}});
 SolverOptions o;o.presolve=false;o.scaling=true;LpWarmStart warm;
 REQUIRE(solve_lp(scaled,o,nullptr,&warm).status==SolveStatus::optimal);
 const auto first=extract_lp_tableau(scaled,o,warm);
 REQUIRE(first.columns[0].restore_coefficient==Catch::Approx(2));
 REQUIRE_FALSE(first.columns[0].integer_lattice);
 auto split=make_lp({0},{{"z",-infinity,infinity,VariableType::integer}},{{"fix",0,0}},{{0,0,1}});
 o.scaling=false;warm={};REQUIRE(solve_lp(split,o,nullptr,&warm).status==SolveStatus::optimal);
 const auto second=extract_lp_tableau(split,o,warm);
 REQUIRE(second.columns[0].original_variable==0);REQUIRE(second.columns[1].original_variable==0);
 REQUIRE_FALSE(second.columns[0].integer_lattice);REQUIRE_FALSE(second.columns[1].integer_lattice);
}
