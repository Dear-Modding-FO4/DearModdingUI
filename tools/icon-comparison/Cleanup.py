from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import stat

from Measurements import footprint
from Models import ARTIFACTS, ROOT


OWNED_DIRECTORIES = (
    "Data", "Model", "Wheels", "Environment", "Cache", "Indexes",
    "Bin", "Logs", "BuildCache", "C",
)


def cleanup_targets() -> tuple[Path, ...]:
    targets = tuple(ARTIFACTS / name for name in OWNED_DIRECTORIES) + (
        ROOT / ".LinkConf" / "xmake" / "dmui-icon-comparison",
        Path(__file__).parent / "__pycache__",
        Path(__file__).parent / "tests" / "__pycache__",
    )
    root = ROOT.resolve()
    for path in targets:
        if path.is_symlink() or path.is_junction() or not path.resolve().is_relative_to(root):
            raise ValueError(f"Refusing unexpected cleanup target: {path}")
    return targets


def remove_tree(path: Path) -> list[str]:
    root = path.resolve()
    if root in {ROOT.resolve(), ARTIFACTS.resolve()} or not root.is_relative_to(ROOT.resolve()):
        raise ValueError(f"Refusing broad or external cleanup target: {path}")
    readonly_files = []

    def retry_readonly(operation, filename, error):
        target = Path(filename)
        if (
            not isinstance(error, PermissionError)
            or target.is_symlink()
            or not target.resolve().is_relative_to(root)
            or not target.is_file()
            or not getattr(target.stat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_READONLY
        ):
            raise error
        target.chmod(stat.S_IWRITE)
        readonly_files.append(str(target))
        operation(filename)

    shutil.rmtree(path, onexc=retry_readonly)
    return readonly_files


def main() -> None:
    parser = argparse.ArgumentParser(description="Delete only named, spike-owned acquired/build artifacts")
    parser.add_argument("--execute", action="store_true", help="Delete after preserving and reviewing the report")
    args = parser.parse_args()
    targets = cleanup_targets()
    manifest = {
        "executed": args.execute,
        "targets": [{"path": str(path), "bytes": footprint(path)} for path in targets],
        "retained": "Source, pins, Catalog/FrozenCorpus/OperatingPoint JSON, and Reports",
        "readonly_files_cleared": [],
    }
    if args.execute:
        for required in ("Comparison.html", "Comparison.json", "Measurements.json"):
            if not (ARTIFACTS / "Reports" / required).is_file():
                raise ValueError(f"Preserve {required} before deleting comparison inputs")
        for path in targets:
            if path.exists():
                manifest["readonly_files_cleared"].extend(remove_tree(path))
        remaining = [str(path) for path in targets if path.exists()]
        if remaining:
            raise RuntimeError(f"Cleanup incomplete: {remaining}")
        (ARTIFACTS / "Reports" / "Cleanup.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8",
        )
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
