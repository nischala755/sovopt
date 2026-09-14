# Numerical robustness and Flight Recorder design

## Scope

Extend AstraNiti in place. Preserve all current solve, model, basis, parser,
backend, API and dashboard interfaces. This increment adds numerical safeguards,
reproducible benchmark tooling, and the Optimization Flight Recorder. It does not
claim QP, parallel MILP, broad branch-and-cut, industrial scale, or measured CUDA
performance.

## Numerical design

`SparseBasis` retains the source basis matrix and performs iterative refinement
after its existing sparse LU solve and transpose solve. It reports solve residuals
and refinement counts without exposing factor storage. The simplex loop uses a
Harris two-pass primal ratio test: the first pass obtains a relaxed maximum step;
the second chooses the largest stable eligible pivot within that step, with basis
column order as the deterministic tie-break. A bounded fingerprint history detects
repeated bases and emits `ANTI_CYCLING_ACTIVATED`; entering pricing remains Bland
while anti-cycling is active. Every basis construction, refinement, ratio test and
numerical warning emits through the existing telemetry callback.

Terminal states remain unchanged. The independent verifier decides whether an
optimal, infeasible or unbounded claim is returned. A safeguard may turn an
uncertain computation into `numerical_failure`; it may never upgrade one.

## Benchmark design

Checked-in manifests contain instance identity, canonical download URL, SHA-256,
format, objective sense, published objective and tolerance. A Python runner
downloads only missing instances, validates checksums, invokes the built AstraNiti
CLI, captures machine/build metadata and writes JSON. A separate baseline adapter
detects configured executable commands and records the exact command and version.
It never links to or enters the production solver path, and absence is reported.

## Flight Recorder design

`sovereign solve model.mps --record run.astra` writes a versioned directory bundle
using existing solve JSON and telemetry data. A directory keeps the format
dependency-free and inspectable. It contains the original `model.mps` plus the
specified JSON artifacts. `checksums.json` stores SHA-256 for every other regular
artifact. Writes occur in a sibling temporary directory and are renamed only after
all artifacts and hashes succeed.

`sovereign replay run.astra` validates names, required artifacts, format version,
sizes and SHA-256 values before parsing content. It reports the recorded status and
timeline. `--reverify` reloads the stored original MPS and reruns the mathematical
solve using the stored configuration, then compares status, objective, primal and
verification outcome. It does not trust `verification.json`. Any mismatch or hash
failure returns a nonzero exit code and names the artifact.

The first schema is intentionally JSON-only and version `1`. JSON serializers live
in the recorder module so CLI, binding and service use the same output. SHA-256 is
implemented locally as a small utility, avoiding a cryptographic package and any
solver dependency.

## Product integration

The Python binding exposes record and replay functions. FastAPI adds a record
creation endpoint, a bundle download endpoint and replay endpoint. The current job
telemetry remains the source of timeline events. The dashboard adds a Flight
Recorder view showing the actual event timeline, numerical-health events,
certificate verification state, and integrity result returned by the API.

## Error and trust rules

Bundles never include environment variables, API keys, prompts, or arbitrary host
files. Artifact filenames are fixed. Replay rejects symlinks and unexpected path
traversal. JSON numbers use finite values or explicit strings for infinities.
Recorder failure does not alter a computed mathematical result, but the CLI record
command fails because its requested evidence artifact was not produced.

## Verification

Tests cover ill-conditioned forward/transpose solves, Harris selection, cycling
telemetry, deterministic event streams, successful record/replay, solution,
certificate and fingerprint tampering, independent re-verification, malformed
bundles, API routes and dashboard rendering. Existing tests run before and after
each critical change. Netlib results remain evidence only when checksum, solve and
independent verification all pass.
