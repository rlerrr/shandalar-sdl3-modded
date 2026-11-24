"""Paid Flash responses, combat entry effects, damage, and actual attack legality."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'flash_remaining_combat_and_activated'
PROFILE = Path(__file__).with_name('data') / 'flash_remaining_combat_and_activated_wave8_profiles.json'
AMBUSHER, THIEF, SAILBACK = 15520, 16795, 17539
FOLLOWERS = {AMBUSHER, THIEF, SAILBACK}
DECK = ('Forest', 'Island', 'Mountain', 'Plains')
FOG, SHOCK, GROWTH = 'Fog', 'Shock', 'Giant Growth'
TUSKER = 'Krosan Tusker'

INTERCEPTOR, TRAP, RANGER, HIXUS = 8551, 6697, 8659, 15151
MODES = {
    INTERCEPTOR: ('double', 'lethal', 'flying_limit', 'defender', 'short'),
    TRAP: ('filtered', 'after_blocks', 'own_attack', 'fuel_short', 'empty'),
    RANGER: ('foreign_untap', 'same_turn', 'reset', 'protected_flight', 'wrong_land'),
    HIXUS: ('multiple', 'release', 'stale', 'noncombat', 'removed_before'),
    AMBUSHER: ('combat_lethal', 'spell_lethal', 'prevented', 'unrelated', 'spell_survives'),
    THIEF: ('charge_and_draw', 'underfunded', 'does_not_block', 'dies_blocking'),
    SAILBACK: ('combat_growth', 'noncombat_twice', 'lethal', 'prevented', 'expiry'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
BEAR, GIANT, SQUIRE = 'Grizzly Bears', 'Hill Giant', 'Squire'
AIR, DRAKE, WALL = 'Air Elemental', 'Wind Drake', 'Wall of Wood'
PING, MURDER, UNSUMMON = 'Prodigal Sorcerer', 'Murder', 'Unsummon'
WITNESSES = frozenset(('Air Elemental', 'Fog', 'Forest', 'Giant Growth', 'Grizzly Bears', 'Hill Giant', 'Island', 'Krosan Tusker', 'Mountain', 'Murder', 'Plains', 'Prodigal Sorcerer', 'Shock', 'Squire', 'Unsummon', 'Wall of Wood', 'Wind Drake'))

ZONE_NAMES = {code: name for name, code in ZONES.items()}
COSTS = {INTERCEPTOR: '{3}{W}', TRAP: '{3}', RANGER: '{1}{G}', HIXUS: '{3}{W}{W}',
         AMBUSHER: '{4}{U}', THIEF: '{1}{U}', SAILBACK: '{4}{G}'}
KEYWORDS = {INTERCEPTOR: {'Flash', 'Defender'}, TRAP: {'Flash'},
            RANGER: {'Flash', 'Flying', 'Protection'}, HIXUS: {'Flash'}, AMBUSHER: {'Flash'},
            THIEF: {'Flash'}, SAILBACK: {'Flash', 'Enrage'}}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in COSTS or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record.get('mana_cost') != COSTS[csvid]
            or set(record.get('keywords', ())) != KEYWORDS[csvid]
            or ('Creature' in record['type_line']) != (csvid != TRAP)):
        raise ValueError('normal Flash combat or damage whole record required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in MODES[shape['csvid']]]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Flash combat and damage variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned Flash combat or damage primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed Flash declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != COSTS.keys()):
        raise ValueError('exact Flash sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('Flash Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented Flash primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact Flash primary CSVID required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = Board(rows[name], int(seat), mode, rows, oracle)
        board.run()
        cases.append(dict(name=f'FlashCombatActivatedRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2739, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=[])))
    return cases


class Board:
    def __init__(self, primary, owner, mode, rows, oracle):
        self.id, self.name = primary['csvid'], primary['name']
        self.owner, self.foe, self.mode = owner, 1-owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life, self.pools = [20, 20], [ZERO.copy(), ZERO.copy()]
        self.add('source', self.name, owner, 'hand')
        self.add('idle', SQUIRE, owner)
        if self.id == INTERCEPTOR:
            if mode == 'defender':
                self.add('idle_attacker', GIANT, owner)
            elif mode != 'short':
                self.add('first', GIANT if mode == 'lethal' else BEAR, self.foe)
                self.add('second', {'lethal': GIANT, 'flying_limit': DRAKE}.get(mode, BEAR), self.foe)
                if mode == 'double':
                    self.add('third', SQUIRE, self.foe)
        elif self.id == TRAP:
            if mode != 'empty':
                actor = owner if mode == 'own_attack' else self.foe
                self.add('first', BEAR, actor)
                self.add('second', GIANT, actor)
                self.add('third', DRAKE, actor)
                self.add('reserve', BEAR, actor)
                if mode == 'after_blocks':
                    self.add('blocker', WALL, owner)
        elif self.id == RANGER:
            if mode == 'protected_flight':
                self.add('incoming', DRAKE, self.foe)
                self.add('blue_flyer', AIR, self.foe)
                self.add('ground', GIANT, self.foe)
                self.add('blue_spell', UNSUMMON, self.foe, 'hand')
            elif mode == 'wrong_land':
                self.add('island', 'Island', owner)
                self.add('foreign_forest', 'Forest', self.foe)
            else:
                self.add('first_forest', 'Forest', owner)
                self.add('reserve_forest', 'Forest', owner)
                self.add('foreign_forest', 'Forest', self.foe)
                self.add('untap_target', PING if mode == 'reset' else BEAR,
                         self.foe if mode == 'foreign_untap' else owner, tapped=True)
        elif self.id == HIXUS:
            self.add('first', BEAR, self.foe)
            self.add('second', GIANT, self.foe)
            self.add('unmarked', SQUIRE, self.foe)
            if mode == 'noncombat':
                self.add('pinger', PING, self.foe)
            if mode in ('release', 'removed_before'):
                self.add('removal', MURDER, self.foe, 'hand')
        else:
            self.energy = 0
            self.add_damage_witnesses()

    def add(self, ref, name, actor, zone='battlefield', *, tapped=False):
        assert actor in (0, 1)
        row = card(name, ref, actor, zone)
        if tapped:
            row['state'] = ['tapped']
        self.setup.append(row)
        self.cards[ref] = dict(name=name, player=actor, zone=zone, tapped=tapped, damage=0)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
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

    def stats(self, ref):
        item = self.cards[ref]
        record = self.oracle[item['name']]
        return tuple(int(record[field]) + item.get('boost', 0) for field in ('power', 'toughness'))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def wallet(self, actor, cost, *, funding=False):
        for color, amount in enumerate(cost):
            self.emit('set_mana' if funding else 'assert_mana', player=actor, value=amount, color=color)
        if funding:
            self.pools[actor] = list(cost)
            self.emit('refresh')

    def cast(self, ref='source', target=None, *, allowed=True):
        item = self.cards[ref]
        actor, name = item['player'], item['name']
        full = cost_from_oracle(self.oracle[name])
        fee = full if allowed else [full[0] - 1, *full[1:]]
        self.wallet(actor, fee, funding=True)
        if target:
            self.emit('choose_target', target, player=actor)
        self.emit('cast', ref, value=int(allowed))
        self.choices()
        self.emit('assert_stack', value=int(allowed))
        if not allowed:
            self.observe(ref, 'in_hand', 1)
            self.wallet(actor, fee)
            return
        self.pools[actor] = ZERO.copy()
        self.wallet(actor, self.pools[actor])
        self.resolve()
        item['zone'] = 'battlefield' if ref == 'source' else 'graveyard'

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def start(self, actor):
        self.emit('turn', player=actor)
        for seat in (0, 1):
            self.wallet(seat, ZERO, funding=True)

    def next_turn(self, actor):
        assert self.pools == [ZERO, ZERO]
        self.emit('advance_turn')
        self.emit('untap_step', player=actor)
        self.emit('upkeep_step', player=actor)
        self.emit('turn', player=actor)
        for item in self.cards.values():
            if item['zone'] == 'battlefield':
                item['damage'] = 0
                if item['player'] == actor:
                    item['tapped'] = False

    def attack(self, attackers):
        self.emit('begin_attackers')
        for target in attackers:
            self.emit('attack', target, value=1)
            self.observe(target, 'attacking', 1)
            self.cards[target]['tapped'] = True
        self.emit('finish_attackers')

    def block(self, blocker, attacker, accepted, *, allowed=True):
        self.emit('block', blocker, attacker, value=int(allowed))
        if allowed:
            accepted[attacker] = blocker
        self.observe(blocker, 'blocking_count', tuple(accepted.values()).count(blocker))

    def damage(self, ref, count, *, ignored=False):
        if not ignored:
            if isinstance(ref, int):
                self.life[ref] -= count
            else:
                item = self.cards[ref]
                item['damage'] += count
                if item['damage'] >= self.stats(ref)[1]:
                    item['zone'] = 'graveyard'

    def finish(self, attackers, accepted, hit, *, declaration_advanced=False, prevented=False):
        if not declaration_advanced:
            self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('refresh')
        self.choices()
        if prevented:
            return
        # Additional blocking only occurs on the zero-power Interceptor.
        # Resolve every other attacker and blocker as simultaneous packets.
        hits = []
        for attacker in attackers:
            if self.cards[attacker]['zone'] != 'battlefield':
                continue
            power = self.stats(attacker)[0]
            if attacker in accepted:
                blocker = accepted[attacker]
                if self.cards[blocker]['zone'] == 'battlefield':
                    hits.append((blocker, power, self.id == RANGER and blocker == 'source'))
                    hits.append((attacker, self.stats(blocker)[0], False))
            else:
                hits.append((hit, power, False))
        for recipient, amount, ignored in hits:
            self.damage(recipient, amount, ignored=ignored)

    def interceptor(self):
        if self.mode == 'short':
            self.cast(allowed=False)
            return
        if self.mode == 'defender':
            self.cast()
            self.next_turn(self.foe)
            self.next_turn(self.owner)
            self.observe('source', 'sick', 0)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.observe('source', 'attacking', 0)
            self.emit('attack', 'idle_attacker', value=1)
            self.cards['idle_attacker']['tapped'] = True
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.finish(('idle_attacker',), {}, self.foe)
            return
        attackers = ('first', 'second', 'third') if self.mode == 'double' else ('first', 'second')
        self.attack(attackers)
        self.cast()
        self.observe('source', 'sick', 1)
        self.emit('begin_blockers')
        assigned = {}
        self.block('source', 'first', assigned)
        self.block('source', 'second', assigned, allowed=self.mode != 'flying_limit')
        if self.mode == 'double':
            self.block('source', 'third', assigned, allowed=False)
        self.finish(attackers, assigned, self.owner)

    def trap(self):
        attackers = () if self.mode == 'empty' else ('first', 'second', 'third')
        if attackers:
            self.attack(attackers)
        self.cast()
        assigned = {}
        already_blocked = self.mode == 'after_blocks'
        if already_blocked:
            self.emit('begin_blockers')
            self.block('blocker', 'second', assigned)
            self.emit('advance_combat')
        fee = [1 if self.mode == 'fuel_short' else 2, 0, 0, 0, 0, 0]
        payable = self.mode != 'fuel_short'
        self.wallet(self.owner, fee, funding=True)
        self.emit('can_activate', 'source', value=int(payable))
        if payable:
            self.emit('activate', 'source', value=1)
            self.choices()
            self.emit('assert_stack', value=1)
            self.wallet(self.owner, ZERO, funding=False)
            self.pools[self.owner] = ZERO.copy()
            self.cards['source']['zone'] = 'graveyard'
            self.emit('assert_zone_multiplicity', player=self.owner, zone=ZONES['graveyard'],
                      index=self.id, value=1)
            self.resolve()
            for attacker in attackers:
                if 'Flying' not in self.oracle[self.cards[attacker]['name']]['keywords']:
                    self.damage(attacker, 2)
            self.receipts()
        if attackers:
            if not already_blocked:
                self.emit('begin_blockers')
            self.finish(attackers, assigned, self.foe if self.mode == 'own_attack' else self.owner,
                        declaration_advanced=already_blocked)

    def ping(self, ref, target):
        actor = self.cards[ref]['player']
        self.emit('can_activate', ref, value=1)
        self.emit('choose_player', player=actor, value=target)
        self.emit('activate', ref, value=1)
        self.emit('assert_stack', value=1)
        self.choices()
        self.cards[ref]['tapped'] = True
        self.resolve()
        self.damage(target, 1)

    def ranger_activate(self, forest):
        self.emit('can_activate', 'source', value=1)
        self.observe('untap_target', 'tapped', 1)
        self.emit('choose_target', forest, player=self.owner)
        self.emit('choose_target', 'untap_target', player=self.owner)
        self.emit('activate', 'source', value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.cards[forest]['zone'] = 'hand'
        returned = sum(item['name'] == 'Forest' and item['zone'] == 'hand' and item['player'] == self.owner
                       for item in self.cards.values())
        self.emit('assert_zone_multiplicity', player=self.owner, zone=ZONES['hand'],
                  index=self.rows['Forest']['csvid'], value=returned)
        self.resolve()
        self.cards['untap_target']['tapped'] = False
        self.receipts()

    def ranger(self):
        if self.mode == 'protected_flight':
            self.attack(('incoming',))
            self.cast()
            self.emit('can_target_creature', 'blue_spell', 'source', value=0)
            self.emit('can_target_creature', 'blue_spell', 'idle', value=1)
            self.emit('begin_blockers')
            assigned = {}
            self.block('source', 'incoming', assigned)
            self.finish(('incoming',), assigned, self.owner)
            self.receipts()
            self.next_turn(self.owner)
            self.observe('source', 'sick', 0)
            self.attack(('source',))
            self.emit('begin_blockers')
            self.block('blue_flyer', 'source', {}, allowed=False)
            self.block('ground', 'source', {}, allowed=False)
            self.finish(('source',), {}, self.foe)
            return
        self.cast()
        if self.mode == 'wrong_land':
            self.emit('can_activate', 'source', value=0)
            return
        self.observe('source', 'sick', 1)
        self.ranger_activate('first_forest')
        if self.mode in ('same_turn', 'reset'):
            self.emit('can_activate', 'source', value=0)
        if self.mode == 'reset':
            self.next_turn(self.owner)
            self.ping('untap_target', self.foe)
            self.ranger_activate('reserve_forest')

    def hixus(self):
        if self.mode == 'stale':
            self.cast()
            self.next_turn(self.foe)
        attackers = ('first', 'second')
        self.attack(attackers)
        if self.mode != 'stale':
            self.cast()
        if self.mode == 'noncombat':
            self.ping('pinger', self.owner)
            self.receipts()
        if self.mode == 'removed_before':
            self.cast('removal', target='source')
            self.cards['source']['zone'] = 'graveyard'
        self.emit('begin_blockers')
        self.finish(attackers, {}, self.owner)
        if self.mode not in ('stale', 'removed_before'):
            for ref in attackers:
                self.cards[ref]['zone'] = 'exile'
        self.receipts()
        if self.mode == 'release':
            self.cast('removal', target='source')
            self.cards['source']['zone'] = 'graveyard'
            for ref in attackers:
                item = self.cards[ref]
                item.update(zone='battlefield', tapped=False)
                self.emit('bind_permanent', ref, player=self.foe, value=self.rows[item['name']]['csvid'], index=-1)

    def add_damage_witnesses(self):
        if self.id in (AMBUSHER, THIEF):
            for index, name in enumerate(DECK):
                self.add(f'library_{index}', name, self.owner, 'library')
        if self.id == AMBUSHER:
            if self.mode in ('combat_lethal', 'prevented'):
                self.add('incoming', GIANT, self.foe)
            if self.mode == 'prevented':
                self.add('fog', FOG, self.owner, 'hand')
            if self.mode in ('spell_lethal', 'spell_survives', 'unrelated'):
                self.add('first_shock', SHOCK, self.foe, 'hand')
            if self.mode == 'spell_survives':
                self.add('growth', GROWTH, self.owner, 'hand')
        elif self.id == THIEF:
            self.add('incoming', TUSKER if self.mode == 'dies_blocking' else BEAR, self.foe)
        else:
            if self.mode == 'noncombat_twice':
                self.add('first_shock', SHOCK, self.foe, 'hand')
                self.add('second_shock', SHOCK, self.foe, 'hand')
            else:
                self.add('incoming', TUSKER if self.mode == 'lethal' else GIANT, self.foe)
                if self.mode == 'prevented':
                    self.add('fog', FOG, self.owner, 'hand')

    def draw(self, count):
        available = [item for item in self.cards.values() if item['zone'] == 'library']
        assert len(available) >= count and all(item['player'] == self.owner for item in available)
        for item in available[:count]:
            item['zone'] = 'hand'

    def take_damage_spell(self, spell, target):
        self.cast(spell, target=target)
        self.damage(target, 2)

    def damage_follower_combat(self, *, block=True, prevent=False):
        incoming = ('incoming',)
        self.attack(incoming)
        if self.cards['source']['zone'] == 'hand':
            self.cast()
        self.emit('begin_blockers')
        assigned = {}
        if block:
            self.block('source', 'incoming', assigned)
        self.emit('advance_combat')
        if self.id == THIEF and block:
            self.energy += 1
            self.emit('assert_energy', player=self.owner, value=self.energy)
        if prevent:
            self.cast('fog')
        self.finish(incoming, assigned, self.owner, declaration_advanced=True, prevented=prevent)

    def ambusher(self):
        if self.mode in ('combat_lethal', 'prevented'):
            prevented = self.mode == 'prevented'
            self.damage_follower_combat(prevent=prevented)
            self.draw(0 if prevented else self.stats('incoming')[0])
            return
        self.cast()
        if self.mode == 'spell_survives':
            self.cast('growth', target='source')
            self.cards['source']['boost'] = 3
        target = 'idle' if self.mode == 'unrelated' else 'source'
        self.take_damage_spell('first_shock', target)
        self.draw(0 if target == 'idle' else 2)

    def thief(self):
        self.damage_follower_combat(block=self.mode != 'does_not_block')
        self.receipts()
        if self.mode not in ('charge_and_draw', 'underfunded'):
            return
        if self.mode == 'underfunded':
            self.next_turn(self.owner)
            self.observe('source', 'sick', 0)
            self.emit('can_activate', 'source', value=0)
            return
        for _ in range(2):
            self.next_turn(self.owner)
            self.next_turn(self.foe)
            self.damage_follower_combat()
            self.receipts()
        self.observe('source', 'sick', 0)
        self.emit('can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.choices()
        self.emit('assert_stack', value=1)
        self.observe('source', 'tapped', 1)
        self.energy -= 3
        self.emit('assert_energy', player=self.owner, value=self.energy)
        self.cards['source']['tapped'] = True
        self.resolve()
        self.draw(1)

    def add_enrage_counter(self):
        assert self.cards['source']['zone'] == 'battlefield'
        self.cards['source']['boost'] = self.cards['source'].get('boost', 0) + 1

    def sailback(self):
        if self.mode == 'noncombat_twice':
            self.cast()
            for spell in ('first_shock', 'second_shock'):
                self.take_damage_spell(spell, 'source')
                self.add_enrage_counter()
                self.receipts()
            return
        self.damage_follower_combat(prevent=self.mode == 'prevented')
        if self.mode in ('combat_growth', 'expiry'):
            self.add_enrage_counter()
        self.receipts()
        if self.mode == 'expiry':
            self.next_turn(self.owner)

    def receipts(self):
        self.emit('assert_stack')
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            if self.id == THIEF:
                self.emit('assert_energy', player=seat, value=self.energy if seat == self.owner else 0)
            self.wallet(seat, self.pools[seat])
            local = [item for item in self.cards.values() if item['player'] == seat]
            counts = Counter((item['name'], item['zone']) for item in local)
            for zone, code in ZONES.items():
                if zone == 'library' and self.id not in FOLLOWERS:
                    continue
                self.emit('assert_zone', player=seat, value=sum(item['zone'] == zone for item in local), zone=code)
                for name in sorted({item['name'] for item in local}):
                    self.emit('assert_zone_multiplicity', player=seat, value=counts[name, zone],
                              zone=code, index=self.rows[name]['csvid'])
        for ref, item in self.cards.items():
            if item['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', item['tapped'])
                if 'Creature' in self.oracle[item['name']]['type_line']:
                    self.observe(ref, 'damage', item['damage'])
        if self.cards['source']['zone'] == 'battlefield' and self.id != TRAP:
            if self.id == SAILBACK:
                self.observe('source', 'plus1_counters', self.cards['source'].get('boost', 0))
            for prop, value in zip(('power', 'toughness'), self.stats('source'), strict=True):
                self.observe('source', prop, value)

    def run(self):
        self.start(self.owner if self.mode in ('defender', 'short', 'own_attack', 'empty', 'stale')
                   else self.foe)
        paths = {INTERCEPTOR: self.interceptor, TRAP: self.trap, RANGER: self.ranger, HIXUS: self.hixus,
                 AMBUSHER: self.ambusher, THIEF: self.thief, SAILBACK: self.sailback}
        paths[self.id]()
        self.receipts()
