"""Paid land and shared regeneration provider fixtures from whole pinned Oracle."""
from functools import lru_cache
import json
from pathlib import Path

from recipes import COLORS, cost_from_oracle
from counter_condition_recurrence_recipes import exact

FAMILY = 'regeneration_land_and_grants'
PROFILE = Path(__file__).with_name('data') / 'regeneration_land_and_grants_wave8_profiles.json'

ELEPHANT, SWARM, HOLLOW, POOL = 414, 8691, 4211, 4062
CARD_NAMES = {ELEPHANT: 'Elephant Graveyard', SWARM: 'Swarmyard',
              HOLLOW: 'Yavimaya Hollow', POOL: 'Spawning Pool'}
MODES = {
    ELEPHANT: ('mana', 'own_elephant', 'foreign_elephant', 'heal', 'double_destroy', 'non_elephant'),
    SWARM: ('mana', 'insect', 'rat', 'spider', 'squirrel', 'foreign_spider', 'non_tribe', 'combat'),
    HOLLOW: ('mana', 'green_paid', 'foreign_paid', 'heal', 'combat', 'terminate', 'spent_green'),
    POOL: ('mana', 'animate_tapped', 'wrong_color', 'regenerate', 'double_destroy', 'skeleton', 'still_land'),
}
PREFIX = 'RegenerationLandAndGrantsRecipe'
SEED = 2897
HAND, BATTLEFIELD, LIBRARY, GRAVEYARD = range(4)
ZERO = [0] * 6
FOREST, GIANT, BEAR = 'Forest', 'Hill Giant', 'Grizzly Bears'
MAMMOTH, ANT, RAT = 'War Mammoth', 'Carrion Ants', 'Ravenous Rats'
SPIDER, SQUIRREL, BARON = 'Giant Spider', 'Squirrel Mob', 'Death Baron'
GROWTH, MURDER, SHOCK, TERMINATE, STONE = 'Giant Growth', 'Murder', 'Shock', 'Terminate', 'Stone Rain'
ANIMATED = 'Spawning Pool Animated'
WITNESSES = {FOREST, GIANT, BEAR, MAMMOTH, ANT, RAT, SPIDER, SQUIRREL, BARON,
             GROWTH, MURDER, SHOCK, TERMINATE, STONE}


class Board:
    def __init__(self, csvid, mode, owner, rows, oracle):
        self.id, self.mode, self.owner, self.foe = csvid, mode, owner, 1-owner
        self.name, self.rows, self.oracle = CARD_NAMES[csvid], rows, oracle
        self.setup, self.steps = [], []
        self.pools = [ZERO.copy(), ZERO.copy()]
        self.depth, self.animated = 0, False
        self.graves = [[], []]
        self.entities = {}
        self.recovered = []
        self.add('source', self.name, owner, HAND)
        self.add('quota', FOREST, owner, HAND)
        if self.id == POOL:
            self.setup_manland()
        else:
            self.setup_targeted()

    def setup_manland(self):
        self.add('probe', GROWTH, self.owner, HAND)
        if self.mode in ('regenerate', 'double_destroy'):
            self.add('removal', MURDER, self.foe, HAND)
        if self.mode == 'double_destroy':
            self.add('second', MURDER, self.foe, HAND)
        if self.mode == 'skeleton':
            self.add('baron', BARON, self.owner, BATTLEFIELD)
        if self.mode == 'still_land':
            self.add('stone', STONE, self.owner, HAND)

    def setup_targeted(self):
        if self.mode == 'mana':
            return
        if self.id == ELEPHANT:
            victim = BEAR if self.mode == 'non_elephant' else MAMMOTH
        elif self.id == SWARM:
            victim = {'insect': ANT, 'rat': RAT, 'spider': SPIDER, 'squirrel': SQUIRREL,
                      'foreign_spider': SPIDER, 'non_tribe': BEAR, 'combat': SPIDER}[self.mode]
        else:
            victim = GIANT
        side = self.foe if self.mode.startswith('foreign') else self.owner
        self.add('victim', victim, side, BATTLEFIELD)
        self.add('removal', TERMINATE if self.mode == 'terminate' else MURDER, self.foe, HAND)
        if self.mode == 'heal':
            self.add('shock', SHOCK, self.foe, HAND)
        if self.mode == 'double_destroy':
            self.add('second', MURDER, self.foe, HAND)

    def add(self, ref, name, side, zone):
        self.setup.append(dict(ref=ref, name=name, player=side, zone=ZONES[zone]))
        self.entities[ref] = dict(name=name, player=side, zone=zone, tapped=0, damage=0)

    def move(self, ref, zone):
        item = self.entities[ref]
        item['zone'] = zone
        if zone == GRAVEYARD:
            self.graves[item['player']].append(item['name'])

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        action = dict(op=op)
        if ref:
            action['ref'] = ref
        if other:
            action['other'] = other
        if prop:
            action['property'] = prop
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
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def fund(self, player, pool):
        self.pools[player] = list(pool)
        for color, count in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=count)
        self.emit('refresh')

    def see_mana(self, player):
        for color, count in enumerate(self.pools[player]):
            self.emit('assert_mana', player=player, color=color, value=count)

    def counts(self):
        self.emit('assert_stack', value=self.depth)
        for side in (0, 1):
            self.see_mana(side)
            self.emit('assert_life', player=side, value=20)
            hand = [item['name'] for item in self.entities.values() if item['player'] == side and item['zone'] == HAND]
            for zone, items in ((HAND, hand), (GRAVEYARD, self.graves[side])):
                if zone == HAND and any(item['player'] == side and item['zone'] == 'stack'
                                        for item in self.entities.values()):
                    continue
                self.emit('assert_zone', player=side, zone=zone, value=len(items))
                for index, name in enumerate(items):
                    self.emit('assert_zone_card', player=side, zone=zone, index=index, value=self.rows[name]['csvid'])
        for ref, item in self.entities.items():
            if item['zone'] == 'stack':
                continue
            live = item['zone'] == BATTLEFIELD
            self.see(ref, 'in_play', live)
            if live:
                self.see(ref, 'tapped', item['tapped'])
                if ref == 'source' and self.animated:
                    self.see(ref, 'csvid', self.rows[ANIMATED]['csvid'])
                    self.see(ref, 'color', 1 << COLORS['B'])
                    self.see(ref, 'power', 2 if self.mode == 'skeleton' else 1)
                    self.see(ref, 'toughness', 2 if self.mode == 'skeleton' else 1)
                    self.see(ref, 'damage', item['damage'])
                elif 'Creature' in self.oracle[item['name']]['type_line']:
                    self.see(ref, 'damage', item['damage'])

    def resolve(self, *, observe_choice_offer=False):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices' if observe_choice_offer else 'assert_choices_consumed')
        self.depth = 0

    def cast(self, ref, target):
        item = self.entities[ref]
        side = item['player']
        self.fund(side, cost_from_oracle(self.oracle[item['name']]))
        self.emit('choose_target', target, player=side)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.depth += 1
        self.move(ref, 'stack')
        self.pools[side] = ZERO.copy()
        self.emit('assert_stack', value=self.depth)
        self.see_mana(side)

    def play(self):
        self.emit('play_land', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.move('source', BATTLEFIELD)
        self.entities['source']['tapped'] = int(self.id == POOL)
        self.emit('play_land', 'quota', value=0)
        self.counts()

    def mana(self):
        if self.id == POOL:
            self.emit('require_can_activate', 'source', value=0)
            self.emit('activate', 'source', value=0)
            self.emit('assert_choices_consumed')
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.emit('advance_turn')
            self.emit('untap_step', player=self.owner)
            self.entities['source']['tapped'] = 0
            self.counts()
        self.emit('require_can_activate', 'source', value=1)
        if self.id == HOLLOW and self.owner == 1:
            self.emit('choose_dialog', player=self.owner, value=0)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.pools[self.owner][COLORS['B'] if self.id == POOL else 0] += 1
        self.entities['source']['tapped'] = 1
        self.counts()

    def kill_spell(self, ref, target, *, protect=False):
        self.cast(ref, target)
        if protect:
            self.emit('choose_regeneration', 'source')
            if self.id in (ELEPHANT, HOLLOW):
                self.emit('choose_dialog', player=self.owner, value=1)
            if self.id != POOL:
                self.emit('choose_target', target, player=self.owner)
        self.resolve()
        self.move(ref, GRAVEYARD)
        if protect:
            self.recovered.append(target)
            self.entities['source']['tapped'] = 1
            self.entities[target].update(tapped=1, damage=0)
            if self.id in (HOLLOW, POOL):
                self.pools[self.owner][COLORS['G'] if self.id == HOLLOW else COLORS['B']] -= 1
        else:
            self.move(target, GRAVEYARD)
            if target == 'source':
                self.animated = False
        self.counts()

    def targeting(self):
        if self.id == HOLLOW:
            self.fund(self.owner, cost_from_oracle({'mana_cost': '{G}'}) if self.mode != 'spent_green' else ZERO)
        if self.mode == 'heal':
            self.cast('shock', 'victim')
            self.resolve()
            self.move('shock', GRAVEYARD)
            self.entities['victim']['damage'] = 2
            self.counts()
        if self.mode == 'combat':
            self.emit('begin_attackers')
            self.emit('attack', 'victim', value=1)
            self.emit('finish_attackers')
            self.entities['victim']['tapped'] = 1
        protected = self.mode not in ('non_elephant', 'non_tribe', 'terminate', 'spent_green')
        self.kill_spell('removal', 'victim', protect=protected)
        if self.mode == 'double_destroy':
            self.kill_spell('second', 'victim')
        if self.mode == 'combat':
            self.see('victim', 'attacking', 0)
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.resolve()
            self.emit('end_combat')
            self.counts()

    def creature(self, expected):
        self.emit('can_target_creature', 'probe', 'source', value=expected)

    def pool(self):
        self.creature(0)
        black = 2 if self.mode in ('regenerate', 'double_destroy') else 1
        budget = [1, black, 0, 0, 0, 0] if self.mode != 'wrong_color' else [2, 0, 0, 0, 0, 0]
        self.fund(self.owner, budget)
        if self.mode == 'wrong_color':
            self.emit('require_can_activate', 'source', value=0)
            self.emit('activate', 'source', value=0)
            self.emit('assert_choices_consumed')
            self.creature(0)
            self.counts()
            return
        # The land is actually tapped from entry, so only Animate is available.
        self.emit('require_can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.depth = 1
        self.pools[self.owner] = [0, black-1, 0, 0, 0, 0]
        self.counts()
        self.resolve()
        self.animated = True
        self.creature(1)
        self.counts()
        if self.mode in ('regenerate', 'double_destroy'):
            self.kill_spell('removal', 'source', protect=True)
        if self.mode == 'double_destroy':
            self.kill_spell('second', 'source')
        elif self.mode == 'still_land':
            self.kill_spell('stone', 'source')
        else:
            self.emit('advance_turn')
            self.animated = False
            self.creature(0)
            self.see('source', 'csvid', POOL)
            self.counts()

    def run(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        self.play()
        if self.mode == 'mana':
            self.mana()
        elif self.id == POOL:
            self.pool()
        else:
            self.targeting()

CLOT, CRYPT, SEDGE, MASTER = 3104, 6357, 8661, 291
CONSTRUCT, CORPSE, SWAMP = 'Sliver Construct', 'Walking Corpse', 'Swamp'
CARD_NAMES.update({CLOT: 'Clot Sliver', CRYPT: 'Crypt Sliver', SEDGE: 'Sedge Sliver', MASTER: 'Zombie Master'})
FOLLOW_MODES = {
    CLOT: ('cast', 'wrong_color', 'unfunded'),
    CRYPT: ('cast', 'wrong_color', 'sick_source'),
    SEDGE: ('no_swamp', 'own_swamp', 'foreign_swamp', 'both_swamps', 'lose_swamp', 'lose_provider'),
    MASTER: ('own_walk', 'foreign_walk', 'own_land_only', 'source_excluded', 'wrong_tribe',
             'own_regenerate', 'foreign_regenerate', 'decline', 'unfunded', 'no_self_regeneration'),
}
WITNESSES.update((CONSTRUCT, CORPSE, SWAMP))
SWARM_HELD = tuple(mode for mode in MODES[SWARM] if mode not in ('mana', 'non_tribe'))
ZONES = ('hand', 'battlefield', 'library', 'graveyard')
ZONE_OPS = {'assert_zone', 'assert_zone_card'}
INDEXED = {'assert_zone_card'}
SEATED = {'turn', 'untap_step', 'choose_dialog', 'choose_target', 'set_mana', 'assert_mana',
          'assert_life'} | ZONE_OPS
VALUES = {'assert_unused_choices', 'choose_dialog', 'cast', 'play_land', 'activate', 'set_mana', 'assert_mana', 'assert_life',
          'assert_card', 'assert_stack', 'require_can_activate', 'can_target_creature',
          'attack', 'block'} | ZONE_OPS
SHARED_GAP = ('The printed grant uses separate unattached internal activation effects for both players. '
              'Visible named references exclude those effects, and the attached-legacy selector requires '
              'a recipient attachment. Direct grant activation and positive regeneration are untested.')
GAPS = {
    CLOT: [SHARED_GAP],
    CRYPT: [SHARED_GAP],
    SEDGE: [SHARED_GAP],
    SWARM: ['The typed regeneration and combat paths require separate review of a native crash during destruction resolution.'],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (not pin or not exact(record, pin['oracle']) or csvid not in CARD_NAMES
            or pin['registry']['csvid'] != csvid or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')
            or 'Regenerate' not in record.get('keywords', ())):
        raise ValueError('whole pinned normal regeneration source required')
    return dict(csvid=csvid, mana=ZERO.copy() if csvid in MODES else cost_from_oracle(record))


def variants(shape):
    csvid = shape['csvid']
    modes = MODES.get(csvid, FOLLOW_MODES.get(csvid))
    return [f'{mode}_{seat}' for mode in modes for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported regeneration variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('reserved regeneration source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared regeneration parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses', 'engine_faces'} or set(pins['witnesses']) != WITNESSES
            or set(pins['engine_faces']) != {ANIMATED}
            or not exact(rows.get(ANIMATED), pins['engine_faces'][ANIMATED])
            or set(pins['sources']) != set(CARD_NAMES.values())):
        raise ValueError('complete regeneration witnesses and animated land identity required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card'
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('regeneration source or witness pin changed: ' + name)
            if group == 'sources':
                if row.get('handler') in (None, '0', 'NULL', 'card_dummy'):
                    raise ValueError('implemented regeneration source required')
                classify(oracle[name], row['csvid'])


class CreatureBoard(Board):
    WALK = ('own_walk', 'foreign_walk', 'own_land_only', 'source_excluded', 'wrong_tribe')
    REGEN = ('own_regenerate', 'foreign_regenerate', 'decline', 'unfunded', 'no_self_regeneration')

    def __init__(self, csvid, mode, owner, rows, oracle):
        self.id, self.mode, self.owner, self.foe = csvid, mode, owner, 1-owner
        self.name, self.rows, self.oracle = CARD_NAMES[csvid], rows, oracle
        self.setup, self.steps = [], []
        self.pools, self.graves = [ZERO.copy(), ZERO.copy()], [[], []]
        self.depth, self.animated = 0, False
        self.entities, self.recovered = {}, []
        self.add('source', self.name, owner, HAND)
        if self.id == SEDGE:
            self.add('own_sliver', CONSTRUCT, owner, BATTLEFIELD)
            self.add('foreign_sliver', CONSTRUCT, self.foe, BATTLEFIELD)
            self.add('other_type', BEAR, owner, BATTLEFIELD)
            if mode in ('own_swamp', 'both_swamps', 'lose_swamp', 'lose_provider'):
                self.add('own_land', SWAMP, owner, HAND)
            if mode in ('foreign_swamp', 'both_swamps'):
                self.add('foreign_land', SWAMP, self.foe, BATTLEFIELD)
            if mode == 'lose_swamp':
                self.add('removal', STONE, self.foe, HAND)
            if mode == 'lose_provider':
                self.add('removal', MURDER, self.foe, HAND)
        if self.id == MASTER and mode in self.WALK:
            attacking = self.foe if mode == 'foreign_walk' else owner
            if mode != 'source_excluded':
                self.add('attacker', BEAR if mode == 'wrong_tribe' else CORPSE, attacking, BATTLEFIELD)
            self.add('blocker', GIANT if mode != 'source_excluded' else BEAR, 1-attacking, BATTLEFIELD)
            self.add('land', SWAMP, attacking if mode == 'own_land_only' else 1-attacking, BATTLEFIELD)
        if self.id == MASTER and mode in self.REGEN:
            controller = self.foe if mode == 'foreign_regenerate' else owner
            if mode != 'no_self_regeneration':
                self.add('zombie', CORPSE, controller, BATTLEFIELD)
            self.add('removal', MURDER, 1-controller, HAND)
        if (self.id, mode) in ((CLOT, 'unfunded'), (CRYPT, 'sick_source')):
            self.add('removal', MURDER, self.foe, HAND)

    def pay_source(self, *, affordable=True):
        cost = cost_from_oracle(self.oracle[self.name])
        pool = cost if affordable else [sum(cost), 0, 0, 0, 0, 0]
        self.fund(self.owner, pool)
        self.emit('cast', 'source', value=int(affordable))
        self.emit('assert_choices_consumed')
        if affordable:
            self.depth = 1
            self.pools[self.owner] = ZERO.copy()
            self.move('source', 'stack')
            self.emit('assert_stack', value=1)
            self.see_mana(self.owner)
            self.resolve()
            self.move('source', BATTLEFIELD)
            self.see('source', 'sick', 1)
        self.counts()
        if affordable:
            self.stats('source')

    def stats(self, ref, bonus=0):
        row = self.oracle[self.entities[ref]['name']]
        for field in ('power', 'toughness'):
            self.see(ref, field, int(row[field]) + bonus)

    def sedge(self):
        self.pay_source()
        if 'own_land' in self.entities:
            self.emit('play_land', 'own_land', value=1)
            self.move('own_land', BATTLEFIELD)
            self.emit('refresh')
        self.sliver_stats()
        if self.mode == 'lose_swamp':
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.emit('turn', player=self.foe)
            self.kill_spell('removal', 'own_land')
            self.sliver_stats()
        if self.mode == 'lose_provider':
            self.kill_spell('removal', 'source')
            self.sliver_stats()

    def sliver_stats(self):
        provider = self.entities['source']['zone'] == BATTLEFIELD
        if provider:
            self.stats('source', int(self.entities.get('own_land', {}).get('zone') == BATTLEFIELD))
        for name in ('own', 'foreign'):
            land = self.entities.get(f'{name}_land', {})
            self.stats(f'{name}_sliver', int(provider and land.get('zone') == BATTLEFIELD))
        self.stats('other_type')
        self.counts()

    def swampwalk(self):
        self.pay_source()
        ref = 'source' if self.mode == 'source_excluded' else 'attacker'
        attacker = self.entities[ref]
        if self.mode in ('foreign_walk', 'source_excluded'):
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            if self.mode == 'source_excluded':
                self.emit('advance_turn')
                self.emit('untap_step', player=self.owner)
            self.emit('turn', player=attacker['player'])
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked = int(self.mode not in ('own_walk', 'foreign_walk'))
        self.emit('block', 'blocker', ref, value=blocked)
        self.emit('advance_combat')
        self.see(ref, 'blocked', blocked)
        self.emit('combat_damage')
        self.resolve()
        if blocked:
            dead = 'blocker' if ref == 'source' else ref
            self.move(dead, GRAVEYARD)
            self.see(dead, 'in_play', 0)
            self.emit('assert_zone_card', player=self.entities[dead]['player'], zone=GRAVEYARD,
                      value=self.rows[self.entities[dead]['name']]['csvid'])
            survivor = ref if ref == 'source' else 'blocker'
            self.see(survivor, 'damage', 2)
            self.see(survivor, 'in_play', 1)
        else:
            self.see(ref, 'in_play', 1)
            self.see('blocker', 'in_play', 1)
        for side in (0, 1):
            took_hit = not blocked and side == 1-attacker['player']
            self.emit('assert_life', player=side, value=18 if took_hit else 20)
        self.emit('end_combat')
        self.emit('assert_choices_consumed')

    def master_regenerate(self):
        self.pay_source()
        victim = 'source' if self.mode == 'no_self_regeneration' else 'zombie'
        controller = self.entities[victim]['player']
        offered = self.mode not in ('unfunded', 'no_self_regeneration')
        protect = self.mode in ('own_regenerate', 'foreign_regenerate')
        if self.mode == 'foreign_regenerate':
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.emit('turn', player=self.foe)
        budget = ZERO.copy()
        if self.mode != 'unfunded':
            budget[COLORS['B']] = 1
        self.fund(controller, budget)
        self.cast('removal', victim)
        if offered:
            self.emit('choose_dialog', player=controller, value=int(not protect))
        # Oracle expects the lawful paid/declined grant to be offered here. Keep
        # numeric evidence if the engine omits the offer and leaves the choice.
        self.resolve(observe_choice_offer=offered)
        self.move('removal', GRAVEYARD)
        if protect:
            self.pools[controller] = ZERO.copy()
            self.entities[victim].update(tapped=1, damage=0)
        else:
            self.move(victim, GRAVEYARD)
        self.counts()

    def run(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        if self.id == SEDGE:
            self.sedge()
        elif self.id == MASTER:
            if self.mode in self.WALK:
                self.swampwalk()
            else:
                self.master_regenerate()
        else:
            self.pay_source(affordable=self.mode != 'wrong_color')
            if self.mode == 'unfunded':
                self.kill_spell('removal', 'source')
            if self.mode == 'sick_source':
                self.kill_spell('removal', 'source')


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    board_type = CreatureBoard if csvid in FOLLOW_MODES else Board
    board = board_type(csvid, mode, int(seat), rows, oracle)
    board.run()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=board.setup, steps=board.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, [])))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered regeneration source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
