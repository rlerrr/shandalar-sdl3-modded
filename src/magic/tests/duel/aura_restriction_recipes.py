"""Whole-Oracle attached restrictions, actual actions and permission release."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from entry_aura_freeze_recipes import FreezeLedger
from library_search_recipes import wave6_exact as exact_pin

FAMILY='aura_restriction'
DATA=Path(__file__).parent/'data/aura_restriction_wave8_profiles.json'
PALETTE=['Elvish Mystic','Copper Myr','Ornithopter','Sol Ring','Jayemdae Tome','Darksteel Citadel','Forest','Seal of Cleansing','Dryad Arbor']


def pins():return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(r):
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r.get('type_line')!='Enchantment — Aura':
        raise ValueError('restriction requires one whole normal actual Aura')
    lines=r['oracle_text'].splitlines();flash=False
    if lines and re.fullmatch(r'Flash(?: \(.*\))?',lines[0]):flash=True;lines=lines[1:]
    match=re.fullmatch(r'Enchant (artifact or creature|artifact|creature)(?: \(.*\))?',lines[0]) if lines else None
    if not match:raise ValueError('complete ordinary enchant predicate required')
    scope=match[1].replace(' ','_');noun='permanent' if scope=='artifact_or_creature' else scope
    q=dict(scope=scope,flash=flash,draw=0,no_attack=False,no_block=False,no_activate=False,no_untap=False)
    for line in lines[1:]:
        if line=='When this Aura enters, draw a card.':
            if q['draw']:raise ValueError('duplicate entry clause')
            q['draw']=1
        elif line==f"Enchanted {noun} can't attack or block, and its activated abilities can't be activated.":q.update(no_attack=True,no_block=True,no_activate=True)
        elif line==f"Enchanted {noun} doesn't untap during its controller's untap step and its activated abilities can't be activated.":q.update(no_untap=True,no_activate=True)
        elif line==f"Enchanted {noun}'s activated abilities can't be activated.":q['no_activate']=True
        elif line==f"Enchanted {noun} doesn't untap during its controller's untap step.":q['no_untap']=True
        else:raise ValueError('unmodeled complete restriction clause: '+line)
    if not any(q[k] for k in ('no_attack','no_block','no_activate','no_untap')) or set(r.get('keywords',[]))-{'Enchant','Flash'} or ('Flash' in r.get('keywords',[]))!=flash:
        raise ValueError('whole static restriction/Flash grammar required')
    return dict(q,clauses=r['oracle_text'].splitlines())


def spec(d,r):
    from recipes import cost_from_oracle
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not isinstance(d.get('variants'),list) or not d['variants'] or len(set(d['variants']))!=len(d['variants']):
        raise ValueError('explicit distinct version2 restriction variants required')
    params=d.get('params');parsed=mechanism(r);profile=pins()['sources'].get(r['name'])
    if not isinstance(params,dict) or set(params)!={'schema','mechanism'} or type(params['schema']) is not int or params['schema']!=1 or not profile or not exact_pin(profile['oracle'],r) or d.get('oracle_text')!=r['oracle_text'] or not exact_pin(params['mechanism'],parsed):
        raise ValueError('whole source/cost/type/restriction pin changed')
    return dict(parsed,family=FAMILY,csvid=profile['csvid'],source_name=r['name'],mana=cost_from_oracle(r))


def eligible(r,p):
    types=set(r['type_line'].partition(' — ')[0].split())
    return bool(types & ({'Creature','Artifact'} if p['scope']=='artifact_or_creature' else {p['scope'].capitalize()}))


def variants(p):
    modes=['attached','payment','insufficient_mana','wrong_color','countered','own_host','foreign_owned_host',
           'source_destroyed','source_bounced','host_destroyed','host_bounced','bounce_recast','cleanup','first_second_untap',
           'ready_snapshot','pending_snapshot','attached_snapshot','resolved_snapshot','nonmatching_entry','no_legal_host','shroud_host','host_bounced_pending',
           'attack','block','attack_after_source_departure','block_after_source_departure','mana_activation','nonmana_activation',
           'mana_after_source_departure','nonmana_after_source_departure','starts_tapped','starts_untapped','manual_untap','two_sources']
    if p['no_untap']:modes.append('two_freeze_sources')
    if p['flash']:modes.append('offturn')
    if p['draw']:modes.append('exact_library')
    modes += [f'host_{i}' for i in range(len(PALETTE))]
    return [f'{m}_{o}' for o in (0,1) for m in modes]


def validate_witnesses(oracle,rows):
    for n,r in pins()['witnesses'].items():
        if not exact_pin(r,oracle.get(n)) or n not in rows or rows[n]['kind']!='card' or rows[n]['handler'] in (None,'NULL') or (rows[n]['handler']=='card_dummy' and not re.fullmatch(r'(?:Flying|Reach|Defender)?(?: \(.*\))?',r['oracle_text'])):
            raise ValueError('restriction witness pin/implementation changed: '+n)
    for n,p in pins()['sources'].items():
        if not exact_pin(p['oracle'],oracle.get(n)) or rows[n]['csvid']!=p['csvid'] or rows[n]['kind']!='card' or rows[n]['handler'] in (None,'NULL','card_dummy'):
            raise ValueError('restriction source identity/implementation changed: '+n)


def _case(name,csvid,d,p,variant,rows,oracle):
    from recipes import card,step,cost_from_oracle
    mode,owner=variant.rsplit('_',1);owner=int(owner);setup=[]
    mana_mode=mode.startswith('mana_');nonmana_mode=mode.startswith('nonmana_') or mode=='two_sources'
    target='Copper Myr' if p['scope']=='artifact' else 'Elvish Mystic'
    if nonmana_mode:target='Jayemdae Tome' if p['scope']=='artifact' else 'Dragon Whelp'
    if mode.startswith('host_') and mode.split('_')[1].isdigit():target=PALETTE[int(mode.split('_')[1])]
    if mode=='no_legal_host':target='Forest'
    if mode=='shroud_host':target='Calcite Snapper'
    host_player=owner if mode in ('own_host','foreign_owned_host') else 1-owner
    valid=mode!='shroud_host' and eligible(oracle[target],p)
    host_tapped=mode in ('starts_tapped','first_second_untap','manual_untap','cleanup','two_freeze_sources')
    def add(n,ref,pl,zone='battlefield',tapped=False,foreign=False):
        item=card(n,ref,pl,zone)
        if zone=='battlefield':item['state']=['tapped' if tapped else 'untapped']+(['opponent_owned'] if foreign else [])
        setup.append(item)
    add(name,'source',owner,'hand');add(target,'host',host_player,tapped=host_tapped,foreign=mode=='foreign_owned_host')
    if valid:add('Grizzly Bears','unselected',host_player,tapped=host_tapped)
    for pl in (0,1):
        add('Seal of Cleansing',f'control_{pl}',pl);add('Plains',f'hand_{pl}',pl,'hand');add('Mountain',f'grave_{pl}',pl,'graveyard');add('Island',f'exile_{pl}',pl,'exile')
        deck=['Forest'] if mode=='exact_library' and pl==owner else ['Forest','Swamp','Hill Giant','Island']
        for i,n in enumerate(deck):add(n,f'library_{pl}_{i}',pl,'library')
    if mode in ('source_bounced','host_bounced','foreign_owned_host','bounce_recast','host_bounced_pending'):add('Boomerang','bounce',owner,'hand')
    if mode=='countered':add('Counterspell','counter',1-owner,'hand')
    if mode=='nonmatching_entry':add('Darksteel Relic','nonmatching',owner,'hand')
    if mode=='manual_untap':add('Twiddle','untapper',owner,'hand');add('Twiddle','retapper',owner,'hand')
    if mana_mode or mode.startswith('nonmana_'):
        add('Elvish Mystic','activation_control',host_player)
        add('Fog','control_spender',host_player,'hand')
    if mana_mode:add('Fog','host_spender',host_player,'hand')
    if mode in ('two_sources','two_freeze_sources'):add(name,'second',owner,'hand')
    if mode.startswith('block'):
        add('Horned Turtle','attacker_0',owner);add('Horned Turtle','attacker_1',owner)
    case=dict(card=name,fixture='empty_main',seed=123,setup=setup);w=FreezeLedger(case,p,mode,owner,rows,oracle)
    actions=[step('turn',player=1-owner if mode=='offturn' else owner,value=0)]
    if mode=='manual_untap':actions.append(step('present_ai_dialogs',value=1))
    pools=[[0]*6,[0]*6];life=[20,20];current=1-owner if mode=='offturn' else owner;attached=set();damage={}
    def cost(n):return [0]*6 if oracle[n]['mana_cost']=='{0}' else cost_from_oracle(oracle[n])
    def funds(pl,vector):pools[pl]=list(vector);actions.extend(step('set_mana',player=pl,color=c,value=n) for c,n in enumerate(vector))
    def pay(pl,vector):
        pools[pl]=[a-b for a,b in zip(pools[pl],vector)]
        if min(pools[pl])<0:raise ValueError('restriction fixture real payment insufficient')
    def draw(pl):
        if not w.zones['library'][pl]:raise ValueError('empty mandatory draw/loss is outside this bounded model')
        w.zones['hand'][pl].append(w.zones['library'][pl].pop(0))
    def cast(ref,response=False):
        e=w.entities[ref];actions.append(step('cast_response' if response else 'cast',ref=ref,value=1));pay(e['player'],cost(e['name']));w.move(ref,'stack')
        actions.extend([step('assert_stack',value=2 if response else 1),step('assert_card',ref=ref,property='in_hand',value=0)])
    def receipts():
        seq=[step('assert_choices_consumed'),step('assert_stack',value=0),step('assert_forced_target_choices',value=0),step('assert_forced_zone_choices',value=0)]
        universe=sorted({e['name'] for e in w.entities.values()})
        for pl in (0,1):
            seq += [step('assert_life',player=pl,value=life[pl]),step('assert_shuffle_count',player=pl,value=0)]
            if mode!='cleanup':seq += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(pools[pl])]
            for zone in ('battlefield','hand','graveyard','exile','library'):
                seq.append(step('assert_zone',player=pl,zone=zone,value=w.count(pl,zone)))
                for n in universe:
                    count=sum(e['zone']=='battlefield' and e['player']==pl and e['name']==n for e in w.entities.values()) if zone=='battlefield' else w.zones[zone][pl].count(n)
                    seq.append(step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[n]['csvid'],value=count))
                if zone=='library':seq += [step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(w.zones[zone][pl])]
            totals=Counter(w.zones['hand'][pl]+w.zones['graveyard'][pl]);seq += [step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=totals[n]) for n in universe]
        for ref,e in w.entities.items():
            if e['zone']!='battlefield':continue
            seq += [step('assert_card',ref=ref,property='in_play',value=1),step('assert_card',ref=ref,property='tapped',value=int(e['tapped']))]
            if 'Creature' in oracle[e['name']]['type_line'].partition(' — ')[0].split():
                seq += [step('assert_card',ref=ref,property='power',value=int(oracle[e['name']]['power'])+e['boost']),step('assert_card',ref=ref,property='toughness',value=int(oracle[e['name']]['toughness']))]
                seq += [step('assert_card',ref=ref,property='damage',value=damage.get(ref,0))]+[step('assert_card',ref=ref,property=k,value=0) for k in ('plus1_counters','minus1_counters')]
        return seq
    def enter(ref='source'):
        w.move(ref,'battlefield');attached.add(ref)
        if p['draw']:draw(owner)
        actions.append(step('resolve'));actions.extend(receipts())
    def leave(ref='source',destination='graveyard'):
        w.move(ref,destination);attached.discard(ref)
    def destroy_source(ref='source'):
        actions.append(step('destroy',ref=ref));leave(ref);actions.append(step('resolve'));actions.extend(receipts())
    def advance_untap():
        nonlocal current,pools
        if any(any(vector) for vector in pools):raise ValueError('spend actual pools before bounded untap API')
        actions.append(step('advance_turn'));current=1-current;pools=[[0]*6,[0]*6];damage.clear()
        for e in w.entities.values():e['boost']=0
        actions.append(step('untap_step',player=current))
        for ref,e in w.entities.items():
            if e['zone']=='battlefield' and e['player']==current and not(ref=='host' and attached and p['no_untap']):e['tapped']=False
        actions.extend(receipts())
    def host_turn():
        advance_untap()
        if current!=host_player:advance_untap()
    def helper(ref,target_ref='host',response=False,dialog=None):
        e=w.entities[ref];funds(e['player'],cost(e['name']));actions.append(step('choose_target',player=e['player'],ref=target_ref));cast(ref,response)
        if dialog is not None:actions.append(step('choose_dialog',player=e['player'],value=dialog))
        actions.append(step('resolve_response' if response else 'resolve'));w.move(ref,'graveyard')
    def spend_green(ref):cast(ref);actions.append(step('resolve'));w.move(ref,'graveyard');actions.extend(receipts())
    def activate_host():
        allowed=not(attached and p['no_activate']);fee=[4,0,0,0,0,0] if target=='Jayemdae Tome' else [0,0,0,0,1,0] if target=='Dragon Whelp' else [0]*6
        funds(host_player,fee);actions.append(step('activate',ref='host',value=int(allowed)))
        actions.append(step('assert_stack',value=int(allowed and nonmana_mode)))
        if allowed:
            if mana_mode:pools[host_player][3]+=1;w.entities['host']['tapped']=True
            elif target=='Jayemdae Tome':pay(host_player,fee);w.entities['host']['tapped']=True;draw(host_player)
            else:pay(host_player,fee);w.entities['host']['boost']+=1
            actions.append(step('resolve'))
        actions.extend(receipts())
        if mana_mode and allowed:spend_green('host_spender')
    funds(0,[0]*6);funds(1,[0]*6);pool=list(p['mana'])
    if mode=='insufficient_mana':pool=[0]*6
    if mode=='wrong_color':
        c=next(i for i in range(1,6) if pool[i]);pool[1 if c!=1 else 2]+=pool[c];pool[c]=0
    funds(owner,pool)
    if mode=='countered':funds(1-owner,cost('Counterspell'))
    actions.append(step('refresh'));ready=len(actions)
    if not valid or mode in ('insufficient_mana','wrong_color'):
        actions.append(step('cast',ref='source',value=0));pending=len(actions);actions.extend(receipts())
    else:
        actions.append(step('choose_target',player=owner,ref='host'));cast('source');actions.append(step('assert_zone',player=owner,zone='hand',value=w.count(owner,'hand')+1));pending=len(actions)
        if mode=='countered':
            actions.append(step('announce_spell',ref='source'));cast('counter',True);actions.append(step('resolve_response'));w.move('source','graveyard');w.move('counter','graveyard');actions.extend(receipts())
        elif mode=='host_bounced_pending':
            actions.append(step('announce_spell',ref='source'));helper('bounce',response=True);w.move('host','hand');leave();actions.append(step('resolve'));actions.extend(receipts())
        else:enter()
    attached_cut=len(actions)
    if mode in ('source_destroyed','attached_snapshot','attack_after_source_departure','block_after_source_departure','mana_after_source_departure','nonmana_after_source_departure'):destroy_source()
    if mode in ('source_bounced','bounce_recast'):
        helper('bounce','source');leave(destination='hand');actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'));actions.extend(receipts())
        if mode=='bounce_recast':funds(owner,p['mana']);actions.append(step('choose_target',player=owner,ref='host'));cast('source');enter()
    if mode=='host_destroyed':
        actions.append(step('destroy',ref='host'));w.move('host','graveyard');leave();actions.append(step('resolve'));actions.extend(receipts())
    if mode in ('host_bounced','foreign_owned_host'):helper('bounce');w.move('host','hand');leave();actions.extend(receipts())
    if mode=='cleanup':actions.append(step('cleanup'));actions.extend(receipts());host_turn()
    if mode=='first_second_untap':host_turn();advance_untap();host_turn()
    if mode=='manual_untap':
        helper('untapper',dialog=1);w.entities['host']['tapped']=False;actions.extend(receipts());helper('retapper',dialog=0);w.entities['host']['tapped']=True;actions.extend(receipts());host_turn()
    if mode=='nonmatching_entry':funds(owner,[0]*6);cast('nonmatching');actions.append(step('resolve'));w.move('nonmatching','battlefield');actions.extend(receipts())
    if mode in ('two_sources','two_freeze_sources'):
        funds(owner,p['mana']);actions.append(step('choose_target',player=owner,ref='host'));cast('second');enter('second');destroy_source()
        if mode=='two_freeze_sources':
            host_turn();actions.append(step('turn',player=current,value=0));destroy_source('second');host_turn()
        else:
            activate_host();destroy_source('second');activate_host()
    if mana_mode or mode.startswith('nonmana_'):
        activate_host()
        # Independent, unselected same-controller mana ability remains legal.
        actions += [step('activate',ref='activation_control',value=1),step('assert_stack',value=0)];pools[host_player][3]+=1;w.entities['activation_control']['tapped']=True
        actions.extend(receipts());spend_green('control_spender')
    if mode.startswith('attack'):
        host_turn();actions += [step('turn',player=current,value=0),step('begin_attackers')]
        allowed=not(attached and p['no_attack']);actions += [step('attack',ref='host',value=int(allowed)),step('attack',ref='unselected',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage')]
        w.entities['host']['tapped']=allowed;w.entities['unselected']['tapped']=True;life[1-host_player]-=2+int(allowed)
        actions.extend(receipts());actions.append(step('end_combat'));actions.extend(receipts())
    if mode.startswith('block'):
        actions += [step('turn',player=current,value=0),step('begin_attackers'),step('attack',ref='attacker_0',value=1),step('attack',ref='attacker_1',value=1),step('finish_attackers'),step('begin_blockers')]
        allowed=not(attached and p['no_block']);actions += [step('block',ref='host',other='attacker_0',value=int(allowed)),step('block',ref='unselected',other='attacker_1',value=1),step('advance_combat'),step('assert_card',ref='attacker_0',property='blocked',value=int(allowed)),step('assert_card',ref='attacker_1',property='blocked',value=1),step('combat_damage')]
        w.entities['attacker_0']['tapped']=w.entities['attacker_1']['tapped']=True;damage.update(attacker_0=int(allowed),attacker_1=2,unselected=1)
        if allowed:
            w.move('host','graveyard')
            if attached:leave()
        else:life[host_player]-=1
        actions.append(step('resolve'));actions.extend(receipts());actions.append(step('end_combat'));actions.extend(receipts())
    if mode in ('ready_snapshot','pending_snapshot','attached_snapshot','resolved_snapshot'):
        # Zero-cost native mana witnesses make this a permission observation,
        # independent of the raw query's separate affordability limitations.
        actions.append(step('can_activate',ref='host',value=int(not(attached and p['no_activate']))))
    if mode in ('ready_snapshot','pending_snapshot','attached_snapshot'):
        cut=ready if mode=='ready_snapshot' else pending if mode=='pending_snapshot' else attached_cut;tail=copy.deepcopy(actions[cut:]);actions=actions[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
    if mode=='resolved_snapshot':actions += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('can_activate',ref='host',value=int(not(attached and p['no_activate']))),step('assert_recorded_state')]
    case.update(name=f'AuraRestrictionRecipe_{csvid}_{variant}',steps=actions,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],covered_clauses=p['clauses'],
        untested_components=['no generic attachment identity observer: actual selected host, actions and departure/zone receipts compose witness','source departure between entry and automatic ETB resolution: unavailable priority boundary','empty mandatory draw loss/replacement effects','arbitrary Aura movement/control/humiliation/phasing/tokens/multiface/multiplayer']))
    return case


def expand(name,csvid,d,p,selected,rows,oracle):
    if csvid!=p['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(p)):
        raise ValueError('restriction source identity/variants changed')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,d,p,v,rows,oracle) for v in selected]
