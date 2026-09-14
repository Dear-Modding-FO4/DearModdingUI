import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from Corpus import load_cases
from Cleanup import remove_tree
from Models import ARTIFACTS, Query, Selection, preserve_baseline
from Report import write_report
from Scoring import choose_threshold, count_outcomes


class ContractTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "nt", "Windows read-only file deletion")
    def test_cleanup_removes_readonly_owned_files_without_broad_root_deletion(self):
        ARTIFACTS.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ARTIFACTS) as temporary:
            target = Path(temporary) / "OwnedCache"
            target.mkdir()
            pack = target / "Pack.idx"
            pack.write_text("owned cache", encoding="ascii")
            pack.chmod(stat.S_IREAD)
            changed = remove_tree(target)
            self.assertEqual(changed, [str(pack)])
            self.assertFalse(target.exists())
        with self.assertRaises(ValueError):
            remove_tree(ARTIFACTS)

    def test_semantics_never_override_selected_or_invalid_raw_values(self):
        suggestion = Selection("selected", 0xE270, "gear", "semantic suggestion")
        for selected in (
            Selection("selected", 0xE3E8, "question", "explicit name"),
            Selection("selected", 0xE270, "gear", "primary metadata"),
            Selection("invalid_raw_glyph", 0, "", "explicit zero"),
            Selection("invalid_raw_glyph", 0x110000, "", "invalid scalar"),
        ):
            self.assertIs(preserve_baseline(selected, suggestion), selected)
        self.assertIs(preserve_baseline(Selection("no_match"), suggestion), suggestion)

    def test_request_limits_are_bytes_and_controls_are_rejected(self):
        for query in (Query("a" * 257), Query("\n"), Query("x", explicit_name="x" * 129),
                      Query("x", explicit_glyph=True), Query("x", explicit_glyph=-1)):
            with self.assertRaises(ValueError):
                query.validate()
        Query("a" * 256, context="\t", explicit_glyph=0).validate()
        with self.assertRaises(ValueError):
            Query("\u00e9" * 129).validate()

    def test_variants_never_cross_held_out_family_boundary(self):
        by_family = {}
        for case in load_cases():
            by_family.setdefault(case.family, set()).add(case.split)
        self.assertEqual(len(by_family), 60)
        self.assertTrue(all(len(splits) == 1 for splits in by_family.values()))

    def test_tuning_rejects_held_out_observations(self):
        with self.assertRaises(ValueError):
            choose_threshold([{"split": "held_out"}], {})

    def test_abstention_calibration_does_not_pretend_unreviewed_positives_are_truth(self):
        def row(stratum, name, score, second, acceptable):
            return {
                "split": "development", "stratum": stratum, "acceptable": acceptable,
                "final": {"baseline": {"status": "no_match"}},
                "raw": {"embedding": {"candidates": [
                    {"name": name, "score": score}, {"name": "runner-up", "score": second},
                ]}},
            }
        result = choose_threshold([
            row("synonym", "ear", 0.685, 0.684, ["speaker-high"]),
            row("negative", "not-subset-of", 0.667, 0.653, []),
        ], {"threshold_grid": [0.6, 0.68, 0.9], "margin_grid": [0, 0.02],
            "threshold_policy": "negative controls only"})
        selected = result["selected"]
        self.assertEqual(selected["threshold"], 0.68)
        self.assertEqual(selected["issued_nonnegative"], 1)
        self.assertEqual(selected["negative_matches"], 0)
        self.assertEqual(selected["within_authored_set"], 0)
        self.assertEqual(selected["outside_authored_set"], 1)

    def test_no_match_is_not_a_correct_selected_question(self):
        row = {
            "family": "nonsense", "acceptable": [], "abstain_ok": True,
            "final": {"baseline": Selection("no_match").to_dict()},
        }
        counts = count_outcomes([row], "baseline")
        self.assertEqual(counts["selections"], 0)
        self.assertEqual(counts["allowed_abstentions"], 1)
        row["final"]["baseline"] = Selection("selected", 0xE3E8, "question").to_dict()
        counts = count_outcomes([row], "baseline")
        self.assertEqual(counts["negative_false_matches"], 1)

    def test_offline_guard_blocks_socket_attempts_in_isolated_process(self):
        completed = subprocess.run([
            sys.executable, "-c",
            "import sys;sys.path.insert(0,sys.argv[1]);from Offline import enforce_offline;"
            "enforce_offline();import socket;socket.create_connection(('example.com',443))",
            str(Path(__file__).resolve().parents[1]),
        ], capture_output=True, text=True)
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("Network operation forbidden", completed.stderr)

    def test_report_escapes_untrusted_heading_and_evidence(self):
        hostile = "<script>alert('heading')</script>"
        selected = Selection("no_match", reason=hostile).to_dict()
        row = {
            "id": hostile, "family": "escape", "split": "development", "stratum": "negative",
            "source": hostile, "query": {"heading": hostile, "context": hostile},
            "acceptable": [], "abstain_ok": True,
            "final": {method: selected for method in ("baseline", "lexical", "embedding")},
            "raw": {method: selected for method in ("lexical", "embedding")},
        }
        ARTIFACTS.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ARTIFACTS) as temporary:
            path = Path(temporary) / "Comparison.html"
            write_report({"rows": [row], "summary": {"splits": {}}, "provenance": {}}, path)
            rendered = path.read_text(encoding="utf-8")
        self.assertNotIn(hostile, rendered)
        self.assertIn("&lt;script&gt;", rendered)


if __name__ == "__main__":
    unittest.main()
