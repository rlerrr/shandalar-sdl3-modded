"""Whole mana-cost Morph creatures through real face-down casts and paid flips."""
import json
import re
from pathlib import Path

from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'morph_creature'
DATA = Path(__file__).parent / 'data/morph_creature_wave8_profiles.json'
FACE_DOWN = 13252
COLORS = 'CBUGRW'
MORPH = re.compile(r'Morph ((?:\{[0-9]+\})?(?:\{[BUGRW]\})+|\{[0-9]+\}) \(You may cast this card face down as a 2/2 creature for \{3\}\. Turn it face up any time for its morph cost\.\)')
KIN = {'Orc':'Orcish Spy', 'Elephant':'Loxodon Mystic'}
MODES = ('cast_default','cast_colored','cast_faceup','choose_facedown','cancel_cast','cast_short','countered',
         'unmorph','unmorph_offturn','wrong_color','short_fee','already_up','pending_shock','pending_smother',
         'mv_facedown','mv_faceup','sick_attack','attack_revealed','ready_replay','face_down_replay','bounce_recast')
WITNESSES = {'Forest','Mountain','Counterspell','Shock','Smother','Unsummon','Coat of Arms',*KIN.values()}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('type_line','').partition(' — ')[0] not in ('Creature','Artifact Creature') or record.get('keywords')!=['Morph']
            or any(not re.fullmatch(r'[0-9]+',record.get(key,'')) for key in ('power','toughness'))):
        raise ValueError('whole creature with only plain mana-cost Morph required')
    match = MORPH.fullmatch(record['oracle_text'])
    if not match or any(color not in COLORS[1:] for color in record['colors']):
        raise ValueError('complete exact mana-cost Morph clause required')
    printed = cost_from_oracle(record)
    unmorph = cost_from_oracle(dict(record,mana_cost=match[1]))
    if printed[0]<3 or sum(printed)<4 or min(unmorph)<0:
        raise ValueError('ordinary and alternate payment controls need independently distinct costs')
    shape = dict(printed=printed,unmorph=unmorph,subtypes=record['type_line'].split(' — ',1)[1].split(),
                power=int(record['power']),toughness=int(record['toughness']),
                color=sum(1<<COLORS.index(c) for c in record['colors']),clauses=record['oracle_text'].splitlines())
    if record['type_line'].startswith('Artifact Creature — '):
        shape['artifact'] = True
    return shape


def spec(declaration,record):
    shape=mechanism(record)
    source=pins()['sources'].get(record['name'])
    params=declaration.get('params')
    if (not source or not exact_pin(source['oracle'],record) or declaration.get('family')!=FAMILY
            or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not isinstance(params,dict)
            or set(params)!={'schema','mechanism'} or type(params['schema']) is not int or params['schema']!=1
            or not exact_pin(params['mechanism'],shape)):
        raise ValueError('independent whole Morph source and printed prices required')
    return dict(shape,csvid=source['csvid'],family=FAMILY)


def variants(shape):
    modes = MODES+('subtype_kin',) if set(shape['subtypes'])&KIN.keys() else MODES
    if not any(shape['unmorph'][1:]):
        modes = tuple('colored_generic_fee' if mode=='wrong_color' else mode for mode in modes)
    return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]


def validate_witnesses(oracle,rows):
    from generate_cases import TOKEN_PROPERTIES

    data=pins()
    if set(data)!={'sources','witnesses','hidden_observer'} or set(data['witnesses'])!=WITNESSES:
        raise ValueError('exact pinned Morph and secondary witness pool required')
    for group in ('sources','witnesses'):
        for name,pin in data[group].items():
            row=rows.get(name,{})
            if (not exact_pin(pin['oracle'],oracle.get(name)) or row.get('kind')!='card' or row.get('csvid')!=pin['csvid']
                    or row.get('handler')!=pin['handler'] or row.get('handler') in (None,'NULL','0','card_dummy')
                    or oracle[name].get('layout')!='normal'):
                raise ValueError('implemented exact Morph source or witness changed: '+name)
            if group=='sources' and any('subtype_'+s not in TOKEN_PROPERTIES and s not in KIN for s in mechanism(oracle[name])['subtypes']):
                raise ValueError('no live subtype observer for '+name)
    hidden=rows['Face-Down Creature']
    if (hidden['csvid']!=FACE_DOWN or data['hidden_observer']!={'name':hidden['name'],'csvid':FACE_DOWN,'handler':hidden['handler']}
            or hidden['handler']!='manalinkeh_card_face_down_creature'):
        raise ValueError('actual native hidden creature identity required')
    for subtype,name in KIN.items():
        if subtype not in oracle[name]['type_line'].split(' — ',1)[1].split():
            raise ValueError('live matching subtype witness required')


def _case(name,csvid,declaration,shape,variant,rows,oracle):
    from generate_cases import TOKEN_PROPERTIES
    from recipes import card,cost_from_oracle,step

    mode,actor=variant.rsplit('_',1)
    owner=int(actor)
    foe=1-owner
    steps=[]
    setup=[card(name,'source',owner,'hand')]
    if mode=='countered':
        setup.append(card('Counterspell','counter',foe,'hand'))
    if mode=='pending_shock':
        setup.append(card('Shock','response',foe,'hand'))
    if mode in ('pending_smother','mv_facedown','mv_faceup'):
        setup.append(card('Smother','response',foe,'hand'))
    if mode=='bounce_recast':
        setup.append(card('Unsummon','bounce',owner,'hand'))
    if mode=='subtype_kin':
        kin=next(KIN[s] for s in shape['subtypes'] if s in KIN)
        setup.extend([card('Coat of Arms','coat',owner,'hand'),card(kin,'kin',foe,'battlefield')])
    for p in (0,1):
        setup.extend(card(label,f'library_{p}_{i}',p,'library') for i,label in enumerate(('Forest','Mountain')))

    entities={entry['ref']:entry for entry in setup}
    zones={entry['ref']:entry['zone'] for entry in setup}
    pools=[[0]*6 for _ in (0,1)]
    life=[20,20]
    hidden=False
    sick=False
    tapped=False
    damage=0
    current=owner
    source='source'
    replay_from=None
    printed=shape['printed']
    unmorph_cost=shape['unmorph']
    wrong_color=next(index for index in range(1,6) if unmorph_cost[index]==0 and printed[index]==0)
    directly_observed=[s for s in shape['subtypes'] if 'subtype_'+s in TOKEN_PROPERTIES]

    def emit(op,**values):
        steps.append(step(op,**values))

    def fund(player,cost):
        pools[player]=list(cost)
        for i,n in enumerate(cost):
            emit('set_mana',player=player,color=i,value=n)
        emit('refresh')

    def pay(player,cost):
        pools[player]=[have-need for have,need in zip(pools[player],cost)]
        if min(pools[player])<0:
            raise ValueError('unfunded legal Morph fixture')

    def check_pools():
        for player in (0,1):
            for i,n in enumerate(pools[player]):
                emit('assert_mana',player=player,color=i,value=n)

    def characteristics(in_battlefield=True):
        actual=FACE_DOWN if hidden else csvid
        boost=int(mode=='subtype_kin' and not hidden and zones['coat']=='battlefield')
        emit('assert_card',ref=source,property='csvid',value=actual)
        emit('assert_card',ref=source,property='power',value=(2 if hidden else shape['power'])+boost)
        emit('assert_card',ref=source,property='toughness',value=(2 if hidden else shape['toughness'])+boost)
        if not in_battlefield:
            return
        for prop,value in [('in_play',1),('sick',int(sick)),('tapped',int(tapped)),('damage',damage)]:
            emit('assert_card',ref=source,property=prop,value=value)
        traits={'color':0 if hidden else shape['color'],'token':0,'artifact':int(not hidden and shape.get('artifact',False))}
        traits.update({'subtype_'+s:int(not hidden) for s in directly_observed})
        for prop,value in traits.items():
            emit('assert_matching_permanents',player=owner,index=actual,property=prop,value=value)
        if mode=='subtype_kin':
            k=entities['kin']['name']
            for prop in ('power','toughness'):
                emit('assert_card',ref='kin',property=prop,value=int(oracle[k][prop])+boost)

    def receipts():
        emit('assert_choices_consumed')
        emit('assert_stack',value=0)
        check_pools()
        for player in (0,1):
            emit('assert_life',player=player,value=life[player])
            for zone in ('hand','battlefield','graveyard','exile','library'):
                emit('assert_zone',player=player,zone=zone,
                     value=sum(zones[ref]==zone for ref,item in entities.items() if item['player']==player))
            for i,label in enumerate(('Forest','Mountain')):
                emit('assert_zone_card',player=player,zone='library',index=i,value=rows[label]['csvid'])
        for ref,item in entities.items():
            if zones[ref]!='library':
                visible=FACE_DOWN if ref=='source' and hidden and zones[ref]=='battlefield' else rows[item['name']]['csvid']
                emit('assert_zone_multiplicity',player=item['player'],zone=zones[ref],index=visible,value=1)
        if zones['source']=='battlefield':
            characteristics()

    def cast_source(down,dialog=None,allowed=True,payment_color=0):
        nonlocal hidden
        if dialog is not None:
            emit('choose_dialog',player=owner,value=dialog)
        emit('cast',ref=source,value=int(allowed))
        emit('assert_choices_consumed')
        emit('assert_stack',value=int(allowed))
        if allowed:
            cost=[0]*6 if down else list(printed)
            if down:
                cost[payment_color]=3
            pay(owner,cost)
            hidden=down
            zones['source']='stack'
            characteristics(in_battlefield=False)
        check_pools()

    def resolve_source():
        nonlocal sick
        emit('resolve')
        emit('refresh')
        zones['source']='battlefield'
        sick=True
        receipts()

    def reveal(allowed=True,with_parent=False,payment=None):
        nonlocal hidden
        emit('can_activate',ref=source,value=int(allowed))
        if payment is not None and owner==0:
            for _ in range(payment[wrong_color]):
                emit('choose_mana',player=owner,color=wrong_color)
        emit('activate',ref=source,value=int(allowed))
        if allowed:
            pay(owner,unmorph_cost if payment is None else payment)
            hidden=False
            emit('refresh')
            check_pools()
            characteristics()
            # The native callback already flipped the permanent. Drain its
            # legacy activation bookkeeping without asserting that proxy is a
            # rules-visible item or interleaving a new scripted action.
            emit('resolve_response' if with_parent else 'resolve')

    def spell(ref,target=None,response=False,allowed=True):
        player=entities[ref]['player']
        cost=cost_from_oracle(oracle[entities[ref]['name']])
        fund(player,cost)
        if target is not None:
            emit('choose_target',ref=target,player=player)
        emit('cast_response' if response else 'cast',ref=ref,value=int(allowed))
        if allowed:
            pay(player,cost)

    def advance():
        nonlocal current,sick
        current=1-current
        emit('advance_turn')
        if current==owner:
            sick=False

    emit('turn',player=owner,value=0)
    fund(0,[0]*6)
    fund(1,[0]*6)
    down=mode!='cast_faceup'
    dialog={'cast_faceup':1,'choose_facedown':0,'cancel_cast':2}.get(mode)
    payment_color=wrong_color if mode=='cast_colored' else 0
    if dialog is not None:
        fund(owner,printed)
    else:
        budget=[0]*6
        budget[payment_color]=2 if mode=='cast_short' else 3
        fund(owner,budget)
    if mode=='face_down_replay':
        replay_from=len(steps)
    if mode=='cast_colored' and owner==0:
        # The human payment prompt spends one floating colored mana per click.
        for _ in range(3):
            emit('choose_mana',player=owner,color=payment_color)
    cast_source(down,dialog,mode not in ('cancel_cast','cast_short'),payment_color)
    if mode in ('cancel_cast','cast_short'):
        receipts()
    elif mode=='countered':
        emit('announce_spell',ref=source)
        spell('counter',response=True)
        emit('resolve_response')
        zones['source']=zones['counter']='graveyard'
        receipts()
    else:
        resolve_source()
    if mode in ('unmorph_offturn','pending_shock','pending_smother'):
        advance()
    if mode=='attack_revealed':
        advance()
        advance()
    if mode=='subtype_kin':
        spell('coat')
        emit('resolve')
        zones['coat']='battlefield'
        receipts()
    if mode in ('pending_shock','pending_smother'):
        spell('response',source)
        emit('assert_stack',value=1)
        emit('assert_choices_consumed')
    if mode in ('unmorph','unmorph_offturn','wrong_color','short_fee','already_up','pending_shock','pending_smother',
                'mv_faceup','sick_attack','attack_revealed','ready_replay','bounce_recast','subtype_kin','colored_generic_fee'):
        fee=list(unmorph_cost)
        if mode=='wrong_color':
            colored=next(i for i in range(1,6) if fee[i])
            fee[colored]-=1
            fee[wrong_color]+=1
        if mode=='short_fee':
            fee[next(i for i,value in enumerate(fee) if value)]-=1
        if mode=='colored_generic_fee':
            fee[wrong_color],fee[0]=fee[0],0
        fund(owner,fee)
        if mode=='ready_replay':
            replay_from=len(steps)
        reveal(mode not in ('wrong_color','short_fee'),mode in ('pending_shock','pending_smother'),
               payment=fee if mode=='colored_generic_fee' else None)
        if mode in ('pending_shock','pending_smother'):
            zones['response']='graveyard'
        if mode=='pending_shock':
            if shape['toughness']<=2:
                zones['source']='graveyard'
            else:
                damage=2
        receipts()
    if mode=='already_up':
        fund(owner,unmorph_cost)
        reveal(allowed=False)
        receipts()
    if mode in ('mv_facedown','mv_faceup'):
        legal=mode=='mv_facedown'
        advance()
        emit('can_cast',ref='response',value=int(legal))
        spell('response',source if legal else None,allowed=legal)
        if legal:
            emit('resolve')
            zones['source']=zones['response']='graveyard'
        receipts()
    if mode=='bounce_recast':
        spell('bounce',source)
        emit('resolve')
        zones['bounce']='graveyard'
        zones['source']='hand'
        receipts()
        source='returned'
        emit('bind_hand_copy',ref=source,player=owner,index=0,value=csvid)
        fund(owner,[3,0,0,0,0,0])
        cast_source(True)
        resolve_source()
    if mode in ('sick_attack','attack_revealed'):
        legal=mode=='attack_revealed'
        emit('begin_attackers')
        emit('attack',ref=source,value=int(legal))
        emit('finish_attackers')
        emit('begin_blockers')
        emit('advance_combat')
        emit('combat_damage')
        emit('end_combat')
        if legal:
            life[foe]-=shape['power']
            tapped=True
        receipts()
    if replay_from is not None:
        suffix=steps[replay_from:]
        steps=steps[:replay_from]+[step('capture')]+suffix+[step('record_state'),step('restore'),step('ai_roundtrip')]+suffix+[step('assert_recorded_state')]
    metadata=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                  covered_clauses=shape['clauses'],untested_components=[
                      'Distinguishing native response windows created by Morph activation bookkeeping from the rules special action.',
                      'Looking at hidden information and face-down reveal requirements in UI, alternate casting permissions, and multiplayer.'])
    return dict(name=f'MorphCreatureRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=metadata)


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if csvid!=shape['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(shape)):
        raise ValueError('explicit implemented distinct Morph variants required')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,declaration,shape,v,rows,oracle) for v in selected]
