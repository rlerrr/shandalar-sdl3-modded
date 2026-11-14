"""Oracle-validated combat-to-player value triggers through real combat.

The grammar describes effects, not source names. Seeded ready creatures and real
casts are separate scenarios; no damage, trigger, or value receipt is injected.
"""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from creature_recipes import REMINDERS

FAMILY = 'combat_value_trigger'
PINS = json.loads((Path(__file__).parent/'data/combat_value_trigger_wave8_witnesses.json').read_text(encoding='utf8'))
PASSIVE = {'Flying','Haste','Vigilance','Reach','Trample'}
NUMBERS = {'a':1,'two':2,'three':3}
LIBRARY = ('Forest','Plains','Grizzly Bears','Island','Mountain','Sol Ring',
           'Lightning Bolt','Forest','Plains','Island','Grizzly Bears','Mountain')


def _classic_parameters(record):
    """Require the complete standalone text to fit the reviewed mechanism."""
    if (not isinstance(record,dict) or record.get('layout')!='normal' or record.get('card_faces')
            or record.get('selected_face') or 'Creature' not in record.get('type_line','')):
        raise ValueError('combat value requires a normal standalone creature')
    for field in ('power','toughness'):
        if not re.fullmatch(r'\d+',record.get(field,'')):
            raise ValueError('combat value requires fixed numeric characteristics')
    if not 1<=int(record['power'])<=5 or not 1<=int(record['toughness'])<=7:
        raise ValueError('combat value characteristics exceed the reviewed board')
    text=record.get('oracle_text','')
    for reminder in re.findall(r'\([^\n]*?\)',text):
        if reminder not in REMINDERS:raise ValueError('unreviewed combat value reminder')
        text=text.replace(reminder,'')
    trigger=None;keywords=[];effects=None
    source=r'(?:this creature|'+re.escape(record['name'])+r')'
    for line in text.splitlines():
        line=line.strip()
        if not line:continue
        match=re.fullmatch(r'Whenever '+source+r' deals (combat )?damage to (a player|an opponent), (.+)\.',line)
        if match:
            if trigger is not None:raise ValueError('multiple combat value triggers need a new model')
            trigger={'scope':'combat' if match[1] else 'any','recipient':'player' if match[2]=='a player' else 'opponent'}
            body=match[3];effects={'draw':0,'discard':0,'mill':False,'self_counters':0}
            draw=re.fullmatch(r'(?:you )?draw (a|two|three) cards?',body)
            discard=re.fullmatch(r'that player discards (a|two|three) cards?',body)
            both=re.fullmatch(r'that player discards (a|two|three) cards? and you draw (a|two|three) cards?',body)
            counters=re.fullmatch(r'put (a|two|three) \+1/\+1 counters? on (?:it|this creature|'+re.escape(record['name'])+r')',body)
            if draw:effects['draw']=NUMBERS[draw[1]]
            elif discard:effects['discard']=NUMBERS[discard[1]]
            elif both:effects.update(discard=NUMBERS[both[1]],draw=NUMBERS[both[2]])
            elif body=='that player mills that many cards':effects['mill']=True
            elif counters:effects['self_counters']=NUMBERS[counters[1]]
            else:raise ValueError('unreviewed combat value effect')
        else:
            canonical={k.lower():k for k in PASSIVE}
            values=[s.strip().lower() for s in line.split(',')]
            if any(s not in canonical for s in values):raise ValueError('additional combat value source clause needs review')
            keywords += [canonical[s] for s in values]
    if trigger is None:raise ValueError('missing player damage value trigger')
    expected=set(keywords)|({'Mill'} if effects['mill'] else set())
    if len(keywords)!=len(set(keywords)) or expected!=set(record.get('keywords',[])):
        raise ValueError('combat value keyword text and Oracle metadata disagree')
    return dict(schema=8,**trigger,effects=effects,keywords=sorted(keywords))


def spec(d,record):
    from recipes import cost_from_oracle
    expected=parameters(record)
    if (d.get('family')!=FAMILY or type(d.get('version')) is not int or d.get('version')!=2 or d.get('oracle_text')!=record['oracle_text']
            or json.dumps(d.get('params'),sort_keys=True)!=json.dumps(expected,sort_keys=True)
            or not isinstance(d.get('variants'),list) or not d['variants']):
        raise ValueError('combat value requires exact Oracle parameters, full text and explicit variants')
    if expected.get('hybrid_pairs'):
        from defensive_creature_recipes import reviewed_payment
        mana,_=reviewed_payment(record,dict(payment_colors=[pair[0] for pair in expected['hybrid_pairs']]))
    else:mana=cost_from_oracle(record)
    p=dict(expected,family=FAMILY,power=int(record['power']),toughness=int(record['toughness']),
           mana=mana,legendary='Legendary' in record['type_line'])
    if record['name'] in PINS.get('input_profiles',{}):p['input_profile']=PINS['input_profiles'][record['name']]
    if len(set(d['variants']))!=len(d['variants']) or any(v not in variants(p) for v in d['variants']):
        raise ValueError('unknown or duplicate combat value variant')
    return p


def _classic_variants(p):
    base=['unblocked','cast','payment','insufficient_mana','fresh_attack','blocked','zero_power',
          'other_attack','attack_pair','defending','tapped','sick','pump','cleanup','two_hits',
          'bounce_after','ready_snapshot','combat_snapshot','bare_zones']
    if not p['legendary']:base.append('two_sources')
    if any(p['mana'][1:]):base.append('wrong_color')
    if p['effects']['discard']:base += ['empty_hand','short_hand']
    if p['effects']['draw'] or p['effects']['mill']:base.append('short_library')
    if p['effects']['mill']:base.append('empty_library')
    return [v+'_'+str(owner) for owner in (0,1) for v in base]


def validate_witnesses(oracle,registry):
    if not isinstance(oracle,dict):raise ValueError('combat value needs pinned witness Oracle context')
    for name,pin in PINS['cards'].items():
        if name not in registry or any(oracle.get(name,{}).get(k)!=v for k,v in pin.items()):
            raise ValueError('combat value witness changed: '+name)
    for name,csvid in PINS.get('token_registry',{}).items():
        if registry.get(name,{}).get('csvid')!=csvid:raise ValueError('combat value token registry bridge changed: '+name)
    for name,profile in PINS.get('input_profiles',{}).items():
        if name not in PINS['cards'] or profile!='optional_trigger':raise ValueError('combat value input profile requires a full pinned source')
    for name,csvid in PINS.get('implemented_color_witnesses',{}).items():
        row=registry.get(name,{})
        if (row.get('csvid')!=csvid or row.get('kind')!='card' or row.get('handler') in (None,'','NULL','card_dummy')
                or oracle.get(name,{}).get('layout')!='normal' or 'Creature' not in oracle[name]['type_line'] or 'Artifact' in oracle[name]['type_line']):
            raise ValueError('combat value implemented nonartifact color witness changed: '+name)


def _mirror(case):
    for item in case['setup']:item['player']=1-item['player']
    seen=set()
    for action in case['steps']:
        if id(action) in seen:continue
        seen.add(id(action))
        if action.get('player') in (0,1):action['player']=1-action['player']
        if action['op']=='choose_player':action['value']=1-action['value']


def _expand_classic(name,csvid,d,p,selected,registry,oracle):
    from recipes import card,step,observe,cost_from_oracle
    validate_witnesses(oracle,registry)
    if (not isinstance(selected,list) or not selected or len(set(selected))!=len(selected)
            or any(v not in variants(p) for v in selected)):
        raise ValueError('combat value expansion requires unique reviewed variants')
    result=[]
    for requested in selected:
        mode,owner=requested.rsplit('_',1);owner=int(owner);bare=mode=='bare_zones'
        keys=set(p['keywords']);effect=p['effects'];setup=[];entities={}
        zones=[{z:[] for z in ('hand','graveyard','exile','library')} for _ in (0,1)]
        pools=[[10]*6,[10]*6];life=[20,20]
        for player in (0,1):
            zones[player]['hand']=[] if bare else ['Forest']*3
            zones[player]['graveyard']=[] if bare else ['Island']
            zones[player]['exile']=[] if bare else ['Mountain']
            zones[player]['library']=list(LIBRARY) if not bare or player==0 and effect['draw'] or player==1 and effect['mill'] else []
        if mode=='empty_hand':zones[1]['hand']=[]
        if mode=='short_hand':zones[1]['hand']=['Forest']
        if mode=='empty_library':zones[1]['library']=[]
        if mode=='short_library':zones[0 if effect['draw'] else 1]['library']=list(LIBRARY[:effect['draw']]) if effect['draw'] else ['Forest']
        # Ordered library and unrelated-zone cards precede source placement.
        for player in (0,1):
            for zone,names in zones[player].items():
                setup += [card(n,f'stock_{zone}_{player}_{i}',player,zone) for i,n in enumerate(names)]
        def add(nm,ref,player=0,zone='battlefield',state=None):
            item=card(nm,ref,player,zone)
            if state:item['state']=state
            setup.append(item)
            if zone=='hand':zones[player]['hand'].append(nm)
            r=oracle[nm]
            entities[ref]=dict(name=nm,player=player,zone=zone,power=int(r['power']),toughness=int(r['toughness']),
                               counters=0,temp_power=0,temp_toughness=0,damage=0,tapped=bool(state and 'tapped' in state),attacking=False)
        casting=mode in ('cast','payment','insufficient_mana','wrong_color','fresh_attack')
        add(name,'source',0,'hand' if casting else 'battlefield',['tapped'] if mode=='tapped' else ['sick'] if mode=='sick' else None)
        add('Hill Giant','idle');add('Wall of Denial','wall',1)
        if mode=='two_sources':add(name,'copy')
        if mode=='defending':add('Hill Giant','enemy',1)
        helper='Giant Growth' if mode=='pump' else 'Hydrosurge' if mode=='zero_power' else 'Unsummon' if mode=='bounce_after' else None
        if helper:setup.append(card(helper,'helper',0,'hand'));zones[0]['hand'].append(helper)
        if mode=='payment':pools[0]=list(p['mana'])
        if mode=='insufficient_mana':pools[0]=[0]*6
        if mode=='wrong_color':pools[0]=[60,0,0,0,0,0]
        actor=1 if mode=='defending' else 0
        pre=[step('turn',player=actor,value=0),step('discard_policy',value=1)]
        pre += [step('set_mana',player=player,color=color,value=value) for player in (0,1) for color,value in enumerate(pools[player])]
        actions=[];combat_cut=None
        def pay(player,amount):pools[player][0]-=sum(amount)
        def move(ref,zone):
            e=entities[ref]
            if e['zone']!='battlefield':zones[e['player']][e['zone']].remove(e['name'])
            if zone!='battlefield':zones[e['player']][zone].append(e['name'])
            e['zone']=zone
            if zone!='battlefield':e.update(counters=0,temp_power=0,temp_toughness=0,damage=0,attacking=False)
        def receipts():
            out=[step('assert_stack',value=0),step('assert_choices_consumed')]
            for ref,e in entities.items():
                alive=e['zone']=='battlefield'
                out += [observe(ref,'in_play',int(alive))]
                if alive:
                    out += [observe(ref,'power',e['power']+e['temp_power']+e['counters']),
                            observe(ref,'toughness',e['toughness']+e['temp_toughness']+e['counters']),
                            observe(ref,'plus1_counters',e['counters']),observe(ref,'damage',e['damage']),
                            observe(ref,'tapped',int(e['tapped'])),observe(ref,'attacking',int(e['attacking']))]
            universe=sorted({c['name'] for c in setup}|set(LIBRARY))
            for player in (0,1):
                out += [step('assert_life',player=player,value=life[player]),step('assert_mana_total',player=player,value=sum(pools[player])),
                        step('assert_zone',player=player,zone='battlefield',value=sum(e['zone']=='battlefield' and e['player']==player for e in entities.values()))]
                for zone,names in zones[player].items():
                    out.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                    if zone=='library':out += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[n]['csvid']) for i,n in enumerate(names)]
                    else:
                        counts=Counter(names)
                        out += [step('assert_zone_multiplicity',player=player,zone=zone,index=registry[n]['csvid'],value=counts[n]) for n in universe]
                out += [step('assert_hand_graveyard_total',player=player,index=registry[n]['csvid'],value=zones[player]['hand'].count(n)+zones[player]['graveyard'].count(n)) for n in universe]
            return out
        def hit(ref,amount):
            if amount<=0:return
            life[1]-=amount
            entities[ref]['counters']+=effect['self_counters']
            for _ in range(effect['discard']):
                if zones[1]['hand']:zones[1]['graveyard'].append(zones[1]['hand'].pop(0))
            for _ in range(effect['draw']):
                if not zones[0]['library']:raise ValueError('draw beyond empty library requires a loss-observation model')
                zones[0]['hand'].append(zones[0]['library'].pop(0))
            if effect['mill']:
                count=min(amount,len(zones[1]['library']));zones[1]['graveyard']+=zones[1]['library'][:count];del zones[1]['library'][:count]
        if casting:
            legal=mode not in ('insufficient_mana','wrong_color')
            actions += [step('cast',ref='source',value=int(legal)),step('assert_stack',value=int(legal)),step('assert_choices_consumed')]
            if legal:
                pay(0,p['mana']);actions += [observe('source','in_hand',0),step('assert_mana_total',player=0,value=sum(pools[0])),step('resolve'),step('assert_choices_consumed')]
                move('source','battlefield')
            else:actions.append(observe('source','in_hand',1))
        if mode in ('pump','zero_power'):
            actions += [step('choose_target',player=0,ref='source'),step('cast',ref='helper',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
            pay(0,cost_from_oracle(oracle[helper]));zones[0]['hand'].remove(helper);zones[0]['graveyard'].append(helper)
            entities['source']['temp_power']=3 if mode=='pump' else -5
            entities['source']['temp_toughness']=3 if mode=='pump' else 0
        def combat(first=True):
            nonlocal actions,combat_cut
            actions.append(step('begin_attackers'));attackers=[]
            if mode=='defending':attackers=['enemy'];actions.append(step('attack',ref='enemy',value=1))
            elif mode=='other_attack':attackers=['idle'];actions.append(step('attack',ref='idle',value=1))
            else:
                legal=mode!='tapped' and (mode not in ('sick','fresh_attack') or 'Haste' in keys)
                actions.append(step('attack',ref='source',value=int(legal)))
                if legal:attackers=['source']
                if mode=='attack_pair':attackers.append('idle');actions.append(step('attack',ref='idle',value=1))
                if mode=='two_sources':attackers.append('copy');actions.append(step('attack',ref='copy',value=1))
            for ref in attackers:
                entities[ref]['attacking']=True;entities[ref]['tapped']=not (ref in ('source','copy') and 'Vigilance' in keys)
            actions += [step('finish_attackers')]
            if first:combat_cut=len(actions)
            actions += [step('begin_blockers')]
            blocked=mode in ('blocked','defending')
            if mode=='blocked':actions.append(step('block',ref='wall',other='source',value=1))
            if mode=='defending':actions.append(step('block',ref='source',other='enemy',value=1))
            actions.append(step('advance_combat'))
            if mode=='blocked' and attackers:
                e=entities['source'];entities['wall']['damage']+=max(0,e['power']+e['temp_power']+e['counters'])
            elif mode=='defending':
                source=entities['source'];enemy=entities['enemy'];source['damage']+=3;enemy['damage']+=max(0,source['power']+source['counters'])
                if source['damage']>=source['toughness']+source['counters']:move('source','graveyard')
                if enemy['damage']>=enemy['toughness']:move('enemy','graveyard')
            else:
                for ref in attackers:
                    e=entities[ref];amount=max(0,e['power']+e['temp_power']+e['counters'])
                    if ref in ('source','copy'):hit(ref,amount)
                    else:life[1]-=amount
            actions += [step('combat_damage'),step('end_combat')]
            for e in entities.values():e['attacking']=False
        if not casting or mode=='fresh_attack':combat()
        if mode=='two_hits':
            actions += receipts()
            actions += [step('set_mana',player=player,color=color,value=0) for player in (0,1) for color in range(6)];pools=[[0]*6,[0]*6]
            actions += [step('advance_turn'),step('untap_step',player=1),step('advance_turn'),step('untap_step',player=0)]
            # The full untap wrapper leaves its genuine untap phase. Select
            # MAIN1 for the same already-current owner after both real turns;
            # this does not replace turn gates, untapping, or their history.
            actions.append(step('turn',player=0,value=0))
            for e in entities.values():e.update(tapped=False,damage=0,temp_power=0,temp_toughness=0)
            combat(False)
        if mode in ('pump','zero_power','cleanup'):
            actions += receipts()
            actions += [step('set_mana',player=player,color=color,value=0) for player in (0,1) for color in range(6)];pools=[[0]*6,[0]*6]
            actions += [step('cleanup'),step('refresh')]
            for e in entities.values():e.update(damage=0,temp_power=0,temp_toughness=0)
        if mode=='bounce_after':
            actions += receipts()+[step('choose_target',player=0,ref='source'),step('cast',ref='helper',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
            pay(0,cost_from_oracle(oracle[helper]));zones[0]['hand'].remove(helper);zones[0]['graveyard'].append(helper);move('source','hand')
            actions += [step('bind_hand',player=0,value=csvid,ref='source'),observe('source','in_hand',1),observe('source','plus1_counters',0)]
        checks=receipts()
        if mode=='payment':checks += [step('assert_mana',player=0,color=color,value=0) for color in range(6)]
        steps=pre+actions+checks
        if mode in ('ready_snapshot','combat_snapshot'):
            split=0 if mode=='ready_snapshot' else combat_cut
            replay=actions[split:]+checks
            steps=pre+copy.deepcopy(actions[:split])+[step('capture')]+copy.deepcopy(replay)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(replay)+[step('assert_recorded_state')]
        gaps=['Noncombat damage from this source; additional combat steps, prevention/replacement, live control changes and multiplayer.',
              'Passive keyword interactions beyond the declared casting, haste/tapping and blocking scenarios (including trample excess) remain separate coverage.',
              'Drawing beyond an empty library and its loss condition remain outside this model; exact-last-card draw and empty/short-library mill are covered.']
        if p['scope']=='any':gaps.append('Any-damage wording is exercised through real combat damage; same-source noncombat damage needs a separate granting-effect fixture.')
        case=dict(name=f'CombatValueTriggerRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,
                  _recipe=dict(family=FAMILY,version=2,variant=requested,params=copy.deepcopy(d['params']),oracle_text=d['oracle_text'],untested_components=gaps))
        if owner:_mirror(case)
        result.append(case)
    return result


ADJACENT_KEYWORDS=PASSIVE|{'Fear','Intimidate','Deathtouch','Prowess'}
ADJACENT_REMINDERS={
    "(This creature can't be blocked except by artifact creatures and/or black creatures.)",
    "(This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)",
    '(Any amount of damage this deals to a creature is enough to destroy it.)',
    '(Whenever you cast a noncreature spell, this creature gets +1/+1 until end of turn.)',
}
BOUNDARY_MODES=('noncombat_player','noncombat_self','noncombat_creature','noncombat_tapped',
                'noncombat_sick','noncombat_fresh','noncombat_snapshot','noncombat_twice')


def _single_parameters(record):
    try:return _classic_parameters(record)
    except ValueError:pass
    if (not isinstance(record,dict) or record.get('layout')!='normal' or record.get('card_faces')
            or record.get('selected_face') or 'Creature' not in record.get('type_line','')
            or any(not re.fullmatch(r'\d+',record.get(k,'')) for k in ('power','toughness'))
            or not 1<=int(record['power'])<=5 or not 1<=int(record['toughness'])<=7):
        raise ValueError('adjacent combat value needs a fixed normal creature in the reviewed range')
    text=record['oracle_text']
    for reminder in re.findall(r'\([^\n]*?\)',text):
        if reminder not in REMINDERS|ADJACENT_REMINDERS:raise ValueError('unreviewed adjacent combat reminder')
        text=text.replace(reminder,'')
    aliases={record['name'],record['name'].split(',')[0],'this creature'}
    source='(?:'+'|'.join(re.escape(n) for n in sorted(aliases))+')'
    trigger=None;keywords=[];inactive=[];effect=None
    for line in text.splitlines():
        line=line.strip()
        if not line:continue
        match=re.fullmatch(r'Whenever '+source+r' deals (combat )?damage to (a player|an opponent), (.+)\.',line)
        if match:
            if trigger is not None:raise ValueError('multiple adjacent combat triggers require another model')
            trigger=dict(scope='combat' if match[1] else 'any',recipient='player' if match[2]=='a player' else 'opponent')
            body=match[3];effect=dict(draw=0,discard=0,mill=False,self_counters=0,target_draw=0,optional=False,exchange=None,token=None)
            draw=re.fullmatch(r'(?:you (may )?)?draw (a|two|three|that many) cards?',body)
            if draw:effect.update(draw='damage' if draw[2]=='that many' else NUMBERS[draw[2]],optional=bool(draw[1]))
            elif body=='you and that player each draw that many cards':effect.update(draw='damage',target_draw='damage')
            elif body=='you may draw a card. If you do, discard a card':effect.update(draw=1,optional=True,exchange='loot')
            elif body=='you may discard a card. If you do, draw a card':effect.update(draw=1,optional=True,exchange='rummage')
            elif body=='that player discards a card and you create a tapped 2/2 black Zombie creature token':
                effect.update(discard=1,token=dict(registry_name='Zombie',count=1,power=2,toughness=2,color=2,subtypes=['Zombie'],tapped=True))
            else:raise ValueError('unreviewed adjacent combat value effect')
        elif ':' in line and re.match(r'^(?:\{|Discard |Sacrifice |Remove |Pay |Tap |Untap |Exile |Return )',line):
            inactive.append(line)
        else:
            canonical={k.lower():k for k in ADJACENT_KEYWORDS};values=[v.strip().lower() for v in line.split(',')]
            if any(v not in canonical for v in values):raise ValueError('unreviewed adjacent source clause')
            keywords += [canonical[v] for v in values]
    if trigger is None or len(keywords)!=len(set(keywords)) or set(keywords)!=set(record.get('keywords',[])):
        raise ValueError('adjacent combat trigger or keyword metadata mismatch')
    return dict(schema=9,**trigger,effects=effect,keywords=sorted(keywords),inactive_abilities=inactive)


def _single_variants(p):
    if p['schema']==8:return _classic_variants(p)+[v+'_'+str(owner) for owner in (0,1) for v in BOUNDARY_MODES]
    base=['unblocked','cast','payment','insufficient_mana','fresh_attack','blocked','zero_power','other_attack',
          'pump','cleanup','two_hits','ready_snapshot','combat_snapshot','bare_zones']
    e=p['effects']
    if not p['legendary'] and not e['optional']:base.append('two_sources')
    if any(p['mana'][1:]):base.append('wrong_color')
    if e['discard']:base += ['empty_hand','short_hand']
    if e['draw'] or e['target_draw']:base.append('short_library')
    if e['exchange']:base.append('empty_source_hand')
    if e['token']:base += ['token_bounce','token_next_turn']
    result=[v+'_'+str(owner) for owner in (0,1) for v in base+list(BOUNDARY_MODES)]
    if e['optional']:result += ['declined_0']+(['declined_1'] if e['exchange']=='rummage' else [])
    return result


class _AdjacentBoard:
    """Independent zone/cost model; emits only ordinary game operations."""
    def __init__(self,name,csvid,d,p,requested,rows,oracle):
        from recipes import card,step,observe,cost_from_oracle
        self.card=card;self.step=step;self.observe=observe;self.cost=cost_from_oracle
        self.name=name;self.csvid=csvid;self.d=d;self.p=p;self.rows=rows;self.oracle=oracle
        self.mode,self.owner=requested.rsplit('_',1);self.owner=int(self.owner);self.requested=requested
        self.e=p['effects'];self.keys=set(p['keywords']);self.steps=[];self.setup=[];self.entities={};self.tokens=[];self.bound=set()
        self.zones=[{z:[] for z in ('hand','graveyard','exile','library')} for _ in (0,1)];self.life=[20,20];self.mana=[60,60]
        self.boundary=self.mode.startswith('noncombat_');self.optional=self.e.get('optional',False)
        bare=self.mode=='bare_zones';hand=2 if self.e['draw']=='damage' or self.e.get('target_draw')=='damage' else 3
        for player in (0,1):
            self.zones[player].update(hand=[] if bare else ['Forest']*hand,graveyard=[] if bare else ['Island'],exile=[] if bare else ['Mountain'],library=list(LIBRARY))
        if self.mode=='empty_source_hand':self.zones[0]['hand']=[]
        if self.mode=='empty_hand':self.zones[1]['hand']=[]
        if self.mode=='short_hand':self.zones[1]['hand']=['Forest']
        if self.mode=='short_library':
            for player,key in ((0,'draw'),(1,'target_draw')):
                if self.e.get(key):self.zones[player]['library']=list(LIBRARY[:self.p['power'] if self.e[key]=='damage' else self.e[key]])
        for player in (0,1):
            for zone,names in self.zones[player].items():self.setup += [card(n,f'stock_{zone}_{player}_{i}',player,zone) for i,n in enumerate(names)]
        self.casting=self.mode in ('cast','payment','insufficient_mana','wrong_color','fresh_attack','noncombat_fresh')
        state=['tapped'] if self.mode in ('tapped','noncombat_tapped') else ['sick'] if self.mode in ('sick','noncombat_sick') else None
        self.add(name,'source',0,'hand' if self.casting else 'battlefield',state)
        self.add('Hill Giant','idle');blocker='Darksteel Gargoyle' if self.keys&{'Fear','Intimidate'} else 'Wall of Denial'
        self.add(blocker,'wall',1)
        if self.mode=='two_sources':self.add(name,'copy')
        if self.boundary:self.add('Hermetic Study','grant',0,'hand')
        helper='Giant Growth' if self.mode=='pump' else 'Hydrosurge' if self.mode=='zero_power' else 'Unsummon' if self.mode=='token_bounce' else None
        if helper:self.setup.append(card(helper,'helper',0,'hand'));self.zones[0]['hand'].append(helper)
        if self.mode=='payment':self.mana[0]=sum(p['mana'])
        if self.mode=='insufficient_mana':self.mana[0]=0
        self.emit('turn',player=0,value=0);self.emit('discard_policy',value=1)
        if self.e.get('exchange')=='rummage':self.emit('present_ai_dialogs',value=1)
        for player in (0,1):
            pool=p['mana'] if player==0 and self.mode=='payment' else [0]*6 if player==0 and self.mode=='insufficient_mana' else [60,0,0,0,0,0] if player==0 and self.mode=='wrong_color' else [10]*6
            for color,value in enumerate(pool):self.emit('set_mana',player=player,color=color,value=value)
        self.ready=len(self.steps)

    def emit(self,op,**kw):self.steps.append(self.step(op,**kw))
    def add(self,name,ref,player=0,zone='battlefield',state=None):
        item=self.card(name,ref,player,zone)
        if state:item['state']=state
        self.setup.append(item)
        if zone=='hand':self.zones[player]['hand'].append(name)
        r=self.oracle[name];creature='Creature' in r['type_line']
        self.entities[ref]=dict(name=name,player=player,zone=zone,creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,counters=0,boost=0,damage=0,tapped=bool(state and 'tapped' in state),token=False)
    def move(self,ref,dest):
        e=self.entities[ref]
        if e['zone']!='battlefield':self.zones[e['player']][e['zone']].remove(e['name'])
        if dest!='battlefield' and not e['token']:self.zones[e['player']][dest].append(e['name'])
        e['zone']='gone' if e['token'] and dest!='battlefield' else dest
    def power(self,ref):
        e=self.entities[ref];return max(0,e['power']+e['counters']+e['boost'])
    def trigger_inputs(self,ref,amount,target=1,combat=True):
        eligible=amount>0 and (combat or self.p['scope']=='any') and (target==1 or self.p['recipient']=='player')
        if not eligible or not self.optional:return
        accept=self.mode!='declined'
        if self.e.get('exchange')=='rummage':
            if self.zones[0]['hand']:self.emit('choose_dialog',player=0,value=0 if accept else 1)
        elif accept and self.owner==0:self.emit('choose_trigger',player=0,ref=ref)
    def draw(self,player,count):
        for _ in range(count):
            if not self.zones[player]['library']:raise ValueError('unreviewed empty-library draw loss')
            self.zones[player]['hand'].append(self.zones[player]['library'].pop(0))
    def discard(self,player,count):
        for _ in range(min(count,len(self.zones[player]['hand']))):
            name=min(self.zones[player]['hand'],key=lambda n:self.rows[n]['csvid']);self.zones[player]['hand'].remove(name);self.zones[player]['graveyard'].append(name)
    def hit(self,ref,amount,target=1,combat=True,primary=True):
        self.life[target]-=amount
        if not primary or amount<=0 or not (combat or self.p['scope']=='any') or target==0 and self.p['recipient']=='opponent':return
        if self.optional and self.mode=='declined':return
        if self.e.get('exchange')=='rummage' and not self.zones[0]['hand']:return
        self.entities[ref]['counters']+=self.e['self_counters'];self.discard(target,self.e['discard'])
        if self.e.get('exchange')=='rummage':self.discard(0,1)
        self.draw(0,amount if self.e['draw']=='damage' else self.e['draw'])
        if self.e.get('target_draw'):self.draw(target,amount if self.e['target_draw']=='damage' else self.e['target_draw'])
        if self.e.get('exchange')=='loot':self.discard(0,1)
        if self.e['mill']:
            count=min(amount,len(self.zones[target]['library']));self.zones[target]['graveyard']+=self.zones[target]['library'][:count];del self.zones[target]['library'][:count]
        if self.e.get('token'):
            t=self.e['token']
            for _ in range(t['count']):
                ref=f'made_{len(self.tokens)}';self.tokens.append(ref)
                self.entities[ref]=dict(name=t['registry_name'],player=0,zone='battlefield',creature=True,power=t['power'],toughness=t['toughness'],counters=0,boost=0,damage=0,tapped=t['tapped'],token=True)
    def bind_tokens(self):
        for index,ref in enumerate(self.tokens):
            if ref not in self.bound:
                self.emit('bind_permanent_copy',player=0,value=self.rows[self.entities[ref]['name']]['csvid'],index=index,ref=ref);self.bound.add(ref)
    def receipts(self):
        self.bind_tokens();self.emit('assert_stack',value=0);self.emit('assert_choices_consumed')
        for ref,e in self.entities.items():
            alive=e['zone']=='battlefield';self.emit('assert_card',ref=ref,property='in_play',value=int(alive))
            if alive:
                self.emit('assert_card',ref=ref,property='tapped',value=int(e['tapped']))
                if e['creature']:
                    for prop,value in (('power',e['power']+e['counters']+e['boost']),('toughness',e['toughness']+e['counters']+e['boost']),('plus1_counters',e['counters']),('damage',e['damage'])):self.emit('assert_card',ref=ref,property=prop,value=value)
                if e['token']:
                    for prop,value in (('token',1),('color',self.e['token']['color']),('subtype_Zombie',1)):self.emit('assert_matching_permanents',player=e['player'],index=self.rows[e['name']]['csvid'],property=prop,value=value)
        universe=sorted({c['name'] for c in self.setup}|{e['name'] for e in self.entities.values()}|set(LIBRARY))
        for player in (0,1):
            self.emit('assert_life',player=player,value=self.life[player]);self.emit('assert_mana_total',player=player,value=self.mana[player])
            self.emit('assert_zone',player=player,zone='battlefield',value=sum(e['player']==player and e['zone']=='battlefield' for e in self.entities.values()))
            for zone,names in self.zones[player].items():
                self.emit('assert_zone',player=player,zone=zone,value=len(names))
                if zone=='library':
                    for i,n in enumerate(names):self.emit('assert_zone_card',player=player,zone=zone,index=i,value=self.rows[n]['csvid'])
                else:
                    for n in universe:self.emit('assert_zone_multiplicity',player=player,zone=zone,index=self.rows[n]['csvid'],value=names.count(n))
    def spell(self,ref,name,target=None,entity=False):
        if target:self.emit('choose_target',player=0,ref=target)
        self.emit('cast',ref=ref,value=1);self.emit('assert_stack',value=1);self.emit('assert_card',ref=ref,property='in_hand',value=0)
        self.mana[0]-=sum(self.cost(self.oracle[name]));self.emit('assert_mana_total',player=0,value=self.mana[0]);self.emit('resolve');self.emit('assert_choices_consumed')
        if entity:self.move(ref,'battlefield')
        else:self.zones[0]['hand'].remove(name);self.zones[0]['graveyard'].append(name)
        if 'Prowess' in self.keys and 'Creature' not in self.oracle[name]['type_line']:
            for key in ('source','copy'):
                if key in self.entities and self.entities[key]['zone']=='battlefield':self.entities[key]['boost']+=1
    def next_own_turn(self):
        for player in (0,1):
            for color in range(6):self.emit('set_mana',player=player,color=color,value=0)
        self.mana=[0,0];self.emit('advance_turn');self.emit('untap_step',player=1);self.emit('advance_turn');self.emit('untap_step',player=0);self.emit('turn',player=0,value=0)
        for e in self.entities.values():e.update(tapped=False,damage=0,boost=0)
    def combat(self,only_token=False):
        self.emit('begin_attackers');attackers=[]
        if only_token:attackers=[self.tokens[0]]
        elif self.mode=='other_attack':attackers=['idle']
        elif self.mode not in ('tapped',) and (self.mode!='fresh_attack' or 'Haste' in self.keys):attackers=['source']+(['copy'] if self.mode=='two_sources' else [])
        else:self.emit('attack',ref='source',value=0)
        for ref in attackers:
            self.emit('attack',ref=ref,value=1);self.entities[ref]['tapped']=not(ref in ('source','copy') and 'Vigilance' in self.keys)
        self.emit('finish_attackers');cut=len(self.steps);self.emit('begin_blockers')
        if self.mode=='blocked':self.emit('block',ref='wall',other='source',value=1)
        self.emit('advance_combat')
        if self.mode=='blocked':
            source=self.entities['source'];wall=self.entities['wall'];damage=self.power('source');wall['damage']+=damage;source['damage']+=wall['power']
            if damage and ('Deathtouch' in self.keys or wall['damage']>=wall['toughness']) and 'Indestructible' not in self.oracle[wall['name']]['keywords']:self.move('wall','graveyard')
            if source['damage']>=source['toughness']+source['counters']+source['boost']:self.move('source','graveyard')
        else:
            for ref in attackers:
                amount=self.power(ref);primary=ref in ('source','copy')
                if primary:self.trigger_inputs(ref,amount)
                self.hit(ref,amount,primary=primary)
        self.emit('combat_damage');self.emit('resolve');self.emit('end_combat');self.bind_tokens();return cut
    def activate_grant(self):
        legal=self.mode!='noncombat_tapped' and (self.mode not in ('noncombat_sick','noncombat_fresh') or 'Haste' in self.keys)
        self.emit('can_activate',ref='grant',value=int(legal))
        if legal:
            if self.mode=='noncombat_creature':self.emit('choose_target',player=0,ref='idle')
            else:self.emit('choose_player',player=0,value=0 if self.mode=='noncombat_self' else 1)
        self.emit('activate',ref='grant',value=int(legal));self.emit('assert_stack',value=int(legal))
        if legal:
            self.entities['source']['tapped']=True;self.emit('assert_card',ref='source',property='tapped',value=1);self.emit('assert_card',ref='grant',property='tapped',value=0)
            if self.mode=='noncombat_creature':
                self.entities['idle']['damage']+=1
                if 'Deathtouch' in self.keys:self.move('idle','graveyard')
            else:
                target=0 if self.mode=='noncombat_self' else 1;self.trigger_inputs('source',1,target,False);self.hit('source',1,target,False)
            self.emit('resolve');self.emit('assert_choices_consumed');self.bind_tokens()
    def run(self):
        mode=self.mode
        if self.casting:
            if mode in ('insufficient_mana','wrong_color'):
                self.emit('cast',ref='source',value=0);self.emit('assert_card',ref='source',property='in_hand',value=1)
            else:self.spell('source',self.name,entity=True)
        if self.boundary:
            self.spell('grant','Hermetic Study','source',True)
            if mode in ('noncombat_sick','noncombat_fresh') and 'Haste' not in self.keys:self.emit('assert_card',ref='source',property='sick',value=1)
            split=len(self.steps);self.activate_grant()
            if mode=='noncombat_twice':self.receipts();self.next_own_turn();self.activate_grant()
        elif not self.casting or mode=='fresh_attack':
            if mode in ('pump','zero_power'):
                self.spell('helper','Giant Growth' if mode=='pump' else 'Hydrosurge','source');self.entities['source']['boost']+=3 if mode=='pump' else 0
                if mode=='zero_power':self.entities['source']['power']-=5
            cut=self.combat()
            if mode in ('two_hits','token_next_turn'):
                self.receipts();self.next_own_turn();self.combat(mode=='token_next_turn')
            if mode in ('pump','zero_power','cleanup'):
                self.receipts()
                for player in (0,1):
                    for color in range(6):self.emit('set_mana',player=player,color=color,value=0)
                self.mana=[0,0];self.emit('cleanup');self.emit('refresh')
                for e in self.entities.values():e.update(boost=0,damage=0)
                if mode=='zero_power':self.entities['source']['power']+=5
            if mode=='token_bounce':self.receipts();self.spell('helper','Unsummon',self.tokens[0]);self.move(self.tokens[0],'hand')
        self.receipts();steps=self.steps
        if mode in ('ready_snapshot','combat_snapshot','noncombat_snapshot'):
            point=self.ready if mode=='ready_snapshot' else cut if mode=='combat_snapshot' else split
            replay=steps[point:];steps=copy.deepcopy(steps[:point])+[self.step('capture')]+copy.deepcopy(replay)+[self.step('record_state'),self.step('ai_roundtrip'),self.step('restore')]+copy.deepcopy(replay)+[self.step('assert_recorded_state')]
        gaps=list(self.p.get('inactive_abilities',[]))+['Additional combat steps, replacement/prevention, other damage-granting effects and multiplayer; optional multiple simultaneous trigger selection and AI trigger decline remain separate observations.']
        case=dict(name=f'CombatValueTriggerRecipe_{self.csvid}_{self.requested}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=steps,
                  _recipe=dict(family=FAMILY,version=2,variant=self.requested,params=copy.deepcopy(self.d['params']),oracle_text=self.d['oracle_text'],untested_components=gaps))
        if self.owner:_mirror(case)
        return case


def expand(name,csvid,d,p,selected,registry,oracle):
    validate_witnesses(oracle,registry)
    if not isinstance(selected,list) or not selected or len(set(selected))!=len(selected) or any(v not in variants(p) for v in selected):raise ValueError('unique combat value variants required')
    if p['schema']==13:return [_ConstrainedBoard(name,csvid,d,p,v,registry,oracle).run() for v in selected]
    if p['schema']==12:return [_LibraryBoard(name,csvid,d,p,v,registry,oracle).run() for v in selected]
    if p['schema']==11:return [_RenownBoard(name,csvid,d,p,v,registry,oracle).run() for v in selected]
    if p['schema']==10:return [_TeamBoard(name,csvid,d,p,v,registry,oracle).run() for v in selected]
    advanced=[v for v in selected if p['schema']==9 or v.rsplit('_',1)[0] in BOUNDARY_MODES]
    classic=[v for v in selected if v not in advanced]
    cases=_expand_classic(name,csvid,d,p,classic,registry,oracle) if classic else []
    cases += [_AdjacentBoard(name,csvid,d,p,v,registry,oracle).run() for v in advanced]
    by_variant={c['_recipe']['variant']:c for c in cases}
    return [by_variant[v] for v in selected]


def _through_team_parameters(record):
    try:return _single_parameters(record)
    except ValueError:pass
    if (record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line','') or any(not re.fullmatch(r'\d+',record.get(k,'')) for k in ('power','toughness'))
            or not 1<=int(record['power'])<=6 or not 1<=int(record['toughness'])<=7):
        raise ValueError('team combat requires a fixed normal creature within6/7 reviewed characteristics')
    text=record['oracle_text'];keywords=[];team=None;bonus=None;effect=None;inactive=[];ability_words=set()
    for line in text.splitlines():
        line=line.strip()
        match=re.fullmatch(r'Whenever a (creature|Vampire|Sliver|Snake)( you control)? deals combat damage to a player, (.+)\.',line)
        anthem=re.fullmatch(r'Other (Snake) creatures you control get \+2/\+2\.',line)
        if match:
            if team is not None:raise ValueError('multiple team combat clauses require a separate model')
            team=dict(tribe=None if match[1]=='creature' else match[1],controller='own' if match[2] else 'any')
            body=match[3]
            if body=='put that many +1/+1 counters on it':effect=dict(attacker_counters='damage',draw=0,optional=False,beneficiary='damager')
            elif body=='put a +1/+1 counter on it':effect=dict(attacker_counters=1,draw=0,optional=False,beneficiary='damager')
            elif body=='you may draw a card':effect=dict(attacker_counters=0,draw=1,optional=True,beneficiary='source_controller')
            elif body=='its controller may draw a card':effect=dict(attacker_counters=0,draw=1,optional=True,beneficiary='damager_controller')
            else:raise ValueError('unreviewed team combat value')
        elif anthem:
            if bonus is not None:raise ValueError('multiple team bonuses require a new model')
            bonus=dict(tribe=anthem[1],power=2,toughness=2,other=True)
        elif line=='Devoid (This card has no color.)':
            if record.get('colors')!=[]:raise ValueError('Devoid needs pinned colorless characteristics')
            keywords.append('Devoid')
        elif ':' in line and re.match(r'^(Ferocious — )?\{',line):
            inactive.append(line)
            if line.startswith('Ferocious — '):ability_words.add('Ferocious')
        elif line in PASSIVE:keywords.append(line)
        else:raise ValueError('unreviewed team combat source clause')
    if team is None or len(keywords)!=len(set(keywords)) or set(keywords)|ability_words!=set(record.get('keywords',[])):
        raise ValueError('team combat clause or keyword metadata mismatch')
    if team['controller']=='any' and (team['tribe']!='Sliver' or effect['beneficiary']!='damager_controller'):
        raise ValueError('unreviewed global team combat beneficiary')
    if bonus and (bonus['tribe']!=team['tribe'] or team['controller']!='own'):raise ValueError('unreviewed allied bonus scope')
    return dict(schema=10,scope='combat',recipient='player',keywords=sorted(keywords),team=team,allied_bonus=bonus,
                attacker_counters=effect['attacker_counters'],beneficiary=effect['beneficiary'],inactive_abilities=inactive,
                effects=dict(draw=effect['draw'],discard=0,mill=False,self_counters=0,target_draw=0,optional=effect['optional'],exchange=None,token=None))


def _through_team_variants(p):
    if p['schema']!=10:return _single_variants(p)
    modes=['unblocked','source_hit','enemy_hit','two_attackers','source_pair','blocked','zero_power','pump','cleanup',
           'two_hits','source_bounce','ready_snapshot','combat_snapshot','noncombat_ally','noncombat_source',
           'cast','payment','insufficient_mana','wrong_color','fresh_attack']
    if p['team']['tribe']:modes += ['wrong_type_hit','mixed_attack']
    if p['effects']['optional']:modes.append('declined')
    return [v+'_'+str(owner) for owner in (0,1) for v in modes if not(v=='declined' and owner==1 and p.get('input_profile')=='optional_trigger')]


class _TeamBoard(_AdjacentBoard):
    """Separate event source, qualified damaging creature and value beneficiary."""
    def __init__(self,name,csvid,d,p,requested,rows,oracle):
        super().__init__(name,csvid,d,p,requested,rows,oracle)
        tribe=p['team']['tribe'];witness={'Vampire':'Barony Vampire','Sliver':'Spinneret Sliver','Snake':'Sakura-Tribe Scout'}.get(tribe,'Grizzly Bears')
        self.add(witness,'ally');self.add(witness,'ally2');self.add(witness,'enemy_ally',1)
        if self.mode in ('noncombat_ally','noncombat_source') and 'grant' not in self.entities:self.add('Hermetic Study','grant',0,'hand')
        if self.mode=='source_bounce':self.setup.append(self.card('Unsummon','helper',0,'hand'));self.zones[0]['hand'].append('Unsummon')
        self.emit('present_ai_dialogs',value=1)
        self.sync_bonus();self.ready=len(self.steps)

    def is_type(self,ref,tribe):
        return not tribe or tribe in self.oracle[self.entities[ref]['name']]['type_line'].split(' — ')[-1].split()

    def qualifies(self,ref):
        return (self.entities[ref]['creature'] and self.is_type(ref,self.p['team']['tribe'])
                and (self.p['team']['controller']=='any' or self.entities[ref]['player']==0))

    def sync_bonus(self):
        bonus=self.p['allied_bonus'];active=self.entities['source']['zone']=='battlefield'
        for ref,e in self.entities.items():
            expected=2 if bonus and active and ref!='source' and e['player']==0 and e['creature'] and e['zone']=='battlefield' and self.is_type(ref,bonus['tribe']) else 0
            delta=expected-e.get('anthem',0);e['power']+=delta;e['toughness']+=delta;e['anthem']=expected

    def spell(self,*args,**kw):
        super().spell(*args,**kw);self.sync_bonus()

    def team_effects(self,hits,combat=True):
        pending={0:0,1:0}
        for ref,amount in hits:
            if amount<=0 or not combat or not self.qualifies(ref):continue
            counters=self.p['attacker_counters']
            self.entities[ref]['counters']+=amount if counters=='damage' else counters
            if self.e['draw']:
                who=self.entities[ref]['player'] if self.p['beneficiary']=='damager_controller' else 0
                pending[who]+=self.e['draw']
        for who,count in pending.items():
            if count:
                if self.p.get('input_profile')=='optional_trigger':
                    if self.mode!='declined' and self.owner==0:
                        for _ in range(count):self.emit('choose_trigger',player=0,ref='source')
                else:self.emit('choose_dialog',player=who,value=2 if self.mode=='declined' else 0)
                if self.mode!='declined':self.draw(who,count)

    def receipts(self):
        super().receipts()
        if 'Devoid' in self.keys and self.entities['source']['zone']=='battlefield':
            self.emit('assert_matching_permanents',player=0,index=self.csvid,property='color',value=0)

    def team_combat(self):
        mode=self.mode;actor=1 if mode=='enemy_hit' else 0
        if actor:self.emit('turn',player=actor,value=0)
        self.emit('begin_attackers')
        attackers=['enemy_ally'] if actor else ['source'] if mode in ('source_hit','fresh_attack') else ['ally','ally2'] if mode=='two_attackers' else ['source','ally'] if mode=='source_pair' else ['idle'] if mode=='wrong_type_hit' else ['ally','idle'] if mode=='mixed_attack' else ['ally']
        if mode=='fresh_attack' and 'Haste' not in self.keys:self.emit('attack',ref='source',value=0);attackers=[]
        for ref in attackers:
            self.emit('attack',ref=ref,value=1);self.entities[ref]['tapped']=not(ref=='source' and 'Vigilance' in self.keys)
        self.emit('finish_attackers');cut=len(self.steps);self.emit('begin_blockers')
        if mode=='blocked':self.emit('block',ref='wall',other='ally',value=1)
        self.emit('advance_combat');hits=[]
        for ref in attackers:
            amount=self.power(ref)
            if mode=='blocked':
                self.entities['wall']['damage']+=amount
                if amount>=self.entities['wall']['toughness']:self.move('wall','graveyard')
            else:self.life[1-actor]-=amount;hits.append((ref,amount))
        self.team_effects(hits);self.emit('combat_damage');self.emit('resolve');self.emit('end_combat');return cut

    def grant_damage(self,ref):
        self.spell('grant','Hermetic Study',ref,True);split=len(self.steps)
        self.emit('can_activate',ref='grant',value=1);self.emit('choose_player',player=0,value=1)
        self.emit('activate',ref='grant',value=1);self.emit('assert_stack',value=1)
        self.entities[ref]['tapped']=True;self.emit('assert_card',ref=ref,property='tapped',value=1);self.emit('assert_card',ref='grant',property='tapped',value=0)
        self.life[1]-=1;self.team_effects([(ref,1)],False);self.emit('resolve');return split

    def run(self):
        mode=self.mode
        if self.casting:
            if mode in ('insufficient_mana','wrong_color'):
                self.emit('cast',ref='source',value=0);self.emit('assert_card',ref='source',property='in_hand',value=1)
            else:self.spell('source',self.name,entity=True)
        if mode in ('noncombat_ally','noncombat_source'):self.grant_damage('ally' if mode=='noncombat_ally' else 'source')
        elif not self.casting or mode=='fresh_attack':
            if mode in ('pump','zero_power'):
                self.spell('helper','Giant Growth' if mode=='pump' else 'Hydrosurge','ally');self.entities['ally']['boost']+=3 if mode=='pump' else 0
                if mode=='zero_power':self.entities['ally']['power']-=5
            cut=self.team_combat()
            if mode=='two_hits':self.receipts();self.next_own_turn();self.team_combat()
            if mode=='source_bounce':self.receipts();self.spell('helper','Unsummon','source');self.move('source','hand');self.sync_bonus()
            if mode in ('pump','zero_power','cleanup'):
                self.receipts()
                for player in (0,1):
                    for color in range(6):self.emit('set_mana',player=player,color=color,value=0)
                self.mana=[0,0];self.emit('cleanup');self.emit('refresh')
                for e in self.entities.values():e.update(boost=0,damage=0)
                if mode=='zero_power':self.entities['ally']['power']+=5
        self.receipts();steps=self.steps
        if mode in ('ready_snapshot','combat_snapshot'):
            point=self.ready if mode=='ready_snapshot' else cut;replay=steps[point:]
            steps=copy.deepcopy(steps[:point])+[self.step('capture')]+copy.deepcopy(replay)+[self.step('record_state'),self.step('ai_roundtrip'),self.step('restore')]+copy.deepcopy(replay)+[self.step('assert_recorded_state')]
        gaps=['Extra combat, replacement/prevention, multiplayer and simultaneous optional sources are separate observations. Optional full acceptance/decline are covered; partial aggregated draw selection is a separate observation.']+list(self.p.get('inactive_abilities',[]))
        if self.p.get('input_profile')=='optional_trigger':gaps.append('AI optional trigger decline remains a separate observation; human decline and both-controller acceptance are exercised.')
        case=dict(name=f'CombatValueTriggerRecipe_{self.csvid}_{self.requested}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=steps,
                  _recipe=dict(family=FAMILY,version=2,variant=self.requested,params=copy.deepcopy(self.d['params']),oracle_text=self.d['oracle_text'],untested_components=gaps))
        if self.owner:_mirror(case)
        return case


RENOWN_KEYWORDS=PASSIVE|{'First strike','Deathtouch','Lifelink'}


def _through_renown_parameters(record):
    try:return _through_team_parameters(record)
    except ValueError:pass
    if (record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line','') or any(not re.fullmatch(r'\d+',record.get(k,'')) for k in ('power','toughness'))
            or not 1<=int(record['power'])<=6 or not 1<=int(record['toughness'])<=7):
        raise ValueError('renown needs fixed normal creature characteristics in the reviewed range')
    renown=None;keywords=[];inactive=[]
    for line in record['oracle_text'].splitlines():
        match=re.fullmatch(r"Renown ([12]) \(When this creature deals combat damage to a player, if it isn't renowned, put (a \+1/\+1 counter|two \+1/\+1 counters) on it and it becomes renowned\.\)",line)
        if match:
            if renown is not None or match[2]!=('a +1/+1 counter' if match[1]=='1' else 'two +1/+1 counters'):
                raise ValueError('renown printed quantity and reminder must agree exactly')
            renown=int(match[1]);continue
        if line.startswith('{') and ':' in line:inactive.append(line);continue
        for reminder in re.findall(r'\([^\n]*?\)',line):
            if reminder not in REMINDERS:raise ValueError('unreviewed renown companion reminder')
            line=line.replace(reminder,'')
        for word in line.strip().split(','):
            word=word.strip()
            if word not in RENOWN_KEYWORDS:raise ValueError('additional renown clause needs another model')
            keywords.append(word)
    if renown is None or len(keywords)!=len(set(keywords)) or set(keywords)|{'Renown'}!=set(record.get('keywords',[])):
        raise ValueError('renown complete text and keyword metadata must agree')
    return dict(schema=11,scope='combat',recipient='player',renown=renown,keywords=sorted(keywords),inactive_abilities=inactive,
                effects=dict(draw=0,discard=0,mill=False,self_counters=0))


def _through_renown_variants(p):
    if p['schema']!=11:return _through_team_variants(p)
    modes=['unblocked','cast','payment','insufficient_mana','wrong_color','fresh_attack','blocked','keyword_block',
           'zero_power','pump','cleanup','two_hits','hexmage_remove','bounce_recast','counter_before_hit','other_attack',
           'two_sources','ready_snapshot','combat_snapshot','between_hits_snapshot','removed_snapshot']+list(BOUNDARY_MODES)
    return [v+'_'+str(owner) for owner in (0,1) for v in modes]


class _RenownBoard(_AdjacentBoard):
    """Renowned state is proved through real hits, removal and a new permanent."""
    def __init__(self,name,csvid,d,p,requested,rows,oracle):
        super().__init__(name,csvid,d,p,requested,rows,oracle)
        self.renowned={ref:False for ref in ('source','copy') if ref in self.entities}
        if self.mode in ('hexmage_remove','removed_snapshot'):self.add('Vampire Hexmage','remover',1)
        helper='Unsummon' if self.mode=='bounce_recast' else 'Hunger of the Howlpack' if self.mode=='counter_before_hit' else None
        if helper:self.setup.append(self.card(helper,'helper',0,'hand'));self.zones[0]['hand'].append(helper)
        if self.mode=='keyword_block' and 'Deathtouch' not in self.keys:self.add('Grizzly Bears','small_blocker',1)
        self.emit('reset_death_history');self.ready=len(self.steps)

    def hit(self,ref,amount,target=1,combat=True,primary=True):
        self.life[target]-=amount
        if primary and 'Lifelink' in self.keys:self.life[self.entities[ref]['player']]+=amount
        if primary and combat and amount>0 and not self.renowned[ref]:
            self.renowned[ref]=True;self.entities[ref]['counters']+=self.p['renown']

    def activate_grant(self):
        legal=self.mode!='noncombat_tapped' and (self.mode not in ('noncombat_sick','noncombat_fresh') or 'Haste' in self.keys)
        super().activate_grant()
        if legal and self.mode=='noncombat_creature' and 'Lifelink' in self.keys:self.life[0]+=1

    def creature_hit(self,attacker,target,amount,primary=False):
        if amount<=0:return
        e=self.entities[target];e['damage']+=amount
        if primary and 'Lifelink' in self.keys:self.life[self.entities[attacker]['player']]+=amount
        if (primary and 'Deathtouch' in self.keys) or e['damage']>=e['toughness']+e['counters']+e['boost']:
            self.move(target,'graveyard')

    def combat(self):
        mode=self.mode;self.emit('begin_attackers');attackers=['idle'] if mode=='other_attack' else ['source']+(['copy'] if mode=='two_sources' else [])
        if mode=='fresh_attack' and 'Haste' not in self.keys:self.emit('attack',ref='source',value=0);attackers=[]
        for ref in attackers:
            self.emit('attack',ref=ref,value=1);self.entities[ref]['tapped']=not(ref in self.renowned and 'Vigilance' in self.keys)
            self.emit('assert_card',ref=ref,property='tapped',value=int(self.entities[ref]['tapped']))
        self.emit('finish_attackers');cut=len(self.steps);self.emit('begin_blockers')
        blocked=mode in ('blocked','keyword_block');blocker='small_blocker' if 'small_blocker' in self.entities else 'wall'
        if blocked:self.emit('block',ref=blocker,other='source',value=1)
        self.emit('advance_combat')
        if blocked:
            amount=self.power('source');back=self.power(blocker)
            lethal=1 if 'Deathtouch' in self.keys else self.entities[blocker]['toughness']-self.entities[blocker]['damage']
            assigned=min(amount,lethal) if 'Trample' in self.keys else amount
            excess=amount-assigned
            if 'First strike' in self.keys:
                self.creature_hit('source',blocker,assigned,True)
                if excess:self.hit('source',excess)
                if self.entities[blocker]['zone']=='battlefield':self.creature_hit(blocker,'source',back)
            else:
                # Both amounts are fixed before the simultaneous damage/death results.
                self.creature_hit(blocker,'source',back);self.creature_hit('source',blocker,assigned,True)
                if excess:self.hit('source',excess)
        else:
            for ref in attackers:self.hit(ref,self.power(ref),primary=ref in self.renowned)
        self.emit('combat_damage');self.emit('resolve');self.emit('end_combat');return cut

    def remove_counters(self):
        self.emit('choose_target',player=1,ref='source');self.emit('activate',ref='remover',value=1);self.emit('assert_stack',value=1)
        self.move('remover','graveyard');self.emit('assert_card',ref='remover',property='in_play',value=0)
        self.emit('assert_card',ref='source',property='plus1_counters',value=self.entities['source']['counters'])
        self.emit('resolve');self.emit('assert_choices_consumed');self.entities['source']['counters']=0

    def bounce_recast(self):
        self.spell('helper','Unsummon','source');self.move('source','hand')
        self.entities['source'].update(counters=0,boost=0,damage=0,tapped=False);self.renowned['source']=False
        self.emit('bind_hand',player=0,value=self.csvid,ref='source');self.emit('assert_card',ref='source',property='in_hand',value=1)
        self.emit('assert_card',ref='source',property='plus1_counters',value=0);self.receipts()
        self.spell('source',self.name,entity=True);self.emit('assert_card',ref='source',property='plus1_counters',value=0)

    def run(self):
        mode=self.mode
        if self.casting:
            if mode in ('insufficient_mana','wrong_color'):
                self.emit('cast',ref='source',value=0);self.emit('assert_card',ref='source',property='in_hand',value=1)
            else:self.spell('source',self.name,entity=True)
        if self.boundary:
            self.spell('grant','Hermetic Study','source',True)
            if mode in ('noncombat_sick','noncombat_fresh') and 'Haste' not in self.keys:self.emit('assert_card',ref='source',property='sick',value=1)
            split=len(self.steps);self.activate_grant()
            if mode=='noncombat_twice':self.receipts();self.next_own_turn();self.activate_grant()
        elif not self.casting or mode=='fresh_attack':
            if mode in ('pump','zero_power','counter_before_hit'):
                helper='Giant Growth' if mode=='pump' else 'Hydrosurge' if mode=='zero_power' else 'Hunger of the Howlpack'
                self.spell('helper',helper,'source')
                if mode=='pump':self.entities['source']['boost']+=3
                elif mode=='zero_power':self.entities['source']['power']-=5
                else:self.entities['source']['counters']+=1
            cut=self.combat()
            if mode in ('two_hits','hexmage_remove','bounce_recast','between_hits_snapshot','removed_snapshot'):
                self.receipts()
                if mode in ('hexmage_remove','removed_snapshot'):self.remove_counters();self.receipts()
                if mode=='bounce_recast':self.bounce_recast()
                between=len(self.steps);self.next_own_turn();self.combat()
            if mode in ('pump','zero_power','cleanup'):
                self.receipts()
                for player in (0,1):
                    for color in range(6):self.emit('set_mana',player=player,color=color,value=0)
                self.mana=[0,0];self.emit('cleanup');self.emit('refresh')
                for e in self.entities.values():e.update(boost=0,damage=0)
                if mode=='zero_power':self.entities['source']['power']+=5
        self.receipts();steps=self.steps
        if mode in ('ready_snapshot','combat_snapshot','between_hits_snapshot','removed_snapshot','noncombat_snapshot'):
            point=self.ready if mode=='ready_snapshot' else cut if mode=='combat_snapshot' else split if mode=='noncombat_snapshot' else between
            replay=steps[point:];steps=copy.deepcopy(steps[:point])+[self.step('capture')]+copy.deepcopy(replay)+[self.step('record_state'),self.step('ai_roundtrip'),self.step('restore')]+copy.deepcopy(replay)+[self.step('assert_recorded_state')]
        gaps=list(self.p.get('inactive_abilities',[]))+['Extra combat, replacement/prevention, multiple blockers, multiplayer and triggered abilities beyond this whole-text renown grammar remain separate work.']
        case=dict(name=f'CombatValueTriggerRecipe_{self.csvid}_{self.requested}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=steps,
                  _recipe=dict(family=FAMILY,version=2,variant=self.requested,params=copy.deepcopy(self.d['params']),oracle_text=self.d['oracle_text'],untested_components=gaps))
        if self.owner:_mirror(case)
        return case


INGEST_LINE='Ingest (Whenever this creature deals combat damage to a player, that player exiles the top card of their library.)'


def _through_library_parameters(record):
    try:return _through_renown_parameters(record)
    except ValueError:pass
    from mill_recipes import WORDS
    if (record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line','') or any(not re.fullmatch(r'\d+',record.get(k,'')) for k in ('power','toughness'))
            or not 1<=int(record['power'])<=6 or not 1<=int(record['toughness'])<=7):
        raise ValueError('library saboteur needs reviewed fixed normal creature characteristics')
    movement=None;scope=None;recipient=None;keywords=[];inactive=[];entry=0;unblockable=False;condition=None
    aliases='(?:this creature|'+re.escape(record['name'])+')'
    amounts={v:k for k,v in WORDS.items() if k<=4}
    for line in record['oracle_text'].splitlines():
        metalcraft=line.startswith('Metalcraft — ')
        body=line.removeprefix('Metalcraft — ')
        match=re.fullmatch(r'Whenever '+aliases+r' deals (combat )?damage to (a player|an opponent), (if you control three or more artifacts, )?(that player|target player) mills (a card|two cards|three cards|four cards)\.',body)
        if line==INGEST_LINE:
            if movement is not None:raise ValueError('multiple library damage clauses require another model')
            scope='combat';recipient='player';movement=dict(zone='exile',amount=1,target='damaged');keywords.append('Ingest')
        elif match:
            if movement is not None or bool(match[3])!=metalcraft:raise ValueError('exact library trigger/condition pairing required')
            scope='combat' if match[1] else 'any';recipient='player' if match[2]=='a player' else 'opponent'
            movement=dict(zone='graveyard',amount=amounts[match[5]],target='choose' if match[4]=='target player' else 'damaged')
            if metalcraft:condition='own_artifacts3'
        elif line=='Devoid (This card has no color.)':
            if record.get('colors')!=[]:raise ValueError('Devoid must have pinned colorless characteristics')
            keywords.append('Devoid')
        elif line=="This creature can't be blocked.":unblockable=True
        elif line=='When this creature enters, if you control another colorless creature, each opponent loses 2 life.':
            if entry:raise ValueError('duplicate library source entry clause')
            entry=2
        elif line.startswith('{') and ':' in line:inactive.append(line)
        else:
            for reminder in re.findall(r'\([^\n]*?\)',line):
                if reminder not in REMINDERS:raise ValueError('unreviewed library source reminder')
                line=line.replace(reminder,'')
            for word in line.strip().split(','):
                word=word.strip()
                if word not in PASSIVE|{'Deathtouch'}:raise ValueError('unreviewed library source clause')
                keywords.append(word)
    expected=set(keywords)|({'Mill'} if movement and movement['zone']=='graveyard' else set())|({'Metalcraft'} if condition else set())
    if movement is None or len(keywords)!=len(set(keywords)) or expected!=set(record.get('keywords',[])):
        raise ValueError('library saboteur full text and keyword metadata disagree')
    return dict(schema=12,scope=scope,recipient=recipient,movement=movement,condition=condition,entry_life_loss=entry,
                unblockable=unblockable,keywords=sorted(keywords),inactive_abilities=inactive,
                effects=dict(draw=0,discard=0,mill=False,self_counters=0))


def _through_library_variants(p):
    if p['schema']!=12:return _through_renown_variants(p)
    modes=['unblocked','cast','payment','insufficient_mana','wrong_color','fresh_attack','blocked','zero_power','pump','cleanup',
           'other_attack','two_hits','last_card_two_hits','duplicate_hits','two_sources','ready_snapshot','combat_snapshot',
           'empty_library','exact_library']+list(BOUNDARY_MODES)
    if p['movement']['amount']>1:modes.append('short_library')
    if p['condition']:modes += ['artifacts_below','artifacts_at','artifacts_above','artifacts_opponent','artifacts_noncreature','artifacts_none']
    if p['entry_life_loss']:modes += ['entry_source_only','entry_own_colorless','entry_two_colorless','entry_opponent_colorless','entry_noncreature_colorless','entry_own_colored','entry_recast']
    result=[v+'_'+str(owner) for owner in (0,1) for v in modes]
    if p['movement']['target']=='choose':result.append('self_target_0')
    return result


class _LibraryBoard(_AdjacentBoard):
    """Reuse ordered-library and destination-multiset receipts with real damage."""
    def __init__(self,name,csvid,d,p,requested,rows,oracle):
        super().__init__(name,csvid,d,p,requested,rows,oracle)
        amount=p['movement']['amount'];self.target=0 if self.mode=='self_target' else 1
        names=None
        if self.mode=='empty_library':names=[]
        if self.mode=='exact_library':names=list(LIBRARY[:amount])
        if self.mode=='short_library':names=list(LIBRARY[:amount-1])
        if self.mode=='last_card_two_hits':names=['Forest']
        if self.mode=='duplicate_hits':names=['Forest','Forest','Plains']+list(LIBRARY)
        if names is not None:
            self.zones[self.target]['library']=names
            self.setup=[c for c in self.setup if not(c['player']==self.target and c['zone']=='library')]
            self.setup=[self.card(n,f'stock_library_{self.target}_{i}',self.target,'library') for i,n in enumerate(names)]+self.setup
        if p['condition']:
            count=2 if self.mode=='artifacts_below' else 4 if self.mode=='artifacts_above' else 0 if self.mode=='artifacts_none' else 3
            actor=1 if self.mode=='artifacts_opponent' else 0
            for i in range(count):
                artifact='Sol Ring' if self.mode=='artifacts_noncreature' or i==count-1 else 'Ornithopter'
                self.add(artifact,f'artifact_{i}',actor,state=['tapped'] if artifact=='Sol Ring' else None)
        if self.mode.startswith('entry_'):
            self.casting=True;self.entities['source']['zone']='hand';self.zones[0]['hand'].append(name)
            next(c for c in self.setup if c['ref']=='source')['zone']='hand'
            if self.mode=='entry_source_only':
                self.setup=[c for c in self.setup if c['ref']!='idle'];del self.entities['idle']
            if self.mode in ('entry_own_colorless','entry_two_colorless','entry_opponent_colorless','entry_recast'):
                for i in range(2 if self.mode=='entry_two_colorless' else 1):self.add('Ornithopter',f'companion_{i}',1 if self.mode=='entry_opponent_colorless' else 0)
            if self.mode=='entry_noncreature_colorless':self.add('Sol Ring','companion',state=['tapped'])
            if self.mode=='entry_own_colored':self.add('Grizzly Bears','companion')
            if self.mode=='entry_recast':self.setup.append(self.card('Unsummon','helper',0,'hand'));self.zones[0]['hand'].append('Unsummon')
        if self.mode=='two_sources' and p['entry_life_loss'] and self.entry_condition('copy'):
            # Battlefield fixture placement uses real ordered entry. The second
            # source enters with the first already present; its ETB resolves.
            self.life[1]-=p['entry_life_loss'];self.emit('assert_life',player=1,value=self.life[1])
        self.ready=len(self.steps)

    def artifact_count(self):
        return sum(e['zone']=='battlefield' and e['player']==0 and 'Artifact' in self.oracle[e['name']]['type_line'] for e in self.entities.values())

    def entry_condition(self,ref):
        return any(key!=ref and e['zone']=='battlefield' and e['player']==0 and e['creature'] and self.oracle[e['name']]['colors']==[] for key,e in self.entities.items())

    def spell(self,ref,name,*args,**kw):
        super().spell(ref,name,*args,**kw)
        if ref=='source' and self.p['entry_life_loss'] and self.entry_condition(ref):self.life[1]-=self.p['entry_life_loss']

    def hit(self,ref,amount,target=1,combat=True,primary=True):
        self.life[target]-=amount
        if not primary or amount<=0 or not(combat or self.p['scope']=='any') or target==0 and self.p['recipient']=='opponent':return
        if self.p['condition'] and self.artifact_count()<3:return
        who=self.target if self.p['movement']['target']=='choose' else target
        if self.p['movement']['target']=='choose':self.emit('choose_player',player=0,value=who)
        # Same bounded top-prefix transfer used by immutable fixed-mill recipes;
        # only its Oracle destination differs. Exile has no rules-defined order.
        count=self.p['movement']['amount'];library=self.zones[who]['library']
        self.zones[who][self.p['movement']['zone']].extend(library[:count]);del library[:count]

    def receipts(self):
        super().receipts()
        if 'Devoid' in self.keys and self.entities['source']['zone']=='battlefield':self.emit('assert_matching_permanents',player=0,index=self.csvid,property='color',value=0)

    def combat(self):
        self.emit('begin_attackers');attackers=['idle'] if self.mode=='other_attack' else ['source']+(['copy'] if self.mode=='two_sources' else [])
        if self.mode=='fresh_attack' and 'Haste' not in self.keys:self.emit('attack',ref='source',value=0);attackers=[]
        for ref in attackers:
            self.emit('attack',ref=ref,value=1);self.entities[ref]['tapped']=not(ref in ('source','copy') and 'Vigilance' in self.keys)
        self.emit('finish_attackers');cut=len(self.steps);self.emit('begin_blockers')
        blocked=self.mode=='blocked' and not self.p['unblockable']
        if self.mode=='blocked':self.emit('block',ref='wall',other='source',value=int(blocked))
        self.emit('advance_combat')
        if blocked:
            amount=self.power('source');self.entities['wall']['damage']+=amount
            if amount>0 and ('Deathtouch' in self.keys or self.entities['wall']['damage']>=self.entities['wall']['toughness']):self.move('wall','graveyard')
        else:
            for ref in attackers:self.hit(ref,self.power(ref),primary=ref in ('source','copy'))
        self.emit('combat_damage');self.emit('resolve');self.emit('end_combat');return cut

    def run(self):
        mode=self.mode
        if self.casting:
            if mode in ('insufficient_mana','wrong_color'):
                self.emit('cast',ref='source',value=0);self.emit('assert_card',ref='source',property='in_hand',value=1)
            else:self.spell('source',self.name,entity=True)
            if mode=='entry_recast':
                self.receipts();self.spell('helper','Unsummon','source');self.move('source','hand')
                self.emit('bind_hand',player=0,value=self.csvid,ref='source');self.emit('assert_card',ref='source',property='in_hand',value=1)
                self.spell('source',self.name,entity=True)
        if self.boundary:
            self.spell('grant','Hermetic Study','source',True)
            if mode in ('noncombat_sick','noncombat_fresh') and 'Haste' not in self.keys:self.emit('assert_card',ref='source',property='sick',value=1)
            split=len(self.steps);self.activate_grant()
            if mode=='noncombat_twice':self.receipts();self.next_own_turn();self.activate_grant()
        elif not self.casting or mode=='fresh_attack':
            if mode in ('pump','zero_power'):
                self.spell('helper','Giant Growth' if mode=='pump' else 'Hydrosurge','source')
                if mode=='pump':self.entities['source']['boost']+=3
                else:self.entities['source']['power']-=5
            cut=self.combat()
            if mode in ('two_hits','last_card_two_hits','duplicate_hits'):self.receipts();self.next_own_turn();self.combat()
            if mode in ('pump','zero_power','cleanup'):
                self.receipts()
                for player in (0,1):
                    for color in range(6):self.emit('set_mana',player=player,color=color,value=0)
                self.mana=[0,0];self.emit('cleanup');self.emit('refresh')
                for e in self.entities.values():e.update(boost=0,damage=0)
                if mode=='zero_power':self.entities['source']['power']+=5
        self.receipts();steps=self.steps
        if mode in ('ready_snapshot','combat_snapshot','noncombat_snapshot'):
            point=self.ready if mode=='ready_snapshot' else cut if mode=='combat_snapshot' else split;replay=steps[point:]
            steps=copy.deepcopy(steps[:point])+[self.step('capture')]+copy.deepcopy(replay)+[self.step('record_state'),self.step('ai_roundtrip'),self.step('restore')]+copy.deepcopy(replay)+[self.step('assert_recorded_state')]
        gaps=list(self.p.get('inactive_abilities',[]))+['Changing intervening-if conditions between trigger announcement and resolution, multiplayer, replacement/prevention and extra combat require separate observations.']
        if self.p['movement']['target']=='choose':gaps.append('AI controller self-target presentation remains a separate observation; both-controller opposite-player targets and human self-target are exercised.')
        case=dict(name=f'CombatValueTriggerRecipe_{self.csvid}_{self.requested}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=steps,
                  _recipe=dict(family=FAMILY,version=2,variant=self.requested,params=copy.deepcopy(self.d['params']),oracle_text=self.d['oracle_text'],untested_components=gaps))
        if self.owner:_mirror(case)
        return case


CONSTRAINED_KEYWORDS={'Flying','First strike','Fear','Intimidate','Shroud','Shadow','Horsemanship','Islandwalk','Swampwalk'}
CONSTRAINED_REMINDERS={"(This creature can't be blocked as long as defending player controls an Island.)",
    "(This creature can't be blocked as long as defending player controls a Swamp.)",
    '(This creature can block or be blocked by only creatures with shadow.)',
    "(This creature can't be blocked except by creatures with horsemanship.)",
    "(This creature can't be the target of spells or abilities.)"}


def parameters(record):
    try:return _through_library_parameters(record)
    except ValueError:pass
    from defensive_creature_recipes import reviewed_payment
    if (record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or 'Creature' not in record.get('type_line','')
            or any(not re.fullmatch(r'\d+',record.get(k,'')) for k in ('power','toughness'))
            or not 1<=int(record['power'])<=6 or not 1<=int(record['toughness'])<=7):raise ValueError('constrained saboteur requires reviewed fixed normal creature')
    pairs=[list(p) for p in re.findall(r'\{([WUBRG])/([WUBRG])\}',record.get('mana_cost',''))]
    reviewed_payment(record,dict(payment_colors=[a for a,b in pairs]) if pairs else {})
    text=record['oracle_text'];keys=[];inactive=[];primary=None;human=False;bonus=0;condition=None
    for reminder in re.findall(r'\([^\n]*?\)',text):
        if reminder not in REMINDERS|ADJACENT_REMINDERS|CONSTRAINED_REMINDERS:raise ValueError('unreviewed constrained reminder')
        text=text.replace(reminder,'')
    aliases='(?:this creature|'+re.escape(record['name'])+'|'+re.escape(record['name'].split(',')[0])+')'
    for line in map(str.strip,text.splitlines()):
        if line in CONSTRAINED_KEYWORDS:keys.append(line);continue
        if line=="This creature can't be blocked by Humans.":human=True;continue
        if line=='This creature gets +3/+3 as long as an opponent has no cards in hand.':bonus=3;continue
        if line.startswith('{') and ':' in line:inactive.append(line);continue
        q=re.fullmatch(r'Whenever '+aliases+r' deals (combat )?damage to (a player|an opponent), (.+)\.',line)
        if not q or primary is not None:raise ValueError('unreviewed constrained source clause or multiple triggers')
        e=dict(draw=0,discard=0,mill=False,self_counters=0,optional=False,both_discard=False,random_discard=False,exchange=None)
        body=q[3]
        if body in ('you may draw a card','you may draw that many cards'):e.update(draw='damage' if 'that many' in body else 1,optional=True)
        elif body in ('that player discards a card','that player discards a card at random'):e.update(discard=1,random_discard=body.endswith('at random'))
        elif body=='each player discards a card':e.update(discard=1,both_discard=True)
        elif body=='put a +1/+1 counter on it':e.update(self_counters=1)
        elif body=='if you control a blue or red permanent, you may discard a card. If you do, draw a card':e.update(draw=1,optional=True,exchange='rummage');condition='own_blue_or_red_permanent'
        else:raise ValueError('unreviewed constrained saboteur reward')
        primary=dict(scope='combat' if q[1] else 'any',recipient='player' if q[2]=='a player' else 'opponent',effects=e)
    expected=set(keys)|({'Landwalk'} if set(keys)&{'Islandwalk','Swampwalk'} else set())|({'Regenerate'} if any('Regenerate ' in t for t in inactive) else set())
    if primary is None or len(keys)!=len(set(keys)) or expected!=set(record.get('keywords',[])):raise ValueError('constrained complete text and keyword metadata disagree')
    walk='Island' if 'Islandwalk' in keys else 'Swamp' if 'Swampwalk' in keys else None
    return dict(schema=13,**primary,keywords=sorted(keys),inactive_abilities=inactive,hybrid_pairs=pairs,
                landwalk=walk,forbidden_humans=human,empty_opponent_hand_bonus=bonus,condition=condition)


def variants(p):
    if p['schema']!=13:return _through_library_variants(p)
    modes=['unblocked','cast','payment','insufficient_mana','wrong_color','fresh_attack','blocked','other_attack',
           'two_hits','cleanup','ready_snapshot','combat_snapshot','bare_zones']
    if not p['effects']['optional']:modes.append('two_sources')
    if 'Shroud' not in p['keywords']:modes+=['pump','zero_power']+list(BOUNDARY_MODES)
    else:modes.append('shroud_target')
    if p['effects']['discard']:modes+=['empty_hand','short_hand']
    if p['landwalk'] or p['forbidden_humans'] or set(p['keywords'])&{'Shadow','Horsemanship','Fear','Intimidate'}:modes.append('forbidden_block')
    if p['landwalk']:modes+=['land_own_only','land_opponent_wrong']
    if set(p['keywords'])&{'Fear','Intimidate'}:modes.append('color_legal_block')
    if 'First strike' in p['keywords']:modes.append('first_strike_block')
    if p['empty_opponent_hand_bonus']:modes+=['last_card_two_hits','refill_hand']
    if p['condition']:modes += ['empty_source_hand','rummage_order','qualifier_none','qualifier_own_blue','qualifier_own_red','qualifier_enemy_blue','qualifier_hand_blue','qualifier_mana_blue','qualifier_island','qualifier_noncreature_blue','qualifier_green']
    if p['hybrid_pairs']:modes+=['payment_other','payment_mixed']
    result=[v+'_'+str(owner) for owner in (0,1) for v in modes]
    if p['effects']['optional']:result.append('declined_0')
    return result


class _ConstrainedBoard(_AdjacentBoard):
    """Real blockers/targets/hand changes qualify already-calibrated rewards."""
    def __init__(self,name,csvid,d,p,requested,rows,oracle):
        from defensive_creature_recipes import reviewed_payment
        mode=requested.rsplit('_',1)[0];chosen=[b if mode=='payment_other' else pair[i%2] if mode=='payment_mixed' else a for i,pair in enumerate(p['hybrid_pairs']) for a,b in [pair]]
        mana,_=reviewed_payment(oracle[name],dict(payment_colors=chosen) if chosen else {})
        super().__init__(name,csvid,d,dict(p,mana=mana),requested,rows,oracle)
        base_cost=self.cost;self.cost=lambda record:mana if record.get('name')==name else base_cost(record)
        self.random_discards=0
        if self.mode in ('payment_other','payment_mixed'):
            self.casting=True;self.entities['source']['zone']='hand';self.zones[0]['hand'].append(name)
            next(c for c in self.setup if c['ref']=='source')['zone']='hand'
            self.mana[0]=sum(mana)
            for action in self.steps:
                if action['op']=='set_mana' and action['player']==0:action['value']=mana[action['color']]
        if self.e['random_discard'] and self.mode not in ('empty_hand','short_hand','bare_zones'):self.replace_hand(1,['Forest','Plains','Mountain'])
        if self.mode=='rummage_order':self.replace_hand(0,['Plains'])
        if self.mode in ('last_card_two_hits','refill_hand'):self.replace_hand(1,['Forest'])
        if self.mode=='refill_hand':
            self.setup.append(self.card('Vision Skeins','helper',0,'hand'));self.zones[0]['hand'].append('Vision Skeins')
            if self.owner==0:
                # The AI chooses its own legal discard without the host policy.
                # Equal refill identities preserve exact receipts for either choice.
                self.zones[1]['library'][1]='Forest'
                [c for c in self.setup if c['player']==1 and c['zone']=='library'][1]['name']='Forest'
        if self.mode.startswith('qualifier_'):
            self.setup=[c for c in self.setup if c['ref']!='idle'];del self.entities['idle']
            choice={'qualifier_own_blue':('Fugitive Wizard',0,'battlefield'),'qualifier_own_red':('Hill Giant',0,'battlefield'),
                    'qualifier_enemy_blue':('Fugitive Wizard',1,'battlefield'),'qualifier_hand_blue':('Fugitive Wizard',0,'hand'),
                    'qualifier_island':('Island',0,'battlefield'),'qualifier_noncreature_blue':('Propaganda',0,'battlefield'),
                    'qualifier_green':('Grizzly Bears',0,'battlefield')}.get(self.mode)
            if choice:self.add(choice[0],'qualifier',choice[1],choice[2])
        blockmode=self.mode in ('blocked','forbidden_block','land_own_only','land_opponent_wrong','first_strike_block','color_legal_block')
        self.blocker='wall'
        if blockmode:
            blocker='Abyssal Specter' if self.mode=='color_legal_block' and 'Fear' in self.keys else 'Slith Firewalker' if self.mode=='color_legal_block' else 'Dauthi Horror' if 'Shadow' in self.keys and self.mode=='blocked' else 'Wei Scout' if 'Horsemanship' in self.keys and self.mode=='blocked' else 'Fugitive Wizard' if p['forbidden_humans'] and self.mode=='forbidden_block' else 'Darksteel Gargoyle' if self.keys&{'Fear','Intimidate'} and self.mode=='blocked' else 'Grizzly Bears' if self.mode=='first_strike_block' or self.keys&{'Fear','Intimidate'} and self.mode=='forbidden_block' else None
            if blocker:self.add(blocker,'chosen_blocker',1);self.blocker='chosen_blocker'
            if p['landwalk'] and self.mode in ('forbidden_block','land_own_only','land_opponent_wrong'):
                land=('Swamp' if p['landwalk']=='Island' else 'Island') if self.mode=='land_opponent_wrong' else p['landwalk']
                self.add(land,'walk_land',0 if self.mode=='land_own_only' else 1)
        if self.mode=='shroud_target':
            self.add('Giant Growth','probe_own',0,'hand');self.add('Giant Growth','probe_enemy',1,'hand');self.add('Hermetic Study','probe_aura',0,'hand')
        self.sync_bonus();self.ready=len(self.steps)

    def replace_hand(self,player,names):
        self.zones[player]['hand']=list(names)
        self.setup=[c for c in self.setup if not(c['player']==player and c['zone']=='hand')]
        self.setup=[self.card(n,f'changed_hand_{player}_{i}',player,'hand') for i,n in enumerate(names)]+self.setup

    def sync_bonus(self):
        for ref in ('source','copy'):
            if ref not in self.entities:continue
            e=self.entities[ref];bonus=self.p['empty_opponent_hand_bonus'] if e['zone']=='battlefield' and not self.zones[1]['hand'] else 0
            delta=bonus-e.get('conditional_bonus',0);e['power']+=delta;e['toughness']+=delta;e['conditional_bonus']=bonus

    def qualifies(self):
        return not self.p['condition'] or any(e['player']==0 and e['zone']=='battlefield' and set(self.oracle[e['name']]['colors'])&{'U','R'} for e in self.entities.values())

    def eligible(self,amount,target,combat):
        return (amount>0 and (combat or self.p['scope']=='any') and (target==1 or self.p['recipient']=='player')
                and self.qualifies() and (not self.e['exchange'] or bool(self.zones[0]['hand'])))

    def trigger_inputs(self,ref,amount,target=1,combat=True):
        if self.e['optional'] and self.eligible(amount,target,combat) and self.mode!='declined' and self.owner==0:self.emit('choose_trigger',player=0,ref=ref)

    def hit(self,ref,amount,target=1,combat=True,primary=True):
        self.life[target]-=amount
        if not primary or not self.eligible(amount,target,combat) or self.e['optional'] and self.mode=='declined':return
        self.entities[ref]['counters']+=self.e['self_counters']
        if self.e['random_discard']:
            self.random_discards+=min(self.e['discard'],len(self.zones[target]['hand'])-self.random_discards)
        else:self.discard(target,self.e['discard'])
        if self.e['both_discard']:self.discard(1-target,self.e['discard'])
        if self.e['exchange']=='rummage':self.discard(0,1)
        self.draw(0,amount if self.e['draw']=='damage' else self.e['draw']);self.sync_bonus()

    def spell(self,ref,name,*args,**kw):
        if ref=='source' and self.p['hybrid_pairs'] and self.owner==0 and self.mode not in ('payment','payment_other','payment_mixed'):
            self.emit('choose_payment',player=0,value=sum(n<<(4*c) for c,n in enumerate(self.p['mana'])))
        super().spell(ref,name,*args,**kw)
        if name=='Vision Skeins':self.draw(0,2);self.draw(1,2);self.emit('refresh')
        self.sync_bonus()

    def receipts(self):
        start=len(self.steps);super().receipts()
        if not self.e['random_discard']:return
        generated=self.steps[start:];del self.steps[start:]
        for action in generated:
            if action.get('player')==1 and action.get('zone') in ('hand','graveyard'):
                if action['op']=='assert_zone':action=dict(action,value=action['value']+(-self.random_discards if action['zone']=='hand' else self.random_discards))
                elif action['op']=='assert_zone_multiplicity':continue
            self.steps.append(action)
        names=Counter(self.zones[1]['hand']+self.zones[1]['graveyard'])
        for n in sorted({c['name'] for c in self.setup}|set(LIBRARY)):
            self.emit('assert_hand_graveyard_total',player=1,index=self.rows[n]['csvid'],value=names[n])
            for zone in ('hand','graveyard'):self.emit('assert_zone_multiplicity_max',player=1,zone=zone,index=self.rows[n]['csvid'],value=names[n])
        self.emit('assert_discard_choices',value=0)

    def combat(self):
        mode=self.mode;self.emit('begin_attackers');attackers=['idle'] if mode=='other_attack' else ['source']+(['copy'] if mode=='two_sources' else [])
        if mode=='fresh_attack':self.emit('attack',ref='source',value=0);attackers=[]
        for ref in attackers:
            self.emit('attack',ref=ref,value=1);self.entities[ref]['tapped']=True
        self.emit('finish_attackers');cut=len(self.steps);self.emit('begin_blockers')
        blocked=mode in ('blocked','land_own_only','land_opponent_wrong','first_strike_block')
        if self.keys&{'Fear','Intimidate'} and mode in ('blocked','forbidden_block','color_legal_block'):
            from evasion_creature_recipes import may_block
            from recipes import COLORS
            r=self.oracle[self.entities[self.blocker]['name']]
            blocked=may_block(dict(keywords=self.p['keywords'],colors=self.oracle[self.name]['colors'],unblockable=False),
                              dict(keywords=r['keywords'],colors=r['colors'],artifact='Artifact' in r['type_line']))
            if blocked!=(mode!='forbidden_block'):raise ValueError('reviewed Fear/Intimidate blocker must match printed legality')
            self.emit('assert_matching_permanents',player=1,index=self.rows[r['name']]['csvid'],property='artifact',value=int('Artifact' in r['type_line']))
            self.emit('assert_matching_permanents',player=1,index=self.rows[r['name']]['csvid'],property='color',value=sum(1<<COLORS[c] for c in r['colors']))
        if blocked or mode=='forbidden_block':self.emit('block',ref=self.blocker,other='source',value=int(blocked))
        self.emit('advance_combat')
        if blocked:
            source=self.entities['source'];blocker=self.entities[self.blocker];amount=self.power('source');back=self.power(self.blocker)
            blocker['damage']+=amount
            dead=blocker['damage']>=blocker['toughness']
            if dead and 'Indestructible' not in self.oracle[blocker['name']].get('keywords',[]):self.move(self.blocker,'graveyard')
            if 'First strike' not in self.keys or blocker['zone']=='battlefield':
                source['damage']+=back
                if source['damage']>=source['toughness']+source['counters']+source['boost']:self.move('source','graveyard')
        else:
            hits=[(ref,self.power(ref)) for ref in attackers]
            for ref,amount in hits:
                if ref in ('source','copy'):self.trigger_inputs(ref,amount)
                self.hit(ref,amount,primary=ref in ('source','copy'))
        self.emit('combat_damage');self.emit('resolve');self.emit('end_combat');return cut

    def run(self):
        mode=self.mode
        if self.casting:
            if mode in ('insufficient_mana','wrong_color'):
                self.emit('cast',ref='source',value=0);self.emit('assert_card',ref='source',property='in_hand',value=1)
            else:self.spell('source',self.name,entity=True)
        if mode=='shroud_target':
            for ref in ('probe_own','probe_enemy','probe_aura'):self.emit('can_target_creature',ref=ref,other='source',value=0)
            self.emit('can_target_creature',ref='probe_own',other='idle',value=1)
        elif self.boundary:
            self.spell('grant','Hermetic Study','source',True)
            if mode in ('noncombat_sick','noncombat_fresh'):self.emit('assert_card',ref='source',property='sick',value=1)
            split=len(self.steps);self.activate_grant()
            if mode=='noncombat_twice':self.receipts();self.next_own_turn();self.activate_grant()
        elif not self.casting or mode=='fresh_attack':
            if mode in ('pump','zero_power'):
                self.spell('helper','Giant Growth' if mode=='pump' else 'Hydrosurge','source')
                if mode=='pump':self.entities['source']['boost']+=3
                else:self.entities['source']['power']-=5
            cut=self.combat()
            if mode in ('two_hits','last_card_two_hits','refill_hand'):
                self.receipts()
                if mode=='refill_hand':self.spell('helper','Vision Skeins');self.receipts()
                self.next_own_turn();self.combat()
            if mode in ('pump','zero_power','cleanup'):
                self.receipts()
                for player in (0,1):
                    for color in range(6):self.emit('set_mana',player=player,color=color,value=0)
                self.mana=[0,0];self.emit('cleanup');self.emit('refresh')
                for e in self.entities.values():e.update(boost=0,damage=0)
                if mode=='zero_power':self.entities['source']['power']+=5
        self.receipts();steps=self.steps
        if mode in ('ready_snapshot','combat_snapshot','noncombat_snapshot'):
            point=self.ready if mode=='ready_snapshot' else cut if mode=='combat_snapshot' else split;replay=steps[point:]
            steps=copy.deepcopy(steps[:point])+[self.step('capture')]+copy.deepcopy(replay)+[self.step('record_state'),self.step('ai_roundtrip'),self.step('restore')]+copy.deepcopy(replay)+[self.step('assert_recorded_state')]
        gaps=list(self.p.get('inactive_abilities',[]))+['AI optional-trigger decline, multiple optional same-card sources, extra combat, replacement/prevention and multiplayer remain separate observations.']
        if 'Shroud' in self.keys:gaps.append('Source-targeting pump/bounce/Aura grant positives are unavailable; real generic target rejection is exercised without invalid UI input.')
        case=dict(name=f'CombatValueTriggerRecipe_{self.csvid}_{self.requested}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=steps,
                  _recipe=dict(family=FAMILY,version=2,variant=self.requested,params=copy.deepcopy(self.d['params']),oracle_text=self.d['oracle_text'],untested_components=gaps))
        if self.owner:_mirror(case)
        return case
