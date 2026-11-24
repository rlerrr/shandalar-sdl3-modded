"""Creature Auras that modify stats and optionally grant a combat ability."""

import re

FAMILY = 'static_aura_stats'
HYBRID_REMINDER = '({B/R} can be paid with either {B} or {R}.)'
RULE = re.compile(r'Enchant creature\nEnchanted creature gets ([+-]\d+)/([+-]\d+)'
                  r'(?: and has (menace|protection from creatures))?\.'
                  r'( \(It can\x27t be blocked except by two or more creatures\.\))?')
STATS = {'Giant Spider': (2, 4), 'Grizzly Bears': (2, 2), 'Eager Cadet': (1, 1), 'Suntail Hawk': (1, 1)}
RESPONSES = {'Unsummon': ('{U}', "Return target creature to its owner's hand."),
             'Naturalize': ('{1}{G}', 'Destroy target artifact or enchantment.')}
CREATURE_ABILITIES = {
    'Prodigal Sorcerer': '{T}: This creature deals 1 damage to any target.',
    'Thrashing Wumpus': '{B}: This creature deals 1 damage to each creature and each player.',
}


def parameters(record):
    if (record['layout'] != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record['type_line'] != 'Enchantment — Aura' or record['keywords'] != ['Enchant']):
        raise ValueError('expected a normal creature Aura with whole static rules')
    text = record['oracle_text']
    cost = record['mana_cost']
    hybrid = cost == '{B/R}'
    if hybrid:
        if not text.startswith(HYBRID_REMINDER + '\n'):
            raise ValueError('expected both printed hybrid alternatives')
        text = text.split('\n', 1)[1]
    elif not re.fullmatch(r'(?:\{[1-9]\})?(?:\{[BRW]\}){1,2}', cost):
        raise ValueError('expected a fixed black, red, or white Aura cost')
    match = RULE.fullmatch(text)
    if not match or match[4] and match[3] != 'menace':
        raise ValueError('expected exactly enchant creature, signed stats and a reviewed ability')
    power, toughness = map(int, match.groups()[:2])
    if power <= 0 or not -2 <= toughness <= 2 or toughness < 0 and match[3]:
        raise ValueError('Aura stat modifiers fall outside the reviewed fixtures')
    params = dict(power=power, toughness=toughness, menace=match[3] == 'menace', cost=cost)
    if match[3] == 'protection from creatures':
        params['protection_creatures'] = True
    return params


def spec(declaration, record):
    params = parameters(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('params') != params
            or declaration.get('oracle_text') != record['oracle_text']):
        raise ValueError('Aura parameters must match the complete printed effect')
    return dict(params, family=FAMILY)


def variants(model):
    modes = ('own', 'opponent', 'fizzle', 'remove_aura', 'leave_host', 'short', 'replay', 'cleanup', 'no_target')
    if model['toughness'] < 0:
        modes += ('lethal',)
    if model['menace']:
        modes += ('menace_single', 'menace_pair')
    if model.get('protection_creatures'):
        modes += ('protected_block', 'protected_target', 'protected_damage')
    if model['cost'] == '{B/R}':
        modes += ('red_payment',)
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def validate_witnesses(oracle, rows):
    for name in (*STATS, *RESPONSES, *CREATURE_ABILITIES, 'Spellbook'):
        if rows[name]['kind'] != 'card' or oracle[name]['layout'] != 'normal':
            raise ValueError('Aura witness changed identity: ' + name)
    for name, (power, toughness) in STATS.items():
        record = oracle[name]
        if (record['power'], record['toughness']) != (str(power), str(toughness)):
            raise ValueError('Aura witness changed stats: ' + name)
    for name, (cost, rules) in RESPONSES.items():
        record = oracle[name]
        if (record['mana_cost'], record['oracle_text']) != (cost, rules) or rows[name]['handler'] in (
                None, '0', 'NULL', 'card_dummy'):
            raise ValueError('Aura witness changed spell: ' + name)
    for name, rules in CREATURE_ABILITIES.items():
        if (oracle[name]['oracle_text'] != rules or not oracle[name]['type_line'].startswith('Creature — ')
                or rows[name]['handler'] in (None, '0', 'NULL', 'card_dummy')):
            raise ValueError('Aura witness changed creature ability: ' + name)
    if oracle['Spellbook']['type_line'] != 'Artifact — Book' or oracle['Suntail Hawk']['keywords'] != ['Flying']:
        raise ValueError('Aura witness changed creature or artifact legality')


def expand(name, csvid, declaration, model, selected, rows, oracle):
    row = rows[name]
    if (row['csvid'] != csvid or row['kind'] != 'card' or row['handler'] in (None, '0', 'NULL', 'card_dummy')
            or not selected or len(set(selected)) != len(selected) or not set(selected) <= set(variants(model))):
        raise ValueError('expected an implemented Aura and unique reviewed variants')
    return [make_case(name, csvid, declaration, model, variant, rows) for variant in selected]


def make_case(name, csvid, declaration, model, variant, rows):
    from recipes import card, cost_from_oracle, observe, step

    mode, seat = variant.rsplit('_', 1)
    owner = int(seat)
    foe = 1 - owner
    victim_seat = foe if mode in ('opponent', 'lethal') else owner
    combat = mode.startswith('menace_') or mode == 'protected_block'
    victim = ('Grizzly Bears' if model['toughness'] == -2 else 'Eager Cadet') if mode == 'lethal' else 'Giant Spider'
    cost = model['cost']
    if cost == '{B/R}':
        cost = '{R}' if mode == 'red_payment' else '{B}'
    price = cost_from_oracle({'mana_cost': cost})
    pool = list(price)
    if mode == 'short':
        pool[1:] = [0] * 5
    response = {'fizzle': 'Unsummon', 'remove_aura': 'Naturalize', 'leave_host': 'Unsummon'}.get(mode)
    reply_pool = cost_from_oracle({'mana_cost': RESPONSES[response][0]}) if response else [0] * 6
    setup = [card(name, 'source', owner, 'hand')]
    steps = [step('turn', player=owner, value=0)]

    def count(player, zone, card_name, value):
        return step('assert_zone_multiplicity', player=player, zone=zone, index=rows[card_name]['csvid'], value=value)

    def stats(pair, ref='victim'):
        return [observe(ref, 'power', pair[0]), observe(ref, 'toughness', pair[1])]

    for player, values in ((owner, pool), (foe, reply_pool)):
        steps += [step('set_mana', player=player, color=color, value=value) for color, value in enumerate(values)]
    if mode == 'no_target':
        setup.append(card('Spellbook', 'artifact', foe))
        steps.append(step('can_cast', ref='source', value=0))
    else:
        setup.append(card(victim, 'victim', victim_seat))
    if mode in ('short', 'no_target'):
        steps += [step('cast', ref='source', value=0), step('assert_stack', value=0),
                  count(owner, 'hand', name, 1), count(owner, 'graveyard', name, 0)]
        steps += [step('assert_mana', player=owner, color=c, value=n) for c, n in enumerate(pool)]
    else:
        original = STATS[victim]
        modified = (original[0] + model['power'], original[1] + model['toughness'])
        if not combat and mode != 'lethal':
            setup.append(card('Grizzly Bears', 'foil', foe))
        if response:
            setup.append(card(response, 'reply', foe, 'hand'))
        steps += stats(original)
        steps += [step('choose_target', player=owner, ref='victim'), step('cast', ref='source', value=1),
                  step('assert_stack', value=1)]
        if mode == 'fizzle':
            steps += [step('announce_spell', ref='source'), step('choose_target', player=foe, ref='victim'),
                      step('cast_response', ref='reply', value=1), step('assert_stack', value=2),
                      step('resolve_response'), count(owner, 'hand', victim, 1),
                      count(owner, 'graveyard', name, 1), count(foe, 'graveyard', response, 1)]
        else:
            result = [step('resolve'), step('refresh'), count(owner, 'hand', name, 0)]
            if mode == 'lethal':
                result += [count(victim_seat, 'graveyard', victim, 1), count(owner, 'graveyard', name, 1)]
            else:
                result += [observe('source', 'in_play', 1), *stats(modified)]
            if mode == 'replay':
                steps += [step('capture'), *result, step('record_state'), step('ai_roundtrip'), step('restore'),
                          *[dict(action) for action in result], step('assert_recorded_state')]
            else:
                steps += result
        if mode in ('remove_aura', 'leave_host'):
            target = 'source' if mode == 'remove_aura' else 'victim'
            steps += [step('choose_target', player=foe, ref=target), step('cast', ref='reply', value=1),
                      step('resolve'), step('refresh'), count(owner, 'graveyard', name, 1),
                      count(foe, 'graveyard', response, 1)]
            steps += stats(original) if mode == 'remove_aura' else [count(owner, 'hand', victim, 1)]
        if mode == 'cleanup':
            steps += [step('cleanup'), step('refresh'), observe('source', 'in_play', 1), *stats(modified)]
        if mode == 'protected_target':
            setup.append(card('Prodigal Sorcerer', 'pinger', foe))
            steps += [step('probe_activation_target', ref='pinger', other='victim', index=-1, value=0),
                      step('probe_activation_target', ref='pinger', other='foil', index=-1, value=1)]
        if mode == 'protected_damage':
            setup.append(card('Thrashing Wumpus', 'pinger', foe))
            steps += [step('set_mana', player=foe, color=1, value=1),
                      step('activate', ref='pinger', value=1), step('assert_stack', value=1), step('resolve'),
                      step('refresh'), observe('victim', 'damage', 0), observe('foil', 'damage', 1),
                      observe('pinger', 'damage', 1), step('assert_life', player=owner, value=19),
                      step('assert_life', player=foe, value=19)]
        if combat:
            pair = mode == 'menace_pair'
            setup.append(card('Suntail Hawk', 'first', foe))
            if pair:
                setup.append(card('Suntail Hawk', 'second', foe))
            steps += [step('begin_attackers'), step('attack', ref='victim', value=1), step('finish_attackers'),
                      step('begin_blockers'), step('block', ref='first', other='victim', value=int(pair))]
            if pair:
                steps.append(step('block', ref='second', other='victim', value=1))
            steps += [step('advance_combat'), observe('victim', 'blocked', int(pair)),
                      observe('first', 'blocking_count', int(pair))]
            if pair:
                steps.append(observe('second', 'blocking_count', 1))
            else:
                steps += [step('combat_damage'), step('resolve'), step('end_combat'),
                          step('assert_life', player=foe, value=20 - modified[0])]
        elif mode != 'lethal':
            steps += stats(STATS['Grizzly Bears'], 'foil')
        steps.append(step('assert_stack', value=0))
        for player in (owner, foe):
            steps += [step('assert_mana', player=player, color=color, value=0) for color in range(6)]
    return dict(name=f'StaticAuraStatsRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=211,
                setup=setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=[]))
