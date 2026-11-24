"""Whole Oracle Flash creatures with paid alternate costs, modes, and rider choices."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import COLORS, card, cost_from_oracle, step

FAMILY = 'flash_remaining_alternate_costs_and_modes'
PROFILE = Path(__file__).with_name('data') / 'flash_remaining_alternate_costs_and_modes_wave8_profiles.json'
PREFIX = 'FlashRemainingAlternateCostsAndModesRecipe'
SEED = 2739
ZONES = {'hand': 0, 'battlefield': 1, 'graveyard': 3, 'exile': 4}
ZONE_NAMES = {value: name for name, value in ZONES.items()} | {2: 'library'}
ZERO = [0] * 6
SEATED = {'assert_life', 'assert_zone', 'assert_zone_card', 'set_mana', 'assert_mana', 'assert_zone_multiplicity',
          'choose_target', 'choose_done_target', 'choose_player', 'choose_dialog', 'choose_zone_card', 'turn', 'untap_step'}
VALUES = {'assert_life', 'assert_card', 'assert_stack', 'assert_zone', 'assert_zone_card', 'assert_zone_multiplicity',
          'choose_dialog', 'choose_player', 'set_mana', 'assert_mana', 'attack', 'block', 'cast', 'cast_response',
          'activate', 'require_can_activate', 'assert_offered_legal_card', 'can_target_creature', 'can_activate',
          'can_activate_hand', 'require_can_activate_hand', 'activate_hand', 'probe_activation_target',
          'assert_unused_choices'}
VINE, OFFAL, BEHEMOTH, ELDER, NIMBLE, SAMUT = 4648, 9401, 16083, 16085, 17183, 17012
MODES = {
    VINE: ('pitch', 'paid_over_pitch', 'wrong_pitch_color', 'forestwalk', 'no_forest'),
    OFFAL: ('evoke_foe', 'evoke_own', 'normal_stays', 'normal_death', 'countered'),
    BEHEMOTH: ('normal_expiry', 'emerge', 'high_mv_floor', 'colored_short', 'combat_flash'),
    ELDER: ('paid_four', 'paid_zero', 'emerge', 'emerge_countered', 'colored_short'),
    NIMBLE: ('cycle_plain', 'cycle_enemy', 'cycle_own', 'flying', 'cycle_short'),
    SAMUT: ('haste_team', 'own_untap', 'foreign_untap', 'white_short'),
}
EMERGE_COSTS = {BEHEMOTH: (7, 1), ELDER: (5, 2)}
BEAR, SQUIRE, GIANT, GROWTH, COUNTER = 'Grizzly Bears', 'Squire', 'Hill Giant', 'Giant Growth', 'Counterspell'
FOREST, ISLAND, ARTISAN, MOUNTAIN, TERROR = 'Forest', 'Island', 'Artisan of Kozilek', 'Mountain', 'Terror'
PINGER = 'Prodigal Sorcerer'
WITNESSES = {BEAR, SQUIRE, GIANT, GROWTH, COUNTER, FOREST, ISLAND, ARTISAN, MOUNTAIN, TERROR, PINGER}
GAPS = {
    OFFAL: ['The legacy selects the graveyard and card during departure resolution. This lane has no separate response window for the pending graveyard card.'],
    ELDER: ['The native callback handles cast-effect choices during announcement. Its independently retained effect through a counter is tested; responding to a separately pending cast trigger is unavailable.'],
    NIMBLE: ['The ordinary host can leave a real activated ability pending. It resolves production triggers synchronously, so no independently pending triggered ability can be supplied to cycling here.'],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (not pin or not exact(pin['oracle'], record) or pin['registry']['csvid'] != csvid or csvid not in MODES
            or 'Flash' not in record.get('keywords', ()) or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole pinned normal Flash source required')
    return dict(csvid=csvid, mana=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Flash alternate-cost variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('reserved alternate-cost Flash source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared Flash alternate-cost parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Flash alternate-cost witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys():
        raise ValueError('exact six-source Flash alternate-cost profile required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Flash source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented alternate-cost Flash source required')
                classify(oracle[name], row['csvid'])


class Board:
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        self.id, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.add('source', name, owner, 'hand')
        if mode == 'countered':
            self.add('response', COUNTER, self.foe, 'hand')

    def add(self, ref, name, player, zone='battlefield', *, tapped=False):
        placement = card(name, ref, player, zone)
        if tapped:
            placement['state'] = ['tapped']
        self.setup.append(placement)
        self.cards[ref] = dict(name=name, player=player, zone=zone, tapped=int(tapped), bonus=0, switched=False, damage=0)

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
        if value or op in VALUES:
            values['value'] = value
        if op in ('assert_zone_multiplicity', 'assert_zone_card', 'assert_zone'):
            values['zone'] = ZONE_NAMES[zone]
            if op != 'assert_zone':
                values['index'] = index
        if op in ('probe_activation_target', 'assert_offered_legal_card'):
            values['index'] = index
        if op in ('set_mana', 'assert_mana'):
            values['color'] = color
        self.steps.append(step(op, **values))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def mana(self, op, seat, amount=ZERO):
        for color, value in enumerate(amount):
            self.emit(op, player=seat, value=value, color=color)

    def fund(self, player, amount=ZERO):
        self.mana('set_mana', player, amount)
        self.emit('refresh')

    def stat(self, ref, name):
        item = self.cards[ref]
        if item['switched']:
            name = 'toughness' if name == 'power' else 'power'
        return int(self.oracle[item['name']][name])+item['bonus']

    def cast(self, ref, *, response=False):
        item = self.cards[ref]
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2 if response else 1)
        self.mana('assert_mana', item['player'])

    def resolve(self, *, response=False, observe_departure=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        if observe_departure:
            self.emit('assert_unused_choices', value=0)
        else:
            self.emit('assert_choices_consumed')
        self.emit('assert_stack')

    def receipts(self):
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            self.mana('assert_mana', seat)
            cards = [item for item in self.cards.values() if item['player'] == seat]
            counts = Counter((item['name'], item['zone']) for item in cards)
            for name in sorted({item['name'] for item in cards}):
                for zone, index in ZONES.items():
                    self.emit('assert_zone_multiplicity', player=seat, index=self.rows[name]['csvid'], zone=index,
                              value=counts[name, zone])
        for ref, item in self.cards.items():
            if item['zone'] != 'battlefield':
                continue
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'tapped', item['tapped'])
            if 'Creature' in self.oracle[item['name']]['type_line']:
                for field in ('power', 'toughness'):
                    self.observe(ref, field, self.stat(ref, field))
                self.observe(ref, 'damage', item['damage'])

    def begin_foe_combat(self, ref):
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        self.cards[ref]['tapped'] = 1
        self.emit('finish_attackers')

    def finish_combat(self, blocker):
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', blocker, 'attacker', value=1)
        self.emit('advance_combat')
        self.observe('attacker', 'blocked', int(blocker is not None))
        self.emit('combat_damage')
        self.resolve()
        if blocker:
            marks = {blocker: self.stat('attacker', 'power'), 'attacker': self.stat(blocker, 'power')}
            for ref, amount in marks.items():
                self.cards[ref]['damage'] = amount
                if amount >= self.stat(ref, 'toughness'):
                    self.cards[ref]['zone'] = 'graveyard'
        self.emit('end_combat')
        self.receipts()

    def next_untap(self, seat):
        self.emit('advance_turn')
        self.emit('untap_step', player=seat)
        for card in self.cards.values():
            card.update(damage=0, bonus=0, switched=False)
            if card['player'] == seat and card['zone'] == 'battlefield':
                card['tapped'] = 0
        self.receipts()


class AlternateBoard(Board):
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        super().__init__(csvid, name, mode, owner, rows, oracle)
        self.sacref = None
        self.trigger_targets = ()
        if csvid == VINE:
            self.setup_vine()
        if csvid == OFFAL:
            self.setup_offal()
        if csvid in EMERGE_COSTS:
            self.setup_emerge()

    def setup_vine(self):
        if self.mode in ('pitch', 'paid_over_pitch'):
            self.add('pitch_card', BEAR, self.owner, 'hand')
            self.add('wrong_pitch', SQUIRE, self.owner, 'hand')
        elif self.mode == 'wrong_pitch_color':
            self.add('wrong_pitch', SQUIRE, self.owner, 'hand')
            self.add('foreign_green', BEAR, self.foe, 'hand')
        else:
            self.add('blocker', BEAR, self.foe)
            self.add('map_land', FOREST if self.mode == 'forestwalk' else ISLAND, self.foe, tapped=True)

    def setup_offal(self):
        self.add('own_grave', SQUIRE, self.owner, 'graveyard')
        self.add('foe_grave', BEAR, self.foe, 'graveyard')
        self.add('foe_other_grave', GIANT, self.foe, 'graveyard')

    def setup_emerge(self):
        if self.mode.startswith('emerge') or self.mode in ('high_mv_floor', 'colored_short'):
            self.sacref = 'sacrifice'
            fodder = ARTISAN if self.mode == 'high_mv_floor' else GIANT
            self.add(self.sacref, fodder, self.owner)
        if self.id == BEHEMOTH:
            if self.mode != 'colored_short':
                self.add('own_probe', GROWTH, self.owner, 'hand')
                self.add('enemy_probe', TERROR, self.foe, 'hand')
            if self.mode == 'combat_flash':
                self.add('attacker', GIANT, self.foe)
        elif self.mode != 'colored_short':
            self.add('enemy_land', MOUNTAIN, self.foe)
            self.add('enemy_creature', BEAR, self.foe)
            self.add('enemy_spare', SQUIRE, self.foe)
            self.add('own_land', FOREST, self.owner)
            if not self.sacref:
                self.add('own_creature', GIANT, self.owner)
            if self.mode == 'paid_four':
                self.trigger_targets = ('enemy_land', 'enemy_creature', 'own_land', 'own_creature')
            elif self.mode.startswith('emerge'):
                self.trigger_targets = ('enemy_land', 'enemy_creature')
            if self.mode == 'emerge_countered':
                self.add('response', COUNTER, self.foe, 'hand')

    def zone_count(self, player, name, zone, count):
        self.emit('assert_zone_multiplicity', player=player, index=self.rows[name]['csvid'], zone=ZONES[zone], value=count)

    def exact_hand(self, player, name, count):
        self.zone_count(player, name, 'hand', count)

    def choose_zone_card(self, name):
        self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])

    def prove_source_negative(self, pool):
        self.emit('cast', 'source', value=0)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.exact_hand(self.owner, self.name, 1)
        self.mana('assert_mana', self.owner, pool)
        self.mana('assert_mana', self.foe)
        if self.sacref:
            self.zone_count(self.owner, self.cards[self.sacref]['name'], 'graveyard', 0)
            self.observe(self.sacref, 'in_play', 1)
        for ref in ('wrong_pitch', 'foreign_green'):
            if ref in self.cards:
                witness = self.cards[ref]
                self.exact_hand(witness['player'], witness['name'], 1)
                self.zone_count(witness['player'], witness['name'], 'exile', 0)

    def run_vine(self):
        if self.mode == 'wrong_pitch_color':
            self.fund(self.owner)
            self.prove_source_negative(ZERO)
            return
        if self.mode == 'pitch':
            self.fund(self.owner)
            self.emit('observe_search_filters', value=1)
            self.choose_zone_card(BEAR)
        else:
            self.fund(self.owner, cost_from_oracle(self.oracle[self.name]))
            if self.mode == 'paid_over_pitch':
                self.emit('present_ai_dialogs', value=1)
                self.emit('choose_dialog', player=self.owner, value=1)
        self.cast('source')
        if self.mode == 'pitch':
            self.emit('assert_offered_legal_card', index=self.rows[BEAR]['csvid'], value=1)
            self.emit('assert_offered_legal_card', index=self.rows[SQUIRE]['csvid'], value=0)
            self.cards['pitch_card']['zone'] = 'exile'
        self.resolve()
        self.cards['source']['zone'] = 'battlefield'
        self.receipts()
        for key in ('pitch_card', 'wrong_pitch'):
            if key in self.cards:
                witness = self.cards[key]
                self.zone_count(self.owner, witness['name'], 'exile', int(witness['zone'] == 'exile'))
        if self.mode in ('forestwalk', 'no_forest'):
            self.next_untap(self.owner)
            self.emit('turn', player=self.owner)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            blocked = self.mode == 'no_forest'
            self.emit('block', 'blocker', 'source', value=int(blocked))
            self.emit('advance_combat')
            self.observe('source', 'blocked', int(blocked))
            self.emit('combat_damage')
            self.resolve()
            self.emit('end_combat')
            self.emit('assert_life', player=self.owner, value=20)
            self.emit('assert_life', player=self.foe, value=20 if blocked else 19)
            self.observe('source', 'damage', 2 if blocked else 0)
            self.observe('blocker', 'damage', 1 if blocked else 0)

    def plan_offal_exile(self, victim):
        owner = self.cards[victim]['player']
        self.emit('choose_player', player=self.owner, value=owner)
        self.choose_zone_card(self.cards[victim]['name'])
        self.cards[victim]['zone'] = 'exile'

    def counter_source(self):
        self.emit('announce_spell', 'source')
        self.fund(self.foe, cost_from_oracle(self.oracle[COUNTER]))
        self.cast('response', response=True)
        self.resolve(response=True)
        self.cards['source']['zone'] = 'graveyard'
        self.cards['response']['zone'] = 'graveyard'

    def run_offal(self):
        evoke = self.mode.startswith('evoke_')
        fee = cost_from_oracle({'mana_cost': '{B}'}) if evoke else cost_from_oracle(self.oracle[self.name])
        self.fund(self.owner, fee)
        if not evoke:
            self.emit('choose_dialog', player=self.owner, value=0)
        self.cast('source')
        if self.mode == 'countered':
            self.counter_source()
        else:
            if evoke:
                self.plan_offal_exile('own_grave' if self.mode == 'evoke_own' else 'foe_grave')
            self.resolve(observe_departure=evoke)
            self.cards['source']['zone'] = 'graveyard' if evoke else 'battlefield'
        self.receipts()
        if self.mode == 'normal_death':
            self.plan_offal_exile('foe_other_grave')
            self.emit('destroy', 'source')
            self.resolve(observe_departure=True)
            self.cards['source']['zone'] = 'graveyard'
            self.receipts()
        for key in ('own_grave', 'foe_grave', 'foe_other_grave'):
            card = self.cards[key]
            self.zone_count(card['player'], card['name'], 'exile', int(card['zone'] == 'exile'))

    def emerge_fee(self, *, short=False):
        generic, blue = EMERGE_COSTS[self.id]
        fodder = self.oracle[self.cards[self.sacref]['name']]
        reduced = max(0, generic - int(fodder['cmc']))
        cost = ZERO.copy()
        cost[0] = reduced
        cost[2] = blue - int(short)
        return cost

    def hexproof(self, enabled):
        self.emit('can_target_creature', 'own_probe', 'source', value=1)
        self.emit('can_target_creature', 'enemy_probe', 'source', value=int(not enabled))

    def run_emerge(self):
        if self.mode == 'colored_short':
            pool = self.emerge_fee(short=True)
            self.fund(self.owner, pool)
            self.prove_source_negative(pool)
            return
        fee = self.emerge_fee() if self.sacref else cost_from_oracle(self.oracle[self.name])
        self.fund(self.owner, fee)
        if self.mode == 'combat_flash':
            self.begin_foe_combat('attacker')
        if self.sacref:
            self.emit('present_ai_dialogs', value=1)
            self.emit('choose_dialog', player=self.owner, value=0)
            # new_sacrifice_impl's own seat-1 path directly picks the only creature.
            if self.owner == 0:
                self.emit('choose_target', self.sacref, player=self.owner)
        if self.id == ELDER:
            for ref in self.trigger_targets:
                self.emit('choose_target', ref, player=self.owner)
                self.cards[ref]['tapped'] = 1
            if len(self.trigger_targets) < 4:
                self.emit('choose_done_target', player=self.owner)
        self.cast('source')
        if self.sacref:
            self.cards[self.sacref]['zone'] = 'graveyard'
            self.zone_count(self.owner, self.cards[self.sacref]['name'], 'graveyard', 1)
            self.observe(self.sacref, 'in_play', 0)
        if self.mode == 'emerge_countered':
            self.counter_source()
        else:
            self.resolve()
            self.cards['source']['zone'] = 'battlefield'
        self.receipts()
        if self.id == BEHEMOTH:
            self.hexproof(True)
            if self.mode == 'combat_flash':
                self.finish_combat('source')
            self.next_untap(self.owner)
            self.hexproof(False)

    def run(self):
        self.emit('turn', player=self.foe)
        self.fund(self.foe)
        if self.id == VINE:
            self.run_vine()
        elif self.id == OFFAL:
            self.run_offal()
        else:
            self.run_emerge()


class AddedBoard(AlternateBoard):
    def __init__(self, csvid, name, mode, owner, rows, oracle):
        super().__init__(csvid, name, mode, owner, rows, oracle)
        if csvid == NIMBLE:
            if mode == 'flying':
                self.add('blocker', BEAR, self.foe)
            else:
                self.setup.append(card(FOREST, 'drawn', self.owner, 'library'))
                if mode in ('cycle_enemy', 'cycle_own'):
                    self.actor = self.foe if mode == 'cycle_enemy' else self.owner
                    self.add('pinger', PINGER, self.actor)
        elif mode == 'haste_team':
            self.add('pinger', PINGER, self.foe, 'hand')
            self.add('ally', BEAR, self.owner, 'hand')
        else:
            self.add('ally', BEAR, self.owner, tapped=True)
            self.add('enemy', SQUIRE, self.foe, tapped=True)

    def cycle(self):
        fee = cost_from_oracle({'mana_cost': '{2}{U}'})
        if self.mode == 'cycle_short':
            fee[COLORS['U']] = 0
        self.fund(self.owner, fee)
        if self.mode == 'cycle_short':
            self.emit('can_activate_hand', 'source', value=0)
            self.emit('activate_hand', 'source', value=0)
            self.emit('assert_stack')
            self.emit('assert_choices_consumed')
            self.mana('assert_mana', self.owner, fee)
            self.exact_hand(self.owner, self.name, 1)
            self.zone_count(self.owner, self.name, 'graveyard', 0)
            self.emit('assert_zone_card', zone=2, player=self.owner, value=self.rows[FOREST]['csvid'])
            return

        pings = self.mode in ('cycle_own', 'cycle_enemy')
        if pings:
            self.emit('choose_player', player=self.actor, value=1-self.actor)
            self.emit('activate', 'pinger', value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=1)
            self.cards['pinger']['tapped'] = 1
            self.observe('pinger', 'tapped', 1)
        self.emit('require_can_activate_hand', 'source', value=1)
        self.emit('choose_dialog', player=self.owner, value=1)
        if self.mode == 'cycle_enemy':
            self.emit('choose_dialog', player=self.owner, value=0)
        self.emit('activate_hand', 'source', value=1)
        if self.mode == 'cycle_enemy':
            self.emit('assert_unused_choices', value=0)
        else:
            self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1+int(pings))
        self.mana('assert_mana', self.owner)
        self.zone_count(self.owner, self.name, 'graveyard', 1)
        self.emit('assert_zone_card', zone=2, player=self.owner, value=self.rows[FOREST]['csvid'])
        self.resolve()
        self.cards['source']['zone'] = 'graveyard'
        self.add_drawn_forest()
        if self.mode == 'cycle_own':
            self.life[self.foe] = 19
        self.receipts()
        self.emit('assert_zone', zone=2, player=self.owner, value=0)

    def add_drawn_forest(self):
        self.cards['drawn'] = dict(name=FOREST, player=self.owner, zone='hand')

    def play(self, ref):
        entity = self.cards[ref]
        self.fund(entity['player'], cost_from_oracle(self.oracle[entity['name']]))
        self.cast(ref)
        self.resolve()
        entity['zone'] = 'battlefield'

    def flying(self):
        self.play('source')
        self.receipts()
        self.next_untap(self.owner)
        self.emit('turn', player=self.owner)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'source', value=0)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 0)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.cards['source']['tapped'] = 1
        self.life[self.foe] = 17
        self.receipts()

    def haste_team(self):
        self.play('pinger')
        self.fund(self.owner, ZERO)
        self.next_untap(self.owner)
        self.emit('turn', player=self.owner)
        self.play('source')
        self.play('ally')
        self.emit('can_activate', 'pinger', value=0)
        self.emit('begin_attackers')
        for ref in ('source', 'ally'):
            self.emit('attack', ref, value=1)
        self.emit('finish_attackers')
        self.observe('source', 'tapped', 0)
        self.observe('ally', 'tapped', 1)
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.cards['ally']['tapped'] = 1
        self.life[self.foe] = 12
        self.receipts()

    def untap(self):
        self.play('source')
        self.receipts()
        target = 'enemy' if self.mode == 'foreign_untap' else 'ally'
        fee = ZERO.copy()
        if self.mode != 'white_short':
            fee[COLORS['W']] = 1
        self.fund(self.owner, fee)
        if self.mode == 'white_short':
            self.emit('can_activate', 'source', value=0)
            self.emit('activate', 'source', value=0)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack')
            self.receipts()
            return
        self.emit('probe_activation_target', 'source', target, index=-1, value=1)
        self.emit('probe_activation_target', 'source', 'source', index=-1, value=0)
        self.emit('require_can_activate', 'source', value=1)
        self.emit('choose_target', target, player=self.owner)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.owner)
        self.observe('source', 'tapped', 1)
        self.observe(target, 'tapped', 1)
        self.resolve()
        self.cards['source']['tapped'] = 1
        self.cards[target]['tapped'] = 0
        self.receipts()

    def run(self):
        self.emit('turn', player=self.foe)
        self.fund(self.foe)
        if self.id == NIMBLE:
            if self.mode == 'flying':
                self.flying()
            else:
                self.cycle()
        elif self.mode == 'haste_team':
            self.haste_team()
        else:
            self.untap()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board_type = AddedBoard if csvid in (NIMBLE, SAMUT) else AlternateBoard
    board = board_type(csvid, name, mode, int(seat), rows, oracle)
    board.run()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, [])))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered Flash alternate-cost source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
