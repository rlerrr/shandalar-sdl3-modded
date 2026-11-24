"""Paid upkeep life changes, mana payments, and printed linked effects."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import cost_from_oracle, step


FAMILY = 'upkeep_damage_and_life'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
SEED = 3686
PREFIX = 'UpkeepDamageAndLifeRecipe'
ZONES = {'hand':0,'battlefield':1,'library':2,'graveyard':3,'exile':4}
ZERO = (0,)*6

DJINN, GOBLET, CONVALESCENCE, CONFIDANT = 2409, 7027, 3472, 7811
TUTELAGE, SANCTUARY, VERMIN, SEER, EMBARGO, ENSLAVE, FELIDAR = 11112, 5353, 2272, 12934, 4458, 8774, 10540
FESTERING, FIREMANE, FORM, WASTES, GERRARD, FEAST, HAVOC = 2972, 7841, 6531, 2486, 5374, 2498, 12744
PILOTS = {DJINN,GOBLET,CONVALESCENCE,CONFIDANT}
MODES = {
    DJINN: ('repeat', 'islandwalk', 'ordinary_block', 'removed'),
    GOBLET: ('two_colors', 'one_color', 'colorless', 'decline', 'removed'),
    CONVALESCENCE: ('nine', 'ten', 'eleven', 'cross_threshold'),
    CONFIDANT: ('high', 'land', 'successive', 'empty', 'removed'),
    TUTELAGE: ('high', 'land', 'successive'),
    SANCTUARY: ('black', 'red', 'both', 'dual', 'foreign_only'),
    VERMIN: ('uninfected', 'one_hit', 'two_hits'),
    SEER: ('both_high', 'zero_and_high', 'flying', 'empty_owner'),
    EMBARGO: ('restriction', 'removed', 'repeat'),
    ENSLAVE: ('foreign_owner', 'self_owner', 'removed'),
    FELIDAR: ('forty', 'thirty_nine', 'combat_then_win'),
    FESTERING: ('upkeep', 'sacrifice', 'short'),
    FIREMANE: ('battlefield', 'grave_gain', 'grave_return', 'first_strike', 'decline'),
    FORM: ('player', 'creature', 'flying_gate'),
    WASTES: ('two_upkeeps', 'deny_gain', 'targeted'),
    GERRARD: ('two_cards', 'empty_hand', 'tap_attack'),
    FEAST: ('opposing_death', 'own_death', 'bounce'),
    HAVOC: ('odd_even', 'deny_gain', 'repeat'),
}
SINGLE = {(GOBLET,'decline'), (FIREMANE,'grave_return'), (FIREMANE,'decline')}


class Board:
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        self.csvid, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.life = [20, 20]
        self.library = [[], []]
        self.used = {name}
        self.add("source", name, owner, "hand")

    def add(self, ref, name, seat, zone, *, opponent_owned=False, tapped=False):
        self.used.add(name)
        self.setup.append((ref, seat, ZONES[zone], self.rows[name]["csvid"], (16 if opponent_owned else 0) | (1 if tapped else 0)))
        self.objects[ref] = dict(name=name, player=seat, zone=zone)
        if zone == "library":
            self.library[seat].append(ref)

    def emit(self, op, ref="", other="", prop="", *, player=0, value=0, zone=0, index=0, color=0):
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

    def resolve(self, *, receipt="assert_choices_consumed"):
        self.emit("resolve")
        self.emit("refresh")
        self.emit(receipt)
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





    def prepare_upkeep(self, player):
        self.emit("advance_turn")
        self.emit("untap_step", player=player)

    def upkeep(self, player, *, receipt="assert_choices_consumed"):
        self.emit("upkeep_step", player=player)
        self.resolve(receipt=receipt)

    def cast_for(self, fee):
        assert sum(fee) == 5
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, fee)
        if self.owner == 0:
            for color in range(1,6):
                for _ in range(fee[color]):
                    self.emit('choose_mana', player=0, color=color)
        self.emit('cast', ref='source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.owner)
        self.complete_source()

    def remove_source(self, name):
        self.add('exit', name, self.owner, 'hand')
        self.pending_cast('exit', player=self.owner, target='source')
        self.resolve()
        self.move('exit', 'graveyard')
        self.move('source', 'graveyard')

    def observe_upkeep(self, seat):
        self.prepare_upkeep(seat)
        self.upkeep(seat)

    def djinn(self):
        attacking = self.mode in ('islandwalk', 'ordinary_block')
        if attacking:
            self.add('blocker', 'Goblin Tinkerer', self.foe, 'battlefield')
            land = 'Island' if self.mode == 'islandwalk' else 'Forest'
            self.add('land', land, self.foe, 'battlefield')
        self.cast_source()
        if self.mode == 'removed':
            self.remove_source('Murder')
        cycles = 2 if self.mode == 'repeat' else 1
        for _ in range(cycles):
            self.observe_upkeep(self.foe)
            self.state()
            self.observe_upkeep(self.owner)
            if self.mode != 'removed':
                self.life[self.owner] -= 2
            self.state()
        if attacking:
            self.emit('turn', player=self.owner)
            self.emit('begin_attackers')
            self.emit('attack', ref='source', value=1)
            self.observe('source', 'attacking', 1)
            self.declaration_end()
            self.complete_combat(attacker='source', blocker='blocker',
                                 legal=self.mode == 'ordinary_block')
            self.state()

    def charge(self, value):
        self.emit('assert_counter', ref='source', prop='charge', value=value)

    def goblet(self):
        fees = {
            'two_colors': (1,2,0,2,0,0),
            'one_color': (0,0,0,0,5,0),
            'colorless': (5,0,0,0,0,0),
        }
        fee = fees.get(self.mode, fees['two_colors'])
        charge = sum(bool(amount) for amount in fee[1:])
        self.cast_for(fee)
        self.charge(charge)
        if self.mode == 'removed':
            self.remove_source('Disenchant')
        cycles = 2 if self.mode == 'two_colors' else 1
        for _ in range(cycles):
            self.observe_upkeep(self.foe)
            self.state()
            self.prepare_upkeep(self.owner)
            accepts = self.mode not in ('decline', 'removed')
            if accepts and self.owner == 0:
                self.emit('choose_trigger_source', ref='source', player=self.owner)
            self.upkeep(self.owner)
            if accepts:
                self.life[self.owner] += charge
            if self.mode != 'removed':
                self.charge(charge)
            self.state()

    def convalescence(self):
        start = {'nine':9, 'ten':10, 'eleven':11, 'cross_threshold':9}[self.mode]
        self.life[self.owner] = start
        self.life[self.foe] = 8
        self.emit('set_life', player=self.owner, value=start)
        self.emit('set_life', player=self.foe, value=8)
        self.cast_source()
        cycles = 3 if self.mode == 'cross_threshold' else 1
        for _ in range(cycles):
            self.observe_upkeep(self.foe)
            self.state()
            self.observe_upkeep(self.owner)
            if self.life[self.owner] <= 10:
                self.life[self.owner] += 1
            self.state()

    def confidant(self):
        library = {
            'high': ('Serra Angel', 'Island'),
            'land': ('Forest', 'Serra Angel'),
            'successive': ('Shock', 'Plains', 'Serra Angel', 'Mountain'),
            'empty': (),
            'removed': ('Serra Angel', 'Forest'),
        }[self.mode]
        self.deck(self.owner, library)
        self.deck(self.foe, ('Swamp', 'Forest'))
        self.cast_source()
        if self.mode == 'removed':
            self.remove_source('Murder')
        for _ in range(3 if self.mode == 'successive' else 1):
            self.observe_upkeep(self.foe)
            self.state()
            self.prepare_upkeep(self.owner)
            active = bool(self.library[self.owner]) and self.mode != 'removed'
            if active and self.owner == 0:
                self.emit('choose_dialog', player=self.owner, value=0)
            self.upkeep(self.owner)
            if active:
                revealed = self.library[self.owner][0]
                record = self.oracle[self.objects[revealed]['name']]
                fee = sum(cost_from_oracle(record)) if record['mana_cost'] else 0
                self.life[self.owner] -= fee
                self.move(revealed, 'hand')
            self.state()


    def cast_permanent_target(self, target):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.pending_cast('source', player=self.owner, target=target)
        self.resolve()
        self.move('source', 'battlefield')
        self.inspect_source()

    def upkeep_pair(self, update=None):
        self.observe_upkeep(self.foe)
        self.state()
        self.observe_upkeep(self.owner)
        if update:
            update()
        self.state()

    def choose_player(self, chooser, target):
        self.emit('choose_player', player=chooser, value=target)

    def zone_cast(self, ref, player, *, target=None, recipient=None, announce=False):
        if recipient is not None:
            self.choose_player(player, recipient)
        self.pending_cast(ref, player=player, target=target)
        if announce:
            self.emit('announce_spell', ref=ref)
        self.resolve()
        self.move(ref, 'graveyard')

    def lose_life(self, player, amount):
        self.life[player] -= amount

    def gain_life(self, player, amount):
        self.life[player] += amount

    def reveal_top(self, player):
        assert self.library[player]
        selected = self.library[player][0]
        record = self.oracle[self.objects[selected]['name']]
        mana_value = sum(cost_from_oracle(record)) if record['mana_cost'] else 0
        self.lose_life(player, mana_value)
        self.move(selected, 'hand')

    def seer(self):
        first = () if self.mode == 'empty_owner' else ('Forest','Shock') if self.mode=='zero_and_high' else ('Serra Angel','Mountain')
        self.deck(self.owner, first)
        self.deck(self.foe, ('Goblin Tinkerer','Swamp'))
        if self.mode == 'flying':
            self.add('blocker','Goblin Tinkerer',self.foe,'battlefield')
        self.cast_source()
        self.observe_upkeep(self.foe)
        self.state()
        self.prepare_upkeep(self.owner)
        seats = [p for p in (self.owner,self.foe) if self.library[p]]
        if self.owner==0:
            for player in seats:
                self.emit('choose_dialog',player=0,value=0)
        self.upkeep(self.owner)
        for player in seats:
            self.reveal_top(player)
        self.state()
        if self.mode=='flying':
            self.emit('turn',player=self.owner)
            self.attack_one('source', blocker='blocker', legal=False)
            self.state()

    def sanctuary(self):
        witnesses={'black':('Vampire Nighthawk',), 'red':('Goblin Tinkerer',),
                   'both':('Vampire Nighthawk','Goblin Tinkerer'), 'dual':('Rakdos Guildmage',),
                   'foreign_only':('Vampire Nighthawk','Goblin Tinkerer')}
        names=witnesses[self.mode]
        seat=self.foe if self.mode=='foreign_only' else self.owner
        for index,name in enumerate(names):
            self.add(f'color_{index}',name,seat,'battlefield')
        self.cast_source()
        colors=set().union(*(set(self.oracle[name]['colors']) for name in names)) if seat==self.owner else set()
        amount=2*len(colors & {'B','R'})
        self.upkeep_pair(lambda:self.gain_life(self.owner,amount))

    def attack_one(self, attacker, *, blocker=None, legal=False):
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=1)
        self.observe(attacker,'attacking',1)
        self.declaration_end()
        self.complete_combat(attacker=attacker,blocker=blocker,legal=legal)

    def vermin(self):
        self.cast_source()
        hits={'uninfected':0,'one_hit':1,'two_hits':2}[self.mode]
        self.upkeep_pair()
        for count in range(1,hits+1):
            self.emit('turn',player=self.owner)
            self.attack_one('source')
            self.state()
            self.upkeep_pair(lambda:self.lose_life(self.foe,count))

    def embargo(self):
        for seat in (self.owner,self.foe):
            self.add(f'artifact_{seat}',"Traveler's Amulet",seat,'battlefield',tapped=True)
            self.add(f'creature_{seat}','Goblin Tinkerer',seat,'battlefield',tapped=True)
            self.add(f'land_{seat}','Forest',seat,'battlefield',tapped=True)
        self.cast_source()
        removed=self.mode=='removed'
        if removed:
            self.remove_source('Disenchant')
        for _ in range(2 if self.mode=='repeat' else 1):
            for seat in (self.foe,self.owner):
                self.prepare_upkeep(seat)
                self.observe(f'land_{seat}','tapped',0)
                for ref in ('artifact','creature'):
                    self.observe(f'{ref}_{seat}','tapped',int(not removed))
                self.upkeep(seat)
                if seat==self.owner and not removed:
                    self.lose_life(seat,2)
                self.state()

    def enslave(self):
        owner=self.owner if self.mode=='self_owner' else self.foe
        self.add('victim','Goblin Tinkerer',owner,'battlefield')
        self.cast_permanent_target('victim')
        self.objects['victim']['player']=self.owner
        self.emit('bind_permanent',ref='controlled',player=self.owner,
                  value=self.rows['Goblin Tinkerer']['csvid'],index=-1)
        self.observe('controlled','power',1)
        self.state()
        removed=self.mode=='removed'
        if removed:
            self.remove_source('Disenchant')
            self.objects['victim']['player']=owner
            self.state()
        self.upkeep_pair(lambda:self.lose_life(owner,int(not removed)))

    def felidar(self):
        start={'forty':40,'thirty_nine':39,'combat_then_win':36}[self.mode]
        self.life[self.owner]=start
        self.emit('set_life',player=self.owner,value=start)
        self.cast_source()
        if self.mode=='combat_then_win':
            self.upkeep_pair()
            self.emit('turn',player=self.owner)
            self.emit('begin_attackers')
            self.emit('attack',ref='source',value=1)
            self.observe('source','tapped',0)
            self.declaration_end()
            self.complete_combat(attacker='source')
            self.gain_life(self.owner,4)
            self.state()
        self.observe_upkeep(self.foe)
        self.state()
        self.observe_upkeep(self.owner)
        if self.life[self.owner]>=40:
            self.emit('assert_life',player=self.foe,value=-99)
            self.emit('advance_scheduled_turn',value=-1)
        else:
            self.state()

    def festering(self):
        self.add('small','Prodigal Sorcerer',self.owner,'battlefield')
        self.add('medium','Goblin Tinkerer',self.foe,'battlefield')
        self.add('large','Serra Angel',self.foe,'battlefield')
        self.cast_source()
        if self.mode=='sacrifice':
            self.fund(self.owner,(0,2,0,0,0,0))
            self.emit('activate',ref='source',value=1)
            self.emit('assert_stack',value=1)
            self.mana('assert_mana',self.owner)
            self.move('source','graveyard')
            self.assert_zone_ref('source','graveyard',1)
            self.resolve()
            self.mass_damage(3)
        else:
            if self.mode=='short':
                poor=(0,1,0,0,0,0)
                self.fund(self.owner,poor)
                self.emit('can_activate',ref='source',value=0)
                self.state(poor)
                self.fund(self.owner,ZERO)
            self.upkeep_pair(lambda:self.mass_damage(1))
        self.state()

    def mass_damage(self, amount):
        for player in (self.owner,self.foe):
            self.lose_life(player,amount)
        for ref in ('small','medium','large'):
            record=self.oracle[self.objects[ref]['name']]
            if int(record['toughness'])<=amount:
                self.move(ref,'graveyard')
            else:
                self.observe(ref,'damage',amount)

    def firemane(self):
        grave=self.mode in ('grave_gain','grave_return')
        if grave:
            self.setup[0]=('source',self.owner,ZONES['graveyard'],self.rows[self.name]['csvid'],0)
            self.move('source','graveyard')
            self.emit('prepare_graveyard_abilities')
            self.emit('turn',player=self.owner)
            for seat in (self.owner,self.foe):
                self.fund(seat,ZERO)
            paid=self.mode=='grave_return'
            lands=('Mountain','Mountain','Plains','Plains',*('Wastes',)*6) if paid else ()
            for i,name in enumerate(lands):
                self.add(f'fee_{i}',name,self.owner,'battlefield',tapped=True)
            self.observe_upkeep(self.foe)
            self.state()
            self.prepare_upkeep(self.owner)
            if paid:
                self.emit('choose_dialog',player=self.owner,value=2)
                for i,_ in enumerate(lands):
                    self.emit('choose_mana_source',ref=f'fee_{i}',player=self.owner)
            self.upkeep(self.owner, receipt='assert_unused_choices' if paid else 'assert_choices_consumed')
            self.gain_life(self.owner,1)
            if paid:
                self.move('source','battlefield')
                for i,_ in enumerate(lands):
                    self.observe(f'fee_{i}','tapped',1)
            self.state()
            return
        first=self.mode=='first_strike'
        if first:
            self.add('air','Serra Angel',self.foe,'battlefield')
            self.add('ground','Goblin Tinkerer',self.foe,'battlefield')
        self.cast_source()
        self.observe_upkeep(self.foe)
        self.state()
        self.prepare_upkeep(self.owner)
        if self.owner==0 and self.mode!='decline':
            self.emit('choose_trigger_source',ref='source',player=0)
        self.upkeep(self.owner)
        if self.mode!='decline':
            self.gain_life(self.owner,1)
        self.state()
        if first:
            self.emit('turn',player=self.owner)
            self.emit('begin_attackers')
            self.emit('attack',ref='source',value=1)
            self.declaration_end()
            self.emit('begin_blockers')
            self.emit('block',ref='ground',other='source',value=0)
            self.emit('block',ref='air',other='source',value=1)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.resolve()
            self.emit('end_combat')
            self.move('air','graveyard')
            self.observe('source','in_play',1)
            self.observe('source','damage',0)
            self.state()

    def form(self):
        flight=self.mode=='flying_gate'
        if self.mode=='creature':
            self.add('victim','Serra Angel',self.foe,'battlefield')
        if flight:
            self.add('ground','Goblin Tinkerer',self.foe,'battlefield')
            self.add('flying','Serra Angel',self.foe,'battlefield')
        self.cast_source()
        self.observe_upkeep(self.foe)
        self.life[self.owner]=5
        self.state()
        if flight:
            self.emit('turn',player=self.foe)
            self.emit('begin_attackers')
            self.emit('attack',ref='ground',value=0)
            self.observe('ground','tapped',0)
            self.emit('attack',ref='flying',value=1)
            self.declaration_end()
            self.complete_combat(attacker='flying')
            self.state()
        self.prepare_upkeep(self.owner)
        self.life[self.owner]=5
        if self.mode=='creature':
            self.emit('choose_target',ref='victim',player=self.owner)
        else:
            self.choose_player(self.owner,self.foe)
        self.upkeep(self.owner)
        if self.mode=='creature':
            self.move('victim','graveyard')
        else:
            self.lose_life(self.foe,5)
        self.state()

    def prevent_gain(self, *, festival):
        if self.mode=='deny_gain':
            for seat in (self.owner,self.foe):
                self.add(f'gain_{seat}','Renewed Faith',seat,'hand')
        if self.mode=='targeted':
            self.add('targeting','Disenchant',self.foe,'hand')
        start={self.owner:21,self.foe:20}
        if festival:
            for player in (self.owner,self.foe):
                self.life[player]=start[player]
                self.emit('set_life',player=player,value=start[player])
        self.cast_source()
        for _ in range(2 if self.mode=='repeat' else 1):
            for player in (self.foe,self.owner):
                self.observe_upkeep(player)
                self.lose_life(player,(self.life[player]+1)//2 if festival else 1)
                self.state()
                if self.mode=='deny_gain':
                    self.zone_cast(f'gain_{player}',player)
                    self.state()
        if self.mode=='targeted':
            self.zone_cast('targeting',self.foe,target='source',announce=True)
            self.move('source','graveyard')
            self.lose_life(self.foe,5)
            self.state()

    def gerrard(self):
        count=0 if self.mode=='empty_hand' else 2
        for i,name in enumerate(('Forest','Island')[:count]):
            self.add(f'hand_{i}',name,self.foe,'hand')
        attack=self.mode=='tap_attack'
        if attack:
            self.add('guard','Goblin Tinkerer',self.foe,'battlefield')
        self.cast_source()
        self.upkeep_pair(lambda:self.gain_life(self.owner,count))
        if not attack:
            return
        self.emit('turn',player=self.owner)
        self.fund(self.owner,(3,0,0,0,0,1))
        self.emit('can_activate',ref='source',value=0)
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.declaration_end()
        self.emit('can_activate',ref='source',value=1)
        self.emit('choose_target',ref='guard',player=self.owner)
        self.emit('activate',ref='source',value=1)
        self.emit('assert_stack',value=1)
        self.mana('assert_mana',self.owner)
        self.resolve()
        self.observe('guard','tapped',1)
        self.complete_combat(attacker='source',blocker='guard',legal=False)
        self.state()

    def feast(self):
        player=self.owner if self.mode=='own_death' else self.foe
        victim='Goblin Tinkerer'
        bouncing=self.mode=='bounce'
        self.add('victim',victim,player,'battlefield')
        self.add('exit','Unsummon' if bouncing else 'Murder',self.owner,'hand')
        augmented=self.mode=='opposing_death'
        if augmented:
            self.add('growth','Giant Growth',self.owner,'hand')
        self.cast_source()
        self.upkeep_pair(lambda:self.lose_life(self.owner,1))
        self.emit('turn',player=self.owner)
        if augmented:
            self.zone_cast('growth',self.owner,target='victim')
            self.observe('victim','toughness',5)
        self.zone_cast('exit',self.owner,target='victim')
        self.move('victim','hand' if bouncing else 'graveyard')
        if augmented:
            self.gain_life(self.owner,5)
        self.state()

    def run(self):
        actions={
            DJINN:self.djinn,GOBLET:self.goblet,CONVALESCENCE:self.convalescence,
            CONFIDANT:self.confidant,TUTELAGE:self.confidant,SANCTUARY:self.sanctuary,
            VERMIN:self.vermin,SEER:self.seer,EMBARGO:self.embargo,ENSLAVE:self.enslave,
            FELIDAR:self.felidar,FESTERING:self.festering,FIREMANE:self.firemane,FORM:self.form,
            WASTES:lambda:self.prevent_gain(festival=False),GERRARD:self.gerrard,FEAST:self.feast,
            HAVOC:lambda:self.prevent_gain(festival=True),
        }
        actions[self.csvid]()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pinned=profiles()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pinned or not exact(pinned['oracle'],record)
            or pinned['registry']['csvid']!=csvid):
        raise ValueError('Pinned implemented upkeep record required')
    return {'csvid':csvid}


def variants(model):
    csvid=model['csvid']
    return [f'{mode}_{seat}' for mode in MODES[csvid]
            for seat in ((0,) if (csvid,mode) in SINGLE else (0,1))]


def validate_witnesses(oracle,rows):
    pinned=profiles()
    if (set(pinned)!={'sources','witnesses'}
            or {r['registry']['csvid'] for r in pinned['sources'].values()}!=MODES.keys()):
        raise ValueError('Exact upkeep profile scope required')
    for collection in pinned.values():
        for name,record in collection.items():
            row=rows.get(name,{})
            if (not exact(record,dict(oracle=oracle.get(name),registry=row))
                    or record['oracle']['layout']!='normal'
                    or row.get('kind')!='card' or row.get('handler') in (None,'0','NULL','card_dummy')):
                raise ValueError(f'Changed implemented upkeep source or witness: {name}')


def spec(declaration,record):
    pin=profiles()['sources'].get((record or {}).get('name'))
    if not pin:
        raise ValueError('Whole pinned upkeep primary required')
    model=classify(record,pin['registry']['csvid'])
    picked=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not exact(declaration.get('params'),model) or not isinstance(picked,list) or not picked
            or any(type(v) is not str for v in picked) or len(picked)!=len(set(picked))
            or set(picked)-set(variants(model))):
        raise ValueError('Exact upkeep recipe and distinct supported variants required')
    return dict(model,family=FAMILY)


def label(csvid,mode,seat):
    return f'{PREFIX}_{csvid}_{mode}_{seat}'


def operations(steps):
    zones={v:k for k,v in ZONES.items()}
    for op,ref,other,prop,player,value,zone,index,color in steps:
        if op in ('untap_step','upkeep_step'):
            yield step(op,player=player)
        elif op=='advance_scheduled_turn':
            yield step(op,value=value)
        elif op=='assert_counter':
            yield step(op,ref=ref,counter_kind=prop,value=value)
        elif op=='choose_mana_source':
            yield step(op,ref=ref)
        elif op=='choose_trigger_source':
            yield step(op,ref=ref,player=player)
        else:
            values=dict(ref=ref,other=other,property=prop,player=player,value=value,
                        zone=zones[zone],index=index,color=color)
            yield step(op,**{k:v for k,v in values.items() if v!=''})


def expand(name,csvid,declaration,model,selected,rows,oracle):
    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or model['csvid']!=csvid or set(selected)-set(variants(model)):
        raise ValueError('Exact upkeep identity and supported variants required')
    by_id={row['csvid']:key for key,row in rows.items()}
    zone_names={i:key for key,i in ZONES.items()}
    gaps={
        VERMIN:['Infection is observed by damage scaling after real hits. The named counter is not exposed directly.'],
        CONFIDANT:['The reveal acknowledgement is observable in the human seat. Public presentation is not independently asserted.'],
        TUTELAGE:['The reveal acknowledgement is observable in the human seat. Public presentation is not independently asserted.'],
        SEER:['The human acknowledges both reveals. Public presentation is not independently asserted.'],
        FIREMANE:['The exact paid graveyard return is exercised from the human payment interface.'],
    }
    output=[]
    for variant in selected:
        mode,seat=variant.rsplit('_',1)
        board=Board(csvid,name,mode,int(seat),rows,oracle)
        board.run()
        assert board.used-{name}<=profiles()['witnesses'].keys()
        setup=[]
        for ref,player,zone,entry,flags in board.setup:
            card=dict(name=by_id[entry],ref=ref,player=player,zone=zone_names[zone])
            if flags:
                assert flags==1
                card['state']=['tapped']
            setup.append(card)
        metadata=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                      oracle_text=declaration['oracle_text'],covered_clauses=declaration['oracle_text'].splitlines(),
                      untested_components=gaps.get(csvid,[]))
        output.append(dict(name=label(csvid,mode,int(seat)),card=name,fixture='empty_main',seed=SEED,
                           setup=setup,steps=list(operations(board.steps)),_recipe=metadata))
    return output
