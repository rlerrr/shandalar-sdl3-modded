"""Paid Suspend with genuine turn progression and exact printed outcomes."""

from collections import Counter
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact


FAMILY = 'suspend_paid_countdown'
PROFILE = Path(__file__).with_name('data') / 'suspend_paid_countdown_wave8_profiles.json'
CARDS = {
    8477: (4, '{U}', 'draw', (), ('Target player draws three cards.',)),
    8529: (5, '{G}', 'ground_haste', (), ()),
    8535: (4, '{1}{U}', 'flying_haste', ('Flying',), ()),
    8599: (3, '{0}', 'lotus', (), ('{T}, Sacrifice this artifact: Add three mana of any one color.',)),
    8648: (1, '{R}', 'damage', ('Rift Bolt deals 3 damage to any target.',), ()),
    8660: (2, '{G}', 'basic_land', ('Search your library for a basic land card, put it onto the battlefield, then shuffle.',), ()),
}
NUMBERS = {2: 'two', 3: 'three', 4: 'four', 5: 'five'}
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}


PULL = 'Pull from Eternity'
WITNESSES = {'Forest', 'Island', 'Mountain', 'Plains', 'Grizzly Bears', 'Suntail Hawk', 'Counterspell', PULL}
GAPS = (
    'The generic legacy runs the initial time-counter clock. Zone and payoff receipts observe the clock through actual upkeeps.',
    'The zero-counter legacy casts automatically. The Oracle decline option has no offered input in this handler.',
    'The free cast resolves in the upkeep callback. The countered variant uses the separately observable normal cast.',
)


def classify(record, csvid):
    from recipes import cost_from_oracle

    if csvid not in CARDS or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces'):
        raise ValueError('whole normal paid Suspend primary required')
    count, cost, effect, before, after = CARDS[csvid]
    only = not record['mana_cost']
    creature = effect in ('ground_haste', 'flying_haste')
    choice = '' if only else 'you may '
    quantity = 'a time counter' if count == 1 else f'{NUMBERS[count]} time counters'
    haste = ' It has haste.' if creature else ''
    clause = (f'Suspend {count}—{cost} (Rather than cast this card from your hand, {choice}pay {cost} '
              f'and exile it with {quantity} on it. At the beginning of your upkeep, remove a time counter. '
              f'When the last is removed, you may cast it without paying its mana cost.{haste})')
    if (record.get('oracle_text') != '\n'.join((*before, clause, *after))
            or 'Suspend' not in record.get('keywords', ())
            or creature != record.get('type_line', '').startswith('Creature — ')
            or only != (effect in ('draw', 'lotus'))):
        raise ValueError('complete Suspend cost, duration and printed payoff required')
    if creature and any(not record.get(stat, '').isdigit() or int(record[stat]) < 1 for stat in ('power', 'toughness')):
        raise ValueError('positive printed creature power and toughness required')
    suspend = [0] * 6 if cost == '{0}' else cost_from_oracle({'mana_cost': cost})
    return dict(csvid=csvid, duration=count, effect=effect, suspend=suspend,
                normal=None if only else cost_from_oracle(record))


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def variants(shape):
    modes = ['parked', 'countdown']
    if any(shape['suspend']):
        modes.append('unfunded')
    if shape['normal'] is not None:
        modes.extend(('normal', 'normal_countered'))
    modes.extend(('replay', 'removed'))
    return [f'{mode}_{seat}' for mode in modes for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Suspend branches required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('complete pinned paid Suspend primary required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole typed Suspend declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete paid Suspend witness set required')
    for group, entries in data.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(pin['oracle'], oracle.get(name)) or not exact(pin['registry'], row)
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact registered Suspend primary or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Suspend primary required')
                classify(oracle[name], row['csvid'])


class Duel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.duration, _, self.effect, *_ = CARDS[csvid]
        shape = classify(record, csvid)
        self.payment, self.normal_payment = shape['suspend'], shape['normal']
        self.only = not record['mana_cost']
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.deck = {
            actor: ['Forest', 'Grizzly Bears', 'Suntail Hawk'] if self.effect == 'basic_land' else
                   ['Island', 'Mountain', 'Grizzly Bears', 'Plains'],
            self.foe: ['Island', 'Plains'],
        }
        self.add('source', actor, record['name'], 'hand')
        for seat, deck in self.deck.items():
            for pos, name in enumerate(deck):
                self.add(f'deck_{seat}_{pos}', seat, name, 'library')
        if self.effect in ('damage', 'flying_haste'):
            self.add('victim', self.foe, 'Grizzly Bears', 'battlefield')
        if mode == 'normal_countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')
        if mode == 'removed':
            self.add('pull', self.foe, PULL, 'hand')
        self.selected_color = 2 if actor == 0 else 4

    def add(self, ref, player, name, zone):
        from recipes import card

        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        from recipes import step

        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'assert_shuffle_count':
            self.steps.append(step(op, player=player, value=value))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op in ('assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'):
            fields['zone'] = next(name for name, number in ZONE.items() if number == zone)
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, amounts):
        for color, amount in enumerate(amounts):
            self.emit(op, player=player, color=color, value=amount)

    def receipts(self, *, ordered_library=True):
        self.emit('assert_stack')
        names = sorted({card['name'] for card in self.cards.values()})
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            for zone in ZONE:
                contents = self.deck[seat] if zone == 'library' else [
                    card['name'] for card in self.cards.values() if card['player'] == seat and card['zone'] == zone
                ]
                self.emit('assert_zone', player=seat, zone=ZONE[zone], value=len(contents))
                counts = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=seat, zone=ZONE[zone], index=self.rows[name]['csvid'],
                              value=counts[name])
                if zone == 'library' and (ordered_library or seat == self.foe):
                    for index, name in enumerate(contents):
                        self.emit('assert_zone_card', player=seat, zone=ZONE[zone], index=index,
                                  value=self.rows[name]['csvid'])

    def suspend_from_hand(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, [0] * 6)
        parked = self.mode != 'unfunded'
        self.mana('set_mana', self.actor, self.payment if parked else [0] * 6)
        self.emit('refresh')
        if parked and not self.only and self.actor == 1:
            # The native AI sees the "Suspend" row even with only one affordable option.
            self.emit('choose_dialog', player=self.actor, value=1)
        self.emit('cast', 'source', value=0)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.actor, [0] * 6)
        self.mana('assert_mana', self.foe, [0] * 6)
        if parked:
            self.cards['source']['zone'] = 'exile'
        self.receipts()

    def plan_last(self):
        if self.effect == 'draw':
            self.emit('choose_player', player=self.actor, value=self.actor)
        elif self.effect == 'damage':
            self.emit('choose_target', 'victim', player=self.actor)
        elif self.effect == 'basic_land':
            self.emit('choose_zone_card', player=self.actor, value=self.rows['Forest']['csvid'])

    def upkeep(self, player, *, last=False):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        if last:
            # The legal acceptance route casts through the actual Suspend legacy.
            self.plan_last()
        self.emit('upkeep_step', player=player)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        for seat in (0, 1):
            self.mana('assert_mana', seat, [0] * 6)
        if not last:
            self.receipts()

    def accept_cast_outcome(self):
        permanent = self.effect in ('ground_haste', 'flying_haste', 'lotus')
        self.cards['source']['zone'] = 'battlefield' if permanent else 'graveyard'
        if self.effect == 'draw':
            for pos in range(3):
                self.cards[f'deck_{self.actor}_{pos}']['zone'] = 'hand'
            del self.deck[self.actor][:3]
        elif self.effect == 'damage':
            self.cards['victim']['zone'] = 'graveyard'
        elif self.effect == 'basic_land':
            self.cards[f'deck_{self.actor}_0']['zone'] = 'battlefield'
            self.deck[self.actor].remove('Forest')
            self.emit('assert_shuffle_count', player=self.actor, value=1)
        self.receipts(ordered_library=self.effect != 'basic_land')
        if permanent:
            self.emit('bind_permanent_copy', 'played', player=self.actor, value=self.csvid)
            self.observe('played', 'in_play', 1)
        if self.effect == 'basic_land':
            self.emit('bind_permanent_copy', 'fetched', player=self.actor, value=self.rows['Forest']['csvid'])
            self.observe('fetched', 'tapped', 0)

    def haste_attack(self):
        self.emit('turn', player=self.actor)
        self.observe('played', 'power', int(self.record['power']))
        self.observe('played', 'toughness', int(self.record['toughness']))
        self.emit('begin_attackers')
        self.emit('attack', 'played', value=1)
        self.observe('played', 'attacking', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if self.effect == 'flying_haste':
            self.emit('block', 'victim', 'played', value=0)
        self.emit('advance_combat')
        self.observe('played', 'blocked', 0)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.life[self.foe] -= int(self.record['power'])
        self.receipts()

    def lotus_mana(self):
        self.emit('turn', player=self.actor)
        self.emit('can_activate', 'played', value=1)
        self.emit('choose_color', player=self.actor, value=self.selected_color)
        if self.actor == 1:
            self.emit('choose_dialog', player=1, value=0)
        self.emit('activate', 'played', value=1)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        mana = [0] * 6
        mana[self.selected_color] = 3
        self.mana('assert_mana', self.actor, mana)
        self.mana('assert_mana', self.foe, [0] * 6)
        self.cards['source']['zone'] = 'graveyard'
        self.receipts()

    def normal_cast(self):
        from recipes import cost_from_oracle

        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, [0] * 6)
        self.mana('set_mana', self.actor, self.normal_payment)
        self.emit('refresh')
        # Printed funding affords both paths. Select the actual Play Normally row.
        self.emit('choose_dialog', player=self.actor, value=0)
        if self.effect == 'damage':
            self.plan_last()
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)
        if self.mode == 'normal_countered':
            self.emit('announce_spell', 'source')
            self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
            self.emit('refresh')
            self.emit('cast_response', 'counter', value=1)
            self.emit('assert_stack', value=2)
            self.emit('resolve_response')
            self.emit('assert_choices_consumed')
            self.mana('assert_mana', self.foe, [0] * 6)
            self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
            self.receipts()
            return
        if self.effect == 'basic_land':
            self.plan_last()
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.accept_cast_outcome()
        if self.effect in ('ground_haste', 'flying_haste'):
            self.observe('played', 'sick', 1)
            self.emit('begin_attackers')
            self.emit('attack', 'played', value=0)
            self.observe('played', 'attacking', 0)
            self.observe('played', 'tapped', 0)

    def remove_from_exile(self):
        from recipes import cost_from_oracle

        self.upkeep(self.foe)
        self.emit('turn', player=self.foe)
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle[PULL]))
        self.emit('refresh')
        self.emit('choose_player', player=self.foe, value=self.actor)
        self.emit('cast', 'pull', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.foe, [0] * 6)
        self.emit('choose_zone_card', player=self.foe, value=self.csvid)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards['pull']['zone'] = self.cards['source']['zone'] = 'graveyard'
        self.receipts()
        for owner_upkeep in range(self.duration):
            if owner_upkeep:
                self.upkeep(self.foe)
            self.upkeep(self.actor)

    def build(self):
        from recipes import step

        if self.mode in ('normal', 'normal_countered'):
            self.normal_cast()
            return
        self.suspend_from_hand()
        if self.mode == 'unfunded':
            return
        if self.mode == 'parked':
            self.upkeep(self.foe)
            return
        if self.mode == 'removed':
            self.remove_from_exile()
            return
        cut = None
        for owner_upkeep in range(1, self.duration + 1):
            self.upkeep(self.foe)
            if self.mode == 'replay' and self.duration == 1:
                cut = len(self.steps)
            self.upkeep(self.actor, last=owner_upkeep == self.duration)
            if self.mode == 'replay' and self.duration > 1 and owner_upkeep == 1:
                cut = len(self.steps)
        self.accept_cast_outcome()
        if self.effect in ('ground_haste', 'flying_haste'):
            self.haste_attack()
        elif self.effect == 'lotus':
            self.lotus_mana()
        if cut is not None:
            suffix = self.steps[cut:]
            self.steps[cut:] = ([step('capture')] + suffix
                                + [step('record_state'), step('ai_roundtrip'), step('restore')]
                                + deepcopy(suffix) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered Suspend primary CSVID required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        game = Duel(csvid, oracle[name], int(actor), mode, rows, oracle)
        game.build()
        cases.append(dict(
            name=f'SuspendPaidCountdownRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2406,
            setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'],
                         untested_components=list(GAPS) if mode != 'normal' and mode != 'normal_countered' else []),
        ))
    return cases
