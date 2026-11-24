"""Entry triggers and card selection scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "entry_trigger_cards_and_selection"
PROFILE = Path(__file__).with_name("data") / "entry_trigger_cards_and_selection_wave8_profiles.json"
INSPECTOR, REFINER, MULLDRIFTER, WELLSPRING = 15990, 16788, 1695, 11462
EXPLORERS, PRISM, SEAS, PYKNITE, LATCHKEY, PIGEONS = 15791, 11021, 10675, 2072, 9376, 2266
DREAM, SUN, KALEIDO, LAMPLIGHTER, WOOD, VIAL = 9770, 18166, 10160, 15881, 16020, 15074
MODES = {
    INSPECTOR: ("entry_clue", "cash_clue", "short_clue", "countered"),
    REFINER: ("paid_entry", "spend_energy", "bounce_recast", "countered"),
    MULLDRIFTER: ("normal_flying", "evoke_exact", "evoke_choice", "evoke_countered"),
    WELLSPRING: ("entry_to_death", "exile", "bounce_recast", "countered", "stolen_death"),
    EXPLORERS: ("entry_clue", "cash_clue", "short_clue", "countered"),
    PRISM: ("filter_blue", "insufficient"),
    SEAS: ("island_conversion", "countered_aura"),
    PYKNITE: ("next_upkeep", "removed_before_upkeep"),
    LATCHKEY: ("no_prowl", "paid_prowl", "normal_after_hit"),
    PIGEONS: ("next_upkeep", "removed_before_upkeep", "flying"),
    DREAM: ("blue_prior", "rival_blue", "flying_only"),
    SUN: ("retain_mana", "restore_search"),
    KALEIDO: ("five_colors", "four_short"),
    LAMPLIGHTER: ("draw_discard", "no_other_zombie", "decline_draw"),
    WOOD: ("own_land_mana", "foe_land_mana"),
    VIAL: ("deny_attack", "deny_block"),
}
PREFIX = "EntryTriggerCardsAndSelectionRecipe"
SEED = 3152
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP = "Forest", "Island", "Mountain", "Plains", "Swamp"
CLUE, CUB, BEAR = "Clue", "Longtusk Cub", "Grizzly Bears"
COUNTER, BOUNCE, REMOVAL, REVOKE, STEAL = "Counterspell", "Boomerang", "Naturalize", "Revoke Existence", "Steal Artifact"
WITNESSES = {FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP, CLUE, CUB, BEAR, COUNTER, BOUNCE, REMOVAL, REVOKE, STEAL}

STINGER, MISTS, WILDS, ZOMBIE, GIANT, MURDER = (
    "Nightshade Stinger", "Reach Through Mists", "Evolving Wilds",
    "Scathe Zombies", "Hill Giant", "Murder",
)
WITNESSES |= {STINGER, MISTS, WILDS, ZOMBIE, GIANT, MURDER}
ZONE_NAMES = {code: name for name, code in ZONES.items()}
FOLLOWERS = (EXPLORERS, PRISM, SEAS, PYKNITE, LATCHKEY, PIGEONS, DREAM, SUN, KALEIDO, LAMPLIGHTER, WOOD, VIAL)
SINGLE_SEAT = {(LAMPLIGHTER, "decline_draw"): 0}
GAPS = {
    WOOD: ["The granted mana activation uses the real attached provider. Discovery in the UI is unobserved."],
    LAMPLIGHTER: ["The human optional stop is exercised. AI discretionary decline is unobserved."],
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
            for seat in ((SINGLE_SEAT[shape["csvid"], mode],) if (shape["csvid"], mode) in SINGLE_SEAT else (0, 1))]


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
                    or row.get("kind") != "card" or oracle[name].get("layout") != ("token" if name == CLUE else "normal")):
                raise ValueError("changed attack source or witness: " + name)
            if group == "sources":
                if not str(row.get("handler", "")).startswith("manalinkeh_card_"):
                    raise ValueError("implemented attack primary required")
                classify(oracle[name], row["csvid"])
    if (oracle[CLUE]["oracle_text"] != "{2}, Sacrifice this token: Draw a card."
            or not oracle[CUB]["oracle_text"].endswith("Pay {E}{E}: Put a +1/+1 counter on this creature.")
            or oracle[COUNTER]["oracle_text"] != "Counter target spell."
            or oracle[BOUNCE]["oracle_text"] != "Return target permanent to its owner's hand."
            or oracle[REMOVAL]["oracle_text"] != "Destroy target artifact or enchantment."
            or oracle[REVOKE]["oracle_text"] != "Exile target artifact or enchantment."
            or oracle[STEAL]["oracle_text"] != "Enchant artifact\nYou control enchanted artifact."
            or not oracle[STINGER]["type_line"].endswith("Faerie Rogue")
            or oracle[MISTS]["oracle_text"] != "Draw a card."
            or not oracle[ZOMBIE]["type_line"].endswith("Zombie")):
        raise ValueError("entry gameplay witness clauses changed")


class Board:
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        self.csvid, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.life = [20, 20]
        self.library = [[], []]
        self.energy = [0, 0]
        self.add("source", name, owner, "hand")

    def add(self, ref, name, seat, zone):
        self.setup.append(card(name, ref, seat, zone))
        self.objects[ref] = dict(name=name, player=seat, zone=zone)
        if zone == "library":
            self.library[seat].append(ref)

    def emit(self, op, ref="", other="", prop="", *, player=0, value=0, zone=0, index=0, color=0):
        if op in {"untap_step", "upkeep_step"}:
            self.steps.append(step(op, player=player))
            return
        fields = dict(ref=ref, other=other, property=prop, player=player, value=value, index=index, color=color)
        values = {key: item for key, item in fields.items() if item or key == "value"}
        if op in {"set_mana", "assert_mana"}:
            values.update(player=player, color=color)
        elif op.startswith("assert_zone"):
            values.update(player=player, zone=ZONE_NAMES[zone], index=index)
        elif op in {"choose_trigger_source", "choose_player", "choose_color", "assert_matching_permanents",
                    "bind_permanent_copy", "bind_hand_copy", "assert_energy"}:
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
            if stat in self.oracle[self.name]:
                self.observe("source", stat, int(self.oracle[self.name][stat]))

    def receipts(self, pools=(ZERO, ZERO), *, omit_battlefield_ids=()):
        for seat in (0, 1):
            self.emit("assert_life", player=seat, value=self.life[seat])
            self.mana("assert_mana", seat, pools[seat])
            objects = [obj for obj in self.objects.values() if obj["player"] == seat]
            counts = Counter((obj["name"], obj["zone"]) for obj in objects)
            for name in sorted({obj["name"] for obj in objects}):
                for zone, code in ZONES.items():
                    if zone == "battlefield" and (seat, name) in omit_battlefield_ids:
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

    def state(self, owner_pool=ZERO, *, omit_battlefield_ids=()):
        self.receipts(self.pools(owner_pool), omit_battlefield_ids=omit_battlefield_ids)
        for player in (0, 1):
            self.assert_top(player)
            self.emit("assert_energy", player=player, value=self.energy[player])

    def token_count(self, count):
        for player in (0, 1):
            self.emit("assert_zone_multiplicity", player=player, zone=1, index=self.rows[CLUE]["csvid"],
                      value=count if player == self.owner else 0)

    def clue_type(self):
        for prop, value in (("token", 1), ("artifact", 1), ("color", 0), ("subtype_Clue", 1)):
            self.emit("assert_matching_permanents", player=self.owner,
                      index=self.rows[CLUE]["csvid"], prop=prop, value=value)

    def assert_unresolved_entry(self):
        self.assert_top(self.owner)
        self.assert_top(self.foe)
        for player in (0, 1):
            self.emit("assert_energy", player=player, value=self.energy[player])
        if self.csvid in (INSPECTOR, EXPLORERS, WOOD):
            self.token_count(0)

    def enter(self, *, cast_cost=None, spent=None, mode=None, countered=False, destination="battlefield",
              target=None, before_resolve=None):
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        fee = self.cost("source") if cast_cost is None else cast_cost
        paid = fee if spent is None else spent
        left = [before - amount for before, amount in zip(fee, paid)]
        assert all(amount >= 0 for amount in left)
        self.fund(self.owner, fee)
        if mode is not None:
            self.emit("choose_dialog", player=self.owner, value=mode)
        if target is not None:
            self.emit("choose_target", ref=target, player=self.owner)
        self.emit("cast", ref="source", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner, left)
        self.assert_unresolved_entry()
        if countered:
            self.emit("announce_spell", ref="source")
            self.fund(self.foe, self.cost("counter"))
            self.emit("cast_response", ref="counter", value=1)
            self.emit("assert_stack", value=2)
            self.mana("assert_mana", self.foe)
            self.emit("resolve_response")
            self.emit("refresh")
            self.emit("assert_choices_consumed")
            self.emit("assert_stack")
            self.move("counter", "graveyard")
            self.move("source", "graveyard")
        else:
            if before_resolve is not None:
                before_resolve()
            self.resolve()
            self.move("source", destination)
            if destination == "battlefield":
                self.inspect_source()
        return left

    def removal(self, ref, *, player=None, target="source"):
        seat = self.foe if player is None else player
        self.emit("turn", player=seat)
        self.fund(seat, self.cost(ref))
        self.emit("choose_target", ref=target, player=seat)
        self.emit("cast", ref=ref, value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", seat)
        self.resolve()
        self.move(ref, "graveyard")

    def inspector(self):
        countered = self.mode == "countered"
        if countered:
            self.add("counter", COUNTER, self.foe, "hand")
        self.enter(countered=countered)
        self.token_count(0 if countered else 1)
        if countered:
            self.state()
            return
        self.clue_type()
        self.state()
        if self.mode not in ("cash_clue", "short_clue"):
            return
        paid = self.mode == "cash_clue"
        self.emit("bind_permanent_copy", ref="clue", player=self.owner, index=0, value=self.rows[CLUE]["csvid"])
        fee = [2 if paid else 1, 0, 0, 0, 0, 0]
        self.fund(self.owner, fee)
        self.emit("can_activate", ref="clue", value=int(paid))
        self.emit("activate", ref="clue", value=int(paid))
        self.emit("assert_stack", value=int(paid))
        self.token_count(int(not paid))
        self.assert_top(self.owner)
        if paid:
            self.mana("assert_mana", self.owner)
            self.resolve()
            self.draw(self.owner, 1)
        self.token_count(int(not paid))
        self.state(ZERO if paid else fee)

    def refiner(self):
        countered = self.mode == "countered"
        replay = self.mode == "bounce_recast"
        if countered:
            self.add("counter", COUNTER, self.foe, "hand")
        if replay:
            self.add("bounce", BOUNCE, self.foe, "hand")
        if self.mode == "spend_energy":
            self.add("cub", CUB, self.owner, "battlefield")
        self.enter(countered=countered)
        if not countered:
            self.draw(self.owner, 1)
            self.energy[self.owner] += 2
        self.state()
        if replay:
            self.removal("bounce")
            self.move("source", "hand")
            self.state()
            self.emit("bind_hand_copy", ref="recast", player=self.owner, index=0, value=self.csvid)
            self.recast(draw=1, energy=2)
        elif self.mode == "spend_energy":
            self.emit("can_activate", ref="cub", value=1)
            self.emit("activate", ref="cub", value=1)
            self.emit("assert_stack", value=1)
            self.energy[self.owner] -= 2
            self.emit("assert_energy", player=self.owner, value=0)
            self.observe("cub", "plus1_counters", 0)
            self.resolve()
            self.observe("cub", "plus1_counters", 1)
            self.observe("cub", "power", 3)
            self.state()

    def recast(self, *, draw, energy=0):
        self.emit("turn", player=self.owner)
        self.fund(self.owner, self.cost("source"))
        self.emit("cast", ref="recast", value=1)
        self.emit("assert_stack", value=1)
        self.assert_unresolved_entry()
        self.resolve()
        self.move("source", "battlefield")
        self.draw(self.owner, draw)
        self.energy[self.owner] += energy
        self.state()

    def mulldrifter(self):
        normal = self.mode == "normal_flying"
        countered = self.mode == "evoke_countered"
        menu = self.mode == "evoke_choice"
        if normal:
            self.add("blocker", BEAR, self.foe, "battlefield")
        if countered:
            self.add("counter", COUNTER, self.foe, "hand")
        evoke = [2, 0, 1, 0, 0, 0]
        left = self.enter(
            cast_cost=self.cost("source") if normal or menu else evoke,
            spent=None if normal else evoke, mode=0 if normal else 1 if menu else None,
            countered=countered, destination="battlefield" if normal else "graveyard",
        )
        if not countered:
            self.draw(self.owner, 2)
        self.state(left)
        if normal:
            self.start_combat(actor=self.owner, attacker="source")
            self.declaration_end()
            self.complete_combat(attacker="source", blocker="blocker", legal=False)
            self.state()

    def wellspring(self):
        mode = self.mode
        if mode == "countered":
            self.add("counter", COUNTER, self.foe, "hand")
            self.enter(countered=True)
            self.state()
            return
        if mode == "stolen_death":
            self.add("steal", STEAL, self.foe, "hand")
        witness = BOUNCE if mode == "bounce_recast" else REVOKE if mode == "exile" else REMOVAL
        self.add("leave", witness, self.owner if mode == "stolen_death" else self.foe, "hand")
        self.enter()
        self.draw(self.owner, 1)
        self.state()
        if mode == "stolen_death":
            self.removal("steal")
            self.move("steal", "battlefield")
            self.objects["source"]["player"] = self.foe
            self.emit("bind_permanent_copy", ref="borrowed", player=self.foe, index=0, value=self.csvid)
            self.emit("assert_zone_multiplicity", player=self.owner, zone=1, index=self.csvid)
            self.state()
            self.removal("leave", player=self.owner, target="borrowed")
            self.objects["source"]["player"] = self.owner
            self.move("source", "graveyard")
            self.move("steal", "graveyard")
            self.draw(self.foe, 1)
            self.emit("assert_zone_multiplicity", player=self.foe, zone=3, index=self.csvid)
            self.state()
            return
        self.removal("leave")
        if mode == "bounce_recast":
            self.move("source", "hand")
            self.state()
            self.emit("bind_hand_copy", ref="recast", player=self.owner, index=0, value=self.csvid)
            self.recast(draw=1)
        elif mode == "exile":
            self.move("source", "exile")
            self.state()
        else:
            self.move("source", "graveyard")
            self.draw(self.owner, 1)
            self.state()

    def prism(self):
        paid = self.mode == "filter_blue"
        self.enter()
        self.draw(self.owner, 1)
        self.state()
        self.fund(self.owner, [int(paid), 0, 0, 0, 0, 0])
        self.emit("can_activate", ref="source", value=int(paid))
        if paid:
            self.emit("choose_color", player=self.owner, value=2)
        self.emit("activate", ref="source", value=int(paid))
        self.emit("assert_unused_choices")
        self.emit("assert_stack")
        self.observe("source", "tapped", int(paid))
        self.state([0, 0, int(paid), 0, 0, 0])

    def seas(self):
        countered = self.mode == "countered_aura"
        self.add("land", FOREST, self.owner, "battlefield")
        if countered:
            self.add("counter", COUNTER, self.foe, "hand")
        self.enter(target="land", countered=countered)
        omitted = ()
        if not countered:
            self.draw(self.owner, 1)
            # The zone reader exposes the derived Island IID, not the
            # enchanted card's original identity. Observe the actual
            # permanent and its converted mana instead of that storage ID.
            omitted = ((self.owner, FOREST), (self.owner, ISLAND))
            self.observe("land", "in_play", 1)
            self.emit("assert_zone", player=self.owner, zone=1, value=2)
        self.state(omit_battlefield_ids=omitted)
        self.emit("can_activate", ref="land", value=1)
        self.emit("activate", ref="land", value=1)
        self.emit("assert_stack")
        self.observe("land", "tapped", 1)
        if not countered:
            self.observe("land", "in_play", 1)
            self.emit("assert_zone", player=self.owner, zone=1, value=2)
        self.state([0, 0, int(not countered), int(countered), 0, 0], omit_battlefield_ids=omitted)

    def delayed_draw(self):
        removed = self.mode == "removed_before_upkeep"
        flies = self.mode == "flying"
        if removed:
            self.add("kill", MURDER, self.foe, "hand")
        if flies:
            self.add("blocker", BEAR, self.foe, "battlefield")
        self.enter()
        self.state()
        if removed:
            self.removal("kill")
            self.move("source", "graveyard")
            self.emit("turn", player=self.owner)
            self.state()
        self.emit("advance_turn")
        self.emit("untap_step", player=self.foe)
        self.state()
        self.emit("upkeep_step", player=self.foe)
        self.emit("assert_unused_choices")
        self.emit("assert_stack")
        self.draw(self.owner, 1)
        self.state()
        if flies:
            self.emit("advance_turn")
            self.emit("turn", player=self.owner)
            self.emit("begin_attackers")
            self.emit("attack", ref="source", value=1)
            self.declaration_end()
            self.complete_combat(attacker="source", blocker="blocker")
            self.state()

    def latchkey(self):
        qualifies = self.mode != "no_prowl"
        pays = self.mode == "paid_prowl"
        if qualifies:
            self.add("faerie", STINGER, self.owner, "battlefield")
        else:
            self.add("blocker", BEAR, self.foe, "battlefield")
        if qualifies:
            self.emit("turn", player=self.owner)
            self.fund(self.owner, ZERO)
            self.fund(self.foe, ZERO)
            self.start_combat(actor=self.owner, attacker="faerie")
            self.declaration_end()
            self.complete_combat(attacker="faerie")
        self.enter(cast_cost=[2, 0, 1, 0, 0, 0] if pays else None,
                   mode=1 if self.mode == "normal_after_hit" else None)
        if pays:
            self.draw(self.owner, 1)
        self.state()
        if not qualifies:
            self.start_combat(actor=self.owner, attacker="source")
            self.declaration_end()
            self.complete_combat(attacker="source", blocker="blocker")
            self.state()

    def blue_spell(self, player):
        self.add("prior_spell", MISTS, player, "hand")
        self.emit("turn", player=self.owner)
        self.fund(1 - player, ZERO)
        self.fund(player, self.cost("prior_spell"))
        self.emit("cast", ref="prior_spell", value=1)
        self.emit("assert_stack", value=1)
        self.resolve()
        self.move("prior_spell", "graveyard")
        self.draw(player, 1)
        self.state()

    def dream(self):
        flying = self.mode == "flying_only"
        if flying:
            self.add("blocker", BEAR, self.foe, "battlefield")
        else:
            self.blue_spell(self.owner if self.mode == "blue_prior" else self.foe)
        self.enter()
        if self.mode == "blue_prior":
            self.draw(self.owner, 1)
        self.state()
        if flying:
            self.start_combat(actor=self.owner, attacker="source")
            self.declaration_end()
            self.complete_combat(attacker="source", blocker="blocker")
            self.state()

    def sun(self):
        restores = self.mode == "restore_search"
        for player in (0, 1):
            self.add(f"search_{player}", WILDS, player, "battlefield")
            self.add(f"forest_{player}", FOREST, player, "battlefield")
        if restores:
            self.add("removal", REMOVAL, self.foe, "hand")
        self.emit("refresh")
        for player in (0, 1):
            self.emit("can_activate", ref=f"search_{player}", value=1)
        self.enter()
        self.draw(self.owner, 1)
        self.state()
        for player in (0, 1):
            self.emit("can_activate", ref=f"search_{player}", value=0)
            self.emit("can_activate", ref=f"forest_{player}", value=1)
        if restores:
            self.removal("removal")
            self.move("source", "graveyard")
            for player in (0, 1):
                self.emit("can_activate", ref=f"search_{player}", value=1)
            self.state()
            return
        for player in (0, 1):
            self.emit("activate", ref=f"forest_{player}", value=1)
            self.observe(f"forest_{player}", "tapped", 1)
            self.mana("assert_mana", player, [0, 0, 0, 1, 0, 0])
        self.emit("assert_stack")
        self.mana("assert_mana", self.owner, [0, 0, 0, 1, 0, 0])
        self.mana("assert_mana", self.foe, [0, 0, 0, 1, 0, 0])

    def kaleidostone(self):
        paid = self.mode == "five_colors"
        self.enter()
        self.draw(self.owner, 1)
        self.state()
        fee = [5 if paid else 4, 0, 0, 0, 0, 0]
        self.fund(self.owner, fee)
        self.emit("can_activate", ref="source", value=int(paid))
        self.emit("activate", ref="source", value=int(paid))
        self.emit("assert_stack")
        if paid:
            self.move("source", "graveyard")
        else:
            self.observe("source", "tapped", 0)
        self.state([0, 1, 1, 1, 1, 1] if paid else fee)

    def lamp(self):
        eligible = self.mode != "no_other_zombie"
        draw = self.mode == "draw_discard"
        if eligible:
            self.add("zombie", ZOMBIE, self.owner, "battlefield")
        else:
            self.add("rival_zombie", ZOMBIE, self.foe, "battlefield")

        def choices():
            self.emit("discard_policy", value=1)
            if draw and self.owner == 0:
                self.emit("choose_trigger", ref="source", player=0)

        self.enter(before_resolve=choices)
        self.emit("discard_policy")
        if draw:
            first = self.library[self.owner][0]
            self.move(first, "hand")
            self.move(first, "graveyard")
        self.state()

    def wood(self):
        player = self.owner if self.mode == "own_land_mana" else self.foe
        self.add("land", FOREST, player, "battlefield")
        if player != self.owner:
            # Enchant land has no controller restriction. Probe the sole
            # legal target before an illegal engine rejection can strand input.
            self.emit("turn", player=self.owner)
            self.fund(self.foe, ZERO)
            fee = self.cost("source")
            self.fund(self.owner, fee)
            self.observe("land", "in_play", 1)
            self.emit("can_cast", ref="source", value=1)
            self.observe("source", "in_hand", 1)
            self.token_count(0)
            self.state(fee)
            return
        self.enter(target="land")
        self.token_count(1)
        self.clue_type()
        self.state()
        self.emit("can_activate_granted_mana", ref="land", other="source", value=1)
        self.emit("choose_color", player=player, value=2)
        self.emit("activate_granted_mana", ref="land", other="source", value=1)
        self.emit("assert_unused_choices")
        self.emit("assert_stack")
        self.observe("land", "tapped", 1)
        for seat in (0, 1):
            self.mana("assert_mana", seat, [0, 0, 2, 0, 0, 0] if seat == player else ZERO)
        self.token_count(1)

    def vial(self):
        attack = self.mode == "deny_attack"
        self.add("target_creature", BEAR, self.foe, "battlefield")
        if not attack:
            self.add("ally_attacker", GIANT, self.owner, "battlefield")
        self.enter()
        self.draw(self.owner, 1)
        self.state()
        self.emit("advance_turn")
        if not attack:
            self.emit("advance_turn")
        self.emit("turn", player=self.foe if attack else self.owner)
        self.fund(self.owner, [1, 0, 0, 0, 0, 0])
        self.emit("choose_target", ref="target_creature", player=self.owner)
        self.emit("can_activate", ref="source", value=1)
        self.emit("activate", ref="source", value=1)
        self.emit("assert_stack", value=1)
        self.move("source", "graveyard")
        self.assert_zone_ref("source", "battlefield", 0)
        self.resolve()
        self.emit("begin_attackers")
        if attack:
            self.emit("attack", ref="target_creature", value=0)
            self.observe("target_creature", "attacking", 0)
            self.declaration_end()
        else:
            self.emit("attack", ref="ally_attacker", value=1)
            self.declaration_end()
            self.complete_combat(attacker="ally_attacker", blocker="target_creature")
        self.state()

    def run(self):
        self.deck(self.owner, (MOUNTAIN, ISLAND, FOREST, PLAINS, SWAMP))
        self.deck(self.foe, (SWAMP, PLAINS, FOREST, ISLAND, MOUNTAIN))
        scenarios = {
            INSPECTOR: self.inspector, REFINER: self.refiner, MULLDRIFTER: self.mulldrifter,
            WELLSPRING: self.wellspring, EXPLORERS: self.inspector, PRISM: self.prism,
            SEAS: self.seas, PYKNITE: self.delayed_draw, LATCHKEY: self.latchkey,
            PIGEONS: self.delayed_draw, DREAM: self.dream, SUN: self.sun, KALEIDO: self.kaleidostone,
            LAMPLIGHTER: self.lamp, WOOD: self.wood, VIAL: self.vial,
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
