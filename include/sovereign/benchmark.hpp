#pragma once
#include <sovereign/backend.hpp>
#include <sovereign/fingerprint.hpp>
#include <optional>

namespace sovereign {
enum class BenchmarkMode { static_cpu, static_cuda, adaptive };

struct BackendMeasurements {
    bool gpu_available = false;
    double cpu_nonzeros_per_second = 0;
    double gpu_nonzeros_per_second = 0;
    double transfer_bytes_per_second = 0;
};

struct BenchmarkOptions {
    BenchmarkMode mode = BenchmarkMode::adaptive;
    Index warmup_runs = 1;
    Index repetitions = 5;
    FirstOrderOptions first_order;
    BackendMeasurements measurements;
};

struct BenchmarkRecord {
    Index repetition = 0;
    BackendKind backend = BackendKind::cpu;
    bool executed = false;
    bool converged = false;
    double wall_seconds = 0;
    double cpu_seconds = 0;
    std::optional<double> gpu_kernel_seconds;
    std::optional<double> transfer_seconds;
    Index iterations = 0;
    double primal_residual = infinity;
    std::string message;
};

struct BenchmarkReport {
    ModelFingerprint model;
    BenchmarkMode mode = BenchmarkMode::adaptive;
    std::vector<BenchmarkRecord> records;
};

[[nodiscard]] BackendKind select_backend(const ModelFingerprint&, Index iterations,
                                         const BackendMeasurements&);
[[nodiscard]] BenchmarkReport benchmark(const Model&, const BenchmarkOptions& = {});
[[nodiscard]] std::string benchmark_json(const BenchmarkReport&);
[[nodiscard]] std::string benchmark_csv(const BenchmarkReport&);
}
