"""Conditional regeneration scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "regeneration_conditional"
PROFILE = Path(__file__).with_name("data") / "regeneration_conditional_wave8_profiles.json"
CLERGY, KNIGHT, TROLL, OGRE = 597, 8591, 9610, 7423
DRAGON_BEAR, SEA, RATS, BATS = 14892, 2239, 8214, 12420
MODES = {
    CLERGY: ("automatic", "deny_fee", "decline_fee", "cannot_regenerate"),
    KNIGHT: ("automatic", "deny_fee", "flanking_combat", "cannot_regenerate"),
    TROLL: ("automatic", "pump_ten", "power_short", "cannot_regenerate"),
    OGRE: ("demon_shield", "rival_demon", "demon_unfunded", "cannot_block"),
    DRAGON_BEAR: ("exact_eight", "tapped_power", "under_eight", "wrong_mana"),
    SEA: ("blocked_by_blue", "blocks_blue", "nonblue_blocker", "history_this_turn", "history_expired"),
    RATS: ("empty_hand", "one_in_hand", "spent_last_card", "unfunded"),
    BATS: ("pay_four", "life_short", "five_life", "flying"),
}
PREFIX = "RegenerationConditionalRecipe"
SEED = 2897
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
TAPPED = 1
MURDER, TERMINATE = "Murder", "Terminate"
BEAR, GIANT, WURM, DEMON = "Grizzly Bears", "Hill Giant", "Scaled Wurm", "Demon of Death's Gate"
WITNESSES = {MURDER, TERMINATE, BEAR, GIANT, WURM, DEMON}

SPINED, MERFOLK, CADET, BLUE_WALL, BOOK = (
    "Spined Wurm", "Merfolk of the Pearl Trident", "Eager Cadet", "Wall of Mist", "Spellbook",
)
WITNESSES |= {SPINED, MERFOLK, CADET, BLUE_WALL, BOOK}
ZONE_NAMES = {code: name for name, code in ZONES.items()}
FOLLOWERS = (DRAGON_BEAR, SEA, RATS, BATS)
NIMBUS_GAP = "The legacy opponent fee appears during destruction. The independent preemptive priority activation is unobserved."
WINDOW_GAP = "The legacy paid regeneration action is exercised through actual destruction. Creating or expiring a preemptive shield is unobserved."
GAPS = {
    CLERGY: [NIMBUS_GAP], KNIGHT: [NIMBUS_GAP], OGRE: [WINDOW_GAP],
    DRAGON_BEAR: [WINDOW_GAP], SEA: [WINDOW_GAP], RATS: [WINDOW_GAP], BATS: [WINDOW_GAP],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get("layout") != "normal" or record.get("selected_face")
            or record.get("card_faces") or "Regenerate" not in record.get("keywords", ())):
        raise ValueError("whole pinned conditional regeneration record required")
    return dict(csvid=csvid, printed_mana=cost_from_oracle(record), rules=record["oracle_text"])


def variants(shape):
    return [f"{mode}_{seat}" for mode in MODES[shape["csvid"]]
            for seat in ((0,) if mode == "decline_reveal" else (0, 1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError("distinct supported conditional regeneration scenarios required")


def spec(declaration, record):
    source = profiles()["sources"].get(record.get("name"))
    if not source or not exact(source["oracle"], record):
        raise ValueError("exact pinned conditional regeneration primary required")
    shape = classify(record, source["registry"]["csvid"])
    if (declaration.get("family") != FAMILY or type(declaration.get("version")) is not int
            or declaration["version"] != 2 or declaration.get("oracle_text") != record["oracle_text"]
            or not exact(declaration.get("params"), shape)):
        raise ValueError("versioned whole conditional regeneration declaration required")
    validate_variants(shape, declaration.get("variants"))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {"sources", "witnesses"} or set(pins["witnesses"]) != WITNESSES:
        raise ValueError("independent exact conditional regeneration witnesses required")
    if {pin["registry"]["csvid"] for pin in pins["sources"].values()} != set(MODES):
        raise ValueError("exact conditional regeneration primaries required")
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin["oracle"]) or not exact(row, pin["registry"])
                    or row.get("kind") != "card"
                    or oracle[name].get("layout") != "normal"):
                raise ValueError("changed conditional regeneration source or witness: " + name)
            if group == "sources":
                if not str(row.get("handler", "")).startswith("manalinkeh_card_"):
                    raise ValueError("implemented conditional regeneration primary required")
                classify(oracle[name], row["csvid"])
    if (oracle[MURDER]["oracle_text"] != "Destroy target creature."
            or oracle[TERMINATE]["oracle_text"] != "Destroy target creature. It can't be regenerated."
            or oracle[DEMON]["type_line"] != "Creature — Demon"
            or (oracle[GIANT]["power"], oracle[WURM]["power"], oracle[SPINED]["power"]) != ("3", "7", "5")
            or (oracle[MERFOLK]["colors"], oracle[CADET]["colors"], oracle[BLUE_WALL]["colors"])
               != (["U"], ["W"], ["U"])
            or (oracle[BLUE_WALL]["power"], oracle[BLUE_WALL]["toughness"]) != ("0", "5")):
        raise ValueError("conditional regeneration witnesses changed")


class Board:
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        self.csvid, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.life = [20, 20]
        self.add("source", name, owner, "hand")

    def add(self, ref, name, seat, zone, *, flags=0):
        fixture = card(name, ref, seat, zone)
        if flags == TAPPED:
            fixture["state"] = ["tapped"]
        else:
            assert flags == 0
        self.setup.append(fixture)
        self.objects[ref] = dict(name=name, player=seat, zone=zone)

    def emit(self, op, ref="", other="", prop="", *, player=0, value=0, zone=0, index=0, color=0):
        fields = dict(ref=ref, other=other, property=prop, player=player, value=value, index=index, color=color)
        values = {key: item for key, item in fields.items() if item or key == "value"}
        if op in {"set_mana", "assert_mana"}:
            values.update(player=player, color=color)
        elif op.startswith("assert_zone"):
            values.update(player=player, zone=ZONE_NAMES[zone])
        self.steps.append(step(op, **values))

    def mana(self, op, seat, values=ZERO):
        for color, value in enumerate(values):
            self.emit(op, player=seat, value=value, color=color)

    def fund(self, seat, values):
        self.mana("set_mana", seat, values)
        self.emit("refresh")

    def cost(self, ref):
        return cost_from_oracle(self.oracle[self.objects[ref]["name"]])

    def move(self, ref, zone):
        obj = self.objects[ref]
        obj["zone"] = zone

    def observe(self, ref, prop, value):
        self.emit("assert_card", ref=ref, prop=prop, value=value)

    def inspect_source(self):
        self.observe("source", "in_play", 1)
        for stat in ("power", "toughness"):
            if stat in self.oracle[self.name]:
                value = int(self.oracle[self.name][stat])
                if self.csvid == RATS and stat == "power":
                    value += int(not any(obj["zone"] == "hand" and obj["player"] == self.owner
                                         for obj in self.objects.values()))
                self.observe("source", stat, value)

    def receipts(self, pools=(ZERO, ZERO)):
        for seat in (0, 1):
            self.emit("assert_life", player=seat, value=self.life[seat])
            self.mana("assert_mana", seat, pools[seat])
            objects = [obj for obj in self.objects.values() if obj["player"] == seat]
            counts = Counter((obj["name"], obj["zone"]) for obj in objects)
            for name in sorted({obj["name"] for obj in objects}):
                for zone, code in ZONES.items():
                    self.emit("assert_zone_multiplicity", player=seat, zone=code,
                              index=self.rows[name]["csvid"], value=counts[name, zone])

    def resolve(self):
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")

    def cast_source(self):
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, self.cost("source"))
        self.emit("cast", ref="source", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner)
        self.resolve()
        self.move("source", "battlefield")
        self.inspect_source()

    def outcomes(self, survives, *, pool=None, tapped=False):
        self.observe("source", "in_play", int(survives))
        if survives:
            self.observe("source", "damage", 0)
            self.observe("source", "tapped", int(tapped))
        else:
            self.move("source", "graveyard")
        self.receipts((pool or ZERO, ZERO) if self.owner == 0 else (ZERO, pool or ZERO))

    def destruction(self, *, fee=0, decline=False, suppressed=False, regenerate=False, blocked_offer=False):
        witness = TERMINATE if suppressed else MURDER
        self.add("destruction", witness, self.foe, "hand")
        self.cast_source()
        owner_pool = [0, int(regenerate or blocked_offer), 0, 0, 0, 0]
        if self.csvid in (CLERGY, KNIGHT) and not fee:
            # Money on the controller must never fund the opponent's fee.
            owner_pool[0] = 1 if self.csvid == CLERGY else 2
        self.fund(self.owner, owner_pool)
        spell_cost = self.cost("destruction")
        fee_pool = [fee, 0, 0, 0, 0, 0]
        self.fund(self.foe, [a+b for a, b in zip(spell_cost, fee_pool)])
        self.emit("choose_target", ref="source", player=self.foe)
        self.emit("cast", ref="destruction", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.foe, fee_pool)
        self.mana("assert_mana", self.owner, owner_pool)
        if fee:
            self.emit("choose_dialog", player=self.foe, value=int(not decline))
        if regenerate or blocked_offer:
            self.emit("choose_regeneration", ref="source")
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_unused_choices", value=int(blocked_offer))
        self.emit("assert_stack")
        self.move("destruction", "graveyard")
        survives = not suppressed and (
            self.csvid == TROLL or (self.csvid in (CLERGY, KNIGHT) and (not fee or decline)) or regenerate
        )
        if regenerate:
            owner_pool = ZERO
        foe_pool = fee_pool if decline else ZERO
        self.observe("source", "in_play", int(survives))
        if survives:
            self.observe("source", "damage", 0)
            self.observe("source", "tapped", 1)
        else:
            self.move("source", "graveyard")
        self.receipts((owner_pool, foe_pool) if self.owner == 0 else (foe_pool, owner_pool))

    def nimbus(self):
        if self.mode == "flanking_combat":
            self.add("blocker", GIANT, self.foe, "battlefield")
            self.cast_source()
            self.emit("advance_turn")
            self.emit("advance_turn")
            self.emit("turn", player=self.owner)
            self.emit("begin_attackers")
            self.emit("attack", ref="source", value=1)
            self.emit("finish_attackers")
            self.emit("begin_blockers")
            self.emit("block", ref="blocker", other="source", value=1)
            self.emit("advance_combat")
            self.observe("blocker", "power", 2)
            self.observe("blocker", "toughness", 2)
            self.emit("combat_damage")
            self.resolve()
            self.observe("source", "attacking", 0)
            self.emit("end_combat")
            self.move("blocker", "graveyard")
            self.outcomes(True, tapped=True)
            return
        optional_fee = self.mode in ("deny_fee", "decline_fee")
        self.destruction(fee=(1 if self.csvid == CLERGY else 2) if optional_fee else 0,
                         decline=self.mode == "decline_fee", suppressed=self.mode == "cannot_regenerate")

    def troll(self):
        if self.mode in ("automatic", "cannot_regenerate"):
            self.destruction(suppressed=self.mode == "cannot_regenerate")
            return
        eligible = self.mode == "pump_ten"
        self.add("seven", WURM, self.owner, "battlefield")
        self.add("three", GIANT, self.owner, "battlefield", flags=0 if eligible else TAPPED)
        self.add("opponent_three", GIANT, self.foe, "battlefield")
        self.cast_source()
        self.observe("seven", "power", 7)
        self.observe("three", "power", 3)
        self.observe("three", "tapped", int(not eligible))
        self.emit("can_activate", ref="source", value=int(eligible))
        if eligible:
            self.emit("choose_target", ref="seven", player=self.owner)
            self.emit("choose_target", ref="three", player=self.owner)
        self.emit("activate", ref="source", value=int(eligible))
        self.emit("assert_stack", value=int(eligible))
        self.emit("assert_unused_choices")
        self.observe("seven", "tapped", int(eligible))
        self.observe("three", "tapped", 1)
        self.observe("opponent_three", "tapped", 0)
        self.observe("source", "tapped", 0)
        self.inspect_source()
        if eligible:
            self.resolve()
            self.observe("source", "power", 25)
            self.observe("source", "toughness", 25)
            self.emit("cleanup")
            self.emit("refresh")
            self.inspect_source()
        self.receipts()

    def ogre(self):
        if self.mode == "cannot_block":
            self.add("attacker", BEAR, self.foe, "battlefield")
            self.cast_source()
            self.emit("advance_turn")
            self.emit("turn", player=self.foe)
            self.emit("begin_attackers")
            self.emit("attack", ref="attacker", value=1)
            self.emit("finish_attackers")
            self.emit("begin_blockers")
            self.emit("block", ref="source", other="attacker", value=0)
            self.emit("advance_combat")
            self.observe("attacker", "blocked", 0)
            self.emit("combat_damage")
            self.resolve()
            self.emit("end_combat")
            self.life[self.owner] -= 2
            self.outcomes(True)
            return
        rival = self.mode == "rival_demon"
        self.add("demon", DEMON, self.foe if rival else self.owner, "battlefield")
        self.destruction(regenerate=self.mode == "demon_shield", blocked_offer=rival)

    def finish_murder(self, mana, *, eligible, life_cost=0):
        self.fund(self.owner, mana)
        self.fund(self.foe, self.cost("follow_murder"))
        self.emit("choose_target", ref="source", player=self.foe)
        self.emit("cast", ref="follow_murder", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.foe)
        self.emit("choose_regeneration", ref="source")
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_unused_choices", value=int(not eligible))
        self.emit("assert_stack")
        self.move("follow_murder", "graveyard")
        if eligible:
            self.life[self.owner] -= life_cost
            self.inspect_source()
        self.outcomes(eligible, pool=ZERO if eligible else mana, tapped=eligible)

    def formidable(self):
        eligible = self.mode in ("exact_eight", "tapped_power")
        self.add("support", GIANT if self.mode == "under_eight" else SPINED, self.owner, "battlefield",
                 flags=TAPPED if self.mode == "tapped_power" else 0)
        self.add("enemy_power", WURM, self.foe, "battlefield")
        self.add("follow_murder", MURDER, self.foe, "hand")
        self.cast_source()
        self.observe("support", "power", 3 if self.mode == "under_eight" else 5)
        self.observe("support", "tapped", int(self.mode == "tapped_power"))
        mana = [1, 0, int(self.mode == "wrong_mana"), int(self.mode != "wrong_mana"), 0, 0]
        self.finish_murder(mana, eligible=eligible)

    def sea_troll(self):
        blocking = self.mode == "blocks_blue"
        history = self.mode in ("history_this_turn", "history_expired")
        blue = self.mode != "nonblue_blocker"
        self.add("opposing_creature", BLUE_WALL if history else MERFOLK if blue else CADET,
                 self.foe, "battlefield")
        if history:
            self.add("follow_murder", MURDER, self.foe, "hand")
        self.cast_source()
        self.emit("advance_turn")
        if not blocking:
            self.emit("advance_turn")
        self.emit("turn", player=self.foe if blocking else self.owner)
        mana = [0, 0, 1, 0, 0, 0]
        self.fund(self.owner, mana)
        attacker, blocker = ("opposing_creature", "source") if blocking else ("source", "opposing_creature")
        self.emit("begin_attackers")
        self.emit("attack", ref=attacker, value=1)
        self.emit("finish_attackers")
        self.emit("begin_blockers")
        self.emit("block", ref=blocker, other=attacker, value=1)
        self.emit("advance_combat")
        if not history:
            self.emit("choose_regeneration", ref="source")
        self.emit("combat_damage")
        self.emit("resolve")
        self.emit("assert_unused_choices", value=int(not blue))
        self.emit("assert_stack")
        self.emit("end_combat")
        if history:
            self.observe("opposing_creature", "in_play", 1)
            self.observe("opposing_creature", "damage", 2)
            self.observe("source", "damage", 0)
            if self.mode == "history_expired":
                self.emit("cleanup")
                self.emit("turn", player=self.foe)
            self.finish_murder(mana, eligible=self.mode == "history_this_turn")
        else:
            self.move("opposing_creature", "graveyard")
            self.outcomes(blue, pool=ZERO if blue else mana, tapped=blue)

    def hellbent(self):
        if self.mode in ("one_in_hand", "spent_last_card"):
            self.add("last_card", BOOK, self.owner, "hand")
        self.add("follow_murder", MURDER, self.foe, "hand")
        self.cast_source()
        if self.mode == "spent_last_card":
            self.emit("cast", ref="last_card", value=1)
            self.emit("assert_stack", value=1)
            self.resolve()
            self.move("last_card", "battlefield")
            self.inspect_source()
        mana = [0, int(self.mode != "unfunded"), 0, 0, 0, 0]
        self.finish_murder(mana, eligible=self.mode in ("empty_hand", "spent_last_card"))

    def marrow_bats(self):
        if self.mode != "flying":
            self.add("follow_murder", MURDER, self.foe, "hand")
            self.cast_source()
            if self.mode != "pay_four":
                self.life[self.owner] = 3 if self.mode == "life_short" else 5
                self.emit("set_life", player=self.owner, value=self.life[self.owner])
            self.finish_murder(ZERO, eligible=self.mode != "life_short", life_cost=4)
            return
        self.add("ground", BEAR, self.foe, "battlefield")
        self.cast_source()
        self.emit("advance_turn")
        self.emit("advance_turn")
        self.emit("turn", player=self.owner)
        self.emit("begin_attackers")
        self.emit("attack", ref="source", value=1)
        self.emit("finish_attackers")
        self.emit("begin_blockers")
        self.emit("block", ref="ground", other="source", value=0)
        self.emit("advance_combat")
        self.observe("source", "blocked", 0)
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")
        self.life[self.foe] -= 4
        self.outcomes(True, tapped=True)

    def run(self):
        scenarios = {
            CLERGY: self.nimbus, KNIGHT: self.nimbus, TROLL: self.troll, OGRE: self.ogre,
            DRAGON_BEAR: self.formidable, SEA: self.sea_troll, RATS: self.hellbent, BATS: self.marrow_bats,
        }
        scenarios[self.csvid]()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()["sources"][name]["registry"]["csvid"] != csvid:
        raise ValueError("exact registered conditional regeneration primary CSVID required")
    output = []
    for variant in selected:
        mode, seat = variant.rsplit("_", 1)
        board = Board(csvid, name, mode, int(seat), rows, oracle)
        board.run()
        output.append(dict(
            name=f"{PREFIX}_{csvid}_{variant}", card=name, fixture="empty_main", seed=SEED,
            setup=board.setup, steps=board.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration["params"],
                         oracle_text=declaration["oracle_text"], untested_components=GAPS.get(csvid, [])),
        ))
    return output
