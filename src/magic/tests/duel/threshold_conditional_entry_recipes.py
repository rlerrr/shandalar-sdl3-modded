"""Whole Threshold conditional entries exercised by actual cast and phase actions."""

from collections import Counter
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'threshold_conditional_entry'
PROFILE = Path(__file__).parent / 'data/threshold_conditional_entry_wave8_profiles.json'
HAND, BOARD, LIBRARY, GRAVE, EXILE = range(5)
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = [0] * 6
MODES = ('six', 'seven', 'opponent_only', 'wrong_color', 'crossing', 'replay')
THRESHOLD = 'Threshold — As long as there are seven or more cards in your graveyard, '
RULES = {
    ('Haste', THRESHOLD + 'this creature has "When this creature enters, creatures you control get +1/+1 and gain trample until end of turn."'):
        (5782, 'team', 'Creature', 'Haste'),
    (THRESHOLD + 'this creature has "When this creature enters, draw three cards, then discard two cards."',):
        (5786, 'loot', 'Creature', ''),
    ('Flying', THRESHOLD + 'this creature has "When this creature enters, nonblack creatures get -2/-2 until end of turn."'):
        (5814, 'shrink', 'Creature', 'Flying'),
    (THRESHOLD + 'this creature has "When this creature enters, it deals 3 damage to any target."',):
        (5853, 'damage', 'Creature', ''),
    (THRESHOLD + 'this enchantment has "Whenever an opponent casts a spell, you may put a creature card from your hand onto the battlefield."',):
        (5959, 'opponent_cast', 'Enchantment', ''),
    ('At the beginning of your upkeep, you may put a +1/+1 counter on this creature.',
     THRESHOLD + 'this creature has "At the beginning of your upkeep, you may put another +1/+1 counter on this creature."'):
        (5519, 'upkeep', 'Creature', ''),
}
WITNESSES = frozenset({'Mountain', 'Forest', 'Island', 'Plains', 'Saltfield Recluse', 'Cloudfin Raptor',
                       'Royal Assassin', 'Ember Beast', 'Dazzling Ramparts', 'Jace Beleren',
                       "Traveler's Amulet", 'Bottle Gnomes', 'Disenchant'})


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    tag = RULES.get(tuple(record['oracle_text'].splitlines()))
    if tag is None or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('Complete normal Threshold conditional entry required')
    csvid, effect, kind, keyword = tag
    expected = {'Threshold', keyword} if keyword else {'Threshold'}
    if (record['type_line'].split(' — ', 1)[0] != kind or set(record['keywords']) != expected
            or len(record['keywords']) != len(expected)):
        raise ValueError('Printed Threshold type or keywords disagree')
    if kind == 'Creature' and any(not re.fullmatch(r'[1-9][0-9]*', record[key]) for key in ('power', 'toughness')):
        raise ValueError('Fixed creature stats required')
    return dict(csvid=csvid, effect=effect, printed=cost_from_oracle(record))


def variants(shape):
    modes = (*MODES, 'seven_creature', 'seven_walker') if shape['effect'] == 'damage' else MODES
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in modes]


def spec(declaration, record):
    shape = parameters(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('Exact whole Threshold declaration required')
    if (not isinstance(chosen, list) or not chosen or any(not isinstance(v, str) for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('Distinct Threshold controls required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    if set(pins['witnesses']) != WITNESSES:
        raise ValueError('Independent Threshold witness roster changed')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])):
            raise ValueError(f'Whole Oracle or registry pin changed: {name}')
    return pins


class Duel:
    def __init__(self, card, csvid, owner, mode, rows, oracle):
        self.source, self.csvid, self.actor, self.enemy = card, csvid, owner, 1-owner
        self.mode, self.rows, self.oracle = mode, rows, oracle
        self.threshold = mode.startswith('seven') or mode == 'replay'
        self.initial_grave = {owner: 7 if self.threshold else 6, self.enemy: 7 if mode == 'opponent_only' else 6}
        self.setup, self.steps, self.counts = [], [], Counter()
        self.life = [20, 20]
        self.decks = [('Forest', 'Island'), ('Forest', 'Island')]
        if csvid == 5786:
            self.decks[owner] = ('Plains', 'Plains', 'Plains', 'Forest', 'Island')
        self.add('source', owner, card['name'], HAND)
        for seat in (0, 1):
            for i in range(self.initial_grave[seat]):
                self.add(f'g_{seat}_{i}', seat, 'Mountain', GRAVE)
            for i, name in enumerate(self.decks[seat]):
                self.add(f'd_{seat}_{i}', seat, name, LIBRARY)
        if csvid == 5782:
            self.add('ally', owner, 'Saltfield Recluse', BOARD)
            if self.threshold or mode == 'crossing':
                # Setup performs real entry. Assassin must enter before the
                # Raptor or evolve will make the intended 0/1 blocker a 1/2.
                self.add('guard', self.enemy, 'Royal Assassin', BOARD)
                self.add('blocker', self.enemy, 'Cloudfin Raptor', BOARD)
            else:
                self.add('blocker', self.enemy, 'Cloudfin Raptor', BOARD)
                self.add('guard', self.enemy, 'Royal Assassin', BOARD)
        if csvid == 5814:
            self.add('own_small', owner, 'Saltfield Recluse', BOARD)
            self.add('foe_small', self.enemy, 'Saltfield Recluse', BOARD)
            self.add('own_black', owner, 'Royal Assassin', BOARD)
            self.add('foe_black', self.enemy, 'Royal Assassin', BOARD)
            self.add('large', self.enemy, 'Ember Beast', BOARD)
        if csvid == 5853:
            self.add('creature', self.enemy, 'Dazzling Ramparts', BOARD)
            self.add('walker', self.enemy, 'Jace Beleren', BOARD)
        if csvid == 5959:
            self.add('own_spell', owner, "Traveler's Amulet", HAND)
            self.add('foe_spell', self.enemy, "Traveler's Amulet", HAND)
            self.add('deploy', owner, 'Bottle Gnomes', HAND)
            self.add('noncreature', owner, 'Plains', HAND)
        if mode == 'crossing':
            self.add('disenchant', owner, 'Disenchant', HAND)
            self.add('threshold_artifact', self.enemy, "Traveler's Amulet", BOARD)

    def add(self, ref, owner, name, zone):
        self.setup.append(dict(name=name, ref=ref, player=owner, zone=ZONES[zone]))
        self.counts[owner, zone, name] += 1

    def emit(self, op, ref='', other='', property_name='', *, player=None, value=None, zone=None,
             index=None, color=None):
        action = dict(op=op)
        for field, item in (
                ('ref', ref or None), ('other', other or None),
                ('counter_kind' if op == 'assert_counter' else 'property', property_name or None),
                ('player', player), ('value', value), ('zone', None if zone is None else ZONES[zone]),
                ('index', index), ('color', color)):
            if item is not None:
                action[field] = item
        self.steps.append(action)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, property_name=prop, value=value)

    def mana(self, op, seat, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=seat, color=color, value=value)

    def zone(self, name, seat, zone, amount):
        self.emit('assert_zone_multiplicity', player=seat, zone=zone, value=amount, index=self.rows[name]['csvid'])

    def combat(self, attacker, blocker, *, blocked):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', blocker, attacker, value=int(blocked))
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', int(blocked))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')

    def move(self, owner, name, origin, destination):
        assert self.counts[owner, origin, name] > 0
        self.counts[owner, origin, name] -= 1
        self.counts[owner, destination, name] += 1

    def stats(self, ref, power, toughness):
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def receipts(self, own_pool=ZERO):
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            self.mana('assert_mana', seat, own_pool if seat == self.actor else ZERO)
            for zone in (HAND, BOARD, LIBRARY, GRAVE, EXILE):
                cards = sorted((n, count) for (p, z, n), count in self.counts.items() if p == seat and z == zone)
                self.emit('assert_zone', player=seat, zone=zone, value=sum(count for _, count in cards))
                for name, count in cards:
                    self.zone(name, seat, zone, count)
            for i, name in enumerate(self.decks[seat]):
                self.emit('assert_zone_card', player=seat, zone=LIBRARY, index=i, value=self.rows[name]['csvid'])

    def short_cast(self):
        printed = cost_from_oracle(self.source)
        missing = next(index for index, required in enumerate(printed) if index and required)
        printed[missing] -= 1
        printed[0] += 1
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.actor, printed)
        self.mana('set_mana', self.enemy, ZERO)
        self.emit('refresh')
        self.emit('cast', 'source', value=0)
        self.observe('source', 'in_hand', 1)
        self.observe('source', 'in_play', 0)
        self.receipts(printed)

    def cross_threshold(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.enemy, ZERO)
        self.mana('set_mana', self.actor, cost_from_oracle(self.oracle['Disenchant']))
        self.emit('refresh')
        self.emit('choose_target', 'threshold_artifact', player=self.actor)
        self.emit('cast', 'disenchant', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('refresh')
        self.move(self.actor, 'Disenchant', HAND, GRAVE)
        self.move(self.enemy, "Traveler's Amulet", BOARD, GRAVE)
        self.threshold = True
        self.receipts()

    def pay(self, ref, name, seat):
        self.mana('set_mana', seat, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        self.emit('cast', ref, value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.observe(ref, 'in_play', 0)
        for player in (0, 1):
            self.mana('assert_mana', player, ZERO)

    def play_source(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        self.mana('set_mana', self.enemy, ZERO)
        if self.csvid == 5786 and self.actor == 0:
            self.emit('discard_policy', value=1)
        self.pay('source', self.source['name'], self.actor)
        if self.csvid == 5853 and self.threshold:
            if self.mode == 'seven_walker':
                self.emit('choose_target', 'walker', player=self.actor)
            elif self.mode == 'seven_creature':
                self.emit('choose_target', 'creature', player=self.actor)
            else:
                self.emit('choose_player', player=self.actor, value=self.enemy)
        self.emit('resolve')
        self.emit('refresh')
        self.move(self.actor, self.source['name'], HAND, BOARD)
        self.observe('source', 'in_play', 1)

    def chieftain(self):
        bonus = int(self.threshold)
        self.stats('source', 3 + bonus, 3 + bonus)
        self.stats('ally', 1 + bonus, 2 + bonus)
        self.stats('guard', 1, 1)
        if self.threshold:
            self.stats('blocker', 0, 1)
            self.emit('assert_counter', 'blocker', property_name='plus1', value=0)
        self.receipts()
        # The paid source is fresh, so the same-turn attack also proves haste.
        self.combat('source', 'blocker', blocked=True)
        self.move(self.enemy, 'Cloudfin Raptor', BOARD, GRAVE)
        self.life[self.enemy] -= 3 * bonus
        self.receipts()
        self.emit('cleanup')
        self.emit('refresh')
        self.stats('source', 3, 3)
        self.stats('ally', 1, 2)
        self.stats('guard', 1, 1)
        self.receipts()

    def sage(self):
        if self.threshold:
            for name in self.decks[self.actor][:3]:
                self.move(self.actor, name, LIBRARY, HAND)
            self.decks[self.actor] = self.decks[self.actor][3:]
            for _ in range(2):
                self.move(self.actor, 'Plains', HAND, GRAVE)
        self.stats('source', 2, 3)
        self.receipts()

    def gloomdrifter(self):
        loss = 2 * int(self.threshold)
        if self.threshold:
            for seat in (0, 1):
                self.move(seat, 'Saltfield Recluse', BOARD, GRAVE)
        self.stats('source', 2, 2)
        self.stats('own_black', 1, 1)
        self.stats('foe_black', 1, 1)
        self.stats('large', 3 - loss, 4 - loss)
        self.receipts()
        # Ember Beast has a second eligible ground defender beside it.
        # Royal Assassin is the direct ground-block attempt against Flying.
        self.emit('advance_turn')
        self.emit('advance_turn')
        self.stats('large', 3, 4)
        self.combat('source', 'foe_black', blocked=False)
        self.life[self.enemy] -= 2
        self.receipts()

    def arsonist(self):
        if self.mode == 'seven_walker':
            self.move(self.enemy, 'Jace Beleren', BOARD, GRAVE)
        else:
            self.emit('assert_counter', 'walker', property_name='loyalty', value=3)
        if self.threshold and self.mode not in ('seven_walker', 'seven_creature'):
            self.life[self.enemy] -= 3
        self.observe('creature', 'damage', 3 if self.mode == 'seven_creature' else 0)
        self.stats('source', 3, 3)
        self.receipts()

    def grounds(self):
        # A real own spell cannot trigger the enchantment, even at seven.
        amulet = "Traveler's Amulet"
        self.pay('own_spell', amulet, self.actor)
        self.emit('announce_spell', 'own_spell')
        self.emit('resolve')
        self.move(self.actor, amulet, HAND, BOARD)
        self.observe('deploy', 'in_hand', 1)
        self.receipts()
        self.emit('advance_turn')
        self.pay('foe_spell', amulet, self.enemy)
        if self.threshold:
            if self.actor == 0:
                self.emit('choose_trigger_source', 'source', player=self.actor)
            self.emit('choose_zone_card', player=self.actor, value=self.rows['Bottle Gnomes']['csvid'])
        self.emit('announce_spell', 'foe_spell')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        if self.threshold:
            self.move(self.actor, 'Bottle Gnomes', HAND, BOARD)
        self.observe('deploy', 'in_play', int(self.threshold))
        self.observe('deploy', 'in_hand', int(not self.threshold))
        self.observe('foe_spell', 'in_play', 0)
        self.zone('Bottle Gnomes', self.actor, BOARD, int(self.threshold))
        self.mana('assert_mana', self.actor, ZERO)
        self.emit('resolve')
        self.move(self.enemy, amulet, HAND, BOARD)
        self.receipts()

    def chlorophant(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.enemy)
        self.emit('upkeep_step', player=self.enemy)
        self.emit('assert_counter', 'source', property_name='plus1', value=0)
        self.receipts()
        self.emit('advance_turn')
        self.emit('untap_step', player=self.actor)
        if self.actor == 0:
            self.emit('choose_trigger_source', 'source', player=self.actor)
        self.emit('upkeep_step', player=self.actor)
        count = 1 + int(self.threshold)
        self.emit('assert_counter', 'source', property_name='plus1', value=count)
        self.stats('source', 1 + count, 1 + count)
        self.receipts()

    def assemble(self):
        for seat, number in self.initial_grave.items():
            self.emit('assert_zone', player=seat, zone=GRAVE, value=number)
            self.zone('Mountain', seat, GRAVE, number)
        if self.mode == 'wrong_color':
            self.short_cast()
            return
        if self.mode == 'crossing':
            self.cross_threshold()
        replay = len(self.steps)
        self.play_source()
        effect = {5782: self.chieftain, 5786: self.sage, 5814: self.gloomdrifter, 5853: self.arsonist,
                  5959: self.grounds, 5519: self.chlorophant}[self.csvid]
        effect()
        if self.mode == 'replay':
            first, tail = self.steps[:replay], self.steps[replay:]
            self.steps = [*first, dict(op='capture'), *tail, dict(op='record_state'),
                          dict(op='restore'), dict(op='ai_roundtrip'), *tail, dict(op='assert_recorded_state')]


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    pins = validate_witnesses(oracle, rows)
    if (name not in pins['sources'] or rows[name]['csvid'] != csvid or csvid != shape['csvid']
            or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('Pinned card and distinct Threshold controls required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        duel = Duel(oracle[name], csvid, int(seat), mode, rows, oracle)
        duel.assemble()
        gaps = ['Replacement effects and multiplayer.']
        if csvid == 5519:
            gaps.append('The bridge accepts both optional counters through the engine’s coalesced source prompt.')
        if csvid == 5786:
            gaps.append('Identical drawn cards preserve exact receipts under either legal discard policy.')
        cases.append(dict(name=f'ThresholdConditionalEntryRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2468, setup=duel.setup, steps=duel.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                                       untested_components=gaps)))
    return cases
