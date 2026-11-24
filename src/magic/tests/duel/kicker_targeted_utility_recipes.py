"""Paid and declined kicker with actual cast and entry targeting."""
import copy
import json
from pathlib import Path
import re

from counted_battlefield_damage_recipes import Game
from library_search_recipes import wave6_exact
from recipes import cost_from_oracle,step


FAMILY='kicker_targeted_utility'
PROFILE=Path(__file__).parent/'data/kicker_targeted_utility_wave8_profiles.json'
KICKER=re.compile(r'Kicker ((?:\{(?:\d+|[BUGRW])\})+) \(You may pay an additional \1 as you cast this spell\.\)')
RULES={
    ('Flying','When this creature enters, if it was kicked, target creature gains flying until end of turn.'):
        ('flying',('expires','grant_defender')),
    ('Flying','When this creature enters, if it was kicked, tap up to three target permanents.'):
        ('tap',('zero_targets','one_target','two_targets','releases')),
    ('When this creature enters, if it was kicked, it deals 2 damage to target creature.',):
        ('damage',('survivor','own_target')),
    ("When this creature enters, if it was kicked, return target permanent to its owner's hand.",):
        ('entry_bounce',('own_target',)),
    ("Return target nonland permanent to its owner's hand. If this spell was kicked, draw a card.",):
        ('spell_bounce',('own_target','lost_target','land_only')),
}
COMMON=('paid','declined','base_only','short_kicker','countered_paid','replay')
DECLINED={'declined','base_only','short_kicker','land_only'}
BEAR='Grizzly Bears'
GOBLIN="Mons's Goblin Raiders"
CANE="Feldon's Cane"
DRAKE='Wind Drake'
DUMMIES={BEAR,GOBLIN,DRAKE,'Hill Giant'}
WITNESSES={*DUMMIES,CANE,'Brittle Effigy','Forest','Island','Plains','Counterspell','Unsummon'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    if not record or record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces'):
        raise ValueError('whole normal kicker Oracle record required')
    cost,*clauses=record['oracle_text'].splitlines()
    fee=KICKER.fullmatch(cost)
    if not fee or tuple(clauses) not in RULES:
        raise ValueError('unreviewed printed kicker fee or complete utility clauses')
    effect,modes=RULES[tuple(clauses)]
    flyers=effect in ('flying','tap')
    if set(record['keywords'])!=({'Kicker','Flying'} if flyers else {'Kicker'}):
        raise ValueError('complete kicker keyword list changed')
    if effect=='spell_bounce':
        if record['type_line']!='Instant':
            raise ValueError('instant bounce required')
        power,toughness=None,None
    else:
        if not record['type_line'].startswith('Creature — '):
            raise ValueError('creature entry required')
        power,toughness=int(record['power']),int(record['toughness'])
    return dict(effect=effect,cast_price=cost_from_oracle(record),fee=cost_from_oracle({'mana_cost':fee[1]}),
                power=power,toughness=toughness,extras=list(modes))


def variants(shape):
    return [f'{mode}_{seat}' for mode in (*COMMON,*shape['extras']) for seat in (0,1)]


def spec(declaration,record):
    pinned=profiles()['sources'].get(record.get('name') if record else None)
    if not pinned or not wave6_exact(pinned['oracle'],record):
        raise ValueError('exact full pinned kicker source required')
    shape=classify(record)
    listed=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or
            declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text'] or
            not wave6_exact(declaration.get('params'),shape) or not isinstance(listed,list) or
            not listed or len(set(listed))!=len(listed) or set(listed)-set(variants(shape))):
        raise ValueError('exact kicker recipe and distinct reviewed variants required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    data=profiles()
    if set(data)!={'sources','witnesses'} or set(data['witnesses'])!=WITNESSES:
        raise ValueError('complete independent kicker witnesses required')
    for group,items in data.items():
        for name,pin in items.items():
            row=rows[name]
            if (row!=pin['registry'] or not wave6_exact(oracle[name],pin['oracle']) or row['kind']!='card'
                    or row['handler'] in (None,'0','NULL') or row['handler']=='card_dummy' and name not in DUMMIES):
                raise ValueError('unreviewed Oracle or native record '+name)
            if group=='sources':
                classify(oracle[name])


class Board(Game):
    def build(self):
        effect=self.effect=self.shape['effect']
        mode=self.mode
        self.paid=mode not in DECLINED
        self.permanent=effect!='spell_bounce'
        self.target_seat=self.owner if (effect=='flying' and mode!='grant_defender') or mode=='own_target' else self.foe
        self.guard={'flying':GOBLIN,'tap':'Plains','damage':'Hill Giant',
                    'entry_bounce':CANE,'spell_bounce':'Forest'}[effect]
        self.target={'flying':BEAR,'tap':BEAR,'damage':BEAR,'entry_bounce':'Forest','spell_bounce':CANE}[effect]
        if mode in ('survivor','lost_target') or mode=='own_target' and effect=='entry_bounce':
            self.target={'survivor':'Hill Giant','lost_target':BEAR,'own_target':'Brittle Effigy'}[mode]
        if mode=='survivor':
            self.guard=BEAR
        if mode!='land_only':
            self.add(self.target,'target',self.target_seat)
        self.add(self.guard,'guard',self.foe)
        if effect=='tap':
            self.add(CANE,'extra_0',self.foe)
            self.add('Brittle Effigy','extra_1',self.foe)
        if effect in ('flying','tap'):
            self.add(DRAKE,'flyer',self.foe)
        if mode=='grant_defender':
            self.add(DRAKE,'attacker',self.owner)
        if mode in ('countered_paid','lost_target'):
            self.add('Counterspell' if mode=='countered_paid' else 'Unsummon','reply',self.foe,'hand')
        for seat in (0,1):
            for index,name in enumerate(('Island','Plains')):
                self.add(name,f'deck_{seat}_{index}',seat,'library')

    def observe(self,ref,prop,value):
        self.emit('assert_card',ref=ref,property=prop,value=int(value))

    def mana(self,op,actor,pool):
        for color,value in enumerate(pool):
            self.emit(op,player=actor,color=color,value=value)

    def zone(self,name,actor,zone,count):
        self.emit('assert_zone_multiplicity',player=actor,zone=zone,index=self.rows[name]['csvid'],value=count)

    def prepare_and_cast(self):
        due=self.shape['cast_price']
        fee=self.shape['fee']
        extra=list(fee)
        if self.mode in ('base_only','land_only'):
            extra=[0]*6
        elif self.mode=='short_kicker':
            color=next(index for index,amount in enumerate(extra) if index and amount)
            extra[color]-=1
        self.remaining=[0]*6 if self.paid else extra
        self.emit('turn',player=self.owner,value=0)
        self.mana('set_mana',self.owner,[base+spare for base,spare in zip(due,extra)])
        self.mana('set_mana',self.foe,[0]*6)
        self.emit('refresh')
        for item in self.setup:
            if item['zone']=='battlefield':
                self.observe(item['ref'],'in_play',1)
                self.observe(item['ref'],'tapped',0)
        if self.mode=='land_only':
            self.emit('require_can_cast',ref='source',value=0)
            self.emit('cast',ref='source',value=0)
            self.emit('assert_stack',value=0)
            self.zone(self.source_name,self.owner,'hand',1)
            self.mana('assert_mana',self.owner,due)
            self.deck_facts(self.owner,drawn=False)
            self.emit('assert_choices_consumed')
            return
        if not self.permanent:
            self.emit('choose_target',ref='target',player=self.owner)
        if self.mode not in ('base_only','short_kicker'):
            self.emit('choose_dialog',player=self.owner,value=int(not self.paid))
        self.emit('cast',ref='source',value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1)
        self.observe('source','in_play',0)
        self.zone(self.target,self.target_seat,'battlefield',1)
        self.zone(self.target,self.target_seat,'graveyard',0)
        self.mana('assert_mana',self.owner,self.remaining)
        self.mana('assert_mana',self.foe,[0]*6)

    @property
    def source_name(self):
        return self.cards['source']['name']

    def entry_targets(self):
        if not self.paid or not self.permanent or self.mode=='countered_paid':
            return ()
        if self.effect!='tap':
            self.emit('choose_target',ref='target',player=self.owner)
            return ('target',)
        limit={'zero_targets':0,'one_target':1,'two_targets':2}.get(self.mode,3)
        picked=('target','guard','extra_0')[:limit]
        for ref in picked:
            self.emit('choose_target',ref=ref,player=self.owner)
        if limit<3:
            self.emit('choose_optional_target',ref='extra_1' if limit==2 else 'guard',player=self.owner)
        return picked

    def deck_facts(self,actor,*,drawn):
        deck=('Plains',) if drawn else ('Island','Plains')
        self.emit('assert_zone',player=actor,zone='library',value=len(deck))
        for index,name in enumerate(deck):
            self.emit('assert_zone_card',player=actor,zone='library',index=index,value=self.rows[name]['csvid'])
        self.zone('Island',actor,'hand',int(drawn))

    def settle(self):
        picks=self.entry_targets()
        countered=self.mode=='countered_paid'
        target_lost=self.mode=='lost_target'
        if countered or target_lost:
            self.emit('announce_spell',ref='source')
            self.cast('reply',target='target' if target_lost else None,response=True)
            self.emit('resolve_response')
        else:
            self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0)
        self.emit('refresh')
        self.mana('assert_mana',self.owner,self.remaining)
        self.mana('assert_mana',self.foe,[0]*6)
        present=self.permanent and not countered
        self.observe('source','in_play',int(present))
        self.zone(self.source_name,self.owner,'battlefield',int(present))
        self.zone(self.source_name,self.owner,'graveyard',int(not present))
        if present:
            self.observe('source','power',self.shape['power'])
            self.observe('source','toughness',self.shape['toughness'])
            self.observe('source','tapped',0)
        moved=not countered and (not self.permanent or self.effect=='entry_bounce' and self.paid)
        killed=self.effect=='damage' and self.paid and not countered and self.mode!='survivor'
        self.zone(self.target,self.target_seat,'hand',int(moved))
        self.zone(self.target,self.target_seat,'battlefield',int(not moved and not killed))
        self.zone(self.target,self.target_seat,'graveyard',int(killed))
        self.zone(self.guard,self.foe,'battlefield',1)
        self.zone(self.guard,self.foe,'graveyard',0)
        if self.effect=='tap':
            self.emit('assert_forced_target_choices',value=0)
            for ref in ('target','guard','extra_0','extra_1','flyer'):
                self.observe(ref,'tapped',int(ref in picks))
        if self.effect=='damage':
            self.observe('guard','damage',0)
            if not killed:
                self.observe('target','damage',2 if self.mode=='survivor' else 0)
        self.deck_facts(self.owner,drawn=self.paid and not self.permanent and not countered and not target_lost)
        self.deck_facts(self.foe,drawn=False)
        if self.mode in ('countered_paid','lost_target'):
            self.zone(self.cards['reply']['name'],self.foe,'graveyard',1)
        for seat in (self.owner,self.foe):
            self.emit('assert_life',player=seat,value=20)

    def combat(self,attacker,blocker,*,blocked):
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref=blocker,other=attacker,value=int(blocked))
        self.emit('advance_combat')
        self.observe(attacker,'blocked',blocked)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')

    def combat_riders(self):
        mode=self.mode
        if mode in ('countered_paid','lost_target'):
            return
        if mode=='releases':
            self.emit('advance_turn')
            self.emit('untap_step',player=self.foe)
            for ref in ('target','guard','extra_0','extra_1','flyer'):
                self.observe(ref,'tapped',0)
            return
        if mode=='grant_defender':
            self.combat('attacker','target',blocked=True)
            self.zone(DRAKE,self.owner,'graveyard',1)
            self.zone(self.target,self.foe,'graveyard',1)
            self.emit('assert_life',player=self.foe,value=20)
            return
        if self.effect=='flying':
            if mode=='expires':
                for turn in (self.foe,self.owner):
                    self.emit('advance_turn')
                    self.emit('untap_step',player=turn)
                self.emit('turn',player=self.owner,value=0)
            grants=self.paid and mode!='expires'
            self.combat('target','guard',blocked=not grants)
            self.emit('assert_life',player=self.foe,value=18 if grants else 20)
            self.observe('target','damage',int(not grants))
            self.zone(self.guard,self.foe,'graveyard',int(not grants))
        if self.effect in ('flying','tap'):
            self.emit('advance_turn')
            self.emit('turn',player=self.foe,value=0)
            self.combat('flyer','source',blocked=True)
            self.emit('assert_life',player=self.owner,value=20)
            self.zone(self.source_name,self.owner,'graveyard',1)
            dead=self.effect=='flying'
            self.zone(DRAKE,self.foe,'graveyard',int(dead))
            if not dead:
                self.observe('flyer','damage',1)

    def run(self):
        self.prepare_and_cast()
        if self.mode=='land_only':
            return
        if self.mode=='replay':
            self.emit('capture')
            start=len(self.steps)
            self.settle()
            tail=copy.deepcopy(self.steps[start:])
            self.emit('record_state')
            self.emit('restore')
            self.emit('ai_roundtrip')
            self.steps.extend(tail)
            self.emit('assert_recorded_state')
        else:
            self.settle()
        self.combat_riders()


def build(name,csvid,declaration,shape,variant,rows,oracle):
    mode,seat=variant.rsplit('_',1)
    world=Board(name,shape,mode,int(seat),rows,oracle)
    world.run()
    return dict(name=f'KickerTargetedUtilityRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=2298,
                setup=world.setup,steps=world.steps,
                _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                             oracle_text=declaration['oracle_text'],
                             untested_components=['Interposition within immediate entry callbacks and multiplayer.']))


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid']!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(shape)):
        raise ValueError('distinct reviewed utility variants and exact native source required')
    validate_witnesses(oracle,rows)
    return [build(name,csvid,declaration,shape,variant,rows,oracle) for variant in selected]
