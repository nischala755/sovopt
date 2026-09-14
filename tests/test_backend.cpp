#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
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

#ifdef SOVEREIGN_ENABLE_CUDA
TEST_CASE("CUDA sparse products agree with the independent CPU implementation", "[backend][cuda]") {
    const auto info=backend(BackendKind::cuda).capabilities();
    if(!info.available){SUCCEED("CUDA was compiled but no runtime device is available");return;}
    const auto matrix=CscMatrix::from_triplets(3,4,{{0,0,2},{2,0,-1},{1,1,3},{0,3,.5},{2,3,4}});
    const std::vector<double>x={1,-2,7,.25},y={.5,-1,2};
    const auto cpu_ax=backend(BackendKind::cpu).multiply(matrix,x),gpu_ax=backend(BackendKind::cuda).multiply(matrix,x);
    const auto cpu_at=backend(BackendKind::cpu).transpose_multiply(matrix,y),gpu_at=backend(BackendKind::cuda).transpose_multiply(matrix,y);
    REQUIRE(cpu_ax.size()==gpu_ax.size());REQUIRE(cpu_at.size()==gpu_at.size());
    for(Index i=0;i<cpu_ax.size();++i)REQUIRE(gpu_ax[i]==Catch::Approx(cpu_ax[i]).margin(1e-12));
    for(Index i=0;i<cpu_at.size();++i)REQUIRE(gpu_at[i]==Catch::Approx(cpu_at[i]).margin(1e-12));
}
#endif
