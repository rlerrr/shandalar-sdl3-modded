"""Whole-Oracle fixed sacrifice spells, with controller choices and owned receipts."""
import json
import re
from pathlib import Path
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

PINS = json.loads((Path(__file__).parent/'data/edict_witnesses.json').read_text(encoding='utf8'))['cards']
NOUNS = {'creature':'a creature', 'permanent':'a permanent', 'artifact':'an artifact',
         'land':'a land', 'artifact_or_enchantment':'an artifact or enchantment',
         'artifact_or_creature':'an artifact or creature', 'creature_or_land':'a creature or land',
         'green_white_creature':'a green or white creature', 'flying_creature':'a creature',
         'multicolored_permanent':'a multicolored permanent', 'artifact_nonartifact_creature':'an artifact creature and a nonartifact creature'}
BASE = {'creature':('Grizzly Bears','Hill Giant'), 'permanent':('Grizzly Bears','Hill Giant'),
        'artifact':('Wall of Spears','Darksteel Myr'), 'land':('Forest','Mountain'),
        'artifact_or_enchantment':('Wall of Spears','Darksteel Myr'),
        'artifact_or_creature':('Grizzly Bears','Hill Giant'), 'creature_or_land':('Grizzly Bears','Hill Giant'),
        'green_white_creature':('Grizzly Bears','Savannah Lions'), 'flying_creature':('Wind Drake','Phantom Monster'),
        'multicolored_permanent':('Watchwolf','Goblin Deathraiders'),
        'artifact_nonartifact_creature':('Wall of Spears','Darksteel Myr')}


def eligible(r, scope):
    kinds=set(r['type_line'].partition(' — ')[0].split())
    if scope=='permanent':return bool(kinds & {'Artifact','Creature','Land','Enchantment','Planeswalker'})
    if scope=='green_white_creature':return 'Creature' in kinds and bool(set(r['colors']) & {'G','W'})
    if scope=='flying_creature':return 'Creature' in kinds and 'Flying' in r['keywords']
    if scope=='multicolored_permanent':return eligible(r,'permanent') and len(r['colors'])>1
    if scope=='artifact_nonartifact_creature':return 'Artifact' in kinds and 'Creature' in kinds
    return bool(kinds & {'creature':{'Creature'},'artifact':{'Artifact'},'land':{'Land'},
                         'artifact_or_enchantment':{'Artifact','Enchantment'},'artifact_or_creature':{'Artifact','Creature'},
                         'creature_or_land':{'Creature','Land'}}[scope])


def effect_text(p):
    if p['mode']=='cost':
        prefix='As an additional cost to cast this spell, sacrifice '+NOUNS[p['scope']]+'. '
        return prefix+{'draw2':'Draw two cards.', 'draw3':'Draw three cards.',
          'draw_power':"Draw cards equal to the sacrificed creature's power.",
          'draw_mv':'Draw cards equal to the mana value of the sacrificed permanent.',
          'draw_power_gain_toughness':"You draw cards equal to the sacrificed creature's power, then you gain life equal to its toughness.",
          'black4':'Add {B}{B}{B}{B}.', 'red3':'Add {R}{R}{R}.',
          'black_mv':"Add an amount of {B} equal to the sacrificed creature's mana value."}[p['receipt']]
    if p['mode']=='after_draw':return 'Draw two cards, then sacrifice a permanent.'
    start={'any':'Target player','opponent':'Target opponent','both':'Each player','opponents':'Each opponent'}[p['recipient']]
    noun='two creatures' if p['count']==2 else NOUNS[p['scope']]
    body=start+' sacrifices '+noun+' of their choice'
    if p['scope']=='flying_creature':body+=' with flying'
    if p['receipt']=='lose1':return body+' and loses 1 life.'
    if p['receipt']=='lose2_if':return body+'. If that player does, they lose 2 life.'
    if p['receipt']=='recipient_gain_toughness':return body+", then gains life equal to that creature's toughness."
    if p['receipt']=='gain_toughness':return body+". You gain life equal to that creature's toughness."
    if p['receipt']=='draw_power':return body+". You draw cards equal to that creature's power."
    return body+'.'


def spec(d,r):
    if isinstance(d.get('params'),dict) and 'profile' in d['params']:return extension_spec(d,r)
    from recipes import cost_from_oracle
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:
        raise ValueError('edict requires pinned complete normal Oracle text and explicit variants')
    p=d.get('params')
    ordinary_keys={'mode','scope','recipient','count','receipt'}
    if not isinstance(p,dict) or set(p) not in (ordinary_keys,ordinary_keys|{'schema','extra_cost_choices'}):raise ValueError('explicit sacrifice mode, predicate, recipient, quantity and receipt required')
    if 'target_choices' in d:
        choices=d['target_choices']
        ordinary=type(choices) is int and choices in (0,1)
        mapped=isinstance(choices,dict) and set(choices)=={'0','1'} and all(type(n) is int and n in (0,1) for n in choices.values())
        if not (ordinary or mapped) or p['recipient'] not in ('any','opponent'):raise ValueError('reviewed per-controller automatic or explicit target-player presentation required')
    if 'extra_cost_choices' in p:
        choices=p['extra_cost_choices']
        if type(p['schema']) is not int or p['schema']!=7 or not isinstance(choices,dict) or set(choices)!={'0','1'} or any(type(n) is not int or n not in (0,1) for n in choices.values()) or p['mode']!='cost' or p['count']!=1:raise ValueError('reviewed extra cost-choice presentation requires one ordinary sacrifice cost')
    if p['mode'] not in ('edict','cost','after_draw') or p['scope'] not in NOUNS or p['recipient'] not in ('self','any','opponent','both','opponents') or type(p['count']) is not int or p['count'] not in (1,2):raise ValueError('unreviewed sacrifice shape')
    if p['mode']=='edict':
        if p['recipient']=='self' or p['count']==2 and p['scope']!='creature' or p['receipt'] not in ('none','lose1','lose2_if','recipient_gain_toughness','gain_toughness','draw_power'):raise ValueError('unreviewed edict quantity or receipt')
        if p['receipt']!='none' and (p['recipient'] not in ('any','opponent') or p['scope'] not in ('creature','green_white_creature') or p['count']!=1):raise ValueError('secondary edict receipt requires single targeted player sacrifice')
    elif p['recipient']!='self' or p['count']!=1:raise ValueError('ordinary cost/resolution self sacrifice required')
    elif p['mode']=='after_draw':
        if p['scope']!='permanent' or p['receipt']!='draw2':raise ValueError('reviewed draw then permanent sacrifice required')
    elif p['receipt'] not in ('draw2','draw3','draw_power','draw_mv','draw_power_gain_toughness','black4','red3','black_mv') or p['scope'] not in ('creature','artifact_or_creature','creature_or_land'):raise ValueError('unreviewed additional sacrifice cost or receipt')
    kinds=r['type_line'].split();lines=r['oracle_text'].splitlines();gaps=[]
    if not {'Instant','Sorcery'} & set(kinds):raise ValueError('ordinary instant/sorcery sacrifice required')
    if lines and lines[-1].startswith('Flashback '):gaps.append(lines.pop())
    if set(r.get('keywords',[]))-{'Flashback'} or ' '.join(lines)!=effect_text(p):raise ValueError('parameters must match the complete pinned sacrifice spell')
    return dict(p,family='edict_spell',mana=cost_from_oracle(r),instant='Instant' in kinds,untested_rules=gaps)


def variants(p):
    if 'profile' in p:return extension_variants(p)
    vs=['mixed','alternate_choice','payment','insufficient_mana','bare_zones','empty_board','stolen','tokens','cleanup','countered','ready_snapshot','pending_snapshot','resolved_snapshot']
    if p['scope'] not in ('multicolored_permanent','green_white_creature'):vs.append('indestructible')
    if p['scope'] in ('creature','permanent','artifact_or_creature','creature_or_land','green_white_creature','flying_creature'):vs.append('hexproof')
    if any(p['mana'][1:]):vs.append('wrong_color')
    if p['count']==2:vs.append('short_board')
    if p['scope'] in ('artifact_or_enchantment','artifact_or_creature','creature_or_land'):vs.append('other_type')
    if p['recipient']=='any':vs+=['own_target','target_shrouded_pending']
    elif p['recipient']=='opponent':vs+=['target_shrouded_pending']
    if p['instant']:vs.append('opposing_turn')
    return [f'{v}_{pl}' for pl in (0,1) for v in vs
            if v!='alternate_choice' or p['recipient']=='both' or (pl==0 if p['recipient']=='self' else pl==1)]


def validate_witnesses(o,rows):
    for name,pin in PINS.items():
        if name not in rows or any(o.get(name,{}).get(k)!=v for k,v in pin.items()):raise ValueError('edict witness changed: '+name)
    for name,pin in EXTENSION_PINS['cards'].items():
        if name not in rows or any(o.get(name,{}).get(k)!=v for k,v in pin.items()):raise ValueError('edict extension witness changed: '+name)
    for name,pin in WAVE4_PINS['cards'].items():
        if name not in rows or any(o.get(name,{}).get(k)!=v for k,v in pin.items()):raise ValueError('edict wave 4 witness changed: '+name)
    for name,pin in WAVE5_PINS['cards'].items():
        if name not in rows or any(o.get(name,{}).get(k)!=v for k,v in pin.items()):raise ValueError('edict wave 5 witness changed: '+name)
    for name,pin in WAVE7_PINS['cards'].items():
        if name not in rows or any(o.get(name,{}).get(k)!=v for k,v in pin.items()):raise ValueError('edict wave 7 witness changed: '+name)


def expand(name,csvid,d,p,selected,rows,o):
    if 'profile' in p:return extension_expand(name,csvid,d,p,selected,rows,o)
    from recipes import card,step,observe,cost_from_oracle
    result=[]
    for variant in selected:
        v,owner=variant.rsplit('_',1);owner=int(owner)
        if variant not in variants(p):raise ValueError('unsupported sacrifice variant: '+variant)
        bare=v=='bare_zones';target=owner if v=='own_target' else 1-owner
        recipients=[owner] if p['recipient']=='self' else [0,1] if p['recipient']=='both' else [1-owner] if p['recipient']=='opponents' else [target]
        zones={z:[[] if bare else list(ns),[] if bare else list(ns)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE))}
        zones['library']=[list(LIBRARY)*4,list(LIBRARY)*4]
        setup=[];entities={};mana=[[20]*6,[20]*6];life=[20,20];actions=[]
        pre=[step('turn',player=owner,value=0)]
        for z,by_player in zones.items():
            for pl,ns in enumerate(by_player):setup.extend(card(n,f'p{pl}_{z}_{i}',pl,z) for i,n in enumerate(ns))
        def add(n,ref,pl,zone='battlefield',token=False,stolen=False):
            item=card(n,ref,pl,zone)
            if zone=='battlefield':item['state']=['tapped']+(['opponent_owned'] if stolen else [])+(['token'] if token else [])
            setup.append(item);entities[ref]=dict(name=n,player=pl,owner=1-pl if stolen else pl,alive=zone=='battlefield',token=token,uncertain=False)
            if zone=='hand':zones['hand'][pl].append(n)
        def leave(ref):
            e=entities[ref];e['alive']=False
            if not e['token']:zones['graveyard'][e['owner']].append(e['name'])
        add(name,'source',owner,'hand')
        # Eligible duplicate identities for AI retain exact zone receipts without
        # pretending to control its policy. Human choices name actual offered refs.
        for pl in (0,1):
            pair=BASE[p['scope']]
            # The fixture's token flag does not construct a copy effect or copy
            # mana-cost data. Use a printed zero-cost body for MV receipts so
            # both an ordinary token and a copied token have the same value.
            if v=='tokens' and (p['receipt']=='draw_mv' or p.get('schema')==7 and p['receipt']=='black_mv'):pair=('Memnite','Memnite')
            if v=='other_type':pair={'artifact_or_enchantment':('Seal of Cleansing','Seal of Fire'),'artifact_or_creature':('Sol Ring','Fountain of Youth'),'creature_or_land':('Forest','Mountain')}[p['scope']]
            if v=='indestructible':
                candidate='Darksteel Citadel' if p['scope']=='land' else 'Darksteel Gargoyle' if p['scope']=='flying_creature' else 'Darksteel Myr'
                if eligible(o[candidate],p['scope']):pair=(candidate,candidate)
            if v=='hexproof':
                candidate='Aven Fleetwing' if p['scope']=='flying_creature' else 'Gladecover Scout'
                pair=(candidate,candidate)
            count=0 if v=='empty_board' else 1 if v=='short_board' else max(3,p['count']+1)
            for i in range(count):
                n=pair[0] if pl==1 else pair[i%2]
                add(n,f'b{pl}_{i}',pl,token=v=='tokens',stolen=v=='stolen')
            if p['scope']=='artifact_nonartifact_creature' and count:
                for i in range(2):add('Grizzly Bears',f'c{pl}_{i}',pl,token=v=='tokens',stolen=v=='stolen')
            # Type controls stay outside the reviewed sacrifice predicate.
            for i,n in enumerate(('Island','Sol Ring','Seal of Cleansing','Scathe Zombies')):
                if not eligible(o[n],p['scope']) and not (p['scope']=='artifact_nonartifact_creature' and 'Creature' in o[n]['type_line']):add(n,f'control{pl}_{i}',pl)
        if v=='countered':add('Counterspell','counter',0,'hand')
        if v=='target_shrouded_pending':add('Gilded Light','shield',target,'hand')
        cost=list(p['mana'])
        if v=='payment':mana[owner]=list(cost)
        if v=='insufficient_mana':mana[owner]=[0]*6
        if v=='wrong_color':
            color=next(i for i in range(1,6) if cost[i]);mana[owner]=list(cost);mana[owner][1 if color!=1 else 2]+=mana[owner][color];mana[owner][color]=0
        for pl in (0,1):pre.extend(step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
        pre.extend([step('refresh'),step('turn',player=1-owner if v=='opposing_turn' else owner,value=0)])
        def pay(vector,pl):mana[pl]=[a-b for a,b in zip(mana[pl],vector)]
        def consume_spell(ref):
            e=entities[ref];zones['hand'][e['player']].remove(e['name']);zones['graveyard'][e['owner']].append(e['name'])
        def choose_sacrifices():
            picked=[]
            for pl in recipients:
                scopes=[p['scope']]+(['nonartifact_creature'] if p['scope']=='artifact_nonartifact_creature' else [])
                for scope in scopes:
                    candidates=[ref for ref,e in entities.items() if e['alive'] and e['player']==pl and (('Creature' in o[e['name']]['type_line'] and 'Artifact' not in o[e['name']]['type_line']) if scope=='nonartifact_creature' else eligible(o[e['name']],scope))]
                    if v=='alternate_choice':candidates=candidates[1:]+candidates[:1]
                    for ref in candidates[:p['count']]:
                        if pl==0:actions.append(step('choose_target',player=pl,ref=ref))
                        else:
                            for candidate in candidates:entities[candidate]['uncertain']=True
                        picked.append(ref)
            return picked
        def sacrifice(picked):
            for ref in picked:leave(ref)
        def draw(pl,n):
            for _ in range(n):zones['hand'][pl].append(zones['library'][pl].pop(0))
        def receipts(picked):
            power=sum(int(o[entities[ref]['name']].get('power',0)) for ref in picked)
            toughness=sum(int(o[entities[ref]['name']].get('toughness',0)) for ref in picked)
            mv=sum(int(o[entities[ref]['name']]['cmc']) for ref in picked)
            receipt=p['receipt']
            if receipt in ('draw2','draw3'):draw(owner,2 if receipt=='draw2' else 3)
            if receipt in ('draw_power','draw_power_gain_toughness'):draw(owner,power)
            if receipt=='draw_mv':draw(owner,mv)
            if receipt in ('gain_toughness','draw_power_gain_toughness'):life[owner]+=toughness
            if receipt=='recipient_gain_toughness':life[target]+=toughness
            if receipt=='lose1':life[target]-=1
            if receipt=='lose2_if' and picked:life[target]-=2
            if receipt=='black4':mana[owner][1]+=4
            if receipt=='black_mv':mana[owner][1]+=mv
            if receipt=='red3':mana[owner][4]+=3
        denied=v in ('insufficient_mana','wrong_color') or p['mode']=='cost' and v=='empty_board'
        cut=0
        if denied:
            if p['mode']=='cost' and v=='empty_board':actions.append(step('can_cast',ref='source',value=0))
            actions.append(step('cast',ref='source',value=0))
        else:
            actions.append(step('require_can_cast',ref='source',value=1))
            choices=d.get('target_choices',1);choices=choices[str(owner)] if isinstance(choices,dict) else choices
            if p['recipient'] in ('any','opponent') and choices:actions.append(step('choose_player',player=owner,value=target))
            picked=choose_sacrifices() if p['mode']=='cost' else []
            # Some broken handlers ask for an additional sacrifice after paying
            # the printed cost. Supply their real input while keeping the
            # Oracle model below at exactly one sacrificed creature.
            extra=p.get('extra_cost_choices',{}).get(str(owner),0)
            if extra:
                remaining=[ref for ref,e in entities.items() if e['alive'] and e['player']==owner and ref not in picked and eligible(o[e['name']],p['scope'])]
                if len(remaining)<extra:raise ValueError('extra cost presentation needs another real eligible permanent')
                actions.extend(step('choose_target',player=owner,ref=ref) for ref in remaining[:extra])
            explicit_counter=v=='countered' and p.get('schema')==7 and p['receipt']=='black_mv'
            if v=='countered' and not explicit_counter:actions.append(step('choose_priority',ref='counter'))
            actions.append(step('cast',ref='source',value=0 if v=='countered' and not explicit_counter else 1));pay(cost,owner);consume_spell('source')
            if p['mode']=='cost':sacrifice(picked)
            if v=='countered':pay(cost_from_oracle(o['Counterspell']),0);consume_spell('counter')
            else:
                actions.append(step('assert_stack',value=1))
                # Additional costs are already gone; resolution sacrifices remain.
                for pl in (0,1):
                    # The harness exposes a pending spell in its controller's
                    # hand slots until resolution removes the physical card.
                    pending=int(pl==owner)
                    actions.extend([step('assert_zone',player=pl,zone='hand',value=len(zones['hand'][pl])+pending),step('assert_zone',player=pl,zone='graveyard',value=len(zones['graveyard'][pl])-pending)])
                actions.extend(step('assert_mana',player=owner,color=i,value=n) for i,n in enumerate(mana[owner]))
            cut=len(actions)
            if explicit_counter:
                actions.extend([step('assert_stack',value=1),step('announce_spell',ref='source'),step('cast_response',ref='counter',value=1),step('assert_stack',value=2),step('resolve_response')])
            elif v=='target_shrouded_pending':
                actions.extend([step('cast',ref='shield',value=1),step('assert_stack',value=2)]);pay(cost_from_oracle(o['Gilded Light']),target);consume_spell('shield')
            if v!='countered' and v!='target_shrouded_pending' and p['mode']!='cost':picked=choose_sacrifices()
            if not explicit_counter:actions.append(step('resolve'))
            actions.append(step('assert_choices_consumed'))
            if v!='countered' and v!='target_shrouded_pending':
                if p['mode']!='cost':sacrifice(picked)
                receipts(picked)
            if v=='cleanup':actions.extend([step('cleanup'),step('refresh')])
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for ref,e in entities.items():
            if e['alive'] and not e['uncertain']:
                checks.extend([observe(ref,'in_play',1),observe(ref,'tapped',1)])
                if 'Creature' in o[e['name']]['type_line']:checks.extend(observe(ref,k,int(o[e['name']][k])) for k in ('power','toughness'))
        names=set(PINS)|{name}
        for pl in (0,1):
            checks.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))])
            checks.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
            for nm in sorted(names):checks.append(step('assert_zone_multiplicity',player=pl,zone='battlefield',index=rows[nm]['csvid'],value=sum(e['alive'] and e['player']==pl and e['name']==nm for e in entities.values())))
            for zone,by_player in zones.items():
                checks.append(step('assert_zone',player=pl,zone=zone,value=len(by_player[pl])))
                checks.extend(step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=by_player[pl].count(nm)) for nm in sorted(names))
                if zone=='library':checks.extend(step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(by_player[pl]))
            checks.extend(step('assert_hand_graveyard_total',player=pl,index=rows[nm]['csvid'],value=zones['hand'][pl].count(nm)+zones['graveyard'][pl].count(nm)) for nm in sorted(names))
        steps=pre+actions+checks
        if v in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
            at=0 if v=='ready_snapshot' else len(actions) if v=='resolved_snapshot' else cut
            steps=pre+actions[:at]+[step('capture')]+actions[at:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[at:]+checks+[step('assert_recorded_state')]
        result.append(dict(name=f'EdictRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family='edict_spell',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_rules=p['untested_rules'],untested_components=['alternative costs/abilities','replacement and death triggers','drawing beyond empty libraries','control changes during resolution','multiplayer','AI choice policy']+(['nonzero-mana-value token copies'] if p['receipt']=='draw_mv' else []))))
    return result


# New reviewed profiles deliberately have their own witness universe. Adding an
# extension never adds assertions to a previously authored legacy declaration.
EXTENSION_PINS=json.loads((Path(__file__).parent/'data/edict_extension_profiles.json').read_text(encoding='utf8'))
WAVE4_PINS=json.loads((Path(__file__).parent/'data/edict_wave4_profiles.json').read_text(encoding='utf8'))
WAVE5_PINS=json.loads((Path(__file__).parent/'data/edict_wave5_profiles.json').read_text(encoding='utf8'))
WAVE7_PINS=json.loads((Path(__file__).parent/'data/edict_wave7_profiles.json').read_text(encoding='utf8'))


def _same(a,b):
    if type(a) is not type(b):return False
    if isinstance(a,dict):return set(a)==set(b) and all(_same(a[k],b[k]) for k in a)
    if isinstance(a,list):return len(a)==len(b) and all(_same(x,y) for x,y in zip(a,b))
    return a==b


def extension_spec(d,r):
    from recipes import cost_from_oracle
    import copy
    wave4=d['params']['profile'] in WAVE4_PINS['profiles']
    wave5=d['params']['profile'] in WAVE5_PINS['profiles']
    wave7=d['params']['profile'] in WAVE7_PINS['profiles']
    pin=(WAVE7_PINS if wave7 else WAVE5_PINS if wave5 else WAVE4_PINS if wave4 else EXTENSION_PINS)['profiles'].get(d['params']['profile'])
    if not pin or d.get('family')!='edict_spell' or type(d.get('version')) is not int or d['version']!=2 or not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or any(r.get(k)!=v for k,v in pin['source'].items()) or d.get('oracle_text')!=r['oracle_text'] or not _same(d['params'],pin['params']):raise ValueError('edict extension requires explicit reviewed parameters and complete source pins')
    p=copy.deepcopy(pin['params']);variable='{X}' in r['mana_cost'];mana_text=r['mana_cost'].replace('{X}','')
    if p.get('hybrid'):mana_text=mana_text.replace('{'+p['hybrid']['symbol']+'}','{'+p['hybrid']['default']+'}')
    p.update(family='edict_spell',mana=cost_from_oracle(dict(mana_cost=mana_text)),instant='Instant' in r['type_line'],csvid=pin['csvid'],variable=variable)
    if wave4:p['_wave4']=True
    if wave5:p['_wave5']=True
    if wave7:p.update(_wave5=True,_wave7=True)
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or any(not isinstance(v,str) for v in vs) or len(vs)!=len(set(vs)) or set(vs)-set(extension_variants(p)):raise ValueError('explicit distinct reviewed edict extension variants required')
    return p


def extension_eligible(r,scope):
    types=set(r['type_line'].partition(' — ')[0].split());subtypes=set(r['type_line'].partition(' — ')[2].split())
    if scope=='none':return False
    if scope=='goblin':return 'Goblin' in subtypes or 'Changeling' in r['keywords']
    if scope=='highest_power_creature':return 'Creature' in types
    if scope=='creature_mv3':return 'Creature' in types and r['cmc']<=3
    if scope=='green_creature':return 'Creature' in types and 'G' in r['colors']
    if scope=='mountain':return 'Land' in types and 'Mountain' in subtypes
    if scope=='planeswalker':return 'Planeswalker' in types
    if scope=='creature_or_planeswalker':return bool(types&{'Creature','Planeswalker'})
    if scope=='artifact_land':return bool(types&{'Artifact','Land'})
    if scope=='creature_land':return bool(types&{'Creature','Land'})
    if scope=='artifact_creature_enchantment':return bool(types&{'Artifact','Creature','Enchantment'})
    return eligible(r,scope)


def wave5_resource_operations(p,owner,target,v,zones,entities,sacrificed,o,rows,life,discard_cards,step,observe):
    inputs=[];after=[]
    def players(role):return (0,1) if role=='both' else (owner,) if role=='self' else (target,) if role=='target' else (1-owner,)
    for operation in p['resolution_operations']:
        kind=operation['kind'];amount=operation.get('amount',1)
        for pl in players(operation['player']):
            if kind=='draw_if_sacrificed' and not any(entities[r]['player']==pl for r in sacrificed):continue
            if kind in ('draw','draw_if_sacrificed'):
                if len(zones['library'][pl])<amount:raise ValueError('bounded resource draw requires enough library cards')
                zones['hand'][pl].extend(zones['library'][pl][:amount]);del zones['library'][pl][:amount]
            elif kind=='discard':discard_cards(pl,amount)
            elif kind=='lose_life':life[pl]-=amount
            elif kind=='gain_life':life[pl]+=amount
            elif kind in ('grave_to_hand','grave_to_battlefield'):
                candidates=[n for n in zones['graveyard'][pl] if extension_eligible(o[n],operation['scope'])]
                if not candidates:continue
                preferred=p.get('other_return') if v=='other_return' else p.get('preferred_return');n=preferred if preferred in candidates else min(candidates,key=lambda nm:rows[nm]['csvid'])
                inputs.append(step('choose_zone_card',player=pl,value=rows[n]['csvid']));zones['graveyard'][pl].remove(n)
                if kind=='grave_to_hand':zones['hand'][pl].append(n)
                else:
                    if any(e['alive'] and e['player']==pl and e['name']==n for e in entities.values()):raise ValueError('resource return requires a distinguishable fresh cohort')
                    ref=f'resource_return{pl}';entities[ref]=dict(name=n,player=pl,owner=pl,alive=True,tapped=False,power=int(o[n].get('power',0)),toughness=int(o[n].get('toughness',0)),damage=0,uncertain=False)
                    after.append(step('bind_permanent_copy',player=pl,value=rows[n]['csvid'],index=0,ref=ref))
                    if 'Creature' in o[n]['type_line']:after.append(observe(ref,'sick',1))
            else:raise ValueError('unreviewed edict resource operation')
    return inputs,after


def wave5_keep_board(p,v,owner,add):
    groups={};kept=set();selectors={}
    for pl in (0,1):
        chooser=owner if p['keep_mode']=='nonland_types' else pl;selectors[pl]=chooser
        if v=='empty_battlefield' or v=='one_empty' and pl!=owner:continue
        pairs=[('artifact',['Sol Ring','Darksteel Relic']),('creature',['Grizzly Bears','Hill Giant']),('enchantment',['Seal of Cleansing','Seal of Fire'])]
        pairs += [('land',['Forest','Mountain'])] if p['keep_mode']=='types' else [('planeswalker',['Jace Beleren'])]
        groups[pl]=[];overlap=None
        for scope,ns in pairs:
            ns=list(ns)
            if chooser==1:ns=[ns[0]]*len(ns)
            if v=='indestructible_rest' and scope in ('artifact','creature'):ns=['Darksteel Relic' if scope=='artifact' else 'Predator Ooze']*2
            if v=='shroud_keep' and scope=='creature':ns=['Deadly Insect']*2
            if v=='overlap_keep' and chooser==0 and scope=='artifact':ns=['Wall of Spears','Darksteel Relic']
            refs=[]
            for i,n in enumerate(ns):
                ref=f'keep_{pl}_{scope}_{i}';add(n,ref,pl,stolen=v=='stolen_rest' and pl==owner and i==1);refs.append(ref)
            chosen=refs[-1] if v=='alternate_keep' else refs[0]
            if v=='overlap_keep' and chooser==0:
                if scope=='artifact':overlap=chosen
                elif scope=='creature':chosen=overlap
            kept.add(chosen);groups[pl].append(dict(scope=scope,refs=refs,chosen=chosen,chooser=chooser))
        if p['keep_mode']=='nonland_types':
            for i,n in enumerate(('Forest','Darksteel Citadel' if owner==0 else 'Forest')):
                ref=f'land_{pl}_{i}';add(n,ref,pl);kept.add(ref)
    return groups,kept


def wave5_variants(p):
    if p.get('multi'):
        common=['effect','payment','insufficient_mana','wrong_color','bare_zones','cleanup','countered','ready_snapshot','pending_snapshot','resolved_snapshot','alternate_choice','stolen_target','indestructible_target','target_left','second_target_left','both_targets_left','target_hexproof','second_target_hexproof','wrong_type']
        if p['multi'].get('controller') in ('same','opponent'):common+=['wrong_controller','short_targets']
        if p['multi'].get('scope')=='nonland':common+=['artifact_target','enchantment_target','planeswalker_target']
        if p.get('allow_own_target'):common+=['own_target']
        if p.get('optional_payment'):common+=['ordinary','empty_cost','stolen_cost','indestructible_cost']
        if p['cost_scope']=='creature':common+=['empty_cost','stolen_cost','indestructible_cost','shroud_cost','cost_is_target']
        if p.get('discard_card_cost'):common+=['empty_hand','one_card','last_choice','large_target','boundary_target']
        if p['multi'].get('sacrifice_per_target'):common.remove('wrong_type')
        if p['instant']:common+=['opposing_turn']
        result=[f'{v}_{pl}' for pl in (0,1) for v in common if not(pl==1 and (v=='alternate_choice' and not p['multi'].get('choice') or v=='own_target' and p['cost_scope']=='creature'))]
        if p['multi'].get('sacrifice_per_target'):result += [f'{v}_x{n}_{pl}' for pl in (0,1) for n in (0,1,2,3) for v in ('effect','payment')]
        return result
    common=['effect','payment','insufficient_mana','wrong_color','bare_zones','cleanup','countered','ready_snapshot','pending_snapshot','resolved_snapshot']
    if p['cost_scope']!='none':common+=['empty_cost','stolen_cost','indestructible_cost']
    if p['effect']=='mass_exchange':common+=['empty_battlefield','no_grave_creatures' if p['scope']=='creature' else 'no_grave_artifacts','duplicate_returns','stolen_creatures' if p['scope']=='creature' else 'stolen_permanents']
    if p['effect']=='mass_exchange' and p['scope']=='artifact':common+=['older_exile_artifact']
    if p.get('keep_mode'):common+=['empty_battlefield','one_empty','alternate_keep','stolen_rest','indestructible_rest','shroud_keep','overlap_keep']
    if p.get('combat'):
        common+=['own_target','empty_recipients','one_recipient','blocking' if p['combat']=='attacking_or_blocking' else 'blocking_negative','indestructible_recipients','hexproof_recipients','target_shrouded_pending']
        if p.get('metalcraft'):common+=['metalcraft_at','metalcraft_removed']
    if p['effect']=='control':common+=['stolen_target','large_target','target_left','target_hexproof','black_only','artifact_only','wrong_type','shroud_cost','destroy_controlled']
    if p['effect']=='reanimate':common+=['empty_graveyard','wrong_graveyard','other_target','target_removed','returned_activate','returned_attack']
    if p.get('delve'):common+=['empty_recipients','one_recipient','own_target','target_shrouded_pending','delve_one','delve_all','delve_payment','delve_countered','delve_snapshot']
    if p.get('resolution_operations'):common+=['empty_recipients','one_empty','empty_hand','one_card','large_hand','other_return','no_initial_grave_creatures']+(['target_shrouded_pending'] if p['target']=='opponent' else [])
    if p.get('discard_hand_cost'):common+=['own_target','empty_hand','one_card','large_hand','target_shrouded_pending']
    if p.get('resolution_sacrifice'):common+=['empty_recipients','discard_mode','mixed_mode','artifact_recipients','land_recipients','indestructible_recipients','shroud_recipients','last_choice']
    if p.get('chosen_number'):common+=['empty_recipients','one_recipient','wide_board','empty_hand']
    if p.get('same_name_search'):common+=['large_target','indestructible_target','stolen_target','target_left','target_hexproof','tokens_only','empty_library','wrong_library','decline_search','shroud_cost']
    if p['instant'] and not p.get('combat'):common+=['opposing_turn']
    result=[f'{v}_{pl}' for pl in (0,1) for v in common]
    if p.get('keep_mode')=='nonland_types':result.remove('overlap_keep_1')
    if p['effect']=='control':result+=['own_target_0']
    if p.get('same_name_search'):result+=['own_target_0']
    if p.get('resolution_sacrifice'):result+=['draw_before_decisions_0']
    if p.get('variable') or p.get('chosen_number'):result += [f'{v}_x{x}_{pl}' for pl in (0,1) for x in ((0,1,3,4) if p.get('chosen_number') else (0,1,3,5)) for v in ('effect','payment')]
    return result


def extension_variants(p):
    if p.get('_wave5'):
        result=wave5_variants(p)
        if p.get('_wave7'):
            result += [f'{v}_{pl}' for pl in (0,1) for v in ('large_toughness','stolen_recipients')]
        return result
    common=['effect','payment','insufficient_mana','wrong_color','bare_zones','cleanup','countered','ready_snapshot','pending_snapshot','resolved_snapshot']
    if p['cost_scope']!='none':common+=['empty_cost','stolen_cost']
    elif p['kind']=='edict':common+=['empty_recipients']
    if p['cost_count']==2:common+=['short_cost']
    if p['cost_scope'] in ('creature','artifact','artifact_or_creature','creature_or_land','land','permanent'):common+=['indestructible_cost']
    if p['target'] in ('any','player','opponent'):common+=['own_target'] if p['target']!='opponent' else []
    if p['target'] in ('any','creature','artifact','artifact_creature_enchantment') and p['effect'] in ('damage','destroy','exile'):
        common+=['large_target','indestructible_target','target_left']
        if p['target']=='any':common+=['creature_target']
        if p['target'] in ('artifact','artifact_creature_enchantment'):common+=['artifact_target']
        if p['target']=='artifact_creature_enchantment':common+=['enchantment_target']
    if p['target'] in ('player','opponent'):common+=['target_shrouded_pending']
    if p['effect']=='discard' or p.get('value_operations'):common+=['empty_hand','one_card','large_hand','last_choice']
    if p['instant']:common+=['opposing_turn']
    if p['quantity'] in ('half_floor','half_ceil','third_ceil','hand','deaths','x'):common+=['wide_board','one_recipient']
    if p.get('value_operations'):common+=['uneven_hands']
    if p.get('search'):
        common+=['empty_library']
        if p['search']['scope'] in ('card','land','basic_land'):common+=['search_land']
        if p['search']['scope']!='card':common+=['wrong_library','decline_search']
        if p['search']['scope']=='land':common+=['search_nonbasic']
        if p['search']['scope']=='basic_land':common+=['wrong_nonbasic']
        if p['search']['scope']=='green_creature':common+=['wrong_color_library']
        if p['search']['quantity']>1:common+=['one_search','short_library']
    if p['quantity']=='surplus_min':common+=['reverse_board','equal_board','one_empty','wide_board','one_recipient']
    if p['quantity']=='keep3':common+=['wide_board','one_recipient']
    if p['scope']=='permanent' and p.get('_wave4'):common+=['land_recipients','artifact_recipients','enchantment_recipients']
    if p.get('recipient_checks'):
        common+=['indestructible_recipients','stolen_recipients']
        if 'creature' in p['scope'] or p['scope']=='permanent':common+=['hexproof_recipients']
    condition=p.get('condition',{}).get('kind')
    if condition=='threshold':common+=['threshold_6','threshold_7']
    if condition=='dragon_as_cast':common+=['controlled_dragon','revealed_dragon','decline_reveal','sacrifice_dragon','empty_with_dragon']
    if condition=='delirium':common+=['below_delirium','planeswalker_choice','no_creatures','no_planeswalkers']
    if condition=='ascend':common+=['city_below','city_at','city_odd','city_even','city_empty']
    if p.get('one_sided'):common+=['one_empty']
    if p.get('flying_proof'):common+=['flying_token']
    if p.get('optional_payment'):common+=['ordinary']
    if p['effect']=='mana':common+=['red_mana','split_mana','zero_value_cost']
    if p.get('allow_own_target'):common+=['own_target','stolen_target','large_target','indestructible_target','target_left','no_creatures']
    if p.get('response_hexproof'):common+=['target_hexproof']
    if p.get('hybrid'):common+=['hybrid_other']
    if p['quantity']=='deaths':common+=['no_history','two_deaths']
    if p['scope']=='creature_mv3':common+=['wrong_value']
    result=[f'{v}_{pl}' for pl in (0,1) for v in common]+[f'alternate_choice_{pl}' for pl in (0,1) if p['cost_count']!='all' and (p['cost_scope']!='none' or p['kind']=='edict') and (pl==0 if p['cost_scope']!='none' else p['recipient']=='both' or pl==1)]
    if p.get('variable'):result += [f'{v}_x{x}_{pl}' for pl in (0,1) for x in (0,1,3,5) for v in ('effect','payment')]
    return result


def extension_expand(name,csvid,d,p,selected,rows,o):
    from recipes import card,step,observe,cost_from_oracle
    if name!=p['profile'] or csvid!=p['csvid'] or len(selected)!=len(set(selected)) or set(selected)-set(extension_variants(p)):raise ValueError('edict extension registry identity or variants changed')
    output=[]
    for variant in selected:
        v,owner=variant.rsplit('_',1);owner=int(owner);x=2
        if '_x' in v:v,raw_x=v.rsplit('_x',1);x=int(raw_x)
        if p.get('chosen_number') and v in ('bare_zones','empty_hand'):x=0
        if p.get('multi',{}).get('sacrifice_per_target') and v in ('empty_cost','cost_is_target'):x=0 if v=='empty_cost' else 1
        multi_count=x if p.get('multi',{}).get('count')=='chosen' else p.get('multi',{}).get('count',0)
        actual_cost_count=multi_count if p.get('multi',{}).get('sacrifice_per_target') else p['cost_count']
        target=owner if v=='own_target' else 1-owner;costscope=p['cost_scope'];cost=list(p['mana']);cost[0]+=x if p.get('variable') else 0
        if p.get('hybrid') and v=='hybrid_other':cost[{'B':1,'U':2,'G':3,'R':4,'W':5}[p['hybrid']['default']]]-=1;cost[{'B':1,'U':2,'G':3,'R':4,'W':5}[p['hybrid']['other']]]+=1
        if v=='sacrifice_dragon':target=owner
        hs=[] if v in ('bare_zones','empty_hand') else list(HAND[:1]) if v=='one_card' else list(HAND)*3 if v=='large_hand' else list(HAND)
        zones={z:[list(ns),list(ns)] for z,ns in (('hand',hs),('graveyard',[] if v=='bare_zones' else GRAVE),('exile',[] if v=='bare_zones' else EXILE),('library',list(LIBRARY)*4))}
        if p.get('initial_graveyard') and v!='bare_zones':zones['graveyard']=[list(p['initial_graveyard']),list(p['initial_graveyard'])]
        if p['effect']=='reanimate' and v=='bare_zones':zones['graveyard'][owner]=[p['preferred']]
        if p.get('discard_card_cost') and v=='bare_zones':zones['hand'][owner]=['Giant Growth']
        if v in ('empty_graveyard','no_grave_creatures','no_grave_artifacts'):zones['graveyard']=[[],[]] if v=='empty_graveyard' else [[n for n in ns if ('Creature' if v=='no_grave_creatures' else 'Artifact') not in o[n]['type_line']] for ns in zones['graveyard']]
        if v=='wrong_graveyard':zones['graveyard'][owner]=['Scathe Zombies','Giant Growth','Forest']
        if v=='duplicate_returns':zones['graveyard']=[ns+(['Sol Ring']*2 if p['scope']=='artifact' else ['Scathe Zombies']*2) for ns in zones['graveyard']]
        if v=='older_exile_artifact':zones['exile']=[['Memnite']+ns for ns in zones['exile']]
        if v=='no_initial_grave_creatures':zones['graveyard']=[[n for n in ns if 'Creature' not in o[n]['type_line']] for ns in zones['graveyard']]
        if v=='draw_before_decisions':zones['hand'][owner]=[];zones['library'][owner]=['Phyrexian Hulk','Giant Growth']+zones['library'][owner]
        if v=='uneven_hands':zones['hand']=[list(HAND[:1]),list(HAND)*2]
        if p.get('hand_counts') and v not in ('bare_zones','empty_hand','one_card','large_hand','uneven_hands'):
            zones['hand'][owner]=list(HAND[:p['hand_counts'][0]]);zones['hand'][1-owner]=list(HAND[:p['hand_counts'][1]])
        if v in ('threshold_6','threshold_7'):zones['graveyard'][owner]=list(GRAVE)+['Mountain']*(int(v[-1])-len(GRAVE))
        if v in ('below_delirium','planeswalker_choice'):zones['graveyard'][owner]=['Grizzly Bears','Giant Growth','Forest','Forest']
        if p.get('search'):
            zones['library'][owner]=[] if v=='empty_library' else ['Grizzly Bears','Memnite','Forest','Sol Ring'] if v=='wrong_library' and p.get('same_name_search') else ['Lightning Bolt','Seal of Cleansing'] if v=='wrong_library' else list(p.get('library_palette',WAVE4_PINS['library_palette']))
            if v=='wrong_nonbasic':zones['library'][owner]=['Darksteel Citadel','Sol Ring']
            if v=='wrong_color_library':zones['library'][owner]=['Wall of Spears','Memnite','Air Elemental','Forest']
            if v=='short_library':zones['library'][owner]=[p['search']['preferred'][0]]
        setup=[];entities={};actions=[];mana=[[40,20,20,20,20,20],[40,20,20,20,20,20]];life=[20,20];unknown=[0,0];possible=[set(),set()];tokens=[[],[]];deaths=[0]
        life=list(p.get('life_totals',life));shuffled=set()
        if p.get('normal_black_cap'):mana[owner][1]=p['normal_black_cap']
        for zone,players in zones.items():
            for pl,ns in enumerate(players):setup.extend(card(n,f'p{pl}_{zone}_{i}',pl,zone) for i,n in enumerate(ns))
        def add(n,ref,pl,zone='battlefield',stolen=False):
            item=card(n,ref,pl,zone)
            if zone=='battlefield':item['state']=['tapped']+(['opponent_owned'] if stolen else [])
            setup.append(item);entities[ref]=dict(name=n,player=pl,owner=1-pl if stolen else pl,alive=zone=='battlefield',tapped=True,power=int(o[n].get('power',0)),toughness=int(o[n].get('toughness',0)),damage=0,uncertain=False)
            if zone=='hand':zones['hand'][pl].append(n)
        def leave(ref,dest='graveyard'):
            e=entities[ref];e['alive']=False;zones[dest][e['owner']].append(e['name'])
            if dest=='graveyard' and 'Creature' in o[e['name']]['type_line']:deaths[0]+=1
        def spell_done(n,pl,dest='graveyard'):zones['hand'][pl].remove(n);zones[dest][pl].append(n)
        def pay(vector,pl):mana[pl]=[a-b for a,b in zip(mana[pl],vector)]
        def quantity(value,total):
            if value=='x':return x
            if value=='half_ceil':return (total+1)//2
            if value=='third_ceil':return (total+2)//3
            return value
        def discard_cards(pl,amount):
            count=min(amount,len(zones['hand'][pl])-unknown[pl])
            if pl==1:unknown[pl]+=count;possible[pl].update(zones['hand'][pl])
            else:
                for _ in range(count):
                    n=(max if v=='last_choice' else min)(zones['hand'][pl],key=lambda nm:rows[nm]['csvid']);zones['hand'][pl].remove(n);zones['graveyard'][pl].append(n)
        add(name,'source',owner,'hand')
        if p.get('initial_battlefield') and v!='empty_battlefield':
            for pl in (0,1):
                for i,n in enumerate(p['initial_battlefield']):add(n,f'initial{pl}_{i}',pl,stolen=v in ('stolen_creatures','stolen_permanents') and pl==owner and extension_eligible(o[n],p['scope']))
        keep_groups,kept=wave5_keep_board(p,v,owner,add) if p.get('keep_mode') else ({},set())
        if p.get('resolution_sacrifice'):
            n={'artifact_recipients':'Sol Ring','land_recipients':'Forest','indestructible_recipients':'Darksteel Relic','shroud_recipients':'Deadly Insect'}.get(v,'Grizzly Bears')
            for i in range(0 if v=='empty_recipients' else 3):add(n,f'resolution_cost{i}',owner)
            add('Grizzly Bears','unrelated_opponent',1-owner)
        combat_refs=[];combat_active=target
        if p.get('combat'):
            blocking=v in ('blocking','blocking_negative');combat_active=1-target if blocking else target
            count=0 if v=='empty_recipients' else 1 if v=='one_recipient' else 3
            for i in range(count):
                n='Darksteel Myr' if v=='indestructible_recipients' else 'Gladecover Scout' if v=='hexproof_recipients' else 'Grizzly Bears' if target==1 else ('Grizzly Bears','Hill Giant','Grizzly Bears')[i]
                if p.get('_wave7') and v=='large_toughness':n='Phyrexian Hulk'
                ref=f'participant{i}';add(n,ref,target,stolen=p.get('_wave7') and v=='stolen_recipients');entities[ref]['tapped']=False;setup[-1]['state']=[s for s in setup[-1]['state'] if s!='tapped']+['untapped'];entities[ref]['combat']='blocking' if blocking else 'attacking';combat_refs.append(ref)
            for ref,pl in (('ready_nonparticipant',target),('opposing_nonparticipant',1-target)):
                add('Grizzly Bears',ref,pl);entities[ref]['tapped']=False;setup[-1]['state']=['untapped']
            if blocking:add('Grizzly Bears','combat_attacker',1-target);entities['combat_attacker']['tapped']=False;setup[-1]['state']=['untapped']
            if p.get('metalcraft'):
                for i,n in enumerate(('Sol Ring','Darksteel Relic','Darksteel Citadel')[:3 if v in ('metalcraft_at','metalcraft_removed') else 2]):add(n,f'metalcraft{i}',owner)
        if costscope!='none':
            pair=BASE.get(costscope,('Mountain','Mountain') if costscope=='mountain' else ('Mogg Raider','Mogg Raider') if costscope=='goblin' else ('Grizzly Bears','Centaur Courser') if costscope=='green_creature' else ('Grizzly Bears','Hill Giant'))
            if p.get('cost_witnesses'):pair=tuple(p['cost_witnesses'])
            if p.get('target_predicate') and v in ('black_only','artifact_only','wrong_type'):pair=('Scathe Zombies','Scathe Zombies')
            if v=='shroud_cost':pair=('Deadly Insect','Deadly Insect')
            if v=='indestructible_cost':pair=('Darksteel Citadel','Darksteel Citadel') if costscope=='land' else ('Darksteel Myr','Darksteel Myr')
            if v=='zero_value_cost':pair=('Memnite','Memnite')
            count=0 if v=='empty_cost' else 1 if v=='short_cost' else 3
            if v=='cost_is_target' or p.get('discard_card_cost') and v=='wrong_type':count=1
            if p.get('optional_payment',{}).get('kind')=='alternative_sacrifice' and v in ('insufficient_mana','wrong_color'):count=1
            for i in range(count):add(pair[0] if owner==1 else pair[i%2],f'cost{i}',owner,stolen=v=='stolen_cost')
            if p.get('cost_palette') and v!='empty_cost':
                for i,n in enumerate(p['cost_palette']):add(n,f'palette_cost{i}',owner,stolen=v=='stolen_cost')
            for i,n in enumerate(('Island','Seal of Cleansing','Gladecover Scout')):
                if not extension_eligible(o[n],costscope) and not(p.get('multi') and v=='wrong_type'):add(n,f'own_control{i}',owner)
            if costscope=='green_creature':add('Hill Giant','wrong_color_cost',owner);add('Exploration','wrong_type_cost',owner)
        elif p['kind']=='edict' and not p.get('combat'):
            grave_types=set().union(*(set(o[n]['type_line'].partition(' — ')[0].split())&{'Artifact','Creature','Enchantment','Instant','Land','Planeswalker','Sorcery','Battle','Kindred'} for n in zones['graveyard'][owner]))
            delirium=p.get('condition',{}).get('kind')=='delirium' and len(grave_types)>=p['condition']['count']
            for pl in (0,1):
                if v in ('empty_recipients','empty_with_dragon') or v=='city_empty' and pl==target:continue
                if p.get('resolution_operations') and v=='one_empty' and pl!=owner:continue
                if p.get('one_sided') and v=='one_empty' and pl!=owner:continue
                scopes={'artifact_land':('artifact','land'),'creature_land':('creature','land'),'creature_or_planeswalker':('creature','planeswalker')}.get(p['scope'],(p['scope'],))
                for si,scope in enumerate(scopes):
                    if scope=='creature' and v=='no_creatures' or scope=='planeswalker' and v=='no_planeswalkers':continue
                    # The AI keeps its own choice policy. In the union branch
                    # give it one eligible identity type, while the human sees
                    # both and explicitly chooses the creature or planeswalker.
                    if pl==1 and p.get('condition',{}).get('kind')=='delirium' and not delirium:
                        if scope==('creature' if v=='planeswalker_choice' else 'planeswalker'):continue
                    pair=('Jace Beleren','Jace Beleren') if scope=='planeswalker' else ('War Mammoth','War Mammoth') if scope=='highest_power_creature' else ('Grizzly Bears','Wall of Spears') if scope=='creature_mv3' else BASE.get(scope,('Grizzly Bears','Hill Giant'))
                    if v=='wrong_value':pair=('War Mammoth','War Mammoth')
                    count=1 if v=='one_recipient' else (8 if v=='wide_board' else 6) if p['quantity']=='hand' else p.get('edict_board_count',3)+2 if v=='wide_board' and p.get('edict_board_count') else 4 if v=='wide_board' else p.get('edict_board_count',3)
                    if p.get('resolution_operations') and v=='no_initial_grave_creatures':count=1
                    if v=='bare_zones' and any(op['kind']=='grave_to_battlefield' for op in p.get('resolution_operations',[])):count=1
                    if p.get('edict_counts'):
                        counts=list(p['edict_counts']);counts=counts[::-1] if v=='reverse_board' else counts;count=counts[int(pl!=owner)]
                        if v=='wide_board':count+=2
                        if v=='equal_board':count=3
                        if v=='one_recipient':count=1
                        if v=='one_empty' and pl!=owner:count=0
                    if v=='land_recipients':pair=('Forest','Forest')
                    if v=='artifact_recipients':pair=('Wall of Spears','Wall of Spears')
                    if v=='enchantment_recipients':pair=('Seal of Cleansing','Seal of Cleansing')
                    if v=='indestructible_recipients' and scope!='planeswalker':pair=('Darksteel Citadel','Darksteel Citadel') if scope=='land' else ('Darksteel Myr','Darksteel Myr')
                    if v=='hexproof_recipients' and scope!='planeswalker':pair=('Gladecover Scout','Gladecover Scout') if scope!='land' else pair
                    if v=='sacrifice_dragon' and pl==owner:pair=('Shivan Dragon','Shivan Dragon');count=1
                    if v in ('city_odd','city_even') and pl==target:count=5 if v=='city_odd' else 4
                    if scope=='planeswalker':count=1
                    for i in range(count):
                        n=('Grizzly Bears','Wall of Spears','Forest','Seal of Cleansing','Darksteel Relic')[i%5] if p.get('mixed_permanents') and v not in ('land_recipients','artifact_recipients','enchantment_recipients','indestructible_recipients','hexproof_recipients') else pair[0] if pl==1 else pair[i%2]
                        add(n,f'edict{pl}_{si*10+i}',pl,stolen=v=='stolen_recipients')
                if p['scope']=='highest_power_creature':add('Grizzly Bears',f'low{pl}',pl)
                if p['scope']=='creature_mv3':add('Craw Wurm',f'high_value{pl}',pl)
                if p['scope']=='creature_mv3' and v!='wrong_value':add('War Mammoth',f'above_boundary{pl}',pl)
        if p.get('unrelated_controls'):
            for pl in (0,1):add('Grizzly Bears',f'unrelated_creature{pl}',pl);add('Seal of Cleansing',f'unrelated_enchantment{pl}',pl)
        if v in ('controlled_dragon','empty_with_dragon'):add('Shivan Dragon','dragon',owner)
        if v in ('revealed_dragon','decline_reveal'):add('Shivan Dragon','dragon_hand',owner,'hand')
        if v.startswith('city_'):
            needed=9 if v=='city_below' else 10
            for i in range(max(0,needed-sum(e['alive'] and e['player']==owner for e in entities.values()))):add('Forest',f'city_control{i}',owner)
        if p.get('keep_land_controls'):
            for pl in (0,1):add('Forest',f'land_control{pl}',pl)
        creature_target=p['target'] in ('creature','artifact','artifact_creature_enchantment') or p['target']=='any' and v in ('creature_target','large_target','indestructible_target','target_left')
        if creature_target:
            victim='Wall of Spears' if p['target']=='artifact' else 'War Mammoth'
            if v=='large_target':victim='Phyrexian Hulk' if p['target']=='artifact' else 'Wall of Ice' if isinstance(p['amount'],int) and p['amount']>=4 and p['effect']=='damage' else 'Craw Wurm'
            if v=='indestructible_target':victim='Darksteel Myr'
            if v=='artifact_target':victim='Sol Ring'
            if v=='enchantment_target':victim='Seal of Cleansing'
            if p.get('allow_own_target') and v=='no_creatures':victim='Sol Ring'
            if v in ('black_only','artifact_only','wrong_type'):victim={'black_only':'Scathe Zombies','artifact_only':'Wall of Spears','wrong_type':'Seal of Cleansing'}[v]
            if p.get('multi') and v=='wrong_type':victim='Forest' if p['multi']['scope']=='nonland' else 'Sol Ring'
            if v=='planeswalker_target':victim='Jace Beleren'
            if p['effect']=='multi_reduce' and v in ('large_target','cleanup'):victim='Autochthon Wurm'
            if v=='boundary_target':victim='Krosan Cloudscraper'
            add(victim,'victim',target if p.get('allow_own_target') else 1-owner,stolen=v=='stolen_target')
        if p.get('multi'):
            if v!='short_targets':
                for i in range(1,3 if p['multi'].get('sacrifice_per_target') else 2):
                    other='Darksteel Myr' if v=='indestructible_target' else 'Mountain' if v=='wrong_type' and p['multi']['scope']=='nonland' else 'Seal of Cleansing' if v=='wrong_type' else ('Hill Giant' if i==1 else 'Craw Wurm')
                    add(other,f'victim{i}',owner if v=='wrong_controller' else entities['victim']['player'])
            for pl in (0,1):
                if costscope!='land' or pl!=owner:add('Forest',f'multi_land_control{pl}',pl)
                add('Forest' if v=='wrong_type' and p['multi']['scope']=='nonland' else 'Sol Ring',f'multi_other_control{pl}',pl)
        if p.get('target_controls'):
            for pl in (0,1):add('Sol Ring',f'untargeted_artifact{pl}',pl);add('Seal of Cleansing' if v=='no_creatures' else 'Gladecover Scout',f'untargeted_control{pl}',pl)
        # Opposing controls are unrelated to own additional-cost eligibility.
        if costscope!='none' and not(p.get('multi') and v=='wrong_type'):
            add('Scathe Zombies' if p.get('target_predicate') else 'Grizzly Bears','opposing_control',1-owner);add('Wall of Spears' if p.get('target_predicate') else 'Gladecover Scout','protected_control',1-owner)
            if p.get('optional_payment') and costscope=='land':add('Forest','opposing_land_control',1-owner)
        if p['effect'] in ('mass_damage','mass_stats'):
            if p.get('creature_controls'):
                for pl in (0,1):
                    for i,n in enumerate(('Grizzly Bears','Hill Giant','Wall of Ice','Darksteel Myr')):add(n,f'creature_control{pl}_{i}',pl)
            else:add('Wall of Ice','large_control',1-owner);add('Darksteel Myr','indestructible_control',1-owner)
        if p['effect']=='mass_destroy':
            for pl in (0,1):
                for i,n in enumerate(('Wall of Spears','Sol Ring','Darksteel Relic','Darksteel Myr','Darksteel Citadel')):add(n,f'artifact_control{pl}_{i}',pl)
        counter_player=1-owner if p.get('counter_opponent') else 0
        if v in ('countered','delve_countered'):add('Counterspell','counter',counter_player,'hand')
        response_victim='victim1' if v.startswith('second_target_') else 'victim'
        if v in ('target_left','second_target_left'):add('Unsummon','response',1-owner,'hand')
        if v=='both_targets_left':add('Evacuation','response',1-owner,'hand')
        if v=='target_shrouded_pending':add('Gilded Light','response',target,'hand')
        if v in ('target_hexproof','second_target_hexproof'):add("Ranger's Guile",'response',entities[response_victim]['player'],'hand')
        if v=='target_removed':add('Reclaim','response',owner,'hand')
        if v=='flying_token':add('Suntail Hawk','flying_attacker',1-owner,'hand')
        if v=='tokens_only':
            for item in setup:
                if item['zone']=='battlefield' and 'Creature' in o[item['name']]['type_line']:item.setdefault('state',[]).append('token')
        optional=p.get('optional_payment');optional_active=False;optional_offered=False;cost_enabled=True
        if optional:
            available=sum(e['alive'] and e['player']==owner and extension_eligible(o[e['name']],costscope) for e in entities.values())>=p['cost_count']
            optional_active=v!='ordinary' and (optional['kind']=='buyback_mana' or available)
            cost_enabled=optional['kind']=='buyback_mana' or optional_active
            if optional['kind']=='buyback_mana' and optional_active:cost[0]+=optional['mana']
            if optional['kind']=='alternative_sacrifice' and optional_active:cost=[0]*6
            optional_offered=available if optional['kind']!='buyback_mana' else True
        if p.get('hybrid'):
            mana[owner]=[40]+list(cost[1:])
            if v=='countered' and owner==0:mana[owner][2]+=2
        if v=='payment':mana[owner]=list(cost)
        delve_cards=[]
        if p.get('delve') and v.startswith('delve_'):
            delve_cards=list(zones['graveyard'][owner]) if v=='delve_all' else ['Forest']
            cost[0]-=len(delve_cards)
            if v=='delve_payment':mana[owner]=list(cost)
        if v=='insufficient_mana':mana[owner]=[0]*6
        if v=='wrong_color':
            mana[owner]=list(cost);c=next(i for i in range(1,6) if mana[owner][i]);mana[owner][1 if c!=1 else 2]+=mana[owner][c];mana[owner][c]=0
        if optional and optional['kind']=='buyback_mana':optional_offered=sum(mana[owner])-sum(p['mana'])>=optional['mana']
        pre=[step('turn',player=1-owner if v=='opposing_turn' else owner,value=0),step('discard_policy',value=2 if v=='last_choice' else 1)]
        if p.get('life_totals'):pre.extend(step('set_life',player=pl,value=life[pl]) for pl in (0,1))
        if p.get('search'):pre.append(step('observe_search_filters',value=1))
        if optional:pre.append(step('present_ai_dialogs',value=1))
        if p.get('delve'):pre.append(step('present_ai_dialogs',value=1))
        if p.get('resolution_sacrifice'):pre.append(step('present_ai_dialogs',value=1))
        for pl in (0,1):pre.extend(step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
        pre.append(step('refresh'))
        if p.get('combat'):
            pre[0]=step('turn',player=combat_active,value=0);pre.append(step('begin_attackers'))
            if v in ('blocking','blocking_negative'):
                pre.extend([step('attack',ref='combat_attacker',value=1),step('finish_attackers'),step('begin_blockers')]);entities['combat_attacker']['tapped']=True
                pre.extend(step('block',ref=ref,other='combat_attacker',value=1) for ref in combat_refs);pre.append(step('advance_combat'))
                pre.extend(observe(ref,'blocking_count',1) for ref in combat_refs)
            else:
                for ref in combat_refs:pre.append(step('attack',ref=ref,value=1));entities[ref]['tapped']=True
                pre.append(step('finish_attackers'));pre.extend(observe(ref,'attacking',1) for ref in combat_refs)
        if p['quantity']=='deaths' or p['secondary_loss']=='deaths':pre.insert(0,step('reset_death_history'))
        history_count=0
        if p['quantity']=='deaths' and v!='no_history':
            history_count=2 if v=='two_deaths' else 1
            for i in range(history_count):add('Grizzly Bears',f'history{i}',owner);pre.append(step('destroy',ref=f'history{i}'));leave(f'history{i}')
        def selection(pl,scope,quantity):
            candidates=[ref for ref,e in entities.items() if e['alive'] and e['player']==pl and extension_eligible(o[e['name']],scope)]
            if p.get('combat'):candidates=[ref for ref in candidates if entities[ref].get('combat') in (('attacking','blocking') if p['combat']=='attacking_or_blocking' else ('attacking',))]
            if scope=='highest_power_creature' and candidates:
                greatest=max(entities[ref]['power'] for ref in candidates);candidates=[ref for ref in candidates if entities[ref]['power']==greatest]
            if v=='alternate_choice':candidates=candidates[1:]+candidates[:1]
            if quantity=='half_floor':quantity=len(candidates)//2
            elif quantity=='half_ceil':quantity=(len(candidates)+1)//2
            elif quantity=='third_ceil':quantity=(len(candidates)+2)//3
            elif quantity=='hand':quantity=len(zones['hand'][owner])
            elif quantity=='deaths':quantity=history_count
            elif quantity=='x':quantity=x
            elif quantity=='surplus_min':
                other=sum(e['alive'] and e['player']==1-pl and extension_eligible(o[e['name']],scope) for e in entities.values());quantity=max(0,len(candidates)-other)
            elif quantity=='keep3':
                kept=candidates[:3];chosen=candidates[3:]
                actions.extend(step('choose_target',player=pl,ref=ref) for ref in kept);return chosen
            elif quantity=='city_half':quantity=(len(candidates)+1)//2 if sum(e['alive'] and e['player']==owner for e in entities.values())>=p['condition']['count'] else 1
            if v=='planeswalker_choice':candidates.sort(key=lambda ref:'Planeswalker' not in o[entities[ref]['name']]['type_line'])
            chosen=candidates if quantity=='all' else candidates[:quantity]
            if quantity!='all' and (pl==0 or p.get('scripted_sacrifice',{}).get(str(pl),False)):actions.extend(step('choose_target',player=pl,ref=ref) for ref in chosen)
            elif quantity!='all':
                for ref in candidates:entities[ref]['uncertain']=True
            return chosen
        def multi_refs():
            if v=='cost_is_target':return ['cost0']+([] if p['multi'].get('sacrifice_per_target') else ['victim1'])
            return [f'victim{i}' if i else 'victim' for i in range(multi_count if not p.get('optional_payment') or optional_active else 1)]
        def target_inputs():
            if p['target']=='none':return []
            if p.get('multi'):
                inputs=[step('choose_target',player=owner,ref=ref) for ref in multi_refs()]
                if p['multi'].get('sacrifice_per_target'):
                    capacity=sum(e['alive'] and e['player']==owner and extension_eligible(o[e['name']],'creature') for e in entities.values())
                    inputs=[step('choose_dialog',player=owner,value=0 if multi_count else 1)]+inputs
                    if multi_count and multi_count<capacity:inputs.append(step('choose_cancel_target',player=owner))
                return inputs
            if p['target']=='graveyard':return [step('choose_zone_card',player=owner,value=rows[grave_target]['csvid'])]
            if creature_target:return [step('choose_target',player=owner,ref='victim')]
            return [step('choose_player',player=owner,value=target) for _ in range(p['target_choices'][str(owner)])]
        grave_target=p.get('other_target') if v in ('other_target','returned_attack') else p.get('preferred')
        cost_refs=[];pending=0
        optional_sac=optional and optional['kind']!='buyback_mana'
        denied=v in ('insufficient_mana','wrong_color') or not optional_sac and (v=='short_cost' or v=='empty_cost' and p['cost_count']!='all') or p.get('allow_own_target') and v=='no_creatures'
        if p.get('_wave5') and v in ('empty_graveyard','wrong_graveyard','black_only','artifact_only','wrong_type'):denied=True
        if v=='tokens_only' or p.get('multi') and v in ('wrong_controller','short_targets'):denied=True
        if p.get('discard_card_cost') and v=='empty_hand':denied=True
        if p.get('multi',{}).get('sacrifice_per_target') and v=='empty_cost':denied=False
        if denied:
            if v in ('empty_cost','short_cost','no_creatures','empty_graveyard','wrong_graveyard','black_only','artifact_only','wrong_type','tokens_only','wrong_controller','short_targets') or p.get('discard_card_cost') and v=='empty_hand':actions.append(step('require_can_cast',ref='source',value=0))
            if not(p.get('multi') and v in ('wrong_controller','short_targets','wrong_type') or p.get('discard_card_cost') and v=='empty_hand'):actions.append(step('cast',ref='source',value=0))
        else:
            actions.append(step('require_can_cast',ref='source',value=1))
            if p.get('chosen_number'):actions.append(step('choose_number',player=owner,value=x))
            if p.get('delve') and zones['graveyard'][owner]:
                # The UI offers its mode menu only when the undiscounted
                # printed cost is affordable, even when Delve itself is legal.
                printed=p['mana'];pool=mana[owner]
                if all(pool[i]>=printed[i] for i in range(1,6)) and sum(pool)-sum(printed[1:])>=printed[0]:actions.append(step('choose_dialog',player=owner,value=0 if delve_cards else 1))
                actions.extend(step('choose_zone_card',player=owner,value=rows[n]['csvid']) for n in delve_cards)
                if delve_cards and len(delve_cards)<min(p['mana'][0],len(zones['graveyard'][owner])):actions.append(step('choose_optional_zone',player=owner,value=rows['Scathe Zombies']['csvid']))
            if p.get('variable'):actions.append(step('choose_x',player=owner,value=x))
            if v in ('revealed_dragon','decline_reveal'):
                actions.append(step('choose_zone_card' if v=='revealed_dragon' else 'choose_optional_zone',player=owner,value=rows['Shivan Dragon']['csvid']))
                if v=='revealed_dragon':actions.extend(step('choose_dialog',player=owner,value=i) for i in p['reveal_dialogs'][str(owner)])
            if not p['cost_first']:actions.extend(target_inputs())
            if optional and optional_offered and optional['dialog_first']:actions.append(step('choose_dialog',player=owner,value=0 if optional_active else 1))
            if costscope!='none' and cost_enabled and actual_cost_count!=0:cost_refs=selection(owner,costscope,actual_cost_count)
            if optional and optional_offered and not optional['dialog_first']:actions.append(step('choose_dialog',player=owner,value=0 if optional_active else 1))
            if p['cost_first']:actions.extend(target_inputs())
            explicit_counter=v in ('countered','delve_countered') and p.get('counter_response')
            if v=='countered' and not explicit_counter:actions.append(step('choose_priority',ref='counter'))
            actions.append(step('cast',ref='source',value=0 if v=='countered' and not explicit_counter else 1));pay(cost,owner)
            for ref in cost_refs:leave(ref)
            if p.get('discard_card_cost'):
                n=(max if v=='last_choice' else min)((n for n in zones['hand'][owner] if n!=name),key=lambda nm:rows[nm]['csvid']);zones['hand'][owner].remove(n);zones['graveyard'][owner].append(n)
            if p.get('discard_hand_cost'):
                discarded=[n for n in zones['hand'][owner] if n!=name];zones['graveyard'][owner].extend(discarded);zones['hand'][owner]=[name]
            if p.get('random_discard_cost'):unknown[owner]+=x;possible[owner].update(n for n in zones['hand'][owner] if n!=name)
            for n in delve_cards:zones['graveyard'][owner].remove(n);zones['exile'][owner].append(n)
            if v=='countered' and not explicit_counter:pay(cost_from_oracle(o['Counterspell']),counter_player);spell_done('Counterspell',counter_player)
            else:
                actions.append(step('assert_stack',value=1))
                for pl in (0,1):actions.extend([step('assert_zone',player=pl,zone='hand',value=len(zones['hand'][pl])-unknown[pl]),step('assert_zone',player=pl,zone='graveyard',value=len(zones['graveyard'][pl])+unknown[pl])])
                actions.extend(step('assert_mana',player=owner,color=i,value=n) for i,n in enumerate(mana[owner]))
            pending=len(actions)
            succeeded=v not in ('countered','delve_countered','target_left','target_shrouded_pending','target_hexproof','target_removed')
            multi_targets=multi_refs() if p.get('multi') else []
            invalid_targets={response_victim} if v in ('target_left','second_target_left','target_hexproof','second_target_hexproof') else set(multi_targets) if v=='both_targets_left' else set()
            if p.get('multi'):succeeded=v!='countered'
            resolution_graveyard=list(zones['graveyard'][owner])
            dragon_as_cast=any(e['alive'] and e['player']==owner and 'Dragon' in o[e['name']]['type_line'].partition(' — ')[2].split() for e in entities.values()) or v=='revealed_dragon'
            destination='hand' if optional_active and optional['kind'] in ('buyback_mana','buyback_sacrifice') else p['source_destination']
            spell_done(name,owner,destination if succeeded else 'graveyard')
            if explicit_counter:
                actions.extend([step('cast_response',ref='counter',value=1),step('assert_stack',value=2),step('resolve_response')]);pay(cost_from_oracle(o['Counterspell']),counter_player);spell_done('Counterspell',counter_player)
            elif v in ('target_left','second_target_left'):
                actions.extend([step('choose_target',player=1-owner,ref=response_victim),step('cast_response',ref='response',value=1),step('assert_stack',value=2),step('resolve_response')]);pay(cost_from_oracle(o['Unsummon']),1-owner);spell_done('Unsummon',1-owner);leave(response_victim,'hand')
            elif v=='both_targets_left':
                actions.extend([step('cast_response',ref='response',value=1),step('assert_stack',value=2),step('resolve_response')]);pay(cost_from_oracle(o['Evacuation']),1-owner);spell_done('Evacuation',1-owner)
                for ref,e in entities.items():
                    if e['alive'] and 'Creature' in o[e['name']]['type_line']:leave(ref,'hand')
            elif v=='target_shrouded_pending':
                if p.get('parent_response_context'):actions.append(step('announce_spell',ref='source'))
                actions.extend([step('cast_response' if p.get('parent_response_context') else 'cast',ref='response',value=1),step('assert_stack',value=2),step('resolve_response' if p.get('parent_response_context') else 'resolve')]);pay(cost_from_oracle(o['Gilded Light']),target);spell_done('Gilded Light',target)
            elif v in ('target_hexproof','second_target_hexproof'):
                pl=entities[response_victim]['player'];actions.extend([step('choose_target',player=pl,ref=response_victim),step('cast_response',ref='response',value=1),step('assert_stack',value=2),step('resolve_response')]);pay(cost_from_oracle(o["Ranger's Guile"]),pl);spell_done("Ranger's Guile",pl);entities[response_victim]['power']+=1;entities[response_victim]['toughness']+=1
            elif v=='target_removed':
                actions.extend([step('choose_zone_card',player=owner,value=rows[grave_target]['csvid']),step('cast_response',ref='response',value=1),step('assert_stack',value=2),step('resolve_response')]);pay(cost_from_oracle(o['Reclaim']),owner);spell_done('Reclaim',owner);zones['graveyard'][owner].remove(grave_target);zones['library'][owner].insert(0,grave_target)
            else:
                sacrificed=[]
                if succeeded and v=='metalcraft_removed':actions.append(step('destroy',ref='metalcraft0'));leave('metalcraft0')
                if succeeded and p.get('keep_mode'):
                    for pl in (owner,1-owner):
                        for group in keep_groups.get(pl,[]):
                            input_group=group
                            if owner==1 and p.get('ai_keep_inputs')=='opponent_twice_when_both' and len(keep_groups)==2:
                                # Bind the legacy AI selector's actual offered
                                # controller. Oracle's mandatory own keep set
                                # remains independent above: homogeneous own
                                # groups guarantee exactly one survivor/type.
                                input_group=next(g for g in keep_groups[1-owner] if g['scope']==group['scope'])
                            actions.append(step('choose_target',player=group['chooser'],ref=input_group['chosen']))
                            if group['chooser']==1:
                                for ref in group['refs']:entities[ref]['uncertain']=len(group['refs'])>1
                if succeeded and p['kind']=='edict':
                    recipients=[0,1] if p['recipient']=='both' else [1-owner] if p['recipient']=='opponents' else [target]
                    if p.get('scripted_order_apnap') and p['recipient']=='both':
                        active=1-owner if v=='opposing_turn' else owner;recipients=[active,1-active]
                    scopes=p.get('sacrifice_order',{'artifact_land':('artifact','land'),'creature_land':('creature','land')}.get(p['scope'],(p['scope'],)))
                    sacrifice_quantity=p['quantity'];condition=p.get('condition',{})
                    if p.get('metalcraft'):sacrifice_quantity=2 if sum(e['alive'] and e['player']==owner and 'Artifact' in o[e['name']]['type_line'] for e in entities.values())>=3 else 1
                    if condition.get('kind')=='threshold' and len(resolution_graveyard)>=condition['count']:recipients=[0,1];sacrifice_quantity='all'
                    if condition.get('kind')=='delirium':
                        types=set().union(*(set(o[n]['type_line'].partition(' — ')[0].split())&{'Artifact','Creature','Enchantment','Instant','Land','Planeswalker','Sorcery','Battle','Kindred'} for n in resolution_graveyard))
                        if len(types)>=condition['count']:scopes=('creature','planeswalker')
                    for pl in recipients:
                        for scope in scopes:sacrificed+=selection(pl,scope,sacrifice_quantity)
                    if v=='wrong_value':actions.append(step('choose_target',player=target,ref=f'edict{target}_0'))
                if succeeded and p['effect']=='mana':
                    generated=sum(int(o[entities[ref]['name']]['cmc']) for ref in cost_refs)
                    colors=[4]*generated if v=='red_mana' else [1]+[4]*(generated-1) if v=='split_mana' and generated else [1]*generated
                    actions.extend(step('choose_color',player=owner,value=c) for c in colors)
                found=None;inspected=[];fetched=[]
                if succeeded and p.get('search'):
                    search=p['search'];inspected=list(zones['library'][owner])
                    def search_eligible(n):
                        r=o[n];scope=search['scope'];types=set(r['type_line'].partition(' — ')[0].split())
                        valid=scope=='card' or scope=='land' and 'Land' in types or scope=='basic_land' and {'Basic','Land'}<=types or scope=='artifact' and 'Artifact' in types or scope in ('creature','green_creature') and 'Creature' in types and (scope=='creature' or 'G' in r['colors'])
                        valid=valid or scope=='same_name' and n==entities['victim']['name']
                        limit=search.get('limit');bound=x if limit=='x' else int(o[entities[cost_refs[0]]['name']]['cmc'])+2 if limit=='cost_mv_plus2' else None
                        return bool(valid and (bound is None or r['cmc']<=bound))
                    legal=[n for n in inspected if search_eligible(n)]
                    preferred='Darksteel Citadel' if v=='search_nonbasic' else 'Forest' if v=='search_land' else search['preferred'][0]
                    if legal and v!='decline_search':
                        desired=[preferred]+search['preferred'];count=1 if v=='one_search' else search['quantity']
                        for _ in range(min(count,len(legal))):
                            found=next((n for n in desired if n in legal),legal[0]);fetched.append(found);legal.remove(found);actions.append(step('choose_zone_card',player=owner,value=rows[found]['csvid']))
                        if len(fetched)<search['quantity']:actions.append(step('choose_optional_zone',player=owner,value=rows[(legal or search['preferred'])[0]]['csvid']))
                    else:actions.append(step('choose_optional_zone',player=owner,value=rows[(legal or search['preferred'])[0]]['csvid']))
                resource_after=[]
                if succeeded and p.get('multi',{}).get('choice'):
                    chooser=entities['victim']['player'];chosen='victim1' if v=='alternate_choice' else 'victim'
                    if p['multi']['choice']=='target':actions.append(step('choose_target',player=chooser,ref=chosen))
                    else:actions.extend(step('choose_dialog',player=chooser,value=n) for n in ([1,0] if v=='alternate_choice' else [0]))
                if succeeded and p.get('resolution_operations'):
                    for ref in sacrificed:leave(ref)
                    resource_inputs,resource_after=wave5_resource_operations(p,owner,target,v,zones,entities,sacrificed,o,rows,life,discard_cards,step,observe);actions.extend(resource_inputs)
                if succeeded and p.get('resolution_sacrifice'):
                    zones['hand'][owner].extend(zones['library'][owner][:x]);del zones['library'][owner][:x]
                    for i in range(x):
                        available=any(e['alive'] and e['player']==owner for e in entities.values())
                        do_sac=available and v not in ('discard_mode','last_choice') and (v!='mixed_mode' or i==0) and (v!='draw_before_decisions' or i>0)
                        if available:actions.append(step('choose_dialog',player=owner,value=int(do_sac)))
                        if do_sac:
                            for ref in selection(owner,'permanent',1):leave(ref)
                        else:
                            discard_cards(owner,1)
                            # The AI's public discard notice is otherwise
                            # auto-acknowledged only when no gameplay dialog is
                            # queued next. Keep it separate from the next mode.
                            if owner==1:actions.append(step('choose_dialog',player=1,value=0))
                actions.append(step('resolve'));actions.extend(resource_after)
                if succeeded and p.get('search'):
                    actions.append(step('assert_forced_zone_choices',value=0));shuffled.add(owner)
                    if found:
                        last_inspected=list(inspected)
                        for n in fetched[:-1]:last_inspected.remove(n)
                        if len(fetched)==search['quantity']:
                            actions.append(step('assert_offered_count',value=len(last_inspected)))
                            actions.extend(step('assert_offered_legal_card',index=rows[n]['csvid'],value=int(search_eligible(n))) for n in sorted(set(last_inspected)))
                        for i,n in enumerate(fetched):
                            zones['library'][owner].remove(n)
                            if p['search']['destination']=='hand':zones['hand'][owner].append(n)
                            else:
                                ref='found' if i==0 else f'found{i}'
                                entities[ref]=dict(name=n,player=owner,owner=owner,alive=True,tapped=False,power=int(o[n].get('power',0)),toughness=int(o[n].get('toughness',0)),damage=0,uncertain=False)
                                index=p['own_return_index'] if p.get('same_name_search') and v=='own_target' else sum(e['alive'] and e['name']==n and e['player']==owner for r,e in entities.items() if r!=ref)
                                actions.append(step('bind_permanent_copy',player=owner,value=rows[n]['csvid'],index=index,ref=ref))
                                if 'Creature' in o[n]['type_line']:actions.append(observe(ref,'sick',1))
                if succeeded and v=='wrong_value':actions.append(step('assert_unused_choices',value=1))
                if succeeded:
                    for operation in p.get('value_operations',[]):
                        minimum=min(len(zones['hand'][pl])-unknown[pl] for pl in (0,1))
                        for pl in (0,1):
                            if operation['kind']=='lose_life':life[pl]-=quantity(operation['amount'],life[pl])
                            elif operation['kind']=='discard':discard_cards(pl,max(0,len(zones['hand'][pl])-unknown[pl]-minimum) if operation['amount']=='surplus_min' else quantity(operation['amount'],len(zones['hand'][pl])-unknown[pl]))
                    if not p.get('resolution_operations'):
                        for ref in sacrificed:leave(ref)
                    power=sum(entities[ref]['power'] for ref in cost_refs);amount=x if p['amount']=='x' else power if p['amount']=='cost_power' else 3 if p['amount']=='kicked' and optional_active else 2 if p['amount']=='kicked' else sum(entities[ref]['toughness'] for ref in cost_refs) if p['amount']=='cost_toughness' else p['amount']
                    if p['effect']=='mass_exchange':
                        returning=[[n for n in zones['graveyard'][pl] if extension_eligible(o[n],p['scope'])] for pl in (0,1)]
                        for pl in (0,1):
                            for n in returning[pl]:zones['graveyard'][pl].remove(n);zones['exile'][pl].append(n)
                        for ref,e in list(entities.items()):
                            if e['alive'] and extension_eligible(o[e['name']],p['scope']):leave(ref)
                        for pl in (0,1):
                            for i,n in enumerate(returning[pl]):
                                zones['exile'][pl].remove(n);ref=f'returned{pl}_{i}'
                                index=sum(e['alive'] and e['player']==pl and e['name']==n for e in entities.values())
                                entities[ref]=dict(name=n,player=pl,owner=pl,alive=True,tapped=False,power=int(o[n].get('power',0)),toughness=int(o[n].get('toughness',0)),damage=0,uncertain=False)
                                actions.append(step('bind_permanent_copy',player=pl,value=rows[n]['csvid'],index=index,ref=ref))
                                if 'Creature' in o[n]['type_line']:actions.append(observe(ref,'sick',1))
                    elif p['effect']=='sacrifice_keep':
                        for ref,e in entities.items():
                            if e['alive'] and ref not in kept:leave(ref)
                    elif p['effect']=='reanimate':
                        zones['graveyard'][owner].remove(grave_target);ref='returned';index=sum(e['alive'] and e['player']==owner and e['name']==grave_target for e in entities.values())
                        entities[ref]=dict(name=grave_target,player=owner,owner=owner,alive=True,tapped=False,power=int(o[grave_target].get('power',0)),toughness=int(o[grave_target].get('toughness',0)),damage=0,uncertain=False)
                        actions.append(step('bind_permanent_copy',player=owner,value=rows[grave_target]['csvid'],index=index,ref=ref))
                        if 'Creature' in o[grave_target]['type_line']:actions.append(observe(ref,'sick',1))
                        if v=='returned_activate':actions.extend([step('activate',ref=ref,value=1),step('resolve')]);mana[owner][0]+=2;entities[ref]['tapped']=True
                        if v=='returned_attack':
                            actions.extend([step('begin_attackers'),step('attack',ref=ref,value=0),step('finish_attackers'),step('advance_turn'),step('advance_turn'),step('begin_attackers'),step('attack',ref=ref,value=1),step('finish_attackers'),observe(ref,'sick',0),observe(ref,'attacking',1)]);entities[ref]['tapped']=True
                    elif p['effect']=='control':
                        e=entities['victim'];changed=e['player']!=owner;e['player']=owner
                        actions.extend([step('bind_permanent_copy',player=owner,value=rows[e['name']]['csvid'],index=0,ref='victim'),observe('victim','sick',int(changed))])
                        if v=='destroy_controlled':actions.append(step('destroy',ref='victim'));leave('victim')
                    elif p['effect']=='edict':
                        for pl in recipients:life[pl]-=p['secondary_loss']
                        if p['gain']=='sacrificed_toughness':life[owner]+=sum(entities[ref]['toughness'] for ref in sacrificed)
                        if p.get('_wave7') and p.get('sacrifice_token'):
                            t=dict(p['sacrifice_token']);t['count']=max(0,sum(entities[ref]['toughness'] for ref in sacrificed));tokens[owner].append(t)
                        if p.get('condition',{}).get('kind')=='dragon_as_cast' and dragon_as_cast:life[owner]+=p['condition']['gain']
                        if p.get('condition',{}).get('kind')=='empty_loss':
                            for pl in recipients:
                                if not any(entities[ref]['player']==pl for ref in sacrificed):life[pl]-=(life[pl]+1)//2
                    elif p['effect'] in ('destroy','exile'):
                        if p['effect']=='exile' or 'Indestructible' not in o[entities['victim']['name']]['keywords']:leave('victim','exile' if p['effect']=='exile' else 'graveyard')
                        life[entities['victim']['player']]-=deaths[0] if p['secondary_loss']=='deaths' else p['secondary_loss']
                    elif p['effect']=='damage':
                        if creature_target:
                            e=entities['victim'];e['damage']+=amount
                            if e['damage']>=e['toughness'] and 'Indestructible' not in o[e['name']]['keywords']:leave('victim')
                        else:life[target]-=amount
                        if p['gain']=='dealt':life[owner]+=amount
                    elif p['effect']=='discard':
                        n=min(amount,len(zones['hand'][target]))
                        if target==1:unknown[target]+=n;possible[target].update(zones['hand'][target])
                        else:
                            for _ in range(n):
                                n=(max if v=='last_choice' else min)(zones['hand'][target],key=lambda nm:rows[nm]['csvid']);zones['hand'][target].remove(n);zones['graveyard'][target].append(n)
                    elif p['effect']=='tokens':
                        t=dict(p['token']);t['count']=power if t['count']=='cost_power' else len(sacrificed) if t['count']=='sacrificed_count' else t['count']
                        for k in ('power','toughness'):
                            if t[k]=='sacrificed_power':t[k]=sum(entities[ref]['power'] for ref in sacrificed)
                        if t['toughness']<=0:t['count']=0
                        tokens[owner].append(t)
                    elif p['effect']=='target_sac_tokens':
                        e=entities['victim'];t=dict(p['token']);t['count']=max(0,e['power']);leave('victim');tokens[e['player']].append(t)
                    elif p['effect']=='gain':life[owner]+=amount
                    elif p['effect']=='lose_life':life[target]-=amount
                    elif p['effect']=='mana':
                        for c in colors:mana[owner][c]+=1
                    elif p['effect']=='mass_destroy':
                        for ref,e in entities.items():
                            if e['alive'] and 'Artifact' in o[e['name']]['type_line'] and 'Indestructible' not in o[e['name']]['keywords']:leave(ref)
                    elif p['effect'] in ('mass_damage','mass_stats'):
                        for ref,e in entities.items():
                            if not e['alive'] or 'Creature' not in o[e['name']]['type_line']:continue
                            if p['effect']=='mass_stats':e['power']-=amount;e['toughness']-=amount
                            else:e['damage']+=amount
                            if e['toughness']<=0 or e['damage']>=e['toughness'] and 'Indestructible' not in o[e['name']]['keywords']:leave(ref)
            if succeeded and p.get('multi'):
                valid=[ref for ref in multi_targets if ref not in invalid_targets and entities[ref]['alive']]
                if p['effect']=='multi_bounce':
                    for ref in valid:leave(ref,'hand')
                elif p['effect']=='multi_destroy':
                    for ref in valid:
                        if 'Indestructible' not in o[entities[ref]['name']]['keywords']:leave(ref)
                elif p['effect']=='multi_reduce':
                    for ref in valid:
                        e=entities[ref];e['power']-=p['amount'];e['toughness']-=p['amount'];e['temporary_reduce']=True
                        if e['toughness']<=0:leave(ref)
                elif valid:
                    chosen=valid[0] if len(valid)==1 else 'victim1' if v=='alternate_choice' else 'victim';leave(chosen)
                    for ref in valid:
                        if ref==chosen:continue
                        if p['effect']=='choice_sac_bounce':leave(ref,'hand')
                        else:
                            e=entities[ref];e['power']-=1;e['toughness']-=1;e['minus1_counters']=1
                            if e['toughness']<=0:leave(ref)
            actions.append(step('assert_choices_consumed'))
            if v=='cleanup':
                actions.extend([step('cleanup'),step('refresh')])
                for e in entities.values():
                    e['damage']=0
                    if e['alive'] and p['effect']=='mass_stats':e['power']=int(o[e['name']].get('power',0));e['toughness']=int(o[e['name']].get('toughness',0))
                    if e['alive'] and e.get('temporary_reduce'):e['power']=int(o[e['name']]['power']);e['toughness']=int(o[e['name']]['toughness'])
            if v=='flying_token':
                tid=rows[p['token']['name']]['csvid'];actions.extend([step('bind_permanent_copy',ref='flying_token',player=owner,index=0,value=tid),step('turn',player=1-owner,value=0),step('cast',ref='flying_attacker',value=1),step('resolve'),step('advance_turn'),step('advance_turn'),step('begin_attackers'),step('attack',ref='flying_attacker',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='flying_token',other='flying_attacker',value=1),observe('flying_token','blocking_count',1)])
                pay(cost_from_oracle(o['Suntail Hawk']),1-owner);zones['hand'][1-owner].remove('Suntail Hawk');entities['flying_attacker'].update(alive=True,tapped=True)
                for t in tokens[owner]:t['sick']=0
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        if p.get('same_name_search') and v=='own_target':checks.append(observe('victim','sick',0))
        for ref,e in entities.items():
            if e['alive'] and not e['uncertain']:
                checks.extend([observe(ref,'in_play',1),observe(ref,'tapped',int(e['tapped']))])
                if 'Creature' in o[e['name']]['type_line']:checks.extend(observe(ref,k,e[k]) for k in ('power','toughness','damage'))
                if e.get('minus1_counters'):checks.append(observe(ref,'minus1_counters',e['minus1_counters']))
        universe=set(p.get('witness_universe',WAVE5_PINS['base_cards']) if p.get('_wave5') else (WAVE4_PINS if p.get('_wave4') else EXTENSION_PINS)['cards'])|{name}
        for pl in (0,1):
            token_counts={t['name']:t['count'] for t in tokens[pl]}
            checks.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values())+sum(token_counts.values()))])
            checks.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
            for n in sorted(universe):checks.append(step('assert_zone_multiplicity',player=pl,zone='battlefield',index=rows[n]['csvid'],value=sum(e['alive'] and e['player']==pl and e['name']==n for e in entities.values())+token_counts.get(n,0)))
            for t in tokens[pl]:
                if not t['count']:continue
                properties=dict(token=1,artifact=0,color=sum(1<<{'B':1,'U':2,'G':3,'R':4,'W':5}[c] for c in t['colors']),power=t['power'],toughness=t['toughness'])
                properties.update({'subtype_'+sub:1 for sub in t['subtypes'] if sub in {'Goblin','Insect','Zombie','Elf','Warrior','Elemental'}})
                if p.get('_wave7') and 'Soldier' in t['subtypes']:properties['subtype_Soldier']=1
                checks.extend(step('assert_matching_permanents',player=pl,index=rows[t['name']]['csvid'],property=k,value=n) for k,n in properties.items())
                for i in range(t['count']):checks.extend([step('bind_permanent_copy',ref=f'token{pl}_{i}',player=pl,index=i,value=rows[t['name']]['csvid']),observe(f'token{pl}_{i}','sick',t.get('sick',1))])
            for zone,players in zones.items():
                names=players[pl];count=len(names)+unknown[pl]*(1 if zone=='graveyard' else -1 if zone=='hand' else 0);checks.append(step('assert_zone',player=pl,zone=zone,value=count))
                for n in sorted(universe):
                    if unknown[pl] and zone=='graveyard' and n in possible[pl]:continue
                    op='assert_zone_multiplicity_max' if unknown[pl] and zone=='hand' else 'assert_zone_multiplicity';checks.append(step(op,player=pl,zone=zone,index=rows[n]['csvid'],value=names.count(n)))
                if zone=='library' and pl not in shuffled:checks.extend(step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(names))
            checks.extend(step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=zones['hand'][pl].count(n)+zones['graveyard'][pl].count(n)) for n in sorted(universe))
        seq=pre+actions+checks
        if v in ('ready_snapshot','pending_snapshot','resolved_snapshot','delve_snapshot'):
            at=0 if v=='ready_snapshot' else len(actions) if v=='resolved_snapshot' else pending
            seq=pre+actions[:at]+[step('capture')]+actions[at:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[at:]+checks+[step('assert_recorded_state')]
        output.append(dict(name=f'EdictExtensionRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family='edict_spell',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps'])))
    return output
