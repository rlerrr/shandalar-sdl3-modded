"""Pinned entry draw/life effects, exercised by casting and legal re-entry."""
import re
from collections import Counter
from creature_recipes import KEYWORDS, REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

COMBAT = ('unblocked', 'block_ground', 'block_flying', 'tapped_attacker', 'tapped_blocker')
WITNESSES = {
    'Unsummon': ('{U}', "Return target creature to its owner's hand."),
    'Cloudshift': ('{W}', 'Exile target creature you control, then return that card to the battlefield under your control.'),
    'Essence Scatter': ('{1}{U}', 'Counter target creature spell.'),
    'Grizzly Bears': ('{1}{G}', '', '2', '2'),
}


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line', '') or declaration.get('oracle_text') != record.get('oracle_text')
            or 'variants' not in declaration):
        raise ValueError('entry value requires a pinned normal single-face creature and reviewed variants')
    p = declaration.get('params')
    if not isinstance(p, dict) or set(p) != {'keywords', 'draw', 'gain_life', 'lose_life'}:
        raise ValueError('explicit entry keywords, draw and life amounts required')
    if any(type(p[k]) is not int or not 0 <= p[k] <= (3 if k == 'draw' else 10) for k in ('draw', 'gain_life', 'lose_life')):
        raise ValueError('bounded integer entry amounts required')
    if bool(p['draw']) == bool(p['gain_life']) or p['lose_life'] and not p['draw']:
        raise ValueError('entry value must draw (optionally with life loss) or gain life')
    if p['draw']:
        amount = {1:'a card', 2:'two cards', 3:'three cards'}[p['draw']]
        clauses = ['draw '+amount, 'you draw '+amount]
        if p['lose_life']:
            clauses = [c+' and '+you+'lose '+str(p['lose_life'])+' life' for c in clauses for you in ('', 'you ')]
    else: clauses = ['you gain '+str(p['gain_life'])+' life']
    expected = {f'When {ref} enters{suffix}, {clause}.' for ref in ('this creature', record['name'])
                for suffix in ('', ' the battlefield') for clause in clauses}
    keys = []; triggers = []; canonical = {k.lower(): k for k in KEYWORDS + ('Hexproof',)}
    for line in record['oracle_text'].splitlines():
        if line in expected: triggers.append(line); continue
        text = line
        for reminder in re.findall(r'\([^\n]*?\)', line):
            if reminder not in REMINDERS | EXTRA_REMINDERS: raise ValueError('unreviewed entry reminder')
            text = text.replace(reminder, '')
        tokens = [t.strip().lower() for t in text.strip().split(',')]
        if any(t not in canonical for t in tokens): raise ValueError('unreviewed entry clause, ability or trigger')
        keys.extend(canonical[t] for t in tokens)
    if (len(triggers) != 1 or p['keywords'] != sorted(keys) or len(set(keys)) != len(keys)
            or set(keys) != set(record.get('keywords', [])) or set(keys) & {'Menace', 'Deathtouch'}):
        raise ValueError('entry value whole-card scope must match explicit parameters')
    for field in ('power', 'toughness'):
        if not re.fullmatch(r'\d+', record.get(field, '')) or not 0 <= int(record[field]) <= 20:
            raise ValueError('bounded numeric entry creature stats required')
    if int(record['toughness']) == 0: raise ValueError('entry creature must survive entry')
    return dict(p, family='etb_value_creature', power=int(record['power']), toughness=int(record['toughness']), mana=cost_from_oracle(record))


def variants(p):
    result = list(COMBAT) + ['cast', 'payment', 'insufficient_mana', 'injured', 'opponent_cast', 'two_copies',
        'bounce_recast', 'bounce_only', 'blink', 'blink_snapshot', 'countered', 'pending_snapshot', 'snapshot',
        'cleanup_once', 'destroy', 'other_creature', 'empty_hand', 'empty_graveyard', 'empty_exile', 'short_library',
        'opponent_injured', 'opponent_bounce', 'opponent_pending_snapshot', 'fresh_attack']
    if any(p['mana'][1:]): result.append('wrong_color')
    return result


def validate_witnesses(oracle, registry):
    for name, fields in WITNESSES.items():
        r = oracle.get(name, {})
        actual = tuple(r.get(k) for k in ('mana_cost', 'oracle_text', 'power', 'toughness')[:len(fields)])
        if name not in registry or r.get('layout') != 'normal' or r.get('card_faces') or actual != fields:
            raise ValueError('entry witness changed: '+name)


def expand(name, csvid, declaration, p, selected, registry, oracle):
    from recipes import card, step, observe, exact_mana, cost_from_oracle
    from creature_recipes import expand as creatures
    result = creatures(name, csvid, declaration, p, [v for v in selected if v in COMBAT], registry)
    for case in result:
        # Battlefield setup uses the real put-into-play path and completes
        # entry effects. Observe that receipt before combat, then require it
        # to remain unchanged rather than assuming a keyword-only creature.
        net_life = p['gain_life']-p['lose_life']
        for action in case['steps']:
            if action['op'] == 'assert_life' and action['player'] == 0:
                action['value'] += net_life
        receipt = [step('assert_life', player=0, value=20+net_life)]
        for player in (0, 1):
            hands = list(HAND)+(list(LIBRARY[:p['draw']]) if player == 0 else [])
            library = LIBRARY[p['draw']:] if player == 0 else LIBRARY
            for zone, names in (('hand', hands), ('graveyard', GRAVE), ('exile', EXILE), ('library', library)):
                receipt.append(step('assert_zone', player=player, zone=zone, value=len(names)))
                for i, n in enumerate(names):
                    receipt.append(step('assert_zone_card', player=player, zone=zone, index=i, value=registry[n]['csvid']) if zone == 'library'
                                   else step('assert_zone_contains', player=player, zone=zone, value=registry[n]['csvid']))
            receipt += [step('assert_hand_graveyard_total', player=player, index=registry[n]['csvid'], value=count)
                        for n, count in Counter(hands+list(GRAVE)).items()]
        case['steps'] = receipt + case['steps'] + receipt
    for v in (v for v in selected if v not in COMBAT):
        owner = int(v.startswith('opponent_')); mana = [[10]*6, [10]*6]; life = [20, 20]
        hands = [list(HAND), list(HAND)]; graves = [list(GRAVE), list(GRAVE)]
        exile = [list(EXILE), list(EXILE)]; libs = [list(LIBRARY), list(LIBRARY)]; battle = [0, 0]
        fixture = 'stocked_zones'; setup = []; pre = []; actions = []; extra = []
        if v.startswith('empty_') or v == 'short_library':
            fixture = 'empty_main'
            if v == 'empty_hand': hands[owner] = []
            if v == 'empty_graveyard': graves[owner] = []
            if v == 'empty_exile': exile[owner] = []
            if v == 'short_library': libs[owner] = libs[owner][:p['draw']]
            for player in (0, 1):
                for zone, names in (('hand', hands[player]), ('graveyard', graves[player]), ('exile', exile[player]), ('library', libs[player])):
                    setup += [card(n, f'p{player}_{zone}_{i}', player, zone) for i, n in enumerate(names)]
        setup.append(card(name, 'source', owner, 'hand')); hands[owner].append(name)
        if owner: pre.append(step('turn', player=owner, value=0))
        if v in ('injured', 'opponent_injured'): life[owner] = 7; pre.append(step('set_life', player=owner, value=7))
        if v in ('payment', 'insufficient_mana', 'wrong_color'):
            mana[owner] = list(p['mana']) if v != 'insufficient_mana' else [0]*6
            if v == 'wrong_color':
                color = next(c for c in range(1, 6) if p['mana'][c]); mana[owner][color] = 0
                mana[owner][1 if color != 1 else 2] += p['mana'][color]
            pre += [step('set_mana', player=owner, color=c, value=n) for c, n in enumerate(mana[owner])]

        def pay(cost, caster=owner):
            mana[caster][:] = [mana[caster][i]-cost[i] for i in range(6)]

        def entry(ref='source'):
            pay(p['mana']); hands[owner].remove(name); battle[owner] += 1
            drawn = libs[owner][:p['draw']]; assert len(drawn) == p['draw']
            hands[owner] += drawn; del libs[owner][:p['draw']]
            life[owner] += p['gain_life']-p['lose_life']
            return [step('cast', ref=ref, value=1), step('assert_stack', value=1), step('resolve'), step('assert_choices_consumed')]

        def witness(spell, ref, target='source', caster=owner):
            setup.append(card(spell, ref, caster, 'hand')); pay(cost_from_oracle(oracle[spell]), caster); graves[caster].append(spell)
            return [step('choose_target', player=caster, ref=target), step('cast', ref=ref, value=1), step('assert_stack', value=1), step('resolve'), step('assert_choices_consumed')]

        alive = v not in ('insufficient_mana', 'wrong_color', 'countered', 'destroy', 'bounce_only', 'opponent_bounce')
        if v in ('insufficient_mana', 'wrong_color'):
            actions += [step('cast', ref='source', value=0), observe('source', 'in_hand', 1)]
        elif v == 'countered':
            setup.append(card('Essence Scatter', 'counter', owner, 'hand')); pay(p['mana']); pay(cost_from_oracle(oracle['Essence Scatter']))
            hands[owner].remove(name); graves[owner] += [name, 'Essence Scatter']
            # Casting opens the real interrupt/response window. The counter
            # helper selects the top spell itself; a countered cast returns 0.
            actions += [step('choose_priority', ref='counter'), step('cast', ref='source', value=0),
                        step('resolve'), step('assert_choices_consumed')]
        else:
            actions += entry()
            if v == 'two_copies':
                setup.append(card(name, 'second', owner, 'hand')); hands[owner].append(name); actions += entry('second')
                extra += [observe('second', 'in_play', 1), observe('second', 'power', p['power']), observe('second', 'toughness', p['toughness'])]
            if v in ('bounce_recast', 'bounce_only', 'opponent_bounce'):
                # Keep the scripted caster human. AI policy can reject bouncing
                # its own permanent even when that target is rules-legal.
                actions += witness('Unsummon', 'bounce', caster=0); battle[owner] -= 1; hands[owner].append(name)
                actions += [step('bind_hand', player=owner, value=csvid, ref='source'), observe('source', 'in_hand', 1)]
                if v == 'bounce_recast': actions += entry()
            if v in ('blink', 'blink_snapshot'):
                actions += witness('Cloudshift', 'blink')
                drawn = libs[owner][:p['draw']]; hands[owner] += drawn; del libs[owner][:p['draw']]
                life[owner] += p['gain_life']-p['lose_life']
                actions += [step('bind_permanent', player=owner, value=csvid, ref='source')]
            if v == 'destroy':
                actions += [step('destroy', ref='source')]; battle[owner] -= 1; graves[owner].append(name)
            if v == 'other_creature':
                actions += [step('cast', ref=f'p{owner}_hand_0', value=1), step('resolve')]
                hands[owner].remove('Grizzly Bears'); pay(cost_from_oracle(oracle['Grizzly Bears'])); battle[owner] += 1
            if v == 'cleanup_once': actions.append(step('cleanup'))
            if v == 'fresh_attack':
                allowed = int('Haste' in p['keywords'] and 'Defender' not in p['keywords'])
                actions += [step('begin_attackers'), step('attack', ref='source', value=allowed), step('finish_attackers')]
                extra += [observe('source', 'attacking', allowed), observe('source', 'tapped', int(allowed and 'Vigilance' not in p['keywords']))]
        checks = [step('assert_choices_consumed'), step('assert_stack', value=0), observe('source', 'in_play', int(alive))]
        if alive: checks += [observe('source', 'power', p['power']), observe('source', 'toughness', p['toughness']), observe('source', 'damage', 0)] + extra
        for player in (0, 1):
            checks += [step('assert_life', player=player, value=life[player]), step('assert_zone', player=player, zone='battlefield', value=battle[player]),
                       step('assert_mana_total', player=player, value=sum(mana[player]))]
            # With exact pools, every colored and generic payment is observable;
            # ordinary surplus pools allow engine choice of generic mana color.
            if v in ('payment', 'insufficient_mana', 'wrong_color'): checks += [step('assert_mana', player=player, color=c, value=mana[player][c]) for c in range(6)]
            for zone, names in (('hand', hands[player]), ('graveyard', graves[player]), ('exile', exile[player]), ('library', libs[player])):
                checks.append(step('assert_zone', player=player, zone=zone, value=len(names)))
                for i, n in enumerate(names):
                    checks.append(step('assert_zone_card', player=player, zone=zone, index=i, value=registry[n]['csvid']) if zone == 'library'
                                  else step('assert_zone_contains', player=player, zone=zone, value=registry[n]['csvid']))
            # Drawn cards may already exist in the stocked hand. Membership
            # alone would miss an incorrect duplicate; verify multiplicity.
            checks += [step('assert_hand_graveyard_total', player=player, index=registry[n]['csvid'], value=count)
                       for n, count in Counter(hands[player]+graves[player]).items()]
        if v == 'snapshot':
            steps = pre + [step('capture')] + actions + checks + [step('record_state'), step('ai_roundtrip'), step('restore')] + actions + checks + [step('assert_recorded_state')]
        elif v in ('pending_snapshot', 'opponent_pending_snapshot', 'blink_snapshot'):
            # The final resolve occurs after a real spell is paid and targeted.
            index = max(i for i, s in enumerate(actions) if s['op'] == 'resolve')
            before, pending = actions[:index], actions[index:]
            steps = pre + before + [step('capture')] + pending + checks + [step('record_state'), step('ai_roundtrip'), step('restore')] + pending + checks + [step('assert_recorded_state')]
        else: steps = pre + actions + checks
        result.append(dict(name=f'EtbValueCreatureRecipe_{csvid}_{v}', card=name, fixture=fixture, seed=123, setup=setup, steps=steps,
                           _recipe=dict(family='etb_value_creature', version=2, variant=v, params=declaration['params'], oracle_text=declaration['oracle_text'],
                           untested_keywords=[k for k in ('Flash', 'Hexproof') if k in p['keywords']])))
    return result
