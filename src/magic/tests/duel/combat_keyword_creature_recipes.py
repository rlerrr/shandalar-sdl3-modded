"""Pinned bushido, flanking and exalted components through complete combat declarations."""
import re
from creature_recipes import KEYWORDS, REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS, protection_values, protects, mirror_cast_cases
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

BASE = ('cast', 'payment', 'insufficient_mana', 'snapshot', 'fresh_attack')
REMINDER = {
    'bushido': '(Whenever this creature blocks or becomes blocked, it gets +{n}/+{n} until end of turn.)',
    'flanking': '(Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)',
    'exalted': '(Whenever a creature you control attacks alone, that creature gets +1/+1 until end of turn.)',
}
WITNESSES = {
    'Hill Giant': ('3', '3', [], ''),
    'Air Elemental': ('4', '4', ['Flying'], 'Flying'),
    'Suntail Hawk': ('1', '1', ['Flying'], 'Flying'),
    'Benalish Cavalry': ('2', '2', ['Flanking'], 'Flanking '+REMINDER['flanking']),
    'Akrasan Squire': ('1', '1', ['Exalted'], 'Exalted '+REMINDER['exalted']),
    'Muck Rats': ('1', '1', [], ''),
    'Colossus of Akros': ('10','10',['Defender','Indestructible','Monstrosity'],"Defender, indestructible\n{10}: Monstrosity 10. (If this creature isn't monstrous, put ten +1/+1 counters on it and it becomes monstrous.)\nAs long as this creature is monstrous, it has trample and can attack as though it didn't have defender."),
}


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line', '') or declaration.get('oracle_text') != record.get('oracle_text')
            or 'variants' not in declaration):
        raise ValueError('combat keyword requires a pinned normal single-face creature and reviewed variants')
    p = declaration.get('params')
    if not isinstance(p, dict) or not {'keywords','mechanic','amount'} <= set(p) or set(p)-{'keywords','mechanic','amount','protection'}:
        raise ValueError('explicit combat keywords, mechanic and amount required')
    mode = p['mechanic']; n = p['amount']
    if mode not in REMINDER or type(n) is not int or not 1 <= n <= 5 or mode != 'bushido' and n != 1:
        raise ValueError('unsupported combat keyword or bonus')
    clause = mode.title() + (f' {n}' if mode == 'bushido' else '')
    expected = {clause, clause+' '+REMINDER[mode].format(n=n)}
    rules = []; keys = []; abilities = []; protection=[]; canonical = {k.lower():k for k in KEYWORDS+('Indestructible',)}
    for line in record['oracle_text'].splitlines():
        if re.match(r'^(?:\{[^}]+\}|Sacrifice |Discard |Remove |Exile |Pay |Tap |Untap |Return )', line) and ':' in line:
            abilities.append(line); continue
        if line in expected: rules.append(line); continue
        text = line
        for reminder in re.findall(r'\([^\n]*?\)', line):
            if reminder not in REMINDERS | EXTRA_REMINDERS: raise ValueError('unreviewed combat keyword reminder')
            text = text.replace(reminder, '')
        if text.strip().startswith('Protection from '):
            protection.extend(protection_values(text.strip()[len('Protection from '):]));continue
        tokens = [k.strip().lower() for k in text.strip().split(',')]
        if any(k not in canonical for k in tokens): raise ValueError('unreviewed passive or triggered combat clause')
        keys += [canonical[k] for k in tokens]
    if (len(rules) != 1 or p['keywords'] != sorted(keys) or len(set(keys)) != len(keys)
            or set(keys) & {'Menace', 'Deathtouch', 'Double strike', 'Lifelink'}
            or mode != 'exalted' and 'Trample' in keys
            or not set(keys+[mode.title()]+(['Protection'] if protection else [])) <= set(record.get('keywords', []))
            or p.get('protection',[])!=sorted(protection) or len(set(protection))!=len(protection)
            or 'protection' in p and not protection):
        raise ValueError('combat parameters must match reviewed passive text and structured keywords')
    for field in ('power', 'toughness'):
        if not re.fullmatch(r'\d+', record.get(field, '')) or not 0 <= int(record[field]) <= 20:
            raise ValueError('bounded numeric combat creature stats required')
    if int(record['toughness']) == 0: raise ValueError('creature must survive entry')
    return dict(p, protection=sorted(protection), family='combat_keyword_creature', power=int(record['power']), toughness=int(record['toughness']),
                mana=cost_from_oracle(record), remaining_abilities=abilities, legendary='Legendary' in record['type_line'])


def variants(p):
    result = list(BASE)
    result += [v+'_'+str(owner) for owner in (0, 1) for v in
               ('attack_alone', 'attack_pair', 'other_alone', 'other_pair', 'defend', 'defend_ground', 'defend_damage', 'defend_cleanup', 'tapped_defend', 'sick_defend',
                'tapped_source', 'sick_source', 'two_sources', 'combat_snapshot', 'cleanup', 'damage')]
    if 'Defender' not in p['keywords']:
        result += ['blocked', 'blocked_damage', 'two_blockers', 'flanking_blocker', 'small_blocker']
    else:
        result = [v for v in result if not v.startswith('attack_pair_')]
    if p['legendary']: result = [v for v in result if not v.startswith('two_sources_')]
    if p['mechanic'] == 'exalted': result += ['source_bounced_before', 'source_bounced_after', 'multiple_attackers_then_bounce']
    result.append('empty_zones')
    if any(p['mana'][1:]): result.append('wrong_color')
    if 'Indestructible' in p['keywords'] and p['mechanic']=='bushido' and 'Defender' not in p['keywords']:result.append('lethal_blocker')
    result += [v+'_1' for v in BASE]
    return result


def validate_witnesses(oracle, registry):
    for name, fields in WITNESSES.items():
        r = oracle.get(name, {})
        if (name not in registry or r.get('layout') != 'normal' or r.get('card_faces')
                or (r.get('power'), r.get('toughness'), sorted(r.get('keywords', [])), r.get('oracle_text'))
                != (fields[0], fields[1], sorted(fields[2]), fields[3])):
            raise ValueError('combat keyword witness changed: '+name)
    if ('Unsummon' not in registry or (oracle.get('Unsummon', {}).get('mana_cost'), oracle.get('Unsummon', {}).get('oracle_text'))
            != ('{U}', "Return target creature to its owner's hand.")):
        raise ValueError('combat keyword bounce witness changed')


def expand(name, csvid, declaration, p, selected, registry, oracle):
    from recipes import card, step, observe, exact_mana
    from creature_recipes import expand as creatures
    base=creatures(name,csvid,declaration,p,[v for v in BASE if v in selected or v+'_1' in selected],registry)
    result=[c for c in base if c['_recipe']['variant'] in selected]+mirror_cast_cases(base,selected)
    mode = p['mechanic']; keys = set(p['keywords']); power = p['power']; tough = p['toughness']; n = p['amount']
    for variant in (v for v in selected if v not in BASE):
        if variant.endswith('_1') and variant[:-2] in BASE:continue
        owner = int(variant.endswith('_1')); v = variant[:-2] if variant.endswith(('_0', '_1')) else variant
        fixture = 'empty_main' if v == 'empty_zones' else 'stocked_zones'
        hands = [list(HAND) if fixture == 'stocked_zones' else [] for _ in (0, 1)]
        graves = [list(GRAVE) if fixture == 'stocked_zones' else [] for _ in (0, 1)]
        mana = [60, 60]; life = [20, 20]; battle = [0, 0]
        setup = [card(name, 'source', owner)]; battle[owner] = 1
        pre = []; run = []; expected = {'source': [power, tough]}
        if v == 'wrong_color':
            setup[0]['zone'] = 'hand'; hands[owner].append(name); battle[owner] = 0
            pool = list(p['mana']); c = next(i for i in range(1, 6) if pool[i]); pool[1 if c != 1 else 2] += pool[c]; pool[c] = 0
            pre += exact_mana(pool); mana[owner] = sum(pool)
            run += [step('cast', ref='source', value=0), observe('source', 'in_hand', 1), observe('source', 'in_play', 0)]
        else:
            defending = v in ('defend', 'defend_ground', 'defend_damage', 'defend_cleanup', 'tapped_defend', 'sick_defend') or v == 'combat_snapshot' and mode != 'exalted' and 'Defender' in keys
            other = v in ('other_alone', 'other_pair', 'tapped_source', 'sick_source', 'two_sources', 'combat_snapshot',
                          'cleanup', 'damage', 'source_bounced_before', 'source_bounced_after', 'multiple_attackers_then_bounce')
            if v == 'combat_snapshot' and mode != 'exalted': other = False
            attacker_owner = 1-owner if defending else owner
            if v in ('tapped_source', 'tapped_defend'): setup[0]['state'] = ['tapped']
            if v in ('sick_source', 'sick_defend'): setup[0]['state'] = ['sick']
            if defending or other:
                an = 'Hill Giant' if v in ('defend_ground', 'defend_damage', 'defend_cleanup', 'tapped_defend', 'sick_defend') or defending and v == 'combat_snapshot' else 'Air Elemental'
                setup.append(card(an, 'attacker', attacker_owner)); battle[attacker_owner] += 1
                expected['attacker'] = list(map(int, WITNESSES[an][:2])); attacker = 'attacker'
            else: attacker = 'source'
            pair = v in ('attack_pair', 'other_pair', 'multiple_attackers_then_bounce')
            if pair:
                setup.append(card('Suntail Hawk', 'second', attacker_owner)); battle[attacker_owner] += 1
                expected['second'] = [1, 1]
            if v == 'two_sources':
                setup.append(card(name, 'copy', owner)); battle[owner] += 1; expected['copy'] = [power, tough]
            bounced = v in ('source_bounced_before', 'source_bounced_after', 'multiple_attackers_then_bounce')
            bounce = [step('choose_target', player=1-owner, ref='source'), step('cast', ref='response', value=1),
                      step('assert_stack', value=1), step('resolve'), step('assert_choices_consumed'),
                      step('bind_hand', player=owner, value=csvid, ref='source'), observe('source', 'in_hand', 1)]
            if bounced:
                # An opposing human response avoids AI strategic refusal of a beneficial self-bounce.
                setup.append(card('Unsummon', 'response', 1-owner, 'hand'))
                graves[1-owner].append('Unsummon'); mana[1-owner] -= 1; hands[owner].append(name); battle[owner] -= 1
            run.append(step('turn', player=attacker_owner, value=0))
            if v == 'source_bounced_before': run += bounce
            run += [step('begin_attackers'), step('attack', ref=attacker, value=int(attacker != 'source' or 'Defender' not in keys)),
                    observe(attacker, 'attacking', int(attacker != 'source' or 'Defender' not in keys))]
            if pair: run.append(step('attack', ref='second', value=1))
            # Provisional selections must not trigger exalted before the complete declaration.
            run += [observe(attacker, 'power', expected[attacker][0]), observe(attacker, 'toughness', expected[attacker][1])]
            if mode == 'exalted' and v == 'two_sources' and owner == 0:
                # The human orders simultaneous triggers. Don't pass this choice and
                # rely on the engine's fallback, which treats mana sources differently.
                # Once one is resolved, the single remaining mandatory trigger is automatic.
                run.append(step('choose_priority', ref='source'))
            run.append(step('finish_attackers'))
            attacks = int(attacker != 'source' or 'Defender' not in keys)+int(pair)
            bonus = (1+int(v == 'two_sources')) if mode == 'exalted' and not defending and attacks == 1 and v != 'source_bounced_before' else 0
            expected[attacker] = [s+bonus for s in expected[attacker]]
            run += [observe(attacker, 'power', expected[attacker][0]), observe(attacker, 'toughness', expected[attacker][1])]
            # Bouncing a second attacker afterwards doesn't retroactively create an "attacks alone" trigger.
            if v == 'multiple_attackers_then_bounce':
                bounce[0] = step('choose_target', player=1-owner, ref='second')
                bounce[-2] = step('bind_hand', player=owner, value=registry['Suntail Hawk']['csvid'], ref='second')
                bounce[-1] = observe('second', 'in_hand', 1)
                hands[owner][-1] = 'Suntail Hawk'
            if v in ('source_bounced_after', 'multiple_attackers_then_bounce'): run += bounce
            run.append(step('begin_blockers'))
            blocked = v in ('blocked', 'blocked_damage', 'two_blockers', 'flanking_blocker', 'small_blocker','lethal_blocker') or v == 'combat_snapshot' and mode != 'exalted' and not defending
            if defending:
                legal = int(v != 'tapped_defend' and (an != 'Air Elemental' or bool(keys & {'Flying', 'Reach'})))
                run += [step('block', ref='source', other=attacker, value=legal), observe('source', 'blocking_count', legal)]
                if mode == 'bushido' and legal: expected['source'] = [power+n, tough+n]
            elif blocked:
                wn = 'Colossus of Akros' if v=='lethal_blocker' else 'Benalish Cavalry' if v == 'flanking_blocker' else 'Muck Rats' if v == 'small_blocker' else 'Air Elemental'
                wp, wt = map(int, WITNESSES[wn][:2]); allowed = int(('Flying' not in keys or wn == 'Air Elemental') and not protects(p,oracle[wn]['colors'],True,'Artifact' in oracle[wn]['type_line']))
                for ref in ('blocker', 'blocker_two')[:2 if v == 'two_blockers' else 1]:
                    setup.append(card(wn, ref, 1-owner)); battle[1-owner] += 1
                    expected[ref] = [wp, wt]
                    run += [step('block', ref=ref, other=attacker, value=allowed), observe(ref, 'blocking_count', allowed)]
                    if mode == 'flanking' and wn != 'Benalish Cavalry' and allowed: expected[ref] = [wp-1, wt-1]
                if mode == 'bushido' and allowed: expected['source'] = [power+n, tough+n]
            run.append(step('advance_combat'))
            if blocked:
                run.append(observe('source', 'blocked', allowed))
            for ref, pt in expected.items():
                if bounced and (ref == 'source' and v != 'multiple_attackers_then_bounce' or ref == 'second' and v == 'multiple_attackers_then_bounce'): continue
                if pt[1] == 0:
                    run.append(observe(ref, 'in_play', 0)); graves[1-owner].append(wn); battle[1-owner] -= 1
                else: run += [observe(ref, 'power', pt[0]), observe(ref, 'toughness', pt[1]), observe(ref, 'damage', 0)]
            if v in ('blocked_damage', 'defend_damage','lethal_blocker'):
                source_pt = expected['source']; opponent_ref = 'attacker' if defending else 'blocker'
                op_pt = expected[opponent_ref]
                source_damage = op_pt[0] if not ('First strike' in keys and source_pt[0] >= op_pt[1]) else 0
                source_alive = source_damage < source_pt[1] or 'Indestructible' in keys; opponent_alive = source_pt[0] < op_pt[1]
                run += [step('combat_damage'), observe('source', 'in_play', int(source_alive)), observe(opponent_ref, 'in_play', int(opponent_alive))]
                if source_alive: run.append(observe('source', 'damage', source_damage))
                else: graves[owner].append(name); battle[owner] -= 1
                if opponent_alive: run.append(observe(opponent_ref, 'damage', source_pt[0]))
                else: graves[1-owner].append(an if defending else wn); battle[1-owner] -= 1
            if v in ('damage', 'cleanup'):
                total = expected[attacker][0]
                life[1-owner] -= total
                run += [step('combat_damage'), step('assert_life', player=1-owner, value=life[1-owner]),
                        step('end_combat'), observe(attacker, 'attacking', 0),
                        observe(attacker, 'power', expected[attacker][0]), observe(attacker, 'toughness', expected[attacker][1])]
                if v == 'cleanup':
                    run += [step('cleanup'), observe(attacker, 'power', 4), observe(attacker, 'toughness', 4)]
            if v == 'small_blocker' and mode == 'flanking':
                # A dead blocker leaves its attacker blocked. Without trample, no combat
                # damage reaches the defending player even when all blockers have gone.
                run += [step('combat_damage'), step('assert_life', player=1-owner, value=20),
                        observe('source', 'damage', 0), step('end_combat')]
            if v == 'defend_cleanup':
                run += [step('cleanup'), observe('source', 'power', power), observe('source', 'toughness', tough),
                        observe('attacker', 'power', 3), observe('attacker', 'toughness', 3)]
            if mode in ('bushido', 'flanking') and blocked and v != 'blocked_damage':
                # End-of-turn cleanup also applies without simulating combat damage: phase advancement alone
                # must not remove the temporary combat modifier before cleanup.
                run += [step('cleanup')]
                for ref, pt in expected.items():
                    if pt[1] == 0: continue
                    original = [power, tough] if ref == 'source' else list(map(int, WITNESSES[wn][:2]))
                    run += [observe(ref, 'power', original[0]), observe(ref, 'toughness', original[1])]
        checks = [step('assert_stack', value=0), step('assert_choices_consumed')]
        for player in (0, 1):
            checks += [step('assert_life', player=player, value=life[player]), step('assert_mana_total', player=player, value=mana[player]),
                       step('assert_zone', player=player, zone='battlefield', value=battle[player])]
            for zone, names in (('hand', hands[player]), ('graveyard', graves[player]),
                                ('library', LIBRARY if fixture == 'stocked_zones' else []), ('exile', EXILE if fixture == 'stocked_zones' else [])):
                checks.append(step('assert_zone', player=player, zone=zone, value=len(names)))
                checks += [step('assert_zone_card', player=player, zone=zone, index=i, value=registry[nm]['csvid']) if zone == 'library'
                           else step('assert_zone_contains', player=player, zone=zone, value=registry[nm]['csvid']) for i, nm in enumerate(names)]
        actions = pre+run+checks
        if v == 'combat_snapshot':
            cut = next(i for i, s in enumerate(run) if s['op'] == ('finish_attackers' if mode == 'exalted' else 'advance_combat'))
            # Exalted captures provisional attackers; bushido/flanking capture provisional blockers.
            # Replay completes the same real declaration instead of directly sending an event.
            actions = pre+run[:cut]+[step('capture')]+run[cut:]+checks+[step('record_state'), step('ai_roundtrip'), step('restore')]+run[cut:]+checks+[step('assert_recorded_state')]
        result.append(dict(name=f'CombatKeywordCreatureRecipe_{csvid}_{variant}', card=name, fixture=fixture, seed=123,
                           setup=setup, steps=actions, _recipe=dict(family='combat_keyword_creature', version=2, variant=variant,
                           params=declaration['params'], oracle_text=declaration['oracle_text'], untested_abilities=p['remaining_abilities'],
                           untested_keywords=[k for k in ('Flash', 'Trample') if k in keys])))
    return result
