# Remaining phases implementation plan

User authorization: continue the previously approved complete architecture, without
new approval gates between reversible local milestones. Original specification:
the supplied Sovereign Optimizer project brief and session requirements.

## Architecture

Original models remain immutable solver inputs. Presolve produces a reversible
mapping. LP standardization is internal and records mappings for original-model
dual certificates. Revised simplex uses sparse LU of an explicit basis, phase I
and II, deterministic Bland pricing, ratio tests, scaling and residual checks.
Independent verification consumes original model data and certificates rather than
basis calculations. MILP consumes LP bounds and verified incumbents. Numerical
backends, telemetry, benchmark experiments and product layers remain separable.

## Execution ledger

- [ ] Phase 3: conservative presolve, fixed substitution, singleton bounds,
  constant rows, transformation journal and reconstruction tests.
- [ ] Phases 4–5: genuine revised simplex, sparse basis factorization, standard
  form, phase I/II, numerical controls, independent primal/dual/ray/Farkas checks.
- [ ] Phases 6–9: branch-and-bound, node queue, two branching strategies, bounds,
  gap/limits, incumbent verification, rounding heuristic and safe modular cuts.
- [ ] Phases 10–14: CPU parallel numerical work, first-order iterations, optional
  CUDA kernels, transfer measurements, fingerprint and measured adaptive policies.
- [ ] Phases 15–16: reproducible benchmark runner and seeded industrial generators.
- [ ] Phases 17–18: pybind11 API, FastAPI asynchronous jobs, verification,
  benchmarks, persisted metadata and telemetry with resource limits.
- [ ] Phases 19–20: React/TypeScript laboratory, model inspection, comparisons,
  optional Mistral explanations and explicitly confirmed structured formulations.
- [ ] Phases 21–23: full integration tests, build/test reports, setup/demo docs,
  dependency audit and explicit hardware/service verification limitations.

## Constraints and rulings

- No existing optimization engine or solver package is installed or invoked.
- Unit/integration expectations come from analytic examples or exhaustive small
  discrete enumeration, never another copy of the same solving algorithm.
- GPU speedup requires measured hardware evidence. This machine has no detected
  CUDA device. CPU fallback is executable; CUDA source cannot be represented as
  runtime-validated without compatible hardware.
- Floating-point certificates are checked with explicit tolerances and reported
  as numerical verification, not exact rational proofs.
- AI is peripheral, opt-in and non-authoritative. API keys use environment
  variables; only explicit allowlisted nonsensitive metadata is sent remotely.
- Tasks own separate source/test files; only the parent edits CMake and shared
  CLI integration. Each milestone must compile and pass its tests before use.

## Shared interfaces

`solution.hpp` defines SolverOptions, SolveResult, tolerances, telemetry and
original-model certificate vectors. `verification.hpp` independently validates
primal values, optimality, Farkas multipliers and rays. `presolve.hpp` exposes the
reduced model and reconstruction mappings. `lp.hpp` exposes solve_lp and sparse
basis operations; `mip.hpp` exposes solve_mip using the same result contract.

## Validation commands

Windows: `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1`.
Repeat final acceptance in Release. Python uses a project-local virtualenv and
pytest. Frontend uses local npm dependencies, TypeScript checking, tests and build.
Run live API/CLI examples and record actual benchmark JSON/CSV, never constants.
