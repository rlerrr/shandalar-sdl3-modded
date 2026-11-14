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
from import_carddata import load_handler_overrides
from case_files import CaseStream, select_case_files
from case_archive import CaseArchiveWriter, legacy_case_hash, write_text_if_changed

ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
STATES = {"tapped": 1, "sick": 2, "attacking": 4, "untapped": 8, "opponent_owned": 16, "token": 32}
OPS = {"choose_target", "choose_damage_target", "choose_damage_player", "choose_cancel_target", "choose_cancel_mana", "choose_player", "choose_zone", "choose_dialog", "choose_number", "choose_color", "choose_mana", "choose_priority", "choose_regeneration", "choose_trigger", "can_activate",
       "activate", "can_cast", "cast", "resolve", "cleanup", "remove_from_combat", "destroy", "attack", "block", "combat_damage", "end_combat",
       "turn", "advance_turn", "begin_attackers", "finish_attackers", "begin_blockers", "advance_combat", "assert_card", "require_card", "assert_zone", "assert_zone_card", "assert_zone_contains", "assert_zone_state",
       "assert_hand_graveyard_total", "assert_offered_count", "assert_offered_card", "record_state", "assert_recorded_state", "bind_permanent", "bind_hand",
       "assert_card_name", "assert_payment_source", "set_mana", "set_life", "set_budget", "assert_mana", "assert_mana_at_most", "assert_mana_total", "assert_life", "assert_energy", "assert_poison", "assert_stack", "assert_targets_consumed", "assert_choices_consumed", "capture", "restore", "ai_roundtrip", "ai_turns"}
PROPERTIES = {"csvid", "power", "toughness", "tapped", "sick", "attacking", "in_play", "in_hand", "damage", "blocking_count", "blocked", "plus1_counters", "minus1_counters"}
OPS |= {"refresh", "choose_zone_card", "choose_payment", "can_activate_hand", "require_can_activate_hand", "activate_hand", "require_can_activate", "can_target_creature"}
OPS.add("probe_activation_target")
OPS.add("choose_x")
OPS.add("assert_zone_multiplicity")
OPS |= {"assert_zone_multiplicity_max", "discard_policy", "assert_discard_choices", "present_ai_dialogs"}
OPS |= {"choose_optional_zone", "assert_forced_zone_choices"}
OPS |= {"set_card_available", "assert_card_available"}
OPS |= {"assert_matching_permanents", "bind_permanent_copy", "bind_hand_copy", "require_can_cast", "assert_unused_choices"}
OPS |= {"record_library", "assert_scry_result"}
OPS |= {"observe_search_filters", "assert_offered_legal_card"}
OPS.add("assert_offered_selection_required")
OPS |= {"cast_response", "resolve_response", "reset_death_history"}
OPS.add("announce_spell")
OPS |= {"can_activate_graveyard", "require_can_activate_graveyard", "activate_graveyard", "prepare_graveyard_abilities"}
OPS.add("untap_step")
OPS.add("upkeep_step")
OPS |= {"can_cast_graveyard", "require_can_cast_graveyard", "cast_graveyard"}
OPS.add("assert_shuffle_count")
OPS.add("assert_counter")
COUNTER_KINDS = {"plus1", "minus1", "charge", "loyalty", "knowledge", "elixir",
                 "divinity", "quest", "blood", "gold", "study", "growth", "ki", "lore", "age", "fade", "time"}

def validate_counter_observation(step):
    if (set(step) != {'op', 'ref', 'counter_kind', 'value'}
            or not isinstance(step.get('ref'), str) or not step['ref']
            or not isinstance(step.get('counter_kind'), str) or step['counter_kind'] not in COUNTER_KINDS
            or type(step.get('value')) is not int or not 0 <= step['value'] <= 2147483647):
        raise ValueError('counter observations require a named permanent, reviewed counter kind and nonnegative count')

TOKEN_PROPERTIES = {"token", "color", "artifact"} | {"subtype_"+n for n in (
    "Angel","Bear","Bird","Centaur","Citizen","Dragon","Drake","Eldrazi","Elf","Goblin",
    "Horror","Human","Insect","Knight","Ally","Minotaur","Myr","Satyr","Servo","Sliver",
    "Soldier","Spirit","Thopter","Warrior","Wurm",
    "Boar","Cat","Spider","Saproling","Phyrexian","Zombie","Construct","Beast","Illusion","Ooze","Squirrel","Plant","Bat","Elemental","Clue",
    "Elephant","Kithkin","Treefolk","Shaman","Snake","Merfolk","Wizard","Golem","Rogue","Scion","Spawn","Wolf","Ape","Frog","Lizard")}
REF_OPS = {"choose_regeneration", "choose_target", "choose_damage_target", "choose_trigger", "can_activate", "activate", "can_cast", "require_can_cast", "cast", "remove_from_combat", "destroy", "attack", "block", "assert_card", "require_card", "assert_card_name"}
REF_OPS |= {"can_activate_hand", "require_can_activate_hand", "activate_hand", "require_can_activate", "can_target_creature"}
REF_OPS.add("probe_activation_target")
REF_OPS.add("cast_response")
REF_OPS.add("announce_spell")
REF_OPS.add("assert_counter")
OPS |= {"choose_trigger_source", "choose_optional_target", "assert_forced_target_choices"}
REF_OPS |= {"choose_trigger_source", "choose_optional_target"}
OPS |= {"choose_activation_mode", "require_activation_mode"}
REF_OPS.add("choose_activation_mode")
OPS |= {"choose_mana_source", "can_activate_granted_mana", "activate_granted_mana"}
REF_OPS |= {"choose_mana_source", "can_activate_granted_mana", "activate_granted_mana"}

def validate_mana_source_choice(step):
    if (set(step) != {'op','ref'} or step.get('op') != 'choose_mana_source'
            or not isinstance(step.get('ref'),str) or not step['ref']):
        raise ValueError('mana-source choices require only a named live source; chooser comes from its reference')

def validate_granted_mana_action(step):
    if (set(step) != {'op','ref','other','value'}
            or step.get('op') not in ('can_activate_granted_mana','activate_granted_mana')
            or not isinstance(step.get('ref'),str) or not step['ref']
            or not isinstance(step.get('other'),str) or not step['other']
            or type(step.get('value')) is not int or step['value'] not in (0,1)):
        raise ValueError('granted mana actions require actual host/provider references and a boolean integer receipt')

def validate_activation_mode_choice(step):
    label = step.get('mode_label')
    if (set(step) != {'op','ref','player','mode_label','fallback_color','allow_auto_nonmana'}
            or not isinstance(step.get('ref'),str) or not step['ref']
            or type(step.get('player')) is not int or step['player'] not in (0,1)
            or not isinstance(label,str) or not label or len(label.encode('utf8'))>127
            or '\n' in label or '\r' in label
            or type(step.get('fallback_color')) is not int or not 0<=step['fallback_color']<=6
            or type(step.get('allow_auto_nonmana')) is not bool):
        raise ValueError('activation mode intent requires an explicit source, chooser, one-line label and reviewed fallback/auto policy')

def validate_activation_mode_observation(step):
    if set(step) != {'op','value'} or type(step.get('value')) is not int or step['value'] not in (0,1):
        raise ValueError('activation mode observation requires only an explicit zero/one receipt')

def validate_named_choice(step):
    if (set(step) != {'op', 'ref', 'player'}
            or not isinstance(step.get('ref'), str) or not step['ref']
            or type(step.get('player')) is not int or step['player'] not in (0, 1)):
        raise ValueError('named trigger/optional target choices require an explicit chooser and reference')

def validate_forced_target_observation(step):
    if (set(step) != {'op', 'value'} or type(step.get('value')) is not int
            or not 0 <= step['value'] <= 2147483647):
        raise ValueError('forced target observations require only an explicit nonnegative count')
REF_OPS |= {"can_activate_graveyard", "require_can_activate_graveyard", "activate_graveyard"}
REF_OPS |= {"can_cast_graveyard", "require_can_cast_graveyard", "cast_graveyard"}

def validate_shuffle_observation(step):
    if (set(step) != {'op', 'player', 'value'}
            or type(step.get('player')) is not int or step['player'] not in (0, 1)
            or type(step.get('value')) is not int or not 0 <= step['value'] <= 2147483647):
        raise ValueError('shuffle observations require only an explicit player and nonnegative count')

def validate_library_observation(step, recorded):
    if type(step.get('player')) is not int or step['player'] not in (0,1):
        raise ValueError('library observations require an explicit player')
    if step['op'] == 'record_library':
        recorded.add(step['player'])
    elif step['player'] not in recorded or type(step.get('value')) is not int or not 0 <= step['value'] <= 20:
        raise ValueError('scry observations require a prior library record and bounded inspected count')

def validate_untap_step(step):
    if type(step.get('player')) is not int or step['player'] not in (0,1) or set(step) != {'op','player'}:
        raise ValueError('untap_step requires only an explicit player zero or one')

def validate_upkeep_step(step):
    if type(step.get('player')) is not int or step['player'] not in (0,1) or set(step) != {'op','player'}:
        raise ValueError('upkeep_step requires only an explicit player zero or one')

def read(path):
    value = json.loads(path.read_text(encoding="utf-8"))
    if value.get("version") != 1:
        raise ValueError(f"Unsupported schema version: {path}")
    return value

def quoted(value):
    return json.dumps(value, ensure_ascii=True)

def template_stats(face):
    # Tokens sharing a name can have different Oracle identities and stats.
    # A registry name alone cannot select a particular Bear/Elf/etc. token.
    if face.get("layout") == "token" or "Token" in face.get("type_line", ""):
        return None
    if "Creature" not in face.get("type_line", "") or face.get("oracle_text", "") not in ("", "Flying", "Flying, vigilance"):
        return None
    if not all(str(face.get(k, "")).isdigit() for k in ("power", "toughness")):
        return None
    return int(face["power"]), int(face["toughness"])


def build(data, output, legacy_fingerprints=False, *, case_files=None, csvids=None, driver_only=False):
    partial = case_files is not None or csvids is not None
    if driver_only and (partial or legacy_fingerprints):
        raise ValueError("driver-only generation cannot select behavior cases or emit behavior fingerprints")
    if driver_only:
        output.mkdir(parents=True, exist_ok=True)
    rows = registry()
    by_name = {r["name"]: r for r in rows if r["kind"] != "internal"}
    oracle = read(data / "oracle.json")
    aliases = read(data / "oracle_aliases.json")["aliases"]
    fixtures = read(data / "fixtures.json")["fixtures"]
    if not driver_only and (data / "cases.json").exists():
        raise ValueError("Move the legacy cases.json into individual files under data/cases/")
    files = select_case_files(data / "cases", case_files=case_files, csvids=csvids) if partial else None
    case_directory = (data / "cases").resolve() if partial else data / "cases"
    cases = () if driver_only else CaseStream(case_directory, by_name, oracle["cards"], aliases, files=files)
    if not driver_only and cases.recipe_count:
        from recipes import validate_context
        validate_context(oracle["cards"], by_name, fixtures)
    names = set()
    registered_csvids = {r["csvid"] for r in rows}
    chunks = ["// Generated from reviewed semantic fixtures and independent Oracle data.\n"]

    def setup(array_name, cards, destination=None):
        refs = set()
        values = []
        for c in cards:
            if c["ref"] in refs or c["name"] not in by_name or c["player"] not in (0, 1):
                raise ValueError(f"Invalid fixture card: {c}")
            refs.add(c["ref"])
            flags = sum(STATES[x] for x in c.get("state", []))
            values.append("{" + ",".join([quoted(c["ref"]), str(c["player"]), str(ZONES[c["zone"]]),
                                        str(by_name[c["name"]]["csvid"]), str(flags)]) + "}")
        (chunks if destination is None else destination).append(f"static const Setup {array_name}[] = {{" + ",".join(values or ['{"",0,0,0,0}']) + "};\n")
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
    archive = None if driver_only else CaseArchiveWriter(output / "duel_cases.bin")
    semantic_hashes = {}
    for i, case in enumerate(cases):
        name = case["name"]
        if name in names or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", name):
            raise ValueError(f"Invalid/duplicate case name: {name}")
        names.add(name)
        f = fixtures[case["fixture"]]
        case_chunks = []
        refs, count = setup(f"case_cards_{i}", case.get("setup", []), case_chunks)
        base_refs = {c["ref"] for c in f["cards"]}
        if refs & base_refs:
            raise ValueError(f"Duplicate instance reference in {name}")
        refs |= base_refs
        o = oracle["cards"].get(aliases.get(case["card"], case["card"]))
        if not o:
            raise ValueError(f"Missing Oracle mapping for {case['card']}")
        expectations = o.get("selected_face", o)
        text = expectations.get("oracle_text", "")
        step_rows = []
        recorded_state = False
        recorded_libraries = set()
        for step in case["steps"]:
            op = step["op"]
            if op == 'choose_mana_source':
                validate_mana_source_choice(step)
            if op in ('can_activate_granted_mana','activate_granted_mana'):
                validate_granted_mana_action(step)
            if op == 'assert_counter':
                validate_counter_observation(step)
            if op in ('choose_trigger_source', 'choose_optional_target'):
                validate_named_choice(step)
            if op == 'assert_forced_target_choices':
                validate_forced_target_observation(step)
            if op == 'choose_activation_mode':
                validate_activation_mode_choice(step)
            if op == 'require_activation_mode':
                validate_activation_mode_observation(step)
            if op == 'assert_shuffle_count':
                validate_shuffle_observation(step)
            if op == 'untap_step':
                validate_untap_step(step)
            if op == 'upkeep_step':
                validate_upkeep_step(step)
            if op == 'present_ai_dialogs' and (type(step.get('value')) is not int or step['value'] not in (0,1)):
                raise ValueError(f'Invalid AI dialog input policy in {name}: {step}')
            if op == 'discard_policy' and (type(step.get('value')) is not int or step['value'] not in (0,1,2)):
                raise ValueError(f'Invalid discard selection policy in {name}: {step}')
            if op == 'assert_discard_choices' and (type(step.get('value')) is not int or step['value']<0):
                raise ValueError(f'Invalid discard selection count in {name}: {step}')
            if op in ('record_library','assert_scry_result'):
                validate_library_observation(step,recorded_libraries)
            if op in ('observe_search_filters','assert_offered_legal_card','assert_offered_selection_required') and (type(step.get('value')) is not int or step['value'] not in (0,1)):
                raise ValueError('search filter observations require explicit boolean integers')
            if op == 'assert_offered_legal_card' and (type(step.get('index')) is not int or step['index'] not in registered_csvids):
                raise ValueError('search filter observations require a registered card identity')
            if op not in OPS or (op in REF_OPS and step.get("ref") not in refs):
                raise ValueError(f"Invalid action in {name}: {step}")
            if step.get("property", "power") not in PROPERTIES | (TOKEN_PROPERTIES if op=="assert_matching_permanents" else set()):
                raise ValueError(f"Invalid observation in {name}: {step}")
            if "other" in step and step["other"] not in refs:
                raise ValueError(f"Unknown reference: {step}")
            if op == "can_target_creature" and step.get("other") not in refs:
                raise ValueError(f"Creature target probe requires a named target: {step}")
            if op == 'probe_activation_target' and (step.get('other') not in refs or type(step.get('value')) is not int or step['value'] not in (0,1) or type(step.get('index')) is not int or not -1<=step['index']<=16):
                raise ValueError(f'Activation target probe requires a named target, boolean result and explicit mode: {step}')
            if op == 'choose_x' and (type(step.get('value')) is not int or not 0<=step['value']<=20 or type(step.get('player')) is not int or step['player'] not in (0,1)):
                raise ValueError('X choices require an explicit controller and integer from zero through twenty')
            if op in {"choose_player", "choose_damage_player"} and (step.get("value") not in (0, 1) or step.get("player", -1) not in (-1, 0, 1)):
                raise ValueError(f"Invalid player target in {name}: {step}")
            if op == "choose_damage_target" and step.get("player", -1) not in (-1, 0, 1):
                raise ValueError(f"Invalid damage chooser in {name}: {step}")
            if op == "choose_cancel_target" and step.get("player", -1) not in (-1, 0, 1):
                raise ValueError(f"Invalid target cancellation in {name}: {step}")
            if op == "choose_cancel_mana" and step.get("player", 0) not in (-1, 0, 1):
                raise ValueError(f"Invalid mana cancellation in {name}: {step}")
            if op == "set_budget" and (type(step.get("value")) is not int or not 200000 <= step["value"] <= 2000000):
                raise ValueError("scripted work budget must be an integer from 200000 to 2000000")
            if op == "assert_offered_count" and (not isinstance(step.get("value"), int) or not 0 <= step["value"] <= 512):
                raise ValueError(f"Invalid offered card count in {name}: {step}")
            if op == "assert_offered_card" and (not isinstance(step.get("index"), int) or not 0 <= step["index"] < 512 or step.get("value") not in registered_csvids):
                raise ValueError(f"Invalid offered card observation in {name}: {step}")
            if op == "choose_optional_zone" and (step.get("player") not in (0, 1) or step.get("value") not in registered_csvids):
                raise ValueError(f"Invalid optional zone fallback in {name}: {step}")
            if op == "assert_forced_zone_choices" and (type(step.get("value")) is not int or step["value"] < 0):
                raise ValueError(f"Invalid forced zone choice count in {name}: {step}")
            if op in {"set_card_available", "assert_card_available"} and (
                type(step.get("index")) is not int or step["index"] not in registered_csvids
                or type(step.get("value")) is not int or step["value"] not in (0, 1)
            ):
                raise ValueError(f"Invalid fixture card availability in {name}: {step}")
            if op in {"assert_zone_multiplicity", "assert_zone_multiplicity_max"} and (
                step.get("player") not in (0, 1) or step.get("zone") not in ZONES
                or step.get("index") not in registered_csvids
                or type(step.get("value")) is not int or step["value"] < 0
            ):
                raise ValueError(f"Invalid zone multiplicity observation in {name}: {step}")
            if op == "assert_matching_permanents" and (
                step.get("player") not in (0,1) or step.get("index") not in registered_csvids
                or type(step.get("value")) is not int or "property" not in step
            ):
                raise ValueError(f"Invalid matching permanent observation in {name}: {step}")
            if op in {"bind_permanent_copy", "bind_hand_copy"} and (
                step.get("player") not in (0,1) or type(step.get("index")) is not int
                or not 0<=step["index"]<16 or step.get("value") not in registered_csvids
                or not isinstance(step.get("ref"),str) or not step["ref"]
            ):
                raise ValueError(f"Invalid card copy binding in {name}: {step}")
            if op in {"bind_permanent_copy", "bind_hand_copy"}:refs.add(step["ref"])
            if op == "assert_targets_consumed" and step.get("value", 0) != 0:
                raise ValueError(f"Target plans must consume every planned target in {name}")
            if op in {"bind_permanent", "bind_hand"} and (
                step.get("player") not in (0, 1) or not isinstance(step.get("index", -1), int)
                or step.get("index", -1) < -1 or step.get("value") not in registered_csvids
                or not isinstance(step.get("ref"), str) or not step["ref"]
            ):
                raise ValueError(f"Invalid permanent binding in {name}: {step}")
            if op in {"bind_permanent", "bind_hand"}:
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
            if op == "choose_mana" and (step.get("color") not in range(7) or step.get("player", 0) not in (0, 1)):
                raise ValueError(f"Invalid mana-pool click in {name}: {step}")
            if op == "record_state":
                recorded_state = True
            elif op == "assert_recorded_state" and not recorded_state:
                raise ValueError(f"Record a result before comparing it in {name}")
            step_rows.append((op,step.get("ref", ""),step.get("other", ""),
                step['mode_label'] if op=='choose_activation_mode' else step.get("counter_kind", "") if op == 'assert_counter' else step.get("property", ""),
                step.get("player",0),int(step['allow_auto_nonmana']) if op=='choose_activation_mode' else step.get("value",0),ZONES.get(step.get("zone","hand"),0),
                step.get("index",-1 if op in {"bind_permanent","bind_hand"} else 0),step['fallback_color'] if op=='choose_activation_mode' else step.get("color",0)))
        metadata=(name,case["card"],o["oracle_id"],text,case["fixture"],case.get("seed",123))
        card_rows=[(c["ref"],c["player"],ZONES[c["zone"]],by_name[c["name"]]["csvid"],sum(STATES[x] for x in c.get("state",[]))) for c in case.get("setup",[])]
        archive.add(metadata,card_rows,step_rows)
        if legacy_fingerprints:
            semantic_hashes[name]=legacy_case_hash(i,metadata,card_rows,step_rows)
        manifest.append({"filter": f"Behavior.{name}", "card": case["card"], "csvid": by_name[case["card"]]["csvid"],
                         "oracle_id": o["oracle_id"], "handler": by_name[case["card"]]["handler"], "fixture":case["fixture"], "seed":case.get("seed",123), "tier":"behavior",
                         "source_file":case["_source_file"]})
        if "_recipe" in case:
            manifest[-1]["recipe"] = case["_recipe"]
    if archive is not None:
        archive.finish()
    if partial:
        selection = {"version":1,"complete":False,"source_files":[path.relative_to(case_directory.parent).as_posix() for path in files]}
        # Subset archives run with an existing driver. They deliberately do not
        # emit build inputs or a partial CTest catalog into its generated tree.
        write_text_if_changed(output / "manifest.json",json.dumps({"version":1,"source":oracle["source"],"selection":selection,"cases":manifest},indent=2)+"\n")
        inventory = {"version":1,"selection":selection,"behavior_cases":len(cases),"recipe_cases":cases.recipe_count,
                     "oracle_sha256":hashlib.sha256((data / "oracle.json").read_bytes()).hexdigest()}
        write_text_if_changed(output / "inventory.json",json.dumps(inventory,indent=2)+"\n")
        if legacy_fingerprints:
            write_text_if_changed(output / "case_semantics_sha256.json",json.dumps(semantic_hashes,indent=2)+"\n")
        print(f"Generated {len(cases)} behavior cases from {len(files)} selected source files")
        return
    write_text_if_changed(output / "duel_case_sources.cmake", "set(duel_generated_case_sources)\n")
    if legacy_fingerprints:
        write_text_if_changed(output / "case_semantics_sha256.json",json.dumps(semantic_hashes,indent=2)+"\n")
    # Deliberately narrow templates: numeric vanilla creatures and flying-only creatures.
    templates = []
    seen = set()
    for r in rows:
        if r["kind"] != "card":
            continue
        o = oracle["cards"].get(aliases.get(r["name"], r["name"]))
        if not o or r["csvid"] in seen:
            continue
        seen.add(r["csvid"])
        face = o.get("selected_face", o)
        if template_stats(face) is not None:
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
    # Wiring checks are separate from Oracle behavior coverage.
    bindings = load_handler_overrides(ROOT / "tools/carddata_handler_overrides.json")
    declarations, checks, binding_rows = [], [], []
    for csvid, record in bindings.items():
        declarations.append(f"extern int {record['handler']}(int, int, event_t);\n")
        checks.append(f"case {csvid}: return actual == {record['handler']};\n")
        binding_rows.append("{" + quoted(record["name"]) + "," + str(csvid) + "}")
        manifest.append({"filter": f"Bindings.Card_{csvid}", "card": record["name"], "csvid": csvid,
                         "fixture": "registry", "seed": 123, "tier": "binding"})
    chunks.append("static const Smoke handler_bindings[] = {" + ",".join(binding_rows) + "};\n")
    for seed in (1,123,999):
        manifest.append({"filter":f"AI.Seed_{seed}","card":"Seeded engine duel","csvid":-1,"fixture":"stocked_zones","seed":seed,"tier":"ai"})
    output.mkdir(parents=True, exist_ok=True)
    write_text_if_changed(output / "duel_handler_bindings.inc","".join(declarations) +
        "int duel_test_handler_matches_override(int csvid) {\n"
        "  for (int iid = 0; iid < SHANDALAR_CARD_DATA_ACTIVE_COUNT; ++iid) {\n"
        "    if ((int)global_cards_data[iid].id != csvid) continue;\n"
        "    int (*actual)(int, int, event_t) = global_cards_data[iid].code_pointer;\n"
        "    switch (csvid) {\n" + "".join(checks) +
        "    default: return 0;\n    }\n  }\n  return 0;\n}\n")
    write_text_if_changed(output / "duel_cases.inc","".join(chunks), encoding="utf-8")
    if driver_only:
        # These are the only inputs compiled into the reusable engine. Behavior
        # archives and their manifests are generated independently by authors.
        print(f"Generated driver inputs: {len(fixtures)} fixtures, {len(templates)} templates, {len(smoke)} smoke cards")
        return
    write_text_if_changed(output / "manifest.json",json.dumps({"version":1,"source":oracle["source"],"cases":manifest},indent=2)+"\n")
    inventory = {"version":1,"records":rows,"oracle_mapped_names":len(oracle["cards"]),
                 "behavior_cases":len(cases),"template_cases":len(templates),"smoke_cards":len(smoke),
                 "oracle_sha256":hashlib.sha256((data / "oracle.json").read_bytes()).hexdigest()}
    inventory["recipe_cases"] = cases.recipe_count
    write_text_if_changed(output / "inventory.json",json.dumps(inventory,indent=2)+"\n")
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
    write_text_if_changed(output / "cases.cmake","".join(cmake))
    print(f"Generated {len(cases)} behavior, {len(templates)} template, {len(smoke)} catalog smoke cases")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=Path(__file__).parent / "data")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--legacy-fingerprints", action="store_true", help="emit migration fingerprints matching the former C++ arrays")
    parser.add_argument("--case-file", action="append", help="only this whole case file, relative to data/cases; repeat to select a union")
    parser.add_argument("--csvid", type=int, action="append", help="only this card's whole case home; repeat to select a union")
    parser.add_argument("--driver-only", action="store_true", help="emit only compiled driver inputs; do not load or expand behavior cases")
    args = parser.parse_args()
    build(args.data, args.output, args.legacy_fingerprints, case_files=args.case_file, csvids=args.csvid, driver_only=args.driver_only)
