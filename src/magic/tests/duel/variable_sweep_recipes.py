"""Complete ordinary single-X creature damage sweeps, using normal X payment.

The reviewed mixed-board sweep model supplies damage, zone and preservation
receipts. This family adds the variable payment boundary and mirrors the actual
casting player; it does not derive rules expectations from card handlers.
"""
import copy
import re

from sweep_recipes import expand as fixed_sweep, validate_witnesses as sweep_witnesses

VALUES = (0, 1, 2, 3, 5, 8)


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record.get('type_line') not in ('Instant', 'Sorcery')
            or record.get('keywords') or declaration.get('oracle_text') != record.get('oracle_text')
            or 'variants' not in declaration):
        raise ValueError('ordinary whole-Oracle single-X creature sweep required')
    parameters = declaration.get('params')
    if (not isinstance(parameters, dict) or set(parameters) != {'flying', 'players'}
            or parameters['flying'] not in ('all', 'with', 'without')
            or type(parameters['players']) is not bool):
        raise ValueError('explicit flying predicate and player damage scope required')
    qualifier = {'all': '', 'with': ' with flying', 'without': ' without flying'}[parameters['flying']]
    text = record['name'] + ' deals X damage to each creature' + qualifier
    text += ' and each player.' if parameters['players'] else '.'
    if record['oracle_text'] != text:
        raise ValueError('variable sweep parameters must match the complete printed effect')
    cost = record.get('mana_cost', '')
    if cost.count('{X}') != 1 or not re.fullmatch(r'(?:\{(?:X|[1-9][0-9]?|[WUBRGC])\})+', cost):
        raise ValueError('one X symbol and ordinary fixed base mana required')
    base = cost_from_oracle(dict(mana_cost=cost.replace('{X}', '')))
    return dict(parameters, family='variable_sweep', base_mana=base,
                instant=record['type_line'] == 'Instant')


def validate_witnesses(oracle, registry):
    sweep_witnesses(oracle, registry)
    counter = oracle.get('Ertai, Wizard Adept', {})
    if ('Ertai, Wizard Adept' not in registry
            or counter.get('oracle_text') != '{2}{U}{U}, {T}: Counter target spell.'
            or (counter.get('power'), counter.get('toughness'), counter.get('colors')) != ('1', '1', ['U'])):
        raise ValueError('variable sweep counter witness changed')


def variants(parameters):
    choices = [(variant, x) for x in VALUES for variant in ('mixed', 'payment')]
    choices += [(variant, x) for x in (0, 5)
                for variant in ('ready_snapshot', 'pending_snapshot', 'resolved_snapshot')]
    extra = ['friendly_only', 'opponent_only', 'empty_board', 'empty_zones', 'injured',
             'cleanup', 'tapped_all', 'untapped_all', 'countered_cast', 'insufficient_mana', 'wrong_color']
    if parameters['instant']:
        extra.append('opposing_turn')
    choices += [(variant, 3) for variant in extra]
    return [f'{variant}_x{x}_{owner}' for owner in (0, 1) for variant, x in choices]


def expand(name, csvid, declaration, parameters, selected, registry, oracle):
    from recipes import card, step, observe
    result = []
    for variant in selected:
        mode, tail = variant.rsplit('_x', 1)
        x, owner = map(int, tail.split('_'))
        mana = list(parameters['base_mana'])
        mana[0] += x
        concrete = dict(family='sweep', effect='damage', targets=['creature'], mana=mana,
                        amount=0 if mode == 'countered_cast' else x,
                        players='all' if parameters['players'] else 'none')
        if parameters['flying'] != 'all':
            concrete['flying'] = parameters['flying'] == 'with'
        base_variant = ('mixed' if mode.endswith('_snapshot') or mode in ('opposing_turn', 'countered_cast')
                        else 'insufficient_mana' if mode == 'wrong_color' else mode)
        case = fixed_sweep(name, csvid, declaration, concrete, [base_variant], registry, oracle)[0]
        actions = []
        for action in case['steps']:
            if action['op'] == 'cast' and action.get('ref') == 'source' and mode not in ('insufficient_mana', 'wrong_color'):
                actions.append(step('choose_x', player=0, value=x))
            actions.append(action)
        if mode == 'wrong_color':
            wrong = list(mana)
            color = next(i for i in range(1, 6) if mana[i])
            wrong[0] += wrong[color]
            wrong[color] = 0
            for action in actions:
                if action['op'] == 'set_mana' and action.get('player') == 0:
                    action['value'] = wrong[action['color']]
                if action['op'] == 'assert_mana_total' and action.get('player') == 0:
                    action['value'] = sum(wrong)
        # The fixed sweep already checks the actual all-zone receipts. Capture
        # precisely the requested boundary, and replay only work after it.
        if mode.endswith('_snapshot'):
            first_query = next(i for i, a in enumerate(actions) if a['op'] == 'can_cast')
            resolution = next(i for i, a in enumerate(actions) if a['op'] == 'resolve')
            checks = next(i for i in range(resolution + 1, len(actions)) if actions[i]['op'] == 'assert_stack')
            if mode == 'ready_snapshot':
                before, replay = actions[:first_query], actions[first_query:]
            elif mode == 'pending_snapshot':
                before, replay = actions[:resolution], actions[resolution:]
            else:
                before, replay = actions, actions[checks:]
            actions = before + [step('capture')] + (replay if mode != 'resolved_snapshot' else [])
            actions += [step('record_state'), step('ai_roundtrip')]
            if mode == 'resolved_snapshot':
                actions += [step('set_life', player=0, value=7), step('cleanup')]
            actions += [step('restore')] + copy.deepcopy(replay) + [step('assert_recorded_state')]
        if mode == 'opposing_turn':
            actions.insert(0, step('turn', player=1, value=0))
        # Mirror player ownership, including every independent zone receipt.
        if owner:
            for item in case['setup'] + actions:
                if item.get('player') in (0, 1):
                    item['player'] = 1 - item['player']
            if mode != 'opposing_turn':
                actions.insert(0, step('turn', player=1, value=0))
        if mode == 'countered_cast':
            # Match the established real-priority counter fixture: the human
            # controller supplies the offered Ertai activation, for either caster.
            case['setup'].append(card('Ertai, Wizard Adept', 'counter', player=0))
            rewritten = []
            for action in actions:
                if action['op'] == 'cast' and action.get('ref') == 'source':
                    rewritten.append(step('choose_priority', ref='counter'))
                    action = dict(action, value=0)
                elif action['op'] == 'assert_stack' and action['value'] == 1:
                    continue
                elif action['op'] == 'assert_mana_total' and action.get('player') == 0:
                    action = dict(action, value=action['value'] - 4)
                elif action['op'] == 'assert_zone' and action.get('player') == 0 and action['zone'] == 'battlefield':
                    action = dict(action, value=action['value'] + 1)
                rewritten.append(action)
            actions = rewritten + [observe('counter', 'in_play', 1), observe('counter', 'tapped', 1)]
        case['steps'] = actions
        case['name'] = f'VariableSweepRecipe_{csvid}_{variant}'
        case['_recipe'] = dict(family='variable_sweep', version=2, variant=variant, x=x,
                               params=declaration['params'], oracle_text=declaration['oracle_text'],
                               untested_components=['damage prevention/replacement', 'regeneration responses',
                                                    'damage/death triggers', 'multiplayer'])
        result.append(case)
    return result
