"""Whole Oracle mill abilities with real payment, targeting and rider actions."""
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import COLORS, card, cost_from_oracle

FAMILY = 'mill_paid_abilities'
PROFILE = Path(__file__).with_name('data') / 'mill_paid_abilities_wave8_profiles.json'
CODEX, WEB, GRIND, MIND = 12685, 11438, 3161, 12023
CARD_NAMES = {CODEX: 'Codex Shredder', WEB: 'Decimator Web', GRIND: 'Grindstone', MIND: 'Mindshrieker'}
MODES = {
    CODEX: ('mill_foe', 'mill_self', 'recover', 'empty_mill', 'tapped'),
    WEB: ('six', 'short', 'empty', 'short_cost', 'player_ward'),
    GRIND: ('run', 'self_run', 'disjoint', 'colorless', 'short_cost', 'tapped'),
    MIND: ('buff', 'own_land', 'stacked_tapped', 'short_cost', 'flying'),
}
PREFIX = 'MillPaidAbilitiesRecipe'
SEED = 2771
HAND, BATTLEFIELD, LIBRARY, GRAVEYARD, EXILE = range(5)
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
TAPPED = 1
ZERO = [0] * 6
FOREST, ISLAND, PLAINS, MOUNTAIN = 'Forest', 'Island', 'Plains', 'Mountain'
SQUIRE, BEAR, GIANT = 'Squire', 'Grizzly Bears', 'Hill Giant'
WATCHWOLF, DRAKE, COUNTER = 'Watchwolf', 'Wind Drake', 'Counterspell'
BOLT, RITUAL, ORB = 'Lightning Bolt', 'Dark Ritual', 'Witchbane Orb'
DEFAULT_DECK = (GIANT, ISLAND, BEAR, SQUIRE, BOLT, RITUAL, PLAINS, WATCHWOLF)
RUN_DECK = (BEAR, WATCHWOLF, DRAKE, COUNTER, BOLT, RITUAL, ISLAND, FOREST)
WITNESSES = set(DEFAULT_DECK + RUN_DECK + (MOUNTAIN, ORB))


MINISTER, TIMEPIECE, STASH, RIVULET = 16453, 16478, 16512, 17162
NEW_MODES = {
    MINISTER: ('mill_foe', 'mill_self', 'exhaust', 'fresh_sick'),
    TIMEPIECE: ('mill', 'shuffle_one', 'shuffle_zero', 'short_cost'),
    STASH: ('mana', 'mill_new_artifact', 'mill_prior_artifact', 'mill_decline', 'mill_no_artifact'),
    RIVULET: ('colorless', 'blue', 'mill_foe', 'mill_self', 'short_color'),
}
HUMAN_ONLY = {TIMEPIECE: ('shuffle_two',), RIVULET: ('mill_other_desert',)}
CARD_NAMES.update({MINISTER: 'Minister of Inquiries', TIMEPIECE: 'Perpetual Timepiece',
                   STASH: 'Sequestered Stash', RIVULET: 'Ipnu Rivulet'})
ARTIFACT, PRIOR_ARTIFACT, DESERT = 'Ornithopter', 'Sol Ring', 'Desert'
WITNESSES.update((ARTIFACT, PRIOR_ARTIFACT, DESERT))
SEATED = {'turn', 'choose_dialog', 'choose_player', 'choose_zone_card', 'choose_zone', 'choose_color',
          'untap_step', 'set_mana', 'assert_mana', 'assert_life', 'assert_poison',
          'assert_energy', 'assert_shuffle_count'}
VALUES = {'assert_card', 'set_mana', 'assert_mana', 'assert_life', 'assert_poison', 'assert_stack',
          'cast', 'play_land', 'activate', 'attack', 'block', 'can_activate', 'require_can_activate',
          'choose_dialog', 'choose_player', 'choose_color', 'choose_zone', 'assert_unused_choices',
          'require_activation_mode', 'assert_energy', 'assert_shuffle_count'} 
ZONE_OPS = {'assert_zone', 'assert_zone_card', 'assert_zone_contains', 'assert_zone_multiplicity'}
SEATED |= ZONE_OPS
VALUES |= ZONE_OPS
INDEXED = {'assert_zone_card', 'assert_zone_multiplicity'}
GAPS = {
    GRIND: ['Odd terminal libraries are held separately because the native continuation evaluates a second card ID before validating the last pair size.'],
    MIND: ['The native effect reads a top-card ID before milling. Empty-library activations need isolated hard-case diagnosis.'],
    TIMEPIECE: ['The engine randomizes the seat-1 maximum number of targets, so that seat uses one-card and zero-card choices. The two-target case uses seat 0.'],
    RIVULET: ['Seat one chooses its Desert automatically with a single eligible witness. Choosing another Desert explicitly uses seat zero.'],
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
    mana = cost_from_oracle(record) if record['mana_cost'] else ZERO.copy()
    return dict(csvid=csvid, mana=mana)


def variants(shape):
    csvid = shape['csvid']
    common = MODES.get(csvid, NEW_MODES.get(csvid))
    return [f'{mode}_{seat}' for mode in common for seat in (0, 1)] + [
        f'{mode}_0' for mode in HUMAN_ONLY.get(csvid, ())]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported paid mill variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('reserved paid mill source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared paid mill parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent paid mill witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != CARD_NAMES.keys():
        raise ValueError('exact eight-source paid mill profile required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact paid mill source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented paid mill source required')
                classify(oracle[name], row['csvid'])


def grind_count(deck, oracle):
    count = 0
    while count < len(deck):
        pair = deck[count:count+2]
        count += len(pair)
        if len(pair) < 2 or not set(oracle[pair[0]]['colors']).intersection(oracle[pair[1]]['colors']):
            break
    return count


class Board:
    def __init__(self, csvid, mode, owner, rows, oracle):
        self.id, self.mode, self.owner, self.foe = csvid, mode, owner, 1-owner
        self.name, self.rows, self.oracle = CARD_NAMES[csvid], rows, oracle
        self.target = owner if mode in ('mill_self', 'self_run', 'own_land') else self.foe
        self.setup, self.steps = [], []
        self.decks = [[ISLAND, MOUNTAIN, SQUIRE], [PLAINS, FOREST, BEAR]]
        self.graves = [[MOUNTAIN], [PLAINS]]
        self.hands = [[], []]
        self.life, self.poison = [20, 20], [0, 0]
        self.stack, self.bonus, self.milled = 0, 0, []
        self.live, self.tapped = mode != 'buff', mode in ('tapped', 'stacked_tapped')
        self.pool = [[0] * 6, [0] * 6]
        self.configure_deck()
        self.add('source', self.name, self.owner, BATTLEFIELD if self.live else HAND,
                 TAPPED if self.tapped else 0)
        if not self.live:
            self.hands[self.owner].append(self.name)
        if mode == 'recover':
            self.graves[self.owner].extend((WATCHWOLF, GIANT))
            self.graves[self.foe].append(COUNTER)
        if mode == 'player_ward':
            self.add('ward', ORB, self.foe, BATTLEFIELD)
        if mode == 'flying':
            self.add('blocker', BEAR, self.foe, BATTLEFIELD)
        for seat in (0, 1):
            for kind, items in (('deck', self.decks[seat]), ('grave', self.graves[seat])):
                for index, card in enumerate(items):
                    self.add(f'{kind}_{seat}_{index}', card, seat, LIBRARY if kind == 'deck' else GRAVEYARD)

    def configure_deck(self):
        if self.mode in ('empty_mill', 'empty'):
            cards = ()
        elif self.id == GRIND and self.mode in ('run', 'self_run'):
            cards = RUN_DECK
        elif self.id == GRIND and self.mode == 'colorless':
            cards = (ISLAND, FOREST, BEAR, WATCHWOLF, PLAINS, MOUNTAIN)
        elif self.id == GRIND:
            cards = (BOLT, RITUAL, BEAR, WATCHWOLF, PLAINS, MOUNTAIN)
        elif self.mode == 'short':
            cards = (BOLT, BEAR)
        elif self.mode == 'own_land':
            cards = (ISLAND, GIANT, FOREST)
        elif self.mode == 'stacked_tapped':
            cards = (GIANT, BEAR, FOREST, BOLT)
        else:
            cards = DEFAULT_DECK
        self.decks[self.target] = list(cards)

    def add(self, ref, name, player, zone, flags=0):
        placement = card(name, ref, player, ZONES[zone])
        if flags:
            placement['state'] = ['tapped']
        self.setup.append(placement)

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

    def see(self, prop, value, ref='source'):
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def fund(self, player, pool):
        self.pool[player] = list(pool)
        for color, value in enumerate(pool):
            self.emit('set_mana', player=player, value=value, color=color)
        self.emit('refresh')

    def mana_receipts(self, player):
        for color, value in enumerate(self.pool[player]):
            self.emit('assert_mana', player=player, value=value, color=color)

    def observations(self):
        self.emit('assert_stack', value=self.stack)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_poison', player=player, value=self.poison[player])
            self.mana_receipts(player)
            for zone, cards in ((LIBRARY, self.decks[player]), (GRAVEYARD, self.graves[player]),
                                (HAND, self.hands[player])):
                self.emit('assert_zone', player=player, zone=zone, value=len(cards))
                for index, card in enumerate(cards):
                    op = 'assert_zone_contains' if zone == HAND else 'assert_zone_card'
                    self.emit(op, player=player, zone=zone, index=0 if zone == HAND else index,
                              value=self.rows[card]['csvid'])
        self.see('in_play', self.live)
        if self.live:
            self.see('tapped', self.tapped)
            if self.id == MIND:
                self.see('power', 1+self.bonus)
                self.see('toughness', 1+self.bonus)

    def play_mind(self):
        self.fund(self.owner, cost_from_oracle(self.oracle[self.name]))
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.pool[self.owner] = ZERO.copy()
        self.mana_receipts(self.owner)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.hands[self.owner].remove(self.name)
        self.live = True
        self.observations()

    def mill(self, count):
        deck = self.decks[self.target]
        batch = deck[:count]
        self.graves[self.target].extend(batch)
        self.milled.extend(batch)
        del deck[:count]
        return batch

    def runtime_cost(self):
        if self.id == CODEX:
            return 5 if self.mode == 'recover' else 0
        return {WEB: 4, GRIND: 3, MIND: 2}[self.id]

    def pay(self):
        cost = self.runtime_cost()
        self.emit('require_can_activate', 'source', value=1)
        if self.id == CODEX:
            self.emit('present_ai_dialogs', value=1)
            self.emit('choose_dialog', player=self.owner, value=int(self.mode == 'recover'))
            if self.mode == 'recover':
                self.emit('choose_zone_card', player=self.owner, value=self.rows[WATCHWOLF]['csvid'])
        # Web's printed opponent restriction supplies the only target directly.
        if self.id != WEB and self.mode != 'recover':
            self.emit('choose_player', player=self.owner, value=self.target)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.pool[self.owner][0] -= cost
        if self.id != MIND:
            self.tapped = True
        if self.mode == 'recover':
            self.live = False
            self.graves[self.owner].append(self.name)
        self.stack += 1
        self.observations()

    def effect(self):
        if self.id == CODEX:
            if self.mode == 'recover':
                self.graves[self.owner].remove(WATCHWOLF)
                self.hands[self.owner].append(WATCHWOLF)
            else:
                self.mill(1)
        elif self.id == WEB:
            self.life[self.target] -= 2
            self.poison[self.target] += 1
            self.mill(6)
        elif self.id == GRIND:
            self.mill(grind_count(self.decks[self.target], self.oracle))
        else:
            milled = self.mill(1)
            self.bonus += int(self.oracle[milled[0]]['cmc']) if milled else 0

    def resolve_activations(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        while self.stack:
            self.effect()
            self.stack -= 1
        self.observations()

    def combat_flying(self):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'source', value=0)
        self.emit('advance_combat')
        self.see('blocked', 0)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.tapped = True
        self.life[self.foe] -= 1+self.bonus
        self.observations()

    def run(self):
        own_turn = self.mode in ('buff', 'flying')
        active = self.owner if own_turn else self.foe
        self.emit('turn', player=active)
        self.fund(self.foe, ZERO)
        if self.mode == 'buff':
            self.play_mind()
        failed = self.mode in ('tapped', 'short_cost', 'player_ward')
        cost = self.runtime_cost()
        generic = (2 if self.mode == 'stacked_tapped' else 1)*cost - int(self.mode == 'short_cost')
        self.fund(self.owner, [generic, 0, 0, 0, 0, 0])
        self.observations()
        if failed:
            self.emit('can_activate', 'source', value=0)
            self.emit('activate', 'source', value=0)
            self.emit('assert_choices_consumed')
            self.observations()
            return
        self.pay()
        if self.mode == 'stacked_tapped':
            self.pay()
        self.resolve_activations()
        if self.id == MIND:
            if self.mode == 'flying':
                self.combat_flying()
            self.emit('advance_turn')
            self.emit('untap_step', player=1-active)
            if active != self.owner:
                self.tapped = False
            self.bonus = 0
            self.observations()


class FollowOnBoard(Board):
    def __init__(self, csvid, mode, owner, rows, oracle):
        super().__init__(csvid, mode, owner, rows, oracle)
        self.energy = [0, 0]
        self.shuffle = [0, 0]
        self.exiles = [[], []]
        if self.id in (MINISTER, STASH, RIVULET):
            self.live = False
            self.setup[0]['zone'] = 'hand'
            self.hands[self.owner].append(self.name)
        if self.id == TIMEPIECE and self.mode in ('shuffle_two', 'short_cost'):
            self.tapped = True
            self.setup[0]['state'] = ['tapped']
        if self.id == TIMEPIECE and self.mode == 'shuffle_two':
            self.graves[self.owner].append(SQUIRE)
            self.add('shuffle_extra', SQUIRE, self.owner, GRAVEYARD)
        if self.id == STASH and self.mode == 'mill_prior_artifact':
            self.graves[self.owner].append(PRIOR_ARTIFACT)
            self.add('prior_artifact', PRIOR_ARTIFACT, self.owner, GRAVEYARD)
        if self.id == RIVULET:
            self.add('non_desert', PLAINS, self.owner, BATTLEFIELD, TAPPED)
            if self.mode == 'mill_other_desert':
                self.add('cost_desert', DESERT, self.owner, BATTLEFIELD, TAPPED)

    def configure_deck(self):
        if self.id == MINISTER:
            super().configure_deck()
        elif self.id in (TIMEPIECE, STASH):
            self.target = self.owner
            deck = list(DEFAULT_DECK)
            if self.id == STASH and self.mode in ('mill_new_artifact', 'mill_decline'):
                deck[2] = ARTIFACT
            self.decks[self.owner] = deck
        elif self.mode.startswith('mill'):
            self.target = self.owner if self.mode == 'mill_self' else self.foe
            self.decks[self.target] = list(DEFAULT_DECK)

    def observations(self):
        start = len(self.steps)
        super().observations()
        if self.shuffle[self.owner]:
            # Oracle requires a shuffle and exact contents. Its random order is unprescribed.
            self.steps[start:] = [s for s in self.steps[start:] if not (
                s['op'] == 'assert_zone_card' and s.get('zone') == 'library' and s['player'] == self.owner)]
            for name in sorted(set(self.decks[self.owner])):
                self.emit('assert_zone_multiplicity', player=self.owner, zone=LIBRARY, index=self.rows[name]['csvid'],
                          value=self.decks[self.owner].count(name))
        for seat in (0, 1):
            self.emit('assert_energy', player=seat, value=self.energy[seat])
            self.emit('assert_shuffle_count', player=seat, value=self.shuffle[seat])
            self.emit('assert_zone', player=seat, zone=EXILE, value=len(self.exiles[seat]))
            for name in self.exiles[seat]:
                self.emit('assert_zone_contains', player=seat, zone=EXILE, value=self.rows[name]['csvid'])
        if self.id == RIVULET:
            self.see('in_play', 1, 'non_desert')
            if self.mode == 'mill_other_desert':
                self.see('in_play', int(DESERT not in self.graves[self.owner]), 'cost_desert')

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.stack = 0

    def reject(self):
        self.emit('require_can_activate', 'source', value=0)
        self.emit('activate', 'source', value=0)
        self.emit('assert_choices_consumed')
        self.observations()

    def chooser(self, name):
        self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])

    def paid_cast_minister(self):
        self.fund(self.owner, cost_from_oracle(self.oracle[self.name]))
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.pool[self.owner] = ZERO.copy()
        self.mana_receipts(self.owner)
        self.resolve()
        self.hands[self.owner].remove(self.name)
        self.live = True
        self.energy[self.owner] = 2
        self.observations()

    def own_untap(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.tapped = False
        self.observations()

    def minister(self):
        self.paid_cast_minister()
        self.reject()
        if self.mode == 'fresh_sick':
            return
        for _ in range(2 if self.mode == 'exhaust' else 1):
            self.own_untap()
            self.emit('require_can_activate', 'source', value=1)
            self.emit('choose_player', player=self.owner, value=self.target)
            self.emit('activate', 'source', value=1)
            self.emit('assert_choices_consumed')
            self.tapped = True
            self.energy[self.owner] -= 1
            self.stack = 1
            self.observations()
            self.resolve()
            self.mill(3)
            self.observations()
            self.reject()
        if self.mode == 'exhaust':
            self.own_untap()
            self.reject()

    def timepiece(self):
        mode = self.mode
        fee = 1 if mode == 'short_cost' else 2 if mode.startswith('shuffle') else 0
        self.fund(self.owner, [fee, 0, 0, 0, 0, 0])
        self.observations()
        if mode == 'short_cost':
            self.reject()
            return
        self.emit('require_can_activate', 'source', value=1)
        self.emit('present_ai_dialogs', value=1)
        if mode == 'mill':
            if self.owner == 1:
                self.emit('choose_dialog', player=self.owner, value=0)
            self.emit('activate', 'source', value=1)
            self.emit('assert_choices_consumed')
            self.stack, self.tapped = 1, True
            self.observations()
            self.resolve()
            self.mill(2)
        else:
            if not self.tapped:
                self.emit('choose_dialog', player=self.owner, value=1)
            selected = [] if mode == 'shuffle_zero' else list(self.graves[self.owner])
            if selected:
                for name in selected:
                    self.chooser(name)
            else:
                self.emit('choose_zone', player=self.owner, value=-1)
            self.emit('activate', 'source', value=1)
            self.emit('assert_choices_consumed')
            self.stack, self.live = 1, False
            self.pool[self.owner] = ZERO.copy()
            self.exiles[self.owner].append(self.name)
            self.observations()
            self.resolve()
            self.shuffle[self.owner] += 1
            for name in selected:
                self.graves[self.owner].remove(name)
                self.decks[self.owner].append(name)
        self.observations()

    def play_land(self):
        self.emit('play_land', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.hands[self.owner].remove(self.name)
        self.live = True
        self.observations()

    def land_mana(self, *, choice=False, color=0):
        self.emit('require_can_activate', 'source', value=1)
        if choice:
            self.emit('choose_color', player=self.owner, value=color)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.tapped = True
        self.pool[self.owner][color] += 1
        if self.id == RIVULET and color == COLORS['U']:
            self.life[self.owner] -= 1
        self.observations()

    def stash(self):
        self.play_land()
        if self.mode == 'mana':
            self.land_mana()
            return
        self.fund(self.owner, [4, 0, 0, 0, 0, 0])
        self.emit('require_can_activate', 'source', value=1)
        self.emit('choose_dialog', player=self.owner, value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.pool[self.owner] = ZERO.copy()
        self.live, self.stack = False, 1
        self.graves[self.owner].append(self.name)
        self.observations()
        returned = {'mill_new_artifact': ARTIFACT, 'mill_prior_artifact': PRIOR_ARTIFACT}.get(self.mode)
        if returned:
            self.chooser(returned)
            if self.owner == 1:
                self.emit('choose_dialog', player=self.owner, value=0)
        elif self.mode == 'mill_decline':
            self.emit('choose_zone', player=self.owner, value=-1)
        self.resolve()
        self.mill(5)
        if returned:
            self.graves[self.owner].remove(returned)
            self.decks[self.owner].insert(0, returned)
        self.observations()

    def declare_rivulet_mode(self, allow_auto):
        self.steps.append(dict(op='choose_activation_mode', ref='source', player=self.owner,
                               mode_label='Sac a desert and mill four cards', fallback_color=0,
                               allow_auto_nonmana=allow_auto))

    def rivulet(self):
        self.play_land()
        if self.mode in ('colorless', 'blue'):
            self.land_mana(choice=True, color=COLORS['U'] if self.mode == 'blue' else 0)
            return
        self.emit('require_can_activate', 'source', value=1)
        if self.mode == 'short_color':
            self.fund(self.owner, [1, 0, 0, 0, 0, 0])
            self.declare_rivulet_mode(False)
            if self.owner == 0:
                # Decline the payment prompt if the unavailable mill mode is offered.
                self.emit('choose_cancel_mana', player=self.owner)
            self.emit('activate', 'source', value=1)
            self.emit('require_activation_mode', value=0)
            self.emit('assert_choices_consumed')
            self.pool[self.owner][0] += 1
            self.tapped = True
            self.observations()
            return
        # The handler's mode offer checks {1}{U}{U}; the paid activation must
        # still cost the printed {1}{U}. Observe the extra blue left unspent.
        self.fund(self.owner, [1, 0, 2, 0, 0, 0])
        self.declare_rivulet_mode(True)
        self.emit('choose_player', player=self.owner, value=self.target)
        pay_other = self.mode == 'mill_other_desert'
        if self.owner == 0:
            self.emit('choose_target', 'cost_desert' if pay_other else 'source', player=self.owner)
        self.emit('activate', 'source', value=1)
        self.emit('require_activation_mode', value=1)
        self.emit('assert_choices_consumed')
        self.pool[self.owner] = [0, 0, 1, 0, 0, 0]
        self.live, self.tapped, self.stack = pay_other, True, 1
        self.graves[self.owner].append(DESERT if pay_other else self.name)
        self.observations()
        self.resolve()
        self.mill(4)
        self.observations()

    def run(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        self.observations()
        action = {MINISTER: self.minister, TIMEPIECE: self.timepiece, STASH: self.stash, RIVULET: self.rivulet}
        action[self.id]()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board_type = FollowOnBoard if csvid in NEW_MODES else Board
    board = board_type(csvid, mode, int(seat), rows, oracle)
    board.run()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, [])))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered paid mill source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
