"""Paid Flash responses, spell access, and selected-color protection."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "flash_entry_spell_access_and_protection"
PROFILE = Path(__file__).with_name("data") / "flash_entry_spell_access_and_protection_wave8_profiles.json"
PREFIX = "FlashEntrySpellAccessAndProtectionRecipe"
SEED = 2717
DUAL, SNAP, HULK, PLAX, TIGER = 14456, 12079, 16558, 8255, 9012
MODES = {
    DUAL: ("copy_draw", "retain_target", "redirect"),
    SNAP: ("select_instant", "select_sorcery"),
    HULK: ("free_instant", "decline"),
    PLAX: ("green_paid", "sacrifice"),
    TIGER: ("targeted_fizzle", "untargeted_prevention", "different_color", "source_removed"),
}
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZONE_NAMES = {code: zone for zone, code in ZONES.items()}
ZERO = [0] * 6
BLUE, RED = 2, 4
DRAW, SORCERY, BOLT = "Reach Through Mists", "Divination", "Lightning Bolt"
BEAR, BOUNCE, PUMP, KILL = "Grizzly Bears", "Unsummon", "Giant Growth", "Murder"
OWN_TOP, FOE_TOP = "Plains", "Mountain"
RIFT, MASK, RED_PROBE, BLUE_PROBE = "Flame Rift", "Ivory Mask", "Flames of the Blood Hand", "Inspiration"
WITNESSES = {DRAW, SORCERY, BOLT, BEAR, BOUNCE, PUMP, KILL, OWN_TOP, FOE_TOP, RIFT, MASK, RED_PROBE, BLUE_PROBE}
GAPS = {
    DUAL: ["The old and new target choices use the engine's copy-target menu. Its independent Cancel route is untested."],
    SNAP: ["Actual target selection is covered. Casting through the unattached granted flashback effect and its expiry require a named legacy action."],
    TIGER: ["Player targeting and damage prevention are covered. No player-enchanting source is used by this cohort."],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get("layout") != "normal" or record.get("selected_face")
            or record.get("card_faces") or record.get("keywords") != ["Flash"]
            or not record.get("oracle_text", "").startswith("Flash")):
        raise ValueError("whole pinned Flash record required")
    return dict(csvid=csvid, printed_mana=cost_from_oracle(record), rules=record["oracle_text"])


def variants(shape):
    return [f"{mode}_{seat}" for mode in MODES[shape["csvid"]] for seat in (0, 1)]


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
                    or row.get("kind") != "card" or oracle[name].get("layout") != "normal"):
                raise ValueError("changed Flash source or witness: " + name)
            if group == "sources":
                if not str(row.get("handler", "")).startswith("manalinkeh_card_"):
                    raise ValueError("implemented Flash primary required")
                classify(oracle[name], row["csvid"])
    if (oracle[RIFT]["oracle_text"] != "Flame Rift deals 4 damage to each player."
            or not oracle[MASK]["oracle_text"].startswith("You have shroud.")
            or oracle[RED_PROBE]["colors"] != ["R"] or oracle[BLUE_PROBE]["colors"] != ["U"]):
        raise ValueError("untargeted red damage and distinct target-color witnesses required")


class Board:
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        self.csvid, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.life = [20, 20]
        self.add("source", name, owner, "hand")

    def add(self, ref, name, seat, zone):
        self.setup.append(card(name, ref, seat, zone))
        self.objects[ref] = dict(name=name, player=seat, zone=zone)

    def emit(self, op, ref="", other="", prop="", *, player=0, value=0, zone=0, index=0, color=0):
        if op == "untap_step":
            self.steps.append(step(op, player=player))
            return
        fields = dict(ref=ref, other=other, property=prop, player=player, value=value, index=index, color=color)
        values = {key: item for key, item in fields.items() if item or key == "value"}
        if op in {"set_mana", "assert_mana"}:
            values.update(player=player, color=color)
        elif op == "assert_zone_multiplicity":
            values.update(player=player, zone=ZONE_NAMES[zone])
        elif op in {"choose_optional_zone", "choose_zone_card", "choose_color", "choose_player"}:
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
        self.objects[ref]["zone"] = zone

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

    def pending_parent(self, *, target=None, target_player=None):
        self.emit("turn", player=self.foe)
        self.fund(self.owner, ZERO)
        self.fund(self.foe, self.cost("parent"))
        if target is not None:
            self.emit("choose_target", ref=target, player=self.foe)
        if target_player is not None:
            self.emit("choose_player", player=self.foe, value=target_player)
        self.emit("cast", ref="parent", value=1)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.foe)
        self.emit("announce_spell", ref="parent")

    def enter(self, pool=None, dialog=None):
        self.fund(self.owner, self.cost("source") if pool is None else pool)
        if dialog is not None:
            self.emit("choose_dialog", player=self.owner, value=dialog)
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

    def dualcaster(self):
        draw = self.mode == "copy_draw"
        self.add("parent", DRAW if draw else BOLT, self.foe, "hand")
        if draw:
            self.add("own_top", OWN_TOP, self.owner, "library")
            self.add("foe_top", FOE_TOP, self.foe, "library")
        self.pending_parent(target_player=None if draw else self.owner)
        self.enter()
        if not draw:
            copy_target = self.owner if self.mode == "retain_target" else self.foe
            self.emit("choose_player", player=self.owner, value=copy_target)
        self.resolve_entry()
        if draw:
            self.move("own_top", "hand")
            self.move("foe_top", "hand")
        else:
            self.life[self.owner] -= 3
            self.life[copy_target] -= 3
        self.inspect_source()
        self.receipts()

    def snapcaster(self):
        self.add("parent", BOLT, self.foe, "hand")
        self.add("own_instant", DRAW, self.owner, "graveyard")
        self.add("own_sorcery", SORCERY, self.owner, "graveyard")
        self.add("own_creature", BEAR, self.owner, "graveyard")
        self.add("other_instant", BOLT, self.foe, "graveyard")
        self.pending_parent(target_player=self.owner)
        self.enter()
        self.emit("observe_search_filters", value=1)
        target = DRAW if self.mode == "select_instant" else SORCERY
        self.emit("choose_zone_card", player=self.owner, value=self.rows[target]["csvid"])
        self.resolve_entry()
        self.emit("assert_offered_count", value=3)
        for name in (DRAW, SORCERY):
            self.emit("assert_offered_legal_card", index=self.rows[name]["csvid"], value=1)
        self.emit("assert_offered_legal_card", index=self.rows[BEAR]["csvid"], value=0)
        self.life[self.owner] -= 3
        self.inspect_source()
        self.receipts()

    def gearhulk(self):
        self.add("parent", BOLT, self.foe, "hand")
        self.add("own_instant", DRAW, self.owner, "graveyard")
        self.add("own_sorcery", SORCERY, self.owner, "graveyard")
        self.add("other_instant", DRAW, self.foe, "graveyard")
        self.add("own_top", OWN_TOP, self.owner, "library")
        self.pending_parent(target_player=self.owner)
        self.enter()
        if self.owner == 0:
            self.emit("choose_trigger", ref="source", player=0)
        self.emit("observe_search_filters", value=1)
        decline = self.mode == "decline"
        selection = "choose_optional_zone" if decline else "choose_zone_card"
        self.emit(selection, player=self.owner, value=self.rows[DRAW]["csvid"])
        self.resolve_entry()
        self.emit("assert_offered_count", value=1)
        self.emit("assert_offered_legal_card", index=self.rows[DRAW]["csvid"], value=1)
        if decline:
            self.emit("assert_forced_zone_choices")
        else:
            self.move("own_top", "hand")
            self.move("own_instant", "exile")
        self.life[self.owner] -= 3
        self.inspect_source()
        self.receipts()

    def plaxmanta(self):
        self.add("parent", BOUNCE, self.foe, "hand")
        self.add("host", BEAR, self.owner, "battlefield")
        self.add("opposing", BEAR, self.foe, "battlefield")
        self.add("friendly_probe", PUMP, self.owner, "hand")
        self.add("removal", KILL, self.foe, "hand")
        self.emit("can_target_creature", ref="friendly_probe", other="host", value=1)
        self.emit("can_target_creature", ref="removal", other="host", value=1)
        self.pending_parent(target="host")
        paid_green = self.mode == "green_paid"
        pool = [0, 0, 1, 1, 0, 0] if paid_green else self.cost("source")
        self.enter(pool, dialog=1 if paid_green and self.owner == 0 else None)
        self.resolve_entry()
        if paid_green:
            self.inspect_source()
            self.emit("can_target_creature", ref="removal", other="source", value=0)
        else:
            self.move("source", "graveyard")
        self.observe("host", "in_play", 1)
        self.emit("can_target_creature", ref="friendly_probe", other="host", value=0)
        self.emit("can_target_creature", ref="removal", other="host", value=0)
        self.emit("can_target_creature", ref="friendly_probe", other="opposing", value=1)
        self.receipts()

        self.emit("cleanup")
        self.emit("refresh")
        self.emit("advance_turn")
        self.emit("untap_step", player=self.owner)
        self.emit("turn", player=self.owner)
        self.emit("can_target_creature", ref="friendly_probe", other="host", value=1)
        self.emit("can_target_creature", ref="removal", other="host", value=1)
        if paid_green:
            self.emit("can_target_creature", ref="removal", other="source", value=1)
        self.fund(self.foe, self.cost("removal"))
        self.emit("choose_target", ref="host", player=self.foe)
        self.emit("cast", ref="removal", value=1)
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        self.move("host", "graveyard")
        self.move("removal", "graveyard")
        self.receipts()

    def run(self):
        actions = {DUAL: self.dualcaster, SNAP: self.snapcaster, HULK: self.gearhulk,
                   PLAX: self.plaxmanta, TIGER: self.tiger}
        actions[self.csvid]()

    def red_spell(self, ref, *, target=None, target_player=None):
        self.fund(self.foe, self.cost(ref))
        if target is not None:
            self.emit("choose_target", ref=target, player=self.foe)
        if target_player is not None:
            self.emit("choose_player", player=self.foe, value=target_player)
        self.emit("cast", ref=ref, value=1)
        self.emit("assert_stack", value=1)
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        self.move(ref, "graveyard")

    def tiger_targets(self, red, blue):
        self.emit("can_cast", ref="red_probe", value=int(red))
        self.emit("can_cast", ref="blue_probe", value=int(blue))

    def tiger(self):
        targeted = self.mode == "targeted_fizzle"
        select_red = self.mode != "different_color"
        remove_source = self.mode == "source_removed"
        self.add("parent", BOLT if targeted else RIFT, self.foe, "hand")
        self.add("other_player_shroud", MASK, self.foe, "battlefield")
        self.add("red_probe", RED_PROBE, self.foe, "hand")
        self.add("blue_probe", BLUE_PROBE, self.foe, "hand")
        self.add("after_expiry", BOLT, self.foe, "hand")
        if remove_source:
            self.add("red_removal", BOLT, self.foe, "hand")
            self.add("continued_damage", RIFT, self.foe, "hand")

        # Shroud removes the caster as a candidate for the player-only probes.
        self.tiger_targets(red=True, blue=True)
        self.pending_parent(target_player=self.owner if targeted else None)
        self.enter()
        self.emit("choose_color", player=self.owner, value=RED if select_red else BLUE)
        self.resolve_entry()
        self.inspect_source()
        if not targeted:
            self.life[self.foe] -= 4
            if not select_red:
                self.life[self.owner] -= 4
        self.tiger_targets(red=not select_red, blue=select_red)
        self.receipts()

        if remove_source:
            # The player, rather than Tiger, has protection from red.
            self.red_spell("red_removal", target="source")
            self.move("source", "graveyard")
            self.red_spell("continued_damage")
            self.life[self.foe] -= 4
            self.tiger_targets(red=False, blue=True)
            self.receipts()

        self.emit("cleanup")
        self.emit("advance_turn")
        self.emit("untap_step", player=self.owner)
        self.emit("turn", player=self.owner)
        self.emit("refresh")
        self.tiger_targets(red=True, blue=True)
        self.red_spell("after_expiry", target_player=self.owner)
        self.life[self.owner] -= 3
        self.receipts()


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
