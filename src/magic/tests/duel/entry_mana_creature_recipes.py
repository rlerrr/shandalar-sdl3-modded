"""Mana production from normal creature entry and its printed conditions."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from defensive_creature_recipes import reviewed_payment
from entry_aura_freeze_recipes import FreezeLedger
from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'entry_mana_creature'
DATA = Path(__file__).parent / 'data/entry_mana_creature_wave8_profiles.json'
COLORS = 'CBUGRW'
PREFIX = {
    'entry': 'When this creature enters, add ',
    'cast_hand': 'When this creature enters, if you cast it from your hand, add ',
    'raid': 'Raid — When this creature enters, if you attacked this turn, add ',
    'revolt': 'Revolt — When this creature enters, if a permanent left the battlefield under your control this turn, add ',
}
SPENDERS = {(0, 0, 0, 1, 1, 0): ('Elvish Mystic', 'Raging Goblin'),
            (0, 3, 0, 0, 0, 0): ('Dread Shade',),
            (0, 0, 0, 0, 3, 0): ('Ball Lightning',),
            (0, 1, 0, 0, 1, 1): ('Typhoid Rats', 'Raging Goblin', 'Boros Elite'),
            (0, 0, 0, 2, 0, 0): ('Strangleroot Geist',)}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record['type_line'].startswith('Creature — ')
            or any(not re.fullmatch(r'[0-9]+', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('normal whole creature with printed numeric characteristics required')
    clause = record['oracle_text']
    for gate, prefix in PREFIX.items():
        match = re.fullmatch(re.escape(prefix) + r'((?:\{[BUGRW]\}){2,3})\.', clause)
        if match:
            produced = [match[1].count('{' + color + '}') for color in COLORS]
            keywords = [gate.title()] if gate in ('raid', 'revolt') else []
            if record.get('keywords') != keywords or tuple(produced) not in SPENDERS:
                break
            hybrid = record['mana_cost'] == '{R/G}{R/G}'
            cost = reviewed_payment(record, dict(payment_colors=['R', 'R']))[0] if hybrid else cost_from_oracle(record)
            return dict(gate=gate, mana=cost, hybrid=hybrid, produced=produced, clause=clause)
    raise ValueError('complete reviewed entry mana clause and keywords required')


def spec(declaration, record):
    model = mechanism(record)
    pin = pins()['sources'].get(record['name'])
    if (not pin or not exact_pin(pin['oracle'], record) or declaration.get('family') != FAMILY
            or not exact_pin(declaration.get('version'), 2) or declaration.get('oracle_text') != record['oracle_text']
            or not exact_pin(declaration.get('params'), dict(schema=1, mechanism=model))):
        raise ValueError('independent whole Oracle, binding, and activation pins required')
    return dict(model, csvid=pin['csvid'], family=FAMILY)


def variants(model):
    modes = ['cast', 'wrong_color', 'insufficient', 'countered', 'spend', 'carry_pool', 'reanimate',
             'blink', 'nonmatching_entry', 'ready_replay', 'pending_replay']
    if model['hybrid']:
        modes += ['pay_green', 'pay_mixed']
    if model['gate'] == 'raid':
        modes += ['no_attack', 'damage_only', 'attack_no_damage', 'removed_attacker',
                  'opponent_attack', 'prior_turn']
    if model['gate'] == 'revolt':
        modes += ['no_departure', 'opponent_bounce', 'own_death', 'own_exile', 'stolen_bounce', 'prior_turn']
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def validate_witnesses(oracle, rows):
    data = pins()
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('csvid') != pin['csvid']
                    or row.get('kind') != 'card' or row.get('handler') != pin['handler']
                    or row['handler'] in (None, 'NULL', '0')
                    or row['handler'] == 'card_dummy' and (group == 'sources' or name not in data['static_witnesses'])):
                raise ValueError('reviewed ordinary card binding changed: ' + name)


def _case(name, csvid, declaration, model, variant, rows, oracle):
    from recipes import card, cost_from_oracle, step

    mode, side = variant.rsplit('_', 1)
    owner = int(side)
    foe = 1-owner
    gate = model['gate']
    debit = mode in ('wrong_color', 'insufficient', 'countered')
    setup = [card(name, 'source', owner, 'graveyard' if mode == 'reanimate' else 'hand')]
    for player in (0, 1):
        top = name if gate in ('raid', 'revolt') and mode == 'reanimate' and player == owner else 'Island'
        setup += [card(n, f'lib_{player}_{i}', player, 'library') for i, n in enumerate((top, 'Mountain', 'Swamp'))]
    if mode == 'countered':
        setup.append(card('Counterspell', 'counter', foe, 'hand'))
    if mode == 'reanimate':
        setup.append(card('Zombify', 'reanimate', owner, 'hand'))
    if mode in ('blink', 'opponent_attack'):
        setup.append(card('Cloudshift', 'blink', owner, 'hand'))
    if mode == 'nonmatching_entry':
        setup.append(card('Grizzly Bears', 'other_creature', owner, 'hand'))
    if mode == 'spend':
        setup += [card(n, f'spend_{i}', owner, 'hand') for i, n in enumerate(SPENDERS[tuple(model['produced'])])]
    raid_combat = gate == 'raid' and not debit and mode not in ('no_attack', 'damage_only', 'blink')
    if raid_combat:
        setup.append(card('Grizzly Bears', 'attacker', foe if mode == 'opponent_attack' else owner))
    if mode == 'attack_no_damage':
        setup.append(card('Fog', 'fog', foe, 'hand'))
    if mode == 'damage_only':
        setup.append(card('Shock', 'shock', owner, 'hand'))
    if mode == 'removed_attacker':
        setup.append(card('Unsummon', 'remove', foe, 'hand'))
    revolt_witness = gate == 'revolt' and not debit and mode not in ('no_departure', 'blink')
    if revolt_witness:
        witness = card('Grizzly Bears', 'witness', foe if mode == 'opponent_bounce' else owner)
        if mode == 'stolen_bounce':
            witness['state'] = ['untapped', 'opponent_owned']
        setup.append(witness)
        removal = {'own_death': 'Doom Blade', 'own_exile': 'Swords to Plowshares'}.get(mode, 'Unsummon')
        setup.append(card(removal, 'remove', owner, 'hand'))
    case = dict(card=name, fixture='empty_main', seed=123, setup=setup)
    world = FreezeLedger(case, model, mode, owner, rows, oracle)
    life = [20, 20]
    mana = [[0]*6, [0]*6]
    current = owner
    attacked = [False, False]
    left = [False, False]
    steps = [step('turn', player=owner, value=0), step('present_ai_dialogs', value=1)]

    def emit(op, **values):
        steps.append(step(op, **values))

    def fund(player, pool):
        mana[player] = list(pool)
        for color, amount in enumerate(pool):
            emit('set_mana', player=player, color=color, value=amount)
        emit('refresh')

    def enter(hand_cast):
        qualifies = {'entry': True, 'cast_hand': hand_cast, 'raid': attacked[owner], 'revolt': left[owner]}
        world.move('source', 'battlefield')
        if qualifies[gate]:
            mana[owner] = [a+b for a,b in zip(mana[owner], model['produced'])]

    def inspect(stack=0):
        emit('assert_choices_consumed')
        emit('assert_stack', value=stack)
        names = sorted({obj['name'] for obj in world.entities.values()})
        for player in (0, 1):
            emit('assert_life', player=player, value=life[player])
            for color, amount in enumerate(mana[player]):
                emit('assert_mana', player=player, color=color, value=amount)
            for zone in ('hand', 'battlefield', 'graveyard', 'exile', 'library'):
                # The raw active-card observer also includes pending spells in its hand bucket.
                observed = ('hand', 'stack') if zone == 'hand' else (zone,)
                counts = Counter(obj['name'] for obj in world.entities.values()
                                 if obj['player'] == player and obj['zone'] in observed)
                emit('assert_zone', player=player, zone=zone, value=sum(counts.values()))
                if zone == 'library':
                    for i, n in enumerate(world.zones[zone][player]):
                        emit('assert_zone_card', player=player, zone=zone, index=i, value=rows[n]['csvid'])
                    continue
                for n in names:
                    emit('assert_zone_multiplicity', player=player, zone=zone, index=rows[n]['csvid'], value=counts[n])
        for ref, obj in world.entities.items():
            if obj['zone'] == 'battlefield' and oracle[obj['name']]['type_line'].startswith('Creature — '):
                emit('assert_card', ref=ref, property='in_play', value=1)
                emit('assert_card', ref=ref, property='tapped', value=int(obj['tapped']))
                for key in ('power', 'toughness'):
                    emit('assert_card', ref=ref, property=key, value=int(oracle[obj['name']][key]))

    def cast(ref, target=None, *, allowed=True, pay=None, prefund=True, pending=0, response=False):
        obj = world.entities[ref]
        player = obj['player']
        record = oracle[obj['name']]
        fee = model['mana'] if ref == 'source' else cost_from_oracle(record)
        if ref == 'source' and mode in ('pay_green', 'pay_mixed'):
            colors = ['G', 'G'] if mode == 'pay_green' else ['R', 'G']
            fee, _ = reviewed_payment(record, dict(payment_colors=colors))
        budget = fee if pay is None else pay
        if prefund:
            fund(player, [a+b for a,b in zip(mana[player], budget)])
        if target is not None:
            emit('choose_target', ref=target, player=player)
        emit('cast_response' if response else 'cast', ref=ref, value=int(allowed))
        if not allowed:
            return
        mana[player] = [have-need for have,need in zip(mana[player], fee)]
        if min(mana[player]) < 0:
            raise ValueError('expected cast payment exceeds actual available fixture mana')
        world.move(ref, 'stack')
        emit('assert_stack', value=pending+1)

    def resolve_non_source(ref):
        emit('resolve')
        obj = world.entities[ref]
        types = oracle[obj['name']]['type_line'].partition(' — ')[0].split()
        world.move(ref, 'graveyard' if set(types) & {'Instant', 'Sorcery'} else 'battlefield')

    def advance():
        nonlocal current

        if any(any(pool) for pool in mana):
            raise ValueError('turn boundary requires both pools empty')
        current = 1-current
        emit('advance_turn')
        emit('untap_step', player=current)
        emit('turn', player=current, value=0)
        attacked[:] = [False, False]
        left[:] = [False, False]
        for obj in world.entities.values():
            if obj['zone'] == 'battlefield' and obj['player'] == current:
                obj['tapped'] = False

    def remove(ref, destination):
        obj = world.entities[ref]
        controller = obj['player']
        cast('remove', ref)
        resolve_non_source('remove')
        left[controller] = True
        if destination == 'exile':
            life[controller] += int(oracle[obj['name']]['power'])
        world.move(ref, destination)

    def combat():
        if mode == 'attack_no_damage':
            cast('fog')
            resolve_non_source('fog')
        emit('begin_attackers')
        emit('attack', ref='attacker', value=1)
        attacked[current] = True
        world.entities['attacker']['tapped'] = True
        emit('finish_attackers')
        if mode == 'removed_attacker':
            remove('attacker', 'hand')
        emit('begin_blockers')
        emit('advance_combat')
        emit('combat_damage')
        if mode not in ('removed_attacker', 'attack_no_damage'):
            life[1-current] -= 2
        emit('end_combat')
        inspect()

    def blink():
        cast('blink', 'source')
        resolve_non_source('blink')
        left[owner] = True
        enter(False)
        emit('bind_permanent', player=owner, ref='source', value=csvid)
        inspect()

    for player in (0, 1):
        fund(player, [0]*6)
    if raid_combat and mode != 'opponent_attack':
        combat()
    if mode == 'damage_only':
        emit('choose_player', player=owner, value=foe)
        cast('shock')
        resolve_non_source('shock')
        life[foe] -= 2
        inspect()
    if revolt_witness:
        remove('witness', {'own_death': 'graveyard', 'own_exile':'exile'}.get(mode, 'hand'))
        inspect()
    if mode == 'prior_turn':
        advance()
        advance()
    if mode == 'carry_pool':
        fund(owner, [0,0,1,0,0,0])
    ready = len(steps)
    if mode == 'reanimate':
        emit('choose_zone_card', player=owner, value=csvid)
        cast('reanimate')
        inspect(stack=1)
        resolve_non_source('reanimate')
        enter(False)
        emit('bind_permanent', player=owner, ref='source', value=csvid)
        inspect()
    elif mode in ('wrong_color', 'insufficient'):
        pool = [0]*6
        if mode == 'wrong_color':
            pool[2] = sum(model['mana'])
        cast('source', allowed=False, pay=pool)
        inspect()
    elif mode == 'countered':
        fee = cost_from_oracle(oracle['Counterspell'])
        fund(foe, fee)
        cast('source')
        emit('announce_spell', ref='source')
        cast('counter', prefund=False, pending=1, response=True)
        emit('resolve_response')
        world.move('counter', 'graveyard')
        world.move('source', 'graveyard')
        inspect()
    else:
        cast('source')
        inspect(stack=1)
        pending_cut = len(steps)
        emit('resolve')
        enter(True)
        inspect()
        if mode == 'spend':
            for i in range(len(SPENDERS[tuple(model['produced'])])):
                ref = f'spend_{i}'
                cast(ref, prefund=False)
                resolve_non_source(ref)
            inspect()
        if mode == 'nonmatching_entry':
            cast('other_creature')
            resolve_non_source('other_creature')
            inspect()
        if mode == 'opponent_attack':
            advance()
            combat()
        if mode in ('blink', 'opponent_attack'):
            blink()
        if mode in ('ready_replay', 'pending_replay'):
            cut = ready if mode == 'ready_replay' else pending_cut
            tail = steps[cut:]
            steps = steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(tail) + [step('assert_recorded_state')]
    case.update(name=f'EntryManaCreatureRecipe_{csvid}_{variant}', steps=steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], covered_clauses=[model['clause']],
                             untested_components=['Separate priority after entry, alternate cast zones, mana replacement, and multiplayer.']))
    return case


def expand(name, csvid, declaration, model, selected, rows, oracle):
    if csvid != model['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(model)):
        raise ValueError('explicit reviewed mana-entry variants required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, model, variant, rows, oracle) for variant in selected]
