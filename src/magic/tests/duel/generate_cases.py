"""Validate semantic fixtures and compile declarative Oracle-backed test data."""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
from import_duel_oracle import registry
from case_files import load_cases

ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
STATES = {"tapped": 1, "sick": 2, "attacking": 4, "untapped": 8}
OPS = {"choose_target", "choose_damage_target", "choose_damage_player", "choose_cancel_target", "choose_player", "choose_zone", "choose_dialog", "choose_number", "choose_color", "choose_priority", "choose_trigger", "can_activate",
       "activate", "can_cast", "cast", "resolve", "cleanup", "remove_from_combat", "destroy", "attack", "block", "combat_damage", "end_combat",
       "turn", "begin_attackers", "finish_attackers", "begin_blockers", "advance_combat", "assert_card", "assert_zone", "assert_zone_card", "assert_zone_contains", "assert_zone_state",
       "assert_hand_graveyard_total", "assert_offered_count", "assert_offered_card", "record_state", "assert_recorded_state", "bind_permanent",
       "set_mana", "assert_mana", "assert_mana_at_most", "assert_mana_total", "assert_life", "assert_stack", "assert_targets_consumed", "assert_choices_consumed", "capture", "restore", "ai_roundtrip", "ai_turns"}
PROPERTIES = {"csvid", "power", "toughness", "tapped", "sick", "attacking", "in_play", "in_hand", "damage", "blocking_count", "blocked"}
REF_OPS = {"choose_target", "choose_damage_target", "choose_trigger", "can_activate", "activate", "can_cast", "cast", "remove_from_combat", "destroy", "attack", "block", "assert_card"}

def read(path):
    value = json.loads(path.read_text(encoding="utf-8"))
    if value.get("version") != 1:
        raise ValueError(f"Unsupported schema version: {path}")
    return value

def quoted(value):
    return json.dumps(value, ensure_ascii=True)

def build(data, output):
    rows = registry()
    by_name = {r["name"]: r for r in rows if r["kind"] != "internal"}
    oracle = read(data / "oracle.json")
    aliases = read(data / "oracle_aliases.json")["aliases"]
    fixtures = read(data / "fixtures.json")["fixtures"]
    if (data / "cases.json").exists():
        raise ValueError("Move the legacy cases.json into individual files under data/cases/")
    cases = load_cases(data / "cases", by_name, oracle["cards"], aliases)
    if any("_recipe" in c for c in cases):
        from recipes import validate_context
        validate_context(oracle["cards"], by_name, fixtures)
    names = set()
    registered_csvids = {r["csvid"] for r in rows}
    chunks = ["// Generated from reviewed semantic fixtures and independent Oracle data.\n"]

    def setup(array_name, cards):
        refs = set()
        values = []
        for c in cards:
            if c["ref"] in refs or c["name"] not in by_name or c["player"] not in (0, 1):
                raise ValueError(f"Invalid fixture card: {c}")
            refs.add(c["ref"])
            flags = sum(STATES[x] for x in c.get("state", []))
            values.append("{" + ",".join([quoted(c["ref"]), str(c["player"]), str(ZONES[c["zone"]]),
                                        str(by_name[c["name"]]["csvid"]), str(flags)]) + "}")
        chunks.append(f"static const Setup {array_name}[] = {{" + ",".join(values or ['{"",0,0,0,0}']) + "};\n")
        return refs, len(cards)

    fixtures_c = []
    for i, (name, f) in enumerate(fixtures.items()):
        _, count = setup(f"fixture_cards_{i}", f["cards"])
        mana = f.get("mana", [[0]*6]*2)
        if len(mana) != 2 or any(len(p) != 6 or any(v < 0 for v in p) for p in mana):
            raise ValueError(f"Invalid mana pools: {name}")
        fixtures_c.append("{" + ",".join([quoted(name), f"fixture_cards_{i}", str(count),
                          str(f.get("turn", 0)), str(int(f.get("combat", False))),
                          "{" + ",".join(map(str, f.get("life", [20,20]))) + "}",
                          "{" + ",".join("{" + ",".join(map(str, p)) + "}" for p in mana) + "}"]) + "}")
    chunks.append("static const Fixture fixtures[] = {" + ",".join(fixtures_c) + "};\n")
    manifest = []
    for i, case in enumerate(cases):
        name = case["name"]
        if name in names or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", name):
            raise ValueError(f"Invalid/duplicate case name: {name}")
        names.add(name)
        f = fixtures[case["fixture"]]
        refs, count = setup(f"case_cards_{i}", case.get("setup", []))
        base_refs = {c["ref"] for c in f["cards"]}
        if refs & base_refs:
            raise ValueError(f"Duplicate instance reference in {name}")
        refs |= base_refs
        o = oracle["cards"].get(aliases.get(case["card"], case["card"]))
        if not o:
            raise ValueError(f"Missing Oracle mapping for {case['card']}")
        expectations = o.get("selected_face", o)
        text = expectations.get("oracle_text", "")
        steps_c = []
        recorded_state = False
        for step in case["steps"]:
            op = step["op"]
            if op not in OPS or (op in REF_OPS and step.get("ref") not in refs):
                raise ValueError(f"Invalid action in {name}: {step}")
            if step.get("property", "power") not in PROPERTIES:
                raise ValueError(f"Invalid observation in {name}: {step}")
            if "other" in step and step["other"] not in refs:
                raise ValueError(f"Unknown reference: {step}")
            if op in {"choose_player", "choose_damage_player"} and (step.get("value") not in (0, 1) or step.get("player", -1) not in (-1, 0, 1)):
                raise ValueError(f"Invalid player target in {name}: {step}")
            if op == "choose_damage_target" and step.get("player", -1) not in (-1, 0, 1):
                raise ValueError(f"Invalid damage chooser in {name}: {step}")
            if op == "choose_cancel_target" and step.get("player", -1) not in (-1, 0, 1):
                raise ValueError(f"Invalid target cancellation in {name}: {step}")
            if op == "assert_offered_count" and (not isinstance(step.get("value"), int) or not 0 <= step["value"] <= 512):
                raise ValueError(f"Invalid offered card count in {name}: {step}")
            if op == "assert_offered_card" and (not isinstance(step.get("index"), int) or not 0 <= step["index"] < 512 or step.get("value") not in registered_csvids):
                raise ValueError(f"Invalid offered card observation in {name}: {step}")
            if op == "assert_targets_consumed" and step.get("value", 0) != 0:
                raise ValueError(f"Target plans must consume every planned target in {name}")
            if op == "bind_permanent" and (
                step.get("player") not in (0, 1) or not isinstance(step.get("index", -1), int)
                or step.get("index", -1) < -1 or step.get("value") not in registered_csvids
                or not isinstance(step.get("ref"), str) or not step["ref"]
            ):
                raise ValueError(f"Invalid permanent binding in {name}: {step}")
            if op == "bind_permanent":
                refs.add(step["ref"])
            if op == "assert_mana_total" and (
                step.get("player") not in (0, 1) or not isinstance(step.get("value"), int) or step["value"] < 0
            ):
                raise ValueError(f"Invalid total mana observation in {name}: {step}")
            if op in {"set_mana", "assert_mana_at_most"} and (
                step.get("player") not in (0, 1) or step.get("color") not in range(6)
                or not isinstance(step.get("value"), int) or step["value"] < 0
            ):
                raise ValueError(f"Invalid mana bound in {name}: {step}")
            if op == "assert_hand_graveyard_total" and (
                step.get("player") not in (0, 1) or step.get("index") not in registered_csvids
                or not isinstance(step.get("value"), int) or step["value"] < 0
            ):
                raise ValueError(f"Invalid hand/graveyard count in {name}: {step}")
            if op == "record_state":
                recorded_state = True
            elif op == "assert_recorded_state" and not recorded_state:
                raise ValueError(f"Record a result before comparing it in {name}")
            steps_c.append("{" + ",".join([quoted(op), quoted(step.get("ref", "")), quoted(step.get("other", "")),
              quoted(step.get("property", "")), str(step.get("player", 0)), str(step.get("value", 0)),
              str(ZONES.get(step.get("zone", "hand"), 0)), str(step.get("index", -1 if op == "bind_permanent" else 0)), str(step.get("color", 0))]) + "}")
        chunks.append(f"static const Step case_steps_{i}[] = {{" + ",".join(steps_c) + "};\n")
        chunks.append(f"static const Case case_{i} = {{" + ",".join([quoted(name),quoted(case["card"]),
          quoted(o["oracle_id"]),quoted(text),quoted(case["fixture"]),str(case.get("seed",123)),
          f"case_cards_{i}",str(count),f"case_steps_{i}",str(len(steps_c))]) + "};\n")
        manifest.append({"filter": f"Behavior.{name}", "card": case["card"], "csvid": by_name[case["card"]]["csvid"],
                         "oracle_id": o["oracle_id"], "fixture":case["fixture"], "seed":case.get("seed",123), "tier":"behavior",
                         "source_file":case["_source_file"]})
        if "_recipe" in case:
            manifest[-1]["recipe"] = case["_recipe"]
    chunks.append("static const Case *cases[] = {" + ",".join(f"&case_{i}" for i in range(len(cases))) + "};\n")
    # Deliberately narrow templates: numeric vanilla creatures and flying-only creatures.
    templates = []
    seen = set()
    for r in rows:
        o = oracle["cards"].get(aliases.get(r["name"], r["name"]))
        if not o or r["csvid"] in seen:
            continue
        seen.add(r["csvid"])
        face = o.get("selected_face", o)
        text = face.get("oracle_text", "")
        if "Creature" in face.get("type_line", "") and text in ("", "Flying", "Flying, vigilance"):
            if not str(face.get("power", "")).isdigit() or not str(face.get("toughness", "")).isdigit():
                continue
            templates.append("{" + ",".join([quoted(r["name"]),str(r["csvid"]),str(face["power"]),str(face["toughness"]),quoted(o["oracle_id"])]) + "}")
            manifest.append({"filter":f"Templates.Card_{r['csvid']}","card":r["name"],"csvid":r["csvid"],"oracle_id":o["oracle_id"],"fixture":"empty_main","seed":123,"tier":"template"})
    chunks.append("static const Template templates[] = {" + ",".join(templates) + "};\n")
    smoke = []
    seen = set()
    for r in rows:
        if r["kind"] != "card" or r["csvid"] < 0 or r["csvid"] in seen:
            continue
        seen.add(r["csvid"])
        smoke.append("{" + ",".join([quoted(r["name"]),str(r["csvid"])]) + "}")
        manifest.append({"filter":f"Smoke.Card_{r['csvid']}","card":r["name"],"csvid":r["csvid"],"fixture":"stocked_zones","seed":123,"tier":"smoke"})
    chunks.append("static const Smoke smoke_cards[] = {" + ",".join(smoke) + "};\n")
    for seed in (1,123,999):
        manifest.append({"filter":f"AI.Seed_{seed}","card":"Seeded engine duel","csvid":-1,"fixture":"stocked_zones","seed":seed,"tier":"ai"})
    output.mkdir(parents=True, exist_ok=True)
    (output / "duel_cases.inc").write_text("".join(chunks), encoding="utf-8")
    (output / "manifest.json").write_text(json.dumps({"version":1,"source":oracle["source"],"cases":manifest},indent=2)+"\n")
    inventory = {"version":1,"records":rows,"oracle_mapped_names":len(oracle["cards"]),
                 "behavior_cases":len(cases),"template_cases":len(templates),"smoke_cards":len(smoke),
                 "oracle_sha256":hashlib.sha256((data / "oracle.json").read_bytes()).hexdigest()}
    inventory["recipe_cases"] = sum("_recipe" in c for c in cases)
    (output / "inventory.json").write_text(json.dumps(inventory,indent=2)+"\n")
    # Configure-time CTest inventory: every case gets its own failure-contained process.
    cmake = []
    for item in manifest:
        tier = item["tier"]
        if tier == "smoke":
            cmake.append("if(SHANDALAR_DUEL_EXTENDED_TESTS)\n")
        cmake.append('add_test(NAME "duel.'+item["filter"]+'" COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_cases.py" --binary "$<TARGET_FILE:duel_card_tests>" ${duel_node_args} --manifest "${duel_generated}/manifest.json" --baseline "${CMAKE_CURRENT_SOURCE_DIR}/data/known_failures.json" --filter "'+item["filter"]+'" --output "${CMAKE_CURRENT_BINARY_DIR}/results")\n')
        cmake.append('set_tests_properties("duel.'+item["filter"]+'" PROPERTIES LABELS "duel;'+tier+'" TIMEOUT 45)\n')
        if "recipe" in item:
            cmake.append('set_property(TEST "duel.'+item["filter"]+'" APPEND PROPERTY LABELS "recipe;recipe_'+item["recipe"]["family"]+'")\n')
        if tier == "smoke":
            cmake.append("endif()\n")
    (output / "cases.cmake").write_text("".join(cmake))
    print(f"Generated {len(cases)} behavior, {len(templates)} template, {len(smoke)} catalog smoke cases")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=Path(__file__).parent / "data")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.data, args.output)
