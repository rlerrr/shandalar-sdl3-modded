"""Whole Oracle Flash entry untaps and temporary stat changes."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'flash_entry_untap_and_temporary_stats'
PROFILE = Path(__file__).with_name('data') / 'flash_entry_untap_and_temporary_stats_wave8_profiles.json'
HIP, BRIAR, CROOK, EXARCH = 13397, 9085, 8511, 11588
MODES = {
    HIP: ('untap', 'no_other', 'offturn_block', 'countered'),
    BRIAR: ('normal_expiry', 'evoke_expiry', 'foe_buff', 'combat_trick', 'countered'),
    CROOK: ('switch_own', 'switch_foe', 'response_switch', 'flying_block', 'countered'),
    EXARCH: ('untap_land', 'tap_land', 'untap_creature', 'tap_creature', 'countered'),
}
PREFIX = 'FlashEntryUntapAndTemporaryStatsRecipe'
SEED = 2717
ZONES = {'hand': 0, 'battlefield': 1, 'graveyard': 3}
TAPPED = 1
ZERO = [0] * 6
SQUIRE, BEAR, GIANT, TURTLE, DRAKE = 'Squire', 'Grizzly Bears', 'Hill Giant', 'Horned Turtle', 'Wind Drake'
ISLAND, FOREST, COUNTER, GROWTH = 'Island', 'Forest', 'Counterspell', 'Giant Growth'
WITNESSES = {SQUIRE, BEAR, GIANT, TURTLE, DRAKE, ISLAND, FOREST, COUNTER, GROWTH}

BELL, OAK = 12117, 15518
MYSTIC = 'Elvish Mystic'
WITNESSES.add(MYSTIC)
MODES[BELL] = ('mass', 'reuse_mana', 'combat_untap', 'solo', 'countered')
MODES[OAK] = ('own_mass', 'foe_mass', 'reach_block', 'empty_foe', 'countered')
ZONE_NAMES = {value: name for name, value in ZONES.items()}
SEATED = {'assert_life', 'set_mana', 'assert_mana', 'assert_zone_multiplicity', 'choose_target',
          'choose_player', 'choose_dialog', 'turn', 'untap_step'}
VALUES = {'assert_life', 'assert_card', 'assert_stack', 'assert_zone_multiplicity', 'choose_dialog',
          'choose_player', 'set_mana', 'assert_mana', 'attack', 'block', 'cast', 'cast_response',
          'activate', 'require_can_activate'}
FLASH = 'Flash (You may cast this spell any time you could cast an instant.)'
PRINTED = {
    HIP: (FLASH, 'When this creature enters, untap another target creature you control.'),
    BRIAR: ('Flash', 'When this creature enters, target creature gets +3/+3 until end of turn.',
            "Evoke {1}{G} (You may cast this spell for its evoke cost. If you do, it's sacrificed when it enters.)"),
    CROOK: ('Flash', 'Flying', "When this creature enters, switch target creature's power and toughness until end of turn."),
    EXARCH: (FLASH, 'When this creature enters, choose one —', '• Untap target permanent you control.',
             '• Tap target permanent an opponent controls.'),
    BELL: (FLASH, 'When this creature enters, untap all creatures you control.'),
    OAK: (FLASH, 'Reach', 'When this creature enters, creatures target player controls get +2/+2 until end of turn. Untap them.'),
}
PRIORITY_GAP = 'Responses before entry use the real stack. This driver does not separate the ETB target choice from its resolution.'
EXARCH_GAP = 'The native entry handler derives the mode from target control; a separate mode menu is not exposed.'

@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (csvid not in PRINTED or not pin or pin['registry']['csvid'] != csvid or not exact(pin['oracle'], record)
            or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or tuple(record.get('oracle_text', '').splitlines()) != PRINTED[csvid] or 'Flash' not in record['keywords']):
        raise ValueError('whole pinned normal Flash entry source required')
    return dict(csvid=csvid, mana=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Flash entry variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('reserved Flash entry source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared Flash entry parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Flash entry witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys():
        raise ValueError('exact six-source Flash entry profile required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Flash entry source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Flash entry source required')
                classify(oracle[name], row['csvid'])


class Board:
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        self.id, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.add('source', name, owner, 'hand')
        if mode == 'countered':
            self.add('response', COUNTER, self.foe, 'hand')
        if mode == 'response_switch':
            self.add('response', GROWTH, self.foe, 'hand')
        if csvid == HIP:
            name = ISLAND if mode == 'no_other' else SQUIRE
            self.add('host', name, owner, tapped=True)
            self.add('rival', BEAR, self.foe, tapped=True)
        if csvid == BRIAR:
            self.add('host', BEAR if mode == 'combat_trick' else SQUIRE, self.foe if mode == 'foe_buff' else owner)
            self.add('rival', SQUIRE, self.foe if mode != 'foe_buff' else owner)
        if csvid == CROOK:
            target_seat = self.foe if mode in ('switch_foe', 'response_switch') else owner
            self.add('host', TURTLE if target_seat == self.foe else SQUIRE, target_seat)
            self.add('rival', BEAR, 1-target_seat)
        if csvid == EXARCH:
            own_target = mode in ('untap_land', 'untap_creature', 'countered')
            own_name, enemy_name = (ISLAND, FOREST) if mode.endswith('_land') else (SQUIRE, GIANT)
            self.add('host', own_name if own_target else enemy_name, owner if own_target else self.foe, tapped=own_target)
            self.add('rival', enemy_name if own_target else own_name, self.foe if own_target else owner, tapped=not own_target)
        if mode in ('combat_trick', 'offturn_block', 'flying_block'):
            self.add('attacker', DRAKE if mode == 'flying_block' else GIANT, self.foe)

    def add(self, ref, name, player, zone='battlefield', *, tapped=False):
        placement = card(name, ref, player, zone)
        if tapped:
            placement['state'] = ['tapped']
        self.setup.append(placement)
        self.cards[ref] = dict(name=name, player=player, zone=zone, tapped=int(tapped), bonus=0, switched=False, damage=0)

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
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def mana(self, op, seat, amount=ZERO):
        for color, value in enumerate(amount):
            self.emit(op, player=seat, value=value, color=color)

    def fund(self, player, amount=ZERO):
        self.mana('set_mana', player, amount)
        self.emit('refresh')

    def stat(self, ref, name):
        item = self.cards[ref]
        if item['switched']:
            name = 'toughness' if name == 'power' else 'power'
        return int(self.oracle[item['name']][name])+item['bonus']

    def cast(self, ref, *, response=False):
        item = self.cards[ref]
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2 if response else 1)
        self.mana('assert_mana', item['player'])

    def resolve(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')

    def receipts(self):
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=20)
            self.mana('assert_mana', seat)
            cards = [item for item in self.cards.values() if item['player'] == seat]
            counts = Counter((item['name'], item['zone']) for item in cards)
            for name in sorted({item['name'] for item in cards}):
                for zone, index in ZONES.items():
                    self.emit('assert_zone_multiplicity', player=seat, index=self.rows[name]['csvid'], zone=index,
                              value=counts[name, zone])
        for ref, item in self.cards.items():
            if item['zone'] != 'battlefield':
                continue
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'tapped', item['tapped'])
            if 'Creature' in self.oracle[item['name']]['type_line']:
                for field in ('power', 'toughness'):
                    self.observe(ref, field, self.stat(ref, field))
                self.observe(ref, 'damage', item['damage'])

    def begin_foe_combat(self, ref):
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        self.cards[ref]['tapped'] = 1
        self.emit('finish_attackers')

    def finish_combat(self, blocker):
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', blocker, 'attacker', value=1)
        self.emit('advance_combat')
        self.observe('attacker', 'blocked', int(blocker is not None))
        self.emit('combat_damage')
        self.resolve()
        if blocker:
            marks = {blocker: self.stat('attacker', 'power'), 'attacker': self.stat(blocker, 'power')}
            for ref, amount in marks.items():
                self.cards[ref]['damage'] = amount
                if amount >= self.stat(ref, 'toughness'):
                    self.cards[ref]['zone'] = 'graveyard'
        self.emit('end_combat')
        self.receipts()

    def enter(self, *, response=False):
        if self.mode != 'no_other':
            self.emit('choose_target', 'host', player=self.owner)
        self.resolve(response=response)
        self.cards['source']['zone'] = 'graveyard' if self.mode == 'evoke_expiry' else 'battlefield'
        host = self.cards['host']
        if self.id in (HIP, EXARCH) and self.mode != 'no_other':
            host['tapped'] = int(host['player'] == self.foe)
        elif self.id == BRIAR:
            host['bonus'] = 3
        elif self.id == CROOK:
            host['switched'] = True

    def next_untap(self, seat):
        self.emit('advance_turn')
        self.emit('untap_step', player=seat)
        for card in self.cards.values():
            card.update(damage=0, bonus=0, switched=False)
            if card['player'] == seat and card['zone'] == 'battlefield':
                card['tapped'] = 0
        self.receipts()

    def run(self):
        self.emit('turn', player=self.foe)
        self.fund(self.foe)
        cost = '{1}{G}' if self.mode == 'evoke_expiry' else self.oracle[self.name]['mana_cost']
        self.fund(self.owner, cost_from_oracle({'mana_cost': cost}))
        if 'attacker' in self.cards:
            self.begin_foe_combat('attacker')
        if self.id == BRIAR and self.mode != 'evoke_expiry':
            self.emit('choose_dialog', player=self.owner, value=0)
        self.cast('source')
        responding = 'response' in self.cards
        if responding:
            self.emit('announce_spell', 'source')
            self.fund(self.foe, cost_from_oracle(self.oracle[self.cards['response']['name']]))
            if self.mode == 'response_switch':
                self.emit('choose_target', 'host', player=self.foe)
                self.cards['host']['bonus'] = 3
            self.cast('response', response=True)
            self.cards['response']['zone'] = 'graveyard'
        if self.mode == 'countered':
            self.resolve(response=True)
            self.cards['source']['zone'] = 'graveyard'
            self.receipts()
            return
        self.enter(response=responding)
        self.receipts()
        if self.mode in ('offturn_block', 'flying_block', 'combat_trick'):
            self.finish_combat('host' if self.mode == 'combat_trick' else 'source')
        if self.id in (BRIAR, CROOK):
            self.next_untap(self.owner)
        if self.mode == 'tap_creature':
            self.emit('begin_attackers')
            self.emit('attack', 'host', value=0)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.resolve()
            self.emit('end_combat')
            self.next_untap(self.owner)
            self.next_untap(self.foe)


class MassBoard(Board):
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        super().__init__(csvid, name, mode, owner, rows, oracle)
        self.recipient = self.foe if mode in ('foe_mass', 'empty_foe') else self.owner
        self.add('own_land', ISLAND, self.owner, tapped=True)
        self.add('foe_land', FOREST, self.foe, tapped=True)
        if mode != 'solo':
            self.add('host', MYSTIC if mode == 'reuse_mana' else BEAR, self.owner, tapped=True)
            self.add('own_second', SQUIRE, self.owner, tapped=True)
        if mode != 'empty_foe':
            self.add('rival', SQUIRE, self.foe, tapped=True)
            self.add('foe_second', BEAR, self.foe, tapped=True)
        if mode in ('combat_untap', 'reach_block'):
            self.add('attacker', DRAKE if mode == 'reach_block' else GIANT, self.foe)
        if mode == 'reuse_mana':
            self.add('growth', GROWTH, self.owner, 'hand')

    def enter(self, *, response=False):
        if self.id == OAK:
            self.emit('choose_player', player=self.owner, value=self.recipient)
        self.resolve(response=response)
        self.cards['source']['zone'] = 'battlefield'
        for permanent in self.cards.values():
            if (permanent['player'] == self.recipient and permanent['zone'] == 'battlefield'
                    and 'Creature' in self.oracle[permanent['name']]['type_line']):
                permanent['tapped'] = 0
                if self.id == OAK:
                    permanent['bonus'] = 2

    def reuse_mana(self):
        self.emit('require_can_activate', 'host', value=1)
        self.emit('activate', 'host', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.cards['host']['tapped'] = 1
        self.observe('host', 'tapped', 1)
        self.mana('assert_mana', self.owner, cost_from_oracle(self.oracle[GROWTH]))
        self.emit('choose_target', 'own_second', player=self.owner)
        self.cast('growth')
        self.resolve()
        self.cards['growth']['zone'] = 'graveyard'
        self.cards['own_second']['bonus'] = 3
        self.receipts()

    def run(self):
        super().run()
        if self.mode == 'countered':
            return
        if self.mode in ('combat_untap', 'reach_block'):
            self.finish_combat('source' if self.mode == 'reach_block' else 'host')
        if self.mode == 'reuse_mana':
            self.reuse_mana()
        if self.id == OAK:
            self.next_untap(self.owner)


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board_type = MassBoard if csvid in (BELL, OAK) else Board
    board = board_type(csvid, name, mode, int(seat), rows, oracle)
    board.run()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'],
                             untested_components=[PRIORITY_GAP, EXARCH_GAP] if csvid == EXARCH else [PRIORITY_GAP]))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered Flash entry source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
