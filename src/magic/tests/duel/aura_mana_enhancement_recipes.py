"""Whole-Oracle Aura mana grants and additional tapped-for-mana production."""
import copy
import json
import re
from collections import Counter
from pathlib import Path
from entry_aura_freeze_recipes import FreezeLedger
from library_search_recipes import wave6_exact as exact_pin

FAMILY='aura_mana_enhancement'
DATA=Path(__file__).parent/'data/aura_mana_enhancement_wave8_profiles.json'
COLORS={'B':1,'U':2,'G':3,'R':4,'W':5}

def pins():return json.loads(DATA.read_text(encoding='utf8'))

def mechanism(r):
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r.get('type_line')!='Enchantment — Aura' or r.get('keywords')!=['Enchant']:
        raise ValueError('one whole normal Enchant Aura required')
    lines=r['oracle_text'].splitlines();m=re.fullmatch(r'Enchant (land|creature|Forest)',lines[0]) if lines else None
    if not m:raise ValueError('whole enchant predicate required')
    q=dict(scope=m[1],draw=0,life=0,entry_color=False)
    for line in lines[1:-1]:
        if line=='When this Aura enters, draw a card.' and not q['draw']:q['draw']=1
        elif re.fullmatch(r'When this Aura enters, you gain [1-9][0-9]* life\.',line) and not q['life']:q['life']=int(re.search(r'gain ([0-9]+)',line)[1])
        elif line=='As this Aura enters, choose a color.' and not q['entry_color']:q['entry_color']=True
        else:raise ValueError('unmodeled or repeated whole entry clause')
    last=lines[-1];noun='creature' if q['scope']=='creature' else 'land'
    g=re.fullmatch(r'Enchanted '+noun+r' has "\{T\}: Add (one mana of any color|two mana of any one color)\."',last)
    if g:q.update(kind='grant',output='any_one',amount=1 if g[1].startswith('one ') else 2)
    else:
        noun='Forest' if q['scope']=='Forest' else 'land'
        b=re.fullmatch(r'Whenever enchanted '+noun+r' is tapped for mana, its controller adds an additional (one mana of any color|\{G\}\{G\}|one mana of the chosen color|two mana in any combination of colors)\.',last)
        if not b or q['scope']=='creature':raise ValueError('complete grant or added mana clause required')
        output={'one mana of any color':'any_one','{G}{G}':'fixed_green','one mana of the chosen color':'chosen','two mana in any combination of colors':'any_combo'}[b[1]]
        q.update(kind='bonus',output=output,amount=2 if output in ('fixed_green','any_combo') else 1)
    if q['entry_color']!=(q['output']=='chosen'):raise ValueError('chosen output and as-entry choice must compose')
    return dict(q,clauses=lines)

def spec(d,r):
    from recipes import cost_from_oracle
    profile=pins()['sources'].get((r or {}).get('name'));params=d.get('params');q=mechanism(r)
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not profile or not exact_pin(profile['oracle'],r) or d.get('oracle_text')!=r['oracle_text'] or not isinstance(params,dict) or set(params)!={'schema','mechanism'} or type(params['schema']) is not int or params['schema']!=1 or not exact_pin(params['mechanism'],q):
        raise ValueError('whole independent source, type, cost and mechanism pins required')
    return dict(q,family=FAMILY,csvid=profile['csvid'],source_name=r['name'],mana=cost_from_oracle(r))

def eligible(r,p):
    types=set(r['type_line'].partition(' — ')[0].split());subtypes=set(r['type_line'].partition(' — ')[2].split())
    return 'Creature' in types if p['scope']=='creature' else 'Land' in types and (p['scope']!='Forest' or 'Forest' in subtypes)

def variants(p):
    modes=['value','payment','insufficient_mana','wrong_color','countered','foreign_owned_host','source_destroyed','source_bounced','host_destroyed','host_bounced','host_bounced_pending','bounce_recast','no_legal_host','starts_tapped','manual_tap','repeat_denied','manual_untap','real_next_untap','spend','two_sources','ready_snapshot','pending_snapshot','attached_snapshot','resolved_snapshot','nonmatching_entry','opponent_turn_activation']
    if p['kind']=='bonus':modes+=['foreign_controller','base_only_after_departure']
    else:modes+=['base_ability','foreign_land_legality' if p['scope']=='land' else 'host_grant_availability']
    if p['output'] in ('any_one','any_combo','chosen'):modes+=['color_'+c.lower() for c in COLORS]
    if p['output']=='any_combo':modes+=['mixed_colors']
    if p['scope']=='Forest':modes+=['nonbasic_forest','wrong_land_type_query']
    if p['scope']=='creature':modes+=['fresh_creature','aged_creature','artifact_creature','shroud_host_query']
    return [f'{m}_{o}' for o in (0,1) for m in modes]

def validate_witnesses(oracle,rows):
    data=pins()
    for n,s in data['witnesses'].items():
        r=rows.get(n,{})
        if not exact_pin(s['oracle'],oracle.get(n)) or r.get('kind')!='card' or r.get('csvid')!=s['csvid'] or r.get('handler')!=s['handler'] or r.get('handler') in (None,'NULL','0'):
            raise ValueError('independent witness identity changed: '+n)
        if r['handler']=='card_dummy' and n not in data['honest_static_only_dummy_witnesses']:raise ValueError('unreviewed dummy witness')
    for n,s in data['sources'].items():
        r=rows.get(n,{})
        if not exact_pin(s['oracle'],oracle.get(n)) or r.get('kind')!='card' or r.get('csvid')!=s['csvid'] or r.get('handler')!=s['handler'] or r.get('handler') in (None,'NULL','0','card_dummy'):raise ValueError('implemented primary pin changed')

def _case(name,csvid,d,p,variant,rows,oracle):
    from recipes import card,step,cost_from_oracle
    mode,owner=variant.rsplit('_',1);owner=int(owner);other=1-owner;setup=[]
    foreign_controller=mode in ('foreign_controller','foreign_land_legality','host_grant_availability')
    hp=other if foreign_controller else owner
    target='Grizzly Bears' if p['scope']=='creature' else 'Forest'
    if mode=='artifact_creature':target='Copper Myr'
    if mode=='nonbasic_forest':target='Tropical Island'
    if mode=='wrong_land_type_query':target='Island'
    if mode=='no_legal_host':target='Forest' if p['scope']=='creature' else 'Grizzly Bears'
    if mode=='shroud_host_query':target='Calcite Snapper' if p['scope']=='creature' else 'Forest'
    def add(n,ref,pl,zone='battlefield',tapped=False,foreign=False):
        e=card(n,ref,pl,zone)
        if zone=='battlefield':e['state']=['tapped' if tapped else 'untapped']+(['opponent_owned'] if foreign else [])
        setup.append(e)
    add(name,'source',owner,'hand');add(target,'host',hp,zone='hand' if mode in ('fresh_creature','aged_creature') else 'battlefield',tapped=mode in ('starts_tapped','insufficient_mana','wrong_color'),foreign=mode=='foreign_owned_host')
    for pl in (0,1):
        add('Plains',f'hand_{pl}',pl,'hand');add('Island',f'grave_{pl}',pl,'graveyard');add('Swamp',f'exile_{pl}',pl,'exile')
        for i,n in enumerate(['Forest','Island','Mountain','Swamp','Plains','Forest']):add(n,f'library_{pl}_{i}',pl,'library')
    if mode in ('source_bounced','host_bounced','foreign_owned_host','host_bounced_pending','bounce_recast'):add('Boomerang','bounce',owner,'hand')
    if mode=='countered':add('Counterspell','counter',other,'hand')
    if mode in ('manual_tap','manual_untap'):add('Twiddle','twiddle',other,'hand')
    if mode=='nonmatching_entry':add('Darksteel Relic','nonmatching',owner,'hand')
    if mode=='two_sources':add(name,'second',owner,'hand')
    if mode in ('real_next_untap','spend','manual_untap'):
        for i in range(3):add('Fog',f'spender_{i}',hp,'hand')
    case=dict(card=name,fixture='empty_main',seed=123,setup=setup);w=FreezeLedger(case,p,mode,owner,rows,oracle)
    pools=[[0]*6 for _ in (0,1)];life=[20,20];attached=[];actions=[step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)];current=owner;sick=set()
    chosen=COLORS[mode[-1].upper()] if mode.startswith('color_') else 3
    def cost(n):return [0]*6 if oracle[n]['mana_cost']=='{0}' else cost_from_oracle(oracle[n])
    def funds(pl,v):
        pools[pl]=list(v);actions.extend(step('set_mana',player=pl,color=c,value=a) for c,a in enumerate(v));actions.append(step('refresh'))
    def pay(pl,v):
        pools[pl]=[a-b for a,b in zip(pools[pl],v)]
        if min(pools[pl])<0:raise ValueError('real exact fixture payment missing')
    def draw(pl):
        if not w.zones['library'][pl]:raise ValueError('empty draw-loss outside this model')
        w.zones['hand'][pl].append(w.zones['library'][pl].pop(0))
    def cast(ref,response=False):
        e=w.entities[ref];actions.append(step('cast_response' if response else 'cast',ref=ref,value=1));pay(e['player'],cost(e['name']));w.move(ref,'stack')
        actions.extend([step('assert_stack',value=2 if response else 1),step('assert_card',ref=ref,property='in_hand',value=0)])
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
            both=Counter(w.zones['hand'][pl]+w.zones['graveyard'][pl]);seq += [step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=both[n]) for n in universe]
        for ref,e in w.entities.items():
            if e['zone']!='battlefield':continue
            seq += [step('assert_card',ref=ref,property='in_play',value=1),step('assert_card',ref=ref,property='tapped',value=int(e['tapped']))]
            if 'Creature' in oracle[e['name']]['type_line'].partition(' — ')[0].split():
                seq += [step('assert_card',ref=ref,property=k,value=int(oracle[e['name']][k])) for k in ('power','toughness')]+[step('assert_card',ref=ref,property='damage',value=0)]
        return seq
    def entry(ref='source'):
        if p['entry_color']:actions.append(step('choose_color',player=owner,value=chosen))
        actions.append(step('resolve'));w.move(ref,'battlefield');attached.append(ref)
        if p['draw']:draw(owner)
        life[owner]+=p['life'];actions.extend(receipts())
    def leave(ref='source',destination='graveyard'):
        w.move(ref,destination)
        if ref in attached:attached.remove(ref)
    def helper(ref,target_ref='host',response=False,dialog=None):
        e=w.entities[ref];funds(e['player'],cost(e['name']));actions.append(step('choose_target',player=e['player'],ref=target_ref));cast(ref,response)
        if dialog is not None:actions.append(step('choose_dialog',player=e['player'],value=dialog))
        actions.append(step('resolve_response' if response else 'resolve'));w.move(ref,'graveyard')
    def output_colors():
        return [3]*p['amount'] if p['output']=='fixed_green' else [chosen]*p['amount'] if p['output']!='any_combo' or mode!='mixed_colors' else [2,4]
    def activate(base=False):
        # The Aura is the native representation only when its controller is the
        # real controller of the granted host ability. Cross-controller grants
        # remain explicit query prototypes until a generic bridge is proven.
        grant=p['kind']=='grant' and bool(attached) and not base
        ref='source' if grant else 'host';has_mana_ability=grant or target in ('Forest','Tropical Island','Copper Myr');allowed=has_mana_ability and not w.entities['host']['tapped'] and 'host' not in sick
        actions.extend([step('refresh'),step('can_activate',ref=ref,value=int(allowed))])
        if not allowed:actions.append(step('activate',ref=ref,value=0));actions.extend(receipts());return
        if grant:
            if owner!=hp:raise ValueError('no fabricated actor for a foreign granted ability')
            actions.append(step('choose_color',player=hp,value=chosen))
        elif p['kind']=='bonus' and attached:
            if target=='Tropical Island':actions.append(step('choose_color',player=hp,value=3))
            for _ in attached:
                if p['output']=='any_one':actions.append(step('choose_color',player=hp,value=chosen))
                elif p['output']=='any_combo':actions.extend(step('choose_color',player=hp,value=c) for c in output_colors())
        actions.append(step('activate',ref=ref,value=1));w.entities['host']['tapped']=True
        if grant:pools[hp][chosen]+=p['amount']
        else:
            pools[hp][3]+=1
            if p['kind']=='bonus':
                for _ in attached:
                    for c in output_colors():pools[hp][c]+=1
        actions.extend(receipts())
    def spend_all_green():
        if any(v for c,v in enumerate(pools[hp]) if c!=3):raise ValueError('bounded real Fog spender only drains green mana')
        count=pools[hp][3]
        for i in range(count):cast(f'spender_{i}');actions.append(step('resolve'));w.move(f'spender_{i}','graveyard')
        actions.extend(receipts())
    funds(0,[0]*6);funds(1,[0]*6)
    if mode in ('fresh_creature','aged_creature'):
        funds(hp,cost(target));cast('host');actions.append(step('resolve'));w.move('host','battlefield');sick.add('host')
        if mode=='aged_creature':
            actions += [step('advance_turn'),step('advance_turn')];sick.clear()
    pool=list(p['mana'])
    if mode=='insufficient_mana':pool=[0]*6
    if mode=='wrong_color':pool[1]+=pool[3];pool[3]=0
    funds(owner,pool)
    if mode=='countered':funds(other,cost('Counterspell'))
    ready=len(actions)
    query=mode in ('foreign_land_legality','host_grant_availability','shroud_host_query','wrong_land_type_query')
    denied=mode in ('insufficient_mana','wrong_color','no_legal_host')
    if query:
        actions.append(step('can_cast',ref='source',value=0 if mode=='wrong_land_type_query' or mode=='shroud_host_query' and p['scope']=='creature' else 1))
        # Foreign land legality is deliberately only a legal raw target query:
        # never send an invalid target to a handler known to reject the host.
        if mode!='host_grant_availability':actions.extend(receipts())
    if denied:
        actions.append(step('cast',ref='source',value=0));actions.extend(receipts())
    elif not query or mode=='host_grant_availability':
        actions.append(step('choose_target',player=owner,ref='host'));cast('source');pending=len(actions)
        if mode=='countered':
            actions.append(step('announce_spell',ref='source'));cast('counter',True);actions.append(step('resolve_response'));w.move('counter','graveyard');leave();actions.extend(receipts())
        elif mode=='host_bounced_pending':
            actions.append(step('announce_spell',ref='source'));helper('bounce',response=True);w.move('host','hand');leave();actions.extend(receipts())
        else:entry()
    attached_cut=len(actions)
    if mode=='host_grant_availability':
        actions += [step('refresh'),step('can_activate_granted_mana',ref='host',other='source',value=1),step('capture')]
        pair_tail=[step('choose_color',player=hp,value=chosen),step('activate_granted_mana',ref='host',other='source',value=1)]
        pools[hp][chosen]+=p['amount'];w.entities['host']['tapped']=True
        pair_tail += receipts()+[step('can_activate_granted_mana',ref='host',other='source',value=0),step('activate_granted_mana',ref='host',other='source',value=0)]+receipts()
        actions += pair_tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(pair_tail)+[step('assert_recorded_state')]
        actions.append(step('destroy',ref='source'));leave();actions += [step('resolve'),step('can_activate_granted_mana',ref='host',other='source',value=0),step('activate_granted_mana',ref='host',other='source',value=0)]+receipts()
    if not denied and not query and mode not in ('countered','host_bounced_pending'):
        if mode in ('source_destroyed','base_only_after_departure'):
            actions.append(step('destroy',ref='source'));leave();actions.append(step('resolve'));actions.extend(receipts())
        if mode in ('source_bounced','bounce_recast'):
            helper('bounce','source');leave(destination='hand');actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'));actions.extend(receipts())
            if mode=='bounce_recast':funds(owner,p['mana']);actions.append(step('choose_target',player=owner,ref='host'));cast('source');entry()
        if mode=='host_destroyed':actions.append(step('destroy',ref='host'));w.move('host','graveyard');leave();actions.append(step('resolve'));actions.extend(receipts())
        if mode in ('host_bounced','foreign_owned_host'):
            helper('bounce');w.move('host','hand');leave();actions.extend(receipts())
        if mode=='two_sources':funds(owner,p['mana']);actions.append(step('choose_target',player=owner,ref='host'));cast('second');entry('second')
        if mode=='nonmatching_entry':funds(owner,[0]*6);cast('nonmatching');actions.append(step('resolve'));w.move('nonmatching','battlefield');actions.extend(receipts())
        if mode=='manual_tap':helper('twiddle',dialog=0);w.entities['host']['tapped']=True;actions.extend(receipts())
        elif mode not in ('payment','host_destroyed','host_bounced','foreign_owned_host'):
            if mode=='opponent_turn_activation':actions.append(step('turn',player=other,value=0));current=other
            activate(base=mode=='base_ability')
            if mode=='repeat_denied':actions += [step('can_activate',ref='source' if p['kind']=='grant' else 'host',value=0)]
            if mode in ('spend','manual_untap','real_next_untap'):spend_all_green()
            if mode=='manual_untap':helper('twiddle',dialog=1);w.entities['host']['tapped']=False;actions.extend(receipts());activate()
            if mode=='real_next_untap':
                if any(any(v) for v in pools):raise ValueError('actual output must be spent before guarded untap')
                for _ in range(2):
                    actions.append(step('advance_turn'));current=1-current;actions.append(step('untap_step',player=current))
                    for e in w.entities.values():
                        if e['zone']=='battlefield' and e['player']==current:e['tapped']=False
                actions += [step('turn',player=hp,value=0)];activate()
    if mode in ('ready_snapshot','pending_snapshot','attached_snapshot'):
        cut=ready if mode=='ready_snapshot' else pending if mode=='pending_snapshot' else attached_cut;tail=copy.deepcopy(actions[cut:]);actions=actions[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
    if mode=='resolved_snapshot':actions += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+receipts()+[step('assert_recorded_state')]
    case.update(name=f'AuraManaRecipe_{csvid}_{variant}',steps=actions,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],covered_clauses=p['clauses'],untested_components=[
        'production UI ability discovery is not observed or repaired; pair uses the actual native provider without actor/payer override' if mode=='host_grant_availability' else 'cross-controller Aura-granted ability execution: only faithful normal-host availability prototype until generic delegation capability is measured',
        'no generic attachment observer; selected-host actions and source/host zone lifecycle compose witness',
        'no action interval between as-entry choice and automatic ETB processing',
        'empty-library mandatory draw loss, replacement effects, arbitrary source/control/type change, phasing, humiliation, multiplayer']))
    return case

def expand(name,csvid,d,p,selected,rows,oracle):
    if csvid!=p['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(p)):raise ValueError('explicit supported source variants required')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,d,p,v,rows,oracle) for v in selected]
