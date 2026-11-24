"""Phase and mill-trigger scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "mill_remaining_phase_and_mill_triggers"
PROFILE = Path(__file__).with_name("data") / "mill_remaining_phase_and_mill_triggers_wave8_profiles.json"
AVEN, PRIEST, GRYFF, THIEF, JIN = 8889, 14448, 14115, 13186, 11623
MODES = {
    AVEN: ("opposing_search", "own_search"),
    PRIEST: ("uncast_exiled", "cast_and_tokens"),
    GRYFF: ("creature_enter", "artifact_enter"),
    THIEF: ("outside_draw", "first_draw"),
    JIN: ("own_end", "source_removed", "unlimited_hand", "hand_limit", "normal_limit_after_removal"),
}
FLOOD, FAMILIAR, INK, ORB = 12682, 2968, 9369, 6723
CRAWL, SEWER, DROWNER, CHAINS = 15773, 11734, 9520, 594
MODES = {
    FLOOD: ("borrowed_mana", "retap_short"),
    FAMILIAR: ("phase_and_leave", "prevention_expires"),
    INK: ("matched_reveal", "nonmatch", "kindred_reveal", "decline_reveal"),
    ORB: ("scheduled_both", "manual_repeat"),
    CRAWL: ("upkeep_and_external", "own_land_only"),
    SEWER: ("choose_self", "choose_opponent"),
    DROWNER: ("own_blue_paid", "foe_blue_paid", "nonblue"),
    CHAINS: ("outside_empty", "outside_with_discard", "first_and_additional"),
}
PREFIX = "MillRemainingPhaseAndMillTriggersRecipe"
SEED = 2809
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZONE_NAMES = {code: zone for zone, code in ZONES.items()}
ZERO, TAPPED, OPPONENT_OWNED = [0] * 6, 1, 16
BOUNCE, BOOMERANG, TWIDDLE, DISENCHANT = "Unsummon", "Boomerang", "Twiddle", "Disenchant"
BEAR, GIANT, BOOK, RIPPLE = "Grizzly Bears", "Hill Giant", "Spellbook", "Reality Ripple"
MERFOLK, KINDRED = "Merfolk of the Pearl Trident", "Merrow Commerce"
PLAINS, FOREST, ISLAND, SWAMP, MOUNTAIN = "Plains", "Forest", "Island", "Swamp", "Mountain"
WITNESSES = {BOUNCE, BOOMERANG, TWIDDLE, DISENCHANT, BEAR, GIANT, BOOK, RIPPLE, MERFOLK, KINDRED,
             PLAINS, FOREST, ISLAND, SWAMP, MOUNTAIN}


DRAW, INSECT, RAIN, BOLT = "Reach Through Mists", "Insect", "Stone Rain", "Lightning Bolt"
WITNESSES |= {DRAW, INSECT, RAIN, BOLT}
FOLLOWERS = (CRAWL, SEWER, DROWNER, CHAINS)
GAPS = {
    FAMILIAR: ["A separate pending phase-out trigger is not exposed. Full untap-to-upkeep effects are observed."],
    INK: ["The automated player reveals a matching card without a decline choice. The headless driver has no independent display receipt."],
    CRAWL: ["The optional upkeep selection is exercised through its acceptance. Independent automated decline is unobserved."],
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
    return [f"{mode}_{seat}" for mode in MODES[shape["csvid"]]
            for seat in ((0,) if mode == "decline_reveal" else (0, 1))]


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
                    or oracle[name].get("layout") != ("token" if name == INSECT else "normal")):
                raise ValueError("changed mill source or witness: " + name)
            if group == "sources":
                if not str(row.get("handler", "")).startswith("manalinkeh_card_"):
                    raise ValueError("implemented mill primary required")
                classify(oracle[name], row["csvid"])
    if (oracle[MERFOLK]["type_line"] != "Creature — Merfolk"
            or oracle[KINDRED]["type_line"] != "Kindred Enchantment — Merfolk"
            or oracle[RAIN]["oracle_text"] != "Destroy target land."
            or oracle[DRAW]["oracle_text"] != "Draw a card."):
        raise ValueError("exact Kinship, land destruction and draw witnesses required")


class Board:
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        self.csvid, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.life = [20, 20]
        self.remaining_mana = [ZERO, ZERO]
        self.library = [[], []]
        self.add("source", name, owner, "hand")

    def add(self, ref, name, seat, zone, *, flags=0):
        fixture = card(name, ref, seat, zone)
        states = [state for bit, state in ((TAPPED, "tapped"), (OPPONENT_OWNED, "opponent_owned")) if flags & bit]
        if states:
            fixture["state"] = states
        self.setup.append(fixture)
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
        elif op in {"choose_player", "choose_trigger_source", "assert_matching_permanents"}:
            values["player"] = player
        if op == "choose_trigger_source":
            values.pop("value")
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
            self.mana("assert_mana", seat, self.remaining_mana[seat])
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

    def cast(self, ref, *, target=None, recipient=None, resolution_dialog=None, destination="graveyard", from_pool=False):
        seat = self.objects[ref]["player"]
        if not from_pool:
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

    def cast_source(self, target=None):
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.cast("source", target=target, destination="battlefield")
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

    def twiddle(self, ref, target, *, untap):
        self.cast(ref, target=target, resolution_dialog=int(untap))
        self.observe(target, "tapped", int(not untap))

    def flooding(self):
        borrowed = self.mode == "borrowed_mana"
        controller = self.foe if borrowed else self.owner
        self.deck(self.owner, (MOUNTAIN, GIANT, PLAINS, SWAMP, FOREST))
        self.deck(self.foe, (SWAMP, BEAR, FOREST, PLAINS, ISLAND))
        self.add("land", ISLAND, controller, "battlefield", flags=OPPONENT_OWNED if borrowed else 0)
        self.add("other_land", FOREST, self.foe, "battlefield")
        if borrowed:
            self.add("decoy", TWIDDLE, self.owner, "hand")
            self.add("spend", BOUNCE, self.foe, "hand")
            self.add("bounce_witness", GIANT, self.owner, "battlefield")
            self.add("return_land", BOOMERANG, self.owner, "hand")
        else:
            for ref in ("tap", "tap_again", "untapper", "retap"):
                self.add(ref, TWIDDLE, self.owner, "hand")
        self.cast_source(target="land")
        self.observe("land", "tapped", 0)
        self.state()

        if borrowed:
            self.twiddle("decoy", "other_land", untap=False)
            self.state()
            self.emit("require_can_activate", ref="land", value=1)
            self.emit("activate", ref="land", value=1)
            self.emit("assert_stack")
            self.mana("assert_mana", self.foe, self.cost("spend"))
            self.observe("land", "tapped", 1)
            self.library_prefix(controller, 3, "graveyard")
            self.cast("spend", target="bounce_witness", from_pool=True)
            self.move("bounce_witness", "hand")
            self.state()

            self.cast("return_land", target="land")
            self.objects["land"]["player"] = self.owner
            self.move("land", "hand")
            self.move("source", "graveyard")
        else:
            self.twiddle("tap", "land", untap=False)
            self.library_prefix(controller, 3, "graveyard")
            self.state()
            self.twiddle("tap_again", "land", untap=False)
            self.state()
            self.twiddle("untapper", "land", untap=True)
            self.state()
            self.twiddle("retap", "land", untap=False)
            self.library_prefix(controller, 3, "graveyard")
        self.state()

    def familiar_presence(self, in_play):
        self.observe("source", "csvid", self.csvid)
        self.observe("source", "in_play", int(in_play))
        self.observe("source", "in_hand", 0)

    def familiar(self):
        shield = self.mode == "prevention_expires"
        self.deck(self.owner, (MOUNTAIN, GIANT, PLAINS, SWAMP, FOREST, ISLAND, BEAR))
        self.deck(self.foe, (SWAMP, FOREST, BEAR))
        self.add("leave", RIPPLE if shield else BOUNCE, self.foe, "hand")
        self.cast_source()
        self.state()
        if shield:
            self.fund(self.owner, [0, 0, 1, 0, 0, 0])
            self.emit("require_can_activate", ref="source", value=1)
            self.emit("activate", ref="source", value=1)
            self.emit("assert_stack", value=1)
            self.mana("assert_mana", self.owner)
            self.resolve()
            self.state()

        self.upkeep(self.foe)
        self.familiar_presence(True)
        self.state()
        self.untap(self.owner)
        self.familiar_presence(shield)
        self.emit("upkeep_step", player=self.owner)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        if not shield:
            self.library_prefix(self.owner, 3, "graveyard")
        self.state()

        if shield:
            # The duration expires at this upkeep. A real opposing spell can
            # now phase out the same physical object during this turn.
            self.cast("leave", target="source")
            self.familiar_presence(False)
            self.library_prefix(self.owner, 3, "graveyard")
            self.state()
        self.upkeep(self.foe)
        self.familiar_presence(False)
        self.state()
        self.upkeep(self.owner)
        self.familiar_presence(True)
        self.state()
        if not shield:
            self.cast("leave", target="source")
            self.move("source", "hand")
            self.library_prefix(self.owner, 3, "graveyard")
            self.state()

    def kinship(self):
        nonmatching = self.mode == "nonmatch"
        declines = self.mode == "decline_reveal"
        top = {"kindred_reveal": KINDRED, "nonmatch": BEAR}.get(self.mode, MERFOLK)
        self.deck(self.owner, (top, ISLAND, FOREST))
        self.deck(self.foe, (MOUNTAIN, GIANT, PLAINS, SWAMP, BEAR))
        self.cast_source()
        self.upkeep(self.foe)
        self.state()
        self.untap(self.owner)
        if self.owner == 0:
            self.emit("choose_trigger_source", ref="source", player=0)
            # The unmatched library card also opens an informational reveal display.
            self.emit("choose_dialog", player=0, value=int(declines))
        self.emit("upkeep_step", player=self.owner)
        self.emit("assert_unused_choices")
        self.emit("assert_stack")
        if not nonmatching and not declines:
            self.library_prefix(self.foe, 3, "graveyard")
        self.state()
        self.draw(self.owner)
        self.state()

    def orb(self):
        scheduled = self.mode == "scheduled_both"
        self.deck(self.owner, (MOUNTAIN, GIANT, PLAINS, SWAMP))
        self.deck(self.foe, (FOREST, BEAR, ISLAND, MOUNTAIN, PLAINS))
        if scheduled:
            self.add("own_a", BOOK, self.owner, "battlefield", flags=TAPPED)
            self.add("own_b", GIANT, self.owner, "battlefield", flags=TAPPED)
            self.add("foe_a", BOOK, self.foe, "battlefield", flags=TAPPED)
            self.add("foe_b", GIANT, self.foe, "battlefield", flags=TAPPED)
            self.add("foe_c", FOREST, self.foe, "battlefield", flags=TAPPED)
        else:
            self.add("witness", BEAR, self.foe, "battlefield", flags=TAPPED)
            for ref in ("first_untap", "repeated_untap", "retap", "after_removal"):
                self.add(ref, TWIDDLE, self.owner, "hand")
            self.add("kill_orb", DISENCHANT, self.owner, "hand")
        self.add("already_untapped", BEAR, self.owner, "battlefield")
        self.cast_source()
        self.state()
        if scheduled:
            self.untap(self.foe)
            for ref in ("foe_a", "foe_b", "foe_c"):
                self.observe(ref, "tapped", 0)
            for ref in ("own_a", "own_b"):
                self.observe(ref, "tapped", 1)
            self.library_prefix(self.foe, 3, "graveyard")
            self.state()
            self.emit("upkeep_step", player=self.foe)
            self.emit("assert_choices_consumed")
            self.draw(self.foe)
            self.state()

            self.untap(self.owner)
            for ref in ("own_a", "own_b", "already_untapped"):
                self.observe(ref, "tapped", 0)
            self.library_prefix(self.owner, 2, "graveyard")
        else:
            self.twiddle("first_untap", "witness", untap=True)
            self.library_prefix(self.foe, 1, "graveyard")
            self.state()
            self.twiddle("repeated_untap", "witness", untap=True)
            self.state()
            self.twiddle("retap", "witness", untap=False)
            self.state()
            self.cast("kill_orb", target="source")
            self.move("source", "graveyard")
            self.twiddle("after_removal", "witness", untap=True)
        self.state()

    def run(self):
        actions = {FLOOD: self.flooding, FAMILIAR: self.familiar, INK: self.kinship, ORB: self.orb,
                   CRAWL: self.crawling, SEWER: self.sewer, DROWNER: self.drowner, CHAINS: self.chains}
        actions[self.csvid]()

    def insects(self, count):
        csvid = self.rows[INSECT]["csvid"]
        for player in (0, 1):
            self.emit("assert_zone_multiplicity", player=player, zone=ZONES["battlefield"],
                      index=csvid, value=count if player == self.owner else 0)
        if count:
            for prop, expected in (("power", 1), ("toughness", 1), ("token", 1),
                                   ("color", 1 << 3), ("subtype_Insect", 1)):
                self.emit("assert_matching_permanents", prop=prop, player=self.owner, index=csvid,
                          value=expected)

    def crawling_upkeep(self, count):
        self.untap(self.owner)
        if self.owner == 0:
            self.emit("choose_trigger_source", ref="source", player=0)
        self.emit("upkeep_step", player=self.owner)
        self.emit("assert_unused_choices")
        self.emit("assert_stack")
        self.library_prefix(self.owner, 2, "graveyard")
        self.insects(count)
        self.state()

    def crawling(self):
        starts_with_lands = self.mode == "upkeep_and_external"
        library = ((MOUNTAIN, FOREST, PLAINS, BEAR, SWAMP) if starts_with_lands
                   else (BEAR, GIANT, ISLAND))
        self.deck(self.owner, library)
        self.deck(self.foe, (GIANT, MOUNTAIN, FOREST))
        self.add("own_land", ISLAND, self.owner, "battlefield")
        self.add("own_rain", RAIN, self.owner, "hand")
        if not starts_with_lands:
            self.add("foe_land", ISLAND, self.foe, "battlefield")
            self.add("other_rain", RAIN, self.owner, "hand")
        self.cast_source()
        if not starts_with_lands:
            self.cast("other_rain", target="foe_land")
            self.move("foe_land", "graveyard")
            self.insects(0)
            self.state()
        self.upkeep(self.foe)
        self.state()
        self.crawling_upkeep(int(starts_with_lands))
        self.emit("turn", player=self.owner)
        self.cast("own_rain", target="own_land")
        self.move("own_land", "graveyard")
        self.insects(1)
        self.state()
        if starts_with_lands:
            self.upkeep(self.foe)
            self.crawling_upkeep(2)

    def sewer_stats(self, chosen):
        total = sum(obj["player"] == chosen and obj["zone"] == "graveyard" for obj in self.objects.values())
        for prop in ("power", "toughness"):
            self.observe("source", prop, total)

    def sewer(self):
        chosen = self.owner if self.mode == "choose_self" else self.foe
        self.add("own_grave", FOREST, self.owner, "graveyard")
        self.add("foe_grave", BEAR, self.foe, "graveyard")
        self.add("foe_grave_extra", GIANT, self.foe, "graveyard")
        self.deck(self.owner, (ISLAND, SWAMP, BEAR))
        self.deck(self.foe, (MOUNTAIN, FOREST, GIANT))
        self.add("own_draw", DRAW, self.owner, "hand")
        self.add("foe_draw", DRAW, self.foe, "hand")
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, self.cost("source"))
        self.emit("cast", ref="source", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner)
        self.emit("choose_player", player=self.owner, value=chosen)
        self.resolve()
        self.move("source", "battlefield")
        self.observe("source", "in_play", 1)
        self.sewer_stats(chosen)
        self.state()

        for ref in ("own_draw", "foe_draw"):
            caster = self.objects[ref]["player"]
            self.fund(caster, self.cost(ref))
            self.emit("cast", ref=ref, value=1)
            self.emit("assert_stack", value=1)
            self.sewer_stats(chosen)
            self.emit("announce_spell", ref=ref)
            if caster == chosen:
                self.library_prefix(caster, 1, "graveyard")
            self.observe_library(caster)
            self.sewer_stats(chosen)
            self.resolve()
            self.move(ref, "graveyard")
            self.library_prefix(caster, 1, "hand")
            self.sewer_stats(chosen)
            self.state()

    def drowner(self):
        nonblue = self.mode == "nonblue"
        caster = self.owner if self.mode == "own_blue_paid" else self.foe
        recipient = 1 - caster
        self.add("witness", BOLT if nonblue else DRAW, caster, "hand")
        self.deck(self.owner, (FOREST, ISLAND, GIANT, SWAMP))
        self.deck(self.foe, (PLAINS, BEAR, MOUNTAIN, SWAMP))
        self.cast_source()
        pool = self.cost("witness").copy()
        if caster == self.owner:
            pool[0] += 1
        else:
            self.fund(self.owner, [1, 0, 0, 0, 0, 0])
        self.fund(caster, pool)
        if nonblue:
            self.emit("choose_player", player=caster, value=self.owner)
        self.emit("cast", ref="witness", value=1)
        self.emit("assert_stack", value=1)
        self.emit("assert_choices_consumed")
        self.mana("assert_mana", self.owner, [1, 0, 0, 0, 0, 0])
        if self.owner == 0:
            self.emit("choose_trigger_source", player=0, ref="source")
        self.emit("choose_player", player=self.owner, value=recipient)
        self.emit("announce_spell", ref="witness")
        unused = (2 if self.owner == 0 else 1) if nonblue else 0
        self.emit("assert_unused_choices", value=unused)
        self.resolve()
        self.move("witness", "graveyard")
        if nonblue:
            self.life[self.owner] -= 3
            self.remaining_mana[self.owner] = [1, 0, 0, 0, 0, 0]
        else:
            self.library_prefix(recipient, 2, "graveyard")
            self.library_prefix(caster, 1, "hand")
        self.state()

    def chains(self):
        first_then_additional = self.mode == "first_and_additional"
        has_discard = self.mode == "outside_with_discard"
        caster = self.owner if has_discard else self.foe
        self.deck(self.owner, (MOUNTAIN, GIANT, PLAINS))
        self.deck(self.foe, (FOREST, ISLAND, SWAMP))
        self.add("draw_spell", DRAW, caster, "hand")
        if has_discard:
            self.add("discardable", BEAR, caster, "hand")
        self.cast_source()
        self.state()
        if first_then_additional:
            first = self.library[caster][0]
            self.upkeep(caster)
            self.draw(caster)
            self.state()

        # The only eligible card is independently known before resolution.
        self.emit("discard_policy", value=1)
        self.cast("draw_spell")
        if has_discard or first_then_additional:
            self.move("discardable" if has_discard else first, "graveyard")
            self.library_prefix(caster, 1, "hand")
        else:
            self.library_prefix(caster, 1, "graveyard")
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
