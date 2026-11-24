"""Level Up creatures with combat keywords, evasion, and an upper-band activation."""

from functools import lru_cache
import json
from pathlib import Path
import re

from level_up_case_support import LevelUpBoard, classify_leveler
from library_search_recipes import wave6_exact as exact


FAMILY = 'level_upper_combat_riders'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
FIREBREATHING = re.compile(r'^((?:\{[WUBRGC0-9]+\})+): This creature gets \+1/\+0 until end of turn\.$')
UPPER_EVASION = "This creature can't be blocked except by black creatures."
RIDERS = {
    (('Flying',), ('Flying, trample',)): 'air',
    (('Deathtouch',), ('First strike, deathtouch',)): 'strike',
    (('Lifelink',), ('Lifelink, indestructible',)): 'life',
    ((), (UPPER_EVASION,)): 'color',
}
COMMON = ('paid_bands', 'offturn_denied', 'pending_denied')
MODES = {
    'air': ('ground_before', 'flight_middle', 'trample_upper', 'pump_offturn'),
    'strike': ('ordinary_damage', 'deathtouch', 'first_strike'),
    'life': ('plain_damage', 'lifelink', 'destroy_before', 'destroy_upper', 'exile_upper'),
    'color': ('nonblack_middle', 'nonblack_upper', 'black_upper'),
}
BLOCKERS = {
    'ground_before': (('Goblin Tinkerer', 'ground', True),),
    'flight_middle': (('Goblin Tinkerer', 'ground', False), ('Welkin Tern', 'flier', True)),
    'trample_upper': (('Welkin Tern', 'flier', True),),
    **{mode: (('Terra Stomper', 'wall', True),) for mode in MODES['strike']},
    'nonblack_middle': (('Goblin Tinkerer', 'ground', True),),
    'nonblack_upper': (('Goblin Tinkerer', 'ground', False),),
    'black_upper': (('Viscera Seer', 'black', True),),
}
REMOVAL = {'destroy_before': 'Murder', 'destroy_upper': 'Murder', 'exile_upper': 'Swords to Plowshares'}
WITNESSES = {'Fog', *REMOVAL.values(), *(name for group in BLOCKERS.values() for name, _, _ in group)}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    model = classify_leveler(record)
    first = tuple(model['first_riders'])
    upper = tuple(model['second_riders'])
    pump = FIREBREATHING.fullmatch(upper[-1]) if upper else None
    effect = RIDERS.get((first, upper[:-1] if pump else upper))
    if effect is None or (effect == 'air') != bool(pump):
        raise ValueError('Whole Oracle rider bands and exact upper-band activation required')
    model['effect'] = effect
    if pump:
        model['pump_cost'] = cost_from_oracle({'mana_cost': pump[1]})
    return model


def variants(model):
    return [f'{mode}_{seat}' for mode in (*COMMON, *MODES[model['effect']]) for seat in (0, 1)]


def spec(declaration, record):
    name = (record or {}).get('name')
    pin = profiles()['sources'].get(name, {})
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('Independently pinned complete leveler required')
    model = classify(record)
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(set(picked)) != len(picked)
            or set(picked) - set(variants(model))):
        raise ValueError('Exact Oracle leveler model and distinct scoped variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('Independently pinned level combat witnesses required')
    for group, layout in (('sources', 'leveler'), ('witnesses', 'normal')):
        for name, pin in pins[group].items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, {'oracle': record, 'registry': row}) or record.get('layout') != layout
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0', 'card_dummy')):
                raise ValueError(f'Pinned implemented leveler or witness changed: {name}')
            if group == 'sources':
                classify(record)


class CombatLevelBoard(LevelUpBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.pump = 0
        self.life = [20, 20]
        self.snapshot = None
        super().__init__(name, shape, mode, owner, rows, oracle)

    def build(self):
        for name, ref, _ in BLOCKERS.get(self.mode, ()):
            self.add(name, ref, self.foe)
        if self.mode in REMOVAL:
            self.add(REMOVAL[self.mode], 'removal', self.foe, 'hand')
        if self.mode == 'pending_denied':
            self.add('Fog', 'fog', self.owner, 'hand')

    def source_stats(self, level=None):
        power, toughness = super().source_stats(level)
        return power + self.pump, toughness

    def settle(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')

    def observe_board(self):
        for who, life in enumerate(self.life):
            self.emit('assert_life', player=who, value=life)
            for zone in ('battlefield', 'hand', 'graveyard', 'exile'):
                count = sum(item['player'] == who and item['zone'] == zone for item in self.cards.values())
                self.emit('assert_zone', player=who, zone=zone, value=count)
        for ref, item in self.cards.items():
            self.emit('assert_zone_multiplicity', player=item['player'], zone=item['zone'],
                      index=self.rows[item['name']]['csvid'], value=1)
            if item['zone'] == 'battlefield':
                self.emit('assert_card', ref=ref, property='damage', value=item['damage'])
                self.emit('assert_card', ref=ref, property='tapped', value=int(item['tapped']))
        if self.cards['source']['zone'] == 'battlefield':
            self.observe_level()
        self.observe_mana()
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')

    def take_turn(self, seat):
        self.advance_turn()
        self.emit('untap_step', player=seat)

    def age_source(self):
        self.take_turn(self.foe)
        self.take_turn(self.owner)
        self.emit('assert_card', ref='source', property='sick', value=0)
        # Start first main on the actual active seat after the real untap.
        self.emit('turn', player=self.owner)

    def denied(self, depth):
        self.emit('can_activate', ref='source', value=0)
        self.emit('activate', ref='source', value=0)
        self.emit('assert_stack', value=depth)
        self.emit('assert_choices_consumed')
        self.observe_mana()
        self.observe_level()

    def use_pump(self):
        self.take_turn(self.foe)
        self.denied(0)
        for _ in range(2):
            fee = self.shape['pump_cost']
            self.fund(self.owner, fee)
            self.emit('can_activate', ref='source', value=1)
            self.emit('activate', ref='source', value=1)
            self.emit('assert_stack', value=1)
            self.pools[self.owner] = [0] * 6
            self.observe_mana()
            self.observe_level()
            self.settle()
            self.pump += 1
            self.observe_level()
        self.take_turn(self.owner)
        self.pump = 0

    def combat(self):
        self.age_source()
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.cards['source']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocks = BLOCKERS.get(self.mode, ())
        for _, ref, legal in blocks:
            self.emit('block', ref=ref, other='source', value=int(legal))
        self.emit('advance_combat')
        for _, ref, legal in blocks:
            self.emit('assert_card', ref=ref, property='blocking_count', value=int(legal))
        blocker = next((ref for _, ref, legal in blocks if legal), None)
        self.emit('assert_card', ref='source', property='blocked', value=int(bool(blocker)))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        power, toughness = self.source_stats()
        if not blocker:
            self.life[self.foe] -= power
            if self.mode in ('lifelink', 'destroy_upper'):
                self.life[self.owner] += power
            return
        target = self.cards[blocker]
        printed = self.oracle[target['name']]
        block_power, block_toughness = int(printed['power']), int(printed['toughness'])
        lethal = power >= block_toughness or self.mode in ('deathtouch', 'first_strike')
        if lethal:
            target['zone'] = 'graveyard'
        else:
            target['damage'] = power
        if self.mode == 'trample_upper':
            self.life[self.foe] -= max(0, power - block_toughness)
        if self.mode != 'first_strike':
            self.cards['source']['damage'] = block_power
            if block_power >= toughness:
                self.cards['source']['zone'] = 'graveyard'

    def remove_source(self):
        self.cast('removal', target='source')
        self.settle()
        self.cards['removal']['zone'] = 'graveyard'
        if self.mode == 'exile_upper':
            self.cards['source']['zone'] = 'exile'
            self.life[self.owner] += self.source_stats()[0]
        elif self.mode == 'destroy_before':
            self.cards['source']['zone'] = 'graveyard'

    def run(self):
        self.play_source()
        first, upper = self.shape['first'][0], self.shape['second'][0]
        mode = self.mode
        if mode == 'paid_bands':
            self.denied(0)
            self.level_to(upper - 1)
            self.snapshot = len(self.steps)
            self.level_to(upper)
        elif mode == 'offturn_denied':
            self.level_to(first)
            self.take_turn(self.foe)
            self.fund(self.owner, self.shape['activation'])
            self.denied(0)
        elif mode == 'pending_denied':
            self.cast('fog')
            self.fund(self.owner, self.shape['activation'])
            self.denied(1)
            self.settle()
            self.cards['fog']['zone'] = 'graveyard'
        elif mode == 'pump_offturn':
            self.level_to(upper)
            self.use_pump()
        else:
            levels = {'ground_before': first - 1, 'flight_middle': first, 'ordinary_damage': 0,
                      'deathtouch': first, 'plain_damage': first - 1, 'lifelink': first,
                      'destroy_before': upper - 1, 'nonblack_middle': first}
            self.level_to(levels.get(mode, upper))
            if mode not in REMOVAL:
                self.combat()
            else:
                if mode == 'destroy_upper':
                    self.combat()
                    self.observe_board()
                self.remove_source()
        self.observe_board()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if name not in profiles()['sources'] or rows[name]['csvid'] != csvid or set(selected) - set(variants(shape)):
        raise ValueError('Reserved full level combat variant required')
    out = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = CombatLevelBoard(name, shape, mode, int(seat), rows, oracle)
        board.run()
        steps = board.steps
        if board.snapshot is not None:
            cut = board.snapshot
            suffix = steps[cut:]
            steps = steps[:cut] + [step('capture')] + suffix + [step('record_state'), step('ai_roundtrip'),
                     step('restore')] + suffix + [step('assert_recorded_state')]
        gaps = ['Direct raw level counter observation', 'Removing level counters through another card']
        if shape['effect'] == 'air':
            gaps.append('Selecting Level Up as a separate action after the firebreathing band has been reached')
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=gaps)
        out.append(dict(name=f'LevelUpperCombatRidersRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                        seed=2087, setup=board.setup, steps=steps, _recipe=metadata))
    return out
