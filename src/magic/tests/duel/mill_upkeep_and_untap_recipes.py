"""Actual upkeep and untap milling with exact card and turn receipts."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "mill_upkeep_and_untap"
PROFILE = Path(__file__).with_name("data") / "mill_upkeep_and_untap_wave8_profiles.json"
AVEN, PRIEST, GRYFF, THIEF, JIN = 8889, 14448, 14115, 13186, 11623
MODES = {
    AVEN: ("opposing_search", "own_search"),
    PRIEST: ("uncast_exiled", "cast_and_tokens"),
    GRYFF: ("creature_enter", "artifact_enter"),
    THIEF: ("outside_draw", "first_draw"),
    JIN: ("own_end", "source_removed", "unlimited_hand", "hand_limit", "normal_limit_after_removal"),
}
VANDAL, CURSE, GENESIS, SIREN = 5788, 11917, 4840, 13806
DEEP, SPLINTER, SERPENT, PATIENT = 1758, 12086, 3057, 18978
MODES = {VANDAL: ("typed_recurrence", "returned_source"), CURSE: ("opposing_owner", "self_owner"),
         GENESIS: ("alternating_values", "zero_then_draw"), SIREN: ("untap_discard", "manual_empty_hand"),
         DEEP: ("tax_declined", "shroud_trample"), SPLINTER: ("growth_trample", "mixed_prefix"),
         SERPENT: ("seven_and_draw", "short_library"), PATIENT: ("mixed_draw", "short_no_lands")}
PREFIX = "MillUpkeepAndUntapRecipe"
SEED = 2771
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZONE_NAMES = {code: zone for zone, code in ZONES.items()}
ZERO = [0] * 6
HUNGER, BOUNCE, TWIDDLE = "Hunger of the Howlpack", "Unsummon", "Twiddle"
BEAR, GIANT, MINION, MURDER = "Grizzly Bears", "Hill Giant", "Minion", "Murder"
PLAINS, FOREST, ISLAND, SWAMP, MOUNTAIN = "Plains", "Forest", "Island", "Swamp", "Mountain"
WITNESSES = {HUNGER, BOUNCE, TWIDDLE, BEAR, GIANT, MINION, MURDER, PLAINS, FOREST, ISLAND, SWAMP, MOUNTAIN}


BOLT = "Lightning Bolt"
WITNESSES.add(BOLT)
FOLLOWERS = (DEEP, SPLINTER, SERPENT, PATIENT)
GAPS = {
    VANDAL: ["Shred counters are inferred from exact mill progression, a separate +1/+1 counter and new-object control. The named shred observer is unavailable."],
    GENESIS: ["Exact Minion CSVID, token flag, color and 1/1 are observed. The driver has no named Minion subtype observer."],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get("layout") != "normal" or record.get("selected_face")
            or record.get("card_faces") or "Mill" not in record.get("keywords", ())):
        raise ValueError("whole pinned mill record required")
    return dict(csvid=csvid, printed_mana=cost_from_oracle(record), rules=record["oracle_text"])


def variants(shape):
    return [f"{mode}_{seat}" for mode in MODES[shape["csvid"]] for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError("distinct supported mill scenarios required")


def spec(declaration, record):
    source = profiles()["sources"].get(record.get("name"))
    if not source or not exact(source["oracle"], record):
        raise ValueError("exact pinned mill primary required")
    shape = classify(record, source["registry"]["csvid"])
    if (declaration.get("family") != FAMILY or type(declaration.get("version")) is not int
            or declaration["version"] != 2 or declaration.get("oracle_text") != record["oracle_text"]
            or not exact(declaration.get("params"), shape)):
        raise ValueError("versioned whole mill declaration required")
    validate_variants(shape, declaration.get("variants"))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {"sources", "witnesses"} or set(pins["witnesses"]) != WITNESSES:
        raise ValueError("independent exact mill witnesses required")
    if {pin["registry"]["csvid"] for pin in pins["sources"].values()} != set(MODES):
        raise ValueError("exact mill primaries required")
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin["oracle"]) or not exact(row, pin["registry"])
                    or row.get("kind") != "card"
                    or oracle[name].get("layout") != ("token" if name == MINION else "normal")):
                raise ValueError("changed mill source or witness: " + name)
            if group == "sources":
                if not str(row.get("handler", "")).startswith("manalinkeh_card_"):
                    raise ValueError("implemented mill primary required")
                classify(oracle[name], row["csvid"])
    if (oracle[MINION]["type_line"] != "Token Creature — Minion" or oracle[MINION]["colors"] != ["B"]
            or oracle[MURDER]["oracle_text"] != "Destroy target creature."
            or oracle[BEAR]["toughness"] != "2" or oracle[GIANT]["type_line"] != "Creature — Giant"):
        raise ValueError("expected Minion, removal and combat witnesses required")


class Board:
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        self.csvid, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.life = [20, 20]
        self.library = [[], []]
        self.tokens = [0, 0]
        self.add("source", name, owner, "hand")

    def add(self, ref, name, seat, zone):
        self.setup.append(card(name, ref, seat, zone))
        self.objects[ref] = dict(name=name, player=seat, zone=zone)
        if zone == "library":
            self.library[seat].append(ref)

    def emit(self, op, ref="", other="", prop="", *, player=0, value=0, zone=0, index=0, color=0):
        if op in {"untap_step", "upkeep_step", "draw_step"}:
            self.steps.append(step(op, player=player))
            return
        if op == "assert_counter":
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        fields = dict(ref=ref, other=other, property=prop, player=player, value=value, index=index, color=color)
        values = {key: item for key, item in fields.items() if item or key == "value"}
        if op in {"set_mana", "assert_mana"}:
            values.update(player=player, color=color)
        elif op.startswith("assert_zone"):
            values.update(player=player, zone=ZONE_NAMES[zone])
        elif op in {"choose_player", "assert_matching_permanents", "bind_hand"}:
            values["player"] = player
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
        if obj["zone"] == "library":
            self.library[obj["player"]].remove(ref)
        obj["zone"] = zone

    def observe(self, ref, prop, value):
        self.emit("assert_card", ref=ref, prop=prop, value=value)

    def inspect_source(self):
        self.observe("source", "in_play", 1)
        for stat in ("power", "toughness"):
            printed = self.oracle[self.name].get(stat, "")
            if printed.isdecimal():
                self.observe("source", stat, int(printed))

    def receipts(self):
        for seat in (0, 1):
            self.emit("assert_life", player=seat, value=self.life[seat])
            self.mana("assert_mana", seat)
            objects = [obj for obj in self.objects.values() if obj["player"] == seat]
            counts = Counter((obj["name"], obj["zone"]) for obj in objects)
            for name in sorted({obj["name"] for obj in objects}):
                for zone, code in ZONES.items():
                    self.emit("assert_zone_multiplicity", player=seat, zone=code,
                              index=self.rows[name]["csvid"], value=counts[name, zone])

    def deck(self, seat, names):
        for index, name in enumerate(names):
            self.add(f"lib_{seat}_{index}", name, seat, "library")

    def library_prefix(self, seat, amount, destination):
        refs = self.library[seat][:amount]
        for ref in refs:
            self.move(ref, destination)
        return [self.objects[ref]["name"] for ref in refs]

    def observe_library(self, seat):
        self.emit("assert_zone", player=seat, zone=ZONES["library"], value=len(self.library[seat]))
        for index, ref in enumerate(self.library[seat]):
            csvid = self.rows[self.objects[ref]["name"]]["csvid"]
            self.emit("assert_zone_card", player=seat, zone=ZONES["library"], index=index, value=csvid)

    def state(self):
        for seat in (0, 1):
            self.observe_library(seat)
        self.receipts()

    def resolve(self):
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")

    def cast(self, ref, *, target=None, recipient=None, resolution_dialog=None, destination="graveyard"):
        seat = self.objects[ref]["player"]
        self.fund(seat, self.cost(ref))
        if target is not None:
            self.emit("choose_target", ref=target, player=seat)
        if recipient is not None:
            self.emit("choose_player", player=seat, value=recipient)
        self.emit("cast", ref=ref, value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", seat)
        if resolution_dialog is not None:
            self.emit("choose_dialog", player=seat, value=resolution_dialog)
        self.resolve()
        self.move(ref, destination)

    def cast_source(self, recipient=None):
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.cast("source", recipient=recipient, destination="battlefield")
        self.inspect_source()

    def untap(self, seat):
        self.emit("advance_turn")
        self.emit("untap_step", player=seat)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")

    def upkeep(self, seat):
        self.untap(seat)
        self.emit("upkeep_step", player=seat)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")

    def draw(self, seat):
        self.emit("draw_step", player=seat)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        self.library_prefix(seat, 1, "hand")

    def vandal(self):
        returned = self.mode == "returned_source"
        self.deck(self.owner, (MOUNTAIN, ISLAND, SWAMP, PLAINS, FOREST, BEAR))
        self.deck(self.foe, (FOREST, MOUNTAIN))
        if returned:
            self.add("bounce", BOUNCE, self.foe, "hand")
        else:
            self.add("other_counter", HUNGER, self.owner, "hand")
        self.cast_source()
        if not returned:
            self.cast("other_counter", target="source")
            self.emit("assert_counter", ref="source", prop="plus1", value=1)
        self.state()
        self.upkeep(self.foe)
        self.state()
        self.upkeep(self.owner)
        self.library_prefix(self.owner, 1, "graveyard")
        self.state()
        if returned:
            self.cast("bounce", target="source")
            self.move("source", "hand")
            self.emit("bind_hand", ref="source", player=self.owner, value=self.csvid, index=-1)
            self.emit("turn", player=self.owner)
            self.cast("source", destination="battlefield")
            self.inspect_source()
        else:
            self.draw(self.owner)
        self.state()
        self.upkeep(self.foe)
        self.state()
        self.upkeep(self.owner)
        self.library_prefix(self.owner, 1 if returned else 2, "graveyard")
        if not returned:
            self.emit("assert_counter", ref="source", prop="plus1", value=1)
        self.state()

    def curse(self):
        own = self.mode == "self_owner"
        enchanted = self.owner if own else self.foe
        self.deck(self.owner, (PLAINS,) if own else (ISLAND, BEAR, SWAMP))
        self.deck(self.foe, (MOUNTAIN, GIANT, FOREST, ISLAND))
        self.cast_source(recipient=enchanted)
        self.state()
        for seat in (self.foe, self.owner):
            self.upkeep(seat)
            if seat == enchanted:
                self.library_prefix(seat, 2, "graveyard")
            self.observe("source", "in_play", 1)
            self.state()
            if not own and seat == enchanted:
                self.draw(seat)
                self.state()

    def observe_minions(self):
        csvid = self.rows[MINION]["csvid"]
        for seat, count in enumerate(self.tokens):
            self.emit("assert_zone_multiplicity", player=seat, zone=ZONES["battlefield"], index=csvid, value=count)
            if count:
                for prop, expected in (("token", 1), ("color", 1 << 1), ("power", 1), ("toughness", 1)):
                    self.emit("assert_matching_permanents", prop=prop, player=seat, index=csvid, value=expected)

    def genesis(self):
        zero = self.mode == "zero_then_draw"
        self.deck(self.owner, (BEAR if zero else GIANT, SWAMP, PLAINS))
        self.deck(self.foe, (FOREST if zero else MURDER, ISLAND, MOUNTAIN))
        self.cast_source()
        self.observe_minions()
        self.state()
        for seat in (self.foe, self.owner):
            self.upkeep(seat)
            name, = self.library_prefix(seat, 1, "graveyard")
            self.tokens[seat] += int(self.oracle[name]["cmc"])
            self.observe_minions()
            self.state()
            if zero and seat == self.foe:
                self.draw(seat)
                self.state()

    def attack_with_flying(self):
        self.emit("turn", player=self.owner)
        self.observe("source", "sick", 0)
        self.emit("begin_attackers")
        self.emit("attack", ref="source", value=1)
        self.emit("finish_attackers")
        self.emit("begin_blockers")
        self.emit("block", ref="ground", other="source")
        self.observe("ground", "blocking_count", 0)
        self.emit("advance_combat")
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")
        self.observe("source", "tapped", 1)
        self.life[self.foe] -= 2

    def siren(self):
        manual = self.mode == "manual_empty_hand"
        self.deck(self.owner, (SWAMP, BEAR))
        self.deck(self.foe, (FOREST, PLAINS, MOUNTAIN))
        self.add("ground", BEAR, self.foe, "battlefield")
        if manual:
            self.add("untapper", TWIDDLE, self.owner, "hand")
            self.add("repeat", TWIDDLE, self.owner, "hand")
        else:
            self.add("discard_subject", GIANT, self.foe, "hand")
        self.cast_source()
        self.upkeep(self.foe)
        self.upkeep(self.owner)
        self.state()
        self.attack_with_flying()
        self.state()
        if manual:
            self.cast("untapper", target="source", resolution_dialog=1)
            self.library_prefix(self.foe, 1, "graveyard")
            self.observe("source", "tapped", 0)
            self.state()
            self.cast("repeat", target="source", resolution_dialog=1)
            self.observe("source", "tapped", 0)
        else:
            self.upkeep(self.foe)
            self.observe("source", "tapped", 1)
            self.state()
            self.emit("discard_policy", value=1)
            self.untap(self.owner)
            self.library_prefix(self.foe, 1, "graveyard")
            self.move("discard_subject", "graveyard")
            self.observe("source", "tapped", 0)
        self.state()
        self.upkeep(self.foe)
        self.draw(self.foe)
        self.state()

    def run(self):
        action = {VANDAL: self.vandal, CURSE: self.curse, GENESIS: self.genesis, SIREN: self.siren,
                  DEEP: self.deep_spawn, SPLINTER: self.splinterfright,
                  SERPENT: self.tolarian_serpent, PATIENT: self.patient_rebuilding}
        action[self.csvid]()

    def trample_combat(self, power):
        self.emit("turn", player=self.owner)
        self.emit("begin_attackers")
        self.emit("attack", ref="source", value=1)
        self.emit("finish_attackers")
        self.emit("begin_blockers")
        self.emit("block", ref="blocker", other="source", value=1)
        self.emit("advance_combat")
        self.observe("source", "blocked", 1)
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")
        self.move("blocker", "graveyard")
        self.life[self.foe] -= power - int(self.oracle[BEAR]["toughness"])

    def deep_tax(self, take_mill):
        self.emit("choose_dialog", player=self.owner, value=int(not take_mill))
        self.emit("upkeep_step", player=self.owner)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        if take_mill:
            assert len(self.library[self.owner]) >= 2
            self.library_prefix(self.owner, 2, "graveyard")
            self.observe("source", "in_play", 1)
        else:
            self.move("source", "graveyard")
        self.state()

    def observe_deep_targets(self, legal):
        for ref in ("foe_probe", "own_probe"):
            self.emit("can_target_creature", ref=ref, other="source", value=int(legal))

    def deep_spawn(self):
        protects = self.mode == "shroud_trample"
        self.deck(self.owner, (MOUNTAIN, ISLAND, SWAMP, FOREST, PLAINS, GIANT))
        self.deck(self.foe, (SWAMP, BEAR))
        if protects:
            self.add("blocker", BEAR, self.foe, "battlefield")
            self.add("removal", MURDER, self.foe, "hand")
            self.add("foe_probe", BOUNCE, self.foe, "hand")
            self.add("own_probe", BOUNCE, self.owner, "hand")
        self.cast_source()
        self.upkeep(self.foe)
        self.state()
        if not protects:
            self.untap(self.owner)
            self.deep_tax(False)
            return

        self.observe_deep_targets(True)
        self.fund(self.foe, self.cost("removal"))
        self.emit("choose_target", ref="source", player=self.foe)
        self.emit("cast", ref="removal", value=1)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack", value=1)
        self.emit("announce_spell", ref="removal")
        self.fund(self.owner, [0, 0, 1, 0, 0, 0])
        self.emit("activate_response", ref="source", value=1)
        self.emit("assert_stack", value=2)
        self.mana("assert_mana", self.owner)
        self.emit("resolve_response")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        self.emit("refresh")
        self.move("removal", "graveyard")
        self.observe("source", "in_play", 1)
        self.observe("source", "tapped", 1)
        self.observe_deep_targets(False)
        self.state()

        self.untap(self.owner)
        self.observe("source", "tapped", 1)
        self.observe_deep_targets(True)
        self.deep_tax(True)
        self.upkeep(self.foe)
        self.observe("source", "tapped", 1)
        self.state()
        self.untap(self.owner)
        self.observe("source", "tapped", 0)
        self.deep_tax(True)
        self.trample_combat(6)
        self.state()

    def observe_grave_stats(self):
        self.emit("refresh")
        total = sum(obj["player"] == self.owner and obj["zone"] == "graveyard"
                    and "Creature" in self.oracle[obj["name"]]["type_line"].split(" — ")[0]
                    for obj in self.objects.values())
        self.observe("source", "in_play", 1)
        self.observe("source", "power", total)
        self.observe("source", "toughness", total)
        return total

    def splinterfright(self):
        combat = self.mode == "growth_trample"
        self.add("own_dead", BEAR, self.owner, "graveyard")
        self.add("foe_dead", GIANT, self.foe, "graveyard")
        self.deck(self.foe, (SWAMP, FOREST))
        if combat:
            self.add("own_other_dead", GIANT, self.owner, "graveyard")
            self.add("blocker", BEAR, self.foe, "battlefield")
            self.deck(self.owner, (GIANT, BEAR, FOREST, PLAINS))
        else:
            self.deck(self.owner, (FOREST, BOLT, GIANT))
        self.cast_source()
        self.observe_grave_stats()
        self.upkeep(self.foe)
        self.observe_grave_stats()
        self.state()
        self.upkeep(self.owner)
        self.library_prefix(self.owner, 2, "graveyard")
        power = self.observe_grave_stats()
        self.state()
        if combat:
            self.trample_combat(power)
        else:
            self.upkeep(self.foe)
            self.upkeep(self.owner)
            self.library_prefix(self.owner, 2, "graveyard")
            self.observe_grave_stats()
        self.state()

    def tolarian_serpent(self):
        short = self.mode == "short_library"
        library = (MOUNTAIN, ISLAND, SWAMP, FOREST, PLAINS, BEAR, GIANT, BOUNCE, HUNGER)
        self.deck(self.owner, library[:3] if short else library)
        self.deck(self.foe, (SWAMP, BEAR, ISLAND))
        self.cast_source()
        self.upkeep(self.foe)
        self.state()
        self.upkeep(self.owner)
        self.library_prefix(self.owner, 7, "graveyard")
        self.state()
        if not short:
            self.draw(self.owner)
            self.state()

    def patient_rebuilding(self):
        mixed = self.mode == "mixed_draw"
        self.deck(self.owner, (MOUNTAIN, SWAMP, ISLAND, FOREST, BEAR))
        self.deck(self.foe, (PLAINS, GIANT, FOREST, SWAMP, BOLT) if mixed else (BEAR, BOLT))
        self.cast_source()
        self.upkeep(self.foe)
        self.state()
        self.untap(self.owner)
        self.emit("choose_player", player=self.owner, value=self.foe)
        self.emit("upkeep_step", player=self.owner)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        milled = self.library_prefix(self.foe, 3, "graveyard")
        land_count = sum("Land" in self.oracle[name]["type_line"].split(" — ")[0] for name in milled)
        self.library_prefix(self.owner, land_count, "hand")
        self.state()
        if mixed:
            self.draw(self.owner)
            self.state()



def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()["sources"][name]["registry"]["csvid"] != csvid:
        raise ValueError("exact registered mill primary CSVID required")
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
