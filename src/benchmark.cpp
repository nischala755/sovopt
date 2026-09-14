#include <sovereign/benchmark.hpp>
#include <sovereign/errors.hpp>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace sovereign {
namespace {
std::string mode_name(BenchmarkMode mode) {
    if (mode == BenchmarkMode::static_cpu) return "static_cpu";
    if (mode == BenchmarkMode::static_cuda) return "static_cuda";
    return "adaptive";
}
std::string escape(std::string_view input) {
    std::string out;
    for (unsigned char c : input) {
        switch (c) { case '\\': out += "\\\\"; break; case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break; case '\r': out += "\\r"; break; case '\t': out += "\\t"; break;
        default: if (c < 0x20) { std::ostringstream h; h << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c); out += h.str(); } else out += char(c); }
    }
    return out;
}
double cpu_since(std::clock_t start) { return static_cast<double>(std::clock()-start)/CLOCKS_PER_SEC; }
BenchmarkRecord execute(const Model& model, BackendKind kind, const FirstOrderOptions& options, Index repetition) {
    const auto wall = std::chrono::steady_clock::now(); const auto cpu = std::clock();
    const auto result = dispatch_relaxation(model, kind, options, false);
    BenchmarkRecord record; record.repetition = repetition; record.backend = kind; record.executed = result.executed;
    record.converged = result.converged; record.iterations = result.telemetry.iterations;
    record.primal_residual = result.telemetry.final_primal_residual; record.message = result.message;
    record.gpu_kernel_seconds = result.telemetry.gpu_kernel_seconds;
    record.transfer_seconds = result.telemetry.transfer_seconds;
    record.wall_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-wall).count();
    record.cpu_seconds = cpu_since(cpu);
    return record;
}
BackendMeasurements measure_backends(const Model& model) {
    BackendMeasurements measured;
    const auto gpu = backend(BackendKind::cuda).capabilities();
    measured.gpu_available = gpu.available;
    std::vector<double> x(model.variables.size(), 1.0);
    constexpr Index trials = 64;
    const auto start = std::chrono::steady_clock::now();
    double guard = 0;
    for (Index i = 0; i < trials; ++i) {
        const auto y = backend(BackendKind::cpu).multiply(model.matrix, x);
        if (!y.empty()) guard += y[i % y.size()];
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    if (seconds > 0 && std::isfinite(guard)) measured.cpu_nonzeros_per_second = static_cast<double>(model.matrix.nonzeros()*trials)/seconds;
    if (gpu.available) {
        FirstOrderOptions probe; probe.iteration_limit = 1;
        const auto result = backend(BackendKind::cuda).solve_relaxation(model, probe);
        if (result.telemetry.gpu_kernel_seconds && *result.telemetry.gpu_kernel_seconds > 0)
            measured.gpu_nonzeros_per_second = static_cast<double>(model.matrix.nonzeros()*3)/ *result.telemetry.gpu_kernel_seconds;
        if (result.telemetry.transfer_seconds && *result.telemetry.transfer_seconds > 0)
            measured.transfer_bytes_per_second = static_cast<double>(fingerprint(model).estimated_bytes*3)/ *result.telemetry.transfer_seconds;
    }
    return measured;
}
}

BackendKind select_backend(const ModelFingerprint& fp, Index iterations, const BackendMeasurements& m) {
    if (!m.gpu_available || !(m.cpu_nonzeros_per_second > 0) || !(m.gpu_nonzeros_per_second > 0) ||
        !(m.transfer_bytes_per_second > 0) || !std::isfinite(m.cpu_nonzeros_per_second) ||
        !std::isfinite(m.gpu_nonzeros_per_second) || !std::isfinite(m.transfer_bytes_per_second)) return BackendKind::cpu;
    const long double operations = static_cast<long double>(fp.nonzeros) * iterations * 2;
    const long double cpu = operations / m.cpu_nonzeros_per_second;
    const long double gpu = operations / m.gpu_nonzeros_per_second +
        static_cast<long double>(fp.estimated_bytes) * 2 / m.transfer_bytes_per_second;
    return gpu < cpu ? BackendKind::cuda : BackendKind::cpu;
}

BenchmarkReport benchmark(const Model& model, const BenchmarkOptions& options) {
    if (!options.repetitions) throw InvalidModelError("benchmark repetitions must be positive");
    BenchmarkReport report; report.model = fingerprint(model); report.mode = options.mode;
    BackendKind selected = BackendKind::cpu;
    if (options.mode == BenchmarkMode::static_cuda) selected = BackendKind::cuda;
    else if (options.mode == BenchmarkMode::adaptive) {
        auto measurements = options.measurements;
        if (!(measurements.cpu_nonzeros_per_second > 0)) measurements = measure_backends(model);
        selected = select_backend(report.model, options.first_order.iteration_limit, measurements);
        if (!backend(selected).capabilities().available) selected = BackendKind::cpu;
    }
    for (Index i = 0; i < options.warmup_runs; ++i) (void)execute(model, selected, options.first_order, i);
    report.records.reserve(options.repetitions);
    for (Index i = 0; i < options.repetitions; ++i) report.records.push_back(execute(model, selected, options.first_order, i));
    return report;
}

std::string benchmark_json(const BenchmarkReport& r) {
    std::ostringstream out; out << std::setprecision(17);
    out << "{\"model_hash\":\"" << r.model.hash_hex << "\",\"variables\":" << r.model.variables
        << ",\"constraints\":" << r.model.constraints << ",\"nonzeros\":" << r.model.nonzeros
        << ",\"mode\":\"" << mode_name(r.mode) << "\",\"records\":[";
    for (Index i = 0; i < r.records.size(); ++i) {
        const auto& x = r.records[i]; if (i) out << ',';
        out << "{\"repetition\":" << x.repetition << ",\"backend\":\"" << backend_name(x.backend)
            << "\",\"executed\":" << (x.executed?"true":"false") << ",\"converged\":" << (x.converged?"true":"false")
            << ",\"wall_seconds\":" << x.wall_seconds << ",\"cpu_seconds\":" << x.cpu_seconds
            << ",\"gpu_kernel_seconds\":"; if (x.gpu_kernel_seconds) out << *x.gpu_kernel_seconds; else out << "null";
        out << ",\"transfer_seconds\":"; if (x.transfer_seconds) out << *x.transfer_seconds; else out << "null";
        out << ",\"iterations\":" << x.iterations << ",\"primal_residual\":";
        if (std::isfinite(x.primal_residual)) out << x.primal_residual; else out << "null";
        out << ",\"message\":\"" << escape(x.message) << "\"}";
    }
    return out.str() + "]}";
}

std::string benchmark_csv(const BenchmarkReport& r) {
    std::ostringstream out; out << std::setprecision(17);
    out << "backend,executed,converged,repetition,wall_seconds,cpu_seconds,gpu_kernel_seconds,transfer_seconds,iterations,primal_residual,message\n";
    for (const auto& x : r.records) {
        out << backend_name(x.backend) << ',' << (x.executed?"true":"false") << ',' << (x.converged?"true":"false") << ','
            << x.repetition << ',' << x.wall_seconds << ',' << x.cpu_seconds << ',';
        if (x.gpu_kernel_seconds) out << *x.gpu_kernel_seconds;
        out << ',';
        if (x.transfer_seconds) out << *x.transfer_seconds;
        out << ',' << x.iterations << ',';
        if (std::isfinite(x.primal_residual)) out << x.primal_residual;
        out << ",\"" << escape(x.message) << "\"\n";
    }
    return out.str();
}
}
