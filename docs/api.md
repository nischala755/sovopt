# Python API and service

Sovereign's Python package builds an optional `sovereign_optimizer` pybind11 module and installs the `service` FastAPI package. The extension compiles the repository's own C++ engine sources; it does not download or call another optimizer.

## Setup

Python 3.10+, CMake 3.25+, a C++20 compiler, and a Python virtual environment are required. From the repository root:

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -e ".[test]"
pytest -q tests/python
sovereign-api
```

The server listens on `127.0.0.1:8000`. Change the uvicorn host explicitly if remote access is intended. If the extension cannot be imported, `GET /health` reports `engine: unavailable`; solve, inspect, verify, and benchmark return HTTP 503. The service never synthesizes solver output.

## Resource and persistence model

Uploaded MPS content is streamed into memory only up to `SOVEREIGN_MAX_UPLOAD_BYTES`, assigned a random server filename, hashed with SHA-256, and stored below `SOVEREIGN_DATA_DIR/uploads`. Client filenames may contain only letters, digits, dot, underscore, and hyphen. SQLite at `SOVEREIGN_DATA_DIR/metadata.sqlite3` persists model, job, result, error, and bounded telemetry metadata.

`SOVEREIGN_MAX_ACTIVE_JOBS` bounds queued/running work with a thread pool and returns HTTP 429 when full. `SOVEREIGN_MAX_TELEMETRY_EVENTS` caps stored events per job. Production operators should additionally set process-level memory/CPU limits, put authentication and TLS at a reverse proxy, and back up or expire the data directory according to local policy.

## Endpoints

Upload raw MPS bytes with `POST /models/upload`; `X-Filename` is optional. The response contains `model_id`, byte count, and SHA-256 fingerprint.

- `GET /health`
- `GET /models`
- `GET /models/{model_id}`
- `GET /models/{model_id}/inspect`
- `GET /models/{model_id}/fingerprint`
- `POST /models/{model_id}/solve` with `{"kind":"lp|mip","options":{...}}`
- `POST /models/{model_id}/verify` with a verification kind and its certificate/result data
- `POST /models/{model_id}/benchmark` with kind, repetitions (1–100), and options
- `POST /models/{model_id}/execution-benchmark` with CPU, GPU, or adaptive modes
- `GET /jobs/{job_id}`
- `GET /jobs/{job_id}/telemetry`

Solve and benchmark return HTTP 202 and a job ID. Poll the job until `state` is `succeeded` or `failed`. Failures are recorded as errors on that job and do not affect other jobs.

The pybind layer supports MPS inspection, LP/MIP solving, independent primal/optimality/infeasibility/unboundedness verification, benchmark repetitions, telemetry callbacks, and creation of a validated in-memory `Model` from confirmed structured data. A subprocess adapter is intentionally absent because the current CLI has no stable machine-readable solve protocol.

Execution benchmarks run the explicitly non-authoritative first-order numerical
workload. GPU records contain measured kernel and transfer durations only when a
CUDA device executed them; unavailable GPU records contain null timings.
