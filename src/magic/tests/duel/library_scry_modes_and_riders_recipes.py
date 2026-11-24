"""Scry mode, timing and rider receipts from paid spells and abilities."""
from collections import Counter, defaultdict
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'library_scry_modes_and_riders'
PROFILE = Path(__file__).with_name('data') / 'library_scry_modes_and_riders_wave8_profiles.json'
INSIGHT, EXCAVATIONS, EYE, ALIGNMENT, BRAIN = 13773, 2350, 13601, 15611, 15754
JACE_SECRET, SAHEELI, NISSA, ANCESTRY = 15871, 16503, 16980, 17321
RITE, ROD, AMULET = 'Rite of Flame', 'Rod of Ruin', "Traveler's Amulet"

COPY, CENTAURS, RETRIBUTION, EAGLES = 13706, 13707, 13708, 13962
MODES = {
    COPY: ('own', 'off_turn', 'lost_target', 'bounce_copy', 'wrong_controller'),
    CENTAURS: ('own', 'off_turn', 'countered', 'enchantment', 'partial'),
    RETRIBUTION: ('own', 'off_turn', 'countered', 'empty_library'),
    EAGLES: ('own', 'countered', 'enchantment', 'flight', 'one_card'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
DECK = ('Grizzly Bears', 'Hill Giant', 'Forest', 'Island', 'Mountain', 'Plains')
FOE_DECK = ('Swamp', 'Mountain', 'Plains')
BEAR, GIANT, MYR = 'Grizzly Bears', 'Hill Giant', 'Darksteel Myr'
JACE, MILLSTONE, SEAL = 'Jace Beleren', 'Millstone', 'Seal of Cleansing'
COUNTER, BOUNCE = 'Counterspell', 'Unsummon'
BIRD, CENTAUR = 'Bird', 'Centaur'
ZERO = [0] * 6
ZONE_NAMES = {code: name for name, code in ZONES.items()}
WITNESSES = frozenset(('Bird', 'Centaur', 'Counterspell', 'Darksteel Myr', 'Forest', 'Grizzly Bears', 'Hill Giant', 'Island', 'Jace Beleren', 'Millstone', 'Mountain', 'Plains', 'Rite of Flame', 'Rod of Ruin', 'Seal of Cleansing', 'Swamp', "Traveler's Amulet", 'Unsummon'))


MODES.update({
    INSIGHT: ('own_host', 'opposing_host', 'off_turn', 'detach'),
    EXCAVATIONS: ('mana', 'scry', 'no_island', 'tapped_island'),
    EYE: ('own_host', 'off_turn', 'detach'),
    ALIGNMENT: ('scry', 'hexproof', 'aligned', 'missing_exile'),
    BRAIN: ('free_cast', 'decline_cast', 'wrong_value', 'scry'),
    JACE_SECRET: ('scry_draw', 'bounce', 'emblem'),
    SAHEELI: ('scry_damage', 'copy', 'copy_artifact', 'search'),
    NISSA: ('scry', 'top_land', 'top_creature', 'top_ineligible', 'animate'),
    ANCESTRY: ('entry', 'no_commander'),
})
PATHS = {
    INSIGHT: 'attachments', EXCAVATIONS: 'excavations', EYE: 'attachments', ALIGNMENT: 'alignment', BRAIN: 'brain',
    JACE_SECRET: 'jace_secret', SAHEELI: 'saheeli', NISSA: 'nissa', ANCESTRY: 'ancestry',
}
GAPS = {
    INSIGHT: ('The aura provider exposes the attached ability; the generic recipient menu is not separately driven.',),
    EYE: ('The equipment provider exposes the attached ability; the generic recipient menu is not separately driven.',),
    ALIGNMENT: ('The informational visibility of the revealed hand has no typed observation; zone prerequisites and terminal outcome are asserted.',),
    NISSA: ('Declining an eligible top card is not representable by the registered callback; the separate eligible and ineligible outcomes preserve the Oracle boundary.',),
    ANCESTRY: ('The two-seat driver has no commander configuration selector. Tapped entry and later untap are exercised; commander identity mana and subtype-specific spending/scry are unexercised.',),
    COPY: ('Exact top-card rearrangement is human-seat only; both seats exercise bottoming.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'scry' not in record.get('oracle_text', '').lower()):
        raise ValueError('normal whole scry primary required')
    return dict(csvid=csvid, printed_cost=record.get('mana_cost', ''))


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{seat}' for seat in (0, 1)
            for mode in MODES[csvid] + (('reorder',) if csvid == COPY and seat == 0 else ())]


def validate_variants(shape, chosen):
    if (type(chosen) is not list or not chosen or any(type(item) is not str for item in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('distinct supported scry variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned scry primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed scry declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys()):
        raise ValueError('exact scry sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != ('token' if name in (BIRD, CENTAUR) else 'normal')):
                raise ValueError('scry Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented scry primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, chosen, rows, oracle):
    validate_variants(shape, chosen)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact scry primary CSVID required')
    cases = []
    for variant in chosen:
        mode, seat = variant.rsplit('_', 1)
        board = Board(rows[name], int(seat), mode, rows, oracle)
        board.run()
        cases.append(dict(name=f'LibraryScryModesAndRidersRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2857, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=list(GAPS.get(csvid, ())))))
    return cases

class Board:
    def __init__(self, primary, owner, mode, rows, oracle):
        self.id, self.name, self.owner, self.foe, self.mode = (
            primary['csvid'], primary['name'], owner, 1-owner, mode)
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.libraries = [[], []]
        self.unordered = set()
        self.life, self.mana = [20, 20], [ZERO.copy(), ZERO.copy()]
        self.active = owner
        self.add('source', self.name, owner, 'hand')
        self.add('opposing', GIANT, self.foe)
        if self.id == COPY:
            if mode != 'wrong_controller':
                self.add('original', BEAR, owner)
            if mode in ('lost_target', 'bounce_copy'):
                self.add('bounce', BOUNCE, self.foe, 'hand')
        if self.id == RETRIBUTION:
            self.add('own_creature', BEAR, owner)
            self.add('survivor', MYR, self.foe)
            self.add('own_walker', JACE, owner, 'hand')
            self.add('foe_walker', JACE, self.foe, 'hand')
            self.add('control', MILLSTONE, owner)
        if mode == 'enchantment':
            self.add('seal', SEAL, self.foe, 'hand')
            self.add('artifact', MILLSTONE, owner)
        if mode == 'countered':
            self.add('counter', COUNTER, self.foe, 'hand')
        deck = {'empty_library': (), 'one_card': DECK[:1]}.get(mode, DECK)
        if self.id in PATHS:
            deck = self.build_follower(deck)
        # An AI recipient with few lands can select nonlands from this scry.
        foe_deck = (GIANT, *FOE_DECK[1:]) if self.id == INSIGHT and owner == 0 and mode == 'opposing_host' else FOE_DECK
        for player in (0, 1):
            for index, name in enumerate(deck if player == owner else foe_deck):
                ref = f'library_{player}_{index}'
                self.add(ref, name, player, 'library')
                self.libraries[player].append(ref)

    def add(self, ref, name, actor, zone='battlefield', *, tapped=False):
        assert actor in (0, 1)
        item = card(name, ref, actor, zone)
        if tapped:
            item['state'] = ['tapped']
        self.setup.append(item)
        self.cards[ref] = dict(name=name, player=actor, zone=zone, token=False)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        if op == 'play_land':
            self.steps.append(step(op, ref=ref, value=value))
            return
        if op == 'choose_trigger_source':
            self.steps.append(step(op, ref=ref, player=player))
            return
        if op == 'advance_scheduled_turn':
            self.steps.append(step(op, value=value))
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

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def wallet(self, who, price, *, setting=False):
        for color, value in enumerate(price):
            self.emit('set_mana' if setting else 'assert_mana', player=who, value=value, color=color)
        if setting:
            self.mana[who] = list(price)
            self.emit('refresh')

    def pending(self, ref, *, target=None, response=False):
        item = self.cards[ref]
        actor = item['player']
        self.wallet(actor, cost_from_oracle(self.oracle[item['name']]), setting=True)
        if target is not None:
            self.emit('choose_target', target, player=actor)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.choices()
        self.emit('assert_stack', value=2 if response else 1)
        self.mana[actor] = ZERO.copy()
        self.wallet(actor, ZERO)

    def resolve(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def cast(self, ref, destination, *, target=None):
        self.pending(ref, target=target)
        self.resolve()
        self.cards[ref]['zone'] = destination

    def next_turn(self):
        assert self.mana == [ZERO, ZERO]
        self.active = 1-self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=self.active)
        self.emit('upkeep_step', player=self.active)
        self.emit('turn', player=self.active)

    def plan_scry(self, amount):
        seat = self.owner
        deck = self.libraries[seat]
        actual = min(len(deck), amount)
        self.emit('record_library', player=seat)
        if self.mode == 'reorder':
            assert seat == 0 and actual == 2
            self.emit('choose_zone', player=seat, value=-1)
            self.emit('choose_zone_card', player=seat, value=self.rows[self.cards[deck[1]]['name']]['csvid'])
            self.emit('choose_zone', player=seat, value=-1)
            deck[0], deck[1] = deck[1], deck[0]
            return
        bottom = min(1, actual) if self.mode == 'partial' else actual
        for ref in deck[:bottom]:
            self.emit('choose_zone_card', player=seat, value=self.rows[self.cards[ref]['name']]['csvid'])
        if bottom != actual:
            self.emit('choose_zone', player=seat, value=-1)
        deck[:] = deck[bottom:] + deck[:bottom]

    def make_tokens(self, name, amount):
        first = 1 if self.id == COPY else 0
        for i in range(amount):
            ref = f'token_{i}'
            self.cards[ref] = dict(name=name, player=self.owner, zone='battlefield', token=True)
            self.emit('bind_permanent_copy', ref, player=self.owner, value=self.rows[name]['csvid'], index=first+i)
        if self.id == COPY:
            self.observe('token_0', 'power', 2)
            self.observe('token_0', 'toughness', 2)
            self.observe('token_0', 'color', 8)
            self.emit('assert_matching_permanents', prop='subtype_Bear', player=self.owner,
                      index=self.rows[name]['csvid'], value=1)
        else:
            power, color = (2, 4) if self.id == EAGLES else (3, 8)
            for prop, expected in (('token', 1), ('power', power), ('toughness', power), ('color', color),
                                   ('artifact', 0), ('subtype_' + name, 1)):
                self.emit('assert_matching_permanents', prop=prop, player=self.owner,
                          index=self.rows[name]['csvid'], value=expected)

    def flight(self):
        self.next_turn()
        self.next_turn()
        self.observe('token_0', 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', 'token_0', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'opposing', 'token_0', value=0)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('refresh')
        self.choices()
        self.life[self.foe] -= 2

    def rider(self):
        if self.id == COPY:
            self.make_tokens(BEAR, 1)
        elif self.id == CENTAURS:
            self.make_tokens(CENTAUR, 2)
        elif self.id == EAGLES:
            self.make_tokens(BIRD, 2)
        else:
            for ref in ('own_creature', 'opposing', 'own_walker', 'foe_walker'):
                self.cards[ref]['zone'] = 'graveyard'

    def run(self):
        off_turn_cast = self.mode == 'off_turn' and self.id not in (RETRIBUTION, EYE, INSIGHT)
        self.active = self.foe if off_turn_cast else self.owner
        self.emit('turn', player=self.active)
        for player in (0, 1):
            self.wallet(player, ZERO, setting=True)
        if self.id in PATHS:
            getattr(self, PATHS[self.id])()
            self.receipts()
            return
        if self.id == RETRIBUTION:
            self.cast('own_walker', 'battlefield')
            self.next_turn()
            self.cast('foe_walker', 'battlefield')
            if self.mode != 'off_turn':
                self.next_turn()
            for ref in ('own_walker', 'foe_walker'):
                self.emit('assert_counter', ref, prop='loyalty', value=3)
        if self.mode == 'wrong_controller':
            self.wallet(self.owner, cost_from_oracle(self.oracle[self.name]), setting=True)
            self.emit('can_cast', 'source', value=0)
            self.receipts()
            return
        self.pending('source', target='original' if self.id == COPY else None)
        lost = self.mode in ('countered', 'lost_target')
        if lost:
            self.emit('announce_spell', 'source')
            answer = 'counter' if self.mode == 'countered' else 'bounce'
            # Counterspell copies the pending cast context into its target.
            aimed = None if self.mode == 'countered' else 'original'
            self.pending(answer, target=aimed, response=True)
            self.resolve(response=True)
            self.cards[answer]['zone'] = 'graveyard'
            if self.mode == 'lost_target':
                self.cards['original']['zone'] = 'hand'
        else:
            sift = 0 if self.mode == 'off_turn' else 1 if self.id == EAGLES else 2
            if sift:
                self.plan_scry(sift)
            self.resolve()
            if sift:
                self.emit('assert_scry_result', player=self.owner, value=sift)
            self.rider()
        self.cards['source']['zone'] = 'graveyard'
        self.receipts()
        if self.mode == 'bounce_copy':
            self.cast('bounce', 'graveyard', target='token_0')
            self.cards['token_0']['zone'] = 'gone'
        if self.mode == 'enchantment':
            self.next_turn()
            self.cast('seal', 'battlefield')
            self.emit('probe_activation_target', 'seal', 'token_0', value=1, index=-1)
            self.emit('probe_activation_target', 'seal', 'artifact', value=1, index=-1)
            self.emit('probe_activation_target', 'seal', 'opposing', value=0, index=-1)
        if self.mode == 'flight':
            self.flight()
        self.receipts()

    def build_follower(self, deck):
        if self.id in (INSIGHT, EYE):
            self.add('host', BEAR, self.foe if self.mode == 'opposing_host' else self.owner)
            if self.mode == 'detach':
                self.add('bounce', BOUNCE, self.foe, 'hand')
        elif self.id == EXCAVATIONS:
            if self.mode != 'no_island':
                self.add('island', 'Island', self.owner, tapped=self.mode == 'tapped_island')
            self.add('forest', 'Forest', self.owner)
            self.add('other_island', 'Island', self.foe)
        elif self.id == ALIGNMENT:
            if self.mode == 'hexproof':
                self.add('seal', SEAL, self.foe, 'hand')
                self.add('artifact', MILLSTONE, self.owner)
            if self.mode in ('aligned', 'missing_exile'):
                self.add('alignment_hand', self.name, self.owner, 'hand')
                self.add('alignment_grave', self.name, self.owner, 'graveyard')
                self.add('alignment_exile', self.name, self.owner if self.mode == 'aligned' else self.foe, 'exile')
        elif self.id == BRAIN:
            if self.mode in ('free_cast', 'decline_cast'):
                self.add('free_spell', RITE, self.owner, 'hand')
            if self.mode in ('wrong_value', 'scry'):
                self.add('wrong_value', COUNTER, self.owner, 'hand')
        elif self.id == JACE_SECRET and self.mode == 'emblem':
            self.add('first_spell', MILLSTONE, self.foe, 'hand')
            self.add('second_spell', 'Traveler\'s Amulet', self.foe, 'hand')
            if self.owner == 1:
                deck = (BEAR, GIANT) * 3 + ('Forest', 'Island')
        elif self.id == SAHEELI:
            if self.mode in ('copy', 'copy_artifact'):
                self.add('host', BEAR if self.mode == 'copy' else MILLSTONE, self.owner)
            if self.mode == 'copy':
                self.add('seal', SEAL, self.foe)
                self.add('artifact', MILLSTONE, self.owner)
            if self.mode == 'search':
                if self.owner == 1:
                    deck = (BEAR, GIANT) * 2 + DECK[2:]
                deck = (*deck, MILLSTONE, ROD, AMULET)
        elif self.id == NISSA:
            if self.mode == 'top_land':
                deck = ('Forest', *deck)
            if self.mode == 'top_creature':
                deck = (GIANT, 'Forest', 'Island', 'Mountain')
            if self.mode == 'top_ineligible':
                deck = (GIANT, *deck)
            if self.mode == 'animate':
                self.add('first_land', 'Forest', self.owner, tapped=True)
                self.add('second_land', 'Island', self.owner, tapped=True)
        return deck

    def basic_activation(self, fee, *, target=None, menu=None, actor=None, ref='source', remainder=ZERO):
        payer = self.owner if actor is None else actor
        self.wallet(payer, fee, setting=True)
        self.emit('can_activate', ref, value=1)
        if menu is not None:
            self.emit('present_ai_dialogs', value=1)
            self.emit('choose_dialog', player=payer, value=menu)
        if target is not None:
            self.emit('choose_target', target, player=payer)
        self.emit('activate', ref, value=1)
        self.choices()
        if menu is not None:
            self.emit('present_ai_dialogs', value=0)
        self.emit('assert_stack', value=1)
        self.mana[payer] = remainder.copy()
        self.wallet(payer, remainder)

    def order_top(self, amount, *, actor=None, draw=False):
        payer = self.owner if actor is None else actor
        deck = self.libraries[payer]
        self.emit('record_library', player=payer)
        for ref in deck[:amount]:
            self.emit('choose_zone_card', player=payer, value=self.rows[self.cards[ref]['name']]['csvid'])
        deck[:] = deck[amount:] + deck[:amount]
        if draw:
            self.cards[deck.pop(0)]['zone'] = 'hand'

    def activated_scry(self, amount, *, actor=None, draw=False):
        payer = self.owner if actor is None else actor
        self.order_top(amount, actor=payer, draw=draw)
        self.resolve()
        if not draw:
            self.emit('assert_scry_result', player=payer, value=amount)

    def attachments(self):
        if self.id == INSIGHT:
            self.cast('source', 'battlefield', target='host')
        else:
            self.cast('source', 'battlefield')
            self.basic_activation([1, 0, 0, 0, 0, 0], target='host', menu=0)
            self.resolve()
        actor = self.cards['host']['player']
        fee = ZERO if self.id == INSIGHT else [1, 0, 0, 0, 0, 0]
        if self.mode == 'detach':
            self.cast('bounce', 'graveyard', target='host')
            self.cards['host']['zone'] = 'hand'
            if self.id == INSIGHT:
                self.cards['source']['zone'] = 'graveyard'
            self.emit('can_activate', 'source', value=0)
            return
        if self.mode == 'off_turn':
            self.next_turn()
        # The DIALOG keeps both rows even when Equip is disabled off-turn.
        menu = 1 if self.id == EYE else None
        self.basic_activation(fee, actor=actor, menu=menu)
        self.observe('host', 'tapped', 1)
        self.activated_scry(1, actor=actor, draw=self.id == INSIGHT)
        self.wallet(actor, fee, setting=True)
        if self.id == INSIGHT or self.mode == 'off_turn':
            self.emit('can_activate', 'source', value=0)

    def excavations(self):
        success = self.mode not in ('no_island', 'tapped_island')
        if success and self.owner == 0:
            self.emit('choose_target', 'island', player=0)
        self.emit('play_land', 'source', value=1)
        self.choices()
        self.cards['source']['zone'] = 'battlefield' if success else 'graveyard'
        if not success:
            return
        self.cards['island']['zone'] = 'graveyard'
        self.observe('source', 'tapped', 0)
        if self.mode == 'scry':
            self.basic_activation([3, 0, 0, 0, 0, 0], menu=1, remainder=[2, 0, 0, 0, 0, 0])
            self.activated_scry(1)
        else:
            self.emit('can_activate', 'source', value=1)
            self.emit('choose_dialog', player=self.owner, value=0)
            self.emit('activate', 'source', value=1)
            self.choices()
            self.emit('assert_stack')
            self.mana[self.owner] = [1, 0, 1, 0, 0, 0]
        self.observe('source', 'tapped', 1)

    def alignment(self):
        self.cast('source', 'battlefield')
        if self.mode == 'scry':
            self.basic_activation([1, 0, 1, 0, 0, 0])
            self.activated_scry(1)
        elif self.mode == 'hexproof':
            self.next_turn()
            self.cast('seal', 'battlefield')
            self.emit('probe_activation_target', 'seal', 'source', value=0, index=-1)
            self.emit('probe_activation_target', 'seal', 'artifact', value=1, index=-1)
        else:
            self.next_turn()
            self.emit('advance_turn')
            self.emit('untap_step', player=self.owner)
            if self.owner == 0:
                self.emit('choose_trigger_source', 'source', player=self.owner)
            self.emit('upkeep_step', player=self.owner)
            self.choices()
            if self.mode == 'aligned':
                self.emit('advance_scheduled_turn', value=-1)

    def brain(self):
        self.cast('source', 'battlefield')
        self.basic_activation([1, 0, 0, 0, 0, 0], menu=0)
        if self.mode == 'free_cast':
            self.emit('choose_zone_card', player=self.owner, value=self.rows[RITE]['csvid'])
            self.cards['free_spell']['zone'] = 'graveyard'
            self.mana[self.owner] = [0, 0, 0, 0, 2, 0]
        else:
            # The optional hand selector still opens when no card matches its filter.
            self.emit('choose_zone', player=self.owner, value=-1)
        self.resolve()
        self.emit('assert_counter', 'source', prop='charge', value=1)
        self.observe('source', 'tapped', 1)
        self.emit('can_activate', 'source', value=0)
        if self.mode == 'scry':
            self.next_turn()
            self.next_turn()
            self.wallet(self.owner, [3, 0, 0, 0, 0, 0], setting=True)
            self.emit('present_ai_dialogs', value=1)
            self.emit('choose_dialog', player=self.owner, value=1)
            if self.owner == 0:
                self.emit('choose_number', player=self.owner, value=1)
            self.emit('activate', 'source', value=1)
            self.choices()
            self.emit('present_ai_dialogs', value=0)
            self.emit('assert_counter', 'source', prop='charge', value=0)
            self.mana[self.owner] = ZERO.copy()
            self.activated_scry(1)

    def loyalty(self, mode, result, *, targets=()):
        self.emit('can_activate', 'source', value=1)
        self.emit('present_ai_dialogs', value=1)
        self.emit('choose_dialog', player=self.owner, value=mode)
        for target in targets:
            self.emit('choose_target', target, player=self.owner)
        self.emit('activate', 'source', value=1)
        self.choices()
        self.emit('present_ai_dialogs', value=0)
        self.emit('assert_stack', value=1)
        if result:
            self.emit('assert_counter', 'source', prop='loyalty', value=result)
        else:
            self.cards['source']['zone'] = 'graveyard'

    def jace_secret(self):
        self.cast('source', 'battlefield')
        self.emit('assert_counter', 'source', prop='loyalty', value=5)
        if self.mode == 'bounce':
            self.loyalty(1, 3, targets=('opposing',))
            self.resolve()
            self.cards['opposing']['zone'] = 'hand'
            return
        if self.mode == 'scry_draw':
            self.loyalty(0, 6)
            self.activated_scry(1, draw=True)
            self.emit('can_activate', 'source', value=0)
            return
        assert self.mode == 'emblem'
        for value in (6, 7, 8):
            self.loyalty(0, value)
            self.activated_scry(1, draw=True)
            self.next_turn()
            self.next_turn()
        self.loyalty(2, 0)
        self.resolve()
        self.next_turn()
        self.pending('first_spell')
        self.emit('announce_spell', 'first_spell')
        self.resolve()
        self.cards['first_spell']['zone'] = 'graveyard'
        self.cast('second_spell', 'battlefield')

    def saheeli(self):
        self.cast('source', 'battlefield')
        self.emit('assert_counter', 'source', prop='loyalty', value=3)
        if self.mode == 'scry_damage':
            self.loyalty(0, 4)
            self.activated_scry(1)
            self.life[self.foe] -= 1
        elif self.mode == 'search':
            for value in (4, 5, 6, 7):
                self.loyalty(0, value)
                self.activated_scry(1)
                self.life[self.foe] -= 1
                self.next_turn()
                self.next_turn()
            self.loyalty(2, 0)
            for name in (MILLSTONE, ROD, AMULET):
                self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])
            self.resolve()
            self.emit('assert_shuffle_count', player=self.owner, value=1)
            deck = self.libraries[self.owner]
            for ref in deck[:]:
                name = self.cards[ref]['name']
                if name in (MILLSTONE, ROD, AMULET):
                    self.cards[ref]['zone'] = 'battlefield'
                    self.emit('bind_permanent', ref, player=self.owner, value=self.rows[name]['csvid'], index=-1)
                    deck.remove(ref)
            self.unordered.add(self.owner)
        else:
            self.loyalty(1, 1, targets=('host',))
            self.resolve()
            self.observe('host', 'in_play', 1)
            name = self.cards['host']['name']
            self.cards['copy'] = dict(name=name, player=self.owner, zone='battlefield', token=True)
            self.emit('bind_permanent_copy', 'copy', player=self.owner, value=self.rows[name]['csvid'], index=1)
            if self.mode == 'copy_artifact':
                self.emit('assert_matching_permanents', prop='artifact', player=self.owner,
                          index=self.rows[name]['csvid'], value=1)
            if self.mode == 'copy':
                self.emit('probe_activation_target', 'seal', 'copy', value=1, index=-1)
                self.emit('probe_activation_target', 'seal', 'artifact', value=1, index=-1)
                self.emit('probe_activation_target', 'seal', 'host', value=0, index=-1)
                self.observe('copy', 'power', 2)
                self.observe('copy', 'toughness', 2)
                self.emit('begin_attackers')
                self.emit('attack', 'copy', value=1)
                self.emit('finish_attackers')
                self.emit('begin_blockers')
                self.emit('advance_combat')
                self.emit('combat_damage')
                self.resolve()
                self.emit('end_combat')
                self.life[self.foe] -= 2
            self.emit('cleanup')
            self.resolve()
            self.observe('copy', 'in_play', 0)
            self.cards['copy']['zone'] = 'gone'

    def nissa(self):
        amount = {'animate': 7, 'top_ineligible': 2, 'top_creature': 4}.get(self.mode, 3)
        self.wallet(self.owner, [amount, 0, 1, 1, 0, 0], setting=True)
        if self.owner == 0:
            self.emit('choose_x', player=self.owner, value=amount)
        self.emit('cast', 'source', value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.mana[self.owner] = ZERO.copy()
        self.resolve()
        self.cards['source']['zone'] = 'battlefield'
        self.emit('assert_counter', 'source', prop='loyalty', value=amount)
        if self.mode == 'scry':
            self.loyalty(0, amount + 2)
            self.activated_scry(2)
        elif self.mode == 'animate':
            lands = ('first_land', 'second_land')
            self.loyalty(2, 1, targets=lands)
            self.resolve()
            self.emit('begin_attackers')
            for ref in lands:
                for prop, value in (('tapped', 0), ('power', 5), ('toughness', 5)):
                    self.observe(ref, prop, value)
                self.emit('attack', ref, value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block', 'opposing', lands[0], value=0)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.resolve()
            self.emit('end_combat')
            self.life[self.foe] -= 10
            self.emit('cleanup')
            self.resolve()
            for ref in lands:
                self.observe(ref, 'power', 0)
                self.observe(ref, 'toughness', 0)
        else:
            self.loyalty(1, amount)
            if self.owner == 0:
                self.emit('choose_zone', player=0, value=-1)
            self.resolve()
            if self.mode != 'top_ineligible':
                self.cards[self.libraries[self.owner].pop(0)]['zone'] = 'battlefield'

    def ancestry(self):
        self.emit('play_land', 'source', value=1)
        self.choices()
        self.cards['source']['zone'] = 'battlefield'
        self.observe('source', 'tapped', 1)
        self.emit('can_activate', 'source', value=0)
        if self.mode == 'no_commander':
            self.next_turn()
            self.next_turn()
            self.observe('source', 'tapped', 0)
            self.wallet(self.owner, ZERO)

    def receipts(self):
        self.emit('assert_stack')
        for player in (0, 1):
            if self.id != ALIGNMENT or self.mode != 'aligned' or player == self.owner:
                self.emit('assert_life', player=player, value=self.life[player])
            self.wallet(player, self.mana[player])
            counts = defaultdict(Counter)
            for item in self.cards.values():
                if item['player'] == player and item['zone'] != 'gone':
                    counts[item['zone']][item['name']] += 1
            for zone, code in ZONES.items():
                self.emit('assert_zone', player=player, zone=code, value=counts[zone].total())
                for name, count in sorted(counts[zone].items()):
                    self.emit('assert_zone_multiplicity', player=player, zone=code, value=count,
                              index=self.rows[name]['csvid'])
            ordered = () if player in self.unordered else self.libraries[player]
            for index, ref in enumerate(ordered):
                self.emit('assert_zone_card', player=player, zone=ZONES['library'], index=index,
                          value=self.rows[self.cards[ref]['name']]['csvid'])
        for ref, item in self.cards.items():
            if item['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)
