"""Actual attack- and block-trigger scopes with Oracle-pinned controls."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import COLORS, cost_from_oracle

FAMILY = 'attack_trigger_team_keywords'
PROFILE = Path(__file__).with_name('data') / 'attack_trigger_team_keywords_wave8_profiles.json'

ANKLE, PEGASUS, QILIN, CAPTAIN = 14217, 13400, 18672, 11979
PREFIX, SEED = 'AttackTriggerTeamKeywordsRecipe', 3012
MODES = {
    ANKLE: ('team', 'resting', 'late', 'other_attack', 'expires'),
    PEGASUS: ('mixed', 'resting', 'late', 'other_attack', 'expires'),
    QILIN: ('companion', 'resting', 'no_other', 'late', 'expires'),
    CAPTAIN: ('attacks', 'blocks', 'other_attack', 'late', 'expires'),
}
HAND, BATTLE, LIBRARY, GRAVE = range(4)
ZERO = (0,) * 6
HUNTER_MANA = (1, 0, 0, 1, 0, 0)
CAD, HUMAN, BEAR, GIANT = 'Eager Cadet', 'Glory Seeker', 'Grizzly Bears', 'Hill Giant'
HUNTER, HAMMER = 'Trophy Hunter', 'Fall of the Hammer'
LATE_BEAR, LATE_HUMAN, FLYER = 'Ashcoat Bear', 'Containment Priest', 'Welkin Tern'
WITNESSES = {CAD, HUMAN, BEAR, GIANT, HUNTER, HAMMER, LATE_BEAR, LATE_HUMAN, FLYER}


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
        if self.id == ANKLE:
            self.ankle_setup()
        elif self.id in (PEGASUS, QILIN):
            self.flying_setup()
        else:
            self.captain_setup()

    def ankle_setup(self):
        if self.mode == 'team':
            self.add('partner', CAD, self.owner)
            self.add('main_blocker', GIANT, self.foe)
            self.add('small_blocker', GIANT, self.foe)
        elif self.mode in ('resting', 'late'):
            if self.mode == 'resting':
                self.add('dealer', CAD, self.owner)
            else:
                self.add('dealer', LATE_BEAR, self.owner, HAND)
            self.add('victim', GIANT, self.foe)
            self.add('hammer', HAMMER, self.owner, HAND)
        else:
            self.add('partner', CAD, self.owner)
            self.add('guard', GIANT, self.foe)

    def flying_setup(self):
        if self.mode == 'mixed':
            self.add('partner', HUMAN, self.owner)
            self.add('nonhuman', BEAR, self.owner)
            self.add('guard', BEAR, self.foe)
        elif self.mode == 'companion':
            self.add('partner', BEAR, self.owner)
            self.add('guard', BEAR, self.foe)
        elif self.mode == 'resting':
            self.add('partner', HUMAN if self.id == PEGASUS else BEAR, self.owner)
            self.add('hunter', HUNTER, self.foe)
        elif self.mode == 'late':
            self.add('partner', HUMAN if self.id == PEGASUS else BEAR, self.owner)
            self.add('late', LATE_HUMAN if self.id == PEGASUS else LATE_BEAR, self.owner, HAND)
            self.add('hunter', HUNTER, self.foe)
        elif self.mode == 'expires':
            self.add('partner', HUMAN if self.id == PEGASUS else BEAR, self.owner)
            self.add('guard', BEAR, self.foe)
            self.add('flyer', FLYER, self.foe)
        else:
            if self.mode == 'other_attack':
                self.add('partner', HUMAN, self.owner)
            self.add('guard', GIANT, self.foe)

    def captain_setup(self):
        self.add('partner', CAD, self.owner)
        self.add('resting_human', HUMAN, self.owner)
        self.add('nonhuman', BEAR, self.owner)
        self.add('foe_human', CAD, self.foe)
        if self.mode == 'late':
            self.add('late', LATE_HUMAN, self.owner, HAND)

    def add(self, ref, name, player, zone=BATTLE):
        assert ref not in self.cards
        self.setup.append(dict(name=name, ref=ref, player=player, zone=ZONES[zone]))
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
        self.move(ref, GRAVE if name == HAMMER else BATTLE)

    def visit(self, player):
        assert player == 1-self.active and self.pools == [ZERO, ZERO]
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)
        self.active = player

    def attack(self, refs, *, target=None):
        self.emit('begin_attackers')
        for ref in refs:
            self.emit('attack', ref, value=1)
            self.see(ref, 'attacking', 1)
            self.see(ref, 'tapped', 1)
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

    def ankle(self):
        if self.mode == 'team':
            self.attack(('source', 'partner'))
            self.blockers((('main_blocker', 'source', 1), ('small_blocker', 'partner', 1)))
            self.damage(dead=('main_blocker', 'small_blocker'), marked=(('source', 0), ('partner', 0)))
        elif self.mode in ('resting', 'late'):
            self.attack(('source',))
            if self.mode == 'late':
                self.cast('dealer')
            self.see('dealer', 'tapped', 0)
            self.see('dealer', 'attacking', 0)
            self.cast('hammer', ('dealer', 'victim'))
            if self.mode == 'resting':
                self.move('victim', GRAVE)
            else:
                self.see('victim', 'damage', 2)
            self.state()
            self.blockers()
            self.damage(hit=2)
        elif self.mode == 'other_attack':
            self.attack(('partner',))
            self.see('source', 'tapped', 0)
            self.blockers((('guard', 'partner', 1),))
            self.damage(dead=('partner',), marked=(('guard', 1),))
        else:
            self.attack(('source',))
            self.blockers()
            self.damage(hit=2)
            self.visit(self.foe)
            self.attack(('guard',))
            self.blockers((('partner', 'guard', 1),))
            self.damage(dead=('partner',), marked=(('guard', 1),))

    def check_flying(self, observed):
        # The activation target probe restores its snapshot, including this pool.
        if not any(self.pools[self.foe]):
            self.fund(self.foe, HUNTER_MANA)
        for ref, legal in observed:
            self.emit('probe_activation_target', 'hunter', ref, index=-1, value=legal)
        self.see_mana(self.foe)

    def flying(self):
        if self.mode == 'mixed':
            self.attack(('source', 'partner', 'nonhuman'))
            self.blockers((('guard', 'partner', 0), ('guard', 'nonhuman', 1)))
            self.damage(hit=3, dead=('guard', 'nonhuman'))
        elif self.mode == 'companion':
            self.attack(('source', 'partner'), target='partner')
            self.blockers((('guard', 'source', 0), ('guard', 'partner', 0)))
            self.damage(hit=4)
        elif self.mode in ('resting', 'late'):
            self.check_flying((('source', 1), ('partner', 0)))
            attackers = ('source', 'partner') if self.mode == 'late' else ('source',)
            self.attack(attackers, target='partner' if self.id == QILIN else None)
            if self.mode == 'late':
                self.cast('late')
                self.check_flying((('partner', 1), ('late', 0)))
            else:
                self.see('partner', 'tapped', 0)
                self.check_flying((('partner', int(self.id == QILIN)),))
            self.blockers()
            self.damage(hit=sum(int(self.oracle[self.cards[ref][0]]['power']) for ref in attackers))
        elif self.mode == 'expires':
            self.attack(('source', 'partner'), target='partner' if self.id == QILIN else None)
            self.blockers((('guard', 'partner', 0),))
            self.damage(hit=3 if self.id == PEGASUS else 4)
            self.visit(self.foe)
            self.attack(('flyer',))
            self.blockers((('partner', 'flyer', 0),))
            self.damage(hit=2)
        else:
            only = 'partner' if self.mode == 'other_attack' else 'source'
            self.attack((only,))
            legal = int(self.mode == 'other_attack')
            self.blockers((('guard', only, legal),))
            if legal:
                self.see('source', 'tapped', 0)
                self.damage(dead=('partner',), marked=(('guard', 2),))
            else:
                self.damage(hit=2)

    def captain_stats(self, pumped):
        self.stats('source')
        self.stats('nonhuman')
        self.stats('foe_human')
        for ref in ('partner', 'resting_human'):
            self.stats(ref, int(pumped))
        if self.mode == 'late':
            self.stats('late')

    def captain(self):
        if self.mode == 'blocks':
            self.visit(self.foe)
            self.attack(('foe_human',))
            self.blockers((('source', 'foe_human', 1),))
            self.captain_stats(True)
            self.damage(dead=('foe_human',), marked=(('source', 1),))
            return
        if self.mode == 'other_attack':
            self.attack(('partner',))
            self.captain_stats(False)
            self.see('source', 'tapped', 0)
            self.blockers((('foe_human', 'partner', 1),))
            self.damage(dead=('partner', 'foe_human'))
            return
        attackers = ('source', 'partner') if self.mode == 'attacks' else ('source',)
        self.attack(attackers)
        if self.mode == 'late':
            self.cast('late')
        self.captain_stats(True)
        if self.mode == 'attacks':
            self.blockers((('foe_human', 'partner', 1),))
            self.damage(hit=2, dead=('foe_human',), marked=(('partner', 1),))
        else:
            self.blockers()
            self.damage(hit=2)
        if self.mode == 'expires':
            self.visit(self.foe)
            self.captain_stats(False)
            self.state()

    def run(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        self.cast('source')
        self.stats('source')
        self.state()
        if self.id == ANKLE:
            self.ankle()
        elif self.id in (PEGASUS, QILIN):
            self.visit(self.foe)
            self.visit(self.owner)
            self.flying()
        else:
            if self.mode != 'blocks':
                self.visit(self.foe)
                self.visit(self.owner)
            self.captain()

DRAKE, CONDOR, WOLF, RAGER, IBEX, ELK = 11791, 13353, 12243, 14860, 15534, 15036
UMBRA, ARMAMENTS, BLADES, BEASTMASTER, OUTRIDER = 10877, 9454, 16331, 12872, 14996
PIKER, SPINED, SCALED, BALOTH = 'Goblin Piker', 'Spined Wurm', 'Scaled Wurm', 'Rumbling Baloth'
FOREST, GROWTH, MURDER, SHOCK, SERVO = 'Forest', 'Giant Growth', 'Murder', 'Shock', 'Servo'
WITNESSES.update((PIKER, SPINED, SCALED, BALOTH, FOREST, GROWTH, MURDER, SHOCK, SERVO))
FOLLOW_MODES = {
    DRAKE: ('companion', 'resting', 'self_flight', 'expires'),
    CONDOR: ('companion', 'resting', 'no_other', 'expires'),
    WOLF: ('single', 'pair', 'expires', 'undying'),
    RAGER: ('warrior_only', 'pair', 'expires'),
    IBEX: ('highest_idle', 'excludes_enemy', 'late'),
    ELK: ('below', 'exactly', 'late_threshold'),
    UMBRA: ('own_host', 'other_attacks', 'foreign_host', 'armor'),
    ARMAMENTS: ('equip_attack', 'equip_block', 'soldier_auto', 'soldier_decline', 'nonsoldier'),
    BLADES: ('counter_buff', 'servo_entry', 'decline'),
    BEASTMASTER: ('base', 'pre_pump', 'late'),
    OUTRIDER: ('exactly', 'below', 'late_threshold'),
}
HUMAN_ONLY = {'soldier_decline', 'decline'}
ALL_MODES = MODES | FOLLOW_MODES
ZONES = ('hand', 'battlefield', 'library', 'graveyard')
ZONE_OPS = {'assert_zone', 'assert_zone_multiplicity'}
INDEXED = {'assert_zone_multiplicity', 'probe_activation_target', 'assert_matching_permanents', 'bind_permanent_copy', 'bind_permanent'}
SEATED = {'turn', 'untap_step', 'choose_target', 'choose_dialog', 'choose_trigger_source',
          'set_mana', 'assert_mana', 'assert_life', 'bind_permanent_copy', 'bind_permanent',
          'assert_matching_permanents'} | ZONE_OPS
VALUES = {'cast', 'activate', 'can_activate', 'choose_dialog', 'present_ai_dialogs', 'set_mana',
          'assert_mana', 'assert_life', 'assert_stack', 'assert_unused_choices', 'assert_card',
          'probe_activation_target', 'attack', 'block', 'assert_counter', 'bind_permanent_copy',
          'bind_permanent', 'assert_matching_permanents'} | ZONE_OPS


class FollowBoard(Board):
    def setup_witnesses(self):
        if self.id in (DRAKE, CONDOR):
            self.flying_setup()
        elif self.id in (WOLF, RAGER):
            if self.mode == 'undying':
                self.add('kill_a', MURDER, self.foe, HAND)
                self.add('kill_b', MURDER, self.foe, HAND)
            else:
                self.add('partner', PIKER if self.id == RAGER else HUMAN, self.owner)
                self.add('guard', GIANT, self.foe)
                if self.mode == 'pair':
                    self.add('second_guard', GIANT, self.foe)
                if self.mode == 'warrior_only':
                    self.add('nonwarrior', CAD, self.owner)
        elif self.id in (IBEX, ELK, OUTRIDER):
            partner = CAD if self.id == IBEX else GIANT if self.id == ELK and self.mode == 'exactly' else BEAR
            if self.id != OUTRIDER:
                self.add('partner', partner, self.owner)
            self.add('guard', CAD if self.id == ELK else BEAR, self.foe)
            if self.id == IBEX:
                if self.mode in ('highest_idle', 'late'):
                    self.add('greatest', SPINED, self.owner)
                else:
                    self.add('enemy_greatest', SCALED, self.foe)
                if self.mode == 'late':
                    self.add('late', LATE_BEAR, self.owner, HAND)
            elif self.id == OUTRIDER:
                self.add('resting', BALOTH if self.mode == 'exactly' else GIANT, self.owner)
            if self.mode == 'late_threshold':
                self.add('late', LATE_BEAR, self.owner, HAND)
        elif self.id in (BLADES, BEASTMASTER):
            self.add('partner', BEAR, self.owner)
            self.add('resting', CAD, self.owner)
            self.add('enemy', GIANT, self.foe)
            if self.mode == 'pre_pump':
                self.add('pump', GROWTH, self.owner, HAND)
            if self.mode == 'late':
                self.add('late', LATE_BEAR, self.owner, HAND)
        elif self.id == UMBRA:
            self.add('host', BEAR, self.foe if self.mode == 'foreign_host' else self.owner)
            if self.mode == 'armor':
                self.add('shock', SHOCK, self.foe, HAND)
                self.add('kill', MURDER, self.foe, HAND)
            else:
                self.add('own_land', FOREST, self.owner)
                self.add('foe_land', FOREST, self.foe)
                self.add('own_spend', GROWTH, self.owner, HAND)
                self.add('foe_spend', GROWTH, self.foe, HAND)
                self.add('idle', GIANT, self.owner)
                self.add('peer', CAD, self.owner)
        elif self.id == ARMAMENTS:
            self.add('host', BEAR, self.owner)
            self.add('partner', CAD, self.owner)
            if self.mode == 'equip_block':
                self.add('guard', BEAR, self.foe)
                self.add('enemy_peer', CAD, self.foe)
            elif self.mode.startswith('soldier') or self.mode == 'nonsoldier':
                self.add('newcomer', BEAR if self.mode == 'nonsoldier' else HUMAN, self.owner, HAND)

    def cast(self, ref, targets=(), *, already_funded=False, resolve_choices=(), optional_source=None):
        name, player, zone = self.cards[ref]
        assert zone == HAND
        cost = tuple(cost_from_oracle(self.oracle[name]))
        if already_funded:
            assert self.pools[player] == cost
        else:
            self.fund(player, cost)
        for target in targets:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.pools[player] = ZERO
        self.see_mana(player)
        if optional_source:
            self.emit('choose_trigger_source', optional_source, player=player)
        for choice in resolve_choices:
            self.emit('choose_dialog', player=player, value=choice)
        self.settle()
        destination = GRAVE if self.oracle[name]['type_line'] == 'Instant' else BATTLE
        self.move(ref, destination)

    def power(self, ref):
        return int(self.oracle[self.cards[ref][0]]['power'])

    def turn_cycle(self):
        self.visit(self.foe)
        self.visit(self.owner)

    def following_flying(self):
        if self.mode == 'resting':
            self.check_flying((('source', 1), ('partner', 0)))
            self.attack(('source',), target='partner')
            self.see('partner', 'tapped', 0)
            self.check_flying((('partner', 1),))
            self.blockers()
            self.damage(hit=self.power('source'))
            return
        if self.mode == 'self_flight':
            self.attack(('source',), target='source')
            self.blockers((('guard', 'source', 0),))
            self.damage(hit=self.power('source'))
            return
        if self.mode == 'no_other':
            self.attack(('source',))
            self.blockers((('guard', 'source', 0),))
            self.damage(hit=self.power('source'))
            return
        attackers = ('source', 'partner')
        self.attack(attackers, target='partner')
        self.blockers((('guard', 'partner', 0),))
        self.damage(hit=sum(self.power(ref) for ref in attackers))
        if self.mode == 'expires':
            self.visit(self.foe)
            self.attack(('flyer',))
            self.blockers((('partner', 'flyer', 0),))
            self.damage(hit=2)

    def menace(self):
        if self.mode == 'undying':
            self.cast('kill_a', ('source',))
            self.move('source', -1)
            token = self.rows[self.name]['csvid']
            self.emit('bind_permanent', 'returned', player=self.owner, value=token, index=-1)
            self.cards['returned'] = [self.name, self.owner, BATTLE]
            self.emit('assert_counter', 'returned', prop='plus1', value=1)
            self.stats('returned', 1)
            self.cast('kill_b', ('returned',))
            self.move('returned', GRAVE)
            self.state()
            return
        attackers = ('source', 'partner', 'nonwarrior') if self.mode == 'warrior_only' else ('source', 'partner')
        self.attack(attackers)
        if self.mode == 'pair':
            self.blockers((('guard', 'partner', 1), ('second_guard', 'partner', 1)))
            # A separate single-block case takes damage. Here the declaration itself
            # is decisive without binding a combat-damage ordering choice.
            self.see('guard', 'blocking_count', 1)
            self.see('second_guard', 'blocking_count', 1)
            return
        blocks = [('guard', 'source', 0), ('guard', 'partner', 0)]
        if self.mode == 'warrior_only':
            blocks.append(('guard', 'nonwarrior', 1))
        self.blockers(blocks)
        hit = sum(self.power(r) for r in attackers if r != 'nonwarrior')
        if self.mode == 'warrior_only':
            self.damage(hit=hit, dead=('nonwarrior',), marked=(('guard', 1),))
        else:
            self.damage(hit=hit)
        if self.mode == 'expires':
            self.turn_cycle()
            self.attack(('partner',))
            self.blockers((('guard', 'partner', 1),))
            self.damage(dead=('partner',), marked=(('guard', self.power('partner')),))

    def ibex(self):
        self.attack(('source', 'partner'))
        bonus = 3 if self.mode == 'excludes_enemy' else 5
        for ref in ('source', 'partner'):
            self.stats(ref, bonus)
        if 'greatest' in self.cards:
            self.stats('greatest', bonus)
        if 'enemy_greatest' in self.cards:
            self.stats('enemy_greatest')
        if self.mode == 'late':
            self.cast('late')
            self.stats('late')
        self.blockers((('guard', 'partner', 1),))
        self.damage(hit=2*bonus+2, dead=('guard',), marked=(('partner', 2),))
        self.visit(self.foe)
        self.stats('source')
        self.stats('partner')
        self.state()

    def formidable(self):
        attacked = ('source', 'partner') if self.id == ELK else ('source',)
        self.attack(attacked)
        if self.mode == 'late_threshold':
            self.cast('late')
        success = self.mode == 'exactly'
        ref = 'partner' if self.id == ELK else 'source'
        self.blockers((('guard', ref, 1),))
        if self.id == ELK:
            self.damage(hit=7 if success else 5, dead=('guard',), marked=(('partner', 1),))
        else:
            self.damage(hit=2, dead=('guard',) if success else ('guard', 'source'),
                        marked=(('source', 0),) if success else ())

    def grow_attackers(self, *, fabricate=False):
        discarded = fabricate and self.mode == 'decline'
        if self.mode == 'pre_pump':
            self.cast('pump', ('source',))
        picked = fabricate and self.owner == 0 and not discarded
        attackers = ('source', 'partner')
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref, value=1)
        if picked:
            self.emit('choose_trigger_source', 'source', player=self.owner)
        self.emit('finish_attackers')
        self.settle()
        if fabricate:
            source_bonus = 0 if self.mode == 'servo_entry' else 2
            self.stats('source', source_bonus)
            bonus = 0 if discarded else 1+source_bonus
        else:
            source_bonus = 3 if self.mode == 'pre_pump' else 0
            self.stats('source', source_bonus)
            bonus = 1+source_bonus
        self.stats('partner', bonus)
        self.stats('resting', 0 if fabricate else bonus)
        if fabricate and self.mode == 'servo_entry':
            for index in range(2):
                self.stats(f'servo_{index}')
        self.stats('enemy')
        if self.mode == 'late':
            self.cast('late')
            self.stats('late')
        self.blockers()
        self.damage(hit=3+source_bonus+bonus)
        self.visit(self.foe)
        self.stats('partner')
        self.stats('source', source_bonus if fabricate else 0)
        self.state()

    def umbra(self):
        self.stats('host', 2)
        if self.mode == 'armor':
            self.cast('shock', ('host',))
            self.see('host', 'damage', 2)
            self.cast('kill', ('host',))
            self.move('source', GRAVE)
            self.stats('host')
            self.see('host', 'damage', 0)
            self.state()
            return
        if self.mode == 'foreign_host':
            self.visit(self.foe)
        for label, seat in (('own', self.owner), ('foe', self.foe)):
            land = f'{label}_land'
            self.emit('activate', land, value=1)
            self.emit('assert_stack')
            self.pools[seat] = (0, 0, 0, 1, 0, 0)
            self.see_mana(seat)
            self.see(land, 'tapped', 1)
            self.cast(f'{label}_spend', ('idle',), already_funded=True)
        ref = 'peer' if self.mode == 'other_attacks' else 'host'
        self.attack((ref,))
        self.see('own_land', 'tapped', int(self.mode != 'own_host'))
        self.see('foe_land', 'tapped', int(self.mode != 'foreign_host'))
        self.blockers()
        self.damage(hit=1 if ref == 'peer' else 4)

    def equip(self, target):
        self.fund(self.owner, (2, 0, 0, 0, 0, 0))
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_target', target, player=self.owner)
        self.emit('activate', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.pools[self.owner] = ZERO
        self.see_mana(self.owner)
        self.settle()

    def armaments(self):
        if self.mode in ('equip_attack', 'equip_block'):
            self.equip('host')
        if self.mode == 'equip_block':
            self.visit(self.foe)
            self.attack(('guard', 'enemy_peer'))
            self.blockers((('host', 'guard', 1),))
            self.stats('host', 2)
            self.damage(hit=1, dead=('guard',), marked=(('host', 2),))
            return
        if self.mode.startswith('soldier') or self.mode == 'nonsoldier':
            offer = self.mode == 'soldier_auto'
            self.cast('newcomer', optional_source='source' if self.owner == 0 and offer else None)
            self.turn_cycle()
            self.attack(('newcomer', 'partner'))
            self.stats('newcomer', 2 if offer else 0)
            self.stats('partner')
            self.blockers()
            self.damage(hit=5 if offer else 3)
        else:
            self.attack(('host', 'partner'))
            self.stats('host', 2)
            self.stats('partner')
            self.blockers()
            self.damage(hit=5)
        self.visit(self.foe)
        self.stats('newcomer' if 'newcomer' in self.cards else 'host')
        self.state()

    def run(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        if self.id == BLADES:
            self.emit('present_ai_dialogs', value=1)
            self.cast('source', resolve_choices=(int(self.mode == 'servo_entry'),))
            if self.mode == 'servo_entry':
                token_id = self.rows[SERVO]['csvid']
                for idx in range(2):
                    ref = f'servo_{idx}'
                    self.emit('bind_permanent_copy', ref, player=self.owner, value=token_id, index=idx)
                    self.cards[ref] = [SERVO, self.owner, BATTLE]
                for prop, val in (('token', 1), ('artifact', 1), ('color', 0), ('power', 1), ('toughness', 1)):
                    self.emit('assert_matching_permanents', player=self.owner, index=token_id, prop=prop, value=val)
            self.emit('assert_counter', 'source', prop='plus1', value=0 if self.mode == 'servo_entry' else 2)
        else:
            self.cast('source', ('host',) if self.id == UMBRA else ())
        self.state()
        if self.id in {DRAKE, CONDOR, WOLF, RAGER, IBEX, ELK, OUTRIDER, BLADES, BEASTMASTER} and self.mode != 'undying':
            self.turn_cycle()
        if self.id in (DRAKE, CONDOR):
            self.following_flying()
        elif self.id in (WOLF, RAGER):
            self.menace()
        elif self.id == IBEX:
            self.ibex()
        elif self.id in (ELK, OUTRIDER):
            self.formidable()
        elif self.id in (BLADES, BEASTMASTER):
            self.grow_attackers(fabricate=self.id == BLADES)
        elif self.id == UMBRA:
            self.umbra()
        else:
            self.armaments()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (not pin or not exact(record, pin['oracle']) or csvid not in ALL_MODES
            or pin['registry']['csvid'] != csvid or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole pinned normal team attack source required')
    return dict(csvid=csvid, mana=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in ALL_MODES[shape['csvid']]
            for seat in ((0,) if mode in HUMAN_ONLY else (0, 1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported team attack variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned team attack source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared team attack parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != ALL_MODES.keys()):
        raise ValueError('complete team attack source and witness pins required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            record = oracle.get(name)
            if not exact(record, pin['oracle']) or not exact(row, pin['registry']):
                raise ValueError('team attack pin changed: ' + name)
            if group == 'sources':
                if row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy'):
                    raise ValueError('implemented team attack source required')
                classify(record, row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered team attack source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    results = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board_type = FollowBoard if csvid in FOLLOW_MODES else Board
        board = board_type(rows[name], mode, int(seat), rows, oracle)
        board.run()
        results.append(dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                             setup=board.setup, steps=board.steps,
                             _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                          oracle_text=declaration['oracle_text'])))
    return results
