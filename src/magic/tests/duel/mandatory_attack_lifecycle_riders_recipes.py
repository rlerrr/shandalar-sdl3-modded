"""Required combat declarations with complete, paid lifecycle and damage riders."""

import copy
import json
import re
from pathlib import Path

from dash_creature_recipes import can_pay
from library_search_recipes import wave6_exact
from required_attack_creature_recipes import CAST, CLAUSE, LIBRARY, RequiredAttackCase


FAMILY = 'mandatory_attack_lifecycle_riders'
PROFILE = Path(__file__).parent / 'data/mandatory_attack_lifecycle_riders_wave8_profiles.json'
OOZE = ('At the beginning of your upkeep, put a +1/+1 counter on this creature. '
        "Then you may pay {X}, where X is the number of +1/+1 counters on it. If you don't, "
        'tap this creature and it deals X damage to you.')
INFECT = ('Infect (This creature deals damage to creatures in the form of -1/-1 counters '
          'and to players in the form of poison counters.)')
ECHO = re.compile(r'Echo (\{[0-9]+\}) \(At the beginning of your upkeep, if this came under your control '
                  r'since the beginning of your last upkeep, sacrifice it unless you pay its echo cost\.\)')
DASH = re.compile(r"Dash (\{[0-9]+\}\{R\}) \(You may cast this spell for its dash cost\. If you do, it gains haste, "
                  r"and it's returned from the battlefield to its owner's hand at the beginning of the next end step\.\)")
REGEN = re.compile(r'(\{[0-9]+\}\{B\}): Regenerate this creature\.')
ANNIHILATOR = re.compile(r'Annihilator ([1-9][0-9]*) \(Whenever this creature attacks, '
                         r'defending player sacrifices \w+ permanents of their choice\.\)')
NUMBERS = {2:'two'}
HANDLERS = {
    'ooze':'manalinkeh_card_primordial_ooze',
    'echo':'manalinkeh_card_thran_war_machine',
    'annihilator':'manalinkeh_card_ulamogs_crusher',
    'infect':'manalinkeh_card_berserkers_of_blood_ridge',
    'regenerate':'manalinkeh_card_deathbellow_raider',
    'dash':'manalinkeh_card_sprinting_warbrute',
}
COMMON = ('omitted','tapped','sick','pacified','opponent_turn','paid_fresh','unfunded','replay')
EXTRAS = {
    'ooze':('upkeep_pay','upkeep_cancel','upkeep_scaled','upkeep_opponent'),
    'echo':('echo_pay','echo_decline','echo_unfunded','echo_once'),
    'annihilator':('annihilator_two','annihilator_short','annihilator_choice'),
    'infect':('infect_blocked','infect_noncombat'),
    'regenerate':('regenerate_paid','regenerate_short','regenerate_wrong_color','regenerate_proactive'),
    'dash':('dash_exact','dash_menu','normal_menu','dash_replay'),
}
ENTRY = set(EXTRAS['echo']) | set(EXTRAS['dash'])
WITNESSES = set(LIBRARY) | {'Pacifism','Dazzling Ramparts','Hermetic Study','Sol Ring'}
DUMMIES = {'Grizzly Bears','Hill Giant'}
ZERO = [0]*6


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    clauses=record.get('oracle_text','').splitlines()
    printed_type=record.get('type_line','').partition(' — ')[0]
    stats=[record.get(prop,'') for prop in ('power','toughness')]
    if (record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces')
            or printed_type not in ('Creature','Artifact Creature') or len(clauses)!=2 or clauses.count(CLAUSE)!=1
            or any(type(stat) is not str or not re.fullmatch(r'[1-9][0-9]*',stat) for stat in stats)
            or not re.fullmatch(r'(?:\{[1-9]\})?(?:\{R\})?|\{[1-9]\}',record.get('mana_cost',''))
            or not record.get('mana_cost')):
        raise ValueError('normal fixed-cost required attacker with one complete rider required')
    text,=(clause for clause in clauses if clause!=CLAUSE)
    extra=None
    quantity=0
    if text==OOZE:
        mode,keys='ooze',[]
    elif text==INFECT:
        mode,keys='infect',['Infect']
    elif match:=ECHO.fullmatch(text):
        mode,keys,extra='echo',['Echo'],match[1]
    elif match:=DASH.fullmatch(text):
        mode,keys,extra='dash',['Dash'],match[1]
    elif match:=REGEN.fullmatch(text):
        mode,keys,extra='regenerate',['Regenerate'],match[1]
    elif (match:=ANNIHILATOR.fullmatch(text)) and text.endswith(f'sacrifices {NUMBERS.get(int(match[1]))} permanents of their choice.)'):
        mode,keys,quantity='annihilator',['Annihilator'],int(match[1])
    else:
        raise ValueError('unreviewed printed rider')
    if record.get('keywords')!=keys:
        raise ValueError('printed keywords must agree with the full rider')
    costs={'printed':cost_from_oracle(record)}
    if extra:
        costs[mode]=cost_from_oracle({'mana_cost':extra})
    if mode=='dash' and (costs['printed'][1:]!=costs['dash'][1:] or costs['printed'][0]<=costs['dash'][0]):
        raise ValueError('reviewed card needs distinct same-color normal and dash costs')
    return dict(mode=mode,costs=costs,power=int(stats[0]),toughness=int(stats[1]),keywords=keys,quantity=quantity)


def handler(record):
    return HANDLERS[classify(record)['mode']]


def variants(shape):
    return [f'{scene}_{seat}' for scene in COMMON+EXTRAS[shape['mode']] for seat in (0,1)
            if (scene,seat) not in {('upkeep_cancel',1),('annihilator_choice',0)}]


def spec(declaration,record):
    shape=classify(record)
    pin=profiles()['sources'].get(record['name'])
    chosen=declaration.get('variants')
    if (not pin or not wave6_exact(pin['oracle'],record) or declaration.get('family')!=FAMILY
            or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not wave6_exact(declaration.get('params'),shape)
            or type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(set(chosen))!=len(chosen) or set(chosen)-set(variants(shape))):
        raise ValueError('pinned complete printed lifecycle, cost and approved variants required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    data=profiles()
    if data['version']!=1 or set(data['witnesses'])!=WITNESSES:
        raise ValueError('complete pinned required-attack witnesses needed')
    for group in ('sources','witnesses'):
        for name,pin in data[group].items():
            row=rows[name]
            if (not wave6_exact(oracle[name],pin['oracle']) or not wave6_exact(row,pin['registry'])
                    or row['kind']!='card' or row['handler'] in (None,'NULL','0')
                    or row['handler']=='card_dummy' and name not in DUMMIES):
                raise ValueError(f'changed source or witness: {name}')
            if group=='sources' and row['handler']!=handler(oracle[name]):
                raise ValueError(f'changed lifecycle handler: {name}')


class Lifecycle(RequiredAttackCase):
    def __init__(self,name,csvid,declaration,shape,variant,rows,oracle):
        self.shape=shape
        self.rider=shape['mode']
        self.counters={}
        self.poison=[0,0]
        self.lands=[]
        self.sacrifices=[]
        super().__init__(name,csvid,declaration,shape,variant,rows,oracle)
        self.configure()
        self.names=sorted({entry['name'] for entry in self.setup})

    def add(self,name,ref,who,zone='battlefield',state=None):
        from recipes import card

        if ref=='source' and self.mode in ENTRY:
            zone='hand'
        entry=card(name,ref,who,zone)
        if state:
            entry['state']=state
        self.setup.append(entry)
        self.zones[zone][who].append(name)
        if zone in ('battlefield','hand'):
            record=self.oracle[name]
            is_creature='Creature' in record['type_line'].partition(' — ')[0].split()
            stats=tuple(int(record[prop]) for prop in ('power','toughness')) if is_creature else None
            self.entities[ref]=dict(name=name,who=who,zone=zone,stats=stats,
                                    tapped=bool(state and 'tapped' in state),damage=0)

    def configure(self):
        if self.mode in ('upkeep_pay','upkeep_cancel'):
            self.add_lands(1)
        if self.mode in ('echo_pay','echo_decline','echo_once'):
            self.add_lands(sum(self.shape['costs']['echo']))
        if self.mode=='infect_blocked':
            self.add('Dazzling Ramparts','blocker',self.other)
        if self.mode=='infect_noncombat':
            self.add('Hermetic Study','aura',self.owner,'hand')
            self.add('Dazzling Ramparts','blocker',self.other)
        if self.mode.startswith('annihilator_'):
            count={'annihilator_two':2,'annihilator_short':1,'annihilator_choice':3}[self.mode]
            self.add('Forest','friendly',self.owner)
            for i,name in enumerate(('Island','Sol Ring','Mountain')[:count]):
                ref=f'sacrifice_{i}'
                self.add(name,ref,self.other)
                self.sacrifices.append(ref)

    def add_lands(self,count):
        for i in range(count):
            ref=f'payment_{i}'
            self.add('Mountain',ref,self.owner)
            self.lands.append(ref)

    def receipt(self,stack=0):
        super().receipt(stack)
        for who in (0,1):
            self.emit('assert_poison',player=who,value=self.poison[who])
        for ref,(kind,count) in self.counters.items():
            if self.entities[ref]['zone']=='battlefield':
                self.emit('assert_counter',ref=ref,counter_kind=kind,value=count)

    def empty_run_receipt(self):
        self.receipt()
        if self.entities['source']['zone']=='hand':
            self.observe('source','in_hand',1)

    def fund(self,seat,cost):
        self.mana[seat]=list(cost)
        for color,amount in enumerate(cost):
            self.emit('set_mana',player=seat,color=color,value=amount)
        self.emit('refresh')

    def source_cast(self,dash=False,menu=False):
        cost=self.shape['costs']['dash' if dash else 'printed']
        budget=list(self.shape['costs']['printed'] if menu else cost)
        if self.rider=='dash':
            alternate=self.shape['costs']['dash']
            if can_pay(budget,self.fee) and can_pay(budget,alternate):
                self.emit('present_ai_dialogs',value=1)
                self.fund(self.owner,budget)
                self.emit('choose_dialog',player=self.owner,value=int(not dash))
            else:
                self.fund(self.owner,budget)
        else:
            self.fund(self.owner,budget)
        self.cast('source',cost)
        if self.rider=='dash':
            self.emit('present_ai_dialogs',value=0)
        if self.rider=='dash' and any(self.mana[self.owner]):
            self.emit('assert_mana_total',player=self.owner,value=sum(self.mana[self.owner]))

    def change_counter(self,ref,kind,n):
        _,previous=self.counters.get(ref,(kind,0))
        self.counters[ref]=(kind,previous+n)
        sign=1 if kind=='plus1' else -1
        self.entities[ref]['stats']=tuple(s+sign*n for s in self.entities[ref]['stats'])

    def visit_upkeep(self,expected_seat,effect=None):
        if any(map(any,self.mana)):
            raise ValueError('real upkeep requires empty pools')
        self.emit('advance_turn')
        self.active=1-self.active
        if self.active!=expected_seat:
            raise ValueError('upkeep must belong to actual next turn')
        self.emit('untap_step',player=self.active)
        for permanent in self.entities.values():
            permanent['damage']=0
            if permanent['zone']=='battlefield' and permanent['who']==self.active:
                permanent['tapped']=False
        self.receipt()
        if effect:
            effect()
        self.emit('upkeep_step',player=self.active)
        self.emit('assert_unused_choices',value=0)
        self.emit('present_ai_dialogs',value=0)
        self.receipt()
        self.emit('turn',player=self.active,value=0)

    def pay_lands(self,cancel=False):
        if self.owner==0:
            if cancel:
                self.emit('choose_cancel_mana',player=0)
            else:
                for land in self.lands:
                    self.emit('choose_mana_source',ref=land)
        if not cancel:
            for land in self.lands:
                self.entities[land]['tapped']=True

    def ooze_upkeep(self):
        self.change_counter('source','plus1',1)
        _,count=self.counters['source']
        paid=self.mode=='upkeep_pay'
        if paid or self.mode=='upkeep_cancel':
            if count!=len(self.lands):
                raise ValueError('cover only exact due land funding')
            self.pay_lands(cancel=not paid)
        if not paid:
            self.entities['source']['tapped']=True
            self.life[self.owner]-=count

    def echo_upkeep(self):
        if self.lands:
            self.emit('present_ai_dialogs',value=1)
            self.emit('choose_dialog',player=self.owner,value=int(self.mode=='echo_decline'))
        if self.mode in ('echo_pay','echo_once'):
            self.pay_lands()
        else:
            self.move('source','graveyard')

    def declare(self,eligible):
        self.emit('begin_required_attackers')
        attackers=[]
        source_live=self.entities['source']['zone']=='battlefield'
        if source_live:
            self.observe('source','attacking',0)
        self.emit('can_finish_attackers',value=int(not eligible))
        if self.mode=='opponent_turn':
            self.attack('ally')
            attackers.append('ally')
        elif source_live:
            self.attack('source',allowed=eligible)
            if eligible:
                attackers.append('source')
        self.emit('can_finish_attackers',value=1)
        picks=[]
        if 'source' in attackers:
            picks=self.sacrifices[:self.shape['quantity']]
            if self.mode=='annihilator_choice':
                picks=self.sacrifices[1:]
            if self.other==0:
                for ref in picks:
                    self.emit('choose_target',ref=ref,player=self.other)
        self.emit('finish_attackers')
        for ref in picks:
            self.move(ref,'graveyard')
        self.receipt()
        return attackers

    def fight(self,eligible=True):
        attackers=self.declare(eligible)
        if not attackers:
            return
        self.emit('begin_blockers')
        blocked=self.mode=='infect_blocked'
        if blocked:
            self.emit('block',ref='blocker',other='source',value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        for ref in attackers:
            power=self.entities[ref]['stats'][0]
            if ref=='source' and self.rider=='infect':
                if blocked:
                    self.change_counter('blocker','minus1',power)
                else:
                    self.poison[1-self.active]+=power
            else:
                self.life[1-self.active]-=power
        self.emit('refresh')
        self.receipt()
        self.emit('end_combat')

    def infect_noncombat(self):
        from recipes import cost_from_oracle

        self.fund(self.owner,cost_from_oracle(self.oracle['Hermetic Study']))
        self.emit('choose_target',ref='source',player=self.owner)
        self.cast('aura',self.mana[self.owner])
        self.emit('require_can_activate',ref='aura',value=1)
        self.emit('choose_target',ref='blocker',player=self.owner)
        self.emit('activate',ref='aura',value=1)
        self.emit('assert_stack',value=1)
        self.emit('resolve')
        self.entities['source']['tapped']=True
        self.change_counter('blocker','minus1',1)
        self.receipt()
        self.fight(eligible=False)

    def regenerate(self):
        from recipes import COLORS

        cost=list(self.shape['costs']['regenerate'])
        exact=self.mode=='regenerate_paid'
        if self.mode=='regenerate_short':
            cost[0]-=1
        if self.mode=='regenerate_wrong_color':
            cost[COLORS['R']],cost[COLORS['B']]=cost[COLORS['B']],0
        self.fund(self.owner,cost)
        if self.mode=='regenerate_proactive':
            # Check printed timing independently, outside the legacy destruction window.
            self.emit('can_activate',ref='source',value=1)
            self.receipt()
            return
        self.emit('choose_regeneration',ref='source')
        self.emit('destroy',ref='source')
        self.emit('assert_unused_choices',value=int(not exact))
        if exact:
            self.mana[self.owner]=list(ZERO)
            self.entities['source']['tapped']=True
        else:
            self.move('source','graveyard')
        self.receipt()
        self.fight(eligible=False)

    def before_next_end(self,dash):
        self.emit('cleanup')
        self.emit('refresh')
        if dash:
            self.move('source','hand')
            self.emit('bind_hand',ref='source',player=self.owner,value=self.csvid)
        self.empty_run_receipt()

    def resolve(self):
        mode=self.mode
        if mode=='unfunded':
            self.cast('source',self.fee,denied=True)
        elif mode.startswith('upkeep_'):
            self.visit_upkeep(self.other)
            if mode!='upkeep_opponent':
                self.visit_upkeep(self.owner,self.ooze_upkeep)
                self.fight(eligible=mode=='upkeep_pay')
                if mode=='upkeep_scaled':
                    self.visit_upkeep(self.other)
                    self.visit_upkeep(self.owner,self.ooze_upkeep)
                    self.fight(eligible=False)
        elif mode.startswith('echo_'):
            self.source_cast()
            self.visit_upkeep(self.other)
            self.visit_upkeep(self.owner,self.echo_upkeep)
            self.fight(eligible=mode in ('echo_pay','echo_once'))
            if mode=='echo_once':
                self.visit_upkeep(self.other)
                self.visit_upkeep(self.owner)
                self.fight()
        elif mode in EXTRAS['dash']:
            dash=mode!='normal_menu'
            self.source_cast(dash=dash,menu=mode in ('dash_menu','normal_menu'))
            self.fight(eligible=dash)
            self.before_next_end(dash)
        elif mode.startswith('regenerate_'):
            self.regenerate()
        elif mode=='infect_noncombat':
            self.infect_noncombat()
        else:
            if mode=='pacified':
                self.emit('choose_target',ref='source',player=self.owner)
                self.cast('aura',self.aura_fee)
            if mode=='paid_fresh':
                if self.rider=='dash':
                    self.source_cast(menu=True)
                else:
                    self.cast('source',self.fee)
            self.fight(eligible=mode not in ('tapped','sick','pacified','opponent_turn','paid_fresh'))


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    from recipes import step

    if rows[name]['csvid']!=csvid or not selected or set(selected)-set(variants(shape)):
        raise ValueError('exact approved mandatory attack lifecycle scenarios required')
    validate_witnesses(oracle,rows)
    gaps=['Multiplayer, competing attack requirements and independently interruptible attack-declaration triggers.']
    if shape['mode']=='ooze':
        gaps.append('AI strategy for voluntary payment cancellation.')
    cases=[]
    for variant in selected:
        world=Lifecycle(name,csvid,declaration,shape,variant,rows,oracle)
        world.receipt()
        world.resolve()
        body=world.steps
        if world.mode in ('replay','dash_replay'):
            body=[step('capture'),*body,step('record_state'),step('ai_roundtrip'),step('restore'),
                  *copy.deepcopy(body),step('assert_recorded_state')]
        cases.append(dict(name=f'MandatoryAttackLifecycleRidersRecipe_{csvid}_{variant}',card=name,
                          fixture='empty_main',seed=123,setup=world.setup,steps=world.before+body,
                          _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                                       oracle_text=declaration['oracle_text'],untested_components=gaps)))
    return cases
