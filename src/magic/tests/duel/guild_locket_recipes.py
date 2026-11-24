"""Both printed mana options and the hybrid sacrifice-to-draw ability."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from entry_aura_freeze_recipes import FreezeLedger
from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'guild_locket'
DATA = Path(__file__).parent / 'data/guild_locket_wave8_profiles.json'
COLORS = 'CBUGRW'
SPENDERS = dict(B='Typhoid Rats', U='Mist-Cloaked Herald', G='Elvish Mystic', R='Raging Goblin', W='Boros Elite')
MANA = re.compile(r'\{T\}: Add \{([BUGRW])\} or \{([BUGRW])\}\.')
DRAW_TAIL = ', {T}, Sacrifice this artifact: Draw two cards.'


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') != 'Artifact' or record.get('keywords') != [] or record.get('mana_cost') != '{3}'):
        raise ValueError('normal whole noncreature artifact with printed cost {3} required')
    clauses = record['oracle_text'].splitlines()
    match = MANA.fullmatch(clauses[0]) if len(clauses)==2 else None
    if not match or match[1]==match[2]:
        raise ValueError('exactly two different printed mana choices required')
    pair = list(match.groups())
    if clauses[1] != ('{'+'/'.join(pair)+'}')*4+DRAW_TAIL:
        raise ValueError('complete matching four-hybrid sacrifice action required')
    return dict(colors=pair, quantity=4, cards_drawn=2, cast_mana=cost_from_oracle(record), clauses=clauses)


def spec(declaration, record):
    model = mechanism(record)
    pin = pins()['sources'].get(record['name'])
    if (not pin or not exact_pin(pin['oracle'],record) or declaration.get('family') != FAMILY
            or not exact_pin(declaration.get('version'),2) or declaration.get('oracle_text') != record['oracle_text']
            or not exact_pin(declaration.get('params'),dict(schema=1,mechanism=model))):
        raise ValueError('independent whole Oracle and explicit Locket model required')
    return dict(model,csvid=pin['csvid'],family=FAMILY)


def variants(model):
    shared = ['mana_first','mana_second','entry_mana','mana_replay','wrong_mana_output','spent_source_denied',
              'tapped_denied','cast_insufficient','cast_countered','draw_reserve','draw_ready_replay','draw_pending_replay']
    exact = [f'draw_{model["quantity"]-count}_{count}' for count in range(model['quantity']+1)]
    negative = ['draw_no_mana', 'draw_one_short', 'draw_wrong_color']
    # Production drives the AI through the named mode's native auto path. On
    # the human path there is a second payment UI if several legal bundles exist.
    return [f'{mode}_{owner}' for owner in (0,1) for mode in (*shared,*exact,*negative)] + ['take_mana_with_draw_0']


def validate_witnesses(oracle, rows):
    for group, records in pins().items():
        for name,pin in records.items():
            row = rows.get(name,{})
            if (not exact_pin(pin['oracle'],oracle.get(name)) or row.get('kind')!='card'
                    or row.get('csvid')!=pin['csvid'] or row.get('handler')!=pin['handler']
                    or row['handler'] in (None,'NULL','0','card_dummy')
                    or group=='sources' and row['handler']!='manalinkeh_gnr_locket'):
                raise ValueError('implemented canonical card binding changed: '+name)


def _case(name, csvid, declaration, model, variant, rows, oracle):
    from recipes import card, cost_from_oracle, step

    mode, side=variant.rsplit('_',1)
    owner=int(side)
    foe=1-owner
    a,b=[COLORS.index(c) for c in model['colors']]
    unrelated=next(c for c in range(1,6) if c not in (a,b))
    casting=mode in ('entry_mana','cast_insufficient','cast_countered')
    source=card(name,'source',owner,'hand' if casting else 'battlefield')
    if mode=='tapped_denied':
        source['state']=['tapped']
    setup=[source]
    spend=mode in ('mana_first','mana_second','entry_mana','mana_replay','spent_source_denied','wrong_mana_output')
    output=b if mode=='mana_second' else unrelated if mode=='wrong_mana_output' else a
    if spend:
        setup.append(card(SPENDERS[COLORS[output]],'spender',owner,'hand'))
    if mode=='cast_countered':
        setup.append(card('Counterspell','counter',foe,'hand'))
    library=['Forest','Island','Mountain','Swamp']
    for player in (0,1):
        setup.append(card('Plains',f'existing_{player}',player,'hand'))
        setup += [card(n,f'deck_{player}_{i}',player,'library') for i,n in enumerate(library)]
    case=dict(card=name,fixture='empty_main',seed=123,setup=setup)
    world=FreezeLedger(case,model,mode,owner,rows,oracle)
    actions=[step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)]
    mana=[[0]*6,[0]*6]

    def emit(op,**values):
        actions.append(step(op,**values))

    def fund(player, pool):
        mana[player]=list(pool)
        for color,value in enumerate(pool):
            emit('set_mana',player=player,color=color,value=value)
        emit('refresh')

    def inspect(stack=0):
        emit('assert_stack',value=stack)
        emit('assert_choices_consumed')
        all_names=sorted({obj['name'] for obj in world.entities.values()})
        for player in (0,1):
            emit('assert_life',player=player,value=20)
            for color,value in enumerate(mana[player]):
                emit('assert_mana',player=player,color=color,value=value)
            for zone in ('hand','battlefield','graveyard','exile','library'):
                # The raw visible-card hand observer includes pending spells.
                observed=('hand','stack') if zone=='hand' else (zone,)
                counts=Counter(obj['name'] for obj in world.entities.values()
                               if obj['player']==player and obj['zone'] in observed)
                emit('assert_zone',player=player,zone=zone,value=sum(counts.values()))
                if zone=='library':
                    for i,n in enumerate(world.zones[zone][player]):
                        emit('assert_zone_card',player=player,zone=zone,index=i,value=rows[n]['csvid'])
                else:
                    for n in all_names:
                        emit('assert_zone_multiplicity',player=player,zone=zone,index=rows[n]['csvid'],value=counts[n])
        if world.entities['source']['zone']=='battlefield':
            emit('assert_card',ref='source',property='in_play',value=1)
            emit('assert_card',ref='source',property='tapped',value=int(world.entities['source']['tapped']))

    def cast(ref, *, allowed=True, live_mana=False, response=False):
        item=world.entities[ref]
        player=item['player']
        fee=cost_from_oracle(oracle[item['name']])
        if live_mana:
            if fee[output]!=1 or sum(fee)!=1 or any(mana[player]):
                raise ValueError('the Locket must be the only payer of one specific mana')
            if owner==0:
                emit('choose_mana_source',ref='source')
            mana[player][output]+=1
            world.entities['source']['tapped']=True
        emit('cast_response' if response else 'cast',ref=ref,value=int(allowed))
        if allowed:
            mana[player]=[have-need for have,need in zip(mana[player],fee)]
            if min(mana[player])<0:
                raise ValueError('case must fund the ordinary printed spell cost')
            world.move(ref,'stack')
            emit('assert_stack',value=2 if response else 1)

    def mode_intent(label,color):
        emit('choose_activation_mode',ref='source',player=owner,mode_label=label,fallback_color=color,
             allow_auto_nonmana=label=='Draw 2 cards')

    fund(0,[0]*6)
    fund(1,[0]*6)
    if casting:
        if mode!='cast_insufficient':
            fund(owner,model['cast_mana'])
        if mode=='cast_countered':
            fund(foe,cost_from_oracle(oracle['Counterspell']))
        cast('source',allowed=mode!='cast_insufficient')
        if mode=='cast_countered':
            emit('announce_spell',ref='source')
            cast('counter',response=True)
            emit('resolve_response')
            world.move('source','graveyard')
            world.move('counter','graveyard')
        elif mode=='entry_mana':
            emit('resolve')
            world.move('source','battlefield')
        inspect()

    if spend:
        ready=len(actions)
        if mode=='wrong_mana_output':
            cast('spender',allowed=False)
        else:
            emit('require_can_activate',ref='source',value=1)
            cast('spender',live_mana=True)
            inspect(stack=1)
            emit('resolve')
            world.move('spender','battlefield')
            if mode=='spent_source_denied':
                emit('require_can_activate',ref='source',value=0)
                emit('activate',ref='source',value=0)
        inspect()
    elif mode=='tapped_denied':
        budget=[0]*6
        budget[a]=model['quantity']
        fund(owner,budget)
        emit('require_can_activate',ref='source',value=0)
        emit('activate',ref='source',value=0)
        inspect()
    elif mode.startswith('draw_') or mode=='take_mana_with_draw':
        budget=[0]*6
        exact=re.fullmatch(r'draw_(\d)_(\d)',mode)
        if exact:
            budget[a],budget[b]=map(int,exact.groups())
        elif mode in ('draw_reserve','draw_ready_replay','draw_pending_replay','take_mana_with_draw'):
            # A separately asserted reserve case proves the cost is still four.
            output_color=b if b==5 else a
            budget[output_color]=model['quantity']+1
        elif mode=='draw_one_short':
            budget[a]=model['quantity']-1
        elif mode=='draw_wrong_color':
            budget[unrelated]=model['quantity']
        fee=model['quantity']
        affordable=budget[a]+budget[b]>=fee
        choose_mana=mode=='take_mana_with_draw'
        draw=affordable and not choose_mana
        fund(owner,budget)
        inspect()
        ready=len(actions)
        emit('require_can_activate',ref='source',value=1)
        paid_a=min(budget[a],fee)
        ai_mana_only=owner==1 and not affordable
        white_human_mana_only=owner==0 and 5 in (a,b) and not affordable
        if not ai_mana_only:
            mode_intent('Get mana' if choose_mana or white_human_mana_only else 'Draw 2 cards',a)
        if choose_mana or ai_mana_only or white_human_mana_only:
            emit('choose_color',player=owner,value=a)
        if draw and owner==0:
            payment=(paid_a << (4*a)) | ((fee-paid_a) << (4*b))
            emit('choose_mode_payment',player=owner,value=payment)
        emit('activate',ref='source',value=1)
        if ai_mana_only:
            # Leave the ensuing informational confirmation to the normal host.
            emit('assert_unused_choices',value=0)
        else:
            emit('require_activation_mode',value=int(draw or choose_mana))
        if draw:
            mana[owner][a]-=paid_a
            mana[owner][b]-=fee-paid_a
            world.move('source','graveyard')
            inspect(stack=1)
            pending=len(actions)
            emit('resolve')
            for i in range(model['cards_drawn']):
                world.move(f'deck_{owner}_{i}','hand')
        else:
            world.entities['source']['tapped']=True
            mana[owner][a]+=1
        inspect()

    if mode in ('mana_replay','draw_ready_replay','draw_pending_replay'):
        cut=pending if mode=='draw_pending_replay' else ready
        tail=actions[cut:]
        actions=actions[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
    case.update(name=f'GuildLocketRecipe_{csvid}_{variant}',steps=actions,
                _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                oracle_text=declaration['oracle_text'],covered_clauses=model['clauses'],
                untested_components=['Modified activation costs, AI choice to prefer mana when both modes are legal, draw replacement, empty-library loss, and multiplayer.']))
    return case


def expand(name, csvid, declaration, model, selected, rows, oracle):
    if csvid!=model['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('explicit distinct reviewed Locket variants required')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,declaration,model,variant,rows,oracle) for variant in selected]
