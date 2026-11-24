"""Convoke payment and distinct entry effects through real casts."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from fight_entry_fighters_recipes import Board
from recipes import cost_from_oracle


FAMILY = 'convoke_entry_effects'
PROFILE = Path(__file__).with_name('data') / 'convoke_entry_effects_wave8_profiles.json'
PREFIX = 'ConvokeEntryEffectsRecipe_'

PHALANX, TOTEM, TRIBUNAL, LOXODON = 7802, 14130, 19318, 19518
REMINDER = "Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for {1} or one mana of that creature's color.)"
PRINTED = {
    PHALANX: ('ravnica.c', '{4}{W}', 'Creature — Human Soldier', (2, 4),
              'When this creature enters, you gain 1 life for each creature you control.'),
    TOTEM: ('m15.c', '{3}{G}', 'Creature — Plant Elemental', (2, 3),
            'When this creature enters, you may put a +1/+1 counter on another target creature.'),
    TRIBUNAL: ('guilds_of_ravnica.c', '{3}{W}', 'Enchantment', None,
               'When this enchantment enters, exile target nonland permanent an opponent controls until this enchantment leaves the battlefield.'),
    LOXODON: ('guilds_of_ravnica.c', '{4}{W}', 'Creature — Elephant Cleric', (4, 4),
              'When this creature enters, put a +1/+1 counter on each creature that convoked it.'),
}
MODES = {
    PHALANX: ('alone', 'mixed', 'countered'),
    TOTEM: ('mixed', 'foe_target', 'decline', 'alone'),
    TRIBUNAL: ('mixed', 'artifact_return', 'only_land'),
    LOXODON: ('full', 'mixed', 'normal', 'countered'),
}
WHITE, GREEN = 'Mother of Runes', 'River Boa'
RED, BLUE = 'Orcish Artillery', 'Prodigal Sorcerer'
WALL, BIRD = 'Dazzling Ramparts', 'Thieving Magpie'
ARTIFACT, GEAR, REMOVAL, COUNTER = 'Millstone', 'Bonesplitter', 'Naturalize', 'Counterspell'
CREATURES = {WHITE: ('W', 1, 1), GREEN: ('G', 2, 1), RED: ('R', 1, 3),
             BLUE: ('U', 1, 1), WALL: ('W', 0, 7), BIRD: ('U', 1, 3)}
WITNESSES = set(CREATURES) | {ARTIFACT, GEAR, REMOVAL, COUNTER, 'Mountain', 'Plains', 'Island'}
GAPS = {LOXODON: ['Convoke cases use at least 18 actual active card slots because the production payment callback '
                  'copies 18 entries from an active-card-sized local array. Small boards are outside this scope.']}
TAPPED, SICK = 'tapped', 'sick'
ZERO = [0] * 6


def stats(record):
    return tuple(int(record[key]) for key in ('power', 'toughness'))


class Duel(Board):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.library = {actor: ['Mountain', 'Plains', 'Island'], self.foe: ['Plains', 'Island']}
        self.initial_states, self.counters = {}, {}
        self.target = None
        self.add('source', actor, record['name'], 'hand')
        self.printed_cost = cost_from_oracle(record)
        self.pool = self.printed_cost.copy()
        self.sponsors = []
        self.choose_sponsors()
        self.add('friendly_artifact', actor, ARTIFACT, 'battlefield')
        for seat, names in self.library.items():
            for index, name in enumerate(names):
                self.add(f'library_{seat}_{index}', seat, name, 'library')
        if csvid == LOXODON:
            self.add_loxodon_witnesses()
        if csvid == TRIBUNAL:
            self.add_tribunal_targets()
        elif csvid != TOTEM or mode != 'alone':
            self.add('opponent_creature', self.foe, WALL, 'battlefield')
        if mode == 'countered':
            self.add('counter', self.foe, COUNTER, 'hand')

    def choose_sponsors(self):
        if self.mode in ('normal', 'alone'):
            if self.mode == 'normal':
                self.add_stateful('spent', self.actor, WHITE, TAPPED)
            return
        colored = GREEN if self.csvid == TOTEM else WHITE
        if self.mode == 'full':
            payer_names = (colored, RED, BLUE, WALL, BIRD)
            self.pool = ZERO.copy()
        else:
            payer_names = (colored, RED)
            self.pool = [self.printed_cost[0] - 1, 0, 0, 0, 0, 0]
        for index, name in enumerate(payer_names):
            ref = f'payer_{index}'
            self.add_stateful(ref, self.actor, name, SICK)
            self.sponsors.append(ref)
        if self.csvid in (PHALANX, LOXODON):
            self.add_stateful('spent', self.actor, BLUE, TAPPED)

    def add_stateful(self, ref, seat, name, state):
        self.add(ref, seat, name, 'battlefield')
        self.setup[-1]['state'] = [state]
        self.initial_states[ref] = state

    def add_loxodon_witnesses(self):
        if self.mode == 'full':
            self.add_stateful('spare', self.actor, BIRD, SICK)
        # The existing engine copies 18 entries from an active-card-sized
        # array. Ordinary inert permanents keep that read within the array.
        active = sum(row['player'] == self.actor and row['zone'] in ('hand', 'battlefield') for row in self.setup)
        for index in range(18 - active):
            self.add(f'board_artifact_{index}', self.actor, ARTIFACT, 'battlefield')

    def add_tribunal_targets(self):
        if self.mode == 'artifact_return':
            self.target = 'victim'
            self.add(self.target, self.foe, GEAR, 'battlefield')
            self.add('removal', self.foe, REMOVAL, 'hand')
            return
        self.add_stateful('opponent_land', self.foe, 'Plains', TAPPED)
        if self.mode == 'mixed':
            self.target = 'victim'
            self.add(self.target, self.foe, WALL, 'battlefield')

    def creature_receipts(self, *, pending=False):
        for ref, card in self.cards.items():
            entry = self.oracle[card['name']]
            if card['zone'] != 'battlefield' or not entry['type_line'].startswith('Creature'):
                continue
            counters = 0 if pending else self.counters.get(ref, 0)
            power, toughness = stats(entry)
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'plus1_counters', counters)
            self.observe(ref, 'power', power + counters)
            self.observe(ref, 'toughness', toughness + counters)
            self.observe(ref, 'tapped', int(ref in self.sponsors or self.initial_states.get(ref) == TAPPED))
            if ref in self.sponsors or ref == 'spare':
                self.observe(ref, 'sick', 1)

    def cast_convoke(self):
        self.emit('turn', player=self.actor)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        self.mana('set_mana', self.actor, self.pool)
        self.emit('refresh')
        if self.actor == 0:
            for ref in self.sponsors:
                self.emit('choose_target', ref, player=self.actor)
        self.emit('cast', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_hand', 0)
        self.observe('source', 'in_play', 0)
        self.mana('assert_mana', self.actor, ZERO)
        self.mana('assert_mana', self.foe, ZERO)
        self.emit('assert_life', player=self.actor, value=20)
        self.creature_receipts(pending=True)

    def counter_source(self):
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle[COUNTER]))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2)
        self.mana('assert_mana', self.foe, ZERO)
        self.emit('resolve_response')
        self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'

    def totem_choice(self):
        if self.mode == 'decline':
            if self.actor == 1:
                self.emit('choose_optional_target', 'payer_0', player=self.actor)
            return
        if self.actor == 0:
            self.emit('choose_trigger', 'source', player=self.actor)
        if self.mode != 'alone':
            ref = 'opponent_creature' if self.mode == 'foe_target' else 'payer_0'
            self.emit('choose_target', ref, player=self.actor)
            self.counters[ref] = 1

    def resolve_entry(self):
        if self.csvid == TOTEM:
            self.totem_choice()
        elif self.target:
            self.emit('choose_target', self.target, player=self.actor)
        self.emit('resolve')
        self.cards['source']['zone'] = 'battlefield'
        if self.csvid == PHALANX:
            self.life[self.actor] += sum(
                item['player'] == self.actor and item['zone'] == 'battlefield'
                and self.oracle[item['name']]['type_line'].startswith('Creature') for item in self.cards.values())
        elif self.csvid == LOXODON:
            self.counters.update({ref: 1 for ref in self.sponsors})
        elif self.target:
            self.cards[self.target]['zone'] = 'exile'

    def tribunal_return(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.emit('upkeep_step', player=self.foe)
        self.emit('turn', player=self.foe)
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle[REMOVAL]))
        self.emit('refresh')
        self.emit('choose_target', 'source', player=self.foe)
        self.emit('cast', 'removal', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.foe, ZERO)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards['source']['zone'] = self.cards['removal']['zone'] = 'graveyard'
        self.cards[self.target]['zone'] = 'battlefield'
        self.observe('source', 'in_play', 0)
        self.emit('bind_permanent_copy', 'returned', player=self.foe, value=self.rows[GEAR]['csvid'], index=0)
        self.observe('returned', 'in_play', 1)
        self.receipts()

    def build(self):
        self.cast_convoke()
        if self.mode == 'countered':
            self.counter_source()
        else:
            self.resolve_entry()
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        if self.mode == 'decline' and self.actor == 1:
            self.emit('assert_forced_target_choices', value=0)
        self.emit('refresh')
        self.observe('source', 'in_play', int(self.cards['source']['zone'] == 'battlefield'))
        if self.target:
            self.observe(self.target, 'in_play', 0)
        self.creature_receipts()
        self.receipts()
        if self.mode == 'artifact_return':
            self.tribunal_return()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in PRINTED or not record or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole normal Convoke entry primary required')
    _, cost, type_line, expected_stats, rider = PRINTED[csvid]
    if (record.get('type_line') != type_line or record.get('mana_cost') != cost
            or record.get('keywords') != ['Convoke'] or record.get('oracle_text') != REMINDER + '\n' + rider
            or (expected_stats is not None
                and (record.get('power'), record.get('toughness')) != tuple(map(str, expected_stats)))):
        raise ValueError('complete Convoke and entry rules required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), stats=list(expected_stats) if expected_stats else None)


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed Convoke entry variants required')


def spec(declaration, record):
    primary = profiles()['sources'].get(record.get('name', '')) if record else None
    if not primary or not exact(primary['oracle'], record):
        raise ValueError('exact pinned Convoke entry record required')
    shape = classify(record, primary['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 Convoke entry declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete Convoke entry source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal'):
            raise ValueError('pinned Convoke entry card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])
    for name, (color, power, toughness) in CREATURES.items():
        if (oracle[name]['colors'] != [color] or stats(oracle[name]) != (power, toughness)
                or not oracle[name]['type_line'].startswith('Creature')):
            raise ValueError('Convoke witness creature color or stats changed: ' + name)
    if (oracle[ARTIFACT]['type_line'] != 'Artifact' or oracle[GEAR]['type_line'] != 'Artifact — Equipment'
            or oracle['Plains']['type_line'] != 'Basic Land — Plains'
            or oracle[COUNTER]['oracle_text'] != 'Counter target spell.'
            or oracle[REMOVAL]['oracle_text'] != 'Destroy target artifact or enchantment.'):
        raise ValueError('reviewed Convoke entry witnesses required')


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered Convoke entry primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = Duel(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        cases.append(dict(name=f'{PREFIX}{csvid}_{variant}', card=name, fixture='empty_main', seed=2688,
                          setup=board.setup, steps=board.steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, []))))
    return cases

