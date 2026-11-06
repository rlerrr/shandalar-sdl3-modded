"""Fixed single-target creature entry damage through real casts and re-entry."""
import json
import re
from collections import Counter
from pathlib import Path

from creature_recipes import KEYWORDS, REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

PINS = json.loads((Path(__file__).parent / 'data/etb_damage_witnesses.json').read_text(encoding='utf8'))['cards']
SCOPES = {'any': 'any target', 'creature': 'target creature',
          'flying_creature': 'target creature with flying',
          'player': 'target player or planeswalker', 'opponent': 'target opponent or planeswalker'}


def spec(d, r):
    from recipes import cost_from_oracle
    if (not r or r.get('layout') != 'normal' or r.get('card_faces') or r.get('selected_face')
            or 'Creature' not in r.get('type_line', '') or d.get('oracle_text') != r.get('oracle_text')
            or 'variants' not in d):
        raise ValueError('entry damage requires a pinned normal creature and explicit variants')
    p = d.get('params')
    if not isinstance(p, dict) or set(p) != {'amount', 'targets', 'damage_you', 'keywords', 'inactive_rules'}:
        raise ValueError('explicit damage, target scope, collateral damage and remaining rules required')
    if (type(p['amount']) is not int or not 1 <= p['amount'] <= 4 or p['targets'] not in SCOPES
            or type(p['damage_you']) is not int or p['damage_you'] not in (0, 1)
            or p['damage_you'] and p['targets'] != 'creature'):
        raise ValueError('reviewed bounded fixed entry damage required')
    if not isinstance(p['keywords'], list) or p['keywords'] != sorted(set(p['keywords'])) or set(p['keywords']) - {'Flying', 'Haste', 'Trample', 'Reach', 'First strike'}:
        raise ValueError('reviewed passive source keywords required')
    inactive = p['inactive_rules']
    if not isinstance(inactive, list) or any(not isinstance(s, str) for s in inactive) or len(set(inactive)) != len(inactive):
        raise ValueError('distinct printed inactive rules required')
    metadata = set(p['keywords'])
    for line in inactive:
        # This component casts the creature; it never cycles or advances upkeep.
        # Unreviewed triggers and activated abilities are deliberately rejected.
        if re.fullmatch(r'Cycling (\{(?:[1-9WUBRGC]|[WUBRG]/[WUBRG])\})+ \(.+Discard this card: Draw a card\.\)', line):
            metadata.add('Cycling')
        else:
            raise ValueError('unreviewed inactive rule')
    suffix = ' and 1 damage to you.' if p['damage_you'] else '.'
    expected = {f'When {ref} enters{ending}, {actor} deals {p["amount"]} damage to {SCOPES[p["targets"]]}{suffix}'
                for ref in ('this creature', r['name'], r['name'].split(',')[0])
                for ending in ('', ' the battlefield') for actor in ('it', r['name'], r['name'].split(',')[0])}
    triggers = []; keys = []; seen = []; canonical = {k.lower(): k for k in KEYWORDS}
    for line in r['oracle_text'].splitlines():
        if line in expected:
            triggers.append(line); continue
        if line in inactive:
            seen.append(line); continue
        text = line
        for reminder in re.findall(r'\([^\n]*?\)', line):
            if reminder not in REMINDERS:
                raise ValueError('unreviewed entry damage reminder')
            text = text.replace(reminder, '')
        for word in text.strip().split(','):
            if word.strip().lower() not in canonical:
                raise ValueError('unreviewed passive, activated ability or extra trigger')
            keys.append(canonical[word.strip().lower()])
    if len(triggers) != 1 or sorted(keys) != p['keywords'] or set(seen) != set(inactive) or set(r.get('keywords', [])) != metadata:
        raise ValueError('entry parameters must match complete Oracle text and metadata')
    for k in ('power', 'toughness'):
        if not re.fullmatch(r'\d+', r.get(k, '')) or not 0 <= int(r[k]) <= 10:
            raise ValueError('bounded numeric entry source stats required')
    if not int(r['toughness']):
        raise ValueError('entry source must survive ordinary entry')
    return dict(p, family='etb_damage_creature', mana=cost_from_oracle(r),
                power=int(r['power']), toughness=int(r['toughness']), legendary='Legendary' in r['type_line'])


def variants(p):
    modes = ['entry', 'payment', 'insufficient_mana', 'bare_zones', 'countered', 'cleanup',
             'bounce_only', 'bounce_recast', 'blink', 'destroy_source', 'exile_source',
             'other_creature', 'other_death', 'ready_snapshot', 'pending_snapshot',
             'resolved_snapshot', 'blink_snapshot']
    if any(p['mana'][1:]):
        modes.append('wrong_color')
    if not p['legendary']:
        modes.append('two_sources')
    if p['targets'] in ('any', 'creature', 'flying_creature'):
        modes += ['friendly_target', 'lethal', 'indestructible']
        if p['targets'] != 'flying_creature' or 'Flying' in p['keywords']:
            modes += ['self_target', 'only_source']
        else:
            modes += ['only_source', 'no_flying']
    if p['targets'] in ('any', 'player'):
        modes += ['own_player', 'opponent_player']
    return [v + '_' + str(owner) for owner in (0, 1) for v in modes]


def validate_witnesses(o, rows):
    if not isinstance(o, dict):
        raise ValueError('entry damage requires pinned witness Oracle context')
    for name, pin in PINS.items():
        if name not in rows or any(o.get(name, {}).get(k) != v for k, v in pin.items()):
            raise ValueError('entry damage witness changed: ' + name)


def expand(name, csvid, d, p, selected, rows, o):
    from recipes import card, step, observe, cost_from_oracle
    result = []
    for variant in selected:
        v, owner = variant.rsplit('_', 1); owner = int(owner)
        bare = v == 'bare_zones'
        zones = {z: [list(ns) if not bare else [] for _ in (0, 1)]
                 for z, ns in (('hand', HAND), ('graveyard', GRAVE), ('exile', EXILE), ('library', LIBRARY))}
        setup = []; entities = {}; mana = [[10] * 6, [10] * 6]; life = [20, 20]
        pre = [step('turn', player=owner, value=0)]; actions = []; pending_cut = None
        is_creature_target = p['targets'] in ('any', 'creature', 'flying_creature') and v not in ('own_player', 'opponent_player')
        player_target = owner if v == 'own_player' else 1 - owner

        def add(nm, ref, player, zone='battlefield'):
            setup.append(card(nm, ref, player, zone))
            entities[ref] = dict(name=nm, player=player, alive=zone == 'battlefield', hand=zone == 'hand', damage=0)
            if zone != 'battlefield':
                zones[zone][player].append(nm)

        def pay(nm, player):
            mana[player] = [a - b for a, b in zip(mana[player], cost_from_oracle(o[nm]))]

        def leave(ref, zone):
            e = entities[ref]; e.update(alive=False, hand=zone == 'hand', damage=0)
            zones[zone][e['player']].append(e['name'])

        add(name, 'source', owner, 'hand')
        target = None
        if is_creature_target:
            if v in ('only_source', 'self_target'):
                if p['targets'] != 'flying_creature' or 'Flying' in p['keywords']:
                    target = 'source'
                if v == 'self_target':
                    # Choosing the source among other legal creatures differs
                    # from having to choose it as the only legal target.
                    add('Darksteel Gargoyle', 'unselected', owner)
                    add('Scaled Wurm', 'unselected_enemy', 1 - owner)
            elif v == 'no_flying':
                add('Scaled Wurm', 'ground', 1 - owner)
                add('Darksteel Relic', 'artifact', 1 - owner)
            else:
                nm = ('Suntail Hawk' if p['targets'] == 'flying_creature' else 'Savannah Lions') if v == 'lethal' else 'Darksteel Gargoyle' if v in ('indestructible', 'blink', 'blink_snapshot', 'bounce_recast', 'two_sources') else 'Air Elemental' if p['targets'] == 'flying_creature' else 'Scaled Wurm'
                add(nm, 'recipient', owner if v == 'friendly_target' else 1 - owner)
                add('Darksteel Gargoyle', 'unselected', owner)
                add('Darksteel Relic', 'artifact', 1 - owner)
                if p['targets'] == 'flying_creature':
                    add('Scaled Wurm', 'ground', 1 - owner)
                target = 'recipient'
        else:
            add('Scaled Wurm', 'unselected', 1 - owner)
            add('Darksteel Relic', 'artifact', owner)

        def choices():
            if is_creature_target:
                return [step('choose_target', player=owner, ref=target)] if target else []
            return [step('choose_player', player=owner, value=player_target)]

        def damage():
            if is_creature_target:
                if target is None:
                    return  # No legal target: the mandatory trigger has no effect.
                e = entities[target]; e['damage'] += p['amount']
                r = o[e['name']]
                if e['damage'] >= int(r['toughness']) and 'Indestructible' not in r.get('keywords', []):
                    leave(target, 'graveyard')
            else:
                life[player_target] -= p['amount']
            life[owner] -= p['damage_you']

        def entry(ref='source'):
            e = entities[ref]; e.update(alive=True, hand=False, damage=0)
            pay(name, owner); zones['hand'][owner].remove(name)
            pending = [observe(ref, 'in_play', 0)] + [step('assert_life', player=player, value=life[player]) for player in (0, 1)]
            if is_creature_target and target and target != ref:
                pending.append(observe(target, 'damage', entities[target]['damage']))
            seq = [step('cast', ref=ref, value=1), step('assert_stack', value=1)] + pending + choices() + [step('resolve'), step('assert_choices_consumed')]
            damage()
            return seq

        def helper(nm, ref, recipient, caster):
            # Helpers enter the initial hand but leave it through normal casting.
            setup.append(card(nm, ref, caster, 'hand')); pay(nm, caster); zones['graveyard'][caster].append(nm)
            return [step('choose_target', player=caster, ref=recipient), step('cast', ref=ref, value=1),
                    step('assert_stack', value=1), step('resolve'), step('assert_choices_consumed')]

        if v in ('payment', 'insufficient_mana', 'wrong_color'):
            mana[owner] = list(p['mana']) if v != 'insufficient_mana' else [0] * 6
            if v == 'wrong_color':
                mana[owner] = [0] * 6
                unused = next(c for c in range(1, 6) if not p['mana'][c])
                mana[owner][unused] = sum(p['mana'])
            pre += [step('set_mana', player=owner, color=c, value=n) for c, n in enumerate(mana[owner])]
        if v in ('insufficient_mana', 'wrong_color'):
            actions += [step('cast', ref='source', value=0), step('assert_choices_consumed')]
        elif v == 'countered':
            setup.append(card('Essence Scatter', 'counter', 0, 'hand'))
            pay(name, owner); pay('Essence Scatter', 0); zones['hand'][owner].remove(name)
            zones['graveyard'][owner].append(name); zones['graveyard'][0].append('Essence Scatter')
            entities['source']['hand'] = False
            actions += [step('choose_priority', player=0, ref='counter'), step('cast', ref='source', value=0),
                        step('resolve'), step('assert_choices_consumed')]
        else:
            actions += entry()
            pending_cut = next(i for i, s in enumerate(actions) if s['op'] in ('choose_target', 'choose_player', 'resolve'))
            if v in ('bounce_only', 'bounce_recast'):
                actions += helper('Unsummon', 'bounce', 'source', 0)
                leave('source', 'hand')
                actions.append(step('bind_hand', player=owner, value=csvid, ref='source'))
                if v == 'bounce_recast':
                    actions += entry()
            if v in ('blink', 'blink_snapshot'):
                seq = helper('Cloudshift', 'blink', 'source', owner)
                # Cloudshift's cast target is consumed first. Entry targeting is
                # chosen only when Cloudshift returns the source to the battlefield.
                cut = next(i for i, s in enumerate(seq) if s['op'] == 'resolve')
                seq[cut:cut] = choices()
                actions += seq + [step('bind_permanent', player=owner, value=csvid, ref='source')]
                entities['source']['damage'] = 0; damage()
                if v == 'blink_snapshot':
                    pending_cut = len(actions) - len(seq) - 1 + cut
            if v == 'destroy_source':
                actions += [step('destroy', ref='source')]; leave('source', 'graveyard')
            if v == 'exile_source':
                actions += helper('Swords to Plowshares', 'exile', 'source', 0)
                leave('source', 'exile'); life[owner] += p['power']
            if v in ('other_creature', 'other_death'):
                add('Grizzly Bears', 'other', owner, 'hand'); pay('Grizzly Bears', owner)
                zones['hand'][owner].remove('Grizzly Bears'); entities['other'].update(alive=True, hand=False)
                actions += [step('cast', ref='other', value=1), step('resolve'), step('assert_choices_consumed')]
                if v == 'other_death':
                    actions.append(step('destroy', ref='other')); leave('other', 'graveyard')
            if v == 'two_sources':
                add(name, 'second', owner, 'hand'); actions += entry('second')
            if v == 'cleanup':
                # Healing the damage at cleanup must not hide a missing entry
                # effect. Observe the printed damage before it is removed.
                if is_creature_target and target:
                    actions += [observe(target, 'in_play', 1), observe(target, 'damage', p['amount'])]
                actions.append(step('cleanup'))
                for e in entities.values():
                    e['damage'] = 0
        checks = [step('assert_stack', value=0), step('assert_choices_consumed')]
        for ref, e in entities.items():
            checks += [observe(ref, 'in_play', int(e['alive'])), observe(ref, 'in_hand', int(e['hand']))]
            if e['alive']:
                r = o[e['name']]
                checks.append(observe(ref, 'tapped', 0))
                if 'Creature' in r['type_line']:
                    checks += [observe(ref, 'power', int(r['power'])), observe(ref, 'toughness', int(r['toughness'])),
                               observe(ref, 'damage', e['damage']), observe(ref, 'plus1_counters', 0), observe(ref, 'minus1_counters', 0)]
        universe = set(PINS) | {name} | set(HAND + GRAVE + EXILE + LIBRARY)
        for player in (0, 1):
            checks += [step('assert_life', player=player, value=life[player]), step('assert_poison', player=player, value=0),
                       step('assert_zone', player=player, zone='battlefield', value=sum(e['alive'] and e['player'] == player for e in entities.values()))]
            if v != 'cleanup':
                checks += [step('assert_mana', player=player, color=c, value=n) for c, n in enumerate(mana[player])]
            for zone, by_player in zones.items():
                ns = by_player[player]; counts = Counter(ns)
                checks.append(step('assert_zone', player=player, zone=zone, value=len(ns)))
                if zone == 'library':
                    checks += [step('assert_zone_card', player=player, zone=zone, index=i, value=rows[nm]['csvid']) for i, nm in enumerate(ns)]
                else:
                    checks += [step('assert_zone_multiplicity', player=player, zone=zone, index=rows[nm]['csvid'], value=counts[nm]) for nm in sorted(universe)]
        seq = pre + actions + checks
        if v in ('ready_snapshot', 'pending_snapshot', 'blink_snapshot'):
            cut = 0 if v == 'ready_snapshot' else pending_cut
            seq = pre + actions[:cut] + [step('capture')] + actions[cut:] + checks + [step('record_state'), step('ai_roundtrip'), step('restore')] + actions[cut:] + checks + [step('assert_recorded_state')]
        if v == 'resolved_snapshot':
            seq += [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore'), step('assert_recorded_state')]
        result.append(dict(name=f'EtbDamageCreatureRecipe_{csvid}_{variant}', card=name, fixture='empty_main' if bare else 'stocked_zones',
                           seed=123, setup=setup, steps=seq, _recipe=dict(family=p['family'], version=2, variant=variant,
                           params=d['params'], oracle_text=d['oracle_text'], untested_components=p['keywords'] + p['inactive_rules'] +
                           ['planeswalker targets', 'target leaves after trigger targeting', 'simultaneous trigger ordering',
                            'protection or replacement effects', 'control changes', 'combat-trigger interactions', 'multiplayer'])))
    return result
