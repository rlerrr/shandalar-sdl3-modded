"""Real upkeep triggers, scoped counters and their printed payouts."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'upkeep_counters'
PROFILE = Path(__file__).with_name('data') / 'upkeep_counters_wave8_profiles.json'
AZOR, CODEX, TYRANT, OOZE, BOMB, CHISEI, WAR = 12665,3702,10115,10116,5494,7451,11723
CROVAX, CYCLONE, REACTOR, SHIELD, DESCENDANT, DESCENT, DIRGE = 3358,405,6875,5535,7625,12347,3747
TWIDDLE, SHATTER, EMPERION, ORNITHOPTER, AXE = 'Twiddle','Shatter','Platinum Emperion','Ornithopter','Lava Axe'
ZOMBIE, LEECH, DARKSTEEL_MYR = 'Walking Corpse','Putrid Leech','Darksteel Myr'
SPHERE, BEETLE, WALL = 'Sphere of the Suns','Clockwork Beetle','Dazzling Ramparts'

VIAL, GROVE, DJINN, EVE = 6856,2388,2669,562
MODES = {
    VIAL:('zero','ramp','decline_counter','decline_creature','miss','offturn','countered'),
    GROVE:('lifecycle','foreign','empty','later_target','countered'),
    DJINN:('both_turns','repeated','empty','trample','removed','countered'),
    EVE:('both_graves','one_side','empty','replay','countered'),
}
ZONES = {'hand':0,'battlefield':1,'library':2,'graveyard':3,'exile':4}
ZERO = [0]*6
COUNTER, MURDER, BOLT = 'Counterspell','Murder','Lightning Bolt'
ARTIFACT, BEAR, SPIDER = 'Millstone','Grizzly Bears','Giant Spider'
MEMNITE, ELF, GIANT, LAND = 'Memnite','Elvish Mystic','Hill Giant','Forest'
FOLLOWERS = {12665: ('white', 'blue', 'setback', 'opposing_hit'), 3702: ('zero', 'one', 'two', 'decline', 'tapped'), 10115: ('repeat', 'locked', 'combat', 'terminal'), 10116: ('none', 'green', 'black', 'both', 'partial', 'decline', 'dual'), 5494: ('opposing', 'own', 'indestructible'), 7451: ('none', 'foreign', 'charge', 'flying'), 11723: ('own_attack', 'opposing_attack'), 3358: ('paid', 'decline', 'flying'), 405: ('unpaid', 'short', 'scale'), 6875: ('one', 'decline', 'threshold', 'indestructible'), 5535: ('unpaid', 'partial', 'opposing_damage'), 7625: ('ahead', 'behind', 'equal', 'growth'), 12347: ('permanent', 'repeat', 'mixed', 'hand_only'), 3747: ('zero', 'one', 'two', 'decline')}
MODES.update(FOLLOWERS)
HUMAN_ONLY = [(3702, 'decline'), (3747, 'decline'), (6856, 'decline_creature'), (6875, 'decline'), (10116, 'decline'), (10116, 'partial')]
WITNESSES = frozenset(('Clockwork Beetle', 'Counterspell', 'Darksteel Myr', 'Dazzling Ramparts', 'Elvish Mystic', 'Forest', 'Giant Spider', 'Grizzly Bears', 'Hill Giant', 'Island', 'Lava Axe', 'Lightning Bolt', 'Memnite', 'Millstone', 'Mountain', 'Murder', 'Ornithopter', 'Plains', 'Platinum Emperion', 'Putrid Leech', 'Shatter', 'Sphere of the Suns', 'Swamp', 'Twiddle', 'Walking Corpse'))
ZONE_NAMES = {code:name for name,code in ZONES.items()}


GAPS = {
    EVE: ('Scream counters are inferred through exact exile duration and the two-owner-upkeep reanimation boundary.',),
    AZOR: ('Filibuster totals are measured through the real fifth qualifying upkeep and damage setback.',),
    CODEX: ('Page totals are measured through actual paid sacrifices and exact drawn identities.',),
    BOMB: ('Fuse totals are measured through the exact detonation upkeep and aftermath.',),
    WAR: ('Strife totals are measured in real attack and block combat power.',),
    CYCLONE: ('Wind totals are measured through exact green payment and damage; an AI player can decline.',),
    SHIELD: ('Delay totals are measured in exact damage replaced and eventual life loss after payment.',),
    DESCENT: ('Despair totals are measured via the exact number of real exile selections.',),
    DIRGE: ('Verse totals are measured through exact paid discard counts; hand inspection is represented by the actual hand selector.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record,csvid):
    if csvid not in MODES or record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces'):
        raise ValueError('whole normal upkeep primary required')
    return dict(csvid=csvid,printed_cost=record['mana_cost'])


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{seat}' for seat in (0,1) for mode in MODES[csvid]
            if seat==0 or (csvid,mode) not in HUMAN_ONLY]


def validate_variants(shape,chosen):
    if (type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(set(chosen))!=len(chosen) or set(chosen)-set(variants(shape))):
        raise ValueError('distinct supported upkeep modes required')


def spec(declaration,record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'],record):
        raise ValueError('exact pinned upkeep source required')
    shape = classify(record,pin['registry']['csvid'])
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not exact(declaration.get('params'),shape)):
        raise ValueError('exact typed upkeep declaration required')
    validate_variants(shape,declaration.get('variants'))
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    pins = profiles()
    if (set(pins)!={'sources','witnesses'} or set(pins['witnesses'])!=WITNESSES
            or {p['registry']['csvid'] for p in pins['sources'].values()}!=MODES.keys()):
        raise ValueError('complete upkeep pins required')
    for group,items in pins.items():
        for name,pin in items.items():
            row = rows.get(name,{})
            if (not exact(oracle.get(name),pin['oracle']) or not exact(row,pin['registry'])
                    or row.get('kind')!='card' or row.get('handler') in (None,'0','NULL')
                    or oracle[name].get('layout')!='normal'):
                raise ValueError('upkeep Oracle or registry changed: '+name)
            if group=='sources':
                if row['handler']=='card_dummy':
                    raise ValueError('implemented upkeep primary required')
                classify(oracle[name],row['csvid'])


def expand(name,csvid,declaration,shape,chosen,rows,oracle):
    validate_variants(shape,chosen)
    validate_witnesses(oracle,rows)
    if shape['csvid']!=csvid or profiles()['sources'][name]['registry']['csvid']!=csvid:
        raise ValueError('exact upkeep primary CSVID required')
    yield_cases = []
    for variant in chosen:
        mode,seat = variant.rsplit('_',1)
        board = Board(rows[name],int(seat),mode,rows,oracle)
        board.run()
        yield_cases.append(dict(name=f'UpkeepCountersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                                seed=3686,setup=board.setup,steps=board.steps,
                                _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                                             oracle_text=declaration['oracle_text'],untested_components=list(GAPS.get(csvid,())))))
    return yield_cases

class Board:
    def __init__(self,record,owner,mode,rows,oracle):
        self.id,self.name = record['csvid'],record['name']
        self.owner,self.foe,self.mode = owner,1-owner,mode
        self.rows,self.oracle = rows,oracle
        self.setup,self.steps,self.objects = [],[],{}
        self.active = owner
        self.life = [20,20]
        self.add('source',self.name,owner,'hand')
        if self.mode == 'countered':
            self.add('counter',COUNTER,self.foe,'hand')
        self.witnesses() if self.id not in FOLLOWERS else self.followers_setup()

    def add(self,ref,name,seat,zone='battlefield'):
        assert ref not in self.objects
        self.setup.append(card(name,ref,seat,zone))
        self.objects[ref] = dict(name=name,player=seat,zone=zone)

    def emit(self,op,ref='',other='',prop='',*,player=0,value=0,zone=0,index=0,color=0):
        if op in ('untap_step','upkeep_step','choose_done_target'):
            self.steps.append(step(op,player=player))
            return
        if op in ('end_combat_step','advance_scheduled_turn','assert_forced_zone_choices'):
            self.steps.append(step(op,value=value))
            return
        if op == 'choose_mana_source':
            self.steps.append(step(op,ref=ref))
            return
        if op == 'assert_counter':
            self.steps.append(step(op,ref=ref,counter_kind=prop,value=value))
            return
        values = dict(player=player,value=value,index=index,color=color)
        if ref:
            values['ref'] = ref
        if other:
            values['other'] = other
        if prop:
            values['property'] = prop
        if op.startswith('assert_zone'):
            values['zone'] = ZONE_NAMES[zone]
        self.steps.append(step(op,**values))

    def observe(self,ref,prop,value):
        self.emit('assert_card',ref,prop=prop,value=value)

    def count(self,ref,value,kind='plus1'):
        self.emit('assert_counter',ref,prop=kind,value=value)

    def stats(self,ref,boost=0):
        record = self.oracle[self.objects[ref]['name']]
        for stat in ('power','toughness'):
            self.observe(ref,stat,int(record[stat])+boost)

    def move(self,ref,zone):
        self.objects[ref]['zone'] = zone

    def pool(self,seat,vals=ZERO,*,fund=False):
        for color,value in enumerate(vals):
            self.emit('set_mana' if fund else 'assert_mana',player=seat,value=value,color=color)
        if fund:
            self.emit('refresh')

    def choices(self):
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_choices_consumed')

    def receipt(self):
        self.emit('assert_stack')
        for seat in (0,1):
            self.emit('assert_life',player=seat,value=self.life[seat])
            self.pool(seat)
            for zone,num in ZONES.items():
                counts = Counter(item['name'] for item in self.objects.values()
                                 if item['zone']==zone and item['player']==seat)
                self.emit('assert_zone',player=seat,zone=num,value=counts.total())
                for name,count in sorted(counts.items()):
                    self.emit('assert_zone_multiplicity',player=seat,zone=num,index=self.rows[name]['csvid'],value=count)

    def pending(self,ref,*,response=False,target=None,target_player=None,printed=None):
        item = self.objects[ref]
        fee = self.oracle[item['name']] if printed is None else {'mana_cost':printed}
        self.pool(item['player'],cost_from_oracle(fee),fund=True)
        if target:
            self.emit('choose_target',target,player=item['player'])
        if target_player is not None:
            self.emit('choose_player',player=item['player'],value=target_player)
        self.emit('cast_response' if response else 'cast',ref,value=1)
        self.choices()
        self.emit('assert_stack',value=2 if response else 1)
        self.pool(item['player'])

    def settle(self,*,response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def cast(self,ref,zone,*,target=None,printed=None):
        self.pending(ref,target=target,printed=printed)
        self.settle()
        self.move(ref,zone)

    def countered(self):
        self.pending('source')
        self.emit('announce_spell','source')
        self.pending('counter',response=True)
        self.settle(response=True)
        self.move('source','graveyard')
        self.move('counter','graveyard')
        self.go(self.foe)
        self.go(self.owner)
        self.receipt()

    def go(self,seat,*,target=None,dialog=None,before=None,terminal=False):
        assert seat==1-self.active
        self.emit('advance_turn')
        self.emit('untap_step',player=seat)
        if target:
            self.emit('choose_target',target,player=seat)
        if dialog is not None:
            self.emit('choose_dialog',player=seat,value=dialog)
        if before:
            before()
        self.emit('upkeep_step',player=seat)
        self.choices()
        self.emit('assert_stack')
        if not terminal:
            self.emit('turn',player=seat)
        self.active = seat

    def witnesses(self):
        p,q,mode = self.owner,self.foe,self.mode
        if self.id==VIAL:
            self.add('one',ELF,p,'hand')
            self.add('two',BEAR,p,'hand')
            self.add('not_creature',ARTIFACT,p,'hand')
            self.add('wrong',GIANT,p,'hand')
            if mode in ('zero','decline_counter'):
                self.add('zero',MEMNITE,p,'hand')
        elif self.id==GROVE:
            if mode in ('lifecycle','foreign','countered'):
                self.add('home',BEAR,p)
                self.add('away',SPIDER,q)
            if mode=='later_target':
                self.add('later',BEAR,p,'hand')
            self.add('noncreature',ARTIFACT,q)
        elif self.id==DJINN:
            self.add('home',BEAR,p)
            self.add('noncreature',ARTIFACT,q)
            if mode!='empty':
                self.add('first',BEAR,q)
                if mode!='trample':
                    self.add('second',ELF,q)
            if mode=='removed':
                self.add('removal',MURDER,q,'hand')
        else:
            self.add('own_noncreature',BOLT,p,'graveyard')
            self.add('foe_noncreature',LAND,q,'graveyard')
            if mode in ('both_graves','one_side','replay','countered'):
                self.add('own_creature',BEAR,p,'graveyard')
                self.add('own_second',SPIDER,p,'graveyard')
            if mode in ('both_graves','replay','countered'):
                self.add('foe_creature',GIANT,q,'graveyard')

    def vial_activation(self,target=None,*,decline=False):
        empty = self.mode=='miss'
        if empty:
            self.emit('observe_search_filters',value=1)
        self.emit('can_activate','source',value=1)
        self.emit('activate','source',value=1)
        self.choices()
        self.emit('assert_stack',value=1)
        self.observe('source','tapped',1)
        if target:
            cid = self.rows[self.objects[target]['name']]['csvid']
            self.emit('choose_zone_card',player=self.owner,value=cid)
        if decline or empty:
            self.emit('choose_zone',player=self.owner,value=-1)
        self.settle()
        if target:
            self.move(target,'battlefield')
            self.stats(target)
        self.emit('can_activate','source',value=0)
        self.receipt()
        if empty:
            self.emit('assert_offered_count',value=4)
            self.emit('assert_offered_selection_required',value=0)
            for ref in ('one','two','not_creature','wrong'):
                self.emit('assert_offered_legal_card',index=self.rows[self.objects[ref]['name']]['csvid'],value=0)
            self.emit('observe_search_filters',value=0)

    def vial(self):
        self.cast('source','battlefield')
        self.count('source',0,'charge')
        self.receipt()
        if self.mode in ('zero','miss'):
            self.vial_activation('zero' if self.mode=='zero' else None)
            self.count('source',0,'charge')
            return
        self.go(self.foe)
        self.count('source',0,'charge')
        decline_count = self.mode=='decline_counter'
        self.go(self.owner,dialog=int(decline_count))
        self.count('source',int(not decline_count),'charge')
        if self.mode=='offturn':
            self.go(self.foe)
            self.count('source',1,'charge')
        if decline_count:
            self.vial_activation('zero')
        elif self.mode=='decline_creature':
            self.vial_activation(decline=True)
        else:
            self.vial_activation('one')
        if self.mode=='ramp':
            self.go(self.foe)
            self.count('source',1,'charge')
            self.go(self.owner,dialog=0)
            self.count('source',2,'charge')
            self.vial_activation('two')

    def grove(self):
        self.cast('source','battlefield')
        self.count('source',3)
        self.go(self.foe)
        self.count('source',3)
        self.receipt()
        if self.mode in ('empty','later_target'):
            self.go(self.owner)
            self.count('source',3)
            if self.mode=='later_target':
                self.cast('later','battlefield')
                self.go(self.foe)
                self.count('source',3)
                self.go(self.owner,target='later')
                self.count('source',2)
                self.count('later',1)
                self.stats('later',1)
            self.receipt()
            return
        plan = ('away',) if self.mode=='foreign' else ('home','away','home')
        receipts = Counter()
        for index,target in enumerate(plan,1):
            self.go(self.owner,target=target)
            receipts[target] += 1
            for ref in ('home','away'):
                self.count(ref,receipts[ref])
                self.stats(ref,receipts[ref])
            self.count('noncreature',0)
            if index<3:
                self.count('source',3-index)
            else:
                self.move('source','graveyard')
            self.receipt()
            if index<len(plan):
                self.go(self.foe)
                self.count('source',3-index)
        if len(plan)==3:
            self.go(self.foe)
            self.go(self.owner)
            self.count('home',2)
            self.count('away',1)
            self.receipt()

    def djinn_board(self,count):
        self.count('home',0)
        self.stats('home')
        self.count('noncreature',0)
        for ref in ('first','second'):
            if ref in self.objects and self.objects[ref]['zone']=='battlefield':
                self.count(ref,count)
                self.stats(ref,count)
        self.receipt()

    def djinn(self):
        self.cast('source','battlefield')
        self.stats('source')
        self.djinn_board(0)
        self.go(self.foe)
        self.djinn_board(0)
        self.go(self.owner)
        self.djinn_board(1)
        if self.mode in ('repeated','removed'):
            if self.mode=='removed':
                self.cast('removal','graveyard',target='source')
                self.move('source','graveyard')
            self.go(self.foe)
            self.djinn_board(1)
            self.go(self.owner)
            self.djinn_board(1 if self.mode=='removed' else 2)
        if self.mode=='trample':
            self.emit('begin_attackers')
            self.emit('attack','source',value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block','first','source',value=1)
            self.emit('advance_combat')
            self.observe('source','blocked',1)
            self.emit('combat_damage')
            self.settle()
            self.emit('end_combat_step')
            self.settle()
            self.life[self.foe] -= 2
            self.move('first','graveyard')
            self.observe('source','damage',3)
            self.djinn_board(1)

    def eve(self):
        self.cast('source','exile')
        self.receipt()
        self.go(self.foe)
        self.receipt()
        self.go(self.owner)
        self.receipt()
        if self.mode=='replay':
            self.emit('capture')
            saved = {key:item.copy() for key,item in self.objects.items()}
            self.eve_resolution()
            self.emit('restore')
            self.objects = saved
            self.receipt()
        self.eve_resolution()
        self.go(self.foe)
        self.go(self.owner)
        self.receipt()

    def eve_resolution(self):
        self.go(self.foe)
        self.receipt()
        self.go(self.owner)
        self.move('source','graveyard')
        for ref in ('own_creature','own_second','foe_creature'):
            if ref in self.objects:
                self.move(ref,'battlefield')
        self.receipt()

    def followers_setup(self):
        p,q,mode = self.owner,self.foe,self.mode
        if self.id==AZOR and mode in ('setback','opposing_hit'):
            self.add('burn',BOLT,q,'hand')
        if self.id==CODEX:
            for i,name in enumerate(('Island','Swamp','Mountain','Forest')):
                self.add(f'library_{i}',name,p,'library')
            if mode=='tapped':
                self.add('tap_spell',TWIDDLE,q,'hand')
        if self.id==TYRANT:
            if mode=='locked':
                self.add('lock',EMPERION,q)
            if mode=='combat':
                self.add('ground',BEAR,q)
                self.add('flyer',ORNITHOPTER,q)
            if mode=='terminal':
                for i in range(3):
                    self.add(f'axe_{i}',AXE,p,'hand')
                self.add('burn',BOLT,p,'hand')
        if self.id==OOZE:
            if mode in ('black','both','partial','decline'):
                self.add('black',ZOMBIE,p)
            if mode in ('green','both','partial','decline'):
                self.add('green',BEAR,p)
            if mode=='dual':
                self.add('dual',LEECH,p)
            self.add('enemy_black',ZOMBIE,q)
            self.add('enemy_green',BEAR,q)
        if self.id==BOMB:
            self.add('victim',DARKSTEEL_MYR if mode=='indestructible' else BEAR,
                     p if mode=='own' else q)
            self.add('unmarked',BEAR,q)
        if self.id==CHISEI:
            if mode in ('charge','flying'):
                self.add('sphere',SPHERE,p,'hand')
            if mode=='foreign':
                self.add('foe_beetle',BEETLE,q,'hand')
            if mode=='flying':
                self.add('blocker',BEAR,q)
        if self.id==WAR:
            self.add('attacker',BEAR,p if mode=='own_attack' else q)
            self.add('blocker',WALL,q if mode=='own_attack' else p)
            self.add('idle',BEAR,1-self.objects['attacker']['player'])
        if self.id==CROVAX:
            if mode=='paid':
                self.add('food',BEAR,p)
                if p==1:
                    self.add('other_food',WALL,p)
            if mode=='flying':
                self.add('blocker',BEAR,q)
        if self.id==CYCLONE:
            if mode!='unpaid':
                for i in range(2 if mode=='scale' else 1):
                    self.add(f'land_{i}',LAND,p)
            if mode=='scale':
                self.add('large',WALL,p)
                self.add('small',BEAR,q)
        if self.id==REACTOR and mode=='indestructible':
            self.add('shatter',SHATTER,q,'hand')
        if self.id==SHIELD:
            self.add('burn',BOLT,q,'hand')
            if mode=='partial':
                self.add('island','Island',p)
                self.add('plains','Plains',p)
        if self.id==DESCENDANT:
            own,foe = {'ahead':(3,1),'behind':(1,3),'equal':(2,2),'growth':(2,0)}[mode]
            for seat,count in ((p,own),(q,foe)):
                for i in range(count):
                    self.add(f'hand_{seat}_{i}',LAND,seat,'hand')
        if self.id==DESCENT:
            for seat in (0,1):
                self.add(f'held_{seat}',LAND,seat,'hand')
                if mode!='hand_only' or seat==p:
                    self.add(f'first_{seat}',ARTIFACT,seat)
                if mode=='repeat':
                    self.add(f'second_{seat}',LAND,seat)
                    self.add(f'third_{seat}',BEAR,seat)
        if self.id==DIRGE:
            for i,name in enumerate(('Island','Swamp',BEAR)):
                self.add(f'held_{i}',name,q,'hand')

    def own_next(self,*,choices=None,terminal=False):
        assert self.active==self.owner
        self.go(self.foe)
        self.go(self.owner,before=choices,terminal=terminal)

    def optional(self,times=1):
        if self.owner==0:
            for _ in range(times):
                self.emit('choose_trigger','source',player=0)

    def burn(self,ref,player):
        item = self.objects[ref]
        amount = 5 if item['name']==AXE else 3
        self.pending(ref,target_player=player)
        self.settle()
        self.move(ref,'graveyard')
        return amount

    def win_receipt(self):
        self.emit('advance_scheduled_turn',value=-1)

    def activate(self,amount=ZERO,target=None):
        self.pool(self.owner,amount,fund=True)
        self.emit('can_activate','source',value=1)
        if target:
            self.emit('choose_target',target,player=self.owner)
        self.emit('activate','source',value=1)
        self.choices()
        self.emit('assert_stack',value=1)
        self.pool(self.owner)

    def basic_battle(self,source,blocker=None,*,deny=None,attack_power=None,block_power=None,hit=0):
        self.emit('begin_attackers')
        self.emit('attack',source,value=1)
        self.emit('finish_attackers')
        if attack_power is not None:
            self.observe(source,'power',attack_power)
        self.emit('begin_blockers')
        if deny:
            self.emit('block',deny,source,value=0)
        if blocker:
            self.emit('block',blocker,source,value=1)
        self.emit('advance_combat')
        self.observe(source,'blocked',int(bool(blocker)))
        if block_power is not None:
            self.observe(blocker,'power',block_power)
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat_step')
        self.settle()
        self.life[1-self.active] -= hit

    def azor(self):
        fee = '{3}{U}{U}' if self.mode=='blue' else '{3}{W}{W}'
        self.cast('source','battlefield',printed=fee)
        self.stats('source')
        setback = int(self.mode=='setback')
        for count in range(1,6+setback):
            self.own_next(terminal=count==5+setback)
            if count<5+setback:
                self.receipt()
            if count==2 and self.mode in ('setback','opposing_hit'):
                recipient = self.owner if setback else self.foe
                self.life[recipient] -= self.burn('burn',recipient)
                self.receipt()
        self.win_receipt()

    def codex(self):
        self.cast('source','battlefield')
        paid_pages = {'zero':0,'one':1,'two':2,'decline':0,'tapped':1}[self.mode]
        for _ in range(2 if self.mode=='two' else int(self.mode!='zero')):
            self.own_next(choices=None if self.mode=='decline' else self.optional)
            self.receipt()
        fee = [4,0,0,0,0,0]
        if self.mode=='tapped':
            self.pending('tap_spell',target='source')
            self.emit('choose_dialog',player=self.foe,value=0)
            self.settle()
            self.move('tap_spell','graveyard')
            self.observe('source','tapped',1)
            self.pool(self.owner,fee,fund=True)
            self.emit('can_activate','source',value=0)
            self.pool(self.owner,ZERO,fund=True)
            self.receipt()
            return
        self.activate(fee)
        self.move('source','graveyard')
        self.settle()
        for i in range(paid_pages):
            self.move(f'library_{i}','hand')
        self.receipt()

    def tyrant(self):
        self.cast('source','battlefield')
        self.stats('source')
        if self.mode=='terminal':
            for ref in ('axe_0','axe_1','axe_2','burn'):
                self.life[self.foe] -= self.burn(ref,self.foe)
            self.receipt()
        self.go(self.foe)
        self.count('source',0)
        self.receipt()
        self.go(self.owner)
        gain = 1 if self.mode=='locked' else 2
        self.life[self.owner] -= 1
        self.life[self.foe] -= gain-1
        self.count('source',gain)
        self.stats('source',gain)
        self.receipt()
        if self.mode=='combat':
            self.basic_battle('source','flyer',deny='ground',attack_power=7,hit=5)
            self.move('flyer','graveyard')
            self.observe('source','damage',0)
        if self.mode=='terminal':
            self.own_next(terminal=True)
            self.win_receipt()
            self.count('source',9)
            return
        if self.mode=='repeat':
            self.own_next()
            self.life = [value-1 for value in self.life]
            self.count('source',4)
            self.stats('source',4)
        self.receipt()

    def ooze(self):
        self.cast('source','battlefield')
        self.go(self.foe)
        self.count('source',0)
        self.stats('source')
        mode = self.mode
        accepted = {'none':0,'black':1,'green':1,'both':2,'partial':1,'decline':0,'dual':2}[mode]
        self.go(self.owner,before=lambda: self.optional(accepted))
        self.count('source',accepted)
        self.stats('source',accepted)
        self.receipt()

    def bomb(self):
        self.cast('source','battlefield')
        self.emit('can_activate','source',value=0)
        self.own_next()
        self.activate(target='victim')
        self.observe('source','tapped',1)
        self.settle()
        self.receipt()
        for _ in range(2):
            self.own_next()
            self.observe('victim','in_play',1)
            self.receipt()
        self.own_next()
        recipient = self.objects['victim']['player']
        self.life[recipient] -= 4
        if self.mode!='indestructible':
            self.move('victim','graveyard')
        self.receipt()
        self.own_next()
        self.receipt()

    def chisei(self):
        self.cast('source','battlefield')
        if 'sphere' in self.objects:
            self.cast('sphere','battlefield')
            self.count('sphere',3,'charge')
        self.go(self.foe)
        if self.mode=='foreign':
            self.cast('foe_beetle','battlefield')
        def counter_kind():
            self.emit('choose_dialog',player=self.owner,value=0)
        sphere = 'sphere' in self.objects
        self.go(self.owner,target='sphere' if sphere else None,before=counter_kind if sphere else None)
        if self.mode in ('foreign','none'):
            self.move('source','graveyard')
            if 'foe_beetle' in self.objects:
                self.count('foe_beetle',2)
        else:
            self.count('sphere',2,'charge')
            if self.mode=='flying':
                self.basic_battle('source',deny='blocker',hit=4)
            else:
                self.go(self.foe)
                self.count('sphere',2,'charge')
                self.go(self.owner,target='sphere',before=counter_kind)
                self.count('sphere',1,'charge')
        self.receipt()

    def crescendo(self):
        self.cast('source','battlefield')
        self.stats('attacker')
        self.stats('blocker')
        self.go(self.foe)
        our_attack = self.mode=='own_attack'
        if our_attack:
            self.go(self.owner)
        self.stats('attacker')
        self.stats('blocker')
        self.basic_battle('attacker','blocker',attack_power=4 if our_attack else 3,
                          block_power=0 if our_attack else 1)
        self.stats('attacker')
        self.stats('blocker')
        self.stats('idle')
        self.receipt()

    def crovax(self):
        self.cast('source','battlefield')
        self.count('source',4)
        self.stats('source',4)
        self.go(self.foe)
        self.count('source',4)
        paid = self.mode=='paid'
        def menu():
            self.emit('choose_dialog',player=self.owner,value=int(not paid))
            if paid and self.owner==0:
                self.emit('choose_target','food',player=self.owner)
        self.go(self.owner,before=menu)
        if paid:
            self.move('food','graveyard')
        self.count('source',5 if paid else 3)
        self.stats('source',5 if paid else 3)
        if self.mode=='flying':
            self.activate([0,1,0,0,0,0])
            self.settle()
            self.basic_battle('source',deny='blocker',hit=3)
            self.go(self.foe)
            self.go(self.owner,dialog=1)
            self.basic_battle('source','blocker')
            self.move('blocker','graveyard')
            self.move('source','graveyard')
        self.receipt()

    def cyclone(self):
        self.cast('source','battlefield')
        maximum = {'unpaid':0,'scale':2,'short':1}[self.mode]
        for amount in range(1,maximum+1):
            def pay():
                if self.owner==0:
                    for index in range(amount):
                        self.emit('choose_mana_source',f'land_{index}')
            self.own_next(choices=pay)
            self.life = [value-amount for value in self.life]
            for index in range(amount):
                self.observe(f'land_{index}','tapped',1)
            if self.mode=='scale':
                self.observe('large','damage',amount)
                if amount==2:
                    self.move('small','graveyard')
                else:
                    self.observe('small','damage',1)
            self.receipt()
        if self.mode!='scale':
            def cancel_mana():
                if self.owner==0:
                    self.emit('choose_cancel_mana',player=self.owner)
            self.own_next(choices=cancel_mana)
            self.move('source','graveyard')
            self.receipt()

    def reactor(self):
        self.cast('source','battlefield')
        self.count('source',0,'charge')
        if self.mode=='decline':
            self.own_next()
            self.count('source',0,'charge')
        elif self.mode=='threshold':
            for count in range(1,21):
                self.own_next(choices=self.optional,terminal=count==20)
                self.count('source',count,'charge')
                if count in (1,19):
                    self.receipt()
            self.win_receipt()
            return
        else:
            self.own_next(choices=self.optional)
            self.count('source',1,'charge')
            if self.mode=='indestructible':
                self.cast('shatter','graveyard',target='source')
                self.observe('source','in_play',1)
                self.count('source',1,'charge')
        self.receipt()

    def shield(self):
        self.cast('source','battlefield')
        p,q = self.owner,self.foe
        victim = q if self.mode=='opposing_damage' else p
        self.burn('burn',victim)
        if victim==q:
            self.life[q] -= 3
        self.receipt()
        partial = self.mode=='partial'
        def pay():
            if partial and p==0:
                self.emit('choose_mana_source','plains')
                self.emit('choose_mana_source','island')
        self.own_next(choices=pay)
        if victim==p:
            self.life[p] -= 2 if partial else 3
        if partial:
            for ref in ('plains','island'):
                self.observe(ref,'tapped',1)
        self.receipt()
        self.own_next()
        self.receipt()

    def descendant(self):
        self.cast('source','battlefield')
        self.go(self.foe)
        self.count('source',0)
        amount = {'ahead':2,'behind':0,'equal':0,'growth':2}[self.mode]
        self.go(self.owner)
        self.count('source',amount)
        self.stats('source',amount)
        self.receipt()
        if self.mode=='growth':
            self.own_next()
            self.count('source',2*amount)
            self.stats('source',2*amount)
            self.receipt()

    def descent(self):
        self.cast('source','battlefield')
        rounds = 2 if self.mode=='repeat' else 1
        for total in range(1,rounds+1):
            plan = {}
            for seat in (0,1):
                if self.mode in ('mixed','hand_only'):
                    plan[seat] = (f'held_{seat}',)
                elif total==1:
                    plan[seat] = (f'first_{seat}',)
                else:
                    plan[seat] = (f'second_{seat}',f'third_{seat}')
            def choose():
                for seat in (0,1):
                    for ref in plan[seat]:
                        if ref.startswith('held_') and (self.mode=='mixed' or seat==self.owner):
                            self.emit('choose_cancel_target',player=seat)
                        self.emit('choose_target',ref,player=seat)
            self.own_next(choices=choose)
            for refs in plan.values():
                for ref in refs:
                    self.move(ref,'exile')
            self.receipt()

    def dirge(self):
        self.cast('source','battlefield')
        amount = {'zero':0,'one':1,'two':2,'decline':1}[self.mode]
        for _ in range(amount):
            self.own_next(choices=self.optional)
        self.activate([0,1,0,0,0,0])
        self.move('source','graveyard')
        for i in range(amount):
            ref = f'held_{i}'
            op = 'choose_optional_zone' if self.mode=='decline' else 'choose_zone_card'
            self.emit(op,player=self.owner,value=self.rows[self.objects[ref]['name']]['csvid'])
        self.settle()
        if self.mode=='decline':
            self.emit('assert_forced_zone_choices',value=0)
        else:
            for i in range(amount):
                self.move(f'held_{i}','graveyard')
        self.receipt()

    def run(self):
        self.emit('turn',player=self.owner)
        self.emit('present_ai_dialogs',value=1)
        for player in (0,1):
            self.pool(player,fund=True)
        self.receipt()
        if self.mode=='countered':
            self.countered()
        else:
            methods = {VIAL:self.vial,GROVE:self.grove,DJINN:self.djinn,EVE:self.eve,
                       AZOR:self.azor,CODEX:self.codex,TYRANT:self.tyrant,OOZE:self.ooze,BOMB:self.bomb,
                       CHISEI:self.chisei,WAR:self.crescendo,CROVAX:self.crovax,CYCLONE:self.cyclone,
                       REACTOR:self.reactor,SHIELD:self.shield,DESCENDANT:self.descendant,
                       DESCENT:self.descent,DIRGE:self.dirge}
            methods[self.id]()
