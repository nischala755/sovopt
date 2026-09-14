#include <sovereign/backend.hpp>
#include <sovereign/errors.hpp>
#include <sovereign/validation.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace sovereign {
#ifdef SOVEREIGN_ENABLE_CUDA
namespace cuda_detail {
BackendCapabilities capabilities();
std::vector<double> multiply(const CscMatrix&, std::span<const double>, bool transpose);
NumericalResult solve_relaxation(const Model&, const FirstOrderOptions&);
}
#endif
namespace {
double elapsed_cpu(std::clock_t start) { return static_cast<double>(std::clock() - start) / CLOCKS_PER_SEC; }

double residual(const Model& m, std::span<const double> x) {
    const auto activity = m.matrix.multiply(x);
    double result = 0;
    for (Index i = 0; i < activity.size(); ++i) {
        if (std::isfinite(m.constraints[i].lower)) result = std::max(result, m.constraints[i].lower - activity[i]);
        if (std::isfinite(m.constraints[i].upper)) result = std::max(result, activity[i] - m.constraints[i].upper);
    }
    for (Index j = 0; j < x.size(); ++j) {
        if (std::isfinite(m.variables[j].lower)) result = std::max(result, m.variables[j].lower - x[j]);
        if (std::isfinite(m.variables[j].upper)) result = std::max(result, x[j] - m.variables[j].upper);
    }
    return std::max(0.0, result);
}

void check_options(const FirstOrderOptions& o) {
    if (o.iteration_limit == 0 || !std::isfinite(o.primal_step) || o.primal_step <= 0 ||
        !std::isfinite(o.dual_step) || o.dual_step <= 0 || !std::isfinite(o.tolerance) || o.tolerance < 0)
        throw InvalidModelError("invalid first-order options");
}

[[noreturn]] void cuda_unavailable() { throw InvalidModelError("CUDA backend is unavailable in this build"); }
}

std::string_view backend_name(BackendKind kind) noexcept { return kind == BackendKind::cpu ? "cpu" : "cuda"; }

BackendCapabilities CpuBackend::capabilities() const {
    const auto workers = std::max(1u, std::thread::hardware_concurrency());
    return {BackendKind::cpu, "CPU", true, true, workers, 0, "native sparse CSC execution"};
}
std::vector<double> CpuBackend::multiply(const CscMatrix& a, std::span<const double> x) const { return a.multiply(x); }
std::vector<double> CpuBackend::transpose_multiply(const CscMatrix& a, std::span<const double> x) const { return a.transpose_multiply(x); }

NumericalResult CpuBackend::solve_relaxation(const Model& m, const FirstOrderOptions& o) const {
    require_valid(m); check_options(o);
    const auto wall_start = std::chrono::steady_clock::now();
    const auto cpu_start = std::clock();
    const Index n = m.variables.size(), rows = m.constraints.size();
    std::vector<double> x(n, 0), average(n, 0), lower_dual(rows, 0), upper_dual(rows, 0);
    for (Index j = 0; j < n; ++j) {
        if (std::isfinite(m.variables[j].lower)) x[j] = std::max(x[j], m.variables[j].lower);
        if (std::isfinite(m.variables[j].upper)) x[j] = std::min(x[j], m.variables[j].upper);
    }
    NumericalResult result;
    result.backend = BackendKind::cpu; result.executed = true;
    result.telemetry.initial_primal_residual = residual(m, x);
    const double sense = m.sense == ObjectiveSense::minimize ? 1.0 : -1.0;
    double stationarity = infinity;
    for (Index iteration = 0; iteration < o.iteration_limit; ++iteration) {
        const auto activity = m.matrix.multiply(x);
        for (Index i = 0; i < rows; ++i) {
            if (std::isfinite(m.constraints[i].lower))
                lower_dual[i] = std::max(0.0, lower_dual[i] + o.dual_step * (m.constraints[i].lower - activity[i]));
            if (std::isfinite(m.constraints[i].upper))
                upper_dual[i] = std::max(0.0, upper_dual[i] + o.dual_step * (activity[i] - m.constraints[i].upper));
        }
        std::vector<double> signed_dual(rows);
        for (Index i = 0; i < rows; ++i) signed_dual[i] = upper_dual[i] - lower_dual[i];
        auto gradient = m.matrix.transpose_multiply(signed_dual);
        stationarity = 0;
        const double diminishing = o.primal_step / std::sqrt(1.0 + static_cast<double>(iteration));
        for (Index j = 0; j < n; ++j) {
            gradient[j] += sense * m.objective[j];
            stationarity = std::max(stationarity, std::abs(gradient[j]));
            x[j] -= diminishing * gradient[j];
            if (std::isfinite(m.variables[j].lower)) x[j] = std::max(x[j], m.variables[j].lower);
            if (std::isfinite(m.variables[j].upper)) x[j] = std::min(x[j], m.variables[j].upper);
            average[j] += x[j];
        }
        result.telemetry.iterations = iteration + 1;
        if (residual(m, x) <= o.tolerance && stationarity <= o.tolerance) { result.converged = true; break; }
    }
    if (o.average_iterates && result.telemetry.iterations)
        for (Index j = 0; j < n; ++j) average[j] /= static_cast<double>(result.telemetry.iterations);
    else average = x;
    // Use the better feasible iterate for telemetry; both came from real iterations.
    if (residual(m, x) < residual(m, average)) average = x;
    result.primal = std::move(average);
    result.row_activity = m.matrix.multiply(result.primal);
    result.telemetry.final_primal_residual = residual(m, result.primal);
    result.telemetry.stationarity_residual = stationarity;
    result.telemetry.objective = m.objective_offset;
    for (Index j = 0; j < n; ++j) result.telemetry.objective += m.objective[j] * result.primal[j];
    result.telemetry.wall_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall_start).count();
    result.telemetry.cpu_seconds = elapsed_cpu(cpu_start);
    result.message = result.converged ? "experimental iteration tolerance reached" : "experimental iteration limit reached";
    return result;
}

BackendCapabilities CudaBackend::capabilities() const {
#ifdef SOVEREIGN_ENABLE_CUDA
    return cuda_detail::capabilities();
#else
    return {BackendKind::cuda, "CUDA", false, false, 0, 0, "configure with SOVEREIGN_ENABLE_CUDA=ON and a CUDA toolkit"};
#endif
}
std::vector<double> CudaBackend::multiply(const CscMatrix& a, std::span<const double> x) const {
#ifdef SOVEREIGN_ENABLE_CUDA
    return cuda_detail::multiply(a, x, false);
#else
    (void)a; (void)x; cuda_unavailable();
#endif
}
std::vector<double> CudaBackend::transpose_multiply(const CscMatrix& a, std::span<const double> x) const {
#ifdef SOVEREIGN_ENABLE_CUDA
    return cuda_detail::multiply(a, x, true);
#else
    (void)a; (void)x; cuda_unavailable();
#endif
}
NumericalResult CudaBackend::solve_relaxation(const Model& m, const FirstOrderOptions& options) const {
#ifdef SOVEREIGN_ENABLE_CUDA
    if (capabilities().available) return cuda_detail::solve_relaxation(m, options);
#else
    (void)m; (void)options;
#endif
    NumericalResult result; result.backend = BackendKind::cuda; result.message = capabilities().detail; return result;
}

const ExecutionBackend& backend(BackendKind kind) {
    static const CpuBackend cpu;
    static const CudaBackend cuda;
    return kind == BackendKind::cpu ? static_cast<const ExecutionBackend&>(cpu) : static_cast<const ExecutionBackend&>(cuda);
}
NumericalResult dispatch_relaxation(const Model& m, BackendKind kind, const FirstOrderOptions& options, bool fallback) {
    const auto& selected = backend(kind);
    if (selected.capabilities().available) return selected.solve_relaxation(m, options);
    if (kind == BackendKind::cuda && fallback) {
        auto result = backend(BackendKind::cpu).solve_relaxation(m, options);
        result.message = "CUDA unavailable; CPU fallback executed; " + result.message;
        return result;
    }
    return selected.solve_relaxation(m, options);
}
}
