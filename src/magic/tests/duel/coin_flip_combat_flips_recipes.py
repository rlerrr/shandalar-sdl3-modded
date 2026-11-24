"""Paid, seeded combat coin effects and their actual rules boundaries."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from next_upkeep_cantrip_additional_riders_recipes import COIN_SEED, actual_call, xorshift
from recipes import card, cost_from_oracle, step

FAMILY = 'coin_flip_combat_flips'
PROFILE = Path(__file__).with_name('data') / 'coin_flip_combat_flips_wave8_profiles.json'
DOLL, DJINN, EFREET, RIG = 11912, 442, 465, 12869
MODES = {
    DOLL: ('win', 'lose', 'player_damage', 'fight', 'destroy_source'),
    DJINN: ('win', 'lose', 'idle', 'blocked_win'),
    EFREET: ('win', 'lose', 'other_blocker', 'replay_block'),
    RIG: ('trample_win', 'damage_sacrifice', 'death_blast', 'death_quiet', 'exile_instead'),
}
FEE_STATS = {DOLL: ('{5}', 1, 1), DJINN: ('{R}{R}{R}', 6, 3),
             EFREET: ('{R}{R}{R}', 3, 6), RIG: ('{4}', 4, 4)}
PREFIX = 'CombatCoinFlipsRecipe'
ZONES = {'hand': 0, 'battlefield': 1, 'graveyard': 3, 'exile': 4}
BEAR, GIANT, WALL = 'Grizzly Bears', 'Hill Giant', 'Wall of Stone'
FODDER = "Mons's Goblin Raiders"
FIGHT, SHOCK, MURDER = 'Prey Upon', 'Shock', 'Murder'
FOREST = 'Forest'
ZERO = [0] * 6

EXILE = 'Swords to Plowshares'
WITNESSES = {BEAR, GIANT, WALL, FODDER, FIGHT, SHOCK, MURDER, FOREST, EXILE}
EFREET_GAP = 'A second same-turn combat is outside the declaration driver, so the fresh-block lock after losing is unobserved.'
ZONE_NAMES = {value: name for name, value in ZONES.items()}
SEATED = {'assert_life', 'set_mana', 'assert_mana', 'assert_zone_multiplicity',
          'choose_target', 'choose_dialog', 'turn', 'untap_step'}
VALUES = {'assert_life', 'assert_card', 'assert_stack', 'assert_zone_multiplicity',
          'assert_unused_choices', 'choose_dialog', 'set_mana', 'assert_mana',
          'attack', 'block', 'can_finish_attackers'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pinned = profiles()['sources'].get(record.get('name'))
    if (not pinned or pinned['registry']['csvid'] != csvid or not exact(pinned['oracle'], record)
            or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('complete pinned normal combat coin primary required')
    mana, power, toughness = FEE_STATS[csvid]
    if (record.get('mana_cost'), record.get('power'), record.get('toughness')) != (mana, str(power), str(toughness)):
        raise ValueError('exact printed combat coin cost and body required')
    return {'csvid': csvid, 'mana': cost_from_oracle(record)}


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def validate_variants(shape, chosen):
    if (type(chosen) is not list or not chosen or any(type(mode) is not str for mode in chosen)
            or len(chosen) != len(set(chosen)) or set(chosen) - set(variants(shape))):
        raise ValueError('distinct supported combat coin variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned:
        raise ValueError('pinned combat coin source required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared combat coin parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent combat coin witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys():
        raise ValueError('exact four-source combat coin profile required')
    for group, entries in pins.items():
        for name, pinned in entries.items():
            row = rows.get(name, {})
            if (not exact(pinned['oracle'], oracle.get(name)) or not exact(pinned['registry'], row)
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact combat coin source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented combat coin primary required')
                classify(oracle[name], row['csvid'])


def coin_call(state, wins):
    # Real player_flips_a_coin samples once for the default call, and the real
    # flipped face is the next xorshift sample. No Krark's Thumb is present.
    advanced = xorshift(xorshift(state))
    face = advanced % 2
    return advanced, face if wins else 1 - face


class Board:
    def __init__(self, csvid, name, scene, owner, rows, oracle):
        self.csvid, self.name, self.scene = csvid, name, scene
        self.owner, self.foe = owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.life, self.actual_calls = [20, 20], []
        self.setup, self.steps, self.cards = [], [], {}
        self.state = COIN_SEED
        self.add('source', name, owner, 'hand')
        if (csvid == DOLL and scene in ('win', 'lose', 'fight')
                or csvid == DJINN and scene == 'blocked_win'
                or csvid == RIG and scene == 'trample_win'):
            self.add('bear', BEAR, self.foe)
        if csvid == DOLL and scene == 'fight':
            self.add('fight', FIGHT, owner, 'hand')
        if csvid == DOLL and scene == 'destroy_source':
            self.add('spell', MURDER, self.foe, 'hand')
        if csvid == RIG and scene == 'exile_instead':
            self.add('spell', EXILE, self.foe, 'hand')
        if csvid == EFREET:
            self.add('giant', GIANT, self.foe)
            if scene == 'other_blocker':
                self.add('wall', WALL, owner)
        if csvid == RIG and scene in ('damage_sacrifice', 'death_blast', 'death_quiet'):
            self.add('spell', SHOCK if scene == 'damage_sacrifice' else MURDER, self.foe, 'hand')
            for player in (0, 1):
                self.add(f'weak_{player}', FODDER, player)
                self.add(f'wall_{player}', WALL, player)
                self.add(f'land_{player}', FOREST, player)

    def add(self, ref, name, player, zone='battlefield'):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = name, player, zone

    def move(self, ref, zone):
        name, player, _ = self.cards[ref]
        self.cards[ref] = name, player, zone

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

    def pool(self, op, seat, values=ZERO):
        for color, amount in enumerate(values):
            self.emit(op, player=seat, color=color, value=amount)

    def receipt(self):
        self.emit('assert_stack')
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            self.pool('assert_mana', seat)
            subset = [(name, zone) for name, p, zone in self.cards.values() if p == seat]
            counts = Counter(subset)
            for name in sorted({name for name, _ in subset}):
                for zone, number in ZONES.items():
                    self.emit('assert_zone_multiplicity', player=seat, zone=number,
                              index=self.rows[name]['csvid'], value=counts[name, zone])
        for ref, (_, _, zone) in self.cards.items():
            if zone == 'battlefield':
                self.observe(ref, 'in_play', 1)

    def pay(self, ref, *targets):
        name, seat, _ = self.cards[ref]
        self.pool('set_mana', seat, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        for target in targets:
            self.emit('choose_target', target, player=seat)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.pool('assert_mana', seat)

    def resolve(self, *, planned=None):
        self.emit('resolve')
        if planned is None:
            self.emit('assert_choices_consumed')
        else:
            self.emit('assert_unused_choices', value=planned)
        self.emit('assert_stack')
        self.emit('refresh')

    def ready(self):
        self.emit('turn', player=self.owner)
        for seat in (0, 1):
            self.pool('set_mana', seat)
        self.pay('source')
        self.resolve()
        self.move('source', 'battlefield')
        _, power, toughness = FEE_STATS[self.csvid]
        self.observe('source', 'power', power)
        self.observe('source', 'toughness', toughness)
        self.receipt()

    def turn(self, seat):
        self.emit('advance_turn')
        self.emit('untap_step', player=seat)
        self.emit('turn', player=seat)
        self.emit('assert_choices_consumed')

    def age_source(self):
        self.turn(self.foe)
        self.turn(self.owner)
        self.observe('source', 'sick', 0)
        self.observe('source', 'tapped', 0)

    def call(self, outcome, *, damage_window=False):
        if damage_window:
            # The production damage-prevention window enters AI search and
            # consumes its setup sample before the ordinary two-sample flip.
            self.state = xorshift(self.state)
        self.state, choice = coin_call(self.state, outcome)
        if not self.actual_calls and not damage_window:
            assert choice == actual_call(outcome)
        self.actual_calls.append(dict(owner=self.owner, wins=outcome, call=choice))
        self.emit('choose_dialog', player=self.owner, value=choice)

    def begin(self, attacker=None, *, required=False):
        self.emit('begin_required_attackers' if required else 'begin_attackers')
        if required:
            self.emit('can_finish_attackers', value=0)
        if attacker is not None:
            self.emit('attack', attacker, value=1)
            self.observe(attacker, 'attacking', 1)
        if required:
            self.emit('can_finish_attackers', value=1)

    def blockers(self, attacker, *blockers):
        self.emit('begin_blockers')
        for blocker in blockers:
            self.emit('block', blocker, attacker, value=1)

    def damage(self, flipped=False):
        self.emit('combat_damage')
        self.resolve(planned=0 if flipped else None)

    def finish(self):
        self.receipt()
        self.emit('end_combat')
        self.emit('assert_choices_consumed')

    def doll(self):
        if self.scene == 'destroy_source':
            self.pay('spell', 'source')
            self.resolve()
            self.move('spell', 'graveyard')
            self.receipt()
            return
        if self.scene == 'fight':
            self.pay('fight', 'source', 'bear')
            # The printed trigger excludes fight damage. Leave one optional
            # actual call ready so an erroneous offer still reports cleanly.
            self.call(True)
            self.resolve(planned=1)
            self.move('fight', 'graveyard')
            self.observe('source', 'damage', 2)
            self.observe('bear', 'damage', 1)
            self.receipt()
            return
        self.age_source()
        self.begin('source')
        self.emit('finish_attackers')
        if self.scene == 'player_damage':
            self.blockers('source')
            self.emit('advance_combat')
            self.damage()
            self.life[self.foe] -= 1
            self.finish()
            return
        self.blockers('source', 'bear')
        self.emit('advance_combat')
        self.observe('source', 'blocked', 1)
        self.call(self.scene == 'win', damage_window=True)
        self.damage(flipped=True)
        self.observe('source', 'damage', 2)
        if self.scene == 'win':
            self.move('bear', 'graveyard')
        else:
            self.observe('bear', 'damage', 1)
        self.finish()

    def djinn(self):
        self.age_source()
        attacking = self.scene != 'idle'
        wins = self.scene in ('win', 'blocked_win')
        self.begin('source' if attacking else None)
        if attacking:
            self.call(wins)
        self.emit('finish_attackers')
        self.emit('assert_unused_choices', value=0)
        self.observe('source', 'attacking', int(wins))
        self.observe('source', 'tapped', int(attacking))
        blocked = self.scene == 'blocked_win'
        blockers = ('bear',) if blocked else ()
        self.blockers('source', *blockers)
        self.emit('advance_combat')
        if blocked:
            self.observe('source', 'blocked', 1)
        self.damage()
        if blocked:
            self.move('bear', 'graveyard')
            self.observe('source', 'damage', 2)
        else:
            self.life[self.foe] -= 6 * int(wins)
        self.finish()

    def efreet(self):
        self.turn(self.foe)
        self.begin('giant')
        self.emit('finish_attackers')
        others = ('wall',) if self.scene == 'other_blocker' else ()
        self.blockers('giant', 'source', *others)
        wins = self.scene in ('win', 'replay_block')
        if self.scene == 'replay_block':
            state = self.state
            self.emit('capture')
            self.declare_efreet(wins)
            self.emit('record_state')
            self.emit('restore')
            self.emit('ai_roundtrip')
            self.state = state
            self.declare_efreet(wins)
            self.emit('assert_recorded_state')
        else:
            self.declare_efreet(wins)
        self.observe('source', 'blocking_count', int(wins))
        self.observe('giant', 'blocked', int(self.scene != 'lose'))
        self.damage()
        if wins:
            self.move('giant', 'graveyard')
            self.observe('source', 'damage', 3)
        elif others:
            self.observe('wall', 'damage', 3)
            self.observe('source', 'damage', 0)
        else:
            self.life[self.owner] -= 3
            self.observe('source', 'damage', 0)
        self.finish()

    def declare_efreet(self, wins):
        self.call(wins)
        self.emit('advance_combat')
        self.emit('assert_unused_choices', value=0)

    def rig(self):
        if self.scene == 'exile_instead':
            self.pay('spell', 'source')
            self.resolve()
            self.move('source', 'exile')
            self.move('spell', 'graveyard')
            self.life[self.owner] += FEE_STATS[RIG][1]
            self.receipt()
            return
        self.emit('reset_death_history')
        if self.scene == 'trample_win':
            self.age_source()
            self.begin('source', required=True)
            self.emit('finish_attackers')
            self.blockers('source', 'bear')
            self.emit('advance_combat')
            self.call(True, damage_window=True)
            self.damage(flipped=True)
            self.life[self.foe] -= 2
            self.move('bear', 'graveyard')
            self.observe('source', 'damage', 2)
            self.finish()
            return
        self.pay('spell', 'source')
        if self.scene == 'damage_sacrifice':
            self.call(False, damage_window=True)  # Shock hit: the surviving Rig sacrifices itself.
            self.call(True)   # Death trigger wins: no explosion.
        else:
            self.call(self.scene == 'death_quiet')  # Murder causes death without damage.
        self.resolve(planned=0)
        self.move('source', 'graveyard')
        self.move('spell', 'graveyard')
        if self.scene == 'death_blast':
            for seat in (0, 1):
                self.life[seat] -= 4
                self.move(f'weak_{seat}', 'graveyard')
                self.observe(f'wall_{seat}', 'damage', 4)
        else:
            for seat in (0, 1):
                self.observe(f'wall_{seat}', 'damage', 0)
        self.receipt()

    def run(self):
        self.ready()
        {DOLL: self.doll, DJINN: self.djinn, EFREET: self.efreet, RIG: self.rig}[self.csvid]()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    scene, seat = variant.rsplit('_', 1)
    board = Board(csvid, name, scene, int(seat), rows, oracle)
    board.run()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=COIN_SEED,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'],
                             untested_components=[EFREET_GAP] if csvid == EFREET else []))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered combat coin source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
