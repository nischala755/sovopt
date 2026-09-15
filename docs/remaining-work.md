# Remaining work after robustness and Flight Recorder increment

## A. Already complete

- C++20 foundation, sparse CSC, model validation, MPS, presolve and scaling.
- Two-phase primal revised simplex with Harris two-pass ratio selection,
  deterministic Bland pricing, bounded basis-history detection, residual-based
  basis refinement, and original-model verification.
- Verified Netlib AFIRO, SC50A and SC50B regressions.
- Small-model MILP branch-and-bound, branching, bounds, limits, rounding and a
  safe cut framework.
- CPU and optional CUDA backends, fingerprints, adaptive experimental workload,
  telemetry, CLI, Python, REST API, dashboard and optional Mistral integration.
- Optimization Flight Recorder recording, SHA-256 integrity, replay,
  independent re-verification, API download and dashboard timeline.
- Checksum-pinned Netlib/MIPLIB manifests and an isolated reference adapter.
- Small convex QP and LP interior-point solves with primal/KKT verification.
- Mathematical demonstrators for crude blending, process optimization,
  production planning and supply chains.

## B. Partially complete

- MILP robustness: small regressions pass, while MIPLIB flugpl exhausts its LP
  iteration budget before finding a verified incumbent.
- Presolve: fixed, constant and singleton transformations exist; duplicate rows,
  safe aggregation and general coefficient strengthening remain absent.
- Scaling uses one-pass row/column normalization rather than iterative
  equilibration with condition estimates.
- Cuts and heuristics are intentionally basic.
- Adaptive execution selects CPU/CUDA for a non-authoritative numerical workload,
  not the authoritative simplex or MILP algorithms.
- Industrial generators cover all named domains, but use deterministic synthetic
  data and do not establish industrial-scale performance.

## C. Missing

- Dual simplex and a real algorithm-selecting `auto` method.
- Authoritative multicore LP/MILP, sparse LU update schemes, general Gomory/MIR
  cuts, feasibility pump and tree restart/persistence. Bounded strong branching
  with real child LP probes and safe
  binary knapsack cover and conflict-clique separators are implemented.
- Sparse large-scale QP KKT factorization, QP file import, infeasibility and
  unboundedness certificates, and Flight Recorder QP schemas. Optimal QP KKT
  certificates now use original-model row and variable coordinates.
- Crude blending, production-planning and supply-chain demonstrations.
- Mittelmann manifest and measured benchmark run.

## D. Needs validation

- A broader Netlib set and successful MIPLIB instances.
- Large degenerate, ill-conditioned and weak-relaxation cases on pinned hardware.
- Broader reference-solver comparisons beyond the measured isolated HiGHS 1.15.1 run.
- Malicious-integrity protection would require signatures; SHA-256 detects changes
  relative to the stored manifest but is not an authenticity signature.

## E. Blocked by hardware or environment

- CUDA toolkit, driver and device measurements: this host exposes Intel UHD only.
  `scripts/cuda-validate.ps1` is ready to build, cross-check, benchmark and export
  evidence on the target NVIDIA laptop.
- Reference comparison: no supported reference executable is installed.
- Mistral live explanation: the configured account returned HTTP 429.
