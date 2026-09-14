# Numerical Robustness and Flight Recorder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Strengthen the existing revised-simplex numerical path and add portable, tamper-evident solve recording and replay.

**Architecture:** Extend `SparseBasis`, the current simplex loop and telemetry in place. Add a standalone recorder module consumed by the existing CLI, Python binding, FastAPI service and React dashboard; add benchmark scripts outside the production solver.

**Tech Stack:** C++20, CMake, Catch2, SHA-256, Python 3/FastAPI/pytest, React/TypeScript/Vitest.

**Spec:** `docs/superpowers/specs/2026-09-14-robustness-flight-recorder-design.md`

## Global Constraints

- Preserve current public solver entry points and all passing behavior.
- Do not add an optimization-engine dependency or fabricate benchmark evidence.
- Every terminal mathematical claim must pass original-model independent verification.
- CUDA measurements remain unavailable unless run on detected CUDA hardware.

---

### Task 1: Basis refinement and metrics

**Files:** Modify `include/sovereign/basis.hpp`, `src/basis.cpp`; test `tests/test_basis.cpp`.

**Interfaces:** Add `BasisSolveInfo`, `last_solve_info()` and internal residual-based refinement while preserving `solve` and `solve_transpose`.

- [ ] Add forward and transpose ill-conditioned residual tests and verify they fail on absent metrics.
- [ ] Store original sparse basis columns, compute scaled infinity residuals, apply up to three LU correction solves, and expose immutable last-solve metrics.
- [ ] Run basis tests and the full native suite.

### Task 2: Harris ratio and cycling telemetry

**Files:** Modify `src/lp.cpp`, `include/sovereign/solution.hpp`; test `tests/test_lp.cpp`.

**Interfaces:** Preserve `solve_lp`; add solver telemetry counters only. Select a leaving row through a private Harris two-pass function.

- [ ] Add a near-degenerate pivot regression and an event assertion for ratio selection and repeated-basis activation.
- [ ] Implement relaxed first pass, stable-pivot second pass, bounded basis signatures and deterministic fallback.
- [ ] Emit factorization, refinement, ratio-test and anti-cycling events; run targeted and full tests.

### Task 3: Recorder core and CLI

**Files:** Create `include/sovereign/recorder.hpp`, `src/recorder.cpp`, `tests/test_recorder.cpp`; modify `CMakeLists.txt`, `tests/CMakeLists.txt`, `src/cli.cpp`.

**Interfaces:** Add `record_solve(path, model_path, model, options, result, events)` and `replay_solve(path, reverify) -> ReplayReport`.

- [ ] Add tests for bundle creation, required artifacts, replay, three artifact tamper cases, malformed input and re-verification; confirm failures before implementation.
- [ ] Implement local SHA-256, canonical artifact serialization, atomic directory publication, integrity validation and re-solve verification.
- [ ] Add `--record`, `replay`, and `--reverify` CLI behavior and executable integration tests.
- [ ] Run recorder/CLI and full native tests.

### Task 4: Benchmark manifests and external baseline adapter

**Files:** Create `benchmarks/manifests/netlib-small.json`, `benchmarks/run_suite.py`, `benchmarks/baselines/run_reference.py`, `tests/python/test_benchmarks.py`; update `docs/benchmark_results.md`.

**Interfaces:** Scripts accept explicit CLI/reference paths and output one versioned JSON report.

- [ ] Add pytest cases for manifest validation, unavailable reference reporting and a fake executable protocol; verify initial failure.
- [ ] Implement checksum-verified acquisition, environment metadata, exact command recording and no-reference behavior.
- [ ] Execute locally available Netlib inputs and publish only observed results.

### Task 5: Python/API integration

**Files:** Modify `python/bindings.cpp`, `service/engine.py`, `service/app.py`, `tests/python/test_native_integration.py`, `tests/python/test_service.py`.

**Interfaces:** Add model record job, download and replay/reverify endpoints using server-owned bundle IDs and paths.

- [ ] Add failing native and API tests for recording, download, replay and tamper responses.
- [ ] Bind recorder operations and implement bounded server-side storage/routes.
- [ ] Run Python tests and API smoke test.

### Task 6: Dashboard evidence view

**Files:** Modify `web/src/api.ts`, `web/src/App.tsx`, `web/src/components.tsx`, `web/src/App.test.tsx`, `web/src/styles.css`.

**Interfaces:** Consume recorder API results; render timeline and integrity state from actual responses.

- [ ] Add failing component tests for recorded events and tamper state.
- [ ] Implement Flight Recorder navigation, creation/download/replay controls, timeline and numerical-health panels.
- [ ] Run Vitest, type checking and production build.

### Task 7: Traceability and final evidence

**Files:** Create `docs/requirements-traceability.md`; update `README.md`, `docs/current-state-audit.md`, `docs/remaining-work.md`, `docs/benchmark_results.md`.

**Interfaces:** Documentation maps claims to code/tests/actual executions.

- [ ] Run native, Python and web suites from clean Release outputs.
- [ ] Record, replay, reverify and deliberately tamper with a copied bundle.
- [ ] Run recognized benchmark inputs present locally and the reference adapter if a solver is detected.
- [ ] Detect CUDA toolkit/driver/device and report only observed capability.
- [ ] Update every traceability status from collected evidence and commit the increment.
