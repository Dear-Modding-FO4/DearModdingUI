#!/usr/bin/env python3

import argparse
import pathlib
import subprocess
import tempfile
import zipfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
PLUGIN_ROOT = pathlib.PurePosixPath("F4SE/Plugins")
ASSET_SOURCE_ROOT = pathlib.PurePosixPath("data/F4SE/Plugins")


def tracked_asset_files(
    root: pathlib.Path = ROOT,
    tracked_paths: list[str] | None = None,
) -> set[str]:
    if tracked_paths is None:
        output = subprocess.check_output(
            ["git", "-C", str(root), "ls-files", "-z", "--", str(ASSET_SOURCE_ROOT)],
        )
        tracked_paths = [
            entry.decode("utf-8")
            for entry in output.split(b"\0")
            if entry
        ]

    assets: set[str] = set()
    prefix = ASSET_SOURCE_ROOT.as_posix() + "/"
    for tracked in tracked_paths:
        normalized = pathlib.PurePosixPath(tracked).as_posix()
        if not normalized.startswith(prefix):
            raise RuntimeError(f"asset manifest entry is outside {ASSET_SOURCE_ROOT}: {tracked}")
        relative = normalized.removeprefix(prefix)
        source = root / pathlib.PurePosixPath(normalized)
        if not source.is_file():
            raise RuntimeError(f"missing tracked package asset: {source}")
        assets.add((PLUGIN_ROOT / relative).as_posix())
    if not assets:
        raise RuntimeError("tracked package asset manifest is empty")
    return assets


def expected_files(variant: str, assets: set[str] | None = None) -> set[str]:
    assets = tracked_asset_files() if assets is None else assets
    binaries = {
        (PLUGIN_ROOT / "DearModdingUI.dll").as_posix(),
        (PLUGIN_ROOT / "DearModdingUI-MCM.dll").as_posix(),
    }
    if variant == "test":
        binaries.add((PLUGIN_ROOT / "dmui-test-client.dll").as_posix())
    return assets | binaries | {"LICENSE", "README.md", "THIRD_PARTY_NOTICES.md"}


def validate_names(variant: str, names: set[str], source: str) -> None:
    expected = expected_files(variant)
    missing = sorted(expected - names)
    unexpected = sorted(names - expected)
    if missing or unexpected:
        details = []
        if missing:
            details.append("missing: " + ", ".join(missing))
        if unexpected:
            details.append("unexpected: " + ", ".join(unexpected))
        raise RuntimeError(f"{source} package membership mismatch; " + "; ".join(details))

    dlls = sorted(name for name in names if name.lower().endswith(".dll"))
    expected_count = 3 if variant == "test" else 2
    if len(dlls) != expected_count:
        raise RuntimeError(f"{source} contains {len(dlls)} DLLs, expected {expected_count}")
    forbidden_suffixes = {".pdb", ".log", ".ilk", ".lib", ".exp"}
    for name in names:
        lowered = name.lower()
        if "forwarding-smoke" in lowered or pathlib.PurePosixPath(lowered).suffix in forbidden_suffixes:
            raise RuntimeError(f"{source} contains forbidden build artifact: {name}")


def folder_names(folder: pathlib.Path) -> set[str]:
    return {
        path.relative_to(folder).as_posix()
        for path in folder.rglob("*")
        if path.is_file()
    }


def archive_names(archive: pathlib.Path) -> set[str]:
    with zipfile.ZipFile(archive) as package:
        names = [name for name in package.namelist() if not name.endswith("/")]
        if len(names) != len(set(names)):
            raise RuntimeError(f"{archive} contains duplicate file entries")
        return set(names)


def validate_contents(folder: pathlib.Path, archive: pathlib.Path) -> None:
    with zipfile.ZipFile(archive) as package:
        bad = package.testzip()
        if bad:
            raise RuntimeError(f"{archive} contains a corrupt entry: {bad}")
        for name in archive_names(archive):
            if package.read(name) != (folder / pathlib.PurePosixPath(name)).read_bytes():
                raise RuntimeError(f"archive differs from assembled package: {name}")


def self_test() -> None:
    release = expected_files("release")
    test = expected_files("test")
    validate_names("release", release, "self-test release")
    validate_names("test", test, "self-test test")

    with tempfile.TemporaryDirectory() as temporary:
        temporary_root = pathlib.Path(temporary)
        asset_root = temporary_root / ASSET_SOURCE_ROOT
        asset_root.mkdir(parents=True)
        required = asset_root / "required.toml"
        required.write_text("fixture", encoding="ascii")
        extra = asset_root / "untracked.toml"
        extra.write_text("must not enter manifest", encoding="ascii")
        synthetic_manifest = ["data/F4SE/Plugins/required.toml"]
        manifested = tracked_asset_files(temporary_root, synthetic_manifest)
        if manifested != {"F4SE/Plugins/required.toml"}:
            raise RuntimeError("untracked asset changed the declared manifest")

        required.unlink()
        try:
            tracked_asset_files(temporary_root, synthetic_manifest)
        except RuntimeError:
            pass
        else:
            raise RuntimeError("missing tracked asset was not rejected")

        stale = set(release)
        stale.add((PLUGIN_ROOT / "dmui-forwarding-smoke.dll").as_posix())
        try:
            validate_names("release", stale, "self-test stale")
        except RuntimeError:
            pass
        else:
            raise RuntimeError("stale smoke DLL was not rejected")

        wrong_variant = set(test)
        try:
            validate_names("release", wrong_variant, "self-test wrong variant")
        except RuntimeError:
            pass
        else:
            raise RuntimeError("test client was not rejected from the release package")

        incomplete = release - {(PLUGIN_ROOT / "DearModdingUI-MCM.dll").as_posix()}
        try:
            validate_names("release", incomplete, "self-test missing bridge")
        except RuntimeError:
            pass
        else:
            raise RuntimeError("missing bridge was not rejected")

        assembled = temporary_root / "assembled"
        assembled.mkdir()
        payload = assembled / "fixture.txt"
        payload.write_text("current", encoding="ascii")
        archive = temporary_root / "fixture.zip"
        with zipfile.ZipFile(archive, "w") as package:
            package.write(payload, "fixture.txt")
        validate_contents(assembled, archive)
        payload.write_text("changed", encoding="ascii")
        try:
            validate_contents(assembled, archive)
        except RuntimeError:
            pass
        else:
            raise RuntimeError("stale archive content was not rejected")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--variant", choices=("release", "test"))
    parser.add_argument("--folder", type=pathlib.Path)
    parser.add_argument("--archive", type=pathlib.Path)
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()

    if arguments.self_test:
        self_test()
    if arguments.variant:
        if not arguments.folder or not arguments.archive:
            parser.error("--variant requires --folder and --archive")
        validate_names(
            arguments.variant,
            folder_names(arguments.folder),
            str(arguments.folder),
        )
        validate_names(
            arguments.variant,
            archive_names(arguments.archive),
            str(arguments.archive),
        )
        validate_contents(arguments.folder, arguments.archive)
    elif not arguments.self_test:
        parser.error("select --self-test or provide --variant")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
