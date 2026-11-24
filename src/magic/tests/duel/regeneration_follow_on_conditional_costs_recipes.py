"""Counter, snow and life regeneration scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "regeneration_follow_on_conditional_costs"
PROFILE = Path(__file__).with_name("data") / "regeneration_follow_on_conditional_costs_wave8_profiles.json"
DEITY, RIME, GHOUL, SENTRY = 9762, 8428, 215, 13546
SPINY, CHANGELING, YULE = 2356, 9260, 16253
MODES = {
    DEITY: ("black_preemptive", "green_preemptive", "exhaust_counters", "trample"),
    RIME: ("snow_land", "ordinary_land", "ordinary_pool", "snow_already_floated"),
    GHOUL: ("own_end", "opposing_end", "two_deaths", "noncreature_only"),
    SENTRY: ("pay_both", "wrong_mana", "short_life", "flying_vigilance"),
    SPINY: ("once", "twice", "opposing_end", "no_regeneration"),
    CHANGELING: ("pay_regenerate", "short_generic", "wrong_color", "shared_types"),
    YULE: ("opposing_creature", "allied_creature", "indestructible", "lands_only"),
}
PREFIX = "RegenerationFollowOnConditionalCostsRecipe"
SEED = 2964
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
MURDER, BEAR, BOOK = "Murder", "Grizzly Bears", "Spellbook"
SNOW, SWAMP = "Snow-Covered Swamp", "Swamp"
WITNESSES = {MURDER, BEAR, BOOK, SNOW, SWAMP}

GIANT, COAT, INGOT, FOREST = "Hill Giant", "Coat of Arms", "Darksteel Ingot", "Forest"
WITNESSES |= {GIANT, COAT, INGOT, FOREST}
STARFISH_TOKEN = {
    "registry": {"csvid": 18834, "iid": 9315, "name": "Starfish", "handler": "manalinkeh_card_rhino_token", "kind": "card"},
    "power": 0, "toughness": 1, "color": 1 << 2, "subtype": "Starfish",
}
ZONE_NAMES = {code: name for name, code in ZONES.items()}
FOLLOWERS = (SPINY, CHANGELING, YULE)
WINDOW = "Legacy destruction-window payment is exercised. Advance setup and expiry of a preemptive shield are unobserved."
GAPS = {
    RIME: [WINDOW],
    GHOUL: [WINDOW, "No named corpse-counter observer. Repeated zero-mana destruction responses infer exact count."],
    SENTRY: [WINDOW],
    SPINY: [WINDOW, "Registered Starfish identity and generated token characteristics are observed. There is no named Starfish subtype observer."],
    CHANGELING: [WINDOW],
    YULE: ["The actual digital handler only charges red and green and plays a sound. The paper food consumption has no duel-state representation or driver input."],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get("layout") != "normal" or record.get("selected_face")
            or record.get("card_faces") or "Regenerate" not in record.get("keywords", ())):
        raise ValueError("whole pinned conditional regeneration record required")
    return dict(csvid=csvid, printed_mana=record["mana_cost"], rules=record["oracle_text"])


def variants(shape):
    return [f"{mode}_{seat}" for mode in MODES[shape["csvid"]]
            for seat in ((0,) if mode == "decline_reveal" else (0, 1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError("distinct supported conditional regeneration scenarios required")


def spec(declaration, record):
    source = profiles()["sources"].get(record.get("name"))
    if not source or not exact(source["oracle"], record):
        raise ValueError("exact pinned conditional regeneration primary required")
    shape = classify(record, source["registry"]["csvid"])
    if (declaration.get("family") != FAMILY or type(declaration.get("version")) is not int
            or declaration["version"] != 2 or declaration.get("oracle_text") != record["oracle_text"]
            or not exact(declaration.get("params"), shape)):
        raise ValueError("versioned whole conditional regeneration declaration required")
    validate_variants(shape, declaration.get("variants"))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {"sources", "witnesses", "token_witnesses"} or set(pins["witnesses"]) != WITNESSES:
        raise ValueError("independent exact conditional regeneration witnesses required")
    if {pin["registry"]["csvid"] for pin in pins["sources"].values()} != set(MODES):
        raise ValueError("exact conditional regeneration primaries required")
    if not exact(pins["token_witnesses"], {"Starfish": STARFISH_TOKEN}) or not exact(rows.get("Starfish"), STARFISH_TOKEN["registry"]):
        raise ValueError("the production Starfish token registration changed")
    for group in ("sources", "witnesses"):
        for name, pin in pins[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin["oracle"]) or not exact(row, pin["registry"])
                    or row.get("kind") != "card"
                    or oracle[name].get("layout") != "normal"):
                raise ValueError("changed conditional regeneration source or witness: " + name)
            if group == "sources":
                if not str(row.get("handler", "")).startswith("manalinkeh_card_"):
                    raise ValueError("implemented conditional regeneration primary required")
                classify(oracle[name], row["csvid"])
    if (oracle[MURDER]["oracle_text"] != "Destroy target creature."
            or oracle[SNOW]["type_line"] != "Basic Snow Land — Swamp"
            or oracle[SWAMP]["type_line"] != "Basic Land — Swamp"
            or oracle[SNOW]["oracle_text"] != oracle[SWAMP]["oracle_text"]
            or not oracle[COAT]["oracle_text"].startswith("Each creature gets +1/+1 for each other creature on the battlefield that shares at least one creature type with it.")
            or not oracle[INGOT]["oracle_text"].startswith("Indestructible")
            or oracle[BEAR]["mana_cost"] != "{1}{G}"):
        raise ValueError("conditional regeneration gameplay witnesses changed")


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
        if op in {"untap_step", "upkeep_step"}:
            self.steps.append(step(op, player=player))
            return
        fields = dict(ref=ref, other=other, property=prop, player=player, value=value, index=index, color=color)
        values = {key: item for key, item in fields.items() if item or key == "value"}
        if op in {"set_mana", "assert_mana"}:
            values.update(player=player, color=color)
        elif op.startswith("assert_zone"):
            values.update(player=player, zone=ZONE_NAMES[zone])
        elif op in {"assert_matching_permanents", "choose_trigger_source"}:
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
        if ref == "source" and self.csvid == DEITY:
            return self.colored_pool(3 if self.mode == "green_preemptive" else 1, 5)
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
                amount = int(self.oracle[self.name][stat]) - (2 if self.csvid == DEITY else 0)
                if self.csvid == CHANGELING and self.mode == "shared_types":
                    amount += 2
                self.observe("source", stat, amount)

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

    def outcomes(self, survives, *, pool=None, tapped=False):
        self.observe("source", "in_play", int(survives))
        if survives:
            self.observe("source", "damage", 0)
            self.observe("source", "tapped", int(tapped))
        else:
            self.move("source", "graveyard")
        self.receipts((pool or ZERO, ZERO) if self.owner == 0 else (ZERO, pool or ZERO))

    def colored_pool(self, color, count=1):
        pool = list(ZERO)
        pool[color] = count
        return pool

    def prepare_murder(self):
        self.fund(self.foe, self.cost("murder"))
        self.emit("choose_target", ref="source", player=self.foe)
        self.emit("cast", ref="murder", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.foe)

    def finish_murder(self, *, eligible, land_choice=False):
        self.emit("choose_regeneration", ref="source")
        if land_choice:
            self.emit("choose_target", ref="land", player=self.owner)
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_unused_choices", value=int(not eligible))
        self.emit("assert_stack")
        self.move("murder", "graveyard")

    def deity_state(self, counters, *, tapped=False):
        self.observe("source", "in_play", 1)
        self.observe("source", "minus1_counters", counters)
        self.observe("source", "power", 7 - counters)
        self.observe("source", "toughness", 7 - counters)
        self.observe("source", "damage", 0)
        self.observe("source", "tapped", int(tapped))

    def deity_activate(self, color, counters):
        self.fund(self.owner, self.colored_pool(color))
        self.emit("can_activate", ref="source", value=1)
        self.emit("activate", ref="source", value=1)
        self.emit("assert_stack", value=1)
        self.mana("assert_mana", self.owner)
        self.observe("source", "minus1_counters", counters)
        self.resolve()
        self.deity_state(counters)

    def begin_attack(self, *, block):
        self.emit("advance_turn")
        self.emit("advance_turn")
        self.emit("turn", player=self.owner)
        self.emit("begin_attackers")
        self.emit("attack", ref="source", value=1)
        self.emit("finish_attackers")
        self.emit("begin_blockers")
        self.emit("block", ref="blocker", other="source", value=int(block))
        self.emit("advance_combat")

    def finish_attack(self):
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")

    def deity(self):
        if self.mode == "trample":
            self.add("blocker", BEAR, self.foe, "battlefield")
        elif self.mode != "exhaust_counters":
            self.add("murder", MURDER, self.foe, "hand")
        self.cast_source()
        self.deity_state(2)
        if self.mode == "trample":
            self.begin_attack(block=True)
            self.finish_attack()
            self.life[self.foe] -= 3
            self.move("blocker", "graveyard")
            self.observe("source", "damage", 2)
            self.observe("source", "minus1_counters", 2)
            self.receipts()
        elif self.mode == "exhaust_counters":
            self.deity_activate(1, 1)
            self.deity_activate(3, 0)
            self.fund(self.owner, self.colored_pool(1))
            self.emit("can_activate", ref="source", value=0)
            self.emit("activate", ref="source", value=0)
            self.mana("assert_mana", self.owner, self.colored_pool(1))
            self.fund(self.owner, ZERO)
            for _ in range(2):
                self.emit("destroy", ref="source")
                self.emit("refresh")
                self.deity_state(0, tapped=True)
            self.emit("destroy", ref="source")
            self.outcomes(False)
        else:
            self.deity_activate(3 if self.mode == "green_preemptive" else 1, 1)
            self.prepare_murder()
            self.resolve()
            self.move("murder", "graveyard")
            self.deity_state(1, tapped=True)
            self.receipts()

    def rimebound(self):
        snow = self.mode in ("snow_land", "snow_already_floated")
        float_first = self.mode in ("snow_already_floated", "ordinary_pool")
        self.add("land", SNOW if snow else SWAMP, self.owner, "battlefield")
        self.add("murder", MURDER, self.foe, "hand")
        self.cast_source()
        if float_first:
            self.emit("activate", ref="land", value=1)
            self.emit("assert_stack")
            self.mana("assert_mana", self.owner, self.colored_pool(1))
            self.observe("land", "tapped", 1)
        self.prepare_murder()
        self.finish_murder(eligible=snow, land_choice=self.mode == "snow_land")
        self.observe("land", "tapped", int(snow or float_first))
        self.outcomes(snow, tapped=snow, pool=self.colored_pool(1) if self.mode == "ordinary_pool" else ZERO)

    def ghoul(self):
        opponent_end = self.mode == "opposing_end"
        deaths = 0 if self.mode == "noncreature_only" else 2 if self.mode == "two_deaths" else 1
        self.add("artifact", BOOK, self.foe, "battlefield")
        for index in range(deaths):
            self.add(f"corpse_{index}", BEAR, self.foe if index == 0 else self.owner, "battlefield")
        self.cast_source()
        if opponent_end:
            self.emit("advance_turn")
            self.emit("turn", player=self.foe)
        self.emit("destroy", ref="artifact")
        self.move("artifact", "graveyard")
        for index in range(deaths):
            ref = f"corpse_{index}"
            self.emit("destroy", ref=ref)
            self.move(ref, "graveyard")
        self.receipts()
        self.emit("advance_turn")
        for index in range(deaths + 1):
            survives = index < deaths
            self.emit("choose_regeneration", ref="source")
            self.emit("destroy", ref="source")
            self.emit("refresh")
            self.emit("assert_unused_choices", value=int(not survives))
            self.outcomes(survives, tapped=survives)

    def sentry(self):
        if self.mode == "flying_vigilance":
            self.add("blocker", BEAR, self.foe, "battlefield")
            self.cast_source()
            self.begin_attack(block=False)
            self.observe("source", "attacking", 1)
            self.observe("source", "tapped", 0)
            self.finish_attack()
            self.life[self.foe] -= 3
            self.outcomes(True)
            return
        self.add("murder", MURDER, self.foe, "hand")
        self.cast_source()
        if self.mode == "short_life":
            self.life[self.owner] = 2
            self.emit("set_life", player=self.owner, value=2)
        eligible = self.mode == "pay_both"
        mana = [0, 1, int(self.mode == "wrong_mana"), 0, 0, int(self.mode != "wrong_mana")]
        self.fund(self.owner, mana)
        self.prepare_murder()
        self.finish_murder(eligible=eligible)
        if eligible:
            self.life[self.owner] -= 3
        self.outcomes(eligible, pool=ZERO if eligible else mana, tapped=eligible)

    def starfish_receipts(self, amount):
        csvid = STARFISH_TOKEN["registry"]["csvid"]
        for player in (0, 1):
            self.emit("assert_zone_multiplicity", player=player, zone=ZONES["battlefield"],
                      index=csvid, value=amount if player == self.owner else 0)
        if amount:
            for prop in ("power", "toughness", "color", "token"):
                expected = 1 if prop == "token" else STARFISH_TOKEN[prop]
                self.emit("assert_matching_permanents", player=self.owner, prop=prop, index=csvid, value=expected)

    def starfish(self):
        amount = {"once": 1, "twice": 2, "opposing_end": 1, "no_regeneration": 0}[self.mode]
        self.cast_source()
        if self.mode == "opposing_end":
            self.emit("advance_turn")
            self.emit("turn", player=self.foe)
        self.fund(self.owner, self.colored_pool(2, amount))
        self.starfish_receipts(0)
        for remaining in reversed(range(amount)):
            self.emit("choose_regeneration", ref="source")
            self.emit("destroy", ref="source")
            self.emit("refresh")
            self.emit("assert_unused_choices")
            self.observe("source", "in_play", 1)
            self.observe("source", "tapped", 1)
            self.observe("source", "damage", 0)
            self.mana("assert_mana", self.owner, self.colored_pool(2, remaining))
            self.starfish_receipts(0)
        self.emit("advance_turn")
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        self.starfish_receipts(amount)
        self.receipts()

    def changeling(self):
        if self.mode == "shared_types":
            self.add("anthem", COAT, self.foe, "battlefield")
            self.add("bear", BEAR, self.owner, "battlefield")
            self.add("giant", GIANT, self.foe, "battlefield")
            self.cast_source()
            for prop in ("subtype_Bear", "subtype_Goblin", "subtype_Dragon", "subtype_Sliver", "subtype_Elf"):
                self.emit("assert_matching_permanents", player=self.owner, prop=prop, index=self.csvid, value=1)
            for ref, stats in (("bear", 3), ("giant", 4)):
                for prop in ("power", "toughness"):
                    self.observe(ref, prop, stats)
            self.receipts()
            return
        self.add("murder", MURDER, self.foe, "hand")
        self.cast_source()
        paid = self.mode == "pay_regenerate"
        mana = [int(self.mode != "short_generic"), int(self.mode != "wrong_color"),
                int(self.mode == "wrong_color"), 0, 0, 0]
        self.fund(self.owner, mana)
        self.prepare_murder()
        self.finish_murder(eligible=paid)
        self.outcomes(paid, pool=ZERO if paid else mana, tapped=paid)

    def upkeep(self, seat):
        self.emit("advance_turn")
        self.emit("untap_step", player=seat)
        self.emit("upkeep_step", player=seat)
        self.emit("assert_unused_choices")
        self.emit("assert_stack")
        self.emit("refresh")

    def yule(self):
        self.add("own_land", FOREST, self.owner, "battlefield")
        self.add("rival_land", FOREST, self.foe, "battlefield")
        dead = self.mode in ("opposing_creature", "allied_creature")
        if dead or self.mode == "indestructible":
            self.add("other", BEAR if dead else INGOT,
                     self.owner if self.mode == "allied_creature" else self.foe, "battlefield")
        self.cast_source()
        self.upkeep(self.foe)
        self.observe("source", "plus1_counters", 0)
        self.receipts()
        self.upkeep(self.owner)
        counters = 2 if dead else 3 if self.mode == "indestructible" else 0
        if dead:
            self.move("other", "graveyard")
        self.observe("source", "plus1_counters", counters)
        self.observe("source", "power", 1 + counters)
        self.observe("source", "toughness", 1 + counters)
        self.receipts()

    def run(self):
        scenarios = {
            DEITY: self.deity, RIME: self.rimebound, GHOUL: self.ghoul, SENTRY: self.sentry,
            SPINY: self.starfish, CHANGELING: self.changeling, YULE: self.yule,
        }
        scenarios[self.csvid]()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()["sources"][name]["registry"]["csvid"] != csvid:
        raise ValueError("exact registered conditional regeneration primary CSVID required")
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
