"""Pinned blocking restrictions exercised through real combat declarations."""
import re
from creature_recipes import KEYWORDS, REMINDERS, BASE
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

WITNESSES = {
    'flying_zero': ('Ornithopter', 0, 2, ['Flying'], 'Flying'),
    'ground_one': ('Muck Rats', 1, 1, [], ''),
    'ground_two': ('Grizzly Bears', 2, 2, [], ''),
    'ground_three': ('Hill Giant', 3, 3, [], ''),
    'ground_four': ('Giant Cockroach', 4, 2, [], ''),
    'flying_one': ('Suntail Hawk', 1, 1, ['Flying'], 'Flying'),
    'flying_two': ('Wind Drake', 2, 2, ['Flying'], 'Flying'),
    'flying_three': ('Snapping Drake', 3, 2, ['Flying'], "Flying (This creature can't be blocked except by creatures with flying or reach.)"),
    'flying_four': ('Air Elemental', 4, 4, ['Flying'], 'Flying'),
    'reach': ('Giant Spider', 2, 4, ['Reach'], 'Reach (This creature can block creatures with flying.)'),
}
SPELLS = {
    'growth': ('Giant Growth', '{G}', 'Target creature gets +3/+3 until end of turn.'),
    'shrink': ('Disfigure', '{B}', 'Target creature gets -2/-2 until end of turn.'),
}


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line', '') or declaration.get('oracle_text') != record.get('oracle_text')
            or 'variants' not in declaration):
        raise ValueError('blocking requires a pinned normal single-face creature and reviewed variants')
    p = declaration.get('params')
    if not isinstance(p, dict) or set(p) != {'keywords', 'restriction', 'power_limit'}:
        raise ValueError('explicit blocking keywords, restriction and power limit required')
    mode = p['restriction']; limit = p['power_limit']
    if (mode not in ('none', 'flying_only', 'power_at_least', 'power_at_most') or type(limit) is not int
            or limit != 0 and mode in ('none', 'flying_only')
            or mode.startswith('power_') and not 1 <= limit <= 4):
        raise ValueError('unsupported blocking restriction or power boundary')
    clause = {'none': "can't block.", 'flying_only': 'can block only creatures with flying.',
              'power_at_least': f"can't block creatures with power {limit} or greater.",
              'power_at_most': f"can't block creatures with power {limit} or less."}[mode]
    expected = {ref+' '+clause for ref in ('This creature', record['name'])}
    rules = []; keys = []; abilities = []; canonical = {k.lower():k for k in KEYWORDS}
    for line in record['oracle_text'].splitlines():
        if re.match(r'^(?:\{[^}]+\}|Sacrifice |Discard |Remove |Exile |Pay |Tap |Untap |Return )', line) and ':' in line:
            abilities.append(line); continue
        if line in expected: rules.append(line); continue
        text = line
        for reminder in re.findall(r'\([^\n]*?\)', line):
            if reminder not in REMINDERS | EXTRA_REMINDERS: raise ValueError('unreviewed blocking reminder')
            text = text.replace(reminder, '')
        tokens = [k.strip().lower() for k in text.strip().split(',')]
        if any(k not in canonical for k in tokens): raise ValueError('unreviewed passive or triggered blocking clause')
        keys += [canonical[k] for k in tokens]
    if (len(rules) != 1 or p['keywords'] != sorted(keys) or len(set(keys)) != len(keys)
            or set(keys) & {'Menace', 'Deathtouch', 'Trample'}):
        raise ValueError('blocking parameters must match reviewed passive text')
    for field in ('power', 'toughness'):
        if not re.fullmatch(r'\d+', record.get(field, '')) or not 0 <= int(record[field]) <= 20:
            raise ValueError('bounded numeric blocking creature stats required')
    if int(record['toughness']) == 0: raise ValueError('blocker must survive entry')
    return dict(p, family='blocking_creature', power=int(record['power']), toughness=int(record['toughness']),
                mana=cost_from_oracle(record), remaining_abilities=abilities)


def variants(p):
    result = [v for v in BASE if v not in ('block_ground', 'block_flying', 'tapped_blocker')]
    result += ['defend_'+w+'_'+str(owner) for w in WITNESSES for owner in (0, 1)]
    result += ['tapped_blocker', 'sick_blocker', 'combat_snapshot', 'cleanup_blocking',
               'growth_before_block', 'shrink_before_block', 'damage_0', 'damage_1', 'empty_zones']
    if any(p['mana'][1:]): result.append('wrong_color')
    return result


def validate_witnesses(oracle, registry):
    for name, power, toughness, keys, text in WITNESSES.values():
        r = oracle.get(name, {})
        if (name not in registry or r.get('layout') != 'normal' or r.get('card_faces')
                or (r.get('power'), r.get('toughness'), r.get('oracle_text'), sorted(r.get('keywords', [])))
                != (str(power), str(toughness), text, sorted(keys))):
            raise ValueError('blocking combat witness changed: '+name)
    for name, cost, text in SPELLS.values():
        if name not in registry or (oracle.get(name, {}).get('mana_cost'), oracle.get(name, {}).get('oracle_text')) != (cost, text):
            raise ValueError('blocking spell witness changed: '+name)


def may_block(p, power, flying):
    if flying and not set(p['keywords']) & {'Flying', 'Reach'}: return False
    mode = p['restriction']
    return (mode != 'none' and (mode != 'flying_only' or flying)
            and (mode != 'power_at_least' or power < p['power_limit'])
            and (mode != 'power_at_most' or power > p['power_limit']))


def expand(name, csvid, declaration, p, selected, registry, oracle):
    from recipes import card, step, observe, exact_mana
    from creature_recipes import expand as creatures
    base = set(BASE)-{'block_ground', 'block_flying', 'tapped_blocker'}
    result = creatures(name, csvid, declaration, p, [v for v in selected if v in base], registry)
    for v in (v for v in selected if v not in base):
        owner = int(v.endswith('_1')); witness = v[len('defend_'):-2] if v.startswith('defend_') else 'ground_three'
        if v == 'growth_before_block': witness = 'ground_one'
        if v in ('sick_blocker', 'tapped_blocker'): witness = 'flying_four'
        wn, wp, wt, wk, _ = WITNESSES[witness]
        fixture = 'empty_main' if v == 'empty_zones' else 'stocked_zones'
        hands = [list(HAND) if fixture == 'stocked_zones' else [] for _ in (0, 1)]
        graves = [list(GRAVE) if fixture == 'stocked_zones' else [] for _ in (0, 1)]
        battle = [1, 1]; mana = [60, 60]; life = [20, 20]
        setup = [card(name, 'source', owner), card(wn, 'attacker', 1-owner)]
        pre = []; run = []; extra = []
        if v == 'wrong_color':
            setup[0]['zone'] = 'hand'; hands[owner].append(name); battle[owner] = 0
            pool = list(p['mana']); c = next(i for i in range(1, 6) if pool[i]); pool[1 if c != 1 else 2] += pool[c]; pool[c] = 0
            pre += exact_mana(pool); mana[owner] = sum(pool)
            run += [step('cast', ref='source', value=0), observe('source', 'in_hand', 1), observe('source', 'in_play', 0)]
        else:
            if v == 'tapped_blocker': setup[0]['state'] = ['tapped']
            if v == 'sick_blocker': setup[0]['state'] = ['sick']
            if v == 'cleanup_blocking': pre.append(step('cleanup'))
            run += [step('turn', player=1-owner, value=0), step('begin_attackers'), step('attack', ref='attacker', value=1),
                    observe('attacker', 'attacking', 1), step('finish_attackers')]
            if v in ('growth_before_block', 'shrink_before_block'):
                spell, _, _ = SPELLS['growth' if v == 'growth_before_block' else 'shrink']
                setup.append(card(spell, 'response', owner, 'hand'))
                delta = 3 if v == 'growth_before_block' else -2; wp += delta; wt += delta
                mana[owner] -= 1; graves[owner].append(spell)
                run += [step('choose_target', player=owner, ref='attacker'), step('cast', ref='response', value=1),
                        step('assert_stack', value=1), step('resolve'), observe('attacker', 'power', wp), observe('attacker', 'toughness', wt)]
            legal = int(v != 'tapped_blocker' and may_block(p, wp, 'Flying' in wk))
            run += [step('begin_blockers'), step('block', ref='source', other='attacker', value=legal),
                    observe('source', 'blocking_count', legal), step('advance_combat'), observe('attacker', 'blocked', legal)]
            if v.startswith('damage_'):
                power = p['power']; first = bool(set(p['keywords']) & {'First strike', 'Double strike'})
                hits = 0 if not legal else 1 + int('Double strike' in p['keywords'] and power < wt)
                incoming = wp if legal and not (first and power >= wt) else 0
                source_alive = incoming < p['toughness']; attacker_alive = not legal or power*hits < wt
                if not legal: life[owner] -= wp
                if legal and 'Lifelink' in p['keywords']: life[owner] += power*hits
                if not source_alive: graves[owner].append(name); battle[owner] -= 1
                if not attacker_alive: graves[1-owner].append(wn); battle[1-owner] -= 1
                run += [step('combat_damage'), observe('source', 'in_play', int(source_alive)), observe('attacker', 'in_play', int(attacker_alive))]
                if source_alive: run.append(observe('source', 'damage', incoming))
                if attacker_alive: run.append(observe('attacker', 'damage', power*hits))
            else:
                extra += [observe('source', 'power', p['power']), observe('source', 'toughness', p['toughness']),
                          observe('source', 'damage', 0), observe('attacker', 'damage', 0),
                          observe('source', 'tapped', int(v == 'tapped_blocker'))]
        checks = [step('assert_stack', value=0), step('assert_choices_consumed')] + extra
        for player in (0, 1):
            checks += [step('assert_life', player=player, value=life[player]), step('assert_mana_total', player=player, value=mana[player]),
                       step('assert_zone', player=player, zone='battlefield', value=battle[player])]
            for zone, names in (('hand', hands[player]), ('graveyard', graves[player]),
                                ('library', LIBRARY if fixture == 'stocked_zones' else []), ('exile', EXILE if fixture == 'stocked_zones' else [])):
                checks.append(step('assert_zone', player=player, zone=zone, value=len(names)))
                checks += [step('assert_zone_card', player=player, zone=zone, index=i, value=registry[n]['csvid']) if zone == 'library'
                           else step('assert_zone_contains', player=player, zone=zone, value=registry[n]['csvid']) for i, n in enumerate(names)]
        actions = pre + run + checks
        if v == 'combat_snapshot':
            # Capture after genuine attacker declarations, before blockers.
            cut = next(i for i, s in enumerate(run) if s['op'] == 'begin_blockers')
            actions = pre+run[:cut]+[step('capture')]+run[cut:]+checks+[step('record_state'), step('ai_roundtrip'), step('restore')]+run[cut:]+checks+[step('assert_recorded_state')]
        result.append(dict(name=f'BlockingCreatureRecipe_{csvid}_{v}', card=name, fixture=fixture, seed=123,
                           setup=setup, steps=actions, _recipe=dict(family='blocking_creature', version=2, variant=v,
                           params=declaration['params'], oracle_text=declaration['oracle_text'], untested_abilities=p['remaining_abilities'],
                           untested_keywords=['Flash'] if 'Flash' in p['keywords'] else [])))
    return result
