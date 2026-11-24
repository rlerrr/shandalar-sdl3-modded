"""Paid Madness with exact upkeep, top-library, and combat receipts."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


FAMILY = 'madness_upkeep_and_library'
PROFILE = Path(__file__).with_name('data') / 'madness_upkeep_and_library_wave8_profiles.json'
GOBLIN = 'Rummaging Goblin'
MADNESS = ' (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.)'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZONES = {number: name for name, number in ZONE.items()}
DESCENT, VISITOR, THOUGHTS, TWINS = 8932, 15743, 15898, 16001
HONDEN = 'Honden of Cleansing Fire'
CARDS = {
    DESCENT: ('{2}{B}{B}', ('At the beginning of each player\'s upkeep, that player loses 1 life and discards a card.',
                            'Hellbent — Skip your upkeep step if you have no cards in hand.'),
              ('Madness', 'Hellbent')),
    VISITOR: ('{1}{B}', ("At the beginning of each player's upkeep, if that player has no cards in hand, "
                        'you draw a card and you lose 1 life.',), ('Madness',)),
    THOUGHTS: ('{1}{U}', ('Look at the top two cards of your library. Put one of them into your hand and the '
                         'other into your graveyard.',), ('Madness',)),
    TWINS: ('{2}{B}', (), ('Madness',)),
}
COMMON = ('paid', 'declined', 'short', 'normal')
EXTRAS = {DESCENT: ('skip_owner',), VISITOR: ('both_full',),
          THOUGHTS: ('paid_first', 'one_card', 'empty_library'), TWINS: ()}
DIRECT = {'normal', 'skip_owner', 'both_full', 'one_card', 'empty_library', 'removed', 'normal_countered'}
PAID = {'paid', 'paid_first', 'offturn_paid'}

FRESH = {DESCENT: 'removed', VISITOR: 'removed', THOUGHTS: 'offturn_paid', TWINS: 'offturn_paid'}
WITNESSES = {GOBLIN, HONDEN, 'Mountain', 'Plains', 'Island', 'Forest', 'Shock', 'Growth Spiral',
             'Giant Growth', 'Hill Giant', 'Disenchant', 'Counterspell'}
GAPS = ('The production Madness discard implementation has no separate exile or Madness-trigger window.',
        'The production upkeep entry point resolves scheduled triggers within a single bounded action.')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid not in CARDS:
        raise ValueError('supported Madness upkeep/library primary required')
    alt, clauses, keywords = CARDS[csvid]
    if (record.get('oracle_text') != '\n'.join((*clauses, f'Madness {alt}' + MADNESS))
            or tuple(record.get('keywords', ())) != keywords or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('exact normal whole Oracle Madness record required')
    if record['type_line'].startswith('Creature — ') != (csvid in (VISITOR, TWINS)):
        raise ValueError('exact supported printed card type required')
    if csvid in (VISITOR, TWINS) and (record.get('power'), record.get('toughness')) != (
            ('3', '1') if csvid == VISITOR else ('3', '5')):
        raise ValueError('exact printed creature stats required')
    return dict(csvid=csvid, normal=cost_from_oracle(record), madness=cost_from_oracle({'mana_cost': alt}))


def variants(shape):
    csvid = shape['csvid']
    modes = (*COMMON, *EXTRAS[csvid], FRESH[csvid], 'normal_countered')
    return [f'{mode}_{owner}' for mode in modes for owner in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Madness upkeep/library variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('exact pinned Madness primary required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed Madness upkeep/library declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != CARDS.keys()):
        raise ValueError('exact Madness upkeep/library primary and witness scope required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('Madness upkeep/library Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Madness primary required')
                classify(oracle[name], row['csvid'])


class Board:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        shape = classify(record, csvid)
        self.normal_cost, self.madness_cost = shape['normal'], shape['madness']
        self.setup, self.steps, self.cards, self.life = [], [], {}, [20, 20]
        self.library = {actor: ['Mountain', 'Plains', 'Island', 'Forest'], self.foe: ['Plains', 'Island']}
        if mode == 'one_card':
            self.library[actor] = ['Mountain']
        elif mode == 'empty_library':
            self.library[actor] = []
        self.upkeep_visits = []
        self.add('source', actor, record['name'], 'hand')
        if mode not in DIRECT:
            self.add('looter', actor, GOBLIN, 'hand')
        for player, names in self.library.items():
            for index, name in enumerate(names):
                self.add(f'd_{player}_{index}', player, name, 'library')
        if csvid == DESCENT:
            self.add('foe_hold', self.foe, 'Shock', 'hand')
            if mode in ('normal', 'skip_owner', 'removed'):
                self.add('upkeep_witness', actor, HONDEN, 'battlefield')
            if mode == 'normal':
                self.add('own_hold', actor, 'Growth Spiral', 'hand')
        elif csvid == VISITOR and mode in ('normal', 'both_full'):
            self.add('foe_hold', self.foe, 'Shock', 'hand')
            if mode == 'both_full':
                self.add('own_hold', actor, 'Giant Growth', 'hand')
        elif csvid == TWINS:
            self.add('blocker', self.foe, 'Hill Giant', 'battlefield')
        if mode == 'normal_countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')
        if mode == 'removed':
            self.add('removal', actor, 'Disenchant' if csvid == DESCENT else 'Shock', 'hand')


    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
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
        if op.startswith('assert_zone'):
            fields['zone'] = ZONES[zone]
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

    def cast_paid(self, ref, cost):
        self.mana('set_mana', self.actor, cost)
        self.emit('refresh')
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[ref]['zone'] = 'battlefield'

    def age(self):
        for player in (self.foe, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            self.emit('turn', player=player)


    def hand(self, player):
        return [ref for ref, item in self.cards.items() if item['player'] == player and item['zone'] == 'hand']

    def next_deck_ref(self, player):
        return next(ref for ref, item in self.cards.items()
                    if item['player'] == player and item['zone'] == 'library')

    def draw(self, player):
        ref = self.next_deck_ref(player)
        self.cards[ref]['zone'] = 'hand'
        assert self.library[player].pop(0) == self.cards[ref]['name']

    def pending_thoughts(self):
        window = self.library[self.actor][:2]
        if not window:
            return
        keep = window[int(self.mode in ('paid', 'offturn_paid'))]
        self.emit('choose_zone_card', player=self.actor, value=self.rows[keep]['csvid'])

    def thoughts_resolution(self):
        window = self.library[self.actor][:2]
        if window:
            keep = int(self.mode in ('paid', 'offturn_paid'))
            top_refs = [ref for ref, item in self.cards.items()
                        if item['player'] == self.actor and item['zone'] == 'library'][:len(window)]
            self.emit('assert_offered_count', value=len(window))
            for i, name in enumerate(window):
                self.emit('assert_offered_card', index=i, value=self.rows[name]['csvid'])
            self.emit('assert_offered_selection_required', value=1)
            for index, ref in enumerate(top_refs):
                self.cards[ref]['zone'] = 'hand' if index == keep else 'graveyard'
            del self.library[self.actor][:len(window)]
        self.cards['source']['zone'] = 'graveyard'
        self.emit('assert_shuffle_count', player=self.actor, value=0)
        self.emit('assert_shuffle_count', player=self.foe, value=0)

    def direct_cast(self):
        if self.csvid != THOUGHTS:
            self.cast_paid('source', self.normal_cost)
            self.receipts()
            return
        self.mana('set_mana', self.actor, self.normal_cost)
        self.emit('refresh')
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)
        self.pending_thoughts()
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.thoughts_resolution()
        self.receipts()

    def selected_discard(self):
        self.cast_paid('looter', cost_from_oracle(self.oracle[GOBLIN]))
        self.age()
        if self.mode == 'offturn_paid':
            self.upkeep(self.foe)
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
        self.emit('can_activate', 'looter', value=1)
        if self.actor == 1:
            self.emit('choose_dialog', player=self.actor, value=0)
        paid = self.mode in PAID
        self.emit('choose_dialog', player=self.actor, value=int(affordable and not paid))
        if paid and self.csvid == THOUGHTS:
            self.pending_thoughts()
        self.emit('activate', 'looter', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        if self.actor == 0:
            self.emit('assert_discard_choices', value=1)
        if paid and self.csvid == THOUGHTS:
            self.thoughts_resolution()
        else:
            self.cards['source']['zone'] = 'battlefield' if paid else 'graveyard'
        self.observe('looter', 'tapped', 1)
        self.mana('assert_mana', self.actor, [0] * 6 if paid else pool)
        self.mana('assert_mana', self.foe, [0] * 6)
        self.receipts(stack=1)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.draw(self.actor)
        self.receipts()
        self.emit('present_ai_dialogs', value=0)

    def upkeep(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('upkeep_step', player=player)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.emit('turn', player=player)
        self.upkeep_visits.append(player)

    def descent(self):
        self.emit('discard_policy', value=1)
        for player in (self.foe, self.actor, self.foe, self.actor):
            holding = self.hand(player)
            assert len(holding) <= 1
            skipped = player == self.actor and not holding
            self.upkeep(player)
            if not skipped:
                self.life[player] -= 1
                if holding:
                    self.cards[holding[0]]['zone'] = 'graveyard'
                if player == self.actor and 'upkeep_witness' in self.cards:
                    self.life[player] += 2
            self.receipts()
        if 'upkeep_witness' in self.cards:
            self.observe('upkeep_witness', 'in_play', 1)

    def visitor(self):
        for player in (self.foe, self.actor, self.foe):
            empty = not self.hand(player)
            self.upkeep(player)
            if empty:
                self.draw(self.actor)
                self.life[self.actor] -= 1
            self.receipts()

    def twins_combat(self):
        if self.mode == 'offturn_paid':
            self.upkeep(self.actor)
        else:
            self.age()
        self.emit('begin_attackers')
        self.emit('attack', 'played', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'played', value=1)
        self.emit('advance_combat')
        self.observe('played', 'blocked', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.observe('played', 'damage', 3)
        self.cards['blocker']['zone'] = 'graveyard'
        self.receipts()



    def counter_normal(self):
        self.mana('set_mana', self.actor, self.normal_cost)
        self.emit('refresh')
        self.emit('cast', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_stack', value=2)
        self.emit('resolve_response')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.mana('assert_mana', self.foe, [0] * 6)
        self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
        self.receipts()
        if self.csvid in (DESCENT, VISITOR):
            for turn in (self.foe, self.actor):
                self.upkeep(turn)
                self.receipts()

    def remove(self):
        name = self.cards['removal']['name']
        self.mana('set_mana', self.actor, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        self.emit('choose_target', 'played', player=self.actor)
        self.emit('cast', 'removal', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards['source']['zone'] = self.cards['removal']['zone'] = 'graveyard'
        self.receipts()
        for turn in (self.foe, self.actor):
            self.upkeep(turn)
            if turn == self.actor and self.csvid == DESCENT:
                self.life[self.actor] += 2
            self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, [0] * 6)
        if self.mode == 'normal_countered':
            self.counter_normal()
            return
        if self.mode in DIRECT:
            self.direct_cast()
        else:
            self.selected_discard()
        if self.cards['source']['zone'] != 'battlefield':
            return
        self.emit('bind_permanent_copy', 'played', player=self.actor, value=self.csvid)
        self.observe('played', 'in_play', 1)
        if self.csvid in (VISITOR, TWINS):
            self.observe('played', 'power', int(self.record['power']))
            self.observe('played', 'toughness', int(self.record['toughness']))
        if self.mode == 'removed':
            self.remove()
            return
        rider = {DESCENT: self.descent, VISITOR: self.visitor, TWINS: self.twins_combat}
        rider[self.csvid]()



def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Madness upkeep/library primary CSVID required')
    cases = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        game = Board(csvid, oracle[name], int(player), mode, rows, oracle)
        game.build()
        cases.append(dict(
            name=f'MadnessUpkeepAndLibraryRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2471,
            setup=game.setup, steps=game.steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=list(GAPS)),
        ))
    return cases
