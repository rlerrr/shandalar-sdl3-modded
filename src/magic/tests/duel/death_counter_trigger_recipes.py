"""Whole-Oracle fixed death counters on a surviving watcher."""
import copy
import json
from collections import Counter
from pathlib import Path
import re

from death_value_trigger_recipes import eligible
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

FAMILY = 'death_counter_trigger'


def pins():
    return json.loads((Path(__file__).parent/'data/death_counter_trigger_wave8_profiles.json').read_text(encoding='utf8'))


def exact(a, b):
    return type(a) is type(b) and (set(a) == set(b) and all(exact(a[k], b[k]) for k in a)
                                 if isinstance(a, dict) else len(a) == len(b) and all(exact(x, y) for x, y in zip(a, b))
                                 if isinstance(a, list) else a == b)


def oracle_model(record):
    if (not isinstance(record, dict) or record.get('layout') != 'normal' or record.get('card_faces') or
            record.get('selected_face') or not record.get('type_line', '').startswith('Creature \u2014 ') or
            record.get('keywords') or any(not re.fullmatch(r'[1-9][0-9]*', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('complete normal numeric death-counter creature required')
    refs = '(?:this creature|'+re.escape(record['name'])+')'
    match = re.fullmatch(r'Whenever (.+), (you may )?put a \+1/\+1 counter on '+refs+r'\.', record['oracle_text'])
    if not match:
        raise ValueError('whole fixed death-counter Oracle clause required')
    predicate = dict(kind='Creature', self_only=False, exclude_source=False, controller='any',
                     owner='any', nontoken=False, subtype=None, threshold=None, self_included=False)
    subject = match[1]
    if subject == 'another creature you control dies':
        predicate.update(exclude_source=True, controller='you')
    elif subject == 'a creature an opponent controls dies':
        predicate['controller'] = 'opponent'
    elif subject == 'a Goblin you control is put into a graveyard from the battlefield':
        predicate.update(kind='Permanent', controller='you', subtype='Goblin')
    elif subject == 'an artifact is put into a graveyard from the battlefield':
        predicate['kind'] = 'Artifact'
    else:
        raise ValueError('unreviewed death-counter predicate')
    return dict(predicate=predicate, optional=bool(match[2]), counters=1)


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (declaration.get('family') != FAMILY or declaration.get('version') != 2 or
            declaration.get('oracle_text') != record.get('oracle_text')):
        raise ValueError('whole pinned death-counter declaration required')
    expected = dict(schema=8, **oracle_model(record))
    if not exact(declaration.get('params'), expected):
        raise ValueError('typed death-counter parameters disagree with Oracle')
    model = dict(copy.deepcopy(expected), family=FAMILY, mana=cost_from_oracle(record),
                 power=int(record['power']), toughness=int(record['toughness']))
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or len(set(selected)) != len(selected) or set(selected)-set(variants(model)):
        raise ValueError('explicit distinct supported death-counter variants required')
    return model


def variants(model):
    modes = ['cast_only', 'payment', 'insufficient_mana', 'wrong_color', 'countered',
             'own_death', 'opponent_death', 'stolen_death', 'lent_death', 'source_foreign_owner',
             'token_death', 'nonmatching', 'artifact_creature', 'noncreature_artifact',
             'opponent_noncreature_artifact', 'noncreature_enchantment', 'source_removed',
             'source_bounced', 'source_exiled', 'source_simultaneous', 'repeat', 'cleanup_once',
             'source_blinked', 'counter_cancel', 'temporary_pump', 'bare_zones',
             'ready_snapshot', 'pending_snapshot', 'death_snapshot', 'resolved_snapshot']
    if not model['optional']:
        modes.append('simultaneous')
    if model['predicate']['subtype'] == 'Goblin':
        modes += ['kindred_goblin', 'opponent_kindred']
    result = [f'{mode}_{player}' for player in (0, 1) for mode in modes]
    if model['optional']:
        result += ['decline_0', 'partial_accept_0']
    return result


def validate_witnesses(oracle, registry):
    data = pins()
    for name, record in data['cards'].items():
        if name not in registry or not exact(oracle.get(name), record):
            raise ValueError('complete death-counter witness changed: '+name)
    for name, source in data['sources'].items():
        row = registry.get(name, {})
        if (not exact(oracle.get(name), source['oracle']) or row.get('csvid') != source['csvid'] or
                row.get('handler') != source['handler'] or row.get('kind') != 'card' or
                row.get('handler') in ('card_dummy', 'NULL', '0')):
            raise ValueError('implemented death-counter source changed: '+name)
        model = oracle_model(oracle[name])
        if model['optional'] and data['presentation'].get(name) != dict(optional='live_source_priority', inner_dialog=None):
            raise ValueError('reviewed live-source optional presentation required')


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import card, step, observe, cost_from_oracle
    validate_witnesses(oracle, registry)
    if name not in pins()['sources'] or pins()['sources'][name]['csvid'] != csvid:
        raise ValueError('unreserved death-counter source')
    if not selected or len(set(selected)) != len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unsupported death-counter selection')
    output = []
    for variant in selected:
        mode, controller = variant.rsplit('_', 1)
        controller = int(controller)
        bare = mode == 'bare_zones'
        zones = {zone: [list(names) if not bare else [] for _ in (0, 1)]
                 for zone, names in [('hand', HAND), ('graveyard', GRAVE), ('exile', EXILE)]}
        zones['library'] = [list(LIBRARY), list(LIBRARY)]
        setup, entities, actions = [], {}, []
        mana, life = [[0]*6, [0]*6], [20, 20]
        pre = [step('reset_death_history'), step('turn', player=controller, value=0)]
        pre += [step('set_mana', player=p, color=c, value=0) for p in (0, 1) for c in range(6)]
        for zone, players in zones.items():
            for player, names in enumerate(players):
                setup += [card(n, f'{zone}_{player}_{i}', player, zone) for i, n in enumerate(names)]

        def add(n, ref, player, zone='battlefield', foreign=False, token=False):
            item = card(n, ref, player, zone)
            if zone == 'battlefield':
                item['state'] = ['untapped']+(['opponent_owned'] if foreign else [])+(['token'] if token else [])
            setup.append(item)
            record = oracle[n]
            entities[ref] = dict(name=n, ref=ref, player=player, owner=1-player if foreign else player,
                                 alive=zone == 'battlefield', hand=zone == 'hand', token=token,
                                 power=int(record.get('power') or 0), toughness=int(record.get('toughness') or 0))
            if zone != 'battlefield':
                zones[zone][player].append(n)

        seeded = mode == 'source_foreign_owner'
        add(name, 'source', controller, 'battlefield' if seeded else 'hand', foreign=seeded)
        source = entities['source']
        counters = boost = damage = 0
        predicate = model['predicate']
        witness = 'Ornithopter' if predicate['kind'] == 'Artifact' else 'Raging Goblin' if predicate['subtype'] else 'Grizzly Bears'
        victim_player = 1-controller if predicate['controller'] == 'opponent' else controller
        if mode in ('own_death', 'stolen_death'):
            victim_player = controller
        elif mode in ('opponent_death', 'lent_death', 'opponent_noncreature_artifact', 'opponent_kindred'):
            victim_player = 1-controller
        if mode == 'nonmatching':
            if predicate['kind'] == 'Creature' and predicate['controller'] == 'you':
                witness = 'Sol Ring'
            else:
                witness = 'Grizzly Bears'
                victim_player = controller
        elif mode == 'artifact_creature':
            witness = 'Ornithopter'
        elif mode in ('noncreature_artifact', 'opponent_noncreature_artifact'):
            witness = 'Sol Ring'
        elif mode == 'noncreature_enchantment':
            witness = 'Exploration'
        elif mode in ('kindred_goblin', 'opponent_kindred'):
            witness = 'Boggart Shenanigans'
        add(witness, 'victim', victim_player, foreign=mode in ('stolen_death', 'lent_death'), token=mode == 'token_death')
        if mode in ('repeat', 'source_blinked', 'simultaneous', 'partial_accept', 'source_simultaneous'):
            add(witness, 'second_victim', victim_player)
        pending_cut = death_cut = ready_cut = None

        def fund(player, pool):
            mana[player] = list(pool)
            return [step('set_mana', player=player, color=c, value=v) for c, v in enumerate(pool)]

        def mana_checks():
            return [step('assert_mana', player=p, color=c, value=v) for p in (0, 1) for c, v in enumerate(mana[p])]

        def stats():
            if not source['alive']:
                return []
            return [observe(source['ref'], prop, value) for prop, value in (
                ('in_play', 1), ('in_hand', 0), ('csvid', csvid), ('plus1_counters', counters),
                ('minus1_counters', 0), ('power', model['power']+counters+boost),
                ('toughness', model['toughness']+counters+boost), ('damage', damage))]

        def spell(n, ref, target=None, caster=None):
            player = 1-controller if caster is None else caster
            add(n, ref, player, 'hand')
            cost = cost_from_oracle(oracle[n])
            sequence = fund(player, cost)
            if target is not None:
                sequence.append(step('choose_target', player=player, ref=target))
            sequence += [step('cast', ref=ref, value=1), step('assert_stack', value=1)]
            zones['hand'][player].remove(n)
            zones['graveyard'][player].append(n)
            entities[ref]['hand'] = False
            mana[player] = [a-b for a, b in zip(mana[player], cost)]
            return sequence

        def leave(ref, zone='graveyard'):
            entity = entities[ref]
            entity.update(alive=False, hand=zone == 'hand')
            if not entity['token']:
                zones[zone][entity['owner']].append(entity['name'])

        def receipt_plan(refs, survives=True, decline=False, partial=False):
            nonlocal counters
            count = sum(eligible(predicate, entities[ref], source, oracle) for ref in refs)
            if not source['alive'] or not survives or not count:
                return []
            accepted = 0 if decline else 1 if partial else count
            counters += accepted*model['counters']
            if model['optional'] and controller == 0 and not decline:
                if count > 1 and not partial:
                    raise ValueError('independent optional all-accept ordering is not calibrated')
                return [step('choose_trigger_source', ref=source['ref'], player=controller)]
            return []

        def effect_checks():
            # Counter/P/T failures expose missing receipts first. The explicit
            # unused0 observer then drains unavailable selector intent without
            # turning a genuine missing trigger into an unscripted UI error.
            return stats()+[step('assert_unused_choices', value=0)]+mana_checks()

        def kill(ref, removal_ref='removal', decline=False):
            nonlocal death_cut
            entity = entities[ref]
            noncreature = 'Creature' not in oracle[entity['name']]['type_line']
            prefix = spell('Naturalize', removal_ref, ref) if noncreature else []
            intent = receipt_plan([ref], survives=ref != 'source', decline=decline)
            death_cut = len(actions)+len(prefix)+len(intent)
            sequence = prefix+intent+[step('resolve') if noncreature else step('destroy', ref=entity['ref'])]
            leave(ref)
            return sequence+effect_checks()

        if seeded:
            actions += stats()
        else:
            pool = list(model['mana'])
            if mode == 'insufficient_mana':
                pool = [0]*6
            elif mode == 'wrong_color':
                unused = next(c for c in range(1, 6) if not pool[c])
                colored = sum(pool[1:])
                pool = [pool[0], 0, 0, 0, 0, 0]
                pool[unused] = colored
            actions += fund(controller, pool)
            ready_cut = len(actions)
            if mode in ('insufficient_mana', 'wrong_color'):
                actions += [step('cast', ref='source', value=0)]
            elif mode == 'countered':
                add('Counterspell', 'counter', 1-controller, 'hand')
                actions += fund(1-controller, cost_from_oracle(oracle['Counterspell']))
                zones['hand'][controller].remove(name)
                zones['graveyard'][controller].append(name)
                source['hand'] = False
                mana[controller] = [0]*6
                zones['hand'][1-controller].remove('Counterspell')
                zones['graveyard'][1-controller].append('Counterspell')
                entities['counter']['hand'] = False
                mana[1-controller] = [0]*6
                actions += [step('cast', ref='source', value=1), step('assert_stack', value=1),
                            step('announce_spell', ref='source'), step('cast_response', ref='counter', value=1),
                            step('assert_stack', value=2), step('resolve_response'), step('assert_choices_consumed')]
            else:
                zones['hand'][controller].remove(name)
                source.update(alive=True, hand=False)
                mana[controller] = [0]*6
                actions += [step('cast', ref='source', value=1), step('assert_stack', value=1),
                            observe('source', 'in_hand', 0)]
                pending_cut = len(actions)
                actions += [step('resolve'), step('assert_choices_consumed')]+stats()+mana_checks()

        skip = mode in ('cast_only', 'payment', 'insufficient_mana', 'wrong_color', 'countered')
        if not skip:
            if mode in ('source_removed', 'source_bounced', 'source_exiled'):
                spell_name = {'source_removed': 'Murder', 'source_bounced': 'Unsummon', 'source_exiled': 'Swords to Plowshares'}[mode]
                actions += spell(spell_name, 'source_removal', source['ref'])+[step('resolve'), step('assert_unused_choices', value=0)]
                if mode == 'source_exiled':
                    life[controller] += model['power']
                leave('source', {'source_removed': 'graveyard', 'source_bounced': 'hand', 'source_exiled': 'exile'}[mode])
                actions += kill('victim')
            elif mode in ('simultaneous', 'partial_accept'):
                # The source casts both legal spells during its own main1.
                # Growth protects the watcher while both ordinary victims die.
                actions += spell('Giant Growth', 'protection_growth', source['ref'], caster=controller)+[step('resolve')]
                boost = 3
                actions += stats()
                actions += spell('Pyroclasm', 'sweep', caster=controller)
                actions += receipt_plan(['victim', 'second_victim'], partial=mode == 'partial_accept')
                actions += [step('resolve')]
                damage = 2
                leave('victim'); leave('second_victim')
                actions += effect_checks()
            elif mode == 'source_simultaneous':
                actions += spell('Wrath of God', 'sweep', caster=controller)+[step('resolve')]
                # The departed source cannot receive counters. Do not select
                # it as a surviving trigger or inspect a graveyard zone index.
                leave('source'); leave('victim'); leave('second_victim')
                actions += [step('assert_unused_choices', value=0)]
            else:
                actions += kill('victim', decline=mode == 'decline')
                if mode == 'repeat':
                    actions += kill('second_victim', 'second_removal')
                elif mode == 'source_blinked':
                    actions += spell('Cloudshift', 'blink', source['ref'], caster=controller)+[step('resolve')]
                    source.update(ref='source_new', player=source['owner'])
                    counters = boost = damage = 0
                    actions += [step('bind_permanent', ref=source['ref'], player=source['player'], value=csvid)]+stats()
                    actions += kill('second_victim', 'second_removal')
                elif mode == 'counter_cancel':
                    actions += [step('require_card', ref=source['ref'], property='plus1_counters', value=1)]
                    actions += spell('Virulent Wound', 'counter_cancel', source['ref'])+[step('resolve')]
                    counters -= 1
                    actions += stats()+[step('cleanup'), step('refresh')]+stats()
                elif mode == 'temporary_pump':
                    actions += spell('Giant Growth', 'growth', source['ref'], caster=controller)+[step('resolve')]
                    boost = 3
                    actions += stats()+[step('cleanup'), step('refresh')]
                    boost = 0
                    actions += stats()
                elif mode == 'cleanup_once':
                    actions += [step('cleanup'), step('refresh')]+stats()

        checks = [step('assert_stack', value=0), step('assert_choices_consumed')]+stats()+mana_checks()
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
        if mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot'):
            cut = {'ready_snapshot': ready_cut, 'pending_snapshot': pending_cut, 'death_snapshot': death_cut}[mode]
            if cut is None:
                raise ValueError('actual death-counter snapshot boundary missing')
            tail = actions[cut:]+checks
            sequence = pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'), step('ai_roundtrip'), step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        elif mode == 'resolved_snapshot':
            sequence += [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore'), step('assert_recorded_state')]
        output.append(dict(name=f'DeathCounterTriggerRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                           setup=setup, steps=sequence, _recipe=dict(family=FAMILY, version=2, variant=variant,
                           params=declaration['params'], oracle_text=declaration['oracle_text'],
                           untested_components=['AI partial-decline choices', 'independently ordered pending triggers/removing source in response',
                                                'additional continuous keyword/type/control changes', 'multiplayer'])))
    return output
