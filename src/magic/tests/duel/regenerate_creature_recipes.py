"""Mana-only self regeneration, with explicit documented legacy timing."""
import re
from creature_recipes import KEYWORDS, REMINDERS, BASE
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

TIMING = 'pre_sixth_destruction_window'
REGEN_REMINDER = '(The next time this creature would be destroyed this turn, instead tap it, remove it from combat, and heal all damage on it.)'
WITNESSES = {
    'Air Elemental': ('{3}{U}{U}', 'Flying', '4', '4'),
    'Scaled Wurm': ('{7}{G}', '', '7', '6'),
    'Giant Growth': ('{G}', 'Target creature gets +3/+3 until end of turn.'),
    'Lightning Bolt': ('{R}', 'Lightning Bolt deals 3 damage to any target.'),
    'Shock': ('{R}', 'Shock deals 2 damage to any target.'),
    'Terminate': ('{B}{R}', "Destroy target creature. It can't be regenerated."),
    'Last Gasp': ('{1}{B}', 'Target creature gets -3/-3 until end of turn.'),
}


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line', '') or declaration.get('oracle_text') != record.get('oracle_text')
            or 'variants' not in declaration):
        raise ValueError('regeneration requires a pinned normal single-face creature and reviewed variants')
    p = declaration.get('params')
    if not isinstance(p, dict) or set(p) != {'keywords', 'activation_cost', 'timing'} or p['timing'] != TIMING:
        raise ValueError('explicit keywords, regeneration cost and legacy timing required')
    cost = p['activation_cost']
    if not isinstance(cost, str) or not re.fullmatch(r'(?:\{[1-9BUGRW]\})+', cost):
        raise ValueError('regeneration component supports ordinary mana-only costs')
    expected = [cost + ': Regenerate ' + ref + '.' for ref in ('this creature', record['name'], record['name'].split(',')[0])]
    keys = []; abilities = []; canonical = {k.lower(): k for k in KEYWORDS + ('Hexproof',)}
    for line in record['oracle_text'].splitlines():
        text = line
        for reminder in re.findall(r'\([^\n]*?\)', line):
            if reminder not in REMINDERS | EXTRA_REMINDERS | {REGEN_REMINDER}:
                raise ValueError('unreviewed regeneration reminder')
            text = text.replace(reminder, '')
        text = text.strip()
        if ':' in text:
            abilities.append(text); continue
        tokens = [t.strip().lower() for t in text.split(',')]
        if any(t not in canonical for t in tokens):
            raise ValueError('unreviewed regeneration passive or triggered clause')
        keys.extend(canonical[t] for t in tokens)
    # Scryfall also tags reminder-text verbs Heal and Regenerate; neither is
    # an additional unconditional keyword or a license for additional rules.
    if (len(abilities) != 1 or abilities[0] not in expected or p['keywords'] != sorted(keys)
            or len(set(keys)) != len(keys) or set(keys) != set(record.get('keywords', [])) - {'Heal', 'Regenerate'}
            or set(keys) & {'Menace', 'Deathtouch', 'Double strike'}):
        raise ValueError('whole-card regeneration scope does not match explicit parameters')
    for field in ('power', 'toughness'):
        if not re.fullmatch(r'\d+', record.get(field, '')) or not 0 <= int(record[field]) <= 9:
            raise ValueError('bounded numeric regeneration stats required')
    if int(record['toughness']) == 0:
        raise ValueError('regenerator must survive entry')
    return dict(family='regenerate_creature', keywords=sorted(keys), power=int(record['power']),
                toughness=int(record['toughness']), mana=cost_from_oracle(record),
                activation_mana=cost_from_oracle(dict(mana_cost=cost)), timing=TIMING)


def variants(p):
    result = list(BASE) + ['legacy_timing', 'destroy', 'regeneration_payment', 'sick', 'tapped', 'tapped_sick',
        'opposing_turn', 'repeat', 'spent_mana', 'insufficient', 'no_response', 'regeneration_snapshot',
        'cleanup_reuse', 'combat_block', 'lethal_damage', 'cannot_regenerate']
    if any(p['activation_mana'][1:]): result.append('wrong_color')
    if 'Defender' not in p['keywords']: result.append('combat_attack')
    if p['toughness'] > 2: result += ['nonlethal_damage', 'heal_existing_damage']
    if p['toughness'] <= 3: result.append('zero_toughness')
    return result


def validate_witnesses(oracle, registry):
    for name, fields in WITNESSES.items():
        r = oracle.get(name, {})
        actual = tuple(r.get(k) for k in ('mana_cost', 'oracle_text', 'power', 'toughness')[:len(fields)])
        if name not in registry or r.get('layout') != 'normal' or r.get('card_faces') or actual != fields:
            raise ValueError('regeneration witness changed: ' + name)


def expand(name, csvid, declaration, p, selected, registry, oracle):
    from recipes import card, step, observe, exact_mana, cost_from_oracle
    from creature_recipes import expand as creatures
    result = creatures(name, csvid, declaration, p, [v for v in selected if v in BASE], registry)
    for v in (v for v in selected if v not in BASE):
        keys = set(p['keywords']); cost = p['activation_mana']; mana = [[10]*6, [10]*6]
        setup = [card(name, 'source')]; pre = []; actions = []; extra = []
        hands = [list(HAND), list(HAND)]; graves = [list(GRAVE), list(GRAVE)]; battle = [1, 0]
        living = v not in ('spent_mana', 'insufficient', 'wrong_color', 'no_response', 'cannot_regenerate', 'zero_toughness')
        count = 0; damage = 0; tapped = int(v in ('tapped', 'tapped_sick'))
        if v in ('sick', 'tapped_sick'): setup[0]['state'] = ['sick']
        if tapped: setup[0].setdefault('state', []).append('tapped')
        if v in ('regeneration_payment', 'spent_mana'): mana[0] = list(cost)
        if v == 'insufficient': mana[0] = [0]*6
        if v == 'wrong_color':
            mana[0] = list(cost); c = next(i for i in range(1, 6) if cost[i]); mana[0][c] = 0
            mana[0][1 if c != 1 else 2] += cost[c]
        if v in ('regeneration_payment', 'spent_mana', 'insufficient', 'wrong_color'): pre += exact_mana(mana[0])
        if v == 'opposing_turn': pre.append(step('turn', player=1, value=0))

        def regenerate():
            nonlocal count, tapped
            count += 1; tapped = 1
            return [step('choose_regeneration', ref='source')]

        def cast_witness(witness, target, ref):
            setup.append(card(witness, ref, zone='hand'))
            hands[0].append(witness); hands[0].remove(witness); graves[0].append(witness)
            wc = cost_from_oracle(oracle[witness]); mana[0][:] = [mana[0][i]-wc[i] for i in range(6)]
            return [step('choose_target', player=0, ref=target), step('cast', ref=ref, value=1), step('assert_stack', value=1)]

        if v == 'legacy_timing': actions += [step('can_activate', ref='source', value=0), step('activate', ref='source', value=0)]
        elif v in ('combat_block', 'combat_attack'):
            setup.append(card('Scaled Wurm' if v == 'combat_block' else 'Air Elemental', 'witness', 1)); battle[1] = 1
            # Two legal Giant Growth casts make a 13/12 ground attacker or
            # 10/10 flying blocker. Both survive first strike and are lethal
            # without introducing trample, evasion failures or death triggers.
            for i in range(2): actions += cast_witness('Giant Growth', 'witness', 'growth'+str(i)) + [step('resolve')]
            extra += [observe('witness', 'in_play', 1), observe('witness', 'damage', p['power'])]
            if v == 'combat_block':
                actions += [step('turn', player=1, value=0), step('begin_attackers'), step('attack', ref='witness', value=1),
                            step('finish_attackers'), step('begin_blockers'), step('block', ref='source', other='witness', value=1)]
                extra.append(observe('source', 'blocking_count', 0))
            else:
                actions += [step('begin_attackers'), step('attack', ref='source', value=1), step('finish_attackers'),
                            step('begin_blockers'), step('block', ref='witness', other='source', value=1)]
                extra.append(observe('source', 'attacking', 0))
            actions += [step('advance_combat')] + regenerate() + [step('combat_damage')]
        elif v in ('lethal_damage', 'regeneration_snapshot'):
            for i in range((p['toughness']+2)//3):
                actions += cast_witness('Lightning Bolt', 'source', 'bolt'+str(i))
                if i == (p['toughness']+2)//3-1: actions += regenerate()
                actions.append(step('resolve'))
        elif v in ('nonlethal_damage', 'heal_existing_damage'):
            actions += cast_witness('Shock', 'source', 'shock') + [step('resolve'), observe('source', 'damage', 2)]
            if v == 'heal_existing_damage': actions += regenerate() + [step('destroy', ref='source')]
            else: damage = 2
        elif v in ('cannot_regenerate', 'zero_toughness'):
            actions += cast_witness('Terminate' if v == 'cannot_regenerate' else 'Last Gasp', 'source', 'fatal') + [step('resolve')]
        else:
            if v not in ('insufficient', 'wrong_color', 'no_response'): actions += regenerate()
            actions.append(step('destroy', ref='source'))
            if v in ('repeat', 'spent_mana', 'cleanup_reuse'):
                actions += [observe('source', 'in_play', 1), observe('source', 'damage', 0), observe('source', 'tapped', 1)]
                if v == 'cleanup_reuse': actions += [step('cleanup'), step('turn', player=1, value=0)]
                if v != 'spent_mana': actions += regenerate()
                actions.append(step('destroy', ref='source'))
        mana[0] = [mana[0][i]-count*cost[i] for i in range(6)]
        if not living: graves[0].append(name); battle[0] = 0
        checks = [step('assert_choices_consumed'), step('assert_stack', value=0), observe('source', 'in_play', int(living))]
        if living:
            checks += [observe('source', 'power', p['power']), observe('source', 'toughness', p['toughness']),
                       observe('source', 'damage', damage), observe('source', 'tapped', tapped)] + extra
        for player in (0, 1):
            checks += [step('assert_life', player=player, value=20), step('assert_zone', player=player, zone='battlefield', value=battle[player]),
                       step('assert_mana_total', player=player, value=sum(mana[player]))]
            checks += [step('assert_mana', player=player, color=i, value=mana[player][i]) for i in range(6)]
            for zone, names in (('hand', hands[player]), ('graveyard', graves[player]), ('exile', EXILE), ('library', LIBRARY)):
                checks.append(step('assert_zone', player=player, zone=zone, value=len(names)))
                for i, n in enumerate(names):
                    checks.append(step('assert_zone_card', player=player, zone=zone, index=i, value=registry[n]['csvid']) if zone == 'library'
                                  else step('assert_zone_contains', player=player, zone=zone, value=registry[n]['csvid']))
        if v == 'regeneration_snapshot':
            # Capture a real pending lethal spell and the still-unconsumed
            # destruction-window decision. Replay must preserve both the
            # stack and the choice's timing constraint across AI backups.
            steps = pre + actions[:-1] + [step('capture')] + actions[-1:] + checks + [step('record_state'), step('ai_roundtrip'), step('restore')] + actions[-1:] + checks + [step('assert_recorded_state')]
        else: steps = pre + actions + checks
        result.append(dict(name=f'RegenerateCreatureRecipe_{csvid}_{v}', card=name, fixture='stocked_zones', seed=123,
                           setup=setup, steps=steps, _recipe=dict(family='regenerate_creature', version=2, variant=v,
                           params=declaration['params'], oracle_text=declaration['oracle_text'], legacy_exception=TIMING,
                           untested_keywords=[k for k in ('Flash', 'Hexproof') if k in keys])))
    return result
