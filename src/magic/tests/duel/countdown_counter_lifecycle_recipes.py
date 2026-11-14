"""Whole-Oracle countdown creatures, real upkeeps and printed counter costs."""
import copy,json,re
from collections import Counter
from pathlib import Path
from entry_aura_freeze_recipes import FreezeLedger
from library_search_recipes import wave6_exact as exact_pin

FAMILY='countdown_counter_lifecycle'
DATA=Path(__file__).parent/'data/countdown_counter_lifecycle_wave8_profiles.json'
def pins():return json.loads(DATA.read_text(encoding='utf8'))

def mechanism(r):
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or not r.get('type_line','').startswith('Creature — ') or any(not re.fullmatch(r'[0-9]+',r.get(k,'')) for k in ('power','toughness')):
        raise ValueError('whole normal independently typed numeric creature required')
    q=dict(keywords=[],entry_tapped=False,edge='none',edge_amount=0,activation='none',generic=0,activation_amount=0)
    countdown=None
    for line in r['oracle_text'].splitlines():
        m=re.fullmatch(r'(Fading|Vanishing) ([1-9][0-9]*) \(.+\)',line)
        if m and countdown is None:countdown=(m[1].lower(),int(m[2]));continue
        if re.fullmatch(r'(Flying|Shroud)(?: \(.*\))?',line):q['keywords'].append(line.split(' ')[0]);continue
        if line=='This creature enters tapped.' and not q['entry_tapped']:q['entry_tapped']=True;continue
        m=re.fullmatch(r'When this creature enters or leaves the battlefield, you gain ([1-9][0-9]*) life\.',line)
        if m and q['edge']=='none':q.update(edge='life',edge_amount=int(m[1]));continue
        m=re.fullmatch(r'When this creature enters or leaves the battlefield, it deals ([1-9][0-9]*) damage to target player or planeswalker\.',line)
        if m and q['edge']=='none':q.update(edge='damage',edge_amount=int(m[1]));continue
        m=re.fullmatch(r'(?:\{([0-9]+)\}, )?Remove a fade counter from this creature: (.+)',line)
        if m and q['activation']=='none':
            effect=m[2];damage=re.fullmatch(r'It deals ([1-9][0-9]*) damage to any target\.',effect)
            if damage:q.update(activation='damage',activation_amount=int(damage[1]))
            elif effect=='Tap target creature.':q['activation']='tap'
            elif effect=='Destroy target artifact.':q['activation']='destroy_artifact'
            else:raise ValueError('complete typed fade-cost effect required')
            q['generic']=int(m[1] or 0);continue
        raise ValueError('unmodeled whole countdown clause: '+line)
    if countdown is None or (q['activation']!='none' and countdown[0]!='fading') or len(set(q['keywords']))!=len(q['keywords']) or set(r.get('keywords',[]))!={countdown[0].capitalize(),*q['keywords']}:
        raise ValueError('one complete countdown plus exact native keywords required')
    return dict(q,kind=countdown[0],counter='fade' if countdown[0]=='fading' else 'time',initial=countdown[1],power=int(r['power']),toughness=int(r['toughness']),clauses=r['oracle_text'].splitlines())

def spec(d,r):
    from recipes import cost_from_oracle
    profile=pins()['sources'].get((r or {}).get('name'));q=mechanism(r);params=d.get('params')
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not profile or not exact_pin(profile['oracle'],r) or d.get('oracle_text')!=r['oracle_text'] or not isinstance(params,dict) or set(params)!={'schema','mechanism'} or type(params['schema']) is not int or params['schema']!=1 or not exact_pin(params['mechanism'],q):
        raise ValueError('independent whole source/cost/type/countdown pins required')
    return dict(q,family=FAMILY,csvid=profile['csvid'],name=r['name'],mana=cost_from_oracle(r))

def variants(p):
    modes=['value','payment','lifecycle','opponent_upkeep','first_upkeep','zero_boundary','snap_zero','insufficient_mana','wrong_color','countered','source_destroyed','nonmatching_entry','ready_snapshot','pending_snapshot','partial_snapshot','resolved_snapshot','expiry_snapshot','two_sources','attack','block']
    if 'Shroud' in p['keywords']:modes+=['shroud_own','shroud_opponent']
    else:modes+=['source_bounced','bounce_recast','control_transfer']
    if 'Flying' in p['keywords']:modes+=['flying_evasion','flying_reach']
    if p['edge']=='damage':modes+=['target_self','planeswalker']
    if p['activation']!='none':
        modes+=['ability','deplete','ability_source_destroyed','ability_target_destroyed','ability_tapped','ability_fresh','ability_zero']
        if p['generic']:modes+=['ability_no_mana']
        if p['activation']=='damage':modes+=['ability_creature','ability_planeswalker','ability_self']
    return [f'{m}_{o}' for o in (0,1) for m in modes]

def validate_witnesses(oracle,rows):
    data=pins()
    for group in ('sources','witnesses'):
        for n,s in data[group].items():
            r=rows.get(n,{})
            if not exact_pin(s['oracle'],oracle.get(n)) or r.get('kind')!='card' or r.get('csvid')!=s['csvid'] or r.get('handler')!=s['handler'] or r.get('handler') in (None,'NULL','0') or s['oracle'].get('layout')!='normal':raise ValueError('independent source/witness pin changed: '+n)
            if r['handler']=='card_dummy' and (group=='sources' or n not in data['honest_static_only_dummy_witnesses']):raise ValueError('unimplemented primary or unreviewed static witness')

def _case(name,csvid,d,p,variant,rows,oracle):
    from recipes import card,step,cost_from_oracle
    mode,owner=variant.rsplit('_',1);owner=int(owner);other=1-owner;setup=[]
    def add(n,ref,pl,zone='battlefield'):
        e=card(n,ref,pl,zone)
        if zone=='battlefield':e['state']=['untapped']
        setup.append(e)
    add(name,'source',owner,'hand')
    for pl in (0,1):
        add('Plains',f'hand_{pl}',pl,'hand');add('Swamp',f'grave_{pl}',pl,'graveyard');add('Island',f'exile_{pl}',pl,'exile')
        for i,n in enumerate(['Forest','Mountain','Swamp','Island']):add(n,f'library_{pl}_{i}',pl,'library')
    if mode=='countered':add('Counterspell','counter',other,'hand')
    if mode in ('source_bounced','bounce_recast'):add('Unsummon','bounce',owner,'hand')
    if mode=='control_transfer':add('Control Magic','control',other,'hand')
    if mode=='snap_zero':add('Aether Snap','snap',owner,'hand')
    if mode=='nonmatching_entry':add('Darksteel Relic','entrant',owner,'hand')
    if mode=='deplete' and p['generic']:add('Sol Ring','zero_spender',owner,'hand')
    if mode=='two_sources':add(name,'second',owner,'hand')
    if mode.startswith('shroud_'):add('Boomerang','probe',owner if mode=='shroud_own' else other,'hand')
    pw=mode in ('planeswalker','ability_planeswalker')
    if pw:add('Jace Beleren','walker',other,'hand')
    if mode=='block':add('Hill Giant','attacker',other)
    if mode in ('attack','flying_evasion','flying_reach'):add('Giant Spider' if mode=='flying_reach' else 'Horned Turtle','blocker',other)
    if mode.startswith('ability') or mode=='deplete':
        if p['activation']=='destroy_artifact':
            for i in range(p['initial']+1):add('Ornithopter',f'target_{i}',other)
        else:add('Hill Giant','target_0',other)
    case=dict(card=name,fixture='empty_main',seed=123,setup=setup);w=FreezeLedger(case,p,mode,owner,rows,oracle)
    actions=[step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)];pools=[[0]*6 for _ in (0,1)];life=[20,20];current=owner;counters={};sick=set();damage={};loyalty={};control=False
    def cost(n):return [0]*6 if oracle[n]['mana_cost']=='{0}' else cost_from_oracle(oracle[n])
    def funds(pl,v):pools[pl]=list(v);actions.extend(step('set_mana',player=pl,color=c,value=a) for c,a in enumerate(v));actions.append(step('refresh'))
    def pay(pl,v):
        pools[pl]=[a-b for a,b in zip(pools[pl],v)]
        if min(pools[pl])<0:raise ValueError('real countdown fixture payment missing')
    def cast(ref,response=False):
        nonlocal actions
        e=w.entities[ref];actions.append(step('cast_response' if response else 'cast',ref=ref,value=1));pay(e['player'],cost(e['name']));w.move(ref,'stack');actions += [step('assert_stack',value=2 if response else 1),step('assert_card',ref=ref,property='in_hand',value=0)]
    def edge_choices(ref='source'):
        if p['edge']!='damage':return []
        pl=w.entities[ref]['player']
        return [step('choose_target',player=pl,ref='walker')] if pw else [step('choose_player',player=pl,value=pl if mode=='target_self' else 1-pl)]
    def edge(ref='source'):
        pl=w.entities[ref]['player']
        if p['edge']=='life':life[pl]+=p['edge_amount']
        elif p['edge']=='damage':
            if pw:loyalty['walker']-=p['edge_amount']
            else:life[pl if mode=='target_self' else 1-pl]-=p['edge_amount']
    def leave(ref='source',destination='graveyard'):
        nonlocal control
        edge(ref);w.move(ref,destination);sick.discard(ref)
        if ref=='source' and control and w.entities['control']['zone']=='battlefield':w.move('control','graveyard');control=False
    def receipts():
        seq=[step('assert_choices_consumed'),step('assert_stack',value=0),step('assert_forced_target_choices',value=0)]
        universe=sorted({e['name'] for e in w.entities.values()})
        for pl in (0,1):
            seq += [step('assert_life',player=pl,value=life[pl]),step('assert_shuffle_count',player=pl,value=0)]+[step('assert_mana',player=pl,color=c,value=v) for c,v in enumerate(pools[pl])]
            for z in ('battlefield','hand','graveyard','exile','library'):
                seq.append(step('assert_zone',player=pl,zone=z,value=w.count(pl,z)))
                for n in universe:
                    v=sum(e['zone']=='battlefield' and e['player']==pl and e['name']==n for e in w.entities.values()) if z=='battlefield' else w.zones[z][pl].count(n)
                    seq.append(step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=v))
                if z=='library':seq += [step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(w.zones[z][pl])]
            totals=Counter(w.zones['hand'][pl]+w.zones['graveyard'][pl]);seq += [step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=totals[n]) for n in universe]
        for ref,e in w.entities.items():
            if e['zone']!='battlefield':continue
            seq += [step('assert_card',ref=ref,property='in_play',value=1),step('assert_card',ref=ref,property='tapped',value=int(e['tapped']))]
            if ref in counters:seq.append(step('assert_counter',ref=ref,counter_kind=p['counter'],value=counters[ref]))
            if ref in loyalty:seq.append(step('assert_counter',ref=ref,counter_kind='loyalty',value=loyalty[ref]))
            if 'Creature' in oracle[e['name']]['type_line'].partition(' — ')[0].split():
                seq += [step('assert_card',ref=ref,property=k,value=int(oracle[e['name']][k])) for k in ('power','toughness')]+[step('assert_card',ref=ref,property='damage',value=damage.get(ref,0))]
        return seq
    def enter(ref='source'):
        actions.extend(edge_choices(ref));actions.append(step('resolve'));w.move(ref,'battlefield');w.entities[ref]['tapped']=p['entry_tapped'];sick.add(ref);counters[ref]=p['initial'];edge(ref);actions.extend(receipts())
    def visit():
        nonlocal current
        if any(any(v) for v in pools):raise ValueError('real upkeep requires all actual mana spent')
        actions.append(step('advance_turn'));current=1-current;damage.clear();actions.append(step('untap_step',player=current))
        for ref,e in w.entities.items():
            if e['zone']=='battlefield' and e['player']==current:e['tapped']=False;sick.discard(ref)
        due=[r for r in counters if w.entities[r]['zone']=='battlefield' and w.entities[r]['player']==current]
        if len(due)>1 and current==0:actions.append(step('choose_trigger_source',player=current,ref=due[0]))
        for ref in due:
            departing=counters[ref]==0 if p['kind']=='fading' else counters[ref]==1
            if departing:actions.extend(edge_choices(ref))
        actions.append(step('upkeep_step',player=current))
        for ref in due:
            if counters[ref]:
                counters[ref]-=1
                if p['kind']=='vanishing' and counters[ref]==0:leave(ref)
            else:leave(ref)
        actions.extend(receipts());actions.append(step('turn',player=current,value=0));return due
    def own_visit():
        pl=w.entities['source']['player']
        visit()
        if current!=pl:visit()
    def expire(zero_only=False):
        limit=p['initial']+2
        while w.entities['source']['zone']=='battlefield' and (not zero_only or counters['source']>0):
            own_visit();limit-=1
            if limit<0:raise ValueError('bounded countdown did not progress')
    def destroy_source():
        actions.extend(edge_choices());actions.append(step('destroy',ref='source'));leave();actions.append(step('resolve'));actions.extend(receipts())
    def attack_combat():
        nonlocal actions
        own_visit();actions += [step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),step('begin_blockers')];w.entities['source']['tapped']=True
        legal='Flying' not in p['keywords'] or mode=='flying_reach';actions.append(step('block',ref='blocker',other='source',value=int(legal)));actions.append(step('advance_combat'));actions.append(step('assert_card',ref='source',property='blocked',value=int(legal)))
        source_dies=legal and int(oracle[w.entities['blocker']['name']]['power'])>=p['toughness']
        if source_dies:actions.extend(edge_choices())
        actions.append(step('combat_damage'))
        if not legal:life[other]-=p['power']
        else:
            damage['source']=int(oracle[w.entities['blocker']['name']]['power']);damage['blocker']=p['power']
            if damage['blocker']>=int(oracle[w.entities['blocker']['name']]['toughness']):w.move('blocker','graveyard')
            if source_dies:leave()
        actions.extend(receipts());actions.append(step('end_combat'));actions.extend(receipts())
    def block_combat():
        nonlocal actions
        own_visit();visit();actions += [step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='source',other='attacker',value=1),step('advance_combat')];w.entities['attacker']['tapped']=True
        source_dies=3>=p['toughness']
        if source_dies:actions.extend(edge_choices())
        actions.append(step('combat_damage'));damage['source']=3;damage['attacker']=p['power']
        if p['power']>=3:w.move('attacker','graveyard')
        if source_dies:leave()
        actions.extend(receipts());actions.append(step('end_combat'));actions.extend(receipts())
    def ability(target=None,depart=None):
        nonlocal actions
        fee=[p['generic'],0,0,0,0,0];funds(owner,[0]*6 if mode=='ability_no_mana' else fee)
        allowed=counters['source']>0 and not(mode=='ability_no_mana' and p['generic'])
        actions.append(step('can_activate',ref='source',value=int(allowed)))
        if not allowed:actions.append(step('activate',ref='source',value=0));actions.extend(receipts());return
        if target is None:target='walker' if mode=='ability_planeswalker' else 'source' if mode=='ability_self' else 'target_0' if p['activation']!='damage' or mode=='ability_creature' or depart=='target' else None
        actions.append(step('choose_player',player=owner,value=other) if target is None else step('choose_target',player=owner,ref=target));actions.append(step('activate',ref='source',value=1));pay(owner,fee);counters['source']-=1
        actions += [step('assert_counter',ref='source',counter_kind='fade',value=counters['source']),step('assert_stack',value=1)]
        if depart=='source':actions.extend(edge_choices());actions.append(step('destroy',ref='source'));leave()
        if depart=='target':actions.append(step('destroy',ref=target));w.move(target,'graveyard')
        source_self_dies=target=='source' and p['activation']=='damage' and p['activation_amount']>=p['toughness']
        if source_self_dies:actions.extend(edge_choices())
        actions.append(step('resolve'))
        if depart!='target':
            if p['activation']=='tap':w.entities[target]['tapped']=True
            elif p['activation']=='destroy_artifact':w.move(target,'graveyard')
            elif target is None:life[other]-=p['activation_amount']
            elif target=='walker':loyalty['walker']-=p['activation_amount']
            else:
                damage[target]=damage.get(target,0)+p['activation_amount']
                if damage[target]>=int(oracle[w.entities[target]['name']]['toughness']):
                    if target=='source':leave()
                    else:w.move(target,'graveyard')
        actions.extend(receipts())
    funds(0,[0]*6);funds(1,[0]*6)
    if pw:funds(other,cost('Jace Beleren'));cast('walker');actions.append(step('resolve'));w.move('walker','battlefield');loyalty['walker']=3;actions.extend(receipts())
    initial=list(p['mana'])
    if mode=='insufficient_mana':initial=[0]*6
    if mode=='wrong_color':
        color=next(i for i in range(1,6) if initial[i]);initial[1 if color!=1 else 2]+=initial[color];initial[color]=0
    funds(owner,initial)
    if mode=='countered':funds(other,cost('Counterspell'))
    ready=len(actions)
    if mode in ('insufficient_mana','wrong_color'):actions.append(step('cast',ref='source',value=0));actions.extend(receipts());pending=len(actions)
    else:
        cast('source');pending=len(actions)
        if mode=='countered':
            actions.append(step('announce_spell',ref='source'));cast('counter',True);actions.append(step('resolve_response'));w.move('counter','graveyard');w.move('source','graveyard');actions.extend(receipts())
        else:enter()
    resolved=len(actions);partial=resolved;expiry=resolved
    active=w.entities['source']['zone']=='battlefield'
    if active:
        if mode in ('lifecycle','planeswalker','target_self'):expire()
        elif mode=='opponent_upkeep':visit()
        elif mode in ('first_upkeep','partial_snapshot'):own_visit();partial=len(actions);expire() if mode=='partial_snapshot' else None
        elif mode=='zero_boundary':expire(True)
        elif mode=='expiry_snapshot':
            # Capture while one fade counter remains (then two own upkeeps) or
            # one time counter remains (then one); no synthetic trigger window.
            while counters['source']>1:own_visit()
            expiry=len(actions);expire()
        elif mode=='snap_zero':
            funds(owner,cost('Aether Snap'));cast('snap');vanish=p['kind']=='vanishing';actions.extend(edge_choices() if vanish else []);actions.append(step('resolve'));w.move('snap','graveyard');counters['source']=0
            if vanish:leave()
            actions.extend(receipts())
            if not vanish:own_visit()
        elif mode=='source_destroyed':destroy_source()
        elif mode in ('source_bounced','bounce_recast'):
            funds(owner,cost('Unsummon'));actions.append(step('choose_target',player=owner,ref='source'));cast('bounce');actions.extend(edge_choices());actions.append(step('resolve'));w.move('bounce','graveyard');leave(destination='hand');actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'));actions.extend(receipts())
            if mode=='bounce_recast':funds(owner,p['mana']);cast('source');enter();expire()
        elif mode=='control_transfer':
            visit();funds(other,cost('Control Magic'));actions.append(step('choose_target',player=other,ref='source'));cast('control');actions.append(step('resolve'));w.move('control','battlefield');w.entities['source']['player']=other;sick.add('source');control=True;actions.append(step('bind_permanent_copy',player=other,value=csvid,index=0,ref='source'));actions.extend(receipts());expire()
        elif mode=='nonmatching_entry':funds(owner,[0]*6);cast('entrant');actions.append(step('resolve'));w.move('entrant','battlefield');actions.extend(receipts())
        elif mode=='two_sources':funds(owner,p['mana']);cast('second');enter('second');own_visit()
        elif mode.startswith('shroud_'):actions.append(step('can_target_creature',ref='probe',other='source',value=0));actions.extend(receipts())
        elif mode in ('attack','flying_evasion','flying_reach'):attack_combat()
        elif mode=='block':block_combat()
        elif mode.startswith('ability') or mode=='deplete':
            if mode=='ability_tapped':own_visit();actions += [step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('end_combat')];w.entities['source']['tapped']=True;life[other]-=p['power'];actions.extend(receipts())
            if mode=='ability_zero':expire(True)
            if mode=='deplete':
                for i in range(p['initial']):ability(target=f'target_{i}' if p['activation']=='destroy_artifact' else 'target_0' if p['activation']=='tap' else None)
                ability()
                if p['generic']:cast('zero_spender');actions.append(step('resolve'));w.move('zero_spender','battlefield');actions.extend(receipts())
                own_visit()
            else:ability(depart='source' if mode=='ability_source_destroyed' else 'target' if mode=='ability_target_destroyed' else None)
    if mode in ('ready_snapshot','pending_snapshot','partial_snapshot','expiry_snapshot'):
        cut={'ready_snapshot':ready,'pending_snapshot':pending,'partial_snapshot':partial,'expiry_snapshot':expiry}[mode];tail=copy.deepcopy(actions[cut:]);actions=actions[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
    if mode=='resolved_snapshot':actions += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+receipts()+[step('assert_recorded_state')]
    case.update(name=f'CountdownLifecycleRecipe_{csvid}_{variant}',steps=actions,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],covered_clauses=p['clauses'],untested_components=['No priority window exposed between automatic last-counter removal and Vanishing sacrifice; final real lifecycle/effects observed.','Damage prevention, linked exile/tutors/copy-token branches excluded by whole-clause parser.','Arbitrary counter replacement/proliferation, extra/skip upkeeps, humiliation/phasing/multiplayer and optional activation cancellation await separate calibration.']))
    return case

def expand(name,csvid,d,p,selected,rows,oracle):
    if csvid!=p['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(p)):raise ValueError('explicit countdown source and supported variants required')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,d,p,v,rows,oracle) for v in selected]
