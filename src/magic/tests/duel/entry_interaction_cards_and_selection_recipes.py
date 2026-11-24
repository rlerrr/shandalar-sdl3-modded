"""Entry triggers and card selection scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "entry_interaction_cards_and_selection"
PROFILE = Path(__file__).with_name("data") / "entry_interaction_cards_and_selection_wave8_profiles.json"
DISCIPLE, GORGER, NAGA, EMISSARY = 13423, 11062, 16972, 17405
MODES = {
    DISCIPLE: ("self_short", "source_devotion", "additional_devotion"),
    GORGER: ("trample_then_exile", "counter_then_bounce", "empty_then_bounce"),
    NAGA: ("mill_two", "keep_reorder", "surveil_triggers"),
    EMISSARY: ("land_first_strike", "nonland_mill", "nonland_keep", "empty_explore"),
}
SINGLE = set()
PREFIX = "EntryInteractionCardsAndSelectionRecipe"
SEED = 3404
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP = "Forest", "Island", "Mountain", "Plains", "Swamp"
BOLT, BEAR, BOOK, GIANT, ZOMBIE = "Lightning Bolt", "Grizzly Bears", "Spellbook", "Hill Giant", "Scathe Zombies"
SWORDS, BOUNCE, SAVAGERY = "Swords to Plowshares", "Unsummon", "Increasing Savagery"
CAMPAIGN = "Disinformation Campaign"
WITNESSES = {FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP, BOLT, BEAR, BOOK, GIANT, ZOMBIE, SWORDS, BOUNCE, SAVAGERY, CAMPAIGN}



RATS, DRAGON, MAGGOTS, MOURNWHELK, CULTIVATOR, BALOTH = 8221, 9583, 3514, 9216, 5846, 11155
SEER, JADELIGHT, AZRA, CONTRACT, SUMMIT, PENDULUM = 15700, 18230, 18575, 18956, 19386, 16766
FOLLOWERS = (RATS, DRAGON, MAGGOTS, MOURNWHELK, CULTIVATOR, BALOTH, SEER, JADELIGHT, AZRA, CONTRACT, SUMMIT, PENDULUM)
MODES.update({
    RATS: ("foe_value", "self_land_haste"),
    DRAGON: ("damage_draw", "decline_after_damage", "flying"),
    MAGGOTS: ("discard_creatures", "decline"),
    MOURNWHELK: ("ordinary", "evoke"),
    CULTIVATOR: ("discard_two", "partial"),
    BALOTH: ("normal", "enemy_discard", "self_discard"),
    SEER: ("exile_and_bounce", "lands_only", "colored_short"),
    JADELIGHT: ("land_then_mill", "keep_twice", "empty_twice"),
    AZRA: ("loot", "decline"),
    CONTRACT: ("four_names", "duplicate_names"),
    SUMMIT: ("tap_one", "tap_none"),
    PENDULUM: ("cost_five", "short"),
})
SINGLE.add((DRAGON, "decline_after_damage"))
MIND_ROT, TRAP, DEMON, GRIS, HELL, VOID = (
    "Mind Rot", "Searing Blaze", "Demon of Death's Gate", "Griselbrand", "Hellcarver Demon", "Lord of the Void",
)
AZGATE, DIGATE, RAGATE = "Azorius Guildgate", "Dimir Guildgate", "Rakdos Guildgate"
WITNESSES |= {MIND_ROT, TRAP, DEMON, GRIS, HELL, VOID, AZGATE, DIGATE, RAGATE}
ZONE_NAMES = {code: name for name, code in ZONES.items()}
GAPS = {
    DISCIPLE: ["The selected reveal pool is observable in headless mode."],
    NAGA: ["Identical AI reorder cards avoid assuming strategy; the human reorder has distinct actual choices."],
    RATS: ["Exact damage amount and targeting are observed through actual life; separate source-prevention predicates are unobserved."],
    DRAGON: ["The damage control uses a real registered life/damage trap witness. AI strategic decline is unobserved."],
    AZRA: ["In a duel, the only player on the caster's team is the caster."],
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

    def add(self, ref, name, seat, zone, *, tapped=False, untapped=False):
        fixture = card(name, ref, seat, zone)
        if tapped:
            fixture["state"] = ["tapped"]
        if untapped:
            fixture["state"] = ["untapped"]
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

    def begin_paid_action(self, ref, owner, target):
        self.pending_cast(ref, player=owner, target=target)
        self.resolve()
        self.move(ref, "graveyard")

    def disciple(self):
        target = self.owner if self.mode == "self_short" else self.foe
        powered = self.mode == "additional_devotion"
        self.deck(self.owner, (FOREST, ISLAND))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        self.add("selected", BOOK, target, "hand")
        if target == self.foe:
            self.add("shown", BEAR, target, "hand")
            self.add("hidden", MOUNTAIN, target, "hand")
            self.add("opposing_black", ZOMBIE, self.foe, "battlefield")
            self.add("opposing_black_second", ZOMBIE, self.foe, "battlefield")
        if powered:
            self.add("extra_devotion", ZOMBIE, self.owner, "battlefield")
            self.add("shown_land", FOREST, target, "hand")
        self.begin_source()
        self.emit("observe_search_filters", value=1)
        self.emit("choose_player", player=self.owner, value=target)
        shown = (BOOK,) if target == self.owner else (BEAR, BOOK, FOREST) if powered else (BEAR, BOOK)
        for name in shown:
            self.zone_choice(target, name)
        self.zone_choice(self.owner, BOOK)
        self.complete_source()
        self.emit("assert_offered_count", value=len(shown))
        for index, name in enumerate(shown):
            self.emit("assert_offered_card", index=index, value=self.rows[name]["csvid"])
        self.move("selected", "graveyard")
        self.state()

    def gorger(self):
        trample = self.mode == "trample_then_exile"
        more = self.mode == "counter_then_bounce"
        counter_count = 2 if trample else int(more)
        self.deck(self.owner, (ISLAND, FOREST, PLAINS, SWAMP, MOUNTAIN, BEAR, BOOK))
        self.deck(self.foe, (MOUNTAIN, ISLAND))
        for index in range(counter_count):
            self.add(f"discard_{index}", BOOK if index == 0 else BEAR, self.owner, "hand")
        self.add("leave", SWORDS if trample else BOUNCE, self.foe, "hand")
        if trample:
            self.add("blocker", GIANT, self.foe, "battlefield")
        if more:
            self.add("growth", SAVAGERY, self.foe, "hand")
        self.begin_source()
        self.emit("discard_policy", value=1)
        self.resolve()
        self.emit("discard_policy")
        self.move("source", "battlefield")
        for index in range(counter_count):
            self.move(f"discard_{index}", "graveyard")
        self.observe("source", "in_play", 1)
        self.observe("source", "plus1_counters", counter_count)
        self.observe("source", "power", 2 + counter_count)
        self.observe("source", "toughness", 2 + counter_count)
        self.state()

        if trample:
            self.start_combat(actor=self.owner, attacker="source")
            self.declaration_end()
            self.emit("begin_blockers")
            self.emit("block", ref="blocker", other="source", value=1)
            self.emit("advance_combat")
            self.observe("source", "blocked", 1)
            self.emit("combat_damage")
            self.resolve()
            self.emit("end_combat")
            self.move("blocker", "graveyard")
            self.life[self.foe] -= 1
            self.observe("source", "damage", 3)
            self.state()
        self.emit("advance_turn")
        self.emit("turn", player=self.foe)
        if more:
            self.begin_paid_action("growth", self.foe, "source")
            counter_count += 5
            self.observe("source", "plus1_counters", counter_count)
            self.observe("source", "power", 2 + counter_count)
            self.state()
        self.begin_paid_action("leave", self.foe, "source")
        self.move("source", "exile" if trample else "hand")
        if trample:
            self.life[self.owner] += 4
        self.draw(self.owner, counter_count)
        self.state()

    def naga(self):
        keep = self.mode == "keep_reorder"
        trigger = self.mode == "surveil_triggers"
        prefix = (BOOK, BOOK, BOOK) if keep and self.owner == 1 else (BOLT, BEAR, BOOK)
        self.deck(self.owner, ((PLAINS,) if trigger else ()) + prefix + (FOREST,))
        self.deck(self.foe, (MOUNTAIN, ISLAND))
        if trigger:
            self.add("campaign", CAMPAIGN, self.owner, "hand")
            self.add("campaign_discard", BOOK, self.foe, "hand")
            self.emit("turn", player=self.owner)
            self.fund(self.foe, ZERO)
            self.emit("discard_policy", value=1)
            self.pending_cast("campaign", player=self.owner)
            self.resolve()
            self.emit("discard_policy")
            self.move("campaign", "battlefield")
            self.draw(self.owner, 1)
            self.move("campaign_discard", "graveyard")
            self.state()
        self.begin_source()
        if keep:
            self.emit("choose_zone", player=self.owner, value=-1)
            if self.owner == 0:
                self.zone_choice(self.owner, BOOK)
                self.emit("choose_zone", player=self.owner, value=-1)
        else:
            self.zone_choice(self.owner, BOLT)
            self.zone_choice(self.owner, BOOK)
            if trigger:
                self.zone_choice(self.owner, BEAR)
            else:
                self.emit("choose_zone", player=self.owner, value=-1)
        self.complete_source()
        if keep and self.owner == 0:
            library = self.library[self.owner]
            self.library[self.owner] = [library[2], library[0], library[1], library[3]]
        elif not keep:
            for ref in self.library[self.owner][:3]:
                if self.objects[ref]["name"] != BEAR or trigger:
                    self.move(ref, "graveyard")
        if trigger:
            self.move("campaign", "hand")
        self.state()

    def emissary(self):
        land = self.mode == "land_first_strike"
        empty = self.mode == "empty_explore"
        mill = self.mode == "nonland_mill"
        if not empty:
            self.deck(self.owner, (FOREST if land else BOOK, ISLAND, PLAINS))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        if land:
            self.add("blocker", BEAR, self.foe, "battlefield")
        self.begin_source()
        if not empty:
            self.emit("choose_dialog", player=self.owner)
            if not land:
                self.emit("choose_dialog", player=self.owner, value=int(mill))
        self.resolve()
        self.move("source", "battlefield")
        if land or mill:
            self.move(f"library_{self.owner}_0", "hand" if land else "graveyard")
        self.observe("source", "in_play", 1)
        self.observe("source", "plus1_counters", int(not land))
        self.observe("source", "power", 2 + int(not land))
        self.observe("source", "toughness", 1 + int(not land))
        self.state()
        if not land:
            return
        self.start_combat(actor=self.owner, attacker="source")
        self.declaration_end()
        self.emit("begin_blockers")
        self.emit("block", ref="blocker", other="source", value=1)
        self.emit("advance_combat")
        self.observe("source", "blocked", 1)
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")
        self.move("blocker", "graveyard")
        self.observe("source", "in_play", 1)
        self.observe("source", "damage", 0)
        self.state()


    def selection_setup(self, deck=(ISLAND, FOREST, PLAINS, SWAMP, MOUNTAIN, BOOK, BEAR, BOLT)):
        self.deck(self.owner, deck)
        self.deck(self.foe, (MOUNTAIN, ISLAND, FOREST))

    def funded_source(self, fee, *, accepted=True, mana_clicks=(), mode=None):
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, fee)
        for color in mana_clicks:
            self.emit("choose_mana", color=color, player=self.owner)
        if mode is not None:
            self.emit("choose_dialog", player=self.owner, value=mode)
        self.emit("cast", ref="source", value=int(accepted))
        self.emit("assert_stack", value=int(accepted))
        self.mana("assert_mana", self.owner, ZERO if accepted else fee)

    def creature_counts(self, count):
        self.observe("source", "in_play", 1)
        self.observe("source", "plus1_counters", count)
        for prop in ("power", "toughness"):
            self.observe("source", prop, int(self.oracle[self.name][prop]) + count)

    def strike(self, *, fresh=False, flying=False):
        if fresh:
            self.emit("begin_attackers")
            self.emit("attack", ref="source", value=1)
            self.observe("source", "attacking", 1)
        else:
            self.start_combat(actor=self.owner, attacker="source")
        self.declaration_end()
        self.complete_combat(attacker="source", blocker="ground" if flying else None)
        self.state()

    def rats(self):
        self_target = self.mode == "self_land_haste"
        target = self.owner if self_target else self.foe
        self.selection_setup()
        self.add("selected", FOREST if self_target else GIANT, target, "hand")
        self.add("reserve", BOOK, target, "hand")
        self.begin_source()
        self.emit("choose_player", player=self.owner, value=target)
        self.zone_choice(target, self.objects["selected"]["name"])
        self.complete_source()
        self.move("selected", "graveyard")
        self.life[target] -= 0 if self_target else 3
        self.state()
        if self_target:
            self.strike(fresh=True)

    def dragon(self):
        fight = self.mode == "flying"
        accept = self.mode == "damage_draw"
        self.selection_setup()
        if fight:
            self.add("ground", BEAR, self.foe, "battlefield")
        else:
            self.add("incoming_bolt", BOLT, self.owner, "hand")
            self.add("disposable", BOOK, self.owner, "hand")
            self.add("trap_registration", TRAP, self.foe, "hand")
            self.emit("turn", player=self.owner)
            self.fund(self.foe, ZERO)
            self.emit("choose_player", player=self.owner, value=self.foe)
            self.pending_cast("incoming_bolt", player=self.owner)
            self.resolve()
            self.move("incoming_bolt", "graveyard")
            self.life[self.foe] -= 3
            self.state()
        self.begin_source()
        if accept and self.owner == 0:
            self.emit("choose_trigger", player=0, ref="source")
        self.emit("discard_policy", value=1)
        self.complete_source()
        self.emit("discard_policy")
        if accept:
            self.move("disposable", "graveyard")
            self.draw(self.owner, 3)
        self.state()
        if fight:
            self.strike(flying=True)

    def selected_discard(self):
        lands = self.csvid == CULTIVATOR
        takes = {"decline": 0, "partial": 1}.get(self.mode, 2)
        choices = (FOREST, ISLAND) if lands else (BEAR, GIANT)
        self.selection_setup()
        self.add("unqualified", BOOK, self.owner, "hand")
        for index, name in enumerate(choices):
            self.add(f"pay_{index}", name, self.owner, "hand")
        self.begin_source()
        for name in choices[:takes]:
            self.zone_choice(self.owner, name)
        if takes < 2 or not lands:
            self.emit("choose_zone", player=self.owner, value=-1)
        self.resolve()
        self.move("source", "battlefield")
        for index in range(takes):
            self.move(f"pay_{index}", "graveyard")
        self.creature_counts(takes if lands else 2 * takes)
        if lands:
            self.draw(self.owner, takes)
        self.state()

    def mournwhelk(self):
        evoke = self.mode == "evoke"
        self.selection_setup()
        self.add("first_discard", BOOK, self.foe, "hand")
        self.add("second_discard", GIANT, self.foe, "hand")
        self.funded_source([3 if evoke else 6, 1, 0, 0, 0, 0], mode=None if evoke else 0)
        self.emit("choose_player", player=self.owner, value=self.foe)
        self.emit("discard_policy", value=1)
        self.resolve()
        self.emit("discard_policy")
        self.move("source", "graveyard" if evoke else "battlefield")
        if not evoke:
            self.inspect_source()
        self.move("first_discard", "graveyard")
        self.move("second_discard", "graveyard")
        self.state()

    def baloth(self):
        normal = self.mode == "normal"
        enemy = self.mode == "enemy_discard"
        self.selection_setup()
        if normal:
            self.begin_source()
            self.complete_source()
            self.life[self.owner] += 4
        else:
            actor = self.foe if enemy else self.owner
            self.add("discard_spell", MIND_ROT, actor, "hand")
            self.add("other_discard", BOOK, self.owner, "hand")
            self.emit("turn", player=actor)
            self.fund(1 - actor, ZERO)
            self.emit("discard_policy", value=1)
            self.emit("choose_player", player=actor, value=self.owner)
            self.pending_cast("discard_spell", player=actor)
            self.resolve()
            self.emit("discard_policy")
            self.move("discard_spell", "graveyard")
            self.move("other_discard", "graveyard")
            self.move("source", "battlefield" if enemy else "graveyard")
            if enemy:
                self.life[self.owner] += 4
        self.state()

    def seer(self):
        paid = self.mode != "colored_short"
        targets = self.mode == "exile_and_bounce"
        self.selection_setup()
        self.add("retained_land", FOREST, self.foe, "hand")
        if targets:
            self.add("exiled_card", BOOK, self.foe, "hand")
            self.add("return_spell", BOUNCE, self.foe, "hand")
        fee = [1, 0, 3, 0, 0, 0] if paid else [0, 0, 4, 0, 0, 0]
        self.funded_source(fee, accepted=paid, mana_clicks=(2, 2, 2) if paid and self.owner == 0 else ())
        if not paid:
            self.state(fee)
            return
        if targets:
            self.zone_choice(self.owner, BOOK)
        else:
            self.emit("choose_zone", player=self.owner, value=-1)
        self.complete_source()
        if targets:
            self.move("exiled_card", "exile")
        self.state()
        if targets:
            self.begin_paid_action("return_spell", self.foe, "source")
            self.move("source", "hand")
            self.draw(self.foe, 1)
            self.state()

    def jadelight(self):
        first_land = self.mode == "land_then_mill"
        empty = self.mode == "empty_twice"
        if not empty:
            self.deck(self.owner, (FOREST, BOOK, ISLAND) if first_land else (BOOK, FOREST, ISLAND))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        self.begin_source()
        if not empty:
            for index in range(2):
                self.emit("choose_dialog", player=self.owner)
                if not (first_land and index == 0):
                    self.emit("choose_dialog", player=self.owner, value=int(first_land))
        self.resolve()
        self.move("source", "battlefield")
        if first_land:
            self.move(f"library_{self.owner}_0", "hand")
            self.move(f"library_{self.owner}_1", "graveyard")
        self.creature_counts(1 if first_land else 2)
        self.state()

    def azra(self):
        chose = self.mode == "loot"
        self.selection_setup()
        self.add("discardable", BOOK, self.owner, "hand")
        self.add("rival_card", BEAR, self.foe, "hand")
        self.begin_source()
        if self.owner == 0:
            self.emit("choose_trigger", ref="source", player=0)
        if chose:
            self.zone_choice(self.owner, BOOK)
        else:
            self.emit("choose_zone", player=self.owner, value=-1)
        self.complete_source()
        if chose:
            self.move("discardable", "graveyard")
            self.draw(self.owner, 1)
        self.state()

    def contract(self):
        wins = self.mode == "four_names"
        self.selection_setup()
        for index, name in enumerate((DEMON, GRIS, HELL, VOID if wins else HELL)):
            self.add(f"demon_{index}", name, self.owner, "battlefield")
        self.begin_source()
        self.complete_source()
        self.draw(self.owner, 4)
        self.life[self.owner] -= 4
        self.state()
        for player in (self.foe, self.owner):
            self.emit("advance_turn")
            self.emit("untap_step", player=player)
            self.emit("upkeep_step", player=player)
            self.resolve()
            if wins and player == self.owner:
                # The production host encodes actual loss in the player's life slot.
                self.life[self.foe] = -99
            self.state()

    def summit(self):
        chooses = self.mode == "tap_one"
        self.selection_setup()
        self.add("first_gate", AZGATE, self.owner, "battlefield", untapped=True)
        self.add("second_gate", DIGATE, self.owner, "battlefield", untapped=True)
        self.add("own_gate_drop", RAGATE, self.owner, "hand")
        self.add("opponent_gate_drop", RAGATE, self.foe, "hand")
        self.add("plain_land", FOREST, self.owner, "battlefield")
        self.begin_source()
        if self.owner == 0 and chooses:
            self.emit("choose_trigger", ref="source", player=0)
        if chooses:
            self.emit("choose_target", ref="first_gate", player=self.owner)
        if chooses or self.owner == 1:
            self.emit("choose_optional_target", ref="second_gate", player=self.owner)
        self.complete_source()
        if chooses:
            self.draw(self.owner, 1)
        for ref in ("first_gate", "second_gate", "plain_land"):
            self.observe(ref, "tapped", int(chooses and ref == "first_gate"))
        self.state()
        self.emit("play_land", ref="own_gate_drop", value=1)
        self.resolve()
        self.move("own_gate_drop", "battlefield")
        self.observe("own_gate_drop", "tapped", 1)
        self.draw(self.owner, 1)
        self.state()
        self.emit("advance_turn")
        self.emit("turn", player=self.foe)
        self.emit("play_land", ref="opponent_gate_drop", value=1)
        self.resolve()
        self.move("opponent_gate_drop", "battlefield")
        self.observe("opponent_gate_drop", "tapped", 1)
        self.state()

    def pendulum(self):
        paid = self.mode == "cost_five"
        self.selection_setup()
        self.begin_source()
        self.complete_source()
        self.life[self.owner] += 3
        self.state()
        fee = [5 if paid else 4, 0, 0, 0, 0, 0]
        self.fund(self.owner, fee)
        self.emit("can_activate", ref="source", value=int(paid))
        self.emit("activate", ref="source", value=int(paid))
        self.emit("assert_stack", value=int(paid))
        if paid:
            self.move("source", "graveyard")
            self.state()
            self.resolve()
            self.draw(self.owner, 1)
        self.state(ZERO if paid else fee)

    def run(self):
        scenarios = {
            DISCIPLE: self.disciple, GORGER: self.gorger, NAGA: self.naga, EMISSARY: self.emissary,
            RATS: self.rats, DRAGON: self.dragon, MAGGOTS: self.selected_discard,
            MOURNWHELK: self.mournwhelk, CULTIVATOR: self.selected_discard, BALOTH: self.baloth,
            SEER: self.seer, JADELIGHT: self.jadelight, AZRA: self.azra, CONTRACT: self.contract,
            SUMMIT: self.summit, PENDULUM: self.pendulum,
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
