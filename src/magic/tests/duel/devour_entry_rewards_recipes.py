"""Devour as-entry choices, printed counters, and card-specific actual rewards."""

import copy
import json
import re
from functools import lru_cache
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld

FAMILY='devour_entry_rewards'
PROFILE=Path(__file__).with_name('data')/'devour_entry_rewards_wave8_profiles.json'
DEVOUR=re.compile(r'Devour ([123]) \(As this creature enters, you may sacrifice any number of creatures\. '
                  r'It enters with (that many|twice that many|three times that many) \+1/\+1 counters on it\.\)')
FACTORS={'that many':1,'twice that many':2,'three times that many':3}
REWARDS={
    'When this creature enters, it deals 3 damage to each creature.':'sweep',
    'When this creature enters, draw a card for each creature it devoured.':'draw',
    'When this creature enters, target player discards a card for each creature it devoured.':'discard',
    'When this creature enters, it deals damage to any target equal to twice the number of Goblins it devoured.':'goblin_damage',
    'At the beginning of your upkeep, create a 1/1 green Saproling creature token for each +1/+1 counter on this creature.':'upkeep',
    '':'counters',
}
HANDLERS={
    'sweep':'manalinkeh_card_caldera_hellion','draw':'manalinkeh_card_skullmulcher',
    'discard':'manalinkeh_card_tar_fiend','goblin_damage':'manalinkeh_card_voracious_dragon',
    'upkeep':'manalinkeh_card_mycoloth','counters':'manalinkeh_card_thunder_thrash_elder',
}
BEAR,GIANT,WURM,SPIDER='Grizzly Bears','Hill Giant','Scaled Wurm','Giant Spider'
GOBLIN,PIKER,SHROUD="Mons's Goblin Raiders",'Goblin Piker','Blurred Mongoose'
COUNTER,BLINK,GROW,KILL='Counterspell','Cloudshift','Hunger of the Howlpack','Murder'
DECK=('Forest','Island','Mountain','Plains','Swamp')
TOKEN='Saproling'
TOKEN_ID=18813
WITNESSES={BEAR,GIANT,WURM,SPIDER,GOBLIN,PIKER,SHROUD,COUNTER,BLINK,GROW,KILL,*DECK}
VANILLA={BEAR,GIANT,WURM,SPIDER,GOBLIN,PIKER}
BOTH=('none','paid_one','countered','wrong_color','snapshot')
HUMAN=('paid_two','partial','decline','borrowed_meal')
MODE_BOTH={
    'upkeep':('upkeep_repeat','external_counter','source_removed'),
    'goblin_damage':('fly_ground','fly_reach','zero_goblins'),
    'counters':('blink_entry',),
}
MODE_HUMAN={
    'sweep':('sweep_scope',),'discard':('short_hand','target_self','empty_hand'),
    'goblin_damage':('two_goblins','creature_recipient'),'counters':('shroud_meal',),
}
GAPS=(
    'The documented AI preferred-sacrifice shortcut returns a card without filling the legacy Devour target slot. '
    'Both-seat positive fixtures have safe ordinary creatures and use the same actual chooser as the human.',
    'The driver drains the entry and its payoff together, so no independent entry-trigger response window is asserted.',
)


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line','').startswith('Creature — ')):
        raise ValueError('complete normal Devour creature required')
    lines=record['oracle_text'].splitlines()
    flying=lines[:1]==['Flying']
    if flying:
        lines=lines[1:]
    match=DEVOUR.fullmatch(lines[0]) if lines else None
    reward=REWARDS.get(lines[1] if len(lines)==2 else '') if len(lines) in (1,2) else None
    if (not match or FACTORS[match[2]]!=int(match[1]) or reward is None
            or record.get('keywords')!=(['Flying','Devour'] if flying else ['Devour'])):
        raise ValueError('complete reviewed Devour amount, rider and reward required')
    return dict(reward=reward, multiplier=int(match[1]), mana=cost_from_oracle(record),
                keywords=['Flying'] if flying else [],power=int(record['power']),toughness=int(record['toughness']))


def variants(params):
    reward=params['reward']
    return ([f'{m}_{p}' for p in (0,1) for m in BOTH+MODE_BOTH.get(reward,())]
            +[m+'_0' for m in HUMAN+MODE_HUMAN.get(reward,())])


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(params))):
        raise ValueError('distinct reviewed Devour variants required')


def spec(declaration,record):
    params=classify(record)
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or profiles()['sources'].get(record['name'],{}).get('oracle')!=record
            or declaration.get('oracle_text')!=record['oracle_text']
            or json.dumps(declaration.get('params'),sort_keys=True)!=json.dumps(params,sort_keys=True)):
        raise ValueError('exact typed Devour declaration with whole pinned Oracle required')
    check_variants(params,declaration.get('variants'))
    return dict(params,family=FAMILY)


def validate_witnesses(oracle, rows):
    data=profiles()
    skipped={r['csvid'] for r in json.loads((PROFILE.parent/'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if (data.get('version')!=1 or not data.get('sources') or set(data.get('witnesses',()))!=WITNESSES
            or set(data.get('tokens',()))!={TOKEN}):
        raise ValueError('exact reviewed Devour profile required')
    for group in ('sources','witnesses','tokens'):
        for name,pin in data[group].items():
            row,record=rows.get(name,{}),oracle.get(name,{})
            if pin!=dict(oracle=record,registry=row):
                raise ValueError(f'changed Devour pin: {name}')
            if group=='tokens':
                if row['csvid']!=TOKEN_ID or record.get('layout')!='token':
                    raise ValueError('exact physical Saproling ID required')
                continue
            if (row.get('kind')!='card' or record.get('layout')!='normal' or row['csvid'] in skipped
                    or row['handler'] in (None,'NULL','0','card_dummy') and name not in VANILLA):
                raise ValueError(f'normal implemented Devour input required: {name}')
            if group=='sources':
                params=classify(record)
                expected=HANDLERS[params['reward']]
                if params['reward']=='counters' and params['multiplier']==1:
                    expected='manalinkeh_card_gluttonous_slime'
                if row['handler']!=expected:
                    raise ValueError(f'changed Devour production handler: {name}')


class DevourWorld(_AttackResourceWorld):
    def __init__(self,name,params,mode,owner,rows,oracle):
        super().__init__(name,dict(params,effect=FAMILY),owner,rows,oracle)
        self.mode=mode
        self.reward=params['reward']
        self.foe=1-owner
        self.counters=0
        self.meals=[]
        self.paid=[]
        self.discard_player=owner if mode=='target_self' else self.foe
        self.configure()

    def add_meal(self,name):
        ref=f'meal_{len(self.meals)}'
        self.add(name,ref,self.owner)
        self.meals.append(ref)

    def configure(self):
        mode,owner,foe=self.mode,self.owner,self.foe
        self.add(self.name,'source',owner,'hand',source=True)
        if mode not in ('none','external_counter'):
            first=GOBLIN if self.reward=='goblin_damage' and mode!='zero_goblins' else (
                SHROUD if mode=='shroud_meal' else BEAR)
            self.add_meal(first)
        if mode in ('paid_two','partial','decline','short_hand','two_goblins','creature_recipient'):
            self.add_meal(PIKER if mode in ('two_goblins','creature_recipient') else BEAR)
        if mode=='borrowed_meal':
            self.setup[-1]['state']=['opponent_owned']
        if mode not in ('decline','none','external_counter','blink_entry'):
            self.paid=list(self.meals[:1] if mode=='partial' else self.meals)
        if mode=='sweep_scope':
            self.add(SPIDER,'friendly_guard',owner)
        self.add(SPIDER if mode=='fly_reach' else GIANT,'foe_creature',foe)
        if self.reward=='sweep':
            self.add(WURM,'resistant',foe)
        if self.reward=='discard':
            hand=() if mode=='empty_hand' else ('Forest',) if mode=='short_hand' else (
                ('Forest','Island','Mountain') if self.discard_player==0 else ('Forest',)*3)
            for i,name in enumerate(hand):
                self.add(name,f'discard_{i}',self.discard_player,'hand')
        if mode=='countered':
            self.add(COUNTER,'counter',foe,'hand')
        if mode=='blink_entry':
            self.add(BLINK,'blink',owner,'hand')
        if mode=='external_counter':
            self.add(GROW,'growth',owner,'hand')
        if mode=='source_removed':
            self.add(KILL,'murder',foe,'hand')
        for p in (0,1):
            for i,n in enumerate(DECK):
                self.add(n,f'library_{p}_{i}',p,'library')
        self.emit('turn',player=owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        if self.reward=='discard':
            self.emit('discard_policy',value=1)
        for p in (0,1):
            self.pool(p,[0]*6)
        self.receipts()

    def observe(self,ref,prop,value):
        self.emit('assert_card',ref=ref,property=prop,value=int(value))

    def receipts(self):
        self.emit('refresh')
        start=len(self.steps)
        super().receipts()
        for step in self.steps[start:]:
            if step.get('ref')=='source' and step.get('property')=='plus1_counters':
                step['value']=self.counters
        if self.reward=='upkeep':
            tokens=self.zones['battlefield'][self.owner].count(TOKEN)
            for p in (0,1):
                for zone in ('battlefield','graveyard','exile'):
                    self.emit('assert_zone_multiplicity',player=p,zone=zone,index=TOKEN_ID,
                              value=tokens if p==self.owner and zone=='battlefield' else 0)
            if tokens:
                for prop,value in (('token',1),('power',1),('toughness',1),('color',8),
                                   ('subtype_Saproling',1),('artifact',0)):
                    self.emit('assert_matching_permanents',player=self.owner,index=TOKEN_ID,property=prop,value=value)

    def pending(self):
        denied=self.mode=='wrong_color'
        self.pool(self.owner,[sum(self.p['mana']),0,0,0,0,0] if denied else self.p['mana'])
        self.emit('cast',ref='source',value=int(not denied))
        if not denied:
            self.mana[self.owner]=[0]*6
        self.emit('assert_stack',value=int(not denied))
        self.emit('assert_unused_choices',value=0)
        self.observe('source','in_hand',denied)
        self.observe('source','in_play',0)
        for color,available in enumerate(self.mana[self.owner]):
            self.emit('assert_mana',player=self.owner,color=color,value=available)
        if denied:
            self.receipts()
        return not denied

    def move_spell(self,name,player):
        self.zones['hand'][player].remove(name)
        self.zones['graveyard'][player].append(name)

    def counter(self):
        from recipes import cost_from_oracle

        self.emit('announce_spell',ref='source')
        self.pool(self.foe,cost_from_oracle(self.o[COUNTER]))
        self.emit('cast_response',ref='counter',value=1)
        self.emit('assert_stack',value=2)
        self.emit('resolve_response')
        self.emit('assert_unused_choices',value=0)
        self.mana[self.foe]=[0]*6
        self.move_spell(COUNTER,self.foe)
        self.leave('source','graveyard')
        self.receipts()

    def plan(self,chosen):
        for ref in chosen:
            self.emit('choose_target',player=self.owner,ref=ref)
        candidates=len(self.meals)+int(self.mode=='sweep_scope')
        if len(chosen)<candidates:
            self.emit('choose_cancel_target',player=self.owner)

    def sacrifice_now(self,chosen):
        for ref in reversed(chosen):
            e=self.entities[ref]
            self.leave(ref,'graveyard')
            if self.mode=='borrowed_meal':
                self.zones['graveyard'][self.owner].remove(e['name'])
                self.zones['graveyard'][self.foe].append(e['name'])
        self.counters=len(chosen)*self.p['multiplier']
        self.entities['source'].update(power=self.p['power']+self.counters,toughness=self.p['toughness']+self.counters)

    def damage(self,ref,amount):
        e=self.entities[ref]
        e['damage']+=amount
        if e['damage']>=e['toughness']:
            self.leave(ref,'graveyard')

    def plan_reward(self):
        if self.reward=='discard':
            # A target player is still chosen for the printed zero result.
            self.emit('choose_player',player=self.owner,value=self.discard_player)
        elif self.reward=='goblin_damage':
            if self.mode=='creature_recipient':
                self.emit('choose_target',player=self.owner,ref='foe_creature')
            else:
                self.emit('choose_player',player=self.owner,value=self.foe)

    def reward_now(self):
        if self.reward=='sweep':
            for ref,e in self.entities.items():
                if e['zone']=='battlefield':
                    self.damage(ref,3)
        elif self.reward=='draw':
            library=self.zones['library'][self.owner]
            count=len(self.paid)
            self.zones['hand'][self.owner]+=library[:count]
            del library[:count]
        elif self.reward=='discard':
            seat=self.discard_player
            ordered=sorted(self.zones['hand'][seat],key=lambda n:self.rows[n]['csvid'])
            for n in ordered[:len(self.paid)]:
                self.move_spell(n,seat)
        elif self.reward=='goblin_damage':
            amount=2*sum('Goblin' in self.o[self.entities[ref]['name']]['type_line'].split(' — ')[1].split()
                         for ref in self.paid)
            if self.mode=='creature_recipient':
                self.damage('foe_creature',amount)
            else:
                self.life[self.foe]-=amount

    def settle(self):
        if self.mode=='countered':
            self.counter()
            return
        self.plan(self.paid)
        self.plan_reward()
        self.emit('resolve')
        self.emit('assert_unused_choices',value=0)
        self.arrive('source')
        self.sacrifice_now(self.paid)
        self.reward_now()
        self.receipts()

    def spell_at_source(self,name,ref,caster,before_resolve=None):
        from recipes import cost_from_oracle

        self.pool(caster,cost_from_oracle(self.o[name]))
        self.emit('choose_target',player=caster,ref='source')
        self.emit('cast',ref=ref,value=1)
        self.emit('assert_stack',value=1)
        if before_resolve is not None:
            before_resolve()
        self.emit('resolve')
        self.emit('assert_unused_choices',value=0)
        self.mana[caster]=[0]*6
        self.move_spell(name,caster)

    def blink(self):
        self.spell_at_source(BLINK,'blink',self.owner,lambda:self.plan(self.meals))
        self.sacrifice_now(self.meals)
        self.emit('bind_permanent',ref='source',player=self.owner,value=self.rows[self.name]['csvid'])
        self.receipts()

    def upkeep(self):
        if self.mode=='external_counter':
            self.spell_at_source(GROW,'growth',self.owner)
            self.counters+=1
            self.entities['source']['power']+=1
            self.entities['source']['toughness']+=1
            self.receipts()
        if self.mode=='source_removed':
            self.spell_at_source(KILL,'murder',self.foe)
            self.leave('source','graveyard')
            self.receipts()
        for _ in range(4 if self.mode=='upkeep_repeat' else 2):
            self.emit('advance_turn')
            self.expire()
            self.current=1-self.current
            self.emit('untap_step',player=self.current)
            for e in self.entities.values():
                if e['zone']=='battlefield' and e['player']==self.current:
                    e.update(tapped=False,sick=False)
            self.receipts()
            self.emit('upkeep_step',player=self.current)
            if self.current==self.owner and self.entities['source']['zone']=='battlefield':
                self.zones['battlefield'][self.owner]+=[TOKEN]*self.counters
            self.emit('assert_unused_choices',value=0)
            self.receipts()
            self.emit('turn',player=self.current,value=0)

    def fly(self):
        self.age()
        blocked=self.mode=='fly_reach'
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref='foe_creature',other='source',value=int(blocked))
        self.observe('foe_creature','blocking_count',blocked)
        self.emit('advance_combat')
        self.observe('source','blocked',blocked)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.entities['source']['tapped']=True
        if blocked:
            self.damage('source',self.entities['foe_creature']['power'])
            self.damage('foe_creature',self.entities['source']['power'])
        else:
            self.life[self.foe]-=self.entities['source']['power']
        self.receipts()


def scenario(world):
    if not world.pending():
        return None
    cut=len(world.steps) if world.mode=='snapshot' else None
    world.settle()
    if world.mode=='countered':
        return None
    if world.reward=='upkeep':
        world.upkeep()
    elif world.mode=='blink_entry':
        world.blink()
    elif world.mode in ('fly_ground','fly_reach'):
        world.fly()
    return cut


def expand(name,csvid,declaration,params,selected,rows,oracle):
    from recipes import step
    check_variants(params,selected)
    validate_witnesses(oracle,rows)
    if profiles()['sources'][name]['registry']['csvid']!=csvid:
        raise ValueError('exact Devour source CSVID required')
    cases=[]
    for variant in selected:
        mode,owner=variant.rsplit('_',1)
        world=DevourWorld(name,params,mode,int(owner),rows,oracle)
        cut=scenario(world)
        steps=world.steps
        if cut is not None:
            tail=steps[cut:]
            steps=steps[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+\
                  copy.deepcopy(tail)+[step('assert_recorded_state')]
        cases.append(dict(name=f'DevourEntryRewardsRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                          seed=190,setup=world.setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,
                              params=declaration['params'],oracle_text=declaration['oracle_text'],untested_components=list(GAPS))))
    return cases

