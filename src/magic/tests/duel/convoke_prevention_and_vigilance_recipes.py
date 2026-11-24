"""Paid Convoke, actual prevention packets, Flash, Flying, and vigilance."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import COLORS, card, cost_from_oracle, step

FAMILY = 'convoke_prevention_and_vigilance'
PROFILE = Path(__file__).with_name('data') / 'convoke_prevention_and_vigilance_wave8_profiles.json'

ANGEL, CHANT, GUARDIAN, CENTAUR = 8883, 7791, 7873, 19477
WHITE, GREEN, ZERO = COLORS['W'], COLORS['G'], [0] * 6
CAD, BEAR, THOPTER = 'Eager Cadet', 'Grizzly Bears', 'Ornithopter'
SQUIRE = 'Squire'
GIANT, BOLT, SHOCK, PING = 'Hill Giant', 'Lightning Bolt', 'Shock', 'Prodigal Sorcerer'
ZONES = {'hand': 0, 'battlefield': 1, 'graveyard': 3}
MODES = {
    ANGEL: ('split', 'creature', 'flying_expiry', 'mixed', 'short', 'depletion'),
    CHANT: ('combat', 'mixed_damage', 'expiry', 'all_creatures', 'short', 'creature_packets'),
    GUARDIAN: ('vigilance', 'colored', 'mixed', 'normal', 'short'),
    CENTAUR: ('vigilance', 'colored', 'mixed', 'normal', 'short'),
}
WITNESSES = frozenset(('Eager Cadet', 'Grizzly Bears', 'Hill Giant', 'Lightning Bolt', 'Ornithopter', 'Prodigal Sorcerer', 'Shock', 'Squire'))

ZONE_NAMES = {code: name for name, code in ZONES.items()}
COSTS = {ANGEL: '{6}{W}{W}', CHANT: '{6}{W}{W}', GUARDIAN: '{6}{G}{W}', CENTAUR: '{3}{G}{W}'}
KEYWORDS = {ANGEL: {'Convoke', 'Flash', 'Flying'}, CHANT: {'Convoke'},
            GUARDIAN: {'Convoke', 'Vigilance'}, CENTAUR: {'Convoke', 'Vigilance'}}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in COSTS or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record.get('mana_cost') != COSTS[csvid]
            or set(record.get('keywords', ())) != KEYWORDS[csvid]
            or (record.get('type_line') == 'Instant') != (csvid == CHANT)):
        raise ValueError('normal Convoke prevention or vigilance record required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in MODES[shape['csvid']]]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Convoke prevention or vigilance variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned Convoke primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed Convoke declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != COSTS.keys()):
        raise ValueError('exact Convoke prevention and vigilance sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('Convoke Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Convoke primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Convoke primary CSVID required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = Board(rows[name], int(seat), mode, rows, oracle)
        board.run()
        cases.append(dict(name=f'ConvokePreventionVigilanceRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2688, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=[])))
    return cases


class Board:
    def __init__(self, row, owner, mode, rows, oracle):
        self.id, self.name, self.owner, self.foe, self.mode = row['csvid'], row['name'], owner, 1-owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards, self.contributors = [], [], {}, []
        self.life, self.mana = [20, 20], [ZERO.copy(), ZERO.copy()]
        self.cost = cost_from_oracle(oracle[self.name])
        self.remainder = self.cost.copy()
        self.shields, self.chant = Counter(), False
        self.add('source', self.name, owner, 'hand')
        self.add('foreign', CAD, self.foe)
        self.add('unavailable', CAD, owner, tapped=True)
        self.contrib(SQUIRE if mode == 'creature' else CAD, WHITE,
                     hand=mode == 'vigilance', tapped=mode == 'short')
        self.contrib(CAD if self.id in (ANGEL, CHANT) else BEAR,
                     WHITE if self.id in (ANGEL, CHANT) else GREEN,
                     tapped=mode == 'short')
        if mode == 'short':
            self.remainder = self.cost.copy()
            self.remainder[WHITE] -= 1
        elif mode == 'normal':
            self.remainder = self.cost.copy()
        elif mode == 'mixed':
            self.contrib(THOPTER, 0)
        elif mode in ('all_creatures', 'creature_packets'):
            for _ in range(self.cost[0]):
                self.contrib(THOPTER, 0)
            self.add('raider', GIANT, owner)
            if mode == 'creature_packets':
                self.add('blocker', GIANT, self.foe)
        if self.id == ANGEL:
            if mode in ('split', 'creature', 'depletion'):
                self.add('damage_parent', SHOCK if mode == 'creature' else BOLT, self.foe, 'hand')
                self.add('later_bolt', BOLT, self.owner if mode == 'split' else self.foe, 'hand')
            if mode == 'flying_expiry':
                self.add('later_bolt', BOLT, self.foe, 'hand')
                self.add('ground', GIANT, self.foe)
        elif self.id == CHANT:
            if mode in ('combat', 'mixed_damage'):
                self.add('raider', GIANT, self.foe)
            if mode in ('mixed_damage', 'expiry'):
                self.add('pinger', PING, self.foe)
            if mode == 'mixed_damage':
                self.add('damage_parent', BOLT, self.foe, 'hand')

    def add(self, ref, name, player, zone='battlefield', *, tapped=False):
        assert player in (0, 1)
        row = card(name, ref, player, zone)
        if tapped:
            row['state'] = ['tapped']
        self.setup.append(row)
        self.cards[ref] = dict(name=name, player=player, zone=zone, tapped=tapped, damage=0)

    def contrib(self, name, color, *, hand=False, tapped=False):
        assert 'Creature' in self.oracle[name]['type_line']
        assert self.oracle[name]['colors'] == {0: [], GREEN: ['G'], WHITE: ['W']}[color]
        ref = f'helper_{len(self.contributors)}'
        self.contributors.append(ref)
        self.add(ref, name, self.owner, 'hand' if hand else 'battlefield', tapped=tapped)
        self.remainder[color] -= 1
        assert min(self.remainder) >= 0

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'cast_response':
            self.steps.append(step(op, ref=ref, value=value))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op.startswith('assert_zone'):
            fields['zone'] = ZONE_NAMES[zone]
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def wallet(self, actor, mana, *, funding=False):
        for color, value in enumerate(mana):
            self.emit('set_mana' if funding else 'assert_mana', player=actor, value=value, color=color)
        if funding:
            self.mana[actor] = list(mana)
            self.emit('refresh')

    def target(self, actor, target):
        if isinstance(target, str):
            self.emit('choose_target', target, player=actor)
        else:
            self.emit('choose_player', player=actor, value=target)

    def pending_spell(self, ref, target=None):
        actor = self.cards[ref]['player']
        self.wallet(actor, cost_from_oracle(self.oracle[self.cards[ref]['name']]), funding=True)
        if target is not None:
            self.target(actor, target)
        self.emit('cast', ref, value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.mana[actor] = ZERO.copy()
        self.wallet(actor, self.mana[actor])

    def resolve(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def resolve_damage(self, target, damage, *, creature_source=False):
        if self.chant and creature_source:
            self.life[self.owner] += damage
            return
        saved = min(self.shields[target], damage)
        self.shields[target] -= saved
        if isinstance(target, str):
            item = self.cards[target]
            item['damage'] += damage - saved
            if item['damage'] >= int(self.oracle[item['name']]['toughness']):
                item['zone'] = 'graveyard'
        else:
            self.life[target] -= damage - saved

    def damage_spell(self, ref, target):
        self.pending_spell(ref, target)
        self.resolve()
        self.cards[ref]['zone'] = 'graveyard'
        self.resolve_damage(target, 3 if self.cards[ref]['name'] == BOLT else 2)
        self.receipts()

    def start(self, actor):
        self.emit('turn', player=actor)
        for seat in (0, 1):
            self.wallet(seat, ZERO, funding=True)

    def convoke(self, shield=(), parent=None, target=None):
        if parent is not None:
            self.pending_spell(parent, target)
            self.emit('announce_spell', parent)
        self.wallet(self.owner, self.remainder, funding=True)
        allowed = self.mode != 'short'
        if self.mode == 'normal':
            # Full printed cost with untapped helpers offers a real alternative.
            self.emit('choose_dialog', player=self.owner, value=1)
        elif allowed:
            assert self.remainder[WHITE] < self.cost[WHITE]
            if self.owner == 0:
                for ref in self.contributors:
                    self.target(0, ref)
            for ref in self.contributors:
                self.cards[ref]['tapped'] = True
        self.emit('cast_response' if parent else 'cast', 'source', value=int(allowed))
        self.choices()
        self.emit('assert_stack', value=(2 if parent else int(allowed)))
        if allowed:
            self.mana[self.owner] = ZERO.copy()
        self.wallet(self.owner, self.mana[self.owner])
        for ref in (*self.contributors, 'foreign', 'unavailable'):
            self.observe(ref, 'tapped', self.cards[ref]['tapped'])
        if not allowed:
            self.observe('source', 'in_hand', 1)
            self.receipts()
            return
        if self.id == ANGEL:
            assert len(shield) == 5
            for recipient in shield:
                self.target(self.owner, recipient)
            self.shields.update(shield)
        self.resolve(response=bool(parent))
        self.cards['source']['zone'] = 'graveyard' if self.id == CHANT else 'battlefield'
        self.chant = self.id == CHANT
        if parent:
            self.cards[parent]['zone'] = 'graveyard'
            self.resolve_damage(target, 3 if self.cards[parent]['name'] == BOLT else 2)
        self.receipts()

    def next_turn(self, actor):
        assert not any(self.mana[0]) and not any(self.mana[1])
        self.emit('advance_turn')
        self.emit('untap_step', player=actor)
        self.emit('upkeep_step', player=actor)
        self.emit('turn', player=actor)
        self.chant = False
        self.shields.clear()
        for item in self.cards.values():
            if item['zone'] == 'battlefield':
                item['damage'] = 0
                if item['player'] == actor:
                    item['tapped'] = False

    def attack(self, ref, *, vigilant=False, grounded=False):
        actor = self.cards[ref]['player']
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        self.observe(ref, 'attacking', 1)
        self.cards[ref]['tapped'] = not vigilant
        self.observe(ref, 'tapped', not vigilant)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if grounded:
            self.emit('block', 'ground', ref, value=0)
            self.observe('ground', 'blocking_count', 0)
        blocked = self.mode == 'creature_packets'
        if blocked:
            self.emit('block', 'blocker', ref, value=1)
            self.observe('blocker', 'blocking_count', 1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('refresh')
        self.choices()
        power = int(self.oracle[self.cards[ref]['name']]['power'])
        if blocked:
            self.resolve_damage('blocker', power, creature_source=True)
            reply = int(self.oracle[self.cards['blocker']['name']]['power'])
            self.resolve_damage(ref, reply, creature_source=True)
        else:
            self.resolve_damage(1-actor, power, creature_source=True)
        self.receipts()

    def ping(self):
        self.emit('can_activate', 'pinger', value=1)
        self.target(self.foe, self.owner)
        self.emit('activate', 'pinger', value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.resolve()
        self.cards['pinger']['tapped'] = True
        self.resolve_damage(self.owner, 1, creature_source=True)
        self.receipts()

    def receipts(self):
        self.emit('assert_stack')
        for actor in (0, 1):
            self.emit('assert_life', player=actor, value=self.life[actor])
            self.wallet(actor, self.mana[actor])
            local = [item for item in self.cards.values() if item['player'] == actor]
            by_zone = Counter((item['name'], item['zone']) for item in local)
            for zone, code in ZONES.items():
                self.emit('assert_zone', player=actor, value=sum(item['zone'] == zone for item in local), zone=code)
                for name in sorted({item['name'] for item in local}):
                    self.emit('assert_zone_multiplicity', player=actor, value=by_zone[name, zone],
                              zone=code, index=self.rows[name]['csvid'])
        for ref, item in self.cards.items():
            if item['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', item['tapped'])
                if 'Creature' in self.oracle[item['name']]['type_line']:
                    self.observe(ref, 'damage', item['damage'])
        if self.cards['source']['zone'] == 'battlefield':
            for prop in ('power', 'toughness'):
                self.observe('source', prop, int(self.oracle[self.name][prop]))

    def run(self):
        response = self.mode in ('split', 'creature', 'mixed_damage', 'depletion')
        on_foe_turn = response or self.id == CHANT and self.mode == 'combat'
        self.start(self.foe if on_foe_turn else self.owner)
        if self.mode == 'vigilance':
            self.pending_spell('helper_0')
            self.resolve()
            self.cards['helper_0']['zone'] = 'battlefield'
            self.observe('helper_0', 'sick', 1)
        if self.id == ANGEL:
            shields = [self.owner] * 5
            if self.mode == 'split':
                shields = [self.owner] * 3 + [self.foe] * 2
            elif self.mode == 'creature':
                shields = ['helper_0'] + [self.owner] * 4
            self.convoke(shields, 'damage_parent' if response else None,
                         'helper_0' if self.mode == 'creature' else self.owner)
            if self.mode in ('split', 'creature', 'depletion'):
                self.damage_spell('later_bolt', self.foe if self.mode == 'split' else self.owner)
            if self.mode == 'flying_expiry':
                self.next_turn(self.foe)
                self.damage_spell('later_bolt', self.owner)
                self.next_turn(self.owner)
                self.attack('source', grounded=True)
        else:
            self.convoke(parent='damage_parent' if response else None, target=self.owner)
            if self.mode == 'mixed_damage':
                self.ping()
            if self.id == CHANT and self.mode in ('combat', 'mixed_damage', 'all_creatures', 'creature_packets'):
                self.attack('raider')
            if self.id == CHANT and self.mode == 'expiry':
                self.next_turn(self.foe)
                self.ping()
            if self.mode == 'vigilance':
                self.observe('source', 'sick', 1)
                self.next_turn(self.foe)
                self.next_turn(self.owner)
                self.observe('source', 'sick', 0)
                self.attack('source', vigilant=True)
