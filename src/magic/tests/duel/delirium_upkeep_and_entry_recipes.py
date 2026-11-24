"""Paid entry, four-type thresholds and real upkeep triggers for Delirium."""

from collections import Counter
from pathlib import Path
import json
import re

from recipes import cost_from_oracle
from counter_condition_recurrence_recipes import exact

FAMILY = 'delirium_upkeep_and_entry'
PROFILE = Path(__file__).parent / 'data/delirium_upkeep_and_entry_wave8_profiles.json'
FOUR = ('Mountain', 'Grizzly Bears', 'Giant Growth', "Traveler's Amulet")
THREE = (*FOUR[:3], 'Mountain')
GRAVE, LIBRARY, HAND, BOARD = 3, 2, 0, 1
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
SCRIBE_DECK = ('Forest', 'Plains', 'Mountain', 'Giant Growth', 'Grizzly Bears', "Traveler's Amulet", 'Island')
MODES = ('three_types', 'four_types', 'opponent_only', 'wrong_color', 'crossing', 'replay')
WITNESSES = frozenset({*FOUR, *SCRIBE_DECK, 'Saltfield Recluse', 'Bottle Gnomes', 'Cloudfin Raptor', 'Disenchant'})
STATIC_GRAVE_WITNESS = 'Grizzly Bears'
OPP_UPKEEP = "Delirium — At the beginning of each opponent's upkeep, if there are four or more card types among cards in your graveyard, "
RULES = {
    ('When this creature enters, it deals 1 damage to each opponent.', OPP_UPKEEP + 'this creature deals 1 damage to that player.'):
        (15834, 'damage', ()),
    ('When this creature enters, each opponent mills three cards.', OPP_UPKEEP + 'that player mills three cards.'):
        (15886, 'mill', ('Mill',)),
    ('Flying, trample', 'When this creature enters, mill four cards.',
     'Delirium — At the beginning of your upkeep, you lose 4 life unless there are four or more card types among cards in your graveyard.'):
        (15891, 'self_mill_life', ('Flying', 'Trample', 'Mill')),
    ('When this creature enters, put a +1/+1 counter on target creature.', OPP_UPKEEP + 'put a +1/+1 counter on target creature.'):
        (15907, 'counters', ()),
    ('When this creature enters, target creature an opponent controls gets -1/-1 until end of turn.',
     OPP_UPKEEP + 'target creature that player controls gets -1/-1 until end of turn.'):
        (15993, 'shrink', ()),
    ('Flying', 'When this creature enters, tap target creature an opponent controls.',
     OPP_UPKEEP + 'tap target creature that player controls.'):
        (15994, 'tap', ('Flying',)),
}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    rules = RULES.get(tuple(record['oracle_text'].splitlines()))
    if (rules is None or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record['type_line'].startswith('Creature — ')
            or any(not re.fullmatch(r'[0-9]+', record[key]) for key in ('power', 'toughness'))):
        raise ValueError('Complete normal upkeep Delirium Oracle record required')
    csvid, effect, prior_keywords = rules
    keywords = {'Delirium', *prior_keywords}
    if len(record['keywords']) != len(keywords) or set(record['keywords']) != keywords:
        raise ValueError('Printed Delirium keywords disagree')
    return dict(effect=effect, csvid=csvid, printed=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{actor}' for actor in (0, 1) for mode in MODES]


def spec(declaration, record):
    shape = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('Exact whole Delirium declaration required')
    if (not isinstance(selected, list) or not selected or any(not isinstance(name, str) for name in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('Distinct supported upkeep Delirium variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    if set(pins['witnesses']) != WITNESSES:
        raise ValueError('Printed Delirium witness set changed')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (row.get('kind') != 'card' or not exact(oracle.get(name), pin['oracle'])
                or not exact(row, pin['registry'])
                or (name != STATIC_GRAVE_WITNESS and row.get('handler') in (None, '0', 'NULL', 'card_dummy'))):
            raise ValueError(f'Whole Oracle or registry pin changed for {name}')
    return pins


class Duel:
    def __init__(self, record, csvid, actor, mode, rows, oracle):
        self.source, self.csvid, self.actor, self.enemy = record, csvid, actor, 1 - actor
        self.mode, self.rows, self.oracle = mode, rows, oracle
        self.active = mode in ('four_types', 'replay')
        self.expected_life = [20, 20]
        self.setup, self.steps, self.counts = [], [], Counter()
        self.decks = [('Mountain', 'Plains'), ('Mountain', 'Plains')]
        self.owner_initial_grave = FOUR if self.active and csvid != 15891 else THREE
        self.foe_initial_grave = FOUR if mode == 'opponent_only' else THREE
        if csvid == 15886:
            self.decks[self.enemy] = SCRIBE_DECK
        if csvid == 15891:
            self.decks[self.actor] = (*(FOUR if self.active else THREE), 'Forest', 'Plains')
        self.add('source', actor, record['name'], HAND)
        for player, names in ((actor, self.owner_initial_grave), (self.enemy, self.foe_initial_grave)):
            for i, name in enumerate(names):
                self.add(f'g_{player}_{i}', player, name, GRAVE)
        for player, names in enumerate(self.decks):
            for i, name in enumerate(names):
                self.add(f'd_{player}_{i}', player, name, LIBRARY)
        if csvid in (15907, 15993, 15994):
            self.add('entry_target', actor if csvid == 15907 else self.enemy, 'Saltfield Recluse', BOARD)
            self.add('upkeep_target', self.enemy, 'Bottle Gnomes', BOARD)
        if csvid == 15891:
            self.add('ground', self.enemy, 'Saltfield Recluse', BOARD)
            self.add('flying', self.enemy, 'Cloudfin Raptor', BOARD)
        if mode == 'crossing':
            self.add('amulet', actor, "Traveler's Amulet", BOARD)
            self.add('disenchant', actor, 'Disenchant', HAND)

    def add(self, ref, player, name, zone):
        self.setup.append(dict(name=name, ref=ref, player=player, zone=ZONES[zone]))
        self.counts[player, zone, name] += 1

    def emit(self, op, ref='', other='', property_name='', *, player=None, value=None, zone=None, index=None, color=None):
        action = dict(op=op)
        for field, val in (
                ('ref', ref or None), ('other', other or None),
                ('counter_kind' if op == 'assert_counter' else 'property', property_name or None),
                ('player', player), ('value', value), ('zone', None if zone is None else ZONES[zone]),
                ('index', index), ('color', color)):
            if val is not None:
                action[field] = val
        self.steps.append(action)

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref, property_name=property_name, value=value)

    def mana(self, op, owner, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=owner, color=color, value=value)

    def zone(self, name, owner, zone, amount):
        self.emit('assert_zone_multiplicity', player=owner, zone=zone, index=self.rows[name]['csvid'], value=amount)

    def move(self, player, name, origin, destination):
        assert self.counts[player, origin, name] > 0
        self.counts[player, origin, name] -= 1
        self.counts[player, destination, name] += 1

    def mill(self, player, count):
        for card in self.decks[player][:count]:
            self.move(player, card, LIBRARY, GRAVE)
        self.decks[player] = self.decks[player][count:]

    def choices_consumed(self):
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)

    def state(self, own_pool=None):
        own_pool = [0] * 6 if own_pool is None else own_pool
        self.choices_consumed()
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.expected_life[player])
            self.mana('assert_mana', player, own_pool if player == self.actor else [0] * 6)
            for zone in (HAND, BOARD, LIBRARY, GRAVE, 4):
                entries = sorted((name, amount) for (seat, place, name), amount in self.counts.items()
                                 if seat == player and place == zone)
                self.emit('assert_zone', player=player, zone=zone, value=sum(n for _, n in entries))
                for name, n in entries:
                    self.zone(name, player, zone, n)
            for index, name in enumerate(self.decks[player]):
                self.emit('assert_zone_card', player=player, zone=LIBRARY, index=index,
                          value=self.rows[name]['csvid'])

    def target_stats(self, entry, upkeep, *, after_foe_untap=False):
        if self.csvid == 15907:
            for name, base, extra in (('entry_target', (1, 2), entry), ('upkeep_target', (1, 3), upkeep)):
                self.emit('assert_counter', name, property_name='plus1', value=extra)
                for prop, start in zip(('power', 'toughness'), base):
                    self.observe(name, prop, start + extra)
        if self.csvid == 15993:
            for name, base, shrink in (('entry_target', (1, 2), entry), ('upkeep_target', (1, 3), upkeep)):
                for prop, start in zip(('power', 'toughness'), base):
                    self.observe(name, prop, start - shrink)
        if self.csvid == 15994:
            self.observe('entry_target', 'tapped', 0 if after_foe_untap else entry)
            self.observe('upkeep_target', 'tapped', upkeep)

    def cast_source(self):
        printed = cost_from_oracle(self.source)
        short = self.mode == 'wrong_color'
        if short:
            missing, = [index for index, cost in enumerate(printed) if index and cost]
            printed[missing] -= 1
            printed[0] += 1
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        self.mana('set_mana', self.actor, printed)
        self.mana('set_mana', self.enemy, [0] * 6)
        self.emit('refresh')
        self.emit('cast', 'source', value=int(not short))
        if short:
            self.observe('source', 'in_hand', 1)
            self.observe('source', 'in_play', 0)
            self.state(printed)
            return
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_play', 0)
        self.zone(self.source['name'], self.actor, BOARD, 0)
        if self.csvid in (15907, 15993, 15994):
            self.emit('choose_target', 'entry_target', player=self.actor)
        self.emit('resolve')
        self.emit('refresh')
        self.move(self.actor, self.source['name'], HAND, BOARD)
        if self.csvid == 15834:
            self.expected_life[self.enemy] -= 1
        if self.csvid == 15886:
            self.mill(self.enemy, 3)
        if self.csvid == 15891:
            self.mill(self.actor, 4)
        self.observe('source', 'in_play', 1)
        for prop in ('power', 'toughness'):
            self.observe('source', prop, int(self.source[prop]))
        self.target_stats(1, 0)
        self.state()

    def real_upkeep(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('refresh')
        matches_opposing = player == self.enemy and self.active
        entry = int(self.csvid == 15907)
        # This real boundary clears the earlier temporary reduction and tap.
        # A permanent Skinner counter still remains on its original recipient.
        current_tap = int(self.csvid == 15994 and player == self.actor and self.active)
        upkeep_counter = int(self.csvid == 15907 and player == self.actor and self.active)
        self.target_stats(entry, current_tap + upkeep_counter, after_foe_untap=True)
        if matches_opposing and self.csvid in (15907, 15993, 15994):
            self.emit('choose_target', 'upkeep_target', player=self.actor)
        self.emit('upkeep_step', player=player)
        self.emit('refresh')
        if matches_opposing and self.csvid == 15834:
            self.expected_life[player] -= 1
        if matches_opposing and self.csvid == 15886:
            self.mill(player, 3)
        if self.csvid == 15891 and player == self.actor and not self.active:
            self.expected_life[self.actor] -= 4
        affected = int(self.active and (player == self.enemy or self.csvid in (15907, 15994)))
        self.target_stats(entry, affected, after_foe_untap=True)
        self.state()

    def combat_rider(self):
        if self.csvid not in (15891, 15994):
            return
        self.emit('turn', player=self.actor)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground' if self.csvid == 15891 else 'entry_target', 'source', value=0)
        if self.csvid == 15891:
            self.emit('block', 'flying', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', int(self.csvid == 15891))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if self.csvid == 15891:
            self.expected_life[self.enemy] -= 3
            self.move(self.enemy, 'Cloudfin Raptor', BOARD, GRAVE)
            self.observe('source', 'damage', 0)
        else:
            self.expected_life[self.enemy] -= 1
        self.state()

    def cross_threshold(self):
        # Both initial upkeeps occurred below threshold. Destroy a real artifact
        # on the controller's next main phase to supply the missing fourth type.
        self.emit('turn', player=self.actor)
        self.observe('amulet', 'in_play', 1)
        self.mana('set_mana', self.actor, cost_from_oracle(self.oracle['Disenchant']))
        self.emit('refresh')
        self.emit('choose_target', 'amulet', player=self.actor)
        self.emit('cast', 'disenchant', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)
        self.emit('resolve')
        self.emit('refresh')
        self.move(self.actor, "Traveler's Amulet", BOARD, GRAVE)
        self.move(self.actor, 'Disenchant', HAND, GRAVE)
        self.active = True
        self.state()

    def assemble(self):
        # Exact grave sizes before initial trigger, independent of the controller's hand and board.
        expected_before = [(self.actor, self.owner_initial_grave), (self.enemy, self.foe_initial_grave)]
        for player, names in expected_before:
            self.emit('assert_zone', player=player, zone=GRAVE, value=len(names))
            for name, count in sorted(Counter(names).items()):
                self.zone(name, player, GRAVE, count)
        self.cast_source()
        if self.mode == 'wrong_color':
            return
        cut = len(self.steps)
        self.real_upkeep(self.enemy)
        self.real_upkeep(self.actor)
        if self.mode == 'crossing':
            self.cross_threshold()
            self.real_upkeep(self.enemy)
            self.real_upkeep(self.actor)
        self.combat_rider()
        if self.mode == 'replay':
            prefix, tail = self.steps[:cut], self.steps[cut:]
            self.steps = [*prefix, dict(op='capture'), *tail, dict(op='record_state'),
                          dict(op='restore'), dict(op='ai_roundtrip'), *tail, dict(op='assert_recorded_state')]


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    pins = validate_witnesses(oracle, rows)
    if (name not in pins['sources'] or csvid != shape['csvid'] or rows[name]['csvid'] != csvid
            or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('Pinned Delirium source and distinct supported cases required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        duel = Duel(oracle[name], csvid, int(actor), mode, rows, oracle)
        duel.assemble()
        cases.append(dict(name=f'DeliriumUpkeepAndEntryRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2406, setup=duel.setup, steps=duel.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                                       untested_components=['Resolution priority during the bounded upkeep driver, graveyard replacement effects, and multiplayer.'])))
    return cases
