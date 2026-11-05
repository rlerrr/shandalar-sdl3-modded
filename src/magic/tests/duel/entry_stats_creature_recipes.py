"""Oracle-pinned entry bonuses and counters, through casting and real re-entry."""
import copy
import re
from collections import Counter
from creature_recipes import oracle_keywords
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

SPELLS = {
    'Unsummon': ('{U}', "Return target creature to its owner's hand."),
    'Cloudshift': ('{W}', 'Exile target creature you control, then return that card to the battlefield under your control.'),
    'Essence Scatter': ('{1}{U}', 'Counter target creature spell.'),
    'Giant Growth': ('{G}', 'Target creature gets +3/+3 until end of turn.'),
}


def spec(d, r):
    from recipes import cost_from_oracle
    if not r or r.get('layout') != 'normal' or d.get('oracle_text') != r.get('oracle_text') or 'variants' not in d:
        raise ValueError('entry stats require complete pinned normal creature text and explicit variants')
    p = d.get('params')
    if not isinstance(p, dict) or set(p) != {'effect', 'power', 'toughness', 'amount', 'keywords'}:
        raise ValueError('explicit entry effect, signed stats, counter amount and passive keywords required')
    if p['effect'] not in ('target_pump', 'team_pump', 'counter') or any(type(p[k]) is not int for k in ('power', 'toughness', 'amount')):
        raise ValueError('reviewed entry stat effect and integer amounts required')
    if p['effect'] == 'counter':
        if not 1 <= p['amount'] <= 3 or p['power'] or p['toughness']:
            raise ValueError('one to three counters and no temporary bonus required')
        clause = 'put ' + {1: 'a', 2: 'two', 3: 'three'}[p['amount']] + ' +1/+1 counter' + ('s' if p['amount'] != 1 else '') + ' on target creature.'
    else:
        if p['amount'] or not 0 <= p['power'] <= 4 or not 0 <= p['toughness'] <= 4 or not (p['power'] or p['toughness']):
            raise ValueError('bounded positive entry bonus and no counters required')
        clause = ('target creature gets ' if p['effect'] == 'target_pump' else 'creatures you control get ') + f"+{p['power']}/+{p['toughness']} until end of turn."
    triggers = {f'When {ref} enters{suffix}, {clause}' for ref in ('this creature', r['name'], r['name'].split(',')[0]) for suffix in ('', ' the battlefield')}
    lines = r.get('oracle_text', '').splitlines()
    if sum(line in triggers for line in lines) != 1:
        raise ValueError('exactly one unconditional entry clause matching the parameters required')
    keys = oracle_keywords(dict(r, oracle_text='\n'.join(line for line in lines if line not in triggers)))
    if p['keywords'] != keys:
        raise ValueError('entry source keywords must match all remaining Oracle text')
    return dict(p, family='entry_stats_creature', base_power=int(r['power']), base_toughness=int(r['toughness']),
                mana=cost_from_oracle(r), legendary='Legendary' in r['type_line'])


def variants(p):
    names = ['cast', 'payment', 'insufficient_mana', 'no_other_creatures', 'cleanup', 'other_creature',
             'late_entry', 'bounce_only', 'bounce_recast', 'blink', 'destroy_source', 'ready_snapshot',
             'pending_snapshot', 'resolved_snapshot', 'blink_snapshot', 'bare_zones', 'countered',
             'tapped_recipient', 'sick_recipient', 'pump_before_source', 'combat']
    if p['effect'] != 'team_pump':
        names += ['self_target', 'enemy_target']
    if any(p['mana'][1:]):
        names.append('wrong_color')
    if not p['legendary']:
        names.append('two_copies')
    return [v + '_' + str(owner) for owner in (0, 1) for v in names]


def validate_witnesses(oracle, registry):
    for name, fields in SPELLS.items():
        if name not in registry or (oracle.get(name, {}).get('mana_cost'), oracle.get(name, {}).get('oracle_text')) != fields:
            raise ValueError('entry stat response changed: ' + name)
    for name, pt in [('Grizzly Bears', ('2', '2')), ('Hill Giant', ('3', '3'))]:
        r = oracle.get(name, {})
        if name not in registry or (r.get('power'), r.get('toughness'), r.get('oracle_text')) != (*pt, ''):
            raise ValueError('entry stat creature witness changed: ' + name)
    r = oracle.get('Darksteel Relic', {})
    if 'Darksteel Relic' not in registry or r.get('type_line') != 'Artifact' or r.get('oracle_text') != 'Indestructible (Effects that say "destroy" don\'t destroy this artifact.)':
        raise ValueError('entry stat noncreature witness changed')


def expand(name, csvid, d, p, selected, registry, oracle):
    from recipes import card, step, observe, cost_from_oracle
    result = []
    for variant in selected:
        v, owner = variant.rsplit('_', 1); owner = int(owner); bare = v == 'bare_zones'
        zones = {z: [[] if bare else list(ns), [] if bare else list(ns)] for z, ns in (('hand', HAND), ('graveyard', GRAVE), ('exile', EXILE), ('library', LIBRARY))}
        mana = [[10]*6, [10]*6]; life = [20, 20]; setup = []; entities = {}; pre = [step('turn', player=owner, value=0)]; actions = []

        def add(nm, ref, pl, zone='battlefield', state=None):
            item = card(nm, ref, pl, zone)
            if state: item['state'] = state
            setup.append(item)
            r = oracle[nm]; creature = 'Creature' in r['type_line']
            entities[ref] = dict(name=nm, player=pl, alive=zone == 'battlefield', hand=zone == 'hand', creature=creature,
                                 power=int(r['power']) if creature else 0, toughness=int(r['toughness']) if creature else 0,
                                 bonus_power=0, bonus_toughness=0, counters=0, tapped=bool(state and 'tapped' in state))
            if zone != 'battlefield': zones[zone][pl].append(nm)

        def pay(nm, pl):
            mana[pl] = [a-b for a, b in zip(mana[pl], cost_from_oracle(oracle[nm]))]

        def affected(target):
            return [ref for ref, e in entities.items() if e['alive'] and e['creature'] and e['player'] == owner] if p['effect'] == 'team_pump' else [target]

        def apply(target):
            for ref in affected(target):
                e = entities[ref]; e['bonus_power'] += p['power']; e['bonus_toughness'] += p['toughness']; e['counters'] += p['amount']

        def choices(target):
            return [] if p['effect'] == 'team_pump' else [step('choose_target', player=owner, ref=target)]

        def entry(ref='source', target='recipient'):
            before = []
            for existing, e in entities.items():
                if e['alive'] and e['creature']:
                    before += [observe(existing, 'power', e['power'] + e['bonus_power'] + e['counters']), observe(existing, 'toughness', e['toughness'] + e['bonus_toughness'] + e['counters']), observe(existing, 'plus1_counters', e['counters'])]
            pay(name, owner); zones['hand'][owner].remove(name); entities[ref]['alive'] = True; entities[ref]['hand'] = False
            apply(target)
            return [step('cast', ref=ref, value=1), step('assert_stack', value=1)] + before + choices(target) + [step('resolve'), step('assert_choices_consumed')]

        def spell(nm, ref, target, caster=owner):
            add(nm, ref, caster, 'hand'); pay(nm, caster); zones['hand'][caster].remove(nm); zones['graveyard'][caster].append(nm); entities[ref]['hand'] = False
            return [step('choose_target', player=caster, ref=target), step('cast', ref=ref, value=1), step('assert_stack', value=1), step('resolve'), step('assert_choices_consumed')]

        add(name, 'source', owner, 'hand')
        if v != 'no_other_creatures':
            add('Grizzly Bears', 'recipient', owner, state=['tapped'] if v == 'tapped_recipient' else ['sick'] if v == 'sick_recipient' else None)
            add('Hill Giant', 'friendly', owner); add('Grizzly Bears', 'enemy', 1-owner); add('Hill Giant', 'enemy_other', 1-owner)
        add('Darksteel Relic', 'artifact', owner)
        target = 'source' if v in ('self_target', 'no_other_creatures') else 'enemy' if v == 'enemy_target' else 'recipient'
        if v in ('payment', 'insufficient_mana', 'wrong_color'):
            mana[owner] = list(p['mana']) if v != 'insufficient_mana' else [0]*6
            if v == 'wrong_color':
                c = next(c for c in range(1, 6) if mana[owner][c]); mana[owner][1 if c != 1 else 2] += mana[owner][c]; mana[owner][c] = 0
            pre += [step('set_mana', player=owner, color=c, value=n) for c, n in enumerate(mana[owner])]
        if v == 'pump_before_source':
            actions += spell('Giant Growth', 'pump', target); entities[target]['bonus_power'] += 3; entities[target]['bonus_toughness'] += 3
        if v in ('insufficient_mana', 'wrong_color'):
            actions += [step('cast', ref='source', value=0), step('assert_choices_consumed')]
        elif v == 'countered':
            add('Essence Scatter', 'counter', 0, 'hand'); pay(name, owner); pay('Essence Scatter', 0)
            zones['hand'][owner].remove(name); zones['graveyard'][owner].append(name); entities['source']['hand'] = False
            zones['hand'][0].remove('Essence Scatter'); zones['graveyard'][0].append('Essence Scatter'); entities['counter']['hand'] = False
            actions += [step('choose_priority', player=0, ref='counter'), step('cast', ref='source', value=0), step('resolve'), step('assert_choices_consumed')]
        else:
            actions += entry(target=target)
            if v == 'two_copies':
                add(name, 'second', owner, 'hand'); actions += entry('second', target)
            if v in ('other_creature', 'late_entry'):
                add('Grizzly Bears', 'late', owner, 'hand'); pay('Grizzly Bears', owner); zones['hand'][owner].remove('Grizzly Bears'); entities['late']['alive'] = True; entities['late']['hand'] = False
                actions += [step('cast', ref='late', value=1), step('resolve'), step('assert_choices_consumed')]
            if v in ('bounce_only', 'bounce_recast'):
                actions += spell('Unsummon', 'bounce', 'source', 0); e = entities['source']; e['alive'] = False; e['hand'] = True; e['bonus_power'] = e['bonus_toughness'] = e['counters'] = 0; zones['hand'][owner].append(name)
                actions += [step('bind_hand', player=owner, value=csvid, ref='source')]
                if v == 'bounce_recast': actions += entry(target=target)
            if v in ('blink', 'blink_snapshot'):
                add('Cloudshift', 'blink', owner, 'hand'); pay('Cloudshift', owner); zones['hand'][owner].remove('Cloudshift'); zones['graveyard'][owner].append('Cloudshift'); entities['blink']['hand'] = False
                e = entities['source']; e['bonus_power'] = e['bonus_toughness'] = e['counters'] = 0
                actions += [step('choose_target', player=owner, ref='source'), step('cast', ref='blink', value=1), step('assert_stack', value=1)] + choices(target) + [step('resolve'), step('assert_choices_consumed'), step('bind_permanent', player=owner, value=csvid, ref='source')]
                apply(target)
            if v == 'destroy_source':
                actions += [step('destroy', ref='source')]; entities['source']['alive'] = False; zones['graveyard'][owner].append(name)
            if v == 'combat':
                e = entities[target]; dealt = e['power'] + e['bonus_power'] + e['counters']; life[1-owner] -= dealt; e['tapped'] = True
                actions += [step('begin_attackers'), step('attack', ref=target, value=1), step('finish_attackers'), step('begin_blockers'), step('advance_combat'), step('combat_damage'), step('resolve'), step('assert_choices_consumed')]
            if v == 'cleanup':
                actions.append(step('cleanup'))
                for e in entities.values(): e['bonus_power'] = e['bonus_toughness'] = 0
        checks = [step('assert_stack', value=0), step('assert_choices_consumed')]
        for ref, e in entities.items():
            checks += [observe(ref, 'in_play', int(e['alive'])), observe(ref, 'in_hand', int(e['hand']))]
            if e['alive']:
                checks += [observe(ref, 'tapped', int(e['tapped']))]
                if e['creature']: checks += [observe(ref, 'power', e['power'] + e['bonus_power'] + e['counters']), observe(ref, 'toughness', e['toughness'] + e['bonus_toughness'] + e['counters']), observe(ref, 'damage', 0), observe(ref, 'plus1_counters', e['counters']), observe(ref, 'minus1_counters', 0)]
        universe = set(HAND + GRAVE + EXILE + LIBRARY) | {e['name'] for e in entities.values()}
        for pl in (0, 1):
            checks += [step('assert_life', player=pl, value=life[pl]), step('assert_poison', player=pl, value=0), step('assert_zone', player=pl, zone='battlefield', value=sum(e['alive'] and e['player'] == pl for e in entities.values()))]
            if v != 'cleanup': checks.append(step('assert_mana_total', player=pl, value=sum(mana[pl])))
            if v in ('payment', 'insufficient_mana', 'wrong_color'): checks += [step('assert_mana', player=pl, color=c, value=n) for c, n in enumerate(mana[pl])]
            for z, players in zones.items():
                ns = players[pl]; checks.append(step('assert_zone', player=pl, zone=z, value=len(ns)))
                if z == 'library': checks += [step('assert_zone_card', player=pl, zone=z, index=i, value=registry[nm]['csvid']) for i, nm in enumerate(ns)]
                else:
                    counts = Counter(ns); checks += [step('assert_zone_multiplicity', player=pl, zone=z, index=registry[nm]['csvid'], value=counts[nm]) for nm in sorted(universe)]
        seq = pre + actions + checks
        if v in ('ready_snapshot', 'pending_snapshot', 'blink_snapshot'):
            cut = 0 if v == 'ready_snapshot' else max(i for i, s in enumerate(actions) if s['op'] == 'resolve')
            tail = actions[cut:] + checks
            seq = pre + actions[:cut] + [step('capture')] + copy.deepcopy(tail) + [step('record_state'), step('ai_roundtrip'), step('restore')] + copy.deepcopy(tail) + [step('assert_recorded_state')]
        if v == 'resolved_snapshot': seq += [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore'), step('assert_recorded_state')]
        result.append(dict(name=f'EntryStatsCreatureRecipe_{csvid}_{variant}', card=name, fixture='empty_main' if bare else 'stocked_zones', setup=setup, steps=seq, seed=123,
                           _recipe=dict(family=p['family'], version=2, variant=variant, params=d['params'], oracle_text=d['oracle_text'], untested_components=['source keyword combat mechanics', 'target becomes illegal after trigger targeting', 'trigger ordering', 'replacement effects', 'control changes'])))
    return result
