"""Whole entry discard-or-sacrifice clauses through actual cast and combat."""

import copy
import itertools
import json
from functools import lru_cache
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld
from defensive_creature_recipes import reviewed_payment
from hybrid_creature_recipes import available

FAMILY = 'entry_discard_payment'
PROFILE = Path(__file__).with_name('data') / 'entry_discard_payment_wave8_profiles.json'
RANDOM_RULE = 'When this creature enters, sacrifice it unless you discard a card at random.'
AVATAR_RULE = ('({B/R} can be paid with either {B} or {R}.)\nFlying\n'
               'When this creature enters, sacrifice it unless you discard two cards.')
RANDOM_CALLBACKS = {'manalinkeh_card_balduvian_horde', 'manalinkeh_card_minotaur_explorer'}
WALL, REACH, AVACYN = 'Fortified Rampart', 'Giant Spider', 'Avacyn, Angel of Hope'
PITCHES = ('Swamp', 'Forest', 'Lightning Bolt')
WITNESSES = {*PITCHES, 'Island', 'Plains', 'Mountain', 'Bribery', 'Counterspell', 'Murder', AVACYN, WALL, REACH}
COMMON = ('funded', 'empty', 'extra', 'countered', 'snapshot', 'wrong_mana', 'mana_short',
          'indestructible', 'borrowed_paid', 'borrowed_empty')
RANDOM = ('decline', 'mixed_random')
AVATAR = ('short_hand', 'all_red', 'mixed_payment', 'flying_ground', 'flying_reach')
AVATAR_HUMAN = ('choose_low', 'choose_high', 'cancel_first', 'cancel_second', 'payment_menu')
GAPS = ('The driver drains the actual entry trigger while resolving its carrier, so no separate pending trigger response is asserted.',
        'Random discard fixtures validate actual eligible-card conservation and counts without estimating a distribution.')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ')):
        raise ValueError('complete normal creature discard-payment Oracle required')
    text = record['oracle_text']
    if text == RANDOM_RULE and record['keywords'] == []:
        amount, random, mana, vectors = 1, True, cost_from_oracle(record), []
    elif text == AVATAR_RULE and record['keywords'] == ['Flying'] and record['mana_cost'] == '{B/R}' * 3:
        amount, random = 2, False
        mana, pairs = reviewed_payment(record, {'payment_colors': ['B'] * 3})
        vectors = sorted({tuple(reviewed_payment(record, {'payment_colors': list(colors)})[0])
                          for colors in itertools.product(*pairs)})
        vectors = [list(vector) for vector in vectors]
    else:
        raise ValueError('unsupported complete optional entry discard cost')
    power, toughness = (record.get(field) for field in ('power', 'toughness'))
    if any(not isinstance(n, str) or not n.isascii() or not n.isdecimal() for n in (power, toughness)):
        raise ValueError('fixed printed combat characteristics required')
    return dict(amount=amount, random=random, mana=mana, hybrid=vectors, keywords=record['keywords'],
                power=int(power), toughness=int(toughness))


def variants(params):
    modes = COMMON + (RANDOM if params['random'] else AVATAR)
    return ([f'{mode}_{actor}' for actor in (0, 1) for mode in modes]
            + ([] if params['random'] else [f'{mode}_0' for mode in AVATAR_HUMAN]))


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(name) is not str for name in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('explicit distinct reviewed entry-discard variants required')


def spec(declaration, record):
    params = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('exact typed whole-card discard-payment declaration required')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    skipped = {row['csvid'] for row in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if pins.get('version') != 1 or set(pins['witnesses']) != WITNESSES:
        raise ValueError('reviewed discard-payment witness set required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            row, record = rows.get(name, {}), oracle.get(name, {})
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card' or record.get('layout') != 'normal'
                    or row['csvid'] in skipped or row['handler'] in (None, 'NULL', '0')
                    or (row['handler'] == 'card_dummy' and name not in (WALL, REACH))):
                raise ValueError(f'unreviewed discard-payment binding: {name}')
            if group == 'sources':
                callbacks = RANDOM_CALLBACKS if classify(record)['random'] else {'manalinkeh_card_avatar_of_discord'}
                if row['handler'] not in callbacks:
                    raise ValueError(f'wrong discard-payment callback: {name}')


class EntryWorld(_AttackResourceWorld):
    def __init__(self, name, params, mode, actor, rows, oracle):
        super().__init__(name, dict(params, effect=FAMILY), actor, rows, oracle)
        self.mode = mode
        self.foe = 1 - actor
        self.borrowed = mode in ('borrowed_paid', 'borrowed_empty')
        self.card_owner = self.foe if self.borrowed else self.owner
        self.random_unfixed = False

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipts(self):
        self.emit('refresh')
        start = len(self.steps)
        super().receipts()
        if self.random_unfixed:
            ids = {self.rows[name]['csvid'] for name in PITCHES}
            self.steps[start:] = [s for s in self.steps[start:] if not (
                s['op'] == 'assert_zone_multiplicity' and s['player'] == self.owner
                and s['zone'] in ('hand', 'graveyard') and s['index'] in ids)]
            for card in PITCHES:
                # Exact one of each across the two zones. The total number
                # discarded and the unchanged opponent zones are separate.
                self.emit('assert_hand_graveyard_total', player=self.owner, index=self.rows[card]['csvid'], value=1)

    def configure(self):
        mode = self.mode
        self.add(self.name, 'source', self.card_owner, 'library' if self.borrowed else 'hand', source=True)
        count = self.p['amount']
        hand = (PITCHES[0],) * count
        if mode in ('short_hand', 'extra', 'empty', 'indestructible', 'borrowed_empty'):
            size = {'short_hand': 1, 'extra': count + 1, 'empty': 0, 'indestructible': 0, 'borrowed_empty': 0}[mode]
            hand = (PITCHES[0],) * size
        if mode in ('mixed_random', 'choose_low', 'choose_high', 'cancel_first', 'cancel_second'):
            hand = PITCHES
        for i, name in enumerate(hand):
            self.add(name, f'pitch_{i}', self.owner, 'hand')
        self.add('Plains', 'foe_holds_i', self.foe, 'hand')
        self.add('Island', 'foe_holds_u', self.foe, 'hand')
        self.add('Mountain', 'foe_library', self.foe, 'library')
        self.add('Forest', 'actor_library', self.owner, 'library')
        self.add('Island', 'old_grave', self.foe, 'graveyard')
        if self.borrowed:
            self.add('Bribery', 'carrier', self.owner, 'hand')
        if mode == 'countered':
            self.add('Counterspell', 'counter', self.foe, 'hand')
        if mode == 'indestructible':
            self.add(AVACYN, 'shield', self.owner)
            self.add('Murder', 'control_kill', self.foe, 'hand')
        if mode in ('flying_ground', 'flying_reach'):
            self.add(WALL if mode == 'flying_ground' else REACH, 'blocker', self.foe)

    def cast(self):
        from recipes import cost_from_oracle

        mode = self.mode
        denied = mode in ('wrong_mana', 'mana_short')
        ref = 'carrier' if self.borrowed else 'source'
        cost = cost_from_oracle(self.o['Bribery']) if self.borrowed else list(self.p['mana'])
        if mode in ('all_red', 'mixed_payment', 'payment_menu'):
            cost = [0, 0 if mode == 'all_red' else 1, 0, 0, 3 if mode == 'all_red' else 2, 0]
        pool = cost
        if mode == 'payment_menu':
            pool = [0, 3, 0, 0, 3, 0]
        elif mode == 'wrong_mana':
            pool = [0, 0, sum(cost), 0, 0, 0]
        elif mode == 'mana_short':
            color = next(i for i, amount in enumerate(cost) if amount)
            pool = cost.copy()
            pool[color] -= 1
        self.pool(self.owner, pool)
        self.emit('refresh')
        if (self.p['hybrid'] and not denied and not self.borrowed and self.owner == 0
                and sum(available(pool, vector) for vector in self.p['hybrid']) > 1):
            assert cost in self.p['hybrid']
            self.emit('choose_payment', player=0, value=sum(n << (4 * color) for color, n in enumerate(cost)))
        self.emit('cast', ref=ref, value=int(not denied))
        self.emit('assert_stack', value=int(not denied))
        self.emit('assert_unused_choices', value=0)
        self.observe(ref, 'in_play', False)
        self.observe(ref, 'in_hand', denied)
        if not denied:
            self.mana[self.owner] = [have - spend for have, spend in zip(pool, cost)]
        for color, n in enumerate(self.mana[self.owner]):
            self.emit('assert_mana', player=self.owner, color=color, value=n)
        # The carrier has not resolved. Its active slot can still count
        # towards the raw hand-zone total, so receipts inspect every eligible
        # payment card directly rather than assert that aggregate here.
        payment_cards = [card for card in self.zones['hand'][self.owner] if card not in (self.name, 'Bribery')]
        for index in range(len(payment_cards)):
            self.observe(f'pitch_{index}', 'in_hand', True)
        for zone in ('graveyard', 'battlefield'):
            self.emit('assert_zone_multiplicity', player=self.owner, zone=zone, index=self.rows[self.name]['csvid'], value=0)
        if denied:
            self.receipts()
        return not denied

    def counter_source(self):
        from recipes import cost_from_oracle

        self.emit('announce_spell', ref='source')
        self.pool(self.foe, cost_from_oracle(self.o['Counterspell']))
        self.emit('cast_response', ref='counter', value=1)
        self.emit('assert_stack', value=2)
        self.emit('resolve_response')
        self.mana[self.foe] = [0] * 6
        self.leave('source', 'graveyard')
        self.zones['hand'][self.foe].remove('Counterspell')
        self.zones['graveyard'][self.foe].append('Counterspell')
        self.receipts()

    def payment(self, hand):
        mode, amount = self.mode, self.p['amount']
        decline = mode in ('decline', 'cancel_first', 'cancel_second')
        borrowed_random = self.borrowed and self.p['random']
        chooser = -1 if borrowed_random else self.owner
        if len(hand) < amount:
            if borrowed_random:
                # The Oracle controller cannot pay, so this input must remain
                # unused. If the old owner is asked, paying with its real hand
                # keeps the object live through the subsequent control transfer.
                self.emit('choose_dialog', player=chooser, value=0)
            return []
        if self.p['random']:
            self.emit('choose_dialog', player=chooser, value=int(decline))
        if mode in ('cancel_first', 'cancel_second'):
            if mode == 'cancel_second':
                self.emit('choose_zone_card', player=self.owner, value=self.rows[PITCHES[0]]['csvid'])
            self.emit('choose_optional_zone', player=self.owner, value=self.rows[PITCHES[1]]['csvid'])
        if decline:
            return []
        if self.p['random']:
            return hand[:1]  # A representative for counts only in mixed_random.
        high = self.mode == 'choose_high'
        return sorted(hand, key=lambda name: self.rows[name]['csvid'], reverse=high)[:amount]

    def settle(self):
        # Bribery uses the unique opponent and the real named library result.
        if self.borrowed:
            self.emit('choose_zone_card', player=self.owner, value=self.rows[self.name]['csvid'])
        hand = self.zones['hand'][self.owner].copy()
        hand.remove('Bribery' if self.borrowed else self.name)
        paid = self.payment(hand)
        self.emit('resolve')
        empty_probe = self.mode == 'borrowed_empty' and self.p['random']
        self.emit('assert_unused_choices', value=int(empty_probe))
        if self.borrowed:
            self.zones['hand'][self.owner].remove('Bribery')
            self.zones['graveyard'][self.owner].append('Bribery')
            self.zones['library'][self.foe].remove(self.name)
            if paid:
                self.entities['source'].update(player=self.owner, zone='battlefield', sick=True)
                self.zones['battlefield'][self.owner].append(self.name)
                self.emit('bind_permanent_copy', ref='source', player=self.owner, value=self.rows[self.name]['csvid'], index=0)
            else:
                self.entities['source']['zone'] = 'graveyard'
                self.zones['graveyard'][self.card_owner].append(self.name)
        elif paid:
            self.arrive('source')
        else:
            self.leave('source', 'graveyard')
        for card in paid:
            self.zones['hand'][self.owner].remove(card)
            self.zones['graveyard'][self.owner].append(card)
        self.random_unfixed = self.mode == 'mixed_random'
        self.receipts()

    def indestructible_control(self):
        self.spell('Murder', 'control_kill', self.foe, 'shield')
        self.receipts()

    def flying_combat(self):
        self.age()
        blocked = self.mode == 'flying_reach'
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='blocker', other='source', value=int(blocked))
        self.observe('blocker', 'blocking_count', blocked)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.entities['source']['tapped'] = True
        if blocked:
            self.entities['source']['damage'] = self.entities['blocker']['power']
            self.leave('blocker', 'graveyard')
        else:
            self.life[self.foe] -= self.p['power']
        self.receipts()


def scenario(world):
    world.configure()
    world.emit('turn', player=world.owner, value=0)
    world.emit('present_ai_dialogs', value=1)
    policy = 0 if world.mode in ('cancel_first', 'cancel_second') else 2 if world.mode == 'choose_high' else 1
    world.emit('discard_policy', value=policy)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()
    if not world.cast():
        return None
    if world.mode == 'countered':
        world.counter_source()
        return None
    cut = len(world.steps) if world.mode == 'snapshot' else None
    world.settle()
    if world.mode == 'indestructible':
        world.indestructible_control()
    if world.mode in ('flying_ground', 'flying_reach'):
        world.flying_combat()
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact entry discard-payment source CSVID required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        world = EntryWorld(name, params, mode, int(actor), rows, oracle)
        cut = scenario(world)
        steps = world.steps
        if cut is not None:
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'EntryDiscardPaymentRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=479,
                          setup=world.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'],
                              untested_components=list(GAPS))))
    return cases
