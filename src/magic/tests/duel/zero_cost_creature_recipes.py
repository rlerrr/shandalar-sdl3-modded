"""Free casts for the red Kobolds and Shield Sphere."""

FAMILY = 'zero_cost_creature'
SHIELD_RULES = 'Defender\nWhenever this creature blocks, put a -0/-1 counter on it.'
TASKMASTER_RULES = 'Other Kobold creatures you control get +1/+0.'


def parameters(record):
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('mana_cost') != '{0}' or record.get('cmc') != 0):
        raise ValueError('a printed zero-cost normal creature is required')
    identity = (record['oracle_text'], record['type_line'], record['power'], record['toughness'],
                record['colors'], record['keywords'])
    if identity == ('', 'Creature — Kobold', '0', '1', ['R'], []):
        return dict(color=16, toughness=1, defender=False)
    if identity == (SHIELD_RULES, 'Artifact Creature — Wall', '0', '6', [], ['Defender']):
        return dict(color=0, toughness=6, defender=True)
    raise ValueError('complete zero-cost Kobold or Shield Sphere rules are required')


def spec(declaration, record):
    params = parameters(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or declaration.get('params') != params):
        raise ValueError('zero-cost creature declaration differs from its printed rules')
    return dict(params, family=FAMILY)


def variants(model):
    modes = ('cast', 'surplus', 'replay', 'attack', 'block', 'unblocked')
    modes += ('repeat_block',) if model['defender'] else ('kobold_lord',)
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def validate_witnesses(oracle, rows):
    witness_text = {'Kobold Taskmaster': TASKMASTER_RULES, 'Grizzly Bears': '',
                    'Llanowar Elves': '{T}: Add {G}.', 'Elvish Mystic': '{T}: Add {G}.'}
    for witness, text in witness_text.items():
        row = rows[witness]
        passive = witness == 'Grizzly Bears'
        if (oracle[witness]['oracle_text'] != text or row['kind'] != 'card'
                or (not passive and row['handler'] in (None, 'NULL', '0', 'card_dummy'))):
            raise ValueError('zero-cost creature witness changed: ' + witness)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    if (rows[name]['csvid'] != csvid or not selected or len(set(selected)) != len(selected)
            or not set(selected) <= set(variants(model))):
        raise ValueError('zero-cost creature cases need an implemented identity and unique variants')
    return [make_case(name, csvid, declaration, model, variant) for variant in selected]


def make_case(name, csvid, declaration, model, variant):
    from recipes import card, observe, step

    mode, seat = variant.rsplit('_', 1)
    owner = int(seat)
    foe = 1 - owner
    defender = model['defender']
    toughness = model['toughness']
    casting = mode in ('cast', 'surplus', 'replay', 'kobold_lord')
    setup = [card(name, 'source', owner, 'hand' if casting else 'battlefield')]
    steps = [step('turn', player=owner, value=0)]

    def zone(player, location, value):
        return step('assert_zone_multiplicity', player=player, zone=location, index=csvid, value=value)

    def traits(power, current_toughness):
        return [observe('source', 'in_play', 1), observe('source', 'power', power),
                observe('source', 'toughness', current_toughness), zone(owner, 'battlefield', 1),
                *[step('assert_matching_permanents', player=owner, index=csvid, property=prop, value=value)
                  for prop, value in [('color', model['color']), ('artifact', int(defender)), ('token', 0)]]]

    if mode == 'kobold_lord':
        setup += [card('Kobold Taskmaster', 'lord', owner), card('Grizzly Bears', 'other', owner)]
    if casting:
        pool = [2, 3, 4, 5, 6, 7] if mode == 'surplus' else [0] * 6
        steps += [step('set_mana', player=player, color=color, value=value)
                  for player, amounts in ((owner, pool), (foe, [0] * 6)) for color, value in enumerate(amounts)]
        steps += [step('cast', ref='source', value=1), step('assert_stack', value=1)]
        result = [step('resolve'), step('refresh'), step('assert_stack', value=0),
                  zone(owner, 'hand', 0), zone(owner, 'graveyard', 0),
                  *traits(int(mode == 'kobold_lord'), toughness), observe('source', 'tapped', 0),
                  *[step('assert_mana', player=player, color=color, value=value)
                    for player, amounts in ((owner, pool), (foe, [0] * 6)) for color, value in enumerate(amounts)]]
        if mode == 'replay':
            steps += [step('capture'), *result, step('record_state'), step('ai_roundtrip'), step('restore'),
                      *[dict(action) for action in result], step('assert_recorded_state')]
        else:
            steps += result
        steps += [step('begin_attackers'), step('attack', ref='source', value=0), step('finish_attackers')]
    if mode == 'kobold_lord':
        steps += [observe('lord', 'power', 1), observe('other', 'power', 2)]
    if mode == 'attack':
        steps += traits(0, toughness) + [step('begin_attackers'), step('attack', ref='source', value=int(not defender)),
                                         observe('source', 'tapped', int(not defender)), step('finish_attackers'),
                                         step('begin_blockers'), step('advance_combat'), step('combat_damage'),
                                         step('resolve'), step('end_combat'), step('assert_life', player=foe, value=20)]
    if mode in ('block', 'unblocked', 'repeat_block'):
        blocked = mode != 'unblocked'
        attackers = ('Llanowar Elves', 'Elvish Mystic') if mode == 'repeat_block' else ('Llanowar Elves',)
        for index, attacker in enumerate(attackers):
            ref = f'attacker_{index}'
            setup.append(card(attacker, ref, foe))
            before = toughness - index
            after = before - int(defender and blocked)
            steps += [step('turn', player=foe, value=0), step('begin_attackers'), step('attack', ref=ref, value=1),
                      step('finish_attackers'), step('begin_blockers')]
            if blocked:
                steps.append(step('block', ref='source', other=ref, value=1))
            steps += [observe('source', 'toughness', before), step('advance_combat'), step('resolve'),
                      *traits(0, after), step('combat_damage'), step('resolve'), step('end_combat'),
                      step('assert_life', player=owner, value=20 if blocked else 19), observe(ref, 'in_play', 1)]
            if defender or not blocked:
                steps += [observe('source', 'damage', int(blocked)), step('cleanup'),
                          *traits(0, after), observe('source', 'damage', 0), zone(owner, 'graveyard', 0)]
            else:
                steps += [observe('source', 'in_play', 0), zone(owner, 'battlefield', 0), zone(owner, 'graveyard', 1)]
    return dict(name=f'ZeroCostCreatureRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=163,
                setup=setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                params=declaration['params'], oracle_text=declaration['oracle_text'],
                untested_components=['Shield Sphere counter identity is inferred from persistent toughness changes.']
                if defender else []))
