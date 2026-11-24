"""Single-cost kicker effects with actual combat, restrictions, and expiry."""
import copy
import json
from pathlib import Path
import re

from library_search_recipes import wave6_exact
from recipes import card, cost_from_oracle, step

FAMILY = 'kicker_combat_and_restrictions'
PROFILE = Path(__file__).parent / 'data/kicker_combat_and_restrictions_wave8_profiles.json'
KICKER = re.compile(r'Kicker ((?:\{(?:\d+|[BUGRW])\})+) \(You may pay an additional \1 as you cast this spell\.\)')
SEED = 2334
PREFIX = 'KickerCombatRestrictionRecipe'
BUSHWHACKER, CHANT, SKIZZIK, BITE, VINES = 10546, 5271, 5118, 10704, 10707
RIDERS = {
    BUSHWHACKER: ('When this creature enters, if it was kicked, creatures you control get +1/+0 and gain haste until end of turn.',),
    CHANT: ("Target player can't cast spells this turn. If this spell was kicked, creatures can't attack this turn.",),
    SKIZZIK: ('Trample, haste', "At the beginning of the end step, if this creature wasn't kicked, sacrifice it."),
    BITE: ('Target creature gets +3/+0 until end of turn. If this spell was kicked, that creature gains lifelink until end of turn. (Damage dealt by the creature also causes its controller to gain that much life.)',),
    VINES: ("Target creature can't be the target of spells or abilities your opponents control this turn. If this spell was kicked, that creature gets +4/+4 until end of turn.",),
}
BEAR = 'Grizzly Bears'
MISTS = 'Reach Through Mists'
GROWTH = 'Giant Growth'
ZERO = [0] * 6


ZONES = {0: 'hand', 1: 'battlefield', 2: 'library', 3: 'graveyard'}
COUNTER = 'Counterspell'
BOUNCE = 'Unsummon'
WITNESSES = {BEAR, MISTS, GROWTH, 'Island', 'Plains', COUNTER, BOUNCE, 'Hill Giant'}
COMMON = ('paid', 'declined', 'base_only', 'wrong_kicker_color', 'countered_paid', 'replay')
EXTRA = {
    BITE: ('lost_target', 'expired_combat'),
    VINES: ('paid_foreign', 'declined_foreign', 'lost_target', 'other_target'),
}
KEYWORDS = {SKIZZIK: {'Kicker', 'Trample', 'Haste'}}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    if not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces'):
        raise ValueError('whole normal kicker Oracle record required')
    cost, *clauses = record['oracle_text'].splitlines()
    fee = KICKER.fullmatch(cost)
    id_by_clauses = {clauses: csvid for csvid, clauses in RIDERS.items()}
    csvid = id_by_clauses.get(tuple(clauses))
    if not fee or csvid is None or set(record['keywords']) != KEYWORDS.get(csvid, {'Kicker'}):
        raise ValueError('unreviewed complete kicker wording, fee, or keywords')
    is_creature = csvid in (BUSHWHACKER, SKIZZIK)
    if (is_creature and not record['type_line'].startswith('Creature — ')) or (not is_creature and record['type_line'] != 'Instant'):
        raise ValueError('unreviewed kicker card type')
    stats = [int(record['power']), int(record['toughness'])] if is_creature else None
    return dict(effect=csvid, cast_price=cost_from_oracle(record),
                fee=cost_from_oracle({'mana_cost': fee[1]}), stats=stats)


def variants(shape):
    return [f'{mode}_{seat}' for mode in (*COMMON, *EXTRA.get(shape['effect'], ())) for seat in (0, 1)]


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name') if record else None)
    if not pinned or not wave6_exact(pinned['oracle'], record):
        raise ValueError('exact full pinned source required')
    shape = classify(record)
    declared = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or
            declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text'] or
            not wave6_exact(declaration.get('params'), shape) or not isinstance(declared, list) or not declared or
            len(set(declared)) != len(declared) or set(declared) - set(variants(shape))):
        raise ValueError('exact kicker recipe and distinct reviewed variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete independent kicker witnesses required')
    for group, items in data.items():
        for name, pin in items.items():
            row = rows[name]
            if (row != pin['registry'] or not wave6_exact(oracle[name], pin['oracle']) or row['kind'] != 'card'
                    or row['handler'] in (None, '0', 'NULL') or row['handler'] == 'card_dummy' and
                    (group == 'sources' or name not in {BEAR, 'Hill Giant', 'Island', 'Plains'})):
                raise ValueError('unreviewed Oracle or native record ' + name)
            if group == 'sources' and classify(oracle[name])['effect'] != row['csvid']:
                raise ValueError('unreviewed native kicker source ' + name)


class Board:
    def __init__(self, record, shape, mode, actor, rows, oracle):
        self.source, self.shape, self.mode = record, shape, mode
        self.csvid, self.actor, self.enemy = shape['effect'], actor, 1 - actor
        self.paid = mode not in {'declined', 'declined_foreign', 'base_only', 'wrong_kicker_color'}
        self.countered = mode == 'countered_paid'
        self.target_lost = mode == 'lost_target'
        self.target_seat = self.enemy if mode.endswith('_foreign') else self.actor
        self.chant_target = self.actor if mode == 'replay' else self.enemy
        csvid = self.csvid
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps = [], []
        self.add('source', actor, record['name'], 0)
        if csvid in (BUSHWHACKER, CHANT):
            self.add('friend', actor, BEAR, 0 if csvid == BUSHWHACKER else 1)
        if csvid in (BUSHWHACKER, CHANT, SKIZZIK):
            self.add('guard', self.enemy, BEAR)
        if csvid == CHANT:
            self.add('own_spell', self.actor, MISTS, 0)
            self.add('opponent_spell', self.enemy, MISTS, 0)
        if csvid in (BITE, VINES):
            self.add('target', self.target_seat, BEAR)
        if csvid == VINES:
            self.add('own_probe', self.actor, GROWTH, 0)
            self.add('opponent_probe', self.enemy, GROWTH, 0)
        if self.countered or self.target_lost:
            self.add('reply', self.enemy, COUNTER if self.countered else BOUNCE, 0)
        if mode in {'lost_target', 'other_target'}:
            self.add('bystander', self.enemy, 'Hill Giant')
        for seat in (0, 1):
            self.add(f'island_{seat}', seat, 'Island', 2)
            self.add(f'plains_{seat}', seat, 'Plains', 2)

    def add(self, ref, actor, name, zone=1):
        self.setup.append(card(name, ref, actor, ZONES[zone]))

    def emit(self, op, ref='', other='', **values):
        if ref:
            values['ref'] = ref
        if other:
            values['other'] = other
        self.steps.append(step(op, **values))

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref, property=property_name, value=value)

    def mana(self, op, actor, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=actor, value=value, color=color)

    def zone(self, name, actor, zone, count):
        self.emit('assert_zone_multiplicity', player=actor, zone=ZONES[zone],
                  index=self.rows[name]['csvid'], value=count)

    def stats(self, ref, power, toughness):
        self.observe(ref, 'in_play', 1)
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def resolve_current(self):
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.emit('refresh')

    def cast_witness(self, ref, name, owner):
        self.mana('set_mana', owner, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.resolve_current()
        self.mana('assert_mana', owner, ZERO)

    def cast_source(self):
        base, fee = self.shape['cast_price'], self.shape['fee']
        extra = list(fee)
        if self.mode == 'base_only':
            extra = list(ZERO)
        elif self.mode == 'wrong_kicker_color':
            color = next(index for index, value in enumerate(fee) if index and value)
            extra[color] -= 1
            blue = next(index for index, value in enumerate(cost_from_oracle(self.oracle[MISTS])) if value)
            extra[blue] += 1

        self.emit('turn', player=self.actor)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        if self.csvid == BUSHWHACKER:
            self.cast_witness('friend', BEAR, self.actor)
            self.stats('friend', 2, 2)
        if self.csvid == VINES:
            self.targeting(1, 1)

        self.mana('set_mana', self.actor, [a + b for a, b in zip(base, extra)])
        self.emit('refresh')
        if self.csvid == CHANT:
            self.emit('choose_player', player=self.actor, value=self.chant_target)
        if self.csvid in (BITE, VINES):
            self.emit('choose_target', 'target', player=self.actor)
        if self.mode not in {'base_only', 'wrong_kicker_color'}:
            self.emit('choose_dialog', player=self.actor, value=int(not self.paid))
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_play', 0)
        self.remaining = ZERO if self.paid else extra
        self.mana('assert_mana', self.actor, self.remaining)
        self.mana('assert_mana', self.enemy, ZERO)

    def resolve_source(self):
        if self.countered or self.target_lost:
            self.emit('announce_spell', 'source')
            reply = COUNTER if self.countered else BOUNCE
            self.mana('set_mana', self.enemy, cost_from_oracle(self.oracle[reply]))
            self.emit('refresh')
            if self.target_lost:
                self.emit('choose_target', 'target', player=self.enemy)
            self.emit('cast_response', 'reply', value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=2)
            self.mana('assert_mana', self.enemy, ZERO)
            self.emit('resolve_response')
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=0)
            self.emit('refresh')
            self.zone(reply, self.enemy, 3, 1)
        else:
            self.resolve_current()
        self.mana('assert_mana', self.actor, self.remaining)
        self.mana('assert_mana', self.enemy, ZERO)
        is_permanent = self.csvid in (BUSHWHACKER, SKIZZIK) and not self.countered
        self.zone(self.source['name'], self.actor, 1, int(is_permanent))
        self.zone(self.source['name'], self.actor, 3, int(not is_permanent))

    def riders(self):
        if self.target_lost:
            self.observe('target', 'in_play', 0)
            self.zone(BEAR, self.target_seat, 0, 1)
            self.zone(BEAR, self.target_seat, 1, 0)
            self.stats('bystander', 3, 3)
            self.life()
            return
        actions = {BUSHWHACKER: self.bushwhacker, CHANT: self.chant, SKIZZIK: self.skizzik,
                   BITE: self.bite, VINES: self.vines}
        actions[self.csvid]()

    def run(self):
        self.cast_source()
        if self.mode != 'replay':
            self.resolve_source()
            self.riders()
            return
        self.emit('capture')
        start = len(self.steps)
        self.resolve_source()
        self.riders()
        tail = copy.deepcopy(self.steps[start:])
        self.emit('record_state')
        self.emit('restore')
        self.emit('ai_roundtrip')
        self.steps.extend(tail)
        self.emit('assert_recorded_state')

    def declare_damage(self, attackers, *, blocker=None):
        self.emit('begin_attackers')
        for ref, allowed in attackers:
            self.emit('attack', ref, value=int(allowed))
            self.observe(ref, 'attacking', int(allowed))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', blocker, attackers[0][0], value=1)
        self.emit('advance_combat')
        if blocker:
            self.observe(attackers[0][0], 'blocked', 1)
        self.emit('combat_damage')
        self.resolve_current()
        self.emit('end_combat')
        self.emit('assert_choices_consumed')

    def life(self, actor=20, enemy=20):
        self.emit('assert_life', player=self.actor, value=actor)
        self.emit('assert_life', player=self.enemy, value=enemy)

    def expire(self):
        self.emit('advance_turn')
        self.emit('assert_choices_consumed')

    def bushwhacker(self):
        if self.countered:
            self.stats('friend', 2, 2)
            self.stats('guard', 2, 2)
            self.declare_damage([('friend', False)])
            self.life()
            return
        power, toughness = self.shape['stats']
        self.stats('source', power + self.paid, toughness)
        self.stats('friend', 2 + self.paid, 2)
        self.stats('guard', 2, 2)
        self.declare_damage([('source', self.paid), ('friend', self.paid)])
        self.life(enemy=15 if self.paid else 20)
        self.expire()
        self.stats('source', power, toughness)
        self.stats('friend', 2, 2)
        self.stats('guard', 2, 2)

    def chant(self):
        if self.countered:
            self.cast_witness('opponent_spell', MISTS, self.enemy)
            self.zone(MISTS, self.enemy, 3, 1)
            self.zone('Island', self.enemy, 0, 1)
            self.declare_damage([('friend', True)])
            self.life(enemy=18)
            return
        is_self = self.chant_target == self.actor
        denied_ref = 'own_spell' if is_self else 'opponent_spell'
        allowed_ref = 'opponent_spell' if is_self else 'own_spell'
        other = 1 - self.chant_target
        denied_cost = cost_from_oracle(self.oracle[MISTS])
        self.mana('set_mana', self.chant_target, denied_cost)
        self.emit('refresh')
        self.emit('cast', denied_ref, value=0)
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.chant_target, denied_cost)
        self.zone(MISTS, self.chant_target, 0, 1)
        self.zone(MISTS, self.chant_target, 3, 0)
        self.zone('Island', self.chant_target, 2, 1)
        self.cast_witness(allowed_ref, MISTS, other)
        self.zone(MISTS, other, 3, 1)
        self.zone('Island', other, 0, 1)
        self.declare_damage([('friend', not self.paid)])
        self.life(enemy=20 if self.paid else 18)
        self.expire()
        self.cast_witness(denied_ref, MISTS, self.chant_target)
        self.zone(MISTS, self.chant_target, 3, 1)
        self.zone('Island', self.chant_target, 0, 1)
        self.declare_damage([('guard', True)])
        self.life(actor=18, enemy=20 if self.paid else 18)

    def skizzik(self):
        power, toughness = self.shape['stats']
        if self.countered:
            self.observe('source', 'in_play', 0)
            self.life()
            self.expire()
            self.zone(self.source['name'], self.actor, 3, 1)
            self.stats('guard', 2, 2)
            return
        self.stats('source', power, toughness)
        self.stats('guard', 2, 2)
        self.declare_damage([('source', True)], blocker='guard')
        self.life(enemy=17)
        self.observe('source', 'damage', 2)
        self.zone(BEAR, self.enemy, 3, 1)
        self.expire()
        self.zone(self.source['name'], self.actor, 1, int(self.paid))
        self.zone(self.source['name'], self.actor, 3, int(not self.paid))
        if self.paid:
            self.stats('source', power, toughness)
            self.observe('source', 'damage', 0)

    def bite(self):
        self.stats('target', 2 if self.countered else 5, 2)
        self.declare_damage([('target', True)])
        self.life(actor=25 if self.paid and not self.countered else 20, enemy=18 if self.countered else 15)
        self.expire()
        self.stats('target', 2, 2)
        if self.mode == 'expired_combat':
            self.expire()
            self.emit('untap_step', player=self.actor)
            self.emit('turn', player=self.actor)
            self.declare_damage([('target', True)])
            self.life(actor=25, enemy=13)

    def targeting(self, own, opponent, ref='target'):
        self.emit('can_target_creature', 'own_probe', ref, value=own)
        self.emit('can_target_creature', 'opponent_probe', ref, value=opponent)

    def vines(self):
        expected = 6 if self.paid and not self.countered else 2
        self.stats('target', expected, expected)
        self.targeting(1, int(self.countered))
        if self.mode == 'other_target':
            self.stats('bystander', 3, 3)
            self.targeting(1, 1, 'bystander')
        self.expire()
        self.stats('target', 2, 2)
        self.targeting(1, 1)
        if self.mode == 'other_target':
            self.stats('bystander', 3, 3)
            self.targeting(1, 1, 'bystander')



def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board = Board(oracle[name], shape, mode, int(seat), rows, oracle)
    board.run()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'],
                             untested_components=['Multiplayer and interposition inside synchronous entry callbacks.']))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape)):
        raise ValueError('distinct reviewed kicker variants and exact native source required')
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
