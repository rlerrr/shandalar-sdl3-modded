"""Attack-trigger costs and damage scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "attack_trigger_costs_and_damage"
PROFILE = Path(__file__).with_name("data") / "attack_trigger_costs_and_damage_wave8_profiles.json"
GARGADON, OGRESS, ARCHER, VENTMAW = 1068, 424, 15235, 15005
ROTTING, NECROPOD, THING, DASH = 5679, 15895, 2859, 14586
HARBOR, SIBILANT, FLAMEBLAST, WOLF = 2503, 2093, 9971, 16026
MTENDA, MARAUDER, TAKENUMA = 2551, 7526, 7571
MODES = {
    GARGADON: ("sacrifice_attack", "sacrifice_block", "no_land", "other_attacker"),
    OGRESS: ("pay_exact", "unfunded", "blocked_payment", "blocking_only", "decline"),
    ARCHER: ("no_elf", "creature_elf", "kindred_elf", "loss_not_damage"),
    VENTMAW: ("flying_producer", "main_two_spend", "other_attacker", "air_blocked", "real_phase_drain"),
    ROTTING: ("exile_attack", "empty_attack", "exile_block", "empty_block", "decline_exile"),
    NECROPOD: ("sacrifice_land", "sacrifice_creature", "block_sacrifice", "menace_single", "menace_pair"),
    THING: ("island", "wrong_land", "blocking_only"),
    DASH: ("normal_attack", "dash_attack", "normal_fresh"),
    HARBOR: ("draw_accept", "draw_decline", "reach_block"),
    SIBILANT: ("draw_accept", "draw_decline", "flying"),
    FLAMEBLAST: ("x_creature", "x_player", "x_planeswalker", "no_red"),
    WOLF: ("pay_discard", "discard_zero", "no_card", "no_mana"),
    MTENDA: ("blue_payment", "decline_blue", "wrong_color", "blocked_payment"),
    MARAUDER: ("no_creature", "decline_sacrifice", "paid_sacrifice", "paid_then_block"),
    TAKENUMA: ("no_demon", "own_demon", "enemy_demon", "block_no_demon"),
}
PREFIX = "AttackTriggerCostsAndDamageRecipe"
SEED = 3012
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
FLOAT = [0, 0, 0, 3, 3, 0]
BEAR, GIANT, AIR = "Grizzly Bears", "Hill Giant", "Air Elemental"
ELF, KINDRED, FIELD = "Elvish Warrior", "Prowess of the Fair", "Energy Field"
FOREST, MOUNTAIN, BALL = "Forest", "Mountain", "Ball Lightning"
WITNESSES = {BEAR, GIANT, AIR, ELF, KINDRED, FIELD, FOREST, MOUNTAIN, BALL}

ISLAND, PLAINS, BOLT = "Island", "Plains", "Lightning Bolt"
JACE, DEMON, BOOK = "Jace Beleren", "Demon of Death's Gate", "Spellbook"
WITNESSES |= {ISLAND, PLAINS, BOLT, JACE, DEMON, BOOK}
ZONE_NAMES = {code: name for name, code in ZONES.items()}
FOLLOWERS = (ROTTING, NECROPOD, THING, DASH, HARBOR, SIBILANT, FLAMEBLAST, WOLF, MTENDA, MARAUDER, TAKENUMA)
SINGLE_SEAT = {(OGRESS, "decline"): 0, (ROTTING, "decline_exile"): 0, (MARAUDER, "paid_then_block"): 1}
GAPS = {
    VENTMAW: ["The bounded combat operations retain normal pools too. Independent mana-drain exemption and end-of-turn expiry remain unobserved."],
    OGRESS: ["Human mana cancellation is observed. The AI's discretionary cancel is not scripted."],
    ROTTING: ["Human can decline graveyard exile; AI discretionary decline is unobserved."],
    NECROPOD: ["The two-blocker Menace test checks both actual assignments and stops before damage ordering."],
    WOLF: ["Creature damage is observed. The permitted planeswalker target has no distinct variant in this pack."],
    MARAUDER: ["A human chooses the exact sacrifice with a second blocker present. AI observes the sole legal sacrifice."],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get("layout") != "normal" or record.get("selected_face")
            or record.get("card_faces") or "attacks" not in record.get("oracle_text", "")):
        raise ValueError("whole pinned attack trigger record required")
    return dict(csvid=csvid, printed_mana=record["mana_cost"], rules=record["oracle_text"])


def variants(shape):
    return [f"{mode}_{seat}" for mode in MODES[shape["csvid"]]
            for seat in ((SINGLE_SEAT[shape["csvid"], mode],) if (shape["csvid"], mode) in SINGLE_SEAT else (0, 1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError("distinct supported attack trigger scenarios required")


def spec(declaration, record):
    source = profiles()["sources"].get(record.get("name"))
    if not source or not exact(source["oracle"], record):
        raise ValueError("exact pinned attack trigger primary required")
    shape = classify(record, source["registry"]["csvid"])
    if (declaration.get("family") != FAMILY or type(declaration.get("version")) is not int
            or declaration["version"] != 2 or declaration.get("oracle_text") != record["oracle_text"]
            or not exact(declaration.get("params"), shape)):
        raise ValueError("versioned whole attack trigger declaration required")
    validate_variants(shape, declaration.get("variants"))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {"sources", "witnesses"} or set(pins["witnesses"]) != WITNESSES:
        raise ValueError("independent exact attack trigger witnesses required")
    if {pin["registry"]["csvid"] for pin in pins["sources"].values()} != set(MODES):
        raise ValueError("exact attack trigger primaries required")
    for group in ("sources", "witnesses"):
        for name, pin in pins[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin["oracle"]) or not exact(row, pin["registry"])
                    or row.get("kind") != "card" or oracle[name].get("layout") != "normal"):
                raise ValueError("changed attack source or witness: " + name)
            if group == "sources":
                if not str(row.get("handler", "")).startswith("manalinkeh_card_"):
                    raise ValueError("implemented attack primary required")
                classify(oracle[name], row["csvid"])
    if (oracle[ELF]["type_line"] != "Creature — Elf Warrior"
            or oracle[KINDRED]["type_line"] != "Kindred Enchantment — Elf"
            or oracle[FIELD]["oracle_text"].splitlines()[0] != "Prevent all damage that would be dealt to you by sources you don't control."
            or oracle[AIR]["oracle_text"] != "Flying"
            or oracle[BALL]["mana_cost"] != "{R}{R}{R}"
            or oracle[DEMON]["type_line"] != "Creature — Demon"
            or oracle[JACE]["type_line"] != "Legendary Planeswalker — Jace"):
        raise ValueError("attack-trigger witness clauses changed")


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
        if op == "assert_counter":
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        fields = dict(ref=ref, other=other, property=prop, player=player, value=value, index=index, color=color)
        values = {key: item for key, item in fields.items() if item or key == "value"}
        if op in {"set_mana", "assert_mana"}:
            values.update(player=player, color=color)
        elif op.startswith("assert_zone"):
            values.update(player=player, zone=ZONE_NAMES[zone], index=index)
        elif op in {"choose_trigger_source", "choose_player", "choose_x"}:
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

    def gargadon(self):
        has_land = self.mode != "no_land"
        blocks = self.mode == "sacrifice_block"
        other = self.mode == "other_attacker"
        if has_land:
            self.add("payment_land", FOREST, self.owner, "battlefield")
        self.add("enemy_land", MOUNTAIN, self.foe, "battlefield")
        if blocks or other:
            self.add("witness", BEAR, self.foe if blocks else self.owner, "battlefield")
        self.cast_source()
        attacker = "witness" if blocks or other else "source"
        self.start_combat(actor=self.foe if blocks else self.owner, attacker=attacker)

        def plan_land_payment():
            # A single legal AI land is selected autonomously by new_sacrifice_impl.
            if self.owner == 0:
                self.emit("choose_target", ref="payment_land", player=0)

        if self.mode == "sacrifice_attack":
            plan_land_payment()
        self.declaration_end()
        if self.mode == "sacrifice_attack":
            self.move("payment_land", "graveyard")
        self.receipts()
        if blocks:
            self.complete_combat(attacker=attacker, blocker="source", legal=True, pre_block=plan_land_payment)
            self.move("payment_land", "graveyard")
        else:
            self.complete_combat(attacker=attacker)
        self.receipts()

    def hasran(self):
        blocked = self.mode == "blocked_payment"
        defends = self.mode == "blocking_only"
        pays = self.mode in ("pay_exact", "blocked_payment")
        declines = self.mode == "decline"
        if blocked or defends:
            self.add("combat_witness", GIANT if defends else BEAR, self.foe, "battlefield")
        if declines:
            self.add("payment_forest", FOREST, self.owner, "battlefield")
            self.add("payment_mountain", MOUNTAIN, self.owner, "battlefield")
        self.cast_source()
        attacker = "combat_witness" if defends else "source"
        self.start_combat(actor=self.foe if defends else self.owner, attacker=attacker)
        pool = [2 if pays else int(self.mode == "unfunded"), 0, 0, 0, 0, 0]
        self.fund(self.owner, pool)
        if declines:
            self.emit("choose_cancel_mana", player=self.owner)
        self.declaration_end()
        if not pays and not defends:
            self.life[self.owner] -= 3
        left = ZERO if pays else pool
        self.receipts(self.pools(left))
        self.complete_combat(attacker=attacker, blocker="source" if defends else "combat_witness" if blocked else None,
                             legal=blocked or defends)
        if declines:
            self.observe("payment_forest", "tapped", 0)
            self.observe("payment_mountain", "tapped", 0)
        self.receipts(self.pools(left))

    def archer(self):
        guarded = self.mode in ("creature_elf", "kindred_elf")
        prevention = self.mode == "loss_not_damage"
        if guarded or prevention:
            witness = ELF if self.mode == "creature_elf" else KINDRED if guarded else FIELD
            self.add("enemy_witness", witness, self.foe, "battlefield")
        self.cast_source()
        self.start_combat(actor=self.owner, attacker="source")
        self.emit("assert_life", player=self.foe, value=20)
        self.declaration_end()
        if not guarded:
            self.life[self.foe] -= 1
        self.receipts()
        self.complete_combat(attacker="source", prevent_damage=prevention)
        self.receipts()

    def ventmaw_real_phase_drain(self):
        self.add("ordinary_mana_source", FOREST, self.owner, "battlefield")
        self.add("follow_spell", BALL, self.owner, "hand")
        self.cast_source()
        self.start_combat(actor=self.owner, attacker="source")
        self.mana("assert_mana", self.owner)
        self.declaration_end()
        self.mana("assert_mana", self.owner, FLOAT)
        self.emit("begin_blockers")
        self.emit("advance_combat")
        self.emit("combat_damage")
        self.resolve()
        self.life[self.foe] -= 4
        self.emit("assert_life", player=self.foe, value=self.life[self.foe])

        # The real mana ability adds one ordinary green to Ventmaw's three.
        self.observe("ordinary_mana_source", "tapped", 0)
        self.emit("activate", ref="ordinary_mana_source", value=1)
        self.observe("ordinary_mana_source", "tapped", 1)
        self.mana("assert_mana", self.owner, [0, 0, 0, 4, 3, 0])
        self.emit("assert_stack")
        self.emit("end_combat_step", value=0)
        self.receipts(self.pools(FLOAT))

        # Red remains spendable on a real postcombat main phase spell.
        self.emit("cast", ref="follow_spell", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner, [0, 0, 0, 3, 0, 0])
        self.resolve()
        self.move("follow_spell", "battlefield")
        self.receipts(self.pools([0, 0, 0, 3, 0, 0]))

    def ventmaw(self):
        if self.mode == "real_phase_drain":
            self.ventmaw_real_phase_drain()
            return
        other = self.mode == "other_attacker"
        blocked = self.mode == "air_blocked"
        spend = self.mode == "main_two_spend"
        if other:
            self.add("witness", BEAR, self.owner, "battlefield")
        elif blocked or self.mode == "flying_producer":
            self.add("blocker", AIR if blocked else BEAR, self.foe, "battlefield")
        if spend:
            self.add("follow_spell", BALL, self.owner, "hand")
        self.cast_source()
        attacker = "witness" if other else "source"
        self.start_combat(actor=self.owner, attacker=attacker)
        self.mana("assert_mana", self.owner)
        self.declaration_end()
        new_pool = ZERO if other else FLOAT
        self.receipts(self.pools(new_pool))
        self.complete_combat(attacker=attacker, blocker="blocker" if "blocker" in self.objects else None, legal=blocked)
        self.receipts(self.pools(new_pool))
        if spend:
            assert self.cost("follow_spell") == [0, 0, 0, 0, 3, 0]
            self.emit("cast", ref="follow_spell", value=1)
            self.emit("assert_stack", value=1)
            self.mana("assert_mana", self.owner, [0, 0, 0, 3, 0, 0])
            self.resolve()
            self.move("follow_spell", "battlefield")
            self.receipts(self.pools([0, 0, 0, 3, 0, 0]))

    def close_combat(self):
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")

    def land_or_creature_choice(self, ref, player):
        if player == 0:
            self.emit("choose_target", ref=ref, player=0)

    def rotting(self):
        blocks = self.mode.endswith("_block")
        offers = self.mode.startswith("exile") or self.mode == "decline_exile"
        survives = self.mode.startswith("exile")
        if offers:
            self.add("grave_fee", BOLT, self.owner, "graveyard")
        self.add("enemy_grave", FOREST, self.foe, "graveyard")
        if blocks:
            self.add("attacker", BEAR, self.foe, "battlefield")
        self.cast_source()
        attacker = "attacker" if blocks else "source"
        self.start_combat(actor=self.foe if blocks else self.owner, attacker=attacker)

        def choose_fee():
            if offers:
                if self.mode == "decline_exile":
                    self.emit("choose_zone", player=self.owner, value=-1)
                else:
                    self.emit("choose_zone_card", player=self.owner, value=self.rows[BOLT]["csvid"])

        if not blocks:
            choose_fee()
        self.declaration_end()
        if blocks:
            if survives:
                self.complete_combat(attacker=attacker, blocker="source", legal=True, pre_block=choose_fee)
            else:
                self.emit("begin_blockers")
                self.emit("block", ref="source", other="attacker", value=1)
                self.emit("advance_combat")
                self.emit("assert_unused_choices")
                self.observe("attacker", "blocked", 1)
                self.close_combat()
        elif survives:
            self.complete_combat(attacker=attacker)
        else:
            self.emit("begin_blockers")
            self.emit("advance_combat")
            self.close_combat()
        if survives:
            self.move("grave_fee", "exile")
        else:
            self.move("source", "graveyard")
        self.receipts()

    def necropod(self):
        blocks = self.mode == "block_sacrifice"
        pairs = self.mode == "menace_pair"
        menace = pairs or self.mode == "menace_single"
        self.add("fee", BEAR if self.mode == "sacrifice_creature" else FOREST, self.owner, "battlefield")
        if blocks or menace:
            self.add("enemy_creature", BEAR, self.foe, "battlefield")
        if pairs:
            self.add("another_blocker", GIANT, self.foe, "battlefield")
        self.cast_source()
        attacker = "enemy_creature" if blocks else "source"
        self.start_combat(actor=self.foe if blocks else self.owner, attacker=attacker)

        def choose_fee():
            self.land_or_creature_choice("fee", self.owner)

        if not blocks:
            choose_fee()
        self.declaration_end()
        if not blocks:
            self.move("fee", "graveyard")
        if blocks:
            self.complete_combat(attacker=attacker, blocker="source", legal=True, pre_block=choose_fee)
            self.move("fee", "graveyard")
        elif pairs:
            self.emit("begin_blockers")
            for ref in ("enemy_creature", "another_blocker"):
                self.emit("block", ref=ref, other="source", value=1)
            self.emit("advance_combat")
            self.observe("source", "blocked", 1)
            for ref in ("enemy_creature", "another_blocker"):
                self.observe(ref, "blocking_count", 1)
            self.emit("assert_unused_choices")
        else:
            self.complete_combat(attacker=attacker, blocker="enemy_creature" if menace else None)
        self.receipts()

    def thing(self):
        blocks = self.mode == "blocking_only"
        eligible = self.mode == "island"
        self.add("fee", ISLAND if eligible else FOREST, self.owner, "battlefield")
        self.add("rival_island", ISLAND, self.foe, "battlefield")
        if blocks:
            self.add("attacker", BEAR, self.foe, "battlefield")
        self.cast_source()
        attacker = "attacker" if blocks else "source"
        self.start_combat(actor=self.foe if blocks else self.owner, attacker=attacker)
        if eligible:
            self.land_or_creature_choice("fee", self.owner)
        self.declaration_end()
        if eligible:
            self.move("fee", "graveyard")
            self.complete_combat(attacker=attacker)
        elif blocks:
            self.complete_combat(attacker=attacker, blocker="source", legal=True)
        else:
            self.move("source", "graveyard")
            self.emit("begin_blockers")
            self.emit("advance_combat")
            self.close_combat()
        self.receipts()

    def shadowspear(self):
        dashed = self.mode == "dash_attack"
        if dashed:
            self.emit("turn", player=self.owner)
            self.emit("present_ai_dialogs", value=1)
            self.fund(self.foe, ZERO)
            self.fund(self.owner, [1, 1, 0, 0, 0, 0])
            self.emit("choose_dialog", player=self.owner, value=0)
            self.emit("cast", ref="source", value=1)
            self.emit("assert_stack", value=1)
            self.mana("assert_mana", self.owner)
            self.resolve()
            self.move("source", "battlefield")
            self.inspect_source()
            self.emit("begin_attackers")
            self.emit("attack", ref="source", value=1)
            self.observe("source", "attacking", 1)
        else:
            self.cast_source()
            if self.mode == "normal_fresh":
                self.emit("begin_attackers")
                self.emit("attack", ref="source", value=0)
                self.observe("source", "attacking", 0)
                self.declaration_end()
                self.receipts()
                return
            self.start_combat(actor=self.owner, attacker="source")
        self.declaration_end()
        self.life[self.foe] -= 1
        self.receipts()
        self.complete_combat(attacker="source")
        self.receipts()
        self.emit("advance_turn")
        if dashed:
            self.move("source", "hand")
        self.receipts()

    def defender_draw(self):
        reaches = self.mode == "reach_block"
        flies = self.mode == "flying"
        if reaches or flies:
            self.add("keyword_witness", AIR if reaches else BEAR, self.foe, "battlefield")
        if not reaches:
            self.add("top", ISLAND, self.foe, "library")
            self.add("next", FOREST, self.foe, "library")
        self.cast_source()
        attacker = "keyword_witness" if reaches else "source"
        self.start_combat(actor=self.foe if reaches else self.owner, attacker=attacker)
        if not reaches:
            self.emit("choose_dialog", player=self.foe, value=int(self.mode == "draw_decline"))
        self.declaration_end()
        if not reaches and self.mode != "draw_decline":
            self.move("top", "hand")
        self.receipts()
        if not reaches:
            self.emit("assert_zone_card", player=self.foe, zone=ZONES["library"], index=0,
                      value=self.rows[ISLAND if self.mode == "draw_decline" else FOREST]["csvid"])
        self.complete_combat(attacker=attacker, blocker="source" if reaches else "keyword_witness" if flies else None,
                             legal=reaches)
        self.receipts()

    def flameblast(self):
        target = "damage_target"
        kind = self.mode.removeprefix("x_")
        if kind in ("creature", "planeswalker"):
            self.add(target, BEAR if kind == "creature" else JACE, self.foe, "battlefield")
        self.cast_source()
        self.start_combat(actor=self.owner, attacker="source")
        paid = kind != "no_red"
        mana = [2, 0, 0, 0, int(paid), 0]
        self.fund(self.owner, mana)
        if paid:
            if self.owner == 0:
                self.emit("choose_trigger_source", ref="source", player=self.owner)
            self.emit("choose_x", player=self.owner, value=2)
            if kind == "player":
                self.emit("choose_player", player=self.owner, value=self.foe)
            else:
                self.emit("choose_target", ref=target, player=self.owner)
        self.declaration_end()
        if kind == "creature":
            self.move(target, "graveyard")
        if kind == "planeswalker":
            self.emit("assert_counter", ref=target, prop="loyalty", value=1)
        if kind == "player":
            self.life[self.foe] -= 2
        left = ZERO if paid else mana
        self.receipts(self.pools(left))
        self.complete_combat(attacker="source")
        self.receipts(self.pools(left))

    def wolf(self):
        self.add("damage_target", GIANT, self.foe, "battlefield")
        legal = self.mode in ("pay_discard", "discard_zero")
        discard = FOREST if self.mode == "discard_zero" else AIR
        if self.mode != "no_card":
            self.add("discard", discard, self.owner, "hand")
        self.cast_source()
        self.start_combat(actor=self.owner, attacker="source")
        mana = ZERO if self.mode == "no_mana" else [1, 0, 0, 0, 1, 0]
        self.fund(self.owner, mana)
        if legal:
            self.emit("choose_zone_card", player=self.owner, value=self.rows[discard]["csvid"])
            self.emit("choose_target", ref="damage_target", player=self.owner)
        self.declaration_end()
        if legal:
            self.move("discard", "graveyard")
        if self.mode == "pay_discard":
            self.move("damage_target", "graveyard")
        elif self.mode == "discard_zero":
            self.observe("damage_target", "damage", 0)
        left = ZERO if legal else mana
        self.receipts(self.pools(left))
        self.complete_combat(attacker="source")
        self.receipts(self.pools(left))

    def mtenda(self):
        blocked = self.mode == "blocked_payment"
        pays = self.mode in ("blue_payment", "blocked_payment")
        offered = self.mode != "wrong_color"
        if blocked:
            self.add("blocker", BEAR, self.foe, "battlefield")
        self.cast_source()
        self.start_combat(actor=self.owner, attacker="source")
        mana = [0, int(not offered), int(offered), 0, 0, 0]
        self.fund(self.foe, mana)
        if offered:
            self.emit("choose_dialog", player=self.foe, value=int(not pays))
        self.declaration_end()
        left = ZERO if pays else mana
        self.mana("assert_mana", self.foe, left)
        if blocked:
            self.emit("begin_blockers")
            self.emit("block", ref="blocker", other="source", value=1)
            self.emit("advance_combat")
            self.observe("source", "blocked", 1)
            self.close_combat()
            self.move("source", "graveyard")
            self.observe("blocker", "damage", 0)
        else:
            self.complete_combat(attacker="source", prevent_damage=pays)
        self.receipts((left, ZERO) if self.foe == 0 else (ZERO, left))

    def marauder(self):
        offered = self.mode != "no_creature"
        sacrifice = self.mode in ("paid_sacrifice", "paid_then_block")
        blocks = self.mode == "paid_then_block"
        if offered:
            self.add("fodder", BEAR, self.foe, "battlefield")
        if blocks:
            self.add("blocker", GIANT, self.foe, "battlefield")
        self.cast_source()
        self.start_combat(actor=self.owner, attacker="source")
        # The legacy availability check consults the attacking side, so it
        # also offers the harmless Decline branch with no defending creature.
        self.emit("choose_dialog", player=self.foe, value=int(sacrifice))
        if sacrifice:
            self.land_or_creature_choice("fodder", self.foe)
        self.declaration_end()
        if sacrifice:
            self.move("fodder", "graveyard")
        self.receipts()
        blocker = "blocker" if blocks else "fodder" if not sacrifice and offered else None
        self.complete_combat(attacker="source", blocker=blocker, legal=blocks)
        self.receipts()

    def bleeder(self):
        blocks = self.mode == "block_no_demon"
        if self.mode in ("own_demon", "enemy_demon"):
            self.add("demon", DEMON, self.owner if self.mode == "own_demon" else self.foe, "battlefield")
        if blocks:
            self.add("attacker", BEAR, self.foe, "battlefield")
        self.cast_source()
        attacker = "attacker" if blocks else "source"
        self.start_combat(actor=self.foe if blocks else self.owner, attacker=attacker)
        if not blocks and self.owner == 0:
            self.emit("choose_trigger_source", ref="source", player=0)
        self.declaration_end()
        if not blocks and self.mode != "own_demon":
            self.life[self.owner] -= 1
        self.receipts()
        self.complete_combat(attacker=attacker, blocker="source" if blocks else None, legal=blocks)
        if blocks:
            self.life[self.owner] -= 1
        self.receipts()

    def run(self):
        cases = {GARGADON: self.gargadon, OGRESS: self.hasran, ARCHER: self.archer, VENTMAW: self.ventmaw,
                 ROTTING: self.rotting, NECROPOD: self.necropod, THING: self.thing, DASH: self.shadowspear,
                 HARBOR: self.defender_draw, SIBILANT: self.defender_draw, FLAMEBLAST: self.flameblast,
                 WOLF: self.wolf, MTENDA: self.mtenda, MARAUDER: self.marauder, TAKENUMA: self.bleeder}
        cases[self.csvid]()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()["sources"][name]["registry"]["csvid"] != csvid:
        raise ValueError("exact registered attack trigger primary CSVID required")
    output = []
    for variant in selected:
        mode, seat = variant.rsplit("_", 1)
        board = Board(csvid, name, mode, int(seat), rows, oracle)
        board.run()
        output.append(dict(
            name=f"{PREFIX}_{csvid}_{variant}", card=name, fixture="empty_main", seed=SEED,
            setup=board.setup, steps=board.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration["params"],
                         oracle_text=declaration["oracle_text"], untested_components=(
                             ["Independent end-of-turn expiry remains unobserved."] if mode == "real_phase_drain"
                             else GAPS.get(csvid, []))),
        ))
    return output
