"""Entry triggers and card selection scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "remaining_entry_library_and_hand"
PROFILE = Path(__file__).with_name("data") / "remaining_entry_library_and_hand_wave8_profiles.json"
EXARCH, EARWIG, SIFTER, SHAMAN = 11572, 9349, 14464, 8231
MODES = {
    EXARCH: ("search_auto", "search_modal", "bottom_borrowed"),
    EARWIG: ("paid_prowl", "ordinary_after_hit", "wrong_type_short"),
    SIFTER: ("both_return_two", "both_partial", "kindred_goblin"),
    SHAMAN: ("all_own", "all_foe", "partial_foe", "decline_own"),
}
SINGLE = {(SHAMAN, "partial_foe"), (SHAMAN, "decline_own")}
PREFIX = "RemainingEntryLibraryAndHandRecipe"
SEED = 3500
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP = "Forest", "Island", "Mountain", "Plains", "Swamp"
BOLT, BEAR, BOOK, GIANT = "Lightning Bolt", "Grizzly Bears", "Spellbook", "Hill Giant"
STINGER, GOBLIN, TARFIRE = "Nightshade Stinger", "Mons's Goblin Raiders", "Tarfire"
WITNESSES = {FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP, BOLT, BEAR, BOOK, GIANT, STINGER, GOBLIN, TARFIRE}



HEAVY, HORIZONS, SCEPTER, CITY, NULLTREAD, SPRITE = 9326, 9777, 8388, 12811, 10326, 9283
TRENCH, DISMISSER, MIRE, GROTESQUE, FELL = 11764, 7997, 15378, 1316, 18919
FOLLOWERS = (HEAVY, HORIZONS, SCEPTER, CITY, NULLTREAD, SPRITE, TRENCH, DISMISSER, MIRE, GROTESQUE, FELL)
MODES.update({
    HEAVY: ("foe_search_trample", "foe_decline"),
    HORIZONS: ("return_upkeep", "zero_selection", "decline_upkeep"),
    SCEPTER: ("matching_counter", "mismatching_cost"),
    CITY: ("last_spell_extra_turn", "opposing_spell", "matching_land"),
    NULLTREAD: ("borrowed_creature", "self_top"),
    SPRITE: ("scaled_reveal", "flying_single_reveal"),
    TRENCH: ("two_lands_trample", "decline", "accept_zero"),
    DISMISSER: ("borrowed_creature", "opponent_creature"),
    MIRE: ("recover_and_mana", "decline_and_mana"),
    GROTESQUE: ("black_paid", "nonblack_flying"),
    FELL: ("discard_entry", "actual_life_loss", "own_discard"),
})
SINGLE |= {(HORIZONS, "decline_upkeep"), (SCEPTER, "mismatching_cost"), (CITY, "matching_land"), (TRENCH, "decline")}
HAWK, ARMOR, MIND_ROT = "Suntail Hawk", "Urza's Armor", "Mind Rot"
WITNESSES |= {HAWK, ARMOR, MIND_ROT}
ZONE_NAMES = {code: name for name, code in ZONES.items()}
GAPS = {
    SHAMAN: ["The AI callback does not offer a per-card choice. AI discretionary partial selection is unobserved."],
    HORIZONS: ["The human declines the optional upkeep. AI strategic decline is unobserved."],
    SCEPTER: ["The headless archive exposes card identities for assertions. Public/private presentation is unobserved. The AI callback only offers matching payments."],
    CITY: ["The matching-land choice is scripted for the human. It distinguishes play from spell cast."],
    TRENCH: ["The human outer decline is exercised. AI strategic decline is unobserved."],
}



@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get("layout") != "normal" or record.get("selected_face")
            or record.get("card_faces") or " enters" not in record.get("oracle_text", "")):
        raise ValueError("whole pinned entry cards and selection record required")
    return dict(csvid=csvid, printed_mana=record["mana_cost"], rules=record["oracle_text"])


def variants(shape):
    return [f"{mode}_{seat}" for mode in MODES[shape["csvid"]]
            for seat in ((0,) if (shape["csvid"], mode) in SINGLE else (0, 1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError("distinct supported entry cards and selection scenarios required")


def spec(declaration, record):
    source = profiles()["sources"].get(record.get("name"))
    if not source or not exact(source["oracle"], record):
        raise ValueError("exact pinned entry cards and selection primary required")
    shape = classify(record, source["registry"]["csvid"])
    if (declaration.get("family") != FAMILY or type(declaration.get("version")) is not int
            or declaration["version"] != 2 or declaration.get("oracle_text") != record["oracle_text"]
            or not exact(declaration.get("params"), shape)):
        raise ValueError("versioned whole entry cards and selection declaration required")
    validate_variants(shape, declaration.get("variants"))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {"sources", "witnesses"} or set(pins["witnesses"]) != WITNESSES:
        raise ValueError("independent exact entry cards and selection witnesses required")
    if {pin["registry"]["csvid"] for pin in pins["sources"].values()} != set(MODES):
        raise ValueError("exact entry cards and selection primaries required")
    for group in ("sources", "witnesses"):
        for name, pin in pins[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin["oracle"]) or not exact(row, pin["registry"])
                    or row.get("kind") != "card" or oracle[name].get("layout") != "normal"):
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
        elif op in {"choose_trigger_source", "choose_player", "choose_color", "assert_matching_permanents",
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

    def state(self, owner_pool=ZERO):
        self.receipts(self.pools(owner_pool))
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




    def zone_choice(self, player, name):
        self.emit("choose_zone_card", player=player, value=self.rows[name]["csvid"])

    def shuffled(self, player):
        for side in (0, 1):
            self.emit("assert_shuffle_count", player=side, value=int(side == player))

    def unordered(self, shuffled_player):
        self.receipts()
        self.emit("assert_zone", player=shuffled_player, zone=ZONES["library"], value=len(self.library[shuffled_player]))
        self.assert_top(1 - shuffled_player)

    def exarch(self):
        bottom = self.mode == "bottom_borrowed"
        modal = self.mode != "search_auto"
        self.deck(self.owner, (FOREST, BEAR, BOOK))
        self.deck(self.foe, (ISLAND, PLAINS))
        self.add("excluded_creature", GIANT, self.foe, "battlefield")
        if modal:
            self.add("artifact", BOOK, self.owner if bottom else self.foe, "battlefield",
                     opponent_owned=bottom)
        self.begin_source()
        if modal or self.owner == 1:
            self.emit("choose_dialog", player=self.owner, value=int(bottom))
        if bottom:
            self.emit("choose_target", player=self.owner, ref="artifact")
        else:
            self.zone_choice(self.owner, BEAR)
        self.complete_source()
        if bottom:
            self.objects["artifact"]["player"] = self.foe
            self.move("artifact", "library")
            self.library[self.foe].append("artifact")
            self.shuffled(None)
            self.state()
        else:
            self.shuffled(self.owner)
            self.unordered(self.owner)
            self.emit("assert_zone_card", player=self.owner, zone=ZONES["library"],
                      index=0, value=self.rows[BEAR]["csvid"])

    def earwig(self):
        prowl = self.mode == "paid_prowl"
        normal = self.mode == "ordinary_after_hit"
        eligible = prowl or normal
        self.deck(self.owner, (FOREST, PLAINS))
        self.deck(self.foe, (ISLAND, BOOK, BEAR, MOUNTAIN, BOLT))
        self.add("hitter", STINGER if eligible else BEAR, self.owner, "battlefield")
        self.emit("turn", player=self.owner)
        self.fund(self.owner, ZERO)
        self.fund(self.foe, ZERO)
        self.start_combat(actor=self.owner, attacker="hitter")
        self.declaration_end()
        self.complete_combat(attacker="hitter")
        self.state()
        self.emit("turn", player=self.owner)
        cost = [3, 2, 0, 0, 0, 0] if normal else [2, 1, 0, 0, 0, 0]
        self.fund(self.owner, cost)
        if normal:
            self.emit("choose_dialog", player=self.owner, value=1)
        self.emit("cast", ref="source", value=int(eligible))
        self.emit("assert_stack", value=int(eligible))
        self.mana("assert_mana", self.owner, ZERO if eligible else cost)
        if not eligible:
            self.shuffled(None)
            self.state(cost)
            return
        if prowl:
            for name in (BOOK, MOUNTAIN, BEAR):
                self.zone_choice(self.owner, name)
        self.complete_source()
        if prowl:
            for index in (1, 3, 2):
                self.move(f"library_{self.foe}_{index}", "exile")
            self.shuffled(self.foe)
            self.unordered(self.foe)
        else:
            self.shuffled(None)
            self.state()

    def sifter(self):
        kindred = self.mode == "kindred_goblin"
        full = self.mode == "both_return_two"
        self.deck(self.owner, (FOREST, MOUNTAIN))
        self.deck(self.foe, (ISLAND, PLAINS))
        for player, chosen in ((self.owner, GOBLIN), (self.foe, BEAR)):
            if kindred and player == self.foe:
                self.add("opposing_instant", BOLT, player, "graveyard")
                continue
            for index in (0, 1):
                self.add(f"candidate_{player}_{index}", chosen, player, "graveyard")
            self.add(f"wrong_type_{player}", GIANT, player, "graveyard")
        if kindred:
            self.add("kindred", TARFIRE, self.owner, "graveyard")
        self.begin_source()
        if kindred:
            self.emit("observe_search_filters", value=1)
        for player, chosen in ((self.owner, GOBLIN), (self.foe, BEAR)):
            if kindred and player == self.foe:
                continue
            # Auto has a unique most-common subtype among the grave cards.
            self.emit("choose_dialog", player=player)
            if kindred:
                self.zone_choice(player, TARFIRE)
                # If the engine incorrectly excludes Kindred, the first optional
                # selector declines safely and the unused stop is ignored.
                self.emit("choose_optional_zone", player=player, value=self.rows[GOBLIN]["csvid"])
            else:
                for _ in range(2 if full else 1):
                    self.zone_choice(player, chosen)
                self.emit("choose_zone", player=player, value=-1)
        self.complete_source()
        if kindred:
            self.move("kindred", "hand")
        else:
            for player in (0, 1):
                for index in range(2 if full else 1):
                    self.move(f"candidate_{player}_{index}", "hand")
        self.state()

    def shaman(self):
        target = self.foe if self.mode in ("all_foe", "partial_foe") else self.owner
        amount = {"partial_foe": 1, "decline_own": 0}.get(self.mode, 2)
        self.deck(self.owner, (FOREST, PLAINS))
        self.deck(self.foe, (ISLAND, SWAMP))
        for player in (0, 1):
            for index, name in enumerate((BOLT, BEAR)):
                self.add(f"grave_{player}_{index}", name, player, "graveyard")
        self.begin_source()
        self.emit("choose_player", player=self.owner, value=target)
        if self.owner == 0:
            for name in (BOLT, BEAR)[:amount]:
                self.zone_choice(self.owner, name)
            for _ in range(2 - amount):
                self.emit("choose_zone", player=self.owner, value=-1)
        self.complete_source()
        for index in range(amount):
            ref = f"grave_{target}_{index}"
            self.move(ref, "library")
            self.library[target].append(ref)
        self.shuffled(target)
        self.unordered(target)


    def common_decks(self):
        self.deck(self.owner, (FOREST, PLAINS, BOOK, MOUNTAIN))
        self.deck(self.foe, (ISLAND, SWAMP, BEAR))

    def actual_upkeep(self, player):
        self.emit("advance_turn")
        self.emit("untap_step", player=player)
        self.emit("upkeep_step", player=player)
        self.resolve()

    def flying_combat(self):
        self.start_combat(actor=self.owner, attacker="source")
        self.declaration_end()
        self.complete_combat(attacker="source", blocker="ground")
        self.state()

    def trample_combat(self, blocker, *, toughness, block_power, power):
        self.start_combat(actor=self.owner, attacker="source")
        self.declaration_end()
        self.emit("begin_blockers")
        self.emit("block", ref=blocker, other="source", value=1)
        self.emit("advance_combat")
        self.observe("source", "blocked", 1)
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")
        self.life[self.foe] -= power - toughness
        self.move(blocker, "graveyard")
        self.observe("source", "damage", block_power)

    def heavy(self):
        search = self.mode == "foe_search_trample"
        self.deck(self.owner, (FOREST, PLAINS))
        self.deck(self.foe, (ISLAND, GIANT, BOOK, BEAR))
        self.begin_source()
        self.emit("choose_dialog", player=self.foe, value=int(not search))
        if search:
            self.zone_choice(self.foe, GIANT)
        self.complete_source()
        self.shuffled(self.foe if search else None)
        if search:
            self.move(f"library_{self.foe}_1", "battlefield")
            self.emit("bind_permanent_copy", ref="fetched", player=self.foe, value=self.rows[GIANT]["csvid"])
            self.observe("fetched", "in_play", 1)
            self.observe("fetched", "tapped", 0)
            self.unordered(self.foe)
            self.objects["fetched"] = self.objects.pop(f"library_{self.foe}_1")
            self.trample_combat("fetched", toughness=3, block_power=3, power=8)
            self.unordered(self.foe)
        else:
            self.state()

    def horizons(self):
        get_land = self.mode != "zero_selection"
        recover = self.mode == "return_upkeep"
        self.deck(self.owner, (PLAINS, ISLAND, BOOK))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        self.begin_source()
        if get_land:
            self.zone_choice(self.owner, PLAINS)
        self.emit("choose_zone", player=self.owner, value=-1)
        self.complete_source()
        if get_land:
            self.move(f"library_{self.owner}_0", "exile")
        self.shuffled(self.owner)
        self.unordered(self.owner)
        self.actual_upkeep(self.foe)
        self.unordered(self.owner)
        if recover and self.owner == 0:
            self.emit("choose_trigger", player=0, ref="source")
            self.zone_choice(0, PLAINS)
        self.actual_upkeep(self.owner)
        if recover:
            self.move(f"library_{self.owner}_0", "hand")
        self.unordered(self.owner)

    def scepter(self):
        matches = self.mode == "matching_counter"
        self.deck(self.owner, (FOREST, PLAINS))
        self.deck(self.foe, (BOLT, BOOK, BEAR, ISLAND, SWAMP, MOUNTAIN))
        self.add("reply", BOLT, self.foe, "hand")
        self.begin_source()
        self.emit("choose_player", player=self.owner, value=self.foe)
        self.complete_source()
        for ref in self.library[self.foe][:5]:
            self.move(ref, "exile")
        self.state()
        self.emit("advance_turn")
        self.emit("turn", player=self.foe)
        self.emit("choose_player", player=self.foe, value=self.owner)
        self.pending_cast("reply", player=self.foe)
        self.emit("announce_spell", ref="reply")
        self.fund(self.owner, [2, 0, 0, 0, 0, 0])
        if self.owner == 0:
            self.emit("choose_dialog", player=0)
            self.zone_choice(0, BOLT if matches else BOOK)
        if self.owner == 0:
            self.emit("can_activate", ref="source", value=1)
        self.emit("activate_response", ref="source", value=1)
        self.emit("assert_stack", value=2)
        self.mana("assert_mana", self.owner)
        self.observe("source", "tapped", 1)
        self.move(f"library_{self.foe}_{0 if matches else 1}", "graveyard")
        self.emit("resolve_response")
        self.emit("refresh")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        self.move("reply", "graveyard")
        if not matches:
            self.life[self.owner] -= 3
        self.state()

    def city(self):
        land = self.mode == "matching_land"
        opponent = self.mode == "opposing_spell"
        actor = self.foe if opponent else self.owner
        same = FOREST if land else BOOK
        self.deck(self.owner, (same,))
        self.deck(self.foe, (ISLAND, SWAMP))
        self.add("played", same, actor, "hand")
        self.begin_source()
        self.emit("choose_zone", player=self.owner, value=-1)
        self.complete_source()
        imprint = f"library_{self.owner}_0"
        self.move(imprint, "exile")
        self.state()
        if opponent:
            self.emit("advance_turn")
            self.emit("turn", player=actor)
        if land:
            self.emit("choose_dialog", player=self.owner)
            self.emit("play_land", ref="played", value=1)
            self.emit("resolve")
            self.emit("refresh")
            self.emit("assert_unused_choices")
            self.emit("assert_stack")
        else:
            self.emit("cast", ref="played", value=1)
            self.emit("assert_stack", value=1)
            if not opponent:
                self.emit("choose_dialog", player=self.owner)
            self.emit("announce_spell", ref="played")
            if opponent:
                self.resolve()
            else:
                self.emit("resolve")
                self.emit("refresh")
                self.emit("assert_unused_choices")
                self.emit("assert_stack")
        self.move("played", "battlefield")
        if not opponent:
            self.move(imprint, "hand")
            self.move("source", "graveyard")
        self.state()
        if not opponent:
            self.emit("advance_scheduled_turn", value=self.owner)
            self.emit("advance_scheduled_turn", value=self.foe)

    def tuck_creature(self):
        self_top = self.mode == "self_top"
        borrowed = self.mode == "borrowed_creature"
        self.common_decks()
        self.add("noncreature", BOOK, self.owner, "battlefield")
        self.add("unselected", GIANT, self.foe, "battlefield")
        if not self_top:
            self.add("chosen", BEAR, self.owner if borrowed else self.foe, "battlefield",
                     opponent_owned=borrowed)
        self.begin_source()
        ref = "source" if self_top else "chosen"
        self.emit("choose_target", player=self.owner, ref=ref)
        self.resolve()
        self.move("source", "battlefield")
        player = self.owner if self_top else self.foe
        self.objects[ref]["player"] = player
        self.move(ref, "library")
        self.library[player].insert(0, ref)
        self.state()

    def sprite(self):
        scaled = self.mode == "scaled_reveal"
        self.common_decks()
        self.add("rival_faerie", STINGER, self.foe, "battlefield")
        self.add("hidden", MOUNTAIN, self.foe, "hand")
        self.add("chosen", BOOK, self.foe, "hand")
        if scaled:
            self.add("own_faerie", STINGER, self.owner, "battlefield")
            self.add("other_reveal", BOLT, self.foe, "hand")
        else:
            self.add("ground", BEAR, self.foe, "battlefield")
        self.begin_source()
        self.emit("observe_search_filters", value=1)
        self.emit("choose_player", player=self.owner, value=self.foe)
        shown = (BOLT, BOOK) if scaled else (BOOK,)
        for card in shown:
            self.zone_choice(self.foe, card)
        self.zone_choice(self.owner, BOOK)
        self.complete_source()
        self.emit("assert_offered_count", value=len(shown))
        for index, name in enumerate(shown):
            self.emit("assert_offered_card", index=index, value=self.rows[name]["csvid"])
        self.move("chosen", "graveyard")
        self.state()
        if not scaled:
            self.flying_combat()

    def trench(self):
        take = self.mode == "two_lands_trample"
        accepted = self.mode != "decline"
        self.common_decks()
        if take:
            self.add("small_flier", HAWK, self.foe, "battlefield")
        self.begin_source()
        if accepted:
            if self.owner == 0:
                self.emit("choose_trigger", ref="source", player=0)
            if take:
                for land in (FOREST, PLAINS):
                    self.zone_choice(self.owner, land)
            self.emit("choose_zone", player=self.owner, value=-1)
        self.resolve()
        self.move("source", "graveyard" if accepted and not take else "battlefield")
        if take:
            for index in (0, 1):
                self.move(f"library_{self.owner}_{index}", "exile")
            for stat in ("power", "toughness"):
                self.observe("source", stat, 2)
        elif not accepted:
            self.inspect_source()
        self.shuffled(self.owner if accepted else None)
        if accepted:
            self.unordered(self.owner)
        else:
            self.state()
        if take:
            self.trample_combat("small_flier", toughness=1, block_power=1, power=2)
            self.unordered(self.owner)

    def mire(self):
        recover = self.mode == "recover_and_mana"
        self.common_decks()
        self.add("corpse", BEAR, self.owner, "graveyard")
        self.add("noncreature_grave", BOOK, self.owner, "graveyard")
        self.add("opposing_corpse", GIANT, self.foe, "graveyard")
        self.emit("turn", player=self.owner)
        self.fund(self.owner, ZERO)
        self.fund(self.foe, ZERO)
        if recover:
            if self.owner == 0:
                self.emit("choose_trigger", player=0, ref="source")
            self.zone_choice(self.owner, BEAR)
        elif self.owner == 1:
            self.emit("choose_zone", player=self.owner, value=-1)
        self.emit("play_land", ref="source", value=1)
        self.resolve()
        self.move("source", "battlefield")
        if recover:
            self.move("corpse", "library")
            self.library[self.owner].insert(0, "corpse")
        self.shuffled(None)
        self.observe("source", "tapped", 1)
        self.emit("can_activate", ref="source")
        self.state()
        self.emit("advance_turn")
        self.emit("advance_turn")
        self.emit("untap_step", player=self.owner)
        self.observe("source", "tapped", 0)
        self.emit("can_activate", ref="source", value=1)
        self.emit("activate", ref="source", value=1)
        self.emit("assert_stack")
        self.observe("source", "tapped", 1)
        self.state([0, 1, 0, 0, 0, 0])

    def grotesque(self):
        black = self.mode == "black_paid"
        self.common_decks()
        self.add("discardable", BOOK, self.foe, "hand")
        if not black:
            self.add("ground", BEAR, self.foe, "battlefield")
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, [1 if black else 2, int(black), 0, 0, 0, 1])
        if self.owner == 0 and black:
            self.emit("choose_dialog", player=0, value=1)
        self.emit("cast", ref="source", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner)
        if black:
            self.emit("choose_player", player=self.owner, value=self.foe)
            self.emit("discard_policy", value=1)
        self.complete_source()
        if black:
            self.emit("discard_policy")
            self.move("discardable", "graveyard")
        self.state()
        if not black:
            self.flying_combat()

    def fell(self):
        own_discard = self.mode == "own_discard"
        prevention = self.mode == "actual_life_loss"
        self.common_decks()
        if own_discard:
            self.add("own_1", BOOK, self.owner, "hand")
            self.add("own_2", BEAR, self.owner, "hand")
            self.add("discard_spell", MIND_ROT, self.owner, "hand")
            self.add("ground", BEAR, self.foe, "battlefield")
        else:
            self.add("discardable", BOOK, self.foe, "hand")
        if prevention:
            self.add("damage_prevention", ARMOR, self.foe, "battlefield")
        self.begin_source()
        self.emit("discard_policy", value=1)
        self.complete_source()
        if not own_discard:
            self.move("discardable", "graveyard")
            self.life[self.foe] -= 2
        self.state()
        if own_discard:
            self.emit("choose_player", player=self.owner, value=self.owner)
            self.pending_cast("discard_spell", player=self.owner)
            self.resolve()
            for ref in ("discard_spell", "own_1", "own_2"):
                self.move(ref, "graveyard")
            self.state()
        self.emit("discard_policy")
        if own_discard:
            self.flying_combat()

    def run(self):
        scenarios = {
            EXARCH: self.exarch, EARWIG: self.earwig, SIFTER: self.sifter, SHAMAN: self.shaman,
            HEAVY: self.heavy, HORIZONS: self.horizons, SCEPTER: self.scepter, CITY: self.city,
            NULLTREAD: self.tuck_creature, SPRITE: self.sprite, TRENCH: self.trench,
            DISMISSER: self.tuck_creature, MIRE: self.mire, GROTESQUE: self.grotesque, FELL: self.fell,
        }
        scenarios[self.csvid]()



def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()["sources"][name]["registry"]["csvid"] != csvid:
        raise ValueError("exact registered entry cards and selection primary CSVID required")
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
