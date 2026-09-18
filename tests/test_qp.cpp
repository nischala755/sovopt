#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <sovereign/qp.hpp>
using namespace sovereign; using Catch::Approx;
namespace { QuadraticModel unconstrained(double q,double c) { QuadraticModel p; p.linear.name="quadratic"; p.linear.variables={{"x",-infinity,infinity}}; p.linear.objective={c}; p.linear.matrix=CscMatrix::from_triplets(0,1,{}); p.quadratic=CscMatrix::from_triplets(1,1,{{0,0,q}}); return p; } }
TEST_CASE("interior point solves a hand-verifiable unconstrained convex QP","[qp]") { const auto r=solve_qp(unconstrained(2,-4)); INFO(r.message); REQUIRE(r.status==SolveStatus::optimal); REQUIRE(r.primal[0]==Approx(2).margin(1e-7)); REQUIRE(r.objective==Approx(-4).margin(1e-7)); REQUIRE(r.verification.passed); }
TEST_CASE("interior point respects variable and row bounds","[qp]") { auto p=unconstrained(2,-6); p.linear.variables[0].lower=0; p.linear.variables[0].upper=2; p.linear.constraints={{"floor",1.5,infinity}}; p.linear.matrix=CscMatrix::from_triplets(1,1,{{0,0,1}}); const auto r=solve_qp(p); INFO(r.message); REQUIRE(r.status==SolveStatus::optimal); REQUIRE(r.primal[0]==Approx(2).margin(2e-5)); REQUIRE(r.objective==Approx(-8).margin(2e-5)); REQUIRE(r.verification.passed); }
TEST_CASE("interior point LP path solves a bounded linear model","[qp][lp]") { Model m; m.variables={{"x",0,infinity},{"y",0,infinity}}; m.objective={-1,-2}; m.constraints={{"capacity",-infinity,4}}; m.matrix=CscMatrix::from_triplets(1,2,{{0,0,1},{0,1,1}}); const auto r=solve_interior_point(m); INFO(r.message); REQUIRE(r.status==SolveStatus::optimal); REQUIRE(r.objective==Approx(-8).margin(2e-5)); REQUIRE(r.verification.passed); }
TEST_CASE("QP rejects a nonsymmetric or nonconvex Hessian","[qp]") { auto p=unconstrained(-1,0); REQUIRE(solve_qp(p).status==SolveStatus::numerical_failure); p.linear.variables.push_back({"y",-infinity,infinity}); p.linear.objective.push_back(0); p.linear.matrix=CscMatrix::from_triplets(0,2,{}); p.quadratic=CscMatrix::from_triplets(2,2,{{0,1,1}}); REQUIRE(solve_qp(p).status==SolveStatus::numerical_failure); }
TEST_CASE("interior point handles a concave maximization QP","[qp]") { auto p=unconstrained(-2,4); p.linear.sense=ObjectiveSense::maximize; const auto r=solve_qp(p); INFO(r.message); REQUIRE(r.status==SolveStatus::optimal); REQUIRE(r.primal[0]==Approx(2).margin(1e-7)); REQUIRE(r.objective==Approx(4).margin(1e-7)); REQUIRE(r.verification.passed); }
TEST_CASE("QP certificate is independently reverified and corruption is rejected","[qp][verification]") { auto p=unconstrained(2,-6);p.linear.variables[0].lower=0;p.linear.variables[0].upper=2;const auto solved=solve_qp(p);REQUIRE(solved.status==SolveStatus::optimal);REQUIRE(verify_qp_optimality(p,solved.primal,solved.objective,solved.certificate).passed);auto bad=solved.certificate;bad.variable_upper[0]+=1;REQUIRE_FALSE(verify_qp_optimality(p,solved.primal,solved.objective,bad).passed); }
TEST_CASE("interior point factors diagonal KKT systems through sparse storage","[qp][sparse-kkt]") {
 constexpr Index n=256;QuadraticModel p;p.linear.variables.reserve(n);p.linear.objective.assign(n,-2);std::vector<Triplet> diagonal;
 for(Index j=0;j<n;++j){p.linear.variables.push_back({"x"+std::to_string(j),-infinity,infinity});diagonal.push_back({j,j,2});}
 p.linear.matrix=CscMatrix::from_triplets(0,n,{});p.quadratic=CscMatrix::from_triplets(n,n,std::move(diagonal));
 const auto r=solve_qp(p);INFO(r.message);REQUIRE(r.status==SolveStatus::optimal);REQUIRE(r.verification.passed);
 REQUIRE(r.kkt_factorizations>=1);REQUIRE(r.max_kkt_nonzeros==n);for(double x:r.primal)REQUIRE(x==Approx(1).margin(1e-7));
}
TEST_CASE("QP returns independently checked infeasibility and unboundedness certificates","[qp][certificates]") {
 auto infeasible=unconstrained(2,0);infeasible.linear.constraints={{"lower",1,infinity},{"upper",-infinity,0}};infeasible.linear.matrix=CscMatrix::from_triplets(2,1,{{0,0,1},{1,0,1}});
 auto r=solve_qp(infeasible);INFO(r.message);REQUIRE(r.status==SolveStatus::infeasible);REQUIRE(r.certificate_verification.passed);
 auto unbounded=unconstrained(0,-1);r=solve_qp(unbounded);INFO(r.message);REQUIRE(r.status==SolveStatus::unbounded);REQUIRE_FALSE(r.ray.empty());REQUIRE(r.verification.passed);
 REQUIRE(verify_qp_unboundedness(unbounded,r.primal,r.ray).passed);auto corrupted=r.ray;corrupted[0]=0;REQUIRE_FALSE(verify_qp_unboundedness(unbounded,r.primal,corrupted).passed);
}
