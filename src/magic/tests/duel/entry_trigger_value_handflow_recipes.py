"""Whole-Oracle entry hand flows using the immutable exchange receipt model.

This adapter validates its own broader entry grammar and real source types.
The shared model is used directly for zone arithmetic, never to rewrite an
Oracle record or pretend that an enchantment is a creature.
"""
import copy
import json
import re
from pathlib import Path

import hand_exchange_recipes as exchange

FAMILY = 'entry_trigger_value'
DATA = Path(__file__).parent / 'data'
AMOUNTS = {'a card': 1, 'two cards': 2, 'three cards': 3, 'four cards': 4}


def pins():
    return json.loads((DATA / 'entry_trigger_value_wave8_handflow_profiles.json').read_text(encoding='utf8'))


def mechanism(record):
    types = set(record.get('type_line', '').partition(' — ')[0].split()) if record else set()
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not types & {'Creature', 'Artifact', 'Enchantment'}
            or {'Aura', 'Equipment'} & set(record.get('type_line', '').split())
            or re.search(r'\{[^}]*[/X][^}]*\}', record.get('mana_cost', ''))):
        raise ValueError('entry hand flow requires a standalone ordinary permanent with a fixed mana cost')
    prefix = r'When (?:this creature|this artifact|this enchantment|' + re.escape(record['name']) + r') enters, '
    clauses = [line for line in record['oracle_text'].splitlines() if re.match(prefix, line)]
    if len(clauses) != 1: raise ValueError('entry hand flow requires exactly one complete entry clause')
    body = re.sub(prefix, '', clauses[0], count=1)
    if body == 'you may discard a card. If you do, draw a card.':
        p = dict(kind='loot', recipient='self', discard=1, draw='discarded', optional='discard')
    elif body == 'you may have target opponent discard a card.':
        p = dict(kind='discard', recipient='opponent', discard=1, draw=0, optional='trigger')
    else:
        match = re.fullmatch(r'discard (a card|two cards|three cards)\.', body)
        if match: p = dict(kind='discard', recipient='self', discard=AMOUNTS[match[1]], draw=0, optional='none')
        else:
            match = re.fullmatch(r'draw (a card|two cards|three cards|four cards), then discard (a card|two cards|three cards)\.', body)
            if not match: raise ValueError('unsupported entry hand-flow clause')
            p = dict(kind='draw_discard', recipient='self', draw=AMOUNTS[match[1]], discard=AMOUNTS[match[2]], optional='none')
    p['clause'] = clauses[0]
    p['gaps'] = [line for line in record['oracle_text'].splitlines() if line != clauses[0]]
    return p


def spec(d, record):
    from recipes import cost_from_oracle
    if (d.get('family') != FAMILY or type(d.get('version')) is not int or d['version'] != 2
            or not isinstance(d.get('variants'), list) or not d['variants']
            or len(d['variants']) != len(set(d['variants']))):
        raise ValueError('entry hand flow requires version two and explicit distinct variants')
    params = d.get('params')
    if not isinstance(params, dict) or set(params) != {'schema', 'mechanism', 'presentation'} or type(params['schema']) is not int or params['schema'] != 2:
        raise ValueError('entry hand flow requires its explicit schema, mechanism and input presentation')
    profile = pins()['sources'].get(record.get('name')) if record else None
    parsed = mechanism(record)
    presentation = params['presentation']
    if (not isinstance(presentation, dict) or set(presentation) != {'human_optional_trigger', 'target_player_choices'}
            or type(presentation['human_optional_trigger']) is not bool
            or type(presentation['target_player_choices']) is not int or presentation['target_player_choices'] not in (0, 1)):
        raise ValueError('unreviewed entry hand-flow input presentation')
    if (not profile or profile['oracle'] != record or d.get('oracle_text') != record['oracle_text']
            or profile['mechanism'] != parsed or params['mechanism'] != parsed
            or presentation != profile['presentation']):
        raise ValueError('entry hand-flow source, whole Oracle and parameters must match')
    creature = 'Creature' in record['type_line'].partition(' — ')[0].split()
    if creature and any(not str(record.get(k, '')).isdigit() for k in ('power', 'toughness')):
        raise ValueError('entry hand flow requires fixed numeric creature stats')
    return dict(parsed, family=FAMILY, handflow=True, csvid=profile['csvid'], source_name=record['name'],
                presentation=copy.deepcopy(presentation), mana=cost_from_oracle(record), creature=creature,
                legendary='Legendary' in record['type_line'], power=int(record['power']) if creature else 0,
                toughness=int(record['toughness']) if creature else 0)


def _core(p, no_effect=False):
    operation = lambda kind, amount: dict(kind=kind, amount=amount, scope='recipient', random=False)
    if no_effect: operations = []
    elif p['kind'] == 'draw_discard': operations = [operation('draw', p['draw']), operation('discard', p['discard'])]
    elif p['kind'] == 'loot': operations = [operation('discard', p['discard']), operation('draw', 'discarded')]
    else: operations = [operation('discard', p['discard'])]
    return dict(mode='entry', recipient=p['recipient'], operations=operations,
                cost=dict(mana='', tap=False, sacrifice=False, pay_life=0, discard=0), keywords=[], mana_ability=False,
                family='hand_exchange', activation_mana=[0]*6, mana=p['mana'], creature=p['creature'], land=False,
                legendary=p['legendary'], power=p['power'], toughness=p['toughness'], untested_rules=p['gaps'], instant=False)


def variants(p):
    supported = exchange.variants(_core(p))
    if not p['creature']: supported = [v for v in supported if not v.startswith('blink_source_')]
    if p['optional'] == 'discard': supported += ['decline_0', 'decline_1', 'decline_trigger_0']
    elif p['optional'] == 'trigger': supported += ['decline_0']
    return supported


def validate_witnesses(oracle, rows):
    data = pins()
    for name, record in data['witnesses'].items():
        if oracle.get(name) != record or name not in rows:
            raise ValueError('entry hand-flow witness changed: ' + name)
    exchange.validate_witnesses(oracle, rows)
    for name, profile in data['sources'].items():
        if (oracle.get(name) != profile['oracle'] or name not in rows or rows[name]['csvid'] != profile['csvid']
                or rows[name]['kind'] != 'card' or rows[name]['handler'] in ('NULL', 'card_dummy', None)):
            raise ValueError('entry hand-flow source identity or implementation changed: ' + name)


def expand(name, csvid, d, p, selected, rows, oracle):
    if csvid != p['csvid'] or not selected or len(selected) != len(set(selected)) or set(selected)-set(variants(p)):
        raise ValueError('entry hand flow requires matching identity and distinct supported variants')
    validate_witnesses(oracle, rows)
    output = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1); owner = int(owner)
        decline = mode in ('decline', 'decline_trigger')
        legacy = 'effect_'+str(owner) if decline else variant
        bridge = dict(d, target_choices=p['presentation']['target_player_choices'])
        case = exchange.expand(name, csvid, bridge, _core(p, no_effect=decline), [legacy], rows, oracle)[0]
        source_pending = False; captured_pending = False; transformed = []
        for operation in case['steps']:
            current = dict(operation)
            if p['optional'] == 'discard' and mode == 'decline' and current['op'] == 'discard_policy': current['value'] = 0
            if current['op'] == 'capture': captured_pending = source_pending
            if current['op'] == 'restore': source_pending = captured_pending
            if current['op'] == 'cast' and current.get('ref') == 'source': source_pending = current['value'] == 1
            if current['op'] == 'activate' and current.get('ref') == 'blinker': source_pending = True
            if current['op'] == 'resolve' and source_pending:
                if p['presentation']['human_optional_trigger'] and owner == 0 and mode != 'decline_trigger' and not (mode == 'decline' and p['optional'] == 'trigger'):
                    transformed.append(dict(op='choose_trigger', ref='source', player=owner))
                if p['optional'] == 'discard' and mode == 'decline':
                    fallback = next(item['name'] for item in case['setup'] if item['player'] == owner and item['zone'] == 'hand' and item['ref'] != 'source')
                    transformed.append(dict(op='choose_optional_zone', player=owner, value=rows[fallback]['csvid']))
                transformed.append(current)
                if p['optional'] == 'discard' and mode == 'decline': transformed.append(dict(op='assert_forced_zone_choices', value=0))
                source_pending = False
                continue
            transformed.append(current)
            if current['op'] == 'cast' and current.get('ref') == 'source' and current['value'] == 1:
                transformed.append(dict(op='assert_card', ref='source', property='in_hand', value=0))
        case['steps'] = transformed
        case['name'] = f'EntryTriggerHandFlowRecipe_{csvid}_{variant}'
        case['_recipe'] = dict(family=FAMILY, version=2, variant=variant, params=d['params'], oracle_text=d['oracle_text'],
                               entry_clause=p['clause'], untested_components=p['gaps'] + ['multiplayer', 'discard/draw replacement effects', 'randomness distribution'])
        output.append(case)
    return output
