"""Paid Flash responses that change search, entry, draw, and hand size."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "flash_remaining_rule_effects"
PROFILE = Path(__file__).with_name("data") / "flash_remaining_rule_effects_wave8_profiles.json"
AVEN, PRIEST, GRYFF, THIEF, JIN = 8889, 14448, 14115, 13186, 11623
MODES = {
    AVEN: ("opposing_search", "own_search"),
    PRIEST: ("uncast_exiled", "cast_and_tokens"),
    GRYFF: ("creature_enter", "artifact_enter"),
    THIEF: ("outside_draw", "first_draw"),
    JIN: ("own_end", "source_removed", "unlimited_hand", "hand_limit", "normal_limit_after_removal"),
}
PREFIX = "FlashRemainingRuleEffectsRecipe"
SEED = 2739
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZONE_NAMES = {code: zone for zone, code in ZONES.items()}
ZERO = [0] * 6
DRAW, TUTOR, TOKEN_SPELL, SOLDIER = "Reach Through Mists", "Rampant Growth", "Raise the Alarm", "Soldier"
REANIMATE, BEAR, BOLT, MURDER = "Zombify", "Grizzly Bears", "Lightning Bolt", "Murder"
WARDEN, VISIONARY, WELLSPRING, DIVINATION = "Soul Warden", "Elvish Visionary", "Ichor Wellspring", "Divination"
PLAINS, FOREST, ISLAND, SWAMP, MOUNTAIN = "Plains", "Forest", "Island", "Swamp", "Mountain"
WITNESSES = {DRAW, TUTOR, TOKEN_SPELL, SOLDIER, REANIMATE, BEAR, BOLT, MURDER, WARDEN, VISIONARY,
             WELLSPRING, DIVINATION, PLAINS, FOREST, ISLAND, SWAMP, MOUNTAIN}


SPELLBOOK, MOAT = "Spellbook", "Moat"
WITNESSES |= {SPELLBOOK, MOAT}
GAPS = {JIN: ["The full AI turn path executes the production hand-limit phase for seat 1 only. Its seat 0 mirror needs a direct real discard-phase action."]}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get("layout") != "normal" or record.get("selected_face")
            or record.get("card_faces") or "Flash" not in record.get("keywords", ())
            or not record.get("oracle_text", "").startswith("Flash")):
        raise ValueError("whole pinned Flash record required")
    return dict(csvid=csvid, printed_mana=cost_from_oracle(record), rules=record["oracle_text"])


def variants(shape):
    return [f"{mode}_{seat}" for mode in MODES[shape["csvid"]]
            for seat in ((0,) if mode in {"hand_limit", "normal_limit_after_removal"} else (0, 1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError("distinct supported Flash scenarios required")


def spec(declaration, record):
    source = profiles()["sources"].get(record.get("name"))
    if not source or not exact(source["oracle"], record):
        raise ValueError("exact pinned Flash primary required")
    shape = classify(record, source["registry"]["csvid"])
    if (declaration.get("family") != FAMILY or type(declaration.get("version")) is not int
            or declaration["version"] != 2 or declaration.get("oracle_text") != record["oracle_text"]
            or not exact(declaration.get("params"), shape)):
        raise ValueError("versioned whole Flash declaration required")
    validate_variants(shape, declaration.get("variants"))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {"sources", "witnesses"} or set(pins["witnesses"]) != WITNESSES:
        raise ValueError("independent exact Flash witnesses required")
    if {pin["registry"]["csvid"] for pin in pins["sources"].values()} != set(MODES):
        raise ValueError("exact Flash primaries required")
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin["oracle"]) or not exact(row, pin["registry"])
                    or row.get("kind") != "card"
                    or oracle[name].get("layout") != ("token" if name == SOLDIER else "normal")):
                raise ValueError("changed Flash source or witness: " + name)
            if group == "sources":
                if not str(row.get("handler", "")).startswith("manalinkeh_card_"):
                    raise ValueError("implemented Flash primary required")
                classify(oracle[name], row["csvid"])
    if (oracle[MOAT]["oracle_text"] != "Creatures without flying can't attack."
            or oracle[SPELLBOOK]["oracle_text"] != "You have no maximum hand size."):
        raise ValueError("exact hand-limit and combat-exclusion witnesses required")


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
        fields = dict(ref=ref, other=other, property=prop, player=player, value=value, index=index, color=color)
        values = {key: item for key, item in fields.items() if item or key == "value"}
        if op in {"set_mana", "assert_mana"}:
            values.update(player=player, color=color)
        elif op.startswith("assert_zone"):
            values.update(player=player, zone=ZONE_NAMES[zone])
        elif op in {"choose_zone_card", "assert_matching_permanents", "assert_shuffle_count", "assert_offered_legal_card"}:
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
            self.observe("source", stat, int(self.oracle[self.name][stat]))

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

    def pending_parent(self, *, grave_target=None):
        self.emit("turn", player=self.foe)
        self.fund(self.owner, ZERO)
        self.fund(self.foe, self.cost("parent"))
        if grave_target is not None:
            self.emit("choose_zone_card", player=self.foe, value=self.rows[grave_target]["csvid"])
        self.emit("cast", ref="parent", value=1)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.foe)
        self.emit("announce_spell", ref="parent")

    def enter(self):
        self.fund(self.owner, self.cost("source"))
        self.emit("cast_response", ref="source", value=1)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack", value=2)
        self.mana("assert_mana", self.owner)

    def resolve_entry(self):
        self.emit("resolve_response")
        self.emit("refresh")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        self.move("source", "battlefield")
        self.move("parent", "graveyard")

    def add_library(self, seat, names):
        for index, name in enumerate(names):
            self.add(f"lib_{seat}_{index}", name, seat, "library")

    def draw(self, seat, amount):
        assert len(self.library[seat]) >= amount
        for ref in self.library[seat][:amount]:
            self.move(ref, "hand")

    def observe_top(self, seat):
        self.emit("assert_zone", player=seat, zone=ZONES["library"], value=len(self.library[seat]))
        if self.library[seat]:
            top = self.objects[self.library[seat][0]]["name"]
            self.emit("assert_zone_card", player=seat, zone=ZONES["library"], index=0, value=self.rows[top]["csvid"])

    def paid_spell(self, ref, *, target=None):
        seat = self.objects[ref]["player"]
        self.fund(seat, self.cost(ref))
        if target is not None:
            self.emit("choose_target", ref=target, player=seat)
        self.emit("cast", ref=ref, value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", seat)

    def drain(self):
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")

    def set_tutor_plan(self, seat, land):
        self.emit("observe_search_filters", value=1)
        self.emit("choose_zone_card", player=seat, value=self.rows[land]["csvid"])

    def tutor_result(self, seat, names, selected):
        self.emit("assert_offered_count", value=len(names))
        for name in names:
            basic = self.oracle[name]["type_line"].startswith("Basic Land")
            self.emit("assert_offered_legal_card", index=self.rows[name]["csvid"], value=int(basic))
        self.emit("assert_shuffle_count", player=seat, value=1)
        self.emit("assert_matching_permanents", prop="tapped", player=seat, value=1,
                  index=self.rows[selected]["csvid"])
        ref = next(ref for ref in self.library[seat] if self.objects[ref]["name"] == selected)
        self.move(ref, "battlefield")

    def flying(self):
        self.emit("advance_turn")
        self.emit("untap_step", player=self.owner)
        self.emit("turn", player=self.owner)
        self.observe("source", "sick", 0)
        self.emit("begin_attackers")
        self.emit("attack", ref="source", value=1)
        self.emit("finish_attackers")
        self.emit("begin_blockers")
        self.emit("block", ref="ground_blocker", other="source")
        self.observe("ground_blocker", "blocking_count", 0)
        self.emit("advance_combat")
        self.emit("combat_damage")
        self.drain()
        self.emit("end_combat")
        self.life[self.foe] -= int(self.oracle[self.name]["power"])

    def aven(self):
        opposed = self.mode == "opposing_search"
        searcher = self.foe if opposed else self.owner
        self.add("parent", TUTOR if opposed else DRAW, self.foe, "hand")
        self.add("ground_blocker", BEAR, self.foe, "battlefield")
        searched_library = (BEAR, PLAINS, BOLT, WARDEN, FOREST, ISLAND)
        self.add_library(searcher, searched_library)
        if not opposed:
            self.add("self_tutor", TUTOR, self.owner, "hand")
            self.add_library(self.foe, (MOUNTAIN,))
        self.pending_parent()
        self.enter()
        if opposed:
            self.set_tutor_plan(searcher, PLAINS)
        self.resolve_entry()
        self.inspect_source()
        if opposed:
            self.tutor_result(searcher, searched_library[:4], PLAINS)
            self.flying()
        else:
            self.draw(self.foe, 1)
            self.emit("advance_turn")
            self.emit("untap_step", player=self.owner)
            self.emit("turn", player=self.owner)
            self.paid_spell("self_tutor")
            self.set_tutor_plan(searcher, ISLAND)
            self.drain()
            self.move("self_tutor", "graveyard")
            self.tutor_result(searcher, searched_library, ISLAND)
        self.receipts()

    def priest(self):
        uncast = self.mode == "uncast_exiled"
        if uncast:
            self.add("parent", REANIMATE, self.foe, "hand")
            self.add("subject", BEAR, self.foe, "graveyard")
        else:
            self.add("parent", BEAR, self.foe, "hand")
            self.add("token_spell", TOKEN_SPELL, self.foe, "hand")
        self.pending_parent(grave_target=BEAR if uncast else None)
        self.enter()
        self.resolve_entry()
        self.inspect_source()
        if uncast:
            self.move("subject", "exile")
        else:
            self.move("parent", "battlefield")
            self.observe("parent", "in_play", 1)
            self.paid_spell("token_spell")
            self.drain()
            self.move("token_spell", "graveyard")
            self.tokens[self.foe] = 2
            token = self.rows[SOLDIER]["csvid"]
            for prop, value in (("token", 1), ("power", 1), ("toughness", 1),
                                ("color", 1 << 5), ("subtype_Soldier", 1)):
                self.emit("assert_matching_permanents", prop=prop, player=self.foe, value=value, index=token)
        for seat in (0, 1):
            for zone, code in ZONES.items():
                amount = self.tokens[seat] if zone == "battlefield" else 0
                self.emit("assert_zone_multiplicity", player=seat, zone=code,
                          value=amount, index=self.rows[SOLDIER]["csvid"])
        self.receipts()

    def gryff(self):
        creature = self.mode == "creature_enter"
        self.add("parent", VISIONARY if creature else WELLSPRING, self.foe, "hand")
        self.add("ground_blocker", BEAR, self.foe, "battlefield")
        self.add("warden", WARDEN, self.foe, "battlefield")
        self.add_library(self.foe, (FOREST, ISLAND))
        if creature:
            self.add("removal", MURDER, self.foe, "hand")
            self.add("after_removal", VISIONARY, self.foe, "hand")
        self.pending_parent()
        self.enter()
        self.resolve_entry()
        self.move("parent", "battlefield")
        self.inspect_source()
        self.observe("parent", "in_play", 1)
        if not creature:
            self.draw(self.foe, 1)
        self.observe_top(self.foe)
        self.receipts()

        if creature:
            self.paid_spell("removal", target="source")
            self.drain()
            self.move("removal", "graveyard")
            self.move("source", "graveyard")
            self.paid_spell("after_removal")
            self.drain()
            self.move("after_removal", "battlefield")
            self.draw(self.foe, 1)
            self.life[self.foe] += 1
            self.observe_top(self.foe)
        else:
            self.flying()
        self.receipts()

    def scheduled_draw(self, seat):
        self.emit("advance_turn")
        self.emit("untap_step", player=seat)
        self.emit("upkeep_step", player=seat)
        self.emit("draw_step", player=seat)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        self.draw(seat, 1)
        self.observe_top(seat)

    def thief(self):
        first_draw = self.mode == "first_draw"
        self.add("parent", DIVINATION, self.foe, "hand")
        self.add("extra_draw", DRAW, self.foe if first_draw else self.owner, "hand")
        self.add_library(self.owner, (ISLAND, SWAMP, FOREST, PLAINS, MOUNTAIN))
        self.add_library(self.foe, (MOUNTAIN, PLAINS, FOREST, ISLAND))
        self.pending_parent()
        self.enter()
        self.resolve_entry()
        self.inspect_source()
        self.draw(self.owner, 2)
        self.observe_top(self.owner)
        self.observe_top(self.foe)
        self.receipts()

        if first_draw:
            self.scheduled_draw(self.owner)
            self.scheduled_draw(self.foe)
            self.receipts()
            # The real draw step left PHASE_DRAW current. An instant cast now
            # supplies the opponent's second draw before any main-phase change.
        self.paid_spell("extra_draw")
        self.drain()
        self.move("extra_draw", "graveyard")
        self.draw(self.owner, 1)
        self.observe_top(self.owner)
        self.observe_top(self.foe)
        self.receipts()

    def run(self):
        action = {AVEN: self.aven, PRIEST: self.priest, GRYFF: self.gryff,
                  THIEF: self.thief, JIN: self.jin}
        action[self.csvid]()

    def jin(self):
        if self.mode in {"hand_limit", "normal_limit_after_removal"}:
            self.jin_hand_limit()
            return
        removed = self.mode == "source_removed"
        unlimited = self.mode == "unlimited_hand"
        self.add("parent", DRAW, self.foe, "hand")
        self.add_library(self.foe, (MOUNTAIN, PLAINS))
        self.add_library(self.owner, (ISLAND, SWAMP, FOREST, PLAINS, MOUNTAIN, ISLAND, SWAMP, FOREST))
        if removed:
            self.add("removal", MURDER, self.foe, "hand")
        if unlimited:
            self.add("no_maximum", SPELLBOOK, self.owner, "battlefield")
            self.add("held_0", BEAR, self.owner, "hand")
            self.add("held_1", BEAR, self.owner, "hand")
        self.pending_parent()
        self.enter()
        self.resolve_entry()
        self.draw(self.foe, 1)
        self.inspect_source()
        self.observe_top(self.owner)
        self.receipts()

        self.emit("advance_turn")
        self.emit("untap_step", player=self.owner)
        self.emit("turn", player=self.owner)
        self.observe_top(self.owner)
        self.receipts()
        if removed:
            self.paid_spell("removal", target="source")
            self.drain()
            self.move("source", "graveyard")
            self.move("removal", "graveyard")
        if unlimited:
            # Retain exact Oracle counts if a native unprinted discard asks
            # for a mandatory choice. The chosen card remains observable.
            self.emit("discard_policy", value=1)
        self.emit("cleanup")
        self.emit("refresh")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        if not removed:
            self.draw(self.owner, 7)
        self.observe_top(self.owner)
        self.receipts()

    def jin_hand_limit(self):
        assert self.owner == 0
        remove_source = self.mode == "normal_limit_after_removal"
        self.add("parent", DRAW, self.foe, "hand")
        self.add("combat_guard", MOAT, self.foe, "battlefield")
        self.add_library(self.owner, (BEAR,) * 10)
        self.add_library(self.foe, (BEAR,) * 4)
        for index in range(8):
            self.add(f"held_{index}", BEAR, self.foe, "hand")
        if remove_source:
            self.add("removal", MURDER, self.foe, "hand")
        self.pending_parent()
        self.enter()
        self.resolve_entry()
        self.draw(self.foe, 1)
        self.inspect_source()
        if remove_source:
            self.paid_spell("removal", target="source")
            self.drain()
            self.move("source", "graveyard")
            self.move("removal", "graveyard")
        self.receipts()

        # ai_turns reaches production discard for seat 1 under its actual
        # AI policy. No card in either hand is castable without new mana.
        self.emit("ai_turns", value=2)
        self.emit("assert_stack")
        self.emit("assert_choices_consumed")
        self.draw(self.foe, 1)
        self.observe_top(self.foe)
        maximum = 7 if remove_source else 0
        self.emit("assert_zone", player=self.foe, zone=ZONES["hand"], value=maximum)
        self.emit("assert_zone_multiplicity", player=self.foe, zone=ZONES["hand"],
                  index=self.rows[BEAR]["csvid"], value=maximum)
        self.emit("assert_zone_multiplicity", player=self.foe, zone=ZONES["graveyard"],
                  index=self.rows[BEAR]["csvid"], value=10 - maximum)
        self.emit("assert_zone_multiplicity", player=self.owner, zone=ZONES["battlefield"],
                  index=self.csvid, value=int(not remove_source))
        for seat in (0, 1):
            self.emit("assert_life", player=seat, value=20)



def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()["sources"][name]["registry"]["csvid"] != csvid:
        raise ValueError("exact registered Flash primary CSVID required")
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
