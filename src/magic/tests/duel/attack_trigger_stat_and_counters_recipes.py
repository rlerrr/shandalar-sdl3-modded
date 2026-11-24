"""Paid attacks, counters, temporary stats, and printed riders."""
from collections import Counter, defaultdict
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'attack_trigger_stat_and_counters'
PROFILE = Path(__file__).with_name('data') / 'attack_trigger_stat_and_counters_wave8_profiles.json'
HERALD, HYDRA, OOZE, FIREBOAR, LARVA, CAPTAIN = 12396, 13306, 12242, 11073, 10688, 14901
EXEMPLAR, WITCH, LIEUTENANT, TERRA, MORTIS, SOLTARI = 16903, 8160, 15105, 13648, 11640, 3292
SERRA, KNIGHT, WORKER = 'Serra Angel', 'Benalish Knight', 'Arcbound Worker'
MURDER, FISTS, BOOMERANG = 'Murder', 'Fists of the Anvil', 'Boomerang'
VANGUARD, MONGOOSE, FOOT_SOLDIER = 'Elite Vanguard', 'Nimble Mongoose', 'Soltari Foot Soldier'

FLOW, CYCLOPS, HEIR, RAIDERS = 5370, 12946, 14281, 13039
MODES = {
    FLOW: ('solo', 'blocked', 'only_ally', 'blocks', 'repeat'),
    CYCLOPS: ('solo', 'attacks_blocked', 'blocks', 'ally_blocks', 'repeat'),
    HEIR: ('meets_power', 'below', 'power_on_foe', 'self_power', 'late_power', 'remove_support',
            'deathtouch', 'prevent_deathtouch', 'defends'),
    RAIDERS: ('solo', 'team', 'repeat', 'removed_ally', 'other_attackers', 'blocks',
              'hybrid_red', 'hybrid_green', 'wrong_mana'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6
BEAR, GIANT, BALOTH, WALL = 'Grizzly Bears', 'Hill Giant', 'Rumbling Baloth', 'Wall of Stone'
BOUNCE, GROWTH, FOG = 'Unsummon', 'Giant Growth', 'Fog'
DECK = ('Island', 'Mountain', 'Plains', 'Forest')
PREFIX = 'AttackTriggerStatAndCountersRecipe'
ZONE_NAMES = {code: name for name, code in ZONES.items()}
WITNESSES = frozenset(('Arcbound Worker', 'Benalish Knight', 'Boomerang', 'Elite Vanguard', 'Fists of the Anvil', 'Fog', 'Forest', 'Giant Growth', 'Grizzly Bears', 'Hill Giant', 'Island', 'Mountain', 'Murder', 'Nimble Mongoose', 'Plains', 'Rumbling Baloth', 'Serra Angel', 'Snow-Covered Forest', 'Soltari Foot Soldier', 'Unsummon', 'Wall of Stone'))


MODES.update({
    HERALD: ('flying_counter', 'own_costs', 'opponent_cost', 'two_counters'),
    HYDRA: ('trample', 'double', 'source_stays'),
    OOZE: ('destroy', 'death_combat', 'later_death', 'different_victim'),
    FIREBOAR: ('switch', 'asymmetric', 'blocked'),
    LARVA: ('forest_count', 'foe_only', 'land_after'),
    CAPTAIN: ('solo_bolster', 'tied_ally', 'tied_shroud', 'first_strike'),
    EXEMPLAR: ('self_entry', 'ally_entry', 'repeat'),
    WITCH: ('take', 'decline', 'opponent_cast', 'threshold', 'below'),
    LIEUTENANT: ('renown_team', 'blocked_first_strike', 'prevented_renown'),
    TERRA: ('foe_lands', 'owner_only', 'foe_bounce'),
    MORTIS: ('face', 'combat_death', 'unpowered_death', 'self_target'),
    SOLTARI: ('ground_illegal', 'shadow_blocks', 'shadow_defends'),
})
RIDER_METHODS = {
    HERALD: 'herald', HYDRA: 'hydra', OOZE: 'ooze', FIREBOAR: 'fireboar',
    LARVA: 'land_scaled', CAPTAIN: 'captain', EXEMPLAR: 'exemplar', WITCH: 'witch',
    LIEUTENANT: 'lieutenant', TERRA: 'land_scaled', MORTIS: 'mortis', SOLTARI: 'soltari',
}
HUMAN_CHOICES = {(WITCH, 'decline'), (MORTIS, 'self_target')}
GAPS = {
    WITCH: ('The Native AI uses a mandatory cast-trigger policy. The optional decline is exercised in seat 0.',),
    MORTIS: ('The Native AI automatically picks a legal preferred opponent. Alternate self targeting is exercised in seat 0.',),
    LIEUTENANT: ('Renown has no direct flag observer; its first-hit counter and later ally-only attack pump distinguish the state.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in MODES or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'Creature' not in record['type_line']
            or 'attacks' not in record['oracle_text'].lower()):
        raise ValueError('normal whole attack-trigger creature required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES[csvid]
            if owner == 0 or (csvid, mode) not in HUMAN_CHOICES]


def validate_variants(shape, chosen):
    if (type(chosen) is not list or not chosen or any(type(item) is not str for item in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('distinct supported attack-trigger variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned attack-trigger primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed attack-trigger declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODES.keys()):
        raise ValueError('exact attack-trigger sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('attack-trigger Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented attack-trigger primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, chosen, rows, oracle):
    validate_variants(shape, chosen)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact attack-trigger primary CSVID required')
    cases = []
    for variant in chosen:
        mode, seat = variant.rsplit('_', 1)
        board = Board(rows[name], int(seat), mode, rows, oracle)
        board.run()
        cases.append(dict(name=f'AttackTriggerStatAndCountersRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=3012, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=list(GAPS.get(csvid, ())))))
    return cases

class Board:
    def __init__(self, row, owner, mode, rows, oracle):
        self.id, self.name, self.owner, self.foe, self.mode = row['csvid'], row['name'], owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life, self.mana = [20, 20], [ZERO.copy(), ZERO.copy()]
        self.active = owner
        self.base = tuple(int(oracle[self.name][trait]) for trait in ('power', 'toughness'))
        self.counters = 0
        self.add('source', self.name, owner, 'hand')
        for actor in (0, 1):
            for index, name in enumerate(DECK):
                self.add(f'deck_{actor}_{index}', name, actor, 'library')
        if self.id in (FLOW, CYCLOPS):
            if mode in ('blocked', 'attacks_blocked'):
                self.add('enemy', BEAR if self.id == FLOW else GIANT, self.foe)
            if mode in ('blocks', 'ally_blocks'):
                self.add('enemy', BALOTH if self.id == CYCLOPS and mode == 'blocks' else BEAR, self.foe)
            if mode in ('only_ally', 'ally_blocks'):
                self.add('ally', GIANT, owner)
        if self.id == HEIR:
            if mode in ('meets_power', 'remove_support', 'below', 'power_on_foe', 'late_power'):
                self.add('support', BALOTH if mode in ('meets_power', 'remove_support') else GIANT, owner)
            if mode in ('power_on_foe', 'defends'):
                self.add('enemy', BALOTH, self.foe)
            if mode in ('self_power', 'late_power'):
                self.add('instant', GROWTH, owner, 'hand')
            if mode == 'remove_support':
                self.add('instant', BOUNCE, self.foe, 'hand')
            if mode in ('deathtouch', 'prevent_deathtouch'):
                self.add('enemy', WALL, self.foe)
            if mode == 'prevent_deathtouch':
                self.add('instant', FOG, self.foe, 'hand')
        if self.id == RAIDERS:
            if mode in ('team', 'repeat', 'removed_ally', 'other_attackers'):
                self.add('ally', BEAR, owner)
                self.add('big_ally', GIANT, owner)
                self.add('stays_back', GIANT, owner)
                self.add('enemy', BALOTH, self.foe)
            if mode == 'removed_ally':
                self.add('instant', BOUNCE, self.foe, 'hand')
            if mode == 'blocks':
                self.add('enemy', BALOTH, self.foe)

        self.build_follow_on()

    def add(self, ref, name, player, zone='battlefield'):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, actor=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'choose_trigger_source':
            self.steps.append(step(op, ref=ref, player=player))
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

    def stats(self, size, ref='source'):
        for prop, value in zip(('power', 'toughness'), size):
            self.observe(ref, prop, value)
        if ref == 'source' and self.id == RAIDERS:
            self.emit('assert_counter', ref, prop='plus1', value=self.counters)

    def choices(self):
        self.emit('assert_unused_choices')
        self.emit('assert_choices_consumed')

    def pool(self, player, amount, *, fund=False):
        for color, value in enumerate(amount):
            self.emit('set_mana' if fund else 'assert_mana', player=player, value=value, color=color)
        if fund:
            self.mana[player] = list(amount)
            self.emit('refresh')

    def settle(self):
        self.emit('resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def price(self, ref):
        if ref == 'source' and self.id == RAIDERS:
            if self.mode in ('hybrid_red', 'hybrid_green', 'wrong_mana'):
                color = {'hybrid_red': 4, 'hybrid_green': 3, 'wrong_mana': 2}[self.mode]
                result = [1, 0, 0, 0, 0, 0]
                result[color] = 3
                return result
            green = 2 if self.owner == 0 else 1
            return [1, 0, 0, green, 3-green, 0]
        return cost_from_oracle(self.oracle[self.cards[ref]['name']])

    def cast(self, ref, *, target=None, expected=True, destination=None, price=None, before_resolve=None):
        actor = self.cards[ref]['actor']
        self.pool(actor, self.price(ref) if price is None else price, fund=True)
        if target is not None:
            self.emit('choose_target', target, player=actor)
        self.emit('cast', ref, value=int(expected))
        self.choices()
        self.emit('assert_stack', value=int(expected))
        if not expected:
            self.observe(ref, 'in_hand', 1)
            return
        self.mana[actor] = ZERO.copy()
        self.pool(actor, ZERO)
        if before_resolve is not None:
            before_resolve()
        self.settle()
        if destination is None:
            destination = 'battlefield' if ref == 'source' else 'graveyard'
        self.cards[ref]['zone'] = destination

    def receipt(self):
        self.emit('assert_stack')
        for actor in (0, 1):
            self.emit('assert_life', player=actor, value=self.life[actor])
            self.pool(actor, self.mana[actor])
            counts = defaultdict(Counter)
            for item in self.cards.values():
                if item['actor'] == actor:
                    counts[item['zone']][item['name']] += 1
            for zone, code in ZONES.items():
                self.emit('assert_zone', player=actor, zone=code, value=counts[zone].total())
                for name, count in sorted(counts[zone].items()):
                    self.emit('assert_zone_multiplicity', player=actor, zone=code, value=count, index=self.rows[name]['csvid'])
        for ref, item in self.cards.items():
            if item['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)

    def visit(self):
        assert self.mana == [ZERO, ZERO]
        self.active = 1-self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=self.active)
        self.emit('upkeep_step', player=self.active)
        self.choices()
        self.emit('assert_stack')
        self.emit('turn', player=self.active)

    def declare(self, attackers, *, trigger_input=None):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref, value=1)
        if trigger_input is not None:
            trigger_input()
        self.emit('finish_attackers')
        self.emit('refresh')
        self.choices()

    def blocks(self, pairs=()):
        self.emit('begin_blockers')
        for blocker, attacker in pairs:
            self.emit('block', blocker, attacker, value=1)
            self.observe(blocker, 'blocking_count', 1)
        self.emit('advance_combat')
        self.emit('refresh')

    def damage(self):
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')

    def to_owner_main(self):
        self.visit()
        self.visit()

    def flow_or_cyclops(self):
        defending = self.mode in ('blocks', 'ally_blocks')
        if defending:
            self.visit()
            blocker = 'ally' if self.mode == 'ally_blocks' else 'source'
            before = (3, 6) if self.id == CYCLOPS and blocker == 'source' else self.base
            self.declare(('enemy',))
            self.stats(self.base)
            self.blocks(((blocker, 'enemy'),))
            self.stats(before)
            self.damage()
            if self.id == CYCLOPS and blocker == 'source':
                self.observe('source', 'damage', 4)
                self.observe('enemy', 'damage', 3)
            else:
                self.cards['enemy']['zone'] = 'graveyard'
                self.observe(blocker, 'damage', 2)
            self.visit()
            self.stats(self.base)
            self.observe(blocker, 'damage', 0)
            return
        self.to_owner_main()
        attack_stats = (self.base[0] + 3, 2 if self.id == FLOW else self.base[1])
        ally_only = self.mode == 'only_ally'
        self.declare(('ally',) if ally_only else ('source',))
        self.stats(self.base if ally_only else attack_stats)
        blocked = self.mode in ('blocked', 'attacks_blocked')
        self.blocks((('enemy', 'source'),) if blocked else ())
        self.damage()
        if blocked:
            for ref in ('source', 'enemy'):
                self.cards[ref]['zone'] = 'graveyard'
            return
        self.life[self.foe] -= 3 if ally_only else attack_stats[0]
        self.visit()
        self.stats(self.base)
        if self.mode == 'repeat':
            self.visit()
            self.declare(('source',))
            self.stats(attack_stats)
            self.blocks()
            self.damage()
            self.life[self.foe] -= attack_stats[0]
            self.visit()
            self.stats(self.base)

    def heir(self):
        if self.mode == 'defends':
            self.visit()
            self.declare(('enemy',))
            self.blocks((('source', 'enemy'),))
            self.stats(self.base)
            self.damage()
            for ref in ('source', 'enemy'):
                self.cards[ref]['zone'] = 'graveyard'
            return
        self.to_owner_main()
        if self.mode == 'self_power':
            self.cast('instant', target='source')
            self.stats((5, 5))
        self.declare(('source',))
        size = {'self_power': 6, 'meets_power': 3, 'remove_support': 3}.get(self.mode, 2)
        self.stats((size, size))
        if self.mode in ('late_power', 'remove_support'):
            self.cast('instant', target='support')
            if self.mode == 'late_power':
                self.stats((6, 6), 'support')
            else:
                self.cards['support']['zone'] = 'hand'
            self.stats((size, size))
        blocked = self.mode in ('deathtouch', 'prevent_deathtouch')
        self.blocks((('enemy', 'source'),) if blocked else ())
        if self.mode == 'prevent_deathtouch':
            self.cast('instant')
        self.damage()
        if self.mode == 'deathtouch':
            self.cards['enemy']['zone'] = 'graveyard'
        elif self.mode == 'prevent_deathtouch':
            self.observe('enemy', 'damage', 0)
        else:
            self.life[self.foe] -= size
        self.visit()
        self.stats(self.base)

    def raiders(self):
        if self.mode == 'blocks':
            self.visit()
            self.declare(('enemy',))
            self.blocks((('source', 'enemy'),))
            self.stats(self.base)
            self.damage()
            self.cards['source']['zone'] = 'graveyard'
            self.observe('enemy', 'damage', 3)
            return
        self.to_owner_main()
        team = self.mode in ('repeat', 'team', 'removed_ally', 'other_attackers')
        attends = self.mode != 'other_attackers'
        attackers = (('source',) if attends else ()) + (('ally', 'big_ally') if team else ())
        self.declare(attackers)
        self.counters = len(attackers) if attends else 0
        size = 3 + self.counters
        self.stats((size, size))
        if team:
            self.stats((3, 3), 'stays_back')
            self.stats((2, 2), 'ally')
        total = (size if attends else 0) + (5 if team else 0)
        if self.mode == 'removed_ally':
            self.cast('instant', target='big_ally')
            self.cards['big_ally']['zone'] = 'hand'
            self.stats((size, size))
            total -= 3
        self.blocks()
        self.damage()
        self.life[self.foe] -= total
        self.visit()
        self.stats((size, size))
        if self.mode == 'repeat':
            self.visit()
            self.declare(('source',))
            self.counters += 1
            size += 1
            self.stats((size, size))
            self.blocks()
            self.damage()
            self.life[self.foe] -= size
            self.visit()
            self.stats((size, size))

    def build_follow_on(self):
        mode = self.mode
        if self.id == HERALD:
            if mode == 'flying_counter':
                self.add('ground', GIANT, self.foe)
            if mode in ('own_costs', 'opponent_cost', 'two_counters'):
                actor = self.foe if mode == 'opponent_cost' else self.owner
                self.add('angel_spell', SERRA, actor, 'hand')
                if mode != 'two_counters':
                    self.add('human_spell', KNIGHT, actor, 'hand')
                if mode == 'own_costs':
                    self.add('other_spell', GIANT, actor, 'hand')
        elif self.id == HYDRA:
            if mode in ('double', 'source_stays'):
                self.add('home_worker', WORKER, self.owner, 'hand')
                self.add('away_worker', WORKER, self.foe, 'hand')
                self.add('zero_friend', BEAR, self.owner)
            if mode == 'trample':
                self.add('victim', BEAR, self.foe)
        elif self.id == OOZE:
            if mode == 'destroy':
                self.add('removal', MURDER, self.foe, 'hand')
            else:
                self.add('victim', BEAR if mode == 'death_combat' else WALL, self.foe)
                if mode != 'death_combat':
                    self.add('removal', MURDER, self.owner, 'hand')
                if mode == 'different_victim':
                    self.add('untouched', GIANT, self.foe)
        elif self.id == FIREBOAR:
            if mode == 'asymmetric':
                self.add('power_spell', FISTS, self.owner, 'hand')
            if mode == 'blocked':
                self.add('victim', BEAR, self.foe)
        elif self.id == LARVA:
            self.add('enemy_forest', 'Forest', self.foe)
            self.add('own_plains', 'Plains', self.owner)
            if mode != 'foe_only':
                self.add('forest_0', 'Forest', self.owner)
                self.add('forest_1', 'Snow-Covered Forest', self.owner)
            if mode == 'land_after':
                self.add('bounce_land', BOOMERANG, self.foe, 'hand')
        elif self.id == CAPTAIN:
            self.add('large_ally', GIANT, self.owner)
            if mode in ('tied_ally', 'tied_shroud'):
                self.add('small_ally', VANGUARD if mode == 'tied_ally' else MONGOOSE, self.owner)
            if mode == 'first_strike':
                self.add('victim', BEAR, self.foe)
        elif self.id == EXEMPLAR:
            self.add('counter_ally', BALOTH, self.owner)
            self.add('enemy_creature', BALOTH, self.foe)
        elif self.id == WITCH:
            if mode in ('take', 'decline'):
                self.add('cast_0', FOG, self.owner, 'hand')
            elif mode == 'opponent_cast':
                self.add('cast_0', BEAR, self.foe, 'hand')
            else:
                for index in range(3 if mode == 'threshold' else 4):
                    spell = GROWTH if index == 2 and mode == 'threshold' else BEAR
                    self.add(f'cast_{index}', spell, self.owner, 'hand')
                self.add('victim', WALL, self.foe)
        elif self.id == LIEUTENANT:
            if mode == 'renown_team':
                self.add('ally', BEAR, self.owner)
                self.add('stays_back', GIANT, self.owner)
            if mode == 'blocked_first_strike':
                self.add('victim', BEAR, self.foe)
            if mode == 'prevented_renown':
                self.add('fog', FOG, self.foe, 'hand')
        elif self.id == TERRA:
            self.add('owned_land', 'Forest', self.owner)
            if mode != 'owner_only':
                for index, name in enumerate(('Island', 'Mountain', 'Plains')):
                    self.add(f'foe_land_{index}', name, self.foe)
            if mode == 'foe_bounce':
                self.add('bounce_land', BOOMERANG, self.owner, 'hand')
        elif self.id == MORTIS:
            if mode == 'combat_death':
                self.add('victim', GIANT, self.foe)
            if mode in ('unpowered_death', 'self_target'):
                self.add('removal', MURDER, self.foe, 'hand')
        elif self.id == SOLTARI:
            self.add('victim', BEAR if mode == 'ground_illegal' else FOOT_SOLDIER, self.foe)

    def counter(self, count, ref='source', counter='plus1'):
        self.emit('assert_counter', ref, prop=counter, value=count)

    def block_gate(self, blocker, accepted):
        self.emit('begin_blockers')
        self.emit('block', blocker, 'source', value=int(accepted))
        self.emit('advance_combat')
        self.emit('refresh')

    def herald(self):
        first_size = 4
        self.to_owner_main()
        self.declare(('source',))
        self.counter(1)
        self.stats((first_size, first_size))
        if self.mode == 'flying_counter':
            self.block_gate('ground', False)
        else:
            self.blocks()
        self.damage()
        self.life[self.foe] -= first_size
        self.visit()
        self.counter(1)
        self.stats((first_size, first_size))
        if self.mode == 'flying_counter':
            return
        if self.mode != 'opponent_cost':
            self.visit()
        due = 0 if self.mode == 'opponent_cost' else 1
        if self.mode == 'two_counters':
            self.declare(('source',))
            self.counter(2)
            self.stats((5, 5))
            self.blocks()
            self.damage()
            self.life[self.foe] -= 5
            self.to_owner_main()
            due = 2
        for ref in ('angel_spell', 'human_spell', 'other_spell'):
            if ref not in self.cards:
                continue
            price = self.price(ref)
            if ref != 'other_spell':
                price[0] -= due
            self.cast(ref, destination='battlefield', price=price)

    def hydra(self):
        self.counter(4)
        self.stats((4, 4))
        if self.mode != 'trample':
            self.cast('home_worker', destination='battlefield')
            self.visit()
            self.cast('away_worker', destination='battlefield')
            self.visit()
            self.counter(1, 'home_worker')
            self.counter(1, 'away_worker')
        else:
            self.to_owner_main()
        stays = self.mode == 'source_stays'
        self.declare(('home_worker',) if stays else ('source',))
        size = 4 if stays else 8
        self.counter(size)
        self.stats((size, size))
        if self.mode != 'trample':
            self.counter(1 if stays else 2, 'home_worker')
            self.counter(1, 'away_worker')
            self.counter(0, 'zero_friend')
        self.blocks((('victim', 'source'),) if self.mode == 'trample' else ())
        self.damage()
        if self.mode == 'trample':
            self.cards['victim']['zone'] = 'graveyard'
            self.life[self.foe] -= 6
        else:
            self.life[self.foe] -= 1 if stays else size
        self.visit()
        self.counter(size)

    def ooze(self):
        self.counter(0)
        if self.mode == 'destroy':
            self.cast('removal', target='source')
            self.counter(0)
            self.stats((1, 1))
            return
        self.to_owner_main()
        self.declare(('source',))
        self.counter(1)
        self.stats((2, 2))
        self.blocks((('victim', 'source'),))
        self.damage()
        count = 1
        if self.mode == 'death_combat':
            self.cards['victim']['zone'] = 'graveyard'
            self.observe('source', 'damage', 2)
            count = 2
        else:
            self.observe('victim', 'damage', 2)
            self.counter(1)
            target = 'victim' if self.mode == 'later_death' else 'untouched'
            self.cast('removal', target=target)
            self.cards[target]['zone'] = 'graveyard'
            count += int(target == 'victim')
        self.counter(count)
        self.stats((count+1, count+1))
        self.visit()
        self.counter(count)

    def fireboar(self):
        self.to_owner_main()
        extra = 4 if self.mode == 'asymmetric' else 0
        if extra:
            self.cast('power_spell', target='source')
            self.stats((5, 7))
        self.declare(('source',))
        self.stats((7, 1+extra))
        blocked = self.mode == 'blocked'
        self.blocks((('victim', 'source'),) if blocked else ())
        self.damage()
        if blocked:
            for ref in ('source', 'victim'):
                self.cards[ref]['zone'] = 'graveyard'
        else:
            self.life[self.foe] -= 7
            self.visit()
            self.stats(self.base)

    def land_scaled(self):
        self.to_owner_main()
        if self.id == LARVA:
            count = 0 if self.mode == 'foe_only' else 2
            size = (2 + count, 2 + count)
            target = 'forest_0'
        else:
            count = 0 if self.mode == 'owner_only' else 3
            size = (count, 4)
            target = 'foe_land_0'
        self.declare(('source',))
        self.stats(size)
        if 'bounce_land' in self.cards:
            self.cast('bounce_land', target=target)
            self.cards[target]['zone'] = 'hand'
            self.stats(size)
        self.blocks()
        self.damage()
        self.life[self.foe] -= size[0]
        self.visit()
        self.stats(self.base)

    def captain(self):
        self.to_owner_main()
        tied = self.mode in ('tied_ally', 'tied_shroud')
        choice = lambda: self.emit('choose_target', 'small_ally', player=self.owner)
        self.declare(('source',), trigger_input=choice if tied else None)
        self.counter(int(not tied))
        self.stats((1, 1) if tied else (2, 2))
        self.counter(0, 'large_ally')
        if tied:
            self.counter(1, 'small_ally')
        first_strike = self.mode == 'first_strike'
        self.blocks((('victim', 'source'),) if first_strike else ())
        self.damage()
        if first_strike:
            self.cards['victim']['zone'] = 'graveyard'
            self.observe('source', 'damage', 0)
        else:
            self.life[self.foe] -= 1 if tied else 2
        self.visit()
        self.counter(1, 'small_ally' if tied else 'source')

    def exemplar(self):
        targets_ally = self.mode == 'ally_entry'
        on_entry = 'counter_ally' if targets_ally else 'source'
        self.counter(3, on_entry, 'minus1')
        self.counter(0, 'enemy_creature', 'minus1')
        self.stats((1, 1), on_entry)
        self.to_owner_main()
        attacks = 4 if self.mode == 'repeat' else 1
        for turn in range(attacks):
            self.declare(('source',))
            remaining = 0 if targets_ally else max(0, 2-turn)
            size = 4 - remaining
            self.counter(remaining, counter='minus1')
            self.stats((size, size))
            if turn < 3 and not targets_ally:
                self.life[self.owner] += 1
            self.emit('assert_life', player=self.owner, value=self.life[self.owner])
            self.blocks()
            self.damage()
            self.life[self.foe] -= size
            self.visit()
            if turn + 1 < attacks:
                self.visit()
        if targets_ally:
            self.counter(3, 'counter_ally', 'minus1')

    def witch(self):
        self.counter(0)
        if self.mode == 'opponent_cast':
            self.visit()
            self.cast('cast_0', destination='battlefield')
            self.counter(0)
            return
        if self.mode in ('threshold', 'below'):
            self.to_owner_main()
        count = 0
        for ref in (ref for ref in self.cards if ref.startswith('cast_')):
            growth = self.cards[ref]['name'] == GROWTH
            creature = self.cards[ref]['name'] == BEAR
            choose = self.mode != 'decline'
            def trigger():
                self.emit('choose_trigger_source', 'source', player=self.owner)
            self.cast(ref, target='source' if growth else None,
                      destination='battlefield' if creature else 'graveyard',
                      before_resolve=trigger if choose and self.owner == 0 else None)
            count += 2 * choose
            self.counter(count)
        if self.mode in ('take', 'decline'):
            self.visit()
            self.counter(count)
            return
        powerful = self.mode == 'threshold'
        size = 10 if powerful else 9
        self.stats((size, size))
        self.declare(('source',))
        self.stats((size, size))
        self.blocks((('victim', 'source'),))
        self.damage()
        self.cards['victim']['zone'] = 'graveyard'
        if powerful:
            self.life[self.foe] -= 2
        self.visit()
        self.stats((count+1, count+1))

    def lieutenant(self):
        self.to_owner_main()
        team = self.mode == 'renown_team'
        self.declare(('source', 'ally') if team else ('source',))
        self.counter(0)
        if team:
            self.stats((2, 2), 'ally')
        blocked = self.mode == 'blocked_first_strike'
        self.blocks((('victim', 'source'),) if blocked else ())
        prevention = self.mode == 'prevented_renown'
        if prevention:
            self.cast('fog')
        self.damage()
        if blocked:
            self.cards['victim']['zone'] = 'graveyard'
            self.observe('source', 'damage', 0)
        elif not prevention:
            self.life[self.foe] -= 4
        self.counter(int(team))
        if not team:
            self.stats(self.base)
            return
        self.stats((3, 2))
        self.to_owner_main()
        self.declare(('source', 'ally'))
        self.counter(1)
        self.stats((3, 2))
        self.stats((3, 3), 'ally')
        self.stats((3, 3), 'stays_back')
        self.blocks()
        self.damage()
        self.counter(1)
        self.life[self.foe] -= 6
        self.visit()
        self.stats((2, 2), 'ally')
        self.stats((3, 2))

    def mortis(self):
        mode = self.mode
        target = self.owner if mode == 'self_target' else self.foe
        def death_pick():
            if self.owner == 0:
                self.emit('choose_player', player=0, value=target)
        if mode in ('unpowered_death', 'self_target'):
            self.cast('removal', target='source', before_resolve=death_pick)
            self.cards['source']['zone'] = 'graveyard'
            self.life[target] -= 2
            return
        self.to_owner_main()
        self.declare(('source',))
        self.stats((4, 2))
        blocked = mode == 'combat_death'
        self.blocks((('victim', 'source'),) if blocked else ())
        if blocked:
            death_pick()
        self.damage()
        self.life[self.foe] -= 4
        if blocked:
            for ref in ('source', 'victim'):
                self.cards[ref]['zone'] = 'graveyard'
        else:
            self.visit()
            self.stats(self.base)

    def soltari(self):
        if self.mode == 'shadow_defends':
            self.visit()
            self.declare(('victim',))
            self.blocks((('source', 'victim'),))
            self.stats(self.base)
            self.damage()
            for ref in ('source', 'victim'):
                self.cards[ref]['zone'] = 'graveyard'
            return
        self.to_owner_main()
        self.declare(('source',))
        self.stats((2, 2))
        shadow = self.mode == 'shadow_blocks'
        self.block_gate('victim', shadow)
        self.damage()
        if shadow:
            self.cards['victim']['zone'] = 'graveyard'
            self.observe('source', 'damage', 1)
        else:
            self.life[self.foe] -= 2
        self.visit()
        self.stats(self.base)

    def run(self):
        self.emit('turn', player=self.active)
        for player in (0, 1):
            self.pool(player, ZERO, fund=True)
        if self.id == EXEMPLAR:
            def entry_input():
                target = 'counter_ally' if self.mode == 'ally_entry' else 'source'
                self.emit('choose_target', target, player=self.owner)
            self.cast('source', before_resolve=entry_input)
        else:
            self.cast('source', expected=self.mode != 'wrong_mana')
        if self.mode != 'wrong_mana':
            if self.id not in (HYDRA, EXEMPLAR):
                self.stats(self.base)
            if self.id in (FLOW, CYCLOPS):
                self.flow_or_cyclops()
            elif self.id == HEIR:
                self.heir()
            elif self.id == RAIDERS:
                self.raiders()
            else:
                getattr(self, RIDER_METHODS[self.id])()
        self.receipt()
