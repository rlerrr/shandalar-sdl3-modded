"""Convoke creature counts and Chief Engineer artifact payment."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "convoke_held_additions"
PROFILE = Path(__file__).with_name("data") / "convoke_held_additions_wave8_profiles.json"
SERAPH, CHIEF = 14169, 14084
RAW_MODES = {SERAPH: ("dynamic_flying", "wrong_color"), CHIEF: ("legacy_transport", "ordinary_cast")}
NEW_MODES = {SERAPH: ("paid_entry", "countered"), CHIEF: ("normal_payment", "grant_lost")}
COSTS = {SERAPH: "{5}{W}{W}", CHIEF: "{1}{U}"}
ZONES = {"hand": 0, "battlefield": 1, "graveyard": 3}
ZONE_NAMES = {index: name for name, index in ZONES.items()}
ZERO, SICK, TAPPED = [0] * 6, 2, 1
SEED = 2688
PREFIX = "ConvokeHeldAdditionsRecipe"
CAD, BEAR, THOPTER = "Eager Cadet", "Grizzly Bears", "Ornithopter"
CRYPT, RING, KILL = "Tormod's Crypt", "Sol Ring", "Murder"
BLUE, COUNTER = "Flying Men", "Counterspell"
WITNESSES = {CAD, BEAR, THOPTER, CRYPT, RING, KILL, BLUE, COUNTER}
GAPS = {CHIEF: [
    "Chief's simulated activation is a legacy implementation control. An opposing reply during its "
    "synchronous nested artifact cast needs a separate priority-menu calibration."
]}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in COSTS or record.get("layout") != "normal" or record.get("selected_face")
            or record.get("card_faces") or record.get("mana_cost") != COSTS[csvid]):
        raise ValueError("whole pinned Convoke record required")
    return dict(csvid=csvid, printed_mana=cost_from_oracle(record), rules=record["oracle_text"])


def variants(shape):
    csvid = shape["csvid"]
    return [f"{mode}_{seat}" for mode in RAW_MODES[csvid] + NEW_MODES[csvid] for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError("distinct supported held Convoke scenarios required")


def spec(declaration, record):
    source = profiles()["sources"].get(record.get("name"))
    if not source or not exact(source["oracle"], record):
        raise ValueError("exact pinned Convoke primary required")
    shape = classify(record, source["registry"]["csvid"])
    if (declaration.get("family") != FAMILY or type(declaration.get("version")) is not int
            or declaration["version"] != 2 or declaration.get("oracle_text") != record["oracle_text"]
            or not exact(declaration.get("params"), shape)):
        raise ValueError("versioned whole Convoke declaration required")
    validate_variants(shape, declaration.get("variants"))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {"sources", "witnesses"} or set(pins["witnesses"]) != WITNESSES:
        raise ValueError("independent exact Convoke witnesses required")
    if {pin["registry"]["csvid"] for pin in pins["sources"].values()} != set(COSTS):
        raise ValueError("exact held Convoke primaries required")
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin["oracle"]) or not exact(row, pin["registry"])
                    or row.get("kind") != "card" or oracle[name].get("layout") != "normal"):
                raise ValueError("changed Convoke source or witness: " + name)
            if group == "sources":
                if not str(row.get("handler", "")).startswith("manalinkeh_card_"):
                    raise ValueError("implemented Convoke primary required")
                classify(oracle[name], row["csvid"])
    if (oracle[COUNTER]["oracle_text"] != "Counter target spell."
            or oracle[KILL]["oracle_text"] != "Destroy target creature."):
        raise ValueError("exact Counterspell and Murder witnesses required")


class Board:
    def __init__(self, card, mode, owner, rows, oracle):
        self.card, self.mode, self.owner, self.foe = card, mode, owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.life = [20, 20]
        self.add("source", card, owner, "hand")

    def add(self, ref, name, seat, zone="battlefield", flags=0):
        setup = card(name, ref, seat, zone)
        states = [label for bit, label in ((TAPPED, "tapped"), (SICK, "sick")) if flags & bit]
        if states:
            setup["state"] = states
        self.setup.append(setup)
        self.objects[ref] = dict(name=name, seat=seat, zone=zone)

    def emit(self, op, ref="", other="", prop="", *, player=0, value=0, zone=0, index=0, color=0):
        if op == "untap_step":
            self.steps.append(step(op, player=player))
            return
        arguments = dict(ref=ref, other=other, property=prop, player=player, value=value, index=index, color=color)
        fields = {key: item for key, item in arguments.items() if item or key == "value"}
        if op in {"set_mana", "assert_mana"}:
            fields.update(player=player, color=color)
        if op == "assert_zone_multiplicity":
            fields.update(player=player, zone=ZONE_NAMES[zone])
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit("assert_card", ref, prop=prop, value=value)

    def mana(self, op, seat, amounts):
        for color, amount in enumerate(amounts):
            self.emit(op, player=seat, value=amount, color=color)

    def fund(self, seat, amounts):
        self.mana("set_mana", seat, amounts)
        self.emit("refresh")

    def cast(self, ref, success=True):
        self.emit("cast", ref, value=int(success))

    def resolve(self):
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")

    def receipts(self):
        for seat in (0, 1):
            self.emit("assert_life", player=seat, value=self.life[seat])
            cards = [item for item in self.objects.values() if item["seat"] == seat]
            counts = Counter((item["name"], item["zone"]) for item in cards)
            for name in sorted({item["name"] for item in cards}):
                for zone, code in ZONES.items():
                    self.emit("assert_zone_multiplicity", player=seat, zone=code,
                              index=self.rows[name]["csvid"], value=counts[name, zone])

    def measure_pt(self, value):
        self.observe("source", "in_play", 1)
        self.observe("source", "power", value)
        self.observe("source", "toughness", value)

    def seraph(self):
        self.add("white_helper", CAD, self.owner, flags=SICK)
        self.add("generic_helper", THOPTER, self.owner, flags=SICK)
        self.add("tapped_creature", BEAR, self.owner, flags=TAPPED)
        self.add("own_noncreature", CRYPT, self.owner)
        self.add("ground_blocker", BEAR, self.foe)
        self.add("foe_white", CAD, self.foe)
        if self.mode == "dynamic_flying":
            self.add("removal", KILL, self.foe, "hand")

        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        pool = [4, 0, 0, 0, 0, 1] if self.mode == "dynamic_flying" else [6, 0, 0, 0, 0, 0]
        self.fund(self.owner, pool)
        self.observe("white_helper", "sick", 1)
        self.observe("generic_helper", "sick", 1)

        if self.mode == "wrong_color":
            self.cast("source", success=False)
            self.emit("assert_choices_consumed")
            self.emit("assert_stack")
            self.mana("assert_mana", self.owner, pool)
            for ref in ("white_helper", "generic_helper", "foe_white"):
                self.observe(ref, "tapped", 0)
            self.receipts()
            return

        if self.owner == 0:
            self.emit("choose_target", "white_helper", player=self.owner)
            self.emit("choose_target", "generic_helper", player=self.owner)
        self.cast("source")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner, ZERO)
        for ref in ("white_helper", "generic_helper", "tapped_creature"):
            self.observe(ref, "tapped", 1)
        for ref in ("own_noncreature", "ground_blocker", "foe_white"):
            self.observe(ref, "tapped", 0)
        self.resolve()
        self.objects["source"]["zone"] = "battlefield"
        self.measure_pt(4)
        self.receipts()

        self.fund(self.foe, cost_from_oracle(self.oracle[KILL]))
        self.emit("choose_target", "white_helper", player=self.foe)
        self.cast("removal")
        self.mana("assert_mana", self.foe, ZERO)
        self.resolve()
        for ref in ("removal", "white_helper"):
            self.objects[ref]["zone"] = "graveyard"
        self.measure_pt(3)

        for seat in (self.foe, self.owner):
            self.emit("advance_turn")
            self.emit("untap_step", player=seat)
            self.emit("turn", player=seat)
        self.observe("source", "sick", 0)
        self.emit("begin_attackers")
        self.emit("attack", "source", value=1)
        self.emit("finish_attackers")
        self.emit("begin_blockers")
        self.emit("block", "ground_blocker", "source")
        self.observe("ground_blocker", "blocking_count", 0)
        self.emit("advance_combat")
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")
        self.life[self.foe] -= 3
        self.measure_pt(3)
        self.receipts()

    def chief(self):
        self.add("artifact_spell", RING, self.owner, "hand")
        self.add("nonartifact_spell", BLUE, self.owner, "hand")
        self.add("opposing_artifact", RING, self.foe, "hand")
        self.add("opposing_creature", CAD, self.foe)
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, cost_from_oracle(self.oracle[self.card]))
        self.cast("source")
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner, ZERO)
        self.resolve()
        self.objects["source"]["zone"] = "battlefield"
        self.observe("source", "sick", 1)
        self.observe("source", "tapped", 0)
        self.observe("source", "power", 1)
        self.observe("source", "toughness", 3)

        self.emit("turn", player=self.foe)
        self.cast("opposing_artifact", success=False)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        self.observe("opposing_creature", "tapped", 0)
        self.emit("turn", player=self.owner)
        self.cast("nonartifact_spell", success=False)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        self.observe("source", "tapped", 0)

        if self.mode == "legacy_transport":
            self.emit("require_can_activate", "source", value=1)
            self.emit("choose_zone_card", player=self.owner, value=self.rows[RING]["csvid"])
        if self.owner == 0:
            self.emit("choose_target", "source", player=self.owner)
        if self.mode == "legacy_transport":
            self.emit("activate", "source", value=1)
            self.emit("assert_choices_consumed")
        else:
            self.cast("artifact_spell")
            # Missing announcement or an unused legitimate payment intent are
            # ordinary observations. The latter clears the exact input scope.
            self.emit("assert_unused_choices")
            self.emit("assert_stack", value=1)
        self.observe("source", "tapped", 1)
        self.mana("assert_mana", self.owner, ZERO)
        self.resolve()
        self.objects["artifact_spell"]["zone"] = "battlefield"
        self.observe("artifact_spell", "in_play", 1)
        self.observe("opposing_creature", "tapped", 0)
        self.receipts()

    def seraph_addition(self):
        countered = self.mode == "countered"
        self.add("own_noncreature", CRYPT, self.owner)
        self.add("foe_creature", CAD, self.foe)
        if countered:
            self.add("white_helper", CAD, self.owner, flags=SICK)
            self.add("generic_helper", THOPTER, self.owner, flags=SICK)
            self.add("counterspell", COUNTER, self.foe, "hand")
        else:
            self.add("new_creature", BEAR, self.owner, "hand")

        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        pool = [4, 0, 0, 0, 0, 1] if countered else cost_from_oracle(self.oracle[self.card])
        self.fund(self.owner, pool)
        if countered and self.owner == 0:
            self.emit("choose_target", "white_helper", player=self.owner)
            self.emit("choose_target", "generic_helper", player=self.owner)
        self.cast("source")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner, ZERO)
        if countered:
            self.observe("white_helper", "tapped", 1)
            self.observe("generic_helper", "tapped", 1)
            self.emit("announce_spell", "source")
            self.fund(self.foe, cost_from_oracle(self.oracle[COUNTER]))
            self.emit("cast_response", "counterspell", value=1)
            self.emit("assert_stack", value=2)
            self.emit("resolve_response")
            self.emit("assert_choices_consumed")
            self.emit("assert_stack")
            self.mana("assert_mana", self.foe, ZERO)
            self.observe("white_helper", "tapped", 1)
            self.observe("generic_helper", "tapped", 1)
            for ref in ("source", "counterspell"):
                self.objects[ref]["zone"] = "graveyard"
        else:
            self.resolve()
            self.objects["source"]["zone"] = "battlefield"
            self.measure_pt(1)
            self.fund(self.owner, cost_from_oracle(self.oracle[BEAR]))
            self.cast("new_creature")
            self.mana("assert_mana", self.owner, ZERO)
            self.resolve()
            self.objects["new_creature"]["zone"] = "battlefield"
            self.measure_pt(2)
        self.observe("own_noncreature", "tapped", 0)
        self.observe("foe_creature", "tapped", 0)
        self.receipts()

    def chief_addition(self):
        grant_lost = self.mode == "grant_lost"
        self.add("artifact_spell", RING, self.owner, "hand")
        self.add("other_payer", THOPTER, self.owner, flags=SICK)
        if grant_lost:
            self.add("removal", KILL, self.foe, "hand")
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, cost_from_oracle(self.oracle[self.card]))
        self.cast("source")
        self.mana("assert_mana", self.owner, ZERO)
        self.resolve()
        self.objects["source"]["zone"] = "battlefield"
        self.observe("source", "in_play", 1)
        if grant_lost:
            self.fund(self.foe, cost_from_oracle(self.oracle[KILL]))
            self.emit("choose_target", "source", player=self.foe)
            self.cast("removal")
            self.mana("assert_mana", self.foe, ZERO)
            self.resolve()
            for ref in ("source", "removal"):
                self.objects[ref]["zone"] = "graveyard"
            self.receipts()
            self.cast("artifact_spell", success=False)
            self.emit("assert_choices_consumed")
            self.emit("assert_stack")
        self.fund(self.owner, cost_from_oracle(self.oracle[RING]))
        self.cast("artifact_spell")
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner, ZERO)
        self.resolve()
        self.objects["artifact_spell"]["zone"] = "battlefield"
        self.observe("artifact_spell", "in_play", 1)
        self.observe("other_payer", "tapped", 0)
        if not grant_lost:
            self.observe("source", "tapped", 0)
        self.receipts()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()["sources"][name]["registry"]["csvid"] != csvid:
        raise ValueError("exact registered Convoke primary CSVID required")
    output = []
    for variant in selected:
        mode, seat = variant.rsplit("_", 1)
        board = Board(name, mode, int(seat), rows, oracle)
        if csvid == SERAPH:
            action = board.seraph if mode in RAW_MODES[csvid] else board.seraph_addition
        else:
            action = board.chief if mode in RAW_MODES[csvid] else board.chief_addition
        action()
        output.append(dict(
            name=f"{PREFIX}_{csvid}_{variant}", card=name, fixture="empty_main", seed=SEED,
            setup=board.setup, steps=board.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration["params"],
                         oracle_text=declaration["oracle_text"], untested_components=GAPS.get(csvid, [])),
        ))
    return output
