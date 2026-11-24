"""Actual repeated Kicker payments and their entry-counter consequences."""

from collections import Counter
import json
from pathlib import Path
import re

from counted_battlefield_damage_recipes import Game
from counter_condition_recurrence_recipes import exact


FAMILY = 'kicker_multikicker_counters'
PROFILE = Path(__file__).parent / 'data/kicker_multikicker_counters_wave8_profiles.json'
KICKER = re.compile(r'Multikicker (\{1\}\{([BUGRW])\}) \(You may pay an additional \1 any number of times as you cast this spell\.\)')
ENTRY = 'This creature enters with a +1/+1 counter on it for each time it was kicked.'
ELVES = 'Other Elf creatures you control get +1/+1 for each +1/+1 counter on this creature.'
ISLAND = "Islandwalk (This creature can't be blocked as long as defending player controls an Island.)"
SWAMP = "Swampwalk (This creature can't be blocked as long as defending player controls a Swamp.)"
RIDERS = {
    (): ('none', ()),
    ('Flying',): ('flying', ('Flying',)),
    ('Haste',): ('haste', ('Haste',)),
    (ISLAND,): ('Island', ('Landwalk', 'Islandwalk')),
    (SWAMP,): ('Swamp', ('Landwalk', 'Swampwalk')),
    (ELVES,): ('elves', ()),
}
WITNESSES = frozenset({'Counterspell', 'Drift of Phantasms', 'Elvish Mystic', 'Island', 'Mountain',
                       'Plains', 'Saltfield Recluse', 'Swamp'})
MODES = {
    'declined': (0, True), 'once': (1, True), 'twice': (2, True), 'thrice': (3, True),
    'exact_two': (2, False), 'short_second': (1, False), 'wrong_kicker_color': (0, False),
    'short_base': (0, False), 'countered': (2, True), 'replay': (3, True),
}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record['type_line'].startswith('Creature — ')
            or any(not re.fullmatch(r'[1-9][0-9]*', record[key]) for key in ('power', 'toughness'))):
        raise ValueError('Normal creature with fixed printed stats required')
    first, *rules = record['oracle_text'].splitlines()
    kicker = KICKER.fullmatch(first)
    other = tuple(rule for rule in rules if rule != ENTRY)
    if not kicker or rules.count(ENTRY) != 1 or other not in RIDERS:
        raise ValueError('Unsupported whole multikicker counter composition')
    mode, keywords = RIDERS[other]
    expected = {'Multikicker', *keywords}
    if len(record['keywords']) != len(expected) or set(record['keywords']) != expected:
        raise ValueError('Printed multikicker keyword metadata disagrees')
    printed = cost_from_oracle(record)
    fee = cost_from_oracle({'mana_cost': kicker[1]})
    if any(due and not fee[color] for color, due in enumerate(printed)):
        raise ValueError('Another base color needs a new payment fixture')
    return dict(printed=printed, fee=fee, rider=mode)


def variants(shape):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES]


def spec(declaration, record):
    model = parameters(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model)):
        raise ValueError('Exact complete multikicker Oracle declaration required')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(name, str) for name in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(model))):
        raise ValueError('Distinct reviewed multikicker payment variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profile()
    if set(data['witnesses']) != WITNESSES or data['menu'] != {'pay': 0, 'pass': 1}:
        raise ValueError('Multikicker witness or production choice pins changed')
    for name, pin in (data['sources'] | data['witnesses']).items():
        row = rows.get(name, {})
        if (row.get('kind') != 'card' or row.get('handler') in (None, 'card_dummy', 'NULL', '0')
                or not exact(row, pin['registry']) or not exact(oracle.get(name), pin['oracle'])):
            raise ValueError(f'Whole implemented Oracle or registry pin changed: {name}')
    return data


class CounterKickerBoard(Game):
    def build(self):
        rider = self.shape['rider']
        self.number, self.offer_pass = MODES[self.mode]
        self.entry_counters = 0
        self.life = [20, 20]
        if rider == 'flying':
            self.add('Saltfield Recluse', 'ground', self.foe)
            self.add('Drift of Phantasms', 'flyer', self.foe)
        if rider in ('Island', 'Swamp'):
            self.add('Drift of Phantasms', 'blocker', self.foe)
            self.add(rider if self.number else 'Mountain', 'land', self.foe, tapped=True)
            if not self.number:
                self.add(rider, 'attacking_matching', self.owner, tapped=True)
        if rider == 'elves':
            self.add('Elvish Mystic', 'ally_elf', self.owner, tapped=True)
            self.add('Elvish Mystic', 'foe_elf', self.foe, tapped=True)
            self.add('Saltfield Recluse', 'other_creature', self.owner)
        if self.mode == 'countered':
            self.add('Counterspell', 'counter', self.foe, 'hand')
        for seat in (0, 1):
            self.add('Plains', f'library_{seat}', seat, 'library')

    def stat(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=value)

    def receipts(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.observe_mana()
        counts = Counter((card['player'], card['zone'], self.rows[card['name']]['csvid']) for card in self.cards.values())
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ('hand', 'battlefield', 'graveyard', 'exile', 'library'):
                multiplicities = [(csvid, n) for (seat, place, csvid), n in counts.items()
                                  if seat == player and place == zone]
                self.emit('assert_zone', player=player, zone=zone, value=sum(n for _, n in multiplicities))
                for csvid, count in sorted(multiplicities):
                    self.emit('assert_zone_multiplicity', player=player, zone=zone, index=csvid, value=count)
            self.emit('assert_zone_card', player=player, zone='library', index=0, value=self.rows['Plains']['csvid'])
        for ref, card in self.cards.items():
            if card['zone'] not in ('hand', 'battlefield'):
                continue
            self.stat(ref, 'in_hand', int(card['zone'] == 'hand'))
            self.stat(ref, 'in_play', int(card['zone'] == 'battlefield'))
            if card['zone'] == 'battlefield' and self.oracle[card['name']].get('power') is not None:
                extra = self.entry_counters if ref in ('source', 'ally_elf') else 0
                for stat in ('power', 'toughness'):
                    self.stat(ref, stat, int(self.oracle[card['name']][stat]) + extra)
                self.stat(ref, 'damage', card['damage'])
        if self.cards['source']['zone'] == 'battlefield':
            self.emit('assert_counter', ref='source', counter_kind='plus1', value=self.entry_counters)

    def budget(self):
        printed, fee = self.shape['printed'], self.shape['fee']
        color, = (index for index, need in enumerate(fee) if index and need)
        remaining = [0] * 6
        if self.offer_pass:
            remaining = fee[:]
        if self.mode == 'short_second':
            remaining[color] = 1
        if self.mode == 'wrong_kicker_color':
            wrong = 2 if color == 1 else 1
            remaining[0] = remaining[wrong] = 1
        pool = [required + self.number * each + spare for required, each, spare in zip(printed, fee, remaining)]
        if self.mode == 'short_base':
            pool[0] += 4
            pool[color] -= 1
            remaining = pool[:]
        return pool, remaining

    def cast_kicked(self, menu, remaining):
        for _ in range(self.number):
            self.emit('choose_dialog', player=self.owner, value=menu['pay'])
        if self.offer_pass:
            self.emit('choose_dialog', player=self.owner, value=menu['pass'])
        allowed = self.mode != 'short_base'
        self.emit('cast', ref='source', value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed))
        self.stat('source', 'in_play', 0)
        self.pools[self.owner] = remaining
        self.observe_mana()
        if allowed:
            self.stat('source', 'in_hand', 0)
            self.cards['source']['zone'] = 'stack'

    def settle(self):
        if self.mode == 'countered':
            from recipes import cost_from_oracle

            self.emit('announce_spell', ref='source')
            # Counterspell takes its target from the pending spell, without a target menu.
            self.cast('counter', response=True, funding=cost_from_oracle(self.oracle['Counterspell']))
            self.emit('resolve_response')
            self.cards['counter']['zone'] = 'graveyard'
            self.cards['source']['zone'] = 'graveyard'
        else:
            self.emit('resolve')
            self.cards['source']['zone'] = 'battlefield'
            self.entry_counters = self.number
        self.emit('refresh')
        self.receipts()

    def combat(self):
        rider = self.shape['rider']
        if rider != 'haste':
            self.emit('advance_turn')
            self.emit('advance_turn')
        if rider == 'flying':
            attempts = [('ground', False), ('flyer', True)]
        elif rider in ('Island', 'Swamp'):
            attempts = [('blocker', not self.number)]
        else:
            attempts = []
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for ref, legal in attempts:
            self.emit('block', ref=ref, other='source', value=int(legal))
        self.emit('advance_combat')
        blocked = any(legal for _, legal in attempts)
        self.stat('source', 'blocked', int(blocked))
        amount = int(self.oracle[self.cards['source']['name']]['power']) + self.entry_counters
        for ref, legal in attempts:
            if legal:
                self.cards[ref]['damage'] = amount
        if not blocked:
            self.life[self.foe] -= amount
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('refresh')
        # Real turn changes can drain the residual pool. Only the cast and
        # resolution checkpoints measure the payment; combat has no mana event.
        for ref, card in self.cards.items():
            if card['zone'] == 'battlefield' and self.oracle[card['name']].get('power') is not None:
                self.stat(ref, 'damage', card['damage'])
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
        self.emit('assert_choices_consumed')

    def run(self, menu):
        from recipes import step

        pool, remaining = self.budget()
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        self.fund(self.foe, [0] * 6)
        self.fund(self.owner, pool)
        self.stat('source', 'in_hand', 1)
        boundary = len(self.steps)
        self.cast_kicked(menu, remaining)
        if self.mode == 'short_base':
            self.receipts()
            return
        self.settle()
        if self.mode in ('declined', 'twice') and self.shape['rider'] in ('flying', 'Island', 'Swamp', 'haste'):
            self.combat()
        if self.mode == 'replay':
            prefix, tail = self.steps[:boundary], self.steps[boundary:]
            self.steps = [*prefix, step('capture'), *tail, step('record_state'), step('restore'),
                          step('ai_roundtrip'), *tail, step('assert_recorded_state')]


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    data = validate_witnesses(oracle, rows)
    if (name not in data['sources'] or rows[name]['csvid'] != csvid or not selected
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('Pinned owner/card/scope required for multikicker cases')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        board = CounterKickerBoard(name, shape, mode, int(owner), rows, oracle)
        board.run(data['menu'])
        cases.append(dict(name=f'KickerMultikickerCountersRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2334, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'],
                                       covered_clauses=declaration['oracle_text'].splitlines(),
                                       untested_components=['Cost reducers, copied spells, multiple simultaneous Elf effects, and replacement effects.'])))
    return cases
