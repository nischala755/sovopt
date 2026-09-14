"""Checksum-verified Netlib runner for the AstraNiti CLI."""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import platform
import subprocess
import time
import urllib.request
from pathlib import Path


def load_manifest(path: Path) -> list[dict]:
    document = json.loads(path.read_text(encoding="utf-8"))
    if document.get("schema_version") != 1 or not isinstance(document.get("instances"), list):
        raise ValueError("unsupported benchmark manifest")
    required = {"name", "url", "sha256", "mps_format", "published_objective", "objective_tolerance"}
    for entry in document["instances"]:
        if not required <= entry.keys() or len(entry["sha256"]) != 64:
            raise ValueError(f"invalid benchmark entry: {entry.get('name', '<unnamed>')}")
    return document["instances"]


def acquire(entry: dict, cache: Path) -> Path:
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / f"{entry['name'].lower()}.mps.gz"
    if not archive.exists():
        urllib.request.urlretrieve(entry["url"], archive)
    actual = hashlib.sha256(archive.read_bytes()).hexdigest()
    if actual != entry["sha256"]:
        raise ValueError(f"checksum mismatch for {entry['name']}: {actual}")
    model = cache / f"{entry['name'].lower()}.mps"
    model.write_bytes(gzip.decompress(archive.read_bytes()))
    return model


def execute(cli: Path, model: Path, entry: dict) -> dict:
    command = [str(cli), "solve", str(model), "--json", "--mps-format", entry["mps_format"]]
    started = time.perf_counter()
    completed = subprocess.run(command, capture_output=True, text=True, timeout=600, check=False)
    elapsed = time.perf_counter() - started
    result = json.loads(completed.stdout) if completed.stdout.strip().startswith("{") else {}
    objective = result.get("objective")
    agrees = objective is not None and abs(objective-entry["published_objective"]) <= entry["objective_tolerance"]
    return {"label": "ASTRANITI", "instance": entry["name"], "command": subprocess.list2cmdline(command),
            "returncode": completed.returncode, "wall_seconds": elapsed, "result": result,
            "published_objective": entry["published_objective"], "objective_agrees": agrees,
            "stderr": completed.stderr}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=Path(__file__).parent/"manifests/netlib-small.json")
    parser.add_argument("--cache", type=Path, default=Path("build/benchmark-cache"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    records = [execute(args.cli, acquire(entry,args.cache),entry) for entry in load_manifest(args.manifest)]
    report = {"schema_version": 1, "platform": platform.platform(), "python": platform.python_version(), "records": records}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2),encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
