"""Entry triggers and card selection scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "entry_follow_on_cards_and_selection"
PROFILE = Path(__file__).with_name("data") / "entry_follow_on_cards_and_selection_wave8_profiles.json"
ORACLE, THICKET, ORATOR, CAMPAIGN = 8186, 15326, 14975, 19340
MODES = {
    ORACLE: ("land_top", "nonland_top", "empty_library"),
    THICKET: ("select_basic", "next_green", "order_human", "decline_look"),
    ORATOR: ("controlled_dragon", "revealed_dragon", "defender_flight", "dragon_removed_pending", "decline_reveal"),
    CAMPAIGN: ("enter", "surveil_and_recast", "opponent_surveil"),
}
SINGLE_SEAT = {(THICKET, "order_human"), (THICKET, "decline_look"), (ORATOR, "decline_reveal")}
PREFIX = "EntryFollowOnCardsAndSelectionRecipe"
SEED = 3212
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP = "Forest", "Island", "Mountain", "Plains", "Swamp"
BOLT, BEAR, BOOK, GIANT = "Lightning Bolt", "Grizzly Bears", "Spellbook", "Hill Giant"
DRAGON, AIR, MURDER, RAIN = "Shivan Dragon", "Air Elemental", "Murder", "Notion Rain"
WITNESSES = {FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP, BOLT, BEAR, BOOK, GIANT, DRAGON, AIR, MURDER, RAIN}



FURY, SEAL, WEB, FETISH, REVERIE, SANDSTONE = 2262, 5574, 5164, 2009, 14616, 15537
BONDS, PUZZLEKNOT, DRAKE, REVELER, LOOKOUT, REJUVENATOR = 15561, 16450, 19323, 19717, 17529, 18916
FOLLOWERS = (FURY, SEAL, WEB, FETISH, REVERIE, SANDSTONE, BONDS, PUZZLEKNOT, DRAKE, REVELER, LOOKOUT, REJUVENATOR)
MODES.update({
    FURY: ("blocked_trample", "unblocked"),
    SEAL: ("seal_graves", "restore_target"),
    WEB: ("nonmana_untap", "restored_untap"),
    FETISH: ("delayed_bonus", "removed_before_draw"),
    REVERIE: ("creature_aura", "land_aura"),
    SANDSTONE: ("behind", "equal_flying"),
    BONDS: ("strip_destroy", "cleanup_restores"),
    PUZZLEKNOT: ("paid_activation", "wrong_color"),
    DRAKE: ("own_zones_flying", "actual_spell_changes_power"),
    REVELER: ("normal_loot", "spectacle_dump"),
    LOOKOUT: ("land_flying", "nonland_mill", "nonland_keep"),
    REJUVENATOR: ("select_land", "no_land_in_five", "decline_land"),
})
SINGLE_SEAT.add((REJUVENATOR, "decline_land"))
NATURALIZE, RAISE, FACTORY = "Naturalize", "Raise Dead", "Mishra's Factory"
HOLY, WILD, SCOUT, MYR = "Holy Strength", "Wild Growth", "Gladecover Scout", "Darksteel Myr"
WITNESSES |= {NATURALIZE, RAISE, FACTORY, HOLY, WILD, SCOUT, MYR}
ZONE_NAMES = {code: name for name, code in ZONES.items()}
GAPS = {
    REJUVENATOR: ["Bottoming receipts verify the untouched prefix and the randomized cards' exact multiset."],
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
            for seat in ((0,) if (shape["csvid"], mode) in SINGLE_SEAT else (0, 1))]


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
                    "bind_permanent_copy", "bind_hand_copy", "choose_trigger", "choose_zone_card", "choose_optional_zone"}:
            values["player"] = player
        if op in {"bind_permanent_copy", "bind_hand_copy"}:
            values["index"] = index
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
        record = self.oracle[self.objects[ref]["name"]]
        if ref == "source" and self.csvid == THICKET:
            assert record["mana_cost"] == ""
            return ZERO
        return cost_from_oracle(record)

    def move(self, ref, zone):
        obj = self.objects[ref]
        if obj["zone"] == "library":
            self.library[obj["player"]].remove(ref)
        obj["zone"] = zone

    def observe(self, ref, prop, value):
        self.emit("assert_card", ref=ref, prop=prop, value=value)

    def inspect_source(self, ref="source"):
        self.observe(ref, "in_play", 1)
        for stat in ("power", "toughness"):
            if stat in self.oracle[self.name] and self.oracle[self.name][stat] != "*":
                self.observe(ref, stat, int(self.oracle[self.name][stat]))

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

    def complete_combat(self, *, attacker, blocker=None, legal=False, prevent_damage=False, pre_block=None, power=None):
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
        if power is None:
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

    def begin_source(self, *, target=None):
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.pending_cast("source", player=self.owner, target=target)
        self.assert_top(self.owner)
        self.assert_top(self.foe)

    def complete_source(self, ref="source", *, inspect=True):
        self.resolve()
        self.move("source", "battlefield")
        if inspect:
            self.inspect_source(ref)

    def coiling(self):
        top = FOREST if self.mode == "land_top" else BOLT
        if self.mode != "empty_library":
            self.deck(self.owner, (top, ISLAND, PLAINS))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        self.begin_source()
        if self.mode != "empty_library" and self.owner == 0:
            self.emit("choose_dialog", player=0, value=0)
        self.complete_source()
        if self.mode != "empty_library":
            land = self.mode == "land_top"
            self.move(f"library_{self.owner}_0", "battlefield" if land else "hand")
        self.state()
        if self.mode == "land_top":
            self.emit("bind_permanent_copy", ref="opened_land", player=self.owner, index=0, value=self.rows[FOREST]["csvid"])
            self.observe("opened_land", "tapped", 0)
            self.emit("activate", ref="opened_land", value=1)
            self.emit("assert_stack")
            self.observe("opened_land", "tapped", 1)
            self.state([0, 0, 0, 1, 0, 0])

    def thicket(self):
        manual = self.mode == "order_human"
        chosen = self.mode in ("select_basic", "order_human")
        if manual:
            self.deck(self.owner, (BOLT, BEAR, FOREST, BOOK, DRAGON, ISLAND))
        elif self.mode != "next_green":
            self.deck(self.owner, (BOLT, FOREST))
        self.deck(self.foe, (MOUNTAIN, PLAINS))
        self.emit("turn", player=self.owner)
        self.fund(self.owner, ZERO)
        self.fund(self.foe, ZERO)
        if chosen:
            if self.owner == 0:
                self.emit("choose_trigger", ref="source", player=0)
            self.emit("choose_zone_card", player=self.owner, value=self.rows[FOREST]["csvid"])
            if self.owner == 0:
                self.emit("choose_dialog", player=0, value=0)
            if manual:
                for name in (DRAGON, BOOK, BEAR):
                    self.emit("choose_zone_card", player=self.owner, value=self.rows[name]["csvid"])
        self.emit("play_land", ref="source", value=1)
        self.resolve()
        self.move("source", "battlefield")
        if manual:
            self.library[self.owner] = [self.library[self.owner][i] for i in (2,5,4,3,1,0)]
        elif chosen:
            self.library[self.owner].reverse()
        self.observe("source", "tapped", 1)
        self.emit("can_activate", ref="source", value=0)
        self.state()
        if self.mode == "next_green":
            self.emit("advance_turn")
            self.emit("advance_turn")
            self.emit("untap_step", player=self.owner)
            self.observe("source", "tapped", 0)
            self.emit("can_activate", ref="source", value=1)
            self.emit("activate", ref="source", value=1)
            self.emit("assert_stack")
            self.observe("source", "tapped", 1)
            self.state([0, 0, 0, 1, 0, 0])

    def orator(self):
        reveals = self.mode in ("revealed_dragon", "decline_reveal")
        controls = self.mode in ("controlled_dragon", "dragon_removed_pending")
        killed = self.mode == "dragon_removed_pending"
        combat = self.mode == "defender_flight"
        self.deck(self.owner, (MOUNTAIN, ISLAND, FOREST))
        self.deck(self.foe, (FOREST, PLAINS))
        if reveals or controls:
            self.add("dragon", DRAGON, self.owner, "hand" if reveals else "battlefield")
        if killed:
            self.add("response", MURDER, self.foe, "hand")
        if combat:
            self.add("air", AIR, self.foe, "battlefield")
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, self.cost("source"))
        if reveals:
            self.emit("choose_optional_zone" if self.mode == "decline_reveal" else "choose_zone_card",
                      player=self.owner, value=self.rows[DRAGON]["csvid"])
            if self.mode == "revealed_dragon" and self.owner == 0:
                self.emit("choose_dialog", player=0, value=0)
        self.emit("cast", ref="source", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner)
        self.assert_top(self.owner)
        if killed:
            self.emit("announce_spell", ref="source")
            self.pending_cast("response", player=self.foe, target="dragon", response=True)
            self.emit("resolve_response")
            self.emit("refresh")
            self.emit("assert_choices_consumed")
            self.emit("assert_stack")
            self.move("source", "battlefield")
            self.move("response", "graveyard")
            self.move("dragon", "graveyard")
            self.inspect_source()
        else:
            self.complete_source()
        if controls or self.mode == "revealed_dragon":
            self.draw(self.owner, 1)
        self.state()
        if not combat:
            return
        self.emit("advance_turn")
        self.emit("advance_turn")
        self.emit("turn", player=self.owner)
        self.emit("begin_attackers")
        self.emit("attack", ref="source")
        self.observe("source", "attacking", 0)
        self.declaration_end()
        self.emit("begin_blockers")
        self.emit("advance_combat")
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")
        self.emit("advance_turn")
        self.emit("turn", player=self.foe)
        self.emit("begin_attackers")
        self.emit("attack", ref="air", value=1)
        self.declaration_end()
        self.complete_combat(attacker="air", blocker="source", legal=True)
        self.state()

    def surveil_and_draw(self, ref, player):
        self.pending_cast(ref, player=player)
        names = [self.objects[entry]["name"] for entry in self.library[player][:2]]
        for name in names:
            self.emit("choose_zone_card", player=player, value=self.rows[name]["csvid"])
        self.resolve()
        self.move(ref, "graveyard")
        for entry in self.library[player][:2]:
            self.move(entry, "graveyard")
        self.draw(player, 2)
        self.life[player] -= 2
        if player == self.owner:
            self.move("source", "hand")

    def campaign(self):
        recast = self.mode == "surveil_and_recast"
        opposing = self.mode == "opponent_surveil"
        cards = (FOREST, MOUNTAIN, ISLAND, PLAINS, SWAMP, BEAR, BOOK, DRAGON)
        self.deck(self.owner, cards)
        self.deck(self.foe, (RAIN,) + cards if opposing else cards)
        self.add("discardable", GIANT, self.foe, "hand")
        if recast:
            self.add("another_discard", GIANT, self.foe, "hand")
            self.add("rain", RAIN, self.owner, "hand")
        self.emit("discard_policy", value=1)
        self.begin_source()
        self.complete_source()
        self.draw(self.owner, 1)
        self.move("discardable", "graveyard")
        self.state()
        if opposing:
            self.emit("advance_turn")
            self.emit("untap_step", player=self.foe)
            self.emit("upkeep_step", player=self.foe)
            self.emit("draw_step", player=self.foe)
            self.emit("assert_choices_consumed")
            rain = self.library[self.foe][0]
            self.move(rain, "hand")
            self.emit("bind_hand_copy", ref=rain, player=self.foe, index=0, value=self.rows[RAIN]["csvid"])
            self.surveil_and_draw(rain, self.foe)
            self.state()
        elif recast:
            self.surveil_and_draw("rain", self.owner)
            self.state()
            self.emit("bind_hand_copy", ref="recast", player=self.owner, index=0, value=self.csvid)
            self.pending_cast("recast", player=self.owner, fee_ref="source")
            self.complete_source("recast")
            self.draw(self.owner, 1)
            self.move("another_discard", "graveyard")
            self.state()
        self.emit("discard_policy")


    def next_upkeep(self, player):
        self.emit("advance_turn")
        self.emit("untap_step", player=player)
        self.emit("upkeep_step", player=player)
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_stack")

    def finish_spell(self, ref, seat, *, target=None):
        self.pending_cast(ref, player=seat, target=target)
        self.resolve()
        self.move(ref, "graveyard")

    def delayed_aura(self):
        blocked = self.mode == "blocked_trample"
        removed = self.mode == "removed_before_draw"
        self.add("host", BEAR, self.owner, "battlefield")
        if blocked:
            self.add("blocker", GIANT, self.foe, "battlefield")
        if removed:
            self.add("removal", NATURALIZE, self.owner, "hand")
        self.begin_source(target="host")
        self.complete_source()
        self.observe("host", "power", 3 if self.csvid == FETISH else 2)
        self.observe("host", "toughness", 3 if self.csvid == FETISH else 2)
        self.state()
        if removed:
            self.finish_spell("removal", self.owner, target="source")
            self.move("source", "graveyard")
            self.observe("host", "power", 2)
            self.observe("host", "toughness", 2)
            self.state()
        self.next_upkeep(self.foe)
        self.draw(self.owner, 1)
        self.state()
        if self.csvid == FETISH:
            return
        self.emit("advance_turn")
        self.emit("turn", player=self.owner)
        self.emit("begin_attackers")
        self.emit("attack", ref="host", value=1)
        self.declaration_end()
        self.emit("begin_blockers")
        if blocked:
            self.emit("block", ref="blocker", other="host", value=1)
        self.emit("advance_combat")
        self.emit("refresh")
        self.observe("host", "power", 6 if blocked else 2)
        self.observe("host", "blocked", int(blocked))
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")
        self.life[self.foe] -= 3 if blocked else 2
        if blocked:
            for ref in ("host", "source", "blocker"):
                self.move(ref, "graveyard")
        self.state()

    def seal(self):
        restored = self.mode == "restore_target"
        self.add("corpse", BEAR, self.owner, "graveyard")
        self.add("raised", RAISE, self.owner, "hand")
        if restored:
            self.add("removal", NATURALIZE, self.owner, "hand")
        self.emit("turn", player=self.owner)
        self.fund(self.owner, self.cost("raised"))
        self.emit("can_cast", ref="raised", value=1)
        self.begin_source()
        self.complete_source()
        self.draw(self.owner, 1)
        self.state()
        self.fund(self.owner, self.cost("raised"))
        self.emit("can_cast", ref="raised", value=0)
        if restored:
            self.finish_spell("removal", self.owner, target="source")
            self.move("source", "graveyard")
            self.fund(self.owner, self.cost("raised"))
            self.emit("can_cast", ref="raised", value=1)
            self.emit("choose_zone", player=-1, value=0)
            self.finish_spell("raised", self.owner)
            self.move("corpse", "hand")
        self.state(ZERO if restored else self.cost("raised"))

    def web(self):
        removed = self.mode == "restored_untap"
        for player in (0, 1):
            self.add(f"factory_{player}", FACTORY, player, "battlefield", tapped=True)
            self.add(f"forest_{player}", FOREST, player, "battlefield", tapped=True)
        if removed:
            self.add("removal", NATURALIZE, self.owner, "hand")
        self.begin_source()
        self.complete_source()
        self.draw(self.owner, 1)
        if removed:
            self.finish_spell("removal", self.owner, target="source")
            self.move("source", "graveyard")
        for player in (self.foe, self.owner):
            self.emit("advance_turn")
            self.emit("untap_step", player=player)
            self.observe(f"factory_{player}", "tapped", int(not removed))
            self.observe(f"forest_{player}", "tapped", 0)
        self.state()

    def reverie(self):
        creature_aura = self.mode == "creature_aura"
        self.add("host", BEAR, self.owner, "battlefield")
        self.add("other_host", GIANT if creature_aura else FOREST, self.owner, "battlefield")
        self.add("other_aura", HOLY if creature_aura else WILD, self.owner, "hand")
        self.emit("turn", player=self.owner)
        self.finish_spell("other_aura", self.owner, target="other_host")
        self.move("other_aura", "battlefield")
        self.begin_source(target="host")
        self.complete_source()
        count = 2 if creature_aura else 1
        self.draw(self.owner, count)
        self.observe("host", "power", 2 + count)
        self.observe("host", "toughness", 2 + count)
        self.state()

    def flying_attack(self, *, power=None):
        self.start_combat(actor=self.owner, attacker="source")
        self.declaration_end()
        self.complete_combat(attacker="source", blocker="ground", power=power)
        self.state()

    def sandstone(self):
        behind = self.mode == "behind"
        for player, count in ((self.owner, 1 if behind else 2), (self.foe, 3 if behind else 2)):
            for n in range(count):
                self.add(f"spare_{player}_{n}", BOOK, player, "hand")
        if not behind:
            self.add("ground", BEAR, self.foe, "battlefield")
        self.begin_source()
        self.complete_source()
        self.draw(self.owner, 2 if behind else 0)
        self.state()
        if not behind:
            self.flying_attack()

    def bonds(self):
        expiry = self.mode == "cleanup_restores"
        self.add("hexproof", SCOUT, self.foe, "battlefield")
        self.add("indestructible", MYR, self.foe, "battlefield")
        self.add("first_kill", MURDER, self.owner, "hand")
        self.add("second_kill", MURDER, self.owner, "hand")
        self.begin_source()
        self.complete_source()
        self.draw(self.owner, 1)
        self.emit("can_target_creature", ref="first_kill", other="hexproof", value=0)
        self.fund(self.owner, [0, 0, 0, 1, 0, 0])
        self.emit("can_activate", ref="source", value=1)
        self.emit("activate", ref="source", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner)
        self.resolve()
        self.emit("can_target_creature", ref="first_kill", other="hexproof", value=1)
        if expiry:
            self.emit("cleanup")
            self.emit("turn", player=self.owner)
            self.emit("refresh")
            self.emit("can_target_creature", ref="first_kill", other="hexproof", value=0)
        else:
            self.finish_spell("first_kill", self.owner, target="hexproof")
            self.move("hexproof", "graveyard")
        self.finish_spell("second_kill", self.owner, target="indestructible")
        if not expiry:
            self.move("indestructible", "graveyard")
        else:
            self.observe("indestructible", "in_play", 1)
        self.state()

    def puzzleknot(self):
        paid = self.mode == "paid_activation"
        self.begin_source()
        self.complete_source()
        self.draw(self.owner, 1)
        self.life[self.owner] -= 1
        self.state()
        fee = [2, 1, 0, 0, 0, 0] if paid else [2, 0, 1, 0, 0, 0]
        self.fund(self.owner, fee)
        self.emit("can_activate", ref="source", value=int(paid))
        self.emit("activate", ref="source", value=int(paid))
        self.emit("assert_stack", value=int(paid))
        if paid:
            self.move("source", "graveyard")
            self.state()
            self.resolve()
            self.draw(self.owner, 1)
            self.life[self.owner] -= 1
        self.state(ZERO if paid else fee)

    def bolt(self):
        self.emit("choose_player", player=self.owner, value=self.foe)
        self.finish_spell("bolt", self.owner)
        self.life[self.foe] -= 3

    def drake(self):
        dynamic = self.mode == "actual_spell_changes_power"
        for ref, name, player, zone in (
            ("grave_spell", BOLT, self.owner, "graveyard"),
            ("exiled_spell", RAIN, self.owner, "exile"),
            ("grave_other", BEAR, self.owner, "graveyard"),
            ("exiled_other", BOOK, self.owner, "exile"),
            ("opposing_grave", BOLT, self.foe, "graveyard"),
            ("opposing_exile", RAIN, self.foe, "exile"),
        ):
            self.add(ref, name, player, zone)
        if dynamic:
            self.add("bolt", BOLT, self.owner, "hand")
        else:
            self.add("ground", BEAR, self.foe, "battlefield")
        self.begin_source()
        self.complete_source()
        self.draw(self.owner, 1)
        self.observe("source", "power", 2)
        self.state()
        if dynamic:
            self.bolt()
            self.observe("source", "power", 3)
            self.state()
        else:
            self.flying_attack(power=2)

    def reveler(self):
        spectacle = self.mode == "spectacle_dump"
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.add("disposable", BOOK, self.owner, "hand")
        if spectacle:
            self.add("another_disposable", BOOK, self.owner, "hand")
            self.add("bolt", BOLT, self.owner, "hand")
            self.bolt()
            self.fund(self.owner, [2, 1, 0, 0, 1, 0])
            self.emit("choose_dialog", player=self.owner, value=0)
            self.emit("cast", ref="source", value=1)
            if self.owner == 0:
                self.emit("assert_unused_choices")
            self.emit("assert_stack", value=1)
            self.mana("assert_mana", self.owner)
        else:
            self.pending_cast("source", player=self.owner)
        self.emit("discard_policy", value=1)
        self.complete_source()
        self.emit("discard_policy")
        self.move("disposable", "graveyard")
        if spectacle:
            self.move("another_disposable", "graveyard")
        self.draw(self.owner, 3 if spectacle else 1)
        self.state()

    def lookout(self):
        land = self.mode == "land_flying"
        mill = self.mode == "nonland_mill"
        if land:
            self.add("ground", BEAR, self.foe, "battlefield")
        self.begin_source()
        self.emit("choose_dialog", player=self.owner, value=0)
        if not land:
            self.emit("choose_dialog", player=self.owner, value=int(mill))
        self.complete_source(inspect=False)
        if land or mill:
            self.move(f"library_{self.owner}_0", "hand" if land else "graveyard")
        self.observe("source", "in_play", 1)
        self.observe("source", "plus1_counters", int(not land))
        for stat in ("power", "toughness"):
            self.observe("source", stat, int(self.oracle[self.name][stat]) + int(not land))
        self.state()
        if land:
            self.flying_attack()

    def rejuvenator(self):
        has_land = self.mode != "no_land_in_five"
        decline = self.mode == "decline_land"
        self.begin_source()
        if has_land:
            self.emit("choose_optional_zone" if decline else "choose_zone_card", player=self.owner,
                      value=self.rows[FOREST]["csvid"])
            if not decline and self.owner == 0:
                self.emit("choose_dialog", player=self.owner)
        else:
            self.emit("choose_zone", player=self.owner, value=-1)
        self.complete_source()
        if has_land and not decline:
            self.move(f"library_{self.owner}_2", "battlefield")
            self.emit("bind_permanent_copy", ref="fetched", player=self.owner, value=self.rows[FOREST]["csvid"])
            self.observe("fetched", "tapped", 1)
            self.emit("can_activate", ref="fetched", value=0)
        self.receipts()
        self.assert_top(self.foe)
        self.emit("assert_zone", player=self.owner, zone=ZONES["library"], value=len(self.library[self.owner]))
        first = ISLAND if has_land else FOREST
        self.emit("assert_zone_card", player=self.owner, zone=ZONES["library"], value=self.rows[first]["csvid"])

    def run(self):
        pilot = {ORACLE: self.coiling, THICKET: self.thicket, ORATOR: self.orator, CAMPAIGN: self.campaign}
        if self.csvid in pilot:
            pilot[self.csvid]()
            return
        if self.csvid == LOOKOUT:
            deck = (FOREST if self.mode == "land_flying" else BOLT, ISLAND, PLAINS)
        elif self.csvid == REJUVENATOR:
            deck = ((BOLT, BEAR, GIANT, BOOK, DRAGON, FOREST) if self.mode == "no_land_in_five"
                    else (BOLT, BEAR, FOREST, BOOK, DRAGON, ISLAND))
        else:
            deck = (ISLAND, PLAINS, SWAMP, MOUNTAIN)
        self.deck(self.owner, deck)
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        followers = {
            FURY: self.delayed_aura, SEAL: self.seal, WEB: self.web, FETISH: self.delayed_aura,
            REVERIE: self.reverie, SANDSTONE: self.sandstone, BONDS: self.bonds, PUZZLEKNOT: self.puzzleknot,
            DRAKE: self.drake, REVELER: self.reveler, LOOKOUT: self.lookout, REJUVENATOR: self.rejuvenator,
        }
        followers[self.csvid]()



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
