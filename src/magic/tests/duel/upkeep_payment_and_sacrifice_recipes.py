"""Build exact upkeep payment and sacrifice cases through real turn boundaries."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from generate_cases import OPS
from recipes import cost_from_oracle
from death_counter_trigger_recipes import exact


RIFT, AKUTA, ANOWON, ANURID = 4938, 7600, 10721, 5767
MODES = {
    RIFT: ("return_creature", "opponent_pays", "controller_pays", "noncreature"),
    AKUTA: ("return_haste", "equal_hand", "no_swamp", "decline"),
    ANOWON: ("both_sacrifice", "one_ineligible", "vampires_only"),
    ANURID: ("pay_protection", "decline_payment", "empty_graveyard"),
}
SINGLE = {(AKUTA, "decline")}
PREFIX = "UpkeepPaymentAndSacrificeRecipe"
SEED = 3686
ZONES = {"hand": 0, "battlefield": 1, "library": 2, "graveyard": 3, "exile": 4}
ZERO = [0] * 6
FOREST, ISLAND, MOUNTAIN, PLAINS, SWAMP = "Forest", "Island", "Mountain", "Plains", "Swamp"
BOLT, BEAR, BOOK, GIANT = "Lightning Bolt", "Grizzly Bears", "Spellbook", "Hill Giant"
VAMPIRE, BLACK, MURDER = "Child of Night", "Scathe Zombies", "Murder"

FAMILY = 'upkeep_payment_and_sacrifice'
PROFILE = Path(__file__).with_name('data') / 'upkeep_payment_and_sacrifice_wave8_profiles.json'
ARCADES, GHOUL, GRASP, SPEAKER, BRAIDS, BRASS, PIT = 568, 2939, 1872, 7167, 5495, 930, 1755
BRINK, CALL, CHROMIUM, VULTURES, CONVERSION, HORROR, LARVA = 3974, 6488, 595, 2954, 45, 600, 7032
SOURCES = {RIFT: 'invasion.c', AKUTA: 'saviors_of_kamigawa.c', ANOWON: 'worldwake.c', ANURID: 'torment.c',
           ARCADES: 'legends.c', GHOUL: 'weatherlight.c', GRASP: 'ice_age.c', SPEAKER: 'champions_of_kamigawa.c',
           BRAIDS: 'odissey.c', BRASS: 'arabian_nights.c', PIT: 'fallen_empires.c', BRINK: 'urza_legacy.c',
           CALL: 'scourge.c', CHROMIUM: 'legends.c', VULTURES: 'weatherlight.c', CONVERSION: 'unlimited.c',
           HORROR: 'legends.c', LARVA: 'fifth_dawn.c'}
FOLLOWERS = {
    ARCADES: ('paid', 'short', 'decline', 'team_pump'),
    GHOUL: ('top_creature', 'no_creature', 'decline'),
    GRASP: ('paid', 'short', 'decline'),
    SPEAKER: ('search_return', 'decline', 'no_demon'),
    BRAIDS: ('artifact', 'land', 'creature', 'ineligible'),
    BRASS: ('paid', 'short', 'ready', 'decline'),
    PIT: ('paid', 'short', 'decline'),
    BRINK: ('empty', 'one_card'),
    CALL: ('both_pay', 'empty_end', 'zombie_end'),
    CHROMIUM: ('paid', 'short', 'rampage'),
    VULTURES: ('top_creature', 'no_creature', 'decline', 'discard'),
    CONVERSION: ('paid', 'short', 'decline'),
    HORROR: ('paid', 'short', 'first_strike', 'indestructible'),
    LARVA: ('paid', 'short', 'decline', 'trample'),
}
FEES = {
    ARCADES: (FOREST, ISLAND, PLAINS), GRASP: (ISLAND, FOREST), PIT: (SWAMP, SWAMP),
    CHROMIUM: (SWAMP, ISLAND, PLAINS), CONVERSION: (MOUNTAIN, MOUNTAIN), HORROR: (SWAMP,) * 6,
    BRASS: (MOUNTAIN,),
}
AIR, OTHER_AIR, DEMON = 'Welkin Tern', 'Aven Fleetwing', 'Abyssal Persecutor'
THRULL, INDESTRUCTIBLE = 'Thrull', 'Indestructibility'
GAPS = {
    SPEAKER: ['The library selection, shuffle and final hand are observed. Reveal visibility is outside the current driver.'],
    BRASS: ['The optional payment is exercised where offered and affordable. The player-1 strategic decline is not scripted.'],
    CHROMIUM: ['The two-flyer Rampage case stops at the post-block priority boundary.'],
    LARVA: ['The resource-short control covers both seats. The discretionary decline is scripted only for seat 0.'],
}
HUMAN_ONLY = {(BRASS, 'decline'), (SPEAKER, 'decline'), (LARVA, 'decline')}


class Board:
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        self.csvid, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.life = [20, 20]
        self.library = [[], []]
        self.add("source", name, owner, "hand")

    def add(self, ref, name, seat, zone, *, opponent_owned=False):
        self.setup.append((ref, seat, ZONES[zone], self.rows[name]["csvid"], 16 if opponent_owned else 0))
        self.objects[ref] = dict(name=name, player=seat, zone=zone)
        if zone == "library":
            self.library[seat].append(ref)

    def emit(self, op, ref="", other="", prop="", *, player=0, value=0, zone=0, index=0, color=0):
        assert op in OPS
        self.steps.append((op, ref, other, prop, player, value, zone, index, color))

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

    def receipts(self, pools=(ZERO, ZERO), *, battlefield_ids=None):
        for seat in (0, 1):
            self.emit("assert_life", player=seat, value=self.life[seat])
            self.mana("assert_mana", seat, pools[seat])
            objects = [obj for obj in self.objects.values() if obj["player"] == seat]
            counts = Counter((obj["name"], obj["zone"]) for obj in objects)
            for name in sorted({obj["name"] for obj in objects}):
                for zone, code in ZONES.items():
                    identity = name
                    if battlefield_ids and zone == 'battlefield':
                        identity = battlefield_ids.get((seat, name), name)
                    self.emit("assert_zone_multiplicity", player=seat, zone=code,
                              index=self.rows[identity]["csvid"], value=counts[name, zone])

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

    def state(self, owner_pool=ZERO, *, battlefield_ids=None):
        self.receipts(self.pools(owner_pool), battlefield_ids=battlefield_ids)
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






    def decks(self):
        self.deck(self.owner, (FOREST, ISLAND, PLAINS))
        self.deck(self.foe, (MOUNTAIN, SWAMP))

    def prepare_upkeep(self, player):
        self.emit("advance_turn")
        self.emit("untap_step", player=player)

    def upkeep(self, player):
        self.emit("upkeep_step", player=player)
        self.resolve()

    def start_paid_source(self):
        self.begin_source()
        self.complete_source()
        self.state()

    def check_other_upkeep(self):
        self.prepare_upkeep(self.foe)
        self.upkeep(self.foe)
        self.state()

    def case_spell(self, ref, player, *, target=None):
        self.pending_cast(ref, player=player, target=target)
        self.resolve()
        self.move(ref, "graveyard")

    def rift(self):
        creature = self.mode != "noncreature"
        payer = self.owner if self.mode == "controller_pays" else self.foe if self.mode == "opponent_pays" else None
        self.decks()
        self.add("discard", BEAR if creature else BOOK, self.owner, "hand")
        self.add("unrelated_hand", GIANT, self.foe, "hand")
        if payer is not None:
            self.life[1 - payer] = 4
            self.emit("set_life", player=1 - payer, value=4)
        self.start_paid_source()
        self.check_other_upkeep()
        self.prepare_upkeep(self.owner)
        if creature:
            for player in (self.owner, self.foe):
                if self.life[player] >= 5:
                    self.emit("choose_dialog", player=player, value=int(player == payer))
        self.upkeep(self.owner)
        self.move("discard", "battlefield" if creature and payer is None else "graveyard")
        if payer is not None:
            self.life[payer] -= 5
        self.state()
        if creature and payer is None:
            self.emit("bind_permanent_copy", ref="returned", player=self.owner, value=self.rows[BEAR]["csvid"])
            for prop, value in (("in_play", 1), ("power", 2), ("toughness", 2)):
                self.observe("returned", prop, value)

    def akuta(self):
        returns = self.mode == "return_haste"
        equal = self.mode == "equal_hand"
        no_swamp = self.mode == "no_swamp"
        decline = self.mode == "decline"
        self.decks()
        self.add("kill", BOLT, self.foe, "hand")
        self.add("foe_card", BOOK, self.foe, "hand")
        self.add("own_first", BOOK, self.owner, "hand")
        if not equal:
            self.add("own_second", BOOK, self.owner, "hand")
        self.add("land_decoy", FOREST, self.owner, "battlefield")
        if not no_swamp:
            self.add("swamp_one", SWAMP, self.owner, "battlefield")
            self.add("swamp_two", SWAMP, self.owner, "battlefield")
        self.start_paid_source()
        self.case_spell("kill", self.foe, target="source")
        self.move("source", "graveyard")
        self.state()
        self.check_other_upkeep()
        self.prepare_upkeep(self.owner)
        if returns or decline:
            self.emit("choose_dialog", player=self.owner, value=int(decline))
            if returns and self.owner == 0:
                self.emit("choose_target", player=0, ref="swamp_one")
        self.upkeep(self.owner)
        if returns:
            self.move("swamp_one", "graveyard")
            self.move("source", "battlefield")
        self.state()
        if not returns:
            return
        self.emit("bind_permanent_copy", ref="returned", player=self.owner, value=self.csvid)
        self.observe("returned", "in_play", 1)
        self.observe("returned", "power", 3)
        self.observe("returned", "toughness", 2)
        self.emit("turn", player=self.owner)
        self.emit("begin_attackers")
        self.emit("attack", ref="returned", value=1)
        self.observe("returned", "attacking", 1)
        self.declaration_end()
        self.emit("begin_blockers")
        self.emit("advance_combat")
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")
        self.life[self.foe] -= 3
        self.state()

    def anowon(self):
        both = self.mode == "both_sacrifice"
        one = self.mode == "one_ineligible"
        self.decks()
        for player in (0, 1):
            self.add(f"vampire_{player}", VAMPIRE, player, "battlefield")
        candidates = (0, 1) if both else (self.foe,) if one else ()
        for player in candidates:
            paid = GIANT if player == 0 and both else BEAR
            self.add(f"victim_{player}", paid, player, "battlefield")
            if player == 0 and both:
                self.add("unselected", BEAR, player, "battlefield")
        self.start_paid_source()
        self.check_other_upkeep()
        self.prepare_upkeep(self.owner)
        if 0 in candidates:
            self.emit("choose_target", player=0, ref="victim_0")
        self.upkeep(self.owner)
        for player in candidates:
            self.move(f"victim_{player}", "graveyard")
        self.state()

    def anurid(self):
        pay = self.mode == "pay_protection"
        empty = self.mode == "empty_graveyard"
        self.decks()
        if not empty:
            self.add("payment", BOLT, self.owner, "graveyard")
            self.add("spared_grave", BOOK, self.owner, "graveyard")
        self.add("enemy_grave", GIANT, self.foe, "graveyard")
        if pay:
            self.add("black_source", MURDER, self.foe, "hand")
            self.add("black_blocker", BLACK, self.foe, "battlefield")
            self.add("ordinary_target", BEAR, self.owner, "battlefield")
        self.start_paid_source()
        self.check_other_upkeep()
        self.prepare_upkeep(self.owner)
        if not empty:
            self.emit("choose_zone_card" if pay else "choose_zone", player=self.owner,
                      value=self.rows[BOLT]["csvid"] if pay else -1)
        self.upkeep(self.owner)
        if pay:
            self.move("payment", "library")
            self.library[self.owner].append("payment")
        else:
            self.move("source", "graveyard")
        self.state()
        if not pay:
            return
        self.emit("can_target_creature", ref="black_source", other="source")
        self.emit("can_target_creature", ref="black_source", other="ordinary_target", value=1)
        self.emit("turn", player=self.owner)
        self.emit("begin_attackers")
        self.emit("attack", ref="source", value=1)
        self.declaration_end()
        self.complete_combat(attacker="source", blocker="black_blocker", legal=False)
        self.state()

    def run(self):
        {RIFT: self.rift, AKUTA: self.akuta, ANOWON: self.anowon, ANURID: self.anurid}[self.csvid]()


class UpkeepBoard(Board):
    def inspect_source(self):
        if self.csvid == ARCADES:
            self.observe('source', 'in_play', 1)
            self.stats('source', 7, 9)
        else:
            super().inspect_source()

    def stats(self, ref, power, toughness):
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def add_fee_lands(self, *, short=False):
        # Check the payable upkeep independently of the changed Mountains' mana ability.
        lands = (PLAINS, PLAINS) if self.csvid == CONVERSION and self.mode == 'paid' else FEES[self.csvid]
        if short:
            lands = lands[:-1]
        refs = [f'fee_{i}' for i in range(len(lands))]
        for ref, name in zip(refs, lands):
            self.add(ref, name, self.owner, 'battlefield')
        return refs

    def pay_lands(self, lands):
        if self.owner == 0:
            for ref in lands:
                self.emit('choose_mana_source', ref=ref)

    def upkeep_fee(self, lands, *, short=False, decline=False, sacrifice=True):
        self.prepare_upkeep(self.owner)
        for ref in lands:
            self.observe(ref, 'tapped', 0)
        if not short:
            self.emit('choose_dialog', player=self.owner, value=int(decline))
        if not short and not decline:
            self.pay_lands(lands)
        self.upkeep(self.owner)
        for ref in lands:
            self.observe(ref, 'tapped', int(not short and not decline))
        if (short or decline) and sacrifice:
            self.move('source', 'graveyard')
        self.observe('source', 'in_play', int(self.objects['source']['zone'] == 'battlefield'))

    def own_attack(self, ref='source'):
        self.emit('turn', player=self.owner)
        self.emit('begin_attackers')
        self.emit('attack', ref=ref, value=1)
        self.declaration_end()

    def dragon_or_horror(self):
        short, decline = self.mode == 'short', self.mode == 'decline'
        exception = self.mode == 'indestructible'
        lands = self.add_fee_lands(short=short or exception)
        if self.mode in ('team_pump', 'first_strike'):
            self.add('ground', BEAR, self.foe, 'battlefield')
        if self.mode == 'team_pump':
            self.add('ready', BEAR, self.owner, 'battlefield')
            self.add('attack_friend', BEAR, self.owner, 'battlefield')
        if self.mode == 'rampage':
            self.add('air_one', AIR, self.foe, 'battlefield')
            self.add('air_two', OTHER_AIR, self.foe, 'battlefield')
        if exception:
            self.add('shield', INDESTRUCTIBLE, self.owner, 'hand')
        self.start_paid_source()
        if exception:
            self.case_spell('shield', self.owner, target='source')
            self.move('shield', 'battlefield')
        if self.mode == 'team_pump':
            self.stats('ready', 2, 4)
            self.stats('ground', 2, 2)
        self.check_other_upkeep()
        self.upkeep_fee(lands, short=short or exception, decline=decline, sacrifice=not exception)
        if short and self.csvid == HORROR:
            self.life[self.owner] -= 7
        if self.mode == 'team_pump':
            self.emit('turn', player=self.owner)
            self.fund(self.owner, (0, 0, 0, 0, 0, 1))
            self.emit('can_activate', ref='source', value=1)
            self.emit('activate', ref='source', value=1)
            self.resolve()
            self.mana('assert_mana', self.owner)
            self.stats('source', 7, 10)
            self.emit('begin_attackers')
            for ref in ('source', 'attack_friend'):
                self.emit('attack', ref=ref, value=1)
            self.declaration_end()
            self.stats('source', 7, 8)
            self.stats('attack_friend', 2, 2)
            self.stats('ready', 2, 4)
            self.emit('begin_blockers')
            self.emit('block', ref='ground', other='source', value=0)
        elif self.mode == 'rampage':
            self.own_attack()
            self.emit('begin_blockers')
            for ref in ('air_one', 'air_two'):
                self.emit('block', ref=ref, other='source', value=1)
            self.emit('advance_combat')
            self.resolve()
            self.stats('source', 9, 9)
            for ref in ('air_one', 'air_two'):
                self.observe(ref, 'blocking_count', 1)
        elif self.mode == 'first_strike':
            self.own_attack()
            self.emit('begin_blockers')
            self.emit('block', ref='ground', other='source', value=1)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.resolve()
            self.emit('end_combat')
            self.observe('source', 'damage', 0)
            self.move('ground', 'graveyard')
        self.state()

    def graveyard_fee(self):
        if self.mode == 'discard':
            self.emit('turn', player=self.foe)
            self.fund(self.owner, ZERO)
            self.fund(self.foe, ZERO)
            self.emit('require_can_activate_hand', ref='source', value=1)
            self.emit('choose_dialog', player=self.owner, value=1)
            self.emit('activate_hand', ref='source', value=1)
            self.resolve()
            self.move('source', 'graveyard')
            self.state()
            return
        pay = self.mode == 'top_creature'
        empty = self.mode == 'no_creature'
        if not empty:
            self.add('lower_creature', GIANT, self.owner, 'graveyard')
            self.add('fee', BEAR, self.owner, 'graveyard')
        self.add('newer_noncreature', BOLT, self.owner, 'graveyard')
        self.add('foe_grave', VAMPIRE, self.foe, 'graveyard')
        if pay and self.csvid == VULTURES:
            self.add('ground', BEAR, self.foe, 'battlefield')
        self.start_paid_source()
        self.check_other_upkeep()
        self.prepare_upkeep(self.owner)
        if not empty:
            self.emit('choose_dialog', player=self.owner, value=int(not pay))
        self.upkeep(self.owner)
        self.move('fee' if pay else 'source', 'exile' if pay else 'graveyard')
        if pay and self.csvid == VULTURES:
            self.own_attack()
            self.complete_combat(attacker='source', blocker='ground')
        self.state()

    def grasp(self):
        short, decline = self.mode == 'short', self.mode == 'decline'
        lands = self.add_fee_lands(short=short)
        self.add('host', BEAR, self.foe, 'battlefield')
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.pending_cast('source', player=self.owner, target='host')
        self.complete_source()
        self.objects['host']['player'] = self.owner
        self.state()
        self.emit('bind_permanent_copy', ref='controlled', player=self.owner, value=self.rows[BEAR]['csvid'])
        self.stats('controlled', 2, 3)
        self.check_other_upkeep()
        self.upkeep_fee(lands, short=short, decline=decline)
        if short or decline:
            self.objects['host']['player'] = self.foe
            self.emit('bind_permanent_copy', ref='released', player=self.foe, value=self.rows[BEAR]['csvid'])
            self.stats('released', 2, 2)
        else:
            self.stats('controlled', 2, 3)
        self.state()

    def speaker(self):
        pay, match = self.mode != 'decline', self.mode != 'no_demon'
        self.deck(self.owner, (DEMON, FOREST) if match else (GIANT, GIANT))
        if pay and match:
            self.add('foe_demon', DEMON, self.foe, 'hand')
        self.start_paid_source()
        self.check_other_upkeep()
        self.prepare_upkeep(self.owner)
        if pay:
            if self.owner == 0:
                self.emit('choose_trigger_source', ref='source', player=0)
            self.emit('choose_zone_card' if match else 'choose_zone', player=self.owner,
                      value=self.rows[DEMON]['csvid'] if match else -1)
        self.upkeep(self.owner)
        self.emit('assert_shuffle_count', player=self.owner, value=int(pay))
        self.emit('assert_shuffle_count', player=self.foe, value=0)
        if pay:
            self.move('source', 'graveyard')
            if match:
                self.move('library_' + str(self.owner) + '_0', 'hand')
        self.state()
        if self.mode != 'search_return':
            return
        self.emit('turn', player=self.foe)
        self.pending_cast('foe_demon', player=self.foe)
        self.resolve()
        self.move('foe_demon', 'battlefield')
        self.state()
        self.emit('turn', player=self.owner)
        demon = 'library_' + str(self.owner) + '_0'
        self.emit('bind_hand_copy', ref='demon', player=self.owner, value=self.rows[DEMON]['csvid'], index=0)
        self.objects['demon'] = self.objects.pop(demon)
        self.pending_cast('demon', player=self.owner)
        self.resolve()
        self.move('demon', 'battlefield')
        self.move('source', 'hand')
        self.state()

    def pointed_sacrifice(self):
        if self.csvid == BRAIDS:
            chosen = {'artifact': BOOK, 'land': FOREST, 'creature': BEAR, 'ineligible': None}[self.mode]
            for player in (0, 1):
                self.add(f'aura_{player}', 'Fervor', player, 'battlefield')
        else:
            chosen = BEAR if self.mode in ('both_pay', 'empty_end') else None
            if self.mode != 'empty_end':
                for player in (0, 1):
                    self.add(f'zombie_{player}', BLACK, player, 'battlefield')
        victims = {}
        if chosen:
            # With two distinct legal permanents the automated chooser can sacrifice Braids.
            # Human artifact/creature choice stays independent of her own forced sacrifice.
            forced_self = self.csvid == BRAIDS and self.owner == 1 and self.mode in ('artifact', 'creature')
            for player in ((0,) if forced_self else (0, 1)):
                victims[player] = f'victim_{player}'
                self.add(victims[player], chosen, player, 'battlefield')
        self.start_paid_source()
        for player in (self.foe, self.owner):
            self.prepare_upkeep(player)
            can_sacrifice_source = self.csvid == BRAIDS and player == self.owner
            if player == 0 and (player in victims or can_sacrifice_source):
                self.emit('choose_target', ref=victims.get(player, 'source'), player=player)
            self.upkeep(player)
            if player in victims:
                self.move(victims[player], 'graveyard')
            elif can_sacrifice_source:
                self.move('source', 'graveyard')
            self.state()
        if self.csvid == CALL:
            self.emit('advance_turn')
            if self.mode == 'empty_end':
                self.move('source', 'graveyard')
            self.state()

    def brass(self):
        ready, short, decline = self.mode == 'ready', self.mode == 'short', self.mode == 'decline'
        lands = self.add_fee_lands(short=short)
        self.start_paid_source()
        if not ready:
            self.emit('advance_turn')
            self.emit('advance_turn')
            self.own_attack()
            self.complete_combat(attacker='source')
            self.observe('source', 'tapped', 1)
        self.check_other_upkeep()
        self.prepare_upkeep(self.owner)
        self.observe('source', 'tapped', int(not ready))
        if not ready and not short and not decline:
            if self.owner == 0:
                self.emit('choose_trigger_source', ref='source', player=self.owner)
            self.pay_lands(lands)
        self.upkeep(self.owner)
        self.observe('source', 'tapped', int(short or decline))
        for ref in lands:
            self.observe(ref, 'tapped', int(not ready and not decline))
        self.state()

    def pit_or_conversion(self):
        short, decline = self.mode == 'short', self.mode == 'decline'
        lands = self.add_fee_lands(short=short)
        if self.csvid == CONVERSION:
            self.add('enemy_first', MOUNTAIN, self.foe, 'battlefield')
            self.add('enemy_second', MOUNTAIN, self.foe, 'battlefield')
        self.start_paid_source()
        self.check_other_upkeep()
        if self.csvid == CONVERSION:
            self.mountain_mana('enemy_first', white=True)
        else:
            self.thrulls(1)
        self.upkeep_fee(lands, short=short, decline=decline)
        if self.csvid == CONVERSION:
            self.mountain_mana('enemy_second', white=not short and not decline)
        else:
            self.emit('advance_turn')
            self.thrulls(1 + int(not short and not decline))
        # Battlefield enumeration reads the current card-data ID during Conversion.
        identities = {(self.foe, MOUNTAIN): PLAINS} if self.csvid == CONVERSION and self.mode == 'paid' else None
        self.state(battlefield_ids=identities)

    def thrulls(self, expected):
        csvid = self.rows[THRULL]['csvid']
        for player, count in ((self.owner, expected), (self.foe, 0)):
            self.emit('assert_zone_multiplicity', player=player, zone=ZONES['battlefield'], index=csvid, value=count)
        for prop, value in (('token', 1), ('color', 2), ('power', 0), ('toughness', 1)):
            self.emit('assert_matching_permanents', player=self.owner, prop=prop, index=csvid, value=value)

    def mountain_mana(self, ref, *, white):
        self.emit('activate', ref=ref, value=1)
        self.mana('assert_mana', self.foe, (0, 0, 0, 0, int(not white), int(white)))
        self.mana('set_mana', self.foe)
        self.emit('refresh')

    def brink(self):
        empty = self.mode == 'empty'
        if not empty:
            self.add('own_hand', BOOK, self.owner, 'hand')
        for ref, name in (('foe_1', BOLT), ('foe_2', BEAR)):
            self.add(ref, name, self.foe, 'hand')
        self.start_paid_source()
        self.check_other_upkeep()
        self.prepare_upkeep(self.owner)
        if empty and self.foe == 0:
            for ref in ('foe_1', 'foe_2'):
                self.emit('choose_zone_card', player=self.foe, value=self.rows[self.objects[ref]['name']]['csvid'])
        self.upkeep(self.owner)
        if empty:
            for ref in ('source', 'foe_1', 'foe_2'):
                self.move(ref, 'graveyard')
        self.state()

    def larva(self):
        short, decline = self.mode == 'short', self.mode == 'decline'
        pay = not short and not decline
        self.add('one', FOREST, self.owner, 'battlefield')
        if not short:
            self.add('two', FOREST, self.owner, 'battlefield')
            self.add('spare', FOREST, self.owner, 'battlefield')
        self.add('foe_land', MOUNTAIN, self.foe, 'battlefield')
        if self.mode == 'trample':
            self.add('blocker', BEAR, self.foe, 'battlefield')
        self.start_paid_source()
        self.check_other_upkeep()
        self.prepare_upkeep(self.owner)
        if not short:
            self.emit('choose_dialog', player=self.owner, value=int(decline))
            if pay and self.owner == 0:
                for ref in ('one', 'two'):
                    self.emit('choose_target', ref=ref, player=self.owner)
        self.upkeep(self.owner)
        if pay:
            self.move('one', 'graveyard')
            self.move('two', 'graveyard')
        else:
            self.move('source', 'graveyard')
        if self.mode == 'trample':
            self.own_attack()
            self.complete_combat(attacker='source', blocker='blocker', legal=True)
            self.life[self.foe] -= 5
        self.state()

    def run(self):
        self.emit('present_ai_dialogs', value=1)
        {ARCADES: self.dragon_or_horror, CHROMIUM: self.dragon_or_horror, HORROR: self.dragon_or_horror,
         GHOUL: self.graveyard_fee, VULTURES: self.graveyard_fee, GRASP: self.grasp, SPEAKER: self.speaker,
         BRAIDS: self.pointed_sacrifice, CALL: self.pointed_sacrifice, BRASS: self.brass,
         PIT: self.pit_or_conversion, CONVERSION: self.pit_or_conversion, BRINK: self.brink, LARVA: self.larva}[self.csvid]()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name', ''))
    if (not pin or pin['registry']['csvid'] != csvid or csvid not in SOURCES
            or not exact(pin['oracle'], record) or record.get('layout') != 'normal' or record.get('card_faces')):
        raise ValueError('complete pinned upkeep rules required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'], keywords=list(record['keywords']))


def variants(shape):
    csvid = shape['csvid']
    modes = MODES[csvid] if csvid in MODES else FOLLOWERS[csvid]
    return [f'{mode}_{seat}' for mode in modes for seat in (0, 1)
            if seat == 0 or (csvid, mode) not in SINGLE | HUMAN_ONLY]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct upkeep rules variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name', '')) if record else None
    if not pin:
        raise ValueError('complete pinned upkeep rules required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 upkeep rules recipe required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(SOURCES) != {entry['registry']['csvid'] for entry in pins['sources'].values()}
            or not pins['witnesses']):
        raise ValueError('complete upkeep rules source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') not in ('card', 'token') or oracle[name].get('card_faces')):
            raise ValueError('pinned upkeep rules card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])


def trial(csvid, name, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    builder = Board if csvid in MODES else UpkeepBoard
    board = builder(csvid, name, mode, int(seat), rows, oracle)
    board.run()
    return board


def setup_record(row, names):
    ref, player, zone, csvid, state = row
    entry = dict(ref=ref, player=player, zone=names[zone], name=csvid)
    if state:
        assert state == 16
        entry['state'] = ['opponent_owned']
    return entry


def action(row, names):
    op, ref, other, prop, player, value, zone, index, color = row
    entry = dict(op=op)
    if ref:
        entry['ref'] = ref
    if other:
        entry['other'] = other
    if prop:
        entry['property'] = prop
    if op in {'untap_step', 'upkeep_step', 'draw_step'}:
        return dict(op=op, player=player)
    if op == 'choose_mana_source':
        return dict(op=op, ref=ref)
    if op == 'choose_trigger_source':
        return dict(op=op, ref=ref, player=player)
    if op == 'assert_shuffle_count':
        return dict(op=op, player=player, value=value)
    entry.update(player=player, value=value, zone=names[zone], index=index, color=color)
    return entry


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if classify(oracle[name], csvid) != {key: shape[key] for key in ('csvid', 'printed_cost', 'keywords')}:
        raise ValueError('exact registered upkeep rules card required')
    zones = {code: name for name, code in ZONES.items()}
    name_by_id = {row['csvid']: name for name, row in rows.items()}
    cases = []
    for variant in selected:
        board = trial(csvid, name, variant, rows, oracle)
        setup = [setup_record(row, zones) for row in board.setup]
        for entry in setup:
            entry['name'] = name_by_id[entry['name']]
        recipe = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                      oracle_text=declaration['oracle_text'])
        if csvid in GAPS:
            recipe['untested_components'] = GAPS[csvid]
        cases.append(dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=setup, steps=[action(row, zones) for row in board.steps], _recipe=recipe))
    return cases
