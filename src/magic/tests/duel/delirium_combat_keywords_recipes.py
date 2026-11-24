"""Real combat with Delirium thresholds and mutable graveyards."""

from collections import Counter
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact


FAMILY = 'delirium_combat_keywords'
PROFILE = Path(__file__).with_name('data') / 'delirium_combat_keywords_wave8_profiles.json'
CLAUSE = ' as long as there are four or more card types among cards in your graveyard.'
MENACE_NOTE = " (A creature with menace can't be blocked except by two or more creatures.)"
SHAPES = {
    15855: ('menace', 0, 0, ('This creature has menace', MENACE_NOTE)),
    15864: ('vigilance', 1, 0, ('This creature gets +1/+0 and has vigilance', '')),
    15874: ('trample', 0, 0, ('This creature has trample', '')),
    15915: ('first_strike', 1, 0, ('This creature gets +1/+0 and has first strike', '')),
    15942: ('double_strike', 0, 0, ('This creature has double strike', '')),
    16219: ('menace', 1, 1, ('This creature gets +1/+1 and has menace', MENACE_NOTE)),
}
COMMON = ('three_duplicates', 'four_types', 'opponent_only')
EXTRA = {15855: 'four_pair', 15864: 'crossing', 15942: 'first_strike_control', 16219: 'four_pair'}
THREE = ('Mountain', 'Grizzly Bears', 'Giant Growth', 'Mountain')
FOUR = ('Mountain', 'Grizzly Bears', 'Giant Growth', "Traveler's Amulet")
MIXED = ('Mountain', 'Giant Growth', 'Ornithopter')
CRYPT = "Tormod's Crypt"
EXPANSION = ('short_funding', 'countered', 'mixed_type', 'lose_fourth', 'replay')
PRESENT = ('four_types', 'four_pair', 'mixed_type', 'lose_fourth', 'short_funding', 'countered')
GAINS = ('crossing', 'gain_fourth', 'replay')
COMBAT_WITNESSES = {'Suntail Hawk', 'Ornithopter', 'Grizzly Bears', 'Squire'}
WITNESSES = set(THREE + FOUR + MIXED) | COMBAT_WITNESSES | {CRYPT, 'Counterspell', 'Disenchant', 'Island', 'Plains'}
GAP = 'Two-blocker menace controls end after assigning both blockers, before damage distribution.'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    from recipes import cost_from_oracle

    if (csvid not in SHAPES or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces') or not record.get('type_line', '').startswith('Creature — ')):
        raise ValueError('implemented whole normal Delirium creature required')
    keyword, power, toughness, (prefix, note) = SHAPES[csvid]
    start = 'First strike\n' if csvid == 15942 else ''
    keywords = {'Delirium', 'First strike'} if csvid == 15942 else {'Delirium'}
    if (record.get('oracle_text') != f'{start}Delirium — {prefix}{CLAUSE}{note}'
            or set(record.get('keywords', ())) != keywords
            or any(not record.get(stat, '').isdigit() or int(record[stat]) < 1 for stat in ('power', 'toughness'))):
        raise ValueError('complete printed Delirium combat clause and base keyword required')
    return dict(csvid=csvid, keyword=keyword, power_delta=power, toughness_delta=toughness,
                mana=cost_from_oracle(record))


def variants(shape):
    csvid = shape['csvid']
    old = COMMON + ((EXTRA[csvid],) if csvid in EXTRA else ())
    extra = EXPANSION + (('gain_fourth',) if csvid != 15864 else ())
    return [f'{mode}_{player}' for mode in old + extra for player in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Delirium combat variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('complete pinned Delirium primary required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared Delirium combat branch required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def types_for(cards, oracle):
    return {kind for name in cards for kind in oracle[name]['type_line'].split(' — ', 1)[0].split()
            if kind in {'Artifact', 'Battle', 'Creature', 'Enchantment', 'Instant',
                        'Kindred', 'Land', 'Planeswalker', 'Sorcery'}}


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Delirium witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Delirium primary or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Delirium primary required')
                classify(oracle[name], row['csvid'])
    assert len(THREE) == len(FOUR) == 4 and len(types_for(THREE, oracle)) == 3
    assert types_for(FOUR, oracle) == types_for(MIXED, oracle) == {'Artifact', 'Creature', 'Instant', 'Land'}
    assert types_for(THREE + ('Disenchant',), oracle) == types_for(THREE, oracle)


class Duel:
    def __init__(self, name, csvid, actor, mode, registry, oracle):
        self.name, self.actor, self.foe, self.mode = name, actor, 1 - actor, mode
        self.csvid, self.registry, self.oracle = csvid, registry, oracle
        self.keyword, self.power_delta, self.toughness_delta, _ = SHAPES[csvid]
        self.active = mode in PRESENT
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.add('source', actor, name, 'hand')
        for player, cards in enumerate(self.graves()):
            for i, card in enumerate(cards):
                self.add(f'g_{player}_{i}', player, card, 'graveyard')
        for player in (0, 1):
            for i, name in enumerate(('Island', 'Plains')):
                self.add(f'd_{player}_{i}', player, name, 'library')
        blocker = {
            'menace': 'Suntail Hawk', 'trample': 'Ornithopter', 'first_strike': 'Grizzly Bears',
        }.get(self.keyword)
        if mode == 'first_strike_control':
            blocker = 'Grizzly Bears'
        if blocker:
            self.add('blocker', self.foe, blocker, 'battlefield')
        if mode == 'four_pair':
            self.add('second', self.foe, 'Squire', 'battlefield')
        if mode in GAINS:
            self.add('amulet', actor, "Traveler's Amulet", 'battlefield')
            self.add('disenchant', actor, 'Disenchant', 'hand')
        if mode == 'lose_fourth':
            self.add('crypt', actor, CRYPT, 'battlefield')
        if mode == 'countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')

    def graves(self):
        result = [(), ()]
        result[self.actor] = MIXED if self.mode == 'mixed_type' else FOUR if self.active else THREE
        if self.mode == 'opponent_only':
            result[self.foe] = FOUR
        return result

    def add(self, ref, player, name, zone):
        from recipes import card

        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(player=player, name=name, zone=zone)

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
        if op in ('assert_zone', 'assert_zone_multiplicity'):
            fields['zone'] = next(name for name, number in ZONE.items() if number == zone)
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, pool):
        for color, amount in enumerate(pool):
            self.emit(op, player=player, color=color, value=amount)

    def cast_pending(self, ref, target=None, *, allowed=True):
        from recipes import cost_from_oracle

        payment = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        if not allowed:
            needed = next(color for color, amount in enumerate(payment) if amount)
            payment[needed] -= 1
        self.mana('set_mana', self.actor, payment)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=self.actor)
        self.emit('cast', ref, value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed))
        self.mana('assert_mana', self.actor, [0] * 6 if allowed else payment)

    def cast(self, ref, destination, target=None):
        self.cast_pending(ref, target)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.cards[ref]['zone'] = destination
        self.emit('refresh')

    def turn_ready(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, [0] * 6)
        self.cast('source', 'battlefield')
        self.observe('source', 'in_play', 1)
        self.observe('source', 'sick', 1)
        for player in (self.foe, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            self.emit('turn', player=player)
        self.observe('source', 'sick', 0)

    def stats(self):
        record = self.oracle[self.name]
        return (int(record['power']) + self.power_delta * int(self.active),
                int(record['toughness']) + self.toughness_delta * int(self.active))

    def state(self):
        self.emit('refresh')
        if self.cards['source']['zone'] == 'battlefield':
            self.observe('source', 'in_play', 1)
            for prop, expected in zip(('power', 'toughness'), self.stats()):
                self.observe('source', prop, expected)
        zones = ('graveyard', 'battlefield', 'hand')
        if self.mode == 'lose_fourth':
            zones += ('exile',)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in zones:
                counts = Counter(c['name'] for c in self.cards.values()
                                 if c['player'] == player and c['zone'] == zone)
                self.emit('assert_zone', player=player, zone=ZONE[zone], value=sum(counts.values()))
                for name in sorted({c['name'] for c in self.cards.values()}):
                    self.emit('assert_zone_multiplicity', player=player, zone=ZONE[zone],
                              index=self.registry[name]['csvid'], value=counts[name])

    def cross_threshold(self):
        self.observe('amulet', 'in_play', 1)
        self.cast('disenchant', 'graveyard', target='amulet')
        self.cards['amulet']['zone'] = 'graveyard'
        self.active = True
        self.state()

    def combat(self):
        power, _ = self.stats()
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.observe('source', 'attacking', 1)
        tapped = not (self.keyword == 'vigilance' and self.active)
        self.observe('source', 'tapped', int(tapped))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked = 'blocker' in self.cards and not (
            self.keyword == 'menace' and self.active and 'second' not in self.cards
        )
        if 'blocker' in self.cards:
            self.emit('block', 'blocker', 'source', value=int(blocked))
        if 'second' in self.cards:
            self.emit('block', 'second', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', int(blocked))
        if 'second' in self.cards:
            # Verify both real assignments before combat damage distribution.
            self.observe('blocker', 'blocking_count', 1)
            self.observe('second', 'blocking_count', 1)
            self.emit('assert_choices_consumed')
            return
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        if not blocked:
            self.life[self.foe] -= power * (2 if self.keyword == 'double_strike' and self.active else 1)
        else:
            self.cards['blocker']['zone'] = 'graveyard'
            if self.keyword == 'first_strike' and not self.active:
                self.cards['source']['zone'] = 'graveyard'
            if self.keyword == 'trample' and self.active:
                lethal = int(self.oracle[self.cards['blocker']['name']]['toughness'])
                self.life[self.foe] -= power - lethal
            if self.cards['source']['zone'] == 'battlefield':
                damage = int(self.keyword == 'menace')
                self.observe('source', 'damage', damage)
        if self.cards['source']['zone'] == 'battlefield':
            self.observe('source', 'tapped', int(tapped))

    def end_threshold(self):
        self.emit('choose_player', player=self.actor, value=self.actor)
        self.emit('activate', 'crypt', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.cards['crypt']['zone'] = 'graveyard'
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        for card in self.cards.values():
            if card['player'] == self.actor and card['zone'] == 'graveyard':
                card['zone'] = 'exile'
        self.active = False
        self.state()

    def deny_entry(self):
        from recipes import cost_from_oracle

        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, [0] * 6)
        if self.mode == 'short_funding':
            self.cast_pending('source', allowed=False)
            self.observe('source', 'in_hand', 1)
        else:
            self.cast_pending('source')
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
        self.state()

    def build(self):
        from recipes import step

        if self.mode in ('short_funding', 'countered'):
            self.deny_entry()
            return
        self.turn_ready()
        self.state()
        cut = len(self.steps)
        if self.mode in GAINS:
            self.cross_threshold()
        if self.mode == 'lose_fourth':
            self.end_threshold()
        self.combat()
        self.state()
        if self.mode == 'replay':
            suffix = self.steps[cut:]
            self.steps[cut:] = ([step('capture')] + suffix
                                + [step('record_state'), step('ai_roundtrip'), step('restore')]
                                + deepcopy(suffix) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Delirium primary CSVID required')
    cases = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        duel = Duel(name, csvid, int(player), mode, rows, oracle)
        duel.build()
        cases.append(dict(
            name=f'DeliriumCombatKeywordsRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2370,
            setup=duel.setup, steps=duel.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'],
                         untested_components=[GAP] if mode == 'four_pair' else []),
        ))
    return cases

