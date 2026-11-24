"""Real cumulative upkeep with card, coin, counter, mana, and token receipts."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from cumulative_upkeep_payment_recipes import REMINDER
from recipes import card, cost_from_oracle, step


FAMILY = 'cumulative_upkeep_special_payment_and_riders'
PROFILE = Path(__file__).with_name('data') / 'cumulative_upkeep_special_payment_and_riders_wave8_profiles.json'
KNOWLEDGE, HEART, LASH, RIDERS, MINOTAUR, ANCIENT = 2929, 2989, 2369, 2376, 8393, 8442
CLAUSES = {
    KNOWLEDGE: ('Cumulative upkeep {1}' + REMINDER,
                'When this enchantment enters, look at the top ten cards of your library, then exile any number of '
                'them and put the rest back on top of your library in any order.',
                'When this enchantment leaves the battlefield, shuffle your library.'),
    HEART: ('Cumulative upkeep {2}' + REMINDER,
            "When a player doesn't pay this enchantment's cumulative upkeep, this enchantment deals X damage to "
            'target player or planeswalker and each creature that player or that planeswalker\'s controller controls, '
            'where X is twice the number of age counters on this enchantment minus 2.'),
    LASH: ('Cumulative upkeep—Exile the top card of your library.' + REMINDER,
           "When a player doesn't pay this enchantment's cumulative upkeep, that player exiles all cards from their library.",
           'Exile the top card of your library: Prevent the next 1 damage that would be dealt to you this turn.'),
    MINOTAUR: ('Cumulative upkeep—Flip a coin.' + REMINDER,
               'Whenever you win a coin flip, this creature deals 1 damage to any target.',
               "Whenever you lose a coin flip, this creature deals 1 damage to any target of an opponent's choice."),
    ANCIENT: ('Trample', 'Cumulative upkeep—Put a +1/+1 counter on a creature an opponent controls.' + REMINDER),
    RIDERS: ('Cumulative upkeep—Have an opponent create a 1/1 red Survivor creature token.' + REMINDER,
             'Trample; rampage 1 (Whenever this creature becomes blocked, it gets +1/+1 until end of turn for each '
             'creature blocking it beyond the first.)'),
}
MODES = {KNOWLEDGE: ('paid_twice', 'decline_second'), HEART: ('foe', 'self', 'zero', 'walker'),
         LASH: ('paid_then_short', 'decline', 'reactive', 'proactive'), RIDERS: ('one_block', 'two_block', 'decline'),
         MINOTAUR: ('heads', 'tails', 'scaled', 'decline'),
         ANCIENT: ('repeat_recipient', 'trample', 'decline', 'no_opposing_creature')}
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZONES = {value: zone for zone, value in ZONE.items()}
DECK = ('Mountain', 'Plains', 'Island', 'Forest', 'Swamp', 'Shock', 'Giant Growth',
        'Hill Giant', 'Wind Drake', 'Grizzly Bears', 'Squire')


WITNESSES = ('Enormous Baloth', 'Forest', 'Giant Growth', 'Grizzly Bears', 'Hill Giant', 'Island', 'Jace Beleren', 'Mountain', 'Plains', 'Shock', 'Squire', 'Swamp', 'Wind Drake')
GAPS = {
    KNOWLEDGE: ('AI chooses its library ordering; the case observes its exact membership and unaffected lower suffix.',),
    RIDERS: ('The Survivor identity and visible token traits are observed; no direct Survivor subtype or multilane damage-order receipt.',),
    MINOTAUR: ('The fixture exercises both calls against real seeded flips and observes one paid damage per flip. '
               'It does not identify the win/loss chooser seat independently of the random outcome.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in CLAUSES or record.get('oracle_text') != '\n'.join(CLAUSES[csvid])
            or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('exact normal whole-record special cumulative upkeep required')
    return dict(csvid=csvid, mana=cost_from_oracle(record))


def variants(shape):
    csvid = shape['csvid']
    result = [f'{mode}_{actor}' for actor in (0, 1) for mode in MODES[csvid]]
    if csvid == KNOWLEDGE:
        result.insert(len(MODES[csvid]), 'exile_order_0')
    return result


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported cumulative-upkeep variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('exact pinned cumulative-upkeep primary required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed cumulative-upkeep declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses', 'token_registry'} or set(pins['witnesses']) != set(WITNESSES)
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != CLAUSES.keys()
            or set(pins['token_registry']) != {'Survivor'}):
        raise ValueError('exact cumulative-upkeep primary and witness scope required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('cumulative-upkeep Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented cumulative-upkeep primary required')
                classify(oracle[name], row['csvid'])
    if not exact(rows.get('Survivor'), pins['token_registry']['Survivor']):
        raise ValueError('exact Survivor token registry record required')


class Board:
    def __init__(self, record, csvid, actor, mode, rows, oracle):
        self.record, self.csvid, self.actor, self.foe, self.mode = record, csvid, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.age, self.tokens, self.turn, self.shuffles = 0, 0, actor, [0, 0]
        self.add('source', actor, record['name'], 'hand')
        if csvid == ANCIENT:
            self.add('own_bear', actor, 'Grizzly Bears', 'battlefield')
            if mode != 'no_opposing_creature':
                self.add('foe_bear', self.foe, 'Grizzly Bears', 'battlefield')
        if csvid == MINOTAUR and mode in ('heads', 'tails'):
            self.add('foe_bear', self.foe, 'Grizzly Bears', 'battlefield')
        if csvid == KNOWLEDGE:
            self.library = {actor: list(DECK), self.foe: ['Island', 'Plains']}
        elif csvid == LASH:
            self.library = {actor: list(DECK[:5]), self.foe: ['Island', 'Plains']}
        else:
            self.library = {seat: ['Island', 'Plains'] for seat in (0, 1)}
        for seat, names in self.library.items():
            for index, name in enumerate(names):
                self.add(f'd_{seat}_{index}', seat, name, 'library')
        self.lands = []
        if csvid in (KNOWLEDGE, HEART):
            for i in range(2 if csvid == KNOWLEDGE else 4):
                ref = f'mana_{i}'
                self.add(ref, actor, 'Mountain', 'battlefield')
                self.lands.append(ref)
        if csvid == HEART:
            for seat in (0, 1):
                self.add(f'bear_{seat}', seat, 'Grizzly Bears', 'battlefield')
                self.add(f'baloth_{seat}', seat, 'Enormous Baloth', 'battlefield')
            if mode == 'walker':
                self.add('walker', self.foe, 'Jace Beleren', 'hand')
        if csvid == LASH and mode in ('reactive', 'proactive'):
            self.add('first_shock', self.foe, 'Shock', 'hand')
            self.add('second_shock', self.foe, 'Shock', 'hand')

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
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        if op == 'choose_mana_source':
            self.steps.append(step(op, ref=ref))
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
        for color, amount in enumerate(pool):
            self.emit(op, player=player, color=color, value=amount)

    def receipt(self, name, seat, zone, count):
        self.emit('assert_zone_multiplicity', player=seat, zone=ZONE[zone], index=self.rows[name]['csvid'], value=count)

    def receipts(self, *, ordered=True):
        self.emit('assert_stack')
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            for zone, zone_id in ZONE.items():
                names = self.library[seat] if zone == 'library' else [
                    card['name'] for card in self.cards.values() if card['player'] == seat and card['zone'] == zone
                ]
                if zone == 'battlefield' and seat == self.foe:
                    names = names + ['Survivor'] * self.tokens
                counts = Counter(names)
                self.emit('assert_zone', player=seat, zone=zone_id, value=len(names))
                for name in sorted({item['name'] for item in self.cards.values()} | ({'Survivor'} if self.csvid == RIDERS else set())):
                    self.receipt(name, seat, zone, counts[name])
                if zone == 'library':
                    own_reordered = seat == self.actor and (not ordered or self.shuffles[seat] > 0)
                    for index, name in enumerate(names):
                        if not own_reordered or (not self.shuffles[seat] and index >= 10):
                            self.emit('assert_zone_card', player=seat, zone=zone_id, index=index,
                                      value=self.rows[name]['csvid'])
        if self.cards['source']['zone'] == 'battlefield':
            self.emit('assert_counter', 'source', prop='age', value=self.age)

    def cast(self, ref, destination, prepare=None, *, picked=None, protect=False):
        name, seat = self.cards[ref]['name'], self.cards[ref]['player']
        self.mana('set_mana', seat, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        if picked is not None:
            self.emit('choose_player', player=seat, value=picked)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', seat, [0] * 6)
        if prepare:
            prepare()
        if protect:
            self.emit('choose_priority', 'source')
            self.emit('choose_damage_player', player=self.actor, value=self.actor)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[ref]['zone'] = destination

    def go_next(self):
        self.turn = 1 - self.turn
        self.emit('advance_turn')
        self.emit('untap_step', player=self.turn)

    def step_upkeep(self, *, pay=None, victim=None, fee=None, payment_choices=None):
        self.go_next()
        paid = gone = False
        needed = 0
        if pay is not None:
            assert self.turn == self.actor and self.cards['source']['zone'] == 'battlefield'
            self.age += 1
            needed = self.age * (fee or 1)
            if fee:
                affordable = len(self.lands) >= needed
            elif self.csvid == LASH:
                affordable = len(self.library[self.actor]) >= needed
            elif self.csvid == ANCIENT:
                affordable = 'foe_bear' in self.cards and self.cards['foe_bear']['zone'] == 'battlefield'
            else:
                affordable = True
            if affordable or self.actor == 1:
                self.emit('choose_dialog', player=self.actor, value=int(not pay or not affordable))
            paid = pay and affordable
            if paid and fee and self.actor == 0:
                for ref in self.lands[:needed]:
                    self.emit('choose_mana_source', ref)
            if paid and payment_choices:
                payment_choices(needed)
            gone = not paid
            if self.csvid == HEART and gone:
                if victim == 'walker':
                    self.emit('choose_target', 'walker', player=self.actor)
                else:
                    self.emit('choose_player', player=self.actor, value=victim)
        self.emit('upkeep_step', player=self.turn)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('turn', player=self.turn)
        self.emit('refresh')
        self.mana('assert_mana', self.actor, [0] * 6)
        self.mana('assert_mana', self.foe, [0] * 6)
        if pay is None:
            self.receipts(ordered=self.csvid != KNOWLEDGE or self.actor == 0)
            return False
        if fee and (self.actor == 0 or not paid or needed == len(self.lands)):
            for index, ref in enumerate(self.lands):
                self.observe(ref, 'tapped', int(paid and index < needed))
        if paid and self.csvid == LASH:
            for _ in range(needed):
                self.exile_top()
        if paid and self.csvid == RIDERS:
            self.tokens += needed
        if gone:
            self.cards['source']['zone'] = 'graveyard'
            if self.csvid == KNOWLEDGE:
                self.shuffles[self.actor] += 1
            if self.csvid == LASH:
                while self.library[self.actor]:
                    self.exile_top()
        return gone

    def owner_upkeep(self, should_pay, *, victim=None, fee=None, payment_choices=None):
        self.step_upkeep()
        return self.step_upkeep(pay=should_pay, victim=victim, fee=fee, payment_choices=payment_choices)

    def exile_named(self, name):
        item = next(c for c in self.cards.values() if c['player'] == self.actor and c['zone'] == 'library'
                    and c['name'] == name)
        item['zone'] = 'exile'
        self.library[self.actor].remove(name)

    def exile_top(self):
        self.exile_named(self.library[self.actor][0])

    def pick(self, name):
        self.emit('choose_zone_card', player=self.actor, value=self.rows[name]['csvid'])

    def knowledge_choices(self):
        if self.actor != 0:
            return
        if self.mode == 'exile_order':
            self.pick('Plains')
            self.pick('Wind Drake')
        self.emit('choose_zone', player=0, value=-1)
        self.pick('Hill Giant')
        self.emit('choose_zone', player=0, value=-1)

    def knowledge(self):
        self.cast('source', 'battlefield', self.knowledge_choices)
        if self.actor == 0:
            if self.mode == 'exile_order':
                self.exile_named('Plains')
                self.exile_named('Wind Drake')
            self.library[self.actor].remove('Hill Giant')
            self.library[self.actor].insert(0, 'Hill Giant')
        self.emit('assert_shuffle_count', player=self.actor, value=0)
        self.receipts(ordered=self.actor == 0)
        self.owner_upkeep(True, fee=1)
        self.receipts(ordered=self.actor == 0)
        self.owner_upkeep(self.mode == 'paid_twice', fee=1)
        self.receipts(ordered=self.actor == 0)
        if self.mode == 'paid_twice':
            self.owner_upkeep(True, fee=1)
            self.receipts(ordered=False)
        self.emit('assert_shuffle_count', player=self.actor, value=1)
        self.emit('assert_shuffle_count', player=self.foe, value=0)

    def heart(self):
        if self.mode == 'walker':
            self.emit('turn', player=self.foe)
            self.turn = self.foe
            self.cast('walker', 'battlefield')
            self.emit('assert_counter', 'walker', prop='loyalty', value=3)
            self.go_next()
            self.emit('turn', player=self.actor)
        self.cast('source', 'battlefield')
        self.receipts()
        if self.mode != 'zero':
            assert not self.owner_upkeep(True, fee=2)
            for seat in (0, 1):
                self.observe(f'bear_{seat}', 'damage', 0)
                self.observe(f'baloth_{seat}', 'damage', 0)
            self.receipts()
        if self.mode == 'self':
            assert not self.owner_upkeep(True, fee=2)
            self.receipts()
        target = self.actor if self.mode == 'self' else self.foe
        gone = self.owner_upkeep(False, fee=2, victim='walker' if self.mode == 'walker' else target)
        assert gone
        damage = 2 * self.age - 2
        if self.mode == 'walker':
            self.emit('assert_counter', 'walker', prop='loyalty', value=3 - damage)
        else:
            self.life[target] -= damage
        if damage:
            self.cards[f'bear_{target}']['zone'] = 'graveyard'
        self.receipts()
        self.observe(f'baloth_{target}', 'damage', damage)
        self.observe(f'baloth_{1-target}', 'damage', 0)
        self.observe(f'bear_{1-target}', 'damage', 0)

    def lash(self):
        self.cast('source', 'battlefield')
        self.receipts()
        if self.mode in ('proactive', 'reactive'):
            proactive = self.mode == 'proactive'
            self.go_next()
            self.emit('turn', player=self.foe)
            if proactive:
                self.emit('can_activate', 'source', value=1)
                self.emit('activate', 'source', value=1)
                self.emit('assert_stack', value=1)
                self.emit('resolve')
                self.emit('assert_choices_consumed')
                self.exile_top()
                self.receipts()
            self.cast('first_shock', 'graveyard', picked=self.actor, protect=not proactive)
            if not proactive:
                self.exile_top()
            self.life[self.actor] -= 1
            self.receipts()
            self.cast('second_shock', 'graveyard', picked=self.actor)
            self.life[self.actor] -= 2
            self.receipts()
        elif self.mode == 'decline':
            self.owner_upkeep(False)
            self.receipts()
        else:
            for _ in range(3):
                self.owner_upkeep(True)
                self.receipts()

    def observe_tokens(self):
        self.receipt('Survivor', self.actor, 'battlefield', 0)
        self.receipt('Survivor', self.foe, 'battlefield', self.tokens)
        if self.tokens:
            token = self.rows['Survivor']['csvid']
            for prop, value in (('token', 1), ('color', 16), ('power', 1), ('toughness', 1), ('tapped', 0)):
                self.emit('assert_matching_permanents', player=self.foe, index=token, prop=prop, value=value)

    def riders(self):
        self.cast('source', 'battlefield')
        self.receipts()
        self.owner_upkeep(True)
        self.observe_tokens()
        self.receipts()
        twice = self.mode != 'decline'
        self.owner_upkeep(twice)
        self.observe_tokens()
        self.receipts()
        if not twice:
            return
        blocks = 2 if self.mode == 'two_block' else 1
        for i in range(blocks):
            self.emit('bind_permanent_copy', f'token_{i}', player=self.foe, index=i, value=self.rows['Survivor']['csvid'])
        self.observe('source', 'power', 3)
        self.observe('source', 'toughness', 4)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for i in range(blocks):
            self.emit('block', f'token_{i}', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'power', 3 + blocks - 1)
        self.observe('source', 'toughness', 4 + blocks - 1)
        if blocks == 2:
            for i in range(blocks):
                self.observe(f'token_{i}', 'blocking_count', 1)
        else:
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.emit('assert_choices_consumed')
            self.tokens -= 1
            self.life[self.foe] -= 2
            self.observe('source', 'damage', 1)
            self.receipts()

    def minotaur(self):
        self.cast('source', 'battlefield')
        self.observe('source', 'power', 3)
        self.observe('source', 'toughness', 3)
        self.receipts()
        if self.mode == 'decline':
            self.owner_upkeep(False)
            self.receipts()
            return

        # Same seed and board for both face calls. Either chooser may legally pick
        # the same recipient, so an unknown RNG offset cannot invalidate the input.
        if self.mode in ('heads', 'tails'):
            def creature_target(count):
                assert count == 1
                self.emit('choose_dialog', player=self.actor, value=int(self.mode == 'tails'))
                self.emit('choose_target', 'foe_bear', player=-1)

            self.owner_upkeep(True, payment_choices=creature_target)
            self.observe('foe_bear', 'damage', 1)
            self.observe('source', 'damage', 0)
            self.receipts()
            return

        for expected_age in (1, 2):
            def player_targets(count):
                assert count == expected_age
                for recipient in (self.foe, self.actor)[:count]:
                    self.emit('choose_dialog', player=self.actor, value=0)
                    self.emit('choose_player', player=-1, value=recipient)
                    self.life[recipient] -= 1

            self.owner_upkeep(True, payment_choices=player_targets)
            self.receipts()

    def ancient(self):
        self.cast('source', 'battlefield')
        self.observe('source', 'power', 5)
        self.observe('source', 'toughness', 5)
        self.receipts()
        if self.mode == 'no_opposing_creature':
            self.owner_upkeep(True)
            self.observe('own_bear', 'plus1_counters', 0)
            self.receipts()
            return
        if self.mode == 'decline':
            self.owner_upkeep(False)
            self.observe('own_bear', 'plus1_counters', 0)
            self.observe('foe_bear', 'plus1_counters', 0)
            self.receipts()
            return

        def bear_payment(count):
            for _ in range(count):
                self.emit('choose_target', 'foe_bear', player=self.actor)

        def observe_bears(expected_count):
            self.observe('own_bear', 'plus1_counters', 0)
            self.observe('foe_bear', 'plus1_counters', expected_count)
            self.observe('foe_bear', 'power', 2 + expected_count)
            self.observe('foe_bear', 'toughness', 2 + expected_count)
            self.observe('source', 'power', 5)
            self.observe('source', 'toughness', 5)
            self.receipts()

        self.owner_upkeep(True, payment_choices=bear_payment)
        observe_bears(1)
        if self.mode == 'repeat_recipient':
            self.owner_upkeep(True, payment_choices=bear_payment)
            observe_bears(3)
            return
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'foe_bear', 'source', value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.cards['foe_bear']['zone'] = 'graveyard'
        self.life[self.foe] -= 2
        self.observe('source', 'damage', 3)
        self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        for seat in (0, 1):
            self.mana('set_mana', seat, [0] * 6)
        {KNOWLEDGE: self.knowledge, HEART: self.heart, LASH: self.lash, RIDERS: self.riders,
         MINOTAUR: self.minotaur, ANCIENT: self.ancient}[self.csvid]()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact cumulative-upkeep primary CSVID required')
    cases = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        game = Board(oracle[name], csvid, int(player), mode, rows, oracle)
        game.build()
        cases.append(dict(
            name=f'CumulativeUpkeepSpecialPaymentAndRidersRecipe_{csvid}_{variant}', card=name,
            fixture='empty_main', seed=2503, setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=list(GAPS.get(csvid, ()))),
        ))
    return cases
