from __future__ import annotations

import html
import json
import os
from pathlib import Path

from Models import QUESTION, ROOT
from Scoring import METHODS


def escape(value: object) -> str:
    return html.escape(str(value), quote=True)


def selection(value: dict) -> str:
    status = value["status"]
    glyph = QUESTION if status == "no_match" else value["glyph"]
    valid = 0 < glyph <= 0x10FFFF and not 0xD800 <= glyph <= 0xDFFF
    icon = f"&#{glyph};" if valid else ""
    name = "Question (abstain)" if status == "no_match" else value["name"] or "Raw glyph"
    candidates = value.get("candidates", [])
    score = ""
    if candidates:
        first = candidates[0]
        score = f"<div>Top score: {first['score']:.4f}"
        if value.get("margin") is not None:
            score += f"; margin: {value['margin']:.4f}"
        score += "</div>"
    nearest = "".join(
        f"<li>{escape(item['name'])}: {item['score']:.4f}<br>{escape(item['reason'])}</li>"
        for item in candidates
        if item["score"] is not None
    )
    return (
        f'<div class="choice"><span class="icon">{icon}</span>'
        f"<strong>{escape(name)}</strong> <code>U+{glyph:04X}</code></div>"
        f'<div class="reason">{escape(value["reason"])}</div>{score}'
        + (f"<details><summary>Nearest candidates</summary><ol>{nearest}</ol></details>" if nearest else "")
    )


def resource_table(document: dict) -> str:
    measurements = document.get("measurements")
    if measurements is None:
        return "<p>Resource measurements have not run yet.</p>"
    rows = []
    for method in METHODS:
        result = measurements["methods"][method]
        warm = result["warm"]
        process_memory = warm.get("native_memory", warm["final_memory"])
        native = ""
        if "native_resolver_ms" in warm:
            native = f"<br>Native only: {warm['native_resolver_ms']['p50']:.4f} / {warm['native_resolver_ms']['p95']:.4f}"
        rows.append(
            f"<tr><td>{method}</td>"
            f"<td>{result['preparation']['initialization_ms']:.2f}</td>"
            f"<td>{result['initialization_ms']['p50']:.2f} / {result['initialization_ms']['p95']:.2f}</td>"
            f"<td>{warm['latency_ms']['p50']:.4f} / {warm['latency_ms']['p95']:.4f}{native}</td>"
            f"<td>{process_memory['working_set'] / 1048576:.1f} / "
            f"{process_memory['peak_working_set'] / 1048576:.1f} / "
            f"{process_memory['private_bytes'] / 1048576:.1f}</td>"
            f"<td>{warm['repeated_selection_changes']} / {warm['compared_repeats']}</td></tr>"
        )
    disks = "".join(
        f"<tr><td>{escape(name)}</td><td>{size / 1048576:.2f}</td></tr>"
        for name, size in measurements["artifact_bytes"].items()
    )
    return f"""
<h2>Measured harness costs</h2>
<p>Windows x64, Ryzen 7 7800X3D. ORT intra-op/inter-op threads are both one; tokenizer parallelism is disabled.
NumPy BLAS threading is not separately pinned. Queries are batch-one with no query cache.
Fresh-process initialization uses prepared indexes and does not flush the OS file cache.
Times are milliseconds. Baseline memory below is its native child; the Python bridge is recorded separately in Measurements.json.
Lexical and embedding memory includes their Python processes, not a hypothetical C++ host.
Native-helper initialization measures process creation; its first-query readiness cost is recorded separately in Measurements.json.</p>
<table><thead><tr><th>Method</th><th>Fresh-index preparation</th><th>Initialization p50 / p95</th><th>Warm query p50 / p95</th>
<th>Working / peak working / private MiB</th><th>Repeated selection changes</th></tr></thead>
<tbody>{"".join(rows)}</tbody></table>
<details><summary>Actual pre-cleanup disk footprint (MiB)</summary>
<p>This ledger includes download archives and installed files, rather than counting only the model.
Environment and indexes serve the harness; existing machine Python and repository dependency checkouts are not included.
No numbers here establish production DLL or game costs.</p>
<table><thead><tr><th>Artifact directory</th><th>MiB</th></tr></thead><tbody>{disks}</tbody></table></details>
<p>OEWN: CC BY 4.0 plus Princeton attribution, no model runtime.
BGE: MIT weights, ONNX Runtime (MIT), Tokenizers (Apache-2.0), and locked transitive packages.
Native Windows inference is feasible through ONNX Runtime's C/C++ API, but exact native tokenizer integration and game lifecycle behavior have not been implemented or measured.</p>
"""


def write_report(document: dict, destination: Path) -> None:
    font = ROOT / "data" / "F4SE" / "Plugins" / "DearModdingUI" / "Fonts" / "Phosphor" / "Phosphor-Fill.ttf"
    relative_font = os.path.relpath(font, destination.parent).replace(os.sep, "/")
    rows = []
    for row in document["rows"]:
        protected = row["final"]["baseline"]["status"] != "no_match"
        cells = []
        for method in METHODS:
            value = selection(row["final"][method])
            if method != "baseline":
                value += "<details><summary>Raw semantic diagnostic"
                value += " (not applied)</summary>" if protected else "</summary>"
                value += selection(row["raw"][method]) + "</details>"
            cells.append(f"<td>{value}</td>")
        rows.append(
            f'<tr data-split="{escape(row["split"])}" data-protected="{str(protected).lower()}">'
            f'<th><code>{escape(row["id"])}</code><h3>{escape(row["query"]["heading"]) or "(empty)"}</h3>'
            f'<p>{escape(row["query"]["context"])}</p>'
            f'<small>{escape(row["split"])} / {escape(row["stratum"])}<br>'
            f'{escape(row["source"])}</small><p>{"Protected baseline match" if protected else "Eligible fallback"}</p>'
            f'<details><summary>Provisional acceptable set</summary>{escape(", ".join(row["acceptable"]) or "No intended match")}'
            f'; abstention allowed: {row["abstain_ok"]}</details></th>'
            + "".join(cells) + "</tr>"
        )
    summary_rows = []
    for split, groups in document["summary"]["splits"].items():
        for method, counts in groups["baseline_misses"].items():
            summary_rows.append(
                f"<tr><td>{escape(split)}</td><td>{method}</td><td>{counts['queries']}</td>"
                f"<td>{counts['within_authored_set']}</td><td>{counts['outside_authored_set']}</td>"
                f"<td>{counts['abstentions']}</td></tr>"
            )
    payload = json.dumps(document["provenance"], indent=2)
    configuration = document["provenance"].get("configuration", {})
    operating_point = (
        f"Embedding operating point: cosine at least {configuration.get('embedding_threshold', 'not recorded')}, "
        f"top-two margin at least {configuration.get('embedding_margin', 'not recorded')}. "
        "Calibrated on development negative controls only; positive quality remains ungraded."
    )
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(f"""<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>Semantic icon comparison</title>
<style>
@font-face{{font-family:Phosphor;src:url("{escape(relative_font)}") format("truetype")}}
body{{font:15px system-ui,sans-serif;background:#13161d;color:#e1e6ee;margin:24px}}
h1{{margin-bottom:8px}}p{{line-height:1.5}}.notice{{border-left:4px solid #e5b86a;padding:12px;background:#24232a}}
table{{border-collapse:collapse;width:100%;margin:20px 0}}th,td{{border:1px solid #3a414e;padding:12px;text-align:left;vertical-align:top}}
thead{{position:sticky;top:0;background:#202631}}th{{width:24%}}td{{width:25%}}
code,small,.reason{{color:#aebbd0}}.icon{{font-family:Phosphor;font-size:32px;vertical-align:middle;margin-right:10px}}
.choice{{margin-bottom:8px}}details{{margin-top:12px}}summary{{cursor:pointer}}li{{margin:10px 0}}
button{{padding:8px 14px;margin:4px;background:#283244;color:#fff;border:1px solid #69758b;border-radius:4px}}
pre{{white-space:pre-wrap;overflow-wrap:anywhere}}
</style>
<h1>Semantic icon comparison</h1>
<p class="notice"><strong>Not a production migration. Quality judgments are provisional.</strong>
The initial acceptable sets are authored hypotheses, not Kuz-approved ground truth.
No confirmed accuracy is claimed. A cosine score is not a probability and cannot be compared with lexical evidence tiers.</p>
<p>Existing selections are locked. Only genuine baseline misses can receive semantic fallback.
Question with an abstention label means no selection, not an explicit Question icon.
Raw diagnostics show possible semantic alternatives without applying them.</p>
<p>{escape(operating_point)}</p>
<h2>Baseline-miss triage, not confirmed quality</h2>
<table><thead><tr><th>Split</th><th>Method</th><th>Queries</th><th>Within authored set</th><th>Outside authored set</th><th>Abstain</th></tr></thead>
<tbody>{"".join(summary_rows)}</tbody></table>
<p>Counts include related variants, not independent samples. Review individual choices below before interpreting this as quality evidence.</p>
{resource_table(document)}
<button onclick="filterRows('all')">All</button><button onclick="filterRows('development')">Development</button>
<button onclick="filterRows('held_out')">Held out</button><button onclick="filterRows('miss')">Baseline misses</button>
<table id="comparison"><thead><tr><th>Heading and context</th><th>Existing resolver</th><th>Open English WordNet</th><th>BGE-small ONNX</th></tr></thead>
<tbody>{"".join(rows)}</tbody></table>
<details><summary>Reproducibility record</summary><pre>{escape(payload)}</pre></details>
<p><small>Lexical excerpts: Open English WordNet team, derived from Princeton WordNet.
Modified: selected definitions, relation paths, and a derived comparison index.
<a href="https://github.com/globalwordnet/english-wordnet/blob/dc343f2683279ecbb13fab4e2fd778d7b162d287/LICENSE.md">CC BY 4.0 and Princeton terms</a>.
Icons: Phosphor, MIT; the shipped license remains beside the font.
BGE-small-en-v1.5: BAAI, MIT.
Links are reference-only; viewing this report does not fetch them.</small></p>
<script>
function filterRows(mode){{document.querySelectorAll('#comparison tbody tr').forEach(row=>{{
row.hidden=!(mode==='all'||row.dataset.split===mode||(mode==='miss'&&row.dataset.protected==='false'));}});}}
</script></html>
""", encoding="utf-8")
