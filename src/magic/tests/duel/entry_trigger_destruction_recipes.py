"""Paid entry effects with actual destruction and sacrifice receipts."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from mill_death_and_entry_recipes import Duel as PaidBoard, stats
from recipes import cost_from_oracle


FAMILY = 'entry_trigger_destruction'
PROFILE = Path(__file__).with_name('data') / 'entry_trigger_destruction_wave8_profiles.json'
PREFIX = 'EntryTriggerDestructionRecipe_'
STARBREAKER, BABOONS, SUNBLAST, HOOLIGAN = 13802, 3541, 11373, 8149
PRINTED = {
    STARBREAKER: ('born_of_the_gods.c', '{3}{G}', 'Creature — Human Warrior', (2, 1), (),
                  'When this creature enters, you may destroy target Aura.'),
    BABOONS: ('exodus.c', '{3}{R}', 'Creature — Monkey', (2, 2), (),
              'When this creature enters, destroy target nonbasic land.'),
    SUNBLAST: ('scars_of_mirrodin.c', '{4}{W}{W}', 'Creature — Angel', (4, 5), ('Flying',),
               'Flying\nWhen this creature enters, destroy all tapped creatures.'),
    HOOLIGAN: ('guildpact.c', '{1}{R}', 'Creature — Goblin Rogue', (2, 1), (),
               'When this creature enters, if {G} was spent to cast it, destroy target artifact.'),
}
MODES = {
    STARBREAKER: ('own_aura', 'foe_aura', 'foreign_host', 'decline', 'non_aura', 'skip_trigger'),
    BABOONS: ('own_nonbasic', 'foe_nonbasic', 'typed_nonbasic', 'basic_only', 'indestructible'),
    SUNBLAST: ('both_controllers', 'untapped', 'indestructible', 'self_tapped', 'flying'),
    HOOLIGAN: ('green_foe', 'green_own', 'no_green', 'green_no_artifact', 'indestructible', 'short_red', 'retained_green'),
}
HUMAN_ONLY = {(STARBREAKER, 'skip_trigger'), (HOOLIGAN, 'retained_green')}
AURA, ENCHANTMENT, HOST = 'Holy Strength', 'Fervor', 'Prodigal Sorcerer'
DUAL, CITADEL, MANLAND = 'Tropical Island', 'Darksteel Citadel', 'Terramorphic Expanse'
ICY, MYR, PRAETOR = 'Icy Manipulator', 'Darksteel Myr', 'Urabrask the Hidden'
GROUND, AIR, ARTIFACT, INGOT = 'Dazzling Ramparts', 'Drift of Phantasms', 'Millstone', 'Darksteel Ingot'
ZERO = (0,) * 6


def label(csvid, mode, actor):
    return f'{PREFIX}{csvid}_{mode}_{actor}'


class Duel(PaidBoard):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.active = actor
        self.library = {0: [], 1: []}
        self.add('source', actor, record['name'], 'hand')
        self.tapped = []
        self.removed = []
        self.payment = None

    def cast_to(self, ref, zone, target=None):
        self.paid(ref, target=target)
        self.settle()
        self.cards[ref]['zone'] = zone

    def source_stats(self):
        power, toughness = stats(self.record)
        self.observe('source', 'in_play', 1)
        self.observe('source', 'power', power)
        self.observe('source', 'toughness', toughness)

    def grave(self, refs):
        for ref in refs:
            self.cards[ref]['zone'] = 'graveyard'
            self.removed.append(ref)
            self.observe(ref, 'in_play', 0)

    def enter(self):
        self.settle()
        self.cards['source']['zone'] = 'battlefield'

    def starbreaker(self):
        has_auras = self.mode != 'non_aura'
        self.add('own_host', self.actor, HOST, 'battlefield')
        self.add('foe_host', self.foe, HOST, 'battlefield')
        self.add('wrong_type', self.foe, ENCHANTMENT, 'battlefield')
        foreign_host = 'own_host' if self.mode == 'foreign_host' else 'foe_host'
        if has_auras:
            self.add('own_aura', self.actor, AURA, 'hand')
            self.add('foe_aura', self.foe, AURA, 'hand')
            self.cast_to('own_aura', 'battlefield', 'own_host')
            self.go(self.foe)
            self.cast_to('foe_aura', 'battlefield', foreign_host)
            self.go(self.actor)
        for ref, bonuses in (('own_host', int(has_auras) + int(self.mode == 'foreign_host')),
                             ('foe_host', int(has_auras and foreign_host == 'foe_host'))):
            self.observe(ref, 'power', 1 + bonuses)
            self.observe(ref, 'toughness', 1 + 2 * bonuses)
        self.paid('source')
        if self.actor == 0 and self.mode != 'skip_trigger':
            self.emit('choose_trigger', 'source', player=self.actor)
        target = 'own_aura' if self.mode == 'own_aura' else 'foe_aura'
        if self.mode == 'decline':
            self.emit('choose_optional_target', target, player=self.actor)
        elif self.mode not in ('non_aura', 'skip_trigger'):
            self.emit('choose_target', target, player=self.actor)
        self.enter()
        if self.mode not in ('decline', 'skip_trigger', 'non_aura'):
            self.grave((target,))
        self.observe('wrong_type', 'in_play', 1)
        for ref in ('own_host', 'foe_host'):
            attached = sum(self.cards.get(aura, {}).get('zone') == 'battlefield' and host == ref
                           for aura, host in (('own_aura', 'own_host'), ('foe_aura', foreign_host)))
            self.observe(ref, 'power', 1 + attached)
            self.observe(ref, 'toughness', 1 + 2 * attached)
        self.source_stats()

    def baboons(self):
        self.add('own_basic', self.actor, 'Island', 'battlefield')
        self.add('foe_basic', self.foe, 'Mountain', 'battlefield')
        self.add('wrong_type', self.foe, HOST, 'battlefield')
        if self.mode != 'basic_only':
            kind = {'typed_nonbasic': DUAL, 'indestructible': CITADEL}.get(self.mode, MANLAND)
            who = self.actor if self.mode == 'own_nonbasic' else self.foe
            self.add('target_land', who, kind, 'battlefield')
        self.paid('source')
        if self.mode != 'basic_only':
            self.observe('target_land', 'in_play', 1)
            self.emit('choose_target', 'target_land', player=self.actor)
        self.enter()
        if self.mode not in ('basic_only', 'indestructible'):
            self.grave(('target_land',))
        elif self.mode == 'indestructible':
            self.observe('target_land', 'in_play', 1)
        for ref in ('own_basic', 'foe_basic', 'wrong_type'):
            self.observe(ref, 'in_play', 1)
        self.source_stats()

    def pay_tap(self, tapper, target):
        self.mana('set_mana', self.actor, (1, 0, 0, 0, 0, 0))
        self.emit('refresh')
        self.emit('choose_target', target, player=self.actor)
        self.emit('activate', tapper, value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.settle()
        self.observe(tapper, 'tapped', 1)
        self.observe(target, 'tapped', 1)
        self.tapped.append(target)

    def sunblast(self):
        if self.mode == 'flying':
            self.add('ground', self.foe, GROUND, 'battlefield')
            self.add('flyer', self.foe, AIR, 'battlefield')
        elif self.mode == 'self_tapped':
            self.add('praetor', self.foe, PRAETOR, 'battlefield')
        else:
            self.add('own_target', self.actor, HOST, 'battlefield')
            self.add('foe_target', self.foe, MYR if self.mode == 'indestructible' else HOST, 'battlefield')
            self.add('own_untapped', self.actor, GROUND, 'battlefield')
            self.add('foe_untapped', self.foe, GROUND, 'battlefield')
            if self.mode in ('both_controllers', 'indestructible'):
                for index, target in enumerate(('own_target', 'foe_target')):
                    tapper = f'tapper_{index}'
                    self.add(tapper, self.actor, ICY, 'battlefield')
                    self.pay_tap(tapper, target)
        self.paid('source')
        for target in self.tapped:
            self.observe(target, 'in_play', 1)
            self.observe(target, 'tapped', 1)
        self.enter()
        if self.mode == 'self_tapped':
            self.grave(('source',))
            self.observe('praetor', 'in_play', 1)
        else:
            self.source_stats()
            self.observe('source', 'tapped', 0)
        if self.mode in ('both_controllers', 'indestructible'):
            self.grave(('own_target', 'foe_target') if self.mode == 'both_controllers' else ('own_target',))
            if self.mode == 'indestructible':
                self.observe('foe_target', 'in_play', 1)
                self.observe('foe_target', 'tapped', 1)
            for ref in ('tapper_0', 'tapper_1'):
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', 1)
        if self.mode in ('untapped', 'both_controllers', 'indestructible'):
            for ref in ('own_untapped', 'foe_untapped'):
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', 0)
        if self.mode == 'flying':
            self.go(self.foe)
            self.go(self.actor)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            self.emit('block', 'ground', 'source', value=0)
            self.emit('block', 'flyer', 'source', value=1)
            self.emit('advance_combat')
            self.observe('source', 'blocked', 1)
            self.emit('combat_damage')
            self.settle()
            self.observe('flyer', 'damage', 4)
            self.emit('end_combat')
            self.settle()

    def hooligan(self):
        uses_green = self.mode not in ('no_green', 'short_red', 'retained_green')
        has_target = self.mode != 'green_no_artifact'
        if has_target:
            who = self.actor if self.mode == 'green_own' else self.foe
            self.add('artifact', who, INGOT if self.mode == 'indestructible' else ARTIFACT, 'battlefield')
        self.add('wrong_type', self.foe, HOST, 'battlefield')
        pool = {
            'short_red': (1, 0, 0, 1, 0, 0),
            'no_green': (1, 0, 0, 0, 1, 0),
            'retained_green': (1, 0, 0, 1, 1, 0),
        }.get(self.mode, (0, 0, 0, 1, 1, 0))
        self.payment = dict(opening=pool, green_spent=uses_green)
        self.mana('set_mana', self.actor, pool)
        self.emit('refresh')
        if self.mode == 'short_red':
            self.emit('cast', 'source', value=0)
            self.observe('source', 'in_hand', 1)
            self.mana('assert_mana', self.actor, pool)
            return
        if self.actor == 0 and (uses_green or self.mode == 'retained_green'):
            self.emit('choose_dialog', player=self.actor, value=int(uses_green))
        self.emit('cast', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        residual = (0, 0, 0, 1, 0, 0) if self.mode == 'retained_green' else ZERO
        self.mana('assert_mana', self.actor, residual)
        if has_target:
            self.observe('artifact', 'in_play', 1)
        if uses_green and has_target:
            self.emit('choose_target', 'artifact', player=self.actor)
        self.enter()
        if uses_green and has_target and self.mode != 'indestructible':
            self.grave(('artifact',))
        elif has_target:
            self.observe('artifact', 'in_play', 1)
        self.source_stats()
        self.observe('wrong_type', 'in_play', 1)
        self.mana('assert_mana', self.actor, residual)

    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        handler = {STARBREAKER: self.starbreaker, BABOONS: self.baboons,
                   SUNBLAST: self.sunblast, HOOLIGAN: self.hooligan}
        handler[self.csvid]()
        self.receipts()


PRINTED.update({13289: ('m14.c',
         '{5}{R}{R}',
         'Creature — Giant',
         (4, 4),
         (),
         'When this creature enters, you may destroy target creature with toughness 2 or less.'),
 8018: ('guildpact.c',
        '{3}{W}',
        'Creature — Thrull Cleric',
        (2, 3),
        ('Haunt',),
        'Haunt (When this creature dies, exile it haunting target creature.)\n'
        'When this creature enters or the creature it haunts dies, destroy target enchantment.'),
 3604: ('portal_1_2_3k.c',
        '{3}{B}{B}',
        'Creature — Nightstalker',
        (3, 2),
        (),
        'When this creature enters, you may have target opponent sacrifice a creature of their choice.'),
 4529: ('mercadian_masques.c',
        '{2}{G}',
        'Creature — Beast',
        (4, 4),
        ('Trample',),
        'Trample\nWhen this creature enters, sacrifice it unless you pay {1} for each card in your hand.'),
 4285: ('portal_1_2_3k.c',
        '{3}{B}{B}',
        'Creature — Human Soldier Assassin',
        (3, 2),
        (),
        'When this creature enters, target opponent chooses a creature they control. Destroy that '
        'creature.'),
 13756: ('born_of_the_gods.c',
         '{3}{G}{G}',
         'Creature — Beast',
         (3, 3),
         ('Tribute',),
         'Tribute 3 (As this creature enters, an opponent of your choice may put three +1/+1 counters on '
         'it.)\n'
         "When this creature enters, if tribute wasn't paid, destroy target noncreature permanent."),
 1008: ('nemesis.c',
        '{2}{B}{B}',
        'Enchantment',
        None,
        (),
        'When this enchantment enters, sacrifice all creatures you control.\n'
        'Creatures you control get +2/+2.'),
 2987: ('weatherlight.c',
        '{1}{G}',
        'Creature — Wurm',
        (3, 2),
        (),
        'When this creature enters, sacrifice it unless you return a basic land card from your graveyard to '
        'your hand.'),
 15609: ('oath_of_the_gatewatch.c',
         '{3}{G}',
         'Creature — Troll',
         (2, 3),
         (),
         'When this creature enters, you may sacrifice a creature or land. If you do, put two +1/+1 '
         'counters on this creature.'),
 2735: ('visions.c',
        '{2}{U}',
        'Creature — Human Knight',
        (2, 2),
        ('Flanking',),
        'Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets '
        '-1/-1 until end of turn.)\n'
        "When this creature enters, you may pay {U}. If you don't, destroy target Knight and it can't be "
        'regenerated.'),
 6968: ('darksteel.c',
        '{8}',
        'Artifact Creature — Golem',
        (7, 10),
        (),
        'When this creature enters or leaves the battlefield, choose a land of each basic land type, then '
        'destroy those lands.'),
 2558: ('mirage.c',
        '{1}',
        'Artifact Creature — Phyrexian Dreadnought',
        (12, 12),
        ('Trample',),
        'Trample\n'
        'When this creature enters, sacrifice it unless you sacrifice any number of creatures with total '
        'power 12 or greater.')})

GIANT, THRULL, NIGHTSTALKER, MEGA, ASSASSINS, DEMOLOK = 13289, 8018, 3604, 4529, 4285, 13756
OFFERING, WURM, TROLL, KNIGHT, TITAN, DREADNOUGHT = 1008, 2987, 15609, 2735, 6968, 2558
FOLLOWERS = {
    GIANT: ('foe_boundary', 'own_creature', 'decline', 'too_tough', 'indestructible'),
    THRULL: ('own_enchantment', 'foe_enchantment', 'haunt_death', 'haunt_bounce', 'no_enchantment'),
    NIGHTSTALKER: ('chosen', 'indestructible', 'no_foe', 'decline'),
    ASSASSINS: ('chosen', 'indestructible', 'no_foe'),
    MEGA: ('zero_hand', 'two_paid', 'one_short', 'trample'),
    DEMOLOK: ('paid', 'foe_artifact', 'own_land', 'creatures_only'),
    OFFERING: ('own_population', 'empty_own', 'reload', 'anthem_removed'),
    WURM: ('return_basic', 'no_lands', 'nonbasic_only', 'decline'),
    TROLL: ('sac_creature', 'sac_land', 'indestructible', 'decline', 'self'),
    KNIGHT: ('pay', 'suppress_regeneration', 'self_only', 'flanking'),
    TITAN: ('all_five', 'typed_dual', 'no_types', 'leaves_bounce', 'leaves_death'),
    DREADNOUGHT: ('exact_twelve', 'below_twelve', 'no_creatures', 'trample'),
}
HUMAN_ONLY |= {(NIGHTSTALKER, 'decline'), (TROLL, 'decline'), (TROLL, 'self')}
TINKERER, GNOMES = 'Goblin Tinkerer', 'Bottle Gnomes'
KILL, BOUNCE, BREAK = 'Murder', 'Unsummon', 'Disenchant'
REGEN_KNIGHT = 'Knight of the Holy Nimbus'
THORN, DRAGON, CHAMELEON = 'Thorn Elemental', 'Shivan Dragon', 'Kavu Chameleon'
SQUEE, WITHSTAND = 'Squee, Goblin Nabob', 'Withstand Death'
LANDS = ('Swamp', 'Island', 'Forest', 'Mountain', 'Plains')
WITNESSES = {AURA, ENCHANTMENT, HOST, DUAL, CITADEL, MANLAND, ICY, MYR, PRAETOR, GROUND, AIR, ARTIFACT, INGOT,
             TINKERER, GNOMES, KILL, BOUNCE, BREAK, REGEN_KNIGHT, THORN, DRAGON, CHAMELEON, SQUEE, WITHSTAND, *LANDS}
GAPS = {
    NIGHTSTALKER: ['choosing among multiple eligible creatures when forced sacrifice uses the engine’s AI picker'],
    DEMOLOK: ['opponent selection among multiple seats in a multiplayer game'],
    MEGA: ['affirmative refusal of an affordable fee when the engine’s automatic mana payment applies'],
    KNIGHT: ['affirmative refusal of an affordable fee when the engine’s automatic mana payment applies'],
}


class FollowDuel(Duel):
    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        handler = {GIANT: self.giant, THRULL: self.thrull, NIGHTSTALKER: self.edict, ASSASSINS: self.edict,
                   MEGA: self.megatherium, DEMOLOK: self.demolok, OFFERING: self.offering, WURM: self.wurm,
                   TROLL: self.troll, KNIGHT: self.knight, TITAN: self.titan, DREADNOUGHT: self.dreadnought}
        handler[self.csvid]()
        self.receipts()

    def action_target(self, who, ref, optional=False):
        self.emit('choose_optional_target' if optional else 'choose_target', ref, player=who)

    def optional_entry(self):
        if self.actor == 0:
            self.emit('choose_trigger', 'source', player=self.actor)

    def cast_surplus(self, extra):
        paid = cost_from_oracle(self.record)
        available = [a + b for a, b in zip(paid, extra)]
        self.mana('set_mana', self.actor, available)
        self.emit('refresh')
        self.emit('cast', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_hand', 0)
        self.mana('assert_mana', self.actor, extra)

    def end_entry(self, grave_refs=()):
        self.enter()
        self.grave(grave_refs)
        if 'source' not in grave_refs:
            self.source_stats()

    def giant(self):
        self.add('large', self.foe, GROUND, 'battlefield')
        if self.mode != 'too_tough':
            name = {'own_creature': HOST, 'indestructible': MYR}.get(self.mode, TINKERER)
            who = self.actor if self.mode == 'own_creature' else self.foe
            self.add('small', who, name, 'battlefield')
        self.paid('source')
        self.optional_entry()
        if self.mode != 'too_tough':
            self.action_target(self.actor, 'small', optional=self.mode == 'decline')
        destroyed = ('small',) if self.mode in ('foe_boundary', 'own_creature') else ()
        self.end_entry(destroyed)

    def thrull(self):
        haunted = self.mode in ('haunt_death', 'haunt_bounce')
        if self.mode != 'no_enchantment':
            who = self.actor if self.mode == 'own_enchantment' else self.foe
            self.add('entry_victim', who, ENCHANTMENT, 'battlefield')
        if haunted:
            self.add('kill_source', self.actor, KILL, 'hand')
            self.add('exit_carrier', self.actor, KILL if self.mode == 'haunt_death' else BOUNCE, 'hand')
            self.add('carrier', self.foe, HOST, 'battlefield')
            self.add('haunt_victim', self.actor, ENCHANTMENT, 'battlefield')
        self.paid('source')
        if self.mode != 'no_enchantment':
            self.action_target(self.actor, 'entry_victim')
        self.end_entry(() if self.mode == 'no_enchantment' else ('entry_victim',))
        if not haunted:
            return
        self.paid('kill_source', target='source')
        self.action_target(self.actor, 'carrier')
        self.settle()
        self.cards['kill_source']['zone'] = 'graveyard'
        self.cards['source']['zone'] = 'exile'
        self.observe('source', 'in_play', 0)
        self.receipts()
        self.paid('exit_carrier', target='carrier')
        died = self.mode == 'haunt_death'
        if died:
            self.action_target(self.actor, 'haunt_victim')
        self.settle()
        self.cards['exit_carrier']['zone'] = 'graveyard'
        if died:
            self.grave(('carrier', 'haunt_victim'))
        else:
            self.cards['carrier']['zone'] = 'hand'
            self.observe('haunt_victim', 'in_play', 1)

    def edict(self):
        self.add('own_creature', self.actor, HOST, 'battlefield')
        has_victim = self.mode != 'no_foe'
        if has_victim:
            self.add('victim', self.foe, MYR if self.mode == 'indestructible' else HOST, 'battlefield')
        # Human defender chooses among two; AI forced-sacrifice uses its real sole eligible creature.
        if self.mode == 'chosen' and (self.foe == 0 or self.csvid == ASSASSINS):
            self.add('spare', self.foe, GROUND, 'battlefield')
        self.paid('source')
        accepts = self.mode != 'decline'
        if self.csvid == NIGHTSTALKER and accepts:
            self.optional_entry()
        if has_victim and accepts and (self.foe == 0 or self.csvid == ASSASSINS):
            self.action_target(self.foe, 'victim')
        destroyed = has_victim and accepts and (self.csvid == NIGHTSTALKER or self.mode != 'indestructible')
        self.end_entry(('victim',) if destroyed else ())
        self.observe('own_creature', 'in_play', 1)
        if 'spare' in self.cards:
            self.observe('spare', 'in_play', 1)

    def trample(self, power):
        self.go(self.foe)
        self.go(self.actor)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.settle()
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 1)
        self.emit('combat_damage')
        self.settle()
        self.grave(('blocker',))
        self.life[self.foe] -= power - 3
        self.emit('assert_life', player=self.foe, value=self.life[self.foe])
        self.emit('end_combat')
        self.settle()

    def megatherium(self):
        if self.mode in ('two_paid', 'one_short'):
            self.add('kept_0', self.actor, 'Forest', 'hand')
            self.add('kept_1', self.actor, HOST, 'hand')
        if self.mode == 'trample':
            self.add('blocker', self.foe, GNOMES, 'battlefield')
        extra = 2 if self.mode == 'two_paid' else int(self.mode == 'one_short')
        self.cast_surplus((extra, 0, 0, 0, 0, 0))
        self.end_entry(('source',) if self.mode == 'one_short' else ())
        self.mana('assert_mana', self.actor, (int(self.mode == 'one_short'), 0, 0, 0, 0, 0))
        if self.mode == 'trample':
            self.trample(4)

    def demolok(self):
        self.add('foe_creature', self.foe, HOST, 'battlefield')
        self.add('own_creature', self.actor, HOST, 'battlefield')
        target = self.mode != 'creatures_only'
        if target:
            who = self.actor if self.mode == 'own_land' else self.foe
            self.add('victim', who, 'Island' if self.mode == 'own_land' else ARTIFACT, 'battlefield')
        self.paid('source')
        tribute = self.mode == 'paid'
        self.emit('choose_dialog', player=self.foe, value=int(not tribute))
        if not tribute and target:
            self.action_target(self.actor, 'victim')
        self.enter()
        if not tribute and target:
            self.grave(('victim',))
        count = 3 * tribute
        self.observe('source', 'plus1_counters', count)
        self.observe('source', 'power', 3 + count)
        self.observe('source', 'toughness', 3 + count)

    def offering(self):
        self.add('foe_creature', self.foe, HOST, 'battlefield')
        self.add('spare', self.actor, ARTIFACT, 'battlefield')
        sacrifices = ('own_creature', 'own_indestructible') if self.mode != 'empty_own' else ()
        for ref, name in zip(sacrifices, (HOST, MYR)):
            self.add(ref, self.actor, name, 'battlefield')
        reload = self.mode in ('reload', 'anthem_removed')
        if reload:
            self.add('new_creature', self.actor, HOST, 'hand')
        if self.mode == 'anthem_removed':
            self.add('disenchant', self.actor, BREAK, 'hand')
        self.paid('source')
        self.enter()
        self.grave(sacrifices)
        self.observe('foe_creature', 'power', 1)
        self.observe('foe_creature', 'toughness', 1)
        self.observe('spare', 'in_play', 1)
        self.receipts()
        if reload:
            self.cast_to('new_creature', 'battlefield')
            self.observe('new_creature', 'power', 3)
            self.observe('new_creature', 'toughness', 3)
        if self.mode == 'anthem_removed':
            self.cast_to('disenchant', 'graveyard', 'source')
            self.grave(('source',))
            self.observe('new_creature', 'power', 1)
            self.observe('new_creature', 'toughness', 1)

    def wurm(self):
        self.add('foe_basic', self.foe, 'Forest', 'graveyard')
        self.add('creature', self.actor, HOST, 'graveyard')
        has_land = self.mode != 'no_lands'
        if has_land:
            chosen = DUAL if self.mode == 'nonbasic_only' else 'Island'
            land_id = self.rows[chosen]['csvid']
            self.add('land', self.actor, chosen, 'graveyard')
        self.paid('source')
        if self.mode == 'return_basic':
            self.emit('choose_zone_card', player=self.actor, value=land_id)
        elif has_land:
            self.emit('observe_search_filters', value=1)
            self.emit('choose_optional_zone', player=self.actor, value=land_id)
        self.end_entry(() if self.mode == 'return_basic' else ('source',))
        if self.mode == 'return_basic':
            self.cards['land']['zone'] = 'hand'
        elif has_land:
            self.emit('assert_offered_legal_card', index=land_id, value=int(self.mode == 'decline'))
            self.emit('assert_forced_zone_choices', value=0)
            self.emit('observe_search_filters', value=0)

    def troll(self):
        self.add('own_noncreature', self.actor, ARTIFACT, 'battlefield')
        self.add('foe_creature', self.foe, HOST, 'battlefield')
        ref = 'source' if self.mode == 'self' else 'fodder'
        if ref != 'source':
            ai_choice = self.actor == 1 and self.mode in ('sac_creature', 'indestructible')
            name = SQUEE if ai_choice else {'sac_land': 'Island', 'indestructible': MYR}.get(self.mode, HOST)
            self.add(ref, self.actor, name, 'battlefield')
            if ai_choice and self.mode == 'indestructible':
                self.add('shield', self.actor, WITHSTAND, 'hand')
                self.add('destroy_probe', self.actor, KILL, 'hand')
                self.cast_to('shield', 'graveyard', ref)
                self.cast_to('destroy_probe', 'graveyard', ref)
                self.observe(ref, 'in_play', 1)
        self.paid('source')
        accepted = self.mode != 'decline'
        if self.actor == 0:
            self.action_target(self.actor, ref, optional=not accepted)
        self.enter()
        if accepted:
            self.grave((ref,))
        if ref != 'source':
            count = 2 * accepted
            self.observe('source', 'plus1_counters', count)
            self.observe('source', 'power', 2 + count)
            self.observe('source', 'toughness', 3 + count)

    def knight(self):
        pays = self.mode in ('pay', 'flanking')
        if self.mode in ('pay', 'suppress_regeneration'):
            self.add('victim', self.foe, REGEN_KNIGHT, 'battlefield')
        if self.mode == 'flanking':
            self.add('blocker', self.foe, HOST, 'battlefield')
        self.cast_surplus((0, 0, int(pays), 0, 0, 0))
        if not pays:
            self.action_target(self.actor, 'victim' if self.mode == 'suppress_regeneration' else 'source')
        self.end_entry(() if pays else ('victim',) if self.mode == 'suppress_regeneration' else ('source',))
        self.mana('assert_mana', self.actor, ZERO)
        if self.mode == 'pay':
            self.observe('victim', 'in_play', 1)
        if self.mode == 'flanking':
            self.go(self.foe)
            self.go(self.actor)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            self.emit('block', 'blocker', 'source', value=1)
            self.emit('advance_combat')
            self.grave(('blocker',))
            self.observe('source', 'blocked', 1)
            self.emit('combat_damage')
            self.settle()
            self.emit('end_combat')
            self.settle()

    def titan(self):
        chosen = []
        leaves = self.mode in ('leaves_bounce', 'leaves_death')
        if self.mode == 'all_five':
            self.add('spare_swamp', self.actor, 'Swamp', 'battlefield')
            for index, name in enumerate(LANDS):
                ref = f'land_{index}'
                self.add(ref, self.foe if index % 2 == 0 else self.actor, name, 'battlefield')
                chosen.append(ref)
        elif self.mode == 'typed_dual':
            self.add('dual', self.foe, DUAL, 'battlefield')
            self.add('preserved', self.actor, 'Island', 'battlefield')
            chosen = ['dual', 'dual']
        elif self.mode == 'no_types':
            self.add('untyped', self.foe, CITADEL, 'battlefield')
        elif leaves:
            self.add('later_land', self.actor, 'Plains', 'hand')
            self.add('foe_land', self.foe, 'Swamp', 'hand')
            self.add('exit_spell', self.actor, BOUNCE if self.mode == 'leaves_bounce' else KILL, 'hand')
        self.paid('source')
        for ref in chosen:
            self.action_target(self.actor, ref)
        self.end_entry(tuple(dict.fromkeys(chosen)))
        if not leaves:
            return
        for seat, ref in ((self.actor, 'later_land'), (self.foe, 'foe_land')):
            if self.active != seat:
                self.go(seat)
            self.emit('play_land', ref, value=1)
            self.cards[ref]['zone'] = 'battlefield'
        self.go(self.actor)
        self.receipts()
        self.paid('exit_spell', target='source')
        self.action_target(self.actor, 'foe_land')
        self.action_target(self.actor, 'later_land')
        self.settle()
        self.cards['exit_spell']['zone'] = 'graveyard'
        self.cards['source']['zone'] = 'hand' if self.mode == 'leaves_bounce' else 'graveyard'
        self.grave(('foe_land', 'later_land'))

    def dreadnought(self):
        enough = self.mode in ('exact_twelve', 'trample')
        if enough:
            self.add('first', self.actor, THORN, 'battlefield')
            self.add('second', self.actor, DRAGON, 'battlefield')
        elif self.mode == 'below_twelve':
            self.add('first', self.actor, THORN, 'battlefield')
            self.add('second', self.actor, CHAMELEON, 'battlefield')
        self.add('foe_creature', self.foe, THORN, 'battlefield')
        if self.mode == 'trample':
            self.add('blocker', self.foe, GNOMES, 'battlefield')
        self.paid('source')
        if self.actor == 0:
            if enough:
                self.action_target(self.actor, 'first')
                self.action_target(self.actor, 'second')
            elif self.mode == 'below_twelve':
                self.action_target(self.actor, 'first', optional=True)
        self.end_entry(('first', 'second') if enough else ('source',))
        if self.mode == 'trample':
            self.trample(12)


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    expected = PRINTED.get(csvid)
    observed = (record.get('mana_cost'), record.get('type_line'), stats(record) if 'power' in record else None,
                tuple(record.get('keywords', ())), record.get('oracle_text'))
    if expected is None or expected[1:] != observed or record.get('layout') != 'normal' or record.get('card_faces'):
        raise ValueError('complete printed entry destruction or sacrifice record required')
    return dict(csvid=csvid, printed_cost=expected[1], keywords=list(expected[4]))


def variants(shape):
    csvid = shape['csvid']
    modes = (MODES if csvid in MODES else FOLLOWERS)[csvid]
    return [f'{mode}_{actor}' for actor in (0, 1) for mode in modes if actor == 0 or (csvid, mode) not in HUMAN_ONLY]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed entry destruction or sacrifice variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name', '')) if record else None
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned entry destruction or sacrifice card required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 destruction or sacrifice recipe required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete destruction or sacrifice source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal' or oracle[name].get('card_faces')):
            raise ValueError('pinned destruction or sacrifice card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered entry destruction or sacrifice card required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        builder = Duel if csvid in MODES else FollowDuel
        board = builder(csvid, oracle[name], int(actor), mode, rows, oracle)
        board.build()
        recipe = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                      oracle_text=declaration['oracle_text'])
        if csvid in GAPS:
            recipe['untested_components'] = GAPS[csvid]
        cases.append(dict(name=label(csvid, mode, actor), card=name, fixture='empty_main', seed=3152,
                          setup=board.setup, steps=board.steps, _recipe=recipe))
    return cases
