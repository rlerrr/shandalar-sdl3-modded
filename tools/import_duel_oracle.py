"""Pin independent Scryfall rules data. Network access is an explicit refresh only.

Accepts JSON arrays and Scryfall's JSONL gzip bulk format. Never reads card handlers
to infer expectations. The engine registry is used only for identity/inventory.
"""
import argparse
import gzip
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "src/magic/tests/duel/data"
FIELDS = ("oracle_id", "name", "oracle_text", "mana_cost", "type_line", "power",
          "toughness", "loyalty", "defense", "keywords", "colors", "color_identity",
          "cmc", "layout", "scryfall_uri", "card_faces")

def registry():
    rows = []
    for line in (ROOT / "src/magic/src/carddata.c").read_text().splitlines():
        match = re.search(r'\{\s*(?:0|255),\s*".*?",\s*\{0\},\s*(-?\d+),.*?,\s*(\w+),\s*0x.*?/\*\s*(\d+):\s*(.*?)\s*\*/', line)
        if match:
            csvid, handler, iid, name = match.groups()
            rows.append({"csvid": int(csvid), "iid": int(iid), "name": name, "handler": handler,
                         "kind": "internal" if name.startswith("#") else "custom" if name.startswith("**") else "card"})
    if not rows:
        raise ValueError("No engine registry records found")
    taxonomy = json.loads((DATA / "registry_kinds.json").read_text(encoding="utf-8"))
    if not isinstance(taxonomy, dict) or taxonomy.get("version") != 1 or not isinstance(taxonomy.get("overrides"), dict):
        raise ValueError("Unsupported reviewed registry classification schema")
    by_id = {r["csvid"]: r for r in rows}
    for key, override in taxonomy["overrides"].items():
        if (not key.isdigit() or int(key) not in by_id or not isinstance(override, dict)
                or set(override) != {"name", "kind", "reason"}
                or override["name"] != by_id[int(key)]["name"]
                or override["kind"] not in ("alternate", "token", "effect")
                or not isinstance(override["reason"], str) or not override["reason"].strip()):
            raise ValueError(f"Invalid reviewed registry classification: {key}")
        by_id[int(key)].update(kind=override["kind"], kind_reason=override["reason"])
    return rows

def read_bulk(path):
    if path.name.endswith(".jsonl.gz"):
        with gzip.open(path, "rt", encoding="utf-8") as source:
            yield from (json.loads(line) for line in source if line.strip())
    else:
        with path.open(encoding="utf-8") as source:
            yield from json.load(source)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bulk", type=Path)
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=DATA / "oracle.json")
    args = parser.parse_args()
    metadata = json.loads(args.metadata.read_text(encoding="utf-8-sig"))
    aliases_path = DATA / "oracle_aliases.json"
    aliases = json.loads(aliases_path.read_text())["aliases"] if aliases_path.exists() else {}
    rows = registry()
    wanted = {aliases.get(r["name"], r["name"]) for r in rows if r["kind"] == "card"}
    cards = {}
    for card in read_bulk(args.bulk):
        names = [card["name"]] + [face["name"] for face in card.get("card_faces", [])]
        for name in names:
            if name in wanted and (name == card["name"] or name not in cards):
                pinned = {field: card[field] for field in FIELDS if field in card}
                if name != card["name"]:
                    pinned["engine_face"] = name
                    face = next(f for f in card["card_faces"] if f["name"] == name)
                    pinned["selected_face"] = {field: face[field] for field in FIELDS if field in face}
                cards[name] = pinned
    result = {"version": 1, "source": {"provider": "Scryfall", "bulk_id": metadata["id"],
              "updated_at": metadata["updated_at"],
              "bulk_sha256": hashlib.sha256(args.bulk.read_bytes()).hexdigest()}, "cards": cards}
    if args.output.exists():
        old = json.loads(args.output.read_text())
        changed = [name for name in sorted(cards) if old.get("cards", {}).get(name) != cards[name]]
        print(f"Changed Oracle entries: {len(changed)}")
        for name in changed:
            print(name)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    missing = sorted(wanted - cards.keys())
    print(f"Pinned {len(cards)} Oracle records; {len(missing)} engine names unmatched")
    (args.output.parent / "oracle_missing.json").write_text(json.dumps({"version": 1, "names": missing}, indent=2) + "\n")

if __name__ == "__main__":
    main()
