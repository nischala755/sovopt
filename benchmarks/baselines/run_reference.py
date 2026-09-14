"""Execute an explicitly supplied reference solver outside AstraNiti."""
from __future__ import annotations

import re
import subprocess
from pathlib import Path
from typing import Sequence


def run_reference(executable: Path, model: Path, arguments: Sequence[str]) -> dict:
    command = [str(executable), *arguments, str(model)]
    rendered = subprocess.list2cmdline(command)
    if not executable.is_file():
        return {"label": "REFERENCE", "available": False, "executed": False,
                "command": rendered, "returncode": None, "objective": None,
                "stdout": "", "stderr": "reference executable unavailable"}
    completed = subprocess.run(command, capture_output=True, text=True, timeout=300, check=False)
    match = re.search(r"(?:objective|Objective)\s*[=:]\s*([-+0-9.eE]+)", completed.stdout)
    objective = float(match.group(1)) if match else None
    return {"label": "REFERENCE", "available": True, "executed": True,
            "command": rendered, "returncode": completed.returncode,
            "objective": objective, "stdout": completed.stdout,
            "stderr": completed.stderr}
