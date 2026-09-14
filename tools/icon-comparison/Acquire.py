from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import subprocess
import sys
import sysconfig
import time
from typing import Callable, Iterable
import urllib.request
import venv
import zipfile

from Models import ARTIFACTS, ROOT


DEPENDENCIES = Path(__file__).resolve().parent / "dependencies"
ACQUISITION_MANIFEST = DEPENDENCIES / "Acquisition.json"
ACQUISITION_INDEX = ARTIFACTS / "Indexes" / "AcquisitionIndex.json"
NETWORK_LOG = ARTIFACTS / "Logs" / "Acquisition.jsonl"
BUFFER_SIZE = 1024 * 1024


class AcquisitionError(RuntimeError):
    pass


@dataclass(frozen=True)
class Artifact:
    name: str
    destination: PurePosixPath
    url: str
    size: int
    license: str
    sha256: str | None = None
    git_blob_sha1: str | None = None
    package: str | None = None
    version: str | None = None


@dataclass(frozen=True)
class Manifest:
    download_cap_bytes: int
    artifact_cap_bytes: int
    environment_base_budget_bytes: int
    environment_wheel_expansion_multiplier: int
    index_and_log_budget_bytes: int
    owner_directories: tuple[str, ...]
    requirements_lock: Path
    provenance: dict
    packages: tuple[dict, ...]
    artifacts: tuple[Artifact, ...]

    @property
    def download_bytes(self) -> int:
        return sum(artifact.size for artifact in self.artifacts)

    @property
    def wheel_bytes(self) -> int:
        return sum(
            artifact.size
            for artifact in self.artifacts
            if artifact.package is not None
        )

    @property
    def projected_artifact_bytes(self) -> int:
        return (
            self.download_bytes
            + self.wheel_bytes * self.environment_wheel_expansion_multiplier
            + self.environment_base_budget_bytes
            + self.index_and_log_budget_bytes
        )


def _load_json(path: Path) -> dict:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise AcquisitionError(f"Missing manifest: {path}") from error
    except (OSError, json.JSONDecodeError) as error:
        raise AcquisitionError(f"Invalid JSON manifest {path}: {error}") from error
    if not isinstance(payload, dict):
        raise AcquisitionError(f"Manifest root must be an object: {path}")
    return payload


def _require_int(payload: dict, name: str) -> int:
    value = payload.get(name)
    if type(value) is not int or value <= 0:
        raise AcquisitionError(f"{name} must be a positive integer")
    return value


def _artifact_from_payload(payload: dict, *, wheel: bool = False) -> Artifact:
    if not isinstance(payload, dict):
        raise AcquisitionError("Every artifact must be an object")
    destination_value = (
        f"Wheels/{payload.get('filename', '')}" if wheel else payload.get("destination")
    )
    try:
        destination = PurePosixPath(destination_value)
    except TypeError as error:
        raise AcquisitionError("Artifact destination must be text") from error
    if (
        destination.is_absolute()
        or not destination.parts
        or ".." in destination.parts
        or "\\" in str(destination)
    ):
        raise AcquisitionError(f"Unsafe artifact destination: {destination_value!r}")
    size = payload.get("size")
    if type(size) is not int or size <= 0:
        raise AcquisitionError(f"Artifact {destination} has an invalid size")
    sha256 = payload.get("sha256")
    git_blob_sha1 = payload.get("git_blob_sha1")
    if bool(sha256) == bool(git_blob_sha1):
        raise AcquisitionError(
            f"Artifact {destination} must have exactly one content identity"
        )
    if sha256 and (
        not isinstance(sha256, str)
        or len(sha256) != 64
        or any(character not in "0123456789abcdef" for character in sha256)
    ):
        raise AcquisitionError(f"Artifact {destination} has an invalid SHA-256")
    if git_blob_sha1 and (
        not isinstance(git_blob_sha1, str)
        or len(git_blob_sha1) != 40
        or any(character not in "0123456789abcdef" for character in git_blob_sha1)
    ):
        raise AcquisitionError(f"Artifact {destination} has an invalid Git blob SHA-1")
    for key in ("name", "url", "license"):
        if not isinstance(payload.get(key), str) or not payload[key]:
            raise AcquisitionError(f"Artifact {destination} has no valid {key}")
    return Artifact(
        name=payload["name"],
        destination=destination,
        url=payload["url"],
        size=size,
        license=payload["license"],
        sha256=sha256,
        git_blob_sha1=git_blob_sha1,
        package=payload.get("name") if wheel else None,
        version=payload.get("version") if wheel else None,
    )


def _normalize_package_name(name: str) -> str:
    return re.sub(r"[-_.]+", "-", name).lower()


def _validate_dependency_closure(packages: list[dict], lock_path: Path) -> None:
    by_name: dict[str, dict] = {}
    for package in packages:
        if not isinstance(package, dict):
            raise AcquisitionError("Every dependency must be an object")
        name = package.get("name")
        version = package.get("version")
        requires = package.get("requires")
        if (
            not isinstance(name, str)
            or not name
            or not isinstance(version, str)
            or not version
            or not isinstance(requires, list)
            or any(not isinstance(requirement, str) for requirement in requires)
        ):
            raise AcquisitionError("Dependency metadata is incomplete")
        normalized = _normalize_package_name(name)
        if normalized in by_name:
            raise AcquisitionError(f"Duplicate dependency: {name}")
        by_name[normalized] = package
    for package in packages:
        for requirement in package["requires"]:
            match = re.match(r"\s*([A-Za-z0-9_.-]+)", requirement)
            if match is None or _normalize_package_name(match.group(1)) not in by_name:
                raise AcquisitionError(
                    f"Dependency closure is missing requirement {requirement!r} "
                    f"from {package['name']}"
                )

    locked: dict[str, tuple[str, str]] = {}
    for number, line in enumerate(
        lock_path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        if not line or line.startswith("#"):
            continue
        match = re.fullmatch(
            r"([A-Za-z0-9_.-]+)==([^\s]+) --hash=sha256:([0-9a-f]{64})", line
        )
        if match is None:
            raise AcquisitionError(f"Invalid hash lock line {number}")
        normalized = _normalize_package_name(match.group(1))
        if normalized in locked:
            raise AcquisitionError(f"Duplicate hash lock entry: {match.group(1)}")
        locked[normalized] = (match.group(2), match.group(3))
    if set(locked) != set(by_name):
        raise AcquisitionError("Hash lock packages do not match dependency manifest")
    for name, package in by_name.items():
        if locked[name] != (package["version"], package["sha256"]):
            raise AcquisitionError(f"Hash lock differs for {package['name']}")


def load_manifest(path: Path = ACQUISITION_MANIFEST) -> Manifest:
    payload = _load_json(path)
    if payload.get("schema") != 1:
        raise AcquisitionError("Unsupported acquisition manifest schema")
    dependency_name = payload.get("dependency_manifest")
    lock_name = payload.get("requirements_lock")
    if not isinstance(dependency_name, str) or not isinstance(lock_name, str):
        raise AcquisitionError("Dependency and lock manifest names must be text")
    dependency_path = path.parent / dependency_name
    lock_path = path.parent / lock_name
    dependencies = _load_json(dependency_path)
    if dependencies.get("schema") != 1 or not isinstance(
        dependencies.get("packages"), list
    ):
        raise AcquisitionError("Unsupported dependency manifest schema")
    if not lock_path.is_file():
        raise AcquisitionError(f"Missing hash lock: {lock_path}")
    _validate_dependency_closure(dependencies["packages"], lock_path)
    owners = payload.get("owner_directories")
    expected_owners = {
        "Data",
        "Model",
        "Wheels",
        "Environment",
        "Cache",
        "Indexes",
        "Logs",
    }
    if not isinstance(owners, list) or set(owners) != expected_owners:
        raise AcquisitionError("Owner directory declaration is incomplete")
    artifacts = [
        _artifact_from_payload(artifact) for artifact in payload.get("artifacts", [])
    ]
    artifacts.extend(
        _artifact_from_payload(package, wheel=True)
        for package in dependencies["packages"]
    )
    destinations = [str(artifact.destination) for artifact in artifacts]
    if len(destinations) != len(set(destinations)):
        raise AcquisitionError("Artifact destinations must be unique")
    owner_set = set(owners)
    if any(artifact.destination.parts[0] not in owner_set for artifact in artifacts):
        raise AcquisitionError("Every artifact must stay in an owner directory")
    manifest = Manifest(
        download_cap_bytes=_require_int(payload, "download_cap_bytes"),
        artifact_cap_bytes=_require_int(payload, "artifact_cap_bytes"),
        environment_base_budget_bytes=_require_int(
            payload, "environment_base_budget_bytes"
        ),
        environment_wheel_expansion_multiplier=_require_int(
            payload, "environment_wheel_expansion_multiplier"
        ),
        index_and_log_budget_bytes=_require_int(
            payload, "index_and_log_budget_bytes"
        ),
        owner_directories=tuple(owners),
        requirements_lock=lock_path,
        provenance=payload.get("provenance", {}),
        packages=tuple(dependencies["packages"]),
        artifacts=tuple(artifacts),
    )
    validate_preflight(manifest)
    return manifest


def validate_preflight(manifest: Manifest) -> None:
    if manifest.download_bytes > manifest.download_cap_bytes:
        raise AcquisitionError(
            f"Locked downloads require {manifest.download_bytes} bytes, above "
            f"the {manifest.download_cap_bytes}-byte cap"
        )
    if manifest.projected_artifact_bytes > manifest.artifact_cap_bytes:
        raise AcquisitionError(
            f"Conservative artifact projection requires "
            f"{manifest.projected_artifact_bytes} bytes, above the "
            f"{manifest.artifact_cap_bytes}-byte cap"
        )


def git_blob_hash(path: Path) -> str:
    size = path.stat().st_size
    digest = hashlib.sha1(usedforsecurity=False)
    digest.update(f"blob {size}\0".encode("ascii"))
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(BUFFER_SIZE), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_hash(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def verify_artifact(path: Path, artifact: Artifact) -> tuple[bool, str]:
    if not path.is_file():
        return False, "missing"
    size = path.stat().st_size
    if size != artifact.size:
        return False, f"size {size}, expected {artifact.size}"
    if artifact.sha256:
        actual = sha256_hash(path)
        if actual != artifact.sha256:
            return False, f"SHA-256 {actual}, expected {artifact.sha256}"
    else:
        actual = git_blob_hash(path)
        if actual != artifact.git_blob_sha1:
            return (
                False,
                f"Git blob SHA-1 {actual}, expected {artifact.git_blob_sha1}",
            )
    return True, "verified"


def _directory_size(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(item.stat().st_size for item in path.rglob("*") if item.is_file())


def _owned_size(
    manifest: Manifest, root: Path, *, include_environment: bool = True
) -> int:
    return sum(
        _directory_size(root / directory)
        for directory in manifest.owner_directories
        if include_environment or directory != "Environment"
    )


def _network_bytes(path: Path = NETWORK_LOG) -> int:
    if not path.is_file():
        return 0
    total = 0
    for line in path.read_text(encoding="utf-8").splitlines():
        try:
            record = json.loads(line)
        except json.JSONDecodeError as error:
            raise AcquisitionError(f"Corrupt acquisition ledger: {path}") from error
        value = record.get("bytes")
        if type(value) is not int or value < 0:
            raise AcquisitionError(f"Invalid acquisition ledger entry: {path}")
        total += value
    return total


def _append_network_record(record: dict, path: Path = NETWORK_LOG) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8", newline="\n") as stream:
        stream.write(json.dumps(record, sort_keys=True) + "\n")


def _local_path(artifact: Artifact, root: Path = ARTIFACTS) -> Path:
    return root.joinpath(*artifact.destination.parts)


def _download_artifact(
    artifact: Artifact,
    manifest: Manifest,
    *,
    root: Path = ARTIFACTS,
    opener: Callable = urllib.request.urlopen,
    attempts: int = 2,
) -> str:
    target = _local_path(artifact, root)
    valid, _ = verify_artifact(target, artifact)
    if valid:
        return "cache_hit"
    if target.exists():
        target.unlink()
    target.parent.mkdir(parents=True, exist_ok=True)
    part = target.with_name(target.name + ".part")
    last_error: Exception | None = None
    for attempt in range(1, attempts + 1):
        consumed = _network_bytes(root / "Logs" / "Acquisition.jsonl")
        if consumed + artifact.size > manifest.download_cap_bytes:
            raise AcquisitionError(
                f"Downloading {artifact.name} would exceed the actual network byte cap "
                f"after {consumed} bytes"
            )
        transferred = 0
        status = "failed"
        started = time.time()
        try:
            request = urllib.request.Request(
                artifact.url,
                headers={"User-Agent": "DearModdingUI-icon-comparison/1"},
            )
            with opener(request, timeout=120) as response, part.open("wb") as stream:
                content_length = response.headers.get("Content-Length")
                if content_length is not None and int(content_length) != artifact.size:
                    raise AcquisitionError(
                        f"{artifact.name} reports {content_length} bytes, expected "
                        f"{artifact.size}"
                    )
                while chunk := response.read(BUFFER_SIZE):
                    transferred += len(chunk)
                    if transferred > artifact.size:
                        raise AcquisitionError(
                            f"{artifact.name} exceeded its locked size"
                        )
                    stream.write(chunk)
            valid, reason = verify_artifact(part, artifact)
            if not valid:
                raise AcquisitionError(f"{artifact.name} failed verification: {reason}")
            part.replace(target)
            status = "downloaded"
            return status
        except Exception as error:
            last_error = error
            if part.exists():
                part.unlink()
        finally:
            _append_network_record(
                {
                    "artifact": str(artifact.destination),
                    "attempt": attempt,
                    "bytes": transferred,
                    "duration_seconds": round(time.time() - started, 6),
                    "status": status,
                },
                root / "Logs" / "Acquisition.jsonl",
            )
    raise AcquisitionError(
        f"Could not acquire {artifact.name} after {attempts} attempts: {last_error}"
    )


def _offline_environment(root: Path = ARTIFACTS) -> dict[str, str]:
    cache = root / "Cache"
    temp = cache / "Temp"
    temp.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment.update(
        {
            "PIP_CACHE_DIR": str(cache / "pip"),
            "PIP_NO_INDEX": "1",
            "HF_HOME": str(cache / "huggingface"),
            "HF_HUB_CACHE": str(cache / "huggingface" / "hub"),
            "HF_HUB_OFFLINE": "1",
            "HF_HUB_DISABLE_TELEMETRY": "1",
            "HF_HUB_DISABLE_XET": "1",
            "TRANSFORMERS_OFFLINE": "1",
            "TMP": str(temp),
            "TEMP": str(temp),
        }
    )
    return environment


def validate_python() -> None:
    if sys.implementation.name != "cpython" or sys.version_info[:3] != (3, 14, 7):
        raise AcquisitionError("Preparation requires the installed CPython 3.14.7")
    if platform.machine().upper() not in {"AMD64", "X86_64"}:
        raise AcquisitionError("Preparation requires Windows x64 Python")
    if sys.platform != "win32":
        raise AcquisitionError("Preparation requires Windows")
    if sysconfig.get_config_var("Py_GIL_DISABLED") not in (None, 0):
        raise AcquisitionError("Preparation requires the standard-GIL build")


def _wheel_license_entries(path: Path) -> list[str]:
    with zipfile.ZipFile(path) as archive:
        return sorted(
            name
            for name in archive.namelist()
            if any(
                part.upper().startswith(("LICENSE", "COPYING", "NOTICE"))
                for part in PurePosixPath(name).parts
            )
        )


def _verified_records(
    manifest: Manifest, root: Path = ARTIFACTS
) -> list[dict[str, object]]:
    records = []
    for artifact in manifest.artifacts:
        path = _local_path(artifact, root)
        valid, reason = verify_artifact(path, artifact)
        if not valid:
            raise AcquisitionError(f"{artifact.name} is not usable: {reason}")
        records.append(
            {
                "name": artifact.name,
                "path": str(artifact.destination),
                "bytes": path.stat().st_size,
                "sha256": sha256_hash(path),
                "git_blob_sha1": artifact.git_blob_sha1,
                "license": artifact.license,
                "wheel_license_entries": (
                    _wheel_license_entries(path)
                    if artifact.package is not None
                    else []
                ),
            }
        )
    return records


def _write_index(
    manifest: Manifest,
    records: Iterable[dict[str, object]],
    *,
    root: Path = ARTIFACTS,
    prepared: bool,
) -> None:
    index = root / "Indexes" / "AcquisitionIndex.json"
    index.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "schema": 1,
        "prepared": prepared,
        "python": {
            "version": platform.python_version(),
            "executable": str(Path(sys.executable).resolve()),
            "implementation": sys.implementation.name,
            "architecture": platform.machine(),
            "standard_gil": sysconfig.get_config_var("Py_GIL_DISABLED") in (None, 0),
        },
        "download": {
            "locked_bytes": manifest.download_bytes,
            "actual_network_bytes": _network_bytes(root / "Logs" / "Acquisition.jsonl"),
            "cap_bytes": manifest.download_cap_bytes,
        },
        "artifacts": {
            "actual_bytes": _owned_size(manifest, root),
            "cap_bytes": manifest.artifact_cap_bytes,
            "conservative_preflight_bytes": manifest.projected_artifact_bytes,
        },
        "provenance": manifest.provenance,
        "dependencies": [
            {
                "name": package["name"],
                "version": package["version"],
                "license": package["license"],
                "filename": package["filename"],
            }
            for package in manifest.packages
        ],
        "files": list(records),
    }
    temporary = index.with_suffix(".json.tmp")
    temporary.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(index)


def download(manifest: Manifest, root: Path = ARTIFACTS) -> dict[str, int]:
    validate_preflight(manifest)
    for directory in manifest.owner_directories:
        (root / directory).mkdir(parents=True, exist_ok=True)
    statuses = {"downloaded": 0, "cache_hit": 0}
    for artifact in manifest.artifacts:
        status = _download_artifact(artifact, manifest, root=root)
        statuses[status] += 1
        print(f"{status}: {artifact.destination}")
    records = _verified_records(manifest, root)
    _write_index(manifest, records, root=root, prepared=False)
    return statuses


def _expanded_wheel_bytes(manifest: Manifest, root: Path) -> int:
    total = 0
    for artifact in manifest.artifacts:
        if artifact.package is None:
            continue
        path = _local_path(artifact, root)
        with zipfile.ZipFile(path) as archive:
            total += sum(info.file_size for info in archive.infolist())
    return total


def _inspect_environment_python(python: Path, environment: dict[str, str]) -> dict:
    script = (
        "import json,platform,sys,sysconfig;"
        "print(json.dumps({'implementation':sys.implementation.name,"
        "'version':platform.python_version(),'architecture':platform.machine(),"
        "'gil_disabled':sysconfig.get_config_var('Py_GIL_DISABLED'),"
        "'base_executable':sys._base_executable}))"
    )
    completed = subprocess.run(
        [str(python), "-c", script],
        check=True,
        cwd=ROOT,
        env=environment,
        capture_output=True,
        text=True,
    )
    try:
        details = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise AcquisitionError(
            "Could not inspect the workspace Python environment"
        ) from error
    if (
        details.get("implementation") != "cpython"
        or details.get("version") != "3.14.7"
        or str(details.get("architecture", "")).upper() not in {"AMD64", "X86_64"}
        or details.get("gil_disabled") not in (None, 0)
        or Path(details.get("base_executable", "")).resolve()
        != Path(sys.executable).resolve()
    ):
        raise AcquisitionError(
            f"Workspace environment does not use the approved interpreter: {details}"
        )
    return details


def prepare(manifest: Manifest, root: Path = ARTIFACTS) -> None:
    validate_python()
    records = _verified_records(manifest, root)
    environment_dir = root / "Environment"
    wheelhouse = root / "Wheels"
    current_without_environment = _owned_size(
        manifest, root, include_environment=False
    )
    projected = (
        current_without_environment
        + _expanded_wheel_bytes(manifest, root)
        + manifest.environment_base_budget_bytes
        + manifest.index_and_log_budget_bytes
    )
    if projected > manifest.artifact_cap_bytes:
        raise AcquisitionError(
            f"Verified wheels project {projected} artifact bytes, above the cap"
        )
    if not (environment_dir / "Scripts" / "python.exe").is_file():
        venv.EnvBuilder(with_pip=True, clear=True).create(environment_dir)
    python = environment_dir / "Scripts" / "python.exe"
    environment = _offline_environment(root)
    _inspect_environment_python(python, environment)
    subprocess.run(
        [
            str(python),
            "-m",
            "pip",
            "install",
            "--no-index",
            "--find-links",
            str(wheelhouse),
            "--require-hashes",
            "-r",
            str(manifest.requirements_lock),
        ],
        check=True,
        cwd=ROOT,
        env=environment,
    )
    subprocess.run(
        [str(python), "-m", "pip", "check"],
        check=True,
        cwd=ROOT,
        env=environment,
    )
    actual = _owned_size(manifest, root)
    if actual > manifest.artifact_cap_bytes:
        raise AcquisitionError(
            f"Prepared artifacts use {actual} bytes, above the artifact cap"
        )
    _write_index(manifest, records, root=root, prepared=True)


def status(manifest: Manifest, root: Path = ARTIFACTS) -> dict:
    files = []
    for artifact in manifest.artifacts:
        path = _local_path(artifact, root)
        valid, reason = verify_artifact(path, artifact)
        files.append(
            {
                "path": str(artifact.destination),
                "valid": valid,
                "status": reason,
            }
        )
    return {
        "locked_download_bytes": manifest.download_bytes,
        "download_cap_bytes": manifest.download_cap_bytes,
        "conservative_artifact_bytes": manifest.projected_artifact_bytes,
        "artifact_cap_bytes": manifest.artifact_cap_bytes,
        "actual_network_bytes": _network_bytes(root / "Logs" / "Acquisition.jsonl"),
        "actual_artifact_bytes": _owned_size(manifest, root),
        "environment_ready": (
            root / "Environment" / "Scripts" / "python.exe"
        ).is_file(),
        "files": files,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Explicit, checksummed acquisition for the semantic icon spike."
    )
    parser.add_argument(
        "phase", choices=("validate", "download", "prepare", "status")
    )
    parser.add_argument("--manifest", type=Path, default=ACQUISITION_MANIFEST)
    args = parser.parse_args(argv)
    try:
        manifest = load_manifest(args.manifest)
        if args.phase == "validate":
            result = status(manifest)
            result["files"] = []
        elif args.phase == "download":
            result = download(manifest)
        elif args.phase == "prepare":
            prepare(manifest)
            result = status(manifest)
        else:
            result = status(manifest)
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (AcquisitionError, OSError, subprocess.CalledProcessError) as error:
        print(f"acquisition failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
