"""Oracle-validated entry exchanges: sacrifice, filtered discard and own bounce.

The mechanism grammar is independent of handler names. Private records pin the
complete source and witnesses; effect receipts follow actual zone ownership.
"""
import copy
import json
import re
from pathlib import Path

FAMILY = 'entry_trigger_value'
DATA = Path(__file__).parent / 'data'
NUMBERS = {'a': 1, 'an': 1, 'another': 1, 'two': 2, 'three': 3}
COLORS = {'white': 'W', 'blue': 'U', 'black': 'B', 'red': 'R', 'green': 'G'}


def load_profiles():
    return json.loads((DATA / 'entry_trigger_value_wave8_profiles.json').read_text(encoding='utf8'))


def load_witnesses():
    return json.loads((DATA / 'entry_trigger_value_wave8_witnesses.json').read_text(encoding='utf8'))['cards']


def mechanism(record):
    """Parse a complete entry clause, retaining all other printed components."""
    if (not record or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'Creature' not in record.get('type_line', '')
            or any(not str(record.get(k, '')).isdigit() for k in ('power', 'toughness'))
            or int(record['toughness']) == 0):
        raise ValueError('entry exchange requires an ordinary fixed-stat creature')
    if re.search(r'\{[^}]*[/X][^}]*\}', record.get('mana_cost', '')):
        raise ValueError('entry exchange ordinary mana cost required')
    prefix = r'When (?:this creature|' + re.escape(record['name']) + r') enters, '
    clauses = [line for line in record['oracle_text'].splitlines() if re.match(prefix, line)]
    if len(clauses) != 1:
        raise ValueError('entry exchange requires one complete ordinary entry clause')
    body = re.sub(prefix, '', clauses[0], count=1)
    p = dict(count=1, scope='creature', colors=[], another=False, recipient='self', targeted=False)
    matched = re.fullmatch(r'sacrifice (a|another) (creature|land|artifact|permanent)\.', body)
    if matched:
        p.update(kind='sacrifice', scope=matched[2], another=matched[1] == 'another')
    elif body == 'each player sacrifices a creature of their choice.':
        p.update(kind='sacrifice', recipient='both')
    elif body == "sacrifice a permanent that's one or more colors.":
        p.update(kind='sacrifice', scope='colored_permanent')
    else:
        matched = re.fullmatch(r'sacrifice it unless you discard (a creature card|a land card|a noncreature card)\.', body)
        if matched:
            p.update(kind='discard_gate', scope={'a creature card': 'creature', 'a land card': 'land', 'a noncreature card': 'noncreature'}[matched[1]])
        else:
            gated = body.startswith('sacrifice it unless you return ')
            bounce = body[len('sacrifice it unless you '):] if gated else body
            matched = re.fullmatch(r"return (a|an|another|two|three) (?:(target) )?((?:(?:white|blue|black|red|green)(?: or (?:white|blue|black|red|green))? )?)(creatures?|permanents?|nonland permanent|artifacts?|lands?|Forests) you control to (?:its|their) owner(?:'s|s') hand\.", bounce)
            if not matched:
                raise ValueError('unsupported entry exchange wording')
            noun = matched[4]
            p.update(kind='bounce_gate' if gated else 'bounce', count=NUMBERS[matched[1]],
                     scope='Forest' if noun == 'Forests' else noun.removesuffix('s').replace(' ', '_'),
                     another=matched[1] == 'another', targeted=bool(matched[2]),
                     colors=[COLORS[word] for word in matched[3].strip().split(' or ') if word])
    p['clause'] = clauses[0]
    p['gaps'] = [line for line in record['oracle_text'].splitlines() if line != clauses[0]]
    return p


def spec(d, record):
    if isinstance(d.get('params'), dict) and d['params'].get('schema') == 4:
        from entry_trigger_value_gate_recipes import spec as gate_spec
        return gate_spec(d, record)
    if isinstance(d.get('params'), dict) and d['params'].get('schema') == 3:
        from entry_trigger_value_selection_recipes import spec as selection_spec
        return selection_spec(d, record)
    if isinstance(d.get('params'), dict) and d['params'].get('schema') == 2:
        from entry_trigger_value_handflow_recipes import spec as handflow_spec
        return handflow_spec(d, record)
    from recipes import cost_from_oracle
    if (d.get('family') != FAMILY or type(d.get('version')) is not int or d['version'] != 2
            or not isinstance(d.get('variants'), list) or not d['variants']
            or len(d['variants']) != len(set(d['variants']))):
        raise ValueError('entry exchange requires version two and explicit unique variants')
    params = d.get('params')
    if not isinstance(params, dict) or set(params) not in ({'schema', 'mechanism'}, {'schema', 'mechanism', 'presentation'}) or type(params['schema']) is not int or params['schema'] != 1:
        raise ValueError('entry exchange requires its explicit mechanism schema')
    pin = load_profiles()['sources'].get(record.get('name')) if record else None
    parsed = mechanism(record)
    presentation = params.get('presentation', {})
    scripted = presentation.get('scripted_sacrifice') if isinstance(presentation, dict) else None
    if (not isinstance(presentation, dict) or set(presentation) - {'scripted_sacrifice'}
            or presentation and (parsed['kind'] != 'sacrifice'
                or not isinstance(scripted, dict) or set(scripted) != {'0', '1'}
                or any(type(v) is not bool for v in scripted.values()))):
        raise ValueError('entry exchange requires separately reviewed input presentation')
    if (not pin or pin['oracle'] != record or d.get('oracle_text') != record['oracle_text']
            or params['mechanism'] != parsed or pin['mechanism'] != parsed
            or presentation != pin.get('presentation', {})):
        raise ValueError('entry exchange whole Oracle and mechanism parameters must match')
    return dict(parsed, family=FAMILY, csvid=pin['csvid'], mana=cost_from_oracle(record), presentation=copy.deepcopy(presentation),
                power=int(record['power']), toughness=int(record['toughness']), source_name=record['name'])


def variants(p):
    if p.get('_entry_gate'):
        from entry_trigger_value_gate_recipes import variants as gate_variants
        return gate_variants(p)
    if p.get('selection'):
        from entry_trigger_value_selection_recipes import variants as selection_variants
        return selection_variants(p)
    if p.get('handflow'):
        from entry_trigger_value_handflow_recipes import variants as handflow_variants
        return handflow_variants(p)
    modes = ['effect', 'payment', 'insufficient_mana', 'bare_zones', 'cleanup', 'countered',
             'ready_snapshot', 'pending_snapshot', 'resolved_snapshot', 'nonmatching_entry']
    if any(p['mana'][1:]): modes.append('wrong_color')
    if p['kind'] in ('discard_gate', 'bounce_gate'):
        modes += ['no_match', 'opponent_only']
    if p['kind'] == 'discard_gate':
        modes += ['decline', 'empty_hand', 'exact_hand', 'alternate_choice', 'blink', 'bounce_recast']
    if p['kind'] in ('bounce', 'bounce_gate'):
        modes += ['alternate_choice', 'foreign_owned', 'blink', 'bounce_recast']
        if not p['another'] and p['scope'] in ('creature', 'permanent', 'nonland_permanent') and not p['colors']:
            modes.append('source_choice')
        if p['scope'] == 'permanent': modes += ['artifact_choice', 'land_choice']
        if p['kind'] == 'bounce' and p['another']: modes.append('no_other')
    if p['kind'] == 'sacrifice':
        modes += ['empty_eligible', 'opponent_only']
    if p['count'] > 1:
        modes.append('one_match')
        if p['kind'] == 'bounce' and p['scope'] == 'creature' and not p['another'] and not p['colors']:
            modes.append('source_only')
    if len(p['colors']) > 1: modes += ['other_color', 'wrong_color_only']
    result = [mode + '_' + str(owner) for owner in (0, 1) for mode in modes]
    if p['kind'] == 'sacrifice':
        result += ['alternate_choice_0', 'foreign_owned_0']
        if p['scope'] == 'creature' and not p['another']: result += ['source_choice_0', 'shroud_choice_0', 'indestructible_choice_0']
    return result


def validate_witnesses(oracle, rows):
    for name, record in load_witnesses().items():
        if oracle.get(name) != record or name not in rows:
            raise ValueError('entry exchange witness pin changed: ' + name)
    for name, profile in load_profiles()['sources'].items():
        if (oracle.get(name) != profile['oracle'] or name not in rows
                or rows[name]['csvid'] != profile['csvid'] or rows[name]['kind'] != 'card'
                or rows[name]['handler'] in ('card_dummy', 'NULL', None)):
            raise ValueError('entry exchange primary identity/implementation changed: ' + name)


def eligible(record, p):
    types = set(record['type_line'].partition(' — ')[0].split())
    scope = p['scope']
    if scope == 'permanent': ok = bool(types & {'Creature', 'Artifact', 'Land', 'Enchantment', 'Planeswalker'})
    elif scope == 'nonland_permanent': ok = 'Land' not in types
    elif scope == 'noncreature': ok = 'Creature' not in types
    elif scope == 'colored_permanent': ok = bool(record.get('colors'))
    elif scope == 'Forest': ok = 'Land' in types and 'Forest' in record['type_line'].partition(' — ')[2].split()
    else: ok = scope.capitalize() in types
    return ok and (not p['colors'] or bool(set(p['colors']) & set(record.get('colors', []))))


def expand(name, csvid, declaration, p, selected, rows, oracle):
    if p.get('_entry_gate'):
        from entry_trigger_value_gate_recipes import expand as gate_expand
        return gate_expand(name, csvid, declaration, p, selected, rows, oracle)
    if p.get('selection'):
        from entry_trigger_value_selection_recipes import expand as selection_expand
        return selection_expand(name, csvid, declaration, p, selected, rows, oracle)
    if p.get('handflow'):
        from entry_trigger_value_handflow_recipes import expand as handflow_expand
        return handflow_expand(name, csvid, declaration, p, selected, rows, oracle)
    if csvid != p['csvid'] or not selected or len(selected) != len(set(selected)) or set(selected) - set(variants(p)):
        raise ValueError('entry exchange requires matching identity and distinct supported variants')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, p, variant, rows, oracle) for variant in selected]


def _case(name, csvid, declaration, p, variant, rows, oracle):
    from recipes import step, cost_from_oracle
    mode, owner = variant.rsplit('_', 1); owner = int(owner)
    setup = []; entities = {}; zones = {z: [[], []] for z in ('hand', 'graveyard', 'exile', 'library')}
    mana = [[20] * 6, [20] * 6]

    def add(card_name, ref, controller, zone='battlefield', foreign=False):
        item = dict(name=card_name, ref=ref, player=controller, zone=zone)
        if zone == 'battlefield': item['state'] = ['tapped'] + (['opponent_owned'] if foreign else [])
        setup.append(item)
        entities[ref] = dict(name=card_name, player=controller, owner=1-controller if foreign else controller,
                             alive=zone == 'battlefield', tapped=zone == 'battlefield')
        if zone != 'battlefield': zones[zone][controller].append(card_name)

    def leave(ref, destination):
        entity = entities[ref]; entity['alive'] = False
        zones[destination][entity['owner']].append(entity['name'])

    def pay(card_name, player):
        mana[player] = [a-b for a, b in zip(mana[player], cost_from_oracle(oracle[card_name]))]

    def preferred():
        if p['scope'] in ('land', 'Forest'): return 'Forest'
        if p['scope'] == 'artifact': return 'Darksteel Relic'
        if p['scope'] == 'noncreature': return 'Giant Growth'
        for candidate in ('Grizzly Bears', 'Goblin Piker', 'Merfolk of the Pearl Trident', 'Savannah Lions', 'Scathe Zombies'):
            if eligible(oracle[candidate], p): return candidate
        raise ValueError('missing independently pinned entry predicate witness')

    if mode != 'bare_zones':
        for player in (0, 1):
            for zone, cards in [('hand', ['Plains', 'Giant Growth']), ('graveyard', ['Mountain']),
                                ('exile', ['Island']), ('library', ['Forest', 'Plains', 'Grizzly Bears', 'Island'])]:
                for i, card_name in enumerate(cards): add(card_name, f'{zone}_{player}_{i}', player, zone)
    if p['kind'] == 'discard_gate':
        if mode in ('empty_hand', 'exact_hand', 'no_match', 'opponent_only'):
            setup[:] = [item for item in setup if not (item['player'] == owner and item['zone'] == 'hand')]
            zones['hand'][owner] = []
        if mode in ('no_match', 'opponent_only'):
            wrong = 'Grizzly Bears' if p['scope'] == 'noncreature' else 'Giant Growth'
            add(wrong, 'wrong_hand', owner, 'hand')
        if mode not in ('empty_hand', 'no_match', 'opponent_only'):
            add(preferred(), 'discard_witness', owner, 'hand')
            if mode == 'alternate_choice': add(preferred(), 'discard_second', owner, 'hand')
        if mode == 'opponent_only': add(preferred(), 'enemy_discard_control', 1-owner, 'hand')

    control_name = 'Forest' if p['scope'] in ('artifact', 'nonland_permanent') else 'Darksteel Relic'
    if mode != 'no_other' or not eligible(oracle[control_name], p): add(control_name, 'own_control', owner)
    add('Darksteel Relic', 'enemy_control', 1-owner)
    if not (p['kind'] == 'sacrifice' and p['recipient'] == 'both' and mode == 'empty_eligible'):
        add('Grizzly Bears', 'enemy_creature', 1-owner)
    if mode == 'opponent_only' and p['scope'] in ('land', 'Forest', 'artifact'):
        add(preferred(), 'enemy_eligible', 1-owner)
    absent = mode in ('empty_eligible', 'no_match', 'opponent_only', 'source_only', 'wrong_color_only', 'no_other')
    if mode == 'wrong_color_only':
        wrong = next(n for n in ('Grizzly Bears', 'Goblin Piker', 'Merfolk of the Pearl Trident', 'Savannah Lions', 'Scathe Zombies') if not eligible(oracle[n], p))
        add(wrong, 'wrong_color_creature', owner)
    if p['kind'] in ('sacrifice', 'bounce', 'bounce_gate') and not absent:
        # The AI sacrifice helper makes its own legal choice. For ordinary
        # creature sacrifices, its only eligible own creature is the source;
        # human cases also exercise explicitly selected alternative creatures.
        ai_source_only = p['kind'] == 'sacrifice' and p['scope'] == 'creature' and owner == 1 and not p['another']
        if not ai_source_only:
            for i in range(1 if mode == 'one_match' else p['count']):
                witness = preferred()
                if mode == 'other_color':
                    mapping = {'G': 'Grizzly Bears', 'R': 'Goblin Piker', 'U': 'Merfolk of the Pearl Trident', 'W': 'Savannah Lions', 'B': 'Scathe Zombies'}
                    witness = mapping[next(c for c in p['colors'] if c not in oracle[witness]['colors'])]
                if mode == 'artifact_choice': witness = 'Darksteel Relic'
                if mode == 'land_choice': witness = 'Forest'
                if mode == 'shroud_choice': witness = 'Deadly Insect'
                if mode == 'indestructible_choice': witness = 'Darksteel Myr'
                add(witness, f'eligible_{i}', owner, foreign=mode == 'foreign_owned')
            if mode == 'alternate_choice': add(preferred(), 'alternative', owner)
    if (mode == 'blink' and p['kind'] == 'bounce' and not p['another']
            and eligible(oracle[name], p) and not eligible(oracle[control_name], p)):
        # Keep an independently prepared matching group for the second entry.
        # Cloudshift replaces the source slot during resolution; a queued
        # choice must not refer to the now-obsolete pre-blink source slot.
        for i in range(p['count']): add(preferred(), f'reentry_eligible_{i}', owner)

    add(name, 'source', owner, 'hand')
    if mode == 'nonmatching_entry':
        add('Hill Giant', 'entry_control', owner, 'hand')
    if mode == 'countered': add('Essence Scatter', 'counter', 0, 'hand')
    if mode == 'blink': add('Cloudshift', 'blink_spell', owner, 'hand')
    if mode == 'bounce_recast': add('Unsummon', 'bounce_spell', owner, 'hand')
    if mode == 'payment': mana[owner] = list(p['mana'])
    if mode == 'insufficient_mana': mana[owner] = [0] * 6
    if mode == 'wrong_color':
        mana[owner] = list(p['mana']); color = next(i for i in range(1, 6) if mana[owner][i])
        mana[owner][1 if color != 1 else 2] += mana[owner][color]; mana[owner][color] = 0
    pre = [step('turn', player=owner, value=0)]
    pre += [step('set_mana', player=player, color=color, value=amount) for player in (0, 1) for color, amount in enumerate(mana[player])]
    pre.append(step('refresh'))
    actions = []; pending_cut = None; unoffered_target_choices = 0

    def inspect():
        result = []
        for ref, entity in entities.items():
            if not entity['alive']: continue
            result += [step('assert_card', ref=ref, property='in_play', value=1),
                       step('assert_card', ref=ref, property='tapped', value=int(entity['tapped']))]
            record = oracle[entity['name']]
            if 'Creature' in record['type_line']:
                result += [step('assert_card', ref=ref, property=k, value=int(record[k])) for k in ('power', 'toughness')]
        return result

    def receipts():
        result = [step('assert_stack', value=0), step('assert_choices_consumed')]
        universe = sorted({e['name'] for e in entities.values()})
        for player in (0, 1):
            result += [step('assert_life', player=player, value=20),
                       step('assert_zone', player=player, zone='battlefield', value=sum(e['alive'] and e['player'] == player for e in entities.values()))]
            if mode != 'cleanup': result += [step('assert_mana', player=player, color=i, value=n) for i, n in enumerate(mana[player])]
            for zone, players in zones.items():
                result.append(step('assert_zone', player=player, zone=zone, value=len(players[player])))
                result += [step('assert_zone_multiplicity', player=player, zone=zone, index=rows[n]['csvid'], value=players[player].count(n)) for n in universe]
                if zone == 'library':
                    result += [step('assert_zone_card', player=player, zone=zone, index=i, value=rows[n]['csvid']) for i, n in enumerate(players[player])]
        return result + inspect()

    def effect():
        nonlocal unoffered_target_choices
        unoffered_target_choices = 0
        choices = []; selected = []
        if p['kind'] == 'discard_gate':
            legal = [n for n in zones['hand'][owner] if eligible(oracle[n], p)]
            if mode == 'decline' or not legal:
                if zones['hand'][owner]: choices.append(step('choose_optional_zone', player=owner, value=rows[zones['hand'][owner][0]]['csvid']))
                leave('source', 'graveyard')
            else:
                picked = legal[-1] if mode == 'alternate_choice' else legal[0]
                choices.append(step('choose_zone_card', player=owner, value=rows[picked]['csvid']))
                zones['hand'][owner].remove(picked); zones['graveyard'][owner].append(picked)
            return choices
        players = (owner, 1-owner) if p['recipient'] == 'both' else (owner,)
        for player in players:
            legal = [ref for ref, entity in entities.items() if entity['alive'] and entity['player'] == player
                     and eligible(oracle[entity['name']], p) and not (p['another'] and ref == 'source')]
            if mode == 'source_choice' and player == owner: legal.sort(key=lambda ref: ref != 'source')
            elif mode == 'alternate_choice' and player == owner: legal.sort(key=lambda ref: ref != 'alternative')
            else: legal.sort(key=lambda ref: (not ref.startswith('eligible_'), ref == 'source'))
            insufficient = len(legal) < p['count']
            picked = [] if insufficient and (p['kind'] == 'bounce_gate' or p['targeted']) else legal[:p['count']]
            if insufficient and p['targeted'] and legal:
                # Observe an engine that wrongly offers a partial mandatory
                # target set. A correct engine leaves these legal fallback
                # inputs unused; the exact unused count clears that probe.
                choices += [step('choose_target', player=player, ref=ref) for ref in legal]
                unoffered_target_choices += len(legal)
            scripted_sacrifice = p.get('presentation', {}).get('scripted_sacrifice', {}).get(str(player), player == 0)
            if p['kind'] == 'sacrifice' and not scripted_sacrifice and len(legal) > p['count']:
                raise ValueError('AI sacrifice fixture must have one forced legal selection')
            for ref in picked:
                if p['kind'] != 'sacrifice' or scripted_sacrifice: choices.append(step('choose_target', player=player, ref=ref))
                selected.append(ref)
        for ref in selected: leave(ref, 'graveyard' if p['kind'] == 'sacrifice' else 'hand')
        if p['kind'] == 'bounce_gate' and len(selected) < p['count']: leave('source', 'graveyard')
        return choices

    def cast_source():
        nonlocal pending_cut
        pay(name, owner)
        result = [step('cast', ref='source', value=1), step('assert_stack', value=1),
                  step('assert_card', ref='source', property='in_hand', value=0),
                  step('assert_card', ref='source', property='in_play', value=0),
                  step('assert_zone', player=owner, zone='hand', value=len(zones['hand'][owner])),
                  step('assert_mana_total', player=owner, value=sum(mana[owner]))]
        zones['hand'][owner].remove(name); entities['source']['alive'] = True; entities['source']['tapped'] = False
        pending_cut = len(actions) + len(result)
        result += effect() + [step('resolve')]
        if unoffered_target_choices: result += [step('assert_unused_choices', value=unoffered_target_choices)]
        result += [step('assert_choices_consumed'), step('refresh')]
        if p['kind'] == 'discard_gate': result += [step('assert_forced_zone_choices', value=0)]
        return result

    if mode in ('insufficient_mana', 'wrong_color'):
        actions.append(step('cast', ref='source', value=0))
    elif mode == 'countered':
        pay(name, owner); pay('Essence Scatter', 0)
        zones['hand'][owner].remove(name); zones['graveyard'][owner].append(name)
        zones['hand'][0].remove('Essence Scatter'); zones['graveyard'][0].append('Essence Scatter')
        actions += [step('choose_priority', ref='counter'), step('cast', ref='source', value=0), step('resolve'), step('assert_choices_consumed')]
    elif mode == 'nonmatching_entry':
        # A fixture-seeded entry source has not completed its entry lifecycle.
        # Establish the source through a real cast and assert its first effect
        # before the unrelated creature arrives. The later cast must not run
        # the source's entry exchange a second time.
        actions += cast_source() + receipts()
        pay('Hill Giant', owner); zones['hand'][owner].remove('Hill Giant'); entities['entry_control']['alive'] = True; entities['entry_control']['tapped'] = False
        actions += [step('cast', ref='entry_control', value=1), step('assert_stack', value=1),
                    step('assert_card', ref='entry_control', property='in_hand', value=0),
                    step('assert_mana_total', player=owner, value=sum(mana[owner])),
                    step('resolve'), step('assert_choices_consumed'), step('refresh')]
    else:
        actions += cast_source()
        if mode in ('blink', 'bounce_recast'):
            if not entities['source']['alive']: raise ValueError('entry lifecycle fixture must retain its source')
            actions += receipts()
            spell = 'Cloudshift' if mode == 'blink' else 'Unsummon'
            ref = 'blink_spell' if mode == 'blink' else 'bounce_spell'
            pay(spell, owner); zones['hand'][owner].remove(spell); zones['graveyard'][owner].append(spell)
            actions += [step('choose_target', player=owner, ref='source'), step('cast', ref=ref, value=1), step('assert_stack', value=1)]
            if mode == 'blink':
                entities['source']['tapped'] = False
                actions += effect() + [step('resolve'), step('assert_choices_consumed'), step('refresh')]
                if entities['source']['alive']:
                    actions.append(step('bind_permanent', ref='source', player=owner, value=csvid))
            else:
                leave('source', 'hand')
                actions += [step('resolve'), step('assert_choices_consumed'), step('refresh'),
                            step('bind_hand', ref='source', player=owner, value=csvid)] + receipts()
                actions += cast_source()
        if mode == 'cleanup': actions += receipts() + [step('cleanup'), step('refresh')]
    checks = receipts()
    if mode in ('ready_snapshot', 'pending_snapshot', 'resolved_snapshot'):
        cut = 0 if mode == 'ready_snapshot' else pending_cut if mode == 'pending_snapshot' else len(actions)
        tail = actions[cut:] + checks
        steps = pre + actions[:cut] + [step('capture')] + copy.deepcopy(tail) + [step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(tail) + [step('assert_recorded_state')]
    else: steps = pre + actions + checks
    return dict(name=f'EntryTriggerValueRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                setup=setup, steps=steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'], oracle_text=declaration['oracle_text'],
                             untested_components=p['gaps'] + ['multiplayer', 'trigger-countering and response windows after entry'],
                             entry_clause=p['clause']))
