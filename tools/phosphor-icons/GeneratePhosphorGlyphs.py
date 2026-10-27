import argparse
import hashlib
import json
from pathlib import Path


EXPECTED_GLYPH_COUNT = 1512
EXPECTED_FONT_SHA256 = (
    "a53f5d2630cab5e3b7536ecb9d69d71519a2190298c22b1f8d770dd37bc2940a"
)
EXPECTED_CATALOG_SHA256 = (
    "fb94760f6163db61ba0dc4606ad48480107c986c862f577163852d1c86172e9f"
)
EXPECTED_METADATA_SHA256 = (
    "f312607486c9ef74690915e32b75e7bb06106810556d794012f4a944d1de4aea"
)
EXPECTED_METADATA_CODEPOINT_MISMATCHES = {
    "building-office": (0xE0FF, 0xE0FE),
    "crane-tower": (0xED49, 0xED48),
    "file-ini": (0xEB33, 0xEB32),
    "file-txt": (0xEB35, 0xEB32),
    "jar-label": (0xE7E1, 0xE7E0),
    "lego-smiley": (0xE8C7, 0xE8C6),
    "question-mark": (0xE3E9, 0xE3E8),
    "solar-roof": (0xED7B, 0xED7A),
    "tip-jar": (0xE7E2, 0xE7E0),
}
NORMALIZATION_EXAMPLES = {
    "DearModdingUI": "dear-modding-ui",
    "UISettings": "ui-settings",
    "3D Camera": "3-d-camera",
    "arrow_counter.clockwise": "arrow-counter-clockwise",
    "  Power---Armor  ": "power-armor",
}


class GenerationError(ValueError):
    pass


def normalize(value: str) -> str:
    result: list[str] = []
    separator_pending = False
    previous_lower_or_digit = False
    previous_upper = False
    for index, value_character in enumerate(value):
        character = ord(value_character)
        next_character = ord(value[index + 1]) if index + 1 < len(value) else 0
        next_lower = ord("a") <= next_character <= ord("z")
        if (
            ord("a") <= character <= ord("z")
            or ord("0") <= character <= ord("9")
        ):
            if separator_pending and result:
                result.append("-")
            result.append(value_character)
            separator_pending = False
            previous_lower_or_digit = True
            previous_upper = False
        elif ord("A") <= character <= ord("Z"):
            if (
                separator_pending
                or previous_lower_or_digit
                or (previous_upper and next_lower)
            ) and result and result[-1] != "-":
                result.append("-")
            result.append(chr(character - ord("A") + ord("a")))
            separator_pending = False
            previous_lower_or_digit = False
            previous_upper = True
        else:
            separator_pending = bool(result)
            previous_lower_or_digit = False
            previous_upper = False
    return "".join(result)


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def validate_pinned_source(source: dict, expected_sha256: str) -> None:
    canonical = json.dumps(
        source, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    )
    actual = hashlib.sha256(canonical.encode("ascii")).hexdigest()
    if actual != expected_sha256:
        raise GenerationError(
            f"pinned Phosphor input checksum mismatch: {actual} != {expected_sha256}"
        )


def add_unique(mapping: dict[str, int], phrase: str, glyph: int, source: str) -> None:
    existing = mapping.get(phrase)
    if existing is not None and existing != glyph:
        raise GenerationError(
            f'normalized {source} collision for "{phrase}": '
            f"0x{existing:04X} and 0x{glyph:04X}"
        )
    mapping[phrase] = glyph


def build_vocabulary(catalog_source: dict, metadata_source: dict, domains_source: dict):
    glyphs: dict[str, int] = {}
    font_aliases: dict[str, int] = {}
    for entry in catalog_source["glyphs"]:
        name = normalize(entry["name"])
        if not name:
            raise GenerationError("font catalog contains an empty normalized name")
        add_unique(glyphs, name, entry["codepoint"], "canonical name")
        for alias_value in entry.get("aliases", []):
            alias = normalize(alias_value)
            if alias:
                add_unique(font_aliases, alias, entry["codepoint"], "font alias")

    if len(glyphs) != EXPECTED_GLYPH_COUNT:
        raise GenerationError(
            f"expected {EXPECTED_GLYPH_COUNT} glyphs, found {len(glyphs)}"
        )

    aliases: dict[str, int] = {}
    tag_pairs: set[tuple[str, int]] = set()
    excluded: list[str] = []
    mismatches: dict[str, tuple[int, int]] = {}
    metadata_names: set[str] = set()
    for entry in metadata_source["icons"]:
        name = normalize(entry["name"])
        metadata_names.add(name)
        glyph = glyphs.get(name)
        if glyph is None:
            excluded.append(name)
            continue
        metadata_codepoint = entry["codepoint"]
        if metadata_codepoint != glyph:
            mismatches[name] = (glyph, metadata_codepoint)

        alias_entry = entry.get("alias")
        if alias_entry:
            alias = normalize(alias_entry["name"])
            if font_aliases.get(alias) != glyph:
                raise GenerationError(
                    f'upstream alias "{alias}" does not match the shipped font'
                )
            canonical_collision = glyphs.get(alias)
            if canonical_collision is not None and canonical_collision != glyph:
                raise GenerationError(
                    f'accepted alias "{alias}" conflicts with a canonical icon'
                )
            add_unique(aliases, alias, glyph, "accepted alias")

        for raw_tag in entry.get("tags", []):
            tag = normalize(raw_tag)
            if (
                not tag
                or tag in {"new", "updated"}
                or tag.isdigit()
            ):
                continue
            tag_pairs.add((tag, glyph))

    missing_metadata = sorted(set(glyphs) - metadata_names)
    if missing_metadata:
        raise GenerationError(
            "shipped icons missing metadata: " + ", ".join(missing_metadata)
        )
    if mismatches != EXPECTED_METADATA_CODEPOINT_MISMATCHES:
        raise GenerationError(
            "metadata/font codepoint mismatch set changed: "
            + json.dumps(
                {
                    name: [f"0x{actual:04X}", f"0x{metadata:04X}"]
                    for name, (actual, metadata) in sorted(mismatches.items())
                },
                sort_keys=True,
            )
        )

    domains: dict[str, int] = {}
    for entry in domains_source["preferences"]:
        phrase = normalize(entry["phrase"])
        icon = normalize(entry["icon"])
        if not phrase:
            raise GenerationError("domain preference contains an empty phrase")
        glyph = glyphs.get(icon)
        if glyph is None:
            raise GenerationError(
                f'domain preference "{phrase}" references unknown icon "{icon}"'
            )
        if phrase in glyphs or phrase in aliases:
            raise GenerationError(
                f'domain preference "{phrase}" duplicates authoritative vocabulary'
            )
        add_unique(domains, phrase, glyph, "domain preference")

    return glyphs, aliases, domains, sorted(tag_pairs), sorted(excluded)


def render_mapping(name: str, entries: list[tuple[str, int]]) -> list[str]:
    lines = [
        f"\tinline constexpr std::array<IconPhraseMapping, {len(entries)}> {name}{{{{"
    ]
    lines.extend(
        f'\t\t{{ "{phrase}", 0x{glyph:04X} }},' for phrase, glyph in entries
    )
    lines.append("\t}};")
    return lines


def render(catalog_source: dict, metadata_source: dict, domains_source: dict) -> tuple[str, list[str]]:
    glyphs, aliases, domains, tags, excluded = build_vocabulary(
        catalog_source, metadata_source, domains_source
    )
    lines = [
        "#pragma once",
        "",
        "// Generated by Tools/GeneratePhosphorGlyphs.py from pinned Phosphor inputs.",
        "namespace dmui",
        "{",
    ]
    lines.extend(render_mapping("kPhosphorIconGlyphs", sorted(glyphs.items())))
    lines.append("")
    lines.extend(render_mapping("kPhosphorIconAliases", sorted(aliases.items())))
    lines.append("")
    lines.extend(render_mapping("kPhosphorIconDomainTerms", sorted(domains.items())))
    lines.append("")
    lines.extend(render_mapping("kPhosphorIconTags", tags))
    lines.extend(["}", ""])
    return "\n".join(lines), excluded


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    sources = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--catalog",
        type=Path,
        default=sources / "PhosphorFillCatalog.json",
    )
    parser.add_argument(
        "--metadata",
        type=Path,
        default=sources / "PhosphorMetadata.json",
    )
    parser.add_argument(
        "--domains",
        type=Path,
        default=sources / "DomainIconPreferences.json",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=root / "include/DearModdingUI/presentation/PhosphorGlyphs.generated.h",
    )
    parser.add_argument("--font", type=Path)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    for source, expected in NORMALIZATION_EXAMPLES.items():
        actual = normalize(source)
        if actual != expected:
            raise GenerationError(
                f'normalization drift for "{source}": "{actual}" != "{expected}"'
            )

    if arguments.font:
        digest = hashlib.sha256(arguments.font.read_bytes()).hexdigest()
        if digest != EXPECTED_FONT_SHA256:
            raise GenerationError(
                "Phosphor Fill font SHA-256 does not match @phosphor-icons/web@2.1.2"
            )

    catalog = read_json(arguments.catalog)
    metadata = read_json(arguments.metadata)
    validate_pinned_source(catalog, EXPECTED_CATALOG_SHA256)
    validate_pinned_source(metadata, EXPECTED_METADATA_SHA256)
    rendered, excluded = render(catalog, metadata, read_json(arguments.domains))
    print(f"Shipped glyphs: {EXPECTED_GLYPH_COUNT}")
    print(f"Excluded metadata icons absent from the shipped font: {len(excluded)}")
    for name in excluded:
        print(f"  {name}")
    print(
        "Pinned metadata/font codepoint differences validated: "
        f"{len(EXPECTED_METADATA_CODEPOINT_MISMATCHES)}"
    )

    if arguments.check:
        if not arguments.output.exists():
            raise GenerationError(f"generated output is missing: {arguments.output}")
        if arguments.output.read_text(encoding="utf-8") != rendered:
            raise GenerationError(f"generated output is stale: {arguments.output}")
        return

    arguments.output.write_text(rendered, encoding="utf-8")


if __name__ == "__main__":
    main()
