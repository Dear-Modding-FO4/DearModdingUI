from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest


TOOL_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL_DIR))

from EmbeddingResolver import (
    DIMENSIONS,
    EmbeddingError,
    EmbeddingResolver,
    _format_query,
    _validate_model_contract,
)
from Models import ARTIFACTS, Icon, Query


GENERIC_ICONS = (
    Icon(0x100, "house", aliases=("home",), tags=("building",)),
    Icon(0x101, "gear", aliases=("cog",), tags=("settings",)),
    Icon(0x102, "heart", tags=("health", "favorite")),
    Icon(0x103, "folder", tags=("files", "directory")),
)
TEST_TEMP = ARTIFACTS / "Cache" / "Tests"


class EmbeddingContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        TEST_TEMP.mkdir(parents=True, exist_ok=True)

    def test_query_template_is_frozen_and_whitespace_normalized(self) -> None:
        self.assertEqual(
            _format_query(Query("  display   settings ", " user   interface ")),
            "display settings Context: user interface",
        )
        self.assertEqual(_format_query(Query(" \t ")), "")

    def test_missing_model_artifacts_are_rejected_before_import(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as directory:
            with self.assertRaisesRegex(EmbeddingError, "Model artifact"):
                EmbeddingResolver(
                    GENERIC_ICONS,
                    model_dir=Path(directory),
                    index_dir=Path(directory) / "Indexes",
                )

    def test_query_validation_is_applied(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as directory:
            with self.assertRaisesRegex(EmbeddingError, "Model artifact"):
                EmbeddingResolver(
                    GENERIC_ICONS,
                    model_dir=Path(directory),
                    index_dir=Path(directory) / "Indexes",
                )
        with self.assertRaisesRegex(ValueError, "at least three"):
            EmbeddingResolver(
                GENERIC_ICONS[:2],
                model_dir=ARTIFACTS / "Model",
                index_dir=ARTIFACTS / "Indexes",
            )

    def test_pooling_contract_rejects_non_cls_configuration(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as directory:
            root = Path(directory)
            (root / "1_Pooling").mkdir()
            (root / "config.json").write_text(
                json.dumps(
                    {
                        "model_type": "bert",
                        "hidden_size": DIMENSIONS,
                        "pad_token_id": 0,
                        "max_position_embeddings": 512,
                    }
                ),
                encoding="utf-8",
            )
            (root / "tokenizer_config.json").write_text(
                json.dumps({"model_max_length": 512}), encoding="utf-8"
            )
            (root / "special_tokens_map.json").write_text(
                json.dumps(
                    {
                        "cls_token": "[CLS]",
                        "mask_token": "[MASK]",
                        "pad_token": "[PAD]",
                        "sep_token": "[SEP]",
                        "unk_token": "[UNK]",
                    }
                ),
                encoding="utf-8",
            )
            (root / "1_Pooling" / "config.json").write_text(
                json.dumps(
                    {
                        "word_embedding_dimension": DIMENSIONS,
                        "pooling_mode_cls_token": False,
                        "pooling_mode_mean_tokens": True,
                    }
                ),
                encoding="utf-8",
            )
            (root / "sentence_bert_config.json").write_text(
                json.dumps({"max_seq_length": 512}), encoding="utf-8"
            )
            (root / "modules.json").write_text(
                json.dumps(
                    [
                        {"type": "sentence_transformers.models.Transformer"},
                        {"type": "sentence_transformers.models.Pooling"},
                        {"type": "sentence_transformers.models.Normalize"},
                    ]
                ),
                encoding="utf-8",
            )

            with self.assertRaisesRegex(EmbeddingError, "Only CLS pooling"):
                _validate_model_contract(root)


@unittest.skipUnless(
    (ARTIFACTS / "Model" / "model.onnx").is_file(),
    "explicit acquisition has not downloaded the model",
)
class EmbeddingInferenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        TEST_TEMP.mkdir(parents=True, exist_ok=True)
        cls.temporary = tempfile.TemporaryDirectory(dir=TEST_TEMP)
        cls.index_dir = Path(cls.temporary.name)
        cls.resolver = EmbeddingResolver(
            GENERIC_ICONS,
            threshold=-1.0,
            margin=0.0,
            index_dir=cls.index_dir,
            threads=1,
        )
        if cls.resolver.index_info != cls.resolver.diagnostics:
            raise AssertionError("index_info must expose preprocessing diagnostics")
        json.dumps(cls.resolver.index_info)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.resolver.close()
        cls.temporary.cleanup()

    def test_sequence_masks_and_maximum_length(self) -> None:
        encoded = self.resolver._encode(
            [
                "short phrase",
                "a substantially longer generic phrase for padding",
                " ".join(["word"] * 600),
            ]
        )
        masks = encoded["attention_mask"]
        ids = encoded["input_ids"]
        types = encoded["token_type_ids"]

        self.assertEqual(ids.shape, masks.shape)
        self.assertEqual(ids.shape, types.shape)
        self.assertEqual(ids.shape[1], 512)
        for row in range(ids.shape[0]):
            active = int(masks[row].sum())
            self.assertEqual(ids[row, 0], 101)
            self.assertEqual(ids[row, active - 1], 102)
            self.assertTrue((masks[row, :active] == 1).all())
            self.assertTrue((masks[row, active:] == 0).all())
            self.assertTrue((types[row] == 0).all())

    def test_batch_matches_single_normalized_finite_vectors(self) -> None:
        import numpy as np

        texts = ["weather panel", "audio controls", "inventory folder"]
        batch = self.resolver._embed_texts(texts)
        singles = np.concatenate(
            [self.resolver._embed_texts([text]) for text in texts], axis=0
        )

        self.assertEqual(batch.shape, (3, DIMENSIONS))
        self.assertTrue(np.isfinite(batch).all())
        self.assertTrue(np.allclose(np.linalg.norm(batch, axis=1), 1.0, atol=1e-6))
        self.assertTrue(np.allclose(batch, singles, rtol=1e-5, atol=1e-6))

    def test_repeat_queries_are_deterministic_and_ties_use_lowest_glyph(self) -> None:
        first = self.resolver.resolve(Query("generic settings"))
        second = self.resolver.resolve(Query("generic settings"))
        self.assertEqual(first, second)

        duplicate_icons = (
            Icon(0x202, "same", tags=("duplicate",)),
            Icon(0x201, "same", tags=("duplicate",)),
            Icon(0x203, "other"),
        )
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as directory:
            resolver = EmbeddingResolver(
                duplicate_icons,
                threshold=-1.0,
                margin=0.0,
                index_dir=Path(directory),
            )
            try:
                result = resolver.resolve(Query("same duplicate"))
                self.assertEqual(result.candidates[0].glyph, 0x201)
                self.assertEqual(result.candidates[1].glyph, 0x202)
            finally:
                resolver.close()

    def test_threshold_and_margin_abstain_but_keep_three_candidates(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as directory:
            resolver = EmbeddingResolver(
                GENERIC_ICONS,
                threshold=1.0,
                margin=2.0,
                index_dir=Path(directory),
            )
            try:
                result = resolver.resolve(Query("ordinary public words"))
                self.assertEqual(result.status, "no_match")
                self.assertEqual(len(result.candidates), 3)
                self.assertIsNotNone(result.margin)
            finally:
                resolver.close()

        empty = self.resolver.resolve(Query(" \t "))
        self.assertEqual(empty.status, "no_match")
        self.assertEqual(empty.candidates, ())
        with self.assertRaisesRegex(ValueError, "disallowed control"):
            self.resolver.resolve(Query("bad\nheading"))

    def test_cache_hit_invalidation_and_corruption_detection(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as directory:
            index_dir = Path(directory)
            first = EmbeddingResolver(
                GENERIC_ICONS, threshold=-1.0, margin=0.0, index_dir=index_dir
            )
            key = first.diagnostics["cache_key"]
            self.assertFalse(first.diagnostics["cache_hit"])
            first.close()

            second = EmbeddingResolver(
                GENERIC_ICONS, threshold=-1.0, margin=0.0, index_dir=index_dir
            )
            self.assertTrue(second.diagnostics["cache_hit"])
            second.close()

            changed = GENERIC_ICONS[:-1] + (
                Icon(0x103, "folder", tags=("documents", "directory")),
            )
            third = EmbeddingResolver(
                changed, threshold=-1.0, margin=0.0, index_dir=index_dir
            )
            self.assertNotEqual(key, third.diagnostics["cache_key"])
            self.assertFalse(third.diagnostics["cache_hit"])
            self.assertGreaterEqual(third.diagnostics["stale_cache_count"], 1)
            third.close()

            cache = index_dir / f"Embedding-{key}.npz"
            cache.write_bytes(cache.read_bytes() + b"corrupt")
            with self.assertRaisesRegex(EmbeddingError, "digest is stale"):
                EmbeddingResolver(
                    GENERIC_ICONS,
                    threshold=-1.0,
                    margin=0.0,
                    index_dir=index_dir,
                )


if __name__ == "__main__":
    unittest.main()
