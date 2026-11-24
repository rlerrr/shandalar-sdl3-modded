"""Entry triggers and card selection scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "departure_resources"
PROFILE = Path(__file__).with_name("data") / "departure_resources_wave8_profiles.json"
ASHEN, DRIPPING, CATHODION, STAR = 7158, 7199, 1208, 8501
MODES = {
    ASHEN: ("single", "simultaneous_global", "same_turn", "later_turn", "exile"),
    DRIPPING: ("single_grounded", "simultaneous_global", "same_turn", "later_turn", "exile"),
    CATHODION: ("colorless_spend", "borrowed_controller", "exile"),
    STAR: ("paid_red", "paid_black", "unfunded", "destroy_draw", "exile"),
}
SINGLE = set()
PREFIX = "DepartureResourcesRecipe"
SEED = 3596
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP = "Forest", "Island", "Mountain", "Plains", "Swamp"
BEAR, BOOK, GIANT = "Grizzly Bears", "Spellbook", "Hill Giant"
KILL, EXILE, SWEEP = "Murder", "Revoke Existence", "Pyroclasm"
SWORDS, NATURALIZE, SPATIAL = "Swords to Plowshares", "Naturalize", "Spatial Contortion"
CHANT, SPIRIT = "Silent-Chant Zubera", "Spirit"
WITNESSES = {FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP, BEAR, BOOK, GIANT,
             KILL, EXILE, SWEEP, SWORDS, NATURALIZE, SPATIAL, CHANT, SPIRIT}



EMBER, FLOATING, SILENT, BLISTER, ANCESTOR, CENTAUR, PROWLER = 7204, 7209, 7379, 11215, 2399, 7789, 11432
MYR, SLITHERMUSE, BEETLES, TERRARION, AUGUR, EXAMINATION, IMPROVEMENT = 6923, 9433, 4156, 7984, 15506, 16728, 16730
FOLLOWERS = (EMBER, FLOATING, SILENT, BLISTER, ANCESTOR, CENTAUR, PROWLER, MYR, SLITHERMUSE, BEETLES, TERRARION,
             AUGUR, EXAMINATION, IMPROVEMENT)
MODES.update({
    EMBER: ("simultaneous_global", "same_turn", "creature_damage", "exile"),
    FLOATING: ("simultaneous_global", "same_turn", "later_turn", "exile"),
    SILENT: ("simultaneous_global", "later_turn", "exile"),
    BLISTER: ("swampwalk_dies", "ground_block_dies", "exile"),
    ANCESTOR: ("own_white_pay", "foe_white_pay", "decline", "unfunded", "nonwhite"),
    CENTAUR: ("green_gain", "white_gain", "green_decline"),
    PROWLER: ("infect_poison", "infect_block", "decline_proliferate"),
    MYR: ("dies", "borrowed", "exile"),
    SLITHERMUSE: ("evoke", "bounce", "equal_hand", "exile"),
    BEETLES: ("sacrifice_both", "destroy_pump", "short"),
    TERRARION: ("mixed_mana", "two_same", "tapped_short", "destroy_draw", "exile"),
    AUGUR: ("own_graveyard", "foe_graveyard", "empty_foe"),
    EXAMINATION: ("paid", "wrong_color", "destroy_draw", "exile"),
    IMPROVEMENT: ("paid", "wrong_color", "destroy_draw", "exile"),
})
SINGLE |= {(ANCESTOR, "decline"), (CENTAUR, "green_decline"), (PROWLER, "decline_proliferate")}
HAWK, BOUNCE = "Suntail Hawk", "Unsummon"
WITNESSES |= {HAWK, BOUNCE}
ZONE_NAMES = {code: name for name, code in ZONES.items()}
GAPS = {
    ANCESTOR: ["The human optional decline is exercised. AI strategic decline is unobserved."],
    CENTAUR: ["The human optional decline is exercised. AI strategic decline is unobserved."],
    PROWLER: ["Human manual decline and automatic poison/permanent increments are exercised. AI discretionary refusal is unobserved."],
}



@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get("layout") != "normal" or record.get("selected_face")
            
            or record.get("card_faces") or not any(
                marker in record.get("oracle_text", "") for marker in ("dies", "put into a graveyard", "leaves the battlefield"))):
        raise ValueError("whole pinned battlefield-departure resource record required")
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
        raise ValueError("exact pinned battlefield-departure primary required")
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
                    or row.get("kind") != "card" or oracle[name].get("layout") != ("token" if name == SPIRIT else "normal")):
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





    def paid_spell(self, ref, actor, *, target=None, discard=False, choices=None):
        self.pending_cast(ref, player=actor, target=target)
        if choices is not None:
            choices()
        if discard:
            self.emit("discard_policy", value=1)
        self.resolve()
        if discard:
            self.emit("discard_policy")
        self.move(ref, "graveyard")

    def spirit_receipts(self, count):
        token_id = self.rows[SPIRIT]["csvid"]
        for player in (0, 1):
            self.emit("assert_zone_multiplicity", player=player, zone=ZONES["battlefield"], index=token_id,
                      value=count if player == self.owner else 0)
        if count:
            for prop, value in (("power", 1), ("toughness", 1), ("color", 0),
                                ("subtype_Spirit", 1), ("token", 1), ("artifact", 0)):
                self.emit("assert_matching_permanents", player=self.owner, index=token_id, prop=prop, value=value)

    def zubera(self):
        simultaneous = self.mode == "simultaneous_global"
        earlier = self.mode in ("same_turn", "later_turn")
        exile = self.mode == "exile"
        ashen = self.csvid == ASHEN
        ember = self.csvid == EMBER
        grounds_target = self.mode == "creature_damage"
        damage_to_creature = ember and grounds_target
        grounded = self.mode == "single_grounded"
        self.deck(self.owner, (FOREST, PLAINS, ISLAND))
        self.deck(self.foe, (MOUNTAIN, SWAMP, PLAINS))
        if ashen:
            # The four identical cards expose exact discard quantity without
            # making the selected card depend on discretionary AI valuation.
            for n in range(4):
                self.add(f"foe_card_{n}", BOOK, self.foe, "hand")
        if simultaneous or earlier:
            self.add("earlier_zubera", CHANT, self.foe, "battlefield")
        if simultaneous:
            self.add("friendly_zubera", CHANT, self.owner, "battlefield")
            self.add("unrelated_creature", BEAR, self.foe, "battlefield")
        if grounded:
            self.add("blocker", BEAR, self.foe, "battlefield")
        if damage_to_creature:
            self.add("recipient", GIANT, self.foe, "battlefield")
        if earlier:
            self.add("prior_kill", KILL, self.owner, "hand")
        self.add("action", SWEEP if simultaneous else SWORDS if exile else KILL, self.owner, "hand")
        self.begin_source()
        self.complete_source()
        self.state()
        if earlier:
            self.paid_spell("prior_kill", self.owner, target="earlier_zubera")
            self.move("earlier_zubera", "graveyard")
            self.life[self.foe] += 2
            self.state()
            if self.mode == "later_turn":
                self.emit("advance_turn")
                self.emit("turn", player=self.foe)
        choices = self.ember_target if ember and not exile else None
        self.paid_spell("action", self.owner, target=None if simultaneous else "source",
                        discard=ashen and not exile, choices=choices)
        self.move("source", "exile" if exile else "graveyard")
        if simultaneous:
            for ref in ("earlier_zubera", "friendly_zubera", "unrelated_creature"):
                self.move(ref, "graveyard")
            self.life[self.owner] += 6
            self.life[self.foe] += 6
        if exile:
            self.life[self.owner] += 1
        amount = {"simultaneous_global": 3, "same_turn": 2, "exile": 0}.get(self.mode, 1)
        if ashen:
            for n in range(amount):
                self.move(f"foe_card_{n}", "graveyard")
        elif self.csvid == DRIPPING:
            self.spirit_receipts(amount)
        elif self.csvid == FLOATING:
            self.draw(self.owner, amount)
        elif self.csvid == SILENT:
            self.life[self.owner] += 2 * amount
        elif ember and not exile:
            if damage_to_creature:
                self.observe("recipient", "damage", amount)
            else:
                self.life[self.foe] -= amount
        self.state()
        if grounded:
            self.emit("bind_permanent_copy", ref="made_spirit", player=self.owner, value=self.rows[SPIRIT]["csvid"])
            self.start_combat(actor=self.owner, attacker="made_spirit")
            self.declaration_end()
            self.emit("begin_blockers")
            self.emit("block", ref="blocker", other="made_spirit", value=1)
            self.emit("advance_combat")
            self.emit("combat_damage")
            self.resolve()
            self.emit("end_combat")
            self.observe("blocker", "damage", 1)
            self.spirit_receipts(0)
            self.state()

    def cathodion(self):
        borrowed = self.mode == "borrowed_controller"
        spent = self.mode == "colorless_spend"
        exile = self.mode == "exile"
        self.deck(self.owner, (FOREST, ISLAND))
        self.deck(self.foe, (SWAMP, PLAINS))
        actor = self.foe if borrowed else self.owner
        self.add("action", EXILE if exile else KILL, actor, "hand")
        if spent:
            self.add("receiver", GIANT, self.foe, "battlefield")
            self.add("colorless_spell", SPATIAL, self.owner, "hand")
        if borrowed:
            self.setup = [row for row in self.setup if row["ref"] != "source"]
            self.add("source", self.name, self.owner, "battlefield", opponent_owned=True)
            self.emit("turn", player=self.foe)
            self.fund(self.owner, ZERO)
            self.fund(self.foe, ZERO)
        else:
            self.begin_source()
            self.complete_source()
        self.state()
        self.paid_spell("action", actor, target="source")
        if borrowed:
            self.objects["source"]["player"] = self.foe
        self.move("source", "exile" if exile else "graveyard")
        self.state(ZERO if exile else [3, 0, 0, 0, 0, 0])
        if spent:
            # Consume the actual death mana without replacing the pool.
            self.emit("choose_target", player=self.owner, ref="receiver")
            self.emit("cast", ref="colorless_spell", value=1)
            self.emit("assert_stack", value=1)
            self.mana("assert_mana", self.owner, [1, 0, 0, 0, 0, 0])
            self.resolve()
            self.move("receiver", "graveyard")
            self.move("colorless_spell", "graveyard")
            self.state([1, 0, 0, 0, 0, 0])

    def star(self):
        active = self.mode.startswith("paid_")
        short = self.mode == "unfunded"
        destroy = self.mode == "destroy_draw"
        self.deck(self.owner, (ISLAND, FOREST, PLAINS))
        self.deck(self.foe, (MOUNTAIN, SWAMP))
        if not active and not short:
            self.add("depart", NATURALIZE if destroy else EXILE, self.foe, "hand")
        self.begin_source()
        self.complete_source()
        self.state()
        if active or short:
            self.fund(self.owner, [int(active), 0, 0, 0, 0, 0])
            self.emit("can_activate", ref="source", value=int(active))
            if active:
                color = 4 if self.mode == "paid_red" else 1
                pool = [int(index == color) for index in range(6)]
                self.emit("choose_color", player=self.owner, value=color)
            self.emit("activate", ref="source", value=int(active))
            if active:
                self.mana("assert_mana", self.owner, pool)
                self.move("source", "graveyard")
                self.assert_zone_ref("source", "graveyard", 1)
                self.resolve()
                self.draw(self.owner, 1)
                self.state(pool)
            else:
                self.state()
        else:
            self.emit("advance_turn")
            self.emit("turn", player=self.foe)
            self.paid_spell("depart", self.foe, target="source")
            self.move("source", "graveyard" if destroy else "exile")
            if destroy:
                self.draw(self.owner, 1)
            self.state()


    def ember_target(self):
        if self.mode == "creature_damage":
            self.emit("choose_target", player=self.owner, ref="recipient")
        else:
            self.emit("choose_player", player=self.owner, value=self.foe)

    def common_decks(self):
        self.deck(self.owner, (ISLAND, PLAINS, FOREST, MOUNTAIN, BEAR))
        self.deck(self.foe, (SWAMP, FOREST, BOOK))

    def funded_source(self, fee, *, mode=None):
        self.emit("turn", player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, fee)
        if mode is not None:
            self.emit("choose_dialog", player=self.owner, value=mode)
        self.emit("cast", ref="source", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner)

    def attack_and_block(self, ref=None, *, legal=False):
        self.start_combat(actor=self.owner, attacker="source")
        self.declaration_end()
        self.emit("begin_blockers")
        if ref is not None:
            self.emit("block", ref=ref, other="source", value=int(legal))
        self.emit("advance_combat")
        self.observe("source", "blocked", int(legal))

    def finish_damage(self):
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")

    def blister(self):
        swamp = self.mode == "swampwalk_dies"
        exiles = self.mode == "exile"
        self.common_decks()
        if exiles or swamp:
            self.add("leave", SWORDS if exiles else KILL, self.owner, "hand")
        if not exiles:
            self.add("blocker", GIANT, self.foe, "battlefield")
        if swamp:
            self.add("swamp", SWAMP, self.foe, "battlefield")
        self.begin_source()
        self.complete_source()
        if exiles:
            self.paid_spell("leave", self.owner, target="source")
            self.move("source", "exile")
            self.life[self.owner] += 2
            self.state()
            return
        self.attack_and_block("blocker", legal=not swamp)
        self.finish_damage()
        if swamp:
            self.life[self.foe] -= 2
            self.state()
            self.paid_spell("leave", self.owner, target="source")
        else:
            self.observe("blocker", "damage", 2)
        self.move("source", "graveyard")
        self.life[self.foe] -= 2
        self.state()

    def ancestor(self):
        actor = self.foe if self.mode == "foe_white_pay" else self.owner
        white = self.mode != "nonwhite"
        paid = self.mode.endswith("_pay")
        available = self.mode != "unfunded"
        take = paid or self.mode == "unfunded"
        self.common_decks()
        self.add("white_or_other", HAWK if white else BEAR, actor, "hand")
        self.add("leave", KILL, self.owner, "hand")
        self.begin_source()
        self.complete_source()
        if actor == self.foe:
            self.emit("advance_turn")
            self.emit("turn", player=actor)
        extra = [int(available), 0, 0, 0, 0, 0]
        fee = self.cost("white_or_other")
        self.fund(self.owner, [a+b for a,b in zip(fee, extra)] if actor == self.owner else extra)
        if actor != self.owner:
            self.fund(actor, fee)
        self.emit("cast", ref="white_or_other", value=1)
        self.emit("assert_stack", value=1)
        if white and take and self.owner == 0:
            self.emit("choose_trigger", ref="source", player=0)
        self.emit("announce_spell", ref="white_or_other")
        self.resolve()
        self.move("white_or_other", "battlefield")
        if paid:
            self.life[self.owner] += 1
        self.state(ZERO if paid else extra)
        self.paid_spell("leave", self.owner, target="source")
        self.move("source", "graveyard")
        self.life[self.owner] += 3
        self.state()

    def centaur(self):
        green = self.mode.startswith("green")
        gain = self.mode != "green_decline"
        self.common_decks()
        self.add("leave", KILL, self.owner, "hand")
        self.funded_source([2, 0, 0, int(green), 0, int(not green)])
        self.complete_source()
        self.state()
        def plan_gain():
            if self.owner == 0 and gain:
                self.emit("choose_trigger", player=0, ref="source")
        self.paid_spell("leave", self.owner, target="source", choices=plan_gain)
        self.move("source", "graveyard")
        if gain:
            self.life[self.owner] += 3
        self.state()

    def prowler(self):
        blocked = self.mode == "infect_block"
        declined = self.mode == "decline_proliferate"
        self.common_decks()
        if blocked:
            self.add("blocker", GIANT, self.foe, "battlefield")
        else:
            self.add("leave", KILL, self.owner, "hand")
        self.begin_source()
        self.complete_source()
        self.attack_and_block("blocker" if blocked else None, legal=blocked)

        def plan_proliferation():
            if self.owner == 0:
                self.emit("choose_dialog", player=0, value=int(declined))
                if declined:
                    self.emit("choose_cancel_target", player=0)
                elif blocked:
                    self.emit("choose_dialog", player=0)

        if blocked:
            plan_proliferation()
        self.finish_damage()
        self.emit("assert_poison", player=self.foe, value=0 if blocked else 2)
        self.emit("assert_poison", player=self.owner)
        if blocked:
            self.move("source", "graveyard")
            self.move("blocker", "graveyard")
        else:
            self.state()
            self.paid_spell("leave", self.owner, target="source", choices=plan_proliferation)
            self.move("source", "graveyard")
            self.emit("assert_poison", player=self.foe, value=2 if declined else 3)
            self.emit("assert_poison", player=self.owner)
        self.state()

    def myr(self):
        borrowed = self.mode == "borrowed"
        exile = self.mode == "exile"
        self.common_decks()
        actor = self.foe if borrowed else self.owner
        self.add("leave", EXILE if exile else KILL, actor, "hand")
        if borrowed:
            self.setup = [row for row in self.setup if row["ref"] != "source"]
            self.add("source", self.name, self.owner, "battlefield", opponent_owned=True)
            self.emit("turn", player=actor)
            self.fund(self.owner, ZERO)
            self.fund(self.foe, ZERO)
        else:
            self.begin_source()
            self.complete_source()
        self.state()
        self.paid_spell("leave", actor, target="source")
        if borrowed:
            self.objects["source"]["player"] = self.foe
        self.move("source", "exile" if exile else "graveyard")
        self.state(ZERO if exile else [1, 0, 0, 0, 0, 0])

    def slithermuse(self):
        evoke = self.mode == "evoke"
        exile = self.mode == "exile"
        equal = self.mode == "equal_hand"
        self.common_decks()
        for index in range(3):
            self.add(f"foe_card_{index}", BOOK, self.foe, "hand")
        if not evoke:
            self.add("leave", SWORDS if exile else BOUNCE, self.foe, "hand")
        if equal:
            for index in range(2):
                self.add(f"own_card_{index}", BOOK, self.owner, "hand")
        self.funded_source([3, 0, 1, 0, 0, 0] if evoke else [2, 0, 2, 0, 0, 0],
                           mode=None if evoke else 0)
        self.resolve()
        if evoke:
            self.move("source", "graveyard")
            self.draw(self.owner, 3)
            self.state()
            return
        self.move("source", "battlefield")
        self.inspect_source()
        self.state()
        self.paid_spell("leave", self.foe, target="source")
        self.move("source", "exile" if exile else "hand")
        if exile:
            self.life[self.owner] += 3
        self.draw(self.owner, 0 if equal else 3 if exile else 2)
        self.state()

    def beetles(self):
        activate = self.mode == "sacrifice_both"
        short = self.mode == "short"
        recipient = self.owner if activate or short else self.foe
        self.common_decks()
        self.add("recipient", BEAR, recipient, "battlefield")
        if not activate and not short:
            self.add("leave", KILL, self.owner, "hand")
        self.begin_source()
        self.complete_source()
        self.state()
        if activate or short:
            fee = [2 if activate else 1, 0, 0, 0, 0, 0]
            self.fund(self.owner, fee)
            if activate:
                self.emit("choose_target", player=self.owner, ref="recipient")
            self.emit("can_activate", ref="source", value=int(activate))
            self.emit("activate", ref="source", value=int(activate))
            if short:
                self.observe("recipient", "power", 2)
                self.state(fee)
                return
            self.mana("assert_mana", self.owner)
            self.resolve()
            self.draw(self.owner, 1)
        else:
            self.paid_spell("leave", self.owner, target="source",
                            choices=lambda: self.emit("choose_target", player=self.owner, ref="recipient"))
        self.move("source", "graveyard")
        for prop in ("power", "toughness"):
            self.observe("recipient", prop, 3)
        self.state()
        self.emit("cleanup")
        self.emit("refresh")
        for prop in ("power", "toughness"):
            self.observe("recipient", prop, 2)
        self.state()

    def terrarion(self):
        activate = self.mode in ("mixed_mana", "two_same")
        short = self.mode == "tapped_short"
        destroy = self.mode == "destroy_draw"
        self.common_decks()
        if not activate and not short:
            self.add("leave", NATURALIZE if destroy else EXILE, self.foe, "hand")
        self.begin_source()
        self.complete_source()
        self.observe("source", "tapped", 1)
        self.state()
        if activate or short:
            if activate:
                self.emit("advance_turn")
                self.emit("advance_turn")
                self.emit("untap_step", player=self.owner)
                self.observe("source", "tapped", 0)
            fee = [2, 0, 0, 0, 0, 0]
            self.fund(self.owner, fee)
            self.emit("can_activate", ref="source", value=int(activate))
            if activate:
                colors = (1, 4) if self.mode == "mixed_mana" else (2, 2)
                for color in colors:
                    self.emit("choose_color", player=self.owner, value=color)
            self.emit("activate", ref="source", value=int(activate))
            if short:
                self.state(fee)
                return
            pool = [colors.count(index) for index in range(6)]
            self.mana("assert_mana", self.owner, pool)
            self.move("source", "graveyard")
            self.resolve()
            self.draw(self.owner, 1)
            self.state(pool)
        else:
            self.emit("advance_turn")
            self.emit("turn", player=self.foe)
            self.paid_spell("leave", self.foe, target="source")
            self.move("source", "graveyard" if destroy else "exile")
            if destroy:
                self.draw(self.owner, 1)
            self.state()

    def augur(self):
        target = self.owner if self.mode == "own_graveyard" else self.foe
        empty = self.mode == "empty_foe"
        self.common_decks()
        self.add("leave", KILL, self.owner, "hand")
        for player in (0, 1):
            self.add(f"noncreature_{player}", BOOK, player, "graveyard")
            if not empty or player == self.owner:
                self.add(f"corpse_{player}", BEAR, player, "graveyard")
                self.add(f"other_corpse_{player}", GIANT, player, "graveyard")
        self.begin_source()
        self.complete_source()
        self.paid_spell("leave", self.owner, target="source",
                        choices=lambda: self.emit("choose_player", player=self.owner, value=target))
        self.move("source", "graveyard")
        amount = 0 if empty else 2 + int(target == self.owner)
        self.draw(self.owner, amount)
        self.life[self.owner] -= amount
        self.state()

    def implement(self):
        paid = self.mode == "paid"
        activate = self.mode in ("paid", "wrong_color")
        destroy = self.mode == "destroy_draw"
        life = self.csvid == IMPROVEMENT
        self.common_decks()
        if not activate:
            self.add("leave", NATURALIZE if destroy else EXILE, self.foe, "hand")
        self.begin_source()
        self.complete_source()
        self.state()
        if activate:
            color = 5 if life else 2
            fee = [int(index == (color if paid else 4)) for index in range(6)]
            self.fund(self.owner, fee)
            self.emit("can_activate", ref="source", value=int(paid))
            self.emit("activate", ref="source", value=int(paid))
            self.emit("assert_stack", value=int(paid))
            if not paid:
                self.state(fee)
                return
            self.mana("assert_mana", self.owner)
            self.move("source", "graveyard")
            self.assert_zone_ref("source", "graveyard", 1)
            self.resolve()
            self.draw(self.owner, 1 if life else 2)
            if life:
                self.life[self.owner] += 2
        else:
            self.emit("advance_turn")
            self.emit("turn", player=self.foe)
            self.paid_spell("leave", self.foe, target="source")
            self.move("source", "graveyard" if destroy else "exile")
            if destroy:
                self.draw(self.owner, 1)
        self.state()

    def run(self):
        scenarios = {
            ASHEN: self.zubera, DRIPPING: self.zubera, CATHODION: self.cathodion, STAR: self.star,
            EMBER: self.zubera, FLOATING: self.zubera, SILENT: self.zubera, BLISTER: self.blister,
            ANCESTOR: self.ancestor, CENTAUR: self.centaur, PROWLER: self.prowler, MYR: self.myr,
            SLITHERMUSE: self.slithermuse, BEETLES: self.beetles, TERRARION: self.terrarion,
            AUGUR: self.augur, EXAMINATION: self.implement, IMPROVEMENT: self.implement,
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
