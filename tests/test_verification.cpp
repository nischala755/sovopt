#include <catch2/catch_test_macros.hpp>
#include <sovereign/verification.hpp>
#include <cmath>
#include <limits>
using namespace sovereign;
TEST_CASE("Singleton implied domains certify rounded duals independently", "[verification]") {
    Model m; m.variables={{"x",-infinity,infinity}}; m.constraints={{"fixed",1,1}};
    m.matrix=CscMatrix::from_triplets(1,1,{{0,0,3}}); m.objective={1};
    REQUIRE(verify_optimality(m,std::vector<double>{1.0/3},1.0/3,DualCertificate{{1.0/3},{0},{0},{0}}).passed);
    m.matrix=CscMatrix::from_triplets(1,1,{{0,0,-3}}); m.constraints[0]={"negative fixed",-1,-1};
    REQUIRE(verify_optimality(m,std::vector<double>{1.0/3},1.0/3,DualCertificate{{0},{1.0/3},{0},{0}}).passed);
    m.variables={{"x",0,infinity},{"y",0,infinity}};
    m.constraints={{"small",-infinity,1e-9},{"large",-infinity,2e9}};
    m.matrix=CscMatrix::from_triplets(2,2,{{0,0,1e-9},{1,1,1e9}}); m.objective={1,1}; m.sense=ObjectiveSense::maximize;
    REQUIRE(verify_optimality(m,std::vector<double>{1,2},3,DualCertificate{{0,0},{1.0/1e-9,1.0/1e9},{0,0},{0,0}}).passed);
}
TEST_CASE("Certificate construction and verification share conservative implied domains", "[verification]") {
    Model m; m.variables={{"x",0,infinity}}; m.constraints={{"cap",-infinity,3}};
    m.matrix=CscMatrix::from_triplets(1,1,{{0,0,2}}); m.objective={1};
    const auto domains=infer_correction_domains(m);
    REQUIRE(domains.size()==1);
    REQUIRE(domains[0].lower==0);
    REQUIRE(std::isfinite(domains[0].upper));
    REQUIRE(domains[0].upper>=1.5);
}
TEST_CASE("Tiny residuals cannot certify an unbounded dual or invalid ray", "[verification]") {
    Model m; m.matrix=CscMatrix::from_triplets(0,1,{}); m.variables={{"x",0,1}}; m.objective={-100};
    REQUIRE_FALSE(verify_unboundedness(m,std::vector<double>{0},std::vector<double>{1e-8}).passed);
    m.variables[0].upper=infinity; m.objective={-1e-9};
    REQUIRE_FALSE(verify_optimality(m,std::vector<double>{0},0,DualCertificate{{},{},{0},{0}}).passed);
}
TEST_CASE("Free-variable stationarity residual invalidates false Farkas certificate", "[verification]") {
    Model m; m.variables={{"x",-infinity,infinity},{"y",0,infinity},{"z",-infinity,infinity}};
    m.constraints={{"demand",1,infinity},{"cap",-infinity,0},{"link",0,0}};
    m.matrix=CscMatrix::from_triplets(3,3,{{0,0,1e-9},{0,1,1},{1,1,1},{2,0,1},{2,2,1}}); m.objective={0,0,0};
    REQUIRE(verify_primal(m,std::vector<double>{1e9,0,-1e9},0).passed);
    REQUIRE_FALSE(verify_infeasibility(m,DualCertificate{{1,0,0},{0,1,0},{0,0,0},{0,0,0}}).passed);
}
TEST_CASE("Finite domain corrections change the certified objective bound", "[verification]") {
    Model m; m.matrix=CscMatrix::from_triplets(0,1,{}); m.variables={{"x",0,1e9}}; m.objective={-1e-9};
    const DualCertificate d{{},{},{0},{0}};
    auto wrong=verify_optimality(m,std::vector<double>{0},0,d);
    REQUIRE_FALSE(wrong.passed); REQUIRE(wrong.dual_bound<=-1);
    auto optimum=verify_optimality(m,std::vector<double>{1e9},-1,d);
    REQUIRE(optimum.passed); REQUIRE(optimum.dual_bound<=-1);
}
TEST_CASE("Cancellation cannot conceal a recession row sign", "[verification]") {
    Model m; m.matrix=CscMatrix::from_triplets(1,3,{{0,0,1e16},{0,1,1},{0,2,-1e16}});
    m.variables={{"x",0,infinity},{"y",0,infinity},{"z",0,infinity}};
    m.constraints={{"cap",-infinity,0}}; m.objective={-1,0,0};
    REQUIRE_FALSE(verify_unboundedness(m,std::vector<double>{0,0,0},std::vector<double>{1,1,1}).passed);
    REQUIRE(verify_unboundedness(m,std::vector<double>{0,0,0},std::vector<double>{1,0,1}).passed);
}
TEST_CASE("Cancellation cannot conceal free-variable stationarity", "[verification]") {
    Model m; m.matrix=CscMatrix::from_triplets(3,2,{{0,0,1},{1,0,1},{2,0,1},{0,1,1},{1,1,1},{2,1,1}});
    m.variables={{"x",-infinity,infinity},{"y",-infinity,infinity}}; m.constraints={{"a",0,infinity},{"b",0,infinity},{"c",-infinity,0}}; m.objective={0,0};
    REQUIRE_FALSE(verify_optimality(m,std::vector<double>{0,0},0,DualCertificate{{1e16,1,0},{0,0,1e16},{0,0},{0,0}}).passed);
    m.objective={1,1};
    REQUIRE(verify_optimality(m,std::vector<double>{0,0},0,DualCertificate{{1e16,1,0},{0,0,1e16},{0,0},{0,0}}).passed);
}
TEST_CASE("Uncertain product underflow is rejected conservatively", "[verification]") {
    Model m; m.matrix=CscMatrix::from_triplets(0,1,{}); m.variables={{"x",0,infinity}}; m.objective={-1e-300};
    REQUIRE_FALSE(verify_unboundedness(m,std::vector<double>{0},std::vector<double>{1e-100}).passed);
}
namespace {
Model example() {
    Model m; m.variables={{"x",0,infinity}}; m.constraints={{"demand",2,infinity}};
    m.matrix=CscMatrix::from_triplets(1,1,{{0,0,1}}); m.objective={3}; m.objective_offset=5; return m;
}
}
TEST_CASE("Original model certificates detect independent corruptions", "[verification]") {
    auto m=example(); std::vector<double> x{2}; DualCertificate d{{3},{0},{0},{0}};
    REQUIRE(verify_optimality(m,x,11,d).passed);
    REQUIRE_FALSE(verify_primal(m,x,12).passed);
    d.row_lower[0]=2; REQUIRE_FALSE(verify_optimality(m,x,11,d).passed);
    d.row_lower[0]=-3; REQUIRE_FALSE(verify_optimality(m,x,11,d).passed);
    d.row_lower={}; REQUIRE_FALSE(verify_optimality(m,x,11,d).passed);
    x[0]=std::numeric_limits<double>::quiet_NaN(); REQUIRE_FALSE(verify_primal(m,x,11).passed);
    REQUIRE_FALSE(verify_primal(m,{},11).passed);
}
TEST_CASE("Maximum objective certificate uses original objective units", "[verification]") {
    auto m=example(); m.constraints[0]={"cap",-infinity,2}; m.sense=ObjectiveSense::maximize;
    REQUIRE(verify_optimality(m,std::vector<double>{2},11,DualCertificate{{0},{3},{0},{0}}).passed);
}
TEST_CASE("Farkas and recession certificates certify analytic models", "[verification]") {
    auto m=example(); m.variables[0].upper=1;
    DualCertificate d{{1},{0},{0},{1}};
    REQUIRE(verify_infeasibility(m,d).passed);
    d.variable_upper[0]=0; REQUIRE_FALSE(verify_infeasibility(m,d).passed);
    m.variables[0].upper=infinity; m.objective={-1};
    REQUIRE(verify_unboundedness(m,std::vector<double>{2},std::vector<double>{1}).passed);
    REQUIRE_FALSE(verify_unboundedness(m,std::vector<double>{2},std::vector<double>{-1}).passed);
    REQUIRE_FALSE(verify_unboundedness(m,std::vector<double>{2},std::vector<double>{0}).passed);
}
TEST_CASE("Integrality and invalid options are checked", "[verification]") {
    auto m=example(); m.variables[0].type=VariableType::integer;
    REQUIRE_FALSE(verify_primal(m,std::vector<double>{2.5},12.5).passed);
    REQUIRE(verify_primal(m,std::vector<double>{2.5},12.5,{},false).passed);
    SolverOptions o; REQUIRE_NOTHROW(validate_options(o));
    o.tolerances.primal=0; REQUIRE_THROWS(validate_options(o)); o={};
    o.time_limit_seconds=-1; REQUIRE_THROWS(validate_options(o)); o={};
    o.mip_gap=-1; REQUIRE_THROWS(validate_options(o)); o={};
    o.branching="invalid"; REQUIRE_THROWS(validate_options(o));
    REQUIRE(status_name(SolveStatus::optimal)=="optimal");
    REQUIRE(status_name(SolveStatus::cancelled)=="cancelled");
}
TEST_CASE("Certificates reject infinite-bound multipliers and nonfinite directions", "[verification]") {
    auto m=example(); DualCertificate d{{3},{0},{0},{1}};
    REQUIRE_FALSE(verify_optimality(m,std::vector<double>{2},11,d).passed);
    d.variable_upper[0]=0; d.row_lower[0]=infinity;
    REQUIRE_FALSE(verify_infeasibility(m,d).passed);
    REQUIRE_FALSE(verify_unboundedness(m,std::vector<double>{2},std::vector<double>{infinity}).passed);
    m.objective.clear(); REQUIRE_FALSE(verify_primal(m,std::vector<double>{2},11).passed);
}
TEST_CASE("Empty infeasible row admits a Farkas certificate", "[verification]") {
    Model m; m.matrix=CscMatrix::from_triplets(1,0,{}); m.constraints={{"impossible",1,infinity}};
    REQUIRE(verify_infeasibility(m,DualCertificate{{1},{0},{},{}}).passed);
    REQUIRE_FALSE(verify_infeasibility(m,DualCertificate{{0},{0},{},{}}).passed);
}
TEST_CASE("Option limits allow immediate stop and reject nonfinite values", "[verification]") {
    SolverOptions o; o.node_limit=0; o.iteration_limit=0; o.time_limit_seconds=0;
    REQUIRE_NOTHROW(validate_options(o));
    o.branching="pseudocost"; REQUIRE_NOTHROW(validate_options(o));
    o.branching="strong"; REQUIRE_NOTHROW(validate_options(o));
    o.strong_branching_candidates=0; REQUIRE_THROWS(validate_options(o));
    o.strong_branching_candidates=5;o.feasibility_pump_passes=0;REQUIRE_THROWS(validate_options(o));
    o.time_limit_seconds=infinity; REQUIRE_THROWS(validate_options(o)); o={};
    o.tolerances.dual=std::numeric_limits<double>::quiet_NaN(); REQUIRE_THROWS(validate_options(o));
    for(auto s:{SolveStatus::infeasible,SolveStatus::unbounded,SolveStatus::time_limit,SolveStatus::node_limit,SolveStatus::iteration_limit,SolveStatus::numerical_failure}) REQUIRE(status_name(s)!="unknown");
}
