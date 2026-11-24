"""Ordered, paid Strive target lists and their actual creature effects."""

import json
from pathlib import Path
import re

from cast_untap_trigger_recipes import exact
from global_attack_tax_recipes import Board


FAMILY = 'strive_target_rewards'
PROFILE = Path(__file__).parent / 'data/strive_target_rewards_wave8_profiles.json'
STRIVE = re.compile(r'Strive — This spell costs ((?:\{[1-9BUGRW]\})+) more to cast for each target beyond the first\.')
SCOPE = 'Any number of target creatures each '
END = ' until end of turn'
EFFECTS = {
    SCOPE+'get +1/+1 and gain flying'+END+'.': ('flying',(1,1)),
    SCOPE+'get +1/+1 and gain indestructible'+END+". (Damage and effects that say \"destroy\" don't destroy them.)": ('indestructible',(1,1)),
    SCOPE+'get +1/+0 and gain lifelink'+END+'. (Damage dealt by a creature with lifelink also causes its controller to gain that much life.)': ('lifelink',(1,0)),
    SCOPE+'gain double strike'+END+'. (They deal both first-strike and regular combat damage.)': ('double',(0,0)),
    SCOPE+'get +2/+0 and gain trample'+END+'.': ('trample',(2,0)),
    'Exile any number of target creatures. For each creature exiled this way, its controller creates a 4/4 blue Sphinx creature token with flying.': ('sphinx',(0,0)),
}
STOCK = ('Forest','Island','Mountain','Plains')
TOKEN = 'Sphinx'
STATIC = {'Grizzly Bears','Centaur Courser','Hill Giant'}
WITNESSES = set(STOCK)|STATIC|{TOKEN,'Counterspell','Unsummon','Blurred Mongoose','Lightning Bolt','Murder'}
COMMON = ('one','opposing','two_mixed','three','spare_all','short_generic','short_color','zero',
          'stop_one','all_unavailable','wrong_base_color','empty_pool','late_first','late_last',
          'late_all','offturn','countered','cleanup','replay')
EXTRA = {'flying':('combat',),'indestructible':('destroy_protection',),'lifelink':('combat','lifelink_enemy'),
         'double':('combat',),'trample':('combat',),'sphinx':('token_flying','foreign_owner')}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    lines=record['oracle_text'].splitlines()
    fee=STRIVE.fullmatch(lines[0]) if len(lines)==2 else None
    if (record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or record['type_line']!='Instant' or record['keywords']!=['Strive']
            or not fee or lines[1] not in EFFECTS):
        raise ValueError('whole fixed Strive cost and reviewed creature reward required')
    effect,delta=EFFECTS[lines[1]]
    base=cost_from_oracle(record)
    extra=cost_from_oracle({'mana_cost':fee[1]})
    color=[position for position in range(1,6) if base[position]]
    if (len(color)!=1 or base[color[0]]!=1 or extra[color[0]]!=1 or extra[0] not in (1,2)
            or sum(extra)!=extra[0]+1):
        raise ValueError('unreviewed Strive payment shape')
    return dict(effect=effect,delta=list(delta),base=base,extra=extra,color=color[0])


def variants(params):
    return [f'{mode}_{seat}' for seat in (0,1) for mode in (*COMMON,*EXTRA[params['effect']])]


def spec(declaration,record):
    params=parameters(record)
    selected=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or not exact(profile()['sources'].get(record['name'],{}).get('oracle'),record)
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),params)):
        raise ValueError('exact pinned Strive cost and whole Oracle required')
    if (not isinstance(selected,list) or not selected or any(type(mode) is not str for mode in selected)
            or len(selected)!=len(set(selected)) or set(selected)-set(variants(params))):
        raise ValueError('distinct reviewed Strive variants required')
    return dict(params,family=FAMILY)


def validate_witnesses(oracle,rows):
    pins=profile()
    if set(pins['witnesses'])!=WITNESSES or len(pins['sources'])!=len(EFFECTS):
        raise ValueError('complete Strive action witness inventory required')
    for name,item in (pins['sources']|pins['witnesses']).items():
        row=rows[name]
        if not exact(oracle[name],item['oracle']) or not exact(row,item['registry']):
            raise ValueError(f'Oracle or registry drift for {name}')
        allowed_kind=('card','token') if name==TOKEN else ('card',)
        if (row['kind'] not in allowed_kind or name not in STATIC and row['handler'] in ('card_dummy','0','NULL',None)):
            raise ValueError(f'expected native witness for {name}')
    token=oracle[TOKEN]
    if (token['type_line']!='Token Creature — Sphinx' or (token['power'],token['toughness'])!=('4','4')
            or 'Flying' not in token['keywords']):
        raise ValueError('exact legacy Sphinx token identity required')


def total(params,count):
    if type(count) is not int or count<0:
        raise ValueError('nonnegative integer target count required')
    return [base+max(0,count-1)*extra for base,extra in zip(params['base'],params['extra'])]


class StriveBoard(Board):
    def __init__(self,name,params,variant,rows,oracle):
        self.mode,seat=variant.rsplit('_',1)
        self.owner,self.foe=int(seat),1-int(seat)
        self.active=self.foe if self.mode in ('offturn','lifelink_enemy') else self.owner
        self.name,self.params,self.rows,self.oracle=name,params,rows,oracle
        self.reward=params['effect']
        self.setup,self.steps,self.entities=[],[],{}
        self.zones={zone:[[],[]] for zone in ('hand','battlefield','graveyard','library','exile')}
        self.mana,self.life=[[0]*6,[0]*6],[20,20]
        self.loyalty=self.replay=None
        self.applied={}
        self.token_counts=[0,0]
        self.true_owners={}
        self.add(name,'source',self.owner,'hand')
        self.seed_board()
        for actor in (0,1):
            for index,card in enumerate(STOCK):
                self.add(card,f'library_{actor}_{index}',actor,'library')

    def seed_board(self):
        mode=self.mode
        if mode=='all_unavailable':
            self.add('Blurred Mongoose','first',self.owner)
            self.add('Blurred Mongoose','second',self.foe)
            self.add('Forest','noncreature',self.foe)
            return
        same_owner=mode in ('combat','destroy_protection','lifelink_enemy')
        first_seat=self.foe if mode=='lifelink_enemy' else self.owner
        second_seat=first_seat if same_owner else self.foe
        self.add('Grizzly Bears','first',first_seat)
        self.add('Centaur Courser','second',second_seat)
        if mode!='spare_all':
            self.add('Hill Giant','third',1-first_seat)
        if mode in ('combat','token_flying') and self.reward in ('flying','trample','sphinx'):
            self.add('Grizzly Bears','blocker',self.foe)
        if mode.startswith('late_'):
            self.add('Unsummon','response',self.foe,'hand')
        elif mode=='countered':
            self.add('Counterspell','response',self.foe,'hand')
        if mode=='destroy_protection':
            self.add('Lightning Bolt','bolt',self.foe,'hand')
            self.add('Murder','destroy',self.foe,'hand')
            self.add('Murder','after_cleanup',self.foe,'hand')
        if mode=='foreign_owner':
            entry=next(card for card in self.setup if card['ref']=='second')
            entry['state'].append('opponent_owned')
            self.true_owners['second']=self.owner

    def selection(self):
        single=('one','short_generic','short_color','stop_one','late_all')
        if self.mode in single:
            return ('first',)
        if self.mode=='opposing':
            return ('second',)
        if self.mode=='three':
            return ('first','second','third')
        if self.mode in ('zero','all_unavailable','wrong_base_color','empty_pool'):
            return ()
        return ('first','second')

    def budget(self,selection):
        mode=self.mode
        if mode in ('zero','stop_one','spare_all'):
            return total(self.params,3)
        result=total(self.params,2 if mode in ('short_generic','short_color') else len(selection))
        if mode=='short_generic':
            result[0]-=1
        elif mode=='short_color':
            result[self.params['color']]-=1
            result[0]+=2
        elif mode=='wrong_base_color':
            result=[sum(result),0,0,0,0,0]
        elif mode=='empty_pool':
            result=[0]*6
        return result

    def receipt(self):
        super().receipt()
        if self.reward!='sphinx':
            return
        for actor,count in enumerate(self.token_counts):
            self.emit('assert_zone_multiplicity',player=actor,zone='battlefield',index=self.rows[TOKEN]['csvid'],value=count)
            if count:
                for property,value in [('token',1),('power',4),('toughness',4),('color',0x04)]:
                    self.emit('assert_matching_permanents',player=actor,index=self.rows[TOKEN]['csvid'],property=property,value=value)

    def spend(self,who,cost):
        self.mana[who]=[have-need for have,need in zip(self.mana[who],cost)]
        assert min(self.mana[who])>=0

    def begin_spell(self,selection):
        budget=self.budget(selection)
        self.fund(self.owner,budget)
        if self.mode=='all_unavailable':
            for target in ('first','second','noncreature'):
                self.emit('can_target_creature',ref='source',other=target,value=0)
            # A spell that can have zero targets is legally available here.
            # The native predicate rejects it before exposing any selector.
            self.emit('can_cast',ref='source',value=1)
            return False
        if self.mode=='replay':
            self.replay=len(self.steps)
        for target in selection:
            self.emit('choose_target',player=self.owner,ref=target)
        if self.mode in ('zero','stop_one'):
            self.emit('choose_done_target',player=self.owner)
        allowed=self.mode not in ('wrong_base_color','empty_pool')
        self.emit('cast',ref='source',value=int(allowed))
        self.emit('assert_unused_choices',value=0)
        self.observe('source','in_hand',not allowed)
        self.emit('assert_stack',value=int(allowed))
        if allowed:
            self.spend(self.owner,total(self.params,len(selection)))
        self.pools()
        return allowed

    def witness_spell(self,ref,target=None,response=False):
        from recipes import cost_from_oracle

        witness=self.entities[ref]
        actor=witness['player']
        fee=cost_from_oracle(self.oracle[witness['name']])
        self.fund(actor,fee)
        if target:
            self.emit('choose_target',player=actor,ref=target)
        self.emit('cast_response' if response else 'cast',ref=ref,value=1)
        self.spend(actor,fee)
        self.emit('assert_stack',value=2 if response else 1)
        self.pools()
        self.emit('resolve_response' if response else 'resolve')
        self.move(ref,'graveyard')
        self.emit('refresh')

    def exile_target(self,ref):
        controller=self.entities[ref]['player']
        owner=self.true_owners.get(ref,controller)
        self.move(ref,'exile')
        if owner!=controller:
            self.zones['exile'][controller].remove(self.entities[ref]['name'])
            self.zones['exile'][owner].append(self.entities[ref]['name'])
        self.token_counts[controller]+=1
        self.zones['battlefield'][controller].append(TOKEN)

    def apply_reward(self,targets):
        for ref in targets:
            if self.reward=='sphinx':
                self.exile_target(ref)
            else:
                entity=self.entities[ref]
                self.applied[ref]=entity['stats']
                entity['stats']=tuple(value+delta for value,delta in zip(entity['stats'],self.params['delta']))

    def resolve_source(self,targets):
        mode=self.mode
        if mode.startswith('late_') or mode=='countered':
            self.emit('announce_spell',ref='source')
            bounced=None if mode=='countered' else targets[-1 if mode=='late_last' else 0]
            self.witness_spell('response',target=bounced,response=True)
            if bounced:
                self.move(bounced,'hand')
                targets=tuple(ref for ref in targets if ref!=bounced)
            else:
                targets=()
        else:
            self.emit('resolve')
            self.emit('refresh')
        self.move('source','graveyard')
        self.apply_reward(targets)
        self.receipt()

    def expire(self):
        for ref,stats in self.applied.items():
            self.entities[ref]['stats']=stats
        for entity in self.entities.values():
            entity['damage']=0
        self.applied.clear()

    def combat(self):
        attackers=('first',) if self.reward=='trample' else ('token',) if self.reward=='sphinx' else ('first','second')
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack',ref=ref,value=1)
            self.entities[ref]['tapped']=True
            self.observe(ref,'tapped',1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if self.reward in ('flying','trample','sphinx'):
            for ref in attackers:
                self.emit('block',ref='blocker',other=ref,value=int(self.reward=='trample'))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        power=sum(self.entities[ref]['stats'][0] for ref in attackers)
        if self.reward=='trample':
            self.move('blocker','graveyard')
            self.move('first','graveyard')
            power-=2
        if self.reward=='double':
            power*=2
        self.life[1-self.active]-=power
        if self.reward=='lifelink':
            self.life[self.active]+=power

    def destroy_protection(self):
        self.witness_spell('bolt','first')
        self.entities['first']['damage']=3
        self.witness_spell('destroy','second')
        self.receipt()
        self.emit('advance_turn')
        self.emit('untap_step',player=self.foe)
        self.emit('turn',player=self.foe,value=0)
        self.expire()
        self.witness_spell('after_cleanup','first')
        self.move('first','graveyard')

    def fly_token(self):
        for actor in (self.foe,self.owner):
            self.emit('advance_turn')
            self.emit('untap_step',player=actor)
        self.emit('turn',player=self.owner,value=0)
        self.emit('bind_permanent',player=self.owner,value=self.rows[TOKEN]['csvid'],ref='token')
        self.entities['token']=dict(name=TOKEN,player=self.owner,zone='battlefield',stats=(4,4),tapped=False,damage=0)
        self.combat()

    def actions(self):
        from recipes import step

        self.emit('turn',player=self.active,value=0)
        selection=self.selection()
        if not self.begin_spell(selection):
            self.receipt()
            return
        self.resolve_source(selection)
        if self.mode in ('combat','lifelink_enemy'):
            self.combat()
        elif self.mode=='destroy_protection':
            self.destroy_protection()
        elif self.mode=='token_flying':
            self.fly_token()
        elif self.mode=='cleanup':
            self.emit('cleanup')
            self.emit('refresh')
            self.expire()
        self.receipt()
        if self.replay is not None:
            tail=self.steps[self.replay:]
            self.steps[self.replay:]=[step('capture'),*tail,step('record_state'),step('ai_roundtrip'),
                                     step('restore'),*tail,step('assert_recorded_state')]


def expand(name,csvid,declaration,params,selected,rows,oracle):
    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or not selected or set(selected)-set(variants(params)):
        raise ValueError('unbound Strive source or invalid variant')
    result=[]
    for variant in selected:
        case=StriveBoard(name,params,variant,rows,oracle)
        case.actions()
        metadata=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                      untested_components=['duplicate-target offers in the priority menu','alternative payments',
                                           'more than three distinct targets','multiplayer and replacement effects'])
        result.append(dict(name=f'StriveTargetRewardRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=1970,
                           setup=case.setup,steps=case.steps,_recipe=metadata))
    return result
