"""Additional draw steps and exact printed riders through the production phases."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'draw_step_additional_draw'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
DRAW_ONE = "At the beginning of each player's draw step, that player draws an additional card."
PROFILES = {
    114: ("At the beginning of each player's draw step, if this artifact is untapped, "
          "that player draws an additional card.", 1, 'tapped_then_untap'),
    1599: ("At the beginning of each player's draw step, that player draws two additional cards.",
           2, 'tapped_unconditional'),
    1315: (DRAW_ONE, 1, 'opposing_legend'),
    13874: ("Flash (You may cast this spell any time you could cast an instant.)\n" + DRAW_ONE,
            1, 'flash_response'),
}
BASICS = ('Forest', 'Island', 'Mountain', 'Plains', 'Swamp',
          'Snow-Covered Forest', 'Snow-Covered Island', 'Snow-Covered Plains')
ZERO = (0,) * 6

COMMON = ('ordinary', 'limited_by_spirit', 'removed')
RITES = ('owner_quota', 'opposing_quota', 'granted_after_first', 'removed_quota', 'stacked_quota', 'quota_replay')
WELL = ('asymmetric', 'limited_entry', 'removed_after_entry', 'stacked_draw', 'countered')
NEW = {
    9004: (DRAW_ONE + '\nEach player may play an additional land on each of their turns.', 1, RITES),
    14500: ("When this enchantment enters, draw two cards.\n"
            "At the beginning of each other player's draw step, that player draws an additional card.\n"
            "At the beginning of your draw step, draw two additional cards.", 1, WELL),
}
WITNESSES = set(BASICS) | {
    'Spirit of the Labyrinth', 'Icy Manipulator', 'Lightning Bolt', 'Murder',
    'Disenchant', 'Counterspell',
}


class DrawCase:
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        self.record, self.csvid, self.owner, self.other, self.mode = record, csvid, owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.extra = (PROFILES.get(csvid) or NEW[csvid])[1]
        self.copies = 1
        self.setup, self.steps = [], []
        self.refs = {}
        self.library = [list(BASICS), list(reversed(BASICS))]
        self.hand = [[], []]
        self.draws = []
        self.source_tapped = False
        self.life = [20, 20]
        self.add('source', owner, 'hand', record['name'])
        for seat in (0, 1):
            for index, card in enumerate(self.library[seat]):
                self.add(f'lib_{seat}_{index}', seat, 'library', card)
        if mode == 'limited_by_spirit':
            self.add('restriction', self.other, 'battlefield', 'Spirit of the Labyrinth')
        if mode == 'removed':
            spell = 'Murder' if csvid == 1315 else 'Disenchant'
            self.add('removal', owner, 'hand', spell)
        if mode in ('tapped_then_untap', 'tapped_unconditional'):
            self.add('tapper', self.other, 'battlefield', 'Icy Manipulator')
        if mode == 'opposing_legend':
            self.add('copy', self.other, 'battlefield', record['name'])
        if mode == 'flash_response':
            self.add('parent', self.other, 'hand', 'Lightning Bolt')

    def add(self, ref, seat, zone, name):
        self.refs[ref] = (seat, name)
        self.setup.append(card(name, ref, seat, zone))
        if zone == 'hand':
            self.hand[seat].append(name)

    def emit(self, op, ref=None, prop=None, *, player=None, value=None, zone=None, index=None, color=None):
        values = dict(ref=ref, property=prop, player=player, value=value, zone=zone, index=index, color=color)
        self.steps.append(step(op, **{name: value for name, value in values.items() if value is not None}))

    def card(self, ref, property_name, value):
        self.emit('assert_card', ref, property_name, value=value)

    def count(self, player, zone, amount):
        self.emit('assert_zone', player=player, zone=zone, value=amount)

    def contains(self, player, zone, name, amount=1):
        self.emit('assert_zone_multiplicity', player=player, zone=zone,
                  index=self.rows[name]['csvid'], value=amount)

    def pool(self, op, player, values):
        for color, amount in enumerate(values):
            self.emit(op, player=player, color=color, value=amount)

    def ready(self):
        self.emit('assert_stack', value=0)
        for player in (0, 1):
            self.pool('assert_mana', player, ZERO)

    def cast(self, ref, *, target=None, target_player=None, response=False):
        payer, spell = self.refs[ref]
        self.pool('set_mana', payer, cost_from_oracle(self.oracle[spell]))
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=payer)
        if target_player is not None:
            self.emit('choose_player', player=payer, value=target_player)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_stack', value=2 if response else 1)
        self.pool('assert_mana', payer, ZERO)
        self.emit('assert_choices_consumed')
        self.hand[payer].remove(spell)

    def resolve(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('assert_choices_consumed')
        self.ready()

    def source(self, tapped=False):
        self.card('source', 'in_play', 1)
        self.card('source', 'tapped', int(tapped))
        self.contains(self.owner, 'battlefield', self.record['name'], self.copies)
        if self.csvid == 1315:
            self.card('source', 'power', 1)
            self.card('source', 'toughness', 3)

    def tap_source(self):
        other = self.other
        self.pool('set_mana', other, (1, 0, 0, 0, 0, 0))
        self.emit('choose_target', 'source', player=other)
        self.emit('activate', 'tapper', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.resolve()
        self.card('tapper', 'tapped', 1)
        self.source(tapped=True)
        self.source_tapped = True

    def advance(self, player):
        assert player == 1 - self.current
        self.current = player
        self.ready()
        self.emit('advance_turn')

    def prepare_draw(self, player):
        self.advance(player)
        self.emit('untap_step', player=player)
        if player == self.owner:
            self.source_tapped = False
        self.emit('upkeep_step', player=player)
        self.ready()
        self.observe_draws()

    def observe_draws(self, *, pending_player=None):
        self.emit('assert_choices_consumed')
        for player, cards in enumerate(self.library):
            self.count(player, 'library', len(cards))
            for index, name in enumerate(cards[:2]):
                self.emit('assert_zone_card', player=player, zone='library',
                          index=index, value=self.rows[name]['csvid'])
            # The raw zone adapter includes pending spells in this view.
            if player != pending_player:
                self.count(player, 'hand', len(self.hand[player]))
                for name, amount in sorted(Counter(self.hand[player]).items()):
                    self.contains(player, 'hand', name, amount)
            self.emit('assert_life', player=player, value=self.life[player])

    def draw_amount(self, player):
        amount = 1 + self.extra
        if self.mode in ('limited_by_spirit', 'removed') or self.csvid == 114 and self.source_tapped:
            amount = 1
        if self.mode == 'opposing_legend':
            amount += 1
        return amount

    def receive_cards(self, player, amount):
        assert len(self.library[player]) >= amount
        self.hand[player].extend(self.library[player][:amount])
        del self.library[player][:amount]

    def draw(self, player):
        amount = self.draw_amount(player)
        self.emit('draw_step', player=player)
        self.receive_cards(player, amount)
        self.draws.append((player, amount))
        self.ready()
        self.observe_draws()

    def build(self):
        flash = self.mode == 'flash_response'
        self.current = self.other if flash else self.owner
        self.emit('turn', player=self.current)
        for seat in (0, 1):
            self.pool('set_mana', seat, ZERO)
        if flash:
            self.cast('parent', target_player=self.owner)
            self.emit('announce_spell', 'parent')
            self.cast('source', response=True)
            self.emit('announce_spell', 'source')
            self.resolve(response=True)
            self.life[self.owner] = 17
            self.contains(self.other, 'graveyard', 'Lightning Bolt')
        else:
            self.cast('source')
            self.resolve()
        self.source()
        if self.mode == 'removed':
            self.cast('removal', target='source')
            self.resolve()
            self.contains(self.owner, 'graveyard', self.record['name'])
            self.contains(self.owner, 'graveyard', self.refs['removal'][1])
            self.contains(self.owner, 'battlefield', self.record['name'], 0)
        elif self.mode in ('tapped_then_untap', 'tapped_unconditional'):
            self.tap_source()
        elif self.mode == 'opposing_legend':
            self.contains(self.other, 'battlefield', self.record['name'])
        elif self.mode == 'limited_by_spirit':
            self.contains(self.other, 'battlefield', 'Spirit of the Labyrinth')

        first = 1 - self.current
        self.prepare_draw(first)
        if self.mode == 'ordinary':
            self.emit('capture')
            self.draw(first)
            self.emit('record_state')
            self.emit('restore')
            self.hand[first].clear()
            self.library[first] = list(BASICS if first == 0 else reversed(BASICS))
            self.emit('ai_roundtrip')
            self.draw(first)
            self.emit('assert_recorded_state')
        else:
            self.draw(first)
        self.prepare_draw(1 - first)
        if self.mode != 'removed':
            self.source(self.source_tapped)
        self.draw(1 - first)



class DrawLaterTurnCase(DrawCase):
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        super().__init__(record, csvid, owner, 'limited_by_spirit', rows, oracle)

    def build(self):
        super().build()
        player = 1 - self.current
        self.prepare_draw(player)
        self.draw(player)


class RitesDrawCase(DrawCase):
    LANDS = ('Plains', 'Island', 'Mountain', 'Swamp')

    def __init__(self, record, csvid, owner, mode, rows, oracle):
        super().__init__(record, csvid, owner, mode, rows, oracle)
        self.land_player = self.other if mode == 'opposing_quota' else owner
        for index, name in enumerate(self.LANDS):
            self.add(f'land_{index}', self.land_player, 'hand', name)
        if mode == 'removed_quota':
            self.add('removal', owner, 'hand', 'Disenchant')
        if mode == 'stacked_quota':
            self.add('copy', owner, 'hand', record['name'])
        self.land_plays = []

    def draw_amount(self, player):
        return 1 + self.copies

    def play(self, index, permitted):
        ref = f'land_{index}'
        player, name = self.refs[ref]
        assert player == self.current
        self.emit('refresh')
        self.emit('play_land', ref, value=int(permitted))
        self.card(ref, 'in_play', int(permitted))
        self.card(ref, 'in_hand', int(not permitted))
        self.contains(player, 'battlefield', name, int(permitted))
        if permitted:
            self.hand[player].remove(name)
            self.land_plays.append(name)
        self.ready()

    def check_quota(self, start, allowance):
        for index in range(start, allowance + 1):
            self.play(index, index < allowance)
        assert len(self.land_plays) == allowance
        self.count(self.land_player, 'battlefield', allowance + self.copies * (self.land_player == self.owner))
        self.observe_draws()

    def build(self):
        self.current = self.owner
        self.emit('turn', player=self.owner)
        for seat in (0, 1):
            self.pool('set_mana', seat, ZERO)
        if self.mode == 'granted_after_first':
            self.play(0, True)
        self.cast('source')
        self.resolve()
        if self.mode == 'stacked_quota':
            self.cast('copy')
            self.resolve()
            self.copies = 2
        self.source()
        if self.mode == 'removed_quota':
            self.cast('removal', target='source')
            self.resolve()
            self.copies = 0
            self.contains(self.owner, 'graveyard', self.record['name'])
            self.contains(self.owner, 'battlefield', self.record['name'], 0)

        if self.mode == 'opposing_quota':
            self.advance(self.other)
        start = len(self.land_plays)
        if self.mode == 'quota_replay':
            self.emit('capture')
        self.check_quota(start, 1 + self.copies)
        if self.mode == 'quota_replay':
            self.emit('record_state')
            self.emit('restore')
            self.hand[self.land_player] = list(self.LANDS)
            self.land_plays.clear()
            self.observe_draws()
            self.check_quota(start, 1 + self.copies)
            self.emit('assert_recorded_state')

        for _ in (0, 1):
            player = 1 - self.current
            self.prepare_draw(player)
            self.draw(player)


class WellDrawCase(DrawCase):
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        super().__init__(record, csvid, owner, mode, rows, oracle)
        if mode == 'limited_entry':
            self.add('restriction', self.other, 'battlefield', 'Spirit of the Labyrinth')
        if mode == 'removed_after_entry':
            self.add('removal', owner, 'hand', 'Disenchant')
        if mode == 'stacked_draw':
            self.add('copy', owner, 'hand', record['name'])
            for seat in (0, 1):
                for index, name in enumerate(self.library[seat][:4]):
                    self.add(f'lib_extra_{seat}_{index}', seat, 'library', name)
                    self.library[seat].append(name)
        if mode == 'countered':
            self.add('counter', self.other, 'hand', 'Counterspell')

    def draw_amount(self, player):
        if self.mode == 'limited_entry':
            return 1
        return 1 + self.copies * (2 if player == self.owner else 1)

    def arrive(self, ref):
        self.cast(ref)
        self.card(ref, 'in_play', 0)
        self.card(ref, 'in_hand', 0)
        self.observe_draws(pending_player=self.owner)
        self.resolve()
        self.receive_cards(self.owner, 1 if self.mode == 'limited_entry' else 2)
        self.observe_draws()

    def countered(self):
        self.cast('source')
        self.emit('announce_spell', 'source')
        self.cast('counter', response=True)
        self.resolve(response=True)
        self.copies = 0
        self.contains(self.owner, 'graveyard', self.record['name'])
        self.contains(self.other, 'graveyard', 'Counterspell')
        self.contains(self.owner, 'battlefield', self.record['name'], 0)
        self.observe_draws()

    def build(self):
        self.current = self.owner
        self.emit('turn', player=self.owner)
        for seat in (0, 1):
            self.pool('set_mana', seat, ZERO)
        if self.mode == 'countered':
            self.countered()
        else:
            self.arrive('source')
            if self.mode == 'stacked_draw':
                self.arrive('copy')
                self.copies = 2
            self.source()
            if self.mode == 'removed_after_entry':
                self.cast('removal', target='source')
                self.resolve()
                self.copies = 0
                self.contains(self.owner, 'graveyard', self.record['name'])
                self.contains(self.owner, 'battlefield', self.record['name'], 0)
                self.observe_draws()
        self.prepare_draw(self.other)
        self.draw(self.other)
        if self.mode != 'limited_entry':
            self.prepare_draw(self.owner)
            self.draw(self.owner)


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid in NEW:
        clause, extra, modes = NEW[csvid]
    else:
        clause, extra, special = PROFILES[csvid]
        modes = COMMON + (special,)
    keywords = ['Flash'] if csvid == 13874 else []
    if (record['oracle_text'] != clause or record.get('layout') != 'normal'
            or record.get('keywords') != keywords or record.get('card_faces') or record.get('selected_face')):
        raise ValueError('Exact whole additional-draw source required')
    return dict(csvid=csvid, extra=extra, price=cost_from_oracle(record),
                modes=list(modes), clauses=clause.splitlines())


def variants(model):
    selected = [f'{mode}_{seat}' for mode in model['modes'] for seat in (0, 1)]
    if model['csvid'] == 114:
        selected.append('same_seat_later_turn_0')
    return selected


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {p['registry']['csvid'] for p in pins['sources'].values()} != PROFILES.keys() | NEW.keys()):
        raise ValueError('Exact additional draw source and witness scope required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, {'oracle': record, 'registry': row}) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Pinned additional draw source or witness changed: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned additional-draw primary required')
    model = classify(record, pin['registry']['csvid'])
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact additional draw clause, cost and scoped variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact additional draw source and scoped variants required')
    board = {9004: RitesDrawCase, 14500: WellDrawCase}.get(csvid, DrawCase)
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scenario = DrawLaterTurnCase if mode == 'same_seat_later_turn' else board
        game = scenario(oracle[name], csvid, int(seat), mode, rows, oracle)
        game.build()
        gaps = ['Draw step has a single bounded completion receipt with exact library and hand conservation. '
                'There is no independent priority window between the ordinary draw and the triggered extra draw.']
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=gaps)
        cases.append(dict(name=f'DrawStepAdditionalDrawRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2528, setup=game.setup, steps=game.steps, _recipe=metadata))
    return cases
