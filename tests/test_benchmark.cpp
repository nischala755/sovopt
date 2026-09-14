#include <catch2/catch_test_macros.hpp>
#include <sovereign/benchmark.hpp>
#include <sovereign/generators.hpp>

using namespace sovereign;

TEST_CASE("Adaptive policy requires measured GPU capability and amortization", "[benchmark]") {
    const auto fp = fingerprint(generate_power_model(9, 3));
    BackendMeasurements absent;
    REQUIRE(select_backend(fp, 1000, absent) == BackendKind::cpu);
    BackendMeasurements measured;
    measured.gpu_available = true;
    measured.cpu_nonzeros_per_second = 1e6;
    measured.gpu_nonzeros_per_second = 1e12;
    measured.transfer_bytes_per_second = 1e12;
    REQUIRE(select_backend(fp, 1000, measured) == BackendKind::cuda);
    measured.transfer_bytes_per_second = 1;
    REQUIRE(select_backend(fp, 1000, measured) == BackendKind::cpu);
}

TEST_CASE("CPU benchmark records real nonnegative timing and serializes JSON and CSV", "[benchmark]") {
    BenchmarkOptions options;
    options.mode = BenchmarkMode::static_cpu;
    options.warmup_runs = 0;
    options.repetitions = 2;
    options.first_order.iteration_limit = 20;
    const auto report = benchmark(generate_logistics_model(5, 2), options);
    REQUIRE(report.records.size() == 2);
    for (const auto& record : report.records) {
        REQUIRE(record.executed);
        REQUIRE(record.backend == BackendKind::cpu);
        REQUIRE(record.wall_seconds >= 0);
        REQUIRE(record.cpu_seconds >= 0);
        REQUIRE_FALSE(record.gpu_kernel_seconds.has_value());
        REQUIRE_FALSE(record.transfer_seconds.has_value());
    }
    const auto json = benchmark_json(report);
    const auto csv = benchmark_csv(report);
    REQUIRE(json.find("\"records\"") != std::string::npos);
    REQUIRE(json.find("gpu_kernel_seconds\":null") != std::string::npos);
    REQUIRE(csv.find("backend,executed") != std::string::npos);
    REQUIRE(csv.find("cpu,true") != std::string::npos);
}

TEST_CASE("GPU benchmark reports unavailable without invented measurements", "[benchmark]") {
#ifndef SOVEREIGN_ENABLE_CUDA
    BenchmarkOptions options;
    options.mode = BenchmarkMode::static_cuda;
    options.repetitions = 1;
    const auto report = benchmark(generate_refinery_model(1, 1), options);
    REQUIRE(report.records.size() == 1);
    REQUIRE_FALSE(report.records[0].executed);
    REQUIRE_FALSE(report.records[0].gpu_kernel_seconds.has_value());
    REQUIRE_FALSE(report.records[0].transfer_seconds.has_value());
#endif
}
