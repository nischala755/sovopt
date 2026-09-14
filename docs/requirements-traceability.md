# SIH requirements traceability

Evidence date: 2026-09-14. Status reflects executable behavior, not class names.

| Area | Requirement | Status | Evidence or gap |
|---|---|---|---|
| Solver core | LP | IMPLEMENTED | Two-phase revised simplex; three verified Netlib instances |
| Solver core | MILP | PARTIALLY IMPLEMENTED | Verified small tests; MIPLIB flugpl reaches iteration limit |
| Solver core | QP | PARTIALLY IMPLEMENTED | Convex QP model and predictor-corrector interior-point solver pass hand-verifiable KKT tests; Newton systems are dense and large-scale evidence is absent |
| Solver core | Sparse computation | IMPLEMENTED | Canonical CSC and sparse-row LU |
| Solver core | Numerical stability | PARTIALLY IMPLEMENTED | Scaling, Harris ratio, refinement and conservative certificates; industrial scale unproved |
| Solver core | Scalability | PARTIALLY IMPLEMENTED | Sparse storage; broad large-scale evidence absent |
| Solver core | Multi-core | NOT IMPLEMENTED | Authoritative LP/MILP paths are single-threaded |
| Solver core | GPU | BLOCKED | CUDA kernels, CPU cross-check tests, probe executable and reproducible validation pack exist; actual NVIDIA measurement requires the target laptop |
| Algorithms | Revised simplex | IMPLEMENTED | Phase I/II, basis, pricing, ratio, pivot and verification |
| Algorithms | Interior-point method | PARTIALLY IMPLEMENTED | Genuine infeasible-start primal-dual LP/QP path with residual verification; sparse KKT factorization and certificates remain |
| Algorithms | First-order method | PARTIALLY IMPLEMENTED | Experimental non-authoritative CPU/CUDA workload |
| Algorithms | Branch-and-bound | IMPLEMENTED | Deterministic best-bound queue and verified pruning |
| Algorithms | Branch-and-cut | PARTIALLY IMPLEMENTED | Safe integer-row strengthening only |
| Algorithms | Cuts | PARTIALLY IMPLEMENTED | No Gomory, MIR, cover or clique separators |
| Algorithms | Presolve | PARTIALLY IMPLEMENTED | Fixed, constant and singleton reductions with reconstruction |
| Algorithms | Heuristics | PARTIALLY IMPLEMENTED | Rounding and fixed-integer LP repair |
| Algorithms | Node selection | IMPLEMENTED | Deterministic best-bound selection |
| Algorithms | Dual simplex | NOT IMPLEMENTED | Current engine is primal revised simplex |
| Industrial | Refinery scheduling | IMPLEMENTED | Seeded mathematical model generator |
| Industrial | Crude blending | IMPLEMENTED | Seeded mathematical blend/demand/quality model generator and solve regression |
| Industrial | Process optimization | IMPLEMENTED | Seeded staged-yield process-flow generator and solve regression |
| Industrial | Production planning | IMPLEMENTED | Seeded multi-product, multi-period integer model generator and solve regression |
| Industrial | Logistics | IMPLEMENTED | Seeded mathematical model generator |
| Industrial | Power dispatch | IMPLEMENTED | Seeded mathematical model generator |
| Industrial | Transportation | PARTIALLY IMPLEMENTED | Logistics overlaps; no named validation |
| Industrial | Supply chain | IMPLEMENTED | Seeded capacitated plant/customer network generator and solve regression |
| Benchmark | Netlib | IMPLEMENTED | AFIRO, SC50A and SC50B match and verify |
| Benchmark | MIPLIB | PARTIALLY IMPLEMENTED | flugpl manifest/run exists and currently fails |
| Benchmark | Mittelmann | NOT IMPLEMENTED | No manifest or measured run |
| Benchmark | Reference comparison | PARTIALLY IMPLEMENTED | Isolated harness exists; no reference executable installed |
| Sovereignty | From-scratch core | IMPLEMENTED | No external optimization engine in core |
| Sovereignty | No solver dependency | IMPLEMENTED | Production build links no solver package |
| Interface | CLI | IMPLEMENTED | Inspect, validate, solve, record and replay |
| Interface | API | IMPLEMENTED | Python and FastAPI solve/verify/benchmark/recorder operations |
| Transparency | Model fingerprint | IMPLEMENTED | Stable logical fingerprint and bundle SHA-256 |
| Transparency | Telemetry | IMPLEMENTED | LP/MILP/backend events and persistent timeline |
| Transparency | Certificate | IMPLEMENTED | Dual, Farkas and ray artifacts |
| Transparency | Independent verification | IMPLEMENTED | Original-model primal/dual/recession checks |
| Transparency | Flight Recorder | IMPLEMENTED | Versioned bundle, SHA-256, replay, reverify and download |
| Product | Dashboard | IMPLEMENTED | Real telemetry, verification and Flight Recorder view |
| Product | Mistral | IMPLEMENTED | Optional non-authoritative integration; live account returns 429 |

The strongest SIH claim remains incomplete: successful MIPLIB coverage,
independently timed reference comparison, large ill-conditioned cases,
authoritative multicore execution and measured CUDA still require evidence.
