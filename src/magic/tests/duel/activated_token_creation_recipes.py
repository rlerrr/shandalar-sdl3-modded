"""Printed fixed-token activations with real costs and creator-derived traits.

Token registry records identify instances only. Their generic stats, colors and
rules are deliberately never used as expectations for another card's creation.
"""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from permanent_value_activation_recipes import parse_cost, mana
from token_creation_recipes import COLORS, KEYWORDS
from token_spell_extension_recipes import SUBTYPES, WORDS

FAMILY = 'activated_token_creation'
DATA = Path(__file__).parent / 'data/activated_token_creation_wave8_profiles.json'


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def creation(text):
    """Consume the entire selected effect, including the optional life rider."""
    match = re.fullmatch(
        r'Create (a|two|three|four|five|six) (tapped )?([0-8])/([1-9]|1[0-2]) '
        r'(white|blue|black|red|green|colorless)(?: and (white|blue|black|red|green))? '
        r'([A-Za-z ]+?) (artifact )?creature tokens?(?: with ([a-z ,]+))?\.'
        r'(?: You gain ([1-9][0-9]?) life\.)?', text)
    if not match:
        raise ValueError('complete fixed immediate token effect required')
    number, tapped, power, toughness, color, second, subtypes, artifact, keywords, life = match.groups()
    types = subtypes.split()
    keys = [k.capitalize() for k in re.split(r' and |, ', keywords)] if keywords else []
    if (not 1 <= len(types) <= 2 or len(set(types)) != len(types)
            or not set(types) <= SUBTYPES | {'Plant'} or len(set(keys)) != len(keys)
            or not set(keys) <= KEYWORDS & {'Flying', 'Haste', 'Deathtouch'} or second == color):
        raise ValueError('unreviewed token subtype/color/keyword form')
    return dict(name=' '.join(types), count=WORDS[number], power=int(power), toughness=int(toughness),
                colors=[COLORS[c] for c in (color, second) if c and c != 'colorless'],
                subtypes=types, artifact=bool(artifact), keywords=keys, tapped=bool(tapped),
                gain_life=int(life or 0))


def parse(record, ability):
    if (not isinstance(record, dict) or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record.get('type_line', '').startswith('Token ')
            or not any(t in record.get('type_line', '') for t in ('Creature', 'Artifact', 'Enchantment', 'Planeswalker'))
            or any(t in record.get('type_line', '') for t in ('Aura', 'Equipment'))):
        raise ValueError('normal whole standalone permanent required')
    lines = record['oracle_text'].splitlines()
    if type(ability) is not int or not 0 <= ability < len(lines) or ': ' not in lines[ability]:
        raise ValueError('explicit complete printed activation line required')
    cost_text, effect = lines[ability].split(': ', 1)
    cost = parse_cost(cost_text)
    if (cost['x'] or cost['pay_life'] or cost['sacrifice'] not in (None, 'land')
            or cost['discard'] not in (0, 1, 2) or 'counter' in cost):
        raise ValueError('activation cost needs a separate actual-lifecycle pilot')
    walker = 'Planeswalker' in record['type_line']
    creature = 'Creature' in record['type_line']
    if walker:
        if 'loyalty' not in cost or not re.fullmatch(r'\d+', record.get('loyalty', '')):
            raise ValueError('printed starting loyalty and real loyalty cost required')
        initial = int(record['loyalty'])
        if initial + cost['loyalty'] <= 0:
            raise ValueError('unaffordable or depleting loyalty needs a lifecycle pilot')
    elif 'loyalty' in cost:
        raise ValueError('loyalty cost on non-planeswalker')
    if creature and any(not re.fullmatch(r'\d+', record.get(k, '')) for k in ('power', 'toughness')):
        raise ValueError('numeric surviving source creature required')
    return dict(family=FAMILY, ability=ability, token=creation(effect), cost=cost, walker=walker, creature=creature,
                initial_loyalty=int(record['loyalty']) if walker else None,
                activation_mana=mana(cost['mana']), gaps=[s for i, s in enumerate(lines) if i != ability])


def spec(declaration, record):
    if declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2:
        raise ValueError('activated token family and integer version 2 required')
    params = declaration.get('params')
    if (not isinstance(params, dict) or set(params) != {'ability', 'interface', 'inactive_clauses'}
            or declaration.get('oracle_text') != (record or {}).get('oracle_text')):
        raise ValueError('whole Oracle text and explicit ability/interface/gaps required')
    result = parse(record, params['ability'])
    source = pins()['sources'].get(record['name'], {})
    if (not source or any(record.get(k) != value for k, value in source['oracle'].items())
            or source['ability'] != result['ability'] or params['interface'] != source['interface']
            or params['inactive_clauses'] != result['gaps']):
        raise ValueError('independently reviewed whole source/interface differs')
    interface = params['interface']
    required = {'dialogs', 'present_ai_dialogs', 'sacrifice_choices', 'casting_colors'}
    optional = {'silent_variants', 'exact_activation_budget_variants', 'manual_ai_discard'}
    if not required <= set(interface) or set(interface) - required - optional:
        raise ValueError('complete activation interface required')
    if (type(interface['present_ai_dialogs']) is not bool or not isinstance(interface['dialogs'], dict)
            or any(k not in ('0', '1') or type(v) is not int or not 0 <= v <= 16 for k, v in interface['dialogs'].items())
            or not isinstance(interface['sacrifice_choices'], dict)
            or any(k not in ('0', '1') or type(v) is not bool for k, v in interface['sacrifice_choices'].items())):
        raise ValueError('typed per-controller real menu and sacrifice inputs required')
    for key in ('silent_variants', 'exact_activation_budget_variants'):
        annotated = interface.get(key, [])
        if (not isinstance(annotated, list) or any(not isinstance(v, str) for v in annotated)
                or len(set(annotated)) != len(annotated) or not set(annotated) <= set(variants(result))):
            raise ValueError('explicit distinct supported interface variants required')
    budgets = interface.get('exact_activation_budget_variants', [])
    if budgets and (not result['cost']['tap'] or result['walker'] or not sum(result['activation_mana'])
                    or any(v.rsplit('_', 1)[0] not in ('tapped', 'repeat') for v in budgets)):
        raise ValueError('exact printed-cost budgets are scoped to tap-cost tapped/repeat controls')
    if ('manual_ai_discard' in interface and
            (type(interface['manual_ai_discard']) is not bool or not result['cost']['discard'])):
        raise ValueError('manual AI discard annotation requires a real discard cost and boolean')
    if interface['casting_colors']:
        from defensive_creature_recipes import reviewed_payment
        result['casting_mana'] = reviewed_payment(record, {'payment_colors': interface['casting_colors']})[0]
    else:
        result['casting_mana'] = mana(record['mana_cost'])
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(result))):
        raise ValueError('explicit nonempty distinct supported token variants required')
    return result


def variants(p):
    modes = ['create', 'payment', 'bare_zones', 'offturn',
             'ready_snapshot', 'pending_snapshot', 'resolved_snapshot', 'source_destroyed_pending',
             'cleanup', 'repeat', 'fresh_attack', 'ready_attack', 'block_ground', 'block_flying',
             'destroy_token', 'bounce_token', 'exile_token']
    # Planeswalkers already acquire printed loyalty by casting in every case.
    # Extra cast/fresh cases would repeat the same model, and a tapped walker
    # requires a real tapping action after that cast rather than fixture state.
    if not p['walker']:
        modes += ['tapped', 'cast']
    if sum(p['activation_mana']):
        modes += ['insufficient_mana']
    if any(p['activation_mana'][1:]):
        modes += ['wrong_color']
    if p['cost']['discard']:
        modes += ['empty_hand']
        if p['cost']['discard'] > 1:
            modes += ['short_hand']
    if p['cost']['sacrifice']:
        modes += ['no_own_land']
    if 'Flying' in p['token']['keywords']:
        modes += ['evasion', 'reach_blocker']
    if 'Deathtouch' in p['token']['keywords']:
        modes += ['deathtouch']
    return [mode + '_' + str(owner) for owner in (0, 1) for mode in modes]


def validate_witnesses(oracle, registry):
    for name, pin in pins()['witnesses'].items():
        if (registry.get(name, {}).get('csvid') != pin['csvid']
                or any(oracle.get(name, {}).get(k) != value for k, value in pin['oracle'].items())):
            raise ValueError('activated token witness changed: ' + name)
    for source in pins()['sources'].values():
        token_name = creation(source['oracle']['oracle_text'].splitlines()[source['ability']].split(': ', 1)[1])['name']
        if not oracle.get(token_name, {}).get('type_line', '').startswith('Token '):
            raise ValueError('registered token identity required; creator supplies characteristics')


def expand(name, csvid, declaration, p, selected, registry, oracle):
    from recipes import card, step
    validate_witnesses(oracle, registry)
    result = []
    token = p['token']; token_id = registry[token['name']]['csvid']; cost = p['cost']
    interface = declaration['params']['interface']
    for variant in selected:
        mode, owner_text = variant.rsplit('_', 1); owner = int(owner_text); other = 1 - owner
        # The manual AI multi-discard path may legally choose a held removal
        # instant. Keep that instant with the opponent instead, and actually
        # cast it off turn after creation. The AI's cost cards remain ordinary
        # disposable lands; its legal discard choice is never overridden.
        spell_caster = other if owner == 1 and (cost['discard'] > 1 or interface.get('manual_ai_discard', False)) else owner
        setup = []; entities = {}; actions = []; hands = [[], []]; graves = [[], []]; exiles = [[], []]; libs = [[], []]
        pools = [[30, 10, 10, 10, 10, 10] for _ in (0, 1)]; totals = [80, 80]; life = [20, 20]
        count = 0; token_tapped = token['tapped']; token_damage = 0
        loyalty = p['initial_loyalty']; casting = p['walker'] or mode == 'cast'
        bare = mode == 'bare_zones'

        def add(nm, ref, who, zone='battlefield', state=None):
            if nm != name and nm not in pins()['witnesses']:
                raise ValueError('unreviewed activation witness: ' + nm)
            item = card(nm, ref, who, zone)
            if zone == 'battlefield':
                item['state'] = list(state or ['untapped'])
            setup.append(item)
            record = oracle[nm]; creature = 'Creature' in record['type_line']
            entities[ref] = dict(name=nm, player=who, alive=zone == 'battlefield', creature=creature,
                                 tapped='tapped' in (state or []), damage=0,
                                 power=int(record['power']) if creature else 0,
                                 toughness=int(record['toughness']) if creature else 0)
            if zone == 'hand':
                hands[who].append(nm)

        def leave(ref, zone='graveyard'):
            entity = entities[ref]; entity['alive'] = False
            (graves if zone == 'graveyard' else hands)[entity['player']].append(entity['name'])

        def receipts(stack=0):
            seq = [step('assert_stack', value=stack), step('assert_choices_consumed')]
            for ref, entity in entities.items():
                # A destroyed source's freed slot may be reused by its tokens.
                # Prove identity/zone absence rather than querying that stale
                # handle after a real creation or other allocation.
                if not entity['alive']:
                    seq.append(step('assert_zone_multiplicity', player=entity['player'], zone='battlefield',
                                    index=registry[entity['name']]['csvid'],
                                    value=sum(e['alive'] and e['player'] == entity['player'] and e['name'] == entity['name'] for e in entities.values())))
                else:
                    seq.append(step('require_card', ref=ref, property='in_play', value=1))
                if entity['alive']:
                    seq.append(step('require_card', ref=ref, property='tapped', value=int(entity['tapped'])))
                    if entity['creature']:
                        seq.extend(step('require_card', ref=ref, property=prop, value=entity[prop]) for prop in ('power', 'toughness', 'damage'))
            if p['walker'] and entities['source']['alive']:
                seq.append(step('assert_counter', ref='source', counter_kind='loyalty', value=loyalty))
            if count:
                properties = dict(token=1, color=sum(1 << {'B':1, 'U':2, 'G':3, 'R':4, 'W':5}[c] for c in token['colors']),
                                  artifact=int(token['artifact']), power=token['power'], toughness=token['toughness'])
                properties.update({'subtype_' + subtype: 1 for subtype in token['subtypes']})
                seq.extend(step('assert_matching_permanents', player=owner, index=token_id, property=prop, value=value) for prop, value in properties.items())
                for i in range(count):
                    ref = 'expected_token_' + str(i)
                    seq += [step('bind_permanent_copy', player=owner, value=token_id, index=i, ref=ref),
                            step('require_card', ref=ref, property='tapped', value=int(i == 0 and token_tapped)),
                            step('require_card', ref=ref, property='damage', value=token_damage if i == 0 else 0)]
            universe = {e['name'] for e in entities.values()} | {token['name'], 'Plains', 'Island', 'Forest'}
            for who in (0, 1):
                seq += [step('assert_life', player=who, value=life[who]), step('assert_mana_total', player=who, value=totals[who]),
                        step('assert_zone', player=who, zone='battlefield', value=sum(e['alive'] and e['player'] == who for e in entities.values()) + (count if who == owner else 0)),
                        step('assert_zone_multiplicity', player=who, zone='battlefield', index=token_id, value=count if who == owner else 0)]
                for zone, names in (('hand', hands[who]), ('graveyard', graves[who]), ('exile', exiles[who]), ('library', libs[who])):
                    seq.append(step('assert_zone', player=who, zone=zone, value=len(names)))
                    if zone == 'library':
                        seq.extend(step('assert_zone_card', player=who, zone=zone, index=i, value=registry[nm]['csvid']) for i, nm in enumerate(names))
                    else:
                        counts = Counter(names)
                        seq.extend(step('assert_zone_multiplicity', player=who, zone=zone, index=registry[nm]['csvid'], value=counts[nm]) for nm in sorted(universe))
            return seq

        if not bare:
            for who in (0, 1):
                hand_count = 4 if who == owner else 1
                if who == owner and mode in ('empty_hand', 'short_hand'):
                    hand_count = 0 if mode == 'empty_hand' else max(0, cost['discard'] - 1)
                for i in range(hand_count):
                    setup.append(card('Plains', f'hand_{who}_{i}', who, 'hand'))
                    hands[who].append('Plains')
                for zone, nm in (('graveyard', 'Forest'), ('exile', 'Island'), ('library', 'Plains')):
                    setup.append(card(nm, zone + '_' + str(who), who, zone))
                    {'graveyard': graves, 'exile': exiles, 'library': libs}[zone][who].append(nm)
        state = ['tapped'] if mode == 'tapped' else ['untapped']
        add(name, 'source', owner, 'hand' if casting else 'battlefield', state)
        add('Hill Giant', 'control', owner)
        add('Grizzly Bears', 'enemy_control', other)
        if cost['sacrifice']:
            if mode != 'no_own_land':
                add('Forest', 'victim', owner, state=['tapped'])
            add('Forest', 'enemy_land', other, state=['tapped'])
        # Setup is installed in full before the script begins. Conditional
        # witnesses and held spells therefore belong to every earlier receipt,
        # even though combat/casting moves them only near the end of the case.
        combat = mode in ('fresh_attack', 'ready_attack', 'block_ground', 'block_flying', 'evasion', 'reach_blocker', 'deathtouch')
        defending = mode in ('block_ground', 'block_flying')
        witness = ('Suntail Hawk' if mode == 'block_flying' else 'Giant Spider' if mode in ('reach_blocker', 'deathtouch')
                   else 'Grizzly Bears' if defending or mode == 'evasion' else None)
        if combat and witness:
            add(witness, 'witness', other)
        if mode == 'bounce_token':
            add('Unsummon', 'bounce', spell_caster, 'hand')
        if mode == 'exile_token':
            add('Swords to Plowshares', 'exile', spell_caster, 'hand')
        actions += [step('turn', player=owner, value=0), step('discard_policy', value=1)]
        if interface['present_ai_dialogs']:
            actions.append(step('present_ai_dialogs', value=1))
        if casting:
            actions.extend(step('set_mana', player=owner, color=color, value=value) for color, value in enumerate(p['casting_mana']))
            actions += [step('cast', ref='source', value=1), step('assert_stack', value=1), step('resolve'), step('refresh'),
                        step('assert_stack', value=0), step('assert_choices_consumed'), step('assert_mana_total', player=owner, value=0)]
            hands[owner].remove(name); entities['source']['alive'] = True; entities['source']['tapped'] = False
            if p['walker']:
                actions.append(step('assert_counter', ref='source', counter_kind='loyalty', value=loyalty))
        if mode == 'payment' or variant in interface.get('exact_activation_budget_variants', []):
            pools[owner] = list(p['activation_mana'])
        if mode == 'insufficient_mana':
            pools[owner] = [0] * 6
        if mode == 'wrong_color':
            pools[owner] = list(p['activation_mana'])
            color = next(i for i in range(1, 6) if pools[owner][i])
            pools[owner][0] += pools[owner][color]; pools[owner][color] = 0
        totals = [sum(pool) for pool in pools]
        actions.extend(step('set_mana', player=who, color=color, value=value) for who in (0, 1) for color, value in enumerate(pools[who]))
        if mode == 'offturn':
            actions.append(step('turn', player=other, value=0))
        actions.append(step('refresh'))
        ready = len(actions)
        legal = (mode not in ('insufficient_mana', 'wrong_color', 'no_own_land')
                 and not (p['walker'] and mode == 'offturn')
                 and not (cost['tap'] and (mode == 'tapped' or casting and p['creature']))
                 and len(hands[owner]) >= cost['discard'])

        def activate(victim='victim'):
            nonlocal count, loyalty
            seq = [step('require_can_activate', ref='source', value=1)]
            if str(owner) in interface['dialogs'] and variant not in interface.get('silent_variants', []):
                seq.append(step('choose_dialog', player=owner, value=interface['dialogs'][str(owner)]))
            if cost['sacrifice'] and interface['sacrifice_choices'].get(str(owner), False):
                seq.append(step('choose_target', player=owner, ref=victim))
            seq += [step('activate', ref='source', value=1), step('assert_stack', value=1), step('assert_choices_consumed')]
            totals[owner] -= sum(p['activation_mana'])
            if cost['tap']:
                entities['source']['tapped'] = True
            if cost['sacrifice']:
                leave(victim)
            if cost['discard']:
                discarded = hands[owner][:cost['discard']]
                graves[owner].extend(discarded); del hands[owner][:cost['discard']]
            if p['walker']:
                loyalty += cost['loyalty']
            seq += receipts(1)
            return seq

        pending = None
        if not legal:
            actions += [step('require_can_activate', ref='source', value=0), step('activate', ref='source', value=0)]
        else:
            actions += activate(); pending = len(actions)
            if mode == 'source_destroyed_pending':
                actions += [step('destroy', ref='source'), step('refresh')]; leave('source')
            actions += [step('resolve'), step('refresh'), step('assert_choices_consumed')]
            count += token['count']; life[owner] += token['gain_life']
            if mode == 'repeat':
                if cost['tap'] or p['walker'] or cost['sacrifice']:
                    actions += [step('require_can_activate', ref='source', value=0), step('activate', ref='source', value=0)]
                else:
                    actions += activate('spare_land')
                    actions += [step('resolve'), step('refresh'), step('assert_choices_consumed')]
                    count += token['count']; life[owner] += token['gain_life']
            if mode == 'cleanup':
                actions += [step('cleanup'), step('refresh')]

        def spell(nm, ref):
            hands[spell_caster].remove(nm); graves[spell_caster].append(nm)
            totals[spell_caster] -= sum(mana(oracle[nm]['mana_cost']))
            return [step('choose_target', player=spell_caster, ref='token'), step('cast', ref=ref, value=1),
                    step('resolve'), step('refresh'), step('assert_choices_consumed')]

        if legal and (combat or mode in ('destroy_token', 'bounce_token', 'exile_token')):
            actions.append(step('bind_permanent_copy', player=owner, value=token_id, index=0, ref='token'))
        if legal and mode == 'destroy_token':
            actions += [step('destroy', ref='token'), step('refresh')]; count -= 1
        if legal and mode == 'bounce_token':
            actions += spell('Unsummon', 'bounce'); count -= 1
        if legal and mode == 'exile_token':
            actions += spell('Swords to Plowshares', 'exile'); count -= 1; life[owner] += token['power']
        if legal and combat:
            keys = set(token['keywords'])
            # The fixture's real turn gate readies tokens. Its phase helpers
            # deliberately preserve explicit mana budgets and omit untapping.
            if not defending and mode != 'fresh_attack':
                actions += [step('advance_turn'), step('advance_turn')]
            attacker = 'witness' if defending else 'token'; blocker = 'token' if defending else 'witness'
            can_attack = defending or mode != 'fresh_attack' or 'Haste' in keys
            actions += [step('turn', player=other if defending else owner, value=0), step('begin_attackers'),
                        step('attack', ref=attacker, value=int(can_attack)), step('finish_attackers'), step('begin_blockers')]
            blocks = bool(witness and can_attack and (defending and (mode != 'block_flying' or 'Flying' in keys)
                          or not defending and ('Flying' not in keys or mode == 'reach_blocker')))
            if witness and can_attack:
                actions.append(step('block', ref=blocker, other=attacker, value=int(blocks)))
            actions += [step('advance_combat'), step('combat_damage'), step('resolve'), step('assert_choices_consumed'), step('end_combat')]
            if defending:
                entities['witness']['tapped'] = True
            elif can_attack:
                token_tapped = 'Vigilance' not in keys
            if blocks:
                entity = entities['witness']; deadly = 'Deathtouch' in keys and token['power'] > 0
                if deadly or token['power'] >= entity['toughness']:
                    leave('witness')
                else:
                    entity['damage'] = token['power']
                if entity['power'] >= token['toughness']:
                    count -= 1; token_tapped = False
                else:
                    token_damage = entity['power']
            elif can_attack:
                life[owner if defending else other] -= entities['witness']['power'] if defending else token['power']
        checks = receipts(); actions += checks
        if mode == 'payment' and legal:
            actions.extend(step('assert_mana', player=owner, color=color, value=0) for color in range(6))
        if mode in ('ready_snapshot', 'pending_snapshot') and legal:
            cut = ready if mode == 'ready_snapshot' else pending
            prefix = actions[:cut]; tail = actions[cut:]
            actions = prefix + [step('capture')] + copy.deepcopy(tail) + [step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(tail) + [step('assert_recorded_state')]
        if mode == 'resolved_snapshot' and legal:
            actions += [step('capture'), step('record_state'), step('set_life', player=other, value=7), step('ai_roundtrip'), step('restore')] + copy.deepcopy(checks) + [step('assert_recorded_state')]
        result.append(dict(name=f'ActivatedTokenCreationRecipe_{csvid}_{p["ability"]}_{variant}', card=name, fixture='empty_main', seed=123,
                           setup=setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                           params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=p['gaps'] +
                           ['replacement effects and token doublers', 'control changes', 'multiple-blocker combat', 'separate activated sibling effects'])))
    return result
