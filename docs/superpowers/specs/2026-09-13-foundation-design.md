# Sovereign Optimizer Phase 0–2

Approved scope: C++20/CMake, sparse matrices, model representation, MPS import,
structural validation, configuration, logging, CLI and tests. No solving code,
solver dependencies, GPU implementation, bindings or product services.

The core library owns no global configuration or logging state. Immutable CSC
matrices are assembled from checked triplets; duplicate entries are summed and
exact zeros removed. Models describe a linear objective and two-sided row and
variable bounds, with continuous, integer and binary domains. Validation returns
structured diagnostics; it does not claim global feasibility.

MPS import supports linear ROWS/COLUMNS/RHS/RANGES/BOUNDS, objective sense/name,
integer markers and explicit free or fixed field layouts. Ambiguous multiple
data vectors and unsupported extensions are errors. Source lines appear in parse
errors. Model validation runs before a parsed model is returned.

The CLI exposes inspect and validate only, text or JSON output, strict configuration,
typed errors and meaningful exit codes. Logging writes filtered JSON lines to
stderr, leaving machine-readable stdout intact. Configuration is a documented
flat YAML scalar subset, with unknown keys rejected.

MSVC x64 and bundled CMake are selected after machine inspection. A PowerShell
build helper discovers Visual Studio without permanent environment changes.
Catch2 is a pinned test-only dependency; the production library uses only the
standard library. CTest includes library and CLI integration tests.

Acceptance: Debug and Release builds, all tests, three independently counted MPS
fixtures, no external optimization engine, documented limitations and next phase.
