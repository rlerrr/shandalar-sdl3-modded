"""Paid Threshold abilities with real card-count transitions and combat."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'threshold_combat_activated'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZONE_NAMES = {number: name for name, number in ZONE.items()}
ZERO = [0] * 6

THRESHOLD = 'Threshold — As long as there are seven or more cards in your graveyard, '
CLAUSES = {
    5542: ('{B}, Discard a card: Regenerate this creature.',
           THRESHOLD + "this creature gets +2/+2 and can't block."),
    5944: ('Flying', THRESHOLD + 'this creature gets +3/+3 and has '
           '"{R}: This creature gets +1/+0 until end of turn."'),
    5593: ('Trample', 'Threshold — {1}{G}: Regenerate this creature. Activate only if there are seven or more cards in your graveyard.'),
    5866: ('Discard a card: This creature gains flying until end of turn.',
           THRESHOLD + "this creature gets +1/+1 and can't block."),
    5673: ('Flying', 'Whenever a creature dealt damage by this creature this turn dies, put a +1/+1 counter on this creature.',
           THRESHOLD + 'this creature is white and has "{T}: Destroy target black creature."'),
    5752: ('{T}: Add {G}.',
           'Threshold — This creature gets +3/+3 as long as there are seven or more cards in your graveyard.'),
}
DELTAS = {5542: 2, 5944: 3, 5866: 1, 5752: 3}
COMMON = ('six', 'seven', 'opponent_only')
EXTRA = {
    5542: ('regen_cross', 'regen_no_discard'),
    5944: ('wrong_red',),
    5866: ('discard_cross', 'empty_hand'),
    5673: ('no_black_target',),
}
TRANSITIONS = ('crossing', 'lose_threshold', 'replay')
WITNESSES = {'Mountain', 'Island', 'Plains', 'Elvish Mystic', 'Wall of Roots', 'Ornithopter',
             'Welkin Tern', 'Muck Rats', 'Murder', 'Giant Growth', "Traveler's Amulet",
             'Disenchant', "Tormod's Crypt", 'Counterspell'}
DUMMY_WITNESSES = {'Muck Rats', 'Ornithopter'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (tuple(record['oracle_text'].splitlines()) != CLAUSES[csvid] or record.get('layout') != 'normal'
            or not record['type_line'].startswith('Creature — ') or 'Threshold' not in record['keywords']
            or record.get('card_faces') or record.get('selected_face')):
        raise ValueError('Exact whole Threshold activated creature required')
    return dict(csvid=csvid, delta=DELTAS.get(csvid, 0), clauses=list(CLAUSES[csvid]),
                price=cost_from_oracle(record), base=[int(record[k]) for k in ('power', 'toughness')])


def variants(model):
    csvid = model['csvid']
    modes = COMMON + EXTRA.get(csvid, ()) + ('countered', 'lose_threshold', 'replay')
    if csvid != 5752:
        modes += ('crossing',)
    if csvid in (5542, 5593):
        modes += ('regen_wrong_mana',)
    return [f'{mode}_{seat}' for mode in modes for seat in (0, 1)]


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {p['registry']['csvid'] for p in pins['sources'].values()} != CLAUSES.keys()):
        raise ValueError('Exact independent source and Threshold witness pins required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            handler = row.get('handler')
            if (not exact(pin, {'oracle': record, 'registry': row}) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or handler in (None, 'NULL', '0')
                    or handler == 'card_dummy' and name not in DUMMY_WITNESSES):
                raise ValueError(f'Pinned Threshold source or witness changed: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned Threshold source required')
    model = classify(record, pin['registry']['csvid'])
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact Threshold clauses and distinct scoped variants required')
    return dict(model, family=FAMILY)

class ThresholdBoard:
    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone, damage=0)

    def emit(self, op, ref='', other='', prop='', *, zone=None, **fields):
        row = step(op, **fields)
        for key, field in [('ref', ref), ('other', other), ('property', prop)]:
            if field:
                row[key] = field
        if zone is not None:
            row['zone'] = ZONE_NAMES[zone]
        self.steps.append(row)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, value=value, color=color)

    def cast(self, ref, destination, target=None):
        cost = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        self.mana('set_mana', self.actor, cost)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=self.actor)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.cards[ref]['zone'] = destination
        self.emit('refresh')

    def receipts(self):
        self.emit('refresh')
        if self.cards['source']['zone'] == 'battlefield':
            power, toughness = self.stats('source')
            self.observe('source', 'in_play', 1)
            self.observe('source', 'power', power)
            self.observe('source', 'toughness', toughness)
        for ref, card in self.cards.items():
            if card['zone'] == 'battlefield' and card['damage']:
                self.observe(ref, 'damage', card['damage'])
        identities = sorted({card['name'] for card in self.cards.values() if card['zone'] != 'library'})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ('hand', 'battlefield', 'graveyard'):
                occurrences = Counter(card['name'] for card in self.cards.values()
                                      if card['player'] == player and card['zone'] == zone)
                self.emit('assert_zone', player=player, zone=ZONE[zone], value=sum(occurrences.values()))
                for name in identities:
                    self.emit('assert_zone_multiplicity', player=player, zone=ZONE[zone],
                              index=self.rows[name]['csvid'], value=occurrences[name])



class ActivatedThresholdDuel(ThresholdBoard):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.counters = 0
        self.pump = (0, 0)
        self.snapshot = None
        self.add('source', actor, record['name'], 'hand')
        seven = mode in ('seven', 'regen_no_discard', 'wrong_red', 'empty_hand', 'no_black_target',
                         'lose_threshold', 'replay', 'regen_wrong_mana')
        for seat in (0, 1):
            count = (7 if seven else 6) if seat == actor else 7 if mode == 'opponent_only' else 0
            for i in range(count):
                self.add(f'g_{seat}_{i}', seat, 'Mountain', 'graveyard')
            for i, name in enumerate(('Island', 'Plains')):
                self.add(f'd_{seat}_{i}', seat, name, 'library')
        if mode == 'countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')
            return
        if csvid in (5542, 5866) and mode in COMMON + TRANSITIONS:
            self.add('witness', self.foe, 'Elvish Mystic', 'battlefield')
        if csvid == 5944 or mode == 'discard_cross':
            self.add('witness', self.foe, 'Wall of Roots', 'battlefield')
        if csvid == 5593:
            self.add('witness', self.foe, 'Ornithopter', 'battlefield')
        if csvid == 5673:
            self.add('witness', self.foe, 'Welkin Tern', 'battlefield')
            self.add('ground', self.foe, 'Wall of Roots', 'battlefield')
            if mode != 'no_black_target':
                self.add('black', self.foe, 'Muck Rats', 'battlefield')
        if mode in ('regen_cross', 'discard_cross') or csvid == 5542 and mode == 'regen_wrong_mana':
            self.add('discard', actor, 'Mountain', 'hand')
        if csvid == 5593 or mode.startswith('regen_'):
            self.add('murder', self.foe, 'Murder', 'hand')
        if csvid == 5752:
            self.add('spend', actor, 'Giant Growth', 'hand')
        if mode == 'crossing':
            self.add('amulet', self.foe, "Traveler's Amulet", 'battlefield')
            self.add('disenchant', actor, 'Disenchant', 'hand')
        if mode in ('lose_threshold', 'replay'):
            # The real human selection supports either seat's graveyard.
            self.add('crypt', 0, "Tormod's Crypt", 'battlefield')

    @property
    def active(self):
        return sum(c['player'] == self.actor and c['zone'] == 'graveyard' for c in self.cards.values()) >= 7

    def stats(self, ref):
        record = self.oracle[self.cards[ref]['name']]
        base = [int(record[k]) for k in ('power', 'toughness')]
        if ref == 'source':
            delta = DELTAS.get(self.csvid, 0) if self.active else 0
            return tuple(value + delta + self.counters + boost for value, boost in zip(base, self.pump))
        return base

    def receipt_source_color(self):
        if self.csvid == 5673:
            self.observe('source', 'color', 32 if self.active else 2)
            self.observe('source', 'plus1_counters', self.counters)

    def advance(self, who):
        self.emit('advance_turn')
        self.emit('untap_step', player=who)
        self.emit('turn', player=who)
        self.pump = (0, 0)
        for entry in self.cards.values():
            entry['damage'] = 0

    def ready(self):
        self.advance(self.foe)
        self.advance(self.actor)
        self.observe('source', 'sick', 0)

    def fight(self, attacker, *, blocker=None, denied=(), tramples=False, grows=False):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.observe(attacker, 'attacking', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for ref in denied:
            self.emit('block', ref, attacker, value=0)
        if blocker:
            self.emit('block', blocker, attacker, value=1)
        self.emit('advance_combat')
        self.observe(attacker, 'blocked', int(blocker is not None))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        attack_power, attack_toughness = self.stats(attacker)
        defender = 1 - self.cards[attacker]['player']
        if blocker is None:
            self.life[defender] -= attack_power
            return
        block_power, block_toughness = self.stats(blocker)
        self.cards[blocker]['damage'] = attack_power
        self.cards[attacker]['damage'] = block_power
        if attack_power >= block_toughness:
            self.cards[blocker]['zone'] = 'graveyard'
            if grows:
                self.counters += 1
        if block_power >= attack_toughness:
            self.cards[attacker]['zone'] = 'graveyard'
        if tramples:
            self.life[defender] -= max(0, attack_power - block_toughness)

    def regenerate(self, *, allowed, discard=False):
        price = [0, 1, 0, 0, 0, 0] if self.csvid == 5542 else [1, 0, 0, 1, 0, 0]
        budget = price.copy()
        if self.mode == 'regen_wrong_mana':
            required = 1 if self.csvid == 5542 else 3
            budget[required], budget[4] = 0, 1
        self.mana('set_mana', self.actor, budget)
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Murder']))
        self.emit('refresh')
        if discard or self.csvid == 5542 and self.mode == 'regen_wrong_mana':
            self.emit('discard_policy', value=1)
        self.emit('choose_target', 'source', player=self.foe)
        self.emit('cast', 'murder', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.foe, ZERO)
        self.emit('choose_regeneration', 'source')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=int(not allowed))
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.cards['murder']['zone'] = 'graveyard'
        self.mana('assert_mana', self.actor, ZERO if allowed else budget)
        if allowed:
            self.cards['source']['damage'] = 0
            self.observe('source', 'tapped', 1)
            self.observe('source', 'damage', 0)
            if discard:
                self.cards['discard']['zone'] = 'graveyard'
        else:
            self.cards['source']['zone'] = 'graveyard'

    def pump_dragon(self):
        price = [0, 0, 0, 1, 0, 0] if self.mode == 'wrong_red' else [0, 0, 0, 0, 1, 0]
        self.mana('set_mana', self.actor, price)
        self.emit('refresh')
        allowed = self.active and self.mode != 'wrong_red'
        self.emit('can_activate', 'source', value=int(allowed))
        self.emit('activate', 'source', value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed))
        self.mana('assert_mana', self.actor, ZERO if allowed else price)
        self.observe('source', 'power', self.stats('source')[0])
        if allowed:
            self.emit('resolve')
            self.emit('refresh')
            self.emit('assert_stack', value=0)
            self.pump = (1, 0)
            self.receipts()
        self.fight('source', denied=('witness',))
        self.receipts()
        if not allowed:
            self.mana('set_mana', self.actor, ZERO)
            self.emit('refresh')
        self.advance(self.foe)

    def flying_discard(self):
        self.emit('discard_policy', value=1)
        self.emit('can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.cards['discard']['zone'] = 'graveyard'
        self.receipts()
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.fight('source', denied=('witness',))
        self.receipts()
        self.ready()
        # The graveyard still powers the static boost after temporary flying expires.
        self.fight('source', blocker='witness')

    def vampire(self):
        self.receipt_source_color()
        allowed = self.active and 'black' in self.cards
        self.emit('can_activate', 'source', value=int(allowed))
        if allowed:
            self.emit('choose_target', 'black', player=self.actor)
        self.emit('activate', 'source', value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed))
        if allowed:
            self.observe('source', 'tapped', 1)
            self.observe('black', 'in_play', 1)
            self.emit('resolve')
            self.emit('refresh')
            self.emit('assert_stack', value=0)
            self.cards['black']['zone'] = 'graveyard'
            self.receipts()
            self.ready()
        self.fight('source', blocker='witness', denied=('ground',), grows=True)
        self.receipt_source_color()

    def werebear_mana(self):
        self.emit('can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        green = [0, 0, 0, 1, 0, 0]
        self.mana('assert_mana', self.actor, green)
        self.observe('source', 'tapped', 1)
        self.emit('can_activate', 'source', value=0)
        self.emit('activate', 'source', value=0)
        assert cost_from_oracle(self.oracle['Giant Growth']) == green
        self.emit('choose_target', 'source', player=self.actor)
        self.emit('cast', 'spend', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.observe('source', 'power', self.stats('source')[0])
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.cards['spend']['zone'] = 'graveyard'
        self.pump = (3, 3)
        self.receipts()
        self.advance(self.foe)

    def change_threshold(self):
        if self.mode == 'crossing':
            self.cast('disenchant', 'graveyard', target='amulet')
            self.cards['amulet']['zone'] = 'graveyard'
        elif self.mode in ('lose_threshold', 'replay'):
            if self.mode == 'replay':
                self.snapshot = len(self.steps)
            crypt = self.cards['crypt']
            self.emit('choose_player', player=crypt['player'], value=self.actor)
            self.emit('activate', 'crypt', value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=1)
            crypt['zone'] = 'graveyard'
            self.receipts()
            self.emit('resolve')
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=0)
            removed = Counter()
            for entry in self.cards.values():
                if entry['player'] == self.actor and entry['zone'] == 'graveyard':
                    entry['zone'] = 'exile'
                    removed[entry['name']] += 1
            self.emit('assert_zone', player=self.actor, zone=ZONE['exile'], value=sum(removed.values()))
            for name, count in sorted(removed.items()):
                self.emit('assert_zone_multiplicity', player=self.actor, zone=ZONE['exile'],
                          index=self.rows[name]['csvid'], value=count)
        self.receipts()
        self.receipt_source_color()

    def countered(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        self.mana('set_mana', self.actor, cost_from_oracle(self.record))
        self.emit('refresh')
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_play', 0)
        self.mana('assert_mana', self.actor, ZERO)
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2)
        self.mana('assert_mana', self.foe, ZERO)
        self.emit('resolve_response')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
        self.receipts()

    def build(self):
        if self.mode == 'countered':
            self.countered()
            return
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        self.cast('source', 'battlefield')
        self.receipts()
        if self.csvid in (5673, 5752):
            self.receipt_source_color()
            self.observe('source', 'sick', 1)
            self.emit('can_activate', 'source', value=0)
            self.emit('activate', 'source', value=0)
        if self.csvid in (5542, 5866) and self.mode in COMMON + TRANSITIONS:
            if self.mode in TRANSITIONS:
                self.change_threshold()
            self.advance(self.foe)
            self.fight('witness', denied=('source',) if self.active else (), blocker=None if self.active else 'source')
        else:
            self.ready()
            if self.mode in TRANSITIONS:
                self.change_threshold()
            if self.csvid == 5542 and self.mode.startswith('regen_'):
                paid = self.mode == 'regen_cross'
                self.regenerate(allowed=paid, discard=paid)
            elif self.csvid == 5593:
                self.fight('source', blocker='witness', tramples=True)
                self.regenerate(allowed=self.active and self.mode != 'regen_wrong_mana')
            elif self.csvid == 5944:
                self.pump_dragon()
            elif self.mode == 'discard_cross':
                self.flying_discard()
            elif self.mode == 'empty_hand':
                self.emit('can_activate', 'source', value=0)
                self.emit('activate', 'source', value=0)
            elif self.csvid == 5673:
                self.vampire()
            elif self.csvid == 5752:
                self.werebear_mana()
        self.receipts()


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact Threshold source and scoped variants required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        game = ActivatedThresholdDuel(csvid, oracle[name], int(seat), mode, rows, oracle)
        game.build()
        actions = game.steps
        if game.snapshot is not None:
            cut = game.snapshot
            suffix = actions[cut:]
            actions = actions[:cut] + [step('capture')] + suffix + [
                step('record_state'), step('restore'), step('ai_roundtrip')
            ] + suffix + [step('assert_recorded_state')]
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'], untested_components=[])
        cases.append(dict(name=f'ThresholdCombatActivatedRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=2370, setup=game.setup, steps=actions, _recipe=metadata))
    return cases

