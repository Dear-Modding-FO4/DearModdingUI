from __future__ import annotations

import json
import math
import os
from pathlib import Path, PurePosixPath
import time
from typing import Iterable

from Acquire import ACQUISITION_MANIFEST, load_manifest, sha256_hash, verify_artifact
from Models import ARTIFACTS, Candidate, Icon, Query, Selection, content_hash


DIMENSIONS = 384
MAX_TOKENS = 512
QUERY_CONTEXT_SEPARATOR = " Context: "
CATALOG_TEMPLATE = "{descriptor}"
CACHE_SCHEMA = 1
MODEL_FILES = (
    "model.onnx",
    "tokenizer.json",
    "vocab.txt",
    "config.json",
    "tokenizer_config.json",
    "1_Pooling/config.json",
    "special_tokens_map.json",
    "modules.json",
    "sentence_bert_config.json",
)


class EmbeddingError(RuntimeError):
    pass


def _read_json(path: Path):
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise EmbeddingError(f"Missing model configuration: {path}") from error
    except (OSError, json.JSONDecodeError) as error:
        raise EmbeddingError(f"Invalid model configuration {path}: {error}") from error
    return payload


def _validate_model_contract(model_dir: Path) -> None:
    config = _read_json(model_dir / "config.json")
    if not isinstance(config, dict):
        raise EmbeddingError("BGE model configuration must be an object")
    if (
        config.get("model_type") != "bert"
        or config.get("hidden_size") != DIMENSIONS
        or config.get("pad_token_id") != 0
        or config.get("max_position_embeddings", 0) < MAX_TOKENS
    ):
        raise EmbeddingError(
            "BGE model configuration does not match the frozen BERT contract"
        )

    tokenizer_config = _read_json(model_dir / "tokenizer_config.json")
    if not isinstance(tokenizer_config, dict):
        raise EmbeddingError("Tokenizer configuration must be an object")
    if tokenizer_config.get("model_max_length") != MAX_TOKENS:
        raise EmbeddingError("Tokenizer maximum length must be 512")

    special_tokens = _read_json(model_dir / "special_tokens_map.json")
    if not isinstance(special_tokens, dict):
        raise EmbeddingError("Tokenizer special token map must be an object")
    expected_tokens = {
        "cls_token": "[CLS]",
        "mask_token": "[MASK]",
        "pad_token": "[PAD]",
        "sep_token": "[SEP]",
        "unk_token": "[UNK]",
    }
    if any(
        special_tokens.get(name) != value for name, value in expected_tokens.items()
    ):
        raise EmbeddingError(
            "Tokenizer special tokens do not match the frozen BERT contract"
        )

    pooling = _read_json(model_dir / "1_Pooling" / "config.json")
    if not isinstance(pooling, dict):
        raise EmbeddingError("Pooling configuration must be an object")
    if pooling.get("word_embedding_dimension") != DIMENSIONS:
        raise EmbeddingError("Pooling dimension must be 384")
    pooling_modes = {
        key: value for key, value in pooling.items() if key.startswith("pooling_mode_")
    }
    if pooling_modes.get("pooling_mode_cls_token") is not True or any(
        value is not False
        for key, value in pooling_modes.items()
        if key != "pooling_mode_cls_token"
    ):
        raise EmbeddingError("Only CLS pooling is permitted")

    sentence_config = _read_json(model_dir / "sentence_bert_config.json")
    if not isinstance(sentence_config, dict):
        raise EmbeddingError("Sentence BERT configuration must be an object")
    if sentence_config.get("max_seq_length") != MAX_TOKENS:
        raise EmbeddingError("Sentence BERT maximum length must be 512")

    modules = _read_json(model_dir / "modules.json")
    if not isinstance(modules, list):
        raise EmbeddingError("Sentence-transformers modules must be a list")
    module_types = [entry.get("type") for entry in modules if isinstance(entry, dict)]
    module_paths = [entry.get("path") for entry in modules if isinstance(entry, dict)]
    if module_types != [
        "sentence_transformers.models.Transformer",
        "sentence_transformers.models.Pooling",
        "sentence_transformers.models.Normalize",
    ] or module_paths != ["", "1_Pooling", "2_Normalize"]:
        raise EmbeddingError(
            "Sentence-transformers modules must be Transformer, CLS Pooling, Normalize"
        )


def _format_query(query: Query) -> str:
    heading = " ".join(query.heading.split())
    context = " ".join(query.context.split())
    if not heading:
        return ""
    return heading + (QUERY_CONTEXT_SEPARATOR + context if context else "")


def _concise_descriptor(icon: Icon, limit: int = 180) -> str:
    descriptor = icon.descriptor
    if len(descriptor) <= limit:
        return descriptor
    return descriptor[: limit - 3].rstrip() + "..."


class EmbeddingResolver:
    def __init__(
        self,
        icons: tuple[Icon, ...],
        threshold: float = 0.85,
        margin: float = 0.02,
        model_dir: Path = ARTIFACTS / "Model",
        index_dir: Path = ARTIFACTS / "Indexes",
        threads: int = 1,
    ):
        if len(icons) < 3:
            raise ValueError("icons must contain at least three entries")
        if len({icon.glyph for icon in icons}) != len(icons):
            raise ValueError("icons must have unique glyphs")
        if (
            isinstance(threshold, bool)
            or not isinstance(threshold, (int, float))
            or not math.isfinite(threshold)
            or not -1.0 <= threshold <= 1.0
        ):
            raise ValueError("threshold must be a finite cosine in [-1, 1]")
        if (
            isinstance(margin, bool)
            or not isinstance(margin, (int, float))
            or not math.isfinite(margin)
            or not 0.0 <= margin <= 2.0
        ):
            raise ValueError("margin must be a finite cosine gap in [0, 2]")
        if type(threads) is not int or threads <= 0:
            raise ValueError("threads must be a positive integer")

        self._icons = tuple(sorted(icons, key=lambda icon: icon.glyph))
        self._threshold = float(threshold)
        self._margin = float(margin)
        self._model_dir = Path(model_dir)
        self._index_dir = Path(index_dir)
        self._threads = threads
        self._session = None
        self._tokenizer = None
        self._np = None
        self._output_name = ""
        self._closed = False
        self.preprocessing: dict[str, object] = {}

        started = time.perf_counter()
        hashes = self._verify_model_artifacts()
        _validate_model_contract(self._model_dir)
        np, ort, tokenizers = self._load_dependencies()
        self._np = np
        self._tokenizers_version = tokenizers.__version__
        self._tokenizer = tokenizers.Tokenizer.from_file(
            str(self._model_dir / "tokenizer.json")
        )
        self._configure_tokenizer()
        self._session = self._create_session(ort)
        self._output_name = self._validate_graph()
        session_options = self._session.get_session_options()
        if (
            session_options.intra_op_num_threads != threads
            or session_options.inter_op_num_threads != 1
        ):
            raise EmbeddingError(
                "ONNX Runtime did not retain the requested CPU threads"
            )
        cache_key, key_inputs = self._cache_identity(hashes, np, ort)
        vectors, cache_hit, stale_count = self._load_or_build_cache(
            cache_key, key_inputs
        )
        self._vectors = vectors
        self.preprocessing = {
            "status": "cache_hit" if cache_hit else "built",
            "cache_hit": cache_hit,
            "cache_key": cache_key,
            "path": str(self._cache_paths(cache_key)[0]),
            "size_bytes": self._cache_paths(cache_key)[0].stat().st_size,
            "stale_cache_count": stale_count,
            "seconds": time.perf_counter() - started,
            "provider": "CPUExecutionProvider",
            "intra_op_threads": session_options.intra_op_num_threads,
            "inter_op_threads": session_options.inter_op_num_threads,
            "catalog_vectors": len(self._icons),
            "dimensions": DIMENSIONS,
            "cross_run_bitwise_guarantee": False,
        }
        self.index_info = dict(self.preprocessing)

    @property
    def diagnostics(self) -> dict[str, object]:
        return dict(self.preprocessing)

    def _verify_model_artifacts(self) -> dict[str, str]:
        manifest = load_manifest(ACQUISITION_MANIFEST)
        expected = {
            str(artifact.destination.relative_to("Model")): artifact
            for artifact in manifest.artifacts
            if artifact.destination.parts[0] == "Model"
            and artifact.destination.name != "README.md"
        }
        if set(expected) != set(MODEL_FILES):
            raise EmbeddingError(
                "Acquisition manifest does not match required model files"
            )
        hashes = {}
        for relative, artifact in expected.items():
            path = self._model_dir.joinpath(*PurePosixPath(relative).parts)
            valid, reason = verify_artifact(path, artifact)
            if not valid:
                raise EmbeddingError(f"Model artifact {relative} is invalid: {reason}")
            hashes[relative] = sha256_hash(path)
        return hashes

    @staticmethod
    def _load_dependencies():
        os.environ.setdefault("HF_HUB_OFFLINE", "1")
        os.environ.setdefault("HF_HUB_DISABLE_TELEMETRY", "1")
        os.environ.setdefault("HF_HUB_DISABLE_XET", "1")
        os.environ.setdefault("TRANSFORMERS_OFFLINE", "1")
        os.environ.setdefault("TOKENIZERS_PARALLELISM", "false")
        try:
            import numpy as np
            import onnxruntime as ort
            import tokenizers
        except ImportError as error:
            raise EmbeddingError(
                "Offline embedding dependencies are unavailable; run "
                "Acquire.py download and Acquire.py prepare explicitly"
            ) from error
        return np, ort, tokenizers

    def _configure_tokenizer(self) -> None:
        required_tokens = {
            "[PAD]": 0,
            "[UNK]": 100,
            "[CLS]": 101,
            "[SEP]": 102,
            "[MASK]": 103,
        }
        actual = {
            token: self._tokenizer.token_to_id(token) for token in required_tokens
        }
        if actual != required_tokens:
            raise EmbeddingError(f"Tokenizer token IDs do not match BERT: {actual}")
        self._tokenizer.enable_truncation(max_length=MAX_TOKENS)
        self._tokenizer.enable_padding(
            direction="right",
            pad_id=0,
            pad_type_id=0,
            pad_token="[PAD]",
        )

    def _create_session(self, ort):
        options = ort.SessionOptions()
        options.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
        options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        options.intra_op_num_threads = self._threads
        options.inter_op_num_threads = 1
        try:
            return ort.InferenceSession(
                str(self._model_dir / "model.onnx"),
                sess_options=options,
                providers=["CPUExecutionProvider"],
            )
        except Exception as error:
            raise EmbeddingError(
                f"Could not load the pinned ONNX graph: {error}"
            ) from error

    def _validate_graph(self) -> str:
        providers = self._session.get_providers()
        if providers != ["CPUExecutionProvider"]:
            raise EmbeddingError(f"Unexpected ONNX execution providers: {providers}")
        inputs = {item.name: item for item in self._session.get_inputs()}
        expected_inputs = {"input_ids", "attention_mask", "token_type_ids"}
        if set(inputs) != expected_inputs:
            raise EmbeddingError(
                f"ONNX inputs are {sorted(inputs)}, expected {sorted(expected_inputs)}"
            )
        for name, item in inputs.items():
            if item.type != "tensor(int64)" or len(item.shape) != 2:
                raise EmbeddingError(
                    f"ONNX input {name} must be a rank-two int64 tensor"
                )
        outputs = {item.name: item for item in self._session.get_outputs()}
        output = outputs.get("last_hidden_state")
        if (
            output is None
            or output.type != "tensor(float)"
            or len(output.shape) != 3
            or output.shape[-1] != DIMENSIONS
        ):
            summary = {
                name: {"type": item.type, "shape": item.shape}
                for name, item in outputs.items()
            }
            raise EmbeddingError(
                f"ONNX graph has no valid last_hidden_state output: {summary}"
            )
        return output.name

    def _cache_identity(self, hashes: dict[str, str], np, ort) -> tuple[str, dict]:
        key_inputs = {
            "schema": CACHE_SCHEMA,
            "model_hash": hashes["model.onnx"],
            "content_hash": content_hash(
                {
                    name: digest
                    for name, digest in hashes.items()
                    if name != "model.onnx"
                }
            ),
            "template_hash": content_hash(
                {
                    "query_context_separator": QUERY_CONTEXT_SEPARATOR,
                    "catalog_template": CATALOG_TEMPLATE,
                }
            ),
            "config_hash": content_hash(
                {
                    "dimensions": DIMENSIONS,
                    "max_tokens": MAX_TOKENS,
                    "pooling": "CLS[:,0]+L2",
                    "padding": "dynamic-right",
                    "numpy": np.__version__,
                    "onnxruntime": ort.__version__,
                    "tokenizers": self._tokenizers_version,
                }
            ),
            "catalog_hash": content_hash(
                [
                    {
                        "glyph": icon.glyph,
                        "name": icon.name,
                        "descriptor": CATALOG_TEMPLATE.format(
                            descriptor=icon.descriptor
                        ),
                    }
                    for icon in self._icons
                ]
            ),
        }
        return content_hash(key_inputs), key_inputs

    def _cache_paths(self, cache_key: str) -> tuple[Path, Path]:
        stem = self._index_dir / f"Embedding-{cache_key}"
        return stem.with_suffix(".npz"), stem.with_suffix(".json")

    def _load_or_build_cache(self, cache_key: str, key_inputs: dict):
        self._index_dir.mkdir(parents=True, exist_ok=True)
        data_path, metadata_path = self._cache_paths(cache_key)
        data_exists = data_path.exists()
        metadata_exists = metadata_path.exists()
        stale_count = len(
            [
                path
                for path in self._index_dir.glob("Embedding-*.json")
                if path != metadata_path
            ]
        )
        if data_exists != metadata_exists:
            raise EmbeddingError(
                f"Incomplete embedding cache; delete {data_path.name} and "
                f"{metadata_path.name} to rebuild offline"
            )
        if data_exists:
            return (
                self._read_cache(data_path, metadata_path, cache_key, key_inputs),
                True,
                stale_count,
            )
        descriptors = [
            CATALOG_TEMPLATE.format(descriptor=icon.descriptor) for icon in self._icons
        ]
        vectors = self._embed_texts(descriptors)
        self._validate_vectors(vectors, len(self._icons), "generated catalog cache")
        glyphs = self._np.asarray(
            [icon.glyph for icon in self._icons], dtype=self._np.int64
        )
        temporary_data = data_path.with_suffix(".npz.tmp")
        with temporary_data.open("wb") as stream:
            self._np.savez(stream, vectors=vectors, glyphs=glyphs)
        metadata = {
            "schema": CACHE_SCHEMA,
            "cache_key": cache_key,
            "key_inputs": key_inputs,
            "data_sha256": sha256_hash(temporary_data),
            "rows": len(self._icons),
            "dimensions": DIMENSIONS,
            "dtype": "float32",
        }
        temporary_metadata = metadata_path.with_suffix(".json.tmp")
        temporary_metadata.write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        temporary_data.replace(data_path)
        temporary_metadata.replace(metadata_path)
        return vectors, False, stale_count

    def _read_cache(
        self, data_path: Path, metadata_path: Path, cache_key: str, key_inputs: dict
    ):
        try:
            metadata = _read_json(metadata_path)
            if (
                metadata.get("schema") != CACHE_SCHEMA
                or metadata.get("cache_key") != cache_key
                or metadata.get("key_inputs") != key_inputs
                or metadata.get("rows") != len(self._icons)
                or metadata.get("dimensions") != DIMENSIONS
                or metadata.get("dtype") != "float32"
                or metadata.get("data_sha256") != sha256_hash(data_path)
            ):
                raise EmbeddingError("Embedding cache metadata or digest is stale")
            with self._np.load(data_path, allow_pickle=False) as archive:
                if set(archive.files) != {"vectors", "glyphs"}:
                    raise EmbeddingError("Embedding cache contains unexpected arrays")
                vectors = archive["vectors"]
                glyphs = archive["glyphs"]
            expected_glyphs = self._np.asarray(
                [icon.glyph for icon in self._icons], dtype=self._np.int64
            )
            if glyphs.dtype != self._np.int64 or not self._np.array_equal(
                glyphs, expected_glyphs
            ):
                raise EmbeddingError("Embedding cache catalog identities are stale")
            self._validate_vectors(vectors, len(self._icons), "cached catalog vectors")
            return vectors
        except EmbeddingError:
            raise
        except Exception as error:
            raise EmbeddingError(
                f"Embedding cache is corrupt; delete {data_path.name} and "
                f"{metadata_path.name} to rebuild offline: {error}"
            ) from error

    def _encode(self, texts: list[str]) -> dict[str, object]:
        encodings = self._tokenizer.encode_batch(texts)
        if not encodings:
            raise EmbeddingError("Tokenizer returned no sequences")
        length = len(encodings[0].ids)
        if length == 0 or length > MAX_TOKENS:
            raise EmbeddingError("Tokenizer produced an invalid sequence length")
        rows = {"input_ids": [], "attention_mask": [], "token_type_ids": []}
        for encoding in encodings:
            if not (
                len(encoding.ids)
                == len(encoding.attention_mask)
                == len(encoding.type_ids)
                == length
            ):
                raise EmbeddingError("Tokenizer batch was not padded consistently")
            if encoding.ids[0] != 101:
                raise EmbeddingError("Tokenizer sequence does not begin with CLS")
            active = sum(encoding.attention_mask)
            if (
                any(mask not in (0, 1) for mask in encoding.attention_mask)
                or encoding.attention_mask != [1] * active + [0] * (length - active)
                or active < 2
                or encoding.ids[active - 1] != 102
                or any(encoding.type_ids)
            ):
                raise EmbeddingError(
                    "Tokenizer sequence masks violate the BERT contract"
                )
            rows["input_ids"].append(encoding.ids)
            rows["attention_mask"].append(encoding.attention_mask)
            rows["token_type_ids"].append(encoding.type_ids)
        return {
            name: self._np.asarray(values, dtype=self._np.int64)
            for name, values in rows.items()
        }

    def _embed_texts(self, texts: Iterable[str], batch_size: int = 32):
        items = list(texts)
        if not items:
            return self._np.empty((0, DIMENSIONS), dtype=self._np.float32)
        batches = []
        for offset in range(0, len(items), batch_size):
            inputs = self._encode(items[offset : offset + batch_size])
            try:
                hidden = self._session.run([self._output_name], inputs)[0]
            except Exception as error:
                raise EmbeddingError(f"ONNX inference failed: {error}") from error
            if (
                hidden.ndim != 3
                or hidden.shape[0] != len(inputs["input_ids"])
                or hidden.shape[2] != DIMENSIONS
            ):
                raise EmbeddingError(
                    f"ONNX output has invalid shape {tuple(hidden.shape)}"
                )
            pooled = self._np.asarray(hidden[:, 0, :], dtype=self._np.float32)
            norms = self._np.linalg.norm(pooled, axis=1, keepdims=True)
            if not self._np.all(self._np.isfinite(norms)) or self._np.any(norms <= 0):
                raise EmbeddingError(
                    "ONNX output contains a zero or non-finite CLS vector"
                )
            batches.append(pooled / norms)
        vectors = self._np.concatenate(batches, axis=0)
        self._validate_vectors(vectors, len(items), "inference output")
        return vectors

    def _validate_vectors(self, vectors, rows: int, source: str) -> None:
        if vectors.dtype != self._np.float32 or vectors.shape != (rows, DIMENSIONS):
            raise EmbeddingError(
                f"{source} must be float32 with shape ({rows}, {DIMENSIONS})"
            )
        if not self._np.all(self._np.isfinite(vectors)):
            raise EmbeddingError(f"{source} contains non-finite values")
        norms = self._np.linalg.norm(vectors, axis=1)
        if not self._np.allclose(norms, 1.0, rtol=1e-5, atol=1e-6):
            raise EmbeddingError(f"{source} contains non-normalized vectors")

    def resolve(self, query: Query) -> Selection:
        if self._closed:
            raise EmbeddingError("Embedding resolver is closed")
        query.validate()
        text = _format_query(query)
        if not text:
            return Selection(status="no_match", reason="empty normalized heading")
        vector = self._embed_texts([text])[0]
        scores = self._vectors @ vector
        order = sorted(
            range(len(self._icons)),
            key=lambda index: (-float(scores[index]), self._icons[index].glyph),
        )
        top = order[:3]
        candidates = tuple(
            Candidate(
                glyph=self._icons[index].glyph,
                name=self._icons[index].name,
                score=float(scores[index]),
                reason=f"nearest descriptor: {_concise_descriptor(self._icons[index])}",
            )
            for index in top
        )
        best = candidates[0]
        gap = best.score - candidates[1].score
        if best.score < self._threshold:
            return Selection(
                status="no_match",
                reason=(
                    f"top cosine {best.score:.6f} is below threshold "
                    f"{self._threshold:.6f}"
                ),
                candidates=candidates,
                margin=gap,
            )
        if gap < self._margin:
            return Selection(
                status="no_match",
                reason=(
                    f"top-two cosine gap {gap:.6f} is below margin "
                    f"{self._margin:.6f}"
                ),
                candidates=candidates,
                margin=gap,
            )
        return Selection(
            status="selected",
            glyph=best.glyph,
            name=best.name,
            reason=f"cosine {best.score:.6f}, top-two gap {gap:.6f}",
            candidates=candidates,
            margin=gap,
        )

    def close(self) -> None:
        self._session = None
        self._tokenizer = None
        self._closed = True
