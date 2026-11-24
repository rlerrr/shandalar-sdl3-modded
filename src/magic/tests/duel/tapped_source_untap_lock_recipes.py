"""Optional source untaps and linked target restrictions through real game steps."""
import copy
import json
import re
from pathlib import Path

from entry_aura_freeze_recipes import FreezeLedger
from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'tapped_source_untap_lock'
DATA = Path(__file__).parent / 'data/tapped_source_untap_lock_wave8_profiles.json'
TARGETS = {
    'artifact, creature, or land': ('Ornithopter', 'Grizzly Bears', 'Forest'),
    'artifact': ('Ornithopter', 'Darksteel Citadel'),
    'creature': ('Grizzly Bears', 'Ornithopter'),
    'land': ('Forest', 'Darksteel Citadel'),
}
NONMATCHES = {'artifact': 'Grizzly Bears', 'creature': 'Darksteel Citadel', 'land': 'Grizzly Bears'}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    if (not record or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record.get('keywords')):
        raise ValueError('whole normal permanent with no additional keywords required')
    type_line = record['type_line'].partition(' — ')[0]
    if type_line not in ('Artifact', 'Creature', 'Artifact Creature'):
        raise ValueError('actual artifact or creature source required')
    creature = 'Creature' in type_line.split()
    noun = 'creature' if creature else 'artifact'
    clauses = record['oracle_text'].splitlines()
    if len(clauses) != 2 or clauses[0] != f'You may choose not to untap this {noun} during your untap step.':
        raise ValueError('whole optional source untap clause required')
    shape = r'(?:\{([1-9][0-9]*)\}, )?\{T\}: Tap target (.+)\. (That permanent|It) doesn\x27t untap during its controller\x27s untap step for as long as this ' + noun + r' remains tapped\.'
    match = re.fullmatch(shape, clauses[1])
    if not match or match[2] not in TARGETS or (match[3] == 'That permanent') != (match[2] == 'artifact, creature, or land'):
        raise ValueError('complete reviewed target and tapped-source duration required')
    if creature and any(not re.fullmatch(r'[0-9]+', record.get(k, '')) for k in ('power', 'toughness')):
        raise ValueError('actual numeric creature source required')
    return dict(scope=match[2], activation_mana=int(match[1] or 0), creature=creature,
                artifact='Artifact' in type_line.split(), clauses=clauses)


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
        raise ValueError('whole independent source and activation pins required')
    return dict(model, csvid=source['csvid'], mana=cost_from_oracle(record), family=FAMILY)


def variants(model):
    modes = ['lifecycle', 'same_controller', 'already_tapped', 'fresh', 'repeat_denied',
             'manual_target', 'manual_source', 'source_bounce', 'target_bounce',
             'cast_countered', 'ready_replay', 'pending_replay', 'locked_replay']
    modes += [f'target_{i}' for i in range(1, len(TARGETS[model['scope']]))]
    if model['activation_mana']:
        modes.append('insufficient_activation')
    if model['scope'] != 'artifact, creature, or land' and not (model['scope'] == 'artifact' and model['artifact']):
        modes.append('no_legal_target')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes] + ['choose_to_untap_0']


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
    enemy = 1 - owner
    target_player = owner if mode == 'same_controller' else enemy
    target_index = int(mode.split('_')[1]) if re.fullmatch(r'target_[0-9]+', mode) else 0
    target_name = TARGETS[model['scope']][target_index]
    if mode == 'no_legal_target':
        target_name = NONMATCHES[model['scope']]
    setup = [card(name, 'source', owner, 'hand'),
             dict(card(target_name, 'target', target_player), state=['untapped'])]
    for player in (0, 1):
        if mode != 'no_legal_target':
            setup.append(dict(card('Hill Giant', f'control_{player}', player), state=['tapped']))
        for i, basic in enumerate(('Island', 'Swamp')):
            setup.append(card(basic, f'library_{player}_{i}', player, 'library'))
    if mode in ('manual_target', 'manual_source'):
        setup += [card('Twiddle', 'untapper', owner, 'hand'), card('Twiddle', 'retapper', owner, 'hand')]
    if mode == 'already_tapped':
        setup.append(card('Twiddle', 'pretapper', owner, 'hand'))
    if mode in ('source_bounce', 'target_bounce'):
        setup.append(card('Boomerang', 'bounce', owner, 'hand'))
    if mode == 'cast_countered':
        setup.append(card('Counterspell', 'counter', enemy, 'hand'))

    case = dict(card=name, fixture='empty_main', seed=123, setup=setup)
    world = FreezeLedger(case, model, mode, owner, rows, oracle)
    steps = [step('turn', player=owner, value=0), step('present_ai_dialogs', value=1)]
    pools = [[0] * 6 for _ in (0, 1)]
    current = owner
    locked = False

    def emit(op, **values):
        steps.append(step(op, **values))

    def fund(player, mana):
        pools[player] = list(mana)
        for color, value in enumerate(mana):
            emit('set_mana', player=player, color=color, value=value)
        emit('refresh')

    def spend(player, cost):
        pools[player] = [amount - fee for amount, fee in zip(pools[player], cost)]
        if min(pools[player]) < 0:
            raise ValueError('fixture has insufficient real mana')

    def cast(ref, response=False):
        obj = world.entities[ref]
        fee = [0] * 6 if oracle[obj['name']]['mana_cost'] == '{0}' else cost_from_oracle(oracle[obj['name']])
        emit('cast_response' if response else 'cast', ref=ref, value=1)
        spend(obj['player'], fee)
        world.move(ref, 'stack')
        emit('assert_stack', value=2 if response else 1)

    def receipts():
        emit('assert_choices_consumed')
        emit('assert_stack', value=0)
        names = sorted({obj['name'] for obj in world.entities.values()})
        for player in (0, 1):
            emit('assert_life', player=player, value=20)
            for color, value in enumerate(pools[player]):
                emit('assert_mana', player=player, color=color, value=value)
            for zone in ('battlefield', 'hand', 'graveyard', 'exile', 'library'):
                emit('assert_zone', player=player, zone=zone, value=world.count(player, zone))
                if zone == 'library':
                    for index, card_name in enumerate(world.zones[zone][player]):
                        emit('assert_zone_card', player=player, zone=zone, index=index, value=rows[card_name]['csvid'])
                    continue
                for card_name in names:
                    count = (sum(obj['name'] == card_name and obj['player'] == player and obj['zone'] == zone for obj in world.entities.values())
                             if zone == 'battlefield' else world.zones[zone][player].count(card_name))
                    emit('assert_zone_multiplicity', player=player, zone=zone, index=rows[card_name]['csvid'], value=count)
        for ref, obj in world.entities.items():
            if obj['zone'] != 'battlefield':
                continue
            emit('assert_card', ref=ref, property='tapped', value=int(obj['tapped']))
            if 'Creature' in oracle[obj['name']]['type_line'].partition(' — ')[0].split():
                for prop in ('power', 'toughness'):
                    emit('assert_card', ref=ref, property=prop, value=int(oracle[obj['name']][prop]))

    def visit(untap_source=False):
        nonlocal current, locked

        if any(any(pool) for pool in pools):
            raise ValueError('real untap starts with genuinely empty mana pools')
        current = 1 - current
        emit('advance_turn')
        source = world.entities['source']
        target = world.entities['target']
        keep_target_locked = locked and source['zone'] == 'battlefield' and source['tapped']
        if source['zone'] == 'battlefield' and source['tapped'] and current == owner == 0:
            if untap_source:
                emit('choose_priority', ref='source', player=owner)
            else:
                emit('choose_priority', player=owner, value=-2)
        emit('untap_step', player=current)
        for ref, obj in world.entities.items():
            if obj['zone'] != 'battlefield' or obj['player'] != current:
                continue
            if ref == 'source' and not untap_source:
                # AI holds the source while its linked target is still in play.
                if owner == 0 or target['zone'] == 'battlefield':
                    continue
            if ref != 'target' or not keep_target_locked:
                obj['tapped'] = False
        if not source['tapped'] or source['zone'] != 'battlefield':
            locked = False
        receipts()
        emit('turn', player=current, value=0)

    def twiddle(ref, spell, untap):
        nonlocal locked

        fund(owner, cost_from_oracle(oracle['Twiddle']))
        emit('choose_target', player=owner, ref=ref)
        cast(spell)
        emit('choose_dialog', player=owner, value=int(untap))
        emit('resolve')
        world.move(spell, 'graveyard')
        world.entities[ref]['tapped'] = not untap
        if ref == 'source' and untap:
            locked = False
        receipts()

    fund(0, [0] * 6)
    fund(1, [0] * 6)
    fund(owner, model['mana'])
    if mode == 'cast_countered':
        fund(enemy, cost_from_oracle(oracle['Counterspell']))
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
        if model['creature'] and mode != 'fresh':
            visit()
            visit()
        if mode == 'already_tapped':
            twiddle('target', 'pretapper', False)
        fee = [model['activation_mana'], 0, 0, 0, 0, 0]
        can_activate = mode not in ('no_legal_target', 'insufficient_activation') and not (mode == 'fresh' and model['creature'])
        fund(owner, fee if mode != 'insufficient_activation' else [0] * 6)
        ready = len(steps)
        emit('can_activate', ref='source', value=int(can_activate))
        if not can_activate:
            emit('activate', ref='source', value=0)
            receipts()
        else:
            emit('choose_target', player=owner, ref='target')
            emit('activate', ref='source', value=1)
            spend(owner, fee)
            world.entities['source']['tapped'] = True
            emit('assert_stack', value=1)
            emit('assert_card', ref='source', property='tapped', value=1)
            emit('assert_card', ref='target', property='tapped', value=int(world.entities['target']['tapped']))
            for color, value in enumerate(pools[owner]):
                emit('assert_mana', player=owner, color=color, value=value)
            pending = len(steps)
            emit('resolve')
            world.entities['target']['tapped'] = True
            locked = True
            receipts()
            active = len(steps)
            if mode == 'repeat_denied':
                fund(owner, fee)
                emit('can_activate', ref='source', value=0)
                emit('activate', ref='source', value=0)
                receipts()
            else:
                visit()
                visit(untap_source=mode == 'choose_to_untap')
                if mode in ('manual_target', 'manual_source'):
                    ref = 'target' if mode == 'manual_target' else 'source'
                    twiddle(ref, 'untapper', True)
                    twiddle(ref, 'retapper', False)
                elif mode in ('source_bounce', 'target_bounce'):
                    ref = 'source' if mode == 'source_bounce' else 'target'
                    fund(owner, cost_from_oracle(oracle['Boomerang']))
                    emit('choose_target', player=owner, ref=ref)
                    cast('bounce')
                    emit('resolve')
                    world.move('bounce', 'graveyard')
                    world.move(ref, 'hand')
                    locked = False
                    receipts()
                visit()
    if mode in ('ready_replay', 'pending_replay', 'locked_replay'):
        cut = {'ready_replay': ready, 'pending_replay': pending, 'locked_replay': active}[mode]
        tail = steps[cut:]
        steps = steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(tail) + [step('assert_recorded_state')]
    case.update(name=f'TappedSourceLockRecipe_{csvid}_{variant}', steps=steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                             untested_components=['The optional untap selection is scripted for player 0. Player 1 uses the native AI policy.',
                                                  'Target control changes, overlapping independent sources and multiplayer.']))
    return case


def expand(name, csvid, declaration, model, selected, rows, oracle):
    if csvid != model['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(model)):
        raise ValueError('explicit reviewed variants required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, model, variant, rows, oracle) for variant in selected]
