"""Whole-Oracle creatures that may block one additional attacker."""

import json
import re
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact


FAMILY = 'additional_blocker'
PROFILE = Path(__file__).parent / 'data/additional_blocker_wave8_profiles.json'
CLAUSE = 'This creature can block an additional creature each combat.'
BLOCK_CAPACITY = {CLAUSE: 1, 'This creature can block an additional seven creatures each combat.': 7,
                  'This creature can block any number of creatures.': None}
EXTENDED_ATTACKERS = {'three_ground': 3, 'four_ground': 4, 'eight_at_limit': 8, 'ninth_rejected': 9, 'unbounded_nine': 9}
PASSIVES = {
    "Defender (This creature can't attack.)": 'Defender',
    'Reach (This creature can block creatures with flying.)': 'Reach',
    "Trample (This creature can deal excess combat damage to the player or planeswalker it's attacking.)": 'Trample',
}
LIBRARY = ('Forest', 'Mountain', 'Plains', 'Grizzly Bears')
WITNESSES = set(LIBRARY) | {'Horned Turtle', 'Maritime Guard', 'Hill Giant', 'Suntail Hawk', 'Wall of Stone', 'Fog', 'Yoked Ox'}
COMMON = ('two_ground', 'reverse', 'third_rejected', 'duplicate_rejected', 'ordinary_limit', 'one_damage',
          'tapped', 'leave_unblocked', 'flyer_second', 'after_blocks_replay', 'next_combat', 'cast_then_block',
          'unfunded')
ATTACK_MODES = {'trample_spill', 'trample_no_excess', 'trample_unblocked'}
CAST_MODES = {'cast_then_block', 'unfunded', 'wrong_color'}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    lines = record.get('oracle_text', '').splitlines()
    keyword = PASSIVES.get(lines[0]) if len(lines) == 2 else None
    clause = lines[-1] if lines else ''
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not re.match(r'(?:Artifact )?Creature — ', record.get('type_line', ''))
            or clause not in BLOCK_CAPACITY or not (lines == [clause] or keyword)
            or record.get('keywords') != ([keyword] if keyword else [])
            or not re.fullmatch(r'\{[1-9]\}(?:\{[WUBRG]\}){0,2}', record.get('mana_cost', ''))
            or not re.fullmatch(r'(?:0|[1-9]\d?)', record.get('power', ''))
            or not re.fullmatch(r'[1-9]\d?', record.get('toughness', ''))):
        raise ValueError('whole fixed-cost normal creature with an additional block and reviewed passive required')
    params = dict(cost=cost_from_oracle(record), power=int(record['power']), toughness=int(record['toughness']),
                  reach=keyword == 'Reach', trample=keyword == 'Trample', additional=BLOCK_CAPACITY[clause])
    if keyword == 'Defender':
        params['defender'] = True
    return params


def variants(params):
    modes = list(COMMON)
    if any(params['cost'][1:]):
        modes.append('wrong_color')
    if params['reach']:
        modes.extend(('flyer_first', 'two_flyers'))
    if params['trample']:
        modes.extend(sorted(ATTACK_MODES))
    if params['additional'] != 1:
        modes.remove('third_rejected')
        modes.extend(('three_ground', 'four_ground'))
        modes.extend(('unbounded_nine',) if params['additional'] is None else ('eight_at_limit', 'ninth_rejected'))
    if params.get('defender'):
        modes.append('defender_rejected')
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in modes]


def spec(declaration, record):
    params = classify(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text']
            or not exact(profile()['sources'].get(record['name']), record)
            or not exact(declaration.get('params'), params)):
        raise ValueError('exact full Oracle and typed additional-block recipe required')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed additional-block variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profile()
    expected = data['sources'] | data['witnesses']
    if set(data['witnesses']) != WITNESSES or set(expected) != set(data['registry']):
        raise ValueError('complete source and witness inventory required')
    for name, info in expected.items():
        row = rows[name]
        if (not exact(oracle[name], info) or row['kind'] != 'card'
                or not exact(data['registry'][name], {key: row[key] for key in ('handler', 'csvid')})):
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in data['sources'].keys() | {'Fog'} and row['handler'] in ('card_dummy', 'NULL', '0'):
            raise ValueError(f'implemented rules handler required for {name}')


class Combat:
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.name, self.csvid, self.variant = name, csvid, variant
        self.owner, self.enemy = int(seat), 1 - int(seat)
        self.params, self.declaration, self.rows, self.oracle = params, declaration, rows, oracle
        self.setup, self.steps, self.creatures = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        mode = self.mode
        self.active = self.owner if mode in ATTACK_MODES | CAST_MODES | {'defender_rejected'} else self.enemy
        self.add(name, 'source', self.owner, 'hand' if mode in CAST_MODES else 'battlefield', tapped=mode in ('tapped', 'ordinary_limit'))
        if mode == 'defender_rejected':
            self.add('Grizzly Bears', 'ordinary', self.owner)
        elif mode in ATTACK_MODES:
            if mode != 'trample_unblocked':
                self.add('Wall of Stone' if mode == 'trample_no_excess' else 'Grizzly Bears', 'opposing_blocker', self.enemy)
        elif mode not in ('unfunded', 'wrong_color'):
            first = 'Suntail Hawk' if mode in ('flyer_first', 'two_flyers') else 'Horned Turtle'
            second = 'Suntail Hawk' if mode in ('flyer_second', 'two_flyers') else 'Maritime Guard'
            self.add(first, 'first', self.enemy)
            self.add(second, 'second', self.enemy)
            if mode == 'third_rejected':
                self.add('Hill Giant', 'third', self.enemy)
            for index in range(3, EXTENDED_ATTACKERS.get(mode, 2) + 1):
                limit_probe = mode in ('eight_at_limit', 'ninth_rejected') and index >= 8
                attacker = 'Suntail Hawk' if limit_probe else 'Yoked Ox'
                self.add(attacker, f'attacker_{index}', self.enemy)
            if mode == 'ordinary_limit':
                self.add('Grizzly Bears', 'ordinary', self.owner)
            if mode == 'next_combat':
                self.add('Fog', 'fog', self.enemy, 'hand')
        for player in (0, 1):
            self.add('Forest', f'grave_{player}', player, 'graveyard')
            self.add('Plains', f'exile_{player}', player, 'exile')
            for index, item in enumerate(LIBRARY):
                self.add(item, f'library_{player}_{index}', player, 'library')
        self.snapshot = 0

    def add(self, name, ref, who, zone='battlefield', *, tapped=False):
        from recipes import card

        item = card(name, ref, who, zone)
        if zone == 'battlefield':
            item['state'] = ['tapped' if tapped else 'untapped']
        if zone in ('battlefield', 'hand'):
            record = self.oracle[name]
            stats = tuple(int(record[key]) for key in ('power', 'toughness')) if 'power' in record else None
            self.creatures[ref] = dict(name=name, who=who, stats=stats, zone=zone, tapped=tapped, damage=0)
        self.zones[zone][who].append(name)
        self.setup.append(item)

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def observe(self, ref, property, value):
        self.emit('assert_card', ref=ref, property=property, value=int(value))

    def move(self, ref, destination):
        entity = self.creatures[ref]
        name, owner, zone = (entity[key] for key in ('name', 'who', 'zone'))
        self.zones[zone][owner].remove(name)
        self.zones[destination][owner].append(name)
        entity['zone'] = destination

    def pools(self, amounts):
        self.mana = [list(amount) for amount in amounts]
        for who, pool in enumerate(self.mana):
            for color, value in enumerate(pool):
                self.emit('set_mana', player=who, color=color, value=value)
        self.emit('refresh')

    def receipts(self, *, unknown_attacker_zones=False):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        for ref, entity in self.creatures.items():
            if entity['zone'] != 'battlefield' or (unknown_attacker_zones and entity['who'] == self.enemy):
                continue
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'tapped', entity['tapped'])
            for field, amount in zip(('power', 'toughness', 'damage'), (*entity['stats'], entity['damage'])):
                self.observe(ref, field, amount)
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for color, value in enumerate(self.mana[who]):
                self.emit('assert_mana', player=who, color=color, value=value)
            for zone, per_player in self.zones.items():
                names = per_player[who]
                if unknown_attacker_zones and who == self.enemy and zone in ('battlefield', 'graveyard'):
                    continue
                self.emit('assert_zone', player=who, zone=zone, value=len(names))
                if zone == 'library':
                    for i, name in enumerate(names):
                        self.emit('assert_zone_card', player=who, zone=zone, index=i, value=self.rows[name]['csvid'])
                else:
                    counts = Counter(names)
                    candidates = {self.name} | {entity['name'] for entity in self.creatures.values()}
                    for name in sorted(candidates):
                        self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=counts[name])

    def cast(self, ref, cost, *, denied=False):
        self.emit('cast', ref=ref, value=int(not denied))
        if denied:
            self.observe(ref, 'in_hand', 1)
            self.receipts()
            return
        self.emit('require_card', ref=ref, property='in_hand', value=0)
        self.emit('assert_stack', value=1)
        who = self.creatures[ref]['who']
        self.mana[who] = [have - paid for have, paid in zip(self.mana[who], cost)]
        for color, value in enumerate(self.mana[who]):
            self.emit('assert_mana', player=who, color=color, value=value)
        self.emit('resolve')
        self.emit('refresh')
        destination = 'graveyard' if self.creatures[ref]['stats'] is None else 'battlefield'
        self.move(ref, destination)
        self.receipts()

    def attack(self, attackers):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref=ref, value=1)
            self.creatures[ref]['tapped'] = True
            self.observe(ref, 'attacking', 1)
        self.emit('finish_attackers')

    def declare_blocks(self, attackers, assignments):
        self.emit('begin_blockers')
        accepted = {}
        for blocker, attacker, legal in assignments:
            self.emit('block', ref=blocker, other=attacker, value=legal)
            if legal:
                accepted[attacker] = blocker
            self.observe(blocker, 'blocking_count', sum(b == blocker for b in accepted.values()))
        self.emit('advance_combat')
        for attacker in attackers:
            self.observe(attacker, 'blocked', attacker in accepted)
        self.receipts()
        return accepted

    def damage(self, attackers, accepted, defending, *, prevented=False):
        grouped = Counter(accepted.values())
        for blocker, count in grouped.items():
            if count > 1 and self.creatures[blocker]['who'] == 0:
                targets = [attacker for attacker in attackers if accepted.get(attacker) == blocker]
                power = self.creatures[blocker]['stats'][0]
                # Each unbanded attacker is a separate damage group scanned
                # in fixture slot order. Assign all power to the first legal
                # group. Repeated ordinary picks spend one point apiece.
                for _ in range(power):
                    self.emit('choose_target', ref=targets[0], player=0)
        self.emit('combat_damage')
        split = any(count > 1 for count in grouped.values()) and not prevented
        if not prevented:
            for attacker in attackers:
                power, _ = self.creatures[attacker]['stats']
                if attacker not in accepted:
                    self.life[defending] -= power
                    continue
                blocker = accepted[attacker]
                block_power, block_toughness = self.creatures[blocker]['stats']
                self.creatures[blocker]['damage'] += power
                if grouped[blocker] == 1:
                    self.creatures[attacker]['damage'] += block_power
                if attacker == 'source' and self.params['trample']:
                    self.life[defending] -= max(0, power - block_toughness)
            for ref, entity in self.creatures.items():
                if entity['zone'] == 'battlefield' and entity['damage'] >= entity['stats'][1]:
                    self.move(ref, 'graveyard')
        self.emit('resolve')
        self.emit('refresh')
        self.receipts(unknown_attacker_zones=split)
        self.emit('end_combat')
        source_survives = self.creatures['source']['zone'] == 'battlefield'
        if source_survives and 'source' not in grouped:
            self.observe('source', 'blocking_count', 0)
        self.emit('cleanup')
        if source_survives and 'source' in grouped:
            # End combat forgets active blocking, but the observer reads the
            # previous assignment indices until actual turn cleanup resets them.
            self.observe('source', 'blocking_count', 0)
        for entity in self.creatures.values():
            entity['damage'] = 0
        self.emit('refresh')
        self.receipts(unknown_attacker_zones=split)

    def defending_combat(self, *, first_round=False):
        mode = self.mode
        attackers = ['first', 'second'] + (['third'] if mode == 'third_rejected' else [])
        attackers += [f'attacker_{index}' for index in range(3, EXTENDED_ATTACKERS.get(mode, 2) + 1)]
        self.attack(attackers)
        attempts = [('source', 'first', 1), ('source', 'second', 1)]
        attempts.extend(('source', attacker, int(index != 9 or mode != 'ninth_rejected'))
                        for index, attacker in enumerate(attackers[2:], 3) if mode in EXTENDED_ATTACKERS)
        if mode == 'reverse':
            attempts.reverse()
        elif mode == 'duplicate_rejected':
            attempts.insert(1, ('source', 'first', 0))
        elif mode == 'third_rejected':
            attempts.append(('source', 'third', 0))
        elif mode == 'ordinary_limit':
            attempts = [('ordinary', 'first', 1), ('ordinary', 'second', 0)]
        elif mode in ('one_damage', 'flyer_second'):
            attempts = attempts[:1] if mode == 'one_damage' else [attempts[0], ('source', 'second', int(self.params['reach']))]
        elif mode == 'tapped':
            attempts = [('source', name, 0) for name in attackers]
        elif mode == 'leave_unblocked':
            attempts = []
        result = self.declare_blocks(attackers, attempts)
        if mode == 'after_blocks_replay':
            self.snapshot = len(self.steps)
        if first_round:
            from recipes import cost_from_oracle

            cost = cost_from_oracle(self.oracle['Fog'])
            mana = [[0] * 6, [0] * 6]
            mana[self.enemy] = cost
            self.pools(mana)
            self.cast('fog', cost)
        self.damage(attackers, result, self.owner, prevented=first_round)

    def case(self):
        from recipes import step

        self.emit('turn', player=self.active, value=0)
        self.pools([[0] * 6, [0] * 6])
        if self.mode in CAST_MODES:
            fee = self.params['cost']
            mana = [[0] * 6, [0] * 6]
            if self.mode == 'cast_then_block':
                mana[self.owner] = fee
            if self.mode == 'wrong_color':
                wrong = next(color for color in range(1, 6) if fee[color] == 0)
                mana[self.owner][wrong] = sum(fee)
            self.pools(mana)
        self.snapshot = len(self.steps)
        self.receipts()
        if self.mode in CAST_MODES:
            denied = self.mode != 'cast_then_block'
            self.cast('source', fee, denied=denied)
            if not denied:
                self.emit('advance_turn')
                self.emit('untap_step', player=self.enemy)
                self.emit('turn', player=self.enemy, value=0)
                self.observe('source', 'sick', 1)
                self.defending_combat()
        elif self.mode == 'defender_rejected':
            self.emit('begin_attackers')
            self.emit('attack', ref='source', value=0)
            self.observe('source', 'attacking', 0)
            self.emit('attack', ref='ordinary', value=1)
            self.creatures['ordinary']['tapped'] = True
            self.observe('ordinary', 'attacking', 1)
            self.emit('finish_attackers')
            attacks = ['ordinary']
            self.damage(attacks, self.declare_blocks(attacks, []), self.enemy)
        elif self.mode in ATTACK_MODES:
            attacks = ['source']
            self.attack(attacks)
            attempts = [] if self.mode == 'trample_unblocked' else [('opposing_blocker', 'source', 1)]
            self.damage(attacks, self.declare_blocks(attacks, attempts), self.enemy)
        else:
            repeat = self.mode == 'next_combat'
            self.defending_combat(first_round=repeat)
            if repeat:
                self.emit('advance_turn')
                self.emit('untap_step', player=self.owner)
                self.emit('advance_turn')
                self.emit('untap_step', player=self.enemy)
                self.emit('turn', player=self.enemy, value=0)
                for attacker in ('first', 'second'):
                    self.creatures[attacker]['tapped'] = False
                    self.observe(attacker, 'tapped', 0)
                self.defending_combat()
        suffix = self.steps[self.snapshot:]
        steps = self.steps[:self.snapshot] + [step('capture')] + suffix
        steps += [step('record_state'), step('restore')] + suffix + [step('assert_recorded_state')]
        return dict(name=f'AdditionalBlockerRecipe_{self.csvid}_{self.variant}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=self.variant,
                        params=self.declaration['params'], oracle_text=self.declaration['oracle_text'], untested_components=[
                            'Multi-block cases allow any legal outgoing combat damage split and independently check both assignments, incoming damage and player life. One-block cases check the exact outgoing exchange.',
                            'Planeswalkers, damage allocation UI, banding and additional combat creation are outside these fixtures.',
                        ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    return [Combat(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]
