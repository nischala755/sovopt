# Phase 0–2 acceptance report

Date: 2026-09-13. Workspace: `C:/Users/keerthish/Desktop/projects/sih2`.

## Implemented

- C++20/CMake production library, application library, executable and Catch2/CTest
  infrastructure, with a Windows build/test helper.
- Immutable canonical CSC matrix assembly, checked coordinates/coefficients,
  duplicate aggregation, zero removal, column views, matrix/vector and transpose
  products.
- Explicit linear objective, two-sided row/variable bounds, continuous/integer/
  binary domains and independently derived model statistics.
- MPS import with supported free/fixed layouts, ranges, bounds, objective metadata,
  markers, line diagnostics and structural validation.
- Structured validation reports and typed parse/model/configuration errors.
- Strict configuration, JSON-line severity-filtered logging, inspect/validate CLI,
  text/JSON output and meaningful error exit codes.

No LP/MILP solver, presolve, CUDA code, bindings, service, frontend, AI provider or
external optimization engine was implemented or installed. This increment does
not claim mathematical solution verification or optimization benchmark results.

## Toolchain detected and used

| Item | Result |
|---|---|
| Primary compiler | MSVC x64 19.51.36248.0, toolset directory 14.51.36231 |
| Visual Studio | Build Tools 2026, installation 18.7.11925.98 |
| MSBuild | 18.7.8+1ac568fee |
| Windows SDK | 10.0.26100.0 |
| CMake and CTest | 4.3.1-msvc1, bundled with Visual Studio |
| Generator | Visual Studio 18 2026, architecture x64 |
| Other detected compiler | MSYS2 UCRT64 GCC 16.1.0; not the acceptance build toolchain |
| Test dependency | Catch2 v3.8.1, revision `2b60af89e23d28eefc081bc930831ee9d45ea58b` |
| Detected graphics | Intel UHD Graphics, driver 32.0.101.7076 |
| CUDA | No nvcc/NVIDIA utility on PATH or CUDA toolkit in the standard location; no NVIDIA adapter detected |

CMake executable:

```text
C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe
```

CTest is in the same directory. Compiler executable:

```text
C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe
```

CMake was found before any installation attempt. No software installation or
permanent PATH/policy change was necessary. PowerShell execution-policy bypass
was scoped to child build processes because the machine disallows scripts by
default. The workspace was initially empty and is now a local Git repository on
`phase-0-2`. Project files remain uncommitted; nothing was pushed or published.

## Verification evidence

| Verification | Result |
|---|---|
| Final Debug configure/build | Passed, exit 0 |
| Final Debug CTest | 39/39 passed; 1.74 seconds |
| Final Release configure/build | Passed, exit 0 |
| Final Release CTest | 39/39 passed; 1.66 seconds |
| Separate `BUILD_TESTING=OFF` Release build | Passed; no `_deps` directory created |
| Production executable CLI integration | Passed against all three examples and error paths |
| Independent source review | No remaining blocking findings |
| Prohibited-engine source/build audit | No matches in `src`, `include`, `tests`, `scripts` or `CMakeLists.txt` |

The 39 CTest checks comprise 38 Catch2 cases and one executable-level integration
script. Test durations above are test-suite wall times, not solver benchmarks.
Final build/test logs are `build/debug-final.log` and `build/release-final.log`.

Commands run from the project root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Release
```

The separate production build used the discovered CMake executable:

```text
cmake -S . -B build/production -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=OFF
cmake --build build/production --config Release --parallel
cmake -DCLI=<absolute-path-to-production-sovereign.exe> -DSOURCE=<absolute-project-root> -P tests/cli_integration.cmake
```

The final source fixes were rebuilt in all three configurations. Regression tests
cover malformed double signs, invalid MPS bytes, dollar-prefixed identifiers and
Windows Ctrl-Z truncation in both file readers. Their old behavior was reproduced
before the fixes. No failing test was removed.

## Actual example inspection results

| File | Variables | Rows | Nonzeros | Integer including binary | Binary | Equalities | Ranged rows | Offset |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `examples/small_lp.mps` | 2 | 2 | 4 | 0 | 0 | 0 | 0 | 0 |
| `examples/mixed.mps` | 3 | 2 | 5 | 2 | 1 | 1 | 0 | 0 |
| `examples/ranges.mps` | 2 | 3 | 4 | 0 | 0 | 1 | 2 | 5 |

All three passed structural validation. The CLI printed these statistics from
the parsed models; integration tests parsed its JSON and compared with literal,
hand-counted fixture expectations. Objective entries are excluded from matrix
nonzeros. No example was solved.

## Current architecture

`sovereign::core` owns matrix/model/parser/validation, using the C++ standard
library only. `sovereign_app` composes configuration, logging and CLI dispatch.
`sovereign` is a thin executable. Catch2 is linked only to `sovereign_tests`.
Future mathematical modules can consume validated original models and CSC column
views while keeping their transformations, bases and numerical state separate.
See [architecture](architecture.md) for invariants, ownership and complexity.

## All authored project files created

There were no pre-existing project files to modify. The 40 authored files are:

```text
.gitignore
CMakeLists.txt
README.md
docs/architecture.md
docs/configuration.md
docs/mps.md
docs/phase-0-2-report.md
docs/superpowers/plans/2026-09-13-foundation.md
docs/superpowers/specs/2026-09-13-foundation-design.md
examples/inspect.yaml
examples/mixed.mps
examples/ranges.mps
examples/small_lp.mps
include/sovereign/cli.hpp
include/sovereign/config.hpp
include/sovereign/errors.hpp
include/sovereign/logging.hpp
include/sovereign/model.hpp
include/sovereign/mps.hpp
include/sovereign/sparse_matrix.hpp
include/sovereign/validation.hpp
scripts/build.ps1
src/cli.cpp
src/config.cpp
src/logging.cpp
src/main.cpp
src/model.cpp
src/mps.cpp
src/sparse_matrix.cpp
src/validation.cpp
tests/CMakeLists.txt
tests/cli_integration.cmake
tests/test_cli.cpp
tests/test_config.cpp
tests/test_infrastructure.cpp
tests/test_logging.cpp
tests/test_model.cpp
tests/test_mps.cpp
tests/test_sparse_matrix.cpp
tests/test_validation.cpp
```

Generated artifacts are isolated under ignored `build/`: CMake/MSBuild projects,
Catch2 source/build files, Debug/Release libraries and executables, CTest logs and
temporary reproduction inputs. Git metadata is under `.git/`. These generated
files are not source deliverables and are reproducible from the build commands.

## Known limitations and technical risks

1. Structural validity does not establish general feasibility, boundedness or
   optimality. No solution or certificate type is advertised yet.
2. The MPS dialect is restricted. Multiple data vectors and multiple `N` rows are
   rejected; `OBJNAME` confirms the sole objective identity. Fixed inline comments,
   extended optimization constructs and compressed input are unsupported. Marker
   upper-bound conventions vary across readers; our `[0,1]` default is documented.
3. Numeric data uses binary64. Duplicate aggregation is deterministic but not exact
   arithmetic. Matrix products do not implement solver tolerances or overflow/
   residual assessment; later numerical decisions must add these checks. Large
   integer values may exceed exact binary64 representation.
4. CSC construction buffers triplets and sorts them. There is no out-of-core
   importer or complete adversarial memory budget. Full row-oriented algorithms
   may need an auxiliary row index, without changing the model's ownership.
5. Configuration is a flat unquoted YAML subset. Future schema expansion must
   preserve strict unknown-key and duplicate-key errors.
6. Windows CLI paths use narrow argv. ASCII paths are tested; non-ASCII paths and
   their error-log UTF-8 encoding need a native Unicode entrypoint.
7. First-time test configuration needs network/Git or an explicitly supplied local
   Catch2 source tree. Production builds have no third-party dependency.
8. CUDA cannot be tested on the detected hardware and is outside this phase.

## Exact next milestone

**Phase 3: conservative presolve with reversible transformations and original-model
solution reconstruction.** Start with fixed-variable substitution, objective/RHS
updates, constant-row handling and safe bound consistency checks, each backed by
hand-verifiable reconstruction and error tests. Preserve original model coordinates
and record every transformation. Only after that buildable milestone should
Phase 4 add revised simplex. No Phase 3 code was started in this increment.
