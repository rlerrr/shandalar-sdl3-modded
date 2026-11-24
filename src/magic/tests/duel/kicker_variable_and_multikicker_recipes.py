"""Whole Oracle variable Kicker and multikicker cast, target and entry cases."""

from collections import Counter
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'kicker_variable_and_multikicker'
PROFILE = Path(__file__).parent / 'data/kicker_variable_and_multikicker_wave8_profiles.json'
VARIABLE = {5020: 2, 5174: 0}
MULTI = {10731: '{B}', 10753: '{R}', 10794: '{W}', 10798: '{1}{W}'}
COMET = 10744
TARGET_PLAYER = 'choose_player'
ZERO = [0] * 6
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
WITNESSES = frozenset({'Island', 'Plains', 'Forest', 'Jace Beleren', 'Saltfield Recluse',
                       'Drift of Phantasms', 'Aven Fisher', 'Lumberknot', 'Saproling',
                       'Elvish Mystic', 'Bottle Gnomes', 'Prodigal Sorcerer', 'Mountain', 'Counterspell'})
MODES = ('declined', 'paid', 'once', 'short_base', 'countered', 'replay')
RIDER = {
    ('When this creature enters, target opponent discards a card for each time it was kicked.',):
        (10731, 'discard', '{B}', 'Creature', ''),
    ('When this creature enters, it deals damage to target player or planeswalker equal to twice the number of times it was kicked.',):
        (10753, 'damage', '{R}', 'Creature', ''),
    ('Flying', 'When this creature enters, you gain 2 life for each time it was kicked.'):
        (10794, 'life', '{W}', 'Creature', 'Flying'),
    ('Creatures you control get +1/+1.', 'When this enchantment enters, return up to X target creature cards from your graveyard to the battlefield, where X is the number of times this enchantment was kicked.'):
        (10798, 'return', '{1}{W}', 'Enchantment', ''),
    ('Choose any target, then choose another target for each time this spell was kicked. Comet Storm deals X damage to each of them.',):
        (COMET, 'cast_targets', '{1}', 'Instant', ''),
    ('Flying', 'When Kangee enters, if it was kicked, put X feather counters on it.',
     'Other Bird creatures get +1/+1 for each feather counter on Kangee.'):
        (5020, 'birds', '{X}{2}', 'Legendary Creature', 'Flying'),
    ('Saproling creatures and other Treefolk creatures get +1/+1.',
     'When Verdeloth enters, if it was kicked, create X 1/1 green Saproling creature tokens.'):
        (5174, 'saprolings', '{X}', 'Legendary Creature', ''),
}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('Whole normal Oracle record required')
    first, *other = record['oracle_text'].splitlines()
    match = RIDER.get(tuple(other))
    if match is None:
        raise ValueError('Unreviewed complete kicker reward')
    csvid, effect, fee, kind, keyword = match
    tag = 'Kicker' if csvid in VARIABLE else 'Multikicker'
    repeat = ' any number of times' if tag == 'Multikicker' else ''
    expected = {tag, keyword} if keyword else {tag}
    base = record['mana_cost']
    if (set(record['keywords']) != expected or len(record['keywords']) != len(expected)
            or first != f'{tag} {fee} (You may pay an additional {fee}{repeat} as you cast this spell.)'
            or record['type_line'].split(' — ', 1)[0] != kind
            or (base == '{X}{R}{R}') != (csvid == COMET)):
        raise ValueError('Printed type, keywords or complete kicker cost disagree')
    if 'Creature' in kind and any(not re.fullmatch('[1-9][0-9]*', record[key]) for key in ('power', 'toughness')):
        raise ValueError('Fixed creature stats required')
    cost_from_oracle({'mana_cost': base.replace('{X}', '')})
    return dict(effect=effect, kicker_cost=fee, base_cost=base, csvid=csvid)


def variants(shape):
    return [f'{mode}_{actor}' for actor in (0, 1) for mode in MODES]


def spec(declaration, record):
    shape = parameters(record)
    selection = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('Exact whole kicker declaration required')
    if (not isinstance(selection, list) or not selection or any(not isinstance(item, str) for item in selection)
            or len(set(selection)) != len(selection) or set(selection) - set(variants(shape))):
        raise ValueError('Distinct declared kicker controls required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    if set(pins['witnesses']) != WITNESSES:
        raise ValueError('Independent witness set changed')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0', 'card_dummy')
                or not exact(row, pin['registry']) or not exact(oracle.get(name), pin['oracle'])):
            raise ValueError(f'Whole Oracle or registry pin changed: {name}')
    return pins


class Case:
    def __init__(self, record, csvid, actor, mode, rows, oracle):
        self.source, self.csvid, self.actor, self.enemy, self.mode = record, csvid, actor, 1 - actor, mode
        self.paid = mode not in ('declined', 'short_base')
        self.count = 1 if mode == 'once' else 2 * int(self.paid)
        self.x = (1 if mode == 'once' else 2) if csvid in VARIABLE else 2
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps = [], []
        self.add('source', actor, record['name'], 0)
        self.expect_zones = Counter()
        if csvid == 10731:
            self.add('keeper', actor, 'Island', 0)
            for i in range(2):
                self.add(f'victim_{i}', self.enemy, 'Plains', 0)
        if csvid in (10753,):
            self.add('walker', self.enemy, 'Jace Beleren')
        if csvid in (10794, 5020, COMET):
            self.add('ground', self.enemy, 'Saltfield Recluse')
        if csvid in (10794, COMET):
            self.add('flyer', self.enemy, 'Drift of Phantasms')
        if csvid == 5020:
            self.add('own_bird', actor, 'Aven Fisher')
            self.add('foe_bird', self.enemy, 'Aven Fisher')
        if csvid == 5174:
            self.add('own_tree', actor, 'Lumberknot')
            self.add('foe_tree', self.enemy, 'Lumberknot')
            self.add('own_sap', actor, 'Saproling', flags=32)
            self.add('foe_sap', self.enemy, 'Saproling', flags=32)
        if csvid == 10798:
            self.add('own_elf', actor, 'Elvish Mystic', flags=1)
            self.add('foe_elf', self.enemy, 'Elvish Mystic')
            self.add('first_grave', actor, 'Bottle Gnomes', 3)
            self.add('second_grave', actor, 'Prodigal Sorcerer', 3)
            self.add('land_grave', actor, 'Forest', 3)
            self.add('foe_grave', self.enemy, 'Prodigal Sorcerer', 3)
        if self.mode == 'countered':
            self.add('counter', self.enemy, 'Counterspell', 0)
        for seat in (0, 1):
            self.add(f'deck_{seat}', seat, 'Mountain', 2)
        self.expect_zones.update((seat, zone, card) for _, seat, zone, card, _ in self.setup)
        self.life = [20, 20]

    def add(self, ref, seat, name, zone=1, *, flags=0):
        self.setup.append((ref, seat, zone, self.rows[name]['csvid'], flags))

    def emit(self, op, ref='', other='', property_name='', *, player=None, value=None,
             zone=None, index=None, color=None):
        action = dict(op=op)
        for field, field_value in (
                ('ref', ref or None), ('other', other or None), ('property', property_name or None),
                ('player', player), ('value', value), ('zone', None if zone is None else ZONES[zone]),
                ('index', index), ('color', color)):
            if field_value is not None:
                action[field] = field_value
        self.steps.append(action)

    def mana(self, op, actor, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=actor, value=value, color=color)

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref, property_name=property_name, value=value)

    def combat(self, attacker, blocker):
        from recipes import step

        self.steps.extend([
            step('begin_attackers'), step('attack', ref=attacker, value=1),
            step('finish_attackers'), step('begin_blockers'),
            step('block', ref=blocker, other=attacker, value=0), step('advance_combat'),
            step('assert_card', ref=attacker, property='blocked', value=0), step('combat_damage'),
            step('resolve'), step('end_combat'), step('assert_choices_consumed'),
        ])

    def move(self, seat, name, origin, destination):
        card = self.rows[name]['csvid']
        assert self.expect_zones[seat, origin, card] > 0
        self.expect_zones[seat, origin, card] -= 1
        self.expect_zones[seat, destination, card] += 1

    def expected_board(self):
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            for zone in (0, 1, 2, 3, 4):
                cards = [(card, n) for (p, z, card), n in self.expect_zones.items() if p == seat and z == zone]
                self.emit('assert_zone', player=seat, zone=zone, value=sum(n for _, n in cards))
                for card, n in sorted(cards):
                    self.emit('assert_zone_multiplicity', player=seat, zone=zone, index=card, value=n)
            self.emit('assert_zone_card', player=seat, zone=2, index=0, value=self.rows['Mountain']['csvid'])

    def setup_cast(self):
        record, card = self.source, self.csvid
        spell_x = card == COMET
        base = cost_from_oracle({'mana_cost': record['mana_cost'].replace('{X}', '')})
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        if card == 10731 and self.enemy == 0:
            self.emit('discard_policy', value=1)
        if card in MULTI:
            fee = cost_from_oracle({'mana_cost': MULTI[card]})
            first = f'Multikicker {MULTI[card]} (You may pay an additional {MULTI[card]} any number of times as you cast this spell.)'
            assert record['oracle_text'].splitlines()[0] == first
            pool = [due + (self.count + 1) * extra for due, extra in zip(base, fee)]
            remaining = fee
        elif card in VARIABLE:
            fixed = VARIABLE[card]
            first = '{X}{2}' if fixed else '{X}'
            assert record['oracle_text'].splitlines()[0] == f'Kicker {first} (You may pay an additional {first} as you cast this spell.)'
            pool = base[:]
            pool[0] += fixed + self.x
            remaining = ZERO if self.paid else [fixed + self.x, 0, 0, 0, 0, 0]
        else:
            assert spell_x and record['mana_cost'] == '{X}{R}{R}'
            assert record['oracle_text'].splitlines()[0] == 'Multikicker {1} (You may pay an additional {1} any number of times as you cast this spell.)'
            pool = base[:]
            pool[0] = self.x + self.count
            remaining = ZERO
        if self.mode == 'short_base':
            pool = base[:]
            required = next(index for index, amount in enumerate(base) if index and amount)
            pool[0] += 3
            pool[required] -= 1
            remaining = pool[:]
        self.mana('set_mana', self.actor, pool)
        self.mana('set_mana', self.enemy, ZERO)
        self.emit('refresh')
        self.observe('source', 'in_hand', 1)
        self.before_choices = len(self.steps)
        if self.mode == 'short_base':
            self.emit('cast', 'source', value=0)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=0)
            self.observe('source', 'in_hand', 1)
            return remaining
        if spell_x:
            self.emit('choose_x', player=self.actor, value=self.x)
            if self.actor == 0:
                self.emit(TARGET_PLAYER, player=self.actor, value=self.enemy)
            # The legacy AI-specific path auto-selects the opponent first.
            for ref in ('ground', 'flyer')[:self.count]:
                self.emit('choose_target', ref, player=self.actor)
        elif card in VARIABLE:
            self.emit('choose_dialog', player=self.actor, value=int(not self.paid))
            if self.paid:
                self.emit('choose_x', player=self.actor, value=self.x)
        else:
            for _ in range(self.count):
                self.emit('choose_dialog', player=self.actor, value=0)
            self.emit('choose_dialog', player=self.actor, value=1)
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_play', 0)
        self.mana('assert_mana', self.actor, remaining)
        self.mana('assert_mana', self.enemy, ZERO)
        return remaining

    def enter(self):
        if self.csvid == 10731 and self.paid:
            self.emit(TARGET_PLAYER, player=self.actor, value=self.enemy)
        if self.csvid == 10753:
            if self.count == 2:
                self.emit('choose_target', 'walker', player=self.actor)
            else:
                self.emit(TARGET_PLAYER, player=self.actor, value=self.enemy)
            # A correct single triggered ability leaves this legal fallback unused.
            self.emit(TARGET_PLAYER, player=self.actor, value=self.enemy)
        returns = ('Bottle Gnomes', 'Prodigal Sorcerer')[:self.count]
        if self.csvid == 10798 and self.paid:
            for name in returns:
                self.emit('choose_zone_card', player=self.actor, value=self.rows[name]['csvid'])
        self.emit('resolve')
        self.emit('assert_unused_choices', value=int(self.csvid == 10753))
        self.emit('assert_stack', value=0)
        self.emit('refresh')
        self.move(self.actor, self.source['name'], 0, 3 if self.csvid == COMET else 1)
        if self.csvid != COMET:
            self.observe('source', 'in_play', 1)
        if self.source.get('power'):
            for prop in ('power', 'toughness'):
                self.observe('source', prop, int(self.source[prop]))
        if self.csvid == 10731 and self.paid:
            for _ in range(self.count):
                self.move(self.enemy, 'Plains', 0, 3)
        if self.csvid == 10753:
            if self.count == 2:
                self.move(self.enemy, 'Jace Beleren', 1, 3)
            else:
                self.life[self.enemy] -= 2 * self.count
        if self.csvid == 10794:
            self.life[self.actor] += 2 * self.count
        if self.csvid == 10798:
            for ref in ('own_elf', 'foe_elf'):
                for prop in ('power', 'toughness'):
                    self.observe(ref, prop, 2 if ref == 'own_elf' else 1)
            if self.paid:
                for i, name in enumerate(returns):
                    self.move(self.actor, name, 3, 1)
                    ref = f'returned_{i}'
                    self.emit('bind_permanent', ref, player=self.actor, value=self.rows[name]['csvid'], index=-1)
                    for prop in ('power', 'toughness'):
                        self.observe(ref, prop, int(self.oracle[name][prop]) + 1)
        if self.csvid == 5020:
            for ref in ('own_bird', 'foe_bird'):
                for prop in ('power', 'toughness'):
                    self.observe(ref, prop, 2 + self.x * int(self.paid))
        if self.csvid == 5174:
            token = self.rows['Saproling']['csvid']
            self.expect_zones[self.actor, 1, token] += self.x * int(self.paid)
            for ref in ('own_tree', 'foe_tree'):
                for prop in ('power', 'toughness'):
                    self.observe(ref, prop, 2)
            for seat in (0, 1):
                for prop, value in (('token', 1), ('subtype_Saproling', 1), ('color', 8),
                                    ('power', 2), ('toughness', 2)):
                    self.emit('assert_matching_permanents', property_name=prop, player=seat, index=token, value=value)
        if self.csvid == COMET:
            self.life[self.enemy] -= self.x
            if self.paid:
                self.move(self.enemy, 'Saltfield Recluse', 1, 3)
            else:
                self.observe('ground', 'damage', 0)
            self.observe('flyer', 'damage', self.x * int(self.count == 2))

    def combat_rider(self):
        if self.csvid not in (10794, 5020):
            return
        self.emit('advance_turn')
        self.emit('advance_turn')
        self.combat('source', 'ground')
        self.life[self.enemy] -= int(self.source['power'])
        self.emit('assert_life', player=self.enemy, value=self.life[self.enemy])

    def counter(self):
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.enemy, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        # Counterspell targets the active parent from the real response context.
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_stack', value=2)
        self.mana('assert_mana', self.enemy, ZERO)
        self.emit('resolve_response')
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.move(self.actor, self.source['name'], 0, 3)
        self.move(self.enemy, 'Counterspell', 0, 3)

    def assemble(self):
        remaining = self.setup_cast()
        if self.mode == 'countered':
            self.counter()
        elif self.mode != 'short_base':
            self.enter()
        self.mana('assert_mana', self.actor, remaining)
        self.mana('assert_mana', self.enemy, ZERO)
        self.expected_board()
        if self.mode in ('paid', 'declined', 'replay'):
            self.combat_rider()
        if self.mode == 'replay':
            cut = self.before_choices
            tail = self.steps[cut:]
            self.steps = [*self.steps[:cut], dict(op='capture'), *tail, dict(op='record_state'),
                          dict(op='restore'), dict(op='ai_roundtrip'), *tail, dict(op='assert_recorded_state')]
        from recipes import card
        names = {row['csvid']: name for name, row in self.rows.items()
                 if row['csvid'] in {item[3] for item in self.setup}}
        setup = []
        for ref, owner, zone, csvid, flags in self.setup:
            item = card(names[csvid], ref, owner, ZONES[zone])
            if flags:
                item['state'] = [{1: 'tapped', 32: 'token'}[flags]]
            setup.append(item)
        return setup, self.steps


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    pins = validate_witnesses(oracle, rows)
    if (name not in pins['sources'] or csvid != shape['csvid'] or rows[name]['csvid'] != csvid
            or not selected or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('Pinned variable kicker source and distinct supported variants required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        setup, steps = Case(oracle[name], csvid, int(actor), mode, rows, oracle).assemble()
        gaps = ['Zone pickers resolve in the production callback without a test priority window.']
        if csvid == 5020:
            gaps.append('Feather counters have no direct DSL observer; exact X payment and both Birds’ stats exercise the effect.')
        if csvid == 10731:
            gaps.append('Identical discard witnesses allow either legal card choice while preserving exact zone assertions.')
        cases.append(dict(name=f'KickerVariableAndMultikickerRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2369, setup=setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                                       untested_components=gaps)))
    return cases
