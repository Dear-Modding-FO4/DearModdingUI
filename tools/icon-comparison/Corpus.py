from __future__ import annotations

from collections import Counter
from dataclasses import dataclass
import json
from pathlib import Path

from Models import ARTIFACTS, Icon, Query, file_hash


SOURCE = Path(__file__).with_name("Corpus.json")
CONFIG = Path(__file__).with_name("Evaluation.json")


@dataclass(frozen=True)
class Case:
    id: str
    family: str
    split: str
    stratum: str
    source: str
    query: Query
    acceptable: tuple[str, ...]
    abstain_ok: bool


def load_cases(path: Path = SOURCE) -> tuple[Case, ...]:
    document = json.loads(path.read_text(encoding="utf-8"))
    families = document["families"]
    if len(families) != 60 or len({f["id"] for f in families}) != 60:
        raise ValueError("Expected 60 distinct corpus families")
    counts = Counter((family["split"], family["stratum"]) for family in families)
    if len(counts) != 12 or any(count != 5 for count in counts.values()):
        raise ValueError("Expected five families in each split/stratum")
    cases = []
    for family in families:
        for variant in [{"id": "base"}, *family.get("variants", [])]:
            value = family | variant
            query = Query(value["heading"], value.get("context", ""))
            query.validate()
            cases.append(Case(
                id=f"{family['id']}/{variant['id']}",
                family=family["id"],
                split=family["split"],
                stratum=family["stratum"],
                source=family["source"],
                query=query,
                acceptable=tuple(value["acceptable"]),
                abstain_ok=value["abstain_ok"],
            ))
    if len({case.id for case in cases}) != len(cases):
        raise ValueError("Duplicate corpus query ID")
    return tuple(cases)


def freeze_corpus(icons: tuple[Icon, ...]) -> dict:
    cases = load_cases()
    names = {icon.name for icon in icons}
    invalid = sorted({name for case in cases for name in case.acceptable} - names)
    if invalid:
        raise ValueError(f"Acceptable sets reference absent catalog names: {invalid}")
    record = {
        "corpus_sha256": file_hash(SOURCE),
        "catalog_sha256": file_hash(ARTIFACTS / "Catalog.json"),
        "family_count": 60,
        "query_count": len(cases),
        "judgment": "provisional authored acceptable sets, not confirmed accuracy",
    }
    path = ARTIFACTS / "FrozenCorpus.json"
    if path.exists():
        if json.loads(path.read_text(encoding="utf-8")) != record:
            raise ValueError("Frozen corpus/catalog changed; do not reuse held-out results")
    else:
        path.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return record


def load_config() -> dict:
    return json.loads(CONFIG.read_text(encoding="utf-8"))
