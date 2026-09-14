#include <catch2/catch_test_macros.hpp>
#include <sovereign/backend.hpp>

using namespace sovereign;

namespace {
Model bounded_example() {
    Model m;
    m.name = "bounded";
    m.variables = {{"x", 0, 2}, {"y", -1, 3}};
    m.constraints = {{"sum", 1, 1}, {"difference", -infinity, 2}};
    m.matrix = CscMatrix::from_triplets(2, 2, {{0,0,1}, {1,0,2}, {0,1,1}, {1,1,-1}});
    m.objective = {1, 2};
    return m;
}
}

TEST_CASE("CPU backend executes sparse products against hand-derived literals", "[backend]") {
    CpuBackend cpu;
    REQUIRE(cpu.capabilities().available);
    const auto m = bounded_example();
    const auto y = cpu.multiply(m.matrix, std::vector<double>{2, 3});
    REQUIRE(y == std::vector<double>{5, 1});
    const auto z = cpu.transpose_multiply(m.matrix, std::vector<double>{4, -2});
    REQUIRE(z == std::vector<double>{0, 6});
}

TEST_CASE("First-order CPU workload projects variables and reduces row violation", "[backend]") {
    CpuBackend cpu;
    FirstOrderOptions options;
    options.iteration_limit = 4000;
    options.primal_step = 0.03;
    options.dual_step = 0.03;
    const auto result = cpu.solve_relaxation(bounded_example(), options);
    REQUIRE(result.executed);
    REQUIRE_FALSE(result.authoritative);
    REQUIRE(result.primal.size() == 2);
    REQUIRE(result.primal[0] >= 0);
    REQUIRE(result.primal[0] <= 2);
    REQUIRE(result.primal[1] >= -1);
    REQUIRE(result.primal[1] <= 3);
    REQUIRE(result.telemetry.final_primal_residual <= result.telemetry.initial_primal_residual);
    REQUIRE(result.telemetry.wall_seconds >= 0);
    REQUIRE(result.telemetry.cpu_seconds >= 0);
}

TEST_CASE("Unavailable CUDA dispatch is truthful and falls back only when requested", "[backend]") {
    CudaBackend cuda;
#ifndef SOVEREIGN_ENABLE_CUDA
    REQUIRE_FALSE(cuda.capabilities().available);
    const auto unavailable = dispatch_relaxation(bounded_example(), BackendKind::cuda, {}, false);
    REQUIRE_FALSE(unavailable.executed);
    REQUIRE(unavailable.backend == BackendKind::cuda);
    const auto fallback = dispatch_relaxation(bounded_example(), BackendKind::cuda, {}, true);
    REQUIRE(fallback.executed);
    REQUIRE(fallback.backend == BackendKind::cpu);
    REQUIRE_FALSE(fallback.authoritative);
#endif
}
