# Remaining work after robustness and Flight Recorder increment

## A. Already complete

- C++20 foundation, sparse CSC, model validation, MPS, presolve and scaling.
- Two-phase primal revised simplex with Harris two-pass ratio selection,
  deterministic Bland pricing, bounded basis-history detection, residual-based
  basis refinement, product-form inverse updates with controlled sparse-LU
  refactorization, original-model verification, and a checked tableau-row
  extraction interface with transformation-integrality metadata.
- Verified Netlib AFIRO, SC50A, SC50B and SC105 regressions.
- Small-model MILP branch-and-bound, branching, bounds, limits, rounding and a
  safe cut framework. A deterministic, budgeted L1 feasibility pump produces
  independently verified root incumbents when its projection succeeds. Root
  separation includes efficacy-filtered single-row Chvatal-Gomory cuts for
  shifted general-integer rows and one conservative root GMI tableau cut round.
- CPU and optional CUDA backends, fingerprints, adaptive experimental workload,
  telemetry, CLI, Python, REST API, dashboard and optional Mistral integration.
- Optimization Flight Recorder recording, SHA-256 integrity, replay,
  independent re-verification, API download and dashboard timeline.
- Checksum-pinned Netlib/MIPLIB manifests and an isolated reference adapter.
- Small convex QP and LP interior-point solves with primal/KKT verification and
  sparse CSC KKT assembly plus sparse partial-pivot LU factorization.
- Mathematical demonstrators for crude blending, process optimization,
  production planning and supply chains.

## B. Partially complete

- MILP robustness: small regressions pass, while MIPLIB flugpl exhausts its LP
  iteration budget before finding a verified incumbent. A budgeted root diving
  heuristic finds verified small-model incumbents but does not resolve flugpl.
- Presolve: fixed, constant, singleton and exact-duplicate-row transformations
  exist; safe aggregation of differently bounded parallel rows and general
  coefficient strengthening remain absent.
- Scaling uses one-pass row/column normalization rather than iterative
  equilibration with condition estimates.
- Cuts and heuristics are intentionally basic.
- Adaptive execution selects CPU/CUDA for a non-authoritative numerical workload,
  not the authoritative simplex or MILP algorithms.
- Industrial generators cover all named domains, but use deterministic synthetic
  data and do not establish industrial-scale performance.

## C. Missing

- General standalone dual-simplex selection and certificates. Compatible MILP
  child bases already use dual-simplex reoptimization with cold fallback.
- Authoritative multicore LP/MILP, Forrest-Tomlin sparse LU updates, multi-round
  cut-pool management, local search and tree restart/persistence. Bounded strong branching
  with real child LP probes and safe
  binary knapsack cover and conflict-clique separators are implemented.
- Fill-reducing ordering and stronger sparse QP factorization safeguards.
  Canonicalization, symmetry/PSD validation and KKT assembly now use sparse
  storage. Standard triangular QPS `QUADOBJ` import is implemented; other vendor
  quadratic sections remain out of scope.
  Flight Recorder v2 now persists and independently replays optimal KKT,
  Farkas infeasibility and Hessian-null recession certificates in original-model
  coordinates.
- Mittelmann manifest and measured benchmark run.

## D. Needs validation

- ADLITTLE and BLEND Netlib reliability, successful MIPLIB instances, and a
  broader Netlib set. SC105 is now verified; ADLITTLE reaches the simplex
  iteration limit and BLEND reaches numerical failure after parsing.
- Large degenerate, ill-conditioned and weak-relaxation cases on pinned hardware.
- Broader reference-solver comparisons beyond the measured isolated HiGHS 1.15.1 run.
- Malicious-integrity protection would require signatures; SHA-256 detects changes
  relative to the stored manifest but is not an authenticity signature.

## E. Blocked by hardware or environment

- CUDA toolkit, driver and device measurements: this host exposes Intel UHD only.
  `scripts/cuda-validate.ps1` is ready to build, cross-check, benchmark and export
  evidence on the target NVIDIA laptop.
- Reference comparison beyond the checked-in isolated HiGHS 1.15.1 run requires
  additional reference executables and benchmark runs.
- Mistral live explanation: the configured account returned HTTP 429.
