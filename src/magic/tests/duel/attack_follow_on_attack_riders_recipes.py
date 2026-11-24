"""Oracle-pinned attack riders, targets, payments, and alternate modes."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import COLORS, cost_from_oracle

FAMILY = 'attack_follow_on_attack_riders'
PROFILE = Path(__file__).with_name('data') / 'attack_follow_on_attack_riders_wave8_profiles.json'

WURM, SWEEPER, ROC, AURELIA = 10804, 12840, 14433, 19287
PREFIX, SEED = 'AttackFollowOnAttackRidersRecipe', 3062
MODES = {
    WURM: ('sweep', 'indestructible', 'idle', 'late'),
    SWEEPER: ('own_land', 'foreign_land', 'empty_land', 'no_land', 'idle'),
    ROC: ('raid', 'raid_no_damage', 'no_raid', 'foe_attacked', 'gain_team', 'gain_single', 'raid_expired'),
    AURELIA: ('mentor', 'equal_power', 'both_colors', 'white_only', 'red_only', 'decline', 'foe_turn'),
}
HAND, BATTLE, LIBRARY, GRAVE = range(4)
ZERO = (0,) * 6
CAD, BEAR, GIANT = 'Eager Cadet', 'Grizzly Bears', 'Hill Giant'
FOREST, GROWTH, FERTILE = 'Forest', 'Wild Growth', 'Fertile Ground'
RELIC, MYR, THOPTER = 'Darksteel Relic', 'Darksteel Myr', 'Ornithopter'
VITALIZE, PIKER, RECRUIT = 'Vitalize', 'Goblin Piker', 'Boros Recruit'
LATE_BEAR, WALL, BIRD = 'Ashcoat Bear', 'Wall of Spears', 'Bird'
WITNESSES = {CAD, BEAR, GIANT, FOREST, GROWTH, FERTILE, RELIC, MYR, THOPTER, VITALIZE,
             PIKER, RECRUIT, LATE_BEAR, WALL, BIRD}

class Board:
    def __init__(self, row, mode, owner, rows, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.mode, self.owner, self.foe = mode, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.pools, self.life = [ZERO, ZERO], [20, 20]
        self.active = owner
        self.add('source', self.name, owner, HAND)
        self.setup_witnesses()

    def setup_witnesses(self):
        action = {WURM: self.wurm_setup, SWEEPER: self.sweeper_setup, ROC: self.roc_setup,
                  AURELIA: self.aurelia_setup}
        action[self.id]()

    def add(self, ref, name, player, zone=BATTLE, *, tapped=False):
        assert ref not in self.cards
        placed = dict(name=name, ref=ref, player=player, zone=ZONES[zone])
        if tapped:
            placed['state'] = ['tapped']
        self.setup.append(placed)
        self.cards[ref] = [name, player, zone]

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

    def fund(self, player, mana):
        self.pools[player] = tuple(mana)
        for color, count in enumerate(mana):
            self.emit('set_mana', player=player, color=color, value=count)
        self.emit('refresh')

    def see_mana(self, player):
        for color, count in enumerate(self.pools[player]):
            self.emit('assert_mana', player=player, color=color, value=count)

    def move(self, ref, zone):
        self.cards[ref][2] = zone

    def state(self):
        self.emit('assert_stack')
        for p in (0, 1):
            self.see_mana(p)
            self.emit('assert_life', player=p, value=self.life[p])
            for zone in (HAND, BATTLE, GRAVE):
                items = Counter(name for name, player, where in self.cards.values()
                                if where == zone and player == p)
                self.emit('assert_zone', player=p, zone=zone, value=items.total())
                if zone != BATTLE:
                    for name, count in sorted(items.items()):
                        self.emit('assert_zone_multiplicity', player=p, zone=zone, value=count,
                                  index=self.rows[name]['csvid'])
        for ref, (_, _, zone) in self.cards.items():
            self.see(ref, 'in_play', int(zone == BATTLE))

    def settle(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack')

    def cast(self, ref, targets=()):
        name, player, zone = self.cards[ref]
        assert zone == HAND
        self.fund(player, cost_from_oracle(self.oracle[name]))
        for target in targets:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.pools[player] = ZERO
        self.see_mana(player)
        self.settle()
        self.move(ref, GRAVE if self.oracle[name]['type_line'] == 'Instant' else BATTLE)

    def visit(self, player):
        assert player == 1-self.active and self.pools == [ZERO, ZERO]
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)
        self.active = player

    def attack(self, refs, *, target=None, already_begun=False, vigilant=()):
        if not already_begun:
            self.emit('begin_attackers')
        for ref in refs:
            self.emit('attack', ref, value=1)
            self.see(ref, 'attacking', 1)
            self.see(ref, 'tapped', int(ref not in vigilant))
        if target:
            self.emit('choose_target', target, player=self.owner)
        self.emit('finish_attackers')
        self.settle()

    def blockers(self, blocks=()):
        self.emit('begin_blockers')
        for blocker, attacker, legal in blocks:
            self.emit('block', blocker, attacker, value=legal)
        self.emit('advance_combat')
        self.settle()
        committed = {attacker for _, attacker, legal in blocks if legal}
        for _, attacker, _ in blocks:
            self.see(attacker, 'blocked', int(attacker in committed))

    def damage(self, *, hit=0, dead=(), marked=()):
        self.emit('combat_damage')
        self.settle()
        for ref in dead:
            self.move(ref, GRAVE)
        for ref, damage in marked:
            self.see(ref, 'damage', damage)
        self.life[1-self.active] -= hit
        self.emit('end_combat')
        self.state()

    def stats(self, ref, bonus=0):
        name = self.cards[ref][0]
        for prop in ('power', 'toughness'):
            self.see(ref, prop, int(self.oracle[name][prop])+bonus)

    def wurm_setup(self):
        self.add('partner', BEAR, self.owner)
        self.add('foreign_creature', GIANT, self.foe)
        self.add('artifact_creature', THOPTER, self.foe)
        self.add('noncreature', RELIC, self.owner)
        self.add('land', FOREST, self.foe)
        if self.mode == 'indestructible':
            self.add('guard', MYR, self.foe)
        if self.mode == 'late':
            self.add('late', LATE_BEAR, self.owner, HAND)

    def sweeper_setup(self):
        if self.mode == 'no_land':
            return
        self.add('own_land', FOREST, self.owner)
        self.add('foe_land', FOREST, self.foe)
        self.add('own_aura', GROWTH, self.owner, HAND)
        self.add('foe_aura', FERTILE, self.foe, HAND)
        self.add('other_aura', FERTILE, self.owner, HAND)
        self.add('clean_land', FOREST, self.owner)
        if self.mode == 'idle':
            self.add('partner', BEAR, self.owner)

    def roc_setup(self):
        if self.mode in ('raid', 'raid_no_damage', 'gain_team', 'raid_expired'):
            self.add('partner', BEAR, self.owner)
        if self.mode == 'gain_team':
            self.add('second', CAD, self.owner)
        if self.mode == 'raid_no_damage':
            self.add('guard', WALL, self.foe)
        elif self.mode in ('raid', 'gain_team', 'gain_single', 'foe_attacked'):
            self.add('guard', BEAR, self.foe)

    def aurelia_setup(self):
        if self.mode in ('mentor', 'equal_power'):
            self.add('partner', CAD if self.mode == 'mentor' else BEAR, self.owner)
            self.add('resting', CAD, self.owner)
            self.add('guard', BEAR, self.foe)
        elif self.mode == 'foe_turn':
            self.add('partner', CAD, self.owner, tapped=True)
            self.add('foe_attacker', BEAR, self.foe)
        else:
            witness = {'both_colors': RECRUIT, 'white_only': CAD, 'red_only': PIKER,
                       'decline': CAD}[self.mode]
            self.add('partner', witness, self.owner, tapped=True)
            if self.mode != 'decline':
                self.add('unlock', VITALIZE, self.owner, HAND)
                self.add('guard', CAD, self.foe)

    def cycle(self):
        self.visit(self.foe)
        self.visit(self.owner)

    def observe_source(self):
        self.see('source', 'tapped', 0)
        self.stats('source')
        self.state()

    def wurm(self):
        self.cast('source')
        self.observe_source()
        self.cycle()
        if self.mode == 'idle':
            self.attack(('partner',))
            self.state()
            self.blockers()
            self.damage(hit=2)
            return
        self.attack(('source', 'partner'))
        for ref in ('partner', 'foreign_creature', 'artifact_creature'):
            self.move(ref, GRAVE)
        # All victim moves are checked before combat damage. Land and relic live.
        self.state()
        if self.mode == 'late':
            self.cast('late')
            self.stats('late')
        if self.mode == 'indestructible':
            self.blockers((('guard', 'source', 1),))
            self.damage(marked=(('guard', 7), ('source', 0)))
        else:
            self.blockers()
            self.damage(hit=7)

    def sweeper(self):
        self.cast('source')
        self.observe_source()
        if self.mode == 'no_land':
            self.cycle()
            self.attack(('source',))
            self.blockers()
            self.damage(hit=4)
            return
        aura_land = 'foe_land' if self.mode == 'foreign_land' else 'own_land'
        other_land = 'own_land' if aura_land == 'foe_land' else 'foe_land'
        self.cast('own_aura', (aura_land,))
        self.cast('other_aura', (other_land,))
        self.visit(self.foe)
        self.cast('foe_aura', (aura_land,))
        self.visit(self.owner)
        self.state()
        if self.mode == 'idle':
            self.attack(('partner',))
        else:
            target = 'clean_land' if self.mode == 'empty_land' else aura_land
            self.attack(('source',), target=target)
            if self.mode != 'empty_land':
                self.move('own_aura', GRAVE)
                self.move('foe_aura', GRAVE)
        self.state()
        self.blockers()
        self.damage(hit=2 if self.mode == 'idle' else 4)

    def bird(self):
        bird_id = self.rows[BIRD]['csvid']
        self.emit('bind_permanent', 'token', player=self.owner, value=bird_id, index=-1)
        self.cards['token'] = [BIRD, self.owner, BATTLE]
        for prop, value in (('token', 1), ('color', 32), ('subtype_Bird', 1), ('power', 3), ('toughness', 4)):
            self.emit('assert_matching_permanents', player=self.owner, prop=prop, index=bird_id, value=value)

    def raid_attack(self):
        self.attack(('partner',))
        if self.mode == 'raid_no_damage':
            self.blockers((('guard', 'partner', 1),))
            self.damage(dead=('partner',), marked=(('guard', 0),))
        else:
            self.blockers()
            self.damage(hit=2)

    def roc(self):
        if self.mode in ('raid', 'raid_no_damage', 'raid_expired'):
            self.raid_attack()
            if self.mode == 'raid_expired':
                self.cycle()
        elif self.mode == 'foe_attacked':
            self.cycle_for_foe_attack()
        self.cast('source')
        if self.mode in ('raid', 'raid_no_damage'):
            self.bird()
        self.observe_source()
        if self.mode == 'raid':
            self.cycle()
            self.attack(('token',))
            self.blockers((('guard', 'token', 0),))
            self.damage(hit=3)
        if self.mode in ('gain_team', 'gain_single'):
            self.cycle()
            refs = ('source', 'partner', 'second') if self.mode == 'gain_team' else ('source',)
            self.attack(refs)
            self.life[self.owner] += len(refs)
            self.emit('assert_life', player=self.owner, value=self.life[self.owner])
            self.blockers((('guard', 'source', 0),))
            self.damage(hit=6 if self.mode == 'gain_team' else 3)

    def cycle_for_foe_attack(self):
        self.visit(self.foe)
        self.attack(('guard',))
        self.blockers()
        self.damage(hit=2)
        self.visit(self.owner)

    def aurelia(self):
        self.cast('source')
        self.observe_source()
        if self.mode in ('mentor', 'equal_power'):
            self.cycle()
            mentor = self.mode == 'mentor'
            self.attack(('source', 'partner'), target='partner' if mentor else None)
            self.emit('assert_counter', 'partner', prop='plus1', value=int(mentor))
            self.emit('assert_counter', 'resting', prop='plus1', value=0)
            self.stats('partner', int(mentor))
            self.blockers((('guard', 'source', 0),))
            self.damage(hit=4)
            self.visit(self.foe)
            self.emit('assert_counter', 'partner', prop='plus1', value=int(mentor))
            self.state()
            return
        if self.mode == 'foe_turn':
            self.visit(self.foe)
            self.emit('begin_required_attackers')
            self.settle()
            self.stats('partner')
            self.attack(('foe_attacker',), already_begun=True)
            self.blockers()
            self.damage(hit=2)
            return
        # A fresh Aurelia and a tapped witness leave no eligible attacker.
        # The production beginning-of-combat flag thus enables real event 0x89.
        self.see('source', 'sick', 1)
        self.see('partner', 'tapped', 1)
        decline = self.mode == 'decline'
        self.emit('choose_optional_target' if decline else 'choose_target', 'partner', player=self.owner)
        self.emit('begin_required_attackers')
        self.settle()
        if decline:
            self.emit('assert_forced_target_choices', value=0)
            self.stats('partner')
            self.attack((), already_begun=True)
            self.blockers()
            self.damage()
            return
        self.see('partner', 'power', int(self.oracle[self.cards['partner'][0]]['power'])+2)
        self.see('partner', 'toughness', 1)
        self.stats('source')
        self.cast('unlock')
        self.see('partner', 'tapped', 0)
        vigilance = () if self.mode == 'red_only' else ('partner',)
        self.attack(('partner',), already_begun=True, vigilant=vigilance)
        self.blockers((('guard', 'partner', 1),))
        excess = {'both_colors': 2, 'white_only': 0, 'red_only': 3}[self.mode]
        dead = ('guard',) if self.mode == 'both_colors' else ('guard', 'partner')
        self.damage(hit=excess, dead=dead)

    def run(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        {WURM: self.wurm, SWEEPER: self.sweeper, ROC: self.roc, AURELIA: self.aurelia}[self.id]()

OGRE, RAVINE, FAMILIAR, VAULT, MANA_DRAGON, WARBOSS = 11325, 10815, 14494, 14649, 11742, 19424
BONES, MORNINGSTAR, MURDER, STONE = 'Bonesplitter', 'Vulshok Morningstar', 'Murder', 'Stone Rain'
ISLAND, SPIDER, FLYER, GOBLIN = 'Island', 'Giant Spider', 'Welkin Tern', 'Goblin'
LEGEND = 'Isamaru, Hound of Konda'
ANIMATED = 'Raging Ravine Animated'
WITNESSES.update((BONES, MORNINGSTAR, MURDER, STONE, ISLAND, SPIDER, FLYER, GOBLIN, LEGEND))
FOLLOW_MODES = {
    OGRE: ('steal', 'attached', 'no_eligible', 'host_leaves'),
    RAVINE: ('entry', 'mana_red', 'mana_green', 'animate_tapped', 'attack_twice', 'still_land'),
    FAMILIAR: ('ordinary_attack', 'ordinary_legend', 'ordinary_idle'),
    VAULT: ('dash_loot', 'dash_decline', 'dash_empty', 'normal_loot', 'wrong_color'),
    MANA_DRAGON: ('attack_paid', 'block_paid', 'attack_zero', 'trample'),
    WARBOSS: ('fresh_token', 'token_blocked', 'mentor', 'foe_turn'),
}
ALL_MODES = MODES | FOLLOW_MODES
ZONES = ('hand', 'battlefield', 'library', 'graveyard')
ZONE_OPS = {'assert_zone', 'assert_zone_multiplicity', 'assert_zone_card'}
INDEXED = {'assert_zone_multiplicity', 'assert_zone_card', 'bind_permanent', 'bind_hand', 'assert_matching_permanents'}
SEATED = {'turn', 'untap_step', 'choose_target', 'choose_optional_target', 'choose_trigger_source',
          'choose_dialog', 'choose_optional_zone', 'choose_zone_card', 'choose_x', 'choose_color',
          'set_mana', 'assert_mana', 'assert_life', 'bind_permanent', 'bind_hand', 'assert_matching_permanents'} | ZONE_OPS
VALUES = {'cast', 'activate', 'can_activate', 'play_land', 'choose_dialog', 'choose_optional_zone',
          'choose_zone_card', 'choose_x', 'choose_color', 'assert_forced_target_choices',
          'assert_forced_zone_choices', 'set_mana', 'assert_mana', 'assert_life', 'assert_stack',
          'assert_unused_choices', 'assert_card', 'assert_matching_permanents', 'assert_counter',
          'bind_permanent', 'bind_hand', 'attack', 'block', 'can_target_creature'} | ZONE_OPS
GAPS = {
    AURELIA: ['The production beginning-of-combat gate is exercised with no preexisting legal attacker; ready-attacker gate remains separately untested.'],
    FAMILIAR: ['This is an ordinary duel with no designated commander. The harness has no commander designation path, so the positive Lieutenant buff and damage trigger are untested.'],
    MANA_DRAGON: ['Join forces uses the two available duel seats. The engine suppresses the AI opponent’s interactive contribution for a Human-controlled Dragon; exact expected paid-opponent cases expose this boundary. Multiplayer and additional teammate contributions are unavailable.'],
    WARBOSS: ['Fresh Warboss cases exercise the production no-ready-attacker gate and actual generated attacking token. Separate aged Mentor cases do not exercise the beginning-of-combat gate with existing legal attackers.'],
}


class FollowBoard(Board):
    def setup_witnesses(self):
        if self.id == OGRE:
            self.add('gear', BONES, self.owner if self.mode == 'no_eligible' else self.foe, HAND)
            self.add('enemy', BEAR, self.foe)
            if self.mode != 'no_eligible':
                self.add('other_gear', MORNINGSTAR, self.foe)
            else:
                self.add('not_gear', RELIC, self.foe)
            if self.mode == 'host_leaves':
                self.add('answer', MURDER, self.foe, HAND)
        elif self.id == RAVINE:
            self.add('probe', VITALIZE, self.owner, HAND)
            self.add('quota', FOREST, self.owner, HAND)
            if self.mode == 'still_land':
                self.add('stone', STONE, self.owner, HAND)
        elif self.id == FAMILIAR:
            self.add('guard', BEAR, self.foe)
            if self.mode in ('ordinary_legend', 'ordinary_idle'):
                # This is a real legendary permanent, without commander designation.
                self.add('legend', LEGEND, self.owner)
        elif self.id == VAULT:
            if self.mode in ('dash_loot', 'dash_decline', 'normal_loot'):
                self.add('feed', FOREST, self.owner, HAND)
                self.setup.append(dict(name=ISLAND, ref='top', player=self.owner, zone='library'))
        elif self.id == MANA_DRAGON:
            witness = FLYER if self.mode == 'block_paid' else SPIDER if self.mode == 'trample' else BEAR
            self.add('guard', witness, self.foe)
        elif self.id == WARBOSS:
            if self.mode == 'token_blocked':
                self.add('guard', BEAR, self.foe)
            if self.mode == 'mentor':
                self.add('partner', CAD, self.owner)
                self.add('resting', CAD, self.owner)
            if self.mode == 'foe_turn':
                self.add('guard', BEAR, self.foe)

    def paid_cast(self, ref, targets=(), *, mana=None, mode=None):
        name, player, zone = self.cards[ref]
        assert zone == HAND
        cost = cost_from_oracle(self.oracle[name]) if mana is None else mana
        self.fund(player, cost)
        if mode is not None:
            self.emit('choose_dialog', player=player, value=mode)
        for target in targets:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_unused_choices')
        self.emit('assert_stack', value=1)
        self.pools[player] = ZERO
        self.see_mana(player)
        self.settle()
        self.move(ref, GRAVE if self.oracle[name]['type_line'] in ('Instant', 'Sorcery') else BATTLE)

    def expect_counter(self, ref, count):
        self.emit('assert_counter', ref, prop='plus1', value=count)

    def equipment_migrates(self, before, after, controller):
        entry = self.cards.pop(before)
        self.emit('bind_permanent', after, player=controller, index=-1,
                  value=self.rows[entry[0]]['csvid'])
        self.cards[after] = [entry[0], controller, BATTLE]

    def foe_equip(self):
        self.fund(self.foe, (1, 0, 0, 0, 0, 0))
        self.emit('can_activate', 'gear', value=1)
        self.emit('choose_target', 'enemy', player=self.foe)
        self.emit('activate', 'gear', value=1)
        self.emit('assert_unused_choices')
        self.emit('assert_stack', value=1)
        self.pools[self.foe] = ZERO
        self.see_mana(self.foe)
        self.settle()
        self.see('enemy', 'power', 4)

    def ogre(self):
        self.paid_cast('source')
        if self.mode == 'no_eligible':
            self.paid_cast('gear')
            self.cycle()
            self.attack(('source',))
            self.stats('source')
            self.blockers()
            self.damage(hit=4)
            return
        self.visit(self.foe)
        self.paid_cast('gear')
        if self.mode == 'attached':
            self.foe_equip()
        self.visit(self.owner)
        self.state()
        self.attack(('source',), target='gear')
        self.equipment_migrates('gear', 'stolen', self.owner)
        self.see('source', 'power', 6)
        self.see('source', 'toughness', 4)
        self.stats('enemy')
        self.state()
        if self.mode == 'host_leaves':
            self.paid_cast('answer', ('source',))
            self.move('source', GRAVE)
            self.state()
        self.blockers()
        self.damage(hit=0 if self.mode == 'host_leaves' else 6)
        self.visit(self.foe)
        self.equipment_migrates('stolen', 'returned', self.foe)
        if self.mode != 'host_leaves':
            self.stats('source')
        self.stats('enemy')
        self.state()

    def animate(self, *, tapped=False, counters=0):
        self.fund(self.owner, (2, 0, 0, 1, 1, 0))
        self.emit('can_activate', 'source', value=1)
        if not tapped:
            self.emit('choose_dialog', player=self.owner, value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_unused_choices')
        self.emit('assert_stack', value=1)
        self.pools[self.owner] = ZERO
        self.see_mana(self.owner)
        self.settle()
        self.see('source', 'csvid', self.rows[ANIMATED]['csvid'])
        self.see('source', 'tapped', int(tapped))
        self.emit('can_target_creature', 'probe', 'source', value=1)
        self.emit('assert_matching_permanents', player=self.owner, prop='color', value=24,
                  index=self.rows[ANIMATED]['csvid'])
        self.expect_counter('source', counters)
        for prop in ('power', 'toughness'):
            self.see('source', prop, 3+counters)

    def ravine(self):
        self.emit('play_land', 'source', value=1)
        self.emit('assert_unused_choices')
        self.move('source', BATTLE)
        self.see('source', 'tapped', 1)
        self.emit('play_land', 'quota', value=0)
        self.emit('can_target_creature', 'probe', 'source', value=0)
        self.state()
        if self.mode == 'entry':
            self.emit('can_activate', 'source', value=0)
            self.emit('activate', 'source', value=0)
            self.state()
            return
        if self.mode == 'animate_tapped':
            self.animate(tapped=True)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.attack((), already_begun=True)
            self.blockers()
            self.damage()
            return
        self.cycle()
        self.see('source', 'tapped', 0)
        if self.mode in ('mana_red', 'mana_green'):
            color = COLORS['R' if self.mode == 'mana_red' else 'G']
            self.emit('choose_color', player=self.owner, value=color)
            self.emit('activate', 'source', value=1)
            self.emit('assert_unused_choices')
            pool = list(ZERO)
            pool[color] = 1
            self.pools[self.owner] = tuple(pool)
            self.see('source', 'tapped', 1)
            self.state()
            return
        self.animate()
        if self.mode == 'still_land':
            self.paid_cast('stone', ('source',))
            self.move('source', GRAVE)
            self.state()
            return
        self.attack(('source',))
        self.expect_counter('source', 1)
        self.see('source', 'power', 4)
        self.blockers()
        self.damage(hit=4)
        self.cycle()
        self.see('source', 'csvid', self.id)
        self.emit('can_target_creature', 'probe', 'source', value=0)
        self.expect_counter('source', 1)
        self.animate(counters=1)
        self.attack(('source',))
        self.expect_counter('source', 2)
        self.see('source', 'power', 5)
        self.blockers()
        self.damage(hit=5)

    def familiar(self):
        self.paid_cast('source')
        self.stats('source')
        self.state()
        if self.mode == 'ordinary_idle':
            self.attack(('legend',))
            self.see('source', 'tapped', 0)
            self.blockers()
            self.damage(hit=2)
            return
        self.attack(('source',))
        self.stats('source')
        self.state()
        self.blockers((('guard', 'source', 0),))
        self.damage(hit=5)

    def vaultbreaker(self):
        if self.mode == 'wrong_color':
            self.fund(self.owner, (4, 0, 0, 0, 0, 0))
            self.emit('cast', 'source', value=0)
            self.emit('assert_unused_choices')
            self.state()
            return
        normal = self.mode == 'normal_loot'
        self.paid_cast('source', mana=None if normal else (2, 0, 0, 0, 1, 0), mode=1 if normal else None)
        self.stats('source')
        self.state()
        if normal:
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.attack((), already_begun=True)
            self.blockers()
            self.damage()
            self.cycle()
        fed = self.mode != 'dash_empty'
        decline = self.mode == 'dash_decline'
        if fed:
            self.emit('assert_zone_card', player=self.owner, zone=LIBRARY, value=self.rows[ISLAND]['csvid'])
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        if fed:
            if self.owner == 0:
                self.emit('choose_trigger_source', 'source', player=self.owner)
            self.emit('choose_optional_zone' if decline else 'choose_zone_card',
                      player=self.owner, value=self.rows[FOREST]['csvid'])
        self.emit('finish_attackers')
        self.settle()
        if fed and not decline:
            self.move('feed', GRAVE)
            self.emit('bind_hand', 'drawn', player=self.owner, value=self.rows[ISLAND]['csvid'], index=-1)
            self.cards['drawn'] = [ISLAND, self.owner, HAND]
        if decline:
            self.emit('assert_forced_zone_choices')
        self.emit('assert_zone', player=self.owner, zone=LIBRARY, value=int(decline))
        self.state()
        self.blockers()
        self.damage(hit=4)
        self.emit('cleanup')
        self.emit('refresh')
        if not normal:
            self.move('source', HAND)
            self.emit('assert_zone_multiplicity', player=self.owner, zone=HAND, value=1, index=self.id)
        self.state()

    def queue_contributions(self, *, announce=False):
        if announce and self.owner == 0:
            self.emit('choose_trigger_source', 'source', player=0)
        for player in (self.owner, self.foe):
            if self.pools[player][0] or player == 0:
                self.emit('choose_x', player=player, value=self.pools[player][0])

    def finish_join_forces(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices')
        self.emit('assert_stack')
        total = sum(pool[0] for pool in self.pools)
        self.pools = [ZERO, ZERO]
        for seat in (0, 1):
            self.see_mana(seat)
        self.see('source', 'power', 5+total)
        self.see('source', 'toughness', 5)
        return total

    def mana_dragon(self):
        self.paid_cast('source')
        self.stats('source')
        self.state()
        blocking = self.mode == 'block_paid'
        if blocking:
            self.visit(self.foe)
            self.attack(('guard',))
        else:
            self.cycle()
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
        if self.mode != 'attack_zero':
            self.fund(self.owner, (2, 0, 0, 0, 0, 0))
            self.fund(self.foe, (1, 0, 0, 0, 0, 0))
        if blocking:
            self.emit('begin_blockers')
            self.emit('block', 'source', 'guard', value=1)
            self.queue_contributions()
            self.emit('advance_combat')
            self.finish_join_forces()
            self.damage(dead=('guard',), marked=(('source',2),))
        else:
            self.queue_contributions(announce=True)
            self.emit('finish_attackers')
            bonus = self.finish_join_forces()
            if self.mode == 'trample':
                self.blockers((('guard', 'source', 1),))
                self.damage(hit=1+bonus, dead=('guard',), marked=(('source',2),))
            else:
                self.blockers((('guard', 'source', 0),))
                self.damage(hit=5+bonus)
        self.emit('cleanup')
        self.emit('refresh')
        self.stats('source')
        self.state()

    def warboss(self):
        self.paid_cast('source')
        self.stats('source')
        self.state()
        if self.mode == 'foe_turn':
            self.visit(self.foe)
            self.emit('begin_required_attackers')
            self.settle()
            self.attack(('guard',), already_begun=True)
            self.blockers()
            self.damage(hit=2)
            return
        if self.mode == 'mentor':
            self.cycle()
            self.attack(('source','partner'), target='partner')
            self.expect_counter('partner', 1)
            self.expect_counter('resting', 0)
            self.stats('partner', 1)
            self.blockers()
            self.damage(hit=4)
            return
        self.see('source', 'sick', 1)
        self.emit('begin_required_attackers')
        self.settle()
        goblin_id = self.rows[GOBLIN]['csvid']
        self.emit('bind_permanent', 'token', player=self.owner, value=goblin_id, index=-1)
        self.cards['token'] = [GOBLIN, self.owner, BATTLE]
        for prop, val in (('token',1), ('color',16), ('subtype_Goblin',1), ('power',1), ('toughness',1)):
            self.emit('assert_matching_permanents', player=self.owner, prop=prop, value=val, index=goblin_id)
        self.see('token', 'attacking', 1)
        self.see('token', 'tapped', 1)
        self.attack((), already_begun=True)
        if self.mode == 'token_blocked':
            self.blockers((('guard', 'token', 1),))
            self.emit('combat_damage')
            self.settle()
            self.cards.pop('token')
            self.see('token', 'in_play', 0)
            self.see('guard', 'damage', 1)
            self.emit('end_combat')
            self.state()
        else:
            self.blockers()
            self.damage(hit=1)
            self.visit(self.foe)
            self.see('token', 'attacking', 0)
            self.state()

    def run(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        actions = {OGRE: self.ogre, RAVINE: self.ravine, FAMILIAR: self.familiar, VAULT: self.vaultbreaker,
                   MANA_DRAGON: self.mana_dragon, WARBOSS: self.warboss}
        actions[self.id]()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (not pin or not exact(record, pin['oracle']) or csvid not in ALL_MODES
            or pin['registry']['csvid'] != csvid or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole pinned normal attack rider primary required')
    return dict(csvid=csvid, mana=list(ZERO) if csvid == RAVINE else cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in ALL_MODES[shape['csvid']] for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported attack rider variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned attack rider required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared attack rider parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses', 'engine_faces'} or set(pins['witnesses']) != WITNESSES
            or set(pins['engine_faces']) != {ANIMATED}
            or not exact(rows.get(ANIMATED), pins['engine_faces'][ANIMATED])
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != ALL_MODES.keys()):
        raise ValueError('complete attack rider source, witness, and animated land pins required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            row = rows.get(name, {})
            record = oracle.get(name)
            if not exact(record, pin['oracle']) or not exact(row, pin['registry']):
                raise ValueError('attack rider pin changed: ' + name)
            if group == 'sources':
                if row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy'):
                    raise ValueError('implemented attack rider primary required')
                classify(record, row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered attack rider primary required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    result = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board_type = FollowBoard if csvid in FOLLOW_MODES else Board
        game = board_type(rows[name], mode, int(seat), rows, oracle)
        game.run()
        result.append(dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=game.setup, steps=game.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, []))))
    return result
