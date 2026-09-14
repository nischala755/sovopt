from pathlib import Path
import time

import pytest
from fastapi.testclient import TestClient

from service.app import Settings, create_app
from service.engine import NativeEngine


def test_native_engine_solves_repository_small_lp():
    native = pytest.importorskip("sovereign_optimizer")
    engine = native.NativeEngine()
    model = Path(__file__).parents[2] / "examples" / "small_lp.mps"
    assert engine.inspect(str(model))["nonzeros"] == 4
    result = engine.solve(str(model), "lp", {}, lambda event: None)
    assert result["status"] == "optimal"
    assert result["objective"] == pytest.approx(9.0)
    assert result["verification"]["passed"] is True
    capabilities = engine.capabilities()
    assert capabilities["cpu_available"] is True
    experiments = engine.execution_benchmark(str(model), ["cpu", "gpu", "adaptive"], 1)
    assert [group["backend"] for group in experiments] == ["cpu", "gpu", "adaptive"]
    assert experiments[0]["records"][0]["status"] in {"executed", "converged"}
    if not capabilities["gpu_available"]:
        assert experiments[1]["records"][0]["status"] == "unavailable"


def test_fastapi_contract_runs_native_solve_and_execution_benchmark(tmp_path):
    model = Path(__file__).parents[2] / "examples" / "small_lp.mps"
    app = create_app(Settings(data_dir=tmp_path), engine=NativeEngine())
    with TestClient(app) as client:
        assert client.post("/models/upload", content=b"not an MPS model", headers={"X-Filename":"invalid.mps"}).status_code == 422
        assert client.get("/models").json() == []
        uploaded = client.post("/models/upload", content=model.read_bytes(), headers={"X-Filename":"small_lp.mps"})
        assert uploaded.status_code == 201
        model_id = uploaded.json()["model_id"]
        assert client.get("/models").json()[0]["id"] == model_id
        assert client.get(f"/models/{model_id}/inspect").json()["nonzeros"] == 4
        submitted = client.post(f"/models/{model_id}/solve", json={"kind":"lp","options":{}})
        job_id = submitted.json()["job_id"]
        for _ in range(100):
            solved = client.get(f"/jobs/{job_id}").json()
            if solved["state"] in {"succeeded","failed"}: break
            time.sleep(0.01)
        assert solved["state"] == "succeeded"
        assert solved["result"]["objective"] == pytest.approx(9)
        assert solved["result"]["verification"]["passed"] is True
        bench = client.post(f"/models/{model_id}/execution-benchmark", json={"backends":["cpu","gpu","adaptive"],"repetitions":1})
        benchmark_id = bench.json()["job_id"]
        for _ in range(100):
            measured = client.get(f"/jobs/{benchmark_id}").json()
            if measured["state"] in {"succeeded","failed"}: break
            time.sleep(0.01)
        assert measured["state"] == "succeeded"
        assert [group["backend"] for group in measured["result"]] == ["cpu","gpu","adaptive"]
