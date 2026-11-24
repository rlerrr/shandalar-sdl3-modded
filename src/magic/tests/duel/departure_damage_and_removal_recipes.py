"""Paid deaths, departure effects, and their attached rules riders."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from mill_death_and_entry_recipes import Duel as PaidBoard, stats


FAMILY = 'departure_damage_and_removal'
PROFILE = Path(__file__).with_name('data') / 'departure_damage_and_removal_wave8_profiles.json'
ABU, BEARER, GANG, KEG = 391, 13850, 12374, 14559
PRINTED = {
    ABU: ('arabian_nights.c', '{W}', 'Creature — Human', (0, 1), (),
          "When this creature dies, destroy all creatures blocking or blocked by it. They can't be regenerated."),
    BEARER: ('journey_into_nyx.c', '{7}{R}', 'Creature — Giant', (10, 10), (),
             'When this creature dies, destroy all permanents at the beginning of the next end step.'),
    GANG: ('avacyn_restored.c', '{5}{R}', 'Creature — Devil', (3, 3), (),
           'When this creature dies, it deals 3 damage divided as you choose among one, two, or three targets.'),
    KEG: ('fate_reforged.c', '{4}', 'Artifact', None, (),
          'At the beginning of your upkeep, sacrifice this artifact.\n'
          'When this artifact is put into a graveyard from the battlefield, it deals 3 damage to any target.'),
}
MODES = {
    ABU: ('attacking_two', 'defending', 'regeneration_block', 'living_combat', 'bounce_blocked', 'noncombat'),
    BEARER: ('next_end_step', 'indestructible', 'bounce', 'exile'),
    GANG: ('one', 'two', 'three', 'bounce', 'exile'),
    KEG: ('own_upkeep', 'destroyed', 'walker', 'own_player', 'bounce', 'exile'),
}
SMALL, WALL, AIR, SKELETON = 'Prodigal Sorcerer', 'Dazzling Ramparts', 'Drift of Phantasms', 'Drudge Skeletons'
ARTIFACT, RELIC, FERVOR, WALKER = 'Millstone', 'Darksteel Relic', 'Fervor', 'Jace, Memory Adept'
KILL, BOUNCE, EXILE = 'Murder', 'Boomerang', 'Swords to Plowshares'
BREAK, BANISH, SHIELD = 'Shatter', 'Revoke Existence', 'Wrap in Vigor'
ZERO = (0,) * 6


def label(csvid, mode, actor):
    return f'DepartureDamageAndRemovalRecipe_{csvid}_{mode}_{actor}'


class Duel(PaidBoard):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.active = actor
        self.library = {0: [], 1: []}
        self.add('source', actor, record['name'], 'hand')

    def enter(self):
        self.paid('source')
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        self.observe('source', 'in_play', 1)
        if self.csvid != KEG:
            for prop, n in zip(('power', 'toughness'), stats(self.record)):
                self.observe('source', prop, n)
        else:
            self.emit('assert_matching_permanents', player=self.actor, index=self.csvid, prop='artifact', value=1)

    def moved(self, ref, zone):
        self.cards[ref]['zone'] = zone
        self.observe(ref, 'in_play', 0)
        if zone == 'hand':
            who = self.cards[ref]['player']
            self.emit('bind_hand_copy', 'returned', player=who, index=0, value=self.rows[self.cards[ref]['name']]['csvid'])
            self.observe('returned', 'in_hand', 1)

    def cast_exit(self, *, choices=()):
        self.paid('exit', target='source')
        self.queue(choices)
        self.settle()
        self.cards['exit']['zone'] = 'graveyard'

    def queue(self, refs):
        for ref in refs:
            if ref in ('owner', 'opponent'):
                self.emit('choose_player', player=self.actor, value=self.actor if ref == 'owner' else self.foe)
            else:
                self.emit('choose_target', ref, player=self.actor)

    def declare(self, attackers, blocks, *, forced_blocker=None):
        self.emit('begin_attackers')
        for attacker in attackers:
            self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.settle()
        if forced_blocker:
            self.emit('choose_target', forced_blocker, player=self.foe)
        self.emit('begin_blockers')
        for blocker, attacker in blocks:
            self.emit('block', blocker, attacker, value=1)
        self.emit('advance_combat')
        self.emit('assert_unused_choices', value=0)

    def combat_end(self):
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')
        self.settle()

    def abu(self):
        defensive = self.mode in ('defending', 'regeneration_block')
        combat = self.mode != 'noncombat'
        living = self.mode == 'living_combat'
        if combat and not living:
            self.add('hit', self.foe, SKELETON if self.mode == 'regeneration_block' else SMALL, 'battlefield')
        if self.mode in ('attacking_two', 'living_combat'):
            self.add('other_blocker', self.foe, AIR, 'battlefield')
        if self.mode == 'regeneration_block':
            self.add('shield', self.foe, SHIELD, 'hand')
        elif self.mode == 'noncombat':
            self.add('exit', self.actor, KILL, 'hand')
            self.add('witness', self.foe, SMALL, 'battlefield')
        elif self.mode == 'bounce_blocked':
            self.add('exit', self.actor, BOUNCE, 'hand')
        if self.mode == 'defending':
            self.add('unblocked', self.foe, SMALL, 'battlefield')
        self.enter()
        if not combat:
            self.cast_exit()
            self.moved('source', 'graveyard')
            self.observe('witness', 'in_play', 1)
            return
        self.go(self.foe)
        if defensive:
            if self.mode == 'regeneration_block':
                self.paid('shield')
                self.settle()
                self.cards['shield']['zone'] = 'graveyard'
            attackers = ('hit', 'unblocked') if self.mode == 'defending' else ('hit',)
            self.declare(attackers, (('source', 'hit'),))
        else:
            self.go(self.actor)
            targets = ('other_blocker',) if living else ('hit', 'other_blocker') if self.mode == 'attacking_two' else ('hit',)
            self.declare(('source',), tuple((ref, 'source') for ref in targets))
            self.observe('source', 'blocked', 1)
        if self.mode == 'bounce_blocked':
            self.cast_exit()
            self.moved('source', 'hand')
        self.combat_end()
        if self.mode == 'bounce_blocked':
            self.observe('hit', 'in_play', 1)
        elif living:
            self.observe('source', 'in_play', 1)
            self.observe('other_blocker', 'in_play', 1)
        else:
            self.moved('source', 'graveyard')
            self.moved('hit', 'graveyard')
            if self.mode == 'attacking_two':
                self.moved('other_blocker', 'graveyard')
            if self.mode == 'defending':
                self.observe('unblocked', 'in_play', 1)
                self.life[self.actor] -= 1

    def bearer(self):
        death = self.mode in ('next_end_step', 'indestructible')
        indestructible = self.mode == 'indestructible'
        for who, name in ((self.actor, 'Island'), (self.foe, 'Forest')):
            self.add(f'land_{who}', who, name, 'battlefield')
            self.setup[-1]['state'] = ['tapped']
        self.add('own', self.actor, SMALL, 'battlefield')
        self.add('foe', self.foe, RELIC if indestructible else ARTIFACT, 'battlefield')
        self.add('enchantment', self.actor, FERVOR, 'battlefield')
        self.add('walker', self.foe, WALKER, 'battlefield')
        self.add('exit', self.actor, KILL if death else BOUNCE if self.mode == 'bounce' else EXILE, 'hand')
        self.enter()
        self.cast_exit()
        self.moved('source', 'graveyard' if death else 'hand' if self.mode == 'bounce' else 'exile')
        if self.mode == 'exile':
            self.life[self.actor] += 10
        survivors = ('own', 'foe', 'enchantment', 'walker', 'land_0', 'land_1')
        for ref in survivors:
            self.observe(ref, 'in_play', 1)
        self.receipts()
        self.go(self.foe)
        for ref in survivors:
            if death and (ref != 'foe' or not indestructible):
                self.moved(ref, 'graveyard')
            else:
                self.observe(ref, 'in_play', 1)

    def gang(self):
        death = self.mode in ('one', 'two', 'three')
        if self.mode in ('two', 'three'):
            self.add('recipient', self.foe, WALL, 'battlefield')
        if self.mode == 'three':
            self.add('own_recipient', self.actor, AIR, 'battlefield')
        self.add('exit', self.actor, KILL if death else BOUNCE if self.mode == 'bounce' else EXILE, 'hand')
        plans = {'one': ('opponent', 'opponent', 'opponent'), 'two': ('opponent', 'recipient', 'opponent'),
                 'three': ('opponent', 'recipient', 'own_recipient')}
        self.enter()
        self.cast_exit(choices=plans.get(self.mode, ()))
        self.moved('source', 'graveyard' if death else 'hand' if self.mode == 'bounce' else 'exile')
        if self.mode == 'exile':
            self.life[self.actor] += 3
        if death:
            self.life[self.foe] -= plans[self.mode].count('opponent')
            if self.mode in ('two', 'three'):
                self.observe('recipient', 'damage', 1)
            if self.mode == 'three':
                self.observe('own_recipient', 'damage', 1)

    def keg(self):
        if self.mode == 'walker':
            self.add('walker', self.foe, WALKER, 'battlefield')
            self.emit('assert_counter', 'walker', counter_kind='loyalty', value=4)
        if self.mode == 'destroyed':
            self.add('recipient', self.foe, WALL, 'battlefield')
        if self.mode != 'own_upkeep':
            action = {'bounce': BOUNCE, 'exile': BANISH}.get(self.mode, BREAK)
            self.add('exit', self.actor, action, 'hand')
        self.enter()
        if self.mode == 'own_upkeep':
            self.go(self.foe)
            self.observe('source', 'in_play', 1)
            self.emit('assert_life', player=self.foe, value=20)
            self.queue(('opponent',))
            self.go(self.actor)
            self.emit('assert_unused_choices', value=0)
            self.moved('source', 'graveyard')
            self.life[self.foe] -= 3
        else:
            planned = {'destroyed': ('recipient',), 'walker': ('walker',), 'own_player': ('owner',)}
            self.cast_exit(choices=planned.get(self.mode, ()))
            self.moved('source', 'hand' if self.mode == 'bounce' else 'exile' if self.mode == 'exile' else 'graveyard')
            if self.mode == 'own_player':
                self.life[self.actor] -= 3
            if self.mode == 'destroyed':
                self.observe('recipient', 'damage', 3)
            if self.mode == 'walker':
                self.emit('assert_counter', 'walker', counter_kind='loyalty', value=1)

    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        {ABU: self.abu, BEARER: self.bearer, GANG: self.gang, KEG: self.keg}[self.csvid]()
        self.receipts()


EFFIGY, MEMBRANE, FLOOD, HAVOC, SPITTER, SPITE, IMPLEMENT = 581, 11575, 2484, 6397, 5065, 9434, 16727
HAG, SHRINE, ALBATROSS, CATOBLEPAS, PROPHET, TYRANT, LICH = 587, 938, 2208, 13478, 4123, 9556, 17335
SOURCES = {csvid: printed[0] for csvid, printed in PRINTED.items()} | {
    EFFIGY: 'legends.c', MEMBRANE: 'new_phyrexia.c', FLOOD: 'mirage.c', HAVOC: 'legions.c', SPITTER: 'invasion.c',
    SPITE: 'morningtide.c', IMPLEMENT: 'aether_revolt.c', HAG: 'legends.c', SHRINE: 'the_dark.c',
    ALBATROSS: 'homelands.c', CATOBLEPAS: 'theros.c', PROPHET: 'urza_destiny.c', TYRANT: 'shadowmoor.c',
    LICH: 'commander_2017.c',
}
FOLLOWERS = {
    EFFIGY: ('death', 'chain', 'unrelated_damage', 'bounce'),
    MEMBRANE: ('white', 'life', 'combat_lethal', 'combat_mark', 'other_death'),
    FLOOD: ('bounce', 'death', 'exile', 'gains_flying', 'one_island'),
    HAVOC: ('death', 'bounce', 'flying'), SPITTER: ('upkeep', 'death', 'exile'),
    SPITE: ('death', 'bounce', 'exile', 'evoke'), IMPLEMENT: ('activate', 'walker', 'destroy', 'bounce', 'wrong_color'),
    HAG: ('pinger', 'earlier_turn', 'unmarked'), SHRINE: ('basic_mountain', 'nonbasic_mountain', 'bounce'),
    ALBATROSS: ('unfunded', 'paid_bury', 'paid_ransom', 'nondamage'),
    CATOBLEPAS: ('death', 'bounce', 'paid_block'), PROPHET: ('death', 'bounce', 'exile'),
    TYRANT: ('death', 'zero_counters', 'bounce'), LICH: ('sacrifice', 'discard', 'life', 'bounce'),
}
GAPS = {
    ALBATROSS: ['multi-creature damage-source ransom order'],
    CATOBLEPAS: ['forced assignment needs the complete blocker picker; paid_block checks payment and the real block/death'],
    LICH: ['choosing multiple modes for distinct opponents requires multiplayer'],
}
SHIVAN, BONE, EMPERION, BLUE = 'Shivan Dragon', 'Bone Shaman', 'Platinum Emperion', 'Wall of Tears'
MYR, MOUNTAIN_DUAL, ISLAND_DUAL = 'Darksteel Myr', 'Volcanic Island', 'Tropical Island'
GOBLIN, JUMP, SHOCK, HEXMAGE = 'Goblin Balloon Brigade', 'Jump', 'Shock', 'Vampire Hexmage'


class DepartureDuel(Duel):
    def basics(self, names):
        for index, name in enumerate(names):
            self.add(f'land_{index}', self.actor, name, 'battlefield')
            self.setup[-1]['state'] = ['tapped']

    def enter_with_fee(self, fee, *, life=0, numbers=(), choices=(), exits=False):
        self.mana('set_mana', self.actor, fee)
        self.emit('refresh')
        for value in numbers:
            self.emit('choose_number', player=self.actor, value=value)
        self.emit('cast', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.life[self.actor] -= life
        self.emit('assert_life', player=self.actor, value=self.life[self.actor])
        self.queue(choices)
        self.settle()
        if exits:
            self.moved('source', 'graveyard')
        else:
            self.cards['source']['zone'] = 'battlefield'
            self.observe('source', 'in_play', 1)

    def add_exit(self, *, artifact=False):
        name = {'bounce': BOUNCE, 'exile': BANISH if artifact else EXILE}.get(self.mode, BREAK if artifact else KILL)
        self.add('exit', self.actor, name, 'hand')

    def ordinary_exit(self, *, choices=(), life=0):
        self.cast_exit(choices=choices)
        destination = {'bounce': 'hand', 'exile': 'exile'}.get(self.mode, 'graveyard')
        self.moved('source', destination)
        if destination == 'exile':
            self.life[self.actor] += life

    def stats_are(self, ref, power, toughness):
        self.observe(ref, 'in_play', 1)
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def effigy(self):
        self.add('recipient', self.foe, WALL, 'battlefield')
        self.add_exit()
        if self.mode == 'chain':
            self.add('other', self.actor, self.record['name'], 'battlefield')
        if self.mode == 'unrelated_damage':
            self.add('ping', self.actor, SHOCK, 'hand')
        self.enter()
        if self.mode == 'chain':
            self.paid('exit', target='other')
            self.queue(('source', 'recipient'))
            self.settle()
            self.cards['exit']['zone'] = 'graveyard'
            self.moved('other', 'graveyard')
            self.moved('source', 'graveyard')
            self.observe('recipient', 'damage', 6)
            return
        if self.mode == 'unrelated_damage':
            self.paid('ping', target='source')
            self.settle()
            self.cards['ping']['zone'] = 'graveyard'
            self.observe('source', 'damage', 2)
        self.ordinary_exit(choices=() if self.mode == 'bounce' else ('recipient',))
        self.observe('recipient', 'damage', 0 if self.mode == 'bounce' else 3)

    def membrane(self):
        combat = self.mode.startswith('combat_')
        if combat:
            self.add('attacker', self.foe, EMPERION if self.mode == 'combat_mark' else BONE, 'battlefield')
            self.add('unblocked', self.foe, SMALL, 'battlefield')
        if self.mode == 'other_death':
            self.add('recipient', self.foe, WALL, 'battlefield')
            self.add_exit()
        life = int(self.mode == 'life')
        self.enter_with_fee((1, 0, 0, 0, 0, 1 - life), life=2 * life, numbers=(life,))
        self.stats_are('source', 0, 3)
        if combat:
            self.go(self.foe)
            self.declare(('attacker', 'unblocked'), (('source', 'attacker'),))
            self.combat_end()
            self.moved('source', 'graveyard')
            self.observe('unblocked', 'in_play', 1)
            self.life[self.actor] -= 1
            if self.mode == 'combat_mark':
                self.observe('attacker', 'damage', 6)
            else:
                self.moved('attacker', 'graveyard')
        elif self.mode == 'other_death':
            self.ordinary_exit()
            self.observe('recipient', 'damage', 0)
        elif self.mode == 'white':
            self.go(self.foe)
            self.go(self.actor)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.combat_end()

    def floodgate(self):
        names = ('Island',) if self.mode == 'one_island' else ('Island', ISLAND_DUAL, 'Snow-Covered Island')
        self.basics(names)
        self.add('ground', self.actor, WALL, 'battlefield')
        self.add('blue', self.foe, BLUE, 'battlefield')
        self.add('flying', self.foe, SHIVAN, 'battlefield')
        if self.mode == 'gains_flying':
            self.add('exit', self.actor, JUMP, 'hand')
        else:
            self.add_exit()
        self.enter()
        if self.mode == 'one_island':
            self.go(self.foe)
            self.go(self.actor)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.combat_end()
            self.emit('turn', player=self.actor)
        self.ordinary_exit()
        self.observe('ground', 'damage', len(names) // 2)
        for ref in ('blue', 'flying'):
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'damage', 0)

    def havoc(self):
        if self.mode == 'flying':
            self.add('ground', self.foe, WALL, 'battlefield')
            self.add('air', self.foe, AIR, 'battlefield')
            self.enter()
            self.go(self.foe)
            self.go(self.actor)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            self.emit('block', 'ground', 'source', value=0)
            self.emit('block', 'air', 'source', value=1)
            self.emit('advance_combat')
            self.combat_end()
            self.moved('air', 'graveyard')
            return
        self.add('own', self.actor, WALL, 'battlefield')
        self.add('foe', self.foe, SMALL, 'battlefield')
        self.add('control', self.foe, ARTIFACT, 'battlefield')
        self.add_exit()
        self.enter()
        self.ordinary_exit()
        changed = self.mode == 'death'
        if changed:
            self.moved('foe', 'graveyard')
        self.stats_are('own', -5 if changed else 0, 2 if changed else 7)
        self.observe('control', 'in_play', 1)
        if changed:
            self.go(self.foe)
            self.stats_are('own', 0, 7)

    def spitter(self):
        self.add('ally', self.actor, WALL, 'battlefield')
        self.add('foe', self.foe, AIR, 'battlefield')
        self.add('control', self.foe, ARTIFACT, 'battlefield')
        if self.mode != 'upkeep':
            self.add_exit()
        self.enter()
        if self.mode == 'upkeep':
            self.go(self.foe)
            self.emit('assert_life', player=self.actor, value=20)
            self.emit('assert_life', player=self.foe, value=20)
            self.go(self.actor)
            self.observe('source', 'damage', 1)
        else:
            self.ordinary_exit(life=2)
        hit = int(self.mode != 'exile')
        self.life = [life - hit for life in self.life]
        self.observe('ally', 'damage', hit)
        self.observe('foe', 'damage', hit)
        self.observe('control', 'in_play', 1)

    def spite(self):
        self.add('victim', self.foe, WALL, 'battlefield')
        if self.mode == 'evoke':
            self.enter_with_fee((1, 0, 0, 0, 2, 0), choices=('victim',), exits=True)
        else:
            self.add_exit()
            self.enter()
            self.ordinary_exit(choices=('victim',), life=6)
        self.observe('victim', 'damage', 6)

    def implement(self):
        activated = self.mode in ('activate', 'wrong_color', 'walker')
        for index, name in enumerate(('Plains', 'Forest')):
            self.add(f'book_{index}', self.actor, name, 'library')
            self.library[self.actor].append(name)
        if not activated:
            self.add_exit(artifact=True)
        if self.mode == 'walker':
            self.add('walker', self.foe, WALKER, 'battlefield')
            self.emit('assert_counter', 'walker', counter_kind='loyalty', value=4)
        self.paid('source')
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        if activated:
            correct = self.mode != 'wrong_color'
            self.mana('set_mana', self.actor, (0, 0, int(not correct), 0, int(correct), 0))
            self.emit('refresh')
            self.emit('can_activate', 'source', value=int(correct))
            if not correct:
                return
            if self.mode == 'walker':
                # Probe the production target chooser, retaining legal-target Oracle without forcing a denied target.
                self.emit('probe_activation_target', 'source', 'walker', index=-1, value=1)
                self.mana('assert_mana', self.actor, (0, 0, 0, 0, 1, 0))
                self.observe('source', 'in_play', 1)
                self.emit('assert_counter', 'walker', counter_kind='loyalty', value=4)
                return
            self.queue(('opponent',))
            self.emit('activate', 'source', value=1)
            self.mana('assert_mana', self.actor, ZERO)
            self.settle()
            self.moved('source', 'graveyard')
            self.life[self.foe] -= 1
        else:
            self.ordinary_exit()
        if self.mode != 'bounce':
            self.library[self.actor].pop(0)
            self.cards['book_0']['zone'] = 'hand'

    def brine(self):
        self.add('pinger', self.foe, SMALL, 'battlefield')
        self.add('other', self.foe, SKELETON, 'battlefield')
        self.add('exit', self.foe, KILL, 'hand')
        self.enter()
        self.go(self.foe)
        if self.mode != 'unmarked':
            self.emit('choose_target', 'source', player=self.foe)
            self.emit('activate', 'pinger', value=1)
            self.emit('assert_stack', value=1)
            self.settle()
            self.observe('source', 'damage', 1)
        if self.mode == 'earlier_turn':
            self.go(self.actor)
            self.go(self.foe)
            self.observe('source', 'damage', 0)
        self.ordinary_exit()
        changed = self.mode == 'pinger'
        self.stats_are('pinger', 0 if changed else 1, 2 if changed else 1)
        self.stats_are('other', 1, 1)
        self.go(self.actor)
        self.go(self.foe)
        self.stats_are('pinger', 0 if changed else 1, 2 if changed else 1)

    def shrine(self):
        name = MOUNTAIN_DUAL if self.mode == 'nonbasic_mountain' else 'Mountain'
        self.basics((name,))
        self.add('own', self.actor, GOBLIN, 'battlefield')
        self.add('foe', self.foe, GOBLIN, 'battlefield')
        self.add('control', self.foe, SMALL, 'battlefield')
        self.add('exit', self.actor, BOUNCE if self.mode == 'bounce' else 'Disenchant', 'hand')
        self.paid('source', target='land_0')
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        for ref in ('own', 'foe'):
            self.stats_are(ref, 1 if self.mode == 'nonbasic_mountain' else 2, 1)
        self.ordinary_exit()
        self.moved('own', 'graveyard')
        self.moved('foe', 'graveyard')
        self.observe('control', 'in_play', 1)
        self.observe('land_0', 'in_play', 1)

    def albatross(self):
        self.add('responder', self.foe, SHIVAN, 'battlefield')
        if self.mode == 'nondamage':
            self.add_exit()
        elif self.mode == 'paid_bury':
            self.add('shield', self.foe, SHIELD, 'hand')
        self.enter()
        if self.mode == 'nondamage':
            self.ordinary_exit()
            self.stats_are('responder', 5, 5)
            return
        self.go(self.foe)
        if self.mode == 'paid_bury':
            self.paid('shield')
            self.settle()
            self.cards['shield']['zone'] = 'graveyard'
        self.declare(('responder',), (('source', 'responder'),))
        paid = self.mode != 'unfunded'
        if paid:
            self.mana('set_mana', self.actor, (1, 0, 1, 0, 0, 0))
            # The legacy offers the damaged creature’s controller a dialog even if the owner prompt is missing.
            self.emit('choose_dialog', player=-1, value=0)
            self.emit('choose_dialog', player=self.foe, value=int(self.mode == 'paid_ransom'))
        else:
            self.emit('choose_dialog', player=self.foe, value=0)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=int(not paid))
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.actor, ZERO)
        self.mana('set_mana', self.actor, ZERO)
        self.emit('end_combat')
        self.settle()
        self.moved('source', 'graveyard')
        if self.mode == 'paid_bury':
            self.moved('responder', 'graveyard')
        else:
            self.observe('responder', 'in_play', 1)
        self.life[self.foe] -= 2 * (self.mode == 'paid_ransom')

    def catoblepas(self):
        self.add('victim', self.foe, WALL, 'battlefield')
        self.add('own', self.actor, WALL, 'battlefield')
        if self.mode == 'paid_block':
            self.add('blocker', self.foe, BONE, 'battlefield')
        else:
            self.add_exit()
        self.enter()
        if self.mode == 'paid_block':
            self.go(self.foe)
            self.go(self.actor)
            self.mana('set_mana', self.actor, (2, 0, 0, 1, 0, 0))
            self.emit('refresh')
            self.emit('can_activate', 'source', value=1)
            self.emit('activate', 'source', value=1)
            self.emit('assert_stack', value=1)
            self.mana('assert_mana', self.actor, ZERO)
            self.settle()
            # The real beginning-of-blockers trigger chooses and assigns the blocker.
            self.declare(('source',), (), forced_blocker='blocker')
            self.observe('blocker', 'blocking_count', 1)
            self.observe('source', 'blocked', 1)
            self.queue(('victim',))
            self.combat_end()
            self.moved('source', 'graveyard')
            self.moved('blocker', 'graveyard')
        else:
            self.ordinary_exit(choices=() if self.mode == 'bounce' else ('victim',))
        self.stats_are('victim', 0 if self.mode == 'bounce' else -3, 7 if self.mode == 'bounce' else 4)
        self.stats_are('own', 0, 7)

    def prophet(self):
        self.add('own', self.actor, WALL, 'battlefield')
        self.add('foe', self.foe, MYR, 'battlefield')
        self.add('other', self.foe, ARTIFACT, 'battlefield')
        self.add_exit()
        self.enter()
        self.ordinary_exit(life=2)
        for ref in ('own', 'foe'):
            if self.mode == 'death':
                self.moved(ref, 'exile')
            else:
                self.observe(ref, 'in_play', 1)
        self.observe('other', 'in_play', 1)

    def grief(self):
        self.add('victim', self.foe, WALL, 'battlefield')
        self.add_exit()
        if self.mode == 'zero_counters':
            self.add('hex', self.actor, HEXMAGE, 'battlefield')
        self.enter_with_fee((5, int(self.actor == 0), 0, 0, int(self.actor == 1), 0))
        self.stats_are('source', 4, 4)
        self.observe('source', 'minus1_counters', 4)
        if self.mode == 'zero_counters':
            self.emit('choose_target', 'source', player=self.actor)
            self.emit('activate', 'hex', value=1)
            self.emit('assert_stack', value=1)
            self.settle()
            self.moved('hex', 'graveyard')
            self.observe('source', 'minus1_counters', 0)
        self.ordinary_exit(choices=() if self.mode == 'bounce' else ('victim',))
        result = 4 if self.mode == 'death' else 0
        self.observe('victim', 'minus1_counters', result)
        self.stats_are('victim', -result, 7 - result)

    def lich(self):
        self.add('victim', self.foe, SMALL, 'battlefield')
        for n in range(2):
            self.add(f'hand_{n}', self.foe, 'Forest', 'hand')
        self.add_exit()
        self.enter()
        self.paid('exit', target='source')
        if self.mode != 'bounce':
            self.emit('choose_dialog', player=self.actor, value=('sacrifice', 'discard', 'life').index(self.mode))
            if self.mode == 'sacrifice' and self.foe == 0:
                self.emit('choose_target', 'victim', player=self.foe)
            if self.mode == 'discard' and self.foe == 0:
                for _ in range(2):
                    self.emit('choose_zone_card', player=self.foe, value=self.rows['Forest']['csvid'])
        self.settle()
        self.cards['exit']['zone'] = 'graveyard'
        self.moved('source', 'hand' if self.mode == 'bounce' else 'graveyard')
        if self.mode == 'sacrifice':
            self.moved('victim', 'graveyard')
        elif self.mode == 'discard':
            for n in range(2):
                self.cards[f'hand_{n}']['zone'] = 'graveyard'
        elif self.mode == 'life':
            self.life[self.foe] -= 5

    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        actions = {EFFIGY: self.effigy, MEMBRANE: self.membrane, FLOOD: self.floodgate, HAVOC: self.havoc,
                   SPITTER: self.spitter, SPITE: self.spite, IMPLEMENT: self.implement, HAG: self.brine,
                   SHRINE: self.shrine, ALBATROSS: self.albatross, CATOBLEPAS: self.catoblepas,
                   PROPHET: self.prophet, TYRANT: self.grief, LICH: self.lich}
        actions[self.csvid]()
        self.receipts()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name', ''))
    if (not pin or pin['registry']['csvid'] != csvid or csvid not in SOURCES
            or not exact(pin['oracle'], record) or record.get('layout') != 'normal' or record.get('card_faces')):
        raise ValueError('complete pinned departure rules required')
    printed = PRINTED.get(csvid)
    if printed and printed[1:] != (record['mana_cost'], record['type_line'], stats(record) if 'power' in record else None,
                                    tuple(record['keywords']), record['oracle_text']):
        raise ValueError('departure rules source text differs from pilot')
    return dict(csvid=csvid, printed_cost=record['mana_cost'], keywords=list(record['keywords']))


def variants(shape):
    csvid = shape['csvid']
    choices = (MODES if csvid in MODES else FOLLOWERS)[csvid]
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in choices]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct departure rules variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name', '')) if record else None
    if not pin:
        raise ValueError('complete pinned departure rules required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 departure rules recipe required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or {source['registry']['csvid'] for source in pins['sources'].values()} != set(SOURCES)
            or not pins['witnesses']):
        raise ValueError('complete departure rules source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry']) or row.get('kind') != 'card'
                or row.get('handler') in (None, '0', 'NULL', 'card_dummy') or oracle[name].get('layout') != 'normal'
                or oracle[name].get('card_faces')):
            raise ValueError('pinned departure rules card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered departure rules card required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        builder = Duel if csvid in MODES else DepartureDuel
        board = builder(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        recipe = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                      oracle_text=declaration['oracle_text'])
        if csvid in GAPS:
            recipe['untested_components'] = GAPS[csvid]
        cases.append(dict(name=label(csvid, mode, seat), card=name, fixture='empty_main', seed=3596,
                          setup=board.setup, steps=board.steps, _recipe=recipe))
    return cases
