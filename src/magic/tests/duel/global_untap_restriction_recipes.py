"""Whole-Oracle global untap restrictions through real turns and casts."""
import copy
import json
import re
from pathlib import Path

from entry_aura_freeze_recipes import FreezeLedger
from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'global_untap_restriction'
DATA = Path(__file__).parent / 'data/global_untap_restriction_wave8_profiles.json'
PALETTES = {
    'nonbasic_land': ('Tropical Island', 'Darksteel Citadel', 'Dryad Arbor', 'Island', 'Wastes', 'Sol Ring'),
    'island': ('Island', 'Tropical Island', 'Forest', 'Wastes', 'Sol Ring'),
    'nonwhite_high': ('Hill Giant', 'Juggernaut', 'Goblin Piker', 'Serra Angel', 'Boros Reckoner', 'Sol Ring'),
    'low': ('Ornithopter', 'Raging Goblin', 'Dryad Arbor', 'Grizzly Bears', 'Sol Ring'),
    'high': ('Hill Giant', 'Juggernaut', 'Goblin Piker', 'Serra Angel', 'Sol Ring'),
    'red': ('Hill Giant', 'Goblin Piker', 'Boros Reckoner', 'Grizzly Bears', 'Juggernaut', 'Sol Ring'),
}
LOCK_CLAUSES = {
    'Nonbasic lands': 'nonbasic_land',
    'Islands': 'island',
    'Nonwhite creatures with power 3 or greater': 'nonwhite_high',
    'Creatures with power 1 or less': 'low',
    'Creatures with power 3 or greater': 'high',
    'Red creatures': 'red',
}
ENTRY_CLAUSES = {'island': 'tap all Islands.', 'red': 'tap all red creatures.'}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    if (not record or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record.get('keywords')):
        raise ValueError('whole normal untap restriction without unmodeled keywords required')
    type_line = record.get('type_line', '')
    creature = type_line.startswith('Creature — ')
    if type_line not in ('Enchantment', 'Artifact') and not creature:
        raise ValueError('standalone permanent required')
    if creature and any(not re.fullmatch(r'[0-9]+', record.get(k, '')) for k in ('power', 'toughness')):
        raise ValueError('actual numeric creature source required')
    clauses = record['oracle_text'].splitlines()
    suffix = " don't untap during their controllers' untap steps."
    scope = LOCK_CLAUSES.get(clauses[-1].removesuffix(suffix)) if clauses and clauses[-1].endswith(suffix) else None
    if not scope:
        raise ValueError('whole static untap filter required')
    entry = len(clauses) == 2 and type_line == 'Enchantment' and scope in ENTRY_CLAUSES and clauses[0] == f'When this enchantment enters, {ENTRY_CLAUSES[scope]}'
    if len(clauses) != 1 and not entry:
        raise ValueError('unmodeled whole entry clause')
    return dict(scope=scope, entry=entry, creature=creature, clauses=clauses)


def spec(declaration, record):
    from recipes import cost_from_oracle

    parsed = mechanism(record)
    profile = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not profile or not exact_pin(profile['oracle'], record)
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int
            or params['schema'] != 1 or not exact_pin(params['mechanism'], parsed)):
        raise ValueError('whole independent Oracle type, mana, and restriction pins required')
    return dict(parsed, csvid=profile['csvid'], mana=cost_from_oracle(record), family=FAMILY)


def restricted(record, scope, boost=0):
    type_line, _, subtypes = record['type_line'].partition(' — ')
    types = set(type_line.split())
    if scope == 'nonbasic_land':
        return 'Land' in types and 'Basic' not in types
    if scope == 'island':
        return 'Island' in subtypes.split()
    if 'Creature' not in types:
        return False
    if scope == 'red':
        return 'R' in record['colors']
    power = int(record['power']) + boost
    if scope == 'low':
        return power <= 1
    return power >= 3 and (scope == 'high' or 'W' not in record['colors'])


def variants(profile):
    modes = ['lifecycle', 'untapped_entry', 'mixed_entry', 'insufficient_mana', 'countered',
             'source_removed', 'manual_untap', 'two_sources', 'ready_replay', 'pending_replay', 'step_replay']
    if any(profile['mana'][1:]):
        modes.append('wrong_color')
    if profile['scope'] in ('high', 'low', 'nonwhite_high'):
        modes.append('power_change')
    if profile['creature']:
        modes.append('source_attacks')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


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


def _case(name, csvid, declaration, p, variant, rows, oracle):
    from recipes import card, cost_from_oracle, step

    mode, side = variant.rsplit('_', 1)
    owner = int(side)
    enemy = 1 - owner
    setup = [card(name, 'source', owner, 'hand')]
    hosts = []
    palette = PALETTES[p['scope']]
    if mode == 'power_change':
        palette = ('Raging Goblin', 'Grizzly Bears') if p['scope'] == 'low' else ('Goblin Piker', 'Hill Giant')
    if mode in ('insufficient_mana', 'wrong_color', 'source_attacks'):
        palette = ()
    for player in (0, 1):
        for index, witness in enumerate(palette):
            ref = f'host_{player}_{index}'
            tapped = mode != 'untapped_entry' and (mode != 'mixed_entry' or index % 2 == 0)
            setup.append(dict(card(witness, ref, player), state=['tapped' if tapped else 'untapped']))
            hosts.append(ref)
        for index, witness in enumerate(('Island', 'Forest', 'Wastes')):
            setup.append(card(witness, f'library_{player}_{index}', player, 'library'))
    if mode == 'countered':
        setup.append(card('Counterspell', 'counter', enemy, 'hand'))
    if mode in ('source_removed', 'two_sources'):
        setup.append(card('Boomerang', 'bounce', owner, 'hand'))
    if mode == 'two_sources':
        setup.append(card(name, 'second', owner, 'hand'))
        setup.append(card('Boomerang', 'bounce_second', owner, 'hand'))
    if mode == 'manual_untap':
        setup.append(card('Twiddle', 'twiddle', owner, 'hand'))
    if mode == 'power_change':
        setup.append(card('Rancor', 'boost', owner, 'hand'))

    case = dict(card=name, fixture='empty_main', seed=123, setup=setup)
    world = FreezeLedger(case, p, mode, owner, rows, oracle)
    steps = [step('turn', player=owner, value=0), step('present_ai_dialogs', value=1)]
    pools = [[0] * 6 for _ in (0, 1)]
    life = [20, 20]
    current = owner
    powers = {}

    def fund(player, mana):
        pools[player] = list(mana)
        steps.extend(step('set_mana', player=player, color=color, value=value) for color, value in enumerate(mana))
        steps.append(step('refresh'))

    def cast(ref, response=False):
        obj = world.entities[ref]
        player = obj['player']
        fee = cost_from_oracle(oracle[obj['name']])
        pools[player] = [amount - spent for amount, spent in zip(pools[player], fee)]
        if min(pools[player]) < 0:
            raise ValueError('fixture has insufficient real mana')
        steps.extend([step('cast_response' if response else 'cast', ref=ref, value=1),
                      step('assert_stack', value=2 if response else 1)])
        world.move(ref, 'stack')

    def receipts():
        result = [step('assert_choices_consumed'), step('assert_stack', value=0)]
        all_names = sorted({obj['name'] for obj in world.entities.values()})
        for player in (0, 1):
            result.append(step('assert_life', player=player, value=life[player]))
            result.extend(step('assert_mana', player=player, color=color, value=value) for color, value in enumerate(pools[player]))
            for zone in ('battlefield', 'hand', 'graveyard', 'exile', 'library'):
                result.append(step('assert_zone', player=player, zone=zone, value=world.count(player, zone)))
                if zone == 'library':
                    result.extend(step('assert_zone_card', player=player, zone=zone, index=i, value=rows[n]['csvid']) for i, n in enumerate(world.zones[zone][player]))
                    continue
                for n in all_names:
                    if zone == 'battlefield':
                        count = sum(obj['player'] == player and obj['zone'] == zone and obj['name'] == n for obj in world.entities.values())
                    else:
                        count = world.zones[zone][player].count(n)
                    result.append(step('assert_zone_multiplicity', player=player, zone=zone, index=rows[n]['csvid'], value=count))
        for ref, obj in world.entities.items():
            if obj['zone'] != 'battlefield':
                continue
            result.extend([step('assert_card', ref=ref, property='in_play', value=1),
                           step('assert_card', ref=ref, property='tapped', value=int(obj['tapped']))])
            pinned = oracle[obj['name']]
            if 'Creature' in pinned['type_line'].partition(' — ')[0].split():
                result.extend([step('assert_card', ref=ref, property='power', value=int(pinned['power']) + powers.get(ref, 0)),
                               step('assert_card', ref=ref, property='toughness', value=int(pinned['toughness']))])
        return result

    def matches(ref):
        return restricted(oracle[world.entities[ref]['name']], p['scope'], powers.get(ref, 0))

    def enter(ref):
        steps.append(step('resolve'))
        world.move(ref, 'battlefield')
        if p['entry']:
            for host in hosts:
                if matches(host):
                    world.entities[host]['tapped'] = True
        steps.extend(receipts())

    def untap_next():
        nonlocal current
        if any(any(pool) for pool in pools):
            raise ValueError('real untap needs genuinely empty mana pools')
        current = 1 - current
        steps.extend([step('advance_turn'), step('untap_step', player=current)])
        active = any(world.entities[ref]['zone'] == 'battlefield' for ref in ('source', 'second') if ref in world.entities)
        for ref, obj in world.entities.items():
            if obj['zone'] == 'battlefield' and obj['player'] == current and (not active or not matches(ref)):
                obj['tapped'] = False
        steps.extend(receipts())
        steps.append(step('turn', player=current, value=0))

    def bounce(ref, spell):
        fund(owner, cost_from_oracle(oracle['Boomerang']))
        steps.append(step('choose_target', player=owner, ref=ref))
        cast(spell)
        steps.append(step('resolve'))
        world.move(spell, 'graveyard')
        world.move(ref, 'hand')
        steps.extend(receipts())

    fund(0, [0] * 6)
    fund(1, [0] * 6)
    payment = list(p['mana'])
    if mode == 'insufficient_mana':
        payment = [0] * 6
    if mode == 'wrong_color':
        color = next(i for i in range(1, 6) if payment[i])
        payment[0] += payment[color]
        payment[color] = 0
    fund(owner, payment)
    if mode == 'countered':
        fund(enemy, cost_from_oracle(oracle['Counterspell']))
    ready = len(steps)
    if mode in ('insufficient_mana', 'wrong_color'):
        steps.append(step('cast', ref='source', value=0))
        steps.extend(receipts())
    else:
        cast('source')
        pending = len(steps)
        if mode == 'countered':
            steps.append(step('announce_spell', ref='source'))
            cast('counter', response=True)
            steps.append(step('resolve_response'))
            world.move('source', 'graveyard')
            world.move('counter', 'graveyard')
            steps.extend(receipts())
            untap_next()
            untap_next()
        else:
            enter('source')
            attached = len(steps)
            if mode == 'manual_untap':
                ref = next(ref for ref in hosts if world.entities[ref]['player'] == owner and matches(ref))
                fund(owner, cost_from_oracle(oracle['Twiddle']))
                steps.append(step('choose_target', player=owner, ref=ref))
                cast('twiddle')
                steps.extend([step('choose_dialog', player=owner, value=1), step('resolve')])
                world.move('twiddle', 'graveyard')
                world.entities[ref]['tapped'] = False
                steps.extend(receipts())
            elif mode == 'power_change':
                ref = f'host_{owner}_0'
                fund(owner, cost_from_oracle(oracle['Rancor']))
                steps.append(step('choose_target', player=owner, ref=ref))
                cast('boost')
                steps.append(step('resolve'))
                world.move('boost', 'battlefield')
                powers[ref] = 2
                steps.extend(receipts())
            elif mode == 'two_sources':
                fund(owner, p['mana'])
                cast('second')
                enter('second')
                bounce('source', 'bounce')
            untap_next()
            untap_next()
            if mode == 'source_attacks':
                steps.extend([step('begin_attackers'), step('attack', ref='source', value=1),
                              step('finish_attackers'), step('begin_blockers'), step('advance_combat'),
                              step('combat_damage'), step('end_combat')])
                world.entities['source']['tapped'] = True
                life[enemy] -= int(oracle[name]['power'])
                steps.extend(receipts())
                untap_next()
                untap_next()
            if mode in ('source_removed', 'two_sources'):
                bounce('second' if mode == 'two_sources' else 'source', 'bounce_second' if mode == 'two_sources' else 'bounce')
                untap_next()
                untap_next()
    if mode in ('ready_replay', 'pending_replay', 'step_replay'):
        cut = {'ready_replay': ready, 'pending_replay': pending, 'step_replay': attached}[mode]
        tail = steps[cut:]
        steps = steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(tail) + [step('assert_recorded_state')]
    case.update(name=f'GlobalUntapRecipe_{csvid}_{variant}', steps=steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], covered_clauses=p['clauses'],
                             untested_components=['Land text and color-word replacement effects, animation and multiplayer.',
                                                  'Other untap-step restrictions and skipped phases.']))
    return case


def expand(name, csvid, declaration, profile, selected, rows, oracle):
    if csvid != profile['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(profile)):
        raise ValueError('explicit supported global untap variants required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, profile, variant, rows, oracle) for variant in selected]
