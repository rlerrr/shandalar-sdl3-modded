"""Generate Oracle-pinned follow-on entry tokens, dynamic stats, and real combat."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'entry_follow_on_tokens'
PROFILE = Path(__file__).with_name('data') / 'entry_follow_on_tokens_wave8_profiles.json'

MONK, TWINGROVE, MASTER, SUMMONER = 11962, 14123, 13481, 13237
PREFIX, SEED = 'EntryFollowOnTokensRecipe', 3212
MODES = {
    MONK: ('base', 'scope', 'shrinks', 'blink'),
    TWINGROVE: ('grows', 'ownership', 'shrink', 'zero'),
    MASTER: ('base', 'devotion', 'lord_removed', 'red_block', 'red_sweep'),
    SUMMONER: ('combat', 'rhino_type', 'source_removed', 'reentry'),
}
HAND, BATTLE, LIBRARY, GRAVE, EXILE = range(5)
ZERO = (0,) * 6
SPIRIT, TREE, ELEMENTAL = 'Spirit', 'Treefolk Warrior', 'Elemental'
KNIGHT, CENTAUR, RHINO = 'Knight', 'Centaur', 'Rhino'
BEAR, CAD, RELIC = 'Grizzly Bears', 'Eager Cadet', 'Darksteel Relic'
FOREST, ISLAND, DUAL = 'Forest', 'Island', 'Tropical Island'
BLINK, MURDER, STONE = 'Cloudshift', 'Murder', 'Stone Rain'
ZULAPORT, WATER, FLYING_MEN = 'Zulaport Cutthroat', 'Water Elemental', 'Flying Men'
PIKER, PYRO, COAT, RHINO_WITNESS = 'Goblin Piker', 'Pyroclasm', 'Coat of Arms', 'Stampeding Rhino'
TOKENS = {SPIRIT:(18829,32,1,1,('Spirit',)), TREE:(18843,8,0,0,('Treefolk','Warrior')),
          ELEMENTAL:(18726,4,1,0,('Elemental',)), KNIGHT:(18776,32,2,2,('Knight',)),
          CENTAUR:(18708,8,3,3,('Centaur',)), RHINO:(18808,8,4,4,())}
WITNESSES = {*TOKENS, BEAR, CAD, RELIC, FOREST, ISLAND, DUAL, BLINK, MURDER, STONE, ZULAPORT,
             WATER, FLYING_MEN, PIKER, PYRO, COAT, RHINO_WITNESS}

def label(csvid, mode, owner):
    return f'{PREFIX}_{csvid}_{mode}_{owner}'


class Board:
    def __init__(self, row, mode, owner, rows, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.mode, self.owner, self.foe = mode, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.pools, self.life = [ZERO, ZERO], [20, 20]
        self.active = owner
        self.add('source', self.name, owner, HAND)
        {MONK:self.monk_setup,TWINGROVE:self.twingrove_setup,
         MASTER:self.master_setup,SUMMONER:self.summoner_setup}[self.id]()

    def add(self, ref, name, player, zone=BATTLE, *, tapped=False):
        assert ref not in self.cards
        self.setup.append((ref, player, zone, self.rows[name]['csvid'], int(tapped)))
        self.cards[ref] = [name, player, zone]

    def emit(self, op, ref='', other='', *, prop='', player=0, value=0, zone=HAND, index=0, color=0):
        self.steps.append((op, ref, other, prop, player, value, zone, index, color))

    def see(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def fund(self, player, mana):
        self.pools[player] = tuple(mana)
        for color, count in enumerate(mana):
            self.emit('set_mana', player=player, color=color, value=count)
        self.emit('refresh')

    def see_mana(self, player):
        for color, count in enumerate(self.pools[player]):
            self.emit('assert_mana', player=player, color=color, value=count)

    def move(self, ref, zone):
        self.cards[ref][2] = zone

    def state(self):
        self.emit('assert_stack')
        for p in (0, 1):
            self.see_mana(p)
            self.emit('assert_life', player=p, value=self.life[p])
            for zone in (HAND, BATTLE, GRAVE):
                items = Counter(name for name, player, where in self.cards.values()
                                if where == zone and player == p)
                self.emit('assert_zone', player=p, zone=zone, value=items.total())
                if zone != BATTLE:
                    for name, count in sorted(items.items()):
                        self.emit('assert_zone_multiplicity', player=p, zone=zone, value=count,
                                  index=self.rows[name]['csvid'])
        for ref, (_, _, zone) in self.cards.items():
            self.see(ref, 'in_play', int(zone == BATTLE))

    def settle(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack')

    def put_on_stack(self, ref, targets=()):
        name, player, zone = self.cards[ref]
        assert zone == HAND
        self.fund(player, cost_from_oracle(self.oracle[name]))
        for target in targets:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.pools[player] = ZERO
        self.see_mana(player)

    def cast(self, ref, targets=()):
        self.put_on_stack(ref, targets)
        self.settle()
        self.move(ref, GRAVE if self.oracle[self.cards[ref][0]]['type_line'] in ('Instant','Sorcery') else BATTLE)

    def visit(self, player):
        assert player == 1-self.active and self.pools == [ZERO, ZERO]
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)
        self.active = player

    def attack(self, refs, *, vigilant=()):
        self.emit('begin_attackers')
        for ref in refs:
            self.emit('attack', ref, value=1)
            self.see(ref, 'attacking', 1)
            self.see(ref, 'tapped', int(ref not in vigilant))
        self.emit('finish_attackers')
        self.settle()

    def blockers(self, blocks=()):
        self.emit('begin_blockers')
        for blocker, attacker, legal in blocks:
            self.emit('block', blocker, attacker, value=legal)
        self.emit('advance_combat')
        self.settle()
        committed = {attacker for _, attacker, legal in blocks if legal}
        for _, attacker, _ in blocks:
            self.see(attacker, 'blocked', int(attacker in committed))

    def damage(self, *, hit=0, dead=(), marked=()):
        self.emit('combat_damage')
        self.settle()
        for ref in dead:
            self.move(ref, GRAVE)
        for ref, damage in marked:
            self.see(ref, 'damage', damage)
        self.life[1-self.active] -= hit
        self.emit('end_combat')
        self.state()

    def stats(self, ref, bonus=0):
        name = self.cards[ref][0]
        for prop in ('power', 'toughness'):
            self.see(ref, prop, int(self.oracle[name][prop])+bonus)

    def monk_setup(self):
        if self.mode in ('scope','shrinks'):
            self.add('own_bear', BEAR, self.owner)
            self.add('enemy_bear', BEAR, self.foe)
            self.add('noncreature', RELIC, self.owner)
        if self.mode == 'base':
            self.add('guard', BEAR, self.foe)
        elif self.mode == 'shrinks':
            self.add('first', MURDER, self.foe, HAND)
            self.add('second', MURDER, self.foe, HAND)
        elif self.mode == 'blink':
            self.add('blink', BLINK, self.owner, HAND)

    def twingrove_setup(self):
        if self.mode != 'zero':
            self.add('basic', FOREST, self.owner)
        if self.mode in ('grows','shrink'):
            self.add('typed_dual', DUAL, self.owner)
        if self.mode in ('ownership','zero'):
            self.add('foreign_forest_a', FOREST, self.foe)
            self.add('foreign_forest_b', FOREST, self.foe)
            self.add('not_forest', ISLAND, self.owner)
        if self.mode == 'grows':
            self.add('land_drop', FOREST, self.owner, HAND)
        elif self.mode == 'shrink':
            self.add('answer', STONE, self.owner, HAND)
        elif self.mode == 'zero':
            self.add('observer', ZULAPORT, self.owner)

    def master_setup(self):
        if self.mode in ('devotion','lord_removed'):
            self.add('elemental', WATER, self.owner)
            self.add('foreign_elemental', WATER, self.foe)
            self.add('blue_non_elemental', FLYING_MEN, self.owner)
        if self.mode == 'lord_removed':
            self.add('answer', MURDER, self.foe, HAND)
        if self.mode == 'red_block':
            self.add('red_guard', PIKER, self.foe)
            self.add('green_guard', BEAR, self.foe)
        elif self.mode == 'red_sweep':
            self.add('sweep', PYRO, self.owner, HAND)

    def summoner_setup(self):
        if self.mode == 'combat':
            self.add('guard', CAD, self.foe)
        elif self.mode == 'rhino_type':
            self.add('coat', COAT, self.owner)
            self.add('rhino_witness', RHINO_WITNESS, self.owner)
        elif self.mode == 'source_removed':
            self.add('answer', MURDER, self.foe, HAND)
        elif self.mode == 'reentry':
            self.add('blink', BLINK, self.owner, HAND)

    def pt(self, ref, power, toughness=None):
        self.see(ref,'power',power)
        self.see(ref,'toughness',power if toughness is None else toughness)

    def birth(self, name, count, *, start=0, delta=0, pt=None, prefix='token'):
        cid,color,power,toughness,types = TOKENS[name]
        for index in range(start,start+count):
            ref=f'{prefix}_{index}'
            self.emit('bind_permanent_copy',ref,player=self.owner,value=cid,index=index)
            self.cards[ref]=[name,self.owner,BATTLE]
            self.see(ref,'tapped',0)
            self.see(ref,'sick',1)
        body = (power+delta,toughness+delta) if pt is None else pt
        fields=(('token',1),('color',color),('artifact',0),('power',body[0]),('toughness',body[1]))
        for prop,val in (*fields,*((f'subtype_{t}',1) for t in types)):
            self.emit('assert_matching_permanents',player=self.owner,prop=prop,value=val,index=cid)

    def rebound_source(self):
        self.cards.pop('source')
        self.emit('bind_permanent','returned',player=self.owner,index=-1,value=self.id)
        self.cards['returned']=[self.name,self.owner,BATTLE]

    def kill_token(self, ref):
        self.cards.pop(ref)
        self.see(ref,'in_play',0)

    def cycle(self):
        self.visit(self.foe)
        self.visit(self.owner)

    def monk(self):
        self.cast('source')
        self.birth(SPIRIT,2)
        base=4 if self.mode in ('scope','shrinks') else 3
        self.pt('source',base)
        self.state()
        if self.mode == 'base':
            self.cycle()
            self.attack(('source','token_0','token_1'),vigilant=('source',))
            self.blockers((('guard','token_0',0),('guard','token_1',0)))
            self.damage(hit=5)
        elif self.mode == 'shrinks':
            self.cast('first',('own_bear',))
            self.move('own_bear',GRAVE)
            self.pt('source',3)
            self.state()
            self.cast('second',('token_0',))
            self.kill_token('token_0')
            self.pt('source',2)
            self.state()
        elif self.mode == 'blink':
            self.cast('blink',('source',))
            self.rebound_source()
            self.birth(SPIRIT,2,start=2)
            self.pt('returned',5)
            self.state()

    def twingrove(self):
        self.cast('source')
        if self.mode == 'zero':
            self.move('source',GRAVE)
            self.life[self.owner]+=2
            self.life[self.foe]-=2
            self.state()
            return
        total=1 if self.mode=='ownership' else 2
        self.pt('source',total)
        self.birth(TREE,1,pt=(total,total))
        self.state()
        if self.mode == 'grows':
            self.emit('play_land','land_drop',value=1)
            self.emit('refresh')
            self.emit('assert_unused_choices')
            self.move('land_drop',BATTLE)
            total+=1
        elif self.mode == 'shrink':
            self.cast('answer',('typed_dual',))
            self.move('typed_dual',GRAVE)
            total-=1
        else:
            return
        self.pt('source',total)
        self.pt('token_0',total)
        self.state()

    def master(self):
        self.cast('source')
        same_deck = self.mode in ('devotion','lord_removed')
        count=4 if same_deck else 1
        self.pt('source',2,1)
        self.birth(ELEMENTAL,count,delta=1)
        if same_deck:
            self.stats('elemental',1)
            self.stats('foreign_elemental')
            self.stats('blue_non_elemental')
        self.state()
        if self.mode == 'lord_removed':
            self.cast('answer',('source',))
            self.move('source',GRAVE)
            for index in range(count):
                self.kill_token(f'token_{index}')
            self.stats('elemental')
            self.stats('foreign_elemental')
            self.state()
        elif self.mode == 'red_sweep':
            self.cast('sweep')
            self.kill_token('token_0')
            self.see('source','damage',0)
            self.pt('source',2,1)
            self.state()
        elif self.mode == 'red_block':
            self.cycle()
            self.attack(('source',))
            self.blockers((('red_guard','source',0),('green_guard','source',1)))
            self.emit('combat_damage')
            self.settle()
            self.move('source',GRAVE)
            self.move('green_guard',GRAVE)
            self.kill_token('token_0')
            self.emit('end_combat')
            self.state()

    def summoner(self):
        if self.mode=='rhino_type':
            self.stats('rhino_witness')
        self.cast('source')
        self.stats('source')
        self.birth(KNIGHT,1,prefix='knight')
        self.birth(CENTAUR,1,prefix='centaur')
        self.birth(RHINO,1,prefix='rhino',delta=int(self.mode=='rhino_type'))
        self.state()
        if self.mode=='combat':
            self.cycle()
            self.attack(('knight_0','centaur_0','rhino_0'),vigilant=('knight_0',))
            self.blockers((('guard','rhino_0',1),))
            self.damage(hit=8,dead=('guard',),marked=(('rhino_0',1),))
        elif self.mode=='rhino_type':
            self.stats('rhino_witness',1)
        elif self.mode=='source_removed':
            self.cast('answer',('source',))
            self.move('source',GRAVE)
            self.state()
        else:
            self.cast('blink',('source',))
            self.rebound_source()
            for name in (KNIGHT,CENTAUR,RHINO):
                self.birth(name,1,start=1,prefix=name.lower())
            self.state()

    def run(self):
        self.emit('turn',player=self.owner)
        self.fund(self.foe,ZERO)
        self.fund(self.owner,ZERO)
        {MONK:self.monk,TWINGROVE:self.twingrove,MASTER:self.master,SUMMONER:self.summoner}[self.id]()


OVERLORD, CHANCELLOR, HEDGE, SARCOMANCY = 13366, 11580, 9796, 3262
SECURITY, TRIAL, RISE, COVENANT = 12813, 17053, 9858, 10324
DRAGON, HORROR_SOURCE, LAMMASU, PHANTASM = 7879, 7880, 7881, 7882
FOLLOW_MODES = {
    OVERLORD: ('devotion','flying','sacrifice'),
    CHANCELLOR: ('count','haste','opening_reveal','opening_decline'),
    HEDGE: ('plains','swamps','foreign_lands','decline_plains'),
    SARCOMANCY: ('own_zombie','no_zombie','foreign_zombie'),
    SECURITY: ('own_land','no_land','foreign_land'),
    TRIAL: ('own_cartouche','foreign_cartouche','recast'),
    RISE: ('paid_x','zero_x','first_strike'),
    COVENANT: ('own_grave','opponent_grave','lifelink'),
    DRAGON: ('entry','haste','knight_first_strike','untargetable'),
    HORROR_SOURCE: ('entry','protected_centaur','trample','untargetable'),
    LAMMASU: ('entry','flying','untargetable'),
    PHANTASM: ('entry','unblockable','untargetable'),
}
ALL_MODES = MODES | FOLLOW_MODES
HUMAN_ONLY = {'opening_reveal','opening_decline','decline_plains'}
HARPY, GOBLIN, KITHKIN, ZOMBIE = 'Harpy', 'Goblin', 'Kithkin Soldier', 'Zombie'
BEAST, SOLDIER, HORROR = 'Beast', 'Goblin Soldier', 'Horror'
TOKENS |= {HARPY:(18756,2,1,1,()), GOBLIN:(18746,16,1,1,('Goblin',)),
           KITHKIN:(18775,32,1,1,('Kithkin','Soldier')), ZOMBIE:(18860,2,2,2,('Zombie',)),
           BEAST:(18695,8,4,2,('Beast',)), SOLDIER:(18749,48,1,1,('Goblin','Soldier')),
           HORROR:(18761,2,4,4,('Horror',))}
PLAINS, SWAMP, SCRUBLAND = 'Plains', 'Swamp', 'Scrubland'
CORPSE = 'Walking Corpse'
CARTOUCHE, LEYLINE = 'Cartouche of Zeal', 'Leyline of Sanctity'
WITNESSES |= {*TOKENS, PLAINS, SWAMP, SCRUBLAND, CORPSE, CARTOUCHE, LEYLINE}
GAPS = {
    TWINGROVE: ['Zulaport Cutthroat distinguishes the two 0/0 deaths; no observer binds a token that dies before the entry callback returns.'],
    SUMMONER: ['The subtype observer has no Rhino enum; Coat of Arms and a distinct nontoken Rhino verify the token shares that subtype.'],
    OVERLORD: ['Harpy has no direct subtype enum; exact token identity and actual airborne combat cover its printed body.'],
    CHANCELLOR: ['Production presents opening choices through the rules engine on first human upkeep. The private fixture cannot choose a separate opening-hand epoch or override the AI seat’s reveal policy; both seats exercise normal ETB. The token base currently exposes Goblin; Phyrexian has no token subtype observer.'],
    HEDGE: ['The AI normally accepts the optional Plains token. The printed decline is exercised through the Human dialog only.'],
    SECURITY: ['The old granted activation selects an existing in-flight damage object. The duel DSL cannot pause between damage creation and prevention processing to activate a nonmana granted ability. The land/Aura legality and ETB token are covered.'],
    DRAGON: ['The suite is two-player; target availability is distinguished with a hexproof sole opponent, and live token ownership is checked.'],
    HORROR_SOURCE: ['The suite is two-player; target availability is distinguished with a hexproof sole opponent, and live token ownership is checked.'],
    LAMMASU: ['The suite is two-player; target availability is distinguished with a hexproof sole opponent, and live token ownership is checked.'],
    PHANTASM: ['The suite is two-player; target availability is distinguished with a hexproof sole opponent, and live token ownership is checked.'],
}


class FollowBoard(Board):
    def __init__(self, row, mode, owner, rows, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.mode, self.owner, self.foe = mode, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.pools, self.life, self.active = [ZERO,ZERO], [20,20], owner
        self.zone_only = set()
        self.add('source',self.name,owner,HAND)
        self.setup_witnesses()

    def setup_witnesses(self):
        p,q,m = self.owner,self.foe,self.mode
        if self.id == OVERLORD:
            if m == 'sacrifice':
                self.add('offering',BEAR,p)
            elif m == 'devotion':
                self.add('devotion',CORPSE,p)
                self.add('foreign_devotion',CORPSE,q)
                self.add('swamp',SWAMP,p)
            else:
                self.add('ground',BEAR,q)
        elif self.id == CHANCELLOR and m == 'count':
            self.add('own_creature',BEAR,p)
            self.add('their_creature',BEAR,q)
            self.add('own_artifact',RELIC,p)
        elif self.id == HEDGE:
            if m in ('plains','decline_plains'):
                self.add('plains',PLAINS,p,tapped=True)
                self.add('dual',SCRUBLAND,p,tapped=True)
            elif m == 'swamps':
                self.add('swamp',SWAMP,p,tapped=True)
                self.add('dual',SCRUBLAND,p,tapped=True)
            else:
                for name in (PLAINS,SWAMP):
                    self.add('ours_'+name.lower(),name,p,tapped=True)
                    self.add('theirs_'+name.lower(),name,q)
                self.add('theirs_dual',SCRUBLAND,q)
            self.add('witness',BEAR,q)
        elif self.id == SARCOMANCY:
            if m != 'own_zombie':
                self.add('removal',MURDER,p,HAND)
            if m == 'foreign_zombie':
                self.add('foreign_zombie',CORPSE,q)
        elif self.id == SECURITY:
            if m != 'no_land':
                self.add('land',ISLAND,q if m=='foreign_land' else p)
            self.add('not_land',BEAR,p)
        elif self.id == TRIAL:
            actor = q if m == 'foreign_cartouche' else p
            self.add('host',BEAR,actor)
            self.add('cartouche',CARTOUCHE,actor,HAND)
        elif self.id == RISE and m == 'first_strike':
            self.add('red',PIKER,p)
            self.add('white',CAD,p)
            self.add('green',BEAR,p)
            for ref in ('red_guard','white_guard','green_guard'):
                self.add(ref,CAD,q)
        elif self.id == COVENANT:
            target = q if m == 'opponent_grave' else p
            for ref,name,who in (('corpse',CORPSE,target),('bear',BEAR,target),
                                 ('noncreature',RELIC,target),('untargeted',CAD,1-target)):
                self.add(ref,name,who,GRAVE)
                self.zone_only.add(ref)
            if m == 'lifelink':
                self.add('foreign_zombie',CORPSE,q)
                self.add('friendly_zombie',CORPSE,p)
        elif self.id in (DRAGON,HORROR_SOURCE,LAMMASU,PHANTASM):
            if m == 'untargetable':
                self.add('hexproof',LEYLINE,q)
            elif m == 'knight_first_strike':
                self.add('guard',BEAR,p)
            elif m == 'protected_centaur':
                self.add('probe',MURDER,p,HAND)

    def state(self):
        self.emit('assert_stack')
        for actor in (0,1):
            self.see_mana(actor)
            self.emit('assert_life',player=actor,value=self.life[actor])
            for zone in (HAND,BATTLE,GRAVE,EXILE) if self.id == COVENANT else (HAND,BATTLE,GRAVE):
                counts = Counter(n for n,p,z in self.cards.values() if (p,z)==(actor,zone))
                self.emit('assert_zone',player=actor,zone=zone,value=counts.total())
                for name,count in sorted(counts.items()):
                    self.emit('assert_zone_multiplicity',player=actor,zone=zone,value=count,
                              index=self.rows[name]['csvid'])
        for ref,(_,_,zone) in self.cards.items():
            if ref not in self.zone_only:
                self.see(ref,'in_play',int(zone==BATTLE))

    def put_on_stack(self, ref, targets=()):
        if ref != 'source' or self.id not in (HEDGE,RISE):
            return super().put_on_stack(ref,targets)
        cast, extra = ((2,0,0,0,0,1),0) if self.id == HEDGE else (
            (0,0,0,0,0,2),2 if self.mode=='paid_x' else 0)
        self.fund(self.owner,(cast[0]+extra,*cast[1:]))
        self.emit('cast',ref,value=1)
        self.emit('assert_unused_choices')
        self.emit('assert_stack',value=1)
        self.pools[self.owner] = (extra,0,0,0,0,0)
        self.see_mana(self.owner)

    def source(self, targets=(), choices=()):
        self.put_on_stack('source',targets)
        for op,ref,value in choices:
            self.emit(op,ref,player=self.owner,value=value)
        self.settle()
        self.move('source',BATTLE)

    def observe_tokens(self, name, count, *, seat=None, pt=None, start=0, prefix='token', sick=1):
        seat = self.owner if seat is None else seat
        cid,color,power,toughness,types = TOKENS[name]
        for i in range(start,start+count):
            ref = f'{prefix}_{i}'
            self.emit('bind_permanent_copy',ref,player=seat,value=cid,index=i)
            self.cards[ref] = [name,seat,BATTLE]
            self.see(ref,'tapped',0)
            if sick is not None:
                self.see(ref,'sick',sick)
        power,toughness = (power,toughness) if pt is None else pt
        for prop,value in (('token',1),('color',color),('power',power),('toughness',toughness),
                           *((f'subtype_{t}',1) for t in types)):
            self.emit('assert_matching_permanents',prop=prop,player=seat,value=value,index=cid)

    def upkeep(self, actor, target=None):
        assert actor == 1-self.active and self.pools == [ZERO,ZERO]
        self.emit('advance_turn')
        self.emit('untap_step',player=actor)
        if target is not None:
            self.emit('choose_target',target,player=actor)
        self.emit('upkeep_step',player=actor)
        self.settle()
        self.emit('turn',player=actor)
        self.active = actor

    def overlord(self):
        self.source()
        self.observe_tokens(HARPY,3 if self.mode=='devotion' else 2)
        self.stats('source')
        self.state()
        if self.mode=='sacrifice':
            self.upkeep(self.foe)
            self.state()
            if self.owner == 0:
                self.upkeep(self.owner,'offering')
                self.move('offering',GRAVE)
                self.state()
            else:
                # The AI chooses its own sacrifice. Count the actual board
                # loss without scripting a Human picker or dictating a choice.
                self.upkeep(self.owner)
                self.emit('assert_zone',player=self.owner,zone=BATTLE,value=3)
                self.emit('assert_life',player=self.owner,value=20)
        elif self.mode=='flying':
            self.cycle()
            self.attack(('source','token_0'))
            self.blockers((('ground','source',0),('ground','token_0',0)))
            self.damage(hit=7)

    def chancellor(self):
        if self.mode.startswith('opening_'):
            assert self.owner==0
            self.emit('turn',player=1)
            self.emit('advance_turn')
            self.emit('untap_step',player=0)
            self.emit('choose_dialog',player=0,value=int(self.mode=='opening_decline'))
            self.emit('upkeep_step',player=0)
            self.settle()
            if self.mode=='opening_reveal':
                self.observe_tokens(GOBLIN,1,sick=None)
            self.state()
            return
        self.source()
        self.observe_tokens(GOBLIN,2 if self.mode=='count' else 1,sick=None)
        self.stats('source')
        self.state()
        if self.mode=='haste':
            self.attack(('token_0',))
            self.blockers()
            self.damage(hit=1)

    def hedge(self):
        choice = ()
        if self.mode in ('plains','decline_plains') and self.owner==0:
            choice = (('choose_dialog','',int(self.mode=='decline_plains')),)
        elif self.mode=='swamps':
            choice = (('choose_target','witness',0),)
        if self.owner==0 and choice:
            choice = (('choose_trigger','source',0),) + choice
        self.source(choices=choice)
        if self.mode=='plains':
            self.observe_tokens(KITHKIN,1)
        if self.mode=='swamps':
            self.stats('witness',-1)
        else:
            self.stats('witness')
        self.see('witness','minus1_counters',int(self.mode=='swamps'))
        self.stats('source')
        self.state()

    def sarcomancy(self):
        self.source()
        self.observe_tokens(ZOMBIE,1)
        self.state()
        if self.mode!='own_zombie':
            self.cast('removal',('token_0',))
            self.kill_token('token_0')
        self.upkeep(self.foe)
        self.state()
        self.upkeep(self.owner)
        self.life[self.owner] -= int(self.mode=='no_zombie')
        self.state()

    def security(self):
        self.fund(self.owner,cost_from_oracle(self.oracle[self.name]))
        self.emit('can_cast','source',value=int(self.mode!='no_land'))
        if self.mode!='own_land':
            self.see('source','in_hand',1)
            self.state()
            return
        self.source(('land',))
        self.observe_tokens(KNIGHT,1)
        self.state()
        self.cycle()
        self.attack(('token_0',),vigilant=('token_0',))
        self.blockers()
        self.damage(hit=2)

    def trial(self):
        self.source()
        self.observe_tokens(BEAST,1)
        self.state()
        if self.mode=='foreign_cartouche':
            self.visit(self.foe)
        self.put_on_stack('cartouche',('host',))
        self.emit('choose_target','host',player=self.active)
        self.settle()
        self.move('cartouche',BATTLE)
        if self.mode!='foreign_cartouche':
            self.move('source',HAND)
        self.state()
        if self.mode=='recast':
            self.cards.pop('source')
            self.emit('bind_hand','returned',player=self.owner,value=self.id,index=-1)
            self.cards['returned']=[self.name,self.owner,HAND]
            self.cast('returned')
            self.observe_tokens(BEAST,1,start=1)
            self.state()

    def rise(self):
        x = 2 if self.mode=='paid_x' else 0
        self.source(choices=(('choose_x','',x),))
        self.pools[self.owner] = ZERO
        if x:
            self.observe_tokens(SOLDIER,x)
        self.state()
        if self.mode=='first_strike':
            self.fund(self.owner,(0,0,0,0,1,0))
            self.emit('can_activate','source',value=1)
            self.emit('activate','source',value=1)
            self.settle()
            self.pools[self.owner] = ZERO
            self.see_mana(self.owner)
            self.attack(('red','white','green'))
            self.blockers(tuple((ref+'_guard',ref,1) for ref in ('red','white','green')))
            self.damage(dead=('red_guard','white_guard','green_guard'),marked=(('red',0),('white',0),('green',1)))

    def covenant(self):
        target = self.foe if self.mode=='opponent_grave' else self.owner
        self.source(choices=(('choose_player','',target),))
        for ref in ('corpse','bear'):
            self.move(ref,EXILE)
        self.observe_tokens(ZOMBIE,2)
        self.state()
        if self.mode=='lifelink':
            self.visit(self.foe)
            self.attack(('foreign_zombie',))
            self.blockers()
            self.damage(hit=2)
            self.visit(self.owner)
            self.attack(('friendly_zombie','token_0'))
            self.blockers()
            self.life[self.owner] += 4
            self.damage(hit=4)

    def hunted(self):
        self.source()
        token,count = {DRAGON:(KNIGHT,3),HORROR_SOURCE:(CENTAUR,2),
                       LAMMASU:(HORROR,1),PHANTASM:(GOBLIN,5)}[self.id]
        self.stats('source')
        if self.mode!='untargetable':
            self.observe_tokens(token,count,seat=self.foe)
        self.state()
        if self.mode in ('haste','flying','unblockable'):
            if self.mode!='haste':
                self.cycle()
            self.attack(('source',))
            self.blockers((('token_0','source',0),))
            self.damage(hit=int(self.oracle[self.name]['power']))
        elif self.mode=='knight_first_strike':
            self.visit(self.foe)
            self.attack(('token_0',))
            self.blockers((('guard','token_0',1),))
            self.damage(dead=('guard',),marked=(('token_0',0),))
        elif self.mode=='protected_centaur':
            self.emit('can_target_creature','probe','source',value=1)
            self.emit('can_target_creature','probe','token_0',value=0)
            self.emit('can_target_creature','probe','token_1',value=0)
        elif self.mode=='trample':
            self.cycle()
            self.attack(('source',))
            self.blockers((('token_0','source',1),))
            self.damage(hit=4,marked=(('token_0',0),('source',3)))

    def run(self):
        self.emit('turn',player=self.owner)
        self.fund(self.foe,ZERO)
        self.fund(self.owner,ZERO)
        actions = {OVERLORD:self.overlord,CHANCELLOR:self.chancellor,HEDGE:self.hedge,
                   SARCOMANCY:self.sarcomancy,SECURITY:self.security,TRIAL:self.trial,
                   RISE:self.rise,COVENANT:self.covenant}
        actions.get(self.id,self.hunted)()


ZONES = ('hand','battlefield','library','graveyard','exile')
ZONE_OPS = {'assert_zone','assert_zone_multiplicity'}
INDEXED = {'assert_zone_multiplicity','assert_matching_permanents','bind_permanent_copy',
           'bind_permanent','bind_hand'}
SEATED = {'turn','untap_step','upkeep_step','choose_target','choose_dialog','choose_player','choose_x','choose_trigger',
          'set_mana','assert_mana','assert_life','bind_permanent_copy','bind_permanent','bind_hand',
          'assert_matching_permanents'} | ZONE_OPS
VALUED = {'cast','can_cast','activate','can_activate','can_target_creature','choose_dialog','choose_player',
          'choose_x','set_mana','assert_mana','assert_life','assert_stack','assert_card','assert_unused_choices',
          'assert_matching_permanents','bind_permanent_copy','bind_permanent','bind_hand','play_land',
          'attack','block'} | ZONE_OPS


def operation(row):
    op,ref,other,prop,player,value,zone,index,color = row
    action = dict(op=op)
    if ref:
        action['ref'] = ref
    if other:
        action['other'] = other
    if prop:
        action['property'] = prop
    if op in SEATED:
        action['player'] = player
    if op in VALUED:
        action['value'] = value
    if op in ZONE_OPS:
        action['zone'] = ZONES[zone]
    if op in INDEXED:
        action['index'] = index
    if op in ('set_mana','assert_mana'):
        action['color'] = color
    return action


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (not pin or not exact(record,pin['oracle']) or csvid not in ALL_MODES
            or pin['registry']['csvid'] != csvid or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole pinned normal follow-on token primary required')
    return dict(csvid=csvid,mana_cost=record['mana_cost'])


def variants(shape):
    return [f'{mode}_{seat}' for mode in ALL_MODES[shape['csvid']]
            for seat in ((0,) if mode in HUMAN_ONLY else (0,1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected)-set(variants(shape))):
        raise ValueError('distinct supported follow-on token variants required')


def spec(declaration,record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned follow-on token primary required')
    shape = classify(record,pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'),shape)):
        raise ValueError('exact declared follow-on token parameters required')
    validate_variants(shape,declaration.get('variants'))
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    pinned = profiles()
    if (set(pinned) != {'sources','witnesses'} or set(pinned['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pinned['sources'].values()} != ALL_MODES.keys()):
        raise ValueError('complete follow-on token source and witness pins required')
    for group,entries in pinned.items():
        for name,pin in entries.items():
            record,row = oracle.get(name),rows.get(name,{})
            if not exact(record,pin['oracle']) or not exact(row,pin['registry']):
                raise ValueError('follow-on token source or witness changed: '+name)
            if group == 'sources':
                if row.get('kind') != 'card' or row.get('handler') in (None,'0','NULL','card_dummy'):
                    raise ValueError('implemented follow-on token primary required')
                classify(record,row['csvid'])
            elif name in TOKENS and (row['csvid'] != TOKENS[name][0] or record['layout'] != 'token'):
                raise ValueError('exact physical token template required')


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered follow-on token source required')
    validate_variants(shape,selected)
    validate_witnesses(oracle,rows)
    result = []
    reverse = {rows[n]['csvid']:n for n in WITNESSES | {name}}
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        board_type = FollowBoard if csvid in FOLLOW_MODES else Board
        game = board_type(rows[name],mode,int(seat),rows,oracle)
        game.run()
        setup = []
        for ref,actor,zone,card,tapped in game.setup:
            item = dict(name=reverse[card],ref=ref,player=actor,zone=ZONES[zone])
            if tapped:
                item['state'] = ['tapped']
            setup.append(item)
        result.append(dict(name=label(csvid,mode,seat),card=name,fixture='empty_main',seed=SEED,
                           setup=setup,steps=[operation(t) for t in game.steps],
                           _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                                        oracle_text=declaration['oracle_text'],untested_components=GAPS.get(csvid,[]))))
    return result

