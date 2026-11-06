"""Whole-Oracle-pinned entry values and entry watchers, with private cohorts."""
import copy
import json
from collections import Counter
from pathlib import Path

DATA = Path(__file__).parent / 'data/entry_value_wave6_witnesses.json'
PIN_FIELDS = ('oracle_id', 'name', 'oracle_text', 'mana_cost', 'type_line', 'power',
              'toughness', 'keywords', 'colors', 'color_identity', 'cmc', 'layout')
BASE_LIBRARY = ('Forest', 'Plains', 'Island', 'Mountain', 'Swamp', 'Forest', 'Island')


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def pin(record):
    return {k: record[k] for k in PIN_FIELDS if k in record}


def exact_pin(a, b):
    if type(a) is not type(b): return False
    if isinstance(a, dict): return a.keys() == b.keys() and all(exact_pin(a[k], b[k]) for k in a)
    if isinstance(a, list): return len(a) == len(b) and all(exact_pin(x, y) for x, y in zip(a, b))
    return a == b


def spec(d, r):
    from recipes import cost_from_oracle
    if (d.get('family') != 'entry_value_extension' or type(d.get('version')) is not int or d['version'] != 2
            or not isinstance(d.get('variants'), list) or not d['variants']
            or not all(isinstance(v, str) for v in d['variants']) or len(set(d['variants'])) != len(d['variants'])):
        raise ValueError('entry extension requires exact family, integer version two and explicit unique variants')
    p = d.get('params')
    if not isinstance(p, dict) or set(p) != {'schema', 'profile'} or type(p['schema']) is not int or p['schema'] != 1:
        raise ValueError('entry extension requires its explicit private schema and profile')
    q = pins()['sources'].get(p['profile'])
    if (not q or not r or not exact_pin(pin(r), q['oracle']) or d.get('oracle_text') != r['oracle_text']
            or p['profile'] != r['name'] or r.get('layout') != 'normal' or r.get('card_faces')
            or r.get('selected_face') or 'Creature' not in r['type_line']
            or 'Token' in r['type_line'] or not str(r.get('power', '')).isdigit()
            or not str(r.get('toughness', '')).isdigit()):
        raise ValueError('entry extension source identity and whole Oracle pin must match')
    if q['mechanism'] not in ('scry', 'other_creature_life', 'counter_condition_draw', 'other_entry_life', 'counted_entry', 'conditional_entry', 'entry_watch', 'land_watch', 'entry_max_counters_draw'):
        raise ValueError('unreviewed entry extension mechanism')
    if q['mechanism'] == 'counted_entry' and (q.get('metric') not in ('own_artifacts', 'own_green_creatures', 'own_black_devotion', 'own_creatures', 'own_green_devotion', 'own_vampires', 'own_gates', 'own_counter_creatures', 'own_named_creatures') or q.get('effect') not in ('gain', 'draw', 'drain', 'draw_lose')):
        raise ValueError('unreviewed entry counting rule')
    if q['mechanism'] == 'conditional_entry' and (q.get('condition') not in ('two_gates', 'own_enchantment', 'other_spirit', 'another_named', 'own_black_permanent', 'three_artifacts') or q.get('effect') not in ('draw', 'gain', 'drain_target')):
        raise ValueError('unreviewed conditional entry rule')
    if q['mechanism'] == 'entry_watch' and (q.get('entry_filter') not in ('creature', 'enchantment', 'Ally', 'Zombie', 'Beast', 'Treefolk', 'flying', 'artifact')
            or q.get('scope') not in ('own', 'any') or type(q.get('include_self')) is not bool
            or q.get('quantity') not in ('fixed', 'toughness', 'power') or q.get('effect') not in ('gain', 'draw', 'drain')):
        raise ValueError('unreviewed entry watcher predicate or receipt')
    if q['mechanism'] == 'land_watch' and (q.get('scope') not in ('own', 'opponent') or q.get('effect') not in ('gain', 'gain_draw') or type(q.get('optional')) is not bool):
        raise ValueError('unreviewed land-entry watcher')
    return dict(q, family='entry_value_extension', mana=cost_from_oracle(r),
                power=int(r['power']), toughness=int(r['toughness']), legendary='Legendary' in r['type_line'])


def variants(p):
    common = ['effect', 'payment', 'insufficient_mana', 'wrong_color', 'countered',
              'ready_snapshot', 'pending_snapshot', 'resolved_snapshot', 'blink',
              'blink_snapshot', 'bounce_recast', 'bounce_only', 'two_copies',
              'cleanup', 'fresh_attack', 'block_ground', 'block_flying', 'bare_zones']
    if p['mechanism'] == 'scry': common += ['keep', 'reorder', 'short_library', 'empty_library']
    if p['mechanism'] == 'other_creature_life': common += ['no_qualifier', 'enemy_only', 'late_creature', 'destroy_other']
    if p['mechanism'] == 'counter_condition_draw': common += ['no_qualifier', 'enemy_counter', 'counter_removed', 'late_counter']
    if p['mechanism'] == 'other_entry_life': common += ['enemy_entry', 'noncreature', 'source_removed', 'repeat_entry']
    if p['mechanism'] == 'counted_entry': common += ['no_qualifier', 'enemy_only', 'late_creature', 'destroy_other']
    if p['mechanism'] == 'conditional_entry':
        common += ['no_qualifier', 'enemy_only', 'condition_removed']
        # Preserve the original ordering of the reviewed first schemas.
        common.insert(len(common)-1, 'threshold_one' if p['condition'] == 'two_gates' else 'two_qualifiers')
        if p['condition'] == 'another_named': common.remove('two_qualifiers')
        if p['condition'] == 'three_artifacts': common[common.index('two_qualifiers')] = 'four_artifacts'
    if p.get('metric') == 'own_gates': common += ['one_matching', 'three_matching']
    if p.get('metric') == 'own_counter_creatures': common += ['two_counters_one_creature', 'two_counted_creatures']
    if p.get('condition') == 'two_gates' or p.get('metric') == 'own_gates': common += ['nongate_land_control']
    if p.get('metric') == 'own_named_creatures': common += ['qualifier_removed']
    if p.get('condition') == 'own_black_permanent': common += ['noncreature_qualifier', 'colorless_symbol']
    if p.get('condition') == 'three_artifacts': common += ['two_artifacts', 'mixed_artifacts']
    if p.get('effect') == 'drain_target': common += ['self_target']
    if p['mechanism'] == 'entry_watch': common += ['enemy_entry', 'nonmatching_entry', 'source_removed', 'repeat_entry', 'no_other_owned_creatures']
    if p.get('entry_filter') in ('creature', 'artifact') and p['mechanism'] == 'entry_watch': common += ['artifact_creature_entry']
    if p['mechanism'] == 'land_watch': common += ['nonmatching_controller', 'nonland_entry', 'source_removed', 'repeat_land', 'same_turn_repeat', 'ability_snapshot', 'atlas_tapped', 'atlas_sick', 'empty_land_hand', 'decline_land', 'creature_land']
    if p['mechanism'] == 'entry_max_counters_draw': common += ['no_qualifier', 'enemy_only', 'boost_before_entry', 'late_creature']
    if p.get('optional'): common += ['decline']
    if p.get('legendary'): common.remove('two_copies')
    if not any(p['mana'][1:]): common.remove('wrong_color')
    return [v + '_' + str(owner) for owner in (0, 1) for v in common
            if (v != 'decline' or owner == 0)
            and (v != 'decline_land' or (owner if p['scope'] == 'own' else 1-owner) == 0)]


def validate_witnesses(o, rows):
    data = pins()
    for n, q in dict(data['witnesses'], **data['sources']).items():
        r = o.get(n, {})
        if (n not in rows or rows[n]['csvid'] != q['csvid'] or not exact_pin(pin(r), q['oracle'])
                or r.get('layout') != 'normal' or r.get('card_faces') or r.get('selected_face')
                or 'Token' in r.get('type_line', '')):
            raise ValueError('entry extension private pin changed: ' + n)
    for n in data['sources']:
        if not rows[n].get('handler') or rows[n]['handler'] in ('NULL', 'card_dummy'):
            raise ValueError('entry extension source is unimplemented: ' + n)


def expand(name, csvid, d, p, selected, rows, o):
    from recipes import card, step, observe, cost_from_oracle
    if csvid != p['csvid']:
        raise ValueError('entry extension source registry identity changed')
    result = []
    for variant in selected:
        v, owner_s = variant.rsplit('_', 1); owner = int(owner_s)
        mechanism = p['mechanism']; setup = []; entities = {}
        mana = [[10] * 6, [10] * 6]; life = [20, 20]
        zones = {z: [[], []] for z in ('hand', 'graveyard', 'exile', 'library')}
        exact = False

        def add(n, ref, pl, zone='battlefield', state=None):
            setup.append(dict(card(n, ref, pl, zone), **({'state': state} if state else {})))
            r = o[n]
            entities[ref] = dict(name=n, player=pl, alive=zone == 'battlefield', hand=zone == 'hand',
                                 creature='Creature' in r['type_line'], power=int(r.get('power', '0')),
                                 toughness=int(r.get('toughness', '0')), counters=0, damage=0,
                                 tapped=bool(state and 'tapped' in state))
            if zone != 'battlefield': zones[zone][pl].append(n)

        if v != 'bare_zones':
            for pl in (0, 1):
                for z, ns in [('graveyard', ['Forest', 'Hill Giant']), ('exile', ['Island', 'Grizzly Bears'])]:
                    for i, n in enumerate(ns): add(n, f'{z}_{pl}_{i}', pl, z)
        for pl in (0, 1):
            library = [] if v == 'empty_library' else list(BASE_LIBRARY[:1]) if v == 'short_library' else list(BASE_LIBRARY) * p.get('library_copies', 1)
            for i, n in enumerate(library): add(n, f'library_{pl}_{i}', pl, 'library')
        add(name, 'source', owner, 'hand')
        add('Grizzly Bears', 'enemy', 1-owner)
        if v not in ('no_qualifier', 'enemy_only', 'no_other_owned_creatures'):
            add('Grizzly Bears', 'ally', owner)
            add('Hill Giant', 'ally_other', owner)
        if not (p.get('metric') == 'own_artifacts' and v in ('no_qualifier', 'enemy_only')):
            add('Darksteel Relic', 'relic', owner)
        if p.get('metric') == 'own_artifacts': add('Darksteel Relic', 'enemy_relic', 1-owner)
        if p.get('metric') == 'own_green_creatures': add('Exploration', 'green_noncreature', owner)
        if p.get('metric') == 'own_black_devotion':
            if v not in ('no_qualifier', 'enemy_only'):
                add('Black Knight', 'black_creature', owner); add('Phyrexian Arena', 'black_enchantment', owner)
            add('Black Knight', 'enemy_black', 1-owner)
            if v != 'bare_zones':
                add('Black Knight', 'old_black_grave', owner, 'graveyard')
                add('Black Knight', 'old_black_exile', owner, 'exile')
        if p.get('metric') == 'own_creatures': add('Hill Giant', 'enemy_count_control', 1-owner)
        if p.get('metric') == 'own_green_devotion':
            if v not in ('no_qualifier', 'enemy_only'):
                add('Leatherback Baloth', 'green_symbols', owner); add('Exploration', 'green_enchantment', owner)
            add('Leatherback Baloth', 'enemy_green_symbols', 1-owner)
            if v != 'bare_zones': add('Leatherback Baloth', 'old_green_grave', owner, 'graveyard')
        if p.get('metric') == 'own_vampires':
            if v not in ('no_qualifier', 'enemy_only'): add('Vampire Lacerator', 'vampire', owner)
            add('Vampire Lacerator', 'enemy_vampire', 1-owner)
            if v != 'bare_zones': add('Vampire Lacerator', 'old_vampire', owner, 'graveyard')
        if p.get('condition') == 'two_gates' or p.get('metric') == 'own_gates':
            count = 0 if v in ('no_qualifier', 'enemy_only', 'nongate_land_control') else 1 if v in ('threshold_one', 'one_matching') else 3 if v == 'three_matching' else 2
            for i, nm in enumerate(('Boros Guildgate', 'Simic Guildgate', 'Azorius Guildgate')[:count]): add(nm, f'gate_{i}', owner, state=['tapped'])
            for i, nm in enumerate(('Boros Guildgate', 'Simic Guildgate')): add(nm, f'enemy_gate_{i}', 1-owner, state=['tapped'])
            if v == 'condition_removed': add('Boomerang', 'gate_bounce', 0, 'hand')
            if v == 'nongate_land_control':
                add('Forest', 'nongate_0', owner, state=['tapped']); add('Forest', 'nongate_1', owner, state=['tapped'])
        if p.get('condition') in ('own_enchantment', 'other_spirit'):
            qualifier = 'Exploration' if p['condition'] == 'own_enchantment' else 'Lantern Kami'
            if v not in ('no_qualifier', 'enemy_only'): add(qualifier, 'condition_qualifier', owner)
            if v == 'two_qualifiers': add(qualifier, 'condition_second', owner)
            add(qualifier, 'enemy_qualifier', 1-owner)
            if v == 'condition_removed': add('Boomerang', 'condition_bounce', 0, 'hand')
        named_entry = p.get('metric') == 'own_named_creatures' or p.get('condition') == 'another_named'
        if named_entry:
            if v != 'no_qualifier': add(name, 'qualifier', 1-owner if v == 'enemy_only' else owner, 'hand')
            if v != 'bare_zones':
                add(name, 'old_named_grave', owner, 'graveyard'); add(name, 'old_named_exile', owner, 'exile')
            if v in ('condition_removed', 'qualifier_removed'): add('Boomerang', 'name_bounce', 0, 'hand')
        if p.get('condition') == 'own_black_permanent':
            if v not in ('no_qualifier', 'enemy_only', 'colorless_symbol'):
                add('Phyrexian Arena' if v == 'noncreature_qualifier' else 'Black Knight', 'condition_qualifier', owner)
            if v == 'two_qualifiers': add('Phyrexian Arena', 'black_second', owner)
            add('Black Knight', 'enemy_black_qualifier', 1-owner)
            if v == 'colorless_symbol': add('Silent Skimmer', 'colorless_symbol', owner)
            if v != 'bare_zones':
                add('Black Knight', 'old_black_condition_grave', owner, 'graveyard')
                add('Black Knight', 'old_black_condition_exile', owner, 'exile')
            if v == 'condition_removed': add('Boomerang', 'condition_bounce', 0, 'hand')
        if p.get('condition') == 'three_artifacts':
            # Remove the ordinary single Relic when the authored board calls
            # for zero artifacts; all prior-schema boards stay untouched.
            if v in ('no_qualifier', 'enemy_only'):
                entities.pop('relic'); setup[:] = [x for x in setup if x['ref'] != 'relic']
            count = 0 if v in ('no_qualifier', 'enemy_only') else 1 if v == 'two_artifacts' else 3 if v == 'four_artifacts' else 2
            for i in range(count): add('Brass Man' if v == 'mixed_artifacts' else 'Darksteel Relic', f'artifact_qualifier_{i}', owner)
            for i in range(3): add('Darksteel Relic', f'enemy_artifact_{i}', 1-owner)
            if v == 'condition_removed': add('Boomerang', 'condition_bounce', 0, 'hand')
        if mechanism == 'entry_max_counters_draw':
            if v not in ('no_qualifier', 'enemy_only'): add('Ancient Crab', 'toughness_control', owner)
            add('Krosan Colossus', 'enemy_power_control', 1-owner)
            if v == 'boost_before_entry': add('Giant Growth', 'growth_spell', owner, 'hand')
        if v == 'two_copies': add(name, 'second', owner, 'hand')
        if v in ('blink', 'blink_snapshot'): add('Cloudshift', 'blink_spell', owner, 'hand')
        if v in ('bounce_only', 'bounce_recast', 'source_removed', 'counter_removed'):
            add('Unsummon', 'bounce_spell', 0, 'hand')
        if v == 'countered': add('Essence Scatter', 'counter_spell', 0, 'hand')
        counter_model = mechanism == 'counter_condition_draw' or p.get('metric') == 'own_counter_creatures'
        qualifying_counter = counter_model and v not in ('no_qualifier', 'late_counter')
        if qualifying_counter or v == 'late_counter': add('Arbor Armament', 'armament', owner, 'hand')
        if v in ('two_counters_one_creature', 'two_counted_creatures'): add('Arbor Armament', 'armament_second', owner, 'hand')
        event = (mechanism in ('other_entry_life', 'entry_watch') or v == 'late_creature') and not (p.get('optional') and v == 'two_copies') and v != 'no_other_owned_creatures'
        if event:
            event_owner = 1-owner if v == 'enemy_entry' else owner
            event_name = 'Terra Stomper' if mechanism == 'entry_max_counters_draw' else p.get('event_card', 'Grizzly Bears')
            if v == 'noncreature': event_name = 'Darksteel Relic'
            if v == 'nonmatching_entry': event_name = 'Darksteel Relic' if p['entry_filter'] == 'creature' else 'Grizzly Bears'
            if v == 'artifact_creature_entry': event_name = 'Brass Man'
            add(event_name, 'event', event_owner, 'hand')
            if v in ('repeat_entry', 'source_removed'): add(p.get('second_event_card', 'Hill Giant'), 'event_second', owner, 'hand')
        if v in ('block_ground', 'block_flying'):
            add('Air Elemental' if v == 'block_flying' else p.get('ground_attacker', 'Hill Giant'), 'attacker', 1-owner)
        if mechanism == 'land_watch':
            land_owner = owner if p['scope'] == 'own' else 1-owner
            if v == 'nonmatching_controller': land_owner = 1-land_owner
            add('Walking Atlas', 'atlas', land_owner, state=['tapped'] if v == 'atlas_tapped' else ['sick'] if v == 'atlas_sick' else None)
            if v != 'empty_land_hand': add('Dryad Arbor' if v == 'creature_land' else 'Forest', 'land', land_owner, 'hand')
            if v in ('repeat_land', 'same_turn_repeat', 'source_removed'): add('Forest', 'land_second', land_owner, 'hand')
            if v in ('same_turn_repeat', 'source_removed'): add('Twiddle', 'untap_spell', 0, 'hand')
            if v == 'nonland_entry': add('Grizzly Bears', 'nonland', land_owner, 'hand')
        universe = sorted({e['name'] for e in entities.values()})

        def pay(n, pl):
            cost = [0] * 6 if o[n]['mana_cost'] == '{0}' else cost_from_oracle(o[n]); remaining = cost[0]
            for c in range(1, 6): mana[pl][c] -= cost[c]
            for c in range(6):
                paid = min(remaining, mana[pl][c]); mana[pl][c] -= paid; remaining -= paid
            assert remaining == 0 and min(mana[pl]) >= 0

        def checks():
            seq = [step('assert_stack', value=0), step('assert_choices_consumed')]
            for ref, e in entities.items():
                if e['alive']:
                    seq += [observe(ref, 'in_play', 1), observe(ref, 'tapped', int(e['tapped']))]
                    if e['creature']:
                        seq += [observe(ref, 'power', e['power'] + e['counters']),
                                observe(ref, 'toughness', e['toughness'] + e['counters']),
                                observe(ref, 'damage', e['damage']), observe(ref, 'plus1_counters', e['counters'])]
                    if ref == 'colorless_symbol': seq += [step('assert_matching_permanents', player=e['player'], index=rows[e['name']]['csvid'], property='color', value=0)]
                    if p.get('self_type_receipt') and e['name'] == name:
                        seq += [step('assert_matching_permanents', player=e['player'], index=csvid, property='artifact', value=int('Artifact' in o[name]['type_line']))]
            if named_entry:
                # Re-entry can reallocate an indistinguishable copy. Observe
                # every named permanent and its exact multiplicity, rather
                # than relying on a first-name binding for both copies.
                for pl in (0, 1):
                    copies = [e for e in entities.values() if e['alive'] and e['player'] == pl and e['name'] == name]
                    seq.append(step('assert_zone_multiplicity', player=pl, zone='battlefield', index=csvid, value=len(copies)))
                    if copies:
                        properties = dict(power=copies[0]['power'], toughness=copies[0]['toughness'], damage=copies[0]['damage'], tapped=int(copies[0]['tapped']))
                        assert all((e['power'],e['toughness'],e['damage'],e['tapped']) == (copies[0]['power'],copies[0]['toughness'],copies[0]['damage'],copies[0]['tapped']) for e in copies)
                        seq += [step('assert_matching_permanents', player=pl, index=csvid, property=k, value=value) for k, value in properties.items()]
            for pl in (0, 1):
                seq += [step('assert_life', player=pl, value=life[pl]),
                        step('assert_poison', player=pl, value=0),
                        step('assert_mana_total', player=pl, value=sum(mana[pl])),
                        step('assert_zone', player=pl, zone='battlefield', value=sum(e['alive'] and e['player'] == pl for e in entities.values()))]
                if exact: seq += [step('assert_mana', player=pl, color=c, value=n) for c, n in enumerate(mana[pl])]
                for z in zones:
                    ns = zones[z][pl]; seq.append(step('assert_zone', player=pl, zone=z, value=len(ns)))
                    seq += [step('assert_zone_multiplicity', player=pl, zone=z, index=rows[n]['csvid'], value=ns.count(n)) for n in universe]
                    if z == 'library' and (pl == 0 or mechanism != 'scry'):
                        seq += [step('assert_zone_card', player=pl, zone=z, index=i, value=rows[n]['csvid']) for i, n in enumerate(ns)]
            return seq

        def move(ref, zone):
            e = entities[ref]; e['alive'] = False; e['hand'] = zone == 'hand'
            e['counters'] = e['damage'] = 0; e['tapped'] = False
            zones[zone][e['player']].append(e['name'])

        def entry_effect(ref):
            e = entities[ref]; pl = e['player']; extra = []
            if mechanism == 'other_entry_life' and e['creature']:
                for key, watcher in entities.items():
                    if (key != ref and watcher['alive'] and watcher['name'] == name
                            and (p.get('scope', 'any') == 'any' or pl == watcher['player'])):
                        accept = not (v == 'decline' and owner == 0)
                        if accept:
                            if p.get('optional') and watcher['player'] == 0: extra.append(step('choose_trigger', player=0, ref=key))
                            life[watcher['player']] += p['amount']
            if mechanism == 'entry_watch':
                rule = p['entry_filter']; record = o[e['name']]
                match = (e['creature'] if rule == 'creature' else
                         'Enchantment' in record['type_line'] if rule == 'enchantment' else
                         'Artifact' in record['type_line'] if rule == 'artifact' else
                         e['creature'] and 'Flying' in record['keywords'] if rule == 'flying' else
                         e['creature'] and rule in record['type_line'])
                if match:
                    watchers = [(key, x) for key, x in entities.items() if x['alive'] and x['name'] == name
                                and (key != ref or p['include_self']) and (p['scope'] == 'any' or pl == x['player'])]
                    for key, watcher in watchers:
                        recipient = watcher['player']
                        # Human acceptance names a unique source. Decline when
                        # identical optional watchers would be ambiguous.
                        if p.get('optional') and recipient == 0:
                            if v == 'decline' or len(watchers) > 1: continue
                            extra.append(step('choose_trigger', player=0, ref=key))
                        amount = p['amount'] * (e[p['quantity']] + e['counters'] if p['quantity'] in ('toughness', 'power') else 1)
                        if p['effect'] == 'gain': life[recipient] += amount
                        elif p['effect'] == 'drain': life[1-recipient] -= amount; life[recipient] += amount
                        elif p['effect'] == 'draw':
                            assert amount <= len(zones['library'][recipient])
                            zones['hand'][recipient] += zones['library'][recipient][:amount]; del zones['library'][recipient][:amount]
                        else: raise ValueError('unreviewed watcher receipt')
            if mechanism == 'land_watch' and e['alive'] and 'Land' in o[e['name']]['type_line']:
                watchers = [(key,x) for key,x in entities.items() if x['alive'] and x['name'] == name
                            and ((pl == x['player']) if p['scope'] == 'own' else (pl != x['player']))]
                accept = not (p['optional'] and owner == 0 and (v == 'decline' or len(watchers) > 1))
                if accept:
                    for key,watcher in watchers:
                        recipient = watcher['player']; amount = p['amount']
                        if p['optional'] and recipient == 0: extra.append(step('choose_trigger', player=0, ref=key))
                        life[recipient] += amount
                        if p['effect'] == 'gain_draw':
                            assert zones['library'][recipient]
                            zones['hand'][recipient].append(zones['library'][recipient].pop(0))
            if e['name'] != name: return extra
            if mechanism == 'entry_max_counters_draw':
                e['counters'] = max((x['power'] + x['counters'] for key,x in entities.items() if key != ref and x['alive'] and x['creature'] and x['player'] == pl), default=0)
                amount = e['power'] + e['counters']; assert amount <= len(zones['library'][pl])
                zones['hand'][pl] += zones['library'][pl][:amount]; del zones['library'][pl][:amount]
            elif mechanism == 'other_creature_life':
                life[pl] += p['amount'] * sum(x['alive'] and x['creature'] and x['player'] == pl and key != ref for key, x in entities.items())
            elif mechanism == 'counter_condition_draw':
                if any(x['alive'] and x['creature'] and x['player'] == pl and x['counters'] for x in entities.values()):
                    assert zones['library'][pl]
                    zones['hand'][pl].append(zones['library'][pl].pop(0))
            elif mechanism in ('counted_entry', 'conditional_entry'):
                own = [x for x in entities.values() if x['alive'] and x['player'] == pl]
                amount = 0
                if mechanism == 'counted_entry':
                    metric = p['metric']
                    if metric == 'own_artifacts': amount = sum('Artifact' in o[x['name']]['type_line'] for x in own)
                    elif metric == 'own_green_creatures': amount = sum(x['creature'] and 'G' in o[x['name']]['colors'] for x in own)
                    elif metric == 'own_black_devotion': amount = sum(o[x['name']]['mana_cost'].count('{B}') for x in own)
                    elif metric == 'own_creatures': amount = sum(x['creature'] for x in own)
                    elif metric == 'own_green_devotion': amount = sum(o[x['name']]['mana_cost'].count('{G}') for x in own)
                    elif metric == 'own_vampires': amount = sum(x['creature'] and 'Vampire' in o[x['name']]['type_line'] for x in own)
                    elif metric == 'own_gates': amount = sum('Land' in o[x['name']]['type_line'] and 'Gate' in o[x['name']]['type_line'] for x in own)
                    elif metric == 'own_counter_creatures': amount = sum(x['creature'] and x['counters'] > 0 for x in own)
                    elif metric == 'own_named_creatures': amount = sum(x['creature'] and x['name'] == name for x in own)
                    else: raise ValueError('unreviewed counting metric')
                    amount *= p['amount']
                else:
                    condition = p['condition']
                    if condition == 'two_gates': active = sum('Land' in o[x['name']]['type_line'] and 'Gate' in o[x['name']]['type_line'] for x in own) >= 2
                    elif condition == 'own_enchantment': active = any('Enchantment' in o[x['name']]['type_line'] for x in own)
                    elif condition == 'other_spirit': active = any(x is not e and 'Spirit' in o[x['name']]['type_line'] for x in own)
                    elif condition == 'another_named': active = any(x is not e and x['creature'] and x['name'] == name for x in own)
                    elif condition == 'own_black_permanent': active = any('B' in o[x['name']]['colors'] for x in own)
                    elif condition == 'three_artifacts': active = sum('Artifact' in o[x['name']]['type_line'] for x in own) >= 3
                    else: raise ValueError('unreviewed entry condition')
                    if active and not (v == 'decline' and owner == 0):
                        amount = p['amount']
                        if p.get('optional') and pl == 0: extra.append(step('choose_trigger', player=0, ref=ref))
                if p['effect'] == 'gain': life[pl] += amount
                elif p['effect'] == 'drain': life[1-pl] -= amount; life[pl] += amount
                elif p['effect'] == 'drain_target':
                    if amount:
                        recipient = pl if v == 'self_target' else 1-pl
                        extra.append(step('choose_player', player=pl, value=recipient))
                        life[recipient] -= amount; life[pl] += amount
                elif p['effect'] in ('draw', 'draw_lose'):
                    assert amount <= len(zones['library'][pl])
                    zones['hand'][pl] += zones['library'][pl][:amount]; del zones['library'][pl][:amount]
                    if p['effect'] == 'draw_lose': life[pl] -= amount
                else: raise ValueError('unreviewed entry value')
            elif mechanism == 'scry':
                count = min(p['amount'], len(zones['library'][pl]))
                extra += [step('record_library', player=pl)]
                if pl == 0 and count:
                    if v in ('keep', 'reorder'):
                        extra.append(step('choose_zone', player=pl, value=-1))
                        if count > 1:
                            if v == 'reorder':
                                extra.append(step('choose_zone', player=pl, value=1))
                                ns = zones['library'][pl]; ns[0], ns[1] = ns[1], ns[0]
                            extra.append(step('choose_zone', player=pl, value=-1))
                    else:
                        extra += [step('choose_zone', player=pl, value=0) for _ in range(count)]
                        ns = zones['library'][pl]; zones['library'][pl] = ns[count:] + ns[:count]
                elif pl == 1 and count:
                    # The real AI tutor may offer an optional selector even
                    # with no matching cards. Decline when offered, without
                    # requiring the callback to exist or forcing an action.
                    extra.append(step('choose_optional_zone', player=pl, value=rows[zones['library'][pl][0]]['csvid']))
                # AI reordering retains ordinary policy; the independent
                # postcondition checks legal scry permutations/conservation.
            return extra

        def cast_creature(ref, no_trigger_probe=False):
            e = entities[ref]; pl = e['player']; n = e['name']; before = list(mana[pl])
            casting_cost = [0] * 6 if o[n]['mana_cost'] == '{0}' else cost_from_oracle(o[n])
            pay(n, pl)
            # Generic costs beyond the floating colorless pool use real pool
            # clicks for human0. Specific colored costs remain automatic.
            generic_choices = [step('choose_mana', player=pl, color=c) for c in range(1, 6)
                               for _ in range(before[c] - casting_cost[c] - mana[pl][c])] if pl == 0 else []
            probe = [step('choose_trigger', player=0, ref='source')] if no_trigger_probe else []
            seq = [step('turn', player=pl, value=0)] + generic_choices + probe + [step('cast', ref=ref, value=1),
                   step('assert_stack', value=1), observe(ref, 'in_hand', 0)]
            seq += [step('assert_mana_total', player=pl, value=sum(mana[pl])),
                    step('assert_zone', player=pl, zone='hand', value=len(zones['hand'][pl])),
                    step('assert_zone_multiplicity', player=pl, zone='hand', index=rows[n]['csvid'], value=zones['hand'][pl].count(n))]
            zones['hand'][pl].remove(n); e['hand'] = False; e['alive'] = True
            seq += entry_effect(ref) + [step('resolve')]
            if no_trigger_probe: seq.append(step('assert_unused_choices', value=1))
            elif mechanism == 'entry_watch' and p.get('optional') and owner == 0:
                # A printed legal optional trigger intent must be consumed.
                # Assert and clear at the resolved boundary so a missing
                # prompt/effect is observable without a terminal harness error.
                seq.append(step('assert_unused_choices', value=0))
            seq += [step('assert_choices_consumed'), step('refresh')]
            if n == name and mechanism == 'scry': seq += [step('assert_scry_result', player=pl, value=p['amount']), step('assert_forced_zone_choices', value=0)]
            return seq

        def spell(ref, target):
            e = entities[ref]; n = e['name']; pl = e['player']; pay(n, pl)
            zones['hand'][pl].remove(n); zones['graveyard'][pl].append(n); e['hand'] = False
            return [step('turn', player=pl, value=0), step('choose_target', player=pl, ref=target)] + ([step('choose_dialog', player=pl, value=1)] if n == 'Twiddle' else []) + [
                    step('cast', ref=ref, value=1), step('assert_stack', value=1), step('resolve'), step('assert_choices_consumed')]

        def land_entry(ref='land'):
            atlas = entities['atlas']; denied = v in ('atlas_tapped', 'atlas_sick')
            seq = [step('turn', player=land_owner, value=0)]
            if v == 'atlas_sick': seq.append(observe('atlas', 'sick', 1))
            if v == 'atlas_tapped': seq.append(observe('atlas', 'tapped', 1))
            seq += [step('require_can_activate', ref='atlas', value=int(not denied)),
                   step('activate', ref='atlas', value=int(not denied))]
            if denied: return seq + [step('assert_stack', value=0)], None
            atlas['tapped'] = True
            seq += [step('assert_stack', value=1), observe('atlas', 'tapped', 1)]
            put = v not in ('decline_land', 'empty_land_hand')
            if ref in entities: seq.append(observe(ref, 'in_hand', 1))
            nm = entities[ref]['name'] if ref in entities else 'Forest'
            seq.append(step('choose_zone_card' if put else 'choose_optional_zone', player=land_owner, value=rows[nm]['csvid']))
            matching = put and entities['source']['alive'] and ((land_owner == owner) if p['scope'] == 'own' else (land_owner != owner))
            negative_probe = p['optional'] and owner == 0 and entities['source']['alive'] and not matching
            if put:
                e = entities[ref]; zones['hand'][land_owner].remove(nm); e['alive'] = True; e['hand'] = False
                seq += entry_effect(ref)
            if negative_probe: seq.append(step('choose_trigger', player=0, ref='source'))
            pending = len(seq)
            seq += [step('resolve')]
            if negative_probe: seq.append(step('assert_unused_choices', value=1))
            seq += [step('assert_choices_consumed'), step('refresh'), step('assert_forced_zone_choices', value=0)]
            if put:
                ordinal = sum(x['alive'] and x['player'] == land_owner and x['name'] == nm for x in entities.values())-1
                seq.append(step('bind_permanent_copy', player=land_owner, value=rows[nm]['csvid'], index=ordinal, ref=ref))
            return seq, pending

        pre = [step('turn', player=owner, value=0)] + checks(); actions = []
        if v == 'boost_before_entry':
            actions += spell('growth_spell', 'ally'); entities['ally']['power'] += 3; entities['ally']['toughness'] += 3
            actions += checks()
        if named_entry and 'qualifier' in entities:
            actions += cast_creature('qualifier') + checks()
        if v in ('condition_removed', 'qualifier_removed') and named_entry:
            actions += spell('name_bounce', 'qualifier'); move('qualifier', 'hand')
            # This copy is not used again. Exact hand/grave/exile conservation
            # verifies the return without ambiguously rebinding identical names.
            actions += checks()
        if qualifying_counter:
            target = 'enemy' if v in ('enemy_counter', 'enemy_only') else 'ally'
            actions += spell('armament', target); entities[target]['counters'] += 1
            actions += checks()
            if v in ('two_counters_one_creature', 'two_counted_creatures'):
                target = 'ally' if v == 'two_counters_one_creature' else 'ally_other'
                actions += spell('armament_second', target); entities[target]['counters'] += 1
                actions += checks()
        if v == 'counter_removed':
            actions += spell('bounce_spell', 'ally'); move('ally', 'hand')
            actions += [step('bind_hand', ref='ally', player=owner, value=rows['Grizzly Bears']['csvid'])] + checks()
        if v == 'condition_removed' and not named_entry:
            target = 'gate_0' if p.get('condition') == 'two_gates' else 'artifact_qualifier_0' if p.get('condition') == 'three_artifacts' else 'condition_qualifier'
            actions += spell('gate_bounce' if target == 'gate_0' else 'condition_bounce', target); move(target, 'hand')
            if p.get('condition') != 'three_artifacts': actions += [step('bind_hand', ref=target, player=owner, value=rows[entities[target]['name']]['csvid'])]
            actions += checks()
        if v in ('payment', 'insufficient_mana', 'wrong_color'):
            exact = True; mana[owner] = list(p['mana']) if v != 'insufficient_mana' else [0] * 6
            if v == 'wrong_color':
                c = next(c for c in range(1, 6) if mana[owner][c]); mana[owner][1 if c != 1 else 2] += mana[owner][c]; mana[owner][c] = 0
            actions += [step('set_mana', player=owner, color=c, value=n) for c, n in enumerate(mana[owner])]
        cut = len(actions)
        if v in ('insufficient_mana', 'wrong_color'):
            actions += [step('cast', ref='source', value=0)]
        elif v == 'countered':
            pay(name, owner); pay('Essence Scatter', 0)
            zones['hand'][owner].remove(name); zones['graveyard'][owner].append(name); entities['source']['hand'] = False
            zones['hand'][0].remove('Essence Scatter'); zones['graveyard'][0].append('Essence Scatter'); entities['counter_spell']['hand'] = False
            actions += [step('turn', player=owner, value=0), step('choose_priority', player=0, ref='counter_spell'),
                        step('cast', ref='source', value=0), step('resolve'), step('assert_choices_consumed')]
        else:
            source_actions = cast_creature('source'); cut += next(i for i, s in enumerate(source_actions) if s['op'] in ('record_library', 'resolve'))
            actions += source_actions + checks()
            if v == 'two_copies': actions += cast_creature('second')
            if v == 'source_removed' and event: actions += cast_creature('event') + checks()
            if v == 'source_removed' and mechanism == 'land_watch':
                first_land, _ = land_entry(); actions += first_land + checks()
            if v in ('bounce_recast', 'bounce_only', 'source_removed'):
                target = 'source'
                actions += spell('bounce_spell', target); move(target, 'hand')
                actions += [step('bind_hand', ref=target, player=entities[target]['player'], value=rows[entities[target]['name']]['csvid'])]
                if v == 'bounce_recast': actions += cast_creature('source')
            if v in ('blink', 'blink_snapshot'):
                actions += spell('blink_spell', 'source')[:-2]
                # The source leaves and re-enters during real Cloudshift resolution.
                e = entities['source']; e['counters'] = e['damage'] = 0; e['tapped'] = False
                cut = len(actions)
                binding = step('bind_permanent_copy', player=owner, value=csvid, index=0, ref='source') if named_entry else step('bind_permanent', player=owner, value=csvid, ref='source')
                # Named copies are symmetric for all later observations. The
                # chosen representative performs no later game action; all
                # copies are independently inspected below. The real blink
                # target was committed before this resolution/capture.
                actions += entry_effect('source') + [step('resolve')]
                if mechanism == 'entry_watch' and p.get('optional') and owner == 0:
                    actions.append(step('assert_unused_choices', value=0))
                actions += [step('assert_choices_consumed'), binding]
                if mechanism == 'scry': actions += [step('assert_scry_result', player=owner, value=p['amount']), step('assert_forced_zone_choices', value=0)]
            if event:
                if v == 'payment':
                    # Observe the exact source payment above before separately
                    # funding the actual creature whose entry tests the watcher.
                    event_pl = entities['event']['player']; event_record = o[entities['event']['name']]
                    mana[event_pl] = [0]*6 if event_record['mana_cost'] == '{0}' else cost_from_oracle(event_record)
                    actions += [step('set_mana', player=event_pl, color=c, value=n) for c, n in enumerate(mana[event_pl])]
                actions += cast_creature('event_second' if v == 'source_removed' else 'event')
                if v == 'repeat_entry': actions += checks() + cast_creature('event_second')
            if mechanism == 'land_watch':
                if v == 'source_removed':
                    actions += spell('untap_spell', 'atlas'); entities['atlas']['tapped'] = False
                    second_land, _ = land_entry('land_second'); actions += second_land
                elif v == 'nonland_entry':
                    actions += cast_creature('nonland', no_trigger_probe=p['optional'] and owner == 0)
                else:
                    first, pending = land_entry(); start = len(actions); actions += first
                    if v == 'ability_snapshot': cut = start + pending
                    if v in ('repeat_land', 'same_turn_repeat'):
                        actions += checks()
                        if v == 'same_turn_repeat':
                            actions += spell('untap_spell', 'atlas'); entities['atlas']['tapped'] = False
                        else:
                            mana = [[0]*6,[0]*6]
                            actions += [step('set_mana',player=pl,color=c,value=0) for pl in (0,1) for c in range(6)]
                            actions += [step('advance_turn'), step('untap_step',player=1-land_owner),
                                        step('advance_turn'), step('untap_step',player=land_owner), step('turn',player=land_owner,value=0)]
                            for e in entities.values():
                                if e['alive']: e['tapped'] = False
                        second, _ = land_entry('land_second'); actions += second
            if v == 'destroy_other': actions += [step('destroy', ref='ally')]; move('ally', 'graveyard')
            if v == 'late_counter': actions += spell('armament', 'ally'); entities['ally']['counters'] += 1
            if v == 'cleanup':
                # The lifecycle helper runs cleanup/end-turn effects while
                # deliberately retaining the fixture's floating mana pools.
                actions.append(step('cleanup'))
            if v == 'fresh_attack':
                actions += [step('turn', player=owner, value=0), step('begin_attackers'), step('attack', ref='source', value=0), step('finish_attackers')]
            if v in ('block_ground', 'block_flying'):
                a = entities['attacker']; source = entities['source']; blocked = v == 'block_ground' or 'Flying' in p['oracle']['keywords']
                actions += [step('turn', player=1-owner, value=0), step('begin_attackers'), step('attack', ref='attacker', value=1),
                            step('finish_attackers'), step('begin_blockers'), step('block', ref='source', other='attacker', value=int(blocked)),
                            step('advance_combat'), step('combat_damage'), step('resolve'), step('assert_choices_consumed')]
                a['tapped'] = True
                if blocked:
                    source['damage'] = a['power'] + a['counters']; a['damage'] = source['power'] + source['counters']
                    for ref in ('source', 'attacker'):
                        e = entities[ref]
                        if e['damage'] >= e['toughness'] + e['counters']: move(ref, 'graveyard')
                else: life[owner] -= a['power']
        tail_checks = checks(); seq = pre + actions + tail_checks
        if v in ('ready_snapshot', 'pending_snapshot', 'blink_snapshot', 'ability_snapshot'):
            k = 0 if v == 'ready_snapshot' else cut
            tail = actions[k:] + tail_checks
            seq = pre + actions[:k] + [step('capture')] + copy.deepcopy(tail) + [step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(tail) + [step('assert_recorded_state')]
        if v == 'resolved_snapshot': seq += [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore'), step('assert_recorded_state')]
        gaps = ['control changes', 'replacement effects', 'multiplayer', 'trigger ordering',
                'changes between entry and separate trigger resolution']
        if mechanism == 'scry' and owner == 1: gaps += ['AI chooses legal scry policy; human-only bottom/reorder forcing']
        if mechanism == 'counter_condition_draw': gaps += ['noncreature counter exclusion', 'pending entry predicate changes']
        if mechanism == 'other_entry_life': gaps += ['simultaneous creature entries', 'token creature entry']
        if mechanism == 'entry_watch': gaps += ['simultaneous entries', 'token entries', 'live entry characteristic changes']
        if mechanism == 'land_watch': gaps += ['simultaneous land entries', 'land-play quota and play timing', 'legendary duplicate-source interaction']
        if mechanism == 'land_watch' and p['optional']: gaps += ['human acceptance of multiple same-name optional watchers; the multiple-copy case declines', 'AI optional decline is not forced']
        if mechanism == 'entry_max_counters_draw': gaps += ['legendary duplicate-source interaction', 'power changes between entry counters and separate draw-trigger resolution', 'Krosan Colossus morph and idle witness combat keywords', 'Giant Growth expiration in the separate boosted-entry case']
        if p.get('optional'): gaps += ['one entry seen by multiple same-name optional watchers', 'AI optional decline is not forced']
        if p.get('metric') == 'own_black_devotion': gaps += ['hybrid and Phyrexian devotion symbols', 'Phyrexian Arena upkeep and idle Black Knight combat abilities']
        if p.get('metric') == 'own_green_creatures': gaps += ['live color changes', 'Exploration additional-land permission']
        if p.get('metric') == 'own_green_devotion': gaps += ['hybrid and Phyrexian devotion symbols', 'Exploration additional-land permission']
        if p.get('metric') == 'own_vampires': gaps += ['Vampire Lacerator upkeep; no upkeep is executed', 'live creature-type changes']
        if named_entry: gaps += ['copy and name-changing effects', 'live creature-type changes']
        if p.get('condition') == 'own_black_permanent': gaps += ['live color changes', 'Phyrexian Arena upkeep and idle qualifier combat/attack abilities']
        if p.get('condition') == 'three_artifacts': gaps += ['live artifact type changes', 'Brass Man untap ability; no untap step is executed']
        gaps += p.get('gaps', [])
        result.append(dict(name=f'EntryValueExtensionRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                           setup=setup, steps=seq, _recipe=dict(family=p['family'], version=2, variant=variant,
                           params=d['params'], oracle_text=d['oracle_text'], untested_components=gaps)))
    return result
