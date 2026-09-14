from __future__ import annotations

from dataclasses import replace
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest


TOOL_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL_DIR))

import Acquire


TEST_TEMP = Acquire.ARTIFACTS / "Cache" / "Tests"


class AcquisitionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        TEST_TEMP.mkdir(parents=True, exist_ok=True)

    def test_locked_dependency_closure_and_budgets(self) -> None:
        manifest = Acquire.load_manifest()
        packages = {package["name"] for package in manifest.packages}

        self.assertIn("onnxruntime", packages)
        self.assertIn("numpy", packages)
        self.assertIn("tokenizers", packages)
        self.assertIn("huggingface-hub", packages)
        self.assertIn("hf-xet", packages)
        self.assertEqual(manifest.download_bytes, 180_828_255)
        self.assertLessEqual(manifest.download_bytes, manifest.download_cap_bytes)
        self.assertLessEqual(
            manifest.projected_artifact_bytes, manifest.artifact_cap_bytes
        )

    def test_missing_and_malformed_manifests_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as directory:
            root = Path(directory)
            with self.assertRaisesRegex(Acquire.AcquisitionError, "Missing manifest"):
                Acquire.load_manifest(root / "missing.json")

            malformed = root / "Acquisition.json"
            malformed.write_text("{", encoding="utf-8")
            with self.assertRaisesRegex(Acquire.AcquisitionError, "Invalid JSON"):
                Acquire.load_manifest(malformed)

    def test_raw_sha256_and_git_blob_identities(self) -> None:
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as directory:
            path = Path(directory) / "artifact.bin"
            content = b"verified bytes\n"
            path.write_bytes(content)
            sha_artifact = Acquire.Artifact(
                name="sha",
                destination=Acquire.PurePosixPath("Data/artifact.bin"),
                url="https://example.invalid/artifact.bin",
                size=len(content),
                license="MIT",
                sha256=hashlib.sha256(content).hexdigest(),
            )
            blob = hashlib.sha1(
                f"blob {len(content)}\0".encode("ascii") + content,
                usedforsecurity=False,
            ).hexdigest()
            blob_artifact = Acquire.Artifact(
                name="blob",
                destination=Acquire.PurePosixPath("Data/artifact.bin"),
                url="https://example.invalid/artifact.bin",
                size=len(content),
                license="MIT",
                git_blob_sha1=blob,
            )

            self.assertEqual(
                Acquire.verify_artifact(path, sha_artifact), (True, "verified")
            )
            self.assertEqual(
                Acquire.verify_artifact(path, blob_artifact), (True, "verified")
            )
            path.write_bytes(content + b"corrupt")
            self.assertFalse(Acquire.verify_artifact(path, sha_artifact)[0])
            path.unlink()
            self.assertEqual(
                Acquire.verify_artifact(path, sha_artifact), (False, "missing")
            )

    def test_verified_cached_artifact_does_not_open_network(self) -> None:
        manifest = Acquire.load_manifest()
        content = b"cached"
        artifact = Acquire.Artifact(
            name="cached",
            destination=Acquire.PurePosixPath("Data/cached.bin"),
            url="https://example.invalid/cached.bin",
            size=len(content),
            license="MIT",
            sha256=hashlib.sha256(content).hexdigest(),
        )
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as directory:
            root = Path(directory)
            target = root / "Data" / "cached.bin"
            target.parent.mkdir()
            target.write_bytes(content)

            def fail_open(*_args, **_kwargs):
                self.fail("verified cached files must not open the network")

            self.assertEqual(
                Acquire._download_artifact(
                    artifact, manifest, root=root, opener=fail_open
                ),
                "cache_hit",
            )
            self.assertFalse((root / "Logs" / "Acquisition.jsonl").exists())

    def test_unsupported_caps_fail_before_network_access(self) -> None:
        manifest = Acquire.load_manifest()
        unsupported = replace(manifest, download_cap_bytes=1)
        with self.assertRaisesRegex(Acquire.AcquisitionError, "above the 1-byte cap"):
            Acquire.validate_preflight(unsupported)

    def test_dependency_closure_rejects_missing_transitive_package(self) -> None:
        packages = [
            {
                "name": "parent",
                "version": "1",
                "requires": ["child>=1"],
                "sha256": "a" * 64,
            }
        ]
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as directory:
            lock = Path(directory) / "Requirements.lock"
            lock.write_text(
                f"parent==1 --hash=sha256:{'a' * 64}\n", encoding="utf-8"
            )
            with self.assertRaisesRegex(
                Acquire.AcquisitionError, "missing requirement"
            ):
                Acquire._validate_dependency_closure(packages, lock)


if __name__ == "__main__":
    unittest.main()
