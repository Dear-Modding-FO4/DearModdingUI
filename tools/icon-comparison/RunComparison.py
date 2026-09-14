from __future__ import annotations

import argparse
from dataclasses import asdict, is_dataclass
import importlib.metadata
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import time

from Baseline import Baseline
from Corpus import CONFIG, SOURCE, freeze_corpus, load_cases, load_config
from Measurements import distribution, footprint, memory
from Models import ARTIFACTS, ROOT, Query, file_hash, load_catalog, preserve_baseline
from Offline import enforce_offline
from Report import write_report
from Scoring import METHODS, choose_threshold, summarize


REPORTS = ARTIFACTS / "Reports"
OPERATING_POINT = ARTIFACTS / "OperatingPoint.json"
HELD_OUT_ACCESS = ARTIFACTS / "HeldOutAccess.json"


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def algorithm_hashes() -> dict:
    directory = Path(__file__).parent
    return {
        name: file_hash(directory / name)
        for name in ("NativeBridge.cpp", "Baseline.py", "Models.py", "LexicalResolver.py",
                     "EmbeddingResolver.py", "Corpus.py", "Scoring.py")
    }


def configuration(require_frozen: bool = False) -> dict:
    config = load_config()
    if OPERATING_POINT.exists():
        point = read_json(OPERATING_POINT)
        if point["corpus_sha256"] != file_hash(SOURCE) or point["config_sha256"] != file_hash(CONFIG):
            raise ValueError("Operating point no longer matches the frozen corpus/configuration")
        if point["algorithm_sha256"] != algorithm_hashes():
            raise ValueError("Resolver/scoring code changed after freezing the operating point")
        config["embedding_threshold"] = point["selected"]["threshold"]
        config["embedding_margin"] = point["selected"]["margin"]
    elif require_frozen:
        raise ValueError("Run development evaluation and tune before accessing held-out results")
    return config


def backend(method: str, config: dict, index_dir: Path | None = None):
    icons = load_catalog()
    options = {"index_dir": index_dir} if index_dir is not None else {}
    if method == "baseline":
        return Baseline()
    if method == "lexical":
        from LexicalResolver import LexicalResolver
        return LexicalResolver(icons, **options)
    if method == "embedding":
        from EmbeddingResolver import EmbeddingResolver
        return EmbeddingResolver(
            icons, threshold=config["embedding_threshold"],
            margin=config["embedding_margin"], threads=config["embedding_threads"],
            **options,
        )
    raise ValueError(f"Unknown comparison method: {method}")


def close(resolver) -> None:
    operation = getattr(resolver, "close", None)
    if operation is not None:
        operation()


def index_metadata(resolver) -> dict | None:
    value = getattr(resolver, "index_info", None)
    return asdict(value) if is_dataclass(value) else value


def provenance(config: dict) -> dict:
    return {
        "corpus_sha256": file_hash(SOURCE),
        "catalog_sha256": file_hash(ARTIFACTS / "Catalog.json"),
        "config_sha256": file_hash(CONFIG),
        "operating_point_sha256": file_hash(OPERATING_POINT) if OPERATING_POINT.exists() else None,
        "configuration": config,
        "native_helper_sha256": file_hash(ARTIFACTS / "Bin" / "dmui-icon-comparison.exe"),
        "api_icon_header_sha256": file_hash(
            ROOT / "Depends" / "commonlibf4" / "lib" / "dearmoddingui-api"
            / "include" / "DearModdingUI" / "IconGlyphs.h"
        ),
        "python": sys.version,
        "platform": platform.platform(),
        "runtime_versions": {
            package: importlib.metadata.version(package)
            for package in ("onnxruntime", "tokenizers", "numpy")
        },
        "source_sha256": {
            path.name: file_hash(path) for path in Path(__file__).parent.glob("*.py")
        },
        "judgment": "provisional authored acceptable sets",
        "network": "Local-file APIs; Hub offline and telemetry disabled; Python socket audit blocks networking. This is not an OS firewall sandbox.",
    }


def evaluate(split: str) -> dict:
    config = configuration(require_frozen=split != "development")
    freeze_corpus(load_catalog())
    if split != "development" and not HELD_OUT_ACCESS.exists():
        write_json(HELD_OUT_ACCESS, {
            "corpus_sha256": file_hash(SOURCE),
            "operating_point_sha256": file_hash(OPERATING_POINT),
            "algorithm_sha256": algorithm_hashes(),
        })
    cases = [case for case in load_cases() if split == "all" or case.split == split]
    resolvers = {}
    preparation = {}
    try:
        for method in METHODS:
            started = time.perf_counter_ns()
            resolvers[method] = backend(method, config)
            preparation[method] = {
                "constructor_ms": (time.perf_counter_ns() - started) / 1e6,
                "index_info": index_metadata(resolvers[method]),
            }
        rows = []
        for case in cases:
            baseline = resolvers["baseline"].resolve(case.query)
            raw = {
                method: resolvers[method].resolve(case.query)
                for method in ("lexical", "embedding")
            }
            rows.append({
                **asdict(case),
                "raw": {method: value.to_dict() for method, value in raw.items()},
                "final": {
                    "baseline": baseline.to_dict(),
                    **{
                        method: preserve_baseline(baseline, value).to_dict()
                        for method, value in raw.items()
                    },
                },
            })
        total_bytes = footprint(ARTIFACTS)
        if total_bytes > config["artifact_limit_bytes"]:
            raise RuntimeError(f"Experiment artifact limit exceeded: {total_bytes} bytes")
        return {
            "provenance": provenance(config), "preparation": preparation,
            "rows": rows, "summary": summarize(rows),
        }
    finally:
        for resolver in resolvers.values():
            close(resolver)


def query_one(query: Query) -> dict:
    config = configuration(require_frozen=True)
    query.validate()
    results = {"query": asdict(query), "raw": {}, "final": {}}
    with Baseline() as baseline:
        selected = baseline.resolve(query)
    results["final"]["baseline"] = selected.to_dict()
    if query.explicit_glyph is not None or (
        query.explicit_name and selected.reason.startswith("explicit")
    ):
        for method in ("lexical", "embedding"):
            results["final"][method] = selected.to_dict()
        results["note"] = "Authoritative explicit selection: no semantic inference performed"
        return results
    for method in ("lexical", "embedding"):
        resolver = backend(method, config)
        try:
            raw = resolver.resolve(query)
            results["raw"][method] = raw.to_dict()
            results["final"][method] = preserve_baseline(selected, raw).to_dict()
        finally:
            close(resolver)
    return results


def worker(method: str, phase: str) -> dict:
    config = configuration(require_frozen=True)
    temporary = None
    if phase == "preparation":
        (ARTIFACTS / "Cache").mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(prefix="Preparation-", dir=ARTIFACTS / "Cache")
    started = time.perf_counter_ns()
    try:
        resolver = backend(method, config, Path(temporary.name) if temporary is not None else None)
    except Exception:
        if temporary is not None:
            temporary.cleanup()
        raise
    initialized = time.perf_counter_ns()
    try:
        idle = memory()
        first_started = time.perf_counter_ns()
        resolver.resolve(Query("Nourishment"))
        first_ms = (time.perf_counter_ns() - first_started) / 1e6
        result = {
            "method": method, "phase": phase,
            "initialization_ms": (initialized - started) / 1e6,
            "first_query_ms": first_ms, "idle_memory": idle,
        }
        if phase == "warm":
            queries = [case.query for case in load_cases()]
            for query in queries[:config["warmup_queries"]]:
                resolver.resolve(query)
            latencies, native_latencies = [], []
            reference = []
            changed = 0
            max_score_delta = 0.0
            for repetition in range(config["warm_repetitions"]):
                for index, query in enumerate(queries):
                    before = time.perf_counter_ns()
                    selected = resolver.resolve(query)
                    latencies.append((time.perf_counter_ns() - before) / 1e6)
                    if method == "baseline":
                        native_latencies.append(resolver.last_native_ns / 1e6)
                    identity = (selected.status, selected.glyph, tuple(c.glyph for c in selected.candidates))
                    scores = tuple(c.score for c in selected.candidates)
                    if repetition == 0:
                        reference.append((identity, scores))
                    else:
                        previous, previous_scores = reference[index]
                        changed += identity != previous
                        if len(scores) != len(previous_scores):
                            raise ValueError("Repeated inference changed candidate count")
                        max_score_delta = max(max_score_delta, max(
                            (abs(a - b) for a, b in zip(scores, previous_scores)
                             if a is not None and b is not None), default=0.0,
                        ))
            result.update(
                latency_ms=distribution(latencies),
                repeated_selection_changes=changed,
                max_repeated_score_delta=max_score_delta,
                compared_repeats=len(queries) * (config["warm_repetitions"] - 1),
            )
            if native_latencies:
                result["native_resolver_ms"] = distribution(native_latencies)
        result["final_memory"] = memory()
        result["index_info"] = index_metadata(resolver)
        if method == "baseline":
            result["native_memory"] = memory(resolver.process.pid)
        return result
    finally:
        close(resolver)
        if temporary is not None:
            temporary.cleanup()


def measure() -> dict:
    if not (REPORTS / "HeldOut.json").exists():
        raise ValueError("Complete frozen held-out evaluation before benchmarking its queries")
    config = configuration(require_frozen=True)
    measurements = {
        "provenance": provenance(config),
        "acquisition_receipt": read_json(ARTIFACTS / "Indexes" / "AcquisitionIndex.json"),
        "initial_development_preparation": read_json(REPORTS / "Development.json")["preparation"],
        "processor": platform.processor(),
        "logical_processors": os.cpu_count(),
        "cpu_note": "Planning probe: AMD Ryzen 7 7800X3D, 8 cores / 16 threads, 63.1 GiB RAM",
        "cold_definition": "Fresh process with prepared index, empty query state, unchanged OS file cache",
        "warm_definition": "Batch-one uncached queries, ten warmups, three corpus passes, CPU provider with one intra-op thread",
        "threading_note": "ORT intra-op/inter-op threads are both one and tokenizer parallelism is disabled. NumPy BLAS threading is not separately pinned; this is not a claim that the whole process has one thread.",
        "methods": {},
    }
    for method in METHODS:
        cold = []
        warm = None
        preparation = None
        for phase in ["preparation"] + ["cold"] * config["cold_processes"] + ["warm"]:
            started = time.perf_counter_ns()
            completed = subprocess.run(
                [sys.executable, str(Path(__file__).resolve()), "worker", "--method", method, "--phase", phase],
                check=True, capture_output=True, text=True, encoding="utf-8",
            )
            data = json.loads(completed.stdout)
            data["whole_process_ms"] = (time.perf_counter_ns() - started) / 1e6
            if phase == "preparation":
                preparation = data
            elif phase == "cold":
                cold.append(data)
            else:
                warm = data
        measurements["methods"][method] = {
            "cold_runs": cold,
            "preparation": preparation,
            "initialization_ms": distribution([entry["initialization_ms"] for entry in cold]),
            "first_query_ms": distribution([entry["first_query_ms"] for entry in cold]),
            "whole_process_ms": distribution([entry["whole_process_ms"] for entry in cold]),
            "warm": warm,
        }
    measurements["artifact_bytes"] = {
        path.name: footprint(path) if path.is_dir() else path.stat().st_size
        for path in ARTIFACTS.iterdir()
    }
    measurements["artifact_bytes"]["total"] = footprint(ARTIFACTS)
    if measurements["artifact_bytes"]["total"] > config["artifact_limit_bytes"]:
        raise RuntimeError("Experiment artifacts exceeded the approved disk limit")
    write_json(REPORTS / "Measurements.json", measurements)
    comparison_path = REPORTS / "Comparison.json"
    if comparison_path.exists():
        document = read_json(comparison_path)
        document["measurements"] = measurements
        write_json(comparison_path, document)
        write_report(document, REPORTS / "Comparison.html")
    return measurements


def main() -> None:
    parser = argparse.ArgumentParser(description="Offline, opt-in semantic icon comparison")
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("prepare")
    evaluation = subparsers.add_parser("evaluate")
    evaluation.add_argument("--split", choices=("development", "held_out", "all"), required=True)
    subparsers.add_parser("tune")
    subparsers.add_parser("benchmark")
    subparsers.add_parser("report")
    query = subparsers.add_parser("query")
    query.add_argument("--heading", required=True)
    query.add_argument("--context", default="")
    query.add_argument("--explicit-name", default="")
    query.add_argument("--explicit-glyph", type=lambda value: int(value, 0))
    worker_parser = subparsers.add_parser("worker", help=argparse.SUPPRESS)
    worker_parser.add_argument("--method", choices=METHODS, required=True)
    worker_parser.add_argument("--phase", choices=("preparation", "cold", "warm"), required=True)
    args = parser.parse_args()
    enforce_offline()
    if args.command == "prepare":
        with Baseline() as baseline:
            baseline.export_catalog()
        print(json.dumps(freeze_corpus(load_catalog()), indent=2))
    elif args.command == "evaluate":
        document = evaluate(args.split)
        name = {"development": "Development", "held_out": "HeldOut", "all": "Comparison"}[args.split]
        write_json(REPORTS / f"{name}.json", document)
        write_report(document, REPORTS / f"{name}.html")
        print(json.dumps({"report": str(REPORTS / f"{name}.html"), "queries": len(document["rows"])}, indent=2))
    elif args.command == "tune":
        if HELD_OUT_ACCESS.exists() or (REPORTS / "HeldOut.json").exists() or (REPORTS / "Comparison.json").exists():
            raise ValueError("Held-out results already exist; tuning is closed")
        document = read_json(REPORTS / "Development.json")
        if document["provenance"]["corpus_sha256"] != file_hash(SOURCE):
            raise ValueError("Development results belong to a different corpus")
        if document["provenance"]["config_sha256"] != file_hash(CONFIG):
            raise ValueError("Development results belong to different evaluation settings")
        point = choose_threshold(document["rows"], load_config())
        point.update(corpus_sha256=file_hash(SOURCE), config_sha256=file_hash(CONFIG),
                     algorithm_sha256=algorithm_hashes())
        write_json(OPERATING_POINT, point)
        print(json.dumps(point["selected"], indent=2))
    elif args.command == "benchmark":
        data = measure()
        print(json.dumps({"measurements": str(REPORTS / "Measurements.json"),
                          "artifact_bytes": data["artifact_bytes"]["total"]}, indent=2))
    elif args.command == "query":
        print(json.dumps(query_one(Query(args.heading, args.context, args.explicit_name, args.explicit_glyph)), indent=2))
    elif args.command == "report":
        document = read_json(REPORTS / "Comparison.json")
        write_report(document, REPORTS / "Comparison.html")
        print(str(REPORTS / "Comparison.html"))
    elif args.command == "worker":
        print(json.dumps(worker(args.method, args.phase)))


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, ImportError, subprocess.CalledProcessError) as error:
        print(f"Comparison failed: {error}", file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError) and error.stderr:
            print(error.stderr, file=sys.stderr)
        sys.exit(1)
