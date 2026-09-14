#pragma once
#include <sovereign/model.hpp>
#include <functional>
#include <string_view>

namespace sovereign {
enum class SolveStatus { optimal, infeasible, unbounded, time_limit, node_limit, iteration_limit, numerical_failure, cancelled };
struct Tolerances {
    double primal = 1e-7;
    double dual = 1e-7;
    double integrality = 1e-7;
    double pivot = 1e-12;
};
struct TelemetryEvent {
    std::string type;
    double elapsed_seconds = 0;
    Index iterations = 0, nodes = 0;
    double objective = infinity, best_bound = -infinity, mip_gap = infinity;
    std::string detail;
};
using TelemetryCallback = std::function<void(const TelemetryEvent&)>;
struct SolverOptions {
    Tolerances tolerances;
    Index iteration_limit = 100000;
    Index node_limit = 10000;
    double time_limit_seconds = 300;
    double mip_gap = 0;
    bool scaling = true;
    bool presolve = true;
    bool deterministic = true;
    bool cuts = true;
    bool rounding = true;
    std::string branching = "most_fractional";
    std::string method = "auto";
    TelemetryCallback telemetry;
    std::function<bool()> cancelled;
};
// All multipliers are nonnegative for the original model's normalized MIN objective.
// Stationarity: sign*c - A^T(row_lower-row_upper) - var_lower + var_upper = 0.
// Farkas uses zero objective in that identity and strictly positive bound sum.
struct DualCertificate {
    std::vector<double> row_lower, row_upper, variable_lower, variable_upper;
};
struct VerificationReport {
    bool passed = false;
    double objective = 0;
    double primal_residual = infinity, dual_residual = infinity;
    double objective_error = infinity, integrality_residual = 0;
    double dual_bound = -infinity, duality_gap = infinity;
    std::vector<std::string> violations;
};
struct SolveResult {
    SolveStatus status = SolveStatus::numerical_failure;
    std::string message;
    std::vector<double> primal, ray;
    DualCertificate certificate;
    VerificationReport verification;
    double objective = infinity, best_bound = -infinity, mip_gap = infinity;
    double runtime_seconds = 0, cpu_seconds = 0, gpu_kernel_seconds = 0, transfer_seconds = 0;
    Index iterations = 0, nodes = 0, nodes_generated = 0, incumbent_updates = 0, cuts_added = 0;
};
[[nodiscard]] std::string_view status_name(SolveStatus status);
void validate_options(const SolverOptions& options);
}
