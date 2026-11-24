"""Large Convoke payments and their printed combat or defensive effects."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact
from recipes import COLORS, card, cost_from_oracle, step

FAMILY = 'convoke_large_and_defensive'
PROFILE = Path(__file__).with_name('data') / 'convoke_large_and_defensive_wave8_profiles.json'
AUTO, SIEGE, ELEMENTAL, GREAT = 7767, 7959, 19279, 19397
COSTS = {AUTO: '{10}{G}{G}{G}{W}{W}', SIEGE: '{5}{G}{G}', ELEMENTAL: '{7}{G}{G}', GREAT: '{7}{G}{G}{G}'}
MODES = ('mixed', 'full_creatures', 'paid', 'tapped_short', 'mixed_rider')
PREFIX = 'ConvokeLargeDefensiveRecipe'
SEED = 2688
ZONES = {'hand': 0, 'battlefield': 1, 'graveyard': 3}
TAPPED, SICK = 1, 2
ZERO = [0] * 6
BEAR, CADET, THOPTER = 'Grizzly Bears', 'Eager Cadet', 'Ornithopter'
STONE, WOOD = 'Wall of Stone', 'Wall of Wood'
GROWTH, MURDER = 'Giant Growth', 'Murder'
WITNESSES = {BEAR, CADET, THOPTER, STONE, WOOD, GROWTH, MURDER}

CONVOKE = 'Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for {1} or one mana of that creature\'s color.)'
RIDERS = {AUTO: 'Trample', SIEGE: 'Trample',
          ELEMENTAL: "Hexproof (This creature can't be the target of spells or abilities your opponents control.)",
          GREAT: 'Indestructible'}
ZONE_NAMES = {value: name for name, value in ZONES.items()}
SEATED = {'assert_life', 'set_mana', 'assert_mana', 'assert_zone_multiplicity', 'choose_target',
          'choose_dialog', 'turn', 'untap_step'}
VALUES = {'assert_life', 'assert_card', 'assert_stack', 'assert_zone_multiplicity', 'choose_dialog',
          'set_mana', 'assert_mana', 'attack', 'block', 'cast', 'can_target_creature'}

@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (csvid not in COSTS or not pin or pin['registry']['csvid'] != csvid or not exact(pin['oracle'], record)
            or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('oracle_text') != f'{CONVOKE}\n{RIDERS[csvid]}' or record.get('mana_cost') != COSTS[csvid]
            or set(record.get('keywords', [])) != {'Convoke', RIDERS[csvid].split()[0]}):
        raise ValueError('whole pinned normal large Convoke record required')
    return dict(csvid=csvid, mana=printed_cost(record['mana_cost']))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported large Convoke variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('reserved large Convoke source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared large Convoke parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Convoke witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != COSTS.keys():
        raise ValueError('exact four-source large Convoke profile required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Convoke source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Convoke source required')
                classify(oracle[name], row['csvid'])


def printed_cost(text):
    symbols = re.findall(r'\{(\d+|[GW])\}', text)
    assert ''.join('{' + symbol + '}' for symbol in symbols) == text
    out = ZERO.copy()
    for symbol in symbols:
        out[0 if symbol.isdigit() else COLORS[symbol]] += int(symbol) if symbol.isdigit() else 1
    return out


class Board:
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        self.id, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.pool = printed_cost(oracle[name]['mana_cost'])
        self.contributors = []
        self.add('source', name, owner, 'hand')
        self.add('foreign', BEAR, self.foe)
        if mode == 'full_creatures':
            for color, witness in ((COLORS['G'], BEAR), (COLORS['W'], CADET), (0, THOPTER)):
                for _ in range(self.pool[color]):
                    self.contribute(witness, color)
        else:
            self.contribute(BEAR, COLORS['G'])
            if self.pool[COLORS['W']]:
                self.contribute(CADET, COLORS['W'])
            self.contribute(THOPTER, 0)
        if mode == 'tapped_short':
            for ref in self.contributors:
                self.cards[ref]['tapped'] = 1
            for item in self.setup:
                if item['ref'] in self.contributors:
                    item['state'].append('tapped')
        else:
            self.add('already_tapped', BEAR, owner, flags=TAPPED)
        if mode == 'mixed_rider':
            if csvid in (AUTO, SIEGE):
                self.add('blocker', STONE if csvid == AUTO else WOOD, self.foe)
            else:
                self.add('own_probe', GROWTH, owner, 'hand')
                self.add('foe_probe', MURDER, self.foe, 'hand')

    def add(self, ref, name, player, zone='battlefield', flags=0):
        placement = card(name, ref, player, zone)
        if flags:
            placement['state'] = ['sick'] if flags == SICK else ['tapped']
        self.setup.append(placement)
        self.cards[ref] = dict(name=name, player=player, zone=zone, tapped=int(bool(flags & TAPPED)))

    def contribute(self, name, color):
        expected = [] if color == 0 else [next(symbol for symbol, code in COLORS.items() if code == color)]
        assert self.oracle[name]['colors'] == expected and 'Creature' in self.oracle[name]['type_line']
        ref = f'helper_{len(self.contributors)}'
        self.add(ref, name, self.owner, flags=SICK)
        self.contributors.append(ref)
        if self.mode != 'paid':
            self.pool[color] -= 1

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        values = {}
        if ref:
            values['ref'] = ref
        if other:
            values['other'] = other
        if prop:
            values['property'] = prop
        if player or op in SEATED:
            values['player'] = player
        if value or op in VALUES:
            values['value'] = value
        if op == 'assert_zone_multiplicity':
            values['zone'] = ZONE_NAMES[zone]
            values['index'] = index
        if op in ('set_mana', 'assert_mana'):
            values['color'] = color
        self.steps.append(step(op, **values))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, seat, amounts=ZERO):
        for color, amount in enumerate(amounts):
            self.emit(op, player=seat, value=amount, color=color)

    def fund(self, seat, amounts):
        self.mana('set_mana', seat, amounts)
        self.emit('refresh')

    def cast(self, ref, target=None, *, allowed=True):
        seat = self.cards[ref]['player']
        if target:
            self.emit('choose_target', target, player=seat)
        self.emit('cast', ref, value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed))
        self.mana('assert_mana', seat, ZERO if allowed else self.pool)

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')

    def receipts(self):
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            cards = [item for item in self.cards.values() if item['player'] == seat]
            counts = Counter((item['name'], item['zone']) for item in cards)
            for name in sorted({item['name'] for item in cards}):
                for zone, code in ZONES.items():
                    self.emit('assert_zone_multiplicity', player=seat, zone=code,
                              index=self.rows[name]['csvid'], value=counts[name, zone])
        for ref, item in self.cards.items():
            if item['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', item['tapped'])

    def spell_helper(self, ref):
        card = self.cards[ref]
        self.fund(card['player'], cost_from_oracle(self.oracle[card['name']]))
        self.cast(ref, 'source')
        self.resolve()
        card['zone'] = 'graveyard'

    def trample(self):
        for seat in (self.foe, self.owner):
            self.emit('advance_turn')
            self.emit('untap_step', player=seat)
            self.emit('turn', player=seat)
            for card in self.cards.values():
                if card['player'] == seat:
                    card['tapped'] = 0
        self.observe('source', 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 1)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        power = int(self.oracle[self.name]['power'])
        blocker = self.cards['blocker']
        excess = power - int(self.oracle[blocker['name']]['toughness'])
        assert excess > 0
        blocker['zone'] = 'graveyard'
        self.cards['source']['tapped'] = 1
        self.life[self.foe] -= excess

    def defensive(self):
        self.emit('can_target_creature', 'foe_probe', 'source', value=int(self.id == GREAT))
        self.emit('can_target_creature', 'own_probe', 'source', value=1)
        if self.id == GREAT:
            self.spell_helper('foe_probe')
            self.observe('source', 'in_play', 1)
        else:
            self.spell_helper('own_probe')
            for key in ('power', 'toughness'):
                self.observe('source', key, int(self.oracle[self.name][key])+3)

    def run(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, self.pool)
        for ref in self.contributors:
            self.observe(ref, 'sick', 1)
            self.observe(ref, 'tapped', self.cards[ref]['tapped'])
        allowed = self.mode != 'tapped_short'
        # Ordinary creature casting uses actual cost and cast events. The bare
        # EVENT_CAN_CAST query is not a permission gate for these cards.
        if self.mode == 'paid':
            self.emit('choose_dialog', player=self.owner, value=1)
        elif allowed:
            if self.owner == 0:
                for ref in self.contributors:
                    self.emit('choose_target', ref, player=0)
            for ref in self.contributors:
                self.cards[ref]['tapped'] = 1
        self.cast('source', allowed=allowed)
        for ref in self.contributors:
            self.observe(ref, 'tapped', self.cards[ref]['tapped'])
        self.observe('foreign', 'tapped', 0)
        if allowed:
            self.observe('already_tapped', 'tapped', 1)
            self.resolve()
            self.cards['source']['zone'] = 'battlefield'
            for prop in ('power', 'toughness'):
                self.observe('source', prop, int(self.oracle[self.name][prop]))
            self.receipts()
            if self.mode == 'mixed_rider':
                if self.id in (AUTO, SIEGE):
                    self.trample()
                else:
                    self.defensive()
        self.receipts()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board = Board(csvid, name, mode, int(seat), rows, oracle)
    board.run()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=[]))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered Convoke source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
