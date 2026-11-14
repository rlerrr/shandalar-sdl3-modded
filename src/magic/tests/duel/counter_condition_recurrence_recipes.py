"""Whole conditional-counter recurrence with printed pump and blocking clauses.

Real death/owner/zone ledger adapted locally from immutable death_recurrence.
"""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from defensive_creature_recipes import reviewed_payment
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY
from hybrid_creature_recipes import script_payments, invalid_payment

FAMILY = 'counter_condition_recurrence'


def pins():
    return json.loads((Path(__file__).parent / 'data/counter_condition_recurrence_wave8_profiles.json').read_text(encoding='utf8'))


def exact(a, b):
    return type(a) is type(b) and (set(a) == set(b) and all(exact(a[k], b[k]) for k in a)
                                 if isinstance(a, dict) else len(a) == len(b) and all(exact(x, y) for x, y in zip(a, b))
                                 if isinstance(a, list) else a == b)


def oracle_model(record):
    if (not isinstance(record, dict) or record.get('layout') != 'normal' or record.get('card_faces') or
            record.get('selected_face') or not record.get('type_line', '').startswith('Creature \u2014 ') or
            any(not re.fullmatch(r'[1-9][0-9]*', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('whole normal numeric recurrence creature required')
    lines = record['oracle_text'].splitlines()
    mechanisms = set(record.get('keywords', [])) & {'Persist', 'Undying'}
    if len(mechanisms) != 1:
        raise ValueError('exactly one printed recurrence keyword required')
    keyword = next(iter(mechanisms))
    symbol = '-1/-1' if keyword == 'Persist' else '+1/+1'
    expected = (f"{keyword} (When this creature dies, if it had no {symbol} counters on it, "
                f"return it to the battlefield under its owner's control with a {symbol} counter on it.)")
    if lines.count(expected) != 1:
        raise ValueError('complete printed owner-return and counter condition required')
    result = dict(mechanism=keyword.lower(), entry_gain=0)
    keys = []
    for line in lines:
        if line == expected:
            continue
        m = re.fullmatch(r'((?:\{[WUBRG](?:/[WUBRG])?\})+): This creature gets \+([1-9][0-9]*)/\+([1-9][0-9]*) until end of turn\.', line)
        if m:
            if 'pump' in result or m[2] != m[3]:
                raise ValueError('one symmetric printed pump required')
            result['pump'] = dict(cost=m[1], amount=int(m[2]))
        elif line in ('Flying', 'Haste'):
            keys.append(line)
        elif line == "This creature can't block." and 'block_rule' not in result:
            result['block_rule'] = 'never'
        elif line == 'This creature can block only creatures with flying.' and 'block_rule' not in result:
            result['block_rule'] = 'flying_only'
        elif line == "Creatures with power less than this creature's power can't block it." and 'block_rule' not in result:
            result['block_rule'] = 'less_power'
        elif line == 'This creature has trample as long as it has a -1/-1 counter on it.' and 'conditional_trample' not in result:
            result['conditional_trample'] = 'minus1'
        else:
            raise ValueError('unreviewed additional whole Oracle clause')
    if len(keys) != len(set(keys)) or set(record['keywords']) != mechanisms | set(keys):
        raise ValueError('printed passive keywords and metadata must agree')
    if keys:
        result['keywords'] = sorted(keys)
    if not any(k in result for k in ('pump', 'block_rule', 'conditional_trample')):
        raise ValueError('coupled conditional recurrence clause required')
    return result


def spec(declaration, record):
    if declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration.get('version') != 2 or declaration.get('oracle_text') != record.get('oracle_text'):
        raise ValueError('complete Oracle recurrence declaration required')
    model = oracle_model(record)
    params = declaration.get('params')
    pairs = re.findall(r'\{([WUBRG])/([WUBRG])\}', record['mana_cost'])
    expected = dict(schema=8, **model)
    if pairs:
        expected['payment_colors'] = params.get('payment_colors') if isinstance(params, dict) else None
    if not exact(params, expected):
        raise ValueError('typed recurrence parameters must match whole Oracle')
    mana, pairs = reviewed_payment(record, params)
    result = dict(copy.deepcopy(params), family=FAMILY, mana=mana, hybrid_pairs=pairs,
                  power=int(record['power']), toughness=int(record['toughness']))
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected) or len(set(selected)) != len(selected) or set(selected)-set(variants(result)):
        raise ValueError('explicit distinct supported recurrence variants required')
    return result


def variants(model):
    modes = ['cast_only', 'payment', 'insufficient_mana', 'wrong_color', 'death', 'destroy',
             'second_death', 'preexisting_counter', 'opposite_counter', 'foreign_owner',
             'token_death', 'bounce_only', 'exile_only', 'other_death', 'cleanup_once',
             'new_object', 'lethal_damage', 'ready_snapshot', 'pending_snapshot',
             'death_snapshot', 'resolved_snapshot', 'bare_zones', 'countered']
    if model['hybrid_pairs']:
        modes.append('hybrid_payment_other')
        if len(model['hybrid_pairs']) > 1:
            modes.append('hybrid_payment_mixed')
    modes += ['counter_reset', 'foreign_return_second', 'stolen_death', 'stolen_second_death']
    if model.get('pump'):
        modes += ['pump_before_death', 'pump_after_return', 'pump_cleanup', 'pump_repeat',
                  'pump_insufficient', 'pump_wrong_color', 'pump_zero_pool', 'pump_snapshot']
        if '/' in model['pump']['cost']:
            modes += ['pump_other_color', 'pump_mixed']
    if model.get('block_rule') == 'never':
        modes += ['block_ground_cycle', 'block_flying_cycle', 'attack_cycle']
    elif model.get('block_rule') == 'flying_only':
        modes += ['block_ground_cycle', 'block_flying_cycle', 'attack_flying_cycle']
    elif model.get('block_rule') == 'less_power':
        modes += ['block_boundary_cycle', 'block_lower_cycle', 'block_equal_cycle', 'block_higher_cycle']
    if model.get('conditional_trample'):
        modes += ['trample_cycle', 'fresh_haste_cycle']
    output = [f'{mode}_{player}' for player in (0, 1) for mode in modes]
    if model['hybrid_pairs']:
        output.append('hybrid_choice_0')
    return output


def validate_witnesses(oracle, registry):
    data = pins()
    for name, record in data['cards'].items():
        if name not in registry or not exact(oracle.get(name), record):
            raise ValueError('complete recurrence witness changed: '+name)
    for name, row in data['implemented_witnesses'].items():
        if not exact(registry.get(name), row) or row.get('handler') in (None, 'card_dummy', 'NULL', '0') or row.get('kind') != 'card':
            raise ValueError('implemented normal witness changed: '+name)
    for name, source in data['sources'].items():
        row = registry.get(name, {})
        if (not exact(oracle.get(name), source['oracle']) or row.get('csvid') != source['csvid'] or
                row.get('handler') != source['handler'] or row.get('kind') != 'card' or
                row.get('handler') in ('card_dummy', 'NULL', '0')):
            raise ValueError('complete implemented recurrence source changed: '+name)


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import card, step, observe, cost_from_oracle
    validate_witnesses(oracle, registry)
    if name not in pins()['sources'] or pins()['sources'][name]['csvid'] != csvid:
        raise ValueError('unreserved recurrence source')
    if not selected or len(set(selected)) != len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unsupported recurrence selection')
    results = []
    for variant in selected:
        mode, controller = variant.rsplit('_', 1)
        controller = int(controller)
        effective = copy.deepcopy(declaration)
        if mode == 'hybrid_payment_other':
            effective['params']['payment_colors'] = [b if a == c else a for (a, b), c in zip(model['hybrid_pairs'], model['payment_colors'])]
        elif mode == 'hybrid_payment_mixed':
            effective['params']['payment_colors'] = [pair[i % 2] for i, pair in enumerate(model['hybrid_pairs'])]
        source_cost, _ = reviewed_payment(oracle[name], effective['params'])
        bare = mode == 'bare_zones'
        zones = {'hand': [list(HAND) if not bare else [] for _ in (0, 1)],
                 'graveyard': [list(GRAVE) if not bare else [] for _ in (0, 1)],
                 'exile': [list(EXILE) if not bare else [] for _ in (0, 1)],
                 'library': [list(LIBRARY), list(LIBRARY)]}
        setup, entities, actions = [], {}, []
        life, mana = [20, 20], [[0]*6, [0]*6]
        pre = [step('reset_death_history'), step('turn', player=controller, value=0)]
        pre += [step('set_mana', player=player, color=color, value=0) for player in (0, 1) for color in range(6)]
        for zone, players in zones.items():
            for player, names in enumerate(players):
                setup.extend(card(n, f'{zone}_{player}_{i}', player, zone) for i, n in enumerate(names))

        def add(n, ref, player, zone='hand', foreign=False, token=False):
            item = card(n, ref, player, zone)
            if zone == 'battlefield':
                item['state'] = ['untapped'] + (['opponent_owned'] if foreign else []) + (['token'] if token else [])
            setup.append(item)
            entities[ref] = dict(name=n, ref=ref, player=player, owner=1-player if foreign else player,
                                 alive=zone == 'battlefield', hand=zone == 'hand', token=token,
                                 plus=0, minus=0, boost=0, damage=0, tapped=0, sick=0)
            if zone != 'battlefield':
                zones[zone][player].append(n)

        seeded = mode in ('foreign_owner', 'foreign_return_second', 'token_death')
        add(name, 'source', controller, 'battlefield' if seeded else 'hand', foreign=mode in ('foreign_owner', 'foreign_return_second'), token=mode == 'token_death')
        source = entities['source']
        if seeded:
            source['owner'] = 1-controller if mode in ('foreign_owner', 'foreign_return_second') else controller
            # Battlefield fixture placement uses real put_into_play: printed
            # ETB effects occur even when no source spell is cast here.
            life[controller] += model['entry_gain']
        if mode == 'other_death':
            add('Elvish Mystic', 'unrelated', controller, 'battlefield')
        # All combat objects exist before any pending or intermediate receipt.
        combat_mode = mode.startswith(('block_', 'attack_', 'trample_', 'fresh_haste_'))
        if combat_mode:
            for stage in ('before', 'after'):
                if mode in ('block_ground_cycle', 'block_flying_cycle'):
                    add('Serra Angel' if 'flying' in mode else 'Elvish Mystic', stage+'_attacker', 1-controller, 'battlefield')
                else:
                    blocker = 'Darksteel Myr' if mode == 'trample_cycle' else ('Elvish Mystic' if mode in ('block_lower_cycle', 'attack_flying_cycle') else 'Serra Angel')
                    add(blocker, stage+'_blocker', 1-controller, 'battlefield')
                    if mode == 'attack_flying_cycle':
                        add('Serra Angel', stage+'_flying_blocker', 1-controller, 'battlefield')
        return_number = 0
        ready_cut = pending_cut = death_cut = None

        def fund(player, pool):
            mana[player] = list(pool)
            return [step('set_mana', player=player, color=c, value=v) for c, v in enumerate(pool)]

        def mana_checks():
            return [step('assert_mana', player=player, color=c, value=v) for player in (0, 1) for c, v in enumerate(mana[player])]

        def characteristics(entity=source):
            if not entity['alive']:
                return []
            record = oracle[entity['name']]
            delta = entity['plus']-entity['minus']+entity['boost']
            return [observe(entity['ref'], prop, value) for prop, value in (
                ('in_play', 1), ('in_hand', 0), ('csvid', registry[entity['name']]['csvid']),
                ('plus1_counters', entity['plus']), ('minus1_counters', entity['minus']),
                ('power', int(record['power'])+delta), ('toughness', int(record['toughness'])+delta),
                ('damage', entity['damage']), ('tapped', entity['tapped']),
                # inspect.sick exposes raw engine bits, which normal haste
                # recalculation clears. Real fresh attacks prove permission.
                ('sick', 0 if entity is source and ('Haste' in model.get('keywords', []) or entity.get('temporary_haste')) else entity['sick']))]+[step('assert_counter', ref=entity['ref'], counter_kind=kind, value=value) for kind, value in (('plus1', entity['plus']), ('minus1', entity['minus']))]

        def cast_creature(ref, cost):
            entity = entities[ref]
            zones['hand'][entity['player']].remove(entity['name'])
            mana[entity['player']] = [a-b for a, b in zip(mana[entity['player']], cost)]
            entity.update(alive=True, hand=False, sick=1)
            return [step('cast', ref=ref, value=1), step('assert_stack', value=1), observe(ref, 'in_hand', 0),
                    step('resolve'), step('assert_choices_consumed')]

        def spell(n, ref, target=None, caster=None, dialog=None):
            player = 1-controller if caster is None else caster
            add(n, ref, player)
            cost = cost_from_oracle(oracle[n])
            sequence = fund(player, cost)
            if target is not None:
                sequence.append(step('choose_target', player=player, ref=target))
            sequence += [step('cast', ref=ref, value=1), step('assert_stack', value=1)]
            if dialog is not None:
                sequence.append(step('choose_dialog', player=player, value=dialog))
            zones['hand'][player].remove(n)
            zones['graveyard'][player].append(n)
            entities[ref]['hand'] = False
            mana[player] = [a-b for a, b in zip(mana[player], cost)]
            return sequence

        def depart(entity, destination='graveyard'):
            entity.update(alive=False, hand=destination == 'hand')
            if not entity['token']:
                zones[destination][entity['owner']].append(entity['name'])

        def recurrence():
            nonlocal return_number
            condition_counter = source['minus'] if model['mechanism'] == 'persist' else source['plus']
            depart(source)
            if source['token'] or condition_counter:
                return []
            zones['graveyard'][source['owner']].remove(name)
            return_number += 1
            source.update(ref=f'returned_{return_number}', player=source['owner'], alive=True, hand=False,
                          plus=int(model['mechanism'] == 'undying'), minus=int(model['mechanism'] == 'persist'),
                          damage=0, tapped=0, boost=0, sick=1, temporary_haste=False)
            life[source['player']] += model['entry_gain']
            return [step('bind_permanent', ref=source['ref'], player=source['player'], value=csvid)]

        def removal(ref='murder_1'):
            nonlocal death_cut
            sequence = spell('Murder', ref, source['ref'])
            death_cut = len(actions)+len(sequence)
            sequence += [step('resolve'), step('assert_choices_consumed')]
            return sequence+recurrence()+characteristics()+mana_checks()

        def counter_control(plus):
            sequence = []
            if plus:
                sequence += spell('Burst of Strength', 'counter_control', source['ref'], caster=controller)
                source['plus'] += 1
                source['tapped'] = 0
                sequence += [step('resolve'), step('refresh'), step('assert_choices_consumed')]
            else:
                add('Fume Spitter', 'counter_control', controller)
                cost = cost_from_oracle(oracle['Fume Spitter'])
                sequence += fund(controller, cost)+cast_creature('counter_control', cost)
                sequence += [step('require_can_activate', ref='counter_control', value=1),
                             step('choose_target', player=controller, ref=source['ref']),
                             step('activate', ref='counter_control', value=1), step('assert_stack', value=1),
                             step('resolve'), step('refresh'), step('assert_choices_consumed')]
                depart(entities['counter_control'])
                source['minus'] += 1
            if source['plus'] and source['minus']:
                source['plus'] -= 1
                source['minus'] -= 1
            if model['toughness']+source['plus']-source['minus'] <= 0:
                sequence += recurrence()
            return sequence+characteristics()+mana_checks()

        def next_own_turn():
            # Complete turn gates and a real own untap before the next legal
            # combat. Each stage uses a different turn; no extra combat is
            # manufactured after the first combat ends in main2.
            for entity in entities.values():
                if entity['alive']:
                    entity.update(damage=0, boost=0, sick=0)
                    if entity['player'] == controller:
                        entity['tapped'] = 0
            return [step('advance_turn'), step('advance_turn'),
                    step('untap_step', player=controller),
                    # The real untap entry point leaves PHASE_UNTAP. Select
                    # main1 on this same already-aged turn before actions.
                    step('turn', player=controller, value=0)]+characteristics()

        def pump():
            nonlocal pump_cut
            actor = source['player']
            clause = model['pump']
            symbols = re.findall(r'\{([WUBRG])(?:/([WUBRG]))?\}', clause['cost'])
            colors = [a for a, b in symbols]
            if mode == 'pump_other_color':
                colors = [b or a for a, b in symbols]
            elif mode == 'pump_mixed':
                colors = [(b or a) if i % 2 else a for i, (a, b) in enumerate(symbols)]
            from recipes import COLORS
            payment = [0]*6
            for c in colors:
                payment[COLORS[c]] += 1
            pool = list(payment)
            denied = mode in ('pump_insufficient', 'pump_wrong_color', 'pump_zero_pool')
            if mode == 'pump_insufficient':
                pool[COLORS[colors[0]]] -= 1
            elif mode == 'pump_zero_pool':
                pool = [0]*6
            elif mode == 'pump_wrong_color':
                allowed = {c for pair in symbols for c in pair if c}
                pool = [0]*6
                pool[COLORS[next(c for c in COLORS if c not in allowed)]] = sum(payment)
            seq = fund(actor, pool)+[step('refresh'), step('require_can_activate', ref=source['ref'], value=int(not denied)),
                                     step('activate', ref=source['ref'], value=int(not denied))]
            if denied:
                return seq+[step('assert_stack', value=0), step('assert_choices_consumed')]+characteristics()+mana_checks()
            mana[actor] = [a-b for a, b in zip(mana[actor], payment)]
            seq += [step('assert_stack', value=1)]+characteristics()+mana_checks()
            pump_cut = len(actions)+len(seq)
            source['boost'] += clause['amount']
            return seq+[step('resolve'), step('refresh'), step('assert_choices_consumed')]+characteristics()+mana_checks()

        def finish_legality_combat(sequence, stage):
            # The ordinary combat API requires completed damage before ending
            # combat. A real paid Fog preserves the legality-control creatures
            # for their separately controlled recurrence deaths; no phase skip.
            sequence += [step('advance_combat')]
            sequence += spell('Fog', stage+'_combat_fog', caster=controller)
            sequence += [step('resolve'), step('refresh'), step('assert_choices_consumed'),
                         step('combat_damage'), step('refresh'), step('end_combat')]
            sequence += [step('assert_life', player=p, value=life[p]) for p in (0, 1)]
            return sequence+characteristics()

        def coupled_combat(stage):
            seq = []
            defensive = mode in ('block_ground_cycle', 'block_flying_cycle')
            haste = 'Haste' in model.get('keywords', [])
            if defensive:
                actor = 1-controller
                # Existing ready opposing witnesses attack on their next real turn.
                seq += [step('advance_turn'), step('untap_step', player=actor)]
                seq += [step('turn', player=actor, value=0)]
                for e in entities.values():
                    if e['alive']:
                        e.update(damage=0, boost=0)
                        # Only the active player's start-turn gate clears its
                        # sickness. A fresh defender remains sick and can block.
                        if e['player'] == actor:
                            e.update(tapped=0, sick=0)
                attacker = entities[stage+'_attacker']
                # Serra Angel has Vigilance; Mystic taps to attack, never for mana.
                attacker['tapped'] = int(attacker['name'] != 'Serra Angel')
                allowed = model['block_rule'] != 'never' and mode == 'block_flying_cycle'
                seq += [step('begin_attackers'), step('attack', ref=attacker['ref'], value=1),
                        step('finish_attackers'), step('begin_blockers'),
                        step('block', ref=source['ref'], other=attacker['ref'], value=int(allowed))]
                return finish_legality_combat(seq, stage)
            if not haste:
                seq += next_own_turn()
            blocker = entities[stage+'_blocker']
            if mode == 'block_higher_cycle':
                seq += spell('Giant Growth', stage+'_blocker_growth', blocker['ref'], caster=1-controller)+[step('resolve'), step('assert_choices_consumed')]
                blocker['boost'] += 3
            elif mode == 'block_equal_cycle' and stage == 'after':
                seq += spell('Burst of Strength', stage+'_blocker_counter', blocker['ref'], caster=1-controller)+[step('resolve'), step('refresh'), step('assert_choices_consumed')]
                blocker['plus'] += 1
            seq += characteristics(blocker)
            source['tapped'] = 1
            seq += [step('begin_attackers'), step('attack', ref=source['ref'], value=1),
                    observe(source['ref'], 'tapped', 1), step('finish_attackers'), step('begin_blockers')]
            allowed = True
            if mode == 'attack_flying_cycle':allowed = False
            elif model.get('block_rule') == 'less_power':
                allowed = (int(oracle[blocker['name']]['power'])+blocker['plus']+blocker['boost'] >=
                           model['power']+source['plus']-source['minus']+source['boost'])
            seq += [step('block', ref=blocker['ref'], other=source['ref'], value=int(allowed))]
            if mode == 'attack_flying_cycle':
                seq += [step('block', ref=stage+'_flying_blocker', other=source['ref'], value=1)]
            if mode in ('trample_cycle', 'fresh_haste_cycle'):
                # Indestructible0/1 blocker survives; no damage is dealt back.
                if mode == 'trample_cycle':
                    power = model['power']+source['plus']-source['minus']+source['boost']
                    toughness = int(oracle[blocker['name']]['toughness'])
                    trample = bool(source['minus'])
                    blocker['damage'] = min(power, toughness) if trample else power
                    if trample:life[1-controller] -= max(0, power-toughness)
                else:
                    return finish_legality_combat(seq, stage)
                seq += [step('advance_combat'), step('combat_damage'),
                        observe(blocker['ref'], 'in_play', 1), observe(blocker['ref'], 'damage', blocker['damage']),
                        step('assert_life', player=1-controller, value=life[1-controller])]
            if mode != 'trample_cycle':
                return finish_legality_combat(seq, stage)
            seq += [step('end_combat')]
            return seq+characteristics()

        pump_cut = None

        if seeded:
            actions += characteristics()+[step('assert_life', player=controller, value=life[controller])]
        else:
            pool = list(source_cost)
            if mode == 'insufficient_mana':
                pool = [0]*6
            elif mode == 'wrong_color':
                used = {c for pair in model['hybrid_pairs'] for c in pair}
                from recipes import COLORS
                used |= {c for c, i in COLORS.items() if source_cost[i]}
                unused = next(c for c in COLORS if c not in used)
                pool = [source_cost[0], 0, 0, 0, 0, 0]
                pool[COLORS[unused]] = sum(source_cost[1:])
            elif mode == 'hybrid_choice':
                from recipes import COLORS
                for a, b in model['hybrid_pairs']:
                    pool[COLORS[a]] = max(pool[COLORS[a]], len(model['hybrid_pairs']))
                    pool[COLORS[b]] = max(pool[COLORS[b]], len(model['hybrid_pairs']))
            actions += fund(controller, pool)
            ready_cut = len(actions)
            if mode in ('insufficient_mana', 'wrong_color'):
                actions += [step('cast', ref='source', value=0)]
            elif mode == 'countered':
                add('Counterspell', 'counter', 1-controller)
                actions += fund(1-controller, cost_from_oracle(oracle['Counterspell']))
                zones['hand'][controller].remove(name)
                zones['graveyard'][controller].append(name)
                source['hand'] = False
                mana[controller] = [a-b for a, b in zip(mana[controller], source_cost)]
                zones['hand'][1-controller].remove('Counterspell')
                zones['graveyard'][1-controller].append('Counterspell')
                entities['counter']['hand'] = False
                mana[1-controller] = [0]*6
                actions += [step('cast', ref='source', value=1), step('assert_stack', value=1),
                            step('announce_spell', ref='source'), step('cast_response', ref='counter', value=1),
                            step('assert_stack', value=2), step('resolve_response'), step('assert_choices_consumed')]
            else:
                cast_sequence = cast_creature('source', source_cost)
                pending_cut = len(actions)+3
                actions += cast_sequence
                life[controller] += model['entry_gain']
                actions += characteristics()+[step('assert_life', player=controller, value=life[controller])]+mana_checks()

        skip = mode in ('cast_only', 'payment', 'insufficient_mana', 'wrong_color', 'countered',
                        'hybrid_payment_other', 'hybrid_payment_mixed', 'hybrid_choice')
        if not skip:
            if mode == 'bounce_only':
                actions += spell('Unsummon', 'removal', source['ref'])+[step('resolve'), step('assert_choices_consumed')]
                depart(source, 'hand')
                source['ref'] = 'source_hand'
                actions.append(step('bind_hand', ref=source['ref'], player=source['owner'], value=csvid))
            elif mode == 'exile_only':
                actions += spell('Swords to Plowshares', 'removal', source['ref'])+[step('resolve'), step('assert_choices_consumed')]
                life[source['player']] += model['power']
                depart(source, 'exile')
            elif mode == 'other_death':
                actions += spell('Murder', 'removal', 'unrelated')+[step('resolve'), step('assert_choices_consumed')]
                depart(entities['unrelated'])
            elif combat_mode:
                actions += coupled_combat('before')
                if mode in ('block_ground_cycle', 'block_flying_cycle'):
                    actions += [step('advance_turn'), step('untap_step', player=controller), step('turn', player=controller, value=0)]
                    for entity in entities.values():
                        if entity['alive']:
                            entity.update(damage=0, boost=0)
                            if entity['player'] == controller:
                                entity.update(tapped=0, sick=0)
                else:
                    actions += next_own_turn()
                actions += removal()
                actions += coupled_combat('after')
            else:
                if mode in ('stolen_death', 'stolen_second_death'):
                    thief = 1-controller
                    actions += [step('advance_turn'), step('untap_step', player=thief), step('turn', player=thief, value=0)]
                    source['sick'] = 0
                    actions += spell('Act of Treason', 'theft', source['ref'], caster=thief)+[step('resolve'), step('refresh'), step('assert_choices_consumed')]
                    source.update(ref='stolen_source', player=thief, tapped=0, temporary_haste=True)
                    actions += [step('bind_permanent', ref='stolen_source', player=thief, value=csvid)]+characteristics()
                if mode == 'new_object':
                    actions += spell('Giant Growth', 'growth', source['ref'], caster=controller)+[step('resolve')]
                    source['boost'] = 3
                    actions += characteristics()
                    actions += spell('Twiddle', 'tap_source', source['ref'], dialog=0)+[step('resolve')]
                    source['tapped'] = 1
                    actions += spell('Shock', 'injury', source['ref'])+[step('resolve')]
                    source['damage'] = 2
                    actions += characteristics()
                if mode in ('preexisting_counter', 'opposite_counter'):
                    plus = (model['mechanism'] == 'undying') == (mode == 'preexisting_counter')
                    actions += counter_control(plus)
                if mode == 'pump_before_death':
                    actions += pump()
                already_returned = mode == 'opposite_counter' and return_number > 0
                if not already_returned:
                    if mode == 'destroy':
                        actions += [step('destroy', ref=source['ref'])]+recurrence()+characteristics()
                    elif mode == 'lethal_damage':
                        spell_name = 'Shock' if model['toughness'] <= 2 else 'Lightning Bolt' if model['toughness'] <= 3 else 'Lightning Blast'
                        actions += spell(spell_name, 'damage_removal', source['ref'])+[step('resolve'), step('assert_choices_consumed')]
                        actions += recurrence()+characteristics()
                    else:
                        actions += removal()
                if mode in ('second_death', 'foreign_return_second', 'stolen_second_death'):
                    actions += removal('murder_2')
                if mode == 'counter_reset':
                    actions += counter_control(model['mechanism'] == 'persist')
                    actions += removal('murder_2')
                if mode.startswith('pump_') and mode != 'pump_before_death':
                    actions += pump()
                    if mode == 'pump_repeat':
                        actions += pump()
                    if mode == 'pump_cleanup':
                        source['boost'] = 0
                        actions += [step('cleanup'), step('refresh')]+characteristics()
                if mode == 'cleanup_once':
                    actions += [step('cleanup'), step('refresh')]+characteristics()

        checks = [step('assert_stack', value=0), step('assert_choices_consumed')]+mana_checks()+characteristics()
        if not source['alive']:
            checks += [observe(source['ref'], 'in_play', 0), observe(source['ref'], 'in_hand', int(source['hand']))]
        universe = {c['name'] for c in setup}
        for player in (0, 1):
            checks += [step('assert_life', player=player, value=life[player]), step('assert_poison', player=player, value=0),
                       step('assert_energy', player=player, value=0),
                       step('assert_zone', player=player, zone='battlefield', value=sum(e['alive'] and e['player'] == player for e in entities.values()))]
            for zone, players in zones.items():
                counts = Counter(players[player])
                checks.append(step('assert_zone', player=player, zone=zone, value=len(players[player])))
                checks += [step('assert_zone_multiplicity', player=player, zone=zone, index=registry[n]['csvid'], value=counts[n]) for n in sorted(universe)]
                if zone == 'library':
                    checks += [step('assert_zone_card', player=player, zone=zone, index=i, value=registry[n]['csvid']) for i, n in enumerate(players[player])]
            checks += [step('assert_zone_multiplicity', player=player, zone='battlefield', index=registry[n]['csvid'],
                            value=sum(e['alive'] and e['player'] == player and e['name'] == n for e in entities.values())) for n in sorted(universe)]
        sequence = pre+actions+checks
        if mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot', 'pump_snapshot'):
            cut = {'ready_snapshot': ready_cut, 'pending_snapshot': pending_cut, 'death_snapshot': death_cut, 'pump_snapshot': pump_cut}[mode]
            if cut is None:
                raise ValueError('recurrence snapshot lacks actual phase boundary')
            tail = actions[cut:]+checks
            sequence = pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'), step('ai_roundtrip'), step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        elif mode == 'resolved_snapshot':
            sequence += [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore'), step('assert_recorded_state')]
        case = dict(name=f'CounterConditionRecurrenceRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                    setup=setup, steps=sequence, _recipe=dict(family=FAMILY, version=2, variant=variant,
                    params=effective['params'], oracle_text=declaration['oracle_text'],
                    untested_components=['Graveyard exile in response to the recurrence trigger', 'multiplayer',
                                         'replacement/prevention interactions beyond token zone-change boundary',
                                         'strict object-generation number; behavioral new-object receipts are asserted']))
        if model['hybrid_pairs']:
            invalid_payment(case, effective, oracle[name])
            script_payments(case, effective, oracle[name])
        results.append(case)
    return results
