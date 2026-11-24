"""Paid self-regeneration and printed upkeep, typed-land, and combat riders."""
from collections import Counter, defaultdict
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'regeneration_self_and_riders'
PROFILE = Path(__file__).with_name('data') / 'regeneration_self_and_riders_wave8_profiles.json'
MUCK, HEDGE, HUNTED, THRUN = 12556, 8790, 7883, 11541
LAND_TYPES = {MUCK: 'Swamp', HEDGE: 'Plains'}
STONE_RAIN, DAY, WRATH, FAERIE = 'Stone Rain', 'Day of Judgment', 'Wrath of God', 'Faerie'
LEYLINE, DURESS, COUNTERSPELL = 'Leyline of Sanctity', 'Duress', 'Counterspell'
SCION = 'Scion of Oona'

TROLL, AUGUR, GLADIATOR, CHILD = 3691, 8887, 13134, 3721
COMMON = ('regenerate', 'short', 'terminate', 'proactive')
MODES = {
    TROLL: ('pay_once', 'decline'),
    AUGUR: ('discard_foe', 'discard_self', 'foe_upkeep'),
    GLADIATOR: ('blocks_two', 'blocks_owner', 'unblocked'),
    CHILD: ('pay_repeat', 'unfunded', 'trample'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
REGEN = {TROLL: [1, 0, 0, 1, 0, 0], AUGUR: [1, 1, 0, 0, 0, 0],
         GLADIATOR: [1, 1, 0, 0, 1, 0], CHILD: [1, 0, 0, 1, 0, 0]}
BEAR, GIANT = 'Grizzly Bears', 'Hill Giant'
MURDER, TERMINATE = 'Murder', 'Terminate'
PREFIX = 'RegenerationSelfAndRidersRecipe'
ZONE_NAMES = {code: name for name, code in ZONES.items()}
WITNESSES = frozenset(('Counterspell', 'Day of Judgment', 'Duress', 'Faerie', 'Forest', 'Grizzly Bears', 'Hill Giant', 'Island', 'Leyline of Sanctity', 'Mountain', 'Murder', 'Plains', 'Scion of Oona', 'Stone Rain', 'Swamp', 'Terminate', 'Wrath of God'))


MODES.update({
    MUCK: ('type_control', 'type_enter_leave', 'two_types'),
    HEDGE: ('type_control', 'type_enter_leave', 'two_types'),
    HUNTED: ('tokens', 'token_flight', 'protected_opponent'),
    THRUN: ('hexproof', 'uncounterable'),
})
REGEN.update({MUCK: [2, 1, 0, 0, 0, 0], HEDGE: [0, 0, 0, 0, 0, 1],
              HUNTED: [0, 0, 0, 1, 0, 0], THRUN: [1, 0, 0, 1, 0, 0]})


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'Creature' not in record['type_line']
            or 'regenerate' not in record['oracle_text'].lower()):
        raise ValueError('normal whole self-regeneration creature required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in COMMON + MODES[shape['csvid']]]


def validate_variants(shape, chosen):
    if (type(chosen) is not list or not chosen or any(type(item) is not str for item in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('distinct supported self-regeneration variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned self-regeneration primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed self-regeneration declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys()):
        raise ValueError('exact regeneration sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != ('token' if name == FAERIE else 'normal')):
                raise ValueError('regeneration Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented regeneration primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, chosen, rows, oracle):
    validate_variants(shape, chosen)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact self-regeneration primary CSVID required')
    cases = []
    for variant in chosen:
        mode, seat = variant.rsplit('_', 1)
        board = Board(rows[name], int(seat), mode, rows, oracle)
        board.run()
        cases.append(dict(name=f'RegenerationSelfAndRidersRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2897, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'])))
    return cases

class Board:
    def __init__(self, row, owner, mode, rows, oracle):
        self.id, self.name, self.owner, self.foe, self.mode = row['csvid'], row['name'], owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life, self.mana = [20, 20], [ZERO.copy(), ZERO.copy()]
        self.active = owner
        self.add('source', self.name, owner, 'hand')
        for actor in (0, 1):
            for index, name in enumerate(('Island', 'Mountain', 'Plains', 'Swamp')):
                self.add(f'deck_{actor}_{index}', name, actor, 'library')
        if mode in ('regenerate', 'short', 'terminate'):
            if self.id == THRUN:
                removal = WRATH if mode == 'terminate' else DAY
            else:
                removal = TERMINATE if mode == 'terminate' else MURDER
            self.add('removal', removal, self.foe, 'hand')
        if mode in ('pay_once', 'decline', 'pay_repeat', 'trample'):
            self.add('land_green', 'Forest', owner)
            self.add('land_other', 'Mountain' if self.id == TROLL else 'Forest', owner)
        if mode in ('discard_foe', 'discard_self', 'foe_upkeep'):
            target = owner if mode == 'discard_self' else self.foe
            self.add('discard_a', 'Mountain', target, 'hand')
            self.add('discard_b', 'Plains', target, 'hand')
        if mode in ('blocks_two', 'blocks_owner', 'unblocked', 'trample'):
            attacker = self.foe if mode == 'blocks_owner' else owner
            for index in range(2 if mode == 'blocks_two' else 1):
                if mode != 'trample':
                    self.add(f'attacker_{index}', GIANT, attacker)
                self.add(f'blocker_{index}', BEAR, 1-attacker)

        self.build_follower()

    def add(self, ref, name, player, zone='battlefield'):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, actor=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'choose_mana_source':
            self.steps.append(step(op, ref=ref))
            return
        if op in ('play_land', 'cast_response'):
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
        self.emit('assert_card', ref, prop=prop, value=value)

    def choices(self, unused=0):
        self.emit('assert_unused_choices', value=unused)
        self.emit('assert_choices_consumed')

    def pool(self, player, amount, *, fund=False):
        for color, value in enumerate(amount):
            self.emit('set_mana' if fund else 'assert_mana', player=player, value=value, color=color)
        if fund:
            self.mana[player] = list(amount)
            self.emit('refresh')

    def cast(self, ref, *, target=None):
        item = self.cards[ref]
        actor = item['actor']
        self.pool(actor, cost_from_oracle(self.oracle[item['name']]), fund=True)
        if target:
            self.emit('choose_target', target, player=actor)
        self.emit('cast', ref, value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.mana[actor] = ZERO.copy()
        self.pool(actor, ZERO)

    def settle(self, *, unused=0):
        self.emit('resolve')
        self.emit('refresh')
        self.choices(unused)
        self.emit('assert_stack')

    def receipt(self):
        self.emit('assert_stack')
        for actor in (0, 1):
            self.emit('assert_life', player=actor, value=self.life[actor])
            self.pool(actor, self.mana[actor])
            counts = defaultdict(Counter)
            for item in self.cards.values():
                if item['actor'] == actor:
                    counts[item['zone']][item['name']] += 1
            for zone, code in ZONES.items():
                self.emit('assert_zone', player=actor, zone=code, value=counts[zone].total())
                for name, count in sorted(counts[zone].items()):
                    self.emit('assert_zone_multiplicity', player=actor, zone=code, value=count, index=self.rows[name]['csvid'])
        for ref, item in self.cards.items():
            if item['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)

    def visit(self, payment=None, *, main=True):
        assert self.mana == [ZERO, ZERO]
        self.active = 1-self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=self.active)
        if payment is not None:
            assert self.active == self.owner
            self.emit('choose_dialog', player=self.owner, value=int(payment == 'decline'))
            if payment == 'pay' and self.owner == 0:
                for ref in ('land_other', 'land_green'):
                    self.emit('choose_mana_source', ref)
        self.emit('upkeep_step', player=self.active)
        self.choices()
        self.emit('assert_stack')
        if payment is not None:
            for ref in ('land_green', 'land_other'):
                self.observe(ref, 'tapped', int(payment == 'pay'))
        if main:
            self.emit('turn', player=self.active)

    def regeneration(self):
        fee = REGEN[self.id].copy()
        if self.mode == 'short':
            fee[next(i for i, amount in enumerate(fee) if amount)] -= 1
        if self.id == THRUN and self.mode != 'proactive':
            self.visit()
        self.pool(self.owner, fee, fund=True)
        if self.mode == 'proactive':
            self.emit('can_activate', 'source', value=1)
            return
        self.cast('removal', target=None if self.id == THRUN else 'source')
        self.emit('choose_regeneration', 'source')
        saved = self.mode == 'regenerate'
        self.settle(unused=int(not saved))
        self.cards['removal']['zone'] = 'graveyard'
        if saved:
            self.mana[self.owner] = ZERO.copy()
            self.observe('source', 'tapped', 1)
            self.observe('source', 'damage', 0)
            self.observe('source', 'attacking', 0)
        else:
            self.cards['source']['zone'] = 'graveyard'

    def rider_upkeep(self):
        self.visit()
        self.receipt()
        first = {'decline': 'decline', 'unfunded': None}.get(self.mode, 'pay')
        self.visit(first)
        if first != 'pay':
            self.cards['source']['zone'] = 'graveyard'
            return
        self.receipt()
        if self.mode == 'trample':
            self.combat(('source',), ('blocker_0',), excess=5)
            return
        self.visit()
        self.visit('pay' if self.id == CHILD else None)
        if self.id == TROLL:
            for ref in ('land_green', 'land_other'):
                self.observe(ref, 'tapped', 0)

    def augur(self):
        self.emit('can_activate', 'source', value=0)
        self.visit(main=False)
        self.emit('can_activate', 'source', value=0)
        if self.mode == 'foe_upkeep':
            return
        self.visit(main=False)
        target = self.owner if self.mode == 'discard_self' else self.foe
        self.emit('discard_policy', value=1)
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_player', player=self.owner, value=target)
        self.emit('activate', 'source', value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_play', 0)
        self.settle()
        self.cards['source']['zone'] = 'graveyard'
        for ref in ('discard_a', 'discard_b'):
            self.cards[ref]['zone'] = 'graveyard'

    def combat(self, attackers, blockers, *, excess=0):
        defending = 1-self.active
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for attacker, blocker in zip(attackers, blockers):
            self.emit('block', blocker, attacker, value=1)
        self.emit('advance_combat')
        if self.id == GLADIATOR:
            self.life[defending] -= len(blockers)
        self.emit('assert_life', player=defending, value=self.life[defending])
        self.emit('assert_life', player=self.active, value=self.life[self.active])
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')
        if blockers:
            for ref in blockers:
                self.cards[ref]['zone'] = 'graveyard'
        else:
            excess = 3
        self.life[defending] -= excess

    def gladiator(self):
        self.visit()
        if self.mode != 'blocks_owner':
            self.visit()
        total = 2 if self.mode == 'blocks_two' else 1
        attackers = tuple(f'attacker_{index}' for index in range(total))
        blockers = () if self.mode == 'unblocked' else tuple(f'blocker_{index}' for index in range(total))
        self.combat(attackers, blockers)

    def build_follower(self):
        if self.id in LAND_TYPES and self.mode in ('type_control', 'type_enter_leave', 'two_types'):
            land = LAND_TYPES[self.id]
            self.add('foe_type', land, self.foe)
            if self.mode == 'type_control':
                self.add('wrong_type', 'Island', self.owner)
            else:
                self.add('my_type', land, self.owner, 'hand')
                if self.mode == 'type_enter_leave':
                    self.add('stone_rain', STONE_RAIN, self.foe, 'hand')
                else:
                    self.add('second_type', land, self.owner)
        if self.id == HUNTED:
            if self.mode == 'protected_opponent':
                self.add('leyline', LEYLINE, self.foe)
                self.add('duress', DURESS, self.owner, 'hand')
            elif self.mode == 'tokens':
                self.add('faerie_lord', SCION, self.foe)
            elif self.mode == 'token_flight':
                self.add('ground', GIANT, self.owner)
        if self.id == THRUN:
            if self.mode == 'uncounterable':
                self.add('counterspell', COUNTERSPELL, self.foe, 'hand')
            elif self.mode == 'hexproof':
                self.add('own_targeter', MURDER, self.owner, 'hand')
                self.add('foe_targeter', MURDER, self.foe, 'hand')
                self.add('plain', BEAR, self.owner)

    def land_bonus(self):
        boost = self.mode == 'two_types'
        power, toughness = (int(self.oracle[self.name][key]) for key in ('power', 'toughness'))
        if self.mode == 'type_control':
            return
        self.emit('play_land', 'my_type', value=1)
        self.choices()
        self.emit('refresh')
        self.cards['my_type']['zone'] = 'battlefield'
        self.observe('source', 'power', power + 1)
        self.observe('source', 'toughness', toughness + 1)
        if not boost:
            self.visit()
            self.cast('stone_rain', target='my_type')
            self.settle()
            self.cards['stone_rain']['zone'] = 'graveyard'
            self.cards['my_type']['zone'] = 'graveyard'
            self.observe('source', 'power', power)
            self.observe('source', 'toughness', toughness)
            self.observe('foe_type', 'in_play', 1)

    def hunted_tokens(self):
        token_id = self.rows[FAERIE]['csvid']
        for ordinal in range(4):
            ref = f'faerie_{ordinal}'
            self.cards[ref] = dict(name=FAERIE, actor=self.foe, zone='battlefield')
            self.emit('bind_permanent_copy', ref, player=self.foe, value=token_id, index=ordinal)
        # Scion's actual static modifier distinguishes the Faerie subtype.
        size = 2 if self.mode == 'tokens' else 1
        for trait, value in (('power', size), ('toughness', size), ('color', 4), ('token', 1)):
            self.emit('assert_matching_permanents', prop=trait, player=self.foe, value=value, index=token_id)

    def hunted(self):
        if self.mode == 'protected_opponent':
            self.pool(self.owner, [0, 1, 0, 0, 0, 0], fund=True)
            self.emit('can_cast', 'duress', value=0)
            return
        if self.mode == 'token_flight':
            self.visit()
            self.emit('begin_attackers')
            self.emit('attack', 'faerie_0', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block', 'ground', 'faerie_0', value=0)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.settle()
            self.emit('end_combat')
            self.life[self.owner] -= 1

    def thrun(self):
        self.emit('can_target_creature', 'own_targeter', 'source', value=1)
        self.emit('can_target_creature', 'foe_targeter', 'source', value=0)
        self.emit('can_target_creature', 'foe_targeter', 'plain', value=1)

    def counter_thrun(self):
        self.emit('announce_spell', 'source')
        self.pool(self.foe, cost_from_oracle(self.oracle[COUNTERSPELL]), fund=True)
        self.emit('cast_response', 'counterspell', value=1)
        self.choices()
        self.emit('assert_stack', value=2)
        self.mana[self.foe] = ZERO.copy()
        self.pool(self.foe, ZERO)
        self.emit('resolve_response')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')
        self.cards['counterspell']['zone'] = 'graveyard'

    def run(self):
        self.emit('turn', player=self.active)
        for player in (0, 1):
            self.pool(player, ZERO, fund=True)
        self.cast('source')
        if self.id == THRUN and self.mode == 'uncounterable':
            self.counter_thrun()
        else:
            self.settle()
        self.cards['source']['zone'] = 'battlefield'
        if self.id == HUNTED and self.mode != 'protected_opponent':
            self.hunted_tokens()
        entry_bonus = int(self.id in LAND_TYPES and self.mode == 'two_types')
        self.observe('source', 'power', int(self.oracle[self.name]['power']) + entry_bonus)
        self.observe('source', 'toughness', int(self.oracle[self.name]['toughness']) + entry_bonus)
        if self.mode in COMMON:
            self.regeneration()
        elif self.id in (TROLL, CHILD):
            self.rider_upkeep()
        elif self.id == AUGUR:
            self.augur()
        elif self.id == GLADIATOR:
            self.gladiator()
        elif self.id in LAND_TYPES:
            self.land_bonus()
        elif self.id == HUNTED:
            self.hunted()
        elif self.id == THRUN and self.mode == 'hexproof':
            self.thrun()
        self.receipt()
