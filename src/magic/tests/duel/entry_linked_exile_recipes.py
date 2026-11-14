"""Whole-Oracle until-source-leaves exile with real source and return callbacks."""
import copy
import json
import re
from pathlib import Path

from library_search_recipes import wave6_exact as exact_pin
from entry_trigger_value_selection_recipes import SelectionLedger

FAMILY='entry_linked_exile'
DATA=Path(__file__).parent/'data/entry_linked_exile_wave8_profiles.json'
PALETTE=['Elvish Mystic','Horned Turtle','Giant Spider','Ornithopter','Dryad Arbor',
         'Darksteel Citadel','Sol Ring','Seal of Cleansing','Darksteel Myr','Hill Giant']
PREDICATES={
    'creature an opponent controls':dict(types=['Creature'],opponent=True,nonland=False,toughness_min=None,mana_value_max=None,tapped=False),
    'nonland permanent an opponent controls':dict(types=['Artifact','Creature','Enchantment','Planeswalker','Battle'],opponent=True,nonland=True,toughness_min=None,mana_value_max=None,tapped=False),
    'creature with toughness 3 or greater':dict(types=['Creature'],opponent=False,nonland=False,toughness_min=3,mana_value_max=None,tapped=False),
    'creature with mana value 3 or less an opponent controls':dict(types=['Creature'],opponent=True,nonland=False,toughness_min=None,mana_value_max=3,tapped=False),
    'creature or enchantment an opponent controls':dict(types=['Creature','Enchantment'],opponent=True,nonland=False,toughness_min=None,mana_value_max=None,tapped=False),
    'artifact or creature an opponent controls':dict(types=['Artifact','Creature'],opponent=True,nonland=False,toughness_min=None,mana_value_max=None,tapped=False),
    'tapped creature an opponent controls':dict(types=['Creature'],opponent=True,nonland=False,toughness_min=None,mana_value_max=None,tapped=True)}


def pins():return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(r):
    kind=set(r.get('type_line','').partition(' — ')[0].split()) if r else set()
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or not kind&{'Creature','Enchantment'} or 'Aura' in r['type_line'].split():raise ValueError('linked exile requires a whole normal standalone Creature or Enchantment')
    pattern=r"When this (creature|enchantment) enters, (you may )?exile target (.+) until this \1 leaves the battlefield\.(?: \(That (?:creature|permanent) returns under its owner's control\.\))?"
    matches=[(line,re.fullmatch(pattern,line)) for line in r['oracle_text'].splitlines()];matches=[(line,m) for line,m in matches if m]
    if len(matches)!=1:raise ValueError('one complete until-source-leaves clause required')
    clause,m=matches[0];other=[line for line in r['oracle_text'].splitlines() if line!=clause]
    if m[3] not in PREDICATES or m[1].capitalize() not in kind or any(not re.fullmatch(r'Flash(?: \(.*\))?',line) for line in other):raise ValueError('unreviewed linked source/predicate/additional rule')
    creature='Creature' in kind
    if creature and any(not str(r.get(k,'')).isdigit() for k in ('power','toughness')):raise ValueError('numeric surviving linked Creature required')
    if set(r.get('keywords',[]))-{'Flash'}:raise ValueError('unreviewed linked source passive')
    return dict(predicate=copy.deepcopy(PREDICATES[m[3]]),optional=bool(m[2]),creature=creature,flash='Flash' in r.get('keywords',[]),entry_clause=clause,other_clauses=other)


def spec(d,r):
    from recipes import cost_from_oracle
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not isinstance(d.get('variants'),list) or not d['variants'] or len(set(d['variants']))!=len(d['variants']):raise ValueError('explicit version2 linked variants required')
    q=d.get('params');parsed=mechanism(r);profile=pins()['sources'].get(r['name'])
    if not isinstance(q,dict) or set(q)!={'schema','mechanism','presentation'} or type(q['schema']) is not int or q['schema']!=1:raise ValueError('linked exile schema required')
    if not profile or not exact_pin(profile['oracle'],r) or d.get('oracle_text')!=r['oracle_text'] or not exact_pin(q['mechanism'],parsed) or not exact_pin(q['presentation'],profile['presentation']):raise ValueError('whole linked Oracle/source/predicate pin changed')
    if set(q['presentation'])!={'human_optional_trigger'} or type(q['presentation']['human_optional_trigger']) is not bool:raise ValueError('actual linked target UI annotation required')
    return dict(parsed,family=FAMILY,scope='permanent',csvid=profile['csvid'],source_name=r['name'],presentation=copy.deepcopy(q['presentation']),mana=cost_from_oracle(r))


def eligible(r,p,source_player,target_player,tapped=False,toughness=None):
    q=p['predicate'];types=set(r['type_line'].partition(' — ')[0].split())
    if not types.intersection(q['types']) or q['opponent'] and source_player==target_player or q['nonland'] and 'Land' in types:return False
    if q['tapped'] and not tapped:return False
    if q['mana_value_max'] is not None and r['cmc']>q['mana_value_max']:return False
    if q['toughness_min'] is not None:
        value=int(r['toughness']) if toughness is None else toughness
        if value<q['toughness_min']:return False
    return True


def variants(p):
    modes=['exile','payment','insufficient_mana','wrong_color','countered','source_destroyed','source_bounced','source_exiled','bounce_recast',
           'cleanup','nonmatching_entry','ready_snapshot','pending_snapshot','linked_snapshot','resolved_snapshot','two_sources','ownership_return','no_targets','bare_zones']
    if p['creature']:modes.append('source_blinked')
    if p['optional']:modes+=['decline','own_target','toughness_pump_return']
    else:modes.append('mandatory_cancel')
    if p['flash']:modes.append('offturn')
    if p['predicate']['tapped']:modes.append('untapped_target')
    else:modes.append('returned_etb')
    modes += ['target_'+str(i) for i in range(len(PALETTE))]
    result=[f'{m}_{o}' for o in (0,1) for m in modes]
    if p['presentation']['human_optional_trigger']:result.append('whole_decline_0')
    return result


def validate_witnesses(oracle,rows):
    for n,r in pins()['witnesses'].items():
        if n not in rows or not exact_pin(r,oracle.get(n)):raise ValueError('linked full witness pin changed: '+n)
    for n,p in pins()['sources'].items():
        if not exact_pin(p['oracle'],oracle.get(n)) or rows[n]['csvid']!=p['csvid'] or rows[n]['kind']!='card' or rows[n]['handler'] in (None,'NULL','card_dummy'):raise ValueError('linked source identity/implementation changed')


class LinkedLedger(SelectionLedger):
    def __init__(self,case,p,mode,owner,rows,oracle):
        super().__init__(case,p,mode,owner,rows,oracle);self.links={}
        for item in case['setup']:
            e=self.entities[item['ref']];e.update(owner=1-item['player'] if 'opponent_owned' in item.get('state',[]) else item['player'],tapped='tapped' in item.get('state',[]),boost=0)

    def exile_target(self,source,target):
        e=self.entities[target]
        if e['zone']!='battlefield':raise ValueError('linked target must be a real live permanent')
        self.zones['exile'][e['owner']].append(e['name']);e.update(zone='exile',player=e['owner'],boost=0,tapped=False);self.links[source]=target

    def release(self,source):
        if source not in self.links:return []
        ref=self.links.pop(source);e=self.entities[ref];self.zones['exile'][e['owner']].remove(e['name']);e.update(zone='battlefield',player=e['owner'],tapped=False,boost=0)
        self.entered_target(ref);return [ref]

    def entered_target(self,ref):
        e=self.entities[ref]
        if e['name'] in ('Elvish Visionary','Wall of Omens'):
            deck=self.zones['library'][e['player']]
            if not deck:raise ValueError('real returned-draw fixture exhausted its library')
            self.zones['hand'][e['player']].append(deck.pop(0))


def _case(name,csvid,d,p,variant,rows,oracle):
    from recipes import card,step,cost_from_oracle
    mode,owner=variant.rsplit('_',1);owner=int(owner);q=p['predicate'];setup=[]
    def legal(n,controller=1-owner,tapped=None,boost=0):
        r=oracle[n];t=int(r['toughness'])+boost if 'Creature' in r['type_line'].partition(' — ')[0].split() else None
        return eligible(r,p,owner,controller,q['tapped'] if tapped is None else tapped,t)
    primary='Sol Ring' if q['nonland'] else next(n for n in PALETTE if legal(n));controller=owner if mode=='own_target' else 1-owner
    if mode=='ownership_return':primary='Giant Spider' if q['toughness_min'] else 'Ornithopter'
    if mode=='toughness_pump_return':primary='Elvish Mystic'
    if mode=='returned_etb':primary='Wall of Omens' if q['toughness_min'] else 'Elvish Visionary'
    if mode.startswith('target_'):primary=PALETTE[int(mode.split('_')[1])]
    seed_tapped=q['tapped'] and mode!='untapped_target'
    no_offer=mode in ('no_targets','untapped_target') or mode.startswith('target_') and not legal(primary,controller,seed_tapped)
    if mode=='no_targets':
        if q['opponent']:controller=owner
        else:primary='Elvish Mystic'
    secondary=next(n for n in PALETTE if n!=primary and legal(n))
    secondary_controller=1-owner
    if no_offer:
        if q['opponent']:secondary_controller=owner
        else:secondary='Ornithopter'
    def add(n,ref,player,zone='battlefield',tapped=False,foreign=False):
        item=card(n,ref,player,zone)
        if zone=='battlefield':item['state']=['tapped' if tapped else 'untapped']+(['opponent_owned'] if foreign else [])
        setup.append(item)
    add(name,'source',owner,'hand')
    if mode=='two_sources':add(name,'second',owner,'hand')
    for player in (0,1):
        if mode!='bare_zones':
            add('Giant Growth',f'hand_{player}',player,'hand');add('Mountain',f'grave_{player}',player,'graveyard');add('Island',f'exile_{player}',player,'exile')
            for i in range(2):add('Forest',f'library_{player}_{i}',player,'library')
    add(primary,'target',controller,'hand' if mode=='returned_etb' else 'battlefield',seed_tapped,mode=='ownership_return')
    add(secondary,'alternate',secondary_controller,tapped=seed_tapped)
    add('Elvish Mystic','friend',owner);add('Swamp','own_land',owner);add('Mountain','enemy_land',1-owner)
    if mode in ('source_bounced','bounce_recast'):add('Boomerang','bounce',owner,'hand')
    if mode=='source_exiled':add('Utter End','exiler',owner,'hand')
    if mode=='source_blinked':add('Cloudshift','blink',owner,'hand')
    if mode=='countered':add('Counterspell','counter',1-owner,'hand')
    if mode=='nonmatching_entry':add('Darksteel Relic','nonmatching',owner,'hand')
    if mode=='toughness_pump_return':add('Giant Growth','pump',owner,'hand')
    case=dict(card=name,fixture='empty_main',seed=123,setup=setup);w=LinkedLedger(case,p,mode,owner,rows,oracle)
    actions=[];pools=[[0]*6,[0]*6];forced=0;current=1-owner if mode in ('offturn','returned_etb') else owner
    actions.append(step('turn',player=current,value=0))
    def cost(n):return [0]*6 if oracle[n]['mana_cost']=='{0}' else cost_from_oracle(oracle[n])
    def funds(player,vector):pools[player]=list(vector);actions.extend(step('set_mana',player=player,color=i,value=n) for i,n in enumerate(vector))
    def pay(player,n):
        pools[player]=[a-b for a,b in zip(pools[player],cost(n))];assert min(pools[player])>=0
    def receipt():
        out=[step('assert_choices_consumed'),step('assert_stack',value=0),step('assert_forced_target_choices',value=forced)]+w.receipts()
        out += [step('assert_mana',player=player,color=i,value=n) for player in (0,1) for i,n in enumerate(pools[player])]
        out += [step('assert_life',player=player,value=20) for player in (0,1)]
        names=sorted({e['name'] for e in w.entities.values()})
        for player in (0,1):
            out += [step('assert_zone_multiplicity',player=player,zone='battlefield',index=rows[n]['csvid'],value=sum(e['zone']=='battlefield' and e['player']==player and e['name']==n for e in w.entities.values())) for n in names]
        for ref,e in w.entities.items():
            if e['zone']!='battlefield':continue
            out += [step('assert_card',ref=ref,property='in_play',value=1),step('assert_card',ref=ref,property='tapped',value=int(e['tapped']))]
            if 'Creature' in oracle[e['name']]['type_line'].partition(' — ')[0].split():
                out += [step('assert_card',ref=ref,property=k,value=int(oracle[e['name']][k])+e['boost']) for k in ('power','toughness')]
                out += [step('assert_card',ref=ref,property=k,value=0) for k in ('damage','plus1_counters','minus1_counters')]
        # Once a source/target leaves, its old physical slot may hold a returned
        # object. Exact owned-zone/multiplicity receipts prove absence instead.
        for ref in ('source','second'):
            if ref in w.entities and w.entities[ref]['zone']=='hand':out.append(step('assert_card',ref=ref,property='in_hand',value=1))
        return out
    def bind_returned(refs):
        for ref in refs:
            e=w.entities[ref];count=sum(x['zone']=='battlefield' and x['player']==e['player'] and x['name']==e['name'] for x in w.entities.values())
            if count!=1:raise ValueError('linked return witness must have unique actual name per controller')
            actions.append(step('bind_permanent',player=e['player'],value=rows[e['name']]['csvid'],ref=ref))
    def cast(ref,response=False):
        e=w.entities[ref];actions.append(step('cast_response' if response else 'cast',ref=ref,value=1));pay(e['player'],e['name']);w.move(ref,'stack')
        actions.extend([step('assert_card',ref=ref,property='in_hand',value=0),step('assert_stack',value=2 if response else 1)])
        actions.extend(step('assert_mana',player=player,color=i,value=n) for player in (0,1) for i,n in enumerate(pools[player]))
    def effect(source,target):
        nonlocal forced
        if mode=='whole_decline':return
        if owner==0 and p['presentation']['human_optional_trigger']:actions.append(step('choose_trigger',player=owner,ref=source))
        if no_offer:
            if any(e['zone']=='battlefield' and eligible(oracle[e['name']],p,owner,e['player'],e['tapped'],int(oracle[e['name']]['toughness'])+e['boost'] if 'Creature' in oracle[e['name']]['type_line'].partition(' — ')[0].split() else None) for e in w.entities.values()):raise ValueError('no-target fixture still has an Oracle legal target')
            actions.append(step('choose_optional_target',player=owner,ref=target));return
        e=w.entities[target];r=oracle[e['name']]
        if not eligible(r,p,owner,e['player'],e['tapped'],int(r['toughness'])+e['boost'] if 'Creature' in r['type_line'].partition(' — ')[0].split() else None):raise ValueError('linked selected target is not Oracle legal')
        if mode=='decline':actions.append(step('choose_optional_target',player=owner,ref=target));return
        if mode=='mandatory_cancel':actions.append(step('choose_optional_target',player=owner,ref=target));forced+=1
        else:actions.append(step('choose_target',player=owner,ref=target))
        w.exile_target(source,target)
    def enter(source='source',target='target'):
        w.move(source,'battlefield');effect(source,target);actions.append(step('resolve'))
        if no_offer:actions.append(step('assert_unused_choices',value=1))
        actions.extend(receipt())
    def depart(source='source',kind='destroy'):
        if kind=='destroy':actions.append(step('destroy',ref=source));w.move(source,'graveyard');returned=w.release(source);actions.append(step('resolve'))
        else:
            helper='bounce' if kind=='bounce' else 'exiler';funds(owner,cost(w.entities[helper]['name']));actions.append(step('choose_target',player=owner,ref=source));cast(helper);actions.append(step('resolve'));w.move(helper,'graveyard');w.move(source,'hand' if kind=='bounce' else 'exile');returned=w.release(source)
            if kind=='bounce':actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref=source))
        bind_returned(returned);actions.extend(receipt())
    funds(0,[0]*6);funds(1,[0]*6)
    if mode=='returned_etb':
        funds(controller,cost(primary));cast('target');actions.append(step('resolve'));w.move('target','battlefield');w.entered_target('target');actions.extend(receipt());actions.append(step('advance_turn'));current=owner
    if mode=='toughness_pump_return':
        funds(owner,cost('Giant Growth'));actions.append(step('choose_target',player=owner,ref='target'));cast('pump');actions.append(step('resolve'));w.move('pump','graveyard');w.entities['target']['boost']=3;actions.extend(receipt())
    pool=list(p['mana'])
    if mode=='insufficient_mana':pool=[0]*6
    if mode=='wrong_color':
        c=next(i for i in range(1,6) if pool[i]);pool[1 if c!=1 else 2]+=pool[c];pool[c]=0
    funds(owner,pool)
    if mode=='countered':funds(1-owner,cost('Counterspell'))
    actions.append(step('refresh'));ready=len(actions)
    if mode in ('insufficient_mana','wrong_color'):actions.append(step('cast',ref='source',value=0));pending=len(actions);actions.extend(receipt())
    else:
        cast('source');actions.append(step('assert_zone',player=owner,zone='hand',value=w.count(owner,'hand')+1));pending=len(actions)
        if mode=='countered':
            actions.append(step('announce_spell',ref='source'));cast('counter',response=True);actions.append(step('resolve_response'));w.move('counter','graveyard');w.move('source','graveyard');actions.extend(receipt())
        else:enter()
    linked_cut=len(actions)
    if mode in ('source_destroyed','linked_snapshot','ownership_return','returned_etb','toughness_pump_return'):depart()
    if mode.startswith('target_'):depart()
    if mode=='source_bounced':depart(kind='bounce')
    if mode=='source_exiled':depart(kind='exile')
    if mode=='bounce_recast':depart(kind='bounce');funds(owner,p['mana']);cast('source');enter(target='alternate')
    if mode=='source_blinked':
        funds(owner,cost('Cloudshift'));actions.append(step('choose_target',player=owner,ref='source'));cast('blink');w.move('blink','graveyard');returned=w.release('source');effect('source','alternate');actions.append(step('resolve'));actions.append(step('bind_permanent',player=owner,value=csvid,ref='source'));bind_returned(returned);actions.extend(receipt())
    if mode=='two_sources':funds(owner,p['mana']);cast('second');enter('second','alternate');depart('source');depart('second')
    if mode=='nonmatching_entry':funds(owner,[0]*6);cast('nonmatching');actions.append(step('resolve'));w.move('nonmatching','battlefield');actions.extend(receipt())
    if mode=='cleanup':actions.append(step('cleanup'));actions.extend(receipt())
    if mode in ('ready_snapshot','pending_snapshot','linked_snapshot'):
        cut=ready if mode=='ready_snapshot' else pending if mode=='pending_snapshot' else linked_cut;tail=copy.deepcopy(actions[cut:]);actions=actions[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
    if mode=='resolved_snapshot':actions += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
    case.update(name=f'EntryLinkedExileRecipe_{csvid}_{variant}',steps=actions,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],covered_clauses=[p['entry_clause']]+p['other_clauses'],
        untested_components=['source departure between entry and automatic own ETB resolution: no exposed priority boundary','tokens/meld/multiface identities','Aura attachment on return','moving one of identical exiled objects before return','control changes/phasing','multiplayer/replacement effects']))
    return case


def expand(name,csvid,d,p,selected,rows,oracle):
    if csvid!=p['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(p)):raise ValueError('linked source identity/variants changed')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,d,p,v,rows,oracle) for v in selected]
