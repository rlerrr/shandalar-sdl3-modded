"""Creatures with a fixed Bloodrush payment, discard and combat pump."""
import json
import re
from collections import Counter
from pathlib import Path

from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'bloodrush_creature'
DATA = Path(__file__).parent / 'data/bloodrush_creature_wave8_profiles.json'
BLOODRUSH = re.compile(
    r'Bloodrush — ((?:\{(?:[0-9]+|[BUGRW])\})+), Discard this card: '
    r'Target attacking creature gets \+([0-9]+)/\+([0-9]+)'
    r'(?: and gains (trample|deathtouch|first strike|double strike))? until end of turn\.')
KEYWORD_BLOCKER = {'Trample': 'Elvish Mystic', 'Deathtouch': 'Dazzling Ramparts',
                   'First strike': 'Hill Giant', 'Double strike': 'Dazzling Ramparts'}
VANILLA_WITNESSES = {'Grizzly Bears', 'Hill Giant', 'Goblin Piker'}
WITNESSES = {*VANILLA_WITNESSES, 'Forest', 'Unsummon', 'Counterspell', *KEYWORD_BLOCKER.values()}
CAST_MODES = ('cast_payment', 'cast_short', 'cast_wrong_color', 'cast_countered', 'printed_keyword', 'printed_double_hit')
HAND_MODES = ('main_no_attack', 'combat_no_attack', 'hand_short', 'hand_wrong_color', 'activation_cancel',
              'paid_own', 'paid_opposing', 'post_block', 'pending_bounced', 'pending_replay')


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ')
            or any(not re.fullmatch(r'[0-9]+', record.get(key, '')) for key in ('power', 'toughness'))):
        raise ValueError('normal whole numeric creature required')
    clauses = record['oracle_text'].splitlines()
    ability = BLOODRUSH.fullmatch(clauses[-1]) if clauses else None
    if not ability:
        raise ValueError('reviewed fixed Bloodrush clause required')
    keyword = ability[4].capitalize() if ability[4] else None
    printed = [keyword] if keyword else []
    if clauses[:-1] != printed or set(record['keywords']) != {'Bloodrush', *printed}:
        raise ValueError('all printed clauses and keywords must match the Bloodrush model')
    cost = cost_from_oracle(record)
    fee = cost_from_oracle(dict(record, mana_cost=ability[1]))
    if not any(cost[1:]) or not any(fee[1:]):
        raise ValueError('printed colored cast and hand payment required')
    return dict(cast=cost, fee=fee, power=int(record['power']), toughness=int(record['toughness']),
                power_bonus=int(ability[2]), toughness_bonus=int(ability[3]), keyword=keyword, clauses=clauses)


def spec(declaration, record):
    model = mechanism(record)
    source = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int or params['schema'] != 1
            or not exact_pin(model, params['mechanism'])):
        raise ValueError('complete independently pinned Bloodrush creature and declaration required')
    return dict(model, family=FAMILY, csvid=source['csvid'])


def variants(model):
    modes = tuple(mode for mode in CAST_MODES if mode != 'printed_double_hit') + HAND_MODES + ('keyword_expires',)
    if not model['keyword']:
        modes = tuple(mode for mode in modes if mode not in ('printed_keyword', 'keyword_expires'))
    if model['keyword'] == 'Double strike':
        modes += ('printed_double_hit', 'double_blocker_gone')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def validate_witnesses(oracle, rows):
    profile = pins()
    if set(profile) != {'sources', 'witnesses'} or set(profile['witnesses']) != WITNESSES:
        raise ValueError('exact Bloodrush sources and witnesses required')
    for category, group in profile.items():
        for name, pin in group.items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('kind') != 'card'
                    or row.get('csvid') != pin['csvid'] or row.get('handler') != pin['handler']
                    or row.get('handler') in (None, 'NULL', '0') or oracle[name].get('layout') != 'normal'):
                raise ValueError('implemented complete Bloodrush source or witness changed: ' + name)
            if row['handler'] == 'card_dummy' and (
                    category != 'witnesses' or name not in VANILLA_WITNESSES or oracle[name]['oracle_text']):
                raise ValueError('generic callback is valid only for the exact vanilla combat witnesses')


def _case(name, csvid, declaration, model, variant, rows, oracle):
    from recipes import card, cost_from_oracle, step

    mode, actor = variant.rsplit('_', 1)
    owner = int(actor)
    foe = 1 - owner
    active = foe if mode == 'paid_opposing' else owner
    defender = 1 - active
    blocker_name = KEYWORD_BLOCKER.get(model['keyword'], 'Elvish Mystic')
    setup = [card(name, 'source', owner, 'hand'), card('Forest', 'other_hand', owner, 'hand')]
    if mode not in CAST_MODES:
        setup.extend([card('Grizzly Bears', 'attacker', active), card('Hill Giant', 'bystander', active)])
    if mode == 'keyword_expires' and model['keyword'] == 'First strike':
        blocker_name = 'Goblin Piker'
    elif mode == 'double_blocker_gone':
        blocker_name = 'Hill Giant'
    if mode in ('post_block', 'printed_keyword', 'keyword_expires', 'double_blocker_gone'):
        setup.append(card(blocker_name, 'blocker', defender))
    if mode == 'cast_countered':
        setup.append(card('Counterspell', 'response', foe, 'hand'))
    if mode == 'pending_bounced':
        setup.append(card('Unsummon', 'response', foe, 'hand'))
    entries = {entry['ref']: entry for entry in setup}
    zones = {ref: entry['zone'] for ref, entry in entries.items()}
    pools = [[0] * 6 for _ in (0, 1)]
    life = [20, 20]
    tapped = set()
    damage = {}
    bonus = {}
    actions = []
    replay_from = None

    def emit(op, **values):
        actions.append(step(op, **values))

    def observe(ref, field, value):
        emit('assert_card', ref=ref, property=field, value=value)

    def pool(player, value):
        pools[player] = list(value)
        for color, amount in enumerate(value):
            emit('set_mana', player=player, color=color, value=amount)
        emit('refresh')

    def pay(player, cost):
        pools[player] = [have - need for have, need in zip(pools[player], cost)]
        if min(pools[player]) < 0:
            raise ValueError('insufficient exact Oracle payment in Bloodrush witness')

    def stats(ref):
        record = oracle[entries[ref]['name']]
        power, toughness = bonus.get(ref, (0, 0))
        return int(record['power']) + power, int(record['toughness']) + toughness

    def receipt(stack=0):
        emit('assert_choices_consumed')
        emit('assert_stack', value=stack)
        for player in (0, 1):
            emit('assert_life', player=player, value=life[player])
            for color, amount in enumerate(pools[player]):
                emit('assert_mana', player=player, color=color, value=amount)
            for zone in ('hand', 'battlefield', 'graveyard', 'exile'):
                cards = Counter(rows[entry['name']]['csvid'] for ref, entry in entries.items()
                                if zones[ref] == zone and entry['player'] == player)
                emit('assert_zone', player=player, zone=zone, value=sum(cards.values()))
                for identity, count in sorted(cards.items()):
                    emit('assert_zone_multiplicity', player=player, zone=zone, index=identity, value=count)
        for ref, zone in zones.items():
            if zone != 'battlefield':
                continue
            power, toughness = stats(ref)
            for field, value in (('in_play', 1), ('tapped', int(ref in tapped)), ('damage', damage.get(ref, 0)),
                                 ('power', power), ('toughness', toughness), ('plus1_counters', 0), ('minus1_counters', 0)):
                observe(ref, field, value)

    def budget(cost):
        value = cost.copy()
        if mode.endswith('_short'):
            value[next(i for i, count in enumerate(value) if count)] -= 1
        elif mode.endswith('_wrong_color'):
            wrong = next((i for i in range(1, 6) if not value[i]), None)
            if wrong is None:
                raise ValueError('Bloodrush fixture needs an independent wrong mana color')
            colored = sum(value[1:])
            value[1:] = [colored if i == wrong else 0 for i in range(1, 6)]
        return value

    def attack(ref=None):
        emit('begin_attackers')
        if ref:
            emit('attack', ref=ref, value=1)
            tapped.add(ref)
        emit('finish_attackers')
        if ref:
            observe(ref, 'attacking', 1)

    def block(ref=None, blocked=False):
        emit('begin_blockers')
        if blocked:
            emit('block', ref='blocker', other=ref, value=1)
        emit('advance_combat')
        if blocked:
            observe(ref, 'blocked', 1)

    def finish_damage(ref=None, *, blocked=False, keyword=None):
        emit('combat_damage')
        emit('end_combat')
        if ref and zones[ref] == 'battlefield':
            power, toughness = stats(ref)
            if not blocked:
                life[defender] -= power * (2 if keyword == 'Double strike' else 1)
            else:
                blocking_power, blocking_toughness = stats('blocker')
                spill = max(0, power - blocking_toughness) if keyword == 'Trample' else 0
                dealt = power - spill
                if keyword in ('First strike', 'Double strike') and dealt >= blocking_toughness:
                    # A slain sole blocker does not assign normal damage. The
                    # attacker remains blocked in the second damage step.
                    blocking_power = 0
                elif keyword == 'Double strike':
                    dealt += power
                damage['blocker'], damage[ref] = dealt, blocking_power
                if dealt >= blocking_toughness or (dealt > 0 and keyword == 'Deathtouch'):
                    zones['blocker'] = 'graveyard'
                if blocking_power >= toughness:
                    zones[ref] = 'graveyard'
                life[defender] -= spill
        receipt()

    def clean():
        emit('cleanup')
        emit('refresh')
        damage.clear()
        bonus.clear()
        receipt()

    def next_own_turn():
        emit('advance_turn')
        emit('advance_turn')
        emit('untap_step', player=owner)
        emit('turn', player=owner, value=0)
        tapped.difference_update(ref for ref, entry in entries.items() if entry['player'] == owner)
        damage.clear()
        bonus.clear()

    def hand_query(available):
        emit('require_can_activate_hand', ref='source', value=int(available))

    def pay_hand():
        hand_query(True)
        emit('choose_dialog', player=owner, value=1)
        emit('choose_target', player=owner, ref='attacker')
        emit('activate_hand', ref='source', value=1)
        pay(owner, model['fee'])
        zones['source'] = 'graveyard'
        receipt(stack=1)

    emit('reset_death_history')
    emit('turn', player=active, value=0)
    emit('present_ai_dialogs', value=1)
    pool(0, [0] * 6)
    pool(1, [0] * 6)
    if mode in CAST_MODES:
        pool(owner, budget(model['cast']))
        success = mode not in ('cast_short', 'cast_wrong_color')
        emit('cast', ref='source', value=int(success))
        if success:
            pay(owner, model['cast'])
            emit('assert_stack', value=1)
            observe('source', 'in_hand', 0)
            if mode == 'cast_countered':
                fee = cost_from_oracle(oracle['Counterspell'])
                pool(foe, fee)
                emit('announce_spell', ref='source')
                emit('cast_response', ref='response', value=1)
                pay(foe, fee)
                emit('assert_stack', value=2)
                emit('resolve_response')
                zones['source'] = zones['response'] = 'graveyard'
            else:
                emit('resolve')
                zones['source'] = 'battlefield'
                observe('source', 'sick', 1)
            emit('refresh')
        receipt()
        if mode in ('printed_keyword', 'printed_double_hit'):
            next_own_turn()
            attack('source')
            blocked = mode == 'printed_keyword'
            block('source', blocked=blocked)
            finish_damage('source', blocked=blocked, keyword=model['keyword'])
    else:
        pool(owner, budget(model['fee']))
        if mode == 'combat_no_attack':
            attack()
        if mode in ('main_no_attack', 'combat_no_attack'):
            observe('attacker', 'attacking', 0)
            hand_query(False)
            emit('activate_hand', ref='source', value=0)
            receipt()
            if mode == 'combat_no_attack':
                block()
                finish_damage()
        else:
            attack('attacker')
            observe('bystander', 'attacking', 0)
            already_blocked = mode in ('post_block', 'double_blocker_gone')
            if already_blocked:
                block('attacker', blocked=True)
            effect = False
            if mode in ('hand_short', 'hand_wrong_color', 'activation_cancel'):
                cancelled = mode == 'activation_cancel'
                hand_query(cancelled)
                if cancelled:
                    emit('choose_dialog', player=owner, value=0)
                emit('activate_hand', ref='source', value=0)
                receipt()
            else:
                pay_hand()
                if mode == 'pending_replay':
                    replay_from = len(actions)
                if mode == 'pending_bounced':
                    fee = cost_from_oracle(oracle['Unsummon'])
                    pool(foe, fee)
                    emit('choose_target', player=foe, ref='attacker')
                    emit('cast', ref='response', value=1)
                    pay(foe, fee)
                    emit('assert_stack', value=2)
                    zones['attacker'], zones['response'] = 'hand', 'graveyard'
                else:
                    effect = True
                    bonus['attacker'] = (model['power_bonus'], model['toughness_bonus'])
                emit('resolve')
                emit('refresh')
                if not effect:
                    observe('attacker', 'in_play', 0)
                receipt()
            if not already_blocked:
                block()
            finish_damage('attacker', blocked=already_blocked, keyword=model['keyword'] if effect else None)
            if effect:
                clean()
            if mode == 'keyword_expires':
                next_own_turn()
                attack('attacker')
                block('attacker', blocked=True)
                finish_damage('attacker', blocked=True)
    if replay_from is not None:
        tail = actions[replay_from:]
        actions = (actions[:replay_from] + [step('capture')] + tail
                   + [step('record_state'), step('restore'), step('ai_roundtrip'), step('present_ai_dialogs', value=1)]
                   + tail + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                    untested_components=['Effects that specifically counter an ability on the stack.',
                                         'Multiple different hand abilities sharing the Rules Engine menu.'])
    return dict(name=f'BloodrushCreatureRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                seed=123, setup=setup, steps=actions, _recipe=metadata)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    if csvid != model['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(model)):
        raise ValueError('distinct explicit reviewed Bloodrush variants required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, model, variant, rows, oracle) for variant in selected]
