"""Real draw-step riders, hand exchanges and maximum-hand-size clauses."""
from collections import Counter
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import COLORS, card, cost_from_oracle, step


FAMILY = 'draw_step_discard_and_hand_size'
PROFILE = Path(__file__).with_name('data') / 'draw_step_discard_and_hand_size_wave8_profiles.json'
ANVIL, CAP, AWARENESS, DRAGON = 2670, 3786, 4838, 15084
OWN_DRAW = 'At the beginning of your draw step, draw an additional card.'
OWN_END = 'At the beginning of your end step, discard your hand.'
CARDS = {
    ANVIL: ('{2}', 'Artifact', [], ('Players have no maximum hand size.',
            "At the beginning of each player's draw step, that player draws an additional card, then discards a card.")),
    CAP: ('{4}', 'Artifact', [], (OWN_DRAW, OWN_END)),
    AWARENESS: ('{3}{U}{U}', 'Enchantment', [], ('As this enchantment enters, discard your hand.', OWN_DRAW)),
    DRAGON: ('{2}{R}{R}', 'Creature — Dragon', ['Flying'], ('Flying', OWN_DRAW, OWN_END)),
}
EXTRA = {ANVIL: ('empty_before_draw',), CAP: ('removed_after_draw',), AWARENESS: ('countered', 'empty_entry'),
         DRAGON: ('removed_after_draw',)}
MODES = ('scope', 'removed')
PREFIX = 'DrawStepDiscardAndHandSizeRecipe'
SEED = 2528
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
REACH = 'Reach Through Mists'
STANDARD_DECK = [REACH, 'Island', 'Swamp', 'Plains', 'Mountain']


OIL, BOX, OVERBEING = 16452, 1168, 9845
LOOTER = 'Rummaging Goblin'
HYBRID = '{G/U}' * 5
CARDS.update({
    OIL: ('{2}{B}{B}', 'Enchantment', [], (
        'This enchantment enters with seven hour counters on it.',
        'At the beginning of your draw step, draw an additional card and remove two hour counters from this enchantment.',
        'Your maximum hand size is equal to the number of hour counters on this enchantment.',
        'Whenever you discard a card, you lose 1 life.',
    )),
    BOX: ('{4}', 'Artifact', [], (
        "At the beginning of each player's draw step, that player puts the cards in their hand on the bottom of their library in any order, then draws that many cards.",
    )),
    OVERBEING: (HYBRID, 'Creature — Spirit Avatar', [], (
        "Overbeing of Myth's power and toughness are each equal to the number of cards in your hand.", OWN_DRAW,
    )),
})
EXTRA.update({OIL: ('discard_loss', 'discard_foe', 'replay'), BOX: ('empty_hand', 'own_scope', 'countered'),
              OVERBEING: ('mixed_replay', 'countered')})
WITNESSES = {'Mountain', 'Forest', 'Island', 'Swamp', 'Plains', REACH, 'Terminate', 'Disenchant',
             'Counterspell', 'Grizzly Bears', LOOTER}
HAND_LIMIT_GAP = ('The driver cleanup never runs the production discard phase or its maximum-hand-size query. '
                  'No finite or unlimited maximum-hand-size assertion is claimed.')
HOUR_GAP = 'The typed counter observer does not expose hour counters, so the seven-hour entry and decrement/floor are unobserved.'

ZONE_NAMES = {value: name for name, value in ZONE.items()}
ZONE_RECEIPTS = {'assert_zone', 'assert_zone_multiplicity', 'assert_zone_card'}
ZONE_INDEX = {'assert_zone_multiplicity', 'assert_zone_card', 'bind_hand_copy'}
SEATED = ZONE_RECEIPTS | {'assert_life', 'assert_mana', 'set_mana', 'bind_hand_copy', 'choose_zone_card',
                         'choose_dialog', 'choose_target', 'turn', 'untap_step', 'upkeep_step', 'draw_step'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in CARDS or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces')):
        raise ValueError('complete normal draw-step primary required')
    mana, type_line, keywords, clauses = CARDS[csvid]
    stats = ('4', '4') if csvid == DRAGON else ('*', '*') if csvid == OVERBEING else None
    if (record.get('oracle_text') != '\n'.join(clauses) or record.get('type_line') != type_line
            or record.get('mana_cost') != mana or record.get('keywords') != keywords
            or stats and (record.get('power'), record.get('toughness')) != stats):
        raise ValueError('complete draw-step text, type, stats and mana required')
    return {'csvid': csvid}


def variants(shape):
    return [f'{mode}_{actor}' for mode in (*MODES, *EXTRA[shape['csvid']]) for actor in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported draw-step variants required')


def spec(declaration, record):
    pinned = profiles()['sources'].get(record.get('name'))
    if not pinned or not exact(pinned['oracle'], record):
        raise ValueError('complete pinned draw-step source required')
    shape = classify(record, pinned['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared draw-step setup required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete draw-step sources and witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != CARDS.keys():
        raise ValueError('exact seven-source profile required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact draw-step source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented draw-step primary required')
                classify(oracle[name], row['csvid'])


def gaps(csvid):
    if csvid == ANVIL:
        return [HAND_LIMIT_GAP]
    if csvid == OIL:
        return [HAND_LIMIT_GAP, HOUR_GAP]
    if csvid == BOX:
        return ['The automated bottom selection has an order-independent set; manual ordering of distinguishable cards is untested.']
    return []


def hybrid_payment(mode, seat):
    blue = 2 if mode == 'mixed_replay' else 5 if seat == 0 else 0
    payment = ZERO.copy()
    payment[COLORS['U']] = blue
    payment[COLORS['G']] = 5 - blue
    return payment


class Duel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.library = [[], []]
        self.life = [20, 20]
        self.current = actor
        self.live = False
        self.add('source', actor, record['name'], 'hand')
        for player in (0, 1):
            if csvid == ANVIL:
                if mode != 'empty_before_draw' or player == 0 and actor == 1:
                    self.add(f'initial_{player}', player, REACH, 'hand')
                deck = ['Island', 'Island', 'Plains'] if mode == 'empty_before_draw' and player == 1 else [
                    REACH, REACH, 'Island', 'Plains', 'Mountain']
            else:
                initial = ('Mountain', 'Forest')
                if csvid == BOX:
                    initial = () if mode == 'empty_hand' else ('Mountain', 'Mountain')
                elif mode in ('discard_loss', 'discard_foe'):
                    initial = ('Forest',)
                if mode != 'empty_entry' or player == self.foe:
                    for slot, name in enumerate(initial):
                        self.add(f'initial_{player}_{slot}', player, name, 'hand')
                deck = (['Mountain', REACH, 'Island', 'Swamp', 'Plains', 'Forest']
                        if csvid == BOX and mode in ('scope', 'empty_hand', 'own_scope') else STANDARD_DECK)
            for index, name in enumerate(deck):
                ref = f'deck_{player}_{index}'
                self.add(ref, player, name, 'library')
                self.library[player].append(ref)
        if mode in ('removed', 'removed_after_draw'):
            self.add('remove', self.foe, 'Terminate' if csvid in (DRAGON, OVERBEING) else 'Disenchant', 'hand')
        if mode == 'countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')
        if mode in ('discard_loss', 'discard_foe'):
            self.add('looter', self.actor if mode == 'discard_loss' else self.foe, LOOTER, 'hand')
        if csvid == DRAGON:
            self.add('ground', self.foe, 'Grizzly Bears', 'battlefield')

    def add(self, ref, seat, name, zone):
        self.setup.append(card(name, ref, seat, zone))
        self.cards[ref] = dict(player=seat, name=name, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        values = {}
        if ref:
            values['ref'] = ref
        if other:
            values['other'] = other
        if prop:
            values['property'] = prop
        if player or op in SEATED:
            values['player'] = player
        if (op.startswith(('assert_', 'choose_')) and op != 'assert_choices_consumed') or value or op in ('block', 'set_mana'):
            values['value'] = value
        if op in ZONE_RECEIPTS:
            values['zone'] = ZONE_NAMES[zone]
        if op in ZONE_INDEX or index:
            values['index'] = index
        if op in ('set_mana', 'assert_mana'):
            values['color'] = color
        self.steps.append(step(op, **values))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, seat, values):
        for color, amount in enumerate(values):
            self.emit(op, player=seat, color=color, value=amount)

    def hand(self, seat):
        return [ref for ref, card in self.cards.items() if card['player'] == seat and card['zone'] == 'hand']

    def receipts(self):
        self.emit('assert_stack')
        for seat in (0, 1):
            self.mana('assert_mana', seat, ZERO)
            self.emit('assert_life', player=seat, value=self.life[seat])
            for zone in ('hand', 'library', 'graveyard'):
                refs = self.library[seat] if zone == 'library' else [ref for ref, card in self.cards.items()
                        if card['player'] == seat and card['zone'] == zone]
                self.emit('assert_zone', player=seat, zone=ZONE[zone], value=len(refs))
                counts = Counter(self.cards[ref]['name'] for ref in refs)
                names = sorted({card['name'] for card in self.cards.values() if card['player'] == seat})
                for name in names:
                    self.emit('assert_zone_multiplicity', player=seat, zone=ZONE[zone],
                              index=self.rows[name]['csvid'], value=counts[name])
                if zone == 'library':
                    for index, ref in enumerate(refs):
                        self.emit('assert_zone_card', player=seat, zone=ZONE[zone], index=index,
                                  value=self.rows[self.cards[ref]['name']]['csvid'])
        self.emit('assert_zone_multiplicity', player=self.actor, zone=ZONE['battlefield'],
                  index=self.rows[self.record['name']]['csvid'], value=int(self.live))
        if self.live and self.csvid == OVERBEING:
            for stat in ('power', 'toughness'):
                self.observe('source', stat, len(self.hand(self.actor)))

    def bind_hand(self, ref, seat, name):
        self.emit('bind_hand_copy', ref, player=seat, value=self.rows[name]['csvid'], index=0)

    def plan_all_discard(self, seat):
        refs = self.hand(seat)
        if seat == 0:
            target_refs = []
            for ref in refs:
                if ref.startswith('deck_'):
                    bound = 'discard_' + ref
                    self.bind_hand(bound, seat, self.cards[ref]['name'])
                    target_refs.append(bound)
                else:
                    target_refs.append(ref)
            for ref in target_refs:
                self.discard_choice(ref.removeprefix('discard_'))
        for ref in refs:
            self.cards[ref]['zone'] = 'graveyard'

    def discard_choice(self, ref):
        self.emit('choose_zone_card', player=0, value=self.rows[self.cards[ref]['name']]['csvid'])

    def learn(self, seat, amount):
        assert len(self.library[seat]) >= amount
        for _ in range(amount):
            self.cards[self.library[seat].pop(0)]['zone'] = 'hand'

    def settle(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.emit('refresh')

    def pay(self, ref, seat, *, target=None, response=False):
        actual_ref = 'ordinary' if ref.startswith('deck_') else ref
        payment = (hybrid_payment(self.mode, seat) if ref == 'source' and self.csvid == OVERBEING
                   else cost_from_oracle(self.oracle[self.cards[ref]['name']]))
        self.mana('set_mana', seat, payment)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=seat)
        self.emit('cast_response' if response else 'cast', actual_ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1 + int(response))
        self.mana('assert_mana', seat, ZERO)

    def cast_source(self):
        self.emit('turn', player=self.actor)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        self.pay('source', self.actor)
        if self.mode == 'countered':
            self.emit('announce_spell', 'source')
            self.pay('counter', self.foe, response=True)
            self.settle(response=True)
            self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
        else:
            self.cards['source']['zone'] = 'battlefield'
            if self.csvid == AWARENESS:
                self.plan_all_discard(self.actor)
            self.settle()
            self.live = True
            self.observe('source', 'in_play', 1)
        self.receipts()

    def remove_source(self):
        self.pay('remove', self.foe, target='source')
        self.settle()
        self.cards['source']['zone'] = self.cards['remove']['zone'] = 'graveyard'
        self.live = False
        self.receipts()

    def plan_end(self):
        if self.live and self.current == self.actor and self.csvid in (CAP, DRAGON):
            self.plan_all_discard(self.actor)

    def draw_turn(self, seat):
        assert seat != self.current
        self.plan_end()
        self.emit('advance_turn')
        self.emit('assert_choices_consumed')
        self.current = seat
        self.receipts()
        self.emit('untap_step', player=seat)
        self.emit('upkeep_step', player=seat)
        self.emit('assert_choices_consumed')
        self.receipts()
        replay = self.mode in ('replay', 'mixed_replay') and seat == self.actor
        old = self.cards, self.library
        if replay:
            self.emit('capture')
            self.cards, self.library = deepcopy(old)
        self.draw_effect(seat)
        if replay:
            self.emit('record_state')
            self.emit('restore')
            self.emit('ai_roundtrip')
            self.cards, self.library = old
            self.draw_effect(seat)
            self.emit('assert_recorded_state')

    def draw_effect(self, seat):
        active_anvil = self.live and self.csvid == ANVIL
        active_box = self.live and self.csvid == BOX
        if active_anvil and seat == 0:
            # This existing hand card is legal before and after the draws.
            self.discard_choice('initial_0')
        if active_box and seat == 0:
            # Auto is the first of the real Auto/Manual rows. All exchanged cards
            # are Mountains, so any bottom order is Oracle-equivalent here.
            self.emit('choose_dialog', player=0, value=0)
        self.emit('draw_step', player=seat)
        self.emit('assert_choices_consumed')
        owner_draw = self.csvid != BOX and seat == self.actor
        self.learn(seat, 1 + int(self.live and (active_anvil or owner_draw)))
        if active_anvil:
            refs = self.hand(seat)
            assert len({self.cards[ref]['name'] for ref in refs}) == 1
            self.cards[refs[0]]['zone'] = 'graveyard'
        if active_box:
            refs = self.hand(seat)
            assert refs and all(self.cards[ref]['name'] == 'Mountain' for ref in refs)
            for ref in refs:
                self.cards[ref]['zone'] = 'library'
            self.library[seat].extend(refs)
            self.learn(seat, len(refs))
        if self.live and self.csvid == OVERBEING and seat == self.actor:
            # The draw phase changes hand size without invalidating the cached P/T.
            self.emit('refresh')
        self.receipts()

    def ordinary_draw(self):
        seat = self.current
        self.emit('turn', player=seat)
        ref = next(ref for ref in self.hand(seat) if self.cards[ref]['name'] == REACH)
        self.bind_hand('ordinary', seat, REACH)
        # The bound copy can come from the fixture hand or from the actual draw step.
        self.mana('set_mana', seat, cost_from_oracle(self.oracle[REACH]))
        self.emit('refresh')
        self.emit('cast', 'ordinary', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', seat, ZERO)
        self.settle()
        self.cards[ref]['zone'] = 'graveyard'
        self.learn(seat, 1)
        self.receipts()

    def flying(self):
        self.observe('source', 'power', 4)
        self.observe('source', 'toughness', 4)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground', 'source', value=0)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 0)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.life[self.foe] -= 4
        self.receipts()

    def visit_main(self, seat):
        assert seat != self.current
        self.emit('advance_turn')
        self.emit('untap_step', player=seat)
        self.emit('upkeep_step', player=seat)
        self.emit('turn', player=seat)
        self.emit('assert_choices_consumed')
        self.current = seat

    def discard_with_looter(self):
        seat = self.actor if self.mode == 'discard_loss' else self.foe
        if self.current != seat:
            self.visit_main(seat)
        self.pay('looter', seat)
        self.settle()
        self.cards['looter']['zone'] = 'battlefield'
        self.visit_main(1 - seat)
        self.visit_main(seat)
        self.observe('looter', 'sick', 0)
        self.observe('looter', 'tapped', 0)
        ref, = self.hand(seat)
        if seat == 0:
            self.emit('discard_policy', value=1)
        self.emit('require_can_activate', 'looter', value=1)
        self.emit('activate', 'looter', value=1)
        self.emit('assert_choices_consumed')
        if seat == 0:
            self.emit('assert_discard_choices', value=1)
        self.observe('looter', 'tapped', 1)
        self.cards[ref]['zone'] = 'graveyard'
        self.settle()
        self.learn(seat, 1)
        self.life[self.actor] -= int(seat == self.actor)
        self.receipts()

    def build(self):
        self.cast_source()
        if self.mode in ('discard_loss', 'discard_foe'):
            self.discard_with_looter()
            return
        if self.mode == 'removed':
            self.remove_source()
        if self.csvid == BOX and self.mode == 'own_scope':
            self.visit_main(self.foe)
            self.draw_turn(self.actor)
            return
        self.draw_turn(self.foe)
        if self.csvid == BOX and self.mode in ('scope', 'empty_hand'):
            return
        if self.mode == 'empty_before_draw':
            if self.foe == 0:
                self.draw_turn(self.actor)
            return
        self.ordinary_draw()
        self.draw_turn(self.actor)
        self.ordinary_draw()
        if self.mode == 'removed_after_draw':
            self.remove_source()
        if self.live and self.csvid == DRAGON:
            self.flying()
        self.plan_end()
        self.emit('cleanup')
        self.emit('assert_choices_consumed')
        self.receipts()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, actor = variant.rsplit('_', 1)
    duel = Duel(csvid, oracle[name], int(actor), mode, rows, oracle)
    duel.build()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=duel.setup, steps=duel.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=gaps(csvid)))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered draw-step source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
