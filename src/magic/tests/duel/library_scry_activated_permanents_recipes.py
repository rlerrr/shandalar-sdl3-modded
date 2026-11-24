"""Activated Scry with exact fees, library choices and printed riders."""
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import COLORS, card, cost_from_oracle

FAMILY = 'library_scry_activated_permanents'
PROFILE = Path(__file__).with_name('data') / 'library_scry_activated_permanents_wave8_profiles.json'
BALL, PENDANT, STARFISH, STORM = 11109, 6874, 13975, 13814
CARD_NAMES = {BALL: 'Crystal Ball', PENDANT: 'Darksteel Pendant',
              STARFISH: 'Sigiled Starfish', STORM: 'Stormcaller of Keranos'}
MODES = {
    BALL: ('choose_one', 'bottom_both', 'keep', 'one_card', 'empty', 'short_mana', 'tapped', 'removed_pending'),
    PENDANT: ('bottom', 'keep', 'resists_destroy', 'exile_pending', 'tapped', 'short_mana'),
    STARFISH: ('bottom', 'keep', 'fresh_sick', 'aged', 'tapped', 'empty'),
    STORM: ('bottom', 'keep', 'short_blue', 'short_generic', 'stack_repeat', 'fresh_haste', 'tapped_scry'),
}
HUMAN_ONLY = {BALL: ('reorder',)}
PREFIX = 'LibraryScryActivatedPermanentsRecipe'
SEED = 2857
HAND, BATTLEFIELD, LIBRARY, GRAVEYARD, EXILE = range(5)
TAPPED = 1
ZERO = [0] * 6
BEAR, BOLT, SQUIRE = 'Grizzly Bears', 'Lightning Bolt', 'Squire'
FOREST, PLAINS, ISLAND, MOUNTAIN = 'Forest', 'Plains', 'Island', 'Mountain'
SHATTER, DUST = 'Shatter', 'Return to Dust'
SCRY_DECK = (BEAR, BOLT, ISLAND, PLAINS, FOREST, SQUIRE)
WITNESSES = set(SCRY_DECK + (MOUNTAIN, SHATTER, DUST))
FEE = {BALL: (1, 0, 0, 0, 0, 0), PENDANT: (1, 0, 0, 0, 0, 0),
       STARFISH: (0, 0, 0, 0, 0, 0), STORM: (1, 0, 1, 0, 0, 0)}
TAP_COST = (BALL, PENDANT, STARFISH)


CHIMERA, SEER, PRISM, LANTERN, THEORIST, THRASIOS, STOCKPILE, AUTOMATON, BONTU = (
    13988, 11187, 15404, 15677, 16272, 16635, 16720, 16823, 16853
)
FOLLOW_MODES = {
    CHIMERA: ('pump_bottom', 'flight', 'short_red', 'empty_deck'),
    SEER: ('self', 'fresh_self'),
    PRISM: ('one_crystal', 'two_crystals', 'scry_three', 'short_white'),
    LANTERN: ('mana', 'scry', 'two_printed', 'already_tapped'),
    THEORIST: ('first_energy', 'exhaust', 'fresh'),
    THRASIOS: ('land', 'draw', 'empty_deck', 'short_generic'),
    STOCKPILE: ('revolt_sac', 'foe_departure', 'no_own_creature'),
    AUTOMATON: ('tapped_scry', 'fresh', 'short_blue'),
    BONTU: ('no_other', 'need_death', 'menace_after_sac', 'block_before_sac', 'block_after_sac', 'indestructible'),
}
HUMAN_ONLY[SEER] = ('other_creature',)
CARD_NAMES.update({CHIMERA: 'Stormchaser Chimera', SEER: 'Viscera Seer', PRISM: 'Prism Array',
                   LANTERN: "Seer's Lantern", THEORIST: 'Aether Theorist', THRASIOS: 'Thrasios, Triton Hero',
                   STOCKPILE: 'Hidden Stockpile', AUTOMATON: 'Watchful Automaton', BONTU: 'Bontu the Glorified'})
FEE.update({CHIMERA: (2, 0, 1, 0, 1, 0), SEER: (0, 0, 0, 0, 0, 0),
            PRISM: (0, 1, 1, 1, 1, 1), LANTERN: (2, 0, 0, 0, 0, 0),
            THEORIST: (0, 0, 0, 0, 0, 0), THRASIOS: (4, 0, 0, 0, 0, 0),
            STOCKPILE: (1, 0, 0, 0, 0, 0), AUTOMATON: (2, 0, 1, 0, 0, 0), BONTU: (1, 1, 0, 0, 0, 0)})
TAP_COST += (THEORIST, LANTERN)
SERVO, GIANT, BOUNCE, MURDER = 'Servo', 'Hill Giant', 'Unsummon', 'Murder'
WITNESSES.update((GIANT, BOUNCE, MURDER))
TOKENS = {SERVO}
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZONE_OPS = {'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'}
INDEXED = {'assert_zone_card', 'assert_zone_multiplicity', 'assert_matching_permanents', 'bind_permanent_copy'}
SEATED = {'turn', 'untap_step', 'choose_zone_card', 'choose_zone', 'choose_target', 'choose_dialog', 'choose_mana',
          'record_library', 'assert_scry_result', 'set_mana', 'assert_mana', 'assert_life', 'assert_energy',
          'assert_shuffle_count', 'assert_matching_permanents', 'bind_permanent_copy'} | ZONE_OPS
VALUES = {'cast', 'activate', 'require_can_activate', 'can_activate', 'assert_life', 'assert_energy', 'set_mana',
          'assert_mana', 'assert_stack', 'assert_shuffle_count', 'assert_card', 'assert_scry_result',
          'choose_zone', 'choose_zone_card', 'choose_dialog', 'attack', 'block', 'play_land',
          'require_activation_mode', 'assert_matching_permanents', 'bind_permanent_copy'} | ZONE_OPS
GAPS = {
    BALL: ['Seat one reorders retained cards in an automatic engine routine. The legal permutation and exact untouched suffix are checked independently.'],
    PRISM: ['Crystal counters are observed through the exact number of paid tap activations before exhaustion. The driver has no direct crystal counter reader.'],
    THRASIOS: ['Partner is a Commander deck-construction rule unavailable in this duel fixture. The paid in-game ability is exercised.'],
    SEER: ['Seat one uses the engine automatic selection of the only eligible creature. A separate-other-creature receipt uses seat zero.'],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (not pin or not exact(pin['oracle'], record) or pin['registry']['csvid'] != csvid or csvid not in CARD_NAMES
            or 'Scry' not in record.get('keywords', ()) or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole pinned normal scry source required')
    return dict(csvid=csvid, mana=cost_from_oracle(record))


def variants(shape):
    csvid = shape['csvid']
    common = MODES.get(csvid, FOLLOW_MODES.get(csvid))
    return [f'{mode}_{seat}' for mode in common for seat in (0, 1)] + [
        f'{mode}_0' for mode in HUMAN_ONLY.get(csvid, ())]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported activated Scry variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('reserved activated Scry source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared activated Scry parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses', 'tokens'} or set(pins['witnesses']) != WITNESSES
            or set(pins['tokens']) != TOKENS):
        raise ValueError('complete independent activated Scry witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != CARD_NAMES.keys():
        raise ValueError('exact thirteen-source activated Scry profile required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if group == 'tokens':
                if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                        or row.get('csvid') != 18816 or oracle[name].get('layout') != 'token'):
                    raise ValueError('exact printed Servo identity required')
                continue
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact activated Scry source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented activated Scry source required')
                classify(oracle[name], row['csvid'])


class Board:
    def __init__(self, csvid, mode, owner, rows, oracle):
        self.id, self.mode, self.owner, self.foe = csvid, mode, owner, 1-owner
        self.name, self.rows, self.oracle = CARD_NAMES[csvid], rows, oracle
        self.setup, self.steps = [], []
        self.lib = [[FOREST, MOUNTAIN, SQUIRE], [MOUNTAIN, FOREST, BOLT]]
        self.lib[owner] = list(SCRY_DECK if mode != 'one_card' else SCRY_DECK[:1]) if mode != 'empty' else []
        self.graves = [[MOUNTAIN], [PLAINS]]
        self.exiles = [[], []]
        self.pools = [ZERO.copy(), ZERO.copy()]
        self.life = [20, 20]
        self.entities = {}
        self.depth, self.scry_count = 0, 0
        self.pending_scry, self.order_unobserved = [], 0
        self.amount = 2 if csvid == BALL else 1
        self.active = self.foe if mode in ('bottom', 'stack_repeat') else self.owner
        cast = mode in ('resists_destroy', 'exile_pending', 'aged', 'fresh_sick', 'fresh_haste')
        self.add('source', self.name, self.owner, HAND if cast else BATTLEFIELD,
                 tapped=mode in ('tapped', 'tapped_scry'))
        if csvid in FOLLOW_MODES:
            self.setup_follow()
        if mode in ('resists_destroy', 'removed_pending', 'exile_pending'):
            self.add('answer', DUST if mode == 'exile_pending' else SHATTER, self.foe, HAND)
        for side in (0, 1):
            for zone, cards in ((LIBRARY, self.lib[side]), (GRAVEYARD, self.graves[side])):
                for index, name in enumerate(cards):
                    self.setup.append(card(name, f'zone_{zone}_{side}_{index}', side, ZONES[zone]))

    def add(self, ref, name, player, zone, *, tapped=False):
        placement = card(name, ref, player, ZONES[zone])
        if tapped:
            placement['state'] = ['tapped']
        self.setup.append(placement)
        self.entities[ref] = dict(name=name, player=player, zone=zone, tapped=int(tapped))

    def move(self, ref, zone):
        item = self.entities[ref]
        item['zone'] = zone
        if zone == GRAVEYARD:
            self.graves[item['player']].append(item['name'])
        elif zone == EXILE:
            self.exiles[item['player']].append(item['name'])

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
        if op in ('set_mana', 'assert_mana', 'choose_mana'):
            action['color'] = color
        self.steps.append(action)

    def see(self, ref, property, value):
        self.emit('assert_card', ref, prop=property, value=int(value))

    def fund(self, player, pool):
        self.pools[player] = list(pool)
        for color, count in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=count)
        self.emit('refresh')

    def see_mana(self, player):
        for color, amount in enumerate(self.pools[player]):
            self.emit('assert_mana', player=player, color=color, value=amount)

    def receipts(self):
        self.emit('assert_stack', value=self.depth)
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            self.see_mana(seat)
            self.emit('assert_shuffle_count', player=seat, value=0)
            hand = [entity['name'] for entity in self.entities.values() if entity['player'] == seat and entity['zone'] == HAND]
            for zone, cards in ((HAND, hand), (LIBRARY, self.lib[seat]), (GRAVEYARD, self.graves[seat]), (EXILE, self.exiles[seat])):
                if zone == HAND and any(item['zone'] == 'stack' and item['player'] == seat
                                       for item in self.entities.values()):
                    continue
                self.emit('assert_zone', player=seat, zone=zone, value=len(cards))
                for index, name in enumerate(cards):
                    if zone == LIBRARY and seat == self.owner and index < self.order_unobserved:
                        continue
                    self.emit('assert_zone_card', player=seat, zone=zone, index=index, value=self.rows[name]['csvid'])
        for ref, item in self.entities.items():
            if item['zone'] == 'stack':
                continue
            self.see(ref, 'in_play', item['zone'] == BATTLEFIELD)
            if item['zone'] == BATTLEFIELD:
                self.see(ref, 'tapped', item['tapped'])
                row = self.oracle[item['name']]
                if 'Creature' in row['type_line']:
                    self.see(ref, 'power', int(row['power']))
                    self.see(ref, 'toughness', int(row['toughness']))

    def cast(self, ref, target=None):
        item = self.entities[ref]
        seat = item['player']
        self.fund(seat, cost_from_oracle(self.oracle[item['name']]))
        if target:
            self.emit('choose_target', target, player=seat)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.pools[seat] = ZERO.copy()
        self.move(ref, 'stack')
        self.depth += 1
        self.emit('assert_stack', value=self.depth)
        self.see_mana(seat)

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.depth = 0

    def cast_source(self):
        self.cast('source')
        self.resolve()
        self.move('source', BATTLEFIELD)
        self.receipts()

    def reject(self):
        self.emit('require_can_activate', 'source', value=0)
        self.emit('activate', 'source', value=0)
        self.emit('assert_choices_consumed')
        self.receipts()

    def activate(self):
        self.emit('require_can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.depth += 1
        self.pools[self.owner] = [a-b for a, b in zip(self.pools[self.owner], FEE[self.id])]
        if self.id in TAP_COST:
            self.entities['source']['tapped'] = 1
        self.receipts()

    def bottom(self, index):
        library = self.lib[self.owner]
        name = library.pop(index)
        self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])
        self.pending_scry.append(name)

    def scry_plan(self, selected):
        lib = self.lib[self.owner]
        view = min(self.amount, len(lib))
        if not view:
            return
        self.pending_scry = []
        if selected in ('bottom', 'both'):
            for _ in range(view if selected == 'both' else 1):
                self.bottom(0)
        elif selected == 'one':
            self.bottom(1)
        remaining = view - len(self.pending_scry)
        if remaining:
            self.emit('choose_zone', player=self.owner, value=-1)
            if remaining == 2 and self.owner == 0:
                if selected == 'reorder':
                    self.emit('choose_zone', player=self.owner, value=1)
                    lib[0], lib[1] = lib[1], lib[0]
                self.emit('choose_zone', player=self.owner, value=-1)
            elif remaining == 2:
                # Seat 1's engine chooses a top order. Preserve the reviewed
                # no-bottom choice and exact unchanged suffix independently.
                self.order_unobserved = 2
        lib.extend(self.pending_scry)
        self.scry_count += 1

    def before_attack(self):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.entities['source']['tapped'] = 1

    def finish_attack(self):
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.see('source', 'blocked', 0)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.life[self.foe] -= 2
        self.receipts()

    def run(self):
        self.emit('turn', player=self.active)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        self.receipts()
        if self.entities['source']['zone'] == HAND:
            self.cast_source()
        if self.mode in ('fresh_sick', 'aged'):
            self.reject()
            if self.mode == 'fresh_sick':
                return
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.emit('advance_turn')
            self.emit('untap_step', player=self.owner)
        if self.mode == 'resists_destroy':
            self.cast('answer', target='source')
            self.resolve()
            self.move('answer', GRAVEYARD)
            self.receipts()
        if self.mode == 'fresh_haste':
            self.before_attack()
        fee = list(FEE[self.id])
        if self.mode == 'short_mana':
            fee[0] = 0
        elif self.mode == 'short_blue':
            fee = [2, 0, 0, 0, 0, 0]
        elif self.mode == 'short_generic':
            fee[0] = 0
        elif self.mode == 'stack_repeat':
            fee = [2*x for x in fee]
        self.fund(self.owner, fee)
        self.receipts()
        if self.mode in ('tapped', 'short_mana', 'short_blue', 'short_generic'):
            self.reject()
            return
        self.emit('record_library', player=self.owner)
        self.activate()
        if self.mode == 'stack_repeat':
            self.activate()
        if self.mode in ('removed_pending', 'exile_pending'):
            self.cast('answer', target='source')
        choices = {'choose_one': 'one', 'bottom_both': 'both', 'reorder': 'reorder',
                   'keep': 'keep', 'removed_pending': 'one'}
        self.scry_plan(choices.get(self.mode, 'bottom'))
        if self.mode == 'stack_repeat':
            self.scry_plan('bottom')
        self.resolve()
        if self.mode in ('removed_pending', 'exile_pending'):
            self.move('answer', GRAVEYARD)
            self.move('source', EXILE if self.mode == 'exile_pending' else GRAVEYARD)
        self.emit('assert_scry_result', player=self.owner, value=2 if self.mode == 'stack_repeat' else self.amount)
        self.receipts()
        if self.mode == 'fresh_haste':
            self.finish_attack()


class FollowerBoard(Board):
    def __init__(self, csvid, mode, owner, rows, oracle):
        self.energy = 0
        self.bonus = 0
        self.servo = False
        super().__init__(csvid, mode, owner, rows, oracle)

    def setup_follow(self):
        self.lib[self.owner] = [BEAR, BOLT, SQUIRE, ISLAND, PLAINS, FOREST]
        if self.mode == 'empty_deck':
            self.lib[self.owner] = []
        if self.id == THRASIOS and self.mode == 'land':
            self.lib[self.owner][:2] = [BOLT, FOREST]
            self.lib[self.owner][-1] = BEAR
        if self.id in (PRISM, THEORIST, THRASIOS) or self.mode in ('fresh_self', 'fresh'):
            self.entities['source']['zone'] = HAND
            self.setup[0]['zone'] = 'hand'
        if self.id == CHIMERA and self.mode == 'flight':
            self.add('blocker', BEAR, self.foe, BATTLEFIELD)
        if self.id == SEER and self.mode == 'other_creature':
            self.add('fodder', BEAR, self.owner, BATTLEFIELD)
        if self.id == PRISM:
            self.amount = 3
            self.add('tap_target', BEAR, self.foe, BATTLEFIELD)
            self.add('tap_other', SQUIRE, self.owner, BATTLEFIELD)
        if self.id == LANTERN and self.mode == 'already_tapped':
            self.entities['source']['tapped'] = 1
            self.setup[0]['state'] = ['tapped']
        if self.id == STOCKPILE:
            if self.mode == 'revolt_sac':
                self.add('fodder', BEAR, self.owner, BATTLEFIELD)
            if self.mode in ('foe_departure', 'no_own_creature'):
                self.add('enemy', BEAR, self.foe, BATTLEFIELD)
            if self.mode == 'foe_departure':
                self.add('bounce', BOUNCE, self.owner, HAND)
        if self.id == BONTU:
            if self.mode in ('menace_after_sac', 'block_after_sac'):
                self.add('fodder', BEAR, self.owner, BATTLEFIELD)
            if self.mode == 'menace_after_sac':
                self.add('blocker', BEAR, self.foe, BATTLEFIELD)
            if self.mode.startswith('block_'):
                self.active = self.foe
                self.add('attacker', GIANT, self.foe, BATTLEFIELD)
            if self.mode == 'indestructible':
                self.add('murder', MURDER, self.foe, HAND)

    def receipts(self):
        start = len(self.steps)
        super().receipts()
        if self.id == THRASIOS and self.mode == 'draw':
            self.steps[start:] = [action for action in self.steps[start:] if action.get('ref') != 'found']
        for action in self.steps[start:]:
            if action['op'] == 'assert_card' and action.get('ref') == 'source' and action['property'] == 'power':
                action['value'] += self.bonus
        if self.id == THEORIST:
            self.emit('assert_energy', player=self.owner, value=self.energy)
            self.emit('assert_energy', player=self.foe, value=0)
        if self.id == STOCKPILE:
            csvid = self.rows[SERVO]['csvid']
            for seat in (0, 1):
                self.emit('assert_zone_multiplicity', player=seat, zone=BATTLEFIELD, index=csvid,
                          value=int(self.servo and self.owner == seat))
            if self.servo:
                for prop, value in (('power', 1), ('toughness', 1), ('artifact', 1), ('token', 1),
                                    ('subtype_Servo', 1), ('color', 0)):
                    self.emit('assert_matching_permanents', prop=prop, player=self.owner, index=csvid, value=value)

    def observe_scry(self, selection='bottom'):
        self.scry_plan(selection)
        self.resolve()
        self.emit('assert_scry_result', player=self.owner, value=self.amount)
        self.receipts()

    def ready_scry(self):
        self.emit('record_library', player=self.owner)
        self.activate()

    def pay_sacrifice(self, ref):
        self.emit('record_library', player=self.owner)
        self.emit('require_can_activate', 'source', value=1)
        if self.owner == 0:
            self.emit('choose_target', ref, player=self.owner)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.depth = 1
        self.pools[self.owner] = [have-fee for have, fee in zip(self.pools[self.owner], FEE[self.id])]
        self.move(ref, GRAVEYARD)
        self.receipts()

    def age(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.entities['source']['tapped'] = 0

    def strike(self, *, blocker=None, damage=0):
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', blocker, 'source', value=0)
        self.emit('advance_combat')
        self.see('source', 'blocked', 0)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.life[self.foe] -= damage
        self.receipts()

    def chimera(self):
        if self.mode == 'flight':
            self.before_attack()
        fee = list(FEE[self.id])
        if self.mode == 'short_red':
            fee[0] += fee[4]
            fee[4] = 0
        self.fund(self.owner, fee)
        if self.mode == 'short_red':
            self.reject()
            return
        self.ready_scry()
        self.scry_plan('keep' if self.mode == 'flight' else 'bottom')
        if self.lib[self.owner]:
            self.emit('choose_dialog', player=self.owner, value=0)
            self.bonus = int(self.oracle[self.lib[self.owner][0]]['cmc'])
        self.resolve()
        self.emit('assert_scry_result', player=self.owner, value=1)
        self.receipts()
        if self.mode == 'flight':
            self.strike(blocker='blocker', damage=2+self.bonus)
        self.emit('cleanup')
        self.bonus = 0
        self.receipts()

    def seer(self):
        if self.mode == 'fresh_self':
            self.cast_source()
        self.pay_sacrifice('fodder' if self.mode == 'other_creature' else 'source')
        self.observe_scry()

    def cast_prism(self):
        if self.mode == 'two_crystals':
            self.fund(self.owner, [3, 0, 1, 0, 1, 0])
            if self.owner == 0:
                self.emit('choose_mana', player=0, color=4)
            self.emit('cast', 'source', value=1)
            self.emit('assert_choices_consumed')
            self.pools[self.owner] = ZERO.copy()
            self.move('source', 'stack')
            self.emit('assert_stack', value=1)
            self.see_mana(self.owner)
            self.resolve()
            self.move('source', BATTLEFIELD)
            self.receipts()
        else:
            self.cast_source()

    def mode_intent(self, label):
        self.steps.append(dict(op='choose_activation_mode', ref='source', player=self.owner,
                               mode_label=label, fallback_color=0, allow_auto_nonmana=False))

    def prism(self):
        self.cast_prism()
        if self.mode == 'scry_three':
            self.fund(self.owner, FEE[self.id])
            self.mode_intent('Scry 3')
            self.ready_scry()
            self.emit('require_activation_mode', value=1)
            self.observe_scry('both')
            return
        refs = ('tap_target', 'tap_other') if self.mode == 'two_crystals' else ('tap_target',)
        for ref in refs:
            self.emit('require_can_activate', 'source', value=1)
            self.emit('choose_dialog', player=self.owner, value=0)
            self.emit('choose_target', ref, player=self.owner)
            self.emit('activate', 'source', value=1)
            self.emit('assert_choices_consumed')
            self.depth = 1
            self.receipts()
            self.resolve()
            self.entities[ref]['tapped'] = 1
            self.receipts()
        if self.mode == 'short_white':
            self.fund(self.owner, (1, 1, 1, 1, 1, 0))
        self.reject()

    def lantern(self):
        if self.mode in ('mana', 'already_tapped'):
            if self.mode == 'already_tapped':
                self.reject()
                return
            self.emit('require_can_activate', 'source', value=1)
            self.emit('choose_dialog', player=self.owner, value=0)
            self.emit('activate', 'source', value=1)
            self.emit('assert_choices_consumed')
            self.pools[self.owner][0] += 1
            self.entities['source']['tapped'] = 1
            self.receipts()
            return
        self.fund(self.owner, [2 if self.mode == 'two_printed' else 3, 0, 0, 0, 0, 0])
        self.mode_intent('Scry 1')
        self.emit('record_library', player=self.owner)
        self.emit('require_can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.emit('require_activation_mode', value=1)
        self.emit('assert_choices_consumed')
        self.depth = 1
        self.pools[self.owner][0] -= 2
        self.entities['source']['tapped'] = 1
        self.receipts()
        self.observe_scry()

    def theorist(self):
        self.cast('source')
        self.resolve()
        self.move('source', BATTLEFIELD)
        self.energy = 3
        self.receipts()
        self.reject()
        if self.mode == 'fresh':
            return
        for _ in range(3 if self.mode == 'exhaust' else 1):
            self.age()
            self.emit('record_library', player=self.owner)
            self.emit('require_can_activate', 'source', value=1)
            self.emit('activate', 'source', value=1)
            self.emit('assert_choices_consumed')
            self.depth = 1
            self.energy -= 1
            self.entities['source']['tapped'] = 1
            self.receipts()
            self.observe_scry()
            self.reject()
        if self.mode == 'exhaust':
            self.age()
            self.reject()

    def thrasios(self):
        self.cast_source()
        if self.mode == 'short_generic':
            self.fund(self.owner, (3, 0, 0, 0, 0, 0))
            self.reject()
            return
        self.fund(self.owner, FEE[self.id])
        self.ready_scry()
        self.scry_plan('bottom' if self.mode == 'land' else 'keep')
        if self.lib[self.owner]:
            self.emit('choose_dialog', player=self.owner, value=0)
        self.resolve()
        if self.lib[self.owner]:
            name = self.lib[self.owner].pop(0)
            land = self.mode == 'land'
            self.entities['found'] = dict(name=name, player=self.owner, zone=BATTLEFIELD if land else HAND,
                                          tapped=int(land))
            if land:
                identity = self.rows[name]['csvid']
                self.emit('bind_permanent_copy', 'found', player=self.owner, index=0, value=identity)
        self.receipts()

    def stockpile(self):
        if self.mode == 'foe_departure':
            self.cast('bounce', target='enemy')
            self.resolve()
            self.move('bounce', GRAVEYARD)
            self.move('enemy', HAND)
        elif self.mode == 'no_own_creature':
            self.fund(self.owner, FEE[self.id])
            self.reject()
            return
        else:
            self.fund(self.owner, FEE[self.id])
            self.pay_sacrifice('fodder')
            self.observe_scry()
        self.emit('advance_turn')
        self.servo = self.mode == 'revolt_sac'
        self.receipts()

    def automaton(self):
        if self.mode == 'fresh':
            self.cast_source()
        fee = (3, 0, 0, 0, 0, 0) if self.mode == 'short_blue' else FEE[self.id]
        self.fund(self.owner, fee)
        if self.mode == 'short_blue':
            self.reject()
        else:
            self.ready_scry()
            self.observe_scry()

    def bontu_sacrifice(self):
        self.fund(self.owner, FEE[self.id])
        self.pay_sacrifice('fodder')
        self.scry_plan('bottom')
        self.resolve()
        self.life[self.owner] += 1
        self.life[self.foe] -= 1
        self.emit('assert_scry_result', player=self.owner, value=1)
        self.receipts()

    def bontu_block(self):
        self.emit('begin_attackers')
        self.emit('attack', 'attacker', value=1)
        self.emit('finish_attackers')
        self.entities['attacker']['tapped'] = 1
        allowed = self.mode == 'block_after_sac'
        if allowed:
            self.bontu_sacrifice()
        self.emit('begin_blockers')
        self.emit('block', 'source', 'attacker', value=int(allowed))
        self.emit('advance_combat')
        self.see('attacker', 'blocked', allowed)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if allowed:
            self.move('attacker', GRAVEYARD)
        else:
            self.life[self.owner] -= 3
        self.receipts()

    def bontu(self):
        self.emit('reset_death_history')
        if self.mode == 'no_other':
            self.fund(self.owner, FEE[self.id])
            self.reject()
        elif self.mode.startswith('block_'):
            self.bontu_block()
        elif self.mode == 'menace_after_sac':
            self.bontu_sacrifice()
            self.before_attack()
            self.strike(blocker='blocker', damage=4)
        else:
            if self.mode == 'indestructible':
                self.cast('murder', target='source')
                self.resolve()
                self.move('murder', GRAVEYARD)
                self.receipts()
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.emit('finish_attackers')
            self.strike()

    def run(self):
        self.emit('turn', player=self.active)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        self.receipts()
        actions = {CHIMERA: self.chimera, SEER: self.seer, PRISM: self.prism, LANTERN: self.lantern,
                   THEORIST: self.theorist, THRASIOS: self.thrasios, STOCKPILE: self.stockpile,
                   AUTOMATON: self.automaton, BONTU: self.bontu}
        actions[self.id]()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board_type = FollowerBoard if csvid in FOLLOW_MODES else Board
    board = board_type(csvid, mode, int(seat), rows, oracle)
    board.run()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, [])))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered activated Scry source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
