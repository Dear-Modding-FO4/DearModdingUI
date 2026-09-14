from __future__ import annotations

from dataclasses import asdict, dataclass
import hashlib
import json
from pathlib import Path
from typing import Literal, Protocol


ROOT = Path(__file__).resolve().parents[2]
ARTIFACTS = ROOT / ".Build" / "IconComparison"
QUESTION = 0xE3E8
Status = Literal["selected", "no_match", "invalid_raw_glyph"]


@dataclass(frozen=True)
class Query:
    heading: str
    context: str = ""
    explicit_name: str = ""
    explicit_glyph: int | None = None

    def validate(self) -> None:
        for name, text, limit in (
            ("heading", self.heading, 256),
            ("context", self.context, 256),
            ("explicit_name", self.explicit_name, 128),
        ):
            if not isinstance(text, str) or len(text.encode("utf-8")) > limit:
                raise ValueError(f"{name} must be text of at most {limit} UTF-8 bytes")
            if any((ord(c) < 32 and c != "\t") or ord(c) == 127 for c in text):
                raise ValueError(f"{name} contains a disallowed control character")
        if self.explicit_glyph is not None and (
            type(self.explicit_glyph) is not int
            or not 0 <= self.explicit_glyph <= 0xFFFFFFFF
        ):
            raise ValueError("explicit_glyph must be an unsigned 32-bit integer")


@dataclass(frozen=True)
class Icon:
    glyph: int
    name: str
    aliases: tuple[str, ...] = ()
    domains: tuple[str, ...] = ()
    tags: tuple[str, ...] = ()

    @property
    def terms(self) -> tuple[str, ...]:
        return tuple(sorted(set((self.name, *self.aliases, *self.domains, *self.tags))))

    @property
    def descriptor(self) -> str:
        related = ", ".join(term.replace("-", " ") for term in self.terms)
        return f"{self.name.replace('-', ' ')}. Related terms: {related}."


@dataclass(frozen=True)
class Candidate:
    glyph: int
    name: str
    score: float | None
    reason: str


@dataclass(frozen=True)
class Selection:
    status: Status
    glyph: int = 0
    name: str = ""
    reason: str = ""
    candidates: tuple[Candidate, ...] = ()
    margin: float | None = None

    def to_dict(self) -> dict:
        return asdict(self)


class Resolver(Protocol):
    def resolve(self, query: Query) -> Selection: ...


def load_catalog(path: Path = ARTIFACTS / "Catalog.json") -> tuple[Icon, ...]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    icons = tuple(
        Icon(
            glyph=entry["glyph"],
            name=entry["name"],
            aliases=tuple(entry["aliases"]),
            domains=tuple(entry["domains"]),
            tags=tuple(entry["tags"]),
        )
        for entry in payload["icons"]
    )
    if len(icons) != 1512 or len({icon.glyph for icon in icons}) != len(icons):
        raise ValueError("Catalog must contain the pinned 1,512 unique Phosphor glyphs")
    return icons


def content_hash(value: object) -> str:
    return hashlib.sha256(
        json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()


def file_hash(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def preserve_baseline(baseline: Selection, suggestion: Selection) -> Selection:
    return suggestion if baseline.status == "no_match" else baseline
