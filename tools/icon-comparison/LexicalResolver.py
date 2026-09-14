from __future__ import annotations

from dataclasses import asdict, dataclass
import gzip
import os
from pathlib import Path
import sqlite3
import time
import unicodedata
import uuid
import xml.etree.ElementTree as ET

from Models import ARTIFACTS, Candidate, Icon, Query, Selection, content_hash, file_hash


OEWN_2025_SHA256 = "9ca6d1dcb75f822fdd66617f7d9da48142ace38dd544d6ad5e2feca1674ad3fe"
SCHEMA_VERSION = "1"
ALGORITHM_VERSION = "bounded-oewn-ordinal-v1"
MAX_ARTIFACT_BYTES = 1024 * 1024 * 1024
MAX_LOOKUP_ROWS = 2048
MIN_MEANINGFUL_COVERAGE = 2 / 3

_STOP_WORDS = frozenset(
    {
        "a",
        "an",
        "and",
        "are",
        "as",
        "at",
        "be",
        "by",
        "for",
        "from",
        "in",
        "is",
        "it",
        "of",
        "on",
        "or",
        "that",
        "the",
        "this",
        "to",
        "with",
    }
)


class LexicalDataError(RuntimeError):
    pass


class LexicalIndexError(RuntimeError):
    pass


@dataclass(frozen=True)
class IndexInfo:
    path: str
    source_digest: str
    catalog_hash: str
    status: str
    build_seconds: float
    size_bytes: int


@dataclass(frozen=True)
class _Phrase:
    text: str
    start: int
    end: int
    covered: int
    total: int
    token_total: int

    @property
    def coverage(self) -> float:
        return self.covered / self.total if self.total else 0.0

    @property
    def full(self) -> bool:
        return self.start == 0 and self.end == self.token_total


@dataclass(frozen=True)
class _Evidence:
    glyph: int
    name: str
    tier: str
    input_phrase: str
    coverage: float
    covered_terms: int
    total_terms: int
    full_phrase: bool
    source_synset: str
    source_definition: str
    target_synset: str
    target_definition: str
    catalog_term: str
    relation: str
    context_overlap: int
    ordinal: int
    sense_eligible: bool = True


def _normalize(text: str) -> str:
    decomposed = unicodedata.normalize("NFKD", text.casefold())
    characters: list[str] = []
    pending_space = False
    for character in decomposed:
        if unicodedata.combining(character):
            continue
        if character.isalnum():
            if pending_space and characters:
                characters.append(" ")
            characters.append(character)
            pending_space = False
        else:
            pending_space = True
    return "".join(characters)


def _tokens(text: str) -> tuple[str, ...]:
    normalized = _normalize(text)
    return tuple(normalized.split()) if normalized else ()


def _meaningful(tokens: tuple[str, ...]) -> frozenset[str]:
    return frozenset(token for token in tokens if token not in _STOP_WORDS)


def _local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def _definition(element: ET.Element) -> str:
    for child in element.iter():
        if _local_name(child.tag) == "Definition":
            return " ".join("".join(child.itertext()).split())
    return ""


def _is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


def _tree_size(root: Path, excluded: Path | None = None) -> int:
    total = 0
    pending = [root]
    excluded_path = excluded.resolve() if excluded is not None else None
    while pending:
        directory = pending.pop()
        try:
            with os.scandir(directory) as entries:
                for entry in entries:
                    try:
                        if entry.is_dir(follow_symlinks=False):
                            pending.append(Path(entry.path))
                        elif entry.is_file(follow_symlinks=False) and (
                            excluded_path is None
                            or Path(entry.path).resolve() != excluded_path
                        ):
                            total += entry.stat(follow_symlinks=False).st_size
                    except FileNotFoundError:
                        continue
        except FileNotFoundError:
            continue
    return total


class LexicalResolver:
    def __init__(
        self,
        icons: tuple[Icon, ...],
        dataset_path: Path = ARTIFACTS / "Data" / "english-wordnet-2025.xml.gz",
        index_dir: Path = ARTIFACTS / "Indexes",
        *,
        expected_digest: str = OEWN_2025_SHA256,
        max_artifact_bytes: int = MAX_ARTIFACT_BYTES,
    ) -> None:
        if not icons:
            raise ValueError("icons must not be empty")
        if len({icon.glyph for icon in icons}) != len(icons):
            raise ValueError("icons must have unique glyphs")
        if (
            len(expected_digest) != 64
            or any(character not in "0123456789abcdefABCDEF" for character in expected_digest)
        ):
            raise ValueError("expected_digest must be a SHA-256 hexadecimal digest")
        if max_artifact_bytes <= 0:
            raise ValueError("max_artifact_bytes must be positive")

        self._icons = icons
        self._dataset_path = Path(dataset_path)
        self._index_dir = Path(index_dir)
        self._expected_digest = expected_digest.lower()
        self._max_artifact_bytes = max_artifact_bytes
        self._connection: sqlite3.Connection | None = None

        source_digest = self._verify_dataset()
        catalog_hash = content_hash(
            [
                asdict(icon)
                for icon in sorted(
                    icons,
                    key=lambda icon: (
                        icon.glyph,
                        icon.name,
                        icon.aliases,
                        icon.domains,
                        icon.tags,
                    ),
                )
            ]
        )
        cache_name = (
            f"Lexical-{SCHEMA_VERSION}-{ALGORITHM_VERSION}-"
            f"{source_digest[:16]}-{catalog_hash[:16]}.sqlite3"
        )
        cache_path = self._index_dir / cache_name

        started = time.perf_counter()
        if cache_path.exists():
            connection = self._open_cache(cache_path, source_digest, catalog_hash)
            status = "cache_hit"
            build_seconds = 0.0
        else:
            self._index_dir.mkdir(parents=True, exist_ok=True)
            self._ensure_budget("before lexical index build")
            self._build_cache(cache_path, source_digest, catalog_hash)
            connection = self._open_cache(cache_path, source_digest, catalog_hash)
            status = "built"
            build_seconds = time.perf_counter() - started

        self._connection = connection
        self.index_info = IndexInfo(
            path=str(cache_path),
            source_digest=source_digest,
            catalog_hash=catalog_hash,
            status=status,
            build_seconds=build_seconds,
            size_bytes=cache_path.stat().st_size,
        )

    def close(self) -> None:
        if self._connection is not None:
            self._connection.close()
            self._connection = None

    def __enter__(self) -> LexicalResolver:
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def resolve(self, query: Query) -> Selection:
        query.validate()
        connection = self._require_connection()
        heading_tokens = _tokens(query.heading)
        meaningful_positions = tuple(
            index
            for index, token in enumerate(heading_tokens)
            if token not in _STOP_WORDS
        )
        if not meaningful_positions:
            return Selection(
                status="no_match",
                reason="lexical OOV: heading has no meaningful normalized terms",
            )

        phrases = self._matched_phrases(connection, heading_tokens, meaningful_positions)
        if not phrases:
            return Selection(
                status="no_match",
                reason="lexical OOV: no OEWN word form matched a whole input phrase",
            )

        context_words = _meaningful(_tokens(query.context))
        evidence, truncated = self._collect_evidence(connection, phrases, context_words)
        if truncated:
            return Selection(
                status="no_match",
                reason=(
                    "lexical ambiguous: a word form exceeded the bounded "
                    f"{MAX_LOOKUP_ROWS}-row evidence limit"
                ),
            )
        if not evidence:
            return Selection(
                status="no_match",
                reason=(
                    "lexical no_match: matched OEWN senses have no same-synset "
                    "or one-hop hypernym catalog term"
                ),
            )

        phrase_senses = self._phrase_senses(connection, phrases, context_words)
        evidence = self._mark_sense_eligibility(evidence, phrase_senses)
        best_by_glyph: dict[int, _Evidence] = {}
        best_eligible_by_glyph: dict[int, _Evidence] = {}
        for item in evidence:
            existing = best_by_glyph.get(item.glyph)
            if existing is None or self._evidence_order(item) < self._evidence_order(existing):
                best_by_glyph[item.glyph] = item
            if item.sense_eligible and self._has_meaningful_coverage(item):
                eligible_existing = best_eligible_by_glyph.get(item.glyph)
                if (
                    eligible_existing is None
                    or self._evidence_order(item)
                    < self._evidence_order(eligible_existing)
                ):
                    best_eligible_by_glyph[item.glyph] = item

        ranked = sorted(best_by_glyph.values(), key=self._evidence_order)
        candidates = tuple(self._candidate(item) for item in ranked[:3])
        eligible = sorted(
            best_eligible_by_glyph.values(), key=self._evidence_order
        )
        margin = (
            float(ranked[0].ordinal - ranked[1].ordinal)
            if len(ranked) > 1
            else None
        )

        if not eligible:
            best = ranked[0]
            if not self._has_meaningful_coverage(best):
                reason = (
                    "lexical weak evidence: best whole-phrase evidence covers "
                    f"{best.covered_terms}/{best.total_terms} meaningful heading terms"
                )
            elif any(not item.sense_eligible for item in ranked):
                reason = (
                    "lexical ambiguous: competing productive senses are not "
                    "disambiguated by gloss/lemma context overlap"
                )
            else:
                reason = "lexical weak evidence: no candidate meets the coverage policy"
            return Selection(
                status="no_match",
                reason=reason,
                candidates=candidates,
                margin=margin,
            )

        winner = eligible[0]
        if (
            self._has_meaningful_coverage(ranked[0])
            and not ranked[0].sense_eligible
            and ranked[0].ordinal >= winner.ordinal
        ):
            return Selection(
                status="no_match",
                reason=(
                    "lexical ambiguous: stronger competing productive senses are not "
                    "disambiguated by gloss/lemma context overlap"
                ),
                candidates=candidates,
                margin=margin,
            )
        if len(eligible) > 1 and eligible[1].ordinal == winner.ordinal:
            return Selection(
                status="no_match",
                reason=(
                    "lexical ambiguous: top candidates have equal ordinal evidence; "
                    "shown in glyph codepoint order"
                ),
                candidates=candidates,
                margin=0.0,
            )

        eligible_margin = (
            float(winner.ordinal - eligible[1].ordinal)
            if len(eligible) > 1
            else None
        )
        return Selection(
            status="selected",
            glyph=winner.glyph,
            name=winner.name,
            reason=self._reason(winner),
            candidates=candidates,
            margin=eligible_margin,
        )

    def _verify_dataset(self) -> str:
        if not self._dataset_path.exists():
            raise LexicalDataError(
                f"OEWN dataset is missing: {self._dataset_path}. "
                "Acquire the pinned artifact explicitly; this resolver never downloads it."
            )
        if not self._dataset_path.is_file():
            raise LexicalDataError(f"OEWN dataset is not a file: {self._dataset_path}")
        try:
            digest = file_hash(self._dataset_path)
        except OSError as error:
            raise LexicalDataError(
                f"Unable to read OEWN dataset {self._dataset_path}: {error}"
            ) from error
        if digest != self._expected_digest:
            raise LexicalDataError(
                f"OEWN dataset SHA-256 mismatch for {self._dataset_path}: "
                f"expected {self._expected_digest}, got {digest}"
            )
        return digest

    def _open_cache(
        self, path: Path, source_digest: str, catalog_hash: str
    ) -> sqlite3.Connection:
        try:
            connection = sqlite3.connect(path)
            connection.row_factory = sqlite3.Row
            check = connection.execute("PRAGMA quick_check").fetchone()
            if check is None or check[0] != "ok":
                raise LexicalIndexError(
                    f"Lexical index integrity check failed for {path}: "
                    f"{check[0] if check else 'no result'}"
                )
            metadata = dict(connection.execute("SELECT key, value FROM metadata"))
        except LexicalIndexError:
            connection.close()
            raise
        except (sqlite3.DatabaseError, OSError) as error:
            try:
                connection.close()
            except UnboundLocalError:
                pass
            raise LexicalIndexError(f"Lexical index is corrupt or unreadable: {path}: {error}") from error

        expected = {
            "schema_version": SCHEMA_VERSION,
            "algorithm_version": ALGORITHM_VERSION,
            "source_digest": source_digest,
            "catalog_hash": catalog_hash,
        }
        mismatches = [
            f"{key}={metadata.get(key)!r}, expected {value!r}"
            for key, value in expected.items()
            if metadata.get(key) != value
        ]
        if mismatches:
            connection.close()
            raise LexicalIndexError(
                f"Lexical index metadata mismatch for {path}: " + "; ".join(mismatches)
            )
        try:
            self._ensure_budget("while opening lexical index")
        except LexicalIndexError:
            connection.close()
            raise
        return connection

    def _build_cache(
        self, path: Path, source_digest: str, catalog_hash: str
    ) -> None:
        temporary = path.with_name(
            f".{path.name}.{os.getpid()}.{uuid.uuid4().hex}.tmp"
        )
        connection: sqlite3.Connection | None = None
        try:
            connection = sqlite3.connect(temporary)
            connection.executescript(
                """
                PRAGMA page_size = 4096;
                PRAGMA journal_mode = DELETE;
                PRAGMA synchronous = FULL;
                PRAGMA temp_store = FILE;

                CREATE TABLE metadata(
                    key TEXT PRIMARY KEY,
                    value TEXT NOT NULL
                ) WITHOUT ROWID;
                CREATE TABLE synsets(
                    id TEXT PRIMARY KEY,
                    definition TEXT NOT NULL
                ) WITHOUT ROWID;
                CREATE TABLE forms(
                    term TEXT NOT NULL,
                    synset_id TEXT NOT NULL,
                    kind TEXT NOT NULL,
                    PRIMARY KEY(term, synset_id, kind)
                ) WITHOUT ROWID;
                CREATE TABLE relations(
                    source_synset TEXT NOT NULL,
                    target_synset TEXT NOT NULL,
                    relation TEXT NOT NULL,
                    PRIMARY KEY(source_synset, target_synset, relation)
                ) WITHOUT ROWID;
                CREATE TABLE catalog_terms(
                    term TEXT NOT NULL,
                    glyph INTEGER NOT NULL,
                    name TEXT NOT NULL,
                    catalog_term TEXT NOT NULL,
                    PRIMARY KEY(term, glyph, catalog_term)
                ) WITHOUT ROWID;
                CREATE TABLE catalog_senses(
                    synset_id TEXT NOT NULL,
                    glyph INTEGER NOT NULL,
                    name TEXT NOT NULL,
                    catalog_term TEXT NOT NULL,
                    PRIMARY KEY(synset_id, glyph, catalog_term)
                ) WITHOUT ROWID;
                """
            )
            self._stream_dataset(connection)
            self._insert_catalog(connection)
            connection.executescript(
                """
                CREATE INDEX forms_synset_idx ON forms(synset_id);
                CREATE INDEX relations_source_idx
                    ON relations(source_synset, relation, target_synset);
                CREATE INDEX catalog_senses_synset_idx
                    ON catalog_senses(synset_id, glyph);
                ANALYZE;
                """
            )
            connection.executemany(
                "INSERT INTO metadata(key, value) VALUES(?, ?)",
                (
                    ("schema_version", SCHEMA_VERSION),
                    ("algorithm_version", ALGORITHM_VERSION),
                    ("source_digest", source_digest),
                    ("catalog_hash", catalog_hash),
                ),
            )
            connection.commit()
            self._ensure_budget("during lexical index build")
            check = connection.execute("PRAGMA integrity_check").fetchone()
            if check is None or check[0] != "ok":
                raise LexicalIndexError(
                    f"New lexical index failed integrity check: "
                    f"{check[0] if check else 'no result'}"
                )
            connection.close()
            connection = None
            temporary.replace(path)
            self._ensure_budget("after lexical index build")
        except (gzip.BadGzipFile, ET.ParseError, EOFError, OSError) as error:
            raise LexicalDataError(
                f"Unable to parse checksummed OEWN LMF dataset "
                f"{self._dataset_path}: {error}"
            ) from error
        except sqlite3.DatabaseError as error:
            raise LexicalIndexError(f"Unable to build lexical index {path}: {error}") from error
        finally:
            if connection is not None:
                connection.close()
            temporary.unlink(missing_ok=True)

    def _stream_dataset(self, connection: sqlite3.Connection) -> None:
        entries = 0
        synsets = 0
        with gzip.open(self._dataset_path, "rb") as stream:
            stack: list[ET.Element] = []
            for event, element in ET.iterparse(stream, events=("start", "end")):
                if event == "start":
                    stack.append(element)
                    continue

                name = _local_name(element.tag)
                processed = False
                if name == "LexicalEntry":
                    written_forms: set[tuple[str, str]] = set()
                    synset_ids: set[str] = set()
                    for child in element.iter():
                        child_name = _local_name(child.tag)
                        if child_name == "Lemma":
                            normalized = _normalize(child.attrib.get("writtenForm", ""))
                            if normalized:
                                written_forms.add((normalized, "lemma"))
                        elif child_name == "Form":
                            normalized = _normalize(child.attrib.get("writtenForm", ""))
                            if normalized:
                                written_forms.add((normalized, "form"))
                        elif child_name == "Sense":
                            synset_id = child.attrib.get("synset", "")
                            if synset_id:
                                synset_ids.add(synset_id)
                    connection.executemany(
                        "INSERT OR IGNORE INTO forms(term, synset_id, kind) VALUES(?, ?, ?)",
                        (
                            (term, synset_id, kind)
                            for term, kind in written_forms
                            for synset_id in synset_ids
                        ),
                    )
                    entries += 1
                    processed = True
                elif name == "Synset":
                    synset_id = element.attrib.get("id", "")
                    if synset_id:
                        connection.execute(
                            "INSERT OR REPLACE INTO synsets(id, definition) VALUES(?, ?)",
                            (synset_id, _definition(element)),
                        )
                        connection.executemany(
                            """
                            INSERT OR IGNORE INTO relations(
                                source_synset, target_synset, relation
                            ) VALUES(?, ?, ?)
                            """,
                            (
                                (
                                    synset_id,
                                    child.attrib.get("target", ""),
                                    child.attrib.get("relType", ""),
                                )
                                for child in element.iter()
                                if _local_name(child.tag) == "SynsetRelation"
                                and child.attrib.get("relType")
                                in {"hypernym", "instance_hypernym"}
                                and child.attrib.get("target")
                            ),
                        )
                        synsets += 1
                        processed = True

                if name in {"LexicalEntry", "Synset"}:
                    if len(stack) >= 2:
                        stack[-2].remove(element)
                    element.clear()
                stack.pop()
                if processed and (entries + synsets) % 5000 == 0:
                    connection.commit()
                    self._ensure_budget("during lexical index build")

        connection.commit()
        if entries == 0 or synsets == 0:
            raise LexicalDataError(
                "OEWN LMF dataset contains no usable lexical entries or synsets"
            )
        orphaned = connection.execute(
            """
            SELECT COUNT(*)
            FROM forms
            LEFT JOIN synsets ON synsets.id = forms.synset_id
            WHERE synsets.id IS NULL
            """
        ).fetchone()[0]
        if orphaned:
            raise LexicalDataError(
                f"OEWN LMF dataset contains {orphaned} forms with missing synsets"
            )

    def _insert_catalog(self, connection: sqlite3.Connection) -> None:
        rows = []
        for icon in self._icons:
            for catalog_term in icon.terms:
                normalized = _normalize(catalog_term)
                if normalized:
                    rows.append((normalized, icon.glyph, icon.name, catalog_term))
        connection.executemany(
            """
            INSERT OR IGNORE INTO catalog_terms(term, glyph, name, catalog_term)
            VALUES(?, ?, ?, ?)
            """,
            rows,
        )
        connection.execute(
            """
            INSERT OR IGNORE INTO catalog_senses(
                synset_id, glyph, name, catalog_term
            )
            SELECT forms.synset_id, catalog_terms.glyph,
                   catalog_terms.name, catalog_terms.catalog_term
            FROM catalog_terms
            JOIN forms ON forms.term = catalog_terms.term
            """
        )
        if connection.execute("SELECT COUNT(*) FROM catalog_senses").fetchone()[0] == 0:
            raise LexicalDataError(
                "No fixed catalog terms occur in the checksummed OEWN dataset"
            )
        connection.commit()
        self._ensure_budget("during lexical catalog indexing")

    def _matched_phrases(
        self,
        connection: sqlite3.Connection,
        heading_tokens: tuple[str, ...],
        meaningful_positions: tuple[int, ...],
    ) -> tuple[_Phrase, ...]:
        total = len(heading_tokens)
        candidates: dict[str, list[tuple[int, int]]] = {}
        if total:
            candidates.setdefault(" ".join(heading_tokens), []).append((0, total))
        for start in range(total):
            for end in range(start + 1, min(total, start + 8) + 1):
                text = " ".join(heading_tokens[start:end])
                candidates.setdefault(text, []).append((start, end))

        found: set[str] = set()
        terms = tuple(candidates)
        for offset in range(0, len(terms), 500):
            batch = terms[offset : offset + 500]
            placeholders = ",".join("?" for _ in batch)
            found.update(
                row[0]
                for row in connection.execute(
                    f"SELECT DISTINCT term FROM forms WHERE term IN ({placeholders})",
                    batch,
                )
            )

        phrases: list[_Phrase] = []
        meaningful = set(meaningful_positions)
        for text in sorted(found):
            for start, end in candidates[text]:
                covered = sum(1 for position in range(start, end) if position in meaningful)
                if covered:
                    phrases.append(
                        _Phrase(
                            text=text,
                            start=start,
                            end=end,
                            covered=covered,
                            total=len(meaningful_positions),
                            token_total=total,
                        )
                    )
        return tuple(
            sorted(
                phrases,
                key=lambda phrase: (
                    -int(phrase.full),
                    -phrase.coverage,
                    -(phrase.end - phrase.start),
                    phrase.text,
                    phrase.start,
                ),
            )
        )

    def _collect_evidence(
        self,
        connection: sqlite3.Connection,
        phrases: tuple[_Phrase, ...],
        context_words: frozenset[str],
    ) -> tuple[list[_Evidence], bool]:
        rows: list[tuple[_Phrase, sqlite3.Row]] = []
        truncated = False
        for phrase in phrases:
            query_rows = connection.execute(
                """
                SELECT DISTINCT
                    catalog_senses.glyph,
                    catalog_senses.name,
                    catalog_senses.catalog_term,
                    forms.synset_id AS source_synset,
                    synsets.definition AS source_definition,
                    forms.synset_id AS target_synset,
                    synsets.definition AS target_definition,
                    'same_synset' AS relation
                FROM forms
                JOIN synsets ON synsets.id = forms.synset_id
                JOIN catalog_senses
                    ON catalog_senses.synset_id = forms.synset_id
                WHERE forms.term = ?
                UNION ALL
                SELECT DISTINCT
                    catalog_senses.glyph,
                    catalog_senses.name,
                    catalog_senses.catalog_term,
                    forms.synset_id AS source_synset,
                    source.definition AS source_definition,
                    relations.target_synset AS target_synset,
                    target.definition AS target_definition,
                    relations.relation AS relation
                FROM forms
                JOIN synsets AS source ON source.id = forms.synset_id
                JOIN relations ON relations.source_synset = forms.synset_id
                JOIN synsets AS target ON target.id = relations.target_synset
                JOIN catalog_senses
                    ON catalog_senses.synset_id = relations.target_synset
                WHERE forms.term = ?
                LIMIT ?
                """,
                (phrase.text, phrase.text, MAX_LOOKUP_ROWS + 1),
            ).fetchall()
            if len(query_rows) > MAX_LOOKUP_ROWS:
                truncated = True
                continue
            rows.extend((phrase, row) for row in query_rows)

        source_synsets = {row["source_synset"] for _, row in rows}
        sense_words = self._sense_words(connection, source_synsets)
        evidence = []
        for phrase, row in rows:
            relation = row["relation"]
            tier = "synonym" if relation == "same_synset" else "hypernym"
            overlap = len(context_words & sense_words.get(row["source_synset"], frozenset()))
            base = 3000 if tier == "synonym" else 2000
            ordinal = (
                base
                + (200 if phrase.full else 0)
                + round(phrase.coverage * 100)
                + min(overlap, 20) * 2
                + min(len(phrase.text.split()), 10)
            )
            evidence.append(
                _Evidence(
                    glyph=row["glyph"],
                    name=row["name"],
                    tier=tier,
                    input_phrase=phrase.text,
                    coverage=phrase.coverage,
                    covered_terms=phrase.covered,
                    total_terms=phrase.total,
                    full_phrase=phrase.full,
                    source_synset=row["source_synset"],
                    source_definition=row["source_definition"],
                    target_synset=row["target_synset"],
                    target_definition=row["target_definition"],
                    catalog_term=row["catalog_term"],
                    relation=relation,
                    context_overlap=overlap,
                    ordinal=ordinal,
                )
            )
        return evidence, truncated

    def _sense_words(
        self, connection: sqlite3.Connection, synset_ids: set[str]
    ) -> dict[str, frozenset[str]]:
        if not synset_ids:
            return {}
        result: dict[str, set[str]] = {synset_id: set() for synset_id in synset_ids}
        ordered = tuple(sorted(synset_ids))
        for offset in range(0, len(ordered), 500):
            batch = ordered[offset : offset + 500]
            placeholders = ",".join("?" for _ in batch)
            for row in connection.execute(
                f"""
                SELECT synsets.id, synsets.definition, forms.term
                FROM synsets
                LEFT JOIN forms
                    ON forms.synset_id = synsets.id AND forms.kind = 'lemma'
                WHERE synsets.id IN ({placeholders})
                """,
                batch,
            ):
                words = result[row["id"]]
                words.update(_meaningful(_tokens(row["definition"])))
                if row["term"]:
                    words.update(_meaningful(_tokens(row["term"])))
        return {
            synset_id: frozenset(words)
            for synset_id, words in result.items()
        }

    def _phrase_senses(
        self,
        connection: sqlite3.Connection,
        phrases: tuple[_Phrase, ...],
        context_words: frozenset[str],
    ) -> dict[str, dict[str, int]]:
        phrase_synsets: dict[str, set[str]] = {}
        for phrase in phrases:
            if phrase.text in phrase_synsets:
                continue
            phrase_synsets[phrase.text] = {
                row[0]
                for row in connection.execute(
                    "SELECT DISTINCT synset_id FROM forms WHERE term = ?",
                    (phrase.text,),
                )
            }
        sense_words = self._sense_words(
            connection,
            {
                synset_id
                for synsets in phrase_synsets.values()
                for synset_id in synsets
            },
        )
        return {
            phrase: {
                synset_id: len(
                    context_words & sense_words.get(synset_id, frozenset())
                )
                for synset_id in synsets
            }
            for phrase, synsets in phrase_synsets.items()
        }

    def _mark_sense_eligibility(
        self,
        evidence: list[_Evidence],
        phrase_senses: dict[str, dict[str, int]],
    ) -> list[_Evidence]:
        resolved: dict[str, str | None] = {}
        for phrase, senses in phrase_senses.items():
            if len(senses) <= 1:
                resolved[phrase] = ""
                continue
            ranked = sorted(
                ((overlap, synset) for synset, overlap in senses.items()),
                key=lambda item: (-item[0], item[1]),
            )
            if ranked[0][0] > 0 and (
                len(ranked) == 1 or ranked[0][0] > ranked[1][0]
            ):
                resolved[phrase] = ranked[0][1]
            else:
                resolved[phrase] = None

        return [
            _Evidence(
                **{
                    **item.__dict__,
                    "sense_eligible": (
                        resolved[item.input_phrase] == ""
                        or resolved[item.input_phrase] == item.source_synset
                    ),
                }
            )
            for item in evidence
        ]

    @staticmethod
    def _has_meaningful_coverage(item: _Evidence) -> bool:
        return item.full_phrase or (
            item.coverage >= MIN_MEANINGFUL_COVERAGE
            and (
                item.covered_terms >= 2
                or item.covered_terms == item.total_terms
            )
        )

    @staticmethod
    def _evidence_order(item: _Evidence) -> tuple[object, ...]:
        return (
            -item.ordinal,
            item.glyph,
            item.name,
            item.catalog_term,
            item.source_synset,
            item.target_synset,
        )

    def _candidate(self, item: _Evidence) -> Candidate:
        return Candidate(
            glyph=item.glyph,
            name=item.name,
            score=float(item.ordinal),
            reason=self._reason(item),
        )

    @staticmethod
    def _reason(item: _Evidence) -> str:
        source_definition = item.source_definition or "(no definition)"
        if item.relation == "same_synset":
            path = (
                f"{item.input_phrase} --same_synset({item.source_synset})--> "
                f"{item.catalog_term}"
            )
            definitions = (
                f"synset={item.source_synset}; definition={source_definition!r}"
            )
        else:
            target_definition = item.target_definition or "(no definition)"
            path = (
                f"{item.input_phrase} --sense({item.source_synset}) "
                f"--{item.relation}--> {item.target_synset} "
                f"--lemma--> {item.catalog_term}"
            )
            definitions = (
                f"source_synset={item.source_synset}; "
                f"source_definition={source_definition!r}; "
                f"target_synset={item.target_synset}; "
                f"target_definition={target_definition!r}"
            )
        return (
            f"tier={item.tier}; ordinal={item.ordinal}; "
            f"coverage={item.covered_terms}/{item.total_terms} "
            f"({round(item.coverage * 100)}%); "
            f"context_overlap={item.context_overlap}; "
            f"catalog_term={item.catalog_term!r}; {definitions}; path={path}"
        )

    def _require_connection(self) -> sqlite3.Connection:
        if self._connection is None:
            raise RuntimeError("LexicalResolver is closed")
        return self._connection

    def _ensure_budget(self, operation: str) -> None:
        usage = self._artifact_usage()
        if usage > self._max_artifact_bytes:
            raise LexicalIndexError(
                f"Icon comparison artifacts use {usage} bytes during {operation}, "
                f"exceeding the configured {self._max_artifact_bytes}-byte budget"
            )

    def _artifact_usage(self) -> int:
        dataset_in_artifacts = _is_relative_to(self._dataset_path, ARTIFACTS)
        index_in_artifacts = _is_relative_to(self._index_dir, ARTIFACTS)
        total = 0
        if (dataset_in_artifacts or index_in_artifacts) and ARTIFACTS.exists():
            total += _tree_size(ARTIFACTS)
        if not dataset_in_artifacts:
            total += self._dataset_path.stat().st_size
        if not index_in_artifacts and self._index_dir.exists():
            total += _tree_size(self._index_dir, self._dataset_path)
        return total
