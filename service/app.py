from __future__ import annotations

import hashlib
import os
import re
import secrets
from contextlib import asynccontextmanager
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path
from threading import BoundedSemaphore
from typing import Literal

from fastapi import FastAPI, Header, HTTPException, Request
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, ConfigDict, Field

from .ai import AIProvider, AIUnavailable, MistralProvider, sanitized_explanation_payload
from .engine import Engine, NativeEngine
from .store import Store


@dataclass(frozen=True)
class Settings:
    data_dir: Path = Path(os.getenv("SOVEREIGN_DATA_DIR", ".sovereign-data"))
    max_upload_bytes: int = int(os.getenv("SOVEREIGN_MAX_UPLOAD_BYTES", 10 * 1024 * 1024))
    max_active_jobs: int = int(os.getenv("SOVEREIGN_MAX_ACTIVE_JOBS", 4))
    max_telemetry_events: int = int(os.getenv("SOVEREIGN_MAX_TELEMETRY_EVENTS", 1000))


class StrictModel(BaseModel): model_config = ConfigDict(extra="forbid")
class SolveRequest(StrictModel):
    kind: str = Field(pattern="^(lp|mip)$")
    options: dict = Field(default_factory=dict)
class VerifyRequest(StrictModel):
    kind: str = Field(pattern="^(primal|optimality|infeasibility|unboundedness)$")
    data: dict
    integer: bool = False
class BenchmarkRequest(StrictModel):
    kind: str = Field(default="lp", pattern="^(lp|mip)$")
    repetitions: int = Field(default=3, ge=1, le=100)
    options: dict = Field(default_factory=dict)
class ExecutionBenchmarkRequest(StrictModel):
    backends: list[Literal["cpu", "gpu", "adaptive"]] = Field(min_length=1, max_length=3)
    repetitions: int = Field(default=3, ge=1, le=100)
class ExplainRequest(StrictModel):
    model_id: str | None = None
    model: dict = Field(default_factory=dict)
    result: dict = Field(default_factory=dict)
    benchmark: dict = Field(default_factory=dict)
class ProposeRequest(StrictModel): prompt: str = Field(min_length=1, max_length=4000)
class ConfirmRequest(StrictModel): confirmation_token: str


def create_app(settings: Settings | None = None, *, engine: Engine | None = None,
               ai_provider: AIProvider | None = None) -> FastAPI:
    cfg = settings or Settings()
    cfg.data_dir.mkdir(parents=True, exist_ok=True)
    uploads = cfg.data_dir / "uploads"; uploads.mkdir(exist_ok=True)
    store, backend = Store(cfg.data_dir / "metadata.sqlite3"), engine or NativeEngine()
    ai = ai_provider or MistralProvider()
    pool = ThreadPoolExecutor(max_workers=cfg.max_active_jobs, thread_name_prefix="sovereign-job")
    slots, proposals = BoundedSemaphore(cfg.max_active_jobs), {}
    @asynccontextmanager
    async def lifespan(_: FastAPI):
        yield
        pool.shutdown(wait=True, cancel_futures=False)
    app = FastAPI(title="Sovereign Optimizer API", version="0.1.0", lifespan=lifespan)

    def model_or_404(model_id: str):
        record=store.model(model_id)
        if not record: raise HTTPException(404,"model not found")
        path=Path(record["path"]).resolve()
        if uploads.resolve() not in path.parents: raise HTTPException(500,"invalid stored model path")
        return record,path

    def submit(kind, work):
        if not slots.acquire(blocking=False): raise HTTPException(429,"active job limit reached")
        jid=store.add_job(kind)
        def run():
            store.set_job(jid,"running")
            try: store.set_job(jid,"succeeded",work(lambda e: store.telemetry(jid,e,cfg.max_telemetry_events)))
            except Exception: store.set_job(jid,"failed",error="engine operation failed")
            finally: slots.release()
        pool.submit(run)
        return {"job_id":jid,"state":"queued"}

    @app.get("/health")
    def health():
        capabilities = backend.capabilities() if backend.available else {"gpu_available":False,"gpu_detail":backend.unavailable_reason}
        return {"status":"ok","engine":"available" if backend.available else "unavailable","reason":backend.unavailable_reason,
                "gpu":"available" if capabilities.get("gpu_available") else "unavailable",
                "gpu_detail":capabilities.get("gpu_detail"),"ai":"available" if os.getenv("MISTRAL_API_KEY") else "unavailable"}

    @app.get("/models")
    def models():
        return [{**r["metadata"],"id":r["id"],"name":r["filename"],"fingerprint":r["sha256"]} for r in store.models()]

    @app.get("/models/{model_id}")
    def model(model_id: str):
        record,_=model_or_404(model_id)
        return {**record["metadata"],"id":model_id,"name":record["filename"],"fingerprint":record["sha256"]}

    @app.post("/models/upload", status_code=201)
    async def upload(request: Request, x_filename: str = Header("model.mps")):
        if not backend.available: raise HTTPException(503,"optimization engine unavailable")
        chunks=[]; size=0
        async for chunk in request.stream():
            size += len(chunk)
            if size>cfg.max_upload_bytes: raise HTTPException(413,"upload exceeds byte limit")
            chunks.append(chunk)
        body=b"".join(chunks)
        if not body: raise HTTPException(400,"empty upload")
        if not re.fullmatch(r"[A-Za-z0-9_.-]{1,128}",x_filename) or Path(x_filename).name != x_filename:
            raise HTTPException(400,"invalid filename")
        digest=hashlib.sha256(body).hexdigest(); path=uploads/f"{secrets.token_hex(16)}.mps"
        with path.open("xb") as output: output.write(body)
        try: metadata=backend.inspect(path)
        except Exception as exc:
            path.unlink(missing_ok=True)
            raise HTTPException(422,f"model validation failed: {exc}") from exc
        mid=store.add_model(path,digest,x_filename); store.metadata(mid,metadata)
        return {"model_id":mid,"sha256":digest,"bytes":len(body)}

    @app.get("/models/{model_id}/fingerprint")
    def fingerprint(model_id: str):
        record,path=model_or_404(model_id)
        optimization=backend.inspect(path) if backend.available else record["metadata"]
        return {"sha256":record["sha256"],"optimization":optimization}

    @app.get("/models/{model_id}/inspect")
    def inspect(model_id: str):
        if not backend.available: raise HTTPException(503,"optimization engine unavailable")
        _,path=model_or_404(model_id)
        try: data=backend.inspect(path); store.metadata(model_id,data); return data
        except Exception as exc: raise HTTPException(422,f"model inspection failed: {exc}") from exc

    @app.post("/models/{model_id}/solve",status_code=202)
    def solve(model_id: str, request: SolveRequest):
        if not backend.available: raise HTTPException(503,"optimization engine unavailable")
        _,path=model_or_404(model_id)
        return submit("solve",lambda emit: backend.solve(path,request.kind,request.options,emit))

    @app.post("/models/{model_id}/verify")
    def verify(model_id: str, request: VerifyRequest):
        if not backend.available: raise HTTPException(503,"optimization engine unavailable")
        _,path=model_or_404(model_id)
        try: return backend.verify(path,request.model_dump())
        except Exception as exc: raise HTTPException(422,f"verification failed: {exc}") from exc

    @app.post("/models/{model_id}/benchmark",status_code=202)
    def benchmark(model_id: str, request: BenchmarkRequest):
        if not backend.available: raise HTTPException(503,"optimization engine unavailable")
        _,path=model_or_404(model_id)
        return submit("benchmark",lambda emit: backend.benchmark(path,request.model_dump(),emit))

    @app.post("/models/{model_id}/execution-benchmark",status_code=202)
    def execution_benchmark(model_id: str, request: ExecutionBenchmarkRequest):
        if not backend.available: raise HTTPException(503,"optimization engine unavailable")
        _,path=model_or_404(model_id)
        return submit("execution_benchmark",lambda _: backend.execution_benchmark(path,request.backends,request.repetitions))

    @app.get("/jobs/{job_id}")
    def job(job_id: str):
        value=store.job(job_id)
        if value is None: raise HTTPException(404,"job not found")
        return value

    @app.get("/jobs/{job_id}/telemetry")
    def telemetry(job_id: str):
        if store.job(job_id) is None: raise HTTPException(404,"job not found")
        return {"events":store.events(job_id),"limit":cfg.max_telemetry_events}

    @app.post("/ai/explain")
    def explain(request: ExplainRequest):
        payload=request.model_dump()
        if request.model_id:
            record,_=model_or_404(request.model_id); payload["model"] = record["metadata"]
        try: return {"available":True,"explanation":ai.explain(sanitized_explanation_payload(payload))}
        except Exception as exc: return {"available":False,"explanation":None,"reason":"AI explanation unavailable" if not isinstance(exc,AIUnavailable) else str(exc)}

    @app.post("/ai/models/explain")
    def explain_model(request: ExplainRequest): return explain(request)

    @app.post("/ai/benchmarks/explain")
    def explain_benchmark(request: ExplainRequest): return explain(request)

    @app.post("/ai/results/explain")
    def explain_result(request: ExplainRequest): return explain(request)

    @app.post("/ai/formulations/propose")
    def propose(request: ProposeRequest):
        try: proposal=ai.propose(request.prompt)
        except Exception as exc: return {"available":False,"proposal":None,"reason":"AI formulation unavailable" if not isinstance(exc,AIUnavailable) else str(exc)}
        if len(proposals) >= 100: raise HTTPException(429,"pending proposal limit reached")
        token=secrets.token_urlsafe(32); proposals[token]=proposal
        return {"available":True,"proposal":proposal,"confirmation_token":token}

    @app.post("/ai/formulations/confirm",status_code=201)
    def confirm(request: ConfirmRequest):
        proposal=proposals.pop(request.confirmation_token,None)
        if proposal is None: raise HTTPException(404,"confirmation token not found or already used")
        if not backend.available: raise HTTPException(503,"optimization engine unavailable")
        return backend.create_model(proposal.model_dump())

    web_dist = Path(os.getenv("SOVEREIGN_WEB_DIST", "web/dist"))
    if web_dist.is_dir():
        app.mount("/", StaticFiles(directory=web_dist, html=True), name="dashboard")
    return app
