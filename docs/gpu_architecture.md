# Execution backend architecture

The execution layer separates numerical experiments from the authoritative LP and
MILP solvers. `ExecutionBackend` provides sparse matrix-vector products and an
experimental first-order LP relaxation workload. `CpuBackend` uses the canonical
CSC matrix directly. `dispatch_relaxation` can explicitly fall back from an
unavailable CUDA request to CPU and records the backend that actually ran.

The first-order kernel applies projected primal-dual iterations to row lower and
upper bounds and projects every primal step onto variable bounds. It reports the
initial and final primal residual, stationarity residual, iteration count,
objective sample, wall time, and process CPU time. Its `authoritative` member is
always false. These iterates are useful for workload and architecture experiments;
they do not establish optimality, infeasibility, or unboundedness. Only the
existing independent verification path can validate solver certificates.

CUDA capability is unavailable unless `SOVEREIGN_ENABLE_CUDA` is defined. The
optional `src/cuda_backend.cu` contains real CSC matvec and transpose kernels,
runtime device probing, CUDA-event kernel timing, and measured host transfer
timing. The CUDA first-order workload dispatches those kernels each iteration.
The backend never runs CPU sparse work under a GPU label. GPU kernel and transfer
durations remain absent rather than zero or estimated whenever CUDA is unavailable.

Adaptive selection consumes measured CPU throughput, GPU throughput, transfer
bandwidth, model byte size, nonzero count, and requested iteration count. It
selects CUDA only when GPU availability is measured and the predicted compute
savings exceed two model transfers. Benchmark dispatch also checks the live
backend capability before execution and otherwise uses CPU.

To enable the optional source in CMake, use `check_language(CUDA)`, then, only
when a CUDA compiler is found, call `enable_language(CUDA)`, add
`src/cuda_backend.cu` to `sovereign_core`, and define
`SOVEREIGN_ENABLE_CUDA=1` for that target. Set CUDA C++20 and enable
separable compilation if required by the selected toolchain. Hardware results
must identify the device, driver, toolkit, and exact build; this CPU-only
environment cannot test or support GPU performance claims.

## NVIDIA validation pack

On a Windows NVIDIA machine with a CUDA toolkit, driver, and Visual Studio C++
toolchain installed, run from PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/cuda-validate.ps1
```

The script enables CUDA in a separate `build/cuda` tree, builds every target,
runs the full CTest suite, checks CUDA CSC products against the independent CPU
implementation, and runs static CPU, static GPU, and measured adaptive trials.
It writes device, driver, CUDA compiler, host, Git revision, kernel time,
transfer time, wall time, residuals, and raw repetition data under
`benchmark-results/cuda`. A numerical mismatch or kernel failure fails the run.

Use `-CudaArchitectures 86` (or the appropriate architecture) if `native` is
not supported by the installed CMake/CUDA combination. `-Scale` controls the
square three-band sparse probe size and defaults to 100,000 variables and about
300,000 nonzeros. This is an execution-kernel experiment; it does not claim an
authoritative LP optimum.
