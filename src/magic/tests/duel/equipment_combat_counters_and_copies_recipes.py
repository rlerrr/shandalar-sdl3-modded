"""Paid Equipment, attack counters, and actual copies of combat equipment."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'equipment_combat_counters_and_copies'
PROFILE = Path(__file__).with_name('data') / 'equipment_combat_counters_and_copies_wave8_profiles.json'
ARMORY, SIGIL, BLADE, JITTE = 13843, 15218, 6619, 7587
MODES = {
    ARMORY: ('alone', 'group', 'other_attacker', 'move'),
    SIGIL: ('alone', 'group', 'other_attacker', 'no_others'),
    BLADE: ('blocked', 'noncombat', 'move', 'repeat_blocked'),
    JITTE: ('pump_life', 'shrink', 'blocked', 'noncombat'),
}
PREFIX = 'EquipmentCombatCountersRecipe'
SEED = 2660
ZONES = {'hand': 0, 'battlefield': 1, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
BEAR, GIANT, CADET, WALL = 'Grizzly Bears', 'Hill Giant', 'Eager Cadet', 'Wall of Stone'
FIGHT = 'Prey Upon'
WITNESSES = {BEAR, GIANT, CADET, WALL, FIGHT}

AXE = 17284
NATURALIZE = 'Naturalize'
WITNESSES.add(NATURALIZE)
MODES[AXE] = ('copy_hit', 'clone_reequip', 'clone_chain', 'clone_blocked', 'clone_noncombat',
              'unattached_stays_idle', 'other_attacker')
ZONE_NAMES = {value: name for name, value in ZONES.items()}
SEATED = {'assert_life', 'set_mana', 'assert_mana', 'assert_zone_multiplicity', 'assert_matching_permanents',
          'bind_permanent_copy', 'choose_target', 'choose_dialog', 'turn', 'untap_step'}
VALUES = {'assert_life', 'assert_card', 'assert_counter', 'assert_stack', 'assert_zone_multiplicity',
          'assert_matching_permanents', 'assert_unused_choices', 'choose_dialog', 'set_mana', 'assert_mana',
          'attack', 'block', 'can_activate', 'activate', 'present_ai_dialogs'}
PRINTED = {
    ARMORY: 'Whenever equipped creature attacks, put a +1/+1 counter on it.',
    SIGIL: 'Whenever equipped creature attacks alone, it gets +1/+1 until end of turn for each other creature you control.',
    BLADE: ('Equipped creature gets +1/+1 for each charge counter on this Equipment.\n'
            'Whenever equipped creature deals combat damage, put a charge counter on this Equipment.'),
    JITTE: ("Whenever equipped creature deals combat damage, put two charge counters on Umezawa's Jitte.\n"
            "Remove a charge counter from Umezawa's Jitte: Choose one —\n"
            '• Equipped creature gets +2/+2 until end of turn.\n'
            '• Target creature gets -1/-1 until end of turn.\n'
            '• You gain 2 life.'),
    AXE: ('Equipped creature gets +2/+0.\n'
          "Whenever equipped creature deals combat damage to a player, create a token that's a copy of this Equipment."),
}

@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (csvid not in PRINTED or not pin or pin['registry']['csvid'] != csvid or not exact(pin['oracle'], record)
            or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('keywords') != ['Equip']):
        raise ValueError('whole pinned normal Equipment source required')
    ability, equip = record.get('oracle_text', '').rsplit('\n', 1)
    fee = '{1}' if csvid == SIGIL else '{2}'
    expected_equip = f'Equip {fee}'
    if csvid in (ARMORY, SIGIL, BLADE):
        expected_equip += f' ({fee}: Attach to target creature you control. Equip only as a sorcery.)'
    if ability != PRINTED[csvid] or equip != expected_equip:
        raise ValueError('exact attack or damage rider and printed equip fee required')
    return dict(csvid=csvid, fee=int(fee[1]), mana=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in (*MODES[shape['csvid']], 'offturn') for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Equipment variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('reserved Equipment source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared Equipment parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Equipment witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys():
        raise ValueError('exact five-source Equipment profile required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Equipment source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Equipment primary required')
                classify(oracle[name], row['csvid'])


class Board:
    def __init__(self, csvid, name, mode, owner, record, rows, oracle):
        self.id, self.name, self.mode, self.owner, self.foe = csvid, name, mode, owner, 1-owner
        self.record, self.rows, self.oracle = record, rows, oracle
        self.equip_fee = int(re.search(r'\nEquip \{([12])\}', record['oracle_text'])[1])
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.attached = None
        self.charges = 0
        self.add('source', name, owner, 'hand')
        self.add('host', BEAR, owner)
        if mode not in ('no_others', 'offturn'):
            self.add('other', GIANT, owner)
        if csvid == SIGIL and mode in ('alone', 'group', 'other_attacker'):
            self.add('spare', CADET, owner)
        if mode in ('blocked', 'repeat_blocked', 'noncombat', 'alone', 'no_others'):
            self.add('wall', WALL, self.foe)
        if mode == 'noncombat':
            self.add('spell', FIGHT, owner, 'hand')
        if mode == 'shrink':
            self.add('small', CADET, self.foe)
            self.add('large', GIANT, self.foe)

    def add(self, ref, name, seat, zone='battlefield'):
        self.setup.append(card(name, ref, seat, zone))
        self.cards[ref] = dict(name=name, seat=seat, zone=zone, counters=0, until_end=0, damage=0, tapped=0)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        values = {}
        if ref:
            values['ref'] = ref
        if other:
            values['other'] = other
        if op == 'assert_counter':
            values['counter_kind'] = prop
        elif prop:
            values['property'] = prop
        if player or op in SEATED:
            values['player'] = player
        if value or op in VALUES:
            values['value'] = value
        if op == 'assert_zone_multiplicity':
            values['zone'] = ZONE_NAMES[zone]
        if op in ('assert_zone_multiplicity', 'assert_matching_permanents', 'bind_permanent_copy'):
            values['index'] = index
        if op in ('set_mana', 'assert_mana'):
            values['color'] = color
        self.steps.append(step(op, **values))

    def prop(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def pool(self, op, seat, amounts=ZERO):
        for color, count in enumerate(amounts):
            self.emit(op, player=seat, color=color, value=count)

    def fund(self, seat, cost):
        self.pool('set_mana', seat, cost)
        self.emit('refresh')

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')

    def cost_paid(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        for seat in (0, 1):
            self.pool('assert_mana', seat)

    def cast(self, ref, *targets):
        card = self.cards[ref]
        self.fund(card['seat'], cost_from_oracle(self.oracle[card['name']]))
        for target in targets:
            self.emit('choose_target', target, player=card['seat'])
        self.emit('cast', ref, value=1)
        self.cost_paid()
        self.resolve()

    def equip(self, target, *, equipment='source'):
        self.fund(self.owner, [self.equip_fee, 0, 0, 0, 0, 0])
        self.emit('require_can_activate', equipment, value=1)
        if self.id == JITTE:
            self.emit('choose_dialog', player=self.owner, value=0)
        self.emit('choose_target', target, player=self.owner)
        self.emit('activate', equipment, value=1)
        self.cost_paid()
        self.resolve()
        self.attached = target

    def stats(self, ref, prop):
        item = self.cards[ref]
        charge_bonus = self.charges if self.id == BLADE and self.attached == ref else 0
        return int(self.oracle[item['name']][prop])+item['counters']+item['until_end']+charge_bonus

    def counter_receipt(self):
        if self.id in (BLADE, JITTE):
            self.emit('assert_counter', 'source', prop='charge', value=self.charges)
        if self.id == ARMORY:
            for ref, card in self.cards.items():
                if card['zone'] == 'battlefield' and card['seat'] == self.owner and ref != 'source':
                    self.emit('assert_counter', ref, prop='plus1', value=card['counters'])

    def counted_cards(self):
        return self.cards.values()

    def observed_cards(self):
        return self.cards.items()

    def receipts(self):
        self.emit('assert_stack')
        self.counter_receipt()
        for seat in (0, 1):
            self.pool('assert_mana', seat)
            self.emit('assert_life', player=seat, value=self.life[seat])
            own = [(card['name'], card['zone']) for card in self.counted_cards() if card['seat'] == seat]
            counts = Counter(own)
            for name in sorted({name for name, _ in own}):
                for zone, code in ZONES.items():
                    self.emit('assert_zone_multiplicity', player=seat, zone=code, index=self.rows[name]['csvid'],
                              value=counts[name, zone])
        for ref, card in self.observed_cards():
            self.prop(ref, 'in_play', int(card['zone'] == 'battlefield'))
            if card['zone'] == 'battlefield':
                self.prop(ref, 'tapped', card['tapped'])
                if ref != 'source':
                    for stat in ('power', 'toughness'):
                        self.prop(ref, stat, self.stats(ref, stat))
                    self.prop(ref, 'damage', card['damage'])

    def combat(self, attackers, *, blocker=None):
        self.emit('begin_attackers')
        for attacker in attackers:
            self.emit('attack', attacker, value=1)
            self.cards[attacker]['tapped'] = 1
        self.emit('finish_attackers')
        self.emit('assert_choices_consumed')
        if self.attached in attackers:
            host = self.cards[self.attached]
            if self.id == ARMORY:
                host['counters'] += 1
            if self.id == SIGIL and len(attackers) == 1:
                host['until_end'] += sum(ref not in (self.attached, 'source') and card['seat'] == self.owner
                                         and card['zone'] == 'battlefield' for ref, card in self.cards.items())
        self.counter_receipt()
        for attacker in attackers:
            self.prop(attacker, 'power', self.stats(attacker, 'power'))
        self.emit('begin_blockers')
        if blocker:
            assert len(attackers) == 1
            self.emit('block', blocker, attackers[0], value=1)
        self.emit('advance_combat')
        for attacker in attackers:
            self.prop(attacker, 'blocked', int(blocker is not None))
        self.emit('combat_damage')
        self.resolve()
        if blocker:
            attacker = attackers[0]
            power = self.stats(attacker, 'power')
            # The 0/8 wall survives repeated 2/3-damage strikes independently
            # of the eventual post-damage bonus on the attacking creature.
            assert power < self.stats(blocker, 'toughness') and self.stats(blocker, 'power') == 0
            self.cards[blocker]['damage'] = power
        else:
            self.life[self.foe] -= sum(self.stats(attacker, 'power') for attacker in attackers)
        if self.attached in attackers:
            self.charges += 1 if self.id == BLADE else 2 if self.id == JITTE else 0
        self.emit('end_combat')
        self.receipts()

    def step_to(self, seat):
        self.emit('advance_turn')
        self.emit('untap_step', player=seat)
        self.emit('turn', player=seat)
        self.emit('assert_choices_consumed')
        for card in self.cards.values():
            card.update(until_end=0, damage=0)
            if card['seat'] == seat and card['zone'] == 'battlefield':
                card['tapped'] = 0
        self.receipts()

    def jitte(self, mode, target=None):
        assert self.id == JITTE and self.charges > 0
        self.emit('require_can_activate', 'source', value=1)
        self.emit('choose_dialog', player=self.owner, value={'pump': 1, 'shrink': 2, 'life': 3}[mode])
        if target:
            self.emit('choose_target', target, player=self.owner)
        self.emit('activate', 'source', value=1)
        self.cost_paid()
        self.charges -= 1
        self.counter_receipt()
        self.resolve()
        if mode == 'pump':
            self.cards[self.attached]['until_end'] += 2
        elif mode == 'life':
            self.life[self.owner] += 2
        else:
            self.cards[target]['until_end'] -= 1
            if self.stats(target, 'toughness') == 0:
                self.cards[target]['zone'] = 'graveyard'
        self.receipts()

    def start(self):
        self.emit('turn', player=self.owner)
        if self.id == JITTE:
            self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.fund(seat, ZERO)
        self.cast('source')
        self.cards['source']['zone'] = 'battlefield'
        self.receipts()

    def run(self):
        self.start()
        if self.mode == 'offturn':
            self.step_to(self.foe)
            budget = [self.equip_fee, 0, 0, 0, 0, 0]
            self.fund(self.owner, budget)
            self.emit('can_activate', 'source', value=0)
            self.pool('assert_mana', self.owner, budget)
            self.counter_receipt()
            return
        self.equip('host')
        self.receipts()
        if self.mode == 'noncombat':
            self.cast('spell', 'host', 'wall')
            self.cards['spell']['zone'] = 'graveyard'
            self.cards['wall']['damage'] = self.stats('host', 'power')
            self.receipts()
            return
        attackers = {'group': ('host', 'other'), 'other_attacker': ('other',)}.get(self.mode, ('host',))
        self.combat(attackers, blocker='wall' if self.mode in ('blocked', 'repeat_blocked') else None)
        if self.mode == 'move':
            self.equip('other')
            self.receipts()
        elif self.mode == 'repeat_blocked':
            self.step_to(self.foe)
            self.step_to(self.owner)
            self.combat(('host',), blocker='wall')
        elif self.mode == 'pump_life':
            self.jitte('pump')
            self.jitte('life')
            self.emit('can_activate', 'source', value=0)
            self.step_to(self.foe)
        elif self.mode == 'shrink':
            self.jitte('shrink', 'small')
            self.jitte('shrink', 'large')
            self.emit('can_activate', 'source', value=0)
            self.step_to(self.foe)
        elif self.id == SIGIL and self.mode == 'alone':
            self.step_to(self.foe)


class AxeBoard(Board):
    def __init__(self, *args):
        super().__init__(*args)
        self.copies = 0
        if self.mode in ('clone_reequip', 'clone_chain', 'clone_blocked', 'clone_noncombat'):
            self.add('removal', NATURALIZE, self.foe, 'hand')
        if self.mode in ('clone_blocked', 'clone_noncombat'):
            self.add('wall', WALL, self.foe)
        if self.mode == 'clone_noncombat':
            self.add('spell', FIGHT, self.owner, 'hand')

    def counted_cards(self):
        token = dict(name=self.name, seat=self.owner, zone='battlefield')
        return [*super().counted_cards(), *[token] * self.copies]

    def observed_cards(self):
        # A later token can reuse the original's battlefield slot. Zone
        # identity, not that stale reference, proves the original's removal.
        return [(ref, card) for ref, card in super().observed_cards()
                if ref != 'source' or card['zone'] == 'battlefield']

    def stats(self, ref, prop):
        return super().stats(ref, prop) + 2 * int(ref == self.attached and prop == 'power')

    def receipts(self):
        super().receipts()
        if not self.copies:
            return
        for prop, value in (('artifact', 1), ('color', 0)):
            self.emit('assert_matching_permanents', player=self.owner, index=self.id, prop=prop, value=value)
        if self.cards['source']['zone'] == 'graveyard':
            self.emit('assert_matching_permanents', player=self.owner, index=self.id, prop='token', value=1)

    def combat(self, attackers, *, blocker=None):
        if self.attached in attackers and blocker is None:
            self.copies += 1
        super().combat(attackers, blocker=blocker)

    def equip_copy(self):
        self.cast('removal', 'source')
        self.cards['removal']['zone'] = 'graveyard'
        self.cards['source']['zone'] = 'graveyard'
        self.attached = None
        self.receipts()
        self.emit('bind_permanent_copy', 'copy', player=self.owner, value=self.id, index=0)
        self.equip('other', equipment='copy')
        self.receipts()

    def next_own_turn(self):
        self.step_to(self.foe)
        self.step_to(self.owner)

    def run(self):
        if self.mode == 'offturn':
            super().run()
            return
        self.start()
        self.equip('host')
        self.receipts()
        self.combat(('other',) if self.mode == 'other_attacker' else ('host',))
        if self.mode == 'unattached_stays_idle':
            self.next_own_turn()
            self.combat(('host',))
        if self.mode not in ('clone_reequip', 'clone_chain', 'clone_blocked', 'clone_noncombat'):
            return
        self.equip_copy()
        if self.mode in ('clone_chain', 'clone_blocked'):
            self.next_own_turn()
            self.combat(('other',), blocker='wall' if self.mode == 'clone_blocked' else None)
        if self.mode == 'clone_noncombat':
            self.cast('spell', 'other', 'wall')
            self.cards['spell']['zone'] = 'graveyard'
            self.cards['wall']['damage'] = self.stats('other', 'power')
            self.receipts()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board_type = AxeBoard if csvid == AXE else Board
    board = board_type(csvid, name, mode, int(seat), oracle[name], rows, oracle)
    board.run()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=[]))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered Equipment source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
