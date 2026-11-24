"""Annihilator and two-player Myriad scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "attack_trigger_follow_on_annihilator_and_myriad"
PROFILE = Path(__file__).with_name("data") / "attack_trigger_follow_on_annihilator_and_myriad_wave8_profiles.json"
ARTISAN, KOZILEK, HAND, BANSHEE = 10870, 10974, 10948, 15498
BETRAYS, PATHRAZER, ULAMOG, VIPER, HERALD, WARCHIEF = 10960, 11013, 11070, 15502, 15519, 15547
MODES = {
    ARTISAN: ("return_cast_attack", "short_defender", "no_creature", "decline_return"),
    KOZILEK: ("draw_cast_attack", "murder_shuffle", "discard_shuffle", "short_defender"),
    HAND: ("cast_full_attack", "spawn_cost_attack", "spawn_short", "normal_with_spawn"),
    BANSHEE: ("hit_discard", "blocked", "empty_hand", "other_attacker"),
    BETRAYS: ("annihilator_take", "sacrifice_tokens", "destroy_not_sacrifice", "own_sacrifice"),
    PATHRAZER: ("annihilator_short", "one_blocker", "two_blockers", "three_blockers"),
    ULAMOG: ("cast_destroy_attack", "indestructible", "discard_shuffle", "countered_cast_trigger"),
    VIPER: ("draw_hit", "decline_draw", "blocked", "other_attacker"),
    HERALD: ("flying_vigilance", "air_trade", "defensive_flight"),
    WARCHIEF: ("fresh_haste", "fresh_blocked", "other_attacker"),
}
PREFIX = "AttackFollowOnAnnihilatorAndMyriadRecipe"
SEED = 3062
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
BEAR, BOOK, BOLT, GIANT = "Grizzly Bears", "Spellbook", "Lightning Bolt", "Hill Giant"
FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP = "Forest", "Island", "Mountain", "Plains", "Swamp"
INVASION, SPAWN, MURDER, ROT = "Skittering Invasion", "Eldrazi Spawn", "Murder", "Mind Rot"
WITNESSES = {BEAR, BOOK, BOLT, GIANT, FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP,
             INVASION, SPAWN, MURDER, ROT}

PET, AIR, COUNTERSPELL = "Blood Vassal", "Air Elemental", "Counterspell"
WITNESSES |= {PET, AIR, COUNTERSPELL}
ZONE_NAMES = {code: name for name, code in ZONES.items()}
FOLLOWERS = (BETRAYS, PATHRAZER, ULAMOG, VIPER, HERALD, WARCHIEF)
SINGLE_SEAT = {(ARTISAN, "decline_return"): 0, (VIPER, "decline_draw"): 0}
MYRIAD_GAP = ["The two-player duel has no additional defending opponent. Multiplayer copy creation and exile are unobserved."]
GAPS = {
    BANSHEE: MYRIAD_GAP, VIPER: MYRIAD_GAP, HERALD: MYRIAD_GAP, WARCHIEF: MYRIAD_GAP,
    PATHRAZER: ["The three simultaneous blocker assignments are observed before combat damage ordering."],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get("layout") != "normal" or record.get("selected_face")
            or record.get("card_faces") or not (set(record.get("keywords", ())) & {"Annihilator", "Myriad"})):
        raise ValueError("whole pinned Annihilator and Myriad record required")
    return dict(csvid=csvid, printed_mana=record["mana_cost"], rules=record["oracle_text"])


def variants(shape):
    return [f"{mode}_{seat}" for mode in MODES[shape["csvid"]]
            for seat in ((SINGLE_SEAT[shape["csvid"], mode],) if (shape["csvid"], mode) in SINGLE_SEAT else (0, 1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError("distinct supported Annihilator and Myriad scenarios required")


def spec(declaration, record):
    source = profiles()["sources"].get(record.get("name"))
    if not source or not exact(source["oracle"], record):
        raise ValueError("exact pinned Annihilator and Myriad primary required")
    shape = classify(record, source["registry"]["csvid"])
    if (declaration.get("family") != FAMILY or type(declaration.get("version")) is not int
            or declaration["version"] != 2 or declaration.get("oracle_text") != record["oracle_text"]
            or not exact(declaration.get("params"), shape)):
        raise ValueError("versioned whole Annihilator and Myriad declaration required")
    validate_variants(shape, declaration.get("variants"))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {"sources", "witnesses"} or set(pins["witnesses"]) != WITNESSES:
        raise ValueError("independent exact Annihilator and Myriad witnesses required")
    if {pin["registry"]["csvid"] for pin in pins["sources"].values()} != set(MODES):
        raise ValueError("exact Annihilator and Myriad primaries required")
    for group in ("sources", "witnesses"):
        for name, pin in pins[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin["oracle"]) or not exact(row, pin["registry"])
                    or row.get("kind") != "card" or oracle[name].get("layout") != ("token" if name == SPAWN else "normal")):
                raise ValueError("changed attack source or witness: " + name)
            if group == "sources":
                if not str(row.get("handler", "")).startswith("manalinkeh_card_"):
                    raise ValueError("implemented attack primary required")
                classify(oracle[name], row["csvid"])
    if (not oracle[INVASION]["oracle_text"].startswith("Create five 0/1 colorless Eldrazi Spawn creature tokens.")
            or oracle[SPAWN]["oracle_text"] != "Sacrifice this creature: Add {C}."
            or oracle[MURDER]["oracle_text"] != "Destroy target creature."
            or oracle[ROT]["oracle_text"] != "Target player discards two cards."
            or oracle[AIR]["oracle_text"] != "Flying"
            or oracle[COUNTERSPELL]["oracle_text"] != "Counter target spell."):
        raise ValueError("Annihilator and Myriad witnesses changed")


class Board:
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        self.csvid, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.life = [20, 20]
        self.library = [[], []]
        self.add("source", name, owner, "hand")

    def add(self, ref, name, seat, zone):
        self.setup.append(card(name, ref, seat, zone))
        self.objects[ref] = dict(name=name, player=seat, zone=zone)
        if zone == "library":
            self.library[seat].append(ref)

    def emit(self, op, ref="", other="", prop="", *, player=0, value=0, zone=0, index=0, color=0):
        fields = dict(ref=ref, other=other, property=prop, player=player, value=value, index=index, color=color)
        values = {key: item for key, item in fields.items() if item or key == "value"}
        if op in {"set_mana", "assert_mana"}:
            values.update(player=player, color=color)
        elif op.startswith("assert_zone"):
            values.update(player=player, zone=ZONE_NAMES[zone], index=index)
        elif op in {"choose_trigger_source", "choose_player", "choose_zone_card", "choose_optional_zone",
                    "assert_shuffle_count", "bind_permanent_copy"}:
            values["player"] = player
        if op == "bind_permanent_copy":
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
        if ref == "source" and self.csvid in (BETRAYS, PATHRAZER, ULAMOG):
            required = 12 if self.csvid == BETRAYS else 11
            if record["mana_cost"] != "{" + str(required) + "}":
                raise ValueError("large generic Annihilator cost changed")
            return [required, 0, 0, 0, 0, 0]
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

    def enter_paid_source(self, *, choices=None, pending=None, cost=None):
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, self.cost("source") if cost is None else cost)
        if choices is not None:
            choices()
        self.emit("cast", ref="source", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner)
        if pending is not None:
            pending()
        self.resolve()
        self.move("source", "battlefield")
        self.inspect_source()

    def sacrifice_witnesses(self, quantity):
        witnesses = (FOREST, BEAR, BOOK, GIANT)
        refs = []
        self.add("own_untouched", MOUNTAIN, self.owner, "battlefield")
        for index, card in enumerate(witnesses[:quantity]):
            ref = f"defending_{index}"
            self.add(ref, card, self.foe, "battlefield")
            refs.append(ref)
        return refs

    def attack_annihilate(self, refs):
        self.start_combat(actor=self.owner, attacker="source")
        if self.foe == 0:
            for ref in refs:
                self.emit("choose_target", ref=ref, player=self.foe)
        self.declaration_end()
        for ref in refs:
            self.move(ref, "graveyard")
        self.receipts()
        self.complete_combat(attacker="source")
        self.receipts()

    def artisan(self):
        mode = self.mode
        target = mode in ("return_cast_attack", "decline_return")
        quantity = 1 if mode == "short_defender" else 2
        refs = self.sacrifice_witnesses(quantity)
        if target:
            self.add("returnable", BEAR, self.owner, "graveyard")
        self.add("own_noncreature_grave", BOLT, self.owner, "graveyard")
        self.add("rival_creature_grave", GIANT, self.foe, "graveyard")

        def choices():
            if target:
                op = "choose_optional_zone" if mode == "decline_return" else "choose_zone_card"
                self.emit(op, player=self.owner, value=self.rows[BEAR]["csvid"])

        def pending():
            if mode == "return_cast_attack":
                self.move("returnable", "battlefield")
                self.assert_zone_ref("returnable", "graveyard", 0)
                self.assert_zone_ref("returnable", "battlefield", 1)
            elif target:
                self.emit("assert_forced_zone_choices")
                self.assert_zone_ref("returnable", "graveyard", 1)
                self.assert_zone_ref("returnable", "battlefield", 0)
            self.assert_zone_ref("own_noncreature_grave", "graveyard", 1)
            self.assert_zone_ref("rival_creature_grave", "graveyard", 1)

        self.enter_paid_source(choices=choices, pending=pending)
        self.attack_annihilate(refs)

    def cast_foe(self, ref, *, player_target=False, target="source", reveal_owner=False):
        self.fund(self.foe, self.cost(ref))
        if player_target:
            self.emit("choose_player", player=self.foe, value=self.owner)
        else:
            self.emit("choose_target", ref=target, player=self.foe)
        self.emit("cast", ref=ref, value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.foe)
        if reveal_owner and self.owner == 0:
            self.emit("choose_dialog", player=0, value=0)
        self.resolve()
        self.move(ref, "graveyard")

    def shuffled_owner_grave(self):
        self.move("source", "graveyard")
        refs = [r for r, obj in self.objects.items() if obj["player"] == self.owner and obj["zone"] == "graveyard"]
        for ref in refs:
            self.move(ref, "library")
            self.library[self.owner].append(ref)
        self.emit("assert_shuffle_count", player=self.owner, value=1)
        self.emit("assert_shuffle_count", player=self.foe, value=0)
        self.emit("assert_zone", player=self.owner, zone=ZONES["library"], value=len(self.library[self.owner]))
        self.assert_top(self.foe)
        self.receipts()

    def kozilek(self):
        mode = self.mode
        self.deck(self.owner, (ISLAND, PLAINS, SWAMP, FOREST, MOUNTAIN))
        self.deck(self.foe, (ISLAND, FOREST))
        self.add("own_grave", BOLT, self.owner, "graveyard")
        self.add("own_creature_grave", GIANT, self.owner, "graveyard")
        self.add("rival_grave", BEAR, self.foe, "graveyard")
        if mode == "discard_shuffle":
            self.add("discard_spell", ROT, self.foe, "hand")
            self.emit("turn", player=self.foe)
            self.fund(self.owner, ZERO)
            self.emit("discard_policy", value=1)
            self.cast_foe("discard_spell", player_target=True, reveal_owner=True)
            self.emit("discard_policy")
            self.shuffled_owner_grave()
            return
        if mode == "murder_shuffle":
            self.add("kill_spell", MURDER, self.foe, "hand")
        refs = [] if mode == "murder_shuffle" else self.sacrifice_witnesses(2 if mode == "short_defender" else 4)

        def pending():
            for ref in self.library[self.owner][:4]:
                self.move(ref, "hand")
                self.assert_zone_ref(ref, "hand", 1)
            self.assert_top(self.owner)
            self.assert_top(self.foe)

        self.enter_paid_source(pending=pending)
        if mode == "murder_shuffle":
            self.cast_foe("kill_spell", reveal_owner=True)
            self.shuffled_owner_grave()
        else:
            self.attack_annihilate(refs)

    def spawn_count(self, amount):
        for player in (0, 1):
            self.emit("assert_zone_multiplicity", player=player, zone=ZONES["battlefield"],
                      index=self.rows[SPAWN]["csvid"], value=amount if player == self.owner else 0)

    def spawn_invasion(self):
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, self.cost("producer"))
        self.emit("cast", ref="producer", value=1)
        self.emit("assert_stack", value=1)
        self.resolve()
        self.move("producer", "graveyard")
        self.spawn_count(5)
        for index in range(5):
            self.emit("bind_permanent_copy", ref=f"spawn_{index}", player=self.owner,
                      index=index, value=self.rows[SPAWN]["csvid"])

    def hand(self):
        mode = self.mode
        refs = self.sacrifice_witnesses(1)
        if mode != "cast_full_attack":
            self.add("producer", INVASION, self.owner, "hand")
            self.spawn_invasion()
        if mode == "spawn_short":
            for index in (0, 1):
                self.emit("activate", ref=f"spawn_{index}", value=1)
            self.emit("assert_stack")
            self.spawn_count(3)
            mana = [2, 0, 0, 0, 0, 0]
            self.mana("assert_mana", self.owner, mana)
            self.emit("can_cast", ref="source", value=0)
            self.emit("cast", ref="source", value=0)
            self.receipts(self.pools(mana))
            return

        def choices():
            if mode == "normal_with_spawn":
                self.emit("choose_dialog", player=self.owner, value=1)
            elif mode == "spawn_cost_attack" and self.owner == 0:
                for index in range(4):
                    self.emit("choose_target", ref=f"spawn_{index}", player=self.owner)

        def pending():
            self.spawn_count({"cast_full_attack": 0, "normal_with_spawn": 5, "spawn_cost_attack": 1}[mode])

        self.enter_paid_source(choices=choices, pending=pending, cost=ZERO if mode == "spawn_cost_attack" else None)
        self.attack_annihilate(refs)

    def banshee(self):
        blocked = self.mode == "blocked"
        other = self.mode == "other_attacker"
        has_card = self.mode != "empty_hand"
        if has_card:
            self.add("defender_hand", BOLT, self.foe, "hand")
        self.add("attackers_hand", BOOK, self.owner, "hand")
        if blocked:
            self.add("blocker", BEAR, self.foe, "battlefield")
        if other:
            self.add("different_attacker", GIANT, self.owner, "battlefield")
        self.cast_source()
        self.emit("discard_policy", value=1)
        attacker = "different_attacker" if other else "source"
        self.start_combat(actor=self.owner, attacker=attacker)
        self.declaration_end()
        self.receipts()
        self.complete_combat(attacker=attacker, blocker="blocker" if blocked else None, legal=blocked)
        self.emit("discard_policy")
        if not blocked and not other and has_card:
            self.move("defender_hand", "graveyard")
        self.receipts()

    def follow_replayed_combat(self):
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")

    def betrayal(self):
        mode = self.mode
        if mode == "sacrifice_tokens":
            self.add("opposing_token_spell", INVASION, self.foe, "hand")
            self.emit("turn", player=self.foe)
            self.fund(self.owner, ZERO)
            self.fund(self.foe, self.cost("opposing_token_spell"))
            self.emit("cast", ref="opposing_token_spell", value=1)
            self.emit("assert_stack", value=1)
            self.resolve()
            self.move("opposing_token_spell", "graveyard")
            spawn = self.rows[SPAWN]["csvid"]
            self.emit("assert_zone_multiplicity", player=self.foe, zone=1, index=spawn, value=5)
            for index in range(2):
                self.emit("bind_permanent_copy", ref=f"defender_spawn_{index}", player=self.foe,
                          index=index, value=spawn)
            self.cast_source()
            self.start_combat(actor=self.owner, attacker="source")
            if self.foe == 0:
                for index in range(2):
                    self.emit("choose_target", ref=f"defender_spawn_{index}", player=self.foe)
            self.declaration_end()
            self.emit("assert_zone_multiplicity", player=self.foe, zone=1, index=spawn, value=3)
            self.emit("assert_zone_multiplicity", player=self.owner, zone=1, index=spawn, value=0)
            self.complete_combat(attacker="source")
            self.receipts()
            return

        if mode == "own_sacrifice":
            self.add("ally_pet", PET, self.owner, "battlefield")
            self.cast_source()
            self.emit("activate", ref="ally_pet", value=1)
            self.resolve()
            self.move("ally_pet", "graveyard")
            self.receipts(self.pools([0, 2, 0, 0, 0, 0]))
            return

        if mode == "destroy_not_sacrifice":
            self.add("enemy_creature", BEAR, self.foe, "battlefield")
            self.add("destroy_spell", MURDER, self.owner, "hand")
            self.cast_source()
            self.fund(self.owner, self.cost("destroy_spell"))
            self.emit("choose_target", ref="enemy_creature", player=self.owner)
            self.emit("cast", ref="destroy_spell", value=1)
            self.emit("assert_stack", value=1)
            self.resolve()
            self.move("enemy_creature", "graveyard")
            self.move("destroy_spell", "graveyard")
            self.receipts()
            self.emit("assert_zone_multiplicity", player=self.owner, zone=1, index=self.rows[BEAR]["csvid"])
            return

        refs = self.sacrifice_witnesses(2)
        self.cast_source()
        self.start_combat(actor=self.owner, attacker="source")
        if self.foe == 0:
            for ref in refs:
                self.emit("choose_target", ref=ref, player=self.foe)
        self.declaration_end()
        for ref in refs:
            name = self.objects[ref]["name"]
            self.emit("assert_zone_multiplicity", player=self.foe, zone=1, index=self.rows[name]["csvid"])
            self.emit("assert_zone_multiplicity", player=self.foe, zone=3, index=self.rows[name]["csvid"])
            self.objects[ref]["player"] = self.owner
        self.receipts()
        self.complete_combat(attacker="source")
        self.receipts()

    def pathrazer(self):
        if self.mode == "annihilator_short":
            refs = self.sacrifice_witnesses(2)
            self.cast_source()
            self.attack_annihilate(refs)
            return
        left = {"one_blocker": 1, "two_blockers": 2, "three_blockers": 3}[self.mode]
        for i in range(left + 3):
            self.add(f"bear_{i}", BEAR, self.foe, "battlefield")
        self.cast_source()
        self.start_combat(actor=self.owner, attacker="source")
        if self.foe == 0:
            for i in range(3):
                self.emit("choose_target", ref=f"bear_{i}", player=self.foe)
        self.declaration_end()
        for i in range(3):
            self.move(f"bear_{i}", "graveyard")
        self.receipts()
        self.emit("begin_blockers")
        for i in range(left):
            ref = f"remaining_{i}"
            self.emit("bind_permanent_copy", ref=ref, player=self.foe, index=i, value=self.rows[BEAR]["csvid"])
            self.emit("block", ref=ref, other="source", value=int(left == 3))
        self.emit("advance_combat")
        self.observe("source", "blocked", int(left == 3))
        if left == 3:
            for i in range(left):
                self.observe(f"remaining_{i}", "blocking_count", 1)
        else:
            self.follow_replayed_combat()
            self.life[self.foe] -= 9
        self.receipts()

    def ulamog(self):
        self.deck(self.owner, (FOREST, MOUNTAIN))
        self.deck(self.foe, (ISLAND, SWAMP))
        self.add("own_grave", BOLT, self.owner, "graveyard")
        self.add("rival_grave", GIANT, self.foe, "graveyard")
        if self.mode == "discard_shuffle":
            self.add("discard_spell", ROT, self.foe, "hand")
            self.emit("turn", player=self.foe)
            self.fund(self.owner, ZERO)
            self.emit("discard_policy", value=1)
            self.cast_foe("discard_spell", player_target=True, reveal_owner=True)
            self.emit("discard_policy")
            self.shuffled_owner_grave()
            return
        if self.mode == "countered_cast_trigger":
            self.ulamog_countered_cast()
            return
        refs = self.sacrifice_witnesses(4) if self.mode == "cast_destroy_attack" else []
        self.add("cast_target", PLAINS, self.foe, "battlefield")
        if self.mode == "indestructible":
            self.add("kill_spell", MURDER, self.foe, "hand")

        def choices():
            self.emit("choose_target", ref="cast_target", player=self.owner)

        self.enter_paid_source(choices=choices)
        # The cast callback queues destruction. The real drain processes its
        # pending regeneration and graveyard work before zone receipts.
        self.move("cast_target", "graveyard")
        self.assert_zone_ref("cast_target", "battlefield", 0)
        self.assert_zone_ref("cast_target", "graveyard", 1)
        if self.mode == "indestructible":
            self.cast_foe("kill_spell")
            self.inspect_source()
            self.emit("assert_shuffle_count", player=self.owner)
            self.assert_top(self.owner)
            self.receipts()
        else:
            self.attack_annihilate(refs)

    def ulamog_countered_cast(self):
        self.add("cast_target", PLAINS, self.foe, "battlefield")
        self.add("counterspell", COUNTERSPELL, self.foe, "hand")
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, self.cost("source"))
        self.emit("choose_target", ref="cast_target", player=self.owner)
        self.emit("cast", ref="source", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner)
        self.emit("announce_spell", ref="source")
        self.fund(self.foe, self.cost("counterspell"))
        self.emit("cast_response", ref="counterspell", value=1)
        self.emit("assert_stack", value=2)
        self.mana("assert_mana", self.foe)
        if self.owner == 0:
            self.emit("choose_dialog", player=0, value=0)
        self.emit("resolve_response")
        self.emit("refresh")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        self.move("counterspell", "graveyard")
        self.move("cast_target", "graveyard")
        self.assert_zone_ref("source", "battlefield", 0)
        self.assert_zone_ref("cast_target", "battlefield", 0)
        self.assert_zone_ref("cast_target", "graveyard", 1)
        self.shuffled_owner_grave()

    def viper(self):
        blocked = self.mode == "blocked"
        other = self.mode == "other_attacker"
        draws = self.mode == "draw_hit"
        self.deck(self.owner, (ISLAND, FOREST))
        if blocked:
            self.add("blocker", BEAR, self.foe, "battlefield")
        if other:
            self.add("different_attacker", GIANT, self.owner, "battlefield")
        self.cast_source()
        attacker = "different_attacker" if other else "source"
        self.start_combat(actor=self.owner, attacker=attacker)
        self.declaration_end()
        self.receipts()
        self.assert_top(self.owner)
        self.emit("begin_blockers")
        if blocked:
            self.emit("block", ref="blocker", other=attacker, value=1)
        self.emit("advance_combat")
        if draws and self.owner == 0:
            self.emit("choose_trigger_source", ref="source", player=0)
        self.emit("combat_damage")
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_unused_choices")
        self.emit("assert_stack")
        self.emit("end_combat")
        if draws:
            self.move(self.library[self.owner][0], "hand")
        if blocked:
            self.move("blocker", "graveyard")
            self.observe("source", "damage", 2)
        else:
            self.life[self.foe] -= int(self.oracle[self.objects[attacker]["name"]]["power"])
        self.assert_top(self.owner)
        self.receipts()

    def herald(self):
        defense = self.mode == "defensive_flight"
        flies = self.mode != "flying_vigilance"
        self.add("opposing_creature", AIR if flies else BEAR, self.foe, "battlefield")
        self.cast_source()
        attacker = "opposing_creature" if defense else "source"
        self.start_combat(actor=self.foe if defense else self.owner, attacker=attacker)
        self.declaration_end()
        self.observe("source", "tapped", 0)
        self.receipts()
        self.complete_combat(attacker=attacker, blocker="source" if defense else "opposing_creature", legal=flies)
        self.receipts()

    def warchief(self):
        other = self.mode == "other_attacker"
        blocked = self.mode == "fresh_blocked"
        if other:
            self.add("different_attacker", GIANT, self.owner, "battlefield")
        if blocked:
            self.add("blocker", BEAR, self.foe, "battlefield")
        self.cast_source()
        attacker = "different_attacker" if other else "source"
        self.emit("begin_attackers")
        self.emit("attack", ref=attacker, value=1)
        self.declaration_end()
        self.receipts()
        self.complete_combat(attacker=attacker, blocker="blocker" if blocked else None, legal=blocked)
        self.receipts()

    def run(self):
        actions = {
            ARTISAN: self.artisan, KOZILEK: self.kozilek, HAND: self.hand, BANSHEE: self.banshee,
            BETRAYS: self.betrayal, PATHRAZER: self.pathrazer, ULAMOG: self.ulamog, VIPER: self.viper,
            HERALD: self.herald, WARCHIEF: self.warchief,
        }
        actions[self.csvid]()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()["sources"][name]["registry"]["csvid"] != csvid:
        raise ValueError("exact registered Annihilator and Myriad primary CSVID required")
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
