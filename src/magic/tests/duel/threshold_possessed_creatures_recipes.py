"""Threshold, printed combat and real destruction for the Possessed cycle."""
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


FAMILY = 'threshold_possessed_creatures'
PROFILE = Path(__file__).with_name('data') / 'threshold_possessed_creatures_wave8_profiles.json'
THRESHOLD = 'Threshold — As long as there are seven or more cards in your graveyard, '
COMMON = ('six', 'seven', 'opponent_only')
NOVEL = ('gain_threshold', 'lose_threshold', 'pending_loss', 'countered', 'short_generic', 'replay')
CRYPT = "Tormod's Crypt"
AMULET = "Traveler's Amulet"
NEW_WITNESSES = {CRYPT, AMULET, 'Disenchant', 'Counterspell'}
GAP = 'Color-word text changes, control changes, and multiplayer are outside these cases.'
SEED = 2370
PREFIX = 'ThresholdPossessedCreaturesRecipe'
AVEN, BARBARIAN, CENTAUR, NOMAD = 5861, 5862, 5863, 5864
PRIMARIES = {
    AVEN: ('Flying', 'blue', 'U', 4, 'Wind Drake', 'Grizzly Bears'),
    BARBARIAN: ('First strike', 'red', 'R', 16, "Mons's Goblin Raiders", 'Grizzly Bears'),
    CENTAUR: ('Trample', 'green', 'G', 8, 'Grizzly Bears', 'Hill Giant'),
    NOMAD: ('Vigilance', 'white', 'W', 32, 'Suntail Hawk', 'Grizzly Bears'),
}
MODES = ('six', 'seven', 'opponent_only', 'seven_ability', 'wrong_black')
ACTIVE = {'seven', 'seven_ability', 'wrong_black'}
ZERO = [0] * 6

WITNESSES = {'Mountain', 'Island', 'Plains', 'Hill Giant', 'Squire', *NEW_WITNESSES}
WITNESSES.update(name for shape in PRIMARIES.values() for name in shape[-2:])
ACTIVE.update({'lose_threshold', 'pending_loss', 'countered', 'short_generic', 'replay'})


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in PRIMARIES or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces') or not record.get('type_line', '').startswith('Creature — ')):
        raise ValueError('complete normal Possessed creature required')
    keyword, color, pip, _, _, _ = PRIMARIES[csvid]
    text = (keyword + '\n' + THRESHOLD + 'this creature gets +1/+1, is black, and has '
            f'"{{2}}{{B}}, {{T}}: Destroy target {color} creature."')
    if (record.get('oracle_text') != text or record.get('keywords') != [keyword, 'Threshold']
            or record.get('power') != '3' or record.get('toughness') != '3'
            or record.get('mana_cost') != '{2}' + f'{{{pip}}}' * 2 or record.get('colors') != [pip]):
        raise ValueError('complete Possessed Threshold ability, cost and keyword required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), fee=cost_from_oracle({'mana_cost': '{2}{B}'}))


def variants(shape):
    return [f'{mode}_{seat}' for mode in (*MODES, *NOVEL) for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Possessed variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('complete pinned Possessed primary required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared Possessed branch required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Possessed witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Possessed primary or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Possessed primary required')
                classify(oracle[name], row['csvid'])
    if oracle['Mountain']['type_line'] != 'Basic Land — Mountain':
        raise ValueError('same-name graveyard witness must be Mountain')


class Duel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.active = mode in ACTIVE
        self.own_grave, self.enemy_grave = (7 if self.active else 6), (7 if mode == 'opponent_only' else 0)
        _, _, self.pip, self.color, self.victim, self.wrong = PRIMARIES[csvid]
        self.fee = classify(record, csvid)['fee']
        self.extra_graves = {actor: [], self.foe: []}
        self.add('source', actor, record['name'], 'hand')
        self.add('victim', self.foe, self.victim, 'battlefield')
        self.add('wrong_color', self.foe, self.wrong, 'battlefield')
        self.add('ally', actor, self.victim, 'battlefield')
        if csvid in (BARBARIAN, CENTAUR) and mode in COMMON:
            self.add('blocker', self.foe, 'Hill Giant' if csvid == BARBARIAN else 'Squire', 'battlefield')
        for player, count in ((actor, self.own_grave), (self.foe, self.enemy_grave)):
            for index in range(count):
                self.add(f'grave_{player}_{index}', player, 'Mountain', 'graveyard')
            for index, name in enumerate(('Island', 'Plains')):
                self.add(f'library_{player}_{index}', player, name, 'library')

        if mode == 'gain_threshold':
            self.add('artifact', self.foe, AMULET, 'battlefield')
            self.add('disenchant', self.actor, 'Disenchant', 'hand')
        if mode in ('lose_threshold', 'pending_loss'):
            self.add('crypt', self.foe, CRYPT, 'battlefield')
        if mode == 'countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', **values):
        if ref:
            values['ref'] = ref
        if other:
            values['other'] = other
        self.steps.append(step(op, **values))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, property=prop, value=value)

    def mana(self, op, player, pool):
        for color, amount in enumerate(pool):
            self.emit(op, player=player, value=amount, color=color)

    def resolve(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.emit('refresh')

    def cast(self, ref, target=None):
        cost = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        self.fund(cost)
        if target:
            self.emit('choose_target', target, player=self.actor)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.resolve()

    def advance(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)

    def zone(self, name, seat, zone, count):
        self.emit('assert_zone_multiplicity', player=seat, zone=zone,
                  index=self.rows[name]['csvid'], value=count)

    def state(self):
        name = self.record['name']
        self.emit('refresh')
        self.observe('source', 'in_play', 1)
        self.observe('source', 'power', 3 + self.active)
        self.observe('source', 'toughness', 3 + self.active)
        self.observe('source', 'color', 2 if self.active else self.color)
        self.zone(name, self.actor, 'hand', 0)
        self.zone(name, self.actor, 'battlefield', 1)
        self.zone(name, self.actor, 'graveyard', 0)
        for seat, count in ((self.actor, self.own_grave), (self.foe, self.enemy_grave)):
            self.emit('assert_zone', player=seat, zone='graveyard', value=count + len(self.extra_graves[seat]))
            self.zone('Mountain', seat, 'graveyard', count)
            for name in self.extra_graves[seat]:
                self.zone(name, seat, 'graveyard', 1)

    def fund(self, pool):
        self.mana('set_mana', self.actor, pool)
        self.emit('refresh')

    def begin_destruction(self):
        for ref, expected in (('victim', 1), ('ally', 1), ('wrong_color', 0)):
            self.emit('probe_activation_target', 'source', ref, index=-1, value=expected)
        self.emit('choose_target', 'victim', player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.mana('assert_mana', self.foe, ZERO)
        self.observe('source', 'tapped', 1)
        self.zone(self.victim, self.foe, 'battlefield', 1)

    def finish_destruction(self):
        self.emit('resolve')
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.zone(self.victim, self.foe, 'battlefield', 0)
        self.zone(self.victim, self.foe, 'graveyard', 1)
        self.zone(self.victim, self.foe, 'exile', 0)
        self.zone(self.victim, self.actor, 'battlefield', 1)
        self.zone(self.wrong, self.foe, 'battlefield', 1)

    def activation(self):
        self.begin_destruction()
        self.finish_destruction()
        self.fund(self.fee)
        self.emit('can_activate', 'source', value=0)
        self.emit('activate', 'source', value=0)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.actor, self.fee)
        self.observe('source', 'tapped', 1)

    def combat(self):
        power = 3 + self.active
        blocked = self.csvid in (BARBARIAN, CENTAUR)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.observe('source', 'attacking', 1)
        self.observe('source', 'tapped', int(self.csvid != NOMAD))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if self.csvid == AVEN:
            self.emit('block', 'wrong_color', 'source', value=0)
        if blocked:
            self.emit('block', 'blocker', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', int(blocked))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.observe('source', 'in_play', 1)
        self.observe('source', 'tapped', int(self.csvid != NOMAD))
        self.observe('source', 'damage', int(self.csvid == CENTAUR))
        if blocked:
            self.zone(self.cards['blocker']['name'], self.foe, 'graveyard', 1)
        damage = 0 if self.csvid == BARBARIAN else power - 2 if self.csvid == CENTAUR else power
        self.emit('assert_life', player=self.foe, value=20 - damage)
        self.emit('assert_life', player=self.actor, value=20)
        self.emit('can_activate', 'source', value=int(self.active and self.csvid == NOMAD))
        self.mana('assert_mana', self.actor, self.fee)

    def gain_threshold(self):
        self.fund(self.fee)
        self.emit('can_activate', 'source', value=0)
        self.cast('disenchant', target='artifact')
        self.active = True
        self.extra_graves[self.actor].append('Disenchant')
        self.extra_graves[self.foe].append(AMULET)
        self.zone(AMULET, self.foe, 'battlefield', 0)
        self.state()

    def begin_crypt(self, stack_count):
        self.emit('choose_player', player=self.foe, value=self.actor)
        self.emit('activate', 'crypt', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=stack_count)
        self.zone(CRYPT, self.foe, 'battlefield', 0)
        self.zone(CRYPT, self.foe, 'graveyard', 1)
        self.zone('Mountain', self.actor, 'graveyard', 7)

    def threshold_lost(self):
        self.active = False
        self.own_grave = 0
        self.extra_graves[self.foe] += [CRYPT]
        self.zone('Mountain', self.actor, 'exile', 7)
        self.state()
        self.emit('can_activate', 'source', value=0)

    def lose_threshold(self):
        self.fund(self.fee)
        self.emit('can_activate', 'source', value=1)
        self.begin_crypt(1)
        self.resolve()
        self.threshold_lost()
        self.emit('activate', 'source', value=0)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.actor, self.fee)
        self.zone(self.victim, self.foe, 'battlefield', 1)
        self.observe('source', 'tapped', 0)

    def pending_loss(self):
        self.begin_destruction()
        self.begin_crypt(2)
        self.finish_destruction()
        self.extra_graves[self.foe].append(self.victim)
        self.threshold_lost()
        self.observe('source', 'tapped', 1)
        self.mana('assert_mana', self.actor, ZERO)

    def counter_source(self):
        self.fund(cost_from_oracle(self.record))
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2)
        self.mana('assert_mana', self.foe, ZERO)
        self.resolve(response=True)
        self.zone(self.record['name'], self.actor, 'hand', 0)
        self.zone(self.record['name'], self.actor, 'battlefield', 0)
        self.zone(self.record['name'], self.actor, 'graveyard', 1)
        self.zone('Counterspell', self.foe, 'graveyard', 1)
        self.zone('Mountain', self.actor, 'graveyard', 7)
        self.zone(self.victim, self.foe, 'battlefield', 1)
        self.zone(self.victim, self.actor, 'battlefield', 1)

    def replay_activation(self):
        self.emit('capture')
        start = len(self.steps)
        self.activation()
        tail = deepcopy(self.steps[start:])
        self.emit('record_state')
        self.emit('restore')
        self.emit('ai_roundtrip')
        self.steps.extend(tail)
        self.emit('assert_recorded_state')

    def build(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        if self.mode == 'countered':
            self.counter_source()
            return
        self.cast('source')
        self.state()
        self.observe('source', 'sick', 1)
        for seat in (self.foe, self.actor):
            self.advance(seat)
        self.observe('source', 'sick', 0)
        self.observe('source', 'tapped', 0)
        self.state()
        for ref, name in (('victim', self.victim), ('ally', self.victim), ('wrong_color', self.wrong)):
            self.observe(ref, 'in_play', 1)
            mask = next(color for _, _, pip, color, _, _ in PRIMARIES.values() if self.oracle[name]['colors'] == [pip])
            self.observe(ref, 'color', mask)
        if self.mode == 'gain_threshold':
            self.gain_threshold()
        elif self.mode == 'lose_threshold':
            self.lose_threshold()
            return
        budget = {
            'wrong_black': cost_from_oracle({'mana_cost': '{2}{U}'}),
            'short_generic': cost_from_oracle({'mana_cost': '{1}{B}'}),
        }.get(self.mode, self.fee)
        self.fund(budget)
        available = self.active and self.mode not in ('wrong_black', 'short_generic')
        self.emit('can_activate', 'source', value=int(available))
        if not available:
            self.emit('activate', 'source', value=0)
            self.emit('assert_stack', value=0)
            self.emit('assert_choices_consumed')
            self.observe('source', 'tapped', 0)
            self.zone(self.victim, self.foe, 'battlefield', 1)
            self.mana('assert_mana', self.actor, budget)
        if self.mode in ('seven_ability', 'gain_threshold'):
            self.activation()
        elif self.mode == 'replay':
            self.replay_activation()
        elif self.mode == 'pending_loss':
            self.pending_loss()
        elif self.mode in COMMON:
            self.combat()



def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    duel = Duel(csvid, oracle[name], int(seat), mode, rows, oracle)
    duel.build()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=duel.setup, steps=duel.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=[GAP]))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact native Possessed source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
