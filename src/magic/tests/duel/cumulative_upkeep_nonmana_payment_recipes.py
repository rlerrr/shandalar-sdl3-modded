"""Nonmana cumulative upkeep payments and their production riders."""

from collections import Counter
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact

FAMILY = 'cumulative_upkeep_nonmana_payment'
PROFILE = Path(__file__).with_name('data') / 'cumulative_upkeep_nonmana_payment_wave8_profiles.json'
REMINDER = (' (At the beginning of your upkeep, put an age counter on this permanent, '
            'then sacrifice it unless you pay its upkeep cost for each age counter on it.)')
CARDS = {
    2924: ('Put a -1/-1 counter on this creature.', '', '',
           ('pay_one', 'pay_twice', 'pay_to_lethal', 'decline_after_one')),
    8344: ('Add {R}.', '', '', ('pay_twice', 'decline_after_one', 'natural_drain')),
    8425: ('Sacrifice a creature.', '', '', ('pay_twice', 'decline_after_one', 'short_two')),
    8467: ('Discard a card.', 'Flying\n',
           '\nWhen this creature dies, draw a card for each age counter on it.',
           ('pay_twice_flying', 'decline_after_one', 'short_two')),
    1949: ('Pay 2 life.', '', '\nWhen this land enters, sacrifice a land.'
           "\nCreatures you control can't attack.\nPrevent all damage that would be dealt to you.",
           ('pay_twice_protect', 'decline_releases')),
    2067: ('Sacrifice a land.', 'Trample\nThis creature enters tapped.\n', '',
           ('pay_twice_trample', 'decline_after_one', 'short_two')),
    3027: ('Draw a card.', '', '\nAt the beginning of your end step, sacrifice a land and discard your hand.',
           ('pay_twice_end_step', 'decline_after_one', 'short_two')),
    8469: ('An opponent gains 1 life.', 'Defender, flying\n', '',
           ('pay_twice_defend', 'decline_after_one', 'prevent_gain')),
}
ZONES = ('hand', 'battlefield', 'library', 'graveyard')
CAST_POOL = [0] * 6
BEARS = 'Grizzly Bears'
DISCARD = 'Plains'
DRAW = 'Island'
KRUPHIX = 'Kruphix, God of Horizons'
ANTS = 'Carrion Ants'
LAND = 'Mountain'
BOLT = 'Lightning Bolt'
AIR = 'Air Elemental'
LEYLINE = 'Leyline of Punishment'
VORTEX_LIBRARY = ('Island', 'Forest', 'Swamp')
WITNESSES = {BEARS, DISCARD, DRAW, KRUPHIX, ANTS, LAND, BOLT, AIR, LEYLINE, *VORTEX_LIBRARY}
REPLAY = {2924: 'pay_twice', 8344: 'pay_twice', 8425: 'pay_twice', 8467: 'decline_after_one'}
GAPS = {
    8344: ['The full upkeep helper ends with mana drain. Actual Kruphix and Carrion Ants prove the converted amount and spend; they cannot observe the original red color.'],
    8467: ['Paid age-three discard and the AI age-three payment policy are outside the bounded cases.'],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    from recipes import cost_from_oracle

    if (csvid not in CARDS or record.get('layout') != 'normal'
            or record.get('card_faces') or record.get('selected_face')):
        raise ValueError('whole normal cumulative upkeep card required')
    cost, before, after, _ = CARDS[csvid]
    keywords = {'Cumulative upkeep'}
    for keyword in ('Flying', 'Defender', 'Trample'):
        if keyword.lower() in before.lower():
            keywords.add(keyword)
    if (record.get('oracle_text') != before + 'Cumulative upkeep—' + cost + REMINDER + after
            or set(record.get('keywords', ())) != keywords):
        raise ValueError('complete nonmana upkeep cost and printed riders required')
    if csvid == 1949 and record.get('mana_cost') == '':
        casting_cost = list(CAST_POOL)
    elif csvid == 2067 and record.get('mana_cost') == '{8}{U}{U}{U}':
        # The fixture writer stores each color separately. The shared small
        # recipe helper caps the total at ten, but this exact pool fits.
        casting_cost = [8, 0, 3, 0, 0, 0]
    else:
        casting_cost = cost_from_oracle(record)
    return dict(csvid=csvid, payment=cost, casting_cost=casting_cost)


def variants(shape):
    csvid = shape['csvid']
    modes = CARDS[csvid][3]
    if csvid in REPLAY:
        modes += ('replay',)
    return [f'{mode}_{player}' for mode in modes for player in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported nonmana upkeep cases required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('complete pinned nonmana upkeep source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('versioned whole nonmana upkeep declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('exact independent nonmana upkeep witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(CARDS):
        raise ValueError('complete nonmana upkeep source cohort required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact nonmana upkeep source or witness changed: ' + name)
            if group == 'sources':
                if not str(row.get('handler', '')).startswith('manalinkeh_card_'):
                    raise ValueError('implemented nonmana upkeep primary required')
                classify(oracle[name], row['csvid'])
    for name, printed in ((ANTS, '{1}: This creature gets +1/+1 until end of turn.'),
                          (BOLT, 'Lightning Bolt deals 3 damage to any target.'),
                          (AIR, 'Flying')):
        if oracle[name]['oracle_text'] != printed:
            raise ValueError('explicit gameplay witness changed: ' + name)
    if (oracle[KRUPHIX]['oracle_text'].splitlines()[-1]
            != 'If you would lose unspent mana, that mana becomes colorless instead.'
            or "Players can't gain life." not in oracle[LEYLINE]['oracle_text'].splitlines()):
        raise ValueError('mana retention and life prevention witnesses required')


class Duel:
    def __init__(self, row, player, mode, rows):
        self.kind, self.name = row['csvid'], row['name']
        self.player, self.enemy, self.mode, self.rows = player, 1 - player, mode, rows
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.age = 0
        self.minus = 0
        # The autonomous sacrifice scans equal-ranked lands by instance slot.
        # Put the existing Mountain first so either seat can keep the Chasm.
        ordered_chasm_lands = self.kind == 1949 and player == 1
        if ordered_chasm_lands:
            self.add('entry_land', player, LAND, 'battlefield')
        self.add('source', player, self.name, 'hand')
        if self.kind == 8344 and mode != 'natural_drain':
            self.add('keeper', player, KRUPHIX, 'battlefield')
            self.add('sink', player, ANTS, 'battlefield')
        if self.kind == 8425:
            for i in range(1 if mode == 'short_two' else 3):
                self.add(f'fodder{i}', player, BEARS, 'battlefield')
            self.add('foe_bear', self.enemy, BEARS, 'battlefield')
        if self.kind == 8467:
            for i in range(1 if mode == 'short_two' else 3):
                self.add(f'fodder{i}', player, DISCARD, 'hand')
            for i in range(3):
                self.add(f'draw{i}', player, DRAW, 'library')
            if mode == 'pay_twice_flying':
                self.add('ground', self.enemy, BEARS, 'battlefield')
        if self.kind == 1949:
            if not ordered_chasm_lands:
                self.add('entry_land', player, LAND, 'battlefield')
            self.add('ground', player, BEARS, 'battlefield')
            self.add('bolt0', self.enemy, BOLT, 'hand')
            if mode == 'decline_releases':
                self.add('bolt1', self.enemy, BOLT, 'hand')
        if self.kind == 2067:
            for i in range(1 if mode == 'short_two' else 3):
                self.add(f'fodder{i}', player, LAND, 'battlefield')
            self.add('enemy_land', self.enemy, LAND, 'battlefield')
            if mode == 'pay_twice_trample':
                self.add('ground', self.enemy, BEARS, 'battlefield')
        if self.kind == 3027:
            for i in range(3):
                self.add(f'fodder{i}', player, LAND, 'battlefield')
            self.add('enemy_land', self.enemy, LAND, 'battlefield')
            self.add('junk', player, DISCARD, 'hand')
            self.add('enemy_hand', self.enemy, DISCARD, 'hand')
            cards = VORTEX_LIBRARY[:2] if mode == 'short_two' else VORTEX_LIBRARY
            for i, name in enumerate(cards):
                self.add(f'draw{i}', player, name, 'library')
        if self.kind == 8469 and mode == 'pay_twice_defend':
            self.add('flyer', self.enemy, AIR, 'battlefield')
        if self.kind == 8469 and mode == 'prevent_gain':
            self.add('leyline', self.enemy, LEYLINE, 'battlefield')

    def add(self, ref, player, name, zone):
        from recipes import card

        self.cards[ref] = [player, name, zone]
        self.setup.append(card(name, ref, player, zone))

    def move(self, ref, zone):
        self.cards[ref][2] = zone

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        from recipes import step

        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        if op == 'play_land':
            self.steps.append(step(op, ref=ref, value=value))
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

    def pool(self, op, player, values):
        for color, amount in enumerate(values):
            self.emit(op, player=player, value=amount, color=color)

    def resources(self, retained=0):
        assert retained >= 0
        self.pool('assert_mana', self.player, [retained, 0, 0, 0, 0, 0])
        self.pool('assert_mana', self.enemy, CAST_POOL)

    def expect_card(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def receipts(self):
        self.emit('assert_stack', value=0)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            names = sorted({name for owner, name, _ in self.cards.values() if owner == player})
            for index, zone in enumerate(ZONES):
                contents = [name for owner, name, where in self.cards.values()
                            if (owner, where) == (player, zone)]
                totals = Counter(contents)
                self.emit('assert_zone', player=player, zone=index, value=len(contents))
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=index,
                              index=self.rows[name]['csvid'], value=totals[name])
        if self.cards['source'][2] == 'battlefield':
            self.expect_card('source', 'in_play', 1)
            self.emit('assert_counter', 'source', prop='age', value=self.age)
            if self.kind == 2924:
                self.emit('assert_counter', 'source', prop='minus1', value=self.minus)
                for prop in ('power', 'toughness'):
                    self.expect_card('source', prop, 9 - self.minus)

    def present(self):
        self.emit('present_ai_dialogs', value=1)
        self.emit('discard_policy', value=1)

    def start(self, cost):
        self.emit('turn', player=self.player, value=0)
        self.pool('set_mana', self.enemy, CAST_POOL)
        self.pool('set_mana', self.player, cost)
        self.emit('refresh')
        if self.kind == 1949:
            self.present()
            if self.player == 0:
                self.emit('choose_target', 'entry_land', player=self.player)
            self.emit('play_land', 'source', value=1)
            self.emit('resolve')
            self.move('entry_land', 'graveyard')
        else:
            self.emit('cast', 'source', value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=1)
            self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.move('source', 'battlefield')
        self.resources()
        self.receipts()
        if self.kind == 2067:
            self.expect_card('source', 'tapped', 1)
        self.present()

    def start_upkeep(self, player):
        self.advance_turn(player)
        self.emit('untap_step', player=player)
        self.resources()
        self.receipts()

    def finish_upkeep(self, player):
        self.emit('upkeep_step', player=player)
        self.emit('assert_choices_consumed')
        self.emit('refresh')

    def advance_turn(self, incoming):
        vortex_end = (self.kind == 3027 and incoming == self.enemy
                      and self.cards['source'][2] == 'battlefield')
        if vortex_end:
            lands = self.fodder('battlefield')
            if lands:
                if self.player == 0:
                    self.emit('choose_target', lands[0], player=self.player)
                self.move(lands[0], 'graveyard')
            for ref, (player, _, zone) in self.cards.items():
                if player == self.player and zone == 'hand':
                    self.move(ref, 'graveyard')
        self.emit('advance_turn')
        if vortex_end:
            self.emit('assert_choices_consumed')

    def fodder(self, zone):
        return [ref for ref, (owner, _, where) in self.cards.items()
                if ref.startswith('fodder') and (owner, where) == (self.player, zone)]

    def die(self):
        self.move('source', 'graveyard')
        if self.kind == 8467:
            for i in range(self.age):
                self.move(f'draw{i}', 'hand')

    def upkeep_choices(self, decline):
        pl, n = self.player, self.age
        if self.kind == 8425:
            payment = self.fodder('battlefield')
            payable = len(payment) + 1 >= n  # The source itself is eligible.
        elif self.kind == 8467:
            payment = self.fodder('hand')
            payable = len(payment) >= n
        elif self.kind == 2067:
            payment = self.fodder('battlefield')
            payable = len(payment) >= n
        elif self.kind == 3027:
            payment = [ref for ref, (_, _, zone) in self.cards.items() if zone == 'library']
            payable = len(payment) >= n
        elif self.kind == 8469:
            payment = []
            payable = 'leyline' not in self.cards
        else:
            payment = []
            payable = True
        pay = payable and not decline
        # The same DIALOG auto-select behavior is covered by the existing
        # cumulative upkeep driver for the sole legal human Decline.
        if payable or pl == 1:
            self.emit('choose_dialog', player=pl, value=int(not pay))
        if not pay:
            self.die()
        if pay and self.kind in (8425, 8467, 2067, 3027):
            assert len(payment) >= n
            for ref in payment[:n]:
                if self.kind in (8425, 2067) and pl == 0:
                    self.emit('choose_target', ref, player=pl)
                if self.kind == 8467 and pl == 1:
                    self.emit('choose_dialog', player=pl, value=0)
                self.move(ref, 'hand' if self.kind == 3027 else 'graveyard')
        if pay and self.kind == 2924:
            self.minus += n
            if self.minus >= 9:
                self.die()
        if pay and self.kind == 1949:
            self.life[pl] -= 2 * n
        if pay and self.kind == 8469:
            self.life[self.enemy] += n
        return pay

    def spend_retained(self):
        for i in range(self.age):
            self.emit('can_activate', 'sink', value=1)
            self.emit('activate', 'sink', value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=1)
            self.emit('resolve')
            self.emit('refresh')
            self.expect_card('sink', 'power', 1 + i)
            self.resources(self.age - i - 1)
        self.emit('can_activate', 'sink', value=0)

    def visit(self, decline=False):
        self.start_upkeep(self.enemy)
        self.finish_upkeep(self.enemy)
        self.resources()
        self.receipts()
        self.start_upkeep(self.player)
        self.age += 1
        if self.kind == 8467 and self.player == 0:
            self.emit('discard_policy', value=1)  # Reset only the witness counter.
        pay = self.upkeep_choices(decline)
        self.finish_upkeep(self.player)
        if self.kind == 8467 and self.player == 0:
            self.emit('assert_discard_choices', value=self.age if pay else 0)
        retained = self.age if self.kind == 8344 and pay and 'keeper' in self.cards else 0
        self.resources(retained)
        self.receipts()
        if retained:
            self.expect_card('keeper', 'in_play', 1)
            self.expect_card('sink', 'power', 0)
            self.spend_retained()
            self.receipts()

    def flying_combat(self):
        self.emit('turn', player=self.player, value=0)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground', 'source', value=0)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        self.life[self.enemy] -= 4
        self.receipts()

    def chasm_bolt(self, index, prevented):
        ref = f'bolt{index}'
        self.pool('set_mana', self.enemy, [0, 0, 0, 0, 1, 0])
        self.emit('refresh')
        self.emit('choose_player', player=self.enemy, value=self.player)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.emit('resolve')
        self.emit('refresh')
        self.move(ref, 'graveyard')
        if not prevented:
            self.life[self.player] -= 3
        self.resources()
        self.receipts()

    def combat(self, attacker, *, allowed=True, blocker=None, attacking_player=None):
        active = self.player if attacking_player is None else attacking_player
        self.emit('turn', player=active, value=0)
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=int(allowed))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', blocker, attacker, value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        if self.kind == 8469 and blocker:
            self.expect_card('source', 'damage', 4)
            self.expect_card('flyer', 'damage', 1)
        self.emit('end_combat')

    def build(self, cost, *, replay=False):
        self.start(cost)
        self.visit()
        if replay:
            cut = len(self.steps)
        if self.mode == 'pay_to_lethal':
            for _ in range(3):
                self.visit()
        if self.kind == 1949:
            self.chasm_bolt(0, prevented=True)
            self.combat('ground', allowed=False)
            self.receipts()
        if self.mode not in ('pay_one', 'pay_to_lethal', 'natural_drain', 'prevent_gain'):
            self.visit(decline=self.mode in ('decline_after_one', 'decline_releases'))
        if self.mode == 'pay_twice_flying':
            self.flying_combat()
        if self.mode == 'decline_releases':
            self.chasm_bolt(1, prevented=False)
            self.combat('ground')
            self.life[self.enemy] -= 2
            self.receipts()
        if self.mode == 'pay_twice_trample':
            self.expect_card('source', 'tapped', 0)
            self.combat('source', blocker='ground')
            self.move('ground', 'graveyard')
            self.life[self.enemy] -= 9
            self.receipts()
        if self.mode == 'pay_twice_end_step':
            self.advance_turn(self.enemy)
            self.resources()
            self.receipts()
        if self.mode == 'pay_twice_defend':
            self.combat('source', allowed=False)
            self.start_upkeep(self.enemy)
            self.finish_upkeep(self.enemy)
            self.combat('flyer', blocker='source', attacking_player=self.enemy)
            self.receipts()
        if replay:
            from recipes import step

            suffix = self.steps[cut:]
            self.steps[cut:] = ([step('capture')] + suffix
                                + [step('record_state'), step('ai_roundtrip'), step('restore')]
                                + deepcopy(suffix) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered nonmana upkeep primary CSVID required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        replay = mode == 'replay'
        game = Duel(rows[name], int(actor), REPLAY[csvid] if replay else mode, rows)
        game.build(shape['casting_cost'], replay=replay)
        cases.append(dict(
            name=f'CumulativeUpkeepNonmanaPaymentRecipe_{csvid}_{variant}', card=name,
            fixture='empty_main', seed=2503, setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, [])),
        ))
    return cases



