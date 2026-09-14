#pragma once
#include <sovereign/model.hpp>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace sovereign {
enum class BackendKind { cpu, cuda };

struct BackendCapabilities {
    BackendKind kind = BackendKind::cpu;
    std::string name;
    bool compiled = false;
    bool available = false;
    Index workers = 0;
    std::size_t device_memory_bytes = 0;
    std::string detail;
};

struct FirstOrderOptions {
    Index iteration_limit = 1000;
    double primal_step = 0.01;
    double dual_step = 0.01;
    double tolerance = 1e-6;
    bool average_iterates = true;
};

struct NumericalTelemetry {
    Index iterations = 0;
    double initial_primal_residual = infinity;
    double final_primal_residual = infinity;
    double stationarity_residual = infinity;
    double objective = infinity;
    double wall_seconds = 0;
    double cpu_seconds = 0;
    std::optional<double> gpu_kernel_seconds;
    std::optional<double> transfer_seconds;
};

// An experimental numerical workload. It never claims a solver status or proof.
struct NumericalResult {
    BackendKind backend = BackendKind::cpu;
    bool executed = false;
    bool converged = false;
    bool authoritative = false;
    std::string message;
    std::vector<double> primal;
    std::vector<double> row_activity;
    NumericalTelemetry telemetry;
};

class ExecutionBackend {
public:
    virtual ~ExecutionBackend() = default;
    [[nodiscard]] virtual BackendCapabilities capabilities() const = 0;
    [[nodiscard]] virtual std::vector<double> multiply(const CscMatrix&, std::span<const double>) const = 0;
    [[nodiscard]] virtual std::vector<double> transpose_multiply(const CscMatrix&, std::span<const double>) const = 0;
    [[nodiscard]] virtual NumericalResult solve_relaxation(const Model&, const FirstOrderOptions& = {}) const = 0;
};

class CpuBackend final : public ExecutionBackend {
public:
    [[nodiscard]] BackendCapabilities capabilities() const override;
    [[nodiscard]] std::vector<double> multiply(const CscMatrix&, std::span<const double>) const override;
    [[nodiscard]] std::vector<double> transpose_multiply(const CscMatrix&, std::span<const double>) const override;
    [[nodiscard]] NumericalResult solve_relaxation(const Model&, const FirstOrderOptions& = {}) const override;
};

class CudaBackend final : public ExecutionBackend {
public:
    [[nodiscard]] BackendCapabilities capabilities() const override;
    [[nodiscard]] std::vector<double> multiply(const CscMatrix&, std::span<const double>) const override;
    [[nodiscard]] std::vector<double> transpose_multiply(const CscMatrix&, std::span<const double>) const override;
    [[nodiscard]] NumericalResult solve_relaxation(const Model&, const FirstOrderOptions& = {}) const override;
};

[[nodiscard]] const ExecutionBackend& backend(BackendKind kind);
[[nodiscard]] NumericalResult dispatch_relaxation(const Model&, BackendKind,
                                                   const FirstOrderOptions& = {}, bool allow_cpu_fallback = true);
[[nodiscard]] std::string_view backend_name(BackendKind) noexcept;
}
