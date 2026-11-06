"""Pinned discard pipelines and caster-chosen hand predicates.

Printed semantic profiles are independent of handlers. Ordinary discard/draw
pipelines reuse hand_exchange; coercion checks exact named hand/zone receipts.
"""
import copy
import json
from collections import Counter
from pathlib import Path
import hand_exchange_recipes as ordinary
from grant_recipes import GRAVE, EXILE, LIBRARY

PINS=json.loads((Path(__file__).parent/'data/discard_spell_extension_witnesses.json').read_text(encoding='utf8'))
PROFILES=PINS['profiles']
HAND=('Grizzly Bears','Lightning Bolt','Sol Ring','Divination','Plains','Craw Wurm','Isamaru, Hound of Konda')


def same(a,b):
    if type(a) is not type(b):return False
    if isinstance(a,dict):return set(a)==set(b) and all(same(a[k],b[k]) for k in a)
    if isinstance(a,list):return len(a)==len(b) and all(same(x,y) for x,y in zip(a,b))
    return a==b


def eligible(r,scope):
    kinds=set(r['type_line'].partition(' — ')[0].split())
    return {'any':True,'nonland':'Land' not in kinds,'creature':'Creature' in kinds,
            'creature_or_planeswalker':bool(kinds&{'Creature','Planeswalker'}),
            'artifact_or_creature':bool(kinds&{'Artifact','Creature'}),
            'nonland_mv3':'Land' not in kinds and r['cmc']<=3,
            'nonlegendary_nonland':'Land' not in kinds and 'Legendary' not in kinds}[scope]


def pipeline(p,r):
    ops=copy.deepcopy(p['operations'])
    q=dict(mode='spell',recipient=p['recipient'],operations=ops,cost=dict(mana='',tap=False,sacrifice=False,pay_life=0,discard=0),keywords=[],mana_ability=False)
    virtual=copy.deepcopy(r);virtual['oracle_text']=sorted(ordinary.effect_texts(q))[0];virtual['keywords']=[]
    d=dict(family='hand_exchange',version=2,params=q,oracle_text=virtual['oracle_text'],variants=['effect_0'])
    return d,ordinary.spec(d,virtual)


def spec(d,r):
    from recipes import cost_from_oracle
    profile=PROFILES.get(r.get('name')) if isinstance(r,dict) else None
    if set(d)-{'family','version','params','oracle_text','variants','target_choices'} or not profile or d.get('family')!='discard_spell_extension' or d.get('version')!=2 or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or any(r.get(k)!=v for k,v in profile['source'].items()) or d.get('oracle_text')!=r.get('oracle_text') or not same(d.get('params'),profile['params']):raise ValueError('discard extension requires complete source pins and explicit reviewed semantic parameters')
    p=copy.deepcopy(profile['params'])
    if p['kind']=='pipeline':pipeline(p,r)
    elif p['kind']!='chosen' or p['scope'] not in ('any','nonland','creature','creature_or_planeswalker','artifact_or_creature','nonland_mv3','nonlegendary_nonland') or type(p['life_loss']) is not int or not 0<=p['life_loss']<=2 or p['recipient'] not in ('any','opponent'):raise ValueError('unreviewed chosen-discard predicate or receipt')
    choices=d.get('target_choices',1)
    if not (type(choices) is int and choices in (0,1,2) or isinstance(choices,dict) and set(choices)=={'0','1'} and all(type(n) is int and n in (0,1,2) for n in choices.values())):raise ValueError('explicit per-controller target presentation count required')
    p.update(family='discard_spell_extension',mana=cost_from_oracle(r),instant='Instant' in r['type_line'],source=copy.deepcopy(profile['source']),csvid=profile['csvid'])
    selected=d.get('variants')
    if not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or len(selected)!=len(set(selected)) or set(selected)-set(variants(p)):raise ValueError('explicit distinct supported discard variants required')
    return p


def variants(p):
    if p['kind']=='pipeline':
        _,q=pipeline(p,p['source'])
        return ordinary.variants(q)
    modes=['effect','alternate_choice','payment','insufficient_mana','wrong_color','empty_hand','no_eligible','one_card','duplicates','bare_zones','countered','target_shrouded_pending','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot']
    if p['scope']=='any':modes.remove('no_eligible')
    if p['scope']=='any':modes.append('land_choice')
    if p['scope']=='nonland_mv3':modes.append('value_boundary')
    if p['scope']=='artifact_or_creature':modes.append('artifact_choice')
    if p['recipient']=='any':modes.append('own_target')
    if p['scope']=='creature_or_planeswalker':modes.append('planeswalker')
    return [f'{v}_{pl}' for pl in (0,1) for v in modes]


def validate_witnesses(o,rows):
    ordinary.validate_witnesses(o,rows)
    for n,pin in PINS['cards'].items():
        if n not in rows or any(o.get(n,{}).get(k)!=v for k,v in pin.items()):raise ValueError('discard extension witness changed: '+n)


def expand(name,csvid,d,p,selected,rows,o):
    if csvid!=p['csvid'] or name!=p['source']['name']:raise ValueError('discard source registry identity changed')
    if set(selected)-set(variants(p)) or len(selected)!=len(set(selected)):raise ValueError('unknown or duplicate discard variant')
    if p['kind']=='pipeline':
        output=[]
        for v in selected:
            prefix,owner=v.rsplit('_',1);component=copy.deepcopy(p)
            if p.get('context')=='main_phase' and prefix in ('opposing_turn','combat'):component['operations']=copy.deepcopy(p['outside_main_operations'])
            base,qs=pipeline(component,p['source']);choices=d.get('target_choices',1)
            base['target_choices']=choices[owner] if isinstance(choices,dict) else choices
            cs=ordinary.expand(name,csvid,base,qs,[v],rows,o)
            for c in cs:
                context=p.get('context','ordinary')
                if context=='no_lands' and any(x['zone']=='battlefield' and 'Land' in o[x['name']]['type_line'] for x in c['setup']):raise ValueError('discard component requires no battlefield lands')
                if context=='no_other_copies' and any(x['zone']=='graveyard' and x['name']==name for x in c['setup']):raise ValueError('base named-card discard requires no earlier copies in either graveyard')
                if context=='no_attacks' and any(x['op']=='attack' for x in c['steps']):raise ValueError('discard component requires no attacks this turn')
                if context=='no_conspire':
                    colors=set(p['source']['colors']);matches=[x for x in c['setup'] if x['zone']=='battlefield' and x['player']==int(owner) and 'Creature' in o[x['name']]['type_line'] and set(o[x['name']]['colors'])&colors]
                    if len(matches)>=2:raise ValueError('base conspire component requires insufficient matching creatures')
                if context=='no_deaths':c['steps'].insert(0,dict(op='reset_death_history'))
                # Optional land untapping and the land-return/discard menu are
                # real resolution choices even when their land branch is empty.
                # Queue presentation inputs at each successful resolution, also
                # after snapshot restore; countered spells never request them.
                inputs=p.get('resolution_inputs',{}).get(owner,[])
                if inputs and prefix!='countered_cast':
                    seq=[]
                    for action in c['steps']:
                        if action['op']=='resolve':
                            seq.extend(dict(op='choose_cancel_target',player=int(owner)) if i['kind']=='cancel_target' else dict(op='choose_dialog',player=int(owner),value=i['value']) for i in inputs)
                        seq.append(action)
                    c['steps']=seq
                c['name']=c['name'].replace('HandExchangeRecipe','DiscardExtensionRecipe')
                c['_recipe']=dict(family='discard_spell_extension',version=2,variant=v,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps'])
            output+=cs
        return output
    return chosen_expand(name,csvid,d,p,selected,rows,o)


def chosen_expand(name,csvid,d,p,selected,rows,o):
    from recipes import card,step,cost_from_oracle
    out=[]
    for variant in selected:
        v,owner=variant.rsplit('_',1);owner=int(owner);target=owner if v=='own_target' else 1-owner
        hand=[] if v=='empty_hand' else ['Plains','Forest','Island'] if v=='no_eligible' else ['Grizzly Bears'] if v=='one_card' else ['Grizzly Bears']*3 if v=='duplicates' else list(HAND)
        if v=='planeswalker':hand=['Jace Beleren','Plains','Island']
        zones={z:[list(ns),list(ns)] for z,ns in (('hand',hand),('graveyard',[] if v=='bare_zones' else GRAVE),('exile',[] if v=='bare_zones' else EXILE),('library',LIBRARY))}
        setup=[];actions=[];mana=[[20]*6,[20]*6];life=[20,20]
        for z,players in zones.items():
            for pl,names in enumerate(players):setup.extend(card(n,f'p{pl}_{z}_{i}',pl,z) for i,n in enumerate(names))
        def add(n,ref,pl):setup.append(card(n,ref,pl,'hand'));zones['hand'][pl].append(n)
        def move_spell(n,pl):zones['hand'][pl].remove(n);zones['graveyard'][pl].append(n)
        def pay(cost,pl):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
        add(name,'source',owner)
        if v=='countered':add('Counterspell','counter',0)
        if v=='target_shrouded_pending':add('Gilded Light','shield',target)
        if v=='payment':mana[owner]=list(p['mana'])
        if v=='insufficient_mana':mana[owner]=[0]*6
        if v=='wrong_color':
            mana[owner]=list(p['mana']);color=next(i for i in range(1,6) if mana[owner][i]);mana[owner][1 if color!=1 else 2]+=mana[owner][color];mana[owner][color]=0
        pre=[step('turn',player=owner,value=0),step('observe_search_filters',value=1)]
        for pl in (0,1):pre.extend(step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
        pre.append(step('refresh'));cut=0;offer_checks=[]
        if v in ('insufficient_mana','wrong_color'):actions.append(step('cast',ref='source',value=0))
        else:
            actions.append(step('require_can_cast',ref='source',value=1))
            choices=d.get('target_choices',1);choices=choices[str(owner)] if isinstance(choices,dict) else choices
            actions.extend(step('choose_player',player=owner,value=target) for _ in range(choices))
            if v=='countered':actions.append(step('choose_priority',ref='counter'))
            actions.append(step('cast',ref='source',value=0 if v=='countered' else 1));pay(p['mana'],owner)
            if v=='countered':move_spell('Counterspell',0);pay(cost_from_oracle(o['Counterspell']),0)
            else:
                actions.append(step('assert_stack',value=1))
                actions.extend(step('assert_mana',player=owner,color=i,value=n) for i,n in enumerate(mana[owner]))
                # The pending spell remains physically counted in hand.
                actions.append(step('assert_zone',player=owner,zone='hand',value=len(zones['hand'][owner])))
            cut=len(actions)
            move_spell(name,owner)
            if v=='target_shrouded_pending':actions.extend([step('cast',ref='shield',value=1),step('assert_stack',value=2)]);move_spell('Gilded Light',target);pay(cost_from_oracle(o['Gilded Light']),target)
            if v not in ('countered','target_shrouded_pending'):
                offered=list(zones['hand'][target]);valid=[n for n in offered if eligible(o[n],p['scope'])]
                if offered:
                    if valid:
                        chosen={'land_choice':'Plains','value_boundary':'Divination','artifact_choice':'Sol Ring'}.get(v,valid[-1] if v=='alternate_choice' else valid[0])
                        actions.append(step('choose_zone_card',player=owner,value=rows[chosen]['csvid']));zones['hand'][target].remove(chosen);zones['graveyard'][target].append(chosen)
                    else:actions.append(step('choose_optional_zone',player=owner,value=rows[offered[0]]['csvid']))
                    offer_checks=[step('assert_offered_count',value=len(offered))]+[step('assert_offered_legal_card',index=rows[n]['csvid'],value=int(eligible(o[n],p['scope']))) for n in sorted(set(offered))]
                life[owner]-=p['life_loss']
            actions.extend([step('resolve'),step('assert_choices_consumed')])
            if v=='cleanup':actions.extend([step('cleanup'),step('refresh')])
        checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0)]+offer_checks
        universe=set(PINS['cards'])|{name}
        for pl in (0,1):
            checks.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=0)])
            checks.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
            for zone,players in zones.items():
                names=players[pl];checks.append(step('assert_zone',player=pl,zone=zone,value=len(names)))
                checks.extend(step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[n]['csvid'],value=names.count(n)) for n in sorted(universe))
                if zone=='library':checks.extend(step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(names))
            checks.extend(step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=zones['hand'][pl].count(n)+zones['graveyard'][pl].count(n)) for n in sorted(universe))
        seq=pre+actions+checks
        if v in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
            at=0 if v=='ready_snapshot' else len(actions) if v=='resolved_snapshot' else cut
            seq=pre+actions[:at]+[step('capture')]+actions[at:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[at:]+checks+[step('assert_recorded_state')]
        out.append(dict(name=f'DiscardExtensionRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family='discard_spell_extension',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps'])))
    return out
