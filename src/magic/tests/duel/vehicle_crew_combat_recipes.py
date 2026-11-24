"""Ordered Crew costs and actual artifact-creature combat."""

from functools import lru_cache
from hashlib import sha256
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from morph_turn_up_target_recipes import Scene


FAMILY = 'vehicle_crew_combat'
PROFILE = Path(__file__).parent/'data/vehicle_crew_combat_wave8_profiles.json'
CREW = re.compile(r'Crew ([1-4])(?: \(Tap any number of creatures you control with total power ([1-4]) or more: This Vehicle becomes an artifact creature until end of turn\.\))?')
ABILITIES = {'':(), 'Flying':('Flying',), 'Menace':('Menace',), 'Vigilance':('Vigilance',),
             'Trample, haste':('Trample','Haste')}
COMMON = ('cast','cast_short','ordered_exact','underpowered','tapped_pilot','opposing_pilot','noncreature_pilot',
          'partial_cancel','surplus','zero_then_paid','fresh_pilot','selector','tapped_vehicle','expires',
          'source_leaves_pending','pilot_leaves_pending','activation_replay','pending_replay','recrew_other',
          'recrew_self_only','fresh_vehicle','attack_uncrewed','attack_combat','offturn_block','live_power','shroud_pilot')
PILOT = {0:'Wall of Tanglecord',1:'Mogg Fanatic',2:'Catapult Squad',3:'Unyielding Krumar',4:'Sengir Vampire',5:'Golgari Rotwurm'}
EXACT_PILOT_POWER = {1:(1,),2:(1,1),3:(2,1),4:(2,2)}
NO_CREW = ('cast','cast_short','attack_uncrewed')
DENIED = ('underpowered','tapped_pilot','opposing_pilot','noncreature_pilot')
GAPS = ('Additional voluntary selections after the native threshold menu closes', 'Multiplayer')


def record_hash(record):
    return sha256(json.dumps(record,ensure_ascii=False,sort_keys=True,separators=(',',':')).encode()).hexdigest()


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record['type_line']!='Artifact — Vehicle' or record.get('layout')!='normal'
            or record.get('card_faces') or record.get('selected_face')
            or any(not re.fullmatch(r'[1-9][0-9]*',record.get(k,'')) for k in ('power','toughness'))):
        raise ValueError('normal fixed-stat Vehicle required')
    clauses = record['oracle_text'].splitlines()
    perk = clauses[0] if len(clauses)==2 else ''
    match = CREW.fullmatch(clauses[-1]) if 1<=len(clauses)<=2 else None
    if (not match or match[2] and match[2]!=match[1] or perk not in ABILITIES
            or set(record['keywords'])!={'Crew',*ABILITIES[perk]}):
        raise ValueError('exact complete Crew and listed passive abilities required')
    return dict(effect='crew',cost=cost_from_oracle(record),threshold=int(match[1]),
                power=int(record['power']),toughness=int(record['toughness']),keywords=list(ABILITIES[perk]))


def variants(model,name=None):
    modes = COMMON
    if (name or model.get('source_name'))=='Sleek Schooner':
        modes += ('overcrew',)
    if 'Flying' in model['keywords']:
        modes += ('flying_blocker','reach_blocker')
    if 'Menace' in model['keywords']:
        modes += ('menace_pair',)
    return [f'{mode}_{seat}' for mode in modes for seat in (0,1)]


def spec(declaration,record):
    model = parameters(record)
    pin = pins()['sources'].get(record['name'],{})
    selected = declaration.get('variants')
    if (pin.get('oracle_record_sha256')!=record_hash(record) or declaration.get('family')!=FAMILY
            or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),model)
            or type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(model,record['name']))):
        raise ValueError('pinned complete Vehicle and exact typed variants required')
    return dict(model,family=FAMILY,source_name=record['name'])


def validate_witnesses(oracle,rows):
    profile = pins()
    if profile['version']!=1:
        raise ValueError('unreviewed Crew profile')
    for group in ('sources','witnesses'):
        for name,pin in profile[group].items():
            row,record = rows.get(name,{}),oracle.get(name,{})
            if (pin['oracle_record_sha256']!=record_hash(record) or not exact(pin['registry'],row)
                    or row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')
                    or record.get('layout')!='normal'):
                raise ValueError(f'changed or unsupported Crew card: {name}')
    if any(int(oracle[name]['power'])!=power for power,name in PILOT.items()):
        raise ValueError('Crew cost creatures changed their printed power')


class CrewScene(Scene):
    def __init__(self,name,model,mode,owner,rows,oracle):
        super().__init__(name,model,mode,owner,rows,oracle)
        self.animated = False
        self.pilots = []
        self.other_pilots = []
        if mode=='offturn_block':
            self.active = self.foe

    def add_pilot(self,name,*,actor=None,tapped=False,zone='battlefield',secondary=False):
        plan = self.other_pilots if secondary else self.pilots
        ref = f"{'later' if secondary else 'pilot'}_{len(plan)}"
        plan.append(ref)
        self.add(name,ref,self.owner if actor is None else actor,zone,tapped=tapped)

    def setup_cards(self):
        mode = self.mode
        threshold = self.model['threshold']
        zone = 'hand' if mode in ('cast','cast_short','fresh_vehicle') else 'battlefield'
        self.add(self.name,'source',self.owner,zone,tapped=mode=='tapped_vehicle')
        self.add('Unsummon','type_probe',self.foe,'hand')
        if mode not in NO_CREW:
            self.setup_cost(threshold)
        if mode in ('source_leaves_pending','pilot_leaves_pending'):
            self.add('Boomerang','bounce',self.foe,'hand')
        if mode=='live_power':
            self.add('Giant Growth','growth',self.owner,'hand')
        if mode in ('attack_combat','fresh_vehicle','flying_blocker','reach_blocker','menace_pair'):
            name = {'flying_blocker':'Welkin Tern','reach_blocker':'Nessian Asp'}.get(mode,'Goblin Tinkerer')
            self.add(name,'blocker',self.foe)
            if mode=='menace_pair':
                self.add('Goblin Tinkerer','second_blocker',self.foe)
        if mode=='offturn_block':
            self.add('Mogg Fanatic','attacker',self.foe)

    def setup_cost(self,threshold):
        mode = self.mode
        if mode=='underpowered':
            self.add_pilot(PILOT[threshold-1])
        elif mode=='tapped_pilot':
            self.add_pilot(PILOT[threshold],tapped=True)
        elif mode=='opposing_pilot':
            self.add_pilot(PILOT[threshold],actor=self.foe)
        elif mode=='noncreature_pilot':
            self.add_pilot('Darksteel Relic')
        elif mode=='fresh_pilot':
            self.add_pilot(PILOT[threshold],zone='hand')
        elif mode=='overcrew':
            self.add_pilot('Mogg Fanatic')
            self.add_pilot('Goblin Tinkerer')
        elif mode=='surplus':
            self.add_pilot(PILOT[threshold+1])
        elif mode=='live_power':
            self.add_pilot(PILOT[1 if threshold==4 else 0])
        elif mode=='shroud_pilot':
            self.add_pilot('Calcite Snapper')
            if threshold>1:
                self.add_pilot(PILOT[threshold-1])
        else:
            if mode=='zero_then_paid' or mode=='partial_cancel' and threshold==1:
                self.add_pilot(PILOT[0])
            for power in EXACT_PILOT_POWER[threshold]:
                self.add_pilot(PILOT[power])
        if mode=='selector':
            self.add(PILOT[threshold],'enemy_pilot',self.foe)
            self.add(PILOT[threshold],'ready_extra',self.owner)
            self.add(PILOT[threshold],'tapped_extra',self.owner,tapped=True)
            self.add('Darksteel Relic','noncreature',self.owner)
            self.add('Calcite Snapper','shroud',self.owner)
        if mode=='recrew_other':
            self.add_pilot(PILOT[threshold],secondary=True)

    def source_traits(self,full=False):
        self.prop('source','csvid',self.csvid)
        self.emit('can_target_creature',ref='type_probe',other='source',value=int(self.animated))
        if full:
            self.emit('assert_matching_permanents',player=self.owner,index=self.csvid,property='artifact',value=1)
        if self.animated:
            p,t = self.stats('source')
            for key,value in [('power',p),('toughness',t),('damage',self.entities['source']['damage'])]:
                self.prop('source',key,value)

    def refresh_zero(self):
        self.emit('turn',player=self.active,value=0)
        self.emit('present_ai_dialogs',value=1)
        for actor in (0,1):
            for color in range(6):
                self.emit('set_mana',player=actor,color=color,value=0)
        self.emit('refresh')
        self.receipts()

    def cast_permanent(self,ref,allowed=True):
        from recipes import cost_from_oracle

        record = self.oracle[self.entities[ref]['name']]
        fee = cost_from_oracle(record)
        budget = fee.copy()
        if not allowed:
            budget[0] -= 1
        self.fund(self.owner,budget)
        self.emit('cast',ref=ref,value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(allowed))
        self.prop(ref,'in_hand',not allowed)
        if allowed:
            self.pay(self.owner,fee)
            self.pools()
            self.resolve()
            self.entities[ref]['zone'] = 'battlefield'
        self.receipts()

    def start_crew(self,plan,*,available=True,cancel=False):
        self.emit('can_activate',ref='source',value=int(available))
        if self.mode=='activation_replay':
            self.replay = len(self.steps)
        if available:
            for ref in plan:
                self.emit('choose_target',ref=ref,player=self.owner)
            if cancel:
                self.emit('choose_cancel_target',player=self.owner)
        paid = available and not cancel
        if paid:
            powers = [self.stats(ref)[0] for ref in plan]
            threshold = self.model['threshold']
            crossed_early = sum(powers[:-1])>=threshold
            if (crossed_early!=(self.mode=='overcrew') or sum(powers)<threshold or len(set(plan))!=len(plan)):
                raise ValueError('reviewed distinct Crew selections must pay the required power')
        self.emit('activate',ref='source',value=int(paid))
        if self.mode=='overcrew':
            self.emit('assert_unused_choices',value=0)
        else:
            self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(paid))
        if paid:
            for ref in plan:
                e = self.entities[ref]
                if e['zone']!='battlefield' or e['controller']!=self.owner or e['tapped']:
                    raise ValueError('Crew cost must use ready actual controlled creatures')
                e['tapped'] = True
        for ref in (*self.pilots,*self.other_pilots):
            self.prop(ref,'tapped',self.entities[ref]['tapped'])
        self.prop('source','tapped',self.entities['source']['tapped'])
        self.source_traits()
        self.pools()
        if not paid:
            self.receipts()
        return paid

    def finish_crew(self):
        mode = self.mode
        response = mode in ('source_leaves_pending','pilot_leaves_pending')
        if self.mode=='pending_replay':
            self.replay = len(self.steps)
        if response:
            target = 'source' if mode=='source_leaves_pending' else self.pilots[0]
            self.bounce_response(target)
            self.leave('bounce','graveyard')
            self.leave(target,'hand')
        else:
            self.resolve()
        self.animated = self.entities['source']['zone']=='battlefield'
        self.receipts()

    def bounce_response(self,target):
        from recipes import cost_from_oracle

        fee = cost_from_oracle(self.oracle['Boomerang'])
        self.fund(self.foe,fee)
        self.emit('choose_target',ref=target,player=self.foe)
        self.emit('cast',ref='bounce',value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=2)
        self.pay(self.foe,fee)
        self.resolve()

    def declare_attacker(self,ref,allowed=True):
        self.emit('begin_attackers')
        self.emit('attack',ref=ref,value=int(allowed))
        self.emit('finish_attackers')
        if allowed:
            vigilance = ref=='source' and 'Vigilance' in self.model['keywords']
            self.entities[ref]['tapped'] = not vigilance
            self.prop(ref,'tapped',not vigilance)

    def finish_combat(self,attacker,blocker,*,blocked):
        self.emit('begin_blockers')
        if blocker:
            self.emit('block',ref=blocker,other=attacker,value=int(blocked))
        if self.mode=='menace_pair':
            self.emit('block',ref='second_blocker',other=attacker,value=1)
        self.emit('advance_combat')
        self.prop(attacker,'blocked',blocked)
        if self.mode=='menace_pair':
            self.prop(blocker,'blocking_count',1)
            self.prop('second_blocker','blocking_count',1)
            self.receipts()
            return
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        power,tough = self.stats(attacker)
        defender = 1-self.active
        if blocked:
            bp,bt = self.stats(blocker)
            trample = attacker=='source' and 'Trample' in self.model['keywords']
            if trample:
                self.life[defender] -= max(0,power-bt)
            for ref,damage,value in ((attacker,bp,tough),(blocker,min(power,bt) if trample else power,bt)):
                if damage>=value:
                    self.leave(ref,'graveyard')
                else:
                    self.entities[ref]['damage'] = damage
        else:
            self.life[defender] -= power
        self.receipts()

    def play(self):
        self.setup_cards()
        self.refresh_zero()
        mode = self.mode
        if mode in ('cast','cast_short','fresh_vehicle'):
            self.cast_permanent('source',mode!='cast_short')
            if mode!='fresh_vehicle':
                return
        if mode=='attack_uncrewed':
            self.declare_attacker('source',allowed=False)
            self.receipts()
            return
        if mode in DENIED:
            self.start_crew((),available=False)
            return
        if mode=='partial_cancel':
            self.start_crew(self.pilots[:1],cancel=True)
            return
        if mode=='fresh_pilot':
            self.cast_permanent(self.pilots[0])
            self.prop(self.pilots[0],'sick',1)
        if mode=='live_power':
            ref = self.pilots[0]
            self.emit('can_activate',ref='source',value=0)
            self.cast_spell('growth',ref)
            self.resolve()
            self.leave('growth','graveyard')
            self.entities[ref]['bonus'] = (3,3)
            self.receipts()
        if mode=='selector':
            for ref,valid in [('ready_extra',1),('enemy_pilot',0),('tapped_extra',0),('noncreature',0),('shroud',1),('source',0)]:
                self.emit('probe_activation_target',ref='source',other=ref,index=-1,value=valid)
        if mode=='shroud_pilot':
            self.emit('can_target_creature',ref='type_probe',other=self.pilots[0],value=0)
        if mode=='offturn_block':
            self.declare_attacker('attacker')
        self.start_crew(self.pilots)
        self.finish_crew()
        if mode=='expires':
            self.animated = False
            self.cleanup()
        elif mode=='recrew_self_only':
            self.emit('can_activate',ref='source',value=0)
        elif mode=='recrew_other':
            self.start_crew(self.other_pilots)
            self.finish_crew()
        elif mode=='offturn_block':
            self.finish_combat('attacker','source',blocked=True)
        elif mode in ('attack_combat','fresh_vehicle','flying_blocker','reach_blocker','menace_pair'):
            keys = self.model['keywords']
            allowed = mode!='fresh_vehicle' or 'Haste' in keys
            if mode=='fresh_vehicle':
                self.prop('source','sick',not allowed)
            self.declare_attacker('source',allowed=allowed)
            if allowed:
                blocked = mode in ('flying_blocker','reach_blocker','menace_pair') or not set(keys)&{'Flying','Menace'}
                self.finish_combat('source','blocker',blocked=blocked)
            else:
                self.receipts()


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if csvid!=pins()['sources'][name]['registry']['csvid']:
        raise ValueError('unreserved Vehicle')
    result = []
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        scene = CrewScene(name,model,mode,int(seat),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            suffix = steps[at:]
            steps = steps[:at]+[step('capture')]+suffix+[step('record_state'),step('ai_roundtrip'),step('restore')]+suffix+[step('assert_recorded_state')]
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                        covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=list(GAPS[1:] if mode=='overcrew' else GAPS))
        result.append(dict(name=f'VehicleCrewCombatRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=1864,
                           setup=scene.setup,steps=steps,_recipe=metadata))
    return result
