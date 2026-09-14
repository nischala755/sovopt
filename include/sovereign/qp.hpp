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
    std::vector<double> primal; double objective=infinity; Index iterations=0;
    double runtime_seconds=0; QpVerificationReport verification;
    DualCertificate certificate;
};
[[nodiscard]] QpResult solve_qp(const QuadraticModel&, const SolverOptions& = {});
[[nodiscard]] QpResult solve_interior_point(const Model&, const SolverOptions& = {});
[[nodiscard]] QpVerificationReport verify_qp_optimality(
    const QuadraticModel&, std::span<const double>, double,
    const DualCertificate&, const Tolerances& = {});
}
