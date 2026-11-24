"""Paid Flash responses, combat entry effects, damage, and actual attack legality."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'flash_entry_combat_and_damage'
PROFILE = Path(__file__).with_name('data') / 'flash_entry_combat_and_damage_wave8_profiles.json'
BARRIER = 15190
TUSKER = 'Krosan Tusker'

ANGEL, HELLKITE, CLOUD, SPIRIT = 14439, 1539, 9099, 12211
MODES = {
    ANGEL: ('enemy_attackers', 'own_attackers', 'blink', 'flight', 'short'),
    HELLKITE: ('split_response', 'face', 'two_players', 'flight', 'short'),
    CLOUD: ('reach_block', 'evoke', 'evoke_response', 'flying_scope', 'short'),
    SPIRIT: ('remove_attacker', 'remove_blocker', 'separate_flyer', 'empty', 'short'),
    BARRIER: ('shrink', 'response', 'fresh_block', 'expiry', 'defender'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'graveyard': 3, 'exile': 4}
ZERO, EVOKE = [0] * 6, [2, 0, 0, 2, 0, 0]
BEAR, GIANT, CADET = 'Grizzly Bears', 'Hill Giant', 'Eager Cadet'
AIR, DRAKE, WALL = 'Air Elemental', 'Wind Drake', 'Wall of Wood'
GROWTH, BLINK = 'Giant Growth', 'Cloudshift'
WITNESSES = frozenset(('Air Elemental', 'Cloudshift', 'Eager Cadet', 'Giant Growth', 'Grizzly Bears', 'Hill Giant', 'Krosan Tusker', 'Wall of Wood', 'Wind Drake'))

ZONE_NAMES = {code: name for name, code in ZONES.items()}
COSTS = {ANGEL: '{5}{W}{W}', HELLKITE: '{6}{R}{R}', CLOUD: '{2}{G}{G}{G}{G}',
         SPIRIT: '{3}{W}', BARRIER: '{3}{U}'}
KEYWORDS = {ANGEL: {'Flash', 'Flying'}, HELLKITE: {'Flash', 'Flying'}, CLOUD: {'Flash', 'Reach', 'Evoke'},
            SPIRIT: {'Flash', 'Flying'}, BARRIER: {'Flash', 'Defender'}}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in COSTS or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record.get('mana_cost') != COSTS[csvid]
            or set(record.get('keywords', ())) != KEYWORDS[csvid] or not record['type_line'].startswith('Creature — ')):
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
        cases.append(dict(name=f'FlashEntryCombatDamageRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2717, setup=board.setup, steps=board.steps,
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
        self.add('idle', GIANT, owner)
        if self.id == ANGEL:
            if mode in ('enemy_attackers', 'own_attackers', 'blink'):
                attacker = owner if mode == 'own_attackers' else self.foe
                self.add('bear', BEAR, attacker)
                self.add('drake', DRAKE, attacker)
                self.add('unmarked', GIANT, attacker)
            if mode == 'enemy_attackers':
                self.add('parent', GROWTH, self.foe, 'hand')
            if mode == 'blink':
                self.add('blink', BLINK, owner, 'hand')
        elif self.id == HELLKITE:
            if mode == 'split_response':
                self.add('attacker', CADET, self.foe)
                self.add('parent', GROWTH, self.foe, 'hand')
        elif self.id == CLOUD and mode != 'short':
            self.add('attacker', AIR, self.foe)
            self.add('my_flyer', DRAKE, owner)
            self.add('foe_ground', BEAR, self.foe)
            if mode == 'evoke_response':
                self.add('small_attacker', DRAKE, self.foe)
                self.add('parent', GROWTH, self.foe, 'hand')
        elif self.id == SPIRIT:
            if mode in ('remove_attacker', 'remove_blocker', 'separate_flyer'):
                self.add('attacker', BEAR, self.foe)
            if mode == 'remove_blocker':
                self.add('blocker', WALL, owner)
            if mode == 'separate_flyer':
                self.add('sky_attacker', DRAKE, self.foe)
        if self.id == BARRIER and mode != 'defender':
            self.add('attacker', TUSKER, self.foe)
            if mode == 'shrink':
                self.add('flyer', AIR, self.foe)
            if mode == 'response':
                self.add('parent', GROWTH, self.foe, 'hand')
        if mode == 'flight':
            self.add('ground', GIANT, self.foe)

    def add(self, ref, name, player, zone='battlefield'):
        assert player in (0, 1)
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone, damage=0, tapped=False)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'cast_response':
            self.steps.append(step(op, ref=ref, value=value))
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
        oracle = self.oracle[self.cards[ref]['name']]
        return tuple(int(oracle[k]) for k in ('power', 'toughness'))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def target(self, target, actor):
        if isinstance(target, str):
            self.emit('choose_target', target, player=actor)
        else:
            self.emit('choose_player', player=actor, value=target)

    def wallet(self, actor, cost, *, setting=False):
        for color, amount in enumerate(cost):
            self.emit('set_mana' if setting else 'assert_mana', player=actor, value=amount, color=color)
        if setting:
            self.pools[actor] = list(cost)
            self.emit('refresh')

    def cast(self, ref, *, target=None, fee=None, success=True, response=False, mode=None):
        item = self.cards[ref]
        who = item['player']
        cost = cost_from_oracle(self.oracle[item['name']]) if fee is None else fee
        self.wallet(who, cost, setting=True)
        if mode is not None:
            self.emit('choose_dialog', player=who, value=mode)
        if target is not None:
            self.target(target, who)
        self.emit('cast_response' if response else 'cast', ref, value=int(success))
        self.choices()
        self.emit('assert_stack', value=2 if response else int(success))
        if success:
            self.pools[who] = ZERO.copy()
        self.wallet(who, self.pools[who])

    def resolve(self, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def damage(self, recipient, amount):
        if isinstance(recipient, int):
            self.life[recipient] -= amount
        else:
            item = self.cards[recipient]
            item['damage'] += amount
            if item['damage'] >= self.stats(recipient)[1]:
                item['zone'] = 'graveyard'

    def cast_source(self, *, parent=None, parent_target=None, divided=(), entry_target=None, evoked=False):
        if parent is not None:
            self.cast(parent, target=parent_target)
            self.emit('announce_spell', parent)
        self.cast('source', fee=EVOKE if evoked else None, response=parent is not None,
                  mode=0 if self.id == CLOUD and not evoked else None)
        for target in divided:
            self.target(target, self.owner)
        if entry_target:
            self.target(entry_target, self.owner)
        self.resolve(response=parent is not None)
        self.cards['source']['zone'] = 'graveyard' if evoked else 'battlefield'
        if parent is not None:
            self.cards[parent]['zone'] = 'graveyard'
        if self.id == HELLKITE:
            assert len(divided) == 5
            for target, amount in Counter(divided).items():
                self.damage(target, amount)
        if self.id == CLOUD:
            for ref, item in self.cards.items():
                if item['zone'] == 'battlefield' and 'Flying' in self.oracle[item['name']]['keywords']:
                    self.damage(ref, 2)
            self.damage(self.owner, 2)
            self.damage(self.foe, 2)

    def start_combat(self, *attackers):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref, value=1)
            self.observe(ref, 'attacking', 1)
            self.cards[ref]['tapped'] = True
        self.emit('finish_attackers')

    def block(self, blocker, attacker, *, legal=True):
        self.emit('block', blocker, attacker, value=int(legal))
        self.observe(blocker, 'blocking_count', int(legal))

    def finish_combat(self, *, advanced=False):
        if not advanced:
            self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('refresh')
        self.choices()

    def next_turn(self, actor):
        assert all(wallet == ZERO for wallet in self.pools)
        self.emit('advance_turn')
        self.emit('untap_step', player=actor)
        self.emit('upkeep_step', player=actor)
        self.emit('turn', player=actor)
        for item in self.cards.values():
            if item['zone'] == 'battlefield':
                item['damage'] = 0
                if item['player'] == actor:
                    item['tapped'] = False

    def flight(self):
        self.next_turn(self.foe)
        self.next_turn(self.owner)
        self.observe('source', 'sick', 0)
        self.start_combat('source')
        self.emit('begin_blockers')
        self.block('ground', 'source', legal=False)
        self.finish_combat()
        self.damage(self.foe, self.stats('source')[0])

    def angel(self):
        if self.mode == 'flight':
            self.cast_source()
            self.flight()
            return
        if self.mode == 'blink':
            self.cast_source()
            self.next_turn(self.foe)
        self.start_combat('bear', 'drake')
        if self.mode == 'blink':
            self.cast('blink', target='source')
            self.resolve()
            self.cards['blink']['zone'] = 'graveyard'
            self.emit('bind_permanent', 'source', player=self.owner, value=self.id, index=-1)
            self.observe('bear', 'attacking', 1)
            self.observe('drake', 'attacking', 1)
            self.damage(self.owner, self.stats('bear')[0] + self.stats('drake')[0])
        else:
            parent = 'parent' if self.mode == 'enemy_attackers' else None
            self.cast_source(parent=parent, parent_target='bear')
            for ref in ('bear', 'drake'):
                self.cards[ref]['zone'] = 'exile'
        self.observe('unmarked', 'in_play', 1)
        self.emit('begin_blockers')
        self.finish_combat()

    def hellkite(self):
        if self.mode == 'split_response':
            self.start_combat('attacker')
            self.cast_source(parent='parent', parent_target='attacker',
                             divided=['attacker'] + [self.foe] * 3 + [self.owner])
            self.emit('begin_blockers')
            self.finish_combat()
        else:
            self.cast_source(divided=[self.foe] * (3 if self.mode == 'two_players' else 5)
                             + ([self.owner] * 2 if self.mode == 'two_players' else []))
            if self.mode == 'flight':
                self.flight()

    def cloudthresher(self):
        evoked = self.mode in ('evoke', 'evoke_response')
        if self.mode == 'flying_scope':
            self.cast_source()
            return
        response = self.mode == 'evoke_response'
        self.start_combat('attacker', *(['small_attacker'] if response else []))
        self.cast_source(parent='parent' if response else None, parent_target='small_attacker', evoked=evoked)
        self.emit('begin_blockers')
        if evoked:
            self.finish_combat()
            self.damage(self.owner, self.stats('attacker')[0])
        else:
            self.block('source', 'attacker')
            self.finish_combat()
            self.damage('attacker', self.stats('source')[0])
            self.damage('source', self.stats('attacker')[0])

    def spirit(self):
        if self.mode == 'empty':
            self.cast_source()
            return
        self.start_combat('attacker', *(['sky_attacker'] if self.mode == 'separate_flyer' else []))
        if self.mode == 'remove_blocker':
            self.emit('begin_blockers')
            self.block('blocker', 'attacker')
            self.emit('advance_combat')
            self.observe('attacker', 'blocked', 1)
            self.cast_source(entry_target='blocker')
            self.observe('blocker', 'blocking_count', 0)
            self.observe('attacker', 'blocked', 1)
            self.finish_combat(advanced=True)
        else:
            self.cast_source(entry_target='attacker')
            self.observe('attacker', 'attacking', 0)
            self.emit('begin_blockers')
            if self.mode == 'separate_flyer':
                self.block('source', 'sky_attacker')
            self.finish_combat()
            if self.mode == 'separate_flyer':
                self.damage('sky_attacker', self.stats('source')[0])
                self.damage('source', self.stats('sky_attacker')[0])

    def barrier(self):
        if self.mode == 'defender':
            self.cast_source()
            self.next_turn(self.foe)
            self.next_turn(self.owner)
            self.observe('source', 'sick', 0)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.observe('source', 'attacking', 0)
            self.emit('attack', 'idle', value=1)
            self.cards['idle']['tapped'] = True
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.finish_combat()
            self.damage(self.foe, self.stats('idle')[0])
            return

        attackers = ['attacker', 'flyer'] if self.mode == 'shrink' else ['attacker']
        self.start_combat(*attackers)
        parent = 'parent' if self.mode == 'response' else None
        self.cast_source(parent=parent, parent_target='attacker', entry_target='attacker')
        power, toughness = self.stats('attacker')
        boost = 3 if parent else 0
        reduced_power = power + boost - 4
        self.observe('attacker', 'power', reduced_power)
        self.observe('attacker', 'toughness', toughness + boost)
        self.observe('idle', 'power', self.stats('idle')[0])
        self.emit('begin_blockers')
        if self.mode == 'fresh_block':
            self.block('source', 'attacker')
        if self.mode == 'shrink':
            self.observe('flyer', 'power', self.stats('flyer')[0])
            self.block('source', 'flyer', legal=False)
        self.finish_combat()
        if self.mode == 'fresh_block':
            self.damage('source', reduced_power)
        else:
            self.damage(self.owner, reduced_power + (self.stats('flyer')[0] if self.mode == 'shrink' else 0))
        self.receipts()

        if self.mode == 'expiry':
            self.next_turn(self.owner)
            self.observe('attacker', 'power', power)
            self.next_turn(self.foe)
            self.start_combat('attacker')
            self.emit('begin_blockers')
            self.finish_combat()
            self.damage(self.owner, power)

    def receipts(self):
        self.emit('assert_stack')
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            self.wallet(seat, self.pools[seat])
            local = [item for item in self.cards.values() if item['player'] == seat]
            counts = Counter((item['name'], item['zone']) for item in local)
            for zone, code in ZONES.items():
                self.emit('assert_zone', player=seat, value=sum(item['zone'] == zone for item in local), zone=code)
                for name in sorted({item['name'] for item in local}):
                    self.emit('assert_zone_multiplicity', player=seat, value=counts[name, zone],
                              zone=code, index=self.rows[name]['csvid'])
        for ref, item in self.cards.items():
            if item['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', item['tapped'])
                self.observe(ref, 'damage', item['damage'])
        if self.cards['source']['zone'] == 'battlefield':
            for prop, amount in zip(('power', 'toughness'), self.stats('source'), strict=True):
                self.observe('source', prop, amount)

    def run(self):
        off_turn = self.mode in ('enemy_attackers', 'split_response', 'reach_block', 'evoke', 'evoke_response',
                                 'remove_attacker', 'remove_blocker', 'separate_flyer',
                                 'shrink', 'response', 'fresh_block', 'expiry')
        self.emit('turn', player=self.foe if off_turn else self.owner)
        for player in (0, 1):
            self.wallet(player, ZERO, setting=True)
        if self.mode == 'short':
            fee = (EVOKE if self.id == CLOUD else cost_from_oracle(self.oracle[self.name])).copy()
            next_colored = next(i for i, amount in enumerate(fee) if i > 0 and amount)
            fee[next_colored] -= 1
            self.cast('source', fee=fee, success=False)
            self.observe('source', 'in_hand', 1)
        else:
            paths = {ANGEL: self.angel, HELLKITE: self.hellkite, CLOUD: self.cloudthresher,
                     SPIRIT: self.spirit, BARRIER: self.barrier}
            paths[self.id]()
        self.receipts()
