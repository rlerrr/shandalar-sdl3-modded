"""Instants that switch a creature's power and toughness for a turn."""

FAMILY = 'instant_switch_stats'
RULES = "Switch target creature's power and toughness until end of turn."
COSTS = {'{1}{B}': (1, 'B'), '{R}': (0, 'R'), '{1}{U/R}': (1, 'UR'), '{U}': (0, 'U')}
LIBRARY = ('Mountain', 'Plains', 'Swamp')


def parameters(record):
    rules = record['oracle_text']
    cost = record['mana_cost']
    if (record['layout'] != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record['type_line'] != 'Instant' or cost not in COSTS
            or rules not in (RULES, RULES + '\nDraw a card.')):
        raise ValueError('expected a complete instant that switches power and toughness')
    generic, colors = COSTS[cost]
    if record['cmc'] != generic + 1:
        raise ValueError('power and toughness switch mana value differs from its cost')
    return dict(generic=generic, colors=colors, draw=int(rules != RULES))


def spec(declaration, record):
    params = parameters(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or declaration.get('params') != params):
        raise ValueError('power and toughness switch declaration differs from its printed rules')
    return dict(params, family=FAMILY)


def variants(model):
    modes = ('cast', 'offturn', 'expiry', 'twice', 'fizzle', 'countered', 'short', 'combat', 'equal_stats')
    if model['colors'] == 'UR':
        modes += ('red_hybrid',)
    return [f'{mode}_{player}' for player in (0, 1) for mode in modes]


def validate_witnesses(oracle, rows):
    creatures = {'Giant Spider': ('2', '4'), 'Grizzly Bears': ('2', '2'), 'Hill Giant': ('3', '3')}
    responses = {'Unsummon': ('{U}', "Return target creature to its owner's hand."),
                 'Negate': ('{1}{U}', 'Counter target noncreature spell.')}
    for name in (*creatures, *responses, *LIBRARY):
        record, row = oracle[name], rows[name]
        if row['kind'] != 'card' or record['layout'] != 'normal':
            raise ValueError('invalid power and toughness switch witness: ' + name)
        if name in creatures and (record['power'], record['toughness']) != creatures[name]:
            raise ValueError('power and toughness switch witness stats changed: ' + name)
        if name in responses and ((record['mana_cost'], record['oracle_text']) != responses[name]
                                  or row['handler'] in (None, 'NULL', '0', 'card_dummy')):
            raise ValueError('power and toughness switch response changed: ' + name)
        if name in LIBRARY and record['type_line'] != f'Basic Land — {name}':
            raise ValueError('power and toughness switch library witness changed: ' + name)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    row = rows[name]
    if (row['csvid'] != csvid or row['kind'] != 'card' or row['handler'] in (None, 'NULL', '0', 'card_dummy')
            or not selected or len(set(selected)) != len(selected) or not set(selected) <= set(variants(model))):
        raise ValueError('power and toughness switch needs an implemented identity and unique variants')
    return [make_case(name, csvid, declaration, model, variant, rows) for variant in selected]


def make_case(name, csvid, declaration, model, variant, rows):
    from recipes import COLORS, card, observe, step

    mode, seat = variant.rsplit('_', 1)
    owner = int(seat)
    foe = 1 - owner
    casts = 2 if mode == 'twice' else 1
    target_owner = owner if mode in ('combat', 'equal_stats') else foe
    target = 'Grizzly Bears' if mode == 'equal_stats' else 'Giant Spider'
    original = (2, 2) if mode == 'equal_stats' else (2, 4)
    chosen_color = COLORS[model['colors'][-1] if mode == 'red_hybrid' else model['colors'][0]]
    pool = [casts * model['generic'], 0, 0, 0, 0, 0]
    pool[chosen_color] = 0 if mode == 'short' else casts
    response = {'fizzle': 'Unsummon', 'countered': 'Negate'}.get(mode)
    foe_pool = [int(mode == 'countered'), 0, int(response is not None), 0, 0, 0]
    setup = [card(name, f'source_{i}', owner, 'hand') for i in range(casts)]
    setup += [card(target, 'target', target_owner), card('Grizzly Bears', 'foil', foe)]
    setup += [card(land, f'library_{i}', owner, 'library') for i, land in enumerate(LIBRARY)]
    steps = [step('turn', player=foe if mode == 'offturn' else owner, value=0)]
    steps += [step('set_mana', player=player, color=color, value=value)
              for player, amounts in ((owner, pool), (foe, foe_pool)) for color, value in enumerate(amounts)]

    def identity(card_name):
        return rows[card_name]['csvid']

    def count(player, zone, card_name, value):
        return step('assert_zone_multiplicity', player=player, zone=zone, index=identity(card_name), value=value)

    def stats(ref, pair):
        return [observe(ref, 'power', pair[0]), observe(ref, 'toughness', pair[1])]

    if mode == 'combat':
        setup.append(card('Hill Giant', 'blocker', foe))
        steps += [step('begin_attackers'), step('attack', ref='target', value=1), step('finish_attackers'),
                  step('begin_blockers'), step('block', ref='blocker', other='target', value=1)]
    if response:
        setup.append(card(response, 'reply', foe, 'hand'))
    steps += stats('target', original)
    drawn = 0
    for index in range(casts):
        source = f'source_{index}'
        if mode == 'short':
            steps += [step('cast', ref=source, value=0), step('assert_stack', value=0),
                      count(owner, 'hand', name, 1), count(owner, 'graveyard', name, 0), *stats('target', original)]
            break
        steps += [step('choose_target', player=owner, ref='target'), step('cast', ref=source, value=1),
                  step('assert_stack', value=1)]
        if response:
            steps.append(step('announce_spell', ref=source))
            if mode == 'fizzle':
                steps.append(step('choose_target', player=foe, ref='target'))
            steps += [step('cast_response', ref='reply', value=1), step('assert_stack', value=2),
                      step('resolve_response'), count(foe, 'graveyard', response, 1)]
        else:
            steps.append(step('resolve'))
            drawn += model['draw']
        steps += [step('refresh'), step('assert_stack', value=0), count(owner, 'hand', name, casts - index - 1),
                  count(owner, 'graveyard', name, index + 1)]
        if mode == 'fizzle':
            steps += [count(target_owner, 'hand', target, 1), count(target_owner, 'battlefield', target, 0)]
        else:
            result = original if response or index == 1 else original[::-1]
            steps += stats('target', result)
    steps += stats('foil', (2, 2))
    steps += [step('assert_zone', player=owner, zone='library', value=len(LIBRARY) - drawn),
              step('assert_zone_card', player=owner, zone='library', index=0, value=identity(LIBRARY[drawn]))]
    steps += [count(owner, 'hand', land, int(i < drawn)) for i, land in enumerate(LIBRARY)]
    for player, amounts in ((owner, pool if mode == 'short' else [0] * 6), (foe, [0] * 6)):
        steps += [step('assert_mana', player=player, color=color, value=value) for color, value in enumerate(amounts)]
    if mode == 'expiry':
        steps += [step('cleanup'), step('refresh'), *stats('target', original)]
    if mode == 'combat':
        steps += [step('advance_combat'), step('combat_damage'), step('resolve'), step('end_combat'),
                  count(owner, 'graveyard', target, 1), count(foe, 'graveyard', 'Hill Giant', 1),
                  step('assert_life', player=foe, value=20)]
    return dict(name=f'InstantSwitchStatsRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=173,
                setup=setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=[]))
