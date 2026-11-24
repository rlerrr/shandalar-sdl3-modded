"""Independent Madness payments through an actual tap-and-discard cost."""

from collections import Counter
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact


FAMILY = 'madness_creature_casting'
PROFILE = Path(__file__).with_name('data') / 'madness_creature_casting_wave8_profiles.json'
MADNESS = ' (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.)'
CARDS = {
    5769: ('{2}{G}', ('Trample',), 'trample'),
    5774: ('{0}', ('{1}{G}: This creature gets +2/+2 until end of turn. Activate only once each turn.',), 'pump'),
    8832: ('{2}{R}', ('Trample',), 'trample'),
    15860: ('{2}{R}', ('Haste (This creature can attack and {T} as soon as it comes under your control.)',), 'haste'),
    16237: ('{2}{B}', (), 'plain'),
}
GOBLIN = 'Rummaging Goblin'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
WITNESSES = {GOBLIN, 'Ornithopter', 'Mountain', 'Plains', 'Island', 'Counterspell'}
GAP = ('The discard callback casts the hand instance with an exile marker. The legacy exposes neither true temporary '
       'exile residency nor a separate Madness trigger response window.')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    from recipes import cost_from_oracle

    if (csvid not in CARDS or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces') or not record.get('type_line', '').startswith('Creature — ')):
        raise ValueError('whole normal Madness creature required')
    alternative, riders, role = CARDS[csvid]
    keywords = {'Madness'} | ({role.capitalize()} if role in ('trample', 'haste') else set())
    if (record.get('oracle_text') != '\n'.join((*riders, 'Madness ' + alternative + MADNESS))
            or set(record.get('keywords', ())) != keywords
            or any(not record.get(stat, '').isdigit() or int(record[stat]) < 1 for stat in ('power', 'toughness'))):
        raise ValueError('complete Madness and printed rider clauses required')
    alternate = [0] * 6 if alternative == '{0}' else cost_from_oracle({'mana_cost': alternative})
    return dict(csvid=csvid, role=role, normal=cost_from_oracle(record), madness=alternate)


def variants(shape):
    modes = ('paid', 'declined', 'normal') + (('short',) if any(shape['madness']) else ())
    modes += ('offturn_paid', 'normal_countered', 'replay')
    return [f'{mode}_{seat}' for mode in modes for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Madness branches required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('complete pinned Madness primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned Madness declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Madness witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Madness card or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Madness primary required')
                classify(oracle[name], row['csvid'])
    if oracle[GOBLIN]['oracle_text'] != '{T}, Discard a card: Draw a card.':
        raise ValueError('actual selected-discard cost and draw witness required')


class Duel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        shape = classify(record, csvid)
        self.normal_cost, self.madness_cost = shape['normal'], shape['madness']
        self.role = CARDS[csvid][2]
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.library = {actor: ['Mountain', 'Plains', 'Island'], self.foe: ['Plains', 'Island']}
        self.add('source', actor, record['name'], 'hand')
        if mode == 'normal_countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')
        elif mode != 'normal':
            self.add('looter', actor, GOBLIN, 'hand')
        for player, names in self.library.items():
            for i, name in enumerate(names):
                self.add(f'd_{player}_{i}', player, name, 'library')
        if self.role == 'trample':
            self.add('blocker', self.foe, 'Ornithopter', 'battlefield')

    def add(self, ref, player, name, zone):
        from recipes import card

        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        from recipes import step

        if op == 'untap_step':
            self.steps.append(step(op, player=player))
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

    def mana(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, color=color, value=value)

    def receipts(self, stack=0):
        self.emit('assert_stack', value=stack)
        names = sorted({c['name'] for c in self.cards.values()})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone, index in ZONE.items():
                contents = self.library[player] if zone == 'library' else [
                    c['name'] for c in self.cards.values() if c['player'] == player and c['zone'] == zone
                ]
                self.emit('assert_zone', player=player, zone=index, value=len(contents))
                occurrences = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=index,
                              index=self.rows[name]['csvid'], value=occurrences[name])
                if zone == 'library':
                    for i, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=index, index=i,
                                  value=self.rows[name]['csvid'])

    def cast_pending(self, ref, cost):
        self.mana('set_mana', self.actor, cost)
        self.emit('refresh')
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)

    def cast_paid(self, ref, cost):
        self.cast_pending(ref, cost)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[ref]['zone'] = 'battlefield'

    def age(self):
        for player in (self.foe, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            self.emit('turn', player=player)

    def selected_discard(self):
        from recipes import cost_from_oracle

        self.cast_paid('looter', cost_from_oracle(self.oracle[GOBLIN]))
        self.age()
        if self.mode == 'offturn_paid':
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.emit('turn', player=self.foe)
        self.observe('looter', 'in_play', 1)
        self.observe('looter', 'tapped', 0)
        self.observe('looter', 'sick', 0)
        self.receipts()
        self.emit('discard_policy', value=1)
        self.emit('present_ai_dialogs', value=1)
        affordable = self.mode != 'short'
        pool = self.madness_cost if affordable else [sum(self.madness_cost), 0, 0, 0, 0, 0]
        self.mana('set_mana', self.actor, pool)
        self.emit('refresh')
        cut = len(self.steps) if self.mode == 'replay' else None
        self.emit('can_activate', 'looter', value=1)
        if self.actor == 1:
            # The native AI selects its sole eligible discard and acknowledges it.
            self.emit('choose_dialog', player=self.actor, value=0)
        paid = self.mode in ('paid', 'offturn_paid', 'replay')
        self.emit('choose_dialog', player=self.actor, value=int(affordable and not paid))
        self.emit('activate', 'looter', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        if self.actor == 0:
            self.emit('assert_discard_choices', value=1)
        self.cards['source']['zone'] = 'battlefield' if paid else 'graveyard'
        self.observe('looter', 'tapped', 1)
        self.mana('assert_mana', self.actor, [0] * 6 if paid else pool)
        self.mana('assert_mana', self.foe, [0] * 6)
        self.receipts(stack=1)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[f'd_{self.actor}_0']['zone'] = 'hand'
        del self.library[self.actor][0]
        self.receipts()
        return cut

    def enter_rider(self):
        self.emit('bind_permanent_copy', 'played', player=self.actor, value=self.csvid)
        self.observe('played', 'in_play', 1)
        self.observe('played', 'power', int(self.record['power']))
        self.observe('played', 'toughness', int(self.record['toughness']))
        if self.mode == 'offturn_paid':
            return
        if self.role in ('trample', 'haste'):
            self.combat()
        elif self.role == 'pump':
            self.pump()

    def combat(self):
        trample = self.role == 'trample'
        if trample:
            self.age()
        self.emit('begin_attackers')
        self.emit('attack', 'played', value=1)
        self.observe('played', 'attacking', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if trample:
            self.emit('block', 'blocker', 'played', value=1)
        self.emit('advance_combat')
        self.observe('played', 'blocked', int(trample))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        damage = int(self.record['power'])
        if trample:
            damage -= int(self.oracle['Ornithopter']['toughness'])
            self.cards['blocker']['zone'] = 'graveyard'
        self.life[self.foe] -= damage
        self.receipts()

    def pump(self):
        two = [2, 0, 0, 2, 0, 0]
        one = [1, 0, 0, 1, 0, 0]
        self.mana('set_mana', self.actor, two)
        self.emit('refresh')
        self.emit('can_activate', 'played', value=1)
        self.emit('activate', 'played', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.observe('played', 'power', 3)
        self.observe('played', 'toughness', 3)
        self.mana('assert_mana', self.actor, one)
        self.emit('can_activate', 'played', value=0)
        self.emit('activate', 'played', value=0)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.actor, one)
        self.emit('advance_turn')
        self.emit('advance_turn')
        self.emit('refresh')
        self.observe('played', 'power', 1)
        self.observe('played', 'toughness', 1)
        self.mana('set_mana', self.actor, one)
        self.emit('refresh')
        self.emit('can_activate', 'played', value=1)
        self.receipts()

    def counter_normal_cast(self):
        from recipes import cost_from_oracle

        self.cast_pending('source', self.normal_cost)
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_stack', value=2)
        self.emit('resolve_response')
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.foe, [0] * 6)
        self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
        self.emit('refresh')
        self.receipts()

    def build(self):
        from recipes import step

        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, [0] * 6)
        if self.mode == 'normal_countered':
            self.counter_normal_cast()
            return
        cut = None
        if self.mode == 'normal':
            self.cast_paid('source', self.normal_cost)
            self.receipts()
        else:
            cut = self.selected_discard()
        if self.mode in ('normal', 'paid', 'offturn_paid', 'replay'):
            self.enter_rider()
        if cut is not None:
            suffix = self.steps[cut:]
            self.steps[cut:] = ([step('capture')] + suffix
                                + [step('record_state'), step('ai_roundtrip'), step('restore')]
                                + deepcopy(suffix) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered Madness primary CSVID required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        game = Duel(csvid, oracle[name], int(actor), mode, rows, oracle)
        game.build()
        cases.append(dict(
            name=f'MadnessCreatureCastingRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2471,
            setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'],
                         untested_components=[GAP] if mode not in ('normal', 'normal_countered') else []),
        ))
    return cases
