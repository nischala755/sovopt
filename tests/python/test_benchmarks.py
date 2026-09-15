import json
from pathlib import Path

from benchmarks.run_suite import load_manifest
from benchmarks.baselines.run_reference import run_reference
from benchmarks.baselines.run_highspy import run as run_highspy


ROOT = Path(__file__).parents[2]


def test_netlib_manifest_has_reproducible_verified_metadata():
    entries = load_manifest(ROOT / "benchmarks/manifests/netlib-small.json")
    assert {entry["name"] for entry in entries} >= {"AFIRO", "SC50A", "SC50B"}
    for entry in entries:
        assert entry["url"].startswith("https://")
        assert len(entry["sha256"]) == 64
        assert entry["objective_tolerance"] > 0
    miplib = load_manifest(ROOT / "benchmarks/manifests/miplib-small.json")
    assert miplib[0]["name"] == "flugpl"
    assert miplib[0]["published_objective"] == 1201500.0


def test_reference_adapter_reports_unavailable_without_inventing_results(tmp_path):
    result = run_reference(tmp_path / "missing-solver", tmp_path / "model.mps", [])
    assert result["label"] == "REFERENCE"
    assert result["available"] is False
    assert result["executed"] is False
    assert result["objective"] is None
    assert result["command"]


def test_reference_adapter_records_exact_real_command(tmp_path):
    model = tmp_path / "model.mps"
    model.write_text("NAME X\nENDATA\n", encoding="ascii")
    fake = tmp_path / "reference.py"
    fake.write_text("import sys; print('objective=12.5'); print('fake 1.0', file=sys.stderr)\n", encoding="utf-8")
    result = run_reference(Path(__import__('sys').executable), model, [str(fake)])
    assert result["available"] is True
    assert result["executed"] is True
    assert result["returncode"] == 0
    assert result["objective"] == 12.5
    assert str(model) in result["command"]

def test_highspy_adapter_reports_an_isolated_missing_install(tmp_path):
    result=run_highspy(tmp_path/"missing.mps",tmp_path/"empty-module-directory")
    assert result["label"]=="REFERENCE"
    assert result["executed"] is False
    assert result["objective"] is None
