"""Whole Oracle variable Suspend with paid X and real timed effects."""

from collections import Counter
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'suspend_variable_countdown'
PROFILE = Path(__file__).parent / 'data/suspend_variable_countdown_wave8_profiles.json'
DURATIONS = {'unfunded': 0, 'x1': 1, 'x2': 2, 'x3': 3, 'wrong_color': 0, 'replay': 2}
SOURCES = {8737: 'draw', 8744: 'soldier', 8766: 'nonbasic', 8783: 'growth', 8839: 'drain'}
REMOVED = "Whenever a time counter is removed from this card while it's exiled, "
RULES = {
    ("Aeon Chronicler's power and toughness are each equal to the number of cards in your hand.",
     REMOVED + 'draw a card.'): (8737, '{3}{U}', False),
    ("Benalish Commander's power and toughness are each equal to the number of Soldiers you control.",
     REMOVED + 'create a 1/1 white Soldier creature token.'): (8744, '{W}{W}', False),
    ("Detritivore's power and toughness are each equal to the number of nonbasic land cards in your opponents' graveyards.",
     REMOVED + 'destroy target nonbasic land.'): (8766, '{3}{R}', True),
    ("Fungal Behemoth's power and toughness are each equal to the number of +1/+1 counters on creatures you control.",
     REMOVED + 'you may put a +1/+1 counter on target creature.'): (8783, '{G}{G}', True),
    ("Roiling Horror's power and toughness are each equal to your life total minus the life total of an opponent with the most life.",
     REMOVED + 'target player loses 1 life and you gain 1 life.'): (8839, '{B}{B}{B}', True),
}
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = [0] * 6
TARGET_LANDS = ('Taiga', 'Savannah')
WITNESSES = frozenset({'Forest', 'Island', 'Mountain', 'Plains', 'Soldier', 'Saltfield Recluse',
                       'Doomed Traveler', *TARGET_LANDS, 'Plateau', 'Volcanic Island', 'Cloudfin Raptor',
                       'Arcbound Worker', 'Royal Assassin'})


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    lines = record['oracle_text'].splitlines()
    if (len(lines) != 3 or record['layout'] != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record['type_line'].startswith('Creature — ')
            or record['power'] != '*' or record['toughness'] != '*' or record['keywords'] != ['Suspend']):
        raise ValueError('Whole variable Suspend creature record required')
    printed = RULES.get((lines[0], lines[2]))
    if not printed:
        raise ValueError('Unsupported variable Suspend action or cost')
    csvid, fee, reminder = printed
    suffix = (f" (Rather than cast this card from your hand, you may pay {{X}}{fee} and exile it with X time counters on it. "
              "At the beginning of your upkeep, remove a time counter. When the last is removed, you may cast it without "
              "paying its mana cost. It has haste.)") if reminder else ''
    if lines[1] != f"Suspend X—{{X}}{fee}. X can't be 0.{suffix}":
        raise ValueError('Whole Suspend clause and its actual X cost required')
    return dict(csvid=csvid, effect=SOURCES[csvid], suspend=cost_from_oracle({'mana_cost': fee}),
                printed=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in DURATIONS]


def spec(declaration, record):
    shape = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('Exact whole variable Suspend declaration required')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('Distinct supported variable countdown cases required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    if set(pins['witnesses']) != WITNESSES:
        raise ValueError('Independent Suspend witness set changed')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (row.get('kind') not in (('card', 'token') if name == 'Soldier' else ('card',))
                or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])):
            raise ValueError(f'Whole implemented Oracle or registry pin changed for {name}')
    return pins


class VariableDuel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe = csvid, record, actor, 1-actor
        self.mode = mode
        self.duration = DURATIONS[mode]
        self.rows, self.oracle = rows, oracle
        self.effect = SOURCES[csvid]
        self.payment = parameters(record)['suspend']
        self.normal_cast = cost_from_oracle(record)
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        if self.effect == 'drain':
            self.life[actor] = 22
        self.deck = {actor: ['Plains', 'Island', 'Forest', 'Mountain'], self.foe: ['Forest', 'Plains']}
        self.add('source', actor, record['name'], 'hand')
        for seat, deck in self.deck.items():
            for i, name in enumerate(deck):
                self.add(f'deck_{seat}_{i}', seat, name, 'library')
        if self.effect == 'draw':
            self.add('initial_hand', actor, 'Forest', 'hand')
        if self.effect == 'soldier':
            self.add('non_soldier', actor, 'Saltfield Recluse', 'battlefield')
            self.add('opposing_soldier', self.foe, 'Doomed Traveler', 'battlefield')
        if self.effect == 'nonbasic':
            for i, name in enumerate((*TARGET_LANDS, 'Plateau') if mode == 'x3' else TARGET_LANDS):
                self.add(f'land_{i}', self.foe, name, 'battlefield')
            self.add('enemy_basic', self.foe, 'Mountain', 'battlefield')
            self.add('owner_nonbasic', actor, 'Volcanic Island', 'graveyard')
        if self.effect == 'growth':
            seed = 'Cloudfin Raptor' if mode == 'unfunded' else 'Arcbound Worker'
            # One true modular counter keeps the paid source alive in either
            # legal last-trigger ordering without a second Evolve trigger.
            self.add('existing_counter', actor, seed, 'battlefield')
            self.add('counter_target', actor, 'Saltfield Recluse', 'battlefield')
            self.add('unaffected', self.foe, 'Royal Assassin', 'battlefield')
        self.removed = 0

    def add(self, ref, player, name, zone):
        self.setup.append(dict(name=name, ref=ref, player=player, zone=zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=None, value=None, zone=None, index=None, color=None):
        action = dict(op=op)
        for field, item in (
                ('ref', ref or None), ('other', other or None),
                ('counter_kind' if op == 'assert_counter' else 'property', prop or None),
                ('player', player), ('value', value), ('zone', None if zone is None else ZONES[zone]),
                ('index', index), ('color', color)):
            if item is not None:
                action[field] = item
        self.steps.append(action)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, amounts):
        for color, amount in enumerate(amounts):
            self.emit(op, player=player, color=color, value=amount)

    def receipts(self, *, ordered_library=True):
        self.emit('assert_stack')
        names = sorted({card['name'] for card in self.cards.values()})
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            for zone in ZONES:
                contents = self.deck[seat] if zone == 'library' else [
                    card['name'] for card in self.cards.values() if card['player'] == seat and card['zone'] == zone
                ]
                self.emit('assert_zone', player=seat, zone=ZONES.index(zone), value=len(contents))
                counts = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=seat, zone=ZONES.index(zone), index=self.rows[name]['csvid'],
                              value=counts[name])
                if zone == 'library' and (ordered_library or seat == self.foe):
                    for index, name in enumerate(contents):
                        self.emit('assert_zone_card', player=seat, zone=ZONES.index(zone), index=index,
                                  value=self.rows[name]['csvid'])

    def menu_required(self, pool):
        # Mana already floating is the only mana the owner can legally spend.
        enough_normal = all(a >= b for a, b in zip(pool[1:], self.normal_cast[1:])) and sum(pool) >= sum(self.normal_cast)
        # Aeon has a direct source callback. The other four use DIALOG with
        # DLG_AUTOCHOOSE_IF_1, which still presents the row to seat 1.
        return enough_normal or (self.actor == 1 and self.csvid != 8737)

    def begin(self):
        self.emit('turn', player=self.actor)
        if self.effect == 'drain':
            self.emit('set_life', player=self.actor, value=self.life[self.actor])
        self.mana('set_mana', self.foe, ZERO)
        funded = self.payment[:] if self.duration else ZERO[:]
        funded[0] += self.duration
        if self.mode == 'wrong_color':
            funded[0] = sum(self.payment) + 2
        remaining = funded if self.mode == 'wrong_color' else ZERO
        self.mana('set_mana', self.actor, funded)
        self.emit('refresh')
        self.observe('source', 'in_hand', 1)
        if self.effect == 'growth':
            self.emit('assert_counter', 'existing_counter', prop='plus1', value=1)
        if self.duration:
            if self.menu_required(funded):
                self.emit('choose_dialog', player=self.actor, value=1)
            self.emit('choose_x', player=self.actor, value=self.duration)
        self.emit('cast', 'source', value=0)
        self.emit('assert_choices_consumed')
        for seat in (0, 1):
            self.mana('assert_mana', seat, remaining if seat == self.actor else ZERO)
        if self.duration:
            self.cards['source']['zone'] = 'exile'
        self.receipts()

    def target_rider(self):
        if self.effect == 'nonbasic':
            self.emit('choose_target', f'land_{self.removed}', player=self.actor)
        if self.effect == 'growth':
            self.emit('choose_target', 'counter_target', player=self.actor)
        if self.effect == 'drain':
            self.emit('choose_player', player=self.actor, value=self.foe)

    def apply_rider(self):
        n = self.removed
        if self.effect == 'draw':
            self.cards[f'deck_{self.actor}_{n}']['zone'] = 'hand'
            del self.deck[self.actor][0]
        if self.effect == 'soldier':
            self.cards[f'soldier_{n}'] = dict(name='Soldier', player=self.actor, zone='battlefield')
        if self.effect == 'nonbasic':
            self.cards[f'land_{n}']['zone'] = 'graveyard'
        if self.effect == 'drain':
            self.life[self.actor] += 1
            self.life[self.foe] -= 1
        self.removed += 1

    def phase(self, seat, *, last=False):
        self.emit('advance_turn')
        self.emit('untap_step', player=seat)
        if seat == self.actor:
            self.target_rider()
        self.emit('upkeep_step', player=seat)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        for player in (0, 1):
            self.mana('assert_mana', player, ZERO)
        if seat == self.actor:
            self.apply_rider()
            if last:
                self.cards['source']['zone'] = 'battlefield'
        if self.effect == 'growth':
            self.emit('assert_counter', 'existing_counter', prop='plus1', value=1)
            self.emit('assert_counter', 'counter_target', prop='plus1', value=self.removed)
            self.emit('assert_counter', 'unaffected', prop='plus1', value=0)
        if self.effect == 'nonbasic':
            self.observe('enemy_basic', 'in_play', 1)
        if self.effect == 'soldier' and self.removed:
            for prop, value in (('token', 1), ('color', 32), ('subtype_Soldier', 1), ('power', 1), ('toughness', 1)):
                self.emit('assert_matching_permanents', prop=prop, player=self.actor,
                          index=self.rows['Soldier']['csvid'], value=value)
        self.receipts()

    def current_power(self):
        if self.effect == 'nonbasic':
            return self.removed
        if self.effect == 'drain':
            return self.life[self.actor] - self.life[self.foe]
        return self.removed + 1

    def finish(self):
        self.emit('bind_permanent_copy', 'played', player=self.actor, index=0, value=self.csvid)
        self.observe('played', 'in_play', 1)
        self.emit('turn', player=self.actor)
        power = self.current_power()
        self.observe('played', 'power', power)
        self.observe('played', 'toughness', power)
        # Real entry from suspend has haste and can attack on the same turn.
        self.emit('begin_attackers')
        self.emit('attack', 'played', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.observe('played', 'blocked', 0)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('refresh')
        self.life[self.foe] -= power
        if self.effect == 'drain':
            self.observe('played', 'power', self.current_power())
            self.observe('played', 'toughness', self.current_power())
        self.receipts()

    def build(self):
        self.begin()
        if not self.duration:
            return
        cut = len(self.steps)
        for index in range(self.duration):
            self.phase(self.foe)
            self.phase(self.actor, last=index == self.duration-1)
        self.finish()
        if self.mode == 'replay':
            first, tail = self.steps[:cut], self.steps[cut:]
            self.steps = [*first, dict(op='capture'), *tail, dict(op='record_state'),
                          dict(op='restore'), dict(op='ai_roundtrip'), *tail, dict(op='assert_recorded_state')]


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    pins = validate_witnesses(oracle, rows)
    if (name not in pins['sources'] or csvid != shape['csvid'] or rows[name]['csvid'] != csvid
            or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('Pinned variable Suspend card and distinct cases required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        duel = VariableDuel(csvid, oracle[name], int(seat), mode, rows, oracle)
        duel.build()
        cases.append(dict(name=f'SuspendVariableCountdownRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2471, setup=duel.setup, steps=duel.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                                       untested_components=['The public driver infers hidden time counters from timed exile and each actual removal effect.',
                                                            'Explicit zero-X refusal, alternate trigger ordering, and multiplayer.'])))
    return cases
