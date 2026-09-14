from __future__ import annotations

from itertools import product


METHODS = ("baseline", "lexical", "embedding")


def count_outcomes(rows: list[dict], method: str, raw: bool = False) -> dict:
    counts = {
        "queries": len(rows), "families": len({row["family"] for row in rows}),
        "selections": 0, "within_authored_set": 0, "outside_authored_set": 0,
        "abstentions": 0, "allowed_abstentions": 0,
        "negative_queries": 0, "negative_false_matches": 0,
    }
    for row in rows:
        result = row["raw"][method] if raw and method != "baseline" else row["final"][method]
        acceptable = row["acceptable"]
        negative = not acceptable
        counts["negative_queries"] += negative
        if result["status"] == "no_match":
            counts["abstentions"] += 1
            counts["allowed_abstentions"] += row["abstain_ok"]
        else:
            counts["selections"] += 1
            matches = result["name"] in acceptable
            counts["within_authored_set"] += matches
            counts["outside_authored_set"] += not matches
            counts["negative_false_matches"] += negative
    return counts


def summarize(rows: list[dict]) -> dict:
    splits = {}
    for split in ("development", "held_out"):
        selected = [row for row in rows if row["split"] == split]
        misses = [row for row in selected if row["final"]["baseline"]["status"] == "no_match"]
        splits[split] = {
            "all_queries": {method: count_outcomes(selected, method) for method in METHODS},
            "baseline_misses": {method: count_outcomes(misses, method) for method in METHODS},
            "raw_diagnostics": {method: count_outcomes(selected, method, True) for method in METHODS},
            "strata": {
                stratum: {
                    method: count_outcomes([row for row in selected if row["stratum"] == stratum], method)
                    for method in METHODS
                }
                for stratum in sorted({row["stratum"] for row in selected})
            },
        }
    bases = {row["family"]: row for row in rows if row["id"].endswith("/base")}
    context = {}
    for method in METHODS:
        pairs = []
        for row in rows:
            if not row["query"]["context"]:
                continue
            base = bases[row["family"]]
            before, after = base["final"][method], row["final"][method]
            pairs.append({
                "id": row["id"],
                "changed": (before["status"], before["glyph"]) != (after["status"], after["glyph"]),
                "before_within_context_set": before["name"] in row["acceptable"],
                "after_within_context_set": after["name"] in row["acceptable"],
            })
        context[method] = pairs
    return {
        "judgment": "PROVISIONAL: authored acceptable sets, not confirmed accuracy",
        "confirmed_quality_queries": 0,
        "splits": splits, "context_pairs": context,
    }


def choose_threshold(development: list[dict], config: dict) -> dict:
    if any(row["split"] != "development" for row in development):
        raise ValueError("Threshold tuning must not inspect held-out rows")
    sweep = []
    for threshold, margin in product(config["threshold_grid"], config["margin_grid"]):
        good = wrong = issued = negative_matches = 0
        for row in development:
            if row["final"]["baseline"]["status"] != "no_match":
                continue
            candidates = row["raw"]["embedding"]["candidates"]
            if len(candidates) < 2:
                continue
            gap = candidates[0]["score"] - candidates[1]["score"]
            if candidates[0]["score"] < threshold or gap < margin:
                continue
            if row["stratum"] == "negative":
                negative_matches += 1
            else:
                issued += 1
            if candidates[0]["name"] in row["acceptable"]:
                good += 1
            else:
                wrong += 1
        sweep.append({
            "threshold": threshold, "margin": margin,
            "within_authored_set": good, "outside_authored_set": wrong,
            "issued_nonnegative": issued,
            "negative_matches": negative_matches,
        })
    eligible = [point for point in sweep if point["negative_matches"] == 0]
    if not eligible:
        raise ValueError("No development operating point meets the declared false-match policy")
    selected = max(eligible, key=lambda point: (
        point["issued_nonnegative"], point["threshold"], point["margin"],
    ))
    return {
        "selected": selected, "sweep": sweep,
        "policy": config["threshold_policy"],
        "label_status": "provisional authored sets; no held-out tuning",
    }
