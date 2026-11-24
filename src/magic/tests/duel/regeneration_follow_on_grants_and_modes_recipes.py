"""Oracle-derived regeneration, costs, intrinsic riders, and modal activation cases."""
from collections import Counter, defaultdict
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import COLORS, cost_from_oracle

FAMILY = 'regeneration_follow_on_grants_and_modes'
PROFILE = Path(__file__).with_name('data') / 'regeneration_follow_on_grants_and_modes_wave8_profiles.json'

DRUDGE, LOCUST, CORE, MOLTEN = 1636, 2530, 1230, 11308
PREFIX, SEED = 'RegenerationFollowOnGrantsAndModesRecipe', 2964
MODES = {
    DRUDGE: ('tokens', 'protect_token', 'leaves', 'not_enough', 'foreign_graveyard', 'wrong_color'),
    LOCUST: ('untap_limit', 'flying', 'regenerate', 'regen_after_untap', 'wrong_color'),
    CORE: ('ping', 'upkeep_pay', 'upkeep_decline', 'upkeep_empty', 'regen', 'regen_exact'),
    MOLTEN: ('creature', 'player', 'walker', 'no_creature', 'upkeep_pay', 'upkeep_decline', 'regen'),
}
HAND, BATTLE, LIBRARY, GRAVE, EXILE = range(5)
ZERO = (0,) * 6
BLACK, GREEN = (0, 1, 0, 0, 0, 0), (0, 0, 0, 1, 0, 0)
BEAR, GIANT, GUARD = 'Grizzly Bears', 'Hill Giant', 'Elite Vanguard'
SKELETON, NONTOKEN, WALKER = 'Skeleton', 'Drudge Skeletons', 'Jace Beleren'
MURDER, DISENCHANT, FOREST, GROWTH = 'Murder', 'Disenchant', 'Forest', 'Giant Growth'
WITNESSES = {BEAR, GIANT, GUARD, SKELETON, NONTOKEN, WALKER, MURDER, DISENCHANT, FOREST, GROWTH}


class Board:
    def __init__(self, row, mode, owner, rows, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.mode, self.owner, self.foe = mode, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.pools, self.life = [ZERO, ZERO], [20, 20]
        self.seeded_graves = set()
        self.add('source', self.name, owner, HAND)
        if self.id == DRUDGE:
            grave_owner = self.foe if mode == 'foreign_graveyard' else owner
            self.add('grave_a', BEAR, grave_owner, GRAVE)
            if mode != 'not_enough':
                self.add('grave_b', GIANT, grave_owner, GRAVE)
            self.add('junk', FOREST, owner, GRAVE)
            if mode == 'protect_token':
                self.add('removal', MURDER, self.foe, HAND)
            if mode == 'leaves':
                self.add('removal', DISENCHANT, self.foe, HAND)
                self.add('non_token', NONTOKEN, self.foe, BATTLE)
                self.add('other_drudge', self.name, self.foe, HAND)
                self.add('foreign_a', BEAR, self.foe, GRAVE)
                self.add('foreign_b', GIANT, self.foe, GRAVE)
        elif self.id == LOCUST:
            if mode == 'untap_limit':
                self.add('spend', GROWTH, owner, HAND)
            if mode in ('untap_limit', 'flying'):
                self.add('ground', BEAR, self.foe, BATTLE)
            if mode in ('regenerate', 'regen_after_untap'):
                self.add('removal', MURDER, self.foe, HAND)
        else:
            if mode in ('upkeep_pay', 'upkeep_decline'):
                self.add('discard', FOREST, owner, HAND)
            if mode in ('regen', 'regen_exact'):
                self.add('removal', MURDER, self.foe, HAND)
                if mode == 'regen_exact':
                    self.add('unaffected', GIANT, self.foe, BATTLE)
            if mode in ('ping', 'creature', 'walker', 'no_creature'):
                victim = GUARD if self.id == CORE else WALKER if mode == 'walker' else GIANT
                self.add('victim', victim, self.foe, BATTLE)
            if self.id == MOLTEN and mode in ('creature', 'player', 'walker'):
                self.add('grave_cost', BEAR, owner, GRAVE)
                self.add('grave_junk', FOREST, owner, GRAVE)
            if mode == 'no_creature':
                self.add('grave_junk', FOREST, owner, GRAVE)
                self.add('enemy_grave', BEAR, self.foe, GRAVE)

    def emit(self, op, ref='', other='', *, prop='', player=0, value=0, zone=HAND, index=0, color=0):
        action = dict(op=op)
        if ref:
            action['ref'] = ref
        if other:
            action['other'] = other
        if prop:
            action['counter_kind' if op == 'assert_counter' else 'property'] = prop
        if op in SEATED:
            action['player'] = player
        if op in VALUES:
            action['value'] = value
        if op in ZONE_OPS:
            action['zone'] = ZONES[zone]
        if op in INDEXED:
            action['index'] = index
        if op in ('set_mana', 'assert_mana'):
            action['color'] = color
        self.steps.append(action)

    def see(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def add(self, ref, name, owner, zone):
        assert ref not in self.cards
        self.cards[ref] = [name, owner, zone]
        self.setup.append(dict(name=name, ref=ref, player=owner, zone=ZONES[zone]))
        if zone == GRAVE:
            self.seeded_graves.add(ref)

    def move(self, ref, zone):
        self.cards[ref][2] = zone

    def fund(self, player, mana):
        self.pools[player] = tuple(mana)
        for color, count in enumerate(mana):
            self.emit('set_mana', player=player, color=color, value=count)
        self.emit('refresh')

    def see_mana(self, player):
        for color, count in enumerate(self.pools[player]):
            self.emit('assert_mana', player=player, color=color, value=count)

    def numeric(self, count=0):
        self.emit('assert_unused_choices', value=count)

    def state(self):
        self.emit('assert_stack')
        zones = defaultdict(Counter)
        for name, player, zone in self.cards.values():
            zones[(player, zone)][name] += 1
        for p in (0, 1):
            self.see_mana(p)
            self.emit('assert_life', player=p, value=self.life[p])
            for zone in (HAND, BATTLE, GRAVE, EXILE):
                names = zones[p, zone]
                self.emit('assert_zone', player=p, zone=zone, value=names.total())
                if zone != BATTLE:
                    for name, count in sorted(names.items()):
                        self.emit('assert_zone_multiplicity', player=p, zone=zone, value=count,
                                  index=self.rows[name]['csvid'])
        for ref, (_, _, zone) in self.cards.items():
            if ref not in self.seeded_graves:
                self.see(ref, 'in_play', int(zone == BATTLE))

    def settle(self, unused=0):
        self.emit('resolve')
        self.numeric(unused)
        self.emit('refresh')
        self.emit('assert_stack')

    def cast(self, ref, target=None, *, existing=False):
        name, player, old = self.cards[ref]
        assert old == HAND
        cost = cost_from_oracle(self.oracle[name])
        if existing:
            assert self.pools[player] == tuple(cost)
        else:
            self.fund(player, cost)
        if target:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.numeric()
        self.emit('assert_stack', value=1)
        self.pools[player] = ZERO
        self.see_mana(player)

    def activate(self, expected=1, *, source='source'):
        self.emit('activate', source, value=expected)
        self.numeric()
        self.emit('assert_stack', value=expected)

    def removal(self, target, *, unused=0, menu=False, reserve=ZERO):
        self.cast('removal', target)
        self.emit('choose_regeneration', target)
        if menu:
            self.emit('choose_dialog', player=self.owner, value=0)
        if unused:
            # Correct paid regeneration never asks for the unrelated damage target.
            self.emit('choose_target', 'unaffected', player=self.owner)
        self.settle(unused)
        self.move('removal', GRAVE)
        self.pools[self.owner] = reserve
        self.see(target, 'tapped', 1)
        self.see(target, 'damage', 0)
        self.see(target, 'attacking', 0)
        if unused:
            self.see('unaffected', 'damage', 0)
        self.state()

    def drudge(self):
        valid = self.mode in ('tokens', 'protect_token', 'leaves')
        if not valid:
            self.fund(self.owner, GREEN if self.mode == 'wrong_color' else BLACK)
            self.emit('can_activate', 'source', value=0)
            self.activate(0)
            self.state()
            return
        self.create_token(self.owner, 'source', ('grave_a', 'grave_b'), 'token')
        if self.mode == 'protect_token':
            self.fund(self.owner, BLACK)
            self.removal('token')
        elif self.mode == 'leaves':
            self.advance(self.foe)
            self.cast('other_drudge')
            self.settle()
            self.move('other_drudge', BATTLE)
            self.create_token(self.foe, 'other_drudge', ('foreign_a', 'foreign_b'), 'foreign_token')
            self.fund(self.owner, BLACK)
            self.cast('removal', 'source')
            self.settle()
            for ref in ('source', 'removal'):
                self.move(ref, GRAVE)
            for ref in ('token', 'foreign_token'):
                self.move(ref, -1)
            self.state()

    def create_token(self, player, source, cost_refs, token):
        self.fund(player, BLACK)
        self.emit('can_activate', source, value=1)
        for ref in cost_refs:
            self.emit('choose_zone_card', player=player, value=self.rows[self.cards[ref][0]]['csvid'])
        self.activate(source=source)
        self.pools[player] = ZERO
        for ref in cost_refs:
            self.move(ref, EXILE)
        self.see_mana(player)
        self.emit('assert_zone', player=player, zone=EXILE, value=2)
        self.settle()
        token_id = self.rows[SKELETON]['csvid']
        self.emit('bind_permanent', token, player=player, value=token_id, index=-1)
        self.cards[token] = [SKELETON, player, BATTLE]
        for prop, val in (('token', 1), ('color', 1 << COLORS['B']), ('power', 1), ('toughness', 1)):
            self.emit('assert_matching_permanents', player=player, prop=prop, value=val, index=token_id)
        self.state()

    def advance(self, player, *, upkeep=False):
        assert self.pools == [ZERO, ZERO]
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        if upkeep:
            self.emit('upkeep_step', player=player)
            self.numeric()
        self.emit('turn', player=player)

    def locust_untap(self):
        self.fund(self.owner, GREEN)
        self.emit('can_activate', 'source', value=1)
        self.activate()
        self.pools[self.owner] = ZERO
        self.see_mana(self.owner)
        self.settle()
        self.see('source', 'tapped', 0)

    def locust(self):
        if self.mode == 'wrong_color':
            self.fund(self.owner, BLACK)
            self.emit('can_activate', 'source', value=0)
            self.activate(0)
            self.state()
            return
        if self.mode in ('regenerate', 'regen_after_untap'):
            if self.mode == 'regen_after_untap':
                self.locust_untap()
            self.fund(self.owner, GREEN)
            menu = self.mode == 'regenerate'
            if menu:
                self.emit('present_ai_dialogs', value=1)
            self.removal('source', menu=menu)
            return
        self.advance(self.foe)
        self.advance(self.owner)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.see('source', 'tapped', 1)
        if self.mode == 'untap_limit':
            self.locust_untap()
            self.fund(self.owner, GREEN)
            self.emit('can_activate', 'source', value=0)
            self.activate(0)
            self.see_mana(self.owner)
        self.emit('begin_blockers')
        self.emit('block', 'ground', 'source', value=0)
        self.emit('advance_combat')
        self.see('source', 'blocked', 0)
        self.emit('combat_damage')
        self.settle()
        self.life[self.foe] -= 1
        self.emit('end_combat')
        self.state()
        if self.mode == 'untap_limit':
            self.cast('spend', 'source', existing=True)
            self.settle()
            self.move('spend', GRAVE)
            self.advance(self.foe)
            self.locust_untap()
            self.state()

    def upkeep(self):
        self.advance(self.foe, upkeep=True)
        self.state()
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        if self.mode != 'upkeep_empty':
            self.emit('choose_dialog', player=self.owner, value=int(self.mode != 'upkeep_pay'))
        if self.mode == 'upkeep_pay' and self.owner == 0:
            self.emit('choose_zone_card', player=self.owner, value=self.rows[FOREST]['csvid'])
        elif self.mode == 'upkeep_pay':
            self.emit('choose_dialog', player=self.owner, value=0)
        self.emit('upkeep_step', player=self.owner)
        self.numeric()
        if self.mode == 'upkeep_pay':
            self.move('discard', GRAVE)
        else:
            self.move('source', GRAVE)
        self.state()

    def masticores(self):
        if self.mode.startswith('upkeep'):
            self.upkeep()
            return
        if self.mode in ('regen', 'regen_exact'):
            reserve = (2, 0, 0, 0, 0, 0) if self.id == CORE and self.mode == 'regen' else ZERO
            self.fund(self.owner, (2 + reserve[0], 0, 0, 0, 0, 0))
            self.removal('source', unused=int(self.mode == 'regen_exact'), reserve=reserve)
            return
        self.fund(self.owner, (2 if self.id == CORE else 4, 0, 0, 0, 0, 0))
        valid = self.mode != 'no_creature'
        self.emit('can_activate', 'source', value=int(valid))
        if not valid:
            self.activate(0)
            self.state()
            return
        if self.mode == 'player':
            self.emit('choose_player', player=self.owner, value=self.foe)
        else:
            self.emit('choose_target', 'victim', player=self.owner)
        if self.id == MOLTEN:
            self.emit('choose_zone_card', player=self.owner, value=self.rows[BEAR]['csvid'])
        self.activate()
        self.pools[self.owner] = ZERO
        if self.id == MOLTEN:
            self.move('grave_cost', EXILE)
            self.emit('assert_zone_multiplicity', player=self.owner, zone=EXILE, value=1, index=self.rows[BEAR]['csvid'])
        self.see_mana(self.owner)
        self.settle()
        if self.mode == 'player':
            self.life[self.foe] -= 4
        else:
            self.move('victim', GRAVE)
        self.state()

    def run(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        self.cast('source')
        self.settle()
        self.move('source', BATTLE)
        self.see('source', 'tapped', 0)
        for attr in ('power', 'toughness'):
            if attr in self.oracle[self.name]:
                self.see('source', attr, int(self.oracle[self.name][attr]))
        self.state()
        if self.id == DRUDGE:
            self.drudge()
        elif self.id == LOCUST:
            self.locust()
        else:
            self.masticores()

TROLL, SERVANT, DRAGON, COLLECTOR = 219, 13800, 11361, 7748
SWAMP, STONE, CORPSE, SPIDER = 'Swamp', 'Stone Rain', 'Walking Corpse', 'Silklash Spider'
FOLLOW_MODES = {
    TROLL: ('own_swamp', 'foreign_swamp', 'own_forest', 'lose_swamp', 'regen'),
    SERVANT: ('inspired_attack', 'stayed_untapped', 'regen_then_untap', 'wrong_color'),
    DRAGON: ('fresh_denial', 'haste_poison', 'infect_creature', 'regen'),
    COLLECTOR: ('upkeep_self', 'upkeep_other', 'upkeep_foreign', 'regen', 'unfunded'),
}
WITNESSES.update((SWAMP, STONE, CORPSE, SPIDER))
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZONE_OPS = {'assert_zone', 'assert_zone_multiplicity'}
INDEXED = {'assert_zone_multiplicity', 'assert_matching_permanents', 'bind_permanent'}
SEATED = {'turn', 'untap_step', 'upkeep_step', 'choose_dialog', 'choose_target',
          'choose_zone_card', 'choose_player', 'set_mana', 'assert_mana', 'assert_life',
          'assert_poison', 'bind_permanent', 'assert_matching_permanents'} | ZONE_OPS
VALUES = {'choose_dialog', 'choose_zone_card', 'choose_player', 'cast', 'activate', 'play_land',
          'set_mana', 'assert_mana', 'assert_life', 'assert_card', 'assert_matching_permanents',
          'assert_poison', 'assert_counter', 'assert_stack', 'assert_unused_choices', 'bind_permanent',
          'can_activate', 'attack', 'block', 'present_ai_dialogs'} | ZONE_OPS
SHIELD_GAP = 'Paid regeneration is tested during actual destruction. Proactive shield creation or expiration is outside this matrix.'


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (not pin or not exact(record, pin['oracle']) or csvid not in MODES | FOLLOW_MODES
            or pin['registry']['csvid'] != csvid or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')
            or 'Regenerate' not in record.get('keywords', ())):
        raise ValueError('whole pinned normal regeneration source required')
    return dict(csvid=csvid, mana=cost_from_oracle(record))


def variants(shape):
    csvid = shape['csvid']
    modes = MODES.get(csvid, FOLLOW_MODES.get(csvid))
    return [f'{mode}_{seat}' for mode in modes for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported regeneration grant and mode variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('reserved regeneration grant and mode source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared regeneration grant and mode parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(MODES | FOLLOW_MODES)):
        raise ValueError('complete regeneration grant and mode witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            record = oracle.get(name)
            if not exact(record, pin['oracle']) or not exact(row, pin['registry']):
                raise ValueError('regeneration grant or mode pin changed: ' + name)
            if group == 'sources':
                if row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy'):
                    raise ValueError('implemented regeneration source required')
                classify(record, row['csvid'])


class FollowBoard(Board):
    def __init__(self, row, mode, owner, rows, oracle):
        super().__init__(row, mode, owner, rows, oracle)
        if self.id == TROLL:
            if mode in ('own_swamp', 'own_forest', 'lose_swamp'):
                self.add('land', FOREST if mode == 'own_forest' else SWAMP, owner, HAND)
            if mode == 'foreign_swamp':
                self.add('land', SWAMP, self.foe, BATTLE)
            if mode == 'lose_swamp':
                self.add('answer', STONE, self.foe, HAND)
        elif self.id == SERVANT:
            if mode in ('regen_then_untap', 'wrong_color'):
                self.add('removal', MURDER, self.foe, HAND)
        elif self.id == DRAGON:
            if mode in ('haste_poison', 'infect_creature'):
                self.add('blocker', BEAR if mode == 'haste_poison' else SPIDER, self.foe, BATTLE)
        elif self.id == COLLECTOR:
            if mode == 'unfunded':
                self.add('removal', MURDER, self.foe, HAND)
            if mode == 'upkeep_other':
                self.add('own_black', CORPSE, owner, BATTLE)
                self.add('other_color', BEAR, owner, BATTLE)
            if mode == 'upkeep_foreign':
                self.add('enemy_black', CORPSE, self.foe, BATTLE)
                self.add('other_color', BEAR, owner, BATTLE)

    def troll(self):
        if self.mode == 'regen':
            self.fund(self.owner, BLACK)
            self.removal('source')
            return
        if self.mode != 'foreign_swamp':
            self.emit('play_land', 'land', value=1)
            self.move('land', BATTLE)
            self.emit('refresh')
        increased = self.mode in ('own_swamp', 'lose_swamp')
        for prop in ('power', 'toughness'):
            self.see('source', prop, 3 if increased else 2)
        if self.mode == 'lose_swamp':
            self.advance(self.foe)
            self.cast('answer', 'land')
            self.settle()
            self.move('answer', GRAVE)
            self.move('land', GRAVE)
            for prop in ('power', 'toughness'):
                self.see('source', prop, 2)
        self.state()

    def servant(self):
        if self.mode in ('regen_then_untap', 'wrong_color'):
            budget = (2, 1, 0, 0, 0, 0) if self.mode == 'regen_then_untap' else (3, 0, 0, 0, 0, 0)
            self.fund(self.owner, budget)
            if self.mode == 'wrong_color':
                self.cast('removal', 'source')
                self.settle()
                self.move('removal', GRAVE)
                self.move('source', GRAVE)
                self.state()
                return
            self.removal('source')
        if self.mode != 'regen_then_untap':
            self.advance(self.foe)
            self.advance(self.owner)
        if self.mode == 'inspired_attack':
            self.unblocked_hit()
            self.life[self.foe] -= 1
            self.see('source', 'tapped', 1)
            self.state()
        self.advance(self.foe)
        self.state()
        self.advance(self.owner)
        if self.mode != 'stayed_untapped':
            self.life[self.owner] += 1
            self.life[self.foe] -= 1
        self.see('source', 'tapped', 0)
        self.state()

    def unblocked_hit(self, *, blocker=None, attack_allowed=1, block_allowed=0):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=attack_allowed)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', blocker, 'source', value=block_allowed)
        self.emit('advance_combat')
        if attack_allowed:
            self.see('source', 'blocked', block_allowed)
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')

    def dragon(self):
        if self.mode == 'regen':
            self.fund(self.owner, (0, 2, 0, 0, 0, 0))
            self.removal('source')
            return
        if self.mode == 'fresh_denial':
            self.fund(self.owner, GREEN)
            self.emit('can_activate', 'source', value=0)
            self.activate(0)
            self.unblocked_hit(attack_allowed=0)
            self.emit('assert_poison', player=self.foe, value=0)
        else:
            self.fund(self.owner, BLACK)
            self.emit('can_activate', 'source', value=1)
            self.activate()
            self.pools[self.owner] = ZERO
            self.see_mana(self.owner)
            self.settle()
            blocked = int(self.mode == 'infect_creature')
            self.unblocked_hit(blocker='blocker', block_allowed=blocked)
            if blocked:
                self.emit('assert_counter', 'blocker', prop='minus1', value=4)
                self.see('blocker', 'toughness', 3)
                self.see('blocker', 'damage', 0)
                self.see('source', 'damage', 2)
            self.emit('assert_poison', player=self.foe, value=0 if blocked else 4)
            self.emit('assert_poison', player=self.owner, value=0)
        self.state()

    def collector(self):
        if self.mode == 'regen':
            self.fund(self.owner, (1, 1, 0, 0, 0, 0))
            self.removal('source')
            return
        if self.mode == 'unfunded':
            self.fund(self.owner, (2, 0, 0, 0, 0, 0))
            self.cast('removal', 'source')
            self.settle()
            self.move('source', GRAVE)
            self.move('removal', GRAVE)
            self.state()
            return
        self.advance(self.foe, upkeep=True)
        self.state()
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        chosen = 'own_black' if self.mode == 'upkeep_other' else 'source'
        self.emit('choose_target', chosen, player=self.owner)
        self.emit('upkeep_step', player=self.owner)
        self.numeric()
        self.move(chosen, HAND)
        self.state()

    def run(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        self.cast('source')
        self.settle()
        self.move('source', BATTLE)
        self.see('source', 'tapped', 0)
        for attr in ('power', 'toughness'):
            self.see('source', attr, int(self.oracle[self.name][attr]))
        self.state()
        action = {TROLL: self.troll, SERVANT: self.servant, DRAGON: self.dragon, COLLECTOR: self.collector}
        action[self.id]()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board_type = FollowBoard if csvid in FOLLOW_MODES else Board
    board = board_type(rows[name], mode, int(seat), rows, oracle)
    board.run()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=[SHIELD_GAP]))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered regeneration grant or mode source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
