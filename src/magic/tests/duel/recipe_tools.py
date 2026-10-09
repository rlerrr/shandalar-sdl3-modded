"""Offline recipe inventory, compact enrollment, and failure-only review reports."""
import argparse
import collections
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

from generate_cases import registry
from recipes import classify, VERSION, VARIANTS

DATA = Path(__file__).parent / "data"


def context(data):
    oracle = json.loads((data / "oracle.json").read_text(encoding="utf-8"))
    aliases = json.loads((data / "oracle_aliases.json").read_text())["aliases"]
    homes = {}
    for path in sorted((data / "cases").rglob("*.json")):
        d = json.loads(path.read_text(encoding="utf-8"))
        if "card" in d:
            if d["csvid"] in homes:
                raise ValueError(f"duplicate card home: {path}")
            homes[d["csvid"]] = (path, d)
    return oracle, aliases, homes


def inventory(data):
    oracle, aliases, homes = context(data)
    records, unique_ids = [], set()
    counts = collections.Counter()
    for row in registry():
        record = oracle["cards"].get(aliases.get(row["name"], row["name"]))
        category = row["kind"]
        if category == "card":
            category = "token" if record and record.get("type_line", "").startswith("Token ") else "mapped" if record else "unmapped"
        counts[category] += 1
        if category == "mapped":
            unique_ids.add(record["oracle_id"])
        spec, reason = classify(row["name"], record) if category == "mapped" else (None, category)
        home = homes.get(row["csvid"])
        records.append(dict(card=row["name"], csvid=row["csvid"], category=category,
                            oracle_id=record.get("oracle_id") if record else None,
                            authored=bool(home), family=spec["family"] if spec else None,
                            manual_reason=reason, enrolled=bool(home and home[1].get("recipes")),
                            dispatch_hint="card_dummy" if row["handler"] == "card_dummy" else "other"))
    families = {}
    for family in sorted({r["family"] for r in records if r["family"]}):
        matching = [r for r in records if r["family"] == family]
        families[family] = dict(eligible=len(matching), enrolled=sum(r["enrolled"] for r in matching),
                                without_authored_home=sum(not r["authored"] for r in matching),
                                dummy_dispatch=sum(r["dispatch_hint"] == "card_dummy" for r in matching))
    return dict(version=1, source=oracle["source"], recipe_version=VERSION,
                counts=dict(counts), mapped_unique_oracle_ids=len(unique_ids),
                mapped_records_sharing_oracle_identity=counts["mapped"] - len(unique_ids),
                identity_note="Shared Oracle IDs describe identity overlap, not a verified printing/set classification.",
                families=families, manual_reasons=dict(collections.Counter(r["manual_reason"] for r in records if r["manual_reason"])),
                records=records)


def enroll(data, names, family, uncovered, limit, write):
    if family and family not in VARIANTS:
        raise ValueError(f"unknown recipe family: {family}")
    report = inventory(data)
    oracle, aliases, homes = context(data)
    rows = [r for r in report["records"] if r["family"] and (not family or r["family"] == family)
            and (not names or r["card"] in names) and (not uncovered or not r["authored"])]
    if names and set(names) != {r["card"] for r in rows}:
        raise ValueError(f"unsupported/unknown requested cards: {sorted(set(names)-{r['card'] for r in rows})}")
    rows = sorted(rows, key=lambda r: r["csvid"])
    # Never partially mutate on a rejected selection: validate everything first.
    if limit is not None:
        rows = rows[:limit]
    plans = []
    for row in rows:
        declaration = dict(family=row["family"], version=VERSION)
        if row["csvid"] in homes:
            path, document = homes[row["csvid"]]
            if any(d.get("family") == row["family"] for d in document.get("recipes", [])):
                continue
        else:
            slug = re.sub(r"[^a-z0-9]+", "-", row["card"].lower()).strip("-")
            path = data / "cases/cards" / f"{row['csvid']//100:03d}" / f"{row['csvid']:05d}-{slug}.json"
            document = dict(version=1, card=row["card"], csvid=row["csvid"])
            if path.exists():
                raise ValueError(f"refusing to overwrite unrelated home: {path}")
        plans.append((path, document, declaration))
    if write:
        for path, document, declaration in plans:
            path.parent.mkdir(parents=True, exist_ok=True)
            if path.exists() and "recipes" not in document:
                # Preserve the original authored content and line endings byte for byte.
                raw = path.read_bytes().rstrip()
                newline = b"\r\n" if b"\r\n" in raw else b"\n"
                addition = json.dumps([declaration], separators=(",", ":")).encode()
                path.write_bytes(raw[:-1].rstrip() + b"," + newline + b'  "recipes": ' + addition + newline + b"}" + newline)
            else:
                document.setdefault("recipes", []).append(declaration)
                path.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return [dict(path=str(p), card=d["card"], csvid=d["csvid"], recipe=r) for p, d, r in plans]


def review(paths, brief=False):
    """Small diagnostics, not model-written replacements or automatic baselines."""
    runs = []
    for path in paths:
        summary = json.loads(path.read_text())
        failures = []
        for result in summary["results"]:
            if result["status"] in ("passed", "known_failure", "unimplemented"):
                continue
            diagnostics = []
            xml = Path(result["xml"])
            if xml.exists():
                for f in ET.parse(xml).iter("failure"):
                    message = f.attrib.get("message", f.text or "")
                    head, _, trace = message.partition("Google Test trace:")
                    operation = re.search(r"step \d+ ([^\r\n]+)", trace)
                    diagnostics.append(dict(operation=operation[1] if operation else None,
                                            assertion="\n".join(head.splitlines()[1:])[:1200]))
            if not diagnostics:
                log = Path(result["log"]).read_text(encoding="utf-8", errors="replace")
                markers = re.findall(r"^(?:DUEL_HARNESS_ERROR|DUEL_ENGINE_ERROR):.*$", log, re.M)
                diagnostics = [dict(error=x) for x in markers] or [dict(error=result["failure_kind"], log=result["log"])]
            item = {k: result.get(k) for k in ("filter", "card", "csvid", "oracle_id", "recipe", "status", "failure_kind", "assertion_count", "source_file", "log", "xml")}
            item["diagnostics"] = diagnostics[:2] if brief else diagnostics
            if brief:
                item["additional_diagnostics"] = max(0, len(diagnostics) - 2)
                item["diagnostic_operations"] = sorted({d["operation"] for d in diagnostics if d.get("operation")})
            failures.append(item)
        # Buckets help review common mechanisms, but never substitute for exact baselines.
        buckets = collections.defaultdict(list)
        for f in failures:
            recipe = f.get("recipe") or {}
            key = "/".join([recipe.get("family", "manual"), recipe.get("variant", ""), f["failure_kind"]])
            buckets[key].append(f["filter"])
        runs.append(dict(summary=str(path), counts=summary["counts"], review_buckets=dict(buckets), failures=failures))
    return dict(version=1, runs=runs)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--data", type=Path, default=DATA)
    sub = p.add_subparsers(dest="command", required=True)
    inv = sub.add_parser("inventory")
    inv.add_argument("--output", type=Path, required=True)
    en = sub.add_parser("enroll")
    en.add_argument("--cards", nargs="+")
    en.add_argument("--family", choices=sorted(VARIANTS))
    en.add_argument("--uncovered", action="store_true")
    en.add_argument("--limit", type=int)
    en.add_argument("--write", action="store_true", help="otherwise print a dry-run plan")
    rep = sub.add_parser("review")
    rep.add_argument("summaries", nargs="+", type=Path)
    rep.add_argument("--output", type=Path, required=True)
    rep.add_argument("--brief", action="store_true", help="two sample assertions per case, plus all operation names and full log/XML links")
    a = p.parse_args()
    if a.command == "enroll":
        if not (a.cards or a.family):
            p.error("enroll requires an explicit card list or family")
        if a.limit is not None and a.limit < 1:
            p.error("limit must be positive")
        print(json.dumps(enroll(a.data, a.cards, a.family, a.uncovered, a.limit, a.write), indent=2))
    else:
        output = inventory(a.data) if a.command == "inventory" else review(a.summaries, a.brief)
        a.output.parent.mkdir(parents=True, exist_ok=True)
        a.output.write_text(json.dumps(output, indent=2) + "\n")
        print(json.dumps(output["families"] if a.command == "inventory" else [r["counts"] for r in output["runs"]], indent=2))


if __name__ == "__main__":
    main()
