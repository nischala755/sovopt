from __future__ import annotations

import time
from pathlib import Path

import pytest

from fastapi.testclient import TestClient

from service.ai import AIUnavailable, FormulationProposal, MistralProvider
from service.app import Settings, create_app


class FakeEngine:
    available = True
    unavailable_reason = None

    def capabilities(self):
        return {"gpu_available": False, "gpu_detail": "test environment"}

    def inspect(self, path: Path):
        return {"name": "tiny", "variables": 2, "constraints": 1, "nonzeros": 2,
                "integer_variables": 0, "binary_variables": 0, "density": 1.0}

    def solve(self, path: Path, kind: str, options: dict, emit):
        emit({"type": "iteration", "iterations": 1})
        return {"status": "optimal", "objective": 1.0, "primal": [1.0, 0.0],
                "verification": {"passed": True, "violations": []}}

    def verify(self, path: Path, request: dict):
        return {"passed": True, "violations": []}

    def benchmark(self, path: Path, request: dict, emit):
        return {"runs": [{"status": "optimal", "runtime_seconds": 0.01}]}

    def execution_benchmark(self, path: Path, modes: list[str], repetitions: int):
        return [{"backend": mode, "records": [{"status": "unavailable" if mode == "gpu" else "executed"} for _ in range(repetitions)]} for mode in modes]

    def create_model(self, formulation: dict):
        return {"name": formulation["name"], "variables": len(formulation["variables"])}

    def record(self, path: Path, bundle: Path, kind: str, options: dict):
        bundle.mkdir()
        (bundle / "manifest.json").write_text("{}")
        return {"status": "optimal", "integrity_passed": True, "timeline": [{"type": "SOLVE_COMPLETED"}]}

    def replay(self, bundle: Path, reverify: bool):
        return {"integrity_passed": True, "reverification_passed": reverify,
                "recorded_status": "optimal", "timeline": [{"type": "SOLVE_COMPLETED"}]}


class FakeAI:
    def explain(self, payload: dict):
        assert "primal" not in str(payload)
        return "The verified result is optimal."

    def propose(self, prompt: str):
        return FormulationProposal.model_validate({
            "name": "proposed", "sense": "minimize",
            "variables": [{"name": "x", "lower": 0, "upper": 5, "type": "continuous"}],
            "constraints": [], "objective": {"x": 1.0}, "objective_offset": 0,
        })


def client(tmp_path, *, engine=None, ai=None, max_upload=1024, max_jobs=2):
    app = create_app(Settings(data_dir=tmp_path, max_upload_bytes=max_upload,
                              max_active_jobs=max_jobs, max_telemetry_events=2),
                     engine=engine if engine is not None else FakeEngine(), ai_provider=ai)
    with TestClient(app) as c:
        yield c


def upload(c):
    r = c.post("/models/upload", content=b"NAME X\nENDATA\n", headers={"x-filename": "tiny.mps"})
    assert r.status_code == 201
    return r.json()["model_id"]


def wait_job(c, job_id):
    for _ in range(100):
        data = c.get(f"/jobs/{job_id}").json()
        if data["state"] in {"succeeded", "failed"}:
            return data
        time.sleep(.01)
    raise AssertionError("job did not finish")


def test_upload_inspect_fingerprint_and_async_solve(tmp_path):
    for c in client(tmp_path):
        mid = upload(c)
        assert c.get("/models").json()[0]["id"] == mid
        assert c.get(f"/models/{mid}").json()["name"] == "tiny.mps"
        assert c.get(f"/models/{mid}/inspect").json()["variables"] == 2
        assert len(c.get(f"/models/{mid}/fingerprint").json()["sha256"]) == 64
        submitted = c.post(f"/models/{mid}/solve", json={"kind": "lp", "options": {}})
        assert submitted.status_code == 202
        job = wait_job(c, submitted.json()["job_id"])
        assert job["state"] == "succeeded"
        telemetry = c.get(f"/jobs/{job['id']}/telemetry").json()["events"]
        assert telemetry[0]["type"] == "iteration"
        submitted = c.post(f"/models/{mid}/execution-benchmark", json={"backends":["cpu","gpu","adaptive"],"repetitions":2})
        benchmark = wait_job(c, submitted.json()["job_id"])
        assert [group["backend"] for group in benchmark["result"]] == ["cpu","gpu","adaptive"]


def test_limits_path_safety_and_engine_unavailable(tmp_path):
    for c in client(tmp_path, max_upload=4):
        assert c.post("/models/upload", content=b"12345").status_code == 413
        assert c.post("/models/upload", content=b"x", headers={"x-filename": "../../x.mps"}).status_code == 400
    class Missing(FakeEngine):
        available = False
        unavailable_reason = "native module not installed"
    for c in client(tmp_path / "missing", engine=Missing()):
        assert c.get("/health").json()["engine"] == "unavailable"
        assert c.post("/models/upload", content=b"NAME X\nENDATA\n").status_code == 503


def test_ai_is_allowlisted_failure_isolated_and_formulation_requires_confirmation(tmp_path):
    for c in client(tmp_path, ai=FakeAI()):
        mid = upload(c)
        explained = c.post("/ai/explain", json={"model_id": mid, "result": {
            "status": "optimal", "objective": 1, "primal": [99], "secret": "no"}})
        assert explained.json()["explanation"].startswith("The verified")
        assert c.post("/ai/models/explain", json={"model_id":mid}).status_code == 200
        assert c.post("/ai/benchmarks/explain", json={"benchmark":{"run_count":1}}).status_code == 200
        assert c.post("/ai/results/explain", json={"result":{"status":"optimal"}}).status_code == 200
        proposal = c.post("/ai/formulations/propose", json={"prompt": "one bounded variable"})
        assert proposal.status_code == 200
        token = proposal.json()["confirmation_token"]
        assert c.post("/ai/formulations/confirm", json={"confirmation_token": "bad"}).status_code == 404
        assert c.post("/ai/formulations/confirm", json={"confirmation_token": token}).status_code == 201

    class BrokenAI(FakeAI):
        def explain(self, payload):
            raise AIUnavailable("offline")
    for c in client(tmp_path / "broken", ai=BrokenAI()):
        response = c.post("/ai/explain", json={"result": {"status": "optimal"}})
        assert response.status_code == 200
        assert response.json()["explanation"] is None
        assert response.json()["available"] is False


def test_job_failure_is_isolated_and_telemetry_is_bounded(tmp_path):
    class SometimesFails(FakeEngine):
        calls = 0
        def solve(self, path, kind, options, emit):
            self.calls += 1
            emit({"type": "one"}); emit({"type": "two"}); emit({"type": "three"})
            if self.calls == 1:
                raise RuntimeError("sensitive internal detail")
            return super().solve(path, kind, options, emit)
    for c in client(tmp_path, engine=SometimesFails()):
        mid = upload(c)
        first = wait_job(c, c.post(f"/models/{mid}/solve", json={"kind":"lp"}).json()["job_id"])
        assert first["state"] == "failed"
        assert first["error"] == "engine operation failed"
        second = wait_job(c, c.post(f"/models/{mid}/solve", json={"kind":"lp"}).json()["job_id"])
        assert second["state"] == "succeeded"
        assert len(c.get(f"/jobs/{second['id']}/telemetry").json()["events"]) == 2


def test_flight_recorder_api_returns_actual_integrity_and_timeline(tmp_path):
    for c in client(tmp_path):
        mid = upload(c)
        created = c.post(f"/models/{mid}/record", json={"kind": "lp", "options": {}})
        assert created.status_code == 201
        recording_id = created.json()["recording_id"]
        replay = c.post(f"/recordings/{recording_id}/replay", json={"reverify": True})
        assert replay.status_code == 200
        assert replay.json()["integrity_passed"] is True
        assert replay.json()["timeline"][0]["type"] == "SOLVE_COMPLETED"
        downloaded = c.get(f"/recordings/{recording_id}/download")
        assert downloaded.status_code == 200
        assert downloaded.headers["content-type"] == "application/zip"


def test_apparent_secret_is_rejected_before_mistral_network_access():
    provider = MistralProvider(api_key="configured-but-unused")
    with pytest.raises(AIUnavailable, match="not sent"):
        provider.propose("password=super-secret formulate a model")
