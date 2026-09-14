# Benchmark methodology

`benchmark` runs the same experimental first-order workload using static CPU,
static CUDA, or adaptive dispatch. Warmups are excluded from records. Each
record contains a fresh `steady_clock` wall duration and `std::clock` process CPU
duration around the complete dispatch. Repetitions, warmups, backend policy, and
first-order options are explicit inputs.

JSON includes the stable model hash and measurable dimensions alongside every
sample. CSV provides the same per-sample timing and residual fields. Missing GPU
kernel and transfer measurements serialize as JSON `null` and empty CSV fields.
An unavailable static CUDA request remains an unavailable record; it does not
silently become a CPU timing. Adaptive mode may choose CPU when capability or
throughput measurements are absent, invalid, or cannot amortize transfer cost.

The refinery, power, and logistics generators accept a fixed 64-bit seed and a
positive scale. They construct canonical `Model` instances with real sparse
coefficients, variable and row bounds, objectives, and domain-specific names.
Repeat a benchmark using the same seed, scale, build type, options, and host.

Report raw repetitions rather than only the fastest sample. For performance
comparisons, use a Release build, isolate the machine from competing work, state
the CPU and memory configuration, and summarize the median plus dispersion from
enough repetitions. Treat very short samples as timer-noise dominated. These
benchmarks characterize numerical execution only; their residuals and objective
samples are not solver correctness or optimality results.
