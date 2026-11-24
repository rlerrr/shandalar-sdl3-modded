"""Kinship upkeep scenarios from exact pinned Oracle records."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = "library_kinship_upkeep"
PROFILE = Path(__file__).with_name("data") / "library_kinship_upkeep_wave8_profiles.json"
ZEPHYR, ELDER, CONSUL, GORGER = 9374, 9377, 9410, 9429
GRUB, GRAY, WOLF_SHAMAN = 9435, 9457, 9465
MODES = {
    ZEPHYR: ("match_combat", "nonmatch", "decline_reveal"),
    ELDER: ("cast_creature", "cast_kindred", "pass_after_reveal", "nonmatch"),
    CONSUL: ("matched_fires", "nonmatch", "decline_reveal"),
    GORGER: ("both_hands", "empty_hands", "nonmatch", "decline_reveal"),
    GRUB: ("opponent_discards", "nonmatch", "decline_reveal"),
    GRAY: ("gain_four", "nonmatch", "decline_reveal"),
    WOLF_SHAMAN: ("create_wolf", "nonmatch", "decline_reveal"),
}
PREFIX = "LibraryKinshipUpkeepRecipe"
SEED = 2857
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
BEAR, GIANT, BOOK = "Grizzly Bears", "Hill Giant", "Spellbook"
SOLDIER, TREEFOLK, ROOTGRAPPLE = "Eager Cadet", "Ironroot Treefolk", "Rootgrapple"
SHAMAN, KNIGHT, GOBLIN = "Elvish Visionary", "Silver Knight", "Mons's Goblin Raiders"
PLAINS, FOREST, ISLAND, SWAMP, MOUNTAIN = "Plains", "Forest", "Island", "Swamp", "Mountain"
WITNESSES = {BEAR, GIANT, BOOK, SOLDIER, TREEFOLK, ROOTGRAPPLE, SHAMAN, KNIGHT, GOBLIN,
             PLAINS, FOREST, ISLAND, SWAMP, MOUNTAIN}

WOLF = "Wolf"
WITNESSES.add(WOLF)
ZONE_NAMES = {code: name for name, code in ZONES.items()}
FOLLOWERS = (GRUB, GRAY, WOLF_SHAMAN)
GAPS = {
    csvid: ["The headless driver has no independent look or reveal display receipt. The AI reveals a matching card automatically."]
    for csvid in MODES
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get("layout") != "normal" or record.get("selected_face")
            or record.get("card_faces") or "Kinship" not in record.get("keywords", ())):
        raise ValueError("whole pinned Kinship record required")
    return dict(csvid=csvid, printed_mana=cost_from_oracle(record), rules=record["oracle_text"])


def variants(shape):
    return [f"{mode}_{seat}" for mode in MODES[shape["csvid"]]
            for seat in ((0,) if mode == "decline_reveal" else (0, 1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError("distinct supported Kinship scenarios required")


def spec(declaration, record):
    source = profiles()["sources"].get(record.get("name"))
    if not source or not exact(source["oracle"], record):
        raise ValueError("exact pinned Kinship primary required")
    shape = classify(record, source["registry"]["csvid"])
    if (declaration.get("family") != FAMILY or type(declaration.get("version")) is not int
            or declaration["version"] != 2 or declaration.get("oracle_text") != record["oracle_text"]
            or not exact(declaration.get("params"), shape)):
        raise ValueError("versioned whole Kinship declaration required")
    validate_variants(shape, declaration.get("variants"))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {"sources", "witnesses"} or set(pins["witnesses"]) != WITNESSES:
        raise ValueError("independent exact Kinship witnesses required")
    if {pin["registry"]["csvid"] for pin in pins["sources"].values()} != set(MODES):
        raise ValueError("exact Kinship primaries required")
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin["oracle"]) or not exact(row, pin["registry"])
                    or row.get("kind") != "card"
                    or oracle[name].get("layout") != ("token" if name == WOLF else "normal")):
                raise ValueError("changed Kinship source or witness: " + name)
            if group == "sources":
                if not str(row.get("handler", "")).startswith("manalinkeh_card_"):
                    raise ValueError("implemented Kinship primary required")
                classify(oracle[name], row["csvid"])
    required_types = {
        SOLDIER: "Creature — Human Soldier", TREEFOLK: "Creature — Treefolk",
        ROOTGRAPPLE: "Kindred Instant — Treefolk", SHAMAN: "Creature — Elf Shaman",
        GOBLIN: "Creature — Goblin", GIANT: "Creature — Giant", WOLF: "Token Creature — Wolf",
    }
    if any(oracle[name]["type_line"] != kind for name, kind in required_types.items()):
        raise ValueError("Kinship witness types changed")
    if (oracle[ROOTGRAPPLE]["oracle_text"] != "Destroy target noncreature permanent. If you control a Treefolk, draw a card."
            or oracle[KNIGHT]["oracle_text"] != "First strike, protection from red"
            or (oracle[WOLF]["power"], oracle[WOLF]["toughness"], oracle[WOLF]["colors"]) != ("2", "2", ["G"])):
        raise ValueError("Kinship payoff witnesses changed")


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
        if op in {"untap_step", "upkeep_step", "draw_step"}:
            self.steps.append(step(op, player=player))
            return
        fields = dict(ref=ref, other=other, property=prop, player=player, value=value, index=index, color=color)
        values = {key: item for key, item in fields.items() if item or key == "value"}
        if op in {"set_mana", "assert_mana"}:
            values.update(player=player, color=color)
        elif op.startswith("assert_zone"):
            values.update(player=player, zone=ZONE_NAMES[zone])
        elif op in {"choose_trigger_source", "assert_matching_permanents"}:
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

    def receipts(self):
        for seat in (0, 1):
            self.emit("assert_life", player=seat, value=self.life[seat])
            self.mana("assert_mana", seat)
            objects = [obj for obj in self.objects.values() if obj["player"] == seat]
            counts = Counter((obj["name"], obj["zone"]) for obj in objects)
            for name in sorted({obj["name"] for obj in objects}):
                for zone, code in ZONES.items():
                    self.emit("assert_zone_multiplicity", player=seat, zone=code,
                              index=self.rows[name]["csvid"], value=counts[name, zone])

    def deck(self, seat, names):
        for index, name in enumerate(names):
            self.add(f"lib_{seat}_{index}", name, seat, "library")

    def library_prefix(self, seat, amount, destination):
        refs = self.library[seat][:amount]
        for ref in refs:
            self.move(ref, destination)
        return [self.objects[ref]["name"] for ref in refs]

    def observe_library(self, seat):
        self.emit("assert_zone", player=seat, zone=ZONES["library"], value=len(self.library[seat]))
        for index, ref in enumerate(self.library[seat]):
            csvid = self.rows[self.objects[ref]["name"]]["csvid"]
            self.emit("assert_zone_card", player=seat, zone=ZONES["library"], index=index, value=csvid)

    def state(self):
        for seat in (0, 1):
            self.observe_library(seat)
        self.receipts()

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

    def untap(self, seat):
        self.emit("advance_turn")
        self.emit("untap_step", player=seat)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")

    def upkeep(self, seat):
        self.untap(seat)
        self.emit("upkeep_step", player=seat)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")

    def draw(self, seat):
        self.emit("draw_step", player=seat)
        self.emit("assert_choices_consumed")
        self.emit("assert_stack")
        self.library_prefix(seat, 1, "hand")

    @property
    def reveal_accepted(self):
        return self.mode not in ("nonmatch", "decline_reveal")

    def resolve_kinship(self, top, *, play=None, play_target=None, discards=False):
        self.deck(self.owner, (top, ISLAND, FOREST, PLAINS, MOUNTAIN))
        self.deck(self.foe, (SWAMP, MOUNTAIN, ISLAND, FOREST, PLAINS))
        self.cast_source()
        self.upkeep(self.foe)
        self.state()
        self.untap(self.owner)
        if discards:
            self.emit("discard_policy", value=1)
        if self.owner == 0:
            self.emit("choose_trigger_source", ref="source", player=0)
            # A human mismatch presents an informational OK notice.
            self.emit("choose_dialog", player=0, value=int(self.mode == "decline_reveal"))
        if play is not None:
            assert self.reveal_accepted
            # A queued AI Play decision would otherwise be consumed by the AI reveal notice.
            if self.owner == 1:
                self.emit("choose_dialog", player=1, value=0)
            self.emit("choose_dialog", player=self.owner, value=int(not play))
        if play_target is not None:
            assert play
            self.emit("choose_target", ref=play_target, player=self.owner)
        self.emit("upkeep_step", player=self.owner)
        self.emit("assert_unused_choices")
        self.emit("assert_stack")
        self.emit("refresh")
        if discards:
            self.emit("discard_policy", value=0)

    def zephyrnaut(self):
        self.add("ground", BEAR, self.foe, "battlefield")
        self.resolve_kinship(BEAR if self.mode == "nonmatch" else SOLDIER)
        expected = 4 if self.reveal_accepted else 2
        self.observe("source", "power", expected)
        self.observe("source", "toughness", expected)
        self.state()
        self.draw(self.owner)
        if self.reveal_accepted:
            self.emit("turn", player=self.owner)
            self.emit("begin_attackers")
            self.emit("attack", ref="source", value=1)
            self.observe("source", "tapped", 0)
            self.emit("finish_attackers")
            self.emit("begin_blockers")
            self.emit("block", ref="ground", other="source", value=0)
            self.emit("advance_combat")
            self.observe("source", "blocked", 0)
            self.emit("combat_damage")
            self.resolve()
            self.emit("end_combat")
            self.life[self.foe] -= 4
            self.emit("cleanup")
            self.emit("refresh")
            self.inspect_source()
        self.state()

    def elder(self):
        kindred = self.mode == "cast_kindred"
        play = self.mode in ("cast_creature", "cast_kindred")
        top = ROOTGRAPPLE if kindred else BEAR if self.mode == "nonmatch" else TREEFOLK
        if kindred:
            self.add("artifact", BOOK, self.foe, "battlefield")
        self.resolve_kinship(top, play=(play if self.reveal_accepted else None),
                             play_target="artifact" if kindred else None)
        if kindred:
            self.library_prefix(self.owner, 1, "graveyard")
            self.move("artifact", "graveyard")
            self.library_prefix(self.owner, 1, "hand")
        elif play:
            self.library_prefix(self.owner, 1, "battlefield")
        self.inspect_source()
        self.state()
        self.draw(self.owner)
        self.state()

    def consul(self):
        for seat in (self.owner, self.foe):
            self.add(f"small_{seat}", BEAR, seat, "battlefield")
            self.add(f"large_{seat}", GIANT, seat, "battlefield")
        self.add("protected", KNIGHT, self.foe, "battlefield")
        self.add("artifact", BOOK, self.foe, "battlefield")
        self.resolve_kinship(BEAR if self.mode == "nonmatch" else SHAMAN)
        marked = 2 if self.reveal_accepted else 0
        for seat in (self.owner, self.foe):
            if self.reveal_accepted:
                self.move(f"small_{seat}", "graveyard")
            self.observe(f"large_{seat}", "damage", marked)
        self.inspect_source()
        self.observe("source", "damage", marked)
        self.observe("protected", "damage", 0)
        self.state()
        if self.reveal_accepted:
            self.emit("cleanup")
            self.emit("refresh")
            for ref in ("source", "large_0", "large_1", "protected"):
                self.observe(ref, "damage", 0)

    def gorger(self):
        old_hand = []
        if self.mode != "empty_hands":
            for seat in (self.owner, self.foe):
                for index, name in enumerate((SOLDIER, GIANT) if seat == self.owner else (BEAR, BOOK)):
                    ref = f"prior_{seat}_{index}"
                    self.add(ref, name, seat, "hand")
                    old_hand.append(ref)
        self.resolve_kinship(BEAR if self.mode == "nonmatch" else GOBLIN, discards=True)
        if self.reveal_accepted:
            for ref in old_hand:
                self.move(ref, "graveyard")
            for seat in (self.owner, self.foe):
                self.library_prefix(seat, 4, "hand")
        self.inspect_source()
        self.state()

    def grubfellows(self):
        self.add("opponent_hand", BOOK, self.foe, "hand")
        self.add("own_hand", SOLDIER, self.owner, "hand")
        self.resolve_kinship(BEAR if self.mode == "nonmatch" else GOBLIN, discards=True)
        if self.reveal_accepted:
            self.move("opponent_hand", "graveyard")
        self.state()

    def graybeard(self):
        self.resolve_kinship(BEAR if self.mode == "nonmatch" else GIANT)
        if self.reveal_accepted:
            self.life[self.owner] += 4
        self.state()

    def wolf_skull(self):
        wolf = self.rows[WOLF]["csvid"]
        self.resolve_kinship(BEAR if self.mode == "nonmatch" else SHAMAN)
        self.state()
        for seat in (0, 1):
            expected = int(self.reveal_accepted and seat == self.owner)
            self.emit("assert_zone_multiplicity", player=seat, zone=ZONES["battlefield"],
                      index=wolf, value=expected)
        if self.reveal_accepted:
            for prop, value in (("power", 2), ("toughness", 2), ("color", 1 << 3), ("token", 1), ("subtype_Wolf", 1)):
                self.emit("assert_matching_permanents", player=self.owner, prop=prop,
                          index=wolf, value=value)

    def run(self):
        scenarios = {
            ZEPHYR: self.zephyrnaut, ELDER: self.elder, CONSUL: self.consul, GORGER: self.gorger,
            GRUB: self.grubfellows, GRAY: self.graybeard, WOLF_SHAMAN: self.wolf_skull,
        }
        scenarios[self.csvid]()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()["sources"][name]["registry"]["csvid"] != csvid:
        raise ValueError("exact registered Kinship primary CSVID required")
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
