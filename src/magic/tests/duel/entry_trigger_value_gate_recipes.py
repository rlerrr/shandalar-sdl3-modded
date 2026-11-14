"""Whole-Oracle entry payment/survival and affinity gates.

Every payment and artifact is produced through ordinary cast callbacks. The
accepted immutable selection ledger owns bounded selection and zone receipts.
"""
import copy
import json
import re
from pathlib import Path

import entry_trigger_value_selection_recipes as selection

FAMILY='entry_trigger_value'
DATA=Path(__file__).parent/'data/entry_trigger_value_wave8_gate_profiles.json'


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line','').partition(' — ')[0].split()
            or any(not str(record.get(k,'')).isdigit() for k in ('power','toughness')) or int(record['toughness'])<=0):
        raise ValueError('entry gate requires a normal fixed-stat creature')
    prefix=r'When (?:this creature|'+re.escape(record['name'])+r') enters, '
    lines=record['oracle_text'].splitlines();clauses=[line for line in lines if re.match(prefix,line)]
    bodies=[re.sub(prefix,'',line,count=1) for line in clauses]
    choose='look at the top three cards of your library, then put one of them into your hand and the rest on the bottom of your library in any order.'
    sacrifice='sacrifice it unless {W} was spent to cast it.'
    artifacts='reveal the top three cards of your library. Put all artifact cards revealed this way into your hand and the rest into your graveyard.'
    affinity=[line for line in lines if re.fullmatch(r'Affinity for artifacts(?: \([^\n]+\))?',line)]
    if len(bodies)==2 and set(bodies)=={choose,sacrifice}:
        p=dict(gate='white_spent',kind='one',window=3,scope='any',optional=False,remainder='bottom',target_player=False,
               selection_clause=clauses[bodies.index(choose)],condition_clause=clauses[bodies.index(sacrifice)])
        tested=set(clauses)
    elif bodies==[artifacts] and len(affinity)==1:
        p=dict(gate='affinity_artifacts',kind='all',window=3,scope='artifact',optional=False,remainder='graveyard',target_player=False,
               selection_clause=clauses[0],condition_clause=affinity[0])
        tested=set(clauses+affinity)
    else:raise ValueError('unsupported complete entry gate clauses')
    p['gaps']=[line for line in lines if line not in tested]
    return p


def spec(d,record):
    from recipes import cost_from_oracle
    if (d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2
            or not isinstance(d.get('variants'),list) or not d['variants'] or len(set(d['variants']))!=len(d['variants'])):
        raise ValueError('entry gate requires explicit version2 distinct variants')
    q=d.get('params')
    if not isinstance(q,dict) or set(q)!={'schema','mechanism','presentation'} or type(q['schema']) is not int or q['schema']!=4:
        raise ValueError('entry gate requires explicit schema4 and reviewed components')
    parsed=mechanism(record);profile=pins()['sources'].get(record['name'])
    if (not profile or profile['oracle']!=record or d.get('oracle_text')!=record['oracle_text']
            or parsed!=profile['mechanism'] or q['mechanism']!=parsed or q['presentation']!=profile['presentation']):
        raise ValueError('entry gate whole Oracle/source/input pin changed')
    expected={'selector':'display','human_payment_dialog':True} if parsed['gate']=='white_spent' else {'selector':'automatic','public_reveal_ack_player':0}
    if q['presentation']!=expected or any(type(q['presentation'][key]) is not type(value) for key,value in expected.items()):
        raise ValueError('unreviewed entry gate input presentation')
    return dict(parsed,_entry_gate=True,family=FAMILY,csvid=profile['csvid'],source_name=record['name'],
                mana=cost_from_oracle(record),power=int(record['power']),toughness=int(record['toughness']),
                creature=True,land=False,presentation=copy.deepcopy(q['presentation']))


def variants(p):
    if p['gate']=='white_spent':
        modes=['white','no_white','insufficient','wrong_blue','alternate_choice','duplicates','cleanup','countered',
               'ready_snapshot','pending_snapshot','resolved_snapshot','bounce_recast','blink_uncast','source_destroyed',
               'empty_library','one_card_library','short_library','no_white_empty','no_white_short']
        return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]+['generic_white_0','white_unspent_0']
    modes=['count0','count1','count2','count6','count7','opponent_artifacts','artifact_in_hand','artifact_in_grave',
           'artifact_creature','nonartifact','insufficient','missing_blue','empty_library','one_card_library','short_library',
           'no_match','duplicates','all_match','cleanup','countered','ready_snapshot','pending_snapshot','resolved_snapshot',
           'bounce_recast','removed_before','removed_after']
    return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]


def validate_witnesses(oracle,rows):
    data=pins()
    for name,record in data['witnesses'].items():
        if oracle.get(name)!=record or name not in rows:raise ValueError('entry gate witness pin changed: '+name)
    for name,profile in data['sources'].items():
        if (oracle.get(name)!=profile['oracle'] or name not in rows or rows[name]['csvid']!=profile['csvid']
                or rows[name]['kind']!='card' or rows[name]['handler'] in (None,'NULL','card_dummy')):
            raise ValueError('entry gate source identity/implementation changed: '+name)
    for name in ['Darksteel Relic','Darksteel Myr','Boomerang','Cloudshift','Counterspell']:
        if data['witnesses'][name].get('layout')!='normal' or rows[name]['handler'] in (None,'NULL','card_dummy'):
            raise ValueError('entry gate typed casting witness must be implemented and normal: '+name)


def _library(p,mode):
    top=['Forest','Grizzly Bears','Lightning Bolt'] if p['gate']=='white_spent' else ['Darksteel Relic','Darksteel Myr','Forest']
    if mode in ('empty_library','no_white_empty'):return []
    if mode=='one_card_library':return top[:1]
    if mode in ('short_library','no_white_short'):return top[:2]
    if mode=='duplicates':top=[top[0],top[0],top[2]]
    if mode=='no_match':top=['Forest','Grizzly Bears','Lightning Bolt']
    if mode=='all_match':top=['Darksteel Relic','Darksteel Myr','Darksteel Relic']
    return top+['Mountain','Island','Counterspell','Darksteel Relic']


def _case(name,csvid,d,p,variant,rows,oracle):
    from recipes import card,step,cost_from_oracle
    mode,owner=variant.rsplit('_',1);owner=int(owner)
    setup=[]
    def add(n,ref,player,zone='hand'):
        setup.append(card(n,ref,player,zone))
    for player in (0,1):
        for i,n in enumerate(['Plains','Giant Growth']):add(n,f'hand_{player}_{i}',player)
        add('Mountain',f'grave_{player}',player,'graveyard');add('Swamp',f'exile_{player}',player,'exile')
        for i,n in enumerate(_library(p,mode)):add(n,f'library_{player}_{i}',player,'library')
    add(name,'source',owner);add('Grizzly Bears','friendly',owner,'battlefield');add('Hill Giant','enemy',1-owner,'battlefield')
    artifact_refs=[];artifact_player=owner
    if p['gate']=='affinity_artifacts':
        count=int(mode[-1]) if re.fullmatch(r'count[01267]',mode) else 0 if mode in ('opponent_artifacts','artifact_in_hand','artifact_in_grave','nonartifact') else 1 if mode=='artifact_creature' else 2
        if mode=='opponent_artifacts':artifact_player=1-owner;count=2
        for i in range(count):
            n='Darksteel Myr' if mode=='artifact_creature' or count==2 and i==1 and not mode.startswith('count') and mode!='opponent_artifacts' else 'Darksteel Relic'
            ref=f'affinity_{i}';add(n,ref,artifact_player);artifact_refs.append(ref)
        if mode in ('artifact_in_hand','artifact_in_grave'):add('Darksteel Relic','off_battlefield_artifact',owner,'hand' if mode=='artifact_in_hand' else 'graveyard')
        if mode=='nonartifact':add('Grizzly Bears','ordinary_creature',owner)
    if mode in ('bounce_recast','removed_before','removed_after'):add('Boomerang','bounce',owner)
    if mode=='blink_uncast':add('Cloudshift','blink',owner)
    if mode=='countered':add('Counterspell','counter',1-owner)
    case=dict(card=name,fixture='empty_main',seed=123,setup=setup)
    world=selection.SelectionLedger(case,p,mode,owner,rows,oracle)
    pools=[[0]*6,[0]*6];actions=[]
    def set_pools(player,vector):
        pools[player]=list(vector)
        actions.extend(step('set_mana',player=player,color=i,value=n) for i,n in enumerate(vector))
    def mana_receipts():return [step('assert_mana',player=player,color=i,value=n) for player in (0,1) for i,n in enumerate(pools[player])]
    def pay(player,vector):
        pools[player]=[n-cost for n,cost in zip(pools[player],vector)]
        if min(pools[player])<0:raise ValueError('gate fixture underfunded an expected payment')
    def cost(n):return [0]*6 if oracle[n]['mana_cost']=='{0}' else cost_from_oracle(oracle[n])
    def receipt():
        result=[step('assert_stack',value=0),step('assert_choices_consumed')]+mana_receipts()+world.receipts()
        e=world.entities['source'];alive=e['zone']=='battlefield'
        result += [step('assert_card',ref='source',property='in_play',value=int(alive)),step('assert_card',ref='source',property='in_hand',value=int(e['zone']=='hand'))]
        if alive:result += [step('assert_card',ref='source',property=key,value=p[key]) for key in ('power','toughness')]
        result += [step('assert_life',player=player,value=20) for player in (0,1)]
        return result
    def plain_cast(ref,inputs=(),payment=None,response=False):
        e=world.entities[ref];player=e['player'];vector=cost(e['name']) if payment is None else payment
        actions.extend(inputs);actions.append(step('cast_response' if response else 'cast',ref=ref,value=1));pay(player,vector);world.move(ref,'stack')
        actions.extend([step('assert_card',ref=ref,property='in_hand',value=0),step('assert_stack',value=2 if response else 1)])
        actions.extend(mana_receipts())
    actions.append(step('turn',player=owner,value=0))
    for player in (0,1):set_pools(player,[0]*6)
    if artifact_refs:
        actions.append(step('turn',player=artifact_player,value=0))
        prep=[sum(cost(world.entities[ref]['name'])[i] for ref in artifact_refs) for i in range(6)]
        set_pools(artifact_player,prep)
        for ref in artifact_refs:
            plain_cast(ref);actions.append(step('resolve'));world.move(ref,'battlefield');actions.extend([step('assert_choices_consumed'),step('assert_card',ref=ref,property='in_play',value=1)])
        actions.extend(mana_receipts())
        if artifact_player!=owner:actions.append(step('advance_turn'))
    if mode=='nonartifact':
        set_pools(owner,cost('Grizzly Bears'));plain_cast('ordinary_creature');actions.append(step('resolve'));world.move('ordinary_creature','battlefield');actions.append(step('assert_choices_consumed'))
    def bounce_artifact(ref,response=False):
        plain_cast('bounce',[step('choose_target',player=owner,ref=ref)],response=response)
        if not response:
            actions.append(step('resolve'));world.move('bounce','graveyard');world.move(ref,'hand');actions.append(step('assert_choices_consumed'))
    if mode=='removed_before':
        set_pools(owner,cost('Boomerang'));bounce_artifact(artifact_refs[-1])
    def artifact_count():
        return sum(e['zone']=='battlefield' and e['player']==owner and 'Artifact' in oracle[e['name']]['type_line'].partition(' — ')[0].split() for e in world.entities.values())
    def source_payment(recast=False):
        if p['gate']=='white_spent':
            no_white=mode in ('no_white','no_white_empty','no_white_short') or recast
            if no_white:return [2,0,1,0,0,0],[2,0,1,0,0,0],[],False
            if mode=='generic_white':return [1,0,1,0,0,1],[1,0,1,0,0,1],[step('choose_dialog',player=0,value=0),step('choose_mana',player=0,color=5)],True
            if mode=='white_unspent':return [2,0,1,0,0,1],[2,0,1,0,0,0],[step('choose_dialog',player=0,value=0)],False
            return [1,0,1,0,0,1],[1,0,1,0,0,1],[step('choose_dialog',player=0,value=1)] if owner==0 else [],True
        payment=list(p['mana']);payment[0]=max(0,payment[0]-artifact_count())
        return payment,payment,[],False
    def resolve_entry(white_spent):
        world.move('source','battlefield')
        if p['gate']=='affinity_artifacts' and world.zones['library'][owner]:actions.append(step('choose_zone',player=0,value=-1))
        actions.extend(world.selection())
        if p['gate']=='white_spent' and not white_spent:world.move('source','graveyard')
        actions.append(step('resolve'));actions.extend(receipt())
    pool,payment,inputs,white=source_payment()
    denied=mode in ('insufficient','wrong_blue','missing_blue')
    if denied:
        pool=list(pool)
        if mode=='insufficient':pool=[1,0,1,0,0,0] if p['gate']=='white_spent' else [max(0,pool[0]-1),0,pool[2],0,0,0]
        else:pool=[2,0,0,0,0,1] if p['gate']=='white_spent' else [pool[0],0,2,0,0,0]
        inputs=[]
    if mode=='removed_after':pool=[n+extra for n,extra in zip(pool,cost('Boomerang'))]
    set_pools(owner,pool)
    if mode=='countered':set_pools(1-owner,cost('Counterspell'))
    actions.append(step('refresh'));ready_cut=len(actions)
    if denied:
        actions.append(step('cast',ref='source',value=0));actions.extend(receipt());pending_cut=len(actions)
    else:
        plain_cast('source',inputs,payment);actions.append(step('assert_zone',player=owner,zone='hand',value=len(world.zones['hand'][owner])+1));pending_cut=len(actions)
        if mode=='countered':
            # Legacy Counterspell uses the real pending spell context supplied
            # by cast_response; it never asks a target-permanent selector.
            actions.append(step('announce_spell',ref='source'));plain_cast('counter',response=True)
            actions.append(step('resolve_response'));world.move('counter','graveyard');world.move('source','graveyard');actions.extend(receipt())
        elif mode=='removed_after':
            actions.append(step('announce_spell',ref='source'));bounce_artifact(artifact_refs[-1],response=True)
            world.move('bounce','graveyard');world.move(artifact_refs[-1],'hand');world.move('source','battlefield')
            if world.zones['library'][owner]:actions.append(step('choose_zone',player=0,value=-1))
            actions.extend(world.selection());actions.append(step('resolve_response'));actions.extend(receipt())
        else:resolve_entry(white)
    if mode=='bounce_recast':
        set_pools(owner,cost('Boomerang'));plain_cast('bounce',[step('choose_target',player=owner,ref='source')]);actions.append(step('resolve'))
        world.move('bounce','graveyard');world.move('source','hand');actions.extend([step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'),step('assert_choices_consumed')])
        pool,payment,inputs,white=source_payment(recast=True);set_pools(owner,pool);plain_cast('source',inputs,payment);resolve_entry(white)
    if mode=='blink_uncast':
        set_pools(owner,cost('Cloudshift'));plain_cast('blink',[step('choose_target',player=owner,ref='source')]);world.move('blink','graveyard')
        actions.extend(world.selection());world.move('source','graveyard');actions.append(step('resolve'));actions.extend(receipt())
    if mode=='source_destroyed':actions.append(step('destroy',ref='source'));world.move('source','graveyard');actions.extend(receipt())
    if mode=='cleanup':actions.append(step('cleanup'));actions.extend(receipt())
    if mode in ('ready_snapshot','pending_snapshot'):
        cut=ready_cut if mode=='ready_snapshot' else pending_cut
        tail=copy.deepcopy(actions[cut:]);actions=actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
    if mode=='resolved_snapshot':actions += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
    case.update(name=f'EntryTriggerGateRecipe_{csvid}_{variant}',steps=actions,
                _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],
                             untested_components=p['gaps']+['independent ETB trigger-order alternatives/intermediate priority','replacement effects','control changes','multiplayer']))
    return case


def expand(name,csvid,d,p,selected,rows,oracle):
    if csvid!=p['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(p)):
        raise ValueError('entry gate requires matching identity and distinct supported variants')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,d,p,variant,rows,oracle) for variant in selected]
