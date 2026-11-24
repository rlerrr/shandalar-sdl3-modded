"""Two real Werewolf faces driven by casts in one actual preceding turn."""
import copy
import json
from collections import Counter
from pathlib import Path

from counted_battlefield_damage_recipes import Game as PaymentBoard
from library_search_recipes import wave6_exact


FAMILY = 'werewolf_spell_history'
PROFILE = Path(__file__).parent / 'data/werewolf_spell_history_wave8_profiles.json'
TRIGGERS = (
    'At the beginning of each upkeep, if no spells were cast last turn, transform this creature.',
    'At the beginning of each upkeep, if a player cast two or more spells last turn, transform this creature.',
)
HUMAN = 'manalinkeh_card_gastaf_shepherd'
WOLF = 'manalinkeh_card_howlpack_of_estwald'
SMALL = "Mons's Goblin Raiders"
DECK = ('Forest','Island','Mountain','Plains')
WITNESSES = {*DECK,'Fog','Counterspell','Hunger of the Howlpack','Unsummon',SMALL,'Grizzly Bears'}
DUMMIES = {SMALL,'Grizzly Bears'}
PATTERNS = {
    'front_zero':(), 'front_one_owner':('owner',), 'front_one_foe':('foe',),
    'front_two_owner':('owner','owner'), 'front_two_foe':('foe','foe'), 'front_split':('owner','foe'),
    'back_zero':(), 'back_one_owner':('owner',), 'back_one_foe':('foe',),
    'back_two_owner':('owner','owner'), 'back_two_foe':('foe','foe'), 'back_split':('owner','foe'),
}
COMMON = ('cast', 'short_cast', 'wrong_color', *PATTERNS, 'front_zero_on_foe', 'back_two_on_owner',
          'countered_second', 'unfunded_activity', 'keep_counters', 'bounce_recast',
          'front_replay', 'back_replay')


def profiles():
    return json.loads(PROFILE.read_bytes())


def face_shape(raw, selected, trigger):
    from recipes import COLORS
    fields=('name','oracle_text','mana_cost','type_line','power','toughness','colors')
    if set(selected)!=set(fields) or any(not wave6_exact(selected[key],raw.get(key)) for key in fields):
        raise ValueError('selected face must agree with the complete pinned face')
    clauses=raw['oracle_text'].splitlines()
    riders=clauses[:-1]
    if clauses[-1:]!=[trigger] or riders not in ([],['First strike'],['Trample']):
        raise ValueError('whole paired trigger and supported combat riders only')
    if (not raw['type_line'].startswith('Creature — ') or 'Werewolf' not in raw['type_line'].split()
            or any(type(raw.get(s)) is not str or not raw[s].isascii() or not raw[s].isdecimal()
                   for s in ('power','toughness'))):
        raise ValueError('reviewed fixed creature characteristics needed')
    return dict(name=raw['name'], power=int(raw['power']), toughness=int(raw['toughness']),
                color=sum(1<<COLORS[color] for color in raw['colors']), keywords=riders, clauses=clauses)


def classify(front,back=None):
    from recipes import cost_from_oracle

    if not front or front.get('layout')!='transform' or len(front.get('card_faces',[]))!=2:
        raise ValueError('whole two-face physical transform card required')
    f,b=front['card_faces']
    if back is None:
        back=profiles()['sources'][f['name']]['back_oracle']
    if (front['engine_face']!=f['name'] or back['engine_face']!=b['name']
            or front['name']!=f['name']+' // '+b['name'] or back['name']!=front['name']
            or back['layout']!='transform' or front['oracle_id']!=back['oracle_id']
            or not wave6_exact(front['card_faces'],back['card_faces'])
            or not f['mana_cost'] or b['mana_cost'] or b.get('color_indicator')!=b['colors']):
        raise ValueError('exact physical front and linked permanent back required')
    sides=[face_shape(raw,record['selected_face'],clause)
           for raw,record,clause in zip((f,b),(front,back),TRIGGERS)]
    expected={'Transform',*sides[0]['keywords'],*sides[1]['keywords']}
    if set(front['keywords'])!=expected or set(back['keywords'])!=expected:
        raise ValueError('whole physical Oracle keyword set changed')
    return dict(price=cost_from_oracle(f),front=sides[0],back=sides[1])


def variants(shape):
    extras=()
    if shape['front']['keywords']==['First strike'] and shape['back']['keywords']==['First strike']:
        extras=('front_first_strike','back_first_strike')
    if shape['back']['keywords']==['Trample']:
        extras=('front_no_trample','back_trample')
    return [f'{scenario}_{owner}' for scenario in COMMON+extras for owner in (0,1)]


def spec(declaration,record):
    name=record.get('engine_face')
    pin=profiles()['sources'].get(name)
    if not pin or not wave6_exact(pin['oracle'],record):
        raise ValueError('exact physical front record needed')
    shape=classify(record,pin['back_oracle'])
    chosen=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['selected_face']['oracle_text']
            or not wave6_exact(declaration.get('params'),shape) or not isinstance(chosen,list)
            or not chosen or len(set(chosen))!=len(chosen) or set(chosen)-set(variants(shape))):
        raise ValueError('complete pinned physical faces and reviewed distinct variants required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    data=profiles()
    if set(data)!={'sources','witnesses'} or set(data['witnesses'])!=WITNESSES:
        raise ValueError('complete ordered history and combat witnesses required')
    for name,pin in data['witnesses'].items():
        row=rows[name]
        record=oracle[name]
        if (pin['registry']!=row or not wave6_exact(pin['oracle'],record) or record.get('layout')!='normal'
                or row['kind']!='card' or row['handler'] in (None,'NULL','0')
                or row['handler']=='card_dummy' and name not in DUMMIES):
            raise ValueError('unreviewed independent witness: '+name)
    for name,pin in data['sources'].items():
        front,back=pin['oracle'],pin['back_oracle']
        shape=classify(front,back)
        current=rows[name]
        other=rows[shape['back']['name']]
        if (name!=shape['front']['name'] or not wave6_exact(oracle[name],front)
                or not wave6_exact(oracle[other['name']],back) or pin['registry']!=current or pin['back_registry']!=other
                or current['kind']!='card' or other['kind']!='card' or current['handler']!=HUMAN
                or other['handler']!=WOLF or other['csvid']!=current['csvid']+1):
            raise ValueError('native pair or complete physical Oracle changed: '+name)


class Board(PaymentBoard):
    def __init__(self,name,shape,mode,owner,rows,oracle):
        self.shape,self.mode,self.owner,self.foe=shape,mode,owner,1-owner
        self.rows,self.oracle=rows,oracle
        self.cards,self.setup,self.steps={},[],[]
        self.pools=[[0]*6,[0]*6]
        self.life=[20,20]
        self.current=owner
        self.history=[0,0]
        self.is_back=False
        self.sick=False
        self.counters=0
        self.snapshot=None
        self.next_spell=[0,0]
        self.add(name,'source',owner,'hand')
        for who in (0,1):
            for i,card in enumerate(DECK):
                self.add(card,f'library_{who}_{i}',who,'library')
        for who,num in enumerate(self.fog_stock()):
            for i in range(num):
                self.add('Fog',f'fog_{who}_{i}',who,'hand')
        if mode=='countered_second':
            self.add('Counterspell','counter',self.foe,'hand')
        if mode=='keep_counters':
            self.add('Hunger of the Howlpack','mark',owner,'hand')
        if mode=='bounce_recast':
            self.add('Unsummon','bounce',self.foe,'hand')
        if mode in ('front_first_strike','back_first_strike','front_no_trample','back_trample'):
            self.add('Grizzly Bears' if mode=='back_first_strike' else SMALL,'blocker',self.foe)

    def fog_stock(self):
        rosters=PATTERNS.get(self.mode,())
        special={
            'front_zero_on_foe':('foe',), 'back_two_on_owner':('foe','foe'),
            'countered_second':('owner','owner'), 'unfunded_activity':('foe',),
            'keep_counters':('owner','owner'), 'front_first_strike':('foe',),
            'front_no_trample':('foe',), 'back_replay':('owner','owner'),
        }
        rosters=special.get(self.mode,rosters)
        return [rosters.count('owner' if who==self.owner else 'foe') for who in (0,1)]

    def face(self):
        return self.shape['back' if self.is_back else 'front']

    def identity(self,item):
        name=self.face()['name'] if item is self.cards['source'] and item['zone']=='battlefield' else item['name']
        return self.rows[name]['csvid']

    def stats(self,ref):
        if ref=='source':
            face=self.face()
            return face['power']+self.counters,face['toughness']+self.counters
        record=self.oracle[self.cards[ref]['name']]
        return int(record['power']),int(record['toughness'])

    def receipts(self):
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0)
        self.observe_mana()
        zone_counts=Counter((item['player'],item['zone'],self.identity(item)) for item in self.cards.values())
        known={self.rows[item['name']]['csvid'] for item in self.cards.values()}
        known.add(self.rows[self.shape['back']['name']]['csvid'])
        for player in (0,1):
            self.emit('assert_life',player=player,value=self.life[player])
            for zone in ('battlefield','hand','graveyard','library','exile'):
                self.emit('assert_zone',player=player,zone=zone,
                          value=sum(c for (seat,area,_),c in zone_counts.items() if seat==player and area==zone))
                if zone=='library':
                    library=[item for item in self.cards.values() if item['player']==player and item['zone']==zone]
                    for index,item in enumerate(library):
                        self.emit('assert_zone_card',player=player,zone=zone,index=index,value=self.identity(item))
                else:
                    for csvid in sorted(known):
                        self.emit('assert_zone_multiplicity',player=player,zone=zone,index=csvid,
                                  value=zone_counts[player,zone,csvid])
        for ref,item in self.cards.items():
            if item['zone']!='battlefield':
                continue
            power,toughness=self.stats(ref)
            for prop,value in [('tapped',int(item['tapped'])),('power',power),('toughness',toughness),('damage',item['damage'])]:
                self.emit('assert_card',ref=ref,property=prop,value=value)
            if ref=='source':
                self.emit('assert_card',ref=ref,property='csvid',value=self.identity(item))
                self.emit('assert_card',ref=ref,property='sick',value=int(self.sick))
                self.emit('assert_card',ref=ref,property='plus1_counters',value=self.counters)
                self.emit('assert_matching_permanents',player=self.owner,index=self.identity(item),
                          property='color',value=self.face()['color'])

    def cast_source(self):
        price=self.shape['price']
        spent=list(price)
        denied=self.mode in ('short_cast','wrong_color')
        if self.mode=='short_cast':
            spent[next(i for i,n in enumerate(spent) if n)]-=1
        elif self.mode=='wrong_color':
            index=next(i for i in range(1,6) if spent[i])
            spent[2],spent[index]=spent[index],0
        self.fund(self.owner,spent)
        self.emit('cast',ref='source',value=int(not denied))
        self.emit('assert_stack',value=int(not denied))
        if not denied:
            self.pools[self.owner]=[0]*6
            self.observe_mana()
            self.emit('resolve')
            self.cards['source'].update(zone='battlefield',tapped=False,damage=0)
            self.sick=True
            self.is_back=False
            self.counters=0
            self.history[self.owner]+=1
        self.receipts()

    def spell(self,ref,*,target=None,allowed=True,response=False):
        self.cast(ref,target=target, allowed=allowed, funding=None if allowed else [0]*6,response=response)
        if allowed:
            self.history[self.cards[ref]['player']]+=1

    def fog(self,who,*,allowed=True,pending=False):
        index=self.next_spell[who]
        self.next_spell[who]+=1
        ref=f'fog_{who}_{index}'
        self.spell(ref,allowed=allowed)
        if allowed and not pending:
            self.emit('resolve')
            self.cards[ref]['zone']='graveyard'
        return ref

    def visit_upkeep(self,*,capture=False):
        if any(map(any,self.pools)):
            raise ValueError('real turn boundary requires empty fixture mana')
        self.emit('advance_turn')
        self.current=1-self.current
        for item in self.cards.values():
            item['damage']=0
            if item['zone']=='battlefield' and item['player']==self.current:
                item['tapped']=False
        if self.current==self.owner:
            self.sick=False
        self.emit('untap_step',player=self.current)
        self.receipts()
        if capture:
            self.snapshot=len(self.steps)
        self.emit('upkeep_step',player=self.current)
        if self.cards['source']['zone']=='battlefield':
            if self.is_back and max(self.history)>=2:
                self.is_back=False
            elif not self.is_back and sum(self.history)==0:
                self.is_back=True
        self.history=[0,0]
        self.receipts()
        self.emit('turn',player=self.current,value=0)

    def combat(self):
        source,blocker=self.cards['source'],self.cards['blocker']
        if self.current!=self.owner or self.sick:
            raise ValueError('combat needs its real ready controller turn')
        keywords=self.face()['keywords']
        power,tough=self.stats('source')
        opposing_power,opposing_tough=self.stats('blocker')
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        source['tapped']=True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref='blocker',other='source',value=1)
        self.emit('advance_combat')
        self.emit('assert_card',ref='source',property='blocked',value=1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        spill=max(0,power-opposing_tough) if 'Trample' in keywords else 0
        self.life[self.foe]-=spill
        blocker['damage']=power-spill
        source['damage']=0 if 'First strike' in keywords and blocker['damage']>=opposing_tough else opposing_power
        if blocker['damage']>=opposing_tough:
            blocker['zone']='graveyard'
        if source['damage']>=tough:
            source['zone']='graveyard'
        self.receipts()

    def run(self):
        self.emit('turn',player=self.owner,value=0)
        for who in (0,1):
            self.fund(who,[0]*6)
        self.cast_source()
        mode=self.mode
        if mode in ('cast','short_cast','wrong_color'):
            return
        if mode=='keep_counters':
            self.spell('mark',target='source')
            self.emit('resolve')
            self.cards['mark']['zone']='graveyard'
            self.counters+=1
            self.receipts()
        self.visit_upkeep()
        if mode in ('front_zero_on_foe','front_first_strike','front_no_trample'):
            self.fog(self.foe)
            self.visit_upkeep()
            if mode=='front_zero_on_foe':
                self.visit_upkeep()
            else:
                self.combat()
            return
        if mode=='front_replay':
            self.visit_upkeep(capture=True)
            return
        if mode=='unfunded_activity':
            self.fog(self.foe,allowed=False)
            self.visit_upkeep()
            return
        if mode.startswith('front_'):
            for actor in PATTERNS[mode]:
                self.fog(self.owner if actor=='owner' else self.foe)
            self.visit_upkeep()
            return
        self.visit_upkeep()
        if mode in ('back_first_strike','back_trample'):
            self.combat()
        elif mode=='back_two_on_owner':
            self.visit_upkeep()
            self.fog(self.foe)
            self.fog(self.foe)
            self.visit_upkeep()
        elif mode=='back_replay':
            self.fog(self.owner)
            self.fog(self.owner)
            self.visit_upkeep(capture=True)
        elif mode=='keep_counters':
            self.fog(self.owner)
            self.fog(self.owner)
            self.visit_upkeep()
        elif mode=='countered_second':
            self.fog(self.owner)
            pending=self.fog(self.owner,pending=True)
            self.emit('announce_spell',ref=pending)
            self.spell('counter',response=True)
            self.emit('resolve_response')
            self.cards['counter']['zone']=self.cards[pending]['zone']='graveyard'
            self.receipts()
            self.visit_upkeep()
        elif mode=='bounce_recast':
            self.spell('bounce',target='source')
            self.emit('resolve')
            self.cards['bounce']['zone']='graveyard'
            self.cards['source']['zone']='hand'
            self.is_back=self.sick=False
            self.counters=0
            self.emit('bind_hand',ref='source',player=self.owner,value=self.rows[self.shape['front']['name']]['csvid'])
            self.receipts()
            self.cast_source()
            self.visit_upkeep()
        else:
            for actor in PATTERNS[mode]:
                self.fog(self.owner if actor=='owner' else self.foe)
            self.visit_upkeep()


def build(name,csvid,declaration,shape,variant,rows,oracle):
    from recipes import step

    mode,seat=variant.rsplit('_',1)
    world=Board(name,shape,mode,int(seat),rows,oracle)
    world.run()
    steps=world.steps
    if world.snapshot is not None:
        cut=world.snapshot
        suffix=copy.deepcopy(steps[cut:])
        steps=steps[:cut]+[step('capture')]+suffix+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(suffix)+[step('assert_recorded_state')]
    return dict(name=f'WerewolfSpellHistoryRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                setup=world.setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,
                 params=declaration['params'],oracle_text=declaration['oracle_text'],
                 untested_components=['Responses inside the synchronous upkeep callback, multiplayer, and non-upkeep transform effects.']))


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid']!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(shape)):
        raise ValueError('only distinct cases for physical fronts may expand')
    validate_witnesses(oracle,rows)
    return [build(name,csvid,declaration,shape,variant,rows,oracle) for variant in selected]
