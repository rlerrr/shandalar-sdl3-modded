"""Entry triggers and card selection scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "entry_rider_cards_and_selection"
PROFILE = Path(__file__).with_name("data") / "entry_rider_cards_and_selection_wave8_profiles.json"
SPY, HARBINGER, ADEPT, REVELER = 12888, 9210, 1359, 16034
MODES = {
    SPY: ("opponent_land", "self_no_land", "opponent_empty"),
    HARBINGER: ("take_islandwalk", "take_no_island", "decline"),
    ADEPT: ("reveal", "pay_three", "short"),
    REVELER: ("exact_discount_prowess", "opponent_spells_ignored", "short"),
}
SINGLE = {(HARBINGER, "decline")}
PREFIX = "EntryRiderCardsAndSelectionRecipe"
SEED = 3308
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP = "Forest", "Island", "Mountain", "Plains", "Swamp"
BOLT, BEAR, BOOK = "Lightning Bolt", "Grizzly Bears", "Spellbook"
MERFOLK, NATURALIZE = "Merrow Reejerey", "Naturalize"
WITNESSES = {FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP, BOLT, BEAR, BOOK, MERFOLK, NATURALIZE}



HERALD, RATS, DWARF, GOBLIN, HIRED, HAG = 984, 6865, 5550, 2712, 4492, 9797
WARDEN, FAMILIAR, SPHINX, EXPLORER, PERSPECTIVES, PATRON = 13936, 6740, 11847, 16301, 16978, 18597
FOLLOWERS = (HERALD, RATS, DWARF, GOBLIN, HIRED, HAG, WARDEN, FAMILIAR, SPHINX, EXPLORER, PERSPECTIVES, PATRON)
MODES.update({
    HERALD: ("choose_bear", "anthem_removed"),
    RATS: ("selected_top", "empty_hand"),
    DWARF: ("one_dwarf", "zero_dwarf"),
    GOBLIN: ("one_goblin", "zero_goblin"),
    HIRED: ("foe_search", "foe_decline"),
    HAG: ("both_thresholds", "forests_only", "swamps_only", "under_threshold"),
    WARDEN: ("found_in_prefix", "creature_outside_prefix"),
    FAMILIAR: ("artifact_flight", "mill_nonartifact", "empty"),
    SPHINX: ("split_five", "one_card_flight"),
    EXPLORER: ("both_search", "only_foe_discards"),
    PERSPECTIVES: ("seven_free", "six_cannot_cycle"),
    PATRON: ("support_both", "own_spell_many", "foe_sources_no_draw"),
})
DWARF_CARD, GOBLIN_CARD = "Dwarven Trader", "Mons's Goblin Raiders"
CYCLER, SAVAGERY = "Barren Moor", "Increasing Savagery"
WITNESSES |= {DWARF_CARD, GOBLIN_CARD, CYCLER, SAVAGERY}
ZONE_NAMES = {code: name for name, code in ZONES.items()}
GAPS = {
    HARBINGER: ["The human trigger can be declined. AI strategic decline and the visual reveal are unobserved."],
    ADEPT: ["Headless presentation alone is unobserved."],
    HERALD: ["The automatic type choice is constrained by the library. Bottom-card order is one lawful ordering; manual ordering is unobserved."],
    DWARF: ["This search calibration selects zero or one card. Choosing an order among multiple searched cards is unobserved."],
    GOBLIN: ["This search calibration selects zero or one card. Choosing an order among multiple searched cards is unobserved."],
    HAG: ["Both thresholds are exercised together and independently. Independent optional acceptance when both thresholds are met is unobserved."],
    WARDEN: ["The source has fewer than two distinct cards to order on the bottom of the library in these fixtures."],
    SPHINX: ["Five identical basic lands make the AI pile size deterministic; the untouched sixth card has a distinct identity."],
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

    def add(self, ref, name, seat, zone, *, tapped=False):
        fixture = card(name, ref, seat, zone)
        if tapped:
            fixture["state"] = ["tapped"]
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
                    "bind_permanent_copy", "bind_hand_copy", "choose_trigger", "choose_zone_card", "choose_optional_zone", "choose_optional_target", "assert_shuffle_count"}:
            values["player"] = player
        if op in {"bind_permanent_copy", "bind_hand_copy"}:
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
        if record["name"] == BOOK:
            assert record["mana_cost"] == "{0}"
            return ZERO
        if record["name"] == "Bedlam Reveler":
            assert record["mana_cost"] == "{6}{R}{R}"
            return [6, 0, 0, 0, 2, 0]
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


    def source_payment(self, fee, *, accepted=True):
        self.fund(self.owner, fee)
        self.emit("cast", ref="source", value=int(accepted))
        self.emit("assert_stack", value=int(accepted))
        self.mana("assert_mana", self.owner, ZERO if accepted else fee)

    def enter(self):
        self.complete_source()

    def combat(self, *, legal):
        self.start_combat(actor=self.owner, attacker="source")
        self.declaration_end()
        self.complete_combat(attacker="source", blocker="ground", legal=legal)
        self.state()

    def spy(self):
        target = self.owner if self.mode == "self_no_land" else self.foe
        empty = self.mode == "opponent_empty"
        flight = self.mode == "opponent_land"
        self.deck(1 - target, (ISLAND, PLAINS))
        if not empty:
            self.deck(target, (BOLT, BEAR, FOREST, BOOK, SWAMP) if flight else (BOLT, BEAR, BOOK))
        if flight:
            self.add("ground", BEAR, self.foe, "battlefield")
        self.begin_source()
        self.emit("choose_player", player=self.owner, value=target)
        if not empty:
            for seat in (self.owner, self.foe):
                self.emit("choose_zone", player=seat, value=-1)
        self.enter()
        for ref in self.library[target][:3]:
            self.move(ref, "graveyard")
        self.state()
        if flight:
            self.combat(legal=False)

    def harbinger(self):
        accepted = self.mode != "decline"
        island = self.mode == "take_islandwalk"
        self.deck(self.owner, (BEAR, MERFOLK, FOREST))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        if accepted:
            self.add("ground", BEAR, self.foe, "battlefield")
        if island:
            self.add("defender_island", ISLAND, self.foe, "battlefield")
        self.begin_source()
        if accepted:
            if self.owner == 0:
                self.emit("choose_trigger", ref="source", player=0)
            self.emit("choose_zone_card", player=self.owner, value=self.rows[MERFOLK]["csvid"])
        self.enter()
        self.emit("assert_shuffle_count", player=self.owner, value=int(accepted))
        self.emit("assert_shuffle_count", player=self.foe)
        if not accepted:
            self.state()
            return
        self.receipts()
        self.assert_top(self.foe)
        self.emit("assert_zone", player=self.owner, zone=ZONES["library"], value=3)
        self.emit("assert_zone_card", player=self.owner, zone=ZONES["library"], value=self.rows[MERFOLK]["csvid"])
        self.start_combat(actor=self.owner, attacker="source")
        self.declaration_end()
        self.complete_combat(attacker="source", blocker="ground", legal=not island)
        self.receipts()
        self.assert_top(self.foe)
        self.emit("assert_zone_card", player=self.owner, zone=ZONES["library"], value=self.rows[MERFOLK]["csvid"])

    def adept(self):
        reveal = self.mode == "reveal"
        accepted = self.mode != "short"
        self.deck(self.owner, (ISLAND, FOREST, PLAINS))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        self.add("non_merfolk", BEAR, self.owner, "hand")
        if reveal:
            self.add("merfolk", MERFOLK, self.owner, "hand")
            self.emit("choose_zone_card", player=self.owner, value=self.rows[MERFOLK]["csvid"])
            if self.owner == 0:
                self.emit("choose_dialog", player=0)
        fee = [1 if reveal else 4 if accepted else 3, 0, 1, 0, 0, 0]
        self.source_payment(fee, accepted=accepted)
        if accepted:
            self.enter()
            self.draw(self.owner, 1)
        self.state(ZERO if accepted else fee)

    def reveler(self):
        discounted = self.mode != "opponent_spells_ignored"
        accepted = self.mode != "short"
        prowess = self.mode == "exact_discount_prowess"
        self.deck(self.owner, (BEAR, BOOK, FOREST, ISLAND))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        self.add("discard_first", BEAR, self.owner, "hand")
        self.add("discard_second", BOOK, self.owner, "hand")
        self.add("nonspell_in_grave", FOREST, self.owner, "graveyard")
        self.add("first_spell", BOLT, self.owner if discounted else self.foe, "graveyard")
        self.add("second_spell", NATURALIZE, self.owner if discounted else self.foe, "graveyard")
        fee = [4 if prowess else 6 if accepted else 3, 0, 0, 0, 2, 0]
        self.source_payment(fee, accepted=accepted)
        if not accepted:
            self.state(fee)
            return
        self.emit("discard_policy", value=1)
        self.enter()
        self.emit("discard_policy")
        for ref in ("discard_first", "discard_second"):
            self.move(ref, "graveyard")
        self.draw(self.owner, 3)
        self.state()
        if not prowess:
            return
        for index, name in enumerate((BEAR, BOOK)):
            library_ref = f"library_{self.owner}_{index}"
            new_ref = "drawn_creature" if index == 0 else "drawn_noncreature"
            self.objects[new_ref] = self.objects.pop(library_ref)
            self.emit("bind_hand_copy", ref=new_ref, player=self.owner, value=self.rows[name]["csvid"])
            self.pending_cast(new_ref, player=self.owner)
            if index == 1:
                self.emit("announce_spell", ref=new_ref)
            self.resolve()
            self.move(new_ref, "battlefield")
            self.observe("source", "power", 3 + index)
            self.observe("source", "toughness", 4 + index)
        self.emit("cleanup")
        self.emit("refresh")
        self.observe("source", "power", 3)
        self.observe("source", "toughness", 4)
        self.state()


    def public_reveal(self, player, count=1):
        if player == 0:
            for _ in range(count):
                self.emit("choose_dialog", player=player)

    def shuffle_receipts(self, shuffled):
        for player in (0, 1):
            self.emit("assert_shuffle_count", player=player, value=int(player in shuffled))

    def exact_library_prefix(self, player, names):
        self.emit("assert_zone", player=player, zone=ZONES["library"], value=len(self.library[player]))
        for index, name in enumerate(names):
            self.emit("assert_zone_card", player=player, zone=ZONES["library"], index=index,
                      value=self.rows[name]["csvid"])

    def choose_zone_card(self, player, name):
        self.emit("choose_zone_card", player=player, value=self.rows[name]["csvid"])

    def rotate_top(self, player, keep, *, window):
        remaining = self.library[player]
        self.library[player] = remaining[window:] + remaining[:window]

    def herald(self):
        removed = self.mode == "anthem_removed"
        self.deck(self.owner, (BOOK, BEAR, ISLAND, BEAR, FOREST))
        self.deck(self.foe, (PLAINS, MOUNTAIN))
        self.add("friendly_bear", BEAR, self.owner, "battlefield")
        self.add("opposing_bear", BEAR, self.foe, "battlefield")
        self.add("decoy", MERFOLK, self.foe, "battlefield")
        if removed:
            self.add("removal", NATURALIZE, self.owner, "hand")
        self.begin_source()
        # Auto's only creature subtype among the cards of the library is Bear.
        self.emit("choose_dialog", player=self.owner)
        self.public_reveal(self.owner, 4)
        self.enter()
        for position in (1, 3):
            self.move(f"library_{self.owner}_{position}", "hand")
        self.rotate_top(self.owner, 0, window=2)
        for ref in ("friendly_bear", "opposing_bear"):
            self.observe(ref, "power", 3)
            self.observe(ref, "toughness", 3)
        self.observe("decoy", "power", 2)
        self.observe("decoy", "toughness", 2)
        self.state()
        if removed:
            self.pending_cast("removal", player=self.owner, target="source")
            self.resolve()
            self.move("source", "graveyard")
            self.move("removal", "graveyard")
            for ref in ("friendly_bear", "opposing_bear"):
                self.observe(ref, "power", 2)
                self.observe(ref, "toughness", 2)
            self.state()

    def rats(self):
        target_has_cards = self.mode == "selected_top"
        self.deck(self.owner, (FOREST, ISLAND))
        self.deck(self.foe, (PLAINS, MOUNTAIN))
        self.add("own_card", BOLT, self.owner, "hand")
        if target_has_cards:
            self.add("selected_card", BOOK, self.foe, "hand")
            self.add("spared_card", BEAR, self.foe, "hand")
        self.begin_source()
        if target_has_cards:
            self.choose_zone_card(self.foe, BOOK)
        self.enter()
        if target_has_cards:
            self.move("selected_card", "library")
            self.library[self.foe].insert(0, "selected_card")
        self.state()

    def recruiter(self):
        dwarf = self.csvid == DWARF
        selects = self.mode.startswith("one_")
        sought = DWARF_CARD if dwarf else GOBLIN_CARD
        decoy = GOBLIN_CARD if dwarf else DWARF_CARD
        self.deck(self.owner, (BOOK, sought, decoy, FOREST))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        self.begin_source()
        if selects:
            self.choose_zone_card(self.owner, sought)
        self.emit("choose_zone", player=self.owner, value=-1)
        if dwarf and selects:
            self.public_reveal(self.owner)
        self.enter()
        self.shuffle_receipts((self.owner,))
        self.receipts()
        self.assert_top(self.foe)
        self.exact_library_prefix(self.owner, (sought,) if selects else ())

    def hired(self):
        accepts = self.mode == "foe_search"
        self.deck(self.owner, (FOREST, ISLAND))
        self.deck(self.foe, (BEAR, SWAMP, BOOK))
        self.begin_source()
        self.emit("choose_dialog", player=self.foe, value=int(not accepts))
        if accepts:
            self.choose_zone_card(self.foe, SWAMP)
        self.enter()
        if accepts:
            self.move(f"library_{self.foe}_1", "battlefield")
            self.emit("bind_permanent_copy", ref="fetched_land", player=self.foe, value=self.rows[SWAMP]["csvid"])
            self.observe("fetched_land", "tapped", 0)
        self.shuffle_receipts((self.foe,) if accepts else ())
        self.receipts()
        self.assert_top(self.owner)
        if not accepts:
            self.assert_top(self.foe)

    def hag(self):
        forests = self.mode in ("both_thresholds", "forests_only")
        swamps = self.mode in ("both_thresholds", "swamps_only")
        self.deck(self.owner, (ISLAND, PLAINS))
        self.deck(self.foe, (MOUNTAIN, BOOK))
        self.add("corpse", BOLT, self.owner, "graveyard")
        self.add("discardable", BEAR, self.foe, "hand")
        for land, present in ((FOREST, forests), (SWAMP, swamps)):
            for n in range(2 if present else 1):
                self.add(f"own_{land.lower()}_{n}", land, self.owner, "battlefield", tapped=True)
            for n in range(2):
                self.add(f"rival_{land.lower()}_{n}", land, self.foe, "battlefield", tapped=True)
        self.source_payment([2, int(not forests), 0, int(forests), 0, 0])
        self.emit("discard_policy", value=1)
        if forests or swamps:
            if self.owner == 0:
                self.emit("choose_trigger", ref="source", player=0)
            if forests:
                self.choose_zone_card(self.owner, BOLT)
            if swamps:
                self.emit("choose_player", player=self.owner, value=self.foe)
        self.enter()
        self.emit("discard_policy")
        if forests:
            self.move("corpse", "library")
            self.library[self.owner].insert(0, "corpse")
        if swamps:
            self.move("discardable", "graveyard")
        self.state()

    def warden(self):
        accepts = self.mode == "found_in_prefix"
        self.deck(self.owner, (FOREST, BEAR, BOOK) if accepts else (FOREST, FOREST, BEAR))
        self.deck(self.foe, (MOUNTAIN, ISLAND))
        for i in range(2):
            self.add(f"forest_{i}", FOREST, self.owner, "battlefield", tapped=True)
        self.add("opposing_forest", FOREST, self.foe, "battlefield", tapped=True)
        self.begin_source()
        if accepts:
            self.choose_zone_card(self.owner, BEAR)
            self.public_reveal(self.owner)
        else:
            self.emit("choose_zone", player=self.owner, value=-1)
            self.choose_zone_card(self.owner, FOREST)
        self.enter()
        if accepts:
            self.move(f"library_{self.owner}_1", "hand")
        self.rotate_top(self.owner, 0, window=1 if accepts else 2)
        self.state()

    def familiar(self):
        found = self.mode == "artifact_flight"
        empty = self.mode == "empty"
        if not empty:
            self.deck(self.owner, (BOOK if found else BOLT, FOREST, ISLAND))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        if found:
            self.add("ground", BEAR, self.foe, "battlefield")
        self.begin_source()
        if not empty:
            self.public_reveal(self.owner)
        self.enter()
        if not empty:
            self.move(f"library_{self.owner}_0", "hand" if found else "graveyard")
        self.state()
        if found:
            self.combat(legal=False)

    def sphinx(self):
        full = self.mode == "split_five"
        self.deck(self.owner, (FOREST,) * 5 + (ISLAND,) if full else (ISLAND,))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        if not full:
            self.add("ground", BEAR, self.foe, "battlefield")
        self.begin_source()
        if full and self.owner == 1:
            for choice in (3, 4, -1):
                self.emit("choose_zone", player=0, value=choice)
        self.emit("choose_zone", player=0, value=-1)
        self.enter()
        for index, ref in enumerate(self.library[self.owner][:(5 if full else 1)]):
            self.move(ref, "hand" if index < 3 else "graveyard")
        self.state()
        if not full:
            self.combat(legal=False)

    def explorer(self):
        both = self.mode == "both_search"
        for player in (0, 1):
            self.deck(player, (BOOK, FOREST, ISLAND))
            self.add(f"disposable_{player}", BEAR, player, "hand")
            for n in range(3):
                self.add(f"land_{player}_{n}", SWAMP, player, "battlefield", tapped=True)
        self.begin_source()
        for player in (self.owner, self.foe):
            if both or player == self.foe:
                self.choose_zone_card(player, BEAR)
                self.choose_zone_card(player, FOREST)
            else:
                self.emit("choose_zone", player=player, value=-1)
        self.enter()
        searched = (self.owner, self.foe) if both else (self.foe,)
        for player in searched:
            self.move(f"disposable_{player}", "graveyard")
            self.move(f"library_{player}_1", "hand")
        self.shuffle_receipts(searched)
        self.receipts()
        if not both:
            self.assert_top(self.owner)

    def perspectives(self):
        free = self.mode == "seven_free"
        self.deck(self.owner, (FOREST, ISLAND, PLAINS, SWAMP, MOUNTAIN))
        self.deck(self.foe, (MOUNTAIN, ISLAND))
        self.add("cycler", CYCLER, self.owner, "hand")
        for n in range(3 if free else 2):
            self.add(f"filler_{n}", BOOK, self.owner, "hand")
        self.emit("can_activate_hand", ref="cycler", value=0)
        self.begin_source()
        self.enter()
        self.draw(self.owner, 3)
        self.state()
        self.emit("can_activate_hand", ref="cycler", value=int(free))
        if free:
            self.emit("choose_dialog", player=self.owner, value=1)
            self.emit("activate_hand", ref="cycler", value=1)
            self.emit("assert_stack", value=1)
            self.move("cycler", "graveyard")
            self.state()
            self.resolve()
            self.draw(self.owner, 1)
            self.state()

    def patron(self):
        support = self.mode == "support_both"
        friendly_spell = self.mode == "own_spell_many"
        self.deck(self.owner, (ISLAND, FOREST, PLAINS))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        self.add("own_bear", BEAR, self.owner, "battlefield")
        self.add("foe_bear", BEAR, self.foe, "battlefield")
        self.add("spared", MERFOLK, self.owner, "battlefield")
        if not support:
            self.add("counter_spell", SAVAGERY, self.owner if friendly_spell else self.foe, "hand")
        self.begin_source()
        self.emit("announce_spell", ref="source")
        for ref in ("own_bear", "foe_bear"):
            self.emit("choose_target" if support else "choose_optional_target", ref=ref, player=self.owner)
        self.enter()
        for ref in ("source", "own_bear", "foe_bear", "spared"):
            self.observe(ref, "plus1_counters", int(support and ref in ("own_bear", "foe_bear")))
        if support:
            self.draw(self.owner, 1)
        self.state()
        if not support:
            caster = self.owner if friendly_spell else self.foe
            if caster == self.foe:
                self.emit("advance_turn")
                self.emit("turn", player=caster)
            self.pending_cast("counter_spell", player=caster, target="foe_bear")
            self.resolve()
            self.move("counter_spell", "graveyard")
            self.observe("foe_bear", "plus1_counters", 5)
            if friendly_spell:
                self.draw(self.owner, 1)
            self.state()

    def run(self):
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        if self.csvid == PERSPECTIVES:
            self.fund(self.owner, ZERO)
        scenarios = {
            SPY: self.spy, HARBINGER: self.harbinger, ADEPT: self.adept, REVELER: self.reveler,
            HERALD: self.herald, RATS: self.rats, DWARF: self.recruiter, GOBLIN: self.recruiter,
            HIRED: self.hired, HAG: self.hag, WARDEN: self.warden, FAMILIAR: self.familiar,
            SPHINX: self.sphinx, EXPLORER: self.explorer, PERSPECTIVES: self.perspectives, PATRON: self.patron,
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
