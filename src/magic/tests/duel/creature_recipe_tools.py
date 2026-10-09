"""Offline, fail-closed enrollment of whole-text vanilla and keyword creatures."""
import argparse
import collections
import json
import re
from pathlib import Path

from creature_recipes import classify, variants
from generate_cases import registry
from recipe_tools import context, DATA
from recipes import expand


def survey(data):
    oracle, aliases, homes = context(data)
    rows = {r["name"]: r for r in registry() if r["kind"] == "card"}
    plans, deferred = [], []
    for name, row in rows.items():
        record = oracle["cards"].get(aliases.get(name, name), {})
        if "Creature" not in record.get("type_line", ""):
            continue
        home = homes.get(row["csvid"])
        if home and home[1].get("recipes"):
            continue
        try:
            specification = classify(record)
        except ValueError as error:
            deferred.append(dict(card=name, csvid=row["csvid"], reason=str(error)))
            continue
        # Vanilla no-op handlers are valid. Other no-op records can need missing
        # keyword hooks or metadata; don't silently call them implemented.
        if specification["keywords"] and row["handler"] == "card_dummy":
            deferred.append(dict(card=name, csvid=row["csvid"], reason="keyword no-op dispatch needs implementation review"))
            continue
        declaration = dict(family="creature_keywords", version=2,
                           params=dict(keywords=specification["keywords"]),
                           oracle_text=record.get("oracle_text", ""), variants=variants(specification))
        cases = expand(name, row["csvid"], declaration, record, rows)
        plans.append(dict(card=name, csvid=row["csvid"], declaration=declaration, cases=len(cases),
                          new_home=not home, covered_components=[k for k in specification["keywords"] if k != "Flash"],
                          gaps=["Flash timing"] if "Flash" in specification["keywords"] else []))
    return sorted(plans, key=lambda p:p["csvid"]), deferred, homes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cards", nargs="+")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    plans, deferred, homes = survey(args.data)
    if args.cards:
        plans = [p for p in plans if p["card"] in args.cards]
        missing = set(args.cards)-{p["card"] for p in plans}
        if missing: parser.error(f"ineligible requested cards: {sorted(missing)}")
    if args.write:
        for plan in plans:
            name, csvid = plan["card"], plan["csvid"]
            if csvid in homes:
                path, document = homes[csvid]
            else:
                slug = re.sub("[^a-z0-9]+", "-", name.lower()).strip("-")
                path = args.data / "cases/cards" / f"{csvid//100:03d}" / f"{csvid:05d}-{slug}.json"
                document = dict(version=1, card=name, csvid=csvid)
                if path.exists(): raise ValueError(f"unrelated existing home: {path}")
            document["recipes"] = [plan["declaration"]]
            document["recipe_coverage"] = ("Whole-text creature recipe: normal casting, exact payment, insufficient mana, populated-zone snapshot replay, "
                "fresh-cast attack eligibility, attack tapping, unblocked damage, grounded/flying block legality, and tapped creature rejection. "
                "Applicable keyword combat components are composed from the declaration; Flash timing, lost/gained abilities, and multiplayer are separate.")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps(document, indent=2, ensure_ascii=False)+"\n", encoding="utf-8")
    report = dict(version=1, cards=len(plans), new_card_homes=sum(p["new_home"] for p in plans),
                  vanilla_cards=sum(not p["declaration"]["params"]["keywords"] for p in plans),
                  keyword_cards=sum(bool(p["declaration"]["params"]["keywords"]) for p in plans),
                  behavior_cases=sum(p["cases"] for p in plans),
                  components=dict(collections.Counter(k for p in plans for k in p["covered_components"])),
                  deferred_reasons=dict(collections.Counter(p["reason"] for p in deferred)), plans=plans, deferred=deferred)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False)+"\n", encoding="utf-8")
    print(json.dumps({k:v for k,v in report.items() if k not in ("plans", "deferred")},indent=2))


if __name__ == "__main__": main()
