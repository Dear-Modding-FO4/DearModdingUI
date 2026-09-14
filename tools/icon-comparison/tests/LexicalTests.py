from __future__ import annotations

from dataclasses import asdict
import gzip
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest


TOOL_DIR = Path(__file__).resolve().parents[1]
if str(TOOL_DIR) not in sys.path:
    sys.path.insert(0, str(TOOL_DIR))

from LexicalResolver import LexicalDataError, LexicalIndexError, LexicalResolver
from Models import ARTIFACTS, Icon, Query


FIXTURE = Path(__file__).parent / "fixtures" / "TinyWordNet.xml"


class LexicalResolverTests(unittest.TestCase):
    def setUp(self) -> None:
        ARTIFACTS.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(
            prefix="LexicalTests-",
            dir=ARTIFACTS,
        )
        self.root = Path(self.temporary.name)
        self.dataset = self.root / "tiny.xml.gz"
        with FIXTURE.open("rb") as source, gzip.GzipFile(
            filename=self.dataset, mode="wb", mtime=0
        ) as destination:
            destination.write(source.read())
        self.digest = hashlib.sha256(self.dataset.read_bytes()).hexdigest()
        self.icons = (
            Icon(0x100, "car"),
            Icon(0x101, "dog"),
            Icon(0x102, "animal"),
            Icon(0x103, "depository"),
            Icon(0x104, "shore"),
            Icon(0x105, "camera"),
            Icon(0x106, "rodent"),
        )
        self.resolver = self._make_resolver()

    def tearDown(self) -> None:
        self.resolver.close()
        self.temporary.cleanup()

    def _make_resolver(self, **overrides: object) -> LexicalResolver:
        arguments = {
            "icons": self.icons,
            "dataset_path": self.dataset,
            "index_dir": self.root / "indexes",
            "expected_digest": self.digest,
        }
        arguments.update(overrides)
        return LexicalResolver(**arguments)

    def test_same_synset_synonym_selects_and_explains_source(self) -> None:
        result = self.resolver.resolve(Query("automobile"))

        self.assertEqual("selected", result.status)
        self.assertEqual("car", result.name)
        self.assertIn("tier=synonym", result.reason)
        self.assertIn("tiny-s-car", result.reason)
        self.assertIn("catalog_term='car'", result.reason)

    def test_hypernym_is_limited_to_one_hop(self) -> None:
        result = self.resolver.resolve(Query("poodle"))

        self.assertEqual("selected", result.status)
        self.assertEqual("dog", result.name)
        self.assertIn("tier=hypernym", result.reason)
        self.assertNotIn("animal", tuple(candidate.name for candidate in result.candidates))

    def test_context_resolves_polysemy_and_missing_context_abstains(self) -> None:
        unresolved = self.resolver.resolve(Query("bank"))
        resolved = self.resolver.resolve(
            Query("bank", context="water beside the river")
        )

        self.assertEqual("no_match", unresolved.status)
        self.assertIn("ambiguous", unresolved.reason)
        self.assertEqual(("depository", "shore"), tuple(
            candidate.name for candidate in unresolved.candidates
        ))
        self.assertEqual("selected", resolved.status)
        self.assertEqual("shore", resolved.name)
        self.assertRegex(resolved.reason, r"context_overlap=[1-9]")

    def test_random_known_token_in_nonsense_compound_is_weak(self) -> None:
        result = self.resolver.resolve(Query("blorb camera xyz"))

        self.assertEqual("no_match", result.status)
        self.assertIn("weak evidence", result.reason)
        self.assertEqual("camera", result.candidates[0].name)
        self.assertIn("coverage=1/3 (33%)", result.candidates[0].reason)

    def test_dataset_word_form_handles_irregular_plural(self) -> None:
        result = self.resolver.resolve(Query("mice"))

        self.assertEqual("selected", result.status)
        self.assertEqual("rodent", result.name)
        self.assertIn("tiny-s-mouse", result.reason)

    def test_verified_cache_hit_is_reported(self) -> None:
        second = self._make_resolver()
        try:
            self.assertEqual("cache_hit", second.index_info.status)
            self.assertEqual(self.resolver.index_info.path, second.index_info.path)
            self.assertIsInstance(second.index_info.path, str)
            json.dumps(asdict(second.index_info))
        finally:
            second.close()

    def test_query_validation_is_used(self) -> None:
        with self.assertRaisesRegex(ValueError, "disallowed control"):
            self.resolver.resolve(Query("bad\nheading"))

    def test_digest_mismatch_is_explicit(self) -> None:
        with self.assertRaisesRegex(LexicalDataError, "SHA-256 mismatch"):
            self._make_resolver(
                index_dir=self.root / "other-index",
                expected_digest="0" * 64,
            )

    def test_corrupt_cache_is_not_silently_rebuilt(self) -> None:
        cache_path = Path(self.resolver.index_info.path)
        self.resolver.close()
        cache_path.write_bytes(b"not a sqlite database")

        with self.assertRaisesRegex(LexicalIndexError, "corrupt or unreadable"):
            self._make_resolver()

    def test_malformed_checksummed_xml_is_explicit(self) -> None:
        malformed = self.root / "malformed.xml.gz"
        with gzip.GzipFile(filename=malformed, mode="wb", mtime=0) as destination:
            destination.write(b"<LexicalResource><broken>")
        digest = hashlib.sha256(malformed.read_bytes()).hexdigest()

        with self.assertRaisesRegex(LexicalDataError, "Unable to parse"):
            self._make_resolver(
                dataset_path=malformed,
                index_dir=self.root / "malformed-index",
                expected_digest=digest,
            )


if __name__ == "__main__":
    unittest.main()
