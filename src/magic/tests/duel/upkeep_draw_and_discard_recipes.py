"""Upkeep draw, discard, and accompanying rules scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "upkeep_draw_and_discard"
PROFILE = Path(__file__).with_name("data") / "upkeep_draw_and_discard_wave8_profiles.json"
PIT, BRINGER, CETA, CARE = 3341, 7021, 5343, 6093
MODES = {
    PIT: ("both_upkeeps", "empty_rival", "mixed_random"),
    BRINGER: ("printed_trample", "rainbow_draw", "short_rainbow", "decline"),
    CETA: ("one_green", "dual_colors", "rival_colors", "one_dual_permanent"),
    CARE: ("five", "six", "cross_by_damage"),
}
SINGLE = {(BRINGER, "decline")}
PREFIX = "UpkeepDrawAndDiscardRecipe"
SEED = 3686
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP = "Forest", "Island", "Mountain", "Plains", "Swamp"
BOLT, BEAR, BOOK, GIANT = "Lightning Bolt", "Grizzly Bears", "Spellbook", "Hill Giant"
DUAL = "Ghor-Clan Rampager"
WITNESSES = {FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP, BOLT, BEAR, BOOK, GIANT, DUAL}


LETHE, EPHARA, GIGA, GRAVE, ONI, MACHINE, NATH = 9943, 13692, 6152, 5573, 7222, 509, 9219
MISTS, OATH, STAFF, CRUELTY, FEROCITY, ZEDRUU, BEASTMASTER = 6737, 3526, 12623, 12492, 12493, 11766, 14503
FOLLOWERS = (LETHE, EPHARA, GIGA, GRAVE, ONI, MACHINE, NATH, MISTS, OATH, STAFF, CRUELTY, FEROCITY, ZEDRUU, BEASTMASTER)
MODES.update({
    LETHE: ("both_discard", "empty_active"),
    EPHARA: ("last_friendly_entry", "foreign_entry", "devotion_indestructible"),
    GIGA: ("shroud_recover", "no_payment", "decline"),
    GRAVE: ("no_card", "exile_instead"),
    ONI: ("rival_ogre_trample", "own_ogre"),
    MACHINE: ("unpaid", "discard", "damage_prevented"),
    NATH: ("random_upkeep", "external_discard", "decline_upkeep"),
    MISTS: ("each_active", "empty_active"),
    OATH: ("fewer", "tied", "decline"),
    STAFF: ("draw_and_ping", "creature_ping"),
    CRUELTY: ("tied_power", "smaller_power", "no_creatures"),
    FEROCITY: ("tied_power", "smaller_power", "no_creatures"),
    ZEDRUU: ("donate", "ownership"),
    BEASTMASTER: ("greater_toughness", "tied_toughness", "smaller_toughness"),
})
SINGLE |= {(GIGA, "decline"), (MACHINE, "discard"), (NATH, "decline_upkeep"), (OATH, "decline")}
SPIDER, MURDER, WRATH, PURITY = "Giant Spider", "Murder", "Wrath of God", "Purity"
AIR, KNIGHT, HAWK = "Air Elemental", "White Knight", "Suntail Hawk"
OGRE, MIND_ROT, ELF, GOBLIN = "Ogre Taskmaster", "Mind Rot", "Elf Warrior", "Mons's Goblin Raiders"
WITNESSES |= {SPIDER, MURDER, WRATH, PURITY, AIR, KNIGHT, HAWK, OGRE, MIND_ROT, ELF, GOBLIN}
ZONE_NAMES = {value: name for name, value in ZONES.items()}
GAPS = {
    GIGA: ["Human optional refusal is tested; AI voluntary refusal with a card available is unobserved."],
    GRAVE: ["The opponent's exile election and draw payoff are tested. The engine directly draws and exposes no controller draw-refusal menu."],
    MACHINE: ["The duel fixture cannot declare a band or assign a creature's damage using Banding."],
    NATH: ["The duel has one possible opponent. The paired optional trigger branches use the actual human trigger chooser or the existing AI decision."],
    OATH: ["Hand threshold and the optional choice are exercised on the source controller's upkeep. Cross-controller choice attribution is unobserved."],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get("layout") != "normal" or record.get("selected_face")
            or record.get("card_faces") or "upkeep" not in record.get("oracle_text", "")):
        raise ValueError("whole pinned upkeep draw and discard record required")
    return dict(csvid=csvid, printed_mana=record["mana_cost"], rules=record["oracle_text"])


def variants(shape):
    return [f"{mode}_{seat}" for mode in MODES[shape["csvid"]]
            for seat in ((0,) if (shape["csvid"], mode) in SINGLE else (0, 1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError("distinct supported upkeep draw and discard scenarios required")


def spec(declaration, record):
    source = profiles()["sources"].get(record.get("name"))
    if not source or not exact(source["oracle"], record):
        raise ValueError("exact pinned upkeep draw and discard primary required")
    shape = classify(record, source["registry"]["csvid"])
    if (declaration.get("family") != FAMILY or type(declaration.get("version")) is not int
            or declaration["version"] != 2 or declaration.get("oracle_text") != record["oracle_text"]
            or not exact(declaration.get("params"), shape)):
        raise ValueError("versioned whole upkeep draw and discard declaration required")
    validate_variants(shape, declaration.get("variants"))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {"sources", "witnesses"} or set(pins["witnesses"]) != WITNESSES:
        raise ValueError("independent exact upkeep draw and discard witnesses required")
    if {pin["registry"]["csvid"] for pin in pins["sources"].values()} != set(MODES):
        raise ValueError("exact upkeep draw and discard primaries required")
    for group in ("sources", "witnesses"):
        for name, pin in pins[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin["oracle"]) or not exact(row, pin["registry"])
                    or row.get("kind") != "card" or oracle[name].get("layout") != ("token" if name == ELF else "normal")):
                raise ValueError("changed entry source or witness: " + name)
            if group == "sources":
                if not str(row.get("handler", "")).startswith("manalinkeh_card_"):
                    raise ValueError("implemented entry primary required")
                classify(oracle[name], row["csvid"])


class Board:
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        self.csvid, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.life = [20, 20]
        self.library = [[], []]
        self.add("source", name, owner, "hand")

    def add(self, ref, name, seat, zone, *, tapped=False, opponent_owned=False):
        fixture = card(name, ref, seat, zone)
        if tapped:
            fixture["state"] = ["tapped"]
        if opponent_owned:
            fixture.setdefault("state", []).append("opponent_owned")
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
            values.update(player=player, zone=ZONE_NAMES[zone], index=index)
        elif op in {"choose_trigger_source", "choose_player", "choose_color", "assert_matching_permanents", "assert_hand_graveyard_total",
                    "bind_permanent_copy", "bind_hand_copy", "choose_trigger", "choose_zone_card", "choose_optional_zone", "choose_optional_target", "assert_shuffle_count", "assert_offered_card"}:
            values["player"] = player
        if op in {"bind_permanent_copy", "bind_hand_copy", "assert_offered_card"}:
            values["index"] = index
        if op in {"choose_trigger_source", "choose_optional_target"}:
            values.pop("value")
        self.steps.append(step(op, **values))

    def mana(self, op, seat, values=ZERO):
        for color, value in enumerate(values):
            self.emit(op, player=seat, value=value, color=color)

    def fund(self, seat, values):
        self.mana("set_mana", seat, values)
        self.emit("refresh")

    def cost(self, ref):
        record = self.oracle[self.objects[ref]["name"]]
        return cost_from_oracle(record)

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
            if stat in self.oracle[self.name]:
                self.observe("source", stat, int(self.oracle[self.name][stat]))

    def receipts(self, pools=(ZERO, ZERO), *, animated_source=False):
        for seat in (0, 1):
            self.emit("assert_life", player=seat, value=self.life[seat])
            self.mana("assert_mana", seat, pools[seat])
            objects = [obj for obj in self.objects.values() if obj["player"] == seat]
            counts = Counter((obj["name"], obj["zone"]) for obj in objects)
            for name in sorted({obj["name"] for obj in objects}):
                for zone, code in ZONES.items():
                    if animated_source and seat == self.owner and name == self.name and zone == "battlefield":
                        # The named ref and its stats observe Ephara while the engine uses its Incarnate CSVID.
                        continue
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

    def start_combat(self, *, actor, attacker):
        self.emit("advance_turn")
        if actor == self.owner:
            self.emit("advance_turn")
        self.emit("turn", player=actor)
        self.emit("begin_attackers")
        self.emit("attack", ref=attacker, value=1)
        self.observe(attacker, "attacking", 1)

    def declaration_end(self):
        self.emit("finish_attackers")
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_unused_choices")
        self.emit("assert_stack")

    def complete_combat(self, *, attacker, blocker=None, legal=False, prevent_damage=False, pre_block=None):
        self.emit("begin_blockers")
        if blocker is not None:
            self.emit("block", ref=blocker, other=attacker, value=int(legal))
        if pre_block is not None:
            pre_block()
        self.emit("advance_combat")
        self.emit("assert_unused_choices")
        self.observe(attacker, "blocked", int(legal))
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")

        attacking = self.objects[attacker]
        power = int(self.oracle[attacking["name"]]["power"])
        if not legal:
            if not prevent_damage:
                self.life[1 - attacking["player"]] -= power
            return
        blocking = self.objects[blocker]
        block_power = int(self.oracle[blocking["name"]]["power"])
        for ref, amount in ((blocker, power), (attacker, block_power)):
            toughness = int(self.oracle[self.objects[ref]["name"]]["toughness"])
            if amount >= toughness:
                self.move(ref, "graveyard")
            else:
                self.observe(ref, "damage", amount)

    def pools(self, owner_pool=ZERO):
        return (owner_pool, ZERO) if self.owner == 0 else (ZERO, owner_pool)

    def deck(self, player, cards):
        for index, card in enumerate(cards):
            self.add(f"library_{player}_{index}", card, player, "library")

    def assert_top(self, player):
        self.emit("assert_zone", player=player, zone=ZONES["library"], value=len(self.library[player]))
        for index, ref in enumerate(self.library[player]):
            self.emit("assert_zone_card", player=player, zone=ZONES["library"], index=index,
                      value=self.rows[self.objects[ref]["name"]]["csvid"])

    def assert_zone_ref(self, ref, zone, amount):
        obj = self.objects[ref]
        self.emit("assert_zone_multiplicity", player=obj["player"], zone=ZONES[zone],
                  index=self.rows[obj["name"]]["csvid"], value=amount)

    def draw(self, player, count):
        assert len(self.library[player]) >= count
        for ref in self.library[player][:count]:
            self.move(ref, "hand")

    def state(self, owner_pool=ZERO, *, animated_source=False):
        self.receipts(self.pools(owner_pool), animated_source=animated_source)
        for player in (0, 1):
            self.assert_top(player)

    def pending_cast(self, ref, *, player, target=None, response=False, fee_ref=None):
        self.fund(player, self.cost(fee_ref or ref))
        if target is not None:
            self.emit("choose_target", ref=target, player=player)
        self.emit("cast_response" if response else "cast", ref=ref, value=1)
        self.emit("assert_stack", value=2 if response else 1)
        self.mana("assert_mana", player)

    def begin_source(self):
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.pending_cast("source", player=self.owner)
        self.assert_top(self.owner)
        self.assert_top(self.foe)

    def complete_source(self):
        self.resolve()
        self.move("source", "battlefield")
        self.inspect_source()





    def prepare_upkeep(self, player):
        self.emit("advance_turn")
        self.emit("untap_step", player=player)

    def upkeep(self, player):
        self.emit("upkeep_step", player=player)
        self.resolve()

    def pit(self):
        empty_rival = self.mode == "empty_rival"
        mixed = self.mode == "mixed_random"
        self.deck(self.owner, (ISLAND, PLAINS))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        for seat in (0, 1):
            if empty_rival and seat == self.foe:
                continue
            for index, name in enumerate((BOOK, FOREST) if mixed else (BOOK, BOOK)):
                self.add(f"discard_{seat}_{index}", name, seat, "hand")
        self.begin_source()
        self.complete_source()
        self.state()
        for seat in (self.foe, self.owner):
            self.prepare_upkeep(seat)
            self.upkeep(seat)
            if mixed:
                for player in (0, 1):
                    acted = int(player == seat or seat == self.owner)
                    self.emit("assert_zone", player=player, zone=ZONES["hand"], value=2 - acted)
                    self.emit("assert_zone", player=player, zone=ZONES["graveyard"], value=acted)
                    for name in (BOOK, FOREST):
                        self.emit("assert_hand_graveyard_total", player=player,
                                  index=self.rows[name]["csvid"], value=1)
                    self.assert_top(player)
                continue
            if not empty_rival or seat == self.owner:
                self.move(f"discard_{seat}_0", "graveyard")
            self.state()

    def bringer(self):
        trample = self.mode == "printed_trample"
        short = self.mode == "short_rainbow"
        decline = self.mode == "decline"
        library = (ISLAND, FOREST, PLAINS, MOUNTAIN)
        if self.owner == 1 and not (short or decline):
            library += (SWAMP, MOUNTAIN, PLAINS) * 3
        self.deck(self.owner, library)
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        if trample:
            self.add("blocker", GIANT, self.foe, "battlefield")
        fee = [7, 0, 2, 0, 0, 0] if trample else [0, 1, 1, int(not short), 1 + int(short), 1]
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, fee)
        self.emit("cast", ref="source", value=int(not short))
        self.emit("assert_stack", value=int(not short))
        if short:
            self.state(fee)
            self.fund(self.owner, ZERO)
        else:
            self.mana("assert_mana", self.owner)
            self.complete_source()
            self.state()
        self.prepare_upkeep(self.foe)
        self.upkeep(self.foe)
        self.state()
        self.prepare_upkeep(self.owner)
        accepts = not (decline or short)
        if accepts and self.owner == 0:
            self.emit("choose_trigger_source", ref="source", player=0)
        self.upkeep(self.owner)
        if accepts:
            self.draw(self.owner, 2)
        self.state()
        if not trample:
            return
        self.emit("turn", player=self.owner)
        self.emit("begin_attackers")
        self.emit("attack", ref="source", value=1)
        self.declaration_end()
        self.emit("begin_blockers")
        self.emit("block", ref="blocker", other="source", value=1)
        self.emit("advance_combat")
        self.observe("source", "blocked", 1)
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")
        self.move("blocker", "graveyard")
        self.life[self.foe] -= 2
        self.observe("source", "damage", 3)
        self.state()

    def ceta(self):
        hostile = self.mode == "rival_colors"
        dual = self.mode == "one_dual_permanent"
        both = self.mode == "dual_colors"
        self.deck(self.owner, (BOOK, BOOK, ISLAND, FOREST))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        holder = self.foe if hostile else self.owner
        if dual:
            self.add("two_colors_one", DUAL, holder, "battlefield")
        else:
            self.add("green", BEAR, holder, "battlefield")
            if both or hostile:
                self.add("red", GIANT, holder, "battlefield")
        self.begin_source()
        self.complete_source()
        assert all(entry["zone"] != "hand" for entry in self.objects.values())
        self.state()
        self.prepare_upkeep(self.foe)
        self.upkeep(self.foe)
        self.state()
        self.prepare_upkeep(self.owner)
        count = 0 if hostile else 2 if both or dual else 1
        if self.owner == 0 and count:
            self.emit("discard_policy", value=1)
        self.upkeep(self.owner)
        if self.owner == 0 and count:
            self.emit("discard_policy")
        if count:
            first = self.library[self.owner][0]
            self.draw(self.owner, count)
            self.move(first, "graveyard")
        self.state()

    def care(self):
        move_life = self.mode == "cross_by_damage"
        self.life[self.owner] = 8 if move_life else 5 if self.mode == "five" else 6
        self.life[self.foe] = 5
        self.deck(self.owner, (ISLAND, FOREST, PLAINS))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        if move_life:
            self.add("crossing", BOLT, self.foe, "hand")
        for player in (0, 1):
            self.emit("set_life", player=player, value=self.life[player])
        self.begin_source()
        self.complete_source()
        self.state()
        self.prepare_upkeep(self.foe)
        self.upkeep(self.foe)
        self.state()
        if move_life:
            self.emit("turn", player=self.foe)
            self.emit("choose_player", player=self.foe, value=self.owner)
            self.pending_cast("crossing", player=self.foe)
            self.resolve()
            self.move("crossing", "graveyard")
            self.life[self.owner] -= 3
            self.state()
        self.prepare_upkeep(self.owner)
        self.upkeep(self.owner)
        if self.life[self.owner] <= 5:
            self.life[self.owner] += 3
            self.draw(self.owner, 1)
        self.state()
        for seat in (self.foe, self.owner):
            self.prepare_upkeep(seat)
            self.upkeep(seat)
        self.state()


    def begin_standard(self, *, inspect=True, animated_source=False):
        self.begin_source()
        if inspect:
            self.complete_source()
        else:
            self.resolve()
            self.move("source", "battlefield")
            self.observe("source", "in_play", 1)
        self.state(animated_source=animated_source)

    def deck_pair(self, *, long=False):
        self.deck(self.owner, (ISLAND, FOREST, PLAINS, MOUNTAIN, SWAMP) * (4 if long else 1))
        self.deck(self.foe, (MOUNTAIN, SWAMP, PLAINS))

    def upkeep_cycle(self, action):
        for active in (self.foe, self.owner):
            self.prepare_upkeep(active)
            action(active)
            self.upkeep(active)
            self.state()

    def finish_spell(self, ref, actor, *, target=None, destination="graveyard"):
        self.pending_cast(ref, player=actor, target=target)
        self.resolve()
        self.move(ref, destination)

    def generic_discard(self):
        empty = self.mode == "empty_active"
        self.deck_pair()
        for player in (0, 1):
            if not empty or player != self.owner:
                self.add(f"held_{player}", BOOK, player, "hand")
        self.begin_standard()
        self.emit("discard_policy", value=1)

        def expected(active):
            victims = ((active,) if self.csvid == MISTS else
                       (self.owner, self.foe) if active == self.owner else ())
            for player in victims:
                ref = f"held_{player}"
                if ref in self.objects and self.objects[ref]["zone"] == "hand":
                    self.move(ref, "graveyard")

        self.upkeep_cycle(expected)
        self.emit("discard_policy")

    def ephara(self):
        high = self.mode == "devotion_indestructible"
        birth = self.mode != "foreign_entry"
        self.deck_pair()
        self.add("bear", BEAR, self.owner if birth else self.foe, "hand")
        self.add("murder", MURDER, self.foe, "hand")
        if high:
            for ref, name in (("air", AIR), ("knight", KNIGHT), ("hawk", HAWK)):
                self.add(ref, name, self.owner, "battlefield")
            self.add("wrath", WRATH, self.owner, "hand")
        self.begin_standard(inspect=high, animated_source=high)
        self.emit("can_target_creature", ref="murder", other="source", value=int(high))
        if high:
            self.finish_spell("wrath", self.owner)
            for ref in ("air", "knight", "hawk"):
                self.move(ref, "graveyard")
            self.emit("can_target_creature", ref="murder", other="source")
            self.observe("source", "in_play", 1)
        if birth:
            self.finish_spell("bear", self.owner, destination="battlefield")
            self.emit("can_target_creature", ref="murder", other="bear", value=1)
        self.prepare_upkeep(self.foe)
        self.upkeep(self.foe)
        if birth:
            self.draw(self.owner, 1)
        else:
            self.emit("turn", player=self.foe)
            self.finish_spell("bear", self.foe, destination="battlefield")
        self.state()
        self.prepare_upkeep(self.owner)
        self.upkeep(self.owner)
        self.state()

    def gigapede(self):
        has_payment = self.mode != "no_payment"
        pay = self.mode == "shroud_recover"
        self.deck_pair()
        self.add("wrath", WRATH, self.owner, "hand")
        self.add("murder", MURDER, self.foe, "hand")
        self.add("target_control", BEAR, self.owner, "battlefield")
        if has_payment:
            self.add("held", BOOK, self.owner, "hand")
        self.begin_standard()
        self.emit("can_target_creature", ref="murder", other="source")
        self.emit("can_target_creature", ref="murder", other="target_control", value=1)
        self.finish_spell("wrath", self.owner)
        self.move("source", "graveyard")
        self.move("target_control", "graveyard")
        self.state()

        def upkeep_choices(active):
            if active != self.owner or not has_payment:
                return
            self.emit("choose_dialog", player=active, value=int(not pay))
            if active == 0:
                self.emit("discard_policy", value=1)
            if pay:
                self.move("held", "graveyard")
                self.move("source", "hand")

        self.upkeep_cycle(upkeep_choices)
        self.emit("discard_policy")

    def gravestorm(self):
        exile = self.mode == "exile_instead"
        self.deck_pair()
        self.add("own_corpse", BEAR, self.owner, "graveyard")
        if exile:
            self.add("foe_corpse", BOOK, self.foe, "graveyard")
        self.begin_standard()

        def expected(active):
            if active == self.owner:
                if exile:
                    self.emit("choose_zone_card", player=self.foe, value=self.rows[BOOK]["csvid"])
                    self.move("foe_corpse", "exile")
                else:
                    self.draw(self.owner, 1)

        self.upkeep_cycle(expected)

    def oni(self):
        protect = self.mode == "own_ogre"
        self.deck_pair()
        self.add("ogre", OGRE, self.owner if protect else self.foe, "battlefield")
        self.add("held", BOOK, self.owner, "hand")
        if not protect:
            self.add("blocker", BEAR, self.foe, "battlefield")
        self.begin_standard()
        self.emit("discard_policy", value=1)

        def expected(active):
            if active == self.owner and not protect:
                self.move("held", "graveyard")

        self.upkeep_cycle(expected)
        self.emit("discard_policy")
        if not protect:
            self.emit("turn", player=self.owner)
            self.emit("begin_attackers")
            self.emit("attack", ref="source", value=1)
            self.declaration_end()
            self.complete_combat(attacker="source", blocker="blocker", legal=True)
            self.life[self.foe] -= 3
            self.state()

    def machine(self):
        pay = self.mode == "discard"
        prevented = self.mode == "damage_prevented"
        self.deck_pair()
        if pay:
            self.add("held", BOOK, self.owner, "hand")
        if prevented:
            self.add("prevention", PURITY, self.owner, "battlefield")
        self.begin_standard()

        def expected(active):
            if active == self.owner:
                if pay:
                    self.emit("choose_dialog", player=0)
                    self.emit("discard_policy", value=1)
                    self.move("held", "graveyard")
                else:
                    self.life[self.owner] += 3 if prevented else -3

        self.upkeep_cycle(expected)
        self.observe("source", "tapped", int(not pay and not prevented))
        self.emit("discard_policy")

    def token_receipt(self, count):
        self.emit("assert_zone_multiplicity", player=self.owner, zone=ZONES["battlefield"],
                  index=self.rows[ELF]["csvid"], value=count)
        if count:
            for prop, value in (("power", 1), ("toughness", 1), ("color", 8),
                                ("subtype_Elf", 1), ("subtype_Warrior", 1)):
                self.emit("assert_matching_permanents", player=self.owner,
                          index=self.rows[ELF]["csvid"], prop=prop, value=value)

    def nath(self):
        outside = self.mode == "external_discard"
        decline = self.mode == "decline_upkeep"
        self.deck_pair()
        self.add("held", BOOK, self.foe, "hand")
        if outside:
            self.add("rot", MIND_ROT, self.owner, "hand")
        self.begin_standard()
        self.token_receipt(0)
        self.emit("discard_policy", value=1)
        if outside:
            self.emit("choose_player", player=self.owner, value=self.foe)
            if self.owner == 0:
                self.emit("choose_trigger_source", ref="source", player=0)
            self.finish_spell("rot", self.owner)
            self.move("held", "graveyard")
        else:
            self.prepare_upkeep(self.foe)
            self.upkeep(self.foe)
            self.state()
            self.token_receipt(0)
            self.prepare_upkeep(self.owner)
            if self.owner == 0 and not decline:
                self.emit("choose_trigger_source", ref="source", player=0)
                self.emit("choose_trigger_source", ref="source", player=0)
            self.upkeep(self.owner)
            if not decline:
                self.move("held", "graveyard")
        self.state()
        self.token_receipt(int(not decline))
        self.emit("discard_policy")

    def oath(self):
        tied = self.mode == "tied"
        decline = self.mode == "decline"
        self.deck_pair(long=True)
        self.add("held", BOOK, self.owner, "hand")
        for index in range(1 if tied else 3):
            self.add(f"other_held_{index}", FOREST, self.foe, "hand")
        self.begin_standard()

        def expected(active):
            if active == self.owner and not tied:
                self.emit("choose_dialog", player=self.owner, value=int(decline))
                if not decline:
                    if self.owner == 0:
                        self.emit("discard_policy", value=1)
                    self.move("held", "graveyard")
                    self.draw(self.owner, 3)

        self.upkeep_cycle(expected)

    def staff(self):
        creature = self.mode == "creature_ping"
        self.deck_pair()
        if creature:
            self.add("victim", GOBLIN, self.foe, "battlefield")
        self.begin_standard()
        self.upkeep_cycle(lambda active: self.draw(self.owner, 1) if active == self.owner else None)
        self.emit("turn", player=self.owner)
        self.emit("can_activate", ref="source", value=1)
        if creature:
            self.emit("choose_target", ref="victim", player=self.owner)
        else:
            self.emit("choose_player", player=self.owner, value=self.foe)
        self.emit("activate", ref="source", value=1)
        self.resolve()
        if creature:
            self.move("victim", "graveyard")
        else:
            self.life[self.foe] -= 1
        self.observe("source", "tapped", 1)
        self.emit("can_activate", ref="source")
        self.state()

    def compare(self):
        none = self.mode == "no_creatures"
        payoff = self.mode in {"tied_power", "tied_toughness", "greater_toughness"}
        is_toughness = self.csvid == BEASTMASTER
        discard = self.csvid == CRUELTY
        self.deck_pair()
        if discard:
            self.add("held", BOOK, self.foe, "hand")
        if not none:
            if self.mode == "greater_toughness":
                own, rival = SPIDER, GIANT
            elif self.mode == "tied_toughness":
                own, rival = SPIDER, AIR
            elif is_toughness:
                own, rival = GIANT, SPIDER
            else:
                own, rival = BEAR, BEAR if payoff else GIANT
            self.add("own_stat", own, self.owner, "battlefield")
            self.add("rival_stat", rival, self.foe, "battlefield")
        self.begin_standard()
        if discard:
            self.emit("discard_policy", value=1)

        def expected(active):
            if active == self.owner and payoff:
                if discard:
                    self.move("held", "graveyard")
                else:
                    self.draw(self.owner, 1)

        self.upkeep_cycle(expected)
        if discard:
            self.emit("discard_policy")

    def zedruu(self):
        donate = self.mode == "donate"
        self.deck_pair()
        self.add("donated", BOOK, self.owner if donate else self.foe, "battlefield",
                 opponent_owned=not donate)
        self.add("borrowed", BEAR, self.owner, "battlefield", opponent_owned=True)
        self.add("rival_own", GIANT, self.foe, "battlefield")
        if not donate:
            self.add("also_owned", HAWK, self.foe, "battlefield", opponent_owned=True)
        self.begin_standard()
        if donate:
            self.fund(self.owner, [0, 0, 1, 0, 1, 1])
            self.emit("can_activate", ref="source", value=1)
            self.emit("choose_target", ref="donated", player=self.owner)
            self.emit("activate", ref="source", value=1)
            self.resolve()
            self.objects["donated"]["player"] = self.foe
            self.state()

        def expected(active):
            if active == self.owner:
                self.draw(self.owner, 1 if donate else 2)
                self.life[self.owner] += 1 if donate else 2

        self.upkeep_cycle(expected)

    def run(self):
        scenarios = {
            PIT: self.pit, BRINGER: self.bringer, CETA: self.ceta, CARE: self.care,
            LETHE: self.generic_discard, EPHARA: self.ephara, GIGA: self.gigapede,
            GRAVE: self.gravestorm, ONI: self.oni, MACHINE: self.machine,
            NATH: self.nath, MISTS: self.generic_discard, OATH: self.oath,
            STAFF: self.staff, CRUELTY: self.compare, FEROCITY: self.compare,
            ZEDRUU: self.zedruu, BEASTMASTER: self.compare,
        }
        scenarios[self.csvid]()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()["sources"][name]["registry"]["csvid"] != csvid:
        raise ValueError("exact registered upkeep draw and discard primary CSVID required")
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
