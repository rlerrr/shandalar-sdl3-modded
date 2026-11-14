"""Oracle-derived death predicates and fixed draw/life receipts.

The model observes real cast, removal, combat and sacrifice paths. Presentation
annotations describe actual menus only; they never determine a rules receipt.
"""
from collections import Counter
import copy
import json
from pathlib import Path
import re

from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

FAMILY = 'death_value_trigger'
DATA_PATH = Path(__file__).parent / 'data/death_value_trigger_wave8_witnesses.json'
PASSIVE = {'Flying', 'Reach', 'Trample', 'Deathtouch', 'Lifelink', 'Defender',
           'Vigilance', 'First strike', 'Double strike', 'Haste', 'Swampwalk',
           'Forestwalk', 'Islandwalk', 'Mountainwalk', 'Plainswalk', 'Shroud',
           'Hexproof', 'Indestructible'}
REMINDERS = {
    '(Any amount of damage this deals to a creature is enough to destroy it.)',
    '(This creature can block creatures with flying.)',
    "(This creature can't be blocked as long as defending player controls a Swamp.)",
    '(This creature can deal excess combat damage to the player or planeswalker it\'s attacking.)',
}


def _pins():
    data = json.loads(DATA_PATH.read_text(encoding='utf8'))
    if data.get('version') != 1:
        raise ValueError('unsupported death value trigger witnesses')
    return data


def _same(first, second):
    if type(first) is not type(second):
        return False
    if isinstance(first, dict):
        return first.keys() == second.keys() and all(_same(first[k], second[k]) for k in first)
    if isinstance(first, list):
        return len(first) == len(second) and all(_same(a, b) for a, b in zip(first, second))
    return first == second


def oracle_model(record):
    """Parse whole abilities, independently of source names and handlers."""
    if (not isinstance(record, dict) or record.get('layout') != 'normal'
            or record.get('card_faces') or record.get('selected_face')):
        raise ValueError('a normal single-face death trigger source is required')
    kinds = set(record.get('type_line', '').partition(' — ')[0].split())
    if not kinds & {'Creature', 'Artifact', 'Enchantment'} or 'Aura' in record.get('type_line', ''):
        raise ValueError('a standalone creature, artifact or enchantment is required')
    trigger = None
    keywords = []
    for line in record.get('oracle_text', '').splitlines():
        if line.startswith(('When ', 'Whenever ')) and (', ' in line) and (
                ' dies, ' in line or ' is put into a graveyard from the battlefield, ' in line):
            if trigger is not None:
                raise ValueError('multiple death abilities need a separate model')
            trigger = line
            continue
        passive = line
        for reminder in re.findall(r'\([^\n]*?\)', line):
            if reminder not in REMINDERS:
                raise ValueError('unreviewed additional death-source reminder')
            passive = passive.replace(reminder, '')
        words = [word.strip() for word in passive.strip().split(',')]
        if not words or any(word not in PASSIVE for word in words):
            raise ValueError('additional source ability needs another mechanism model')
        keywords.extend(words)
    if trigger is None or sorted(keywords) != sorted(record.get('keywords', [])):
        raise ValueError('complete death source text and keywords must agree')
    head, effect = trigger.split(', ', 1)
    head = re.sub(r'^Whenever |^When ', '', head)
    for name in sorted({record['name'], record['name'].split(',')[0]}, key=len, reverse=True):
        head = head.replace(name, 'this creature')
    subject = re.sub(r' dies$| is put into a graveyard from the battlefield$', '', head)
    predicate = dict(kind='Creature', self_only=False, exclude_source=False,
                     controller='any', owner='any', nontoken=False, subtype=None,
                     threshold=None, self_included=False)
    if subject in ('this creature', 'this artifact', 'this enchantment'):
        predicate.update(kind=subject.split()[1].capitalize(), self_only=True)
    else:
        if subject.startswith('this creature or another '):
            predicate['self_included'] = True
            subject = 'a ' + subject[len('this creature or another '):]
        elif subject.startswith('another '):
            predicate['exclude_source'] = True
            subject = 'a ' + subject[len('another '):]
        subject = re.sub(r'^an ', 'a ', subject)
        if ' owned by another player' in subject:
            predicate['owner'] = 'other'
            subject = subject.replace(' owned by another player', '')
        if ' an opponent controls' in subject:
            predicate['controller'] = 'opponent'
            subject = subject.replace(' an opponent controls', '')
        if ' you control' in subject:
            predicate['controller'] = 'you'
            subject = subject.replace(' you control', '')
        threshold = re.search(r' with (power|toughness) (\d+) or greater$', subject)
        if threshold:
            predicate['threshold'] = dict(property=threshold[1], minimum=int(threshold[2]))
            subject = subject[:threshold.start()]
        if subject.startswith('a nontoken '):
            predicate['nontoken'] = True
            subject = 'a ' + subject[len('a nontoken '):]
        noun = subject.removeprefix('a ')
        if noun in ('creature', 'artifact', 'enchantment', 'permanent'):
            predicate['kind'] = noun.capitalize()
        elif noun in ('Zombie', 'Vampire', 'Goblin', 'Spirit', 'Human', 'Aetherborn'):
            predicate.update(kind='Creature', subtype=noun)
        else:
            raise ValueError('unreviewed death predicate: ' + subject)
    receipt = dict(draw=0, gain=0, loss=0, recipient='you', optional=False)
    body = effect.removesuffix('.')
    if body.startswith('you may have '):
        receipt['optional'] = True
        body = body[len('you may have '):]
    elif body.startswith('you may '):
        receipt['optional'] = True
        body = 'you ' + body[len('you may '):]
    if body == "that creature's controller may draw a card":
        receipt.update(draw=1, recipient='dead_controller', optional=True)
    elif match := re.fullmatch(r'(?:you )?draw (a card|two cards|three cards)(?: and you lose (\d+) life)?', body):
        receipt.update(draw={'a card': 1, 'two cards': 2, 'three cards': 3}[match[1]], loss=int(match[2] or 0))
    elif match := re.fullmatch(r'you gain (\d+) life', body):
        receipt['gain'] = int(match[1])
    elif match := re.fullmatch(r'(target player|target opponent|each opponent) loses? (\d+) life(?: and you gain (\d+) life)?', body):
        receipt.update(recipient={'target player': 'target_player', 'target opponent': 'target_opponent',
                                  'each opponent': 'opponents'}[match[1]], loss=int(match[2]), gain=int(match[3] or 0))
    else:
        raise ValueError('unreviewed death receipt: ' + effect)
    if not any(receipt[k] for k in ('draw', 'gain', 'loss')) or any(not 0 <= receipt[k] <= 8 for k in ('draw', 'gain', 'loss')):
        raise ValueError('bounded nonzero fixed death receipt required')
    if 'Creature' in kinds:
        if any(not re.fullmatch(r'\d+', record.get(k, '')) for k in ('power', 'toughness')) or int(record['toughness']) == 0:
            raise ValueError('fixed numeric creature characteristics required')
    return dict(predicate=predicate, receipt=receipt, keywords=sorted(keywords))


def spec(declaration, record):
    from recipes import cost_from_oracle
    if not isinstance(record, dict) or declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text'):
        raise ValueError('complete pinned Oracle death text and version 2 required')
    model = oracle_model(record)
    params = declaration.get('params')
    if not isinstance(params, dict) or set(params) != {'schema', 'predicate', 'receipt', 'keywords', 'presentation'} or type(params['schema']) is not int or params['schema'] != 8:
        raise ValueError('explicit schema 8 predicate, receipt and presentation required')
    if any(not _same(params[k], model[k]) for k in model):
        raise ValueError('death model must match whole Oracle wording')
    presentation = params['presentation']
    if (not isinstance(presentation, dict) or set(presentation) != {'optional', 'target'}
            or presentation['optional'] not in ('none', 'trigger', 'trigger_draw', 'draw_dialog', 'controller_number')
            or presentation['target'] not in ('implicit', 'choose')):
        raise ValueError('reviewed actual trigger presentation required')
    if not model['receipt']['optional'] and presentation['optional'] != 'none':
        raise ValueError('mandatory Oracle ability cannot have an optional input plan')
    ui = _pins()['presentations'].get(record['name'])
    if ui is None or presentation != ui['presentation']:
        raise ValueError('source input presentation has not been reviewed')
    mana = cost_from_oracle(record)
    result = dict(copy.deepcopy(params), family=FAMILY, mana=mana,
                  creature='Creature' in record['type_line'], legendary='Legendary' in record['type_line'])
    supported = variants(result)
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or len(selected) != len(set(selected)) or set(selected) - set(supported):
        raise ValueError('explicit distinct supported death variants required')
    return result


def variants(model):
    modes = ['cast_only', 'payment', 'insufficient_mana', 'own_death', 'opponent_death',
             'self_death', 'token_death', 'artifact_death', 'enchantment_death',
             'stolen_death', 'bounce_victim', 'exile_victim', 'source_bounced',
             'lethal_damage', 'zero_toughness', 'sacrifice', 'combat_death',
             'two_deaths', 'simultaneous', 'cleanup_once', 'countered',
             'ready_snapshot', 'pending_snapshot', 'death_snapshot', 'resolved_snapshot', 'bare_zones']
    if any(model['mana'][1:]):
        modes.append('wrong_color')
    if not model['legendary'] and model['presentation']['optional'] not in ('trigger', 'trigger_draw'):
        modes.append('two_sources')
    predicate = model['predicate']
    if predicate['kind'] == 'Enchantment':
        modes = [m for m in modes if m not in ('lethal_damage', 'zero_toughness', 'sacrifice', 'combat_death')]
    if predicate['self_only']:
        modes.append('source_destroy_spell')
    if predicate['threshold']:
        modes += ['below_threshold', 'at_threshold', 'above_threshold', 'weakened_source_death']
    if predicate['subtype']:
        modes += ['wrong_subtype_death', 'changeling_death']
    if predicate['owner'] == 'other':
        modes += ['lent_death', 'opponent_artifact_death', 'opponent_enchantment_death',
                  'opponent_land_death', 'stolen_artifact_death']
    if model['receipt']['recipient'] in ('target_player', 'target_opponent'):
        modes += ['opponent_shrouded', 'both_players_shrouded']
    if model['receipt']['optional']:
        modes.append('decline')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes if mode != 'decline' or owner == 0]


def validate_witnesses(oracle, registry):
    pins = _pins()
    for name, record in pins['cards'].items():
        if name not in registry or any(oracle.get(name, {}).get(k) != v for k, v in record.items()):
            raise ValueError('death value trigger witness changed: ' + name)
    for name, ui in pins['presentations'].items():
        if name not in registry or registry[name]['handler'] != ui['handler'] or oracle.get(name, {}).get('oracle_id') != ui['oracle_id']:
            raise ValueError('death trigger source UI identity changed: ' + name)


def eligible(predicate, entity, source, oracle):
    if entity['ref'] == source['ref'] and predicate['self_included']:
        return True
    if predicate['self_only'] and entity['ref'] != source['ref'] or predicate['exclude_source'] and entity['ref'] == source['ref']:
        return False
    record = oracle[entity['name']]
    kinds = set(record['type_line'].partition(' — ')[0].split())
    if predicate['kind'] != 'Permanent' and predicate['kind'] not in kinds:
        return False
    if predicate['controller'] == 'you' and entity['player'] != source['player'] or predicate['controller'] == 'opponent' and entity['player'] == source['player']:
        return False
    if predicate['owner'] == 'other' and entity['owner'] == source['player'] or predicate['nontoken'] and entity['token']:
        return False
    if predicate['subtype'] and predicate['subtype'] not in record['type_line'].partition(' — ')[2].split() and 'Changeling' not in record['keywords']:
        return False
    threshold = predicate['threshold']
    return threshold is None or entity[threshold['property']] >= threshold['minimum']


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import card, step, observe, cost_from_oracle
    if name not in registry or registry[name]['csvid'] != csvid or len(selected) != len(set(selected)) or set(selected) - set(variants(model)):
        raise ValueError('death trigger registry identity or variant changed')
    output = []
    for variant in selected:
        mode, owner_text = variant.rsplit('_', 1)
        owner = int(owner_text)
        bare = mode == 'bare_zones'
        zones = {zone: [list(names) if not bare else [], list(names) if not bare else []]
                 for zone, names in [('hand', HAND), ('graveyard', GRAVE), ('exile', EXILE)]}
        zones['library'] = [list(LIBRARY) * 4, list(LIBRARY) * 4]
        setup, entities, actions = [], {}, []
        mana, life = [[40] * 6, [40] * 6], [20, 20]
        shroud = [False, False]
        pre = [step('reset_death_history'), step('turn', player=owner, value=0)]
        for zone, players in zones.items():
            for player, names in enumerate(players):
                setup += [card(n, f'p{player}_{zone}_{i}', player, zone) for i, n in enumerate(names)]

        def add(n, ref, player, zone='battlefield', token=False, stolen=False, tapped=True):
            entry = card(n, ref, player, zone)
            if zone == 'battlefield':
                entry['state'] = ['tapped' if tapped else 'untapped'] + (['token'] if token else []) + (['opponent_owned'] if stolen else [])
            setup.append(entry)
            r = oracle[n]
            entities[ref] = dict(name=n, ref=ref, player=player, owner=1-player if stolen else player,
                                 alive=zone == 'battlefield', hand=zone == 'hand', token=token,
                                 tapped=int(tapped and zone == 'battlefield'), damage=0,
                                 power=int(r.get('power', '0')), toughness=int(r.get('toughness', '0')))
            if zone != 'battlefield':
                zones[zone][player].append(n)

        def pay(n, player):
            mana[player] = [a-b for a, b in zip(mana[player], cost_from_oracle(oracle[n]))]

        def finish_spell(n, player):
            zones['hand'][player].remove(n)
            zones['graveyard'][player].append(n)

        def cast_permanent(ref):
            e = entities[ref]
            pay(e['name'], e['player'])
            zones['hand'][e['player']].remove(e['name'])
            e.update(alive=True, hand=False, tapped=0)
            return [step('cast', ref=ref, value=1), step('assert_stack', value=1),
                    observe(ref, 'in_hand', 0), step('resolve'), step('assert_choices_consumed')]

        def leave(ref, destination='graveyard'):
            e = entities[ref]
            e.update(alive=False, hand=destination == 'hand')
            if not e['token']:
                zones[destination][e['owner']].append(e['name'])

        def witnesses_for_death(refs):
            source_refs = [ref for ref in ('source', 'second') if ref in entities and entities[ref]['alive']]
            return [(source_ref, [ref for ref in refs if eligible(model['predicate'], entities[ref], entities[source_ref], oracle)])
                    for source_ref in source_refs]

        def queue_and_receipts(refs, decline=False):
            plan = []
            for source_ref, matches in witnesses_for_death(refs):
                if not matches:
                    continue
                source = entities[source_ref]
                chooser = source['player']
                receipt = model['receipt']
                presentation = model['presentation']
                if receipt['recipient'] == 'dead_controller':
                    amounts = Counter(entities[ref]['player'] for ref in matches)
                    # Fecundity's mandatory event asks each creature controller
                    # how many cards to draw. Its human menu is real even at0;
                    # the AI's ordinary policy draws the bounded offered amount
                    # while the fixture keeps at least20 cards behind it.
                    human_draw = 0 if decline else amounts[0] * receipt['draw']
                    plan.append(step('choose_number', player=0, value=human_draw))
                    for player in (0, 1):
                        amount = human_draw if player == 0 else amounts[player] * receipt['draw']
                        if amount > len(zones['library'][player]) - 20:
                            raise ValueError('controller draw fixture must preserve the real AI policy floor')
                        zones['hand'][player].extend(zones['library'][player][:amount])
                        del zones['library'][player][:amount]
                    continue
                inner_draw = presentation['optional'] in ('draw_dialog', 'trigger_draw')
                dying_draw_legacy = source_ref in refs and presentation['optional'] == 'trigger_draw'
                if presentation['optional'] in ('trigger', 'trigger_draw') and chooser == 0 and (not decline or inner_draw) and not dying_draw_legacy:
                    # A surviving source may share its display identity with an
                    # invisible legacy. Pin that real instance; a dying source
                    # still needs the original legacy handoff selector.
                    operation = 'choose_trigger' if source_ref in refs else 'choose_trigger_source'
                    plan.append(step(operation, player=chooser, ref=source_ref))
                if inner_draw:
                    # A surviving Harvester has an optional outer trigger and
                    # an actual draw/No menu. Its dying-source legacy is already
                    # mandatory, so only that inner menu is presented.
                    no_draw = 1 if len(matches) * receipt['draw'] == 1 else 2
                    plan.append(step('choose_dialog', player=chooser, value=no_draw if decline else 0))
                if decline:
                    continue
                recipient = chooser
                if receipt['recipient'] in ('opponents', 'target_opponent', 'target_player'):
                    recipient = 1-chooser
                    if receipt['recipient'] == 'target_player' and shroud[recipient]:
                        recipient = chooser
                    if receipt['recipient'].startswith('target_') and shroud[recipient]:
                        continue
                    if presentation['target'] == 'choose':
                        plan.extend(step('choose_player', player=chooser, value=recipient) for _ in matches)
                amount = len(matches)
                if receipt['draw']:
                    n = amount * receipt['draw']
                    if len(zones['library'][chooser]) < n:
                        raise ValueError('bounded death draw needs enough library')
                    zones['hand'][chooser].extend(zones['library'][chooser][:n])
                    del zones['library'][chooser][:n]
                life[chooser] += amount * receipt['gain']
                life[recipient] -= amount * receipt['loss']
            return plan

        def remove_with(n, ref, target, caster=None):
            player = 1-owner if caster is None else caster
            add(n, ref, player, 'hand')
            pay(n, player)
            finish_spell(n, player)
            entities[ref]['hand'] = False
            return [step('choose_target', player=player, ref=target), step('cast', ref=ref, value=1), step('assert_stack', value=1)]

        def death_actions(ref, removal_ref='removal', decline=False):
            kinds = oracle[entities[ref]['name']]['type_line'].partition(' — ')[0].split()
            watched = any(ref in matches for _, matches in witnesses_for_death([ref]))
            if 'Enchantment' in kinds and 'Creature' not in kinds and watched:
                # Standalone destroy does not finish enchantment-only GFP
                # triggers. Use an ordinary paid spell and normal resolution.
                return (remove_with('Naturalize', removal_ref, ref) +
                        queue_and_receipts([ref], decline=decline) + [step('resolve')])
            return queue_and_receipts([ref], decline=decline) + [step('destroy', ref=ref)]

        add(name, 'source', owner, 'hand')
        predicate = model['predicate']
        opposing_death = mode == 'opponent_death' or mode.startswith('opponent_') and mode.endswith('_death') or mode == 'lent_death'
        death_player = 1-owner if opposing_death else owner
        witness = ('Ornithopter' if predicate['kind'] == 'Artifact' else
                   'Exploration' if predicate['kind'] == 'Enchantment' and not predicate['self_only'] else
                   'Scathe Zombies' if predicate['subtype'] == 'Zombie' else
                   'Phyrexian Hulk' if predicate['threshold'] else 'Grizzly Bears')
        if mode == 'sacrifice':
            witness = 'Bottle Gnomes' if predicate['kind'] == 'Artifact' else 'Spore Frog'
        elif mode in ('artifact_death', 'opponent_artifact_death', 'stolen_artifact_death'):
            witness = 'Sol Ring'
        elif mode in ('enchantment_death', 'opponent_enchantment_death'):
            witness = 'Exploration'
        elif mode == 'opponent_land_death':
            witness = 'Forest'
        elif mode == 'wrong_subtype_death':
            witness = 'Grizzly Bears'
        elif mode == 'changeling_death':
            witness = 'Woodland Changeling'
        elif mode in ('below_threshold', 'at_threshold', 'above_threshold'):
            threshold = predicate['threshold']
            expected_minimum = 5 if threshold['property'] == 'power' else 4
            if threshold['minimum'] != expected_minimum:
                raise ValueError('new threshold needs an independently reviewed witness boundary')
            witness = {'below_threshold': 'Air Elemental' if threshold['property'] == 'power' else 'Centaur Courser',
                       'at_threshold': 'Phyrexian Hulk',
                       'above_threshold': 'Craw Wurm' if threshold['property'] == 'power' else 'Scaled Wurm'}[mode]
        add(witness, 'victim', death_player, token=mode == 'token_death',
            stolen=mode in ('stolen_death', 'lent_death', 'stolen_artifact_death'), tapped=mode != 'combat_death')
        add('Hill Giant', 'survivor', 1-owner)
        if mode == 'simultaneous':
            add('Eager Cadet', 'other_victim', 1-owner)
        if mode == 'two_deaths':
            add('Grizzly Bears', 'other_victim', 1-owner)
        if mode == 'two_sources':
            add(name, 'second', owner, 'hand')
        if mode in ('payment', 'insufficient_mana', 'wrong_color'):
            mana[owner] = list(model['mana']) if mode != 'insufficient_mana' else [0] * 6
            if mode == 'wrong_color':
                color = next(i for i in range(1, 6) if mana[owner][i])
                amount = mana[owner][color]
                mana[owner][color] = 0
                mana[owner][1 if color != 1 else 2] += amount
        pre.extend(step('set_mana', player=player, color=color, value=value)
                   for player in (0, 1) for color, value in enumerate(mana[player]))
        pre.append(step('refresh'))
        source_pending = 0
        death_cut = None
        denied = mode in ('insufficient_mana', 'wrong_color')
        if denied:
            actions += [step('cast', ref='source', value=0), step('assert_choices_consumed')]
        elif mode == 'countered':
            add('Counterspell', 'counter', 1-owner, 'hand')
            pay(name, owner)
            pay('Counterspell', 1-owner)
            finish_spell(name, owner)
            finish_spell('Counterspell', 1-owner)
            entities['source']['hand'] = entities['counter']['hand'] = False
            actions += [step('cast', ref='source', value=1), step('assert_stack', value=1),
                        step('announce_spell', ref='source'), step('cast_response', ref='counter', value=1),
                        step('assert_stack', value=2), step('resolve_response'), step('assert_choices_consumed')]
        else:
            actions += cast_permanent('source')
            source_pending = 3
            actions += [step('assert_life', player=owner, value=life[owner]),
                        step('assert_zone', player=owner, zone='library', value=len(zones['library'][owner]))]
            if mode == 'two_sources':
                actions += cast_permanent('second')
            if mode in ('opponent_shrouded', 'both_players_shrouded'):
                for player in ([1-owner] if mode == 'opponent_shrouded' else [0, 1]):
                    ref = f'shield{player}'
                    add('Gilded Light', ref, player, 'hand')
                    pay('Gilded Light', player)
                    finish_spell('Gilded Light', player)
                    entities[ref]['hand'] = False
                    actions += [step('cast', ref=ref, value=1), step('resolve')]
                    shroud[player] = True
            if mode not in ('cast_only', 'payment'):
                if mode == 'source_bounced':
                    actions += remove_with('Unsummon' if model['creature'] else 'Disperse', 'bounce', 'source') + [step('resolve')]
                    leave('source', 'hand')
                    actions.append(step('bind_hand', ref='source', player=owner, value=csvid))
                if mode == 'bounce_victim':
                    target = 'source' if predicate['self_only'] else 'victim'
                    bounce = 'Unsummon' if 'Creature' in oracle[entities[target]['name']]['type_line'] else 'Disperse'
                    actions += remove_with(bounce, 'removal', target) + [step('resolve')]
                    leave(target, 'hand')
                    # The fixture already contains this printed identity in
                    # hand. Exact zone receipts prove the real bounce without
                    # pretending that an ambiguous copy is the original slot.
                    entities[target]['inspect_instance'] = False
                elif mode == 'exile_victim':
                    target = 'source' if predicate['self_only'] else 'victim'
                    creature_target = 'Creature' in oracle[entities[target]['name']]['type_line']
                    actions += remove_with('Swords to Plowshares' if creature_target else 'Return to Dust', 'removal', target) + [step('resolve')]
                    if creature_target:
                        life[entities[target]['player']] += entities[target]['power']
                    leave(target, 'exile')
                elif mode in ('lethal_damage', 'zero_toughness', 'death_snapshot', 'source_destroy_spell'):
                    target = 'source' if mode == 'source_destroy_spell' or predicate['self_only'] else 'victim'
                    if mode == 'lethal_damage':
                        spell = 'Lightning Bolt' if entities[target]['toughness'] <= 3 else 'Lightning Blast'
                    elif mode == 'zero_toughness':
                        spell = 'Disfigure' if entities[target]['toughness'] <= 2 else 'Grasp of Darkness'
                    else:
                        spell = 'Murder' if 'Creature' in oracle[entities[target]['name']]['type_line'] else 'Naturalize'
                    actions += remove_with(spell, 'removal', target)
                    if mode == 'zero_toughness' and predicate['threshold']:
                        reduction = 2 if spell == 'Disfigure' else 4
                        entities[target]['power'] -= reduction
                        entities[target]['toughness'] -= reduction
                    actions += queue_and_receipts([target])
                    death_cut = len(actions)
                    actions.append(step('resolve'))
                    leave(target)
                elif mode == 'sacrifice':
                    actions += queue_and_receipts(['victim'])
                    death_cut = len(actions)
                    actions += [step('require_can_activate', ref='victim', value=1), step('activate', ref='victim', value=1),
                                observe('victim', 'in_play', 0), step('resolve')]
                    leave('victim')
                    if witness == 'Bottle Gnomes':
                        life[death_player] += 3
                elif mode == 'combat_death':
                    add('Scaled Wurm', 'attacker', 1-owner, tapped=False)
                    actions += [step('turn', player=1-owner, value=0), step('begin_attackers'),
                                step('attack', ref='attacker', value=1), step('finish_attackers'),
                                step('begin_blockers'), step('block', ref='victim', other='attacker', value=1), step('advance_combat')]
                    entities['attacker'].update(tapped=1, damage=entities['victim']['power'])
                    actions += queue_and_receipts(['victim'])
                    actions += [step('combat_damage'), step('end_combat')]
                    leave('victim')
                elif mode == 'simultaneous':
                    sweep = 'Planar Cleansing' if predicate['kind'] in ('Enchantment', 'Permanent') else 'Wrath of God'
                    add(sweep, 'wrath', 1-owner, 'hand')
                    pay(sweep, 1-owner)
                    finish_spell(sweep, 1-owner)
                    entities['wrath']['hand'] = False
                    # Wrath has no target. All current creatures share one
                    # real death event batch; source LKI is evaluated first.
                    actions.append(step('turn', player=1-owner, value=0))
                    actions += [step('cast', ref='wrath', value=1), step('assert_stack', value=1)]
                    dying = [ref for ref, entity in entities.items() if entity['alive'] and
                             ('Land' not in oracle[entity['name']]['type_line'] if sweep == 'Planar Cleansing' else 'Creature' in oracle[entity['name']]['type_line'])]
                    actions += queue_and_receipts(dying)
                    actions.append(step('resolve'))
                    for ref in dying:
                        leave(ref)
                elif mode == 'weakened_source_death':
                    actions += remove_with('Diminish', 'diminish', 'source') + [step('resolve')]
                    entities['source'].update(power=1, toughness=1)
                    actions += [observe('source', 'power', 1), observe('source', 'toughness', 1)]
                    actions += remove_with('Murder', 'removal', 'source')
                    actions += queue_and_receipts(['source']) + [step('resolve')]
                    leave('source')
                else:
                    self_receipt_modes = {'ready_snapshot', 'pending_snapshot', 'resolved_snapshot',
                                          'bare_zones', 'cleanup_once', 'two_deaths', 'two_sources'}
                    dying = ['source'] if mode == 'self_death' or predicate['self_only'] and mode in self_receipt_modes else ['victim']
                    if predicate['threshold'] and dying == ['victim']:
                        actions += [observe('victim', prop, entities['victim'][prop]) for prop in ('power', 'toughness')]
                    # These ordinary paths have one corpse. Preserve their
                    # original actions unless a watched enchantment needs the
                    # real spell lifecycle calibrated by the Naturalize probe.
                    actions += death_actions(dying[0], decline=mode == 'decline')
                    death_cut = len(actions)-1
                    for ref in dying:
                        leave(ref)
                    if mode == 'two_deaths':
                        actions += death_actions('other_victim', 'removal_other')
                        leave('other_victim')
                    if mode == 'two_sources' and predicate['self_only']:
                        actions += death_actions('second', 'removal_second')
                        leave('second')
                actions.append(step('assert_choices_consumed'))
                if mode == 'cleanup_once':
                    actions += [step('cleanup'), step('refresh')]
        checks = [step('assert_stack', value=0), step('assert_choices_consumed')]
        for ref, entity in entities.items():
            if entity.get('inspect_instance', True):
                checks += [observe(ref, 'in_play', int(entity['alive'])), observe(ref, 'in_hand', int(entity['hand']))]
            if entity['alive'] and 'Creature' in oracle[entity['name']]['type_line']:
                checks += [observe(ref, key, entity[key]) for key in ('power', 'toughness', 'damage', 'tapped')]
        universe = {entry['name'] for entry in setup} | {name}
        for player in (0, 1):
            checks += [step('assert_life', player=player, value=life[player]),
                       step('assert_poison', player=player, value=0), step('assert_energy', player=player, value=0),
                       step('assert_zone', player=player, zone='battlefield', value=sum(e['alive'] and e['player'] == player for e in entities.values()))]
            if mode != 'cleanup_once':
                checks.append(step('assert_mana_total', player=player, value=sum(mana[player])))
            if mode in ('payment', 'insufficient_mana', 'wrong_color'):
                checks.extend(step('assert_mana', player=player, color=color, value=value) for color, value in enumerate(mana[player]))
            for zone, players in zones.items():
                names = players[player]
                checks.append(step('assert_zone', player=player, zone=zone, value=len(names)))
                counts = Counter(names)
                checks.extend(step('assert_zone_multiplicity', player=player, zone=zone,
                                   index=registry[n]['csvid'], value=counts[n]) for n in sorted(universe))
                if zone == 'library':
                    checks.extend(step('assert_zone_card', player=player, zone=zone, index=i, value=registry[n]['csvid']) for i, n in enumerate(names))
            checks.extend(step('assert_zone_multiplicity', player=player, zone='battlefield', index=registry[n]['csvid'],
                               value=sum(e['alive'] and e['player'] == player and e['name'] == n for e in entities.values())) for n in sorted(universe))
        sequence = pre + actions + checks
        if mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot'):
            cut = 0 if mode == 'ready_snapshot' else source_pending if mode == 'pending_snapshot' else death_cut
            if cut is None:
                raise ValueError('death snapshot lacks a real pending death action')
            sequence = pre + actions[:cut] + [step('capture')] + actions[cut:] + checks + [step('record_state'), step('ai_roundtrip'), step('restore')] + actions[cut:] + checks + [step('assert_recorded_state')]
        elif mode == 'resolved_snapshot':
            sequence += [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore'), step('assert_recorded_state')]
        output.append(dict(name=f'DeathValueTriggerRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                           setup=setup, steps=sequence, _recipe=dict(family=FAMILY, version=2, variant=variant,
                           params=declaration['params'], oracle_text=declaration['oracle_text'],
                           untested_components=model['keywords'] + ['unrelated replacement/prevention and trigger interactions',
                           'live source control/type changes', 'independently selected targets in simultaneous targeted triggers',
                           'alternative legal human player target when the handler selects implicitly',
                           'empty-library draw loss', 'multiplayer'] +
                           (['source enchantment destroyed simultaneously with the watched creatures']
                            if model['receipt']['recipient'] == 'dead_controller' else []))))
    return output
