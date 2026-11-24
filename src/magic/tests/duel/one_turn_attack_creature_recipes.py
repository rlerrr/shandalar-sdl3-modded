"""Hasty attackers with a mandatory sacrifice at the next end step."""
import json
import re
from pathlib import Path

from library_search_recipes import wave6_exact as exact_pin
from morph_creature_recipes import FACE_DOWN, MORPH

FAMILY = 'one_turn_attack_creature'
DATA = Path(__file__).parent / 'data/one_turn_attack_creature_wave8_profiles.json'
HASTE = 'Haste (This creature can attack and {T} as soon as it comes under your control.)'
TRAMPLE_HASTE = ("Trample, haste (This creature can deal excess combat damage to the player or planeswalker it's attacking. "
                 'This creature can attack and {T} as soon as it comes under your control.)')
INFECT = ('Infect (This creature deals damage to creatures in the form of -1/-1 counters and to players '
          'in the form of poison counters.)')
END = 'At the beginning of the end step, sacrifice this creature.'
MODES = ('cast_payment','cast_short','cast_wrong_color','countered','fresh_hit','combat_small','combat_high',
         'combat_indestructible','combat_fog','sick_witness','pacified','end_own','end_other','late_combat_end',
         'source_bounced','bounce_recast','indestructible_sacrifice','replay_cast','replay_end','two_end_steps')
BLOCKERS = {'combat_small':'Elvish Mystic', 'combat_high':'Dazzling Ramparts', 'combat_indestructible':'Darksteel Myr',
            'hidden_blocked':'Darksteel Myr'}
WITNESSES = {'Forest','Mountain','Bloodbriar','Elvish Piper','Fog','Pacifism','Indestructibility','Lightning Bolt',
             'Unsummon','Counterspell',*BLOCKERS.values()}
MORPH_MODES = ('hidden_default','hidden_fresh','hidden_aged','hidden_blocked','hidden_choice','hidden_countered',
               'morph_short','morph_wrong_color','morph_attack','morph_end_other','morph_replay')


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line','').startswith('Creature — ')
            or any(not re.fullmatch(r'[0-9]+',record.get(key,'')) for key in ('power','toughness'))):
        raise ValueError('whole normal creature with printed numeric stats required')
    oracle_clauses=record['oracle_text'].splitlines()
    morph=MORPH.fullmatch(oracle_clauses[-1]) if oracle_clauses else None
    clauses=oracle_clauses[:-1] if morph else oracle_clauses
    if not clauses or clauses[-1]!=END or clauses[0] not in ('Haste',HASTE,'Trample, haste',TRAMPLE_HASTE):
        raise ValueError('complete haste and each-turn mandatory self sacrifice required')
    trample=clauses[0].startswith('Trample')
    infect=clauses[1:-1]==[INFECT]
    keywords={'Haste'}
    if trample:
        keywords.add('Trample')
    if infect:
        keywords.add('Infect')
    if morph:
        keywords.add('Morph')
    if (clauses[1:-1] and not infect) or set(record['keywords'])!=keywords:
        raise ValueError('all printed keywords and clauses must match the reviewed composition')
    cost=cost_from_oracle(record)
    if not any(cost[1:]):
        raise ValueError('ordinary printed colored cost required for the payment controls')
    shape=dict(trample=trample,infect=infect,cost=cost,power=int(record['power']),toughness=int(record['toughness']),clauses=oracle_clauses)
    if morph:
        shape['morph']=cost_from_oracle(dict(record,mana_cost=morph[1]))
    return shape


def spec(declaration,record):
    model=mechanism(record)
    source=pins()['sources'].get(record['name'])
    params=declaration.get('params')
    if (not source or not exact_pin(source['oracle'],record) or declaration.get('family')!=FAMILY
            or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not isinstance(params,dict)
            or set(params)!={'schema','mechanism'} or type(params['schema']) is not int
            or params['schema']!=1 or not exact_pin(model,params['mechanism'])):
        raise ValueError('independent exact one-turn creature and all printed clauses required')
    return dict(model,family=FAMILY,csvid=source['csvid'])


def variants(model):
    modes=MODES
    if 'morph' in model:
        modes=tuple('wrong_color_uses_morph' if mode=='cast_wrong_color' else mode for mode in MODES)+MORPH_MODES
    return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]


def validate_witnesses(oracle,rows):
    profile=pins()
    if set(profile)!={'sources','witnesses'} or set(profile['witnesses'])!=WITNESSES:
        raise ValueError('pinned complete one-turn creature witness pool required')
    for group in ('sources','witnesses'):
        for name,pin in profile[group].items():
            row=rows.get(name,{})
            if (not exact_pin(pin['oracle'],oracle.get(name)) or row.get('kind')!='card' or row.get('csvid')!=pin['csvid']
                    or row.get('handler')!=pin['handler'] or row.get('handler') in (None,'NULL','0','card_dummy')
                    or oracle[name].get('layout')!='normal'):
                raise ValueError('implemented independent one-turn source or witness changed: '+name)


def _case(name,csvid,declaration,model,variant,rows,oracle):
    from recipes import card,cost_from_oracle,step
    from generate_cases import TOKEN_PROPERTIES

    mode,actor=variant.rsplit('_',1)
    owner=int(actor)
    foe=1-owner
    start=foe if mode=='end_other' else owner
    setup=[card(name,'source',owner,'hand'),card('Bloodbriar','witness',owner)]
    if mode in BLOCKERS:
        setup.append(card(BLOCKERS[mode],'blocker',foe))
    for modes,label,ref,controller,zone in (
            (('end_other',),'Elvish Piper','piper',owner,'battlefield'),
            (('sick_witness',),'Elvish Mystic','sick',owner,'hand'),
            (('countered','hidden_countered'),'Counterspell','counter',foe,'hand'),
            (('combat_fog',),'Fog','fog',owner,'hand'),
            (('pacified',),'Pacifism','aura',owner,'hand'),
            (('indestructible_sacrifice',),'Indestructibility','aura',owner,'hand'),
            (('indestructible_sacrifice',),'Lightning Bolt','bolt',foe,'hand'),
            (('source_bounced','bounce_recast'),'Unsummon','bounce',foe,'hand')):
        if mode in modes:
            item=card(label,ref,controller,zone)
            if ref=='piper':
                item['state']=['untapped']
            setup.append(item)
    for player in (0,1):
        setup.extend(card(label,f'library_{player}_{i}',player,'library') for i,label in enumerate(('Forest','Mountain')))
    entities={entry['ref']:entry for entry in setup}
    zones={ref:item['zone'] for ref,item in entities.items()}
    life=[20,20]
    poison=[0,0]
    pools=[[0]*6 for _ in (0,1)]
    marks={}
    minus={}
    tapped=set()
    sacrifice_count=0
    actions=[]
    replay_from=None
    source='source'
    face_down=False
    morph=model.get('morph')
    down_variant=mode in MORPH_MODES or mode=='wrong_color_uses_morph'
    color_mask=sum(1<<'CBUGRW'.index(c) for c in oracle[name]['colors'])
    subtypes=[s for s in oracle[name]['type_line'].split(' — ',1)[1].split() if 'subtype_'+s in TOKEN_PROPERTIES]

    def emit(op,**values):
        actions.append(step(op,**values))

    def property(ref,name,value):
        emit('assert_card',ref=ref,property=name,value=value)

    def pool(player,cost):
        pools[player]=list(cost)
        for color,value in enumerate(cost):
            emit('set_mana',player=player,color=color,value=value)
        emit('refresh')

    def pay(player,cost):
        pools[player]=[have-need for have,need in zip(pools[player],cost)]
        if min(pools[player])<0:
            raise ValueError('exact printed one-turn creature payment required')

    def receipts():
        emit('assert_choices_consumed')
        emit('assert_stack',value=0)
        for player in (0,1):
            emit('assert_life',player=player,value=life[player])
            emit('assert_poison',player=player,value=poison[player])
            for color,value in enumerate(pools[player]):
                emit('assert_mana',player=player,color=color,value=value)
            for zone in ('hand','battlefield','graveyard','exile','library'):
                emit('assert_zone',player=player,zone=zone,
                     value=sum(zones[ref]==zone for ref,item in entities.items() if item['player']==player))
            for i,label in enumerate(('Forest','Mountain')):
                emit('assert_zone_card',player=player,zone='library',index=i,value=rows[label]['csvid'])
        for ref,item in entities.items():
            zone=zones[ref]
            if zone=='library':
                continue
            source_hidden=ref=='source' and face_down and zone=='battlefield'
            visible=FACE_DOWN if source_hidden else rows[item['name']]['csvid']
            emit('assert_zone_multiplicity',player=item['player'],zone=zone,index=visible,value=1)
            if zone!='battlefield' or not oracle[item['name']]['type_line'].startswith(('Creature — ','Artifact Creature — ')):
                continue
            alias=source if ref=='source' else ref
            property(alias,'in_play',1)
            property(alias,'tapped',int(ref in tapped))
            property(alias,'damage',marks.get(ref,0))
            property(alias,'minus1_counters',minus.get(ref,0))
            added=sacrifice_count if ref=='witness' else 0
            property(alias,'plus1_counters',added)
            toughness=2 if source_hidden else int(oracle[item['name']]['toughness'])
            power=2 if source_hidden else int(oracle[item['name']]['power'])
            property(alias,'toughness',toughness+added-minus.get(ref,0))
            if not minus.get(ref,0):
                property(alias,'power',power+added)
            if ref=='source' and morph is not None:
                property(alias,'csvid',visible)
                traits={'color':0 if source_hidden else color_mask,'artifact':0,'token':0}
                traits.update({'subtype_'+s:int(not source_hidden) for s in subtypes})
                for attr,value in traits.items():
                    emit('assert_matching_permanents',player=owner,index=visible,property=attr,value=value)

    def cast(ref,*,target=None,allowed=True,budget=None,response=False):
        player=entities[ref]['player']
        price=cost_from_oracle(oracle[entities[ref]['name']])
        pool(player,price if budget is None else budget)
        if target is not None:
            emit('choose_target',player=player,ref=target)
        if ref=='source' and morph is not None and allowed:
            emit('choose_dialog',player=owner,value=1)
        emit('cast_response' if response else 'cast',ref=source if ref=='source' else ref,value=int(allowed))
        if allowed:
            pay(player,price)
            zones[ref]='stack'
            emit('assert_stack',value=2 if response else 1)
            property(source if ref=='source' else ref,'in_hand',0)

    def cast_hidden():
        nonlocal face_down
        budget=[3,0,0,0,0,0]
        payment_color=0
        if mode=='hidden_choice':
            budget[4]=model['cost'][4]
        if mode=='wrong_color_uses_morph':
            payment_color=3
            budget[0]=0
            budget[payment_color]=3
        pool(owner,budget)
        if mode=='hidden_choice':
            emit('choose_dialog',player=owner,value=0)
        if payment_color and owner==0:
            for _ in range(3):
                emit('choose_mana',player=owner,color=payment_color)
        emit('cast',ref=source,value=1)
        cost=[0]*6
        cost[payment_color]=3
        pay(owner,cost)
        zones['source']='stack'
        face_down=True
        emit('assert_choices_consumed')
        emit('assert_stack',value=1)
        property(source,'csvid',FACE_DOWN)
        property(source,'power',2)
        property(source,'toughness',2)

    def flip():
        nonlocal face_down
        fee=morph.copy()
        if mode=='morph_short':
            fee[4]-=1
        if mode=='morph_wrong_color':
            fee[3],fee[4]=fee[4],0
        pool(owner,fee)
        allowed=mode not in ('morph_short','morph_wrong_color')
        emit('can_activate',ref=source,value=int(allowed))
        emit('activate',ref=source,value=int(allowed))
        if allowed:
            pay(owner,morph)
            face_down=False
            emit('refresh')
            property(source,'csvid',csvid)
            property(source,'power',model['power'])
            property(source,'toughness',model['toughness'])
            emit('resolve')
        receipts()

    def resolve(ref,destination):
        emit('resolve')
        emit('refresh')
        zones[ref]=destination
        receipts()

    def piper_entry():
        nonlocal source
        green=[0,0,0,1,0,0]
        pool(owner,green)
        emit('require_can_activate',ref='piper',value=1)
        emit('activate',ref='piper',value=1)
        pay(owner,green)
        tapped.add('piper')
        emit('assert_stack',value=1)
        property('piper','tapped',1)
        property(source,'in_hand',1)
        emit('choose_zone_card',player=owner,value=csvid)
        emit('resolve')
        emit('refresh')
        source='entered'
        emit('bind_permanent_copy',ref=source,player=owner,index=0,value=csvid)
        zones['source']='battlefield'
        receipts()

    def end_step(advance=False):
        nonlocal sacrifice_count
        if any(any(pool) for pool in pools):
            raise ValueError('real end-step fixtures have no floating mana')
        emit('advance_turn' if advance else 'cleanup')
        emit('refresh')
        if zones['source']=='battlefield' and not face_down:
            zones['source']='graveyard'
            sacrifice_count+=1
            if zones.get('aura')=='battlefield':
                zones['aura']='graveyard'
        marks.clear()
        receipts()

    def deal_to_opponent(amount):
        if model['infect']:
            poison[foe]+=amount
        else:
            life[foe]-=amount

    def fight():
        blocked='blocker' in entities
        legal=mode not in ('pacified','hidden_fresh')
        emit('begin_attackers')
        if mode=='sick_witness':
            emit('attack',ref='sick',value=0)
        emit('attack',ref=source,value=int(legal))
        if legal:
            tapped.add('source')
        emit('finish_attackers')
        emit('begin_blockers')
        if blocked:
            emit('block',ref='blocker',other=source,value=1)
        emit('advance_combat')
        if blocked:
            property(source,'blocked',1)
        emit('combat_damage')
        emit('end_combat')
        if legal and mode!='combat_fog':
            damage=2 if face_down else model['power']
            if blocked:
                blocker=oracle[entities['blocker']['name']]
                lethal=int(blocker['toughness'])
                spill=max(0,damage-lethal) if model['trample'] and not face_down else 0
                dealt=damage-spill
                if model['infect']:
                    minus['blocker']=dealt
                else:
                    marks['blocker']=dealt
                if dealt>=lethal and (model['infect'] or 'Indestructible' not in blocker['keywords']):
                    zones['blocker']='graveyard'
                incoming=int(blocker['power'])
                marks['source']=incoming
                if incoming>=(2 if face_down else model['toughness']):
                    zones['source']='graveyard'
                deal_to_opponent(spill)
            else:
                deal_to_opponent(damage)
        receipts()

    emit('turn',player=start,value=0)
    for player in (0,1):
        pool(player,[0]*6)
    if mode=='sick_witness':
        cast('sick')
        resolve('sick','battlefield')
    if mode=='end_other':
        piper_entry()
    elif down_variant:
        cast_hidden()
        if mode=='hidden_countered':
            emit('announce_spell',ref=source)
            cast('counter',response=True)
            emit('resolve_response')
            zones['source']=zones['counter']='graveyard'
            receipts()
        else:
            resolve('source','battlefield')
    elif mode in ('cast_short','cast_wrong_color'):
        budget=model['cost'].copy()
        if mode=='cast_short':
            if morph is not None:
                budget=[2,0,0,0,0,0]
            else:
                budget[next(i for i,value in enumerate(budget) if value)]-=1
        else:
            correct=next(i for i in range(1,6) if budget[i])
            wrong=next(i for i in range(1,6) if not budget[i])
            budget[wrong],budget[correct]=budget[correct],0
        cast('source',budget=budget,allowed=False)
        receipts()
    else:
        if mode=='replay_cast':
            replay_from=len(actions)
        cast('source')
        if mode=='countered':
            emit('announce_spell',ref=source)
            cast('counter',response=True)
            emit('resolve_response')
            zones['source']=zones['counter']='graveyard'
            receipts()
        else:
            resolve('source','battlefield')
    if mode=='combat_fog':
        cast('fog')
        resolve('fog','graveyard')
    if mode in ('pacified','indestructible_sacrifice'):
        cast('aura',target=source)
        resolve('aura','battlefield')
    if mode=='indestructible_sacrifice':
        cast('bolt',target=source)
        marks['source']=3
        resolve('bolt','graveyard')
    if mode in ('source_bounced','bounce_recast'):
        cast('bounce',target=source)
        zones['source']='hand'
        resolve('bounce','graveyard')
        if mode=='bounce_recast':
            source='returned'
            emit('bind_hand_copy',ref=source,player=owner,index=0,value=csvid)
            cast('source')
            resolve('source','battlefield')
    if mode in ('fresh_hit','combat_fog','sick_witness','pacified','late_combat_end','bounce_recast') or mode in BLOCKERS:
        if mode!='hidden_blocked':
            fight()
    if mode=='hidden_fresh':
        fight()
    if mode in ('hidden_aged','hidden_blocked'):
        end_step(advance=True)
        end_step(advance=True)
        fight()
    if mode=='morph_end_other':
        end_step(advance=True)
    if mode in ('morph_short','morph_wrong_color','morph_attack','morph_end_other','morph_replay'):
        if mode=='morph_replay':
            replay_from=len(actions)
        flip()
    if mode in ('morph_attack','morph_replay'):
        fight()
    if mode=='replay_end':
        replay_from=len(actions)
    if mode not in ('cast_payment','cast_short','cast_wrong_color','hidden_choice','morph_short','morph_wrong_color'):
        end_step(advance=mode=='two_end_steps')
        if mode=='two_end_steps':
            end_step()
    if replay_from is not None:
        suffix=actions[replay_from:]
        actions=actions[:replay_from]+[step('capture')]+suffix+[step('record_state'),step('restore'),step('ai_roundtrip')]+suffix+[step('assert_recorded_state')]
    metadata=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                  covered_clauses=model['clauses'],untested_components=[
                      'Separate response priority inside the end-step trigger and simultaneous sacrifice trigger ordering.',
                      'Granted tap abilities, alternate combat recipients, control transfer, multiplayer.'])
    if morph is not None:
        metadata['untested_components'].append('Distinguishing priority during the internal native Morph activation carrier.')
    return dict(name=f'OneTurnAttackCreatureRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,_recipe=metadata)


def expand(name,csvid,declaration,model,selected,rows,oracle):
    if csvid!=model['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('distinct explicit reviewed one-turn attack variants required')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,declaration,model,variant,rows,oracle) for variant in selected]
