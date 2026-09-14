from __future__ import annotations

import json
import os
import re
from typing import Literal, Protocol

import httpx
from pydantic import BaseModel, ConfigDict, Field


class AIUnavailable(RuntimeError):
    pass


def reject_apparent_secrets(text: str) -> None:
    patterns = (r"-----BEGIN [A-Z ]*PRIVATE KEY-----", r"\b(?:api[_-]?key|password|secret|token)\s*[:=]\s*\S+",
                r"\b(?:sk|key)-[A-Za-z0-9_-]{16,}\b")
    if any(re.search(pattern, text, re.IGNORECASE) for pattern in patterns):
        raise AIUnavailable("request appears to contain a secret and was not sent")


class ProposalVariable(BaseModel):
    model_config = ConfigDict(extra="forbid")
    name: str = Field(pattern=r"^[A-Za-z_][A-Za-z0-9_]{0,63}$")
    lower: float
    upper: float
    type: Literal["continuous", "integer", "binary"]


class ProposalConstraint(BaseModel):
    model_config = ConfigDict(extra="forbid")
    name: str = Field(pattern=r"^[A-Za-z_][A-Za-z0-9_]{0,63}$")
    lower: float
    upper: float
    coefficients: dict[str, float]


class FormulationProposal(BaseModel):
    model_config = ConfigDict(extra="forbid")
    name: str = Field(min_length=1, max_length=64)
    sense: Literal["minimize", "maximize"]
    variables: list[ProposalVariable] = Field(max_length=1000)
    constraints: list[ProposalConstraint] = Field(max_length=1000)
    objective: dict[str, float]
    objective_offset: float = 0


class AIProvider(Protocol):
    def explain(self, payload: dict) -> str: ...
    def propose(self, prompt: str) -> FormulationProposal: ...


class MistralProvider:
    def __init__(self, api_key: str | None = None, model: str = "mistral-small-latest",
                 base_url: str = "https://api.mistral.ai/v1"):
        self._key = api_key or os.getenv("MISTRAL_API_KEY")
        self._model = model
        self._base_url = base_url.rstrip("/")

    def _chat(self, messages: list[dict], *, json_mode: bool = False) -> str:
        if not self._key:
            raise AIUnavailable("MISTRAL_API_KEY is not configured")
        body = {"model": self._model, "messages": messages, "temperature": 0}
        if json_mode:
            body["response_format"] = {"type": "json_object"}
        try:
            response = httpx.post(f"{self._base_url}/chat/completions", json=body,
                                  headers={"Authorization": f"Bearer {self._key}"}, timeout=20)
            response.raise_for_status()
            return response.json()["choices"][0]["message"]["content"]
        except (httpx.HTTPError, KeyError, TypeError, ValueError) as exc:
            raise AIUnavailable("Mistral explanation unavailable") from exc

    def explain(self, payload: dict) -> str:
        return self._chat([{"role": "system", "content": "Explain this optimization metadata. Do not claim independent verification."},
                           {"role": "user", "content": json.dumps(payload, separators=(",", ":"))}])

    def propose(self, prompt: str) -> FormulationProposal:
        reject_apparent_secrets(prompt)
        content = self._chat([{"role": "system", "content": "Return only JSON matching the supplied linear formulation schema; never return executable code."},
                              {"role": "user", "content": prompt[:4000]}], json_mode=True)
        try:
            return FormulationProposal.model_validate_json(content)
        except ValueError as exc:
            raise AIUnavailable("Mistral returned an invalid formulation") from exc


def sanitized_explanation_payload(data: dict) -> dict:
    result = data.get("result") or {}
    model = data.get("model") or {}
    benchmark = data.get("benchmark") or {}
    return {
        "model": {k: model[k] for k in ("variables", "constraints", "nonzeros", "integer_variables", "binary_variables", "density") if k in model},
        "result": {k: result[k] for k in ("status", "objective", "best_bound", "mip_gap", "runtime_seconds", "iterations", "nodes", "verification_passed") if k in result},
        "benchmark": {k: benchmark[k] for k in ("run_count", "statuses", "runtime_seconds") if k in benchmark},
    }
