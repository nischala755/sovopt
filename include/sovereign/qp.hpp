#pragma once
#include <sovereign/solution.hpp>

namespace sovereign {
struct QuadraticModel { Model linear; CscMatrix quadratic; };
struct QpVerificationReport {
    bool passed=false; double objective=infinity, primal_residual=infinity;
    double stationarity_residual=infinity, complementarity_residual=infinity;
    std::vector<std::string> violations;
};
struct QpResult {
    SolveStatus status=SolveStatus::numerical_failure; std::string message;
    std::vector<double> primal, ray; double objective=infinity; Index iterations=0;
    Index kkt_factorizations=0, max_kkt_nonzeros=0;
    double runtime_seconds=0; QpVerificationReport verification;
    DualCertificate certificate;
    VerificationReport certificate_verification;
};
[[nodiscard]] QpResult solve_qp(const QuadraticModel&, const SolverOptions& = {});
[[nodiscard]] QpResult solve_interior_point(const Model&, const SolverOptions& = {});
[[nodiscard]] QpVerificationReport verify_qp_optimality(
    const QuadraticModel&, std::span<const double>, double,
    const DualCertificate&, const Tolerances& = {});
[[nodiscard]] QpVerificationReport verify_qp_unboundedness(
    const QuadraticModel&, std::span<const double> feasible_point,
    std::span<const double> direction, const Tolerances& = {});
}
