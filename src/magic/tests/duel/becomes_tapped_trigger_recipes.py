"""Whole-Oracle self-tap triggers from actual spells, combat, and costs."""
import copy
import json
import re
from pathlib import Path

from entry_aura_freeze_recipes import FreezeLedger
from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'becomes_tapped_trigger'
DATA = Path(__file__).parent / 'data/becomes_tapped_trigger_wave8_profiles.json'
TRIGGER = 'Whenever this creature becomes tapped, '
EFFECTS = {
    'you may draw a card.': 'draw',
    'you gain 1 life.': 'gain',
    'each opponent loses 1 life and you gain 1 life.': 'drain',
    'it deals 1 damage to each opponent.': 'damage',
    'you may discard a card. If you do, draw a card.': 'loot',
    "target creature can't block this turn.": 'block',
    "you may pay {1}{U}. If you do, return target permanent to its owner's hand.": 'paid_bounce',
}
OPTIONAL = {'draw', 'loot'}
UNPAYABLE_COST = ('insufficient_mana', 'wrong_color')
PAID_BOUNCE_MODES = ('bounce_land', 'bounce_artifact', 'bounce_enchantment', 'bounce_source',
                     'owned_by_other', *UNPAYABLE_COST)
UNPAID_BOUNCE = ('decline', *UNPAYABLE_COST)


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    if (not record or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or not record['type_line'].startswith('Creature — ')
            or any(not re.fullmatch(r'[0-9]+', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('whole normal creature with printed numeric characteristics required')
    clauses = record['oracle_text'].splitlines()
    keyword = clauses[0] if clauses and clauses[0] in ('Flying', 'First strike') else None
    body = clauses[1:] if keyword else clauses
    effect = EFFECTS.get(body[0].removeprefix(TRIGGER)) if len(body) == 1 and body[0].startswith(TRIGGER) else None
    if not effect or record.get('keywords') != ([keyword] if keyword else []):
        raise ValueError('complete supported tap trigger and keyword required')
    model = dict(effect=effect, keyword=keyword, clauses=clauses)
    if effect == 'paid_bounce':
        from recipes import cost_from_oracle

        cost = body[0].removeprefix(TRIGGER).split('pay ', 1)[1].split('.', 1)[0]
        model['trigger_mana'] = cost_from_oracle({'mana_cost': cost})
    return model


def spec(declaration, record):
    from recipes import cost_from_oracle

    model = mechanism(record)
    source = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int
            or params['schema'] != 1 or not exact_pin(params['mechanism'], model)):
        raise ValueError('whole independent source and trigger pins required')
    return dict(model, csvid=source['csvid'], mana=cost_from_oracle(record), family=FAMILY)


def variants(model):
    modes = ['entry', 'spell_tap', 'other_taps', 'already_tapped', 'untap_retap', 'opponent_turn',
             'attack', 'fresh_attack', 'cost_tap', 'cast_countered', 'ready_replay', 'pending_replay']
    if model['effect'] in OPTIONAL:
        modes.append('decline')
    if model['effect'] in ('damage', 'drain'):
        modes += ['prevent_all_damage', 'prevent_combat_damage']
    if model['effect'] == 'block':
        modes.append('block_expires')
    if model['keyword'] == 'Flying':
        modes += ['ground_block_denied', 'reach_block']
    if model['keyword'] == 'First strike':
        modes.append('first_strike')
    if model['effect'] == 'paid_bounce':
        modes += PAID_BOUNCE_MODES
    cells = [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]
    if model['effect'] == 'paid_bounce':
        cells.append('decline_0')
    return cells


def validate_witnesses(oracle, rows):
    data = pins()
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('kind') != 'card'
                    or row.get('csvid') != pin['csvid'] or row.get('handler') != pin['handler']
                    or row.get('handler') in (None, 'NULL', '0')
                    or (row['handler'] == 'card_dummy' and
                        (group == 'sources' or name not in data['static_witnesses']))):
                raise ValueError('independent card identity or implementation changed: ' + name)


def _case(name, csvid, declaration, model, variant, rows, oracle):
    from recipes import card, cost_from_oracle, step

    mode, side = variant.rsplit('_', 1)
    owner = int(side)
    foe = 1 - owner
    effect = model['effect']
    setup = [card(name, 'source', owner, 'hand')]
    if effect == 'loot':
        setup.append(card('Plains', 'fodder', owner, 'hand'))
    twiddle_player = foe if effect == 'loot' or mode == 'opponent_turn' else owner
    paid_bounce = effect == 'paid_bounce'
    mana_witnesses = []
    bounce_targets = []
    if paid_bounce:
        target, controller = {
            'bounce_land': ('Forest', owner),
            'bounce_artifact': ('Darksteel Relic', foe),
            'bounce_enchantment': ('Earthcraft', owner),
        }.get(mode, ('Horned Turtle', foe))
        if mode == 'bounce_source':
            bounce_targets.append('source')
        else:
            fixture_target = card(target, 'bounce_target', controller)
            if mode == 'owned_by_other':
                fixture_target['state'] = ['untapped', 'opponent_owned']
            elif mode == 'bounce_land' and owner == 1:
                # Keep the target legal without offering the AI a third payer.
                fixture_target['state'] = ['tapped']
            setup.append(fixture_target)
            bounce_targets.append('bounce_target')
        if mode == 'untap_retap':
            setup.append(card('Forest', 'retap_target', foe))
            bounce_targets.append('retap_target')
        if mode in UNPAYABLE_COST:
            twiddle_player = foe
            lands = [('Island', 2)] if mode == 'insufficient_mana' else [('Mountain', 4)] * 2
        else:
            lands = [('Wastes', 0), ('Island', 2)]
        for i, (land, color) in enumerate(lands):
            ref = f'trigger_land_{i}'
            setup.append(card(land, ref, owner))
            mana_witnesses.append((ref, color))
        offered = [sum(color == i for _, color in mana_witnesses) for i in range(6)]
        if mode not in UNPAYABLE_COST and offered != model['trigger_mana']:
            raise ValueError('payment witnesses must provide the complete trigger cost')
    for player in (0, 1):
        setup.extend(card(n, f'deck_{player}_{i}', player, 'library') for i, n in enumerate(('Mountain', 'Swamp', 'Island')))
    if effect == 'block' or mode in ('ground_block_denied', 'reach_block', 'first_strike'):
        blocker = {'ground_block_denied': 'Horned Turtle', 'reach_block': 'Giant Spider', 'first_strike': 'Grizzly Bears'}.get(mode, 'Horned Turtle')
        setup.append(card(blocker, 'blocker', foe))
    if mode in ('other_taps', 'block_expires'):
        setup.append(card('Hill Giant', 'ally', owner))
    if mode == 'cast_countered':
        setup.append(card('Counterspell', 'counter', foe, 'hand'))
    if mode in ('spell_tap', 'opponent_turn', 'other_taps', 'already_tapped', 'untap_retap',
                'ready_replay', 'pending_replay', 'decline', 'prevent_all_damage', 'block_expires',
                *PAID_BOUNCE_MODES):
        setup.append(card('Twiddle', 'tap_spell', twiddle_player, 'hand'))
    if mode in ('already_tapped', 'untap_retap'):
        setup.append(card('Twiddle', 'second_spell', twiddle_player, 'hand'))
    if mode == 'untap_retap':
        setup.append(card('Twiddle', 'third_spell', twiddle_player, 'hand'))
    if mode == 'prevent_all_damage':
        setup.append(card('Safe Passage', 'shield', foe, 'hand'))
    if mode == 'prevent_combat_damage':
        setup.append(card('Fog', 'shield', foe, 'hand'))
    if mode == 'cost_tap':
        setup.append(card('Earthcraft', 'engine', owner, 'hand'))
        setup.append(dict(card('Forest', 'land', owner), state=['tapped']))
    case = dict(card=name, fixture='empty_main', seed=123, setup=setup)
    world = FreezeLedger(case, model, mode, owner, rows, oracle)
    steps = [step('turn', player=owner, value=0), step('present_ai_dialogs', value=1)]
    if effect == 'loot':
        steps.append(step('discard_policy', value=1))
    life = [20, 20]
    mana = [[0] * 6 for _ in (0, 1)]
    damage = {}
    discard_count = 0
    bounce_count = 0
    current = owner
    restricted = False

    def emit(op, **values):
        steps.append(step(op, **values))

    def fund(player, pool):
        mana[player] = list(pool)
        for color, amount in enumerate(pool):
            emit('set_mana', player=player, color=color, value=amount)
        emit('refresh')

    def cast(ref, response=False, target=None):
        obj = world.entities[ref]
        player = obj['player']
        fee = cost_from_oracle(oracle[obj['name']])
        if not response:
            fund(player, fee)
        if mana[player] != fee:
            raise ValueError('real cast requires exact printed fixture payment')
        if target:
            emit('choose_target', player=player, ref=target)
        emit('cast_response' if response else 'cast', ref=ref, value=1)
        emit('assert_stack', value=2 if response else 1)
        mana[player] = [0] * 6
        world.move(ref, 'stack')

    def receipts():
        emit('assert_choices_consumed')
        emit('assert_stack', value=0)
        if effect == 'loot':
            emit('assert_discard_choices', value=discard_count)
        names = sorted({obj['name'] for obj in world.entities.values()})
        for player in (0, 1):
            emit('assert_life', player=player, value=life[player])
            for color, amount in enumerate(mana[player]):
                emit('assert_mana', player=player, color=color, value=amount)
            for zone in ('battlefield', 'hand', 'graveyard', 'exile', 'library'):
                emit('assert_zone', player=player, zone=zone, value=world.count(player, zone))
                if zone == 'library':
                    for index, n in enumerate(world.zones[zone][player]):
                        emit('assert_zone_card', player=player, zone=zone, index=index, value=rows[n]['csvid'])
                    continue
                for n in names:
                    count = (sum(obj['name'] == n and obj['player'] == player and obj['zone'] == zone for obj in world.entities.values())
                             if zone == 'battlefield' else world.zones[zone][player].count(n))
                    emit('assert_zone_multiplicity', player=player, zone=zone, index=rows[n]['csvid'], value=count)
        for ref, obj in world.entities.items():
            if obj['zone'] != 'battlefield':
                continue
            emit('assert_card', ref=ref, property='tapped', value=int(obj['tapped']))
            record = oracle[obj['name']]
            if 'Creature' in record['type_line'].partition(' — ')[0].split():
                for prop in ('power', 'toughness'):
                    emit('assert_card', ref=ref, property=prop, value=int(record[prop]))
                emit('assert_card', ref=ref, property='damage', value=damage.get(ref, 0))

    def trigger_choices():
        if effect in OPTIONAL:
            emit('choose_dialog', player=owner, value=int(mode == 'decline'))
        elif effect == 'block':
            emit('choose_target', player=owner, ref='blocker')
        elif paid_bounce:
            # This legacy tap callback opens its actual mana window before its
            # target selector. The human can cancel a real optional payment.
            if mode in UNPAID_BOUNCE:
                if owner == 0:
                    emit('choose_cancel_mana', player=owner)
                return
            if owner == 0:
                for ref, _ in mana_witnesses:
                    emit('choose_mana_source', ref=ref)
            emit('choose_target', player=owner, ref=bounce_targets[bounce_count])

    def trigger_effect():
        nonlocal discard_count, restricted, bounce_count

        if effect in OPTIONAL and mode != 'decline':
            if effect == 'loot':
                hand_refs = [ref for ref, obj in world.entities.items() if obj['player'] == owner and obj['zone'] == 'hand']
                if len(hand_refs) != 1:
                    raise ValueError('both controllers need exactly one eligible discard')
                world.move(hand_refs[0], 'graveyard')
                discard_count += int(owner == 0)
            top = world.zones['library'][owner][0]
            draw = next(ref for ref, obj in world.entities.items() if (obj['name'], obj['player'], obj['zone']) == (top, owner, 'library'))
            world.move(draw, 'hand')
        elif effect == 'gain':
            life[owner] += 1
        elif effect == 'drain':
            life[owner] += 1
            life[foe] -= 1
        elif effect == 'damage' and mode != 'prevent_all_damage':
            life[foe] -= 1
        elif effect == 'block':
            restricted = True
        elif paid_bounce and mode not in UNPAID_BOUNCE:
            for ref, _ in mana_witnesses:
                world.entities[ref]['tapped'] = True
            world.move(bounce_targets[bounce_count], 'hand')
            bounce_count += 1

    def visit():
        nonlocal current, restricted

        if any(any(pool) for pool in mana):
            raise ValueError('real untap starts with genuinely empty mana pools')
        current = 1 - current
        emit('advance_turn')
        emit('untap_step', player=current)
        restricted = False
        damage.clear()
        for obj in world.entities.values():
            if obj['zone'] == 'battlefield' and obj['player'] == current:
                obj['tapped'] = False
        emit('turn', player=current, value=0)

    def twiddle(ref, target='source', untap=False, snapshot=False):
        cast(ref, target=target)
        cut = len(steps)
        obj = world.entities[target]
        triggers = target == 'source' and not untap and not obj['tapped']
        emit('choose_dialog', player=world.entities[ref]['player'], value=int(untap))
        if triggers:
            trigger_choices()
        emit('resolve')
        world.move(ref, 'graveyard')
        obj['tapped'] = not untap
        if triggers:
            trigger_effect()
        receipts()
        if snapshot:
            return cut
        return None

    def combat(attacker, fresh=False, attempt_block=False):
        emit('begin_attackers')
        if not fresh and attacker == 'source':
            trigger_choices()
        emit('attack', ref=attacker, value=int(not fresh))
        if not fresh:
            world.entities[attacker]['tapped'] = True
            if attacker == 'source':
                trigger_effect()
        emit('finish_attackers')
        emit('begin_blockers')
        blocked = attempt_block and not restricted and mode != 'ground_block_denied'
        if attempt_block:
            emit('block', ref='blocker', other=attacker, value=int(blocked))
        emit('advance_combat')
        if not fresh and attempt_block:
            emit('assert_card', ref=attacker, property='blocked', value=int(blocked))
        emit('combat_damage')
        if not fresh:
            attacker_card = oracle[world.entities[attacker]['name']]
            attack_power = int(attacker_card['power'])
            if blocked:
                blocker_card = oracle[world.entities['blocker']['name']]
                if attack_power >= int(blocker_card['toughness']):
                    world.move('blocker', 'graveyard')
                else:
                    damage['blocker'] = attack_power
                if mode != 'first_strike':
                    damage[attacker] = int(blocker_card['power'])
            elif mode != 'prevent_combat_damage':
                life[foe] -= attack_power
        emit('end_combat')
        receipts()

    fund(0, [0] * 6)
    fund(1, [0] * 6)
    if mode == 'cast_countered':
        fund(foe, cost_from_oracle(oracle['Counterspell']))
    cast('source')
    if mode == 'cast_countered':
        emit('announce_spell', ref='source')
        cast('counter', response=True)
        emit('resolve_response')
        world.move('source', 'graveyard')
        world.move('counter', 'graveyard')
        receipts()
    else:
        emit('resolve')
        world.move('source', 'battlefield')
        receipts()
        ready = len(steps)
        if mode in ('attack', 'first_strike', 'reach_block', 'ground_block_denied', 'prevent_combat_damage', 'block_expires'):
            visit()
            visit()
            if mode == 'prevent_combat_damage':
                cast('shield')
                emit('resolve')
                world.move('shield', 'graveyard')
            combat('source', attempt_block=effect == 'block' or mode in ('first_strike', 'reach_block', 'ground_block_denied'))
            if mode == 'block_expires':
                visit()
                visit()
                combat('ally', attempt_block=True)
        elif mode == 'fresh_attack':
            combat('source', fresh=True)
        elif mode == 'cost_tap':
            cast('engine')
            emit('resolve')
            world.move('engine', 'battlefield')
            emit('choose_target', player=owner, ref='source')
            emit('choose_target', player=owner, ref='land')
            trigger_choices()
            emit('activate', ref='engine', value=1)
            emit('assert_stack', value=1)
            emit('assert_card', ref='source', property='tapped', value=1)
            emit('assert_card', ref='land', property='tapped', value=1)
            world.entities['source']['tapped'] = True
            trigger_effect()
            emit('resolve')
            world.entities['land']['tapped'] = False
            receipts()
        elif mode != 'entry':
            if mode == 'opponent_turn':
                visit()
            if mode == 'prevent_all_damage':
                cast('shield')
                emit('resolve')
                world.move('shield', 'graveyard')
            if paid_bounce and mode in UNPAYABLE_COST:
                for ref, color in mana_witnesses:
                    emit('activate', ref=ref, value=1)
                    world.entities[ref]['tapped'] = True
                    mana[owner][color] += 1
                receipts()
            pending = twiddle('tap_spell', target='ally' if mode == 'other_taps' else 'source', snapshot=mode == 'pending_replay')
            if mode == 'already_tapped':
                twiddle('second_spell')
            elif mode == 'untap_retap':
                twiddle('second_spell', untap=True)
                if paid_bounce:
                    visit()
                    visit()
                twiddle('third_spell')
    if mode in ('ready_replay', 'pending_replay'):
        cut = ready if mode == 'ready_replay' else pending
        tail = steps[cut:]
        steps = steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(tail) + [step('assert_recorded_state')]
    gaps = ['Crew, extra combat phases, replacement effects, and multiplayer.']
    if paid_bounce:
        gaps += ['Target announcement before payment and responses to a separately pending trigger.',
                 'Voluntary refusal through the native AI mana-payment policy.']
    case.update(name=f'BecomesTappedRecipe_{csvid}_{variant}', steps=steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                             untested_components=gaps))
    return case


def expand(name, csvid, declaration, model, selected, rows, oracle):
    if csvid != model['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(model)):
        raise ValueError('explicit reviewed variants required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, model, variant, rows, oracle) for variant in selected]
