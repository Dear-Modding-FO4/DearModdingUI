import argparse
import hashlib
import json
from pathlib import Path


EXPECTED_ANCHORS = {
    "app-window": 0xE5DA,
    "gear": 0xE270,
    "monitor": 0xE32E,
    "puzzle-piece": 0xE596,
    "question": 0xE3E8,
    "sun": 0xE472,
    "trash": 0xE4A6,
}
EXPECTED_COUNT = 1512
EXPECTED_FONT_SHA256 = "a53f5d2630cab5e3b7536ecb9d69d71519a2190298c22b1f8d770dd37bc2940a"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("selection", type=Path)
    parser.add_argument("font", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()

    if hashlib.sha256(arguments.font.read_bytes()).hexdigest() != EXPECTED_FONT_SHA256:
        raise ValueError("Phosphor Fill font SHA-256 does not match @phosphor-icons/web@2.1.2")

    source = json.loads(arguments.selection.read_text(encoding="utf-8"))
    glyphs = {
        icon["properties"]["name"].removesuffix("-fill"): icon["properties"]["code"]
        for icon in source["icons"]
    }
    if len(glyphs) != EXPECTED_COUNT:
        raise ValueError(f"expected {EXPECTED_COUNT} named icons, found {len(glyphs)}")
    for name, codepoint in EXPECTED_ANCHORS.items():
        if glyphs.get(name) != codepoint:
            raise ValueError(f"{name} does not match the pinned codepoint")

    lines = [
        "#pragma once",
        "",
        "// Generated from @phosphor-icons/web@2.1.2 src/fill/selection.json.",
        "namespace DearModdingUI",
        "{",
        f"\tinline constexpr std::array<IconGlyphMapping, {EXPECTED_COUNT}> kPhosphorIconGlyphs{{{{",
    ]
    lines.extend(
        f'\t\t{{ "{name}", 0x{codepoint:04X} }},'
        for name, codepoint in sorted(glyphs.items())
    )
    lines.extend(["\t}};", "}", ""])
    arguments.output.write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
