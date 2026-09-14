from __future__ import annotations

from pathlib import Path
from typing import Callable, Protocol


class Engine(Protocol):
    available: bool
    unavailable_reason: str | None
    def capabilities(self) -> dict: ...
    def inspect(self, path: Path) -> dict: ...
    def solve(self, path: Path, kind: str, options: dict, emit: Callable[[dict], None]) -> dict: ...
    def verify(self, path: Path, request: dict) -> dict: ...
    def benchmark(self, path: Path, request: dict, emit: Callable[[dict], None]) -> dict: ...
    def execution_benchmark(self, path: Path, modes: list[str], repetitions: int) -> list[dict]: ...
    def create_model(self, formulation: dict) -> dict: ...
    def record(self, path: Path, bundle: Path, kind: str, options: dict) -> dict: ...
    def replay(self, bundle: Path, reverify: bool) -> dict: ...


class NativeEngine:
    def __init__(self):
        try:
            from sovereign_optimizer import NativeEngine as Impl
            self._impl = Impl()
            self.available, self.unavailable_reason = True, None
        except (ImportError, OSError) as exc:
            self._impl = None
            self.available, self.unavailable_reason = False, str(exc)

    def _require(self):
        if self._impl is None:
            raise RuntimeError(self.unavailable_reason or "native engine unavailable")
        return self._impl

    def capabilities(self) -> dict: return self._require().capabilities()

    def inspect(self, path: Path) -> dict: return self._require().inspect(str(path))
    def solve(self, path: Path, kind: str, options: dict, emit): return self._require().solve(str(path), kind, options, emit)
    def verify(self, path: Path, request: dict): return self._require().verify(str(path), request)
    def benchmark(self, path: Path, request: dict, emit): return self._require().benchmark(str(path), request, emit)
    def execution_benchmark(self, path: Path, modes: list[str], repetitions: int): return self._require().execution_benchmark(str(path), modes, repetitions)
    def create_model(self, formulation: dict): return self._require().create_model(formulation)
    def record(self, path: Path, bundle: Path, kind: str, options: dict): return self._require().record(str(path), str(bundle), kind, options)
    def replay(self, bundle: Path, reverify: bool): return self._require().replay(str(bundle), reverify)
