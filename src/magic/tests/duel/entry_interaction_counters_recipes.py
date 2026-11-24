"""Actual entry, counters, life, type gates and printed riders."""
from collections import Counter, defaultdict
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'entry_interaction_counters'
PROFILE = Path(__file__).with_name('data') / 'entry_interaction_counters_wave8_profiles.json'
CLASP, ENGINE, ELITE, AVEN, SCAVENGER, RAPTOR = 11225, 11226, 14543, 14855, 15000, 15598
GLADE, MARAUDER, GEARHULK, SKATE, GREATMAW, HAULER = 15604, 16273, 16564, 16600, 16885, 17001
CONTAGION = {CLASP:1, ENGINE:2}
BOLSTER = {ELITE:2, AVEN:1, SCAVENGER:2}
SUPPORT = {RAPTOR:2, GLADE:6}
SAGE, SPHERE, BEETLE, CORE = "Sage of Shaila's Claim", 'Sphere of the Suns', 'Clockwork Beetle', 'Coretapper'
SEEDER = 'Contagion Clasp'
LEVELERS = {'Hada Spy Patrol','Student of Warfare'}

CORPS, MOA, MASTODON, TEACHER = 14219, 4140, 14618, 11074
MODES = {
    CORPS: ('one', 'split', 'self', 'recast', 'countered'),
    MOA: ('own_entry', 'foe_entry', 'destroy', 'echo_pay', 'echo_decline', 'echo_short', 'countered'),
    MASTODON: ('solo', 'weak', 'tie', 'reach', 'countered'),
    TEACHER: ('empty', 'mixed', 'threshold', 'opponent_only', 'countered'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
BEAR, BALOTH, KOBOLD, MONGOOSE = 'Grizzly Bears', 'Rumbling Baloth', 'Kobolds of Kher Keep', 'Nimble Mongoose'
COUNTER, UNSUMMON, MURDER = 'Counterspell', 'Unsummon', 'Murder'
FOREST, WASTES, FLYER = 'Forest', 'Wastes', 'Serra Angel'
HADA, STUDENT = 'Hada Spy Patrol', 'Student of Warfare'
ZONE_NAMES = {code: name for name, code in ZONES.items()}
WITNESSES = frozenset(('Clockwork Beetle', 'Contagion Clasp', 'Coretapper', 'Counterspell', 'Forest', 'Grizzly Bears', 'Hada Spy Patrol', 'Kobolds of Kher Keep', 'Murder', 'Nimble Mongoose', 'Rumbling Baloth', "Sage of Shaila's Claim", 'Serra Angel', 'Sphere of the Suns', 'Student of Warfare', 'Unsummon', 'Wastes'))


MODES.update({
    CLASP: ('entry_enemy','entry_own','proliferate','decline','short','countered'),
    ENGINE: ('entry_enemy','entry_own','proliferate','decline','short','countered'),
    ELITE: ('solo','weak','tie','tap','plain_attack','countered'),
    AVEN: ('solo','weak','tie','flying','countered'),
    SCAVENGER: ('solo','weak','tie','countered'),
    RAPTOR: ('cap','partial','empty','flying','countered'),
    GLADE: ('cap','partial','empty','death','no_counter_death','foreign_death','countered'),
    MARAUDER: ('full','partial','decline','flying','empty','countered'),
    GEARHULK: ('one','split','trample','countered'),
    SKATE: ('empty','mixed','multiple','decline','countered'),
    GREATMAW: ('other','self','clear','alone','countered'),
    HAULER: ('empty','add_charge','remove_charge','countered'),
})
HUMAN_ONLY = {(CLASP,'decline'),(ENGINE,'decline'),(MARAUDER,'partial'),(MARAUDER,'decline')}
GAPS = {
    CORPS: ('The legacy selects one recipient per counter. Per-recipient totals distinguish allocation; the selector does not expose declared targets separately.',),
    TEACHER: ('Level counter amounts use exact printed thresholds and genuine paid Level Up receipts.',),
    CLASP: ('Seat 1 uses the ordinary automatic proliferate strategy. Manual refusal is exercised in seat 0.',),
    ENGINE: ('Seat 1 uses the ordinary automatic proliferate strategy. Manual refusal is exercised in seat 0.',),
    MARAUDER: ('Seat 1 automatically moves counters from safe donors. Partial movement and refusal use the human menu.',),
}

@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'enters' not in record['oracle_text'].lower()):
        raise ValueError('normal whole entry-trigger primary required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES[csvid]
            if owner == 0 or (csvid,mode) not in HUMAN_ONLY]


def validate_variants(shape, chosen):
    if (type(chosen) is not list or not chosen or any(type(item) is not str for item in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('distinct supported entry-trigger variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned entry-trigger primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed entry-trigger declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys()):
        raise ValueError('exact entry-trigger sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != ('leveler' if name in LEVELERS else 'normal')):
                raise ValueError('entry-trigger Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented entry-trigger primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, chosen, rows, oracle):
    validate_variants(shape, chosen)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact entry-trigger primary CSVID required')
    cases = []
    for variant in chosen:
        mode, seat = variant.rsplit('_', 1)
        board = Board(rows[name], int(seat), mode, rows, oracle)
        board.run()
        cases.append(dict(name=f'EntryInteractionCountersRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=3404, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=list(GAPS.get(csvid, ())))))
    return cases

class Board:
    def __init__(self, record, owner, mode, rows, oracle):
        self.id, self.name = record['csvid'], record['name']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.wallet = [ZERO.copy(), ZERO.copy()]
        self.add('source', self.name, owner, 'hand')
        if mode == 'countered':
            self.add('counter', COUNTER, self.foe, 'hand')
        self.energy = [0,0]
        self.setup_counters()
        self.setup_followers()

    def add(self, ref, name, player, zone='battlefield'):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step','upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'choose_mana_source':
            self.steps.append(step(op, ref=ref))
            return
        if op == 'end_combat_step':
            self.steps.append(step(op, value=value))
            return
        if op == 'choose_optional_target':
            self.steps.append(step(op, ref=ref, player=player))
            return
        if op == 'assert_forced_target_choices':
            self.steps.append(step(op, value=value))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
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

    def stats(self, ref, addition=0):
        for prop in ('power', 'toughness'):
            value = int(self.oracle[self.cards[ref]['name']][prop])
            self.observe(ref, prop, value + addition)

    def count(self, ref, value, prop='plus1'):
        self.emit('assert_counter', ref, prop=prop, value=value)

    def move(self, ref, zone):
        self.cards[ref]['zone'] = zone

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def pool(self, player, amount, *, fund=False):
        for color, value in enumerate(amount):
            self.emit('set_mana' if fund else 'assert_mana', player=player, value=value, color=color)
        if fund:
            self.wallet[player] = amount.copy()
            self.emit('refresh')

    def announce(self, ref, *, target=None, response=False):
        player = self.cards[ref]['player']
        cost = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        self.pool(player, cost, fund=True)
        if target is not None:
            self.emit('choose_target', target, player=player)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.choices()
        self.emit('assert_stack', value=2 if response else 1)
        self.wallet[player] = ZERO.copy()
        self.pool(player, ZERO)

    def resolve(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def spell(self, ref, *, target=None, destination='graveyard'):
        self.announce(ref, target=target)
        self.resolve()
        self.move(ref, destination)

    def receipt(self):
        self.emit('assert_stack')
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            if self.id in CONTAGION:
                self.emit('assert_energy', player=player, value=self.energy[player])
            self.pool(player, self.wallet[player])
            for zone, code in ZONES.items():
                counts = Counter(item['name'] for item in self.cards.values()
                                 if item['player'] == player and item['zone'] == zone)
                self.emit('assert_zone', player=player, zone=code, value=counts.total())
                for name, count in sorted(counts.items()):
                    self.emit('assert_zone_multiplicity', player=player, zone=code,
                              index=self.rows[name]['csvid'], value=count)
        for ref, item in self.cards.items():
            if item['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)

    def bounced(self):
        self.move('source', 'hand')
        self.emit('bind_hand_copy', 'source', player=self.owner, value=self.id)

    def countered(self):
        self.announce('source')
        self.emit('announce_spell', 'source')
        self.announce('counter', response=True)
        self.resolve(response=True)
        self.move('source', 'graveyard')
        self.move('counter', 'graveyard')
        self.receipt()

    def setup_counters(self):
        mode = self.mode
        if self.id == CORPS:
            self.add('first', BEAR, self.owner)
            self.add('second', BALOTH, self.owner)
            self.add('enemy', BEAR, self.foe)
            if mode == 'recast':
                self.add('bounce', UNSUMMON, self.owner, 'hand')
        if self.id == MOA:
            self.add('ally', BEAR, self.owner)
            self.add('enemy', BEAR, self.foe)
            if mode == 'destroy':
                self.add('kill_spell', MURDER, self.foe, 'hand')
            if mode in ('echo_pay', 'echo_decline', 'echo_short'):
                self.add('generic_0', WASTES, self.owner)
                self.add('green', FOREST, self.owner)
                if mode != 'echo_short':
                    self.add('generic_1', WASTES, self.owner)
        if self.id == MASTODON:
            self.add('opposing', KOBOLD, self.foe)
            if mode in ('weak', 'tie'):
                self.add('weak', KOBOLD, self.owner)
            if mode == 'tie':
                self.add('chosen', MONGOOSE, self.owner)
                self.add('strong', BALOTH, self.owner)
            if mode == 'reach':
                self.add('attacker', FLYER, self.foe)
        if self.id == TEACHER:
            self.add('ordinary', BEAR, self.owner)
            if mode != 'empty':
                self.add('other_leveler', HADA, self.foe)
            if mode in ('mixed', 'threshold', 'countered'):
                self.add('leveler_a', HADA, self.owner, 'hand')
                self.add('leveler_b', STUDENT, self.owner, 'hand')

    def enter_targets(self, targets=()):
        self.announce('source')
        for ref in targets:
            self.emit('choose_target', ref, player=self.owner)
        self.resolve()
        self.move('source', 'battlefield')

    def corps(self):
        targets = {'split': ('first', 'second'), 'self': ('source', 'source')}.get(self.mode, ('first', 'first'))
        self.enter_targets(targets)
        additions = Counter(targets)
        self.check_corps(additions)
        if self.mode == 'recast':
            self.spell('bounce', target='source')
            self.bounced()
            self.receipt()
            self.enter_targets(('first', 'second'))
            additions.update(('first', 'second'))
            self.check_corps(additions)

    def check_corps(self, additions):
        for ref in ('source', 'first', 'second', 'enemy'):
            self.count(ref, additions[ref])
            self.stats(ref, additions[ref])
        self.receipt()

    def moas_counters(self, *, ally, enemy):
        for ref, count in (('ally', ally), ('enemy', enemy)):
            self.count(ref, count)
            self.stats(ref, count)
        if self.cards['source']['zone'] == 'battlefield':
            self.stats('source')
        self.receipt()

    def real_upkeep(self, player, choices=None):
        assert self.wallet == [ZERO, ZERO]
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        if choices is not None:
            choices()
        self.emit('upkeep_step', player=player)
        self.choices()
        self.emit('assert_stack')
        self.emit('turn', player=player)

    def moa(self):
        mode = self.mode
        self.enter_targets(('enemy',) if mode == 'foe_entry' else ('ally',))
        initial = int(mode != 'foe_entry')
        self.moas_counters(ally=initial, enemy=1-initial)
        if mode == 'destroy':
            self.announce('kill_spell', target='source')
            self.emit('choose_target', 'enemy', player=self.owner)
            self.resolve()
            self.move('kill_spell', 'graveyard')
            self.move('source', 'graveyard')
            self.moas_counters(ally=1, enemy=1)
        if not mode.startswith('echo_'):
            return
        self.real_upkeep(self.foe)
        for ref in ('generic_0', 'generic_1', 'green'):
            if ref in self.cards:
                self.observe(ref, 'tapped', 0)
        paid = mode == 'echo_pay'

        def echo_decisions():
            if mode != 'echo_short':
                self.emit('choose_dialog', player=self.owner, value=int(not paid))
            if paid and self.owner == 0:
                for ref in ('generic_0', 'generic_1', 'green'):
                    self.emit('choose_mana_source', ref)
            if not paid:
                self.emit('choose_target', 'enemy', player=self.owner)

        self.real_upkeep(self.owner, echo_decisions)
        if not paid:
            self.move('source', 'graveyard')
        for ref in ('generic_0', 'generic_1', 'green'):
            if ref in self.cards:
                self.observe(ref, 'tapped', int(paid))
        self.moas_counters(ally=1, enemy=int(not paid))
        if paid:
            self.real_upkeep(self.foe)
            self.real_upkeep(self.owner)
            for ref in ('generic_0', 'generic_1', 'green'):
                self.observe(ref, 'tapped', 0)
            self.moas_counters(ally=1, enemy=0)

    def mastodon(self):
        target = {'weak':'weak','tie':'chosen'}.get(self.mode,'source')
        self.enter_targets(('chosen',) if self.mode == 'tie' else ())
        for ref in ('source', 'weak', 'chosen', 'strong', 'opposing'):
            if ref in self.cards:
                number = 5 if ref == target else 0
                self.count(ref, number)
                self.stats(ref, number)
        self.receipt()
        if self.mode == 'reach':
            self.real_upkeep(self.foe)
            self.emit('begin_attackers')
            self.emit('attack', 'attacker', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block', 'source', 'attacker', value=1)
            self.observe('source', 'blocking_count', 1)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.resolve()
            self.move('attacker', 'graveyard')
            self.observe('source', 'damage', 4)
            self.emit('end_combat_step')
            self.resolve()
            self.receipt()

    def teacher(self):
        eligible = 'leveler_a' in self.cards
        if eligible:
            for ref in ('leveler_a','leveler_b'):
                self.spell(ref, destination='battlefield')
                self.stats(ref)
        if 'other_leveler' in self.cards:
            self.stats('other_leveler')
        self.enter_targets()
        self.count('source', 0)
        self.count('ordinary', 0)
        self.stats('source')
        self.stats('ordinary')
        if 'other_leveler' in self.cards:
            self.stats('other_leveler')
        if eligible:
            self.stats('leveler_a', 1)
            self.stats('leveler_b', 2)
            for ref in ('leveler_a','leveler_b'):
                self.count(ref, 0)
        self.receipt()
        if self.mode == 'threshold':
            self.pool(self.owner, [2, 0, 1, 0, 0, 0], fund=True)
            self.emit('can_activate', 'leveler_a', value=1)
            self.emit('activate', 'leveler_a', value=1)
            self.choices()
            self.emit('assert_stack', value=1)
            self.wallet[self.owner] = ZERO.copy()
            self.pool(self.owner, ZERO)
            self.stats('leveler_a', 1)
            self.resolve()
            self.stats('leveler_a', 2)
            self.stats('leveler_b', 2)
            self.stats('other_leveler')
            self.receipt()

    def setup_followers(self):
        mode = self.mode
        if self.id in CONTAGION:
            self.add('own', BALOTH, self.owner)
            self.add('other', BALOTH, self.foe)
            self.add('second', BALOTH, self.foe)
            if mode in ('proliferate', 'decline'):
                self.add('charge_source', SPHERE, self.owner, 'hand')
                self.add('energy_source', SAGE, self.owner, 'hand')
        if self.id in BOLSTER:
            self.add('opposing', BEAR, self.foe)
            if mode in ('weak', 'tie', 'tap', 'plain_attack'):
                self.add('weak', KOBOLD, self.owner)
            if mode == 'tie':
                self.add('chosen', MONGOOSE, self.owner)
            if mode == 'plain_attack':
                self.add('other_ally', BALOTH, self.owner)
        if self.id in SUPPORT:
            if mode == 'cap':
                for idx in range(SUPPORT[self.id]):
                    self.add(f'support_{idx}', BEAR, self.owner if idx!=1 else self.foe)
                self.add('fallback', BALOTH, self.foe)
            if mode in ('partial', 'flying', 'death', 'no_counter_death','foreign_death'):
                self.add('support_0', BEAR, self.foe if mode=='foreign_death' else self.owner)
                self.add('fallback', BALOTH, self.foe)
            if mode in ('death','no_counter_death','foreign_death'):
                if mode == 'no_counter_death':
                    self.add('plain_ally', BEAR, self.owner)
                self.add('kill_spell', MURDER, self.owner if mode=='foreign_death' else self.foe, 'hand')
        if self.id == MARAUDER:
            if mode in ('full','partial','decline'):
                if mode == 'full':
                    self.add('donor', BEAR, self.owner)
                    self.add('grow_donor', 'Armament Corps', self.owner, 'hand')
                else:
                    self.add('donor', BEETLE, self.owner, 'hand')
                self.add('enemy_donor', BEETLE, self.foe, 'hand')
            if mode in ('full','flying'):
                self.add('blocker', BEAR, self.foe)
        if self.id == GEARHULK:
            self.add('opposing', BEAR, self.foe)
            if mode == 'split':
                self.add('own_ally', BEAR, self.owner)
        if self.id in (SKATE, HAULER):
            self.add('fallback', BALOTH, self.foe)
            if mode != 'empty':
                self.add('mixed', BEETLE, self.owner, 'hand')
                self.add('core', CORE, 0, 'hand')
                if self.id == SKATE:
                    self.add('charge_source', SPHERE, self.foe, 'hand')
        if self.id == GREATMAW:
            if mode != 'alone':
                self.add('own', BALOTH, self.owner)
                self.add('opposing', BALOTH, self.foe)
            if mode == 'clear':
                self.add('seed', SEEDER, self.owner, 'hand')

    def opponent_cast(self, ref):
        self.real_upkeep(self.foe)
        self.spell(ref, destination='battlefield')
        self.real_upkeep(self.owner)

    def age(self):
        self.real_upkeep(self.foe)
        self.real_upkeep(self.owner)

    def creature_receipt(self, ref, count, *, negative=False):
        self.count(ref,count,'minus1' if negative else 'plus1')
        self.stats(ref,-count if negative else count)

    def cast_entry(self, targets=(), *, chooser=None, optional=()):
        self.announce('source')
        for ref in targets:
            self.emit('choose_target',ref,player=self.owner)
        if chooser is not None:
            chooser()
        for ref in optional:
            self.emit('choose_optional_target',ref,player=self.owner)
        self.resolve()
        self.move('source','battlefield')

    def fee_activation(self, fee, ref='source', target=None):
        player = self.cards[ref]['player']
        self.pool(player, fee, fund=True)
        self.emit('can_activate',ref,value=1)
        if target is not None:
            self.emit('choose_target',target,player=player)
        self.emit('activate',ref,value=1)
        self.choices()
        self.emit('assert_stack',value=1)
        self.wallet[player] = ZERO.copy()
        self.pool(player,ZERO)

    def damage_attack(self, ref, power, *, flying=False, trample=False, lifelink=False):
        self.emit('begin_attackers')
        self.emit('attack',ref,value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if flying or trample:
            blocker = next(ref for ref in ('blocker','fallback','opposing') if ref in self.cards)
            self.emit('block',blocker,ref,value=int(trample))
        self.emit('advance_combat')
        self.observe(ref,'blocked',int(trample))
        self.emit('combat_damage')
        self.resolve()
        if trample:
            self.move('opposing','graveyard')
            self.observe(ref,'damage',2)
        self.life[self.foe] -= power - 2 * int(trample)
        if lifelink:
            self.life[self.owner] += power
        self.emit('end_combat_step')
        self.resolve()
        self.receipt()

    def contagion(self):
        mode = self.mode
        if mode in ('proliferate','decline'):
            self.spell('charge_source',destination='battlefield')
            self.spell('energy_source',destination='battlefield')
            self.energy[self.owner] = 3
            self.count('charge_source',3,'charge')
            self.receipt()
        receiver = 'own' if mode=='entry_own' else 'other'
        chosen = ('own',) if receiver=='own' else ('other','second') if self.id==ENGINE else ('other',)
        self.announce('source')
        if self.id==ENGINE:
            self.emit('choose_player',player=self.owner,value=self.cards[receiver]['player'])
        else:
            self.emit('choose_target',receiver,player=self.owner)
        self.resolve()
        self.move('source','battlefield')
        for ref in ('own','other','second'):
            self.creature_receipt(ref,int(ref in chosen),negative=True)
        self.receipt()
        if mode == 'short':
            fee = [3,0,0,0,0,0]
            self.pool(self.owner,fee,fund=True)
            self.emit('can_activate','source',value=0)
            self.emit('activate','source',value=0)
            self.observe('source','tapped',0)
            self.receipt()
            return
        if mode not in ('proliferate','decline'):
            return
        self.fee_activation([4,0,0,0,0,0])
        repeats = CONTAGION[self.id]
        take = mode != 'decline'
        if self.owner==0:
            for _ in range(repeats):
                self.emit('choose_dialog',player=0,value=1)
                if not take:
                    self.emit('choose_cancel_target',player=0)
                    continue
                for ref in ('charge_source',*chosen):
                    self.emit('choose_target',ref,player=0)
                    self.emit('choose_dialog',player=0,value=0)
                self.emit('choose_player',player=0,value=0)
                self.emit('choose_dialog',player=0,value=0)
        self.resolve()
        self.observe('source','tapped',1)
        self.emit('can_activate','source',value=0)
        added = repeats * int(take)
        self.count('charge_source',3+added,'charge')
        self.energy[self.owner] += added
        for ref in ('own','other','second'):
            self.creature_receipt(ref,(1+added)*int(ref in chosen),negative=True)
        self.receipt()

    def bolster_follower(self):
        amount = BOLSTER[self.id]
        target = {'weak':'weak','tie':'chosen','tap':'weak','plain_attack':'weak'}.get(self.mode,'source')
        self.cast_entry(('chosen',) if self.mode=='tie' else ())
        for ref in ('source','weak','chosen','opposing','other_ally'):
            if ref in self.cards:
                self.creature_receipt(ref,amount*int(ref==target))
        self.receipt()
        if self.mode=='flying':
            self.age()
            self.damage_attack('source',int(self.oracle[self.name]['power'])+amount,flying=True)
        elif self.mode in ('tap','plain_attack'):
            self.age()
            attack = 'weak' if self.mode=='tap' else 'other_ally'
            self.emit('begin_attackers')
            self.emit('attack',attack,value=1)
            if self.mode=='tap':
                self.emit('choose_target','opposing',player=self.owner)
            self.emit('finish_attackers')
            self.choices()
            self.observe('opposing','tapped',int(self.mode=='tap'))
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.resolve()
            self.life[self.foe] -= amount if self.mode=='tap' else 4
            self.emit('end_combat_step')
            self.resolve()
            self.receipt()

    def support_follower(self):
        cap = SUPPORT[self.id]
        if self.mode=='cap':
            targets = tuple(f'support_{idx}' for idx in range(cap))
            fallback = ()
        elif self.mode=='empty':
            targets = ()
            fallback = ()
        else:
            targets = ('support_0',)
            fallback = ('fallback',)*(cap-1)
        self.cast_entry(targets,optional=fallback)
        if fallback:
            self.emit('assert_forced_target_choices')
        for ref in self.cards:
            if self.cards[ref]['zone']=='battlefield':
                self.creature_receipt(ref,int(ref in targets))
        self.receipt()
        if self.mode=='flying':
            self.age()
            self.damage_attack('source',int(self.oracle[self.name]['power']),flying=True)
        elif self.mode in ('death','no_counter_death','foreign_death'):
            victim = 'plain_ally' if self.mode=='no_counter_death' else 'support_0'
            self.spell('kill_spell',target=victim)
            self.move(victim,'graveyard')
            self.life[self.owner] += 2 * int(self.mode=='death')
            self.receipt()

    def marauder(self):
        mode = self.mode
        amount = {'full':2,'partial':1}.get(mode,0)
        donor = 'donor' in self.cards
        if donor:
            self.opponent_cast('enemy_donor')
            if mode == 'full':
                self.announce('grow_donor')
                for _ in range(2):
                    self.emit('choose_target','donor',player=self.owner)
                self.resolve()
                self.move('grow_donor','battlefield')
            else:
                self.spell('donor',destination='battlefield')
            self.creature_receipt('enemy_donor',2)
            self.creature_receipt('donor',2)

        def decisions():
            if donor and self.owner==0:
                self.emit('choose_dialog',player=0,value=int(mode=='decline'))
                if mode!='decline':
                    self.emit('choose_number',player=0,value=amount)

        self.cast_entry(chooser=decisions)
        self.creature_receipt('source',amount)
        if donor:
            self.creature_receipt('donor',2-amount)
            self.creature_receipt('enemy_donor',2)
        self.receipt()
        if mode in ('full','flying'):
            self.age()
            self.damage_attack('source',2+amount,flying=True,lifelink=True)

    def gearhulk(self):
        targets = ('source','own_ally','source','own_ally') if self.mode=='split' else ('source',)*4
        self.cast_entry(targets)
        self.emit('assert_matching_permanents',prop='artifact',player=self.owner,index=self.id,value=1)
        for ref in ('source','own_ally','opposing'):
            if ref in self.cards:
                self.creature_receipt(ref,targets.count(ref))
        self.receipt()
        if self.mode=='trample':
            self.age()
            self.damage_attack('source',8,trample=True)

    def seed_mixed(self):
        self.spell('mixed',destination='battlefield')
        if self.owner==0:
            self.spell('core',destination='battlefield')
        else:
            # The AI witness selector accepts only artifacts with native charge usage.
            # The human Coretapper can legally add a charge counter to either Clockwork Beetle.
            self.opponent_cast('core')
        self.age()
        self.emit('choose_dialog',player=0,value=0)
        self.fee_activation(ZERO,ref='core',target='mixed')
        self.resolve()
        self.creature_receipt('mixed',2)
        self.count('mixed',1,'charge')
        self.receipt()

    def skate(self):
        mode = self.mode
        if mode=='empty':
            self.cast_entry(('fallback',),optional=('source',))
            self.count('fallback',0)
            self.count('fallback',0,'minus1')
        else:
            self.opponent_cast('charge_source')
            self.seed_mixed()
            targets = () if mode=='decline' else ('mixed','charge_source') if mode=='multiple' else ('mixed',)
            self.cast_entry(targets,optional=('fallback',))
            self.creature_receipt('mixed',4 if 'mixed' in targets else 2)
            self.count('mixed',2 if 'mixed' in targets else 1,'charge')
            self.count('charge_source',6 if 'charge_source' in targets else 3,'charge')
        self.emit('assert_forced_target_choices')
        self.receipt()

    def greatmaw(self):
        mode = self.mode
        if mode=='clear':
            self.announce('seed')
            self.emit('choose_target','own',player=self.owner)
            self.resolve()
            self.move('seed','battlefield')
            self.creature_receipt('own',1,negative=True)
        target = 'own' if mode=='other' else 'source'
        removal = ('own',) if mode=='clear' else ('own',) if mode=='self' else ()
        self.cast_entry((target,*removal))
        for ref in ('source','own','opposing'):
            if ref in self.cards:
                self.creature_receipt(ref,2*int(ref==target),negative=True)
        self.receipt()

    def hauler(self):
        if self.mode=='empty':
            self.cast_entry(('fallback',))
            self.count('fallback',0,'charge')
            self.count('fallback',0)
        else:
            self.seed_mixed()
            add_charge = self.mode=='add_charge'
            self.announce('source')
            self.emit('present_ai_dialogs',value=int(self.owner==1))
            self.emit('choose_target','mixed',player=self.owner)
            # One prompt selects each type. The second always selects the last remaining type.
            for choice in (0,int(not add_charge),0,int(add_charge)):
                self.emit('choose_dialog',player=self.owner,value=choice)
            self.resolve()
            self.emit('present_ai_dialogs',value=0)
            self.move('source','battlefield')
            self.count('mixed',2 if add_charge else 0,'charge')
            self.creature_receipt('mixed',1 if add_charge else 3)
        self.receipt()

    def run(self):
        self.emit('turn', player=self.owner)
        for player in (0, 1):
            self.pool(player, ZERO, fund=True)
        self.receipt()
        if self.mode == 'countered':
            if self.id == TEACHER:
                for ref in ('leveler_a','leveler_b'):
                    self.spell(ref, destination='battlefield')
                    self.stats(ref)
            self.countered()
            if self.id == TEACHER:
                for ref in ('leveler_a','leveler_b'):
                    self.stats(ref)
            return
        methods = {CORPS: self.corps, MOA: self.moa, MASTODON: self.mastodon, TEACHER: self.teacher}
        methods.update({CLASP:self.contagion, ENGINE:self.contagion, ELITE:self.bolster_follower,
                        AVEN:self.bolster_follower, SCAVENGER:self.bolster_follower,
                        RAPTOR:self.support_follower, GLADE:self.support_follower, MARAUDER:self.marauder,
                        GEARHULK:self.gearhulk, SKATE:self.skate, GREATMAW:self.greatmaw, HAULER:self.hauler})
        methods[self.id]()
