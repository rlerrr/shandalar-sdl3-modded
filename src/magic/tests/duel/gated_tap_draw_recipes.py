"""Whole-card draw abilities gated by a real hand or casting history."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from entry_aura_freeze_recipes import FreezeLedger
from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'gated_tap_draw'
DATA = Path(__file__).parent / 'data/gated_tap_draw_wave8_profiles.json'
GATES = {'you have exactly seven cards in hand': 7, 'you have no cards in hand': 0,
         "you've cast a noncreature spell this turn": None}
MANA_CLAUSES = {'{T}: Add {C}.', '{T}: Add {C}. ({C} represents colorless mana.)'}
DRAW = re.compile(r'(?:(\{[1-9]\})(\{C\})?, )?\{T\}: Draw a card(, then discard a card)?\. '
                  r'Activate(?: this ability)? only if (.+)\.')


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('keywords') != []):
        raise ValueError('normal whole record without additional keywords required')
    card_type = record['type_line'].partition(' — ')[0]
    if card_type not in ('Creature', 'Artifact', 'Land'):
        raise ValueError('ordinary creature, artifact, or land required')
    if card_type == 'Creature' and any(not re.fullmatch(r'[0-9]+', record.get(k, '')) for k in ('power', 'toughness')):
        raise ValueError('numeric printed creature characteristics required')
    clauses = record['oracle_text'].splitlines()
    mana_mode = bool(clauses and clauses[0] in MANA_CLAUSES)
    draw = DRAW.fullmatch(clauses[-1]) if len(clauses) == 1 + mana_mode else None
    if draw is None or draw[4] not in GATES:
        raise ValueError('complete conditional tap-draw text required')
    if card_type == 'Land' and record['mana_cost']:
        raise ValueError('ordinary land must have no printed mana cost')
    return dict(card_type=card_type, cast_mana=[0]*6 if card_type == 'Land' else cost_from_oracle(record),
                generic=int(draw[1][1:-1]) if draw[1] else 0, colorless=int(bool(draw[2])),
                hand_size=GATES[draw[4]], mana_mode=mana_mode, discard=bool(draw[3]), clauses=clauses)


def spec(declaration, record):
    model = mechanism(record)
    pin = pins()['sources'].get(record['name'])
    if (not pin or not exact_pin(record, pin['oracle']) or declaration.get('family') != FAMILY
            or not exact_pin(declaration.get('version'), 2)
            or declaration.get('oracle_text') != record['oracle_text']
            or not exact_pin(declaration.get('params'), dict(schema=1, mechanism=model))
            or model['mana_mode'] != bool(pin['mana_label'])):
        raise ValueError('independent complete Oracle, declaration, and activation-mode pins required')
    return dict(model, csvid=pin['csvid'], mana_label=pin['mana_label'], family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (not exact_pin(oracle.get(name), pin['oracle']) or row.get('kind') != 'card'
                    or row.get('csvid') != pin['csvid'] or row.get('handler') != pin['handler']
                    or row['handler'] in (None, 'NULL', '0')
                    or row['handler'] == 'card_dummy' and (group == 'sources' or name not in data['static_witnesses'])):
                raise ValueError('reviewed ordinary card binding changed: ' + name)


def variants(model):
    modes = ['effect', 'entry', 'tapped_denied', 'repeat_denied', 'offturn',
             'ready_replay', 'pending_replay', 'source_removed']
    if model['card_type'] != 'Land':
        modes.append('cast_unfunded')
    if model['card_type'] == 'Creature':
        modes.append('fresh_denied')
    if model['hand_size'] is None:
        modes += ['no_spell', 'creature_spell', 'other_caster', 'prior_turn', 'countered_spell']
    else:
        modes += ['hand_above', 'hand_enters_limit', 'opponent_hand']
        if model['hand_size'] == 7:
            modes += ['hand_below', 'hand_leaves_limit', 'pending_hand_changes']
    if model['mana_mode']:
        modes += ['mana_at_limit', 'mana_outside_limit']
    if model['generic']:
        modes.append('insufficient_mana')
    if model['colorless']:
        modes += ['exact_colorless', 'exact_mixed', 'wrong_color']
    cells = [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]
    if model['discard']:
        cells.append('discard_original_0')
    return cells


def _case(name, csvid, declaration, model, variant, rows, oracle):
    from recipes import card, cost_from_oracle, step

    mode, side = variant.rsplit('_', 1)
    owner = int(side)
    foe = 1-owner
    hand_size = model['hand_size']
    spell_gate = hand_size is None
    land = model['card_type'] == 'Land'
    creature = model['card_type'] == 'Creature'
    casting = mode in ('entry', 'cast_unfunded', 'fresh_denied')
    source = card(name, 'source', owner, 'hand' if casting else 'battlefield')
    if mode == 'tapped_denied':
        source['state'] = ['tapped']
    setup = [source]
    fog = (spell_gate and mode not in ('no_spell', 'creature_spell', 'cast_unfunded')
           and (mode != 'entry' or creature)) or mode in ('hand_enters_limit', 'hand_leaves_limit', 'pending_hand_changes')
    fog_owner = foe if mode == 'other_caster' else owner
    if fog:
        setup.append(card('Fog', 'fog', fog_owner, 'hand'))
    if mode == 'countered_spell':
        setup.append(card('Counterspell', 'counter', foe, 'hand'))
    if mode == 'creature_spell':
        setup.append(card('Grizzly Bears', 'bear', owner, 'hand'))
    if mode == 'discard_original':
        setup.append(card('Forest', 'original', owner, 'hand'))
    if not spell_gate:
        fillers = hand_size - int(mode in ('pending_hand_changes', 'hand_leaves_limit'))
        if mode in ('hand_above', 'opponent_hand', 'mana_outside_limit'):
            fillers += 1
        elif mode == 'hand_below':
            fillers -= 1
        setup += [card('Plains', f'own_hand_{i}', owner, 'hand') for i in range(fillers)]
    other_hand = hand_size if mode == 'opponent_hand' else 2
    setup += [card('Swamp', f'other_hand_{i}', foe, 'hand') for i in range(other_hand)]
    for player in (0, 1):
        setup += [card(card_name, f'deck_{player}_{i}', player, 'library')
                  for i, card_name in enumerate(('Mountain', 'Island', 'Forest'))]
    case = dict(card=name, fixture='empty_main', seed=123, setup=setup)
    world = FreezeLedger(case, model, mode, owner, rows, oracle)
    actions = []
    mana = [[0]*6, [0]*6]
    spell_count = [0, 0]
    current = foe if mode == 'offturn' else owner
    sick = False
    discarded = 0

    def emit(op, **values):
        actions.append(step(op, **values))

    def fund(player, pool):
        mana[player] = list(pool)
        for color, amount in enumerate(pool):
            emit('set_mana', player=player, color=color, value=amount)
        emit('refresh')

    def cast(ref, *, allowed=True, pending=0, response=False, resolve=True):
        obj = world.entities[ref]
        record = oracle[obj['name']]
        player = obj['player']
        types = record['type_line'].partition(' — ')[0].split()
        fee = [0]*6 if 'Land' in types else cost_from_oracle(record)
        if allowed:
            fund(player, [have + need for have, need in zip(mana[player], fee)])
        emit('cast_response' if response else 'cast', ref=ref, value=int(allowed))
        if not allowed:
            return
        emit('assert_stack', value=pending+1)
        mana[player] = [have - need for have, need in zip(mana[player], fee)]
        spell_count[player] += int('Land' not in types and 'Creature' not in types)
        world.move(ref, 'stack')
        if resolve:
            emit('resolve')
            world.move(ref, 'graveyard' if set(types) & {'Instant', 'Sorcery'} else 'battlefield')

    def visit():
        nonlocal current, sick

        if any(any(pool) for pool in mana):
            raise ValueError('turn boundary requires both actual pools empty')
        current = 1-current
        emit('advance_turn')
        emit('untap_step', player=current)
        emit('turn', player=current, value=0)
        spell_count[:] = [0, 0]
        if current == owner:
            sick = False

    def inspect(stack=0):
        emit('assert_stack', value=stack)
        emit('assert_choices_consumed')
        if model['discard']:
            emit('assert_discard_choices', value=discarded)
        if world.entities['source']['zone'] == 'battlefield':
            emit('assert_card', ref='source', property='in_play', value=1)
            emit('assert_card', ref='source', property='tapped', value=int(world.entities['source']['tapped']))
            if creature:
                for key in ('power', 'toughness'):
                    emit('assert_card', ref='source', property=key, value=int(oracle[name][key]))
                emit('assert_card', ref='source', property='sick', value=int(sick))
        universe = sorted({obj['name'] for obj in world.entities.values()})
        for player in (0, 1):
            emit('assert_life', player=player, value=20)
            for color, amount in enumerate(mana[player]):
                emit('assert_mana', player=player, color=color, value=amount)
            for zone in ('hand', 'battlefield', 'library', 'graveyard', 'exile'):
                emit('assert_zone', zone=zone, player=player, value=world.count(player, zone))
                if zone == 'library':
                    for i, card_name in enumerate(world.zones[zone][player]):
                        emit('assert_zone_card', zone=zone, player=player, index=i, value=rows[card_name]['csvid'])
                    continue
                names = (obj['name'] for obj in world.entities.values() if obj['zone'] == zone and obj['player'] == player)
                counts = Counter(names)
                for card_name in universe:
                    emit('assert_zone_multiplicity', zone=zone, player=player, index=rows[card_name]['csvid'], value=counts[card_name])

    emit('turn', player=current, value=0)
    emit('present_ai_dialogs', value=1)
    if model['discard']:
        emit('discard_policy', value=1)
    for player in (0, 1):
        fund(player, [0]*6)
    if casting:
        cast('source', allowed=mode != 'cast_unfunded')
        if mode == 'cast_unfunded':
            inspect()
        else:
            sick = creature
            inspect()
            if mode == 'entry' and creature:
                visit()
                visit()

    if mode != 'cast_unfunded':
        if mode == 'countered_spell':
            cast('fog', resolve=False)
            emit('announce_spell', ref='fog')
            cast('counter', pending=1, response=True, resolve=False)
            emit('resolve_response')
            world.move('fog', 'graveyard')
            world.move('counter', 'graveyard')
        elif fog and mode != 'pending_hand_changes':
            cast('fog')
        elif mode == 'creature_spell':
            cast('bear')
        if mode == 'prior_turn':
            visit()

        fee = model['generic'] + model['colorless']
        budget = [fee + model['colorless'], 0, 0, 0, 0, 0]
        if mode == 'exact_colorless':
            budget[0] = fee
        elif mode == 'exact_mixed':
            budget[0], budget[2] = model['colorless'], model['generic']
        elif mode == 'wrong_color':
            budget[0], budget[2] = 0, fee
        elif mode == 'insufficient_mana':
            budget[0] = 0 if model['colorless'] else fee-1
        fund(owner, budget)
        inspect()
        ready = len(actions)

        gate = bool(spell_count[owner]) if spell_gate else world.count(owner, 'hand') == hand_size
        can_tap = not sick and not world.entities['source']['tapped']
        draw_legal = can_tap and gate and sum(budget) >= fee and budget[0] >= model['colorless']
        take_mana = model['mana_mode'] and can_tap and (mode.startswith('mana_') or not draw_legal)
        legal = draw_legal or take_mana
        emit('require_can_activate', ref='source', value=int(legal))
        # The Wreckage's AI menu stays visible when only mana is legal.
        ai_mana_only = csvid == 15674 and owner == 1 and take_mana and not draw_legal
        wants_draw = mode != 'mana_at_limit' and not ai_mana_only
        mode_probe = model['mana_mode'] and legal
        if mode_probe:
            emit('choose_activation_mode', ref='source', player=owner, fallback_color=0, allow_auto_nonmana=False,
                 mode_label='Draw a card' if wants_draw else model['mana_label'])
        if mode == 'exact_mixed' and owner == 0:
            for _ in range(model['generic']):
                emit('choose_mana', player=owner, color=2)
        emit('activate', ref='source', value=int(legal))
        if mode_probe:
            emit('require_activation_mode', value=int(draw_legal or ai_mana_only))
        if legal:
            world.entities['source']['tapped'] = True

        if take_mana:
            mana[owner][0] += 1
        elif draw_legal:
            colorless_spent = min(fee, budget[0])
            mana[owner][0] -= colorless_spent
            mana[owner][2] -= fee-colorless_spent
            inspect(stack=1)
            pending_cut = len(actions)
            if mode == 'source_removed':
                emit('destroy', ref='source')
                world.move('source', 'graveyard')
                inspect(stack=1)
            if mode == 'pending_hand_changes':
                cast('fog', pending=1, resolve=False)
            emit('resolve')
            if mode == 'pending_hand_changes':
                world.move('fog', 'graveyard')
            drawn = f'deck_{owner}_0'
            world.move(drawn, 'hand')
            if model['discard']:
                world.move('original' if mode == 'discard_original' else drawn, 'graveyard')
                discarded += int(owner == 0)
        if mode == 'repeat_denied':
            fund(owner, budget)
            emit('require_can_activate', ref='source', value=0)
            emit('activate', ref='source', value=0)
        inspect()
        if mode in ('ready_replay', 'pending_replay'):
            cut = ready if mode == 'ready_replay' else pending_cut
            tail = actions[cut:]
            actions = actions[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(tail) + [step('assert_recorded_state')]

    case.update(name=f'GatedTapDrawRecipe_{csvid}_{variant}', steps=actions,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                             untested_components=['Draw replacement, empty-library loss, control changes, land-play quotas, and multiplayer.']))
    return case


def expand(name, csvid, declaration, model, selected, rows, oracle):
    if csvid != model['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(model)):
        raise ValueError('explicit distinct reviewed variants required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, model, variant, rows, oracle) for variant in selected]
