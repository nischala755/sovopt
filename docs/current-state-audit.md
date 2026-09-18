# Current-state audit

Audit date: 2026-09-14. Baseline commit: `778fd77`. The audit is based on
source inspection and an untouched Release test run: 93/93 CTest cases, 7/7
Python tests, 2/2 web tests, and a clean TypeScript build.

Status meanings: **implemented** means executable behavior and tests exist;
**partial** means useful behavior exists but does not meet the full requirement;
**missing** means no production implementation was found; **blocked** means the
code exists but this host cannot validate it.

| Requirement | Existing implementation | Location | Status | Gap |
|---|---|---|---|---|
| C++ solver | C++20 core library and CLI executable | `CMakeLists.txt`, `src/` | Implemented | Industrial scale is not established |
| Sparse matrix | Canonical immutable CSC, products and transpose products | `include/sovereign/sparse_matrix.hpp`, `src/sparse_matrix.cpp` | Implemented | No parallel SpMV in authoritative solver |
| Model representation | Two-sided rows, bounds, objective sense, integer domains | `include/sovereign/model.hpp` | Implemented | No quadratic objective structure |
| MPS/QPS parser | Strict free/fixed reader with resource limits and triangular `QUADOBJ` support | `src/mps.cpp` | Implemented | Other vendor quadratic sections are unsupported |
| LP | Two-phase primal revised simplex with Harris ratio selection, refinement and original-space verification | `src/lp.cpp`, `src/lp_standard.cpp` | Partial | Dual simplex and sparse update schemes are absent |
| MILP | Deterministic best-bound branch-and-bound over verified LP relaxations | `src/mip.cpp` | Partial | Single-threaded; limited cuts and primal heuristic |
| QP | Convex quadratic model, QPS `QUADOBJ` import and infeasible-start primal-dual predictor-corrector method | `include/sovereign/qp.hpp`, `src/qp.cpp` | Implemented for small convex QPs | Sparse KKT factorization is present, but canonicalization and PSD validation remain dense |
| Presolve | Fixed substitution, constant rows, singleton tightening, reconstruction | `src/presolve.cpp` | Partial | Duplicate rows, general implied bounds, aggregation and richer postsolve are absent |
| Scaling | Row/column scaling in LP standard-form conversion | `src/lp_standard.cpp` | Partial | No iterative equilibration report or condition metrics |
| Numerical tolerances | Explicit primal, dual, integrality and pivot tolerances | `include/sovereign/solution.hpp` | Implemented | No method-specific stability policy |
| Certificates | Original-coordinate dual/Farkas/ray structures | `include/sovereign/solution.hpp`, `src/lp.cpp` | Partial | SC50A dual reconstruction currently fails strict postsolve verification |
| Independent verifier | Primal, dual optimality, infeasibility and recession checks | `src/verification.cpp` | Implemented | Binary64 numerical verification, not exact proof; MILP global bound is solver-owned |
| Branch-and-bound | LP relaxation at every node with verified pruning | `src/mip.cpp` | Implemented | No restart or tree persistence |
| Branching | Most-fractional and learned pseudo-cost | `src/mip.cpp` | Implemented | No strong branching |
| Node selection | Deterministic best-bound queue with ID tie-break | `src/mip.cpp` | Implemented | No selectable depth/hybrid policy |
| Cuts | Safe integer-row bound strengthening | `src/cuts.cpp` | Partial | No Gomory, MIR, cover or clique separators |
| Heuristics | Rounding followed by fixed-integer LP repair | `src/mip.cpp` | Partial | No diving, feasibility pump or local search |
| MIP gap | Normalized bound/incumbent gap and termination | `src/mip.cpp` | Implemented | Depends on single-threaded tree processing |
| Parallelism | CPU capability reports hardware concurrency | `src/backend.cpp` | Missing | Authoritative LP/MILP algorithms are single-threaded |
| CPU backend | CSC products and experimental primal-dual workload | `src/backend.cpp` | Implemented | First-order result is deliberately non-authoritative |
| CUDA backend | Optional real CSC kernels, device probing, CPU cross-checks, benchmark probe and evidence script | `src/cuda_backend.cu`, `tools/cuda_probe.cpp`, `scripts/cuda-validate.ps1` | Prepared; measurement blocked | No CUDA compiler/device is available on this host |
| Adaptive execution | Measurement-based CPU/CUDA selection for experimental workload | `src/benchmark.cpp` | Partial | Does not select authoritative simplex/MILP execution strategies |
| Model fingerprint | Stable endian-independent hash and structural metrics | `src/fingerprint.cpp` | Implemented | Existing hash is 64-bit, not cryptographic |
| Strategy engine | Backend selection based on measured compute and transfer cost | `src/benchmark.cpp` | Partial | No LP algorithm selector |
| Telemetry | Structured LP/MILP callbacks and backend timings | `include/sovereign/solution.hpp`, `src/lp.cpp`, `src/mip.cpp` | Implemented | No persistent solve timeline or factorization/refinement events |
| CLI | Inspect, validate, solve and benchmark commands | `src/cli.cpp` | Implemented | Record/replay commands are absent |
| Python API | Native pybind11 interface | `python/bindings.cpp` | Implemented | No recorder API |
| REST API | FastAPI models, asynchronous solves, telemetry, verification and benchmarks | `service/app.py`, `service/store.py` | Implemented | No bundle download/replay endpoints |
| Dashboard | React engineering console using actual API data | `web/src/` | Implemented | No persistent timeline, bundle integrity or replay view |
| Mistral integration | Server-side optional explanations and confirmation-gated formulations | `service/ai.py`, `service/app.py` | Implemented | Live call is currently rate/quota limited; AI remains optional |
| Netlib | Automated AFIRO, SC50A and SC50B manifest and verified regressions | `benchmarks/manifests/netlib-small.json`, `tests/test_lp.cpp` | Implemented | Broader coverage remains useful |
| MIPLIB | Checksum-pinned flugpl manifest and measured failed run | `benchmarks/manifests/miplib-small.json` | Partial | Solver reaches iteration limit |
| Reference solver harness | Isolated exact-command adapter | `benchmarks/baselines/run_reference.py` | Partial | No reference executable installed on this host |
| Result serialization | CLI JSON plus first-order benchmark JSON/CSV | `src/cli.cpp`, `src/benchmark.cpp` | Partial | No versioned complete solve artifact |
| Industrial models | Seeded refinery, crude blending, process, production, power, logistics and supply-chain generators | `src/generators.cpp` | Implemented as demonstrators | Industrial data sets and industrial-scale performance validation remain absent |
| Optimization Flight Recorder | Versioned LP/MILP/QP artifacts, SHA-256, replay, reverify, REST download and UI timeline | `src/recorder.cpp`, `service/app.py`, `web/src/components.tsx` | Implemented | SHA-256 integrity is not a digital signature |

## Existing public interfaces to preserve

- `solve_lp(const Model&, const SolverOptions&)` and
  `solve_mip(const Model&, const SolverOptions&)` return `SolveResult`.
- `SparseBasis` constructs from a `CscMatrix` and basis-column span, and exposes
  `solve`, `solve_transpose`, `dimension`, and `nonzeros`.
- `SolverOptions::telemetry` is the structured event integration point.
- `read_mps[_file]`, `fingerprint`, `benchmark`, and the backend abstract class
  are independent modules and remain source compatible.
- `run_cli` owns command dispatch; `service.engine` is the Python/native boundary;
  the dashboard consumes only REST responses.

## Baseline conclusion

The repository contains a real solver and product stack and must be extended in
place. The highest-impact verified gaps are numerical refinement and ratio-test
safeguards, reproducible benchmark automation, and portable solve evidence. QP,
parallel authoritative solving, broad cuts, and large-scale performance remain
separate substantial milestones rather than documentation changes.
