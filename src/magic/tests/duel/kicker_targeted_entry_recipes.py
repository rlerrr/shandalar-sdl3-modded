"""Paid targeted kicker entry effects and the exact older native controls."""

from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path
import re


FAMILY = 'kicker_targeted_entry'
PROFILE = Path(__file__).with_name('data') / 'kicker_targeted_entry_wave8_profiles.json'
KICKER = re.compile(r'Kicker ((?:\{(?:\d+|[BUGRW])\})+) \(You may pay an additional \1 as you cast this spell\.\)')
TRIGGER = 'When this creature enters, if it was kicked, destroy target '
CLAUSES = {
    'nonbasic_land': ('nonbasic land.', ('Haste',), ('Haste',)),
    'creature': ('creature.', ('Flying',), ('Flying',)),
    'artifact_enchantment': ('artifact or enchantment.', (), ()),
    'noncreature': ('noncreature permanent.', (), ()),
    'flying_creature': ('creature with flying.', ('Reach (This creature can block creatures with flying.)',), ('Reach',)),
    'artifact_bury': ("artifact. It can't be regenerated.", (), ()),
}
TARGETS = {
    'nonbasic_land': (('Taiga', 'Taiga'), 'Forest'),
    'creature': (('Hill Giant', 'Hill Giant'), 'Grizzly Bears'),
    'artifact_enchantment': (("Traveler's Amulet", 'Sanguine Bond'), 'Grizzly Bears'),
    'noncreature': (("Traveler's Amulet", "Traveler's Amulet"), 'Memnite'),
    'flying_creature': (('Suntail Hawk', 'Suntail Hawk'), 'Grizzly Bears'),
    'artifact_bury': (('Clay Statue', 'Clay Statue'), 'Sanguine Bond'),
}
PASSIVE_WITNESSES = {'Grizzly Bears', 'Hill Giant', 'Memnite', 'Suntail Hawk', 'Wind Drake'}
WITNESSES = ({name for (targets, guard) in TARGETS.values() for name in (*targets, guard)}
             | {'Wind Drake', 'Island', 'Plains', 'Counterspell'})
MODES = ('paid', 'declined', 'base_only', 'wrong_color', 'countered', 'scope', 'replay')
PAY_KICKER = {'paid', 'countered', 'scope', 'replay'}
DISPLAY_KICKER = PAY_KICKER | {'declined'}
GAP = ('The legacy enter callback selects its target and applies the destroy instruction in one pass. '
       'The driver cannot place a response between those actions.')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    if (record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ')
            or any(not re.fullmatch(r'[1-9][0-9]*', record.get(key, '')) for key in ('power', 'toughness'))):
        raise ValueError('whole normal targeted kicker creature required')
    lines = record['oracle_text'].splitlines()
    kicker = KICKER.fullmatch(lines[0]) if lines else None
    kind = next((key for key, (clause, passive, keywords) in CLAUSES.items()
                 if lines[1:] == [*passive, TRIGGER + clause]
                 and set(record.get('keywords', ())) == {'Kicker', *keywords}), None)
    if kicker is None or kind is None:
        raise ValueError('unsupported targeted kicker and complete printed clauses required')
    return dict(kind=kind, mana=cost_from_oracle(record), kicker=cost_from_oracle({'mana_cost': kicker[1]}))


def variants(shape):
    return [f'{mode}_{actor}' for mode in MODES for actor in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected
            or any(type(value) is not str for value in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed targeted kicker variants required')


def spec(declaration, record):
    from counter_condition_recurrence_recipes import exact

    shape = classify(record)
    source = profiles()['sources'].get(record['name'])
    if (not source or not exact(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete exact implementation pin and independently priced kicker required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    from counter_condition_recurrence_recipes import exact

    data = profiles()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete independent target, guard, response and combat pins required')
    for group, records in data.items():
        for name, pin in records.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or row.get('handler') == 'card_dummy' and name not in PASSIVE_WITNESSES
                    or oracle[name]['layout'] != 'normal'):
                raise ValueError('exact registered kicker source or witness changed: ' + name)
            if group == 'sources':
                classify(oracle[name])


class KickerCase:
    def __init__(self, record, shape, actor, mode, rows, oracle):
        self.source, self.shape, self.kind = record, shape, shape['kind']
        self.actor, self.enemy, self.mode = actor, 1 - actor, mode
        self.oracle, self.rows = oracle, rows
        self.setup, self.steps = [], []
        self.setup_board()

    def setup_board(self):
        targets, guard = TARGETS[self.kind]
        self.target_name = targets[1 - self.actor] if self.mode == 'scope' else targets[self.actor]
        self.target_ref = 'target'
        if self.mode == 'scope':
            if self.kind == 'creature':
                self.target_name, self.target_ref = self.source['name'], 'source'
            elif self.kind != 'artifact_enchantment':
                self.target_name, self.target_ref = None, None
        self.guard = guard
        self.add('source', self.actor, self.source['name'], 'hand')
        if self.target_ref == 'target':
            self.add('target', self.enemy, self.target_name)
        self.add('guard', self.enemy, self.guard)
        if self.kind == 'flying_creature' and self.mode != 'scope':
            self.add('flyer', self.enemy, 'Wind Drake')
        if self.mode == 'countered':
            self.add('counter', self.enemy, 'Counterspell', 'hand')
        for seat in (0, 1):
            self.add(f'deck_{seat}_0', seat, 'Island', 'library')
            self.add(f'deck_{seat}_1', seat, 'Plains', 'library')

    def add(self, ref, actor, name, zone='battlefield'):
        from recipes import card

        self.setup.append(card(name, ref, actor, zone))

    def emit(self, op, ref='', other='', property_name='', **fields):
        from recipes import step

        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if property_name:
            fields['property'] = property_name
        self.steps.append(step(op, **fields))

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref, property_name=property_name, value=value)

    def mana(self, op, actor, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=actor, value=value, color=color)

    def zone(self, name, actor, zone, count):
        self.emit('assert_zone_multiplicity', player=actor, zone=zone, index=self.rows[name]['csvid'], value=count)

    def prepare_and_cast(self):
        base, extra = self.shape['mana'], self.shape['kicker']
        funded = [initial + kicker for initial, kicker in zip(base, extra)]
        if self.mode == 'base_only':
            funded = base
        allowed = self.mode != 'wrong_color'
        if not allowed:
            funded = [sum(funded), 0, 0, 0, 0, 0]
        self.actor_left = (extra if self.mode == 'declined' else [0] * 6) if allowed else funded
        self.defender_left = [2, 0, 0, 0, 0, 0] if self.kind == 'artifact_bury' and self.mode != 'countered' else [0] * 6

        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.actor, funded)
        self.mana('set_mana', self.enemy, self.defender_left)
        self.emit('refresh')
        if self.target_ref == 'target':
            self.observe('target', 'in_play', 1)
        self.observe('guard', 'in_play', 1)
        if self.mode in DISPLAY_KICKER:
            self.emit('choose_dialog', player=self.actor, value=int(self.mode == 'declined'))
        self.emit('cast', 'source', value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed))
        self.observe('source', 'in_play', 0)
        if self.target_ref == 'target':
            self.zone(self.target_name, self.enemy, 'battlefield', 1)
            self.zone(self.target_name, self.enemy, 'graveyard', 0)
        self.mana('assert_mana', self.actor, self.actor_left)
        self.mana('assert_mana', self.enemy, self.defender_left)
        if not allowed:
            self.emit('refresh')
            self.observe('source', 'in_hand', 1)
            self.zone(self.source['name'], self.actor, 'hand', 1)
            self.zone(self.source['name'], self.actor, 'battlefield', 0)
            self.zone(self.source['name'], self.actor, 'graveyard', 0)
            self.emit('assert_life', player=self.actor, value=20)
            self.emit('assert_life', player=self.enemy, value=20)
        return allowed

    def choose_entry(self):
        if self.kind == 'artifact_enchantment' and self.actor == 0:
            self.emit('choose_trigger', 'source', player=self.actor)
        self.emit('choose_target', self.target_ref, player=self.actor)
        if self.kind == 'artifact_bury':
            self.emit('choose_regeneration', 'target')

    def enter(self):
        destroyed = self.mode in PAY_KICKER and self.target_ref is not None
        if destroyed:
            self.choose_entry()
        self.emit('resolve')
        if destroyed and self.kind == 'artifact_bury':
            self.emit('assert_unused_choices', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.emit('refresh')
        dies = destroyed and self.target_ref == 'source'
        if not dies:
            self.observe('source', 'in_play', 1)
            self.observe('source', 'power', int(self.source['power']))
            self.observe('source', 'toughness', int(self.source['toughness']))
        self.observe('guard', 'in_play', 1)
        self.zone(self.source['name'], self.actor, 'hand', 0)
        self.zone(self.source['name'], self.actor, 'battlefield', int(not dies))
        if dies:
            self.zone(self.source['name'], self.actor, 'graveyard', 1)
        if self.target_ref == 'target':
            self.zone(self.target_name, self.enemy, 'battlefield', int(not destroyed))
            self.zone(self.target_name, self.enemy, 'graveyard', int(destroyed))
        self.zone(self.guard, self.enemy, 'battlefield', 1)
        self.zone(self.guard, self.enemy, 'graveyard', 0)
        self.emit('assert_life', player=self.actor, value=20)
        self.emit('assert_life', player=self.enemy, value=20)

    def interrupted(self):
        from recipes import cost_from_oracle

        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.enemy, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_stack', value=2)
        self.emit('resolve_response')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.emit('refresh')
        self.zone(self.source['name'], self.actor, 'hand', 0)
        self.zone(self.source['name'], self.actor, 'battlefield', 0)
        self.zone(self.source['name'], self.actor, 'graveyard', 1)
        self.zone('Counterspell', self.enemy, 'graveyard', 1)
        self.zone(self.target_name, self.enemy, 'battlefield', 1)
        self.zone(self.target_name, self.enemy, 'graveyard', 0)
        self.observe('target', 'in_play', 1)
        self.observe('guard', 'in_play', 1)
        self.emit('assert_life', player=self.actor, value=20)
        self.emit('assert_life', player=self.enemy, value=20)

    def combat(self, attacker, blocker=None, *, blocked=False):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker is not None:
            self.emit('block', blocker, attacker, value=int(blocked))
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', int(blocked))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')

    def riders(self):
        if self.kind == 'nonbasic_land':
            self.combat('source')
            self.emit('assert_life', player=self.enemy, value=18)
        elif self.kind == 'creature':
            for _ in range(2):
                self.emit('advance_turn')
            self.combat('source', 'guard')
            self.emit('assert_life', player=self.enemy, value=18)
            self.observe('guard', 'damage', 0)
        elif self.kind == 'flying_creature':
            self.emit('advance_turn')
            self.combat('flyer', 'source', blocked=True)
            self.emit('assert_life', player=self.actor, value=20)
            self.observe('source', 'damage', 2)
            self.observe('flyer', 'damage', 1)

    def play(self):
        from recipes import step

        if not self.prepare_and_cast():
            return
        if self.mode == 'countered':
            self.interrupted()
        else:
            cut = len(self.steps)
            self.enter()
        self.mana('assert_mana', self.actor, self.actor_left)
        self.mana('assert_mana', self.enemy, self.defender_left)
        if self.mode == 'countered':
            return
        if self.mode != 'scope':
            self.riders()
        if self.mode == 'replay':
            suffix = self.steps[cut:]
            self.steps[cut:] = ([step('capture')] + suffix
                                + [step('record_state'), step('ai_roundtrip'), step('restore')]
                                + deepcopy(suffix) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact implemented kicker source CSVID required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        case = KickerCase(oracle[name], shape, int(actor), mode, rows, oracle)
        case.play()
        cases.append(dict(
            name=f'KickerTargetedEntryRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
            seed=2298, setup=case.setup, steps=case.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=[GAP]),
        ))
    return cases
