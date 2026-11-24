"""Actual paid Suspend countdowns and whole mass-effect outcomes."""
from collections import Counter
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


FAMILY = 'suspend_mass_effects'
PROFILE = Path(__file__).with_name('data') / 'suspend_mass_effects_wave8_profiles.json'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZONE_NAMES = {number: name for name, number in ZONE.items()}
DICHO, HYPER, LIVING, BALANCE, WHEEL = 8767, 8577, 8596, 8646, 8730
CARDS = {
    DICHO: (3, '{1}{U}{U}', 'search', 'For each tapped nonland permanent target opponent controls, search that '
            "player's library for a card with the same name as that permanent. Put those cards onto the battlefield "
            'under your control, then that player shuffles.\n'
            'Suspend 3—{1}{U}{U} (Rather than cast this card from your hand, you may pay {1}{U}{U} and exile it '
            'with three time counters on it. At the beginning of your upkeep, remove a time counter. When the '
            'last is removed, you may cast it without paying its mana cost.)'),
    HYPER: (3, '{1}{G}{G}', 'offer', 'Suspend 3—{1}{G}{G}\nStarting with you, each player may put an artifact, '
            'creature, enchantment, or land card from their hand onto the battlefield. Repeat this process '
            'until no one puts a card onto the battlefield.'),
    LIVING: (3, '{2}{B}{B}', 'exchange', 'Suspend 3—{2}{B}{B}\nEach player exiles all creature cards from their '
             'graveyard, then sacrifices all creatures they control, then puts all cards they exiled this way '
             'onto the battlefield.'),
    BALANCE: (6, '{W}', 'balance', 'Suspend 6—{W}\nEach player chooses a number of lands they control equal to the '
              'number of lands controlled by the player who controls the fewest, then sacrifices the rest. '
              'Players sacrifice creatures and discard cards the same way.'),
    WHEEL: (4, '{1}{R}', 'wheel', 'Suspend 4—{1}{R} (Rather than cast this card from your hand, pay {1}{R} and '
            'exile it with four time counters on it. At the beginning of your upkeep, remove a time counter. '
            'When the last is removed, you may cast it without paying its mana cost.)\n'
            'Each player discards their hand, then draws seven cards.'),
}
COMMON = ('parked', 'countdown', 'unfunded')
EXTRAS = {DICHO: ('search_empty', 'returned_owner'), HYPER: ('one_declines', 'both_decline'),
          LIVING: ('no_grave_creatures',), BALANCE: ('choices',), WHEEL: ('empty_hand',)}

FRESH = ('wrong_color', 'first_owner')
REPLAY = {HYPER, LIVING, WHEEL}
WITNESSES = {
    'Island', 'Plains', 'Grizzly Bears', 'Hill Giant', 'Wind Drake', 'Forest', 'Unsummon',
    'Glorious Anthem', 'Mountain', "Traveler's Amulet", 'Darksteel Myr', 'Icy Manipulator',
    'Giant Growth', 'Fog', 'Shock', 'Suntail Hawk', 'Enormous Baloth', 'Giant Spider',
}
GAPS = (
    'The production Suspend trigger lacks a free-cast decline prompt. Acceptance is covered.',
    'Legacy effect slots are opaque to ordinary card references. Paid exile and actual upkeep timing observe the clock.',
)


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid not in CARDS:
        raise ValueError('supported mass-effect Suspend primary required')
    duration, payment, effect, text = CARDS[csvid]
    if (record.get('oracle_text') != text or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')
            or record.get('type_line') != 'Sorcery' or record.get('keywords') != ['Suspend']
            or record.get('mana_cost') != ('{7}{U}{U}' if csvid == DICHO else '')):
        raise ValueError('exact whole printed Suspend and mass spell required')
    return dict(csvid=csvid, duration=duration, effect=effect, mana=cost_from_oracle({'mana_cost': payment}))


def variants(shape):
    csvid = shape['csvid']
    modes = (*COMMON, *EXTRAS[csvid], *FRESH, *(('replay',) if csvid in REPLAY else ()))
    return [f'{mode}_{owner}' for mode in modes for owner in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Suspend mass variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('exact complete Suspend primary required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed mass Suspend declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != CARDS.keys()):
        raise ValueError('complete independent Suspend primaries and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Suspend primary or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Suspend primary required')
                classify(oracle[name], row['csvid'])


class Board:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.duration, _, self.effect, _ = CARDS[csvid]
        self.payment = classify(record, csvid)['mana']
        self.only = not record['mana_cost']
        self.setup, self.steps, self.cards, self.life = [], [], {}, [20, 20]
        self.deck = {seat: ['Island', 'Plains'] for seat in (0, 1)}
        if csvid == DICHO:
            self.deck[actor] = ['Grizzly Bears', 'Island', 'Plains']
            self.deck[self.foe] = ['Grizzly Bears', 'Hill Giant', 'Wind Drake', 'Forest', 'Island', 'Plains']
        elif csvid == WHEEL:
            self.deck[actor] = ['Island', 'Mountain', 'Grizzly Bears', 'Plains',
                                'Forest', 'Suntail Hawk', 'Island', 'Hill Giant']
            self.deck[self.foe] = ['Plains', 'Wind Drake', 'Forest', 'Mountain',
                                   'Plains', 'Enormous Baloth', 'Island', 'Giant Spider']
        self.add('source', actor, record['name'], 'hand')
        for seat, cards in self.deck.items():
            for index, name in enumerate(cards):
                self.add(f'deck_{seat}_{index}', seat, name, 'library')
        self.witnesses()

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
            fields['zone'] = ZONE_NAMES[zone]
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, amounts):
        for color, amount in enumerate(amounts):
            self.emit(op, player=player, color=color, value=amount)

    def suspend_from_hand(self):
        denied = self.mode in ('unfunded', 'wrong_color')
        pool = list(self.payment) if self.mode != 'unfunded' else [0] * 6
        if self.mode == 'wrong_color':
            color = next(i for i in range(1, 6) if pool[i])
            wrong = next(i for i in range(1, 6) if not pool[i])
            pool[color] -= 1
            pool[wrong] += 1
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, [0] * 6)
        self.mana('set_mana', self.actor, pool)
        self.emit('refresh')
        if not denied and not self.only and self.actor == 1:
            self.emit('choose_dialog', player=self.actor, value=1)
        self.emit('cast', 'source', value=0)
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.actor, pool if denied else [0] * 6)
        self.mana('assert_mana', self.foe, [0] * 6)
        if not denied:
            self.cards['source']['zone'] = 'exile'
        self.receipts()

    def upkeep(self, player, *, last=False):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        if last:
            self.plan_last()
        self.emit('upkeep_step', player=player)
        self.resolve()
        for seat in (0, 1):
            self.mana('assert_mana', seat, [0] * 6)
        if not last:
            self.receipts()

    def witnesses(self):
        if self.csvid == DICHO:
            self.add('bear', self.foe, 'Grizzly Bears', 'battlefield')
            self.add('giant', self.foe, 'Hill Giant', 'battlefield')
            self.add('untapped', self.foe, 'Wind Drake', 'battlefield')
            self.add('land', self.foe, 'Forest', 'battlefield')
            if self.mode == 'returned_owner':
                self.add('bounce', self.actor, 'Unsummon', 'hand')
        elif self.csvid == HYPER:
            self.add('first', self.actor, 'Grizzly Bears', 'hand')
            self.add('second', self.actor, 'Glorious Anthem', 'hand')
            self.add('other_first', self.foe, 'Mountain', 'hand')
            if self.mode != 'one_declines':
                self.add('other_second', self.foe, "Traveler's Amulet", 'hand')
        elif self.csvid == LIVING:
            if self.mode != 'no_grave_creatures':
                self.add('return_own', self.actor, 'Hill Giant', 'graveyard')
                self.add('return_foe', self.foe, 'Wind Drake', 'graveyard')
            self.add('grave_foil', self.foe, 'Mountain', 'graveyard')
            self.add('own_victim', self.actor, 'Grizzly Bears', 'battlefield')
            self.add('foe_victim', self.foe, 'Darksteel Myr', 'battlefield')
            self.add('spectator', self.actor, 'Icy Manipulator', 'battlefield')
        elif self.csvid == BALANCE:
            if self.mode == 'choices':
                self.add('extra_land', 0, 'Forest', 'battlefield')
                self.add('extra_creature', 0, 'Hill Giant', 'battlefield')
                self.add('keep_land_0', 0, 'Mountain', 'battlefield')
                self.add('keep_land_1', 1, 'Mountain', 'battlefield')
                self.add('keep_creature_0', 0, 'Grizzly Bears', 'battlefield')
                self.add('keep_creature_1', 1, 'Grizzly Bears', 'battlefield')
                for index, name in enumerate(sorted(('Giant Growth', 'Fog'), key=lambda n: self.rows[n]['csvid'])):
                    self.add('extra_hand' if index == 0 else 'keep_hand_0', 0, name, 'hand')
                self.add('keep_hand_1', 1, 'Shock', 'hand')
            else:
                self.add('extra_land', self.foe, 'Forest', 'battlefield')
                self.add('extra_creature', self.actor, 'Darksteel Myr', 'battlefield')
                self.add('extra_hand', self.foe, 'Fog', 'hand')
            self.add('spectator', self.actor, 'Icy Manipulator', 'battlefield')
        elif self.mode != 'empty_hand':
            self.add('own_discard_0', self.actor, 'Giant Growth', 'hand')
            self.add('own_discard_1', self.actor, 'Forest', 'hand')
            self.add('foe_discard', self.foe, 'Shock', 'hand')

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')

    def bind(self, ref, player, name, index=0):
        self.emit('bind_permanent_copy', ref, player=player, index=index, value=self.rows[name]['csvid'])

    def life_check(self):
        for player, expected in enumerate(self.life):
            self.emit('assert_life', player=player, value=expected)

    def final_dicho_foe_turn(self):
        if self.mode == 'search_empty':
            return
        self.emit('turn', player=self.foe)
        self.emit('begin_attackers')
        for ref in ('bear', 'giant'):
            self.emit('attack', ref, value=1)
            self.observe(ref, 'tapped', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.life[self.actor] -= 5
        self.life_check()
        self.emit('activate', 'land', value=1)
        self.resolve()
        self.observe('land', 'tapped', 1)
        self.observe('untapped', 'tapped', 0)
        self.emit('assert_mana', player=self.foe, color=3, value=1)
        self.mana('set_mana', self.foe, [0] * 6)

    def pick(self, player, ref, *, decline=False):
        self.emit('choose_optional_zone' if decline else 'choose_zone_card', player=player,
                  value=self.rows[self.cards[ref]['name']]['csvid'])

    def plan_hyper(self):
        if self.mode == 'both_decline':
            self.pick(self.actor, 'first', decline=True)
            self.pick(self.foe, 'other_first', decline=True)
        elif self.mode == 'one_declines':
            self.pick(self.actor, 'first')
            self.pick(self.foe, 'other_first', decline=True)
            self.pick(self.actor, 'second')
            # The opponent can make the same legal decline in each improving round.
            self.pick(self.foe, 'other_first', decline=True)
            self.pick(self.foe, 'other_first', decline=True)
        else:
            for owner_ref, foe_ref in (('first', 'other_first'), ('second', 'other_second')):
                self.pick(self.actor, owner_ref)
                self.pick(self.foe, foe_ref)

    def plan_last(self):
        if self.csvid == DICHO:
            self.emit('choose_player', player=self.actor, value=self.foe)
            if self.mode != 'search_empty':
                for index in (0, 1):
                    self.pick(self.actor, f'deck_{self.foe}_{index}')
        elif self.csvid == HYPER:
            self.plan_hyper()
        elif self.csvid == WHEEL and self.mode in ('countdown', 'replay'):
            self.emit('discard_policy', value=1)
        elif self.csvid == BALANCE:
            self.emit('discard_policy', value=1)
            for ref in ('extra_land', 'extra_creature'):
                if self.cards[ref]['player'] == 0:
                    self.emit('choose_target', ref, player=0)

    def receipts(self, *, shuffled=False):
        # After Dichotomancy the targeted library is checked by membership.
        # Every other library is compared in full order.
        self.emit('assert_stack')
        if shuffled:
            self.life_check()
        names = sorted({item['name'] for item in self.cards.values()})
        for seat in (0, 1):
            if not shuffled:
                self.emit('assert_life', player=seat, value=self.life[seat])
            for zone, zone_id in ZONE.items():
                items = self.deck[seat] if zone == 'library' else [
                    item['name'] for item in self.cards.values() if item['player'] == seat and item['zone'] == zone
                ]
                self.emit('assert_zone', player=seat, zone=zone_id, value=len(items))
                counts = Counter(items)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=seat, zone=zone_id,
                              index=self.rows[name]['csvid'], value=counts[name])
                if zone == 'library' and (not shuffled or seat == self.actor):
                    for index, name in enumerate(items):
                        self.emit('assert_zone_card', player=seat, zone=zone_id,
                                  index=index, value=self.rows[name]['csvid'])

    def resolve_dicho(self):
        if self.mode != 'search_empty':
            for index in (0, 1):
                chosen = self.cards[f'deck_{self.foe}_{index}']
                self.deck[self.foe].remove(chosen['name'])
                chosen.update(player=self.actor, zone='battlefield')
        self.emit('assert_shuffle_count', player=self.foe, value=1)
        self.emit('assert_shuffle_count', player=self.actor, value=0)
        self.receipts(shuffled=True)
        if self.mode == 'returned_owner':
            self.bind('copy', self.actor, 'Grizzly Bears')
            self.emit('turn', player=self.actor)
            self.observe('copy', 'in_play', 1)
            self.mana('set_mana', self.actor, cost_from_oracle(self.oracle['Unsummon']))
            self.emit('refresh')
            self.emit('choose_target', 'copy', player=self.actor)
            self.emit('cast', 'bounce', value=1)
            self.emit('assert_stack', value=1)
            self.mana('assert_mana', self.actor, [0] * 6)
            self.resolve()
            self.cards['bounce']['zone'] = 'graveyard'
            self.cards[f'deck_{self.foe}_0'].update(player=self.foe, zone='hand')
            self.receipts(shuffled=True)

    def resolve_hyper(self):
        if self.mode != 'both_decline':
            self.cards['first']['zone'] = self.cards['second']['zone'] = 'battlefield'
            if self.mode != 'one_declines':
                self.cards['other_first']['zone'] = self.cards['other_second']['zone'] = 'battlefield'
        self.receipts()
        if self.mode in ('countdown', 'replay'):
            self.bind('grown', self.actor, 'Grizzly Bears')
            self.observe('grown', 'power', 3)
            self.observe('grown', 'toughness', 3)

    def resolve_living(self):
        self.cards['own_victim']['zone'] = self.cards['foe_victim']['zone'] = 'graveyard'
        if self.mode != 'no_grave_creatures':
            self.cards['return_own']['zone'] = self.cards['return_foe']['zone'] = 'battlefield'
        self.receipts()
        self.observe('spectator', 'in_play', 1)

    def resolve_balance(self):
        for ref in ('extra_land', 'extra_creature', 'extra_hand'):
            self.cards[ref]['zone'] = 'graveyard'
        self.emit('assert_discard_choices', value=int(self.cards['extra_hand']['player'] == 0))
        self.receipts()
        self.observe('spectator', 'in_play', 1)

    def resolve_wheel(self):
        for ref, card in self.cards.items():
            if ref != 'source' and card['zone'] == 'hand':
                card['zone'] = 'graveyard'
        for seat, deck in self.deck.items():
            for index in range(7):
                self.cards[f'deck_{seat}_{index}']['zone'] = 'hand'
            del deck[:7]
        self.receipts()



    def play(self):
        self.suspend_from_hand()
        if self.mode in ('unfunded', 'wrong_color'):
            return
        if self.mode in ('parked', 'first_owner'):
            self.upkeep(self.foe)
            if self.mode == 'first_owner':
                self.upkeep(self.actor)
            return
        for visit in range(1, self.duration + 1):
            last = visit == self.duration
            self.upkeep(self.foe)
            if last and self.csvid == DICHO:
                self.final_dicho_foe_turn()
            self.upkeep(self.actor, last=last)
        self.cards['source']['zone'] = 'graveyard'
        outcome = {DICHO: self.resolve_dicho, HYPER: self.resolve_hyper, LIVING: self.resolve_living,
                   BALANCE: self.resolve_balance, WHEEL: self.resolve_wheel}
        outcome[self.csvid]()

    def build(self):
        self.play()
        if self.mode == 'replay':
            suffix = self.steps
            self.steps = ([step('capture')] + suffix + [step('record_state'), step('ai_roundtrip'), step('restore')]
                          + deepcopy(suffix) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Suspend primary CSVID required')
    cases = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        board = Board(csvid, oracle[name], int(player), mode, rows, oracle)
        board.build()
        cases.append(dict(
            name=f'SuspendMassEffectsRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2439,
            setup=board.setup, steps=board.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=list(GAPS)),
        ))
    return cases
