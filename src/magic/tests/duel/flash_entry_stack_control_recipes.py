"""Paid Flash responses and entry abilities with live stack targets."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from fight_entry_fighters_recipes import Board
from recipes import cost_from_oracle


FAMILY = 'flash_entry_stack_control'
PROFILE = Path(__file__).with_name('data') / 'flash_entry_stack_control_wave8_profiles.json'
PREFIX = 'FlashEntryStackControlRecipe_'

WHELK, SNAKE, SPRITE, QUELLER = 8522, 5405, 9265, 16192
FRILLED = 19629
FRILLED_NAME = 'Frilled Mystic'
PRINTED = {
    WHELK: ('time_spiral.c', '{4}{U}{U}', 'Illusion', (1, 1), ('Flying', 'Flash'),
            "Flash\nFlying\nWhen this creature enters, counter target spell. Put X +1/+1 counters on this creature, where X is that spell's mana value."),
    SNAKE: ('apocalypse.c', '{1}{G}{U}{U}', 'Snake', (2, 2), ('Flash',),
            'Flash\nWhen this creature enters, counter target spell.'),
    SPRITE: ('lorwyn.c', '{1}{U}', 'Faerie Wizard', (1, 1), ('Flying', 'Flash'),
             'Flash\nFlying\nWhen this creature enters, counter target spell with mana value X or less, where X is the number of Faeries you control.'),
    QUELLER: ('eldritch_moon.c', '{1}{W}{U}', 'Spirit', (2, 3), ('Flying', 'Flash'),
              "Flash\nFlying\nWhen this creature enters, exile target spell with mana value 4 or less.\nWhen this creature leaves the battlefield, the exiled card's owner may cast that card without paying its mana cost."),
    FRILLED: ('ravnica_allegiance.c', '{G}{G}{U}{U}', 'Elf Lizard Wizard', (3, 2), ('Flash',),
              'Flash\nWhen this creature enters, you may counter target spell.'),
}
MODES = {
    WHELK: ('low', 'high', 'uncounterable', 'flight'),
    SNAKE: ('low', 'high', 'uncounterable', 'empty'),
    SPRITE: ('solo_flight', 'below_count', 'own_faerie', 'foe_faerie', 'own_kindred'),
    QUELLER: ('four', 'five', 'uncounterable', 'leave_cast', 'leave_decline', 'flight'),
    FRILLED: ('low', 'decline', 'uncounterable', 'empty_countered'),
}
LOW = 'Reach Through Mists'
TWO = 'Think Twice'
FOUR = 'Weave Fate'
HIGH = "Jace's Ingenuity"
UNC_HUGE, UNC_SMALL = 'Terra Stomper', 'Great Sable Stag'
BOUNCE = 'Unsummon'
COUNTER = 'Counterspell'
FAERIE, KINDRED = "Oona's Gatewarden", 'Bitterblossom'
GROUND, AIR = 'Dazzling Ramparts', 'Drift of Phantasms'
PARENT_DRAWS = {LOW: 1, TWO: 1, FOUR: 2, HIGH: 3}
WITNESSES = set(PARENT_DRAWS) | {UNC_HUGE, UNC_SMALL, BOUNCE, COUNTER, FAERIE, KINDRED, GROUND, AIR,
                                'Forest', 'Island', 'Mountain', 'Plains'}
GAPS = {FRILLED: ['The empty-stack Flash case uses a real Counterspell before entry. Empty-stack entry resolution '
                  'is outside this scope because the callback formats the stack target name before testing for one.']}
ZERO = [0] * 6


def stats(record):
    return tuple(int(record[key]) for key in ('power', 'toughness'))


def parent_for(csvid, mode):
    if mode in ('empty', 'empty_countered'):
        return None
    if mode == 'uncounterable':
        return UNC_HUGE if csvid == WHELK else UNC_SMALL
    if mode in ('high', 'five'):
        return HIGH
    if mode == 'four':
        return FOUR
    if csvid == SPRITE and mode != 'solo_flight':
        return TWO
    return LOW


class Duel(Board):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.library = {actor: ['Mountain', 'Plains'], self.foe: ['Island', 'Forest', 'Plains', 'Mountain']}
        self.drawn = 0
        self.counters = 0
        self.parent = parent_for(csvid, mode)
        self.parent_mv = sum(cost_from_oracle(oracle[self.parent])) if self.parent else None
        self.add('source', actor, record['name'], 'hand')
        if self.parent:
            self.add('parent', self.foe, self.parent, 'hand')
        for seat, names in self.library.items():
            for index, name in enumerate(names):
                self.add(f'library_{seat}_{index}', seat, name, 'library')
        if mode in ('flight', 'solo_flight'):
            self.add('ground', self.foe, GROUND, 'battlefield')
            self.add('air', self.foe, AIR, 'battlefield')
        if csvid == SPRITE:
            if mode == 'own_kindred':
                self.add('kindred', actor, KINDRED, 'battlefield')
            if mode in ('foe_faerie', 'own_faerie'):
                self.add('faerie', actor if mode == 'own_faerie' else self.foe, FAERIE, 'battlefield')
                self.add('own_wall', actor, GROUND, 'battlefield')
        if mode in ('leave_cast', 'leave_decline'):
            self.add('bounce', self.foe, BOUNCE, 'hand')
        if mode == 'empty_countered':
            self.add('counter', self.foe, COUNTER, 'hand')

    def charge_cast(self, ref, *, response=False, target=None):
        item = self.cards[ref]
        seat = item['player']
        self.mana('set_mana', seat, cost_from_oracle(self.oracle[item['name']]))
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=seat)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2 if response else 1)
        self.observe(ref, 'in_hand', 0)
        self.observe(ref, 'in_play', 0)
        for player in (0, 1):
            self.mana('assert_mana', player, ZERO)

    def draw_from_parent(self):
        count = PARENT_DRAWS[self.parent]
        for index in range(self.drawn, self.drawn + count):
            self.cards[f'library_{self.foe}_{index}']['zone'] = 'hand'
        self.drawn += count
        del self.library[self.foe][:count]

    def projected_parent(self):
        if self.csvid == QUELLER and self.parent_mv <= 4:
            self.cards['parent']['zone'] = 'exile'
            return
        if self.csvid == WHELK:
            self.counters = self.parent_mv
        if self.parent in (UNC_HUGE, UNC_SMALL):
            self.cards['parent']['zone'] = 'battlefield'
            return
        self.cards['parent']['zone'] = 'graveyard'
        if (self.csvid == QUELLER or (self.csvid == SPRITE and self.parent_mv > self.faerie_count())
                or (self.csvid == FRILLED and self.mode == 'decline')):
            self.draw_from_parent()

    def faerie_count(self):
        return sum(item['player'] == self.actor and item['zone'] == 'battlefield'
                   and 'Faerie' in self.oracle[item['name']]['type_line'].split(' — ')[-1].split()
                   for item in self.cards.values())

    def inspect_entry(self):
        self.emit('refresh')
        power, toughness = stats(self.record)
        self.observe('source', 'in_play', 1)
        self.observe('source', 'plus1_counters', self.counters)
        self.observe('source', 'power', power + self.counters)
        self.observe('source', 'toughness', toughness + self.counters)
        if self.parent:
            self.observe('parent', 'in_play', int(self.cards['parent']['zone'] == 'battlefield'))
        self.receipts()

    def queller_leaves(self):
        self.charge_cast('bounce', target='source')
        self.emit('present_ai_dialogs', value=1)
        self.emit('choose_dialog', player=self.foe, value=int(self.mode == 'leave_decline'))
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('present_ai_dialogs', value=0)
        self.emit('refresh')
        self.cards['source']['zone'] = 'hand'
        self.cards['bounce']['zone'] = 'graveyard'
        if self.mode == 'leave_cast':
            self.cards['parent']['zone'] = 'graveyard'
            self.draw_from_parent()
        self.observe('source', 'in_play', 0)
        for seat in (0, 1):
            self.mana('assert_mana', seat, ZERO)
        self.receipts()

    def flying_combat(self):
        # The Flash creature entered on the opponent's turn and now reaches
        # its actual next turn before being declared as an attacker.
        self.emit('advance_turn')
        self.emit('untap_step', player=self.actor)
        self.emit('upkeep_step', player=self.actor)
        self.emit('turn', player=self.actor)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground', 'source', value=0)
        self.emit('block', 'air', 'source', value=1)
        self.emit('advance_combat')
        self.observe('ground', 'blocking_count', 0)
        self.observe('air', 'blocking_count', 1)
        self.observe('source', 'blocked', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        power = stats(self.record)[0] + self.counters
        self.observe('air', 'in_play', 1)
        self.observe('air', 'damage', power)
        self.observe('source', 'in_play', 1)
        self.observe('source', 'damage', 0)
        self.receipts()

    def frilled_choice(self):
        # The Human accepts the optional trigger first. Both seats then
        # receive the callback's actual Counter/Pass dialog.
        if self.actor == 0:
            self.emit('choose_trigger', 'source', player=self.actor)
        self.emit('choose_dialog', player=self.actor, value=int(self.mode == 'decline'))

    def counter_frilled_before_entry(self):
        self.emit('announce_spell', 'source')
        self.charge_cast('counter', response=True)
        self.emit('resolve_response')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
        self.observe('source', 'in_play', 0)
        self.receipts()

    def build(self):
        self.emit('turn', player=self.foe)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        if self.parent:
            self.charge_cast('parent')
            self.emit('announce_spell', 'parent')
            self.emit('assert_stack', value=1)
        self.charge_cast('source', response=bool(self.parent))
        if self.csvid == FRILLED:
            if not self.parent:
                self.counter_frilled_before_entry()
                return
            self.frilled_choice()
        self.emit('resolve_response' if self.parent else 'resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.cards['source']['zone'] = 'battlefield'
        if self.parent:
            self.projected_parent()
        self.inspect_entry()
        if self.mode in ('leave_cast', 'leave_decline'):
            self.queller_leaves()
        if self.mode in ('flight', 'solo_flight'):
            self.flying_combat()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in PRINTED or not record or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole normal Flash stack-control primary required')
    _, cost, subtype, printed_stats, keywords, rules = PRINTED[csvid]
    if (record.get('type_line') != 'Creature — ' + subtype or record.get('mana_cost') != cost
            or record.get('keywords') != list(keywords) or record.get('oracle_text') != rules
            or (record.get('power'), record.get('toughness')) != tuple(map(str, printed_stats))):
        raise ValueError('complete printed Flash, statistics and stack rules required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), stats=list(printed_stats))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed Flash stack-control variants required')


def spec(declaration, record):
    primary = profiles()['sources'].get(record.get('name', '')) if record else None
    if not primary or not exact(primary['oracle'], record):
        raise ValueError('exact pinned Flash stack-control record required')
    shape = classify(record, primary['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 Flash stack-control declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete Flash stack-control source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal'):
            raise ValueError('pinned Flash stack-control card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])
    if ({name: sum(cost_from_oracle(oracle[name])) for name in PARENT_DRAWS} != {LOW: 1, TWO: 2, FOUR: 4, HIGH: 5}
            or oracle[LOW]['oracle_text'] != 'Draw a card.'
            or not oracle[TWO]['oracle_text'].startswith('Draw a card.\nFlashback')
            or oracle[FOUR]['oracle_text'] != 'Draw two cards.'
            or oracle[HIGH]['oracle_text'] != 'Draw three cards.'
            or not all(oracle[name]['oracle_text'].startswith("This spell can't be countered.")
                       for name in (UNC_HUGE, UNC_SMALL))
            or oracle[BOUNCE]['oracle_text'] != "Return target creature to its owner's hand."
            or oracle[COUNTER]['oracle_text'] != 'Counter target spell.'
            or oracle[KINDRED]['type_line'] != 'Kindred Enchantment — Faerie'
            or oracle[FAERIE]['type_line'] != 'Creature — Faerie Soldier'
            or stats(oracle[GROUND]) != (0, 7) or 'Flying' in oracle[GROUND]['keywords']
            or stats(oracle[AIR]) != (0, 5) or 'Flying' not in oracle[AIR]['keywords']):
        raise ValueError('reviewed Flash stack-control witnesses required')


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered Flash stack-control primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = Duel(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        cases.append(dict(name=f'{PREFIX}{csvid}_{variant}', card=name, fixture='empty_main', seed=2717,
                          setup=board.setup, steps=board.steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, []))))
    return cases

