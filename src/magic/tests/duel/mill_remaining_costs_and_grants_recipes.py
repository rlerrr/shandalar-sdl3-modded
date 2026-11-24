"""Paid mill, creature grants, and the Oracle-defined companion effects."""
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import COLORS, card, cost_from_oracle

FAMILY = 'mill_remaining_costs_and_grants'
PROFILE = Path(__file__).with_name('data') / 'mill_remaining_costs_and_grants_wave8_profiles.json'
PENDANT, ASSIST, INTELLECT, MILLIKIN = 5516, 11928, 13697, 5611
CARD_NAMES = {PENDANT: 'Charmed Pendant', ASSIST: 'Deranged Assistant',
              INTELLECT: 'Evanescent Intellect', MILLIKIN: 'Millikin'}
MODES = {
    PENDANT: ('colored', 'generic_cost', 'zero', 'hybrid', 'empty', 'response'),
    ASSIST: ('paid', 'spent', 'fresh', 'empty', 'tapped'),
    INTELLECT: ('mill_foe', 'mill_self', 'foreign_host', 'short_color', 'fresh_host', 'pending_removed', 'tapped_host'),
    MILLIKIN: ('paid', 'spent', 'fresh', 'empty', 'tapped'),
}
PREFIX = 'MillRemainingCostsAndGrantsRecipe'
SEED = 2809
HAND, BATTLEFIELD, LIBRARY, GRAVEYARD = range(4)
TAPPED = 1
ZERO = [0] * 6
FOREST, PLAINS, ISLAND, MOUNTAIN = 'Forest', 'Plains', 'Island', 'Mountain'
BEAR = 'Grizzly Bears'
FIXED, GENERIC, HYBRID = 'Undermine', 'Archaeomancer', 'Boros Recruit'
SPEND, INITIATOR, ANSWER = 'Bonesplitter', 'Think Twice', 'Disenchant'
WITNESSES = {FOREST, PLAINS, ISLAND, MOUNTAIN, BEAR, FIXED, GENERIC, HYBRID, SPEND, INITIATOR, ANSWER}


PHENAX, SLIVER, MENTOR, CONCOCTION = 13780, 8658, 12476, 15955
FOLLOW_MODES = {
    PHENAX: ('grant_low', 'change_toughness', 'devotion_threshold', 'opponent_only'),
    SLIVER: ('cast', 'countered', 'no_printed_hand_ability'),
    MENTOR: ('paired_both', 'decline', 'pair_later', 'partner_leaves'),
    CONCOCTION: ('destroy', 'target_leaves', 'empty_deck', 'empty_hand', 'wrong_color', 'no_target'),
}
CARD_NAMES.update({
    PHENAX: 'Phenax, God of Deception', SLIVER: 'Screeching Sliver',
    MENTOR: 'Stern Mentor', CONCOCTION: 'Sinister Concoction',
})
TURTLE, FROST, BLACK_DOUBLE = 'Horned Turtle', 'Wall of Frost', 'Dread Warlock'
GROWTH, BOUNCE, COUNTER = 'Giant Growth', 'Unsummon', 'Counterspell'
WITNESSES.update((TURTLE, FROST, BLACK_DOUBLE, GROWTH, BOUNCE, COUNTER))
ZONES = ('hand', 'battlefield', 'library', 'graveyard')
ZONE_OPS = {'assert_zone', 'assert_zone_card', 'assert_zone_contains', 'assert_zone_multiplicity'}
INDEXED = {'assert_zone_card', 'assert_zone_multiplicity'}
SEATED = {'turn', 'untap_step', 'choose_player', 'choose_dialog', 'choose_target', 'set_mana', 'assert_mana',
          'assert_mana_total', 'assert_life', 'choose_trigger', 'choose_optional_target'} | ZONE_OPS
VALUES = {'choose_player', 'choose_dialog', 'cast', 'cast_response', 'activate', 'activate_response',
          'set_mana', 'assert_mana', 'assert_mana_total', 'assert_life', 'assert_card', 'assert_stack',
          'require_can_activate', 'can_activate', 'can_target_creature', 'can_activate_hand',
          'discard_policy', 'assert_forced_target_choices'} | ZONE_OPS
GAPS = {
    PENDANT: ['Hybrid permits either printed color. The implementation offers no choice dialog, and this test verifies one mana from the legal pair.'],
    INTELLECT: ['Separate gameplay UI authorization for a foreign host is outside the direct native provider route.'],
    SLIVER: ['The printed grant uses two unattached internal activated effects. Current test bindings hide both effects, and the attached-legacy activation API requires a recipient attachment. The grant remains untested.'],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (not pin or not exact(pin['oracle'], record) or pin['registry']['csvid'] != csvid or csvid not in CARD_NAMES
            or 'Mill' not in record.get('keywords', ()) or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole pinned normal mill source required')
    return dict(csvid=csvid, mana=cost_from_oracle(record))


def variants(shape):
    csvid = shape['csvid']
    common = MODES.get(csvid, FOLLOW_MODES.get(csvid))
    return [f'{mode}_{seat}' for mode in common for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported mill cost and grant variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('reserved mill cost and grant source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared mill cost and grant parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent mill cost and grant witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != CARD_NAMES.keys():
        raise ValueError('exact eight-source mill cost and grant profile required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact mill cost and grant source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented mill cost and grant source required')
                classify(oracle[name], row['csvid'])


class Board:
    def __init__(self, csvid, mode, owner, rows, oracle):
        self.id, self.mode, self.owner, self.foe = csvid, mode, owner, 1-owner
        self.name, self.rows, self.oracle = CARD_NAMES[csvid], rows, oracle
        self.setup, self.steps = [], []
        self.decks = [[ISLAND, MOUNTAIN, BEAR, GENERIC, FIXED], [PLAINS, FOREST, BEAR, HYBRID, FIXED]]
        self.graves = [[MOUNTAIN], [PLAINS]]
        self.hands = [[], []]
        self.pools = [ZERO.copy(), ZERO.copy()]
        self.entities = {}
        self.depth, self.milled = 0, []
        self.host_actor = self.foe if mode == 'foreign_host' else self.owner
        self.target = self.owner if mode in ('mill_self', 'foreign_host') else self.foe
        if csvid in FOLLOW_MODES:
            self.setup_follow()
        elif csvid == INTELLECT:
            self.add('source', self.name, self.owner, HAND)
            self.add('host', BEAR, self.host_actor, HAND if mode == 'fresh_host' else BATTLEFIELD,
                     tapped=mode == 'tapped_host')
            self.add('other_host', BEAR, 1-self.host_actor, BATTLEFIELD)
            if mode == 'pending_removed':
                self.add('answer', ANSWER, self.foe, HAND)
        else:
            cast = mode in ('paid', 'colored', 'fresh')
            self.add('source', self.name, self.owner, HAND if cast else BATTLEFIELD, tapped=mode == 'tapped')
            if mode == 'spent':
                self.add('spend', SPEND, self.owner, HAND)
            elif mode == 'response':
                self.add('initiator', INITIATOR, self.foe, HAND)
            top = {'generic_cost': GENERIC, 'zero': FOREST, 'hybrid': HYBRID}.get(mode, FIXED)
            self.decks[self.owner] = [] if mode == 'empty' else [top, ISLAND, BEAR, PLAINS]
        for seat in (0, 1):
            for zone, items in ((LIBRARY, self.decks[seat]), (GRAVEYARD, self.graves[seat])):
                for index, name in enumerate(items):
                    self.setup.append(card(name, f'zone_{zone}_{seat}_{index}', seat, ZONES[zone]))

    def add(self, ref, name, owner, zone, *, tapped=False):
        placement = card(name, ref, owner, ZONES[zone])
        if tapped:
            placement['state'] = ['tapped']
        self.setup.append(placement)
        self.entities[ref] = dict(name=name, owner=owner, zone=zone, tapped=int(tapped))
        if zone == HAND:
            self.hands[owner].append(name)

    def move(self, ref, zone):
        card = self.entities[ref]
        owner, name = card['owner'], card['name']
        if card['zone'] == HAND:
            self.hands[owner].remove(name)
        card['zone'] = zone
        if zone == GRAVEYARD:
            self.graves[owner].append(name)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        action = dict(op=op)
        if ref:
            action['ref'] = ref
        if other:
            action['other'] = other
        if prop:
            action['property'] = prop
        if player or op in SEATED:
            action['player'] = player
        if value or op in VALUES:
            action['value'] = value
        if op in INDEXED:
            action['index'] = index
        if op in ZONE_OPS:
            action['zone'] = ZONES[zone]
        if op in ('set_mana', 'assert_mana'):
            action['color'] = color
        self.steps.append(action)

    def see(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def fund(self, player, pool):
        self.pools[player] = list(pool)
        for color, count in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=count)
        self.emit('refresh')

    def see_mana(self, player):
        if self.mode == 'hybrid' and player == self.owner and self.entities['source']['tapped']:
            self.emit('assert_mana_total', player=player, value=1)
            colors = (0, COLORS['B'], COLORS['U'], COLORS['G'])
        else:
            colors = range(6)
        for color in colors:
            self.emit('assert_mana', player=player, color=color, value=self.pools[player][color])

    def receipts(self, *, stack=True):
        if stack:
            self.emit('assert_stack', value=self.depth)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=20)
            self.see_mana(player)
            for zone, names in ((LIBRARY, self.decks[player]), (GRAVEYARD, self.graves[player]),
                                (HAND, self.hands[player])):
                # Native hand counts still include the active slot of a pending cast.
                if zone == HAND and any(item['zone'] == 'stack' and item['owner'] == player
                                         for item in self.entities.values()):
                    continue
                self.emit('assert_zone', player=player, zone=zone, value=len(names))
                for index, name in enumerate(names):
                    op = 'assert_zone_contains' if zone == HAND else 'assert_zone_card'
                    self.emit(op, player=player, zone=zone, index=0 if zone == HAND else index,
                              value=self.rows[name]['csvid'])
        for ref, card in self.entities.items():
            if card['zone'] != 'stack':
                self.see(ref, 'in_play', int(card['zone'] == BATTLEFIELD))
            if card['zone'] == BATTLEFIELD:
                self.see(ref, 'tapped', card['tapped'])
                oracle = self.oracle[card['name']]
                if 'Creature' in oracle['type_line']:
                    for property in ('power', 'toughness'):
                        self.see(ref, property, int(oracle[property]))

    def cast(self, ref, *, target=None, fund=True, response=False):
        card = self.entities[ref]
        actor = card['owner']
        fee = cost_from_oracle(self.oracle[card['name']])
        if fund:
            self.fund(actor, fee)
        else:
            assert self.pools[actor] == fee
        if target:
            self.emit('choose_target', target, player=actor)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.pools[actor] = ZERO.copy()
        self.move(ref, 'stack')
        self.depth += 1
        self.emit('assert_stack', value=self.depth)
        self.see_mana(actor)

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.depth = 0

    def play_permanent(self, ref, *, target=None, fund=True):
        self.cast(ref, target=target, fund=fund)
        self.resolve()
        self.move(ref, BATTLEFIELD)

    def mill(self, player, count):
        deck = self.decks[player]
        batch = deck[:count]
        self.graves[player].extend(batch)
        self.milled.extend(batch)
        del deck[:count]
        return batch

    def reject(self):
        self.emit('require_can_activate', 'source', value=0)
        self.emit('activate', 'source', value=0)
        self.emit('assert_choices_consumed')
        self.receipts()

    def source_mana(self):
        mode = self.mode
        if mode in ('paid', 'colored', 'fresh'):
            self.play_permanent('source')
        if mode == 'paid':
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.emit('advance_turn')
            self.emit('untap_step', player=self.owner)
        self.receipts()
        if mode in ('empty', 'tapped', 'fresh'):
            self.reject()
            return
        if mode == 'response':
            self.cast('initiator')
            self.emit('announce_spell', 'initiator')
        self.emit('require_can_activate', 'source', value=1)
        self.emit('activate_response' if mode == 'response' else 'activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        top, = self.mill(self.owner, 1)
        self.entities['source']['tapped'] = 1
        if self.id != PENDANT:
            self.pools[self.owner][0] = 1
        elif mode != 'hybrid' and self.oracle[top]['mana_cost']:
            self.pools[self.owner] = cost_from_oracle(self.oracle[top])
            self.pools[self.owner][0] = 0
        # Both the milled identity and produced mana are observed immediately,
        # before draining any engine activation bookkeeping.
        self.receipts(stack=False)
        self.resolve()
        if mode == 'response':
            self.move('initiator', GRAVEYARD)
            self.hands[self.foe].append(self.decks[self.foe].pop(0))
        self.receipts()
        if mode == 'spent':
            self.play_permanent('spend', fund=False)
            self.receipts()

    def aura(self):
        if self.mode == 'fresh_host':
            self.play_permanent('host')
        self.play_permanent('source', target='host')
        self.receipts()
        actor = self.host_actor
        paid = self.mode not in ('short_color', 'fresh_host', 'tapped_host')
        budget = [1, 0, 1, 0, 0, 0]
        if self.mode == 'short_color':
            budget = [2, 0, 0, 0, 0, 0]
        self.fund(actor, budget)
        if not paid:
            self.reject()
            return
        self.emit('require_can_activate', 'source', value=1)
        # The native Aura provider presents the target selector, while the real
        # attached recipient pays the fee and taps.
        self.emit('choose_player', player=self.owner, value=self.target)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.pools[actor] = ZERO.copy()
        self.entities['host']['tapped'] = 1
        self.depth = 1
        self.receipts()
        if self.mode == 'pending_removed':
            self.cast('answer', target='source')
        self.resolve()
        if self.mode == 'pending_removed':
            self.move('answer', GRAVEYARD)
            self.move('source', GRAVEYARD)
        self.mill(self.target, 3)
        self.receipts()

    def run(self):
        active = self.foe if self.mode == 'response' else self.owner
        self.emit('turn', player=active)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        if self.id == INTELLECT:
            self.aura()
        else:
            self.source_mana()



class FollowBoard(Board):
    def __init__(self, csvid, mode, owner, rows, oracle):
        self.life = [20, 20]
        self.phenax_creature = False
        self.bonuses = {}
        self.concoction_paid = False
        super().__init__(csvid, mode, owner, rows, oracle)

    def setup_follow(self):
        mode = self.mode
        self.add('source', self.name, self.owner, HAND)
        if self.id == PHENAX:
            self.decks[self.foe] = [FIXED, ISLAND, BEAR, FOREST, HYBRID, PLAINS, GENERIC, MOUNTAIN]
            self.add('foreign_creature', BLACK_DOUBLE, self.foe, BATTLEFIELD)
            self.add('probe', GROWTH, self.owner, HAND)
            if mode != 'opponent_only':
                self.add('host', TURTLE, self.owner, BATTLEFIELD)
            if mode == 'devotion_threshold':
                self.add('blue_double', FROST, self.owner, BATTLEFIELD)
                self.add('black_double', BLACK_DOUBLE, self.owner, BATTLEFIELD)
                self.add('bounce', BOUNCE, self.foe, HAND)
                self.add('answer', ANSWER, self.foe, HAND)
        elif self.id == SLIVER:
            if mode == 'countered':
                self.add('counter', COUNTER, self.foe, HAND)
        elif self.id == MENTOR:
            self.add('rival', TURTLE, self.foe, BATTLEFIELD)
            self.add('partner', BEAR, self.owner, HAND if mode == 'pair_later' else BATTLEFIELD)
            if mode == 'partner_leaves':
                self.add('bounce', BOUNCE, self.foe, HAND)
        else:
            if mode != 'empty_hand':
                self.add('discard', FOREST, self.owner, HAND)
            if mode != 'no_target':
                self.add('victim', BEAR, self.foe, BATTLEFIELD)
            if mode == 'target_leaves':
                self.add('bounce', BOUNCE, self.foe, HAND)
            if mode == 'empty_deck':
                self.decks[self.owner] = []

    def receipts(self, *, stack=True):
        begin = len(self.steps)
        super().receipts(stack=stack)
        revised = []
        for action in self.steps[begin:]:
            if action['op'] == 'assert_life':
                action['value'] = self.life[action['player']]
            if action['op'] == 'assert_card' and action.get('property') in ('power', 'toughness'):
                ref = action['ref']
                if ref == 'source' and self.id == PHENAX and not self.phenax_creature:
                    continue
                action['value'] += self.bonuses.get(ref, 0)
            if (self.concoction_paid and action['op'] == 'assert_zone_card' and action.get('zone') == 'graveyard'
                    and action['player'] == self.owner and action['index'] > 0):
                # The identities of the paid costs matter even if the engine
                # orders simultaneous graveyard costs differently.
                continue
            revised.append(action)
        self.steps[begin:] = revised
        if self.concoction_paid:
            for name in sorted(set(self.graves[self.owner])):
                self.emit('assert_zone_multiplicity', player=self.owner, zone=GRAVEYARD,
                          index=self.rows[name]['csvid'], value=self.graves[self.owner].count(name))

    def bounce(self, ref):
        entity = self.entities[ref]
        self.move(ref, HAND)
        self.hands[entity['owner']].append(entity['name'])
        entity['tapped'] = 0

    def start_grant(self, ref, target, *, select_host=False, mode=None):
        self.emit('require_can_activate', 'source', value=1)
        if select_host:
            self.emit('choose_target', ref, player=self.owner)
        if mode is not None:
            self.emit('choose_dialog', player=self.owner, value=mode)
        self.emit('choose_player', player=self.owner, value=target)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.depth = 1
        self.entities[ref]['tapped'] = 1
        self.receipts()

    def finish_grant(self, recipient, count):
        self.resolve()
        self.mill(recipient, count)
        self.receipts()

    def source_is_creature(self):
        self.emit('can_target_creature', 'probe', 'source', value=int(self.phenax_creature))

    def phenax(self):
        self.play_permanent('source')
        self.phenax_creature = self.mode == 'devotion_threshold'
        self.source_is_creature()
        self.receipts()
        if self.mode == 'opponent_only':
            self.reject()
            return
        if self.mode == 'devotion_threshold':
            self.cast('answer', target='source')
            self.resolve()
            self.move('answer', GRAVEYARD)
            self.receipts()
            self.source_is_creature()
            self.cast('bounce', target='blue_double')
            self.resolve()
            self.move('bounce', GRAVEYARD)
            self.bounce('blue_double')
            self.phenax_creature = False
            self.source_is_creature()
            self.receipts()
        self.start_grant('host', self.foe, select_host=True)
        amount = int(self.oracle[TURTLE]['toughness'])
        if self.mode == 'change_toughness':
            self.cast('probe', target='host')
            self.resolve()
            self.move('probe', GRAVEYARD)
            self.bonuses['host'] = 3
            self.mill(self.foe, amount+3)
            self.receipts()
            self.emit('cleanup')
            self.bonuses.clear()
            self.receipts()
        else:
            self.finish_grant(self.foe, amount)

    def sliver(self):
        if self.mode == 'no_printed_hand_ability':
            self.fund(self.owner, [3, 0, 0, 0, 0, 0])
            self.emit('can_activate_hand', 'source', value=0)
            self.receipts()
        elif self.mode == 'countered':
            self.cast('source')
            self.emit('announce_spell', 'source')
            self.cast('counter', response=True)
            self.emit('resolve_response')
            self.emit('refresh')
            self.emit('assert_choices_consumed')
            self.depth = 0
            self.move('counter', GRAVEYARD)
            self.move('source', GRAVEYARD)
            self.receipts()
        else:
            self.play_permanent('source')
            self.receipts()

    def pair_choice(self, *, entrant='source'):
        if self.owner == 0:
            self.emit('choose_trigger', 'source', player=0)
        if entrant == 'source':
            action = 'choose_optional_target' if self.mode == 'decline' else 'choose_target'
            self.emit(action, 'partner', player=self.owner)

    def age_pair(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.emit('turn', player=self.owner)

    def mentor(self):
        self.cast('source')
        if self.mode != 'pair_later':
            self.pair_choice()
        self.resolve()
        self.move('source', BATTLEFIELD)
        self.receipts()
        if self.mode == 'decline':
            self.emit('assert_forced_target_choices', value=0)
            self.reject()
        elif self.mode == 'partner_leaves':
            self.cast('bounce', target='partner')
            self.resolve()
            self.move('bounce', GRAVEYARD)
            self.bounce('partner')
            self.age_pair()
            self.reject()
        elif self.mode == 'pair_later':
            self.age_pair()
            self.cast('partner')
            self.pair_choice(entrant='partner')
            self.resolve()
            self.move('partner', BATTLEFIELD)
            self.start_grant('source', self.owner)
            self.finish_grant(self.owner, 2)
        else:
            self.age_pair()
            self.start_grant('source', self.foe, mode=0)
            self.finish_grant(self.foe, 2)
            self.start_grant('partner', self.owner)
            self.finish_grant(self.owner, 2)
            self.reject()

    def concoction(self):
        self.play_permanent('source')
        fee = [0, 1, 0, 0, 0, 0]
        if self.mode == 'wrong_color':
            fee = [0, 0, 1, 0, 0, 0]
        self.fund(self.owner, fee)
        self.receipts()
        if self.mode == 'empty_deck':
            self.emit('can_activate', 'source', value=0)
            self.receipts()
            return
        if self.mode in ('empty_hand', 'wrong_color', 'no_target'):
            self.reject()
            return
        self.emit('discard_policy', value=1)
        self.emit('require_can_activate', 'source', value=1)
        self.emit('choose_target', 'victim', player=self.owner)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.depth = 1
        self.pools[self.owner] = ZERO.copy()
        self.life[self.owner] -= 1
        self.move('discard', GRAVEYARD)
        self.mill(self.owner, 1)
        self.move('source', GRAVEYARD)
        self.concoction_paid = True
        self.receipts()
        if self.mode == 'target_leaves':
            self.cast('bounce', target='victim')
        self.resolve()
        if self.mode == 'target_leaves':
            self.move('bounce', GRAVEYARD)
            self.bounce('victim')
        else:
            self.move('victim', GRAVEYARD)
        self.receipts()

    def run(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        action = {PHENAX: self.phenax, SLIVER: self.sliver, MENTOR: self.mentor, CONCOCTION: self.concoction}
        action[self.id]()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board_type = FollowBoard if csvid in FOLLOW_MODES else Board
    board = board_type(csvid, mode, int(seat), rows, oracle)
    board.run()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, [])))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered mill cost and grant source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
