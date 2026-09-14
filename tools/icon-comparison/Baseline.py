from __future__ import annotations

from dataclasses import asdict
import json
from pathlib import Path
import subprocess

from Models import ARTIFACTS, Query, Selection


class Baseline:
    def __init__(self, executable: Path = ARTIFACTS / "Bin" / "dmui-icon-comparison.exe"):
        if not executable.is_file():
            raise FileNotFoundError(f"Build the opt-in dmui-icon-comparison target: {executable}")
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, encoding="utf-8",
        )
        self.last_native_ns = 0

    def _call(self, request: dict) -> dict:
        if self.process.poll() is not None:
            raise RuntimeError(f"Native helper exited: {self.process.stderr.read()}")
        self.process.stdin.write(json.dumps(request, ensure_ascii=True) + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError(f"Native helper closed its output: {self.process.stderr.read()}")
        result = json.loads(line)
        if "error" in result:
            raise ValueError(f"Native helper rejected request: {result['error']}")
        return result

    def export_catalog(self, path: Path = ARTIFACTS / "Catalog.json") -> None:
        result = self._call({"op": "catalog"})
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    def resolve(self, query: Query) -> Selection:
        query.validate()
        result = self._call({"op": "resolve", "requests": [asdict(query)]})["results"][0]
        self.last_native_ns = result["elapsed_ns"]
        if result["status"] not in {"selected", "no_match", "invalid_raw_glyph"}:
            raise ValueError("Unknown native selection status")
        return Selection(
            status=result["status"], glyph=result["glyph"],
            name=result["name"], reason=result["reason"],
        )

    def close(self) -> None:
        if self.process.poll() is None:
            self.process.stdin.close()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.terminate()
                self.process.wait(timeout=5)
        self.process.stdout.close()
        self.process.stderr.close()

    def __enter__(self) -> Baseline:
        return self

    def __exit__(self, *_: object) -> None:
        self.close()
