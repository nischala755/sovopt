# Foundation architecture

## Core data and invariants

`CscMatrix` privately owns `size_t` row/column dimensions, column offsets, row
indices and double values. `from_triplets` checks every coordinate and finite
coefficient, stable-sorts by column and row, sums duplicates, rejects overflow and
removes exact zeros. Every column's row indices are strictly increasing. Empty
columns and zero dimensions are supported. Column views are read-only spans whose
lifetime ends when the matrix is destroyed, assigned or moved. Do not retain views
across those operations; use moved-from matrices only for destruction/assignment.

Construction costs O(k log k) for k input entries. Final storage is O(n + nnz),
where n is the column count; no dense matrix is formed. Matrix products return
dense result vectors and cost O(nnz + output length). Products use IEEE double
arithmetic without a solver tolerance or residual policy. Callers must assess
nonfinite or inaccurate results before future mathematical decisions.

`Model` stores the original model's names, two-sided row bounds, variable bounds
and domains, objective coefficients, sense and additive offset. Binary variables
are also integer variables in statistics. Integer domain metadata is representable
now even though no integer optimization exists. Public model data supports explicit
construction and editing; validation is required after edits. Matrix and objective
storage are separate, and the objective row is excluded from constraint counts.

## Validation boundary

`validate` returns issue codes and messages without mutating input. It checks
dimensions, finite objective data, names, enum values, correctly oriented infinities,
bound ordering, binary limits, nonempty integer intervals and impossible zero rows.
CSC invariants are enforced by construction, so validation does not scan duplicate
storage that callers cannot create through the interface.

General consistency between nonempty rows and variable bounds is intentionally
left to presolve and solving. Passing validation must never become an optimal,
feasible or bounded solver status. `require_valid` throws `InvalidModelError` with
all issues; the MPS reader translates these into an import error.

## Import and application boundaries

`read_mps` accepts an input stream and explicit options. Each call has isolated
state, stable variable/row encounter order and source-line errors. The file overload
owns only the stream. Parsed models are validated before they are returned.

Configuration is an application concern. It contains only settings that have
implemented behavior; no solver or GPU settings are accepted. `Logger` owns no
global state; a mutex serializes records written through the same logger instance.
The caller owns the output stream, which must outlive the logger. Separate loggers
sharing a stream require caller synchronization. Logs deliberately have no fabricated
timings, solver events or nondeterministic timestamps.

`run_cli` takes argument views plus explicit output/error streams, making actual
dispatch behavior testable without subprocess mocks. Executable tests additionally
verify exit codes, model statistics and valid JSON using CMake's JSON parser.
JSON numbers use the classic locale and 17 significant digits.
MPS model names/data are ASCII. The current Windows entrypoint uses narrow argv;
non-ASCII filesystem paths and their encoding in error logs need a future native
Unicode entrypoint. ASCII paths are the tested CLI contract for this increment.

## Build graph and extension points

- `sovereign_core`: sparse matrix, model, validation and MPS reader; standard library.
- `sovereign_app`: configuration, logging and CLI; core plus standard threading.
- `sovereign`: thin `main` linked to the application library.
- `sovereign_tests`: Catch2 test runner; links the application and transitively core.

Future presolve, LP bases and branch-and-bound state belong in separate modules.
Presolve should retain a transformation journal to reconstruct original coordinates.
Future numerical backends can operate on CSC column views without coupling storage
to CUDA ownership. No virtual solver methods or unfinished implementations are added
in anticipation of those phases.
