# Phase 0–2 Implementation Plan

> Execute inline with test-first development and verification at each milestone.

**Goal:** Build a tested model-loading foundation without implementing optimization.

**Architecture:** CSC storage feeds a solver-independent linear model. Parser,
validation, configuration and logging have separate interfaces. CLI composes them.

**Tech Stack:** C++20, CMake, MSVC x64, Catch2 and CTest.

**Spec:** `docs/superpowers/specs/2026-09-13-foundation-design.md`

## Global constraints

- No mathematical optimization engine dependency or solver implementation.
- Every component has executable tests; no placeholder results.
- No permanent PATH changes; use discovered Visual Studio tools.
- Only Phase 0–2 is implemented in this increment.

## Milestones

- [x] Build/test infrastructure: CMakeLists.txt, scripts/build.ps1, tests/CMakeLists.txt.
  Configure using bundled CMake and Visual Studio 18 2026 x64. Pin Catch2 3.8.1.
  Build and execute a C++20 infrastructure test before adding core components.
- [x] Sparse matrix and model: include/sovereign/{sparse_matrix,model,errors}.hpp,
  src/{sparse_matrix,model}.cpp, tests/test_{sparse_matrix,model}.cpp.
  First test duplicate cancellation, sorted columns, dimension failures, Ax/A^Tx,
  finite values and exact hand-counted model statistics. Implement CSC construction,
  column spans and multiplication; model is explicit data plus derived statistics.
- [x] Validation: include/sovereign/validation.hpp, src/validation.cpp,
  tests/test_validation.cpp. Test dimensions, names, invalid bounds, NaNs, integer
  domains and empty impossible rows before implementing structured diagnostics.
- [x] MPS: include/sovereign/mps.hpp, src/mps.cpp, tests/test_mps.cpp,
  examples/{small_lp,mixed,ranges}.mps. Test objective, RHS, range sign rules,
  all supported bounds, integer markers, fixed continuations, malformed input,
  unknown references, duplicate records and unsupported sections before parser code.
- [x] Application: include/sovereign/{config,logging,cli}.hpp,
  src/{config,logging,cli,main}.cpp, tests/test_{config,logging,cli}.cpp.
  Test strict scalar config, escaped filtered logs, text/JSON inspection, validation,
  bad arguments and I/O failures before implementation. Run executable integration
  checks through CTest. Add README, architecture and MPS/config documentation.
- [x] Review and acceptance: independently review source, resolve findings, run
  Debug and Release builds/tests, run three CLI fixtures, record tool versions,
  dependency audit, limitations, created-file inventory and exact next milestone.

## Test contracts

```cpp
// Duplicate entries cancel; CSC retains only the nonzero mathematical sum.
auto a = CscMatrix::from_triplets(2, 2, {{1,0,3}, {1,0,-3}, {0,1,4}});
REQUIRE(a.nonzeros() == 1);
REQUIRE(a.multiply(std::vector<double>{2,3}) == std::vector<double>{12,0});
// Validation checks structure, not arbitrary LP feasibility.
REQUIRE(validate(model).ok());
// MPS objective row is excluded from constraint nonzeros/statistics.
REQUIRE(read_mps(stream).matrix.nonzeros() == 4);
```

For each component, write these behavioral tests and relevant boundary cases,
run the build to observe the missing interface, implement, then run CTest.
Build command: `powershell -File scripts/build.ps1 -Configuration Debug`.
Acceptance also uses `-Configuration Release` and direct `sovereign inspect` runs.
