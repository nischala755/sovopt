# Current-state audit

Audit updated: 2026-09-20. The current evidence baseline is maintained by the
Release CTest, Python, and web verification commands documented in the README.

Status meanings: **implemented** means executable behavior and tests exist;
**partial** means useful behavior exists but does not meet the full requirement;
**missing** means no production implementation was found; **blocked** means the
code exists but this host cannot validate it.

| Requirement | Existing implementation | Location | Status | Gap |
|---|---|---|---|---|
| C++ solver | C++20 core library and CLI executable | `CMakeLists.txt`, `src/` | Implemented | Industrial scale is not established |
| Sparse matrix | Canonical immutable CSC, products and transpose products | `include/sovereign/sparse_matrix.hpp`, `src/sparse_matrix.cpp` | Implemented | No parallel SpMV in authoritative solver |
| Model representation | Two-sided rows, bounds, linear/quadratic objectives and integer domains | `include/sovereign/model.hpp`, `include/sovereign/qp.hpp` | Implemented | MIQP is not implemented |
| MPS/QPS parser | Strict free/fixed reader with resource limits, descriptive Netlib NAME titles and triangular `QUADOBJ` support | `src/mps.cpp` | Implemented | Other vendor quadratic sections are unsupported |
| LP | Two-phase revised simplex, Harris ratio selection, refinement, product-form sparse basis updates and original-space verification | `src/lp.cpp`, `src/lp_standard.cpp`, `src/basis.cpp` | Partial | Standalone dual-simplex selection and stronger factor update schemes are absent |
| MILP | Deterministic best-bound branch-and-bound over verified LP relaxations | `src/mip.cpp` | Partial | Single-threaded; limited cuts and primal heuristic |
| QP | Convex quadratic model, QPS `QUADOBJ` import and infeasible-start primal-dual predictor-corrector method | `include/sovereign/qp.hpp`, `src/qp.cpp` | Implemented for small convex QPs | Canonicalization and PSD/KKT paths are sparse, but elimination ordering and fill control remain limited |
| Presolve | Fixed substitution, constant rows, singleton tightening, exact duplicate rows and reconstruction | `src/presolve.cpp` | Partial | Differently bounded parallel rows, aggregation and richer postsolve are absent |
| Scaling | Row/column scaling in LP standard-form conversion | `src/lp_standard.cpp` | Partial | No iterative equilibration report or condition metrics |
| Numerical tolerances | Explicit primal, dual, integrality and pivot tolerances | `include/sovereign/solution.hpp` | Implemented | No method-specific stability policy |
| Certificates | Original-coordinate dual/Farkas/ray structures | `include/sovereign/solution.hpp`, `src/lp.cpp` | Partial | SC50A dual reconstruction currently fails strict postsolve verification |
| Independent verifier | Primal, dual optimality, infeasibility and recession checks | `src/verification.cpp` | Implemented | Binary64 numerical verification, not exact proof; MILP global bound is solver-owned |
| Branch-and-bound | LP relaxation at every node with verified pruning | `src/mip.cpp` | Implemented | No restart or tree persistence |
| Branching | Most-fractional, learned pseudo-cost and bounded strong branching with real LP probes | `src/mip.cpp` | Implemented | Reliability on difficult MIPLIB remains incomplete |
| Node selection | Deterministic best-bound queue with ID tie-break | `src/mip.cpp` | Implemented | No selectable depth/hybrid policy |
| Cuts | Integer-row strengthening, row CG, conservative tableau GMI, binary cover and conflict-clique separation | `src/cuts.cpp`, `src/mip.cpp` | Partial | Multi-round cut pools, aging and broader MIR aggregation are absent |
| Heuristics | Rounding, fixed-integer LP repair, deterministic budgeted L1 feasibility pump and root diving | `src/mip.cpp` | Partial | No neighborhood local search; flugpl has no incumbent |
| MIP gap | Normalized bound/incumbent gap and termination | `src/mip.cpp` | Implemented | Depends on single-threaded tree processing |
| Parallelism | CPU capability reports hardware concurrency | `src/backend.cpp` | Missing | Authoritative LP/MILP algorithms are single-threaded |
| CPU backend | CSC products and experimental primal-dual workload | `src/backend.cpp` | Implemented | First-order result is deliberately non-authoritative |
| CUDA backend | Optional real CSC kernels, device probing, CPU cross-checks, benchmark probe and evidence script | `src/cuda_backend.cu`, `tools/cuda_probe.cpp`, `scripts/cuda-validate.ps1` | Prepared; measurement blocked | No CUDA compiler/device is available on this host |
| Adaptive execution | Measurement-based CPU/CUDA selection for experimental workload | `src/benchmark.cpp` | Partial | Does not select authoritative simplex/MILP execution strategies |
| Model fingerprint | Stable endian-independent hash and structural metrics | `src/fingerprint.cpp` | Implemented | Existing hash is 64-bit, not cryptographic |
| Strategy engine | Backend selection based on measured compute and transfer cost | `src/benchmark.cpp` | Partial | No LP algorithm selector |
| Telemetry | Structured LP/MILP/backend events plus persistent recorder timeline | `include/sovereign/solution.hpp`, `src/recorder.cpp` | Implemented | Cross-process distributed tracing is absent |
| CLI | Inspect, validate, solve, QPS, record and replay commands | `src/cli.cpp` | Implemented | Benchmark orchestration remains script-based |
| Python API | Native pybind11 solve and verification interface | `python/bindings.cpp` | Implemented | QP bindings remain narrower than the C++ API |
| REST API | FastAPI asynchronous solves, telemetry, verification, benchmarks and recorder operations | `service/app.py`, `service/store.py` | Implemented | Distributed durable job storage is absent |
| Dashboard | React engineering console with telemetry, verification and Flight Recorder views | `web/src/` | Implemented | Production authentication is deployment-specific |
| Mistral integration | Server-side optional explanations and confirmation-gated formulations | `service/ai.py`, `service/app.py` | Implemented | Live call is currently rate/quota limited; AI remains optional |
| Netlib | Automated seven-instance manifest with verified results | `benchmarks/manifests/netlib-small.json`, `benchmarks/results/netlib-2026-09-20-seven.json` | Partial | BLEND fails strict dual-certificate verification; larger BANDM times out |
| MIPLIB | Checksum-pinned flugpl and p0033 runs | `benchmarks/manifests/miplib-small.json`, `benchmarks/results/miplib-2026-09-22-expanded.json` | Partial | p0033 has a verified optimal-value incumbent without a proof; flugpl has no incumbent |
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
