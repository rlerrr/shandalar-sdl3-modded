"""Devour based on real entry, with permanent keyword, mana and entry riders."""

import copy
import json
from functools import lru_cache
from pathlib import Path

from devour_entry_rewards_recipes import (
    BEAR, BLINK, COUNTER, DECK, DEVOUR, FACTORS, GIANT, GROW, DevourWorld,
)

FAMILY='devour_permanent_rewards'
PROFILE=Path(__file__).with_name('data')/'devour_permanent_rewards_wave8_profiles.json'
SQUARE=('Devour X, where X is the number of creatures devoured this way (As this creature enters, you may '
        'sacrifice any number of creatures. It enters with X +1/+1 counters on it for each of those creatures.)')
TRAMPLE='{G}: This creature gains trample until end of turn.'
HATCH='This creature has flying and trample if it devoured a creature.'
THRINAX=('Each other creature you control enters with an additional X +1/+1 counters on it, where X is '
         'the number of +1/+1 counters on this creature.')
HANDLERS={
    'flash':'manalinkeh_card_gluttonous_slime',
    'conditional':'manalinkeh_card_hellkite_hatchling',
    'haste':'manalinkeh_card_predator_dragon',
    'activation':'manalinkeh_card_thorn_thrash_viashino',
    'square':'manalinkeh_card_thromok_the_insatiable',
    'shared_counters':'manalinkeh_card_bloodspore_thrinax',
}
MEAL_TWO='Squire'
GROUND='Steel Wall'
REACH='Tree Monkey'
DRAW='Divination'
WITNESSES={BEAR,BLINK,COUNTER,GIANT,GROW,MEAL_TWO,GROUND,REACH,DRAW,*DECK}
VANILLA={BEAR,GIANT,MEAL_TWO,GROUND,REACH}
BASE=('none','paid_one','wrong_color','countered','snapshot')
RIDERS={
    'flash':('flash_response',),
    'conditional':('paid_ground','none_ground','paid_trample','none_trample'),
    'haste':('fresh_paid','fresh_none','fresh_reach'),
    'activation':('activate_trample','no_activation','wrong_activation','expired_trample'),
    'square':(),
    'shared_counters':('friendly_entry','enemy_entry','growth_entry','zero_entry'),
}
HUMAN={
    'square':('three','legend'),
    'shared_counters':('noncast_entry',),
}
GAPS=(
    'Positive AI payment uses ordinary creatures and the real chooser. The legacy preferred-sacrifice shortcut '
    'returns a slot without setting the target fields used by Devour.',
    'A pending Flash cast uses real announced spell context. Separate priority inside replacement processing '
    'is outside the test driver.',
)


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('complete normal permanent with Devour required')
    lines=record['oracle_text'].splitlines()
    if lines==[SQUARE] and record['type_line'].startswith('Legendary Creature — '):
        reward,multiplier,keywords='square',None,[]
    else:
        body=tuple(x for x in lines if DEVOUR.fullmatch(x) is None)
        shape={
            ('Flash',):('flash',1,['Flash']),
            (HATCH,):('conditional',1,[]),
            ('Flying, haste',):('haste',2,['Flying','Haste']),
            (TRAMPLE,):('activation',2,[]),
            (THRINAX,):('shared_counters',1,[]),
        }.get(body)
        if shape is None or not record['type_line'].startswith('Creature — '):
            raise ValueError('complete supported Devour rider required')
        reward,multiplier,keywords=shape
        matches=[m for text in lines if (m:=DEVOUR.fullmatch(text))]
        if len(matches)!=1 or int(matches[0][1])!=multiplier or FACTORS[matches[0][2]]!=multiplier:
            raise ValueError('exact printed Devour multiplier required')
    if record['keywords']!=keywords+['Devour']:
        raise ValueError('all printed Devour rider keywords required')
    return dict(reward=reward,multiplier=multiplier,keywords=keywords,mana=cost_from_oracle(record),
                power=int(record['power']),toughness=int(record['toughness']))


def variants(params):
    reward=params['reward']
    return [f'{m}_{p}' for p in (0,1) for m in BASE+RIDERS[reward]]+[
        m+'_0' for m in ('paid_two','partial','decline')+HUMAN.get(reward,())]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(params))):
        raise ValueError('distinct reviewed Devour rider variants required')


def spec(declaration, record):
    params=classify(record)
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or profiles()['sources'].get(record['name'],{}).get('oracle')!=record
            or declaration.get('oracle_text')!=record['oracle_text']
            or json.dumps(declaration.get('params'),sort_keys=True)!=json.dumps(params,sort_keys=True)):
        raise ValueError('typed Devour declaration must equal whole pinned Oracle')
    check_variants(params,declaration.get('variants'))
    return dict(params,family=FAMILY)


def validate_witnesses(oracle,rows):
    data=profiles()
    skipped={r['csvid'] for r in json.loads((PROFILE.parent/'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version')!=1 or not data.get('sources') or set(data.get('witnesses',()))!=WITNESSES:
        raise ValueError('exact reviewed Devour rider profile required')
    for group in ('sources','witnesses'):
        for name,pin in data[group].items():
            row,record=rows.get(name,{}),oracle.get(name,{})
            if (pin!=dict(oracle=record,registry=row) or row.get('kind')!='card' or record.get('layout')!='normal'
                    or row['csvid'] in skipped or (row['handler'] in (None,'NULL','0','card_dummy') and name not in VANILLA)):
                raise ValueError(f'normal implemented Devour rider pin required: {name}')
            if group=='sources' and row['handler']!=HANDLERS[classify(record)['reward']]:
                raise ValueError(f'changed Devour rider production handler: {name}')


class RiderWorld(DevourWorld):
    def configure(self):
        mode,owner,foe=self.mode,self.owner,self.foe
        self.other_counts={}
        self.add(self.name,'source',owner,'hand',source=True)
        self.flash_parent=mode=='flash_response'
        if self.flash_parent:
            self.current=foe
            self.add(DRAW,'parent',foe,'hand')
        if mode not in ('none','none_ground','none_trample','fresh_none','zero_entry'):
            self.add_meal(BEAR)
        if mode in ('paid_two','partial','decline','three'):
            self.add_meal(MEAL_TWO)
        if mode=='three':
            self.add_meal(GIANT)
        if mode!='decline':
            self.paid=list(self.meals[:1] if mode=='partial' else self.meals)
        if self.reward=='square':
            # The production steps are the same real choices. Only the Oracle
            # calculation differs: each of n selected creatures adds n counters.
            self.p['multiplier']=len(self.paid)
        if mode=='countered':
            self.add(COUNTER,'counter',foe,'hand')
        if self.reward in ('conditional','haste','activation'):
            if mode.endswith('ground') or mode in ('fresh_paid','fresh_none'):
                block=GROUND
            elif mode in ('paid_trample','none_trample','fresh_reach'):
                block=REACH
            else:
                block=GIANT
            self.add(block,'blocker',foe)
        if mode in ('friendly_entry','enemy_entry','growth_entry','zero_entry'):
            self.new_controller=foe if mode=='enemy_entry' else owner
            self.add(MEAL_TWO,'new_creature',self.new_controller,'hand')
        if mode=='growth_entry':
            self.add(GROW,'growth',owner,'hand')
        if mode=='noncast_entry':
            self.add(GIANT,'resident',owner)
            self.add(BLINK,'blink',owner,'hand')
        if mode=='legend':
            self.add(self.name,'second_legend',owner,'hand',source=True)
            self.add(MEAL_TWO,'late_meal',owner,'hand')
        for p in (0,1):
            for i,n in enumerate(DECK):
                self.add(n,f'library_{p}_{i}',p,'library')
        self.emit('turn',player=self.current,value=0)
        self.emit('present_ai_dialogs',value=1)
        for p in (0,1):
            self.pool(p,[0]*6)
        self.receipts()

    def receipts(self):
        start=len(self.steps)
        super().receipts()
        kept=[]
        for step in self.steps[start:]:
            ref=step.get('ref')
            if ref=='source' and step.get('property')=='sick' and 'Haste' in self.p['keywords']:
                # Haste is asserted through fresh attacker permission.
                continue
            if ref in self.other_counts and step.get('property')=='plus1_counters':
                step['value']=self.other_counts[ref]
            kept.append(step)
        self.steps[start:]=kept

    def plan(self,chosen):
        if self.mode=='noncast_entry':
            # The existing Thrinax witness also controls a future blink target.
            # Make one real selection, then stop with the resident still live.
            for ref in chosen:
                self.emit('choose_target',player=self.owner,ref=ref)
            self.emit('choose_cancel_target',player=self.owner)
        else:
            super().plan(chosen)

    def reward_now(self):
        if self.reward=='square' and self.counters==0:
            self.leave('source','graveyard')

    def flash_pending(self):
        from recipes import cost_from_oracle

        self.pool(self.foe,cost_from_oracle(self.o[DRAW]))
        self.emit('cast',ref='parent',value=1)
        self.emit('assert_stack',value=1)
        self.emit('announce_spell',ref='parent')
        self.mana[self.foe]=[0]*6
        self.pool(self.owner,self.p['mana'])
        self.emit('cast_response',ref='source',value=1)
        self.emit('assert_stack',value=2)
        self.emit('assert_unused_choices',value=0)
        self.mana[self.owner]=[0]*6

    def settle_flash(self):
        self.plan(self.paid)
        self.emit('resolve_response')
        self.emit('assert_unused_choices',value=0)
        self.arrive('source')
        self.sacrifice_now(self.paid)
        self.move_spell(DRAW,self.foe)
        deck=self.zones['library'][self.foe]
        self.zones['hand'][self.foe]+=deck[:2]
        del deck[:2]
        self.receipts()

    def fight(self,*,blocked,trample,age_first=True):
        if age_first:
            self.age()
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref='blocker',other='source',value=int(blocked))
        self.observe('blocker','blocking_count',blocked)
        self.emit('advance_combat')
        self.observe('source','blocked',blocked)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        attacker,defender=self.entities['source'],self.entities['blocker']
        attacker['tapped']=True
        if blocked:
            dealt=attacker['power']
            incoming=defender['power']
            if trample:
                excess=max(0,dealt-defender['toughness'])
                self.life[self.foe]-=excess
                dealt-=excess
            self.damage('source',incoming)
            self.damage('blocker',dealt)
        else:
            self.life[self.foe]-=attacker['power']
        self.receipts()

    def activation(self, paid):
        from recipes import COLORS

        color=COLORS['G' if paid else 'R']
        pool=[0]*6
        pool[color]=1
        self.pool(self.owner,pool)
        self.emit('can_activate',ref='source',value=int(paid))
        self.emit('activate',ref='source',value=int(paid))
        self.emit('assert_stack',value=int(paid))
        if paid:
            self.emit('resolve')
            self.mana[self.owner]=[0]*6
        self.receipts()

    def cast_witness(self, name, ref, player, target=None):
        from recipes import cost_from_oracle

        self.pool(player,cost_from_oracle(self.o[name]))
        if target:
            self.emit('choose_target',player=player,ref=target)
        self.emit('cast',ref=ref,value=1)
        self.emit('assert_stack',value=1)
        self.emit('resolve')
        self.emit('assert_unused_choices',value=0)
        self.mana[player]=[0]*6

    def grant_counters(self,ref,count):
        self.other_counts[ref]=count
        for stat in ('power','toughness'):
            self.entities[ref][stat]+=count

    def observe_later_entry(self):
        if self.mode=='noncast_entry':
            self.cast_witness(BLINK,'blink',self.owner,'resident')
            self.move_spell(BLINK,self.owner)
            self.emit('bind_permanent',ref='resident',player=self.owner,value=self.rows[GIANT]['csvid'])
            self.entities['resident']['sick']=True
            self.grant_counters('resident',self.counters)
        else:
            if self.mode=='growth_entry':
                # The real meal just died this turn, so Hunger uses Morbid.
                self.cast_witness(GROW,'growth',self.owner,'source')
                self.move_spell(GROW,self.owner)
                self.counters+=3
                for stat in ('power','toughness'):
                    self.entities['source'][stat]+=3
                self.receipts()
            if self.new_controller!=self.current:
                self.next_turn()
            self.cast_witness(MEAL_TWO,'new_creature',self.new_controller)
            self.arrive('new_creature')
            self.grant_counters('new_creature',self.counters if self.new_controller==self.owner else 0)
        self.receipts()

    def choose_legend(self):
        self.cast_witness(MEAL_TWO,'late_meal',self.owner)
        self.arrive('late_meal')
        self.receipts()
        self.pool(self.owner,self.p['mana'])
        self.emit('cast',ref='second_legend',value=1)
        self.emit('assert_stack',value=1)
        self.emit('choose_target',player=self.owner,ref='late_meal')
        self.emit('choose_cancel_target',player=self.owner)
        self.emit('choose_target',player=self.owner,ref='source')
        self.emit('resolve')
        self.emit('assert_unused_choices',value=0)
        self.mana[self.owner]=[0]*6
        self.leave('late_meal','graveyard')
        self.leave('source','graveyard')
        self.arrive('second_legend')
        self.grant_counters('second_legend',1)
        self.receipts()


def scenario(world):
    mode=world.mode
    if world.flash_parent:
        world.flash_pending()
        world.settle_flash()
        return None
    if not world.pending():
        return None
    cut=len(world.steps) if mode=='snapshot' else None
    world.settle()
    if mode in ('paid_ground','none_ground','paid_trample','none_trample'):
        world.fight(blocked=mode!='paid_ground',trample=mode=='paid_trample')
    elif mode in ('fresh_paid','fresh_none','fresh_reach'):
        world.fight(blocked=mode=='fresh_reach',trample=False,age_first=False)
    elif mode in ('activate_trample','wrong_activation','no_activation','expired_trample'):
        world.age()
        if mode!='no_activation':
            world.activation(mode!='wrong_activation')
        if mode!='wrong_activation':
            world.fight(blocked=True,trample=mode=='activate_trample',age_first=mode=='expired_trample')
    elif mode in ('friendly_entry','enemy_entry','growth_entry','zero_entry','noncast_entry'):
        world.observe_later_entry()
    elif mode=='legend':
        world.choose_legend()
    return cut


def expand(name,csvid,declaration,params,selected,rows,oracle):
    from recipes import step
    check_variants(params,selected)
    validate_witnesses(oracle,rows)
    if profiles()['sources'][name]['registry']['csvid']!=csvid:
        raise ValueError('exact Devour rider CSVID required')
    cases=[]
    for variant in selected:
        mode,owner=variant.rsplit('_',1)
        world=RiderWorld(name,params,mode,int(owner),rows,oracle)
        cut=scenario(world)
        steps=world.steps
        if cut is not None:
            tail=steps[cut:]
            steps=steps[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+\
                  copy.deepcopy(tail)+[step('assert_recorded_state')]
        cases.append(dict(name=f'DevourPermanentRewardsRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                          seed=193,setup=world.setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,
                              params=declaration['params'],oracle_text=declaration['oracle_text'],untested_components=list(GAPS))))
    return cases

