"""Oracle-pinned battlefield departure and token lifecycle tests."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from generate_cases import OPS
from recipes import cost_from_oracle

FAMILY = "departure_tokens"
PROFILE = Path(__file__).resolve().parent / "data/departure_tokens_wave8_profiles.json"

SKULKER, EGG, REEF, SLIME = 14083,13282,14482,11150
PREFIX, SEED = 'DepartureTokensRecipe',3596
MODES = {
    SKULKER: ('zero','seat_draw','typed','islandwalk','nonisland','bounce'),
    EGG: ('defender','combat_death','pump_flight','exile'),
    REEF: ('chain','typed_chain','fish_exile','whale_bounce','source_bounce'),
    SLIME: ('split','sweep','large_exile','source_bounce','typed'),
}
CONTROLS = ((SKULKER,'islandwalk',0),(SKULKER,'seat_draw',1),(EGG,'combat_death',1),(EGG,'pump_flight',0),
            (REEF,'typed_chain',0),(REEF,'whale_bounce',1),(SLIME,'split',0),(SLIME,'sweep',1))
HAND,BATTLE,LIBRARY,GRAVE,EXILE = range(5)
ZERO = (0,)*6
SQUID,DRAGON,FISH,WHALE,KRAKEN,OOZE = 'Squid','Dragon','Fish','Whale','Kraken','Ooze'
TOKENS = {SQUID:(18832,4),DRAGON:(18719,16),FISH:(18738,4),WHALE:(18854,4),KRAKEN:(18780,4),OOZE:(18796,8)}
MURDER,BOUNCE,SWORDS,DRAW,SWEEP = 'Murder','Unsummon','Swords to Plowshares','Divination','Pyroclasm'
COAT,SAND,EEL,NARWHAL,HATCHLING,SCAVENGER = 'Coat of Arms','Sand Squid','Coral Eel','Narwhal','Kraken Hatchling','Scavenging Ooze'
WALL,CAD,GIANT,DUAL,FOREST,PLAINS,ISLAND = ('Wall of Stone','Eager Cadet','Hill Giant','Tropical Island',
                                         'Forest','Plains','Island')
WITNESSES = {*TOKENS,MURDER,BOUNCE,SWORDS,DRAW,SWEEP,COAT,SAND,EEL,NARWHAL,HATCHLING,SCAVENGER,
             WALL,CAD,GIANT,DUAL,FOREST,PLAINS,ISLAND}


def label(csvid,mode,seat):
    return f'{PREFIX}_{csvid}_{mode}_{seat}'


class Board:
    def __init__(self, row, mode, actor, rows, oracle, *, source_zone=HAND):
        self.id,self.name = row['csvid'],row['name']
        self.mode,self.actor,self.foe,self.active = mode,actor,1-actor,actor
        self.rows,self.oracle = rows,oracle
        self.setup,self.steps,self.cards = [],[],{}
        self.mana,self.life = [ZERO,ZERO],[20,20]
        self.add('source',self.name,actor,source_zone)
        self.witnesses()

    def add(self, ref, name, player, zone=BATTLE):
        assert ref not in self.cards
        self.setup.append((ref,player,zone,self.rows[name]['csvid'],0))
        self.cards[ref]=[name,player,zone]

    def removals(self, *names):
        for index,name in enumerate(names):
            self.add(f'removal_{index}',name,self.foe,HAND)

    def witnesses(self):
        p,q,m = self.actor,self.foe,self.mode
        if self.id==SKULKER:
            self.removals(BOUNCE if m=='bounce' else MURDER)
            if m=='seat_draw':
                self.add('enemy_draw',FOREST,q,LIBRARY)
                self.add('own_draw',PLAINS,p,LIBRARY)
            elif m!='zero':
                self.add('draw',DRAW,p,HAND)
                self.add('first_draw',PLAINS,p,LIBRARY)
                self.add('second_draw',ISLAND,p,LIBRARY)
            if m in ('islandwalk','nonisland'):
                self.add('defending_land',DUAL if m=='islandwalk' else FOREST,q)
                self.add('wall',WALL,q)
            elif m=='typed':
                self.add('coat',COAT,p)
                self.add('squid_witness',SAND,q)
        elif self.id==EGG:
            if m=='defender':
                self.add('cadet',CAD,p)
            elif m=='combat_death':
                self.add('giant',GIANT,q)
            elif m=='pump_flight':
                self.add('wall',WALL,q)
                self.removals(MURDER)
            else:
                self.removals(SWORDS)
        elif self.id==REEF:
            sequences = {'chain':(MURDER,MURDER,MURDER,MURDER),'typed_chain':(MURDER,MURDER,MURDER),
                         'fish_exile':(MURDER,SWORDS),'whale_bounce':(MURDER,MURDER,BOUNCE),
                         'source_bounce':(BOUNCE,)}
            self.removals(*sequences[m])
            if m=='typed_chain':
                self.add('coat',COAT,p)
                for ref,name in (('eel',EEL),('narwhal',NARWHAL),('hatchling',HATCHLING)):
                    self.add(ref,name,q)
        else:
            sequences = {'split':(MURDER,MURDER,MURDER,MURDER),'sweep':(MURDER,SWEEP),
                         'large_exile':(MURDER,SWORDS),'source_bounce':(BOUNCE,), 'typed':(MURDER,)}
            self.removals(*sequences[m])
            if m=='typed':
                self.add('coat',COAT,p)
                self.add('scavenger',SCAVENGER,q)

    def emit(self, op, ref='', other='', *, prop='', player=0, value=0, zone=HAND, index=0, color=0):
        assert op in OPS
        self.steps.append((op,ref,other,prop,player,value,zone,index,color))

    def see(self, ref, prop, value):
        self.emit('assert_card',ref,prop=prop,value=value)

    def stats(self, ref, power, toughness=None):
        self.see(ref,'power',power)
        self.see(ref,'toughness',power if toughness is None else toughness)

    def funds(self, p, pool=ZERO):
        self.mana[p]=tuple(pool)
        for color,count in enumerate(pool):
            self.emit('set_mana',player=p,color=color,value=count)
        self.emit('refresh')

    def wallet(self, p):
        for color,count in enumerate(self.mana[p]):
            self.emit('assert_mana',player=p,color=color,value=count)

    def state(self):
        self.emit('assert_stack')
        for p in (0,1):
            self.wallet(p)
            self.emit('assert_life',player=p,value=self.life[p])
            for zone in (HAND,BATTLE,LIBRARY,GRAVE,EXILE):
                counts = Counter(name for name,player,z in self.cards.values() if (player,z)==(p,zone))
                self.emit('assert_zone',player=p,zone=zone,value=counts.total())
                for name,count in sorted(counts.items()):
                    self.emit('assert_zone_multiplicity',player=p,zone=zone,value=count,index=self.rows[name]['csvid'])

    def settle(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices')
        self.emit('assert_stack')

    def cast(self, ref, target=None):
        name,p,zone = self.cards[ref]
        assert zone==HAND
        self.funds(p,cost_from_oracle(self.oracle[name]))
        if target:
            self.emit('choose_target',target,player=p)
        self.emit('cast',ref,value=1)
        self.emit('assert_unused_choices')
        self.emit('assert_stack',value=1)
        self.mana[p]=ZERO
        self.wallet(p)
        self.settle()
        self.cards[ref][2]=GRAVE if self.oracle[name]['type_line'] in ('Instant','Sorcery') else BATTLE

    def visit(self, p, draw=None):
        assert p==1-self.active and self.mana==[ZERO,ZERO]
        self.emit('advance_turn')
        self.emit('untap_step',player=p)
        if draw:
            self.emit('upkeep_step',player=p)
            self.emit('draw_step',player=p)
            self.settle()
            self.cards[draw][2]=HAND
        self.emit('turn',player=p)
        self.active=p

    def cycle(self):
        self.visit(self.foe)
        self.visit(self.actor)

    def depart(self, index, target='source', *, gain=0):
        p = self.cards[target][1]
        ref = f'removal_{index}'
        spell = self.cards[ref][0]
        self.cast(ref,target)
        if target!='source':
            self.cards.pop(target)
        else:
            self.cards[target][2] = {MURDER:GRAVE,BOUNCE:HAND,SWORDS:EXILE}[spell]
        if spell==SWORDS:
            self.life[p]+=gain

    def bind_tokens(self, name, refs):
        for index,ref in enumerate(refs):
            self.emit('bind_permanent_copy',ref,player=self.actor,index=index,value=TOKENS[name][0])
            self.cards[ref]=[name,self.actor,BATTLE]

    def token_facts(self, name, count, power):
        csvid,color = TOKENS[name]
        self.emit('assert_zone_multiplicity',player=self.actor,zone=BATTLE,value=count,index=csvid)
        self.emit('assert_zone_multiplicity',player=self.foe,zone=BATTLE,index=csvid)
        facts = [('token',1),('color',color),('artifact',0),('power',power),('toughness',power)]
        if name==DRAGON:
            facts.append(('subtype_Dragon',1))
        for prop,value in facts:
            self.emit('assert_matching_permanents',prop=prop,player=self.actor,value=value,index=csvid)

    def start_combat(self, attacker, *, denied=None, blocker=None, legal=1):
        self.emit('begin_attackers')
        if denied:
            self.emit('attack',denied,value=0)
        self.emit('attack',attacker,value=1)
        self.emit('finish_attackers')
        self.settle()
        self.emit('begin_blockers')
        if blocker:
            self.emit('block',blocker,attacker,value=legal)
        self.emit('advance_combat')
        self.settle()
        if blocker:
            self.see(attacker,'blocked',legal)

    def damage(self, hit):
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')
        self.life[1-self.active]-=hit

    def skulker(self):
        p,q,m = self.actor,self.foe,self.mode
        self.cast('source')
        self.see('source','plus1_counters',0)
        if m=='seat_draw':
            self.visit(q,draw='enemy_draw')
            self.see('source','plus1_counters',0)
            self.visit(p,draw='own_draw')
            count = 1
        elif m!='zero':
            self.cast('draw')
            self.cards['first_draw'][2]=self.cards['second_draw'][2]=HAND
            count = 2
        else:
            count = 0
        self.see('source','plus1_counters',count)
        self.stats('source',1+count+int(m=='typed'))
        self.state()
        self.depart(0)
        if count and m!='bounce':
            self.bind_tokens(SQUID,tuple(f'squid_{i}' for i in range(count)))
            self.token_facts(SQUID,count,3 if m=='typed' else 1)
        if m=='typed':
            self.stats('squid_witness',int(self.oracle[SAND]['power'])+2,
                       int(self.oracle[SAND]['toughness'])+2)
        self.state()
        if m in ('islandwalk','nonisland'):
            self.cycle()
            blocks = m=='nonisland'
            self.start_combat('squid_0',blocker='wall',legal=int(blocks))
            self.damage(0 if blocks else 1)
            self.see('wall','damage',int(blocks))
            self.state()

    def egg(self):
        p,q,m = self.actor,self.foe,self.mode
        self.cast('source')
        self.stats('source',0,2)
        if m=='defender':
            self.cycle()
            self.start_combat('cadet',denied='source')
            self.damage(1)
            self.state()
            return
        if m=='combat_death':
            self.visit(q)
            self.start_combat('giant',blocker='source')
            self.damage(0)
            self.cards['source'][2]=GRAVE
        else:
            self.depart(0)
        if m=='exile':
            self.state()
            return
        self.bind_tokens(DRAGON,('dragon',))
        self.token_facts(DRAGON,1,2)
        self.state()
        if m!='pump_flight':
            return
        self.cycle()
        self.funds(p,(0,0,0,1,0,0))
        self.emit('can_activate','dragon',value=0)
        self.wallet(p)
        self.funds(p,(0,0,0,0,2,0))
        for remaining in (1,0):
            self.emit('can_activate','dragon',value=1)
            self.emit('activate','dragon',value=1)
            self.mana[p]=(0,0,0,0,remaining,0)
            self.wallet(p)
            self.settle()
            self.stats('dragon',4-remaining,2)
        self.start_combat('dragon',blocker='wall',legal=0)
        self.damage(4)
        self.emit('cleanup')
        self.settle()
        self.stats('dragon',2)
        self.state()

    def reef(self):
        m = self.mode
        self.cast('source')
        self.stats('source',0,1)
        self.depart(0)
        if m=='source_bounce':
            self.state()
            return
        typed = m=='typed_chain'
        self.bind_tokens(FISH,('fish',))
        self.token_facts(FISH,1,3+int(typed))
        if typed:
            self.stats('eel',3,2)
            self.stats('narwhal',2)
            self.stats('hatchling',0,4)
        self.state()
        self.depart(1,'fish',gain=3)
        if m=='fish_exile':
            self.state()
            return
        self.bind_tokens(WHALE,('whale',))
        self.token_facts(WHALE,1,6+int(typed))
        if typed:
            self.stats('eel',2,1)
            self.stats('narwhal',3)
            self.stats('hatchling',0,4)
        self.state()
        self.depart(2,'whale')
        if m=='whale_bounce':
            self.state()
            return
        self.bind_tokens(KRAKEN,('kraken',))
        self.token_facts(KRAKEN,1,9+int(typed))
        if typed:
            self.stats('eel',2,1)
            self.stats('narwhal',2)
            self.stats('hatchling',1,5)
        self.state()
        if m=='chain':
            self.depart(3,'kraken')
            self.state()

    def slime(self):
        p,m = self.actor,self.mode
        self.cast('source')
        self.stats('source',5 if m=='typed' else 4)
        self.depart(0)
        if m=='source_bounce':
            self.state()
            return
        self.bind_tokens(OOZE,('large_0','large_1'))
        self.token_facts(OOZE,2,4 if m=='typed' else 2)
        if m=='typed':
            self.stats('scavenger',4)
        self.state()
        if m=='typed':
            return
        if m=='sweep':
            if self.active!=self.foe:
                self.visit(self.foe)
            self.cast('removal_1')
            self.cards.pop('large_0')
            self.cards.pop('large_1')
        else:
            self.depart(1,'large_0',gain=2)
            if m=='large_exile':
                self.token_facts(OOZE,1,2)
                self.state()
                return
            self.stats('large_1',2)
            self.emit('assert_zone_multiplicity',player=p,zone=BATTLE,value=3,index=TOKENS[OOZE][0])
            self.depart(2,'large_1')
        self.bind_tokens(OOZE,tuple(f'small_{i}' for i in range(4)))
        self.token_facts(OOZE,4,1)
        self.state()
        if m=='split':
            self.depart(3,'small_0')
            self.token_facts(OOZE,3,1)
            self.state()

    def run(self):
        self.emit('turn',player=self.actor)
        self.funds(self.foe)
        self.funds(self.actor)
        {SKULKER:self.skulker,EGG:self.egg,REEF:self.reef,SLIME:self.slime}[self.id]()


CHRONO, CARVER, SLAVEDRIVER, BORDER, LIEGE, WALKER = 8755, 14461, 10153, 9372, 2999, 9456
WORLDSPINE, WURMCOIL, EMISSARY, SIRE, GUARDIAN, COURIER, ANGEL = 12873, 11413, 14634, 15580, 15676, 15761, 12920
FOLLOW = {
    CHRONO: ('vanish', 'early_destroy', 'flying'),
    CARVER: ('base_power', 'sacrifice', 'intimidate_black', 'intimidate_artifact'),
    SLAVEDRIVER: ('destroy', 'bounce', 'exile', 'unearth_end', 'unearth_bounce'),
    BORDER: ('zero', 'kithkin', 'additional_counter', 'bounce'),
    LIEGE: ('own_pay', 'foe_pay', 'both_pay', 'neither_pay'),
    WALKER: ('normal_stays', 'bounce', 'exile', 'evoke'),
    WORLDSPINE: ('destroy', 'exile', 'countered', 'trample'),
    WURMCOIL: ('source_combat', 'token_combat', 'bounce'),
    EMISSARY: ('manifest_creature', 'manifest_land', 'empty', 'bounce'),
    SIRE: ('animate', 'foe_land', 'flying'),
    GUARDIAN: ('creature_count', 'reach', 'exile'),
    COURIER: ('investigate', 'crack', 'bounce'),
    ANGEL: ('return', 'no_angel', 'exile', 'flying'),
}
ALL_MODES = MODES | FOLLOW

HORROR, ZOMBIE, KITHKIN, SQUIRREL, ELEMENTAL, WURM, CLUE, CLERIC = (
    'Horror', 'Zombie', 'Kithkin Soldier', 'Squirrel', 'Elemental', 'Wurm', 'Clue', 'Cleric')
FOLLOW_TOKENS = {
    HORROR: (18761, 2, ('Horror',)), ZOMBIE: (18860, 2, ('Zombie',)),
    KITHKIN: (18775, 32, ('Kithkin', 'Soldier')), SQUIRREL: (18833, 8, ('Squirrel',)),
    ELEMENTAL: (18726, 8, ('Elemental',)), WURM: (18859, 8, ('Wurm',)),
    CLUE: (18711, 0, ('Clue',)), CLERIC: (18710, 34, ()),
}
SERRA, STEEL, BLACK_BLOCKER = 'Serra Angel', 'Steel Wall', 'Wall of Bone'
FRIEND_KITHKIN, ARMAMENT = 'Kithkin Greatheart', 'Arbor Armament'
COUNTERSPELL, SHOCK, CRYPT, STONE_RAIN = 'Counterspell', 'Shock', "Tormod's Crypt", 'Stone Rain'
FACEDOWN = 'Face-Down Creature'
FACE_CSVID = 13252
WITNESSES |= {*FOLLOW_TOKENS, SERRA, STEEL, BLACK_BLOCKER, FRIEND_KITHKIN,
              ARMAMENT, COUNTERSPELL, SHOCK, CRYPT, STONE_RAIN, FACEDOWN}


class FollowBoard(Board):
    def __init__(self, row, mode, actor, rows, oracle):
        graveyard_start = row['csvid']==SLAVEDRIVER and mode.startswith('unearth')
        super().__init__(row,mode,actor,rows,oracle,source_zone=GRAVE if graveyard_start else HAND)

    def witnesses(self):
        p,q,m = self.actor,self.foe,self.mode
        if self.id==CHRONO:
            if m=='early_destroy':
                self.removals(MURDER)
            elif m=='flying':
                self.add('wall',WALL,q)
        elif self.id==CARVER:
            self.removals(MURDER)
            if m=='sacrifice':
                self.add('victim',CAD,p)
                self.add('enemy',CAD,q)
            elif m.startswith('intimidate_'):
                self.add('wall',WALL,q)
                self.add('legal',BLACK_BLOCKER if m.endswith('black') else STEEL,q)
        elif self.id==SLAVEDRIVER:
            if m!='unearth_end':
                self.removals({'bounce':BOUNCE,'exile':SWORDS,'unearth_bounce':BOUNCE,
                               'destroy':MURDER}[m])
        elif self.id==BORDER:
            self.removals(BOUNCE if m=='bounce' else MURDER)
            self.add('enemy_kin',FRIEND_KITHKIN,q)
            self.add('nonkin',CAD,p)
            if m!='zero':
                self.add('kin',FRIEND_KITHKIN,p)
            if m=='kithkin':
                self.add('second_kin',FRIEND_KITHKIN,p)
            if m=='additional_counter':
                self.add('armament',ARMAMENT,p,HAND)
        elif self.id==LIEGE:
            self.removals(MURDER)
        elif self.id==WALKER:
            if m in ('bounce','exile'):
                self.removals(BOUNCE if m=='bounce' else SWORDS)
        elif self.id==WORLDSPINE:
            self.add('library_witness',FOREST,p,LIBRARY)
            if m in ('destroy','trample','exile'):
                self.removals(SWORDS if m=='exile' else MURDER)
            if m=='trample':
                self.add('wall',STEEL,q)
            if m=='countered':
                self.add('counter',COUNTERSPELL,q,HAND)
        elif self.id==WURMCOIL:
            if m!='source_combat':
                self.removals(BOUNCE if m=='bounce' else MURDER)
            if m in ('source_combat','token_combat'):
                self.add('wall',WALL,q)
        elif self.id==EMISSARY:
            self.removals(BOUNCE if m=='bounce' else MURDER)
            if m!='empty':
                self.add('top',GIANT if m=='manifest_creature' else FOREST,p,LIBRARY)
                self.add('second',PLAINS,p,LIBRARY)
        elif self.id==SIRE:
            if m=='flying':
                self.add('wall',WALL,q)
            else:
                self.removals(MURDER)
                self.add('enemy_land',PLAINS,q)
                if m=='animate':
                    self.add('own_land',FOREST,p)
                    self.add('land_removal',STONE_RAIN,q,HAND)
        elif self.id==GUARDIAN:
            if m=='reach':
                self.add('flier',SERRA,q)
            else:
                self.removals(SWORDS if m=='exile' else MURDER)
                self.add('spent_creature',CAD,p,GRAVE)
                self.add('spent_noncreature',SHOCK,p,GRAVE)
                self.add('enemy_spent',GIANT,q,GRAVE)
        elif self.id==COURIER:
            self.removals(BOUNCE if m=='bounce' else MURDER)
            if m=='crack':
                self.add('drawn',FOREST,p,LIBRARY)
                self.add('remaining',PLAINS,p,LIBRARY)
        elif self.id==ANGEL:
            if m=='flying':
                self.add('wall',WALL,q)
            elif m=='no_angel':
                self.removals(MURDER)
                self.add('crypt',CRYPT,q)
            else:
                self.removals(SWORDS if m=='exile' else MURDER)

    def cast(self, ref, target=None, *, fee=None, mode=None, after_cast=None):
        if (fee,mode,after_cast)==(None,None,None):
            super().cast(ref,target)
            return
        name,p,zone = self.cards[ref]
        assert zone==HAND
        self.funds(p,cost_from_oracle(self.oracle[name]) if fee is None else fee)
        if mode is not None:
            self.emit('choose_dialog',player=p,value=mode)
        if target:
            self.emit('choose_target',target,player=p)
        self.emit('cast',ref,value=1)
        self.emit('assert_unused_choices')
        self.emit('assert_stack',value=1)
        self.mana[p]=ZERO
        self.wallet(p)
        if after_cast:
            after_cast()
        self.settle()
        self.cards[ref][2]=GRAVE if self.oracle[name]['type_line'] in ('Instant','Sorcery') else BATTLE

    def bind(self, name, ref, seat=None, index=0):
        p = self.actor if seat is None else seat
        self.emit('bind_permanent_copy',ref,player=p,index=index,value=self.rows[name]['csvid'])
        self.cards[ref]=[name,p,BATTLE]

    def generated(self, name, qty, power, toughness=None, *, player=None, color=None, artifact=False):
        p = self.actor if player is None else player
        cid, printed_color, types = FOLLOW_TOKENS[name]
        self.emit('assert_zone_multiplicity',player=p,zone=BATTLE,value=qty,index=cid)
        facts = [('token',1),('color',printed_color if color is None else color),('artifact',int(artifact))]
        if power is not None:
            facts += [('power',power),('toughness',power if toughness is None else toughness)]
        facts += [(f'subtype_{kind}',1) for kind in types]
        if artifact and name==WURM:
            facts.append(('subtype_Phyrexian',1))
        if qty:
            for prop,value in facts:
                self.emit('assert_matching_permanents',prop=prop,player=p,value=value,index=cid)
        for index in range(qty):
            self.cards[f'{name}_{p}_{index}']=[name,p,BATTLE]

    def pay_activation(self, ref, fee, *, target=None, sacrifice=False):
        p = self.cards[ref][1]
        self.funds(p,fee)
        self.emit('can_activate',ref,value=1)
        if target and (not sacrifice or p==0):
            self.emit('choose_target',target,player=p)
        self.emit('activate',ref,value=1)
        self.emit('assert_stack',value=1)
        self.mana[p]=ZERO
        self.wallet(p)
        self.settle()

    def check_flying(self, power):
        self.cycle()
        self.start_combat('source',blocker='wall',legal=0)
        self.damage(power)
        self.state()

    def visit_for_upkeep(self, p):
        assert p==1-self.active and self.mana==[ZERO,ZERO]
        self.emit('advance_turn')
        self.emit('untap_step',player=p)
        self.emit('upkeep_step',player=p)
        self.settle()
        self.emit('turn',player=p)
        self.active=p

    def chronozoa(self):
        p,m = self.actor,self.mode
        self.cast('source')
        self.stats('source',3)
        self.emit('assert_counter','source',prop='time',value=3)
        if m=='early_destroy':
            self.visit_for_upkeep(self.foe)
            self.emit('assert_counter','source',prop='time',value=3)
            self.depart(0)
            self.state()
        elif m=='flying':
            self.visit_for_upkeep(self.foe)
            self.visit_for_upkeep(p)
            self.emit('assert_counter','source',prop='time',value=2)
            self.start_combat('source',blocker='wall',legal=0)
            self.damage(3)
            self.state()
        else:
            for remaining in (2,1,0):
                self.visit_for_upkeep(self.foe)
                self.visit_for_upkeep(p)
                if remaining:
                    self.emit('assert_counter','source',prop='time',value=remaining)
            self.cards['source'][2]=GRAVE
            for i in range(2):
                self.bind(self.name,f'copy_{i}',index=i)
                self.stats(f'copy_{i}',3)
                self.emit('assert_counter',f'copy_{i}',prop='time',value=3)
            for prop,value in (('token',1),('color',4),('subtype_Illusion',1)):
                self.emit('assert_matching_permanents',prop=prop,player=p,value=value,index=self.id)
            self.state()

    def carver(self):
        p,m = self.actor,self.mode
        self.cast('source')
        self.stats('source',2)
        if m=='sacrifice':
            self.pay_activation('source',(1,1,0,0,0,0),target='victim',sacrifice=True)
            self.cards['victim'][2]=GRAVE
            self.stats('source',4)
            self.see('source','plus1_counters',2)
        elif m.startswith('intimidate_'):
            self.cycle()
            self.start_combat('source',blocker='wall',legal=0)
            self.damage(2)
            self.state()
            self.cycle()
            self.start_combat('source',blocker='legal')
            self.damage(0)
            self.see('legal','damage',2)
        self.depart(0)
        self.generated(HORROR,1,4 if m=='sacrifice' else 2)
        self.state()

    def slavedriver(self):
        p,m = self.actor,self.mode
        if not m.startswith('unearth'):
            self.cast('source')
            self.depart(0,gain=4)
            self.generated(ZOMBIE,1,2)
            self.state()
            return
        self.emit('prepare_graveyard_abilities')
        self.emit('present_ai_dialogs',value=1)
        self.funds(p,(3,1,0,0,0,0))
        self.emit('require_can_activate_graveyard','source',value=1)
        self.emit('choose_dialog',player=p,value=1)
        self.emit('activate_graveyard','source',value=1)
        self.emit('assert_stack',value=1)
        self.mana[p]=ZERO
        self.wallet(p)
        self.emit('assert_zone_multiplicity',player=p,zone=GRAVE,index=self.id,value=1)
        self.settle()
        self.cards.pop('source')
        self.bind(self.name,'returned')
        self.stats('returned',4)
        self.start_combat('returned')
        self.damage(4)
        if m=='unearth_bounce':
            self.cast('removal_0','returned')
        else:
            self.emit('cleanup')
            self.settle()
        self.cards['returned'][2]=EXILE
        self.generated(ZOMBIE,1,2)
        self.state()

    def borderguard(self):
        m = self.mode
        own_kins = sum(ref in self.cards for ref in ('kin','second_kin'))
        self.cast('source')
        self.see('source','plus1_counters',own_kins)
        if m=='additional_counter':
            self.cast('armament','source')
            self.see('source','plus1_counters',own_kins+1)
        self.depart(0)
        count = 0 if m=='bounce' else own_kins+int(m=='additional_counter')
        self.generated(KITHKIN,count,1)
        self.state()

    def liege(self):
        p,q,m = self.actor,self.foe,self.mode
        self.cast('source')
        paid = [0,0]
        paid[p] = 2 if m in ('own_pay','both_pay') else 0
        paid[q] = 1 if m in ('foe_pay','both_pay') else 0
        if paid[p]:
            self.funds(p,(paid[p],0,0,0,0,0))
        def payments():
            self.funds(q,(paid[q],0,0,0,0,0))
            for seat in (p,q):
                if paid[seat]:
                    self.emit('choose_x',player=seat,value=paid[seat])
            self.mana=[ZERO,ZERO]
        self.cast('removal_0','source',after_cast=payments)
        self.cards['source'][2]=GRAVE
        for seat in (0,1):
            self.generated(SQUIRREL,paid[seat],1,player=seat)
        self.state()

    def walker(self):
        m = self.mode
        if m=='evoke':
            self.cast('source',fee=(4,0,0,1,0,0))
            self.cards['source'][2]=GRAVE
        else:
            self.cast('source',mode=0)
            self.stats('source',7)
            if m in ('bounce','exile'):
                self.depart(0,gain=7)
        self.generated(ELEMENTAL,int(m!='normal_stays'),4)
        self.state()

    def worldspine(self):
        p,q,m = self.actor,self.foe,self.mode
        assert self.oracle[self.name]['mana_cost']=='{8}{G}{G}{G}'
        fee = (8,0,0,3,0,0)
        if m=='countered':
            self.funds(p,fee)
            self.funds(q,cost_from_oracle(self.oracle[COUNTERSPELL]))
            self.emit('cast','source',value=1)
            self.emit('announce_spell','source')
            self.emit('cast_response','counter',value=1)
            if p==0:
                self.emit('choose_dialog',player=0,value=0)
            self.emit('resolve_response')
            self.settle()
            self.mana=[ZERO,ZERO]
            self.cards['counter'][2]=GRAVE
        else:
            self.cast('source',fee=fee)
            self.stats('source',15)
            if p==0 and m!='exile':
                self.cast('removal_0','source',after_cast=lambda: self.emit('choose_dialog',player=0,value=0))
            else:
                self.depart(0,gain=15)
        if m!='exile':
            self.cards['source'][2]=LIBRARY
        qty = 3 if m in ('destroy','trample') else 0
        self.generated(WURM,qty,5)
        self.emit('assert_shuffle_count',player=p,value=int(m!='exile'))
        self.state()
        if m=='trample':
            self.bind(WURM,'wurm')
            self.cycle()
            self.start_combat('wurm',blocker='wall')
            self.damage(1)
            self.cards['wall'][2]=GRAVE
            self.cards.pop('wurm')
            self.state()

    def wurmcoil(self):
        p,q,m = self.actor,self.foe,self.mode
        self.cast('source')
        self.stats('source',6)
        if m=='source_combat':
            self.cycle()
            self.start_combat('source',blocker='wall')
            self.damage(0)
            self.life[p]+=6
            self.cards['wall'][2]=GRAVE
            self.see('source','damage',0)
        else:
            self.depart(0)
            if m=='token_combat':
                self.generated(WURM,2,3,color=0,artifact=True)
                self.bind(WURM,'touch')
                self.bind(WURM,'link',index=1)
                self.cycle()
                self.start_combat('touch',blocker='wall')
                self.damage(0)
                self.cards['wall'][2]=GRAVE
                self.cycle()
                self.start_combat('link')
                self.damage(3)
                self.life[p]+=3
                for ref in ('touch','link'):
                    self.cards.pop(ref)
        self.state()

    def emissary(self):
        p,m = self.actor,self.mode
        self.cast('source')
        self.depart(0)
        if m.startswith('manifest_'):
            self.cards.pop('top')
            self.bind(FACEDOWN,'face')
            self.stats('face',2)
            for prop,value in (('token',0),('color',0),('artifact',0)):
                self.emit('assert_matching_permanents',prop=prop,player=p,value=value,index=FACE_CSVID)
            if m=='manifest_creature':
                self.funds(p,cost_from_oracle(self.oracle[GIANT]))
                self.emit('can_activate_granted_legacy','face',index=self.id,value=1)
                self.emit('activate_granted_legacy','face',index=self.id,value=1)
                self.mana[p]=ZERO
                self.wallet(p)
                self.settle()
                self.cards.pop('face')
                self.bind(GIANT,'turned')
                self.stats('turned',3)
            else:
                self.funds(p,(4,0,0,0,0,0))
                self.emit('can_activate_granted_legacy','face',index=self.id)
                self.wallet(p)
        self.state()

    def sire(self):
        p,q,m = self.actor,self.foe,self.mode
        self.cast('source')
        if m=='flying':
            self.check_flying(3)
            return
        def choices():
            if p==0:
                self.emit('choose_trigger','source',player=p)
            if m=='animate':
                self.emit('choose_target','own_land',player=p)
        self.cast('removal_0','source',after_cast=choices)
        self.cards['source'][2]=GRAVE
        self.see('enemy_land','plus1_counters',0)
        if m=='animate':
            self.stats('own_land',3)
            self.see('own_land','plus1_counters',3)
            self.emit('assert_matching_permanents',prop='subtype_Elemental',player=p,value=1,
                      index=self.rows[FOREST]['csvid'])
            self.start_combat('own_land')
            self.damage(3)
            self.cycle()
            self.see('own_land','plus1_counters',3)
            self.stats('own_land',3)
            self.visit(q)
            self.cast('land_removal','own_land')
            self.cards['own_land'][2]=GRAVE
        self.state()

    def guardian(self):
        p,q,m = self.actor,self.foe,self.mode
        self.cast('source')
        if m=='reach':
            self.visit(q)
            self.start_combat('flier',blocker='source')
            self.damage(0)
            self.cards['source'][2]=GRAVE
            self.see('flier','damage',3)
        else:
            self.depart(0,gain=3)
        qty = int(m!='exile')
        self.generated(ELEMENTAL,qty,1+int(m=='creature_count'))
        self.state()

    def courier(self):
        p,m = self.actor,self.mode
        self.cast('source')
        self.depart(0)
        if m=='bounce':
            self.generated(CLUE,0,None,artifact=True)
        else:
            self.generated(CLUE,1,None,artifact=True)
        if m=='crack':
            self.bind(CLUE,'clue')
            self.emit('capture')
            replay_start = len(self.steps)
            self.funds(p,(1,0,0,0,0,0))
            self.emit('can_activate','clue',value=0)
            self.wallet(p)
            self.pay_activation('clue',(2,0,0,0,0,0))
            self.cards.pop('clue')
            self.cards.pop(f'{CLUE}_{p}_0')
            self.cards['drawn'][2]=HAND
            self.emit('assert_zone_card',player=p,zone=LIBRARY,value=self.rows[PLAINS]['csvid'])
            self.state()
            paid_draw = self.steps[replay_start:]
            self.emit('record_state')
            self.emit('ai_roundtrip')
            self.emit('restore')
            self.steps.extend(paid_draw)
            self.emit('assert_recorded_state')
            return
        self.state()

    def angel(self):
        p,q,m = self.actor,self.foe,self.mode
        self.cast('source')
        if m=='flying':
            self.check_flying(5)
            return
        self.depart(0,gain=5)
        if m=='exile':
            self.generated(CLERIC,0,1)
        else:
            self.generated(CLERIC,1,1)
        if m in ('return','no_angel'):
            self.bind(CLERIC,'cleric')
            if m=='no_angel':
                self.emit('choose_player',player=q,value=p)
                self.emit('activate','crypt',value=1)
                self.settle()
                self.cards['crypt'][2]=GRAVE
                self.cards['source'][2]=EXILE
            fee = cost_from_oracle(self.oracle[self.name])
            self.funds(p,fee)
            self.emit('can_activate','cleric',value=0)
            self.wallet(p)
            self.funds(p)
            self.cycle()
            self.pay_activation('cleric',fee)
            self.cards.pop('cleric')
            self.cards.pop(f'{CLERIC}_{p}_0')
            if m=='return':
                self.cards.pop('source')
                self.bind(self.name,'returned')
                self.stats('returned',5)
        self.state()

    def run(self):
        self.emit('turn',player=self.actor)
        self.funds(self.foe)
        self.funds(self.actor)
        method = {CHRONO:self.chronozoa,CARVER:self.carver,SLAVEDRIVER:self.slavedriver,
                  BORDER:self.borderguard,LIEGE:self.liege,WALKER:self.walker,WORLDSPINE:self.worldspine,
                  WURMCOIL:self.wurmcoil,EMISSARY:self.emissary,SIRE:self.sire,GUARDIAN:self.guardian,
                  COURIER:self.courier,ANGEL:self.angel}
        method[self.id]()


ZONES = ('hand','battlefield','library','graveyard','exile')
ZONE_OPS = {'assert_zone','assert_zone_multiplicity','assert_zone_card'}
INDEXED = {'assert_zone_multiplicity','assert_zone_card','assert_matching_permanents','bind_permanent_copy',
           'can_activate_granted_legacy','activate_granted_legacy'}
SEATED = ZONE_OPS | {'set_mana','assert_mana','assert_life','assert_shuffle_count','choose_player','choose_dialog',
                     'choose_x','choose_trigger','choose_target','turn','untap_step','upkeep_step','draw_step',
                     'assert_matching_permanents','bind_permanent_copy'}
VALUED = ZONE_OPS | INDEXED | {'set_mana','assert_mana','assert_life','assert_shuffle_count','choose_player',
                               'choose_dialog','choose_x','turn','assert_stack','assert_card','assert_counter',
                               'assert_unused_choices','cast','cast_response','can_activate','activate',
                               'present_ai_dialogs','attack','block','activate_graveyard','require_can_activate_graveyard'}

GAPS = {
    SLAVEDRIVER: ['Unearth denial during another player’s turn or while the stack is nonempty.'],
    WALKER: ['No positive evoke choice when both prices are affordable.'],
    WORLDSPINE: ['Graveyard arrival from a discarded or milled card, and shuffle position in a larger library.'],
    WURMCOIL: ['Identical token CSV identities use generation order to distinguish their combat abilities.'],
    ANGEL: ['The direct subtype observer does not expose Cleric. Its generated card identity, color, and activated return are checked.'],
}


def operation(row):
    op,ref,other,prop,player,value,zone,index,color = row
    result = dict(op=op)
    if ref:
        result['ref'] = ref
    if other:
        result['other'] = other
    if prop:
        result['counter_kind' if op=='assert_counter' else 'property'] = prop
    if op in SEATED:
        result['player'] = player
    if op in VALUED:
        result['value'] = value
    if op in ZONE_OPS:
        result['zone'] = ZONES[zone]
    if op in INDEXED:
        result['index'] = index
    if op in ('set_mana','assert_mana'):
        result['color'] = color
    return result


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def exact(actual, pinned):
    return json.dumps(actual,sort_keys=True,ensure_ascii=False)==json.dumps(pinned,sort_keys=True,ensure_ascii=False)


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (not pin or not exact(record,pin['oracle']) or csvid not in ALL_MODES
            or pin['registry']['csvid']!=csvid or record.get('layout')!='normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole pinned normal departure creature required')
    return dict(csvid=csvid,mana_cost=record['mana_cost'])


def variants(shape):
    return [f'{mode}_{seat}' for mode in ALL_MODES[shape['csvid']] for seat in (0,1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(shape))):
        raise ValueError('distinct supported departure variants required')


def spec(declaration,record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned departure primary required')
    shape = classify(record,pin['registry']['csvid'])
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not exact(declaration.get('params'),shape)):
        raise ValueError('exact declared departure parameters required')
    validate_variants(shape,declaration.get('variants'))
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    pinned = profiles()
    if (set(pinned)!={'sources','witnesses'} or set(pinned['witnesses'])!=WITNESSES
            or {pin['registry']['csvid'] for pin in pinned['sources'].values()}!=ALL_MODES.keys()):
        raise ValueError('complete departure source and witness pins required')
    for group,entries in pinned.items():
        for name,pin in entries.items():
            record,row = oracle.get(name),rows.get(name,{})
            if not exact(record,pin['oracle']) or not exact(row,pin['registry']):
                raise ValueError('departure source or witness changed: '+name)
            if group=='sources':
                if row.get('kind')!='card' or row.get('handler') in (None,'0','NULL','card_dummy'):
                    raise ValueError('implemented departure primary required')
                classify(record,row['csvid'])
            elif name in TOKENS or name in FOLLOW_TOKENS:
                token_row = TOKENS[name] if name in TOKENS else FOLLOW_TOKENS[name]
                if row['csvid']!=token_row[0] or record['layout']!='token':
                    raise ValueError('exact generated token identity required: '+name)
            elif name==FACEDOWN and (record is not None or row['csvid']!=FACE_CSVID):
                raise ValueError('exact face-down permanent identity required')


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid']!=csvid or shape['csvid']!=csvid:
        raise ValueError('exact registered departure source required')
    validate_variants(shape,selected)
    validate_witnesses(oracle,rows)
    result = []
    names = {rows[n]['csvid']:n for n in WITNESSES|{name}}
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        board = (FollowBoard if csvid in FOLLOW else Board)(rows[name],mode,int(seat),rows,oracle)
        board.run()
        setup = [dict(name=names[cid],ref=ref,player=player,zone=ZONES[zone])
                 for ref,player,zone,cid,flags in board.setup]
        result.append(dict(name=label(csvid,mode,seat),card=name,fixture='empty_main',seed=SEED,
                           setup=setup,steps=[operation(t) for t in board.steps],
                           _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                                        oracle_text=declaration['oracle_text'],untested_components=GAPS.get(csvid,[]))))
    return result
