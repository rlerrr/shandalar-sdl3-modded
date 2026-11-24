"""Crew and paid, blocking, or end-of-combat Vehicle riders."""

from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from vehicle_crew_combat_recipes import CrewScene, PILOT, record_hash


FAMILY = 'vehicle_crew_utility'
PROFILE = Path(__file__).parent/'data/vehicle_crew_utility_wave8_profiles.json'
CREW = re.compile(r'Crew ([1-5]) \(Tap any number of creatures you control with total power \1 or more: This Vehicle becomes an artifact creature until end of turn\.\)')
RIDERS = {
    '{T}: Add one mana of any color.': 'palette',
    "This Vehicle can't be blocked by creatures with power 2 or less.": 'block_floor',
    'At end of combat, if this Vehicle attacked or blocked this combat, put a velocity counter on it. Then if it has two or more velocity counters on it, sacrifice it and draw two cards.': 'velocity',
    '{1}{W}: Another target Vehicle you control becomes an artifact creature until end of turn.': 'animate_other',
}
COMMON = ('cast','cast_short','ordered_exact','underpowered','partial_cancel','expires','pending_replay')
PALETTE = {'mana_black':1,'mana_blue':2,'mana_green':3,'mana_red':4,'mana_white':5,
           'fresh_mana':3,'crewed_mana':4,'fresh_crewed_denied':2,'spend_blue':2,'both_mana':5}
PAID = ('other_value','other_combat','other_fresh','other_expiry','other_replay','both_other',
        'other_short','other_wrong_color','other_enemy','other_nonvehicle','other_self_only','other_target_lost','other_source_lost')
DENIED_PAID = ('other_short','other_wrong_color','other_enemy','other_nonvehicle','other_self_only')
MODES = {
    'palette': (*PALETTE,'tapped_mana_denied','crew_tapped'),
    'block_floor': ('power_one','power_two','power_three','power_rises','power_falls','block_replay'),
    'velocity': ('attack_once','block_once','no_participation','attack_twice','attack_then_block','attack_replay'),
    'animate_other': (*PAID,'both_crew'),
}
GAPS = ['Multiplayer','Voluntary additional Crew selections after the native menu threshold']
EFFECT_GAPS = {
    'velocity': ['Draw replacement effects','Empty-library loss','Removing the source while the end-of-combat trigger is pending'],
    'animate_other': ['Control change during the targeted activation'],
}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line')!='Artifact — Vehicle' or set(record['keywords'])!={'Crew'}):
        raise ValueError('normal whole Vehicle required')
    rider, _, crew = record['oracle_text'].rpartition('\n')
    match = CREW.fullmatch(crew)
    effect = RIDERS.get(rider)
    if not effect or not match or not all(re.fullmatch(r'[1-9][0-9]*',record.get(k,'')) for k in ('power','toughness')):
        raise ValueError('exact Vehicle rider and Crew clause required')
    result = dict(effect=effect,cost=cost_from_oracle(record),threshold=int(match[1]),
                  power=int(record['power']),toughness=int(record['toughness']),keywords=[])
    if effect=='animate_other':
        result['alternate_cost'] = cost_from_oracle({'mana_cost':rider.split(': ',1)[0]})
    return result


def variants(model):
    return [f'{mode}_{seat}' for mode in (*COMMON,*MODES[model['effect']]) for seat in (0,1)]


def spec(declaration,record):
    model = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),model)
            or pins()['sources'].get(record['name'],{}).get('oracle_record_sha256')!=record_hash(record)
            or type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('pinned whole Vehicle and exact typed variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle,rows):
    profile = pins()
    if profile['version']!=1:
        raise ValueError('unreviewed Vehicle utility profile')
    for group in ('sources','witnesses'):
        for name,pin in profile[group].items():
            row,record = rows.get(name,{}),oracle.get(name,{})
            if (pin['oracle_record_sha256']!=record_hash(record) or not exact(pin['registry'],row)
                    or row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')
                    or record.get('layout')!='normal'):
                raise ValueError(f'changed or unsupported Vehicle mechanic record: {name}')


class UtilityScene(CrewScene):
    def __init__(self,name,model,mode,owner,rows,oracle):
        super().__init__(name,model,mode,owner,rows,oracle)
        self.velocity = 0
        self.other_animated = False
        if mode=='block_once':
            self.active = self.foe

    def setup_cards(self):
        mode,effect = self.mode,self.effect
        in_hand = mode in ('cast','cast_short','fresh_mana','fresh_crewed_denied')
        tapped = effect=='palette' and mode in ('underpowered','tapped_mana_denied','crew_tapped')
        self.add(self.name,'source',self.owner,'hand' if in_hand else 'battlefield',tapped=tapped)
        self.add('Unsummon','type_probe',self.foe,'hand')
        if (mode not in ('cast','cast_short','tapped_mana_denied',*PAID,*PALETTE)
                or mode in ('both_other','both_mana','crewed_mana','fresh_crewed_denied')):
            if self.model['threshold']==5:
                powers = (4,) if mode=='underpowered' else (2,3)
                for power in powers:
                    self.add_pilot(PILOT[power])
            else:
                self.setup_cost(self.model['threshold'])
        if mode=='fresh_crewed_denied':
            self.add_pilot(PILOT[3],secondary=True)
        if mode=='spend_blue':
            self.add('Unsummon','spend',self.owner,'hand')
            self.add('Mogg Fanatic','victim',self.foe)
        if effect=='block_floor' and mode in MODES['block_floor']:
            power = {'power_one':1,'power_three':3,'power_falls':3}.get(mode,2)
            self.add(PILOT[power],'blocker',self.foe)
            if mode in ('power_rises','power_falls'):
                self.add('Giant Growth' if mode=='power_rises' else 'Disfigure','change',self.foe,'hand')
        if effect=='velocity':
            for i,card in enumerate(('Island','Plains','Forest')):
                self.add(card,f'library_{i}',self.owner,'library')
            if mode in ('block_once','attack_then_block','no_participation'):
                self.add('Mogg Fanatic','attacker',self.owner if mode=='no_participation' else self.foe)
            if mode=='attack_then_block':
                self.add('Twiddle','untap',self.owner,'hand')
                for _ in range(2):
                    self.add_pilot(PILOT[1],secondary=True)
        if (mode in PAID and mode!='other_self_only') or mode=='both_crew':
            self.add('Darksteel Relic' if mode=='other_nonvehicle' else 'Sleek Schooner','recipient',
                     self.foe if mode=='other_enemy' else self.owner,'hand' if mode=='other_fresh' else 'battlefield')
        if mode in ('other_target_lost','other_source_lost'):
            self.add('Boomerang','bounce',self.foe,'hand')

    def source_traits(self,full=False):
        super().source_traits(full)
        if self.effect=='velocity':
            self.emit('assert_counter',ref='source',counter_kind='velocity',value=self.velocity)

    def recipient_traits(self):
        target = self.entities.get('recipient')
        if target is None or target['zone']!='battlefield':
            return
        self.emit('can_target_creature',ref='type_probe',other='recipient',value=int(self.other_animated))
        if self.other_animated:
            for key,value in zip(('power','toughness'),self.stats('recipient')):
                self.prop('recipient',key,value)

    def receipts(self):
        super().receipts()
        self.recipient_traits()

    def start_crew(self,plan,*,available=True,cancel=False):
        at = len(self.steps)
        result = super().start_crew(plan,available=available,cancel=cancel)
        if available and (self.effect=='palette' or self.effect=='animate_other' and (self.owner==1 or self.mode=='both_crew')):
            from recipes import step

            self.steps.insert(at+1,step('choose_dialog',player=self.owner,value=1))
        return result

    def mana_activation(self):
        mode = self.mode
        color = PALETTE[mode]
        denied = mode=='fresh_crewed_denied'
        self.emit('can_activate',ref='source',value=1)
        self.emit('choose_dialog',player=self.owner,value=0)
        self.emit('choose_color',player=self.owner,value=color)
        self.emit('activate',ref='source',value=int(not denied))
        if denied:
            self.emit('assert_unused_choices',value=1)
        else:
            self.mana[self.owner][color] += 1
            self.entities['source']['tapped'] = True
        self.receipts()
        if mode=='spend_blue':
            self.emit('choose_target',ref='victim',player=self.owner)
            self.emit('cast',ref='spend',value=1)
            self.emit('assert_stack',value=1)
            self.mana[self.owner][color] -= 1
            self.pools()
            self.resolve()
            self.leave('spend','graveyard')
            self.leave('victim','hand')
            self.receipts()

    def stomp_combat(self):
        mode = self.mode
        self.declare_attacker('source')
        if mode in ('power_rises','power_falls'):
            self.cast_spell('change','blocker')
            self.resolve()
            self.leave('change','graveyard')
            self.entities['blocker']['bonus'] = (3,3) if mode=='power_rises' else (-2,-2)
            self.receipts()
        if mode=='block_replay':
            self.replay = len(self.steps)
        self.finish_combat('source','blocker',blocked=self.stats('blocker')[0]>2)

    def dragster_end(self,attacker,*,blocked=False,qualified=True):
        self.emit('begin_blockers')
        if blocked:
            self.emit('block',ref='source',other=attacker,value=1)
        self.emit('advance_combat')
        self.prop(attacker,'blocked',int(blocked))
        self.receipts()
        self.emit('combat_damage')
        self.resolve()
        if blocked:
            self.entities['source']['damage'] = self.stats(attacker)[0]
            self.leave(attacker,'graveyard')
        else:
            self.life[1-self.active] -= self.stats(attacker)[0]
        self.receipts()
        self.emit('end_combat')
        if qualified:
            self.velocity += 1
            if self.velocity>=2:
                self.leave('source','graveyard')
                self.animated = False
                for ref in ('library_0','library_1'):
                    self.entities[ref]['zone'] = 'hand'
        self.resolve()
        self.receipts()

    def next_main(self,active):
        self.animated = False
        self.emit('advance_turn')
        self.emit('untap_step',player=active)
        self.emit('turn',player=active,value=0)
        self.active = active
        for entity in self.entities.values():
            entity['damage'] = 0
            if entity['zone']=='battlefield' and entity['controller']==active:
                entity['tapped'] = False
        self.receipts()

    def dragster_combat(self):
        mode = self.mode
        if mode=='no_participation':
            self.declare_attacker('attacker')
            self.dragster_end('attacker',qualified=False)
            return
        if mode=='attack_replay':
            self.replay = len(self.steps)
        self.declare_attacker('source')
        self.dragster_end('source')
        if mode not in ('attack_twice','attack_then_block'):
            return
        self.next_main(self.foe)
        if mode=='attack_twice':
            self.next_main(self.owner)
            self.start_crew(self.pilots)
            self.finish_crew()
            self.declare_attacker('source')
            self.dragster_end('source')
        else:
            # The opposing turn does not untap the Dragster that attacked.
            self.cast_spell('untap','source')
            self.emit('choose_dialog',player=self.owner,value=1)
            self.resolve()
            self.leave('untap','graveyard')
            self.entities['source']['tapped'] = False
            self.receipts()
            self.declare_attacker('attacker')
            self.start_crew(self.other_pilots)
            self.finish_crew()
            self.dragster_end('attacker',blocked=True)

    def outside_animation(self):
        mode = self.mode
        fee = self.model['alternate_cost']
        budget = list(fee)
        if mode=='other_short':
            budget[0] -= 1
        if mode=='other_wrong_color':
            budget[5] -= 1
            budget[1] += 1
        if mode=='other_fresh':
            self.cast_permanent('recipient')
        self.fund(self.owner,budget)
        self.pools()
        valid = mode not in DENIED_PAID
        self.emit('can_activate',ref='source',value=int(valid))
        if mode=='other_replay':
            self.replay = len(self.steps)
        if valid:
            if self.owner==1 or mode=='both_other':
                self.emit('choose_dialog',player=self.owner,value=0)
            self.emit('choose_target',ref='recipient',player=self.owner)
        self.emit('activate',ref='source',value=int(valid))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(valid))
        if not valid:
            self.receipts()
            return
        self.pay(self.owner,fee)
        self.pools()
        self.prop('source','tapped',0)
        self.recipient_traits()
        if mode in ('other_target_lost','other_source_lost'):
            ref = 'recipient' if mode=='other_target_lost' else 'source'
            self.bounce_response(ref)
            self.leave('bounce','graveyard')
            self.leave(ref,'hand')
        else:
            self.resolve()
        self.other_animated = mode!='other_target_lost'
        self.receipts()
        if mode in ('other_combat','other_fresh'):
            allowed = mode!='other_fresh'
            self.declare_attacker('recipient',allowed)
            if allowed:
                self.finish_combat('recipient',None,blocked=False)
            else:
                self.receipts()
        if mode in ('other_expiry','other_combat'):
            self.other_animated = False
            self.cleanup()

    def play(self):
        self.setup_cards()
        self.refresh_zero()
        mode = self.mode
        if mode in ('cast','cast_short','fresh_mana','fresh_crewed_denied'):
            self.cast_permanent('source',mode!='cast_short')
            if mode in ('cast','cast_short'):
                return
        if mode in PAID:
            self.outside_animation()
            return
        if mode=='tapped_mana_denied':
            self.emit('can_activate',ref='source',value=0)
            self.emit('activate',ref='source',value=0)
            self.receipts()
            return
        if mode in PALETTE and mode not in ('crewed_mana','fresh_crewed_denied'):
            self.mana_activation()
            return
        if mode=='underpowered':
            self.start_crew((),available=False)
            return
        if mode=='partial_cancel':
            self.start_crew(self.pilots[:1],cancel=True)
            return
        if mode=='both_crew':
            self.fund(self.owner,self.model['alternate_cost'])
        if mode=='block_once':
            self.declare_attacker('attacker')
        self.start_crew(self.pilots)
        self.finish_crew()
        if mode=='expires':
            self.animated = False
            self.cleanup()
        elif mode in ('crewed_mana','fresh_crewed_denied'):
            self.mana_activation()
        elif self.effect=='block_floor' and mode in MODES['block_floor']:
            self.stomp_combat()
        elif mode=='block_once':
            self.dragster_end('attacker',blocked=True)
        elif self.effect=='velocity' and mode in MODES['velocity']:
            self.dragster_combat()


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if csvid!=pins()['sources'][name]['registry']['csvid'] or set(selected)-set(variants(model)):
        raise ValueError('unreserved Vehicle utility record or variant')
    result = []
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        scene = UtilityScene(name,model,mode,int(seat),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            cut = scene.replay
            suffix = steps[cut:]
            steps = steps[:cut]+[step('capture')]+suffix+[step('record_state'),step('ai_roundtrip'),step('restore')]+suffix+[step('assert_recorded_state')]
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                        covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=[*GAPS,*EFFECT_GAPS.get(model['effect'],[])])
        result.append(dict(name=f'VehicleCrewUtilityRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=1937,
                           setup=scene.setup,steps=steps,_recipe=metadata))
    return result
