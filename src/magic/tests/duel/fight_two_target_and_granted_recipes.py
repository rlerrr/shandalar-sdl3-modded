"""Exact independently selected Fight targets, paid Strive, and death rewards."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


FAMILY = 'fight_two_target_and_granted'
PROFILE = Path(__file__).with_name('data') / 'fight_two_target_and_granted_wave8_profiles.json'
FEUD, TACTICS, FEED, DISSENSION = 12140, 13971, 13577, 15789
GRANTED_MODES = ('grant_two', 'grant_sick', 'grant_target_lost')
GRANTED_GAPS = ('Cross-controller activation and ambiguous identical grants are outside this bridge.',)
MODES = {
    FEUD: ('opposed', 'same_foe', 'mutual', 'lose_first', 'lose_second', 'prevention'),
    TACTICS: ('one', 'two', 'foreign', 'zero', 'short_color', 'lose_one', *GRANTED_MODES),
    FEED: ('kill_now', 'delayed', 'expired', 'lose_own', 'lose_foe', 'other_death'),
    DISSENSION: ('blocks', 'lose_blocker', 'one_blocker', 'no_blockers'),
}
WITNESSES = ('Cho-Manno, Revolutionary', 'Grizzly Bears', 'Hill Giant', 'Krosan Tusker', 'Millstone', 'Murder', 'Squire', 'Unsummon', 'Wall of Wood')

ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZONES = {value: name for name, value in ZONE.items()}
CARDS = {
    FEUD: ('Sorcery', '{4}{R}{R}'),
    TACTICS: ('Instant', '{1}{G}'),
    FEED: ('Sorcery', '{2}{G}'),
    DISSENSION: ('Instant', '{3}{R}{R}'),
}
GAPS = {
    TACTICS: ('Actual Strive, boost, target loss, and expiry are covered. A granted tap/fight '
              'requires a test action that can name its live attached native effect.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in CARDS or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')
            or (record.get('type_line'), record.get('mana_cost')) != CARDS[csvid]
            or set(record.get('keywords', ())) != ({'Fight', 'Strive'} if csvid == TACTICS else {'Fight'})):
        raise ValueError('exact normal whole Fight/Strive record required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in MODES[shape['csvid']]]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported two-target Fight variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned Fight primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed two-target Fight declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != set(WITNESSES)
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != CARDS.keys()):
        raise ValueError('exact two-target Fight sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('pinned Fight Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Fight primary required')
                classify(oracle[name], row['csvid'])


class Board:
    def __init__(self, record, owner, mode, rows, oracle):
        self.name, self.owner, self.foe, self.mode = record['name'], owner, 1 - owner, mode
        self.csvid, self.rows, self.oracle = rows[self.name]['csvid'], rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.mana, self.life = [[0] * 6 for _ in (0, 1)], [20, 20]
        self.active = owner
        self.add('source', owner, self.name, 'hand')
        if self.csvid == FEUD:
            if mode == 'same_foe':
                self.add('first', self.foe, 'Squire')
                self.add('second', self.foe, 'Hill Giant')
            else:
                self.add('first', owner, 'Grizzly Bears' if mode == 'mutual' else 'Hill Giant')
                self.add('second', self.foe, 'Cho-Manno, Revolutionary' if mode == 'prevention' else 'Grizzly Bears')
        elif self.csvid == TACTICS:
            self.add('first', owner, 'Grizzly Bears', 'hand' if mode == 'grant_sick' else 'battlefield')
            self.add('second', self.foe if mode == 'foreign' else owner, 'Hill Giant')
            if mode in GRANTED_MODES:
                self.add('untargeted', owner, 'Squire')
                self.add('duelist_two', self.foe, 'Wall of Wood')
            if mode == 'short_color':
                self.add('sink', owner, 'Millstone', 'hand')
        elif self.csvid == FEED:
            self.add('first', owner, 'Hill Giant' if mode == 'kill_now' else 'Wall of Wood')
            self.add('second', self.foe, 'Grizzly Bears')
            if mode in ('delayed', 'expired', 'lose_own', 'other_death'):
                self.add('finisher', owner, 'Murder', 'hand')
        else:
            self.add('raider', owner, 'Krosan Tusker')
            self.add('first', self.foe, 'Hill Giant')
            self.add('second', self.foe, 'Grizzly Bears')
        self.add('spare', self.foe, 'Squire')
        if mode in ('lose_first', 'lose_second', 'lose_own', 'lose_foe', 'lose_one', 'lose_blocker', 'grant_target_lost'):
            self.add('answer', self.foe, 'Unsummon', 'hand')

    def add(self, ref, player, name, zone='battlefield'):
        item = self.oracle[name]
        printed = (int(item['power']), int(item['toughness'])) if item.get('power') else None
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone, printed=printed,
                               boost=0, damage=0, tapped=False)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step', 'choose_done_target'):
            self.steps.append(step(op, player=player))
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

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def observe(self, ref, name, value):
        self.emit('assert_card', ref, prop=name, value=value)

    def wallet(self, player, cost, *, setting=False):
        for color, value in enumerate(cost):
            self.emit('set_mana' if setting else 'assert_mana', player=player, color=color, value=value)
        if setting:
            self.mana[player] = list(cost)

    def fund(self, player, cost):
        assert not any(self.mana[player])
        self.wallet(player, cost, setting=True)
        self.emit('refresh')

    def stats(self, ref):
        row = self.cards[ref]
        return tuple(value + row['boost'] for value in row['printed'])

    def packet(self):
        self.emit('assert_stack')
        self.choices()
        for ref, row in self.cards.items():
            if row['zone'] == 'battlefield' and row['printed']:
                for property_, value in zip(('power', 'toughness'), self.stats(ref), strict=True):
                    self.observe(ref, property_, value)
                self.observe(ref, 'damage', row['damage'])
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.wallet(player, self.mana[player])
            for zone, zone_id in ZONE.items():
                cards = [row['name'] for row in self.cards.values() if row['zone'] == zone and row['player'] == player]
                self.emit('assert_zone', player=player, zone=zone_id, value=len(cards))
                counts = Counter(cards)
                for name in sorted({item['name'] for item in self.cards.values()}):
                    self.emit('assert_zone_multiplicity', player=player, zone=zone_id,
                              index=self.rows[name]['csvid'], value=counts[name])

    def cast(self, ref, targets=(), *, stack=1, response=False, budget=None, fee=None, done=False, funded=False):
        row = self.cards[ref]
        actor = row['player']
        cost = cost_from_oracle(self.oracle[row['name']]) if fee is None else fee
        if not funded:
            self.fund(actor, cost if budget is None else budget)
        for target in targets:
            self.emit('choose_target', target, player=actor)
        if done:
            self.emit('choose_done_target', player=actor)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.choices()
        self.emit('assert_stack', value=stack)
        self.mana[actor] = [have - price for have, price in zip(self.mana[actor], cost, strict=True)]
        assert min(self.mana[actor]) >= 0
        self.wallet(actor, self.mana[actor])

    def resolve(self, *refs, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.choices()
        for ref in refs:
            self.cards[ref]['zone'] = 'graveyard'

    def bounce_response(self, target):
        self.emit('announce_spell', 'source')
        self.cast('answer', (target,), stack=2, response=True)
        self.resolve('answer', 'source', response=True)
        self.cards[target]['zone'] = 'hand'
        self.cards[target]['damage'] = 0

    def fight(self, first_ref='first', second_ref='second'):
        first, second = self.cards[first_ref], self.cards[second_ref]
        first_power, first_toughness = self.stats(first_ref)
        second_power, second_toughness = self.stats(second_ref)
        if second['name'] != 'Cho-Manno, Revolutionary':
            second['damage'] += first_power
        first['damage'] += second_power
        if first['damage'] >= first_toughness:
            first['zone'] = 'graveyard'
        if second['damage'] >= second_toughness:
            second['zone'] = 'graveyard'

    def next_turn(self):
        assert not any(self.mana[0]) and not any(self.mana[1])
        self.active = 1 - self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=self.active)
        self.emit('upkeep_step', player=self.active)
        self.emit('turn', player=self.active)
        for entry in self.cards.values():
            entry['boost'] = entry['damage'] = 0

    def feud(self):
        self.cast('source', ('first', 'second'))
        if self.mode.startswith('lose_'):
            self.bounce_response(self.mode.removeprefix('lose_'))
        else:
            self.resolve('source')
            self.fight()
        self.packet()

    def granted_action(self, ref, available, *, activate=False):
        op = 'activate_granted_legacy' if activate else 'can_activate_granted_legacy'
        self.steps.append(step(op, ref=ref, index=TACTICS, value=int(available)))

    def activate_fight(self, ref, opponent):
        self.granted_action(ref, True)
        self.emit('choose_target', opponent, player=self.owner)
        self.granted_action(ref, True, activate=True)
        self.choices()
        self.emit('assert_stack', value=1)
        self.observe(ref, 'tapped', 1)

    def granted_tactics(self):
        if self.mode == 'grant_sick':
            self.cast('first')
            self.resolve()
            self.cards['first']['zone'] = 'battlefield'
            self.observe('first', 'sick', 1)
        for recipient in ('first', 'second'):
            self.granted_action(recipient, False)

        targets = ('first',) if self.mode == 'grant_target_lost' else ('first', 'second')
        price = cost_from_oracle(self.oracle[self.name])
        price[3] += len(targets) - 1
        self.cast('source', targets, fee=price)
        self.resolve('source')
        for recipient in targets:
            self.cards[recipient]['boost'] += 1
        self.granted_action('untargeted', False)
        self.packet()

        if self.mode == 'grant_sick':
            self.granted_action('first', False)
            self.granted_action('first', False, activate=True)
            self.observe('first', 'tapped', 0)
            self.emit('assert_stack')
        else:
            self.activate_fight('first', 'spare')
            self.observe('second', 'tapped', 0)
            if self.mode == 'grant_target_lost':
                self.granted_action('second', False)
                self.cast('answer', ('spare',), stack=2)
                self.resolve('answer')
                self.cards['spare']['zone'] = 'hand'
            else:
                self.resolve()
                self.fight('first', 'spare')
            self.observe('first', 'tapped', 1)
            self.granted_action('first', False)
            self.packet()

        if 'second' in targets:
            self.activate_fight('second', 'duelist_two')
            self.resolve()
            self.fight('second', 'duelist_two')
            self.observe('second', 'tapped', 1)
            self.granted_action('second', False)
            self.packet()

        self.next_turn()
        self.next_turn()
        for recipient in ('first', 'second'):
            self.observe(recipient, 'sick', 0)
            self.observe(recipient, 'tapped', 0)
            self.granted_action(recipient, False)
        self.packet()

    def tactics(self):
        if self.mode in GRANTED_MODES:
            self.granted_tactics()
            return
        targets = {'zero': (), 'one': ('first',), 'short_color': ('first',)}
        targets = targets.get(self.mode, ('first', 'second'))
        price = cost_from_oracle(self.oracle[self.name])
        price[3] += max(0, len(targets) - 1)
        funds = list(price)
        if self.mode == 'short_color':
            funds[0] += 2
        self.cast('source', targets, fee=price, budget=funds, done=self.mode == 'zero')
        if self.mode == 'lose_one':
            self.bounce_response('first')
        else:
            self.resolve('source')
        for target in targets:
            if self.cards[target]['zone'] == 'battlefield':
                self.cards[target]['boost'] += 1
        self.packet()
        if self.mode == 'short_color':
            self.cast('sink', funded=True)
            self.resolve('sink')
            self.cards['sink']['zone'] = 'battlefield'
            self.packet()
        self.next_turn()
        self.packet()

    def feed(self):
        self.cast('source', ('second', 'first'))
        if self.mode == 'lose_own':
            self.bounce_response('first')
        elif self.mode == 'lose_foe':
            self.bounce_response('second')
        else:
            self.resolve('source')
            self.fight()
            if self.mode == 'kill_now':
                self.life[self.owner] += 3
        self.packet()
        if 'finisher' not in self.cards:
            return
        if self.mode == 'expired':
            self.next_turn()
            self.packet()
        target = 'spare' if self.mode == 'other_death' else 'second'
        self.cast('finisher', (target,))
        self.resolve('finisher')
        self.cards[target]['zone'] = 'graveyard'
        if self.mode in ('delayed', 'lose_own'):
            self.life[self.owner] += 3
        self.packet()

    def dissension(self):
        self.emit('begin_attackers')
        self.emit('attack', 'raider', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked = {'no_blockers': (), 'one_blocker': ('first',)}.get(self.mode, ('first', 'second'))
        for ref in blocked:
            self.emit('block', ref, 'raider', value=1)
        self.emit('advance_combat')
        for ref in ('first', 'second', 'spare'):
            self.observe(ref, 'blocking_count', int(ref in blocked))
        if len(blocked) < 2:
            self.fund(self.owner, cost_from_oracle(self.oracle[self.name]))
            self.emit('can_cast', 'source', value=0)
            self.packet()
            return
        self.cast('source', ('first', 'second'))
        if self.mode == 'lose_blocker':
            self.bounce_response('second')
        else:
            self.resolve('source')
            self.fight()
        self.packet()
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.cards['first']['zone'] = 'graveyard'
        self.cards['raider']['damage'] = 3
        self.packet()

    def build(self):
        self.emit('turn', player=self.owner)
        for actor in (0, 1):
            self.wallet(actor, self.mana[actor], setting=True)
        self.emit('refresh')
        {FEUD: self.feud, TACTICS: self.tactics, FEED: self.feed, DISSENSION: self.dissension}[self.csvid]()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact two-target Fight primary CSVID required')
    cases = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        board = Board(oracle[name], int(player), mode, rows, oracle)
        board.build()
        cases.append(dict(name=f'FightTwoTargetGrantRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2633, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'],
                                       untested_components=list(GRANTED_GAPS if mode in GRANTED_MODES else GAPS.get(csvid, ())))))
    return cases
