"""Paid temporary artifact creatures and their actual printed mana modes."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from entry_aura_freeze_recipes import FreezeLedger
from guild_locket_recipes import SPENDERS
from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'dual_mana_artifact_animation'
DATA = Path(__file__).parent / 'data/dual_mana_artifact_animation_wave8_profiles.json'
COLORS = 'CBUGRW'
WORDS = dict(B='black', U='blue', G='green', R='red', W='white')
SUFFIXES = {
    ' until end of turn.': 'none',
    " until end of turn and can't be blocked this turn.": 'unblockable',
    ' with flying until end of turn.': 'flying',
    ' with first strike until end of turn.': 'first_strike',
    ' with deathtouch until end of turn.': 'deathtouch',
    " with hexproof until end of turn. (It can't be the target of spells or abilities your opponents control.)": 'hexproof',
    ' with double strike until end of turn. (It deals both first-strike and regular combat damage.)': 'double_strike',
    ' with trample until end of turn.': 'trample',
    ' with lifelink until end of turn.': 'lifelink',
}
LOOT_CLAUSE = 'Whenever this artifact deals combat damage to a player, you may draw a card. If you do, discard a card.'
SUBTYPE_WITNESSES = {'Crab': 'Horseshoe Crab', 'Devil': 'Scalding Devil', 'Thrull': 'Dutiful Thrull'}
EXTENDED_COMBAT = {'double_strike','trample','lifelink','loot'}
DEFENDING = {'lifelink_defending','loot_defending'}
PREVENTED = {'lifelink_prevented','loot_prevented'}
BASE = ('mana_first','mana_second','entry_mana','wrong_mana_output','cast_unfunded','cast_countered',
        'animate','cleanup','offturn','tapped_animate','short_fee','wrong_fee','ready_replay','pending_replay',
        'fresh_attack','attack','destroy_animated')


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line')!='Artifact' or record.get('mana_cost')!='{3}' or record.get('keywords')!=[]):
        raise ValueError('normal whole noncreature artifact with printed cost {3} required')
    clauses = record['oracle_text'].splitlines()
    loot=len(clauses)==3 and clauses[2]==LOOT_CLAUSE
    mana = re.fullmatch(r'\{T\}: Add \{([BUGRW])\} or \{([BUGRW])\}\.',clauses[0]) if len(clauses)==2 or loot else None
    if not mana or mana[1]==mana[2]:
        raise ValueError('exactly two printed mana options required')
    colors=list(mana.groups())
    a,b=colors
    animation=clauses[1]
    if loot:
        fee,body=animation.split(': ',1)
        prefix='Until end of turn, this artifact becomes a '
        if not body.startswith(prefix) or not body.endswith('.'):
            raise ValueError('complete Izzet duration and artifact creature text required')
        animation=fee+': This artifact becomes a '+body[len(prefix):-1]+' until end of turn.'
    form=re.fullmatch(r'(\{4\})?'+re.escape(f'{{{a}}}{{{b}}}: This artifact becomes a ')
                      +r'(\d)/(\d) '+re.escape(f'{WORDS[a]} and {WORDS[b]} ')
                      +r'(Bird|Wolf|Insect|Horror|Crab|Devil|Dragon|Soldier|Beast|Thrull|Elemental) artifact creature(.+)',animation)
    if not form or form[5] not in SUFFIXES:
        raise ValueError('complete printed form, types, colors, cost and duration required')
    generic, creature_type = int(bool(form[1]))*4, form[4]
    extra=SUFFIXES[form[5]]
    if loot:
        if creature_type!='Elemental' or extra!='none' or (form[2],form[3])!=('2','1'):
            raise ValueError('whole Izzet Elemental form and independent loot clause required')
        extra='loot'
    if bool(generic)!=(creature_type=='Dragon') or generic and (form[2],form[3],extra)!=('4','4','flying'):
        raise ValueError('only the complete Keyrune and Dragon Monument grammars are reviewed')
    fee=cost_from_oracle({'mana_cost':clauses[1].split(': ',1)[0]})
    return dict(colors=colors,fee=fee,cast_mana=cost_from_oracle(record),power=int(form[2]),toughness=int(form[3]),
                creature_type=creature_type,extra=extra,clauses=clauses)


def spec(declaration,record):
    profile=mechanism(record)
    pin=pins()['sources'].get(record['name'])
    if (not pin or not exact_pin(pin['oracle'],record) or declaration.get('family')!=FAMILY
            or not exact_pin(declaration.get('version'),2) or declaration.get('oracle_text')!=record['oracle_text']
            or not exact_pin(declaration.get('params'),dict(schema=1,mechanism=profile))):
        raise ValueError('independent whole Oracle and animation model required')
    return dict(profile,csvid=pin['csvid'],family=FAMILY)


def variants(profile):
    modes=list(BASE)
    extra=profile['extra']
    if extra in {'flying','unblockable','deathtouch','first_strike'}|EXTENDED_COMBAT:
        modes.append('keyword_combat')
    if extra=='flying':
        modes.append('reach_block')
    if extra=='hexproof':
        modes.append('hexproof_casts')
    if profile['creature_type'] in SUBTYPE_WITNESSES:
        modes.append('shared_subtype')
    if extra=='trample':
        modes.append('trample_no_excess')
    if extra=='lifelink':
        modes+=['lifelink_defending','lifelink_prevented']
    if extra=='loot':
        modes+=['loot_defending','loot_prevented']
    cells=[f'{mode}_{owner}' for owner in (0,1) for mode in modes]
    if extra=='loot':
        cells+=['loot_decline_0','loot_choice_0']
    return cells


def validate_witnesses(oracle,rows):
    data=pins()
    for group in ('sources','witnesses'):
        for name,pin in data[group].items():
            row=rows.get(name,{})
            if (not exact_pin(pin['oracle'],oracle.get(name)) or row.get('kind')!='card'
                    or row.get('csvid')!=pin['csvid'] or row.get('handler')!=pin['handler']
                    or row['handler'] in (None,'NULL','0')
                    or row['handler']=='card_dummy' and (group=='sources' or name not in data['static_witnesses'])):
                raise ValueError('reviewed canonical card binding changed: '+name)


def _case(name,csvid,d,p,variant,rows,oracle):
    from recipes import card,cost_from_oracle,step

    mode,side=variant.rsplit('_',1);owner=int(side);foe=1-owner
    a,b=[COLORS.index(c) for c in p['colors']]
    extra=p['extra']
    wrong=next(c for c in range(1,6) if c not in (a,b))
    cast_source=mode in ('entry_mana','cast_unfunded','cast_countered','fresh_attack')
    mana_case=mode in ('mana_first','mana_second','entry_mana','wrong_mana_output')
    output=b if mode=='mana_second' else wrong if mode=='wrong_mana_output' else a
    source=card(name,'source',owner,'hand' if cast_source else 'battlefield')
    if mode in ('tapped_animate','short_fee','wrong_fee'):
        source['state']=['tapped']
    setup=[source,card('Giant Growth','own_probe',owner,'hand'),card('Unsummon','foe_probe',foe,'hand')]
    if extra=='loot':
        setup.pop(1)
        if mode=='loot_choice':
            setup.append(card('Fog','old_card',owner,'hand'))
    for player in (0,1):
        setup += [card(n,f'deck_{player}_{i}',player,'library') for i,n in enumerate(('Forest','Island','Mountain'))]
    if mana_case:
        setup.append(card(SPENDERS[COLORS[output]],'spender',owner,'hand'))
    if mode=='cast_countered':
        setup.append(card('Counterspell','counter',foe,'hand'))
    if mode=='destroy_animated':
        setup.append(card('Disenchant','removal',owner if p['extra']=='hexproof' else foe,'hand'))
    if mode in ('keyword_combat','reach_block','trample_no_excess') or mode in DEFENDING:
        blocker='Giant Spider' if mode=='reach_block' else 'Wall of Stone' if p['extra']=='deathtouch' else 'Grizzly Bears'
        if mode=='trample_no_excess' or mode=='keyword_combat' and extra in ('double_strike','lifelink'):
            blocker='Wall of Stone'
        setup.append(card(blocker,'blocker',foe))
    if mode in PREVENTED:
        setup.append(card('Fog','prevention',foe,'hand'))
    if mode=='shared_subtype':
        setup += [card('Coat of Arms','coat',owner,'hand'),card(SUBTYPE_WITNESSES[p['creature_type']],'kin',foe),
                  card('Grizzly Bears','nonkin',owner)]
    case=dict(card=name,fixture='empty_main',seed=123,setup=setup)
    world=FreezeLedger(case,p,mode,owner,rows,oracle)
    actions=[step('turn',player=foe if mode=='offturn' or mode in DEFENDING else owner,value=0),step('present_ai_dialogs',value=int(not mana_case))]
    mana=[[0]*6,[0]*6]
    life=[20,20]
    active=False
    growth=0
    damage={}
    discard_receipts=0
    color_mask=(1<<a)|(1<<b)

    def emit(op,**values):
        actions.append(step(op,**values))

    def fund(player,pool):
        mana[player]=list(pool)
        for c,n in enumerate(pool):
            emit('set_mana',player=player,color=c,value=n)
        emit('refresh')

    def cast(ref,target=None,*,allowed=True,live_mana=False,response=False,prefund=True):
        item=world.entities[ref];player=item['player'];fee=cost_from_oracle(oracle[item['name']])
        if allowed and prefund and not live_mana:
            fund(player,[have+need for have,need in zip(mana[player],fee)])
        if live_mana:
            if fee[output]!=1 or sum(fee)!=1 or any(mana[player]):
                raise ValueError('the matching one-mana spell must use its only colored source')
            if owner==0:
                emit('choose_mana_source',ref='source')
            mana[player][output]+=1
            world.entities['source']['tapped']=True
        if target is not None:
            emit('choose_target',player=player,ref=target)
        emit('cast_response' if response else 'cast',ref=ref,value=int(allowed))
        if allowed:
            mana[player]=[have-need for have,need in zip(mana[player],fee)]
            if min(mana[player])<0:
                raise ValueError('real casting budget is insufficient')
            world.move(ref,'stack')
            emit('assert_stack',value=2 if response else 1)

    def resolve(ref):
        emit('resolve')
        types=oracle[world.entities[ref]['name']]['type_line'].partition(' — ')[0].split()
        world.move(ref,'graveyard' if set(types)&{'Instant','Sorcery'} else 'battlefield')

    def match(prop,value):
        emit('assert_matching_permanents',player=owner,index=csvid,property=prop,value=value)

    def inspect(stack=0):
        emit('assert_stack',value=stack)
        emit('assert_choices_consumed')
        if extra=='loot':
            emit('assert_discard_choices',value=discard_receipts)
        names=sorted({obj['name'] for obj in world.entities.values()})
        for player in (0,1):
            emit('assert_life',player=player,value=life[player])
            for c,n in enumerate(mana[player]):
                emit('assert_mana',player=player,color=c,value=n)
            for zone in ('hand','battlefield','graveyard','exile','library'):
                observed=('hand','stack') if zone=='hand' else (zone,)
                count=Counter(obj['name'] for obj in world.entities.values() if obj['player']==player and obj['zone'] in observed)
                emit('assert_zone',player=player,zone=zone,value=sum(count.values()))
                if zone=='library':
                    for i,n in enumerate(world.zones[zone][player]):
                        emit('assert_zone_card',player=player,zone=zone,index=i,value=rows[n]['csvid'])
                else:
                    for n in names:
                        emit('assert_zone_multiplicity',player=player,zone=zone,index=rows[n]['csvid'],value=count[n])
        if world.entities['source']['zone']=='battlefield':
            match('artifact',1)
            match('token',0)
            match('color',color_mask if active else 0)
            emit('assert_card',ref='source',property='tapped',value=int(world.entities['source']['tapped']))
            emit('can_target_creature',ref='source',other='source',value=int(active))
            emit('can_target_creature',ref='foe_probe',other='source',value=int(active and p['extra']!='hexproof'))
            if p['creature_type'] not in SUBTYPE_WITNESSES:
                match('subtype_'+p['creature_type'],int(active))
            if active:
                bonus=growth+int(mode=='shared_subtype')
                emit('assert_card',ref='source',property='power',value=p['power']+bonus)
                emit('assert_card',ref='source',property='toughness',value=p['toughness']+bonus)
                emit('assert_card',ref='source',property='damage',value=damage.get('source',0))
        for ref in ('kin','nonkin','blocker'):
            obj=world.entities.get(ref)
            if obj and obj['zone']=='battlefield':
                card_data=oracle[obj['name']]
                for prop in ('power','toughness'):
                    emit('assert_card',ref=ref,property=prop,value=int(card_data[prop])+int(active and ref=='kin'))
                emit('assert_card',ref=ref,property='damage',value=damage.get(ref,0))

    def combat():
        fresh=mode=='fresh_attack'
        attempt=mode in ('keyword_combat','reach_block')
        blocked=attempt and (mode=='reach_block' or p['extra'] not in ('flying','unblockable'))
        emit('begin_attackers')
        emit('attack',ref='source',value=int(not fresh))
        if not fresh:
            world.entities['source']['tapped']=True
        emit('finish_attackers')
        emit('begin_blockers')
        if attempt:
            emit('block',ref='blocker',other='source',value=int(blocked))
        emit('advance_combat')
        if attempt:
            emit('assert_card',ref='source',property='blocked',value=int(blocked))
        emit('combat_damage')
        if blocked:
            blocker=oracle[world.entities['blocker']['name']]
            killed=p['power']>=int(blocker['toughness']) or p['extra']=='deathtouch'
            if killed:
                world.move('blocker','graveyard')
            else:
                damage['blocker']=p['power']
            if not (killed and p['extra']=='first_strike'):
                if int(blocker['power'])>=p['toughness']:
                    world.move('source','graveyard')
                else:
                    damage['source']=int(blocker['power'])
        elif not fresh:
            life[foe]-=p['power']
        emit('end_combat')
        inspect()

    def additional_combat():
        nonlocal discard_receipts

        fresh=mode=='fresh_attack'
        defending=mode in DEFENDING
        blocked=defending or mode in ('keyword_combat','trample_no_excess')
        attacker,blocker=('blocker','source') if defending else ('source','blocker')
        if mode in PREVENTED:
            cast('prevention')
            resolve('prevention')
        emit('begin_attackers')
        emit('attack',ref=attacker,value=int(not fresh))
        if not fresh:
            world.entities[attacker]['tapped']=True
        emit('finish_attackers')
        emit('begin_blockers')
        if blocked:
            emit('block',ref=blocker,other=attacker,value=1)
        emit('advance_combat')
        if blocked:
            emit('assert_card',ref=attacker,property='blocked',value=1)
        accepts=extra=='loot' and not fresh and not blocked and mode not in PREVENTED and mode!='loot_decline'
        if accepts and owner==0:
            emit('choose_trigger',player=owner,ref='source')
        emit('combat_damage')
        if not fresh and mode not in PREVENTED:
            points=p['power']*(2 if extra=='double_strike' else 1)
            if blocked:
                enemy=oracle[world.entities['blocker']['name']]
                toughness,power=int(enemy['toughness']),int(enemy['power'])
                if extra=='trample':
                    life[foe]-=max(0,points-toughness)
                if points>=toughness:
                    world.move('blocker','graveyard')
                else:
                    damage['blocker']=points
                if power>=p['toughness']:
                    world.move('source','graveyard')
                else:
                    damage['source']=power
            else:
                life[foe]-=points
            if extra=='lifelink':
                life[owner]+=points
        if extra=='loot':
            emit('resolve')
        if accepts:
            drawn=f'deck_{owner}_0'
            world.move(drawn,'hand')
            world.move('old_card' if mode=='loot_choice' else drawn,'graveyard')
            discard_receipts+=int(owner==0)
        emit('end_combat')
        inspect()

    if extra=='loot':
        emit('discard_policy',value=1)
    for player in (0,1):
        fund(player,[0]*6)
    if cast_source:
        if mode=='cast_countered':
            fund(foe,cost_from_oracle(oracle['Counterspell']))
        cast('source',allowed=mode!='cast_unfunded')
        if mode=='cast_countered':
            emit('announce_spell',ref='source')
            cast('counter',response=True,prefund=False)
            emit('resolve_response')
            world.move('source','graveyard')
            world.move('counter','graveyard')
        elif mode!='cast_unfunded':
            resolve('source')
        inspect()
    if mana_case:
        legal=mode!='wrong_mana_output'
        cast('spender',allowed=legal,live_mana=legal,prefund=False)
        if legal:
            inspect(stack=1)
            resolve('spender')
        inspect()
    elif mode not in ('cast_unfunded','cast_countered'):
        if mode=='shared_subtype':
            cast('coat')
            resolve('coat')
        budget=list(p['fee'])
        if mode=='short_fee':
            budget[0 if budget[0] else a]-=1
        if mode=='wrong_fee':
            budget[a]-=1
            budget[wrong]+=1
        fund(owner,budget)
        inspect()
        ready=len(actions)
        legal=mode not in ('short_fee','wrong_fee')
        emit('require_can_activate',ref='source',value=int(legal))
        if legal:
            emit('choose_activation_mode',ref='source',player=owner,mode_label='Become Dragon' if p['fee'][0] else 'Animate',
                 fallback_color=a,allow_auto_nonmana=True)
        emit('activate',ref='source',value=int(legal))
        if legal:
            emit('require_activation_mode',value=1)
            mana[owner]=[have-need for have,need in zip(mana[owner],p['fee'])]
            inspect(stack=1)
            pending=len(actions)
            emit('resolve')
            emit('refresh')
            active=True
        inspect()
        if mode in ('fresh_attack','attack','keyword_combat','reach_block','trample_no_excess','loot_decline','loot_choice') or mode in DEFENDING|PREVENTED:
            if extra in EXTENDED_COMBAT:
                additional_combat()
            else:
                combat()
        if mode=='hexproof_casts':
            fund(foe,cost_from_oracle(oracle['Unsummon']))
            emit('require_can_cast',ref='foe_probe',value=0)
            cast('foe_probe',allowed=False)
            cast('own_probe','source')
            resolve('own_probe')
            growth=3
            inspect()
        if mode=='destroy_animated':
            cast('removal','source')
            resolve('removal')
            world.move('source','graveyard')
            inspect()
        if mode in ('cleanup','shared_subtype','hexproof_casts'):
            emit('cleanup')
            emit('refresh')
            active=False
            growth=0
            inspect()
        if mode in ('ready_replay','pending_replay'):
            cut=ready if mode=='ready_replay' else pending
            tail=actions[cut:]
            actions=actions[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]

    gaps=['Other type-changing layers, control changes, animation cost modifiers, and multiplayer.']
    if extra=='loot':
        gaps+=['Voluntary decline through the native AI trigger policy, additional effects granting source noncombat damage.']
    case.update(name=f'DualManaArtifactAnimationRecipe_{csvid}_{variant}',steps=actions,
                _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],
                             covered_clauses=p['clauses'],untested_components=gaps))
    return case


def expand(name,csvid,d,p,selected,rows,oracle):
    if csvid!=p['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(p)):
        raise ValueError('explicit reviewed artifact animation variants required')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,d,p,v,rows,oracle) for v in selected]
