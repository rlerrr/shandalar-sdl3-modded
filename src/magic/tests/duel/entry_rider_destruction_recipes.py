"""Paid destructive entry effects and their independently printed riders."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from mill_death_and_entry_recipes import Duel as PaidBoard, stats


FAMILY = 'entry_rider_destruction'
PROFILE = Path(__file__).with_name('data') / 'entry_rider_destruction_wave8_profiles.json'
ARTIFICER, PUZZLEKNOT, MANTICORE, KNIGHT = 15206, 16377, 16967, 19412
PRINTED = {
    ARTIFICER: ('origins.c', '{2}{U}{R}', 'Creature — Human Artificer', (2, 3), ('Haste',),
                'Haste (This creature can attack and {T} as soon as it comes under your control.)\n'
                'When this creature enters, you may have it deal damage to target creature equal to the number of artifacts you control.'),
    PUZZLEKNOT: ('kaladesh.c', '{2}', 'Artifact', None, (),
                 'When this artifact enters, it deals 1 damage to any target.\n'
                 '{2}{R}, Sacrifice this artifact: It deals 1 damage to any target.'),
    MANTICORE: ('amonkhet.c', '{4}{R}', 'Creature — Manticore', (5, 4), (),
                'When this creature enters, put a -1/-1 counter on target creature you control. '
                'This creature deals 3 damage to target opponent or planeswalker.'),
    KNIGHT: ('guilds_of_ravnica.c', '{1}{G}{W}', 'Creature — Dryad Knight', (2, 1), (),
             'When this creature enters, choose one —\n• Put two +1/+1 counters on this creature.\n'
             '• Destroy target artifact or enchantment.\n• You gain 4 life.'),
}
MODES = {
    ARTIFICER: ('three_types', 'own_target', 'foe_only', 'decline', 'fresh_attack'),
    PUZZLEKNOT: ('two_player_hits', 'creatures', 'walker', 'short', 'wrong_color', 'own_player'),
    MANTICORE: ('opponent', 'self', 'lethal_counter', 'foe_walker', 'own_walker', 'prevented'),
    KNIGHT: ('counters', 'artifact', 'enchantment', 'life', 'indestructible', 'no_destruction_targets'),
}
ARTIFACT, MYR, TREE, INGOT = 'Millstone', 'Darksteel Myr', 'Tree of Tales', 'Darksteel Ingot'
HOST, WALL, ENCHANTMENT = 'Prodigal Sorcerer', 'Dazzling Ramparts', 'Fervor'
WALKER, FIELD = 'Jace, Memory Adept', 'Energy Field'
ZERO = (0,) * 6


def label(csvid, mode, actor):
    return f'EntryRiderDestructionRecipe_{csvid}_{mode}_{actor}'


class Duel(PaidBoard):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.active = actor
        self.library = {0: [], 1: []}
        self.add('source', actor, record['name'], 'hand')

    def choose(self, ref):
        self.emit('choose_target', ref, player=self.actor)

    def player_target(self, seat):
        self.emit('choose_player', player=self.actor, value=seat)

    def grave(self, ref):
        self.cards[ref]['zone'] = 'graveyard'
        self.observe(ref, 'in_play', 0)

    def enter(self, *, unoffered_targets=0):
        if unoffered_targets:
            self.emit('resolve')
            self.emit('assert_unused_choices', value=unoffered_targets)
            self.emit('assert_choices_consumed')
            self.emit('refresh')
        else:
            self.settle()
        self.cards['source']['zone'] = 'battlefield'
        self.observe('source', 'in_play', 1)

    def loyalty(self, value):
        self.emit('assert_counter', 'walker', counter_kind='loyalty', value=value)

    def artificer(self):
        attacking = self.mode == 'fresh_attack'
        artifacts = (ARTIFACT, MYR, TREE) if self.mode == 'three_types' else (ARTIFACT,) if self.mode in ('own_target', 'decline') else ()
        for index, name in enumerate(artifacts):
            self.add(f'artifact_{index}', self.actor, name, 'battlefield')
        if self.mode in ('three_types', 'foe_only'):
            self.add('foe_artifact', self.foe, ARTIFACT, 'battlefield')
            self.add('foe_myr', self.foe, MYR, 'battlefield')
        target = 'source' if attacking else 'victim'
        if not attacking:
            owner = self.actor if self.mode == 'own_target' else self.foe
            self.add('victim', owner, HOST if self.mode == 'own_target' else WALL, 'battlefield')
        self.paid('source')
        if self.actor == 0:
            self.emit('choose_trigger', 'source', player=self.actor)
        if self.mode == 'decline':
            self.emit('choose_optional_target', target, player=self.actor)
        else:
            self.choose(target)
        self.enter()
        if self.mode == 'own_target':
            self.grave('victim')
        else:
            self.observe(target, 'damage', len(artifacts) if self.mode != 'decline' else 0)
        self.observe('source', 'power', 2)
        self.observe('source', 'toughness', 3)
        if attacking:
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.settle()
            self.life[self.foe] -= 2
            self.emit('end_combat')
            self.settle()

    def puzzleknot(self):
        if self.mode == 'creatures':
            self.add('kill_target', self.foe, HOST, 'battlefield')
            self.add('later', self.actor, WALL, 'battlefield')
        if self.mode == 'walker':
            self.add('walker', self.foe, WALKER, 'battlefield')
            self.loyalty(4)
        self.paid('source')
        if self.mode in ('creatures', 'walker'):
            self.choose('kill_target' if self.mode == 'creatures' else 'walker')
        else:
            self.player_target(self.actor if self.mode == 'own_player' else self.foe)
        self.enter()
        if self.mode == 'creatures':
            self.grave('kill_target')
            self.observe('later', 'damage', 0)
        elif self.mode == 'walker':
            self.loyalty(3)
        else:
            self.life[self.actor if self.mode == 'own_player' else self.foe] -= 1
        self.receipts()
        funds = {'short': (1, 0, 0, 0, 1, 0), 'wrong_color': (2, 0, 0, 1, 0, 0)}.get(self.mode, (2, 0, 0, 0, 1, 0))
        allowed = self.mode not in ('short', 'wrong_color')
        self.mana('set_mana', self.actor, funds)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=int(allowed))
        if not allowed:
            self.mana('assert_mana', self.actor, funds)
            return
        if self.mode in ('creatures', 'walker'):
            self.choose('later' if self.mode == 'creatures' else 'walker')
        else:
            self.player_target(self.foe)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.grave('source')
        if self.mode == 'walker':
            self.loyalty(3)
        self.settle()
        if self.mode == 'creatures':
            self.observe('later', 'damage', 1)
        elif self.mode == 'walker':
            self.loyalty(2)
        else:
            self.life[self.foe] -= 1

    def manticore(self):
        counter_target = 'source' if self.mode == 'self' else 'own'
        if counter_target == 'own':
            self.add('own', self.actor, HOST if self.mode == 'lethal_counter' else WALL, 'battlefield')
        self.add('foe_creature', self.foe, HOST, 'battlefield')
        walker = self.mode in ('own_walker', 'foe_walker')
        if walker:
            self.add('walker', self.actor if self.mode == 'own_walker' else self.foe, WALKER, 'battlefield')
            self.loyalty(4)
        if self.mode == 'prevented':
            self.add('field', self.foe, FIELD, 'battlefield')
        self.paid('source')
        self.choose(counter_target)
        if walker:
            self.choose('walker')
        else:
            self.player_target(self.foe)
        self.enter()
        if self.mode == 'lethal_counter':
            self.grave('own')
        else:
            self.observe(counter_target, 'minus1_counters', 1)
            target_stats = stats(self.oracle[self.cards[counter_target]['name']])
            self.observe(counter_target, 'power', target_stats[0] - 1)
            self.observe(counter_target, 'toughness', target_stats[1] - 1)
        self.observe('foe_creature', 'minus1_counters', 0)
        if walker:
            self.loyalty(1)
        elif self.mode != 'prevented':
            self.life[self.foe] -= 3

    def knight(self):
        targets = self.mode != 'no_destruction_targets'
        if targets:
            self.add('artifact', self.foe, INGOT if self.mode == 'indestructible' else ARTIFACT, 'battlefield')
            self.add('enchantment', self.actor, ENCHANTMENT, 'battlefield')
        self.add('creature', self.foe, WALL, 'battlefield')
        self.paid('source')
        mode = {'artifact': 1, 'enchantment': 1, 'indestructible': 1, 'life': 2}.get(self.mode, 0)
        self.emit('choose_dialog', player=self.actor, value=mode)
        if mode == 1:
            self.choose('enchantment' if self.mode == 'enchantment' else 'artifact')
        self.enter()
        if self.mode in ('artifact', 'enchantment'):
            self.grave(self.mode)
        if mode == 2:
            self.life[self.actor] += 4
        bonus = 2 * (mode == 0)
        self.observe('source', 'plus1_counters', bonus)
        self.observe('source', 'power', 2 + bonus)
        self.observe('source', 'toughness', 1 + bonus)

    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        {ARTIFICER: self.artificer, PUZZLEKNOT: self.puzzleknot, MANTICORE: self.manticore, KNIGHT: self.knight}[self.csvid]()
        self.receipts()


HECATOMB, DRAGON, FIREBOMBERS, CUTPURSE, FOOTPAD = 1965, 2880, 4848, 4575, 4576
YETI, SHAMBLER, SHAMAN, TORNADO, HELLION, OGRE, REVOLUTIONARY = 8448, 8686, 9289, 7142, 8877, 14536, 16736
SOURCES = {
    ARTIFICER: 'origins.c', PUZZLEKNOT: 'kaladesh.c', MANTICORE: 'amonkhet.c', KNIGHT: 'guilds_of_ravnica.c',
    HECATOMB: 'ice_age.c', DRAGON: 'portal_1_2_3k.c', FIREBOMBERS: 'prophecy.c',
    CUTPURSE: 'mercadian_masques.c', FOOTPAD: 'mercadian_masques.c', YETI: 'coldsnap.c',
    SHAMBLER: 'time_spiral.c', SHAMAN: 'lorwyn.c', TORNADO: 'fifth_dawn.c', HELLION: 'planar_chaos.c',
    OGRE: 'fate_reforged.c', REVOLUTIONARY: 'aether_revolt.c',
}
FOLLOWERS = {
    HECATOMB: ('four', 'empty', 'swamp', 'wrong_swamp'),
    DRAGON: ('typed_count', 'zero', 'own', 'flying'),
    FIREBOMBERS: ('equal', 'below', 'both_choose', 'mixed_types'),
    CUTPURSE: ('pay', 'short', 'no_permanent'),
    FOOTPAD: ('pay', 'short', 'no_permanent'),
    YETI: ('survive', 'mutual_death', 'own_only', 'snow_return', 'no_snow', 'offturn'),
    SHAMBLER: ('grounded', 'death', 'bounce', 'echo_pay', 'echo_short'),
    SHAMAN: ('self_count', 'two_giants', 'lethal'),
    TORNADO: ('fliers', 'normal_damage', 'ignore_blocker'),
    HELLION: ('damage', 'zero', 'cannot_prevent', 'echo_pay', 'echo_short'),
    OGRE: ('counter', 'artifact', 'indestructible', 'own'),
    REVOLUTIONARY: ('choose', 'indestructible', 'other_only', 'decline'),
}
HUMAN_ONLY = {(REVOLUTIONARY, 'decline')} | {(HELLION, mode) for mode in FOLLOWERS[HELLION]}
GAPS = {
    HECATOMB: ['declining the four-creature payment while four or more are available',
               'a controlled nonbasic Swamp and a planeswalker damage recipient'],
    CUTPURSE: ['refusing an affordable automatic generic payment'],
    FOOTPAD: ['refusing an affordable automatic generic payment'],
    YETI: ['returning a borrowed creature to a different owner', 'offering previously floated snow mana'],
    HELLION: ['an exact independently selected damage amount for the native AI seat'],
    REVOLUTIONARY: ['an explicit decline when the legacy AI sacrifice picker selects automatically'],
}
AIR, SHIVAN, PLATEAU = 'Drift of Phantasms', 'Shivan Dragon', 'Plateau'
GIANT, SNOW, SHIELD = 'Bone Shaman', 'Snow-Covered Forest', 'Safe Passage'
BOUNCE, MURDER = 'Unsummon', 'Murder'


class RiderDuel(Duel):
    def setup_land(self, ref, name, seat, *, tapped=False):
        self.add(ref, seat, name, 'battlefield')
        if tapped:
            self.setup[-1]['state'] = ['tapped']

    def cast_to_grave(self, ref, *, target=None):
        self.paid(ref, target=target)
        self.settle()
        self.cards[ref]['zone'] = 'graveyard'

    def source_stats(self, bonus=0):
        power, toughness = stats(self.record)
        self.observe('source', 'power', power + bonus)
        self.observe('source', 'toughness', toughness + bonus)

    def fight_with_blocker(self, *, ignore=None, require_air=False):
        self.go(self.foe)
        self.go(self.actor)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.settle()
        self.emit('begin_blockers')
        self.emit('block', 'ground', 'source', value=int(not require_air))
        if require_air:
            self.emit('block', 'air', 'source', value=1)
        if ignore is not None:
            self.emit('choose_dialog', player=self.actor, value=int(ignore))
        self.emit('advance_combat')
        self.emit('assert_unused_choices', value=0)
        self.observe('source', 'blocked', 1)
        self.emit('combat_damage')
        self.settle()
        if require_air:
            self.grave('air')
        elif ignore:
            self.life[self.foe] -= 6
            self.observe('ground', 'damage', 0)
        else:
            self.observe('ground', 'damage', 6)
        self.emit('end_combat')
        self.settle()

    def hecatomb(self):
        paid = self.mode != 'empty'
        for index in range(4 if paid else 0):
            self.add(f'fodder_{index}', self.actor, HOST, 'battlefield')
        if self.mode in ('swamp', 'wrong_swamp'):
            self.setup_land('land', 'Swamp' if self.mode == 'swamp' else 'Mountain', self.actor, tapped=True)
            self.setup_land('foe_land', 'Swamp', self.foe, tapped=True)
        self.paid('source')
        if paid and self.actor == 0:
            for index in range(4):
                self.choose(f'fodder_{index}')
        if not paid:
            self.settle()
            self.grave('source')
            return
        self.enter()
        for index in range(4):
            self.grave(f'fodder_{index}')
        if self.mode not in ('swamp', 'wrong_swamp'):
            return
        self.go(self.foe)
        self.go(self.actor)
        self.observe('land', 'tapped', 0)
        self.observe('foe_land', 'tapped', 0)
        allowed = self.mode == 'swamp'
        self.emit('can_activate', 'source', value=int(allowed))
        if allowed:
            self.choose('land')
            self.player_target(self.foe)
            self.emit('activate', 'source', value=1)
            self.emit('assert_stack', value=1)
            self.observe('land', 'tapped', 1)
            self.observe('foe_land', 'tapped', 0)
            self.emit('assert_life', player=self.foe, value=20)
            self.settle()
            self.life[self.foe] -= 1
            self.emit('can_activate', 'source', value=0)

    def dragon(self):
        land_names = {'typed_count': ('Mountain', PLATEAU, 'Forest'), 'own': ('Mountain',)}.get(self.mode, ())
        for index, land in enumerate(land_names):
            self.setup_land(f'land_{index}', land, self.actor, tapped=True)
        self.setup_land('foe_mountain', 'Mountain', self.foe, tapped=True)
        if self.mode == 'flying':
            self.add('ground', self.foe, WALL, 'battlefield')
            self.add('air', self.foe, AIR, 'battlefield')
            target = 'ground'
        else:
            own = self.mode == 'own'
            target = 'victim'
            self.add(target, self.actor if own else self.foe, HOST if own else WALL, 'battlefield')
        self.paid('source')
        self.choose(target)
        self.enter()
        self.source_stats()
        if self.mode == 'own':
            self.grave(target)
        else:
            self.observe(target, 'damage', 2 if self.mode == 'typed_count' else 0)
        if self.mode == 'flying':
            self.fight_with_blocker(require_air=True)

    def firebombers(self):
        if self.mode == 'mixed_types':
            lists = [('Island', 'Island', 'Island', TREE), ('Island',) * 4]
        else:
            counts = {'equal': (3, 3), 'below': (2, 3), 'both_choose': (5, 4)}[self.mode]
            lists = [('Island',) * n for n in counts]
        for seat, lands in enumerate(lists):
            for index, land in enumerate(lands):
                self.setup_land(f'land_{seat}_{index}', land, seat, tapped=True)
            self.add(f'nonland_{seat}', seat, ARTIFACT, 'battlefield')
        self.paid('source')
        below_floor = self.mode == 'below'
        # The captured negative-quantity picker can legally select either of the two live lands.
        # Correct rules offer neither choice and preserve both lands.
        for index in range(0 if below_floor else 3, len(lists[0])):
            self.emit('choose_target', f'land_0_{index}', player=0)
        self.enter(unoffered_targets=len(lists[0]) if below_floor else 0)
        self.source_stats()
        for seat, lands in enumerate(lists):
            for index in range(3, len(lands)):
                ref = f'land_{seat}_{index}'
                if seat == 0:
                    self.grave(ref)
                else:
                    # Identical Islands make either legal AI choice yield the same zone receipt.
                    self.cards[ref]['zone'] = 'graveyard'
            self.observe(f'nonland_{seat}', 'in_play', 1)

    def rishadan(self):
        amount = 1 if self.csvid == CUTPURSE else 2
        pays = self.mode == 'pay'
        has_fodder = self.mode != 'no_permanent'
        if has_fodder:
            self.add('victim', self.foe, MYR, 'battlefield')
        self.add('own', self.actor, ARTIFACT, 'battlefield')
        self.paid('source')
        reserve = (amount if pays else amount - 1, 0, 0, 0, 0, 0)
        self.mana('set_mana', self.foe, reserve)
        if has_fodder and not pays and self.foe == 0:
            self.emit('choose_target', 'victim', player=self.foe)
        self.enter()
        self.source_stats()
        if has_fodder and not pays:
            self.grave('victim')
        self.observe('own', 'in_play', 1)
        self.mana('assert_mana', self.foe, ZERO if pays else reserve)

    def yeti(self):
        victim = self.mode not in ('own_only',)
        activated = self.mode in ('snow_return', 'no_snow', 'offturn')
        self.add('own', self.actor, HOST, 'battlefield')
        if victim:
            self.add('victim', self.foe, GIANT if self.mode == 'mutual_death' else HOST, 'battlefield')
        if activated:
            self.setup_land('snow', SNOW if self.mode != 'no_snow' else 'Forest', self.actor, tapped=True)
            for index in range(2):
                self.setup_land(f'generic_{index}', 'Forest', self.actor, tapped=True)
        self.paid('source')
        if victim:
            self.choose('victim')
        if self.mode == 'mutual_death':
            self.settle()
            self.cards['source']['zone'] = 'battlefield'
            self.grave('source')
            self.grave('victim')
            return
        self.enter()
        self.source_stats()
        self.observe('own', 'damage', 0)
        self.observe('source', 'damage', int(victim))
        if victim:
            self.grave('victim')
        if not activated:
            return
        self.go(self.foe)
        self.go(self.actor)
        if self.mode == 'offturn':
            self.go(self.foe)
        self.observe('snow', 'tapped', 0)
        allowed = self.mode == 'snow_return'
        self.emit('can_activate', 'source', value=int(allowed))
        if allowed:
            funds = (2, 0, 0, 0, 0, 0)
            self.mana('set_mana', self.actor, funds)
            self.choose('snow')
            self.emit('activate', 'source', value=1)
            self.emit('assert_stack', value=1)
            self.observe('source', 'in_play', 1)
            self.observe('snow', 'tapped', 1)
            self.mana('assert_mana', self.actor, ZERO)
            self.settle()
            self.cards['source']['zone'] = 'hand'
            self.emit('bind_hand_copy', 'returned', player=self.actor, index=0, value=self.csvid)
            self.observe('returned', 'in_hand', 1)

    def add_echo_lands(self, names):
        lands = [f'echo_land_{index}' for index in range(len(names))]
        for ref, name in zip(lands, names):
            self.setup_land(ref, name, self.actor, tapped=True)
        return lands

    def echo_attempt(self, lands, *, pay, contingent_short=False):
        self.go(self.foe)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.actor)
        for ref in lands:
            self.observe(ref, 'tapped', 0)
        if pay or contingent_short:
            self.emit('choose_dialog', player=self.actor, value=int(not pay))
        if pay and self.actor == 0:
            for ref in lands:
                self.emit('choose_mana_source', ref)
        self.emit('upkeep_step', player=self.actor)
        self.emit('assert_unused_choices', value=int(contingent_short))
        self.emit('assert_stack', value=0)
        self.emit('turn', player=self.actor)
        self.active = self.actor
        if not pay:
            self.grave('source')
        else:
            self.observe('source', 'in_play', 1)
        for ref in lands:
            self.observe(ref, 'tapped', int(pay))
        self.mana('assert_mana', self.actor, ZERO)

    def shambler(self):
        self.add('ground', self.actor, WALL, 'battlefield')
        self.add('foe_ground', self.foe, WALL, 'battlefield')
        self.add('air', self.foe, AIR, 'battlefield')
        if self.mode.startswith('echo_'):
            generic = 3 if self.mode == 'echo_pay' else 1
            lands = self.add_echo_lands(('Forest',) * generic + ('Mountain',))
        effect = {'death': MURDER, 'bounce': BOUNCE}.get(self.mode)
        if effect:
            self.add('exit', self.actor, effect, 'hand')
        self.paid('source')
        self.enter()
        self.source_stats()
        for ref in ('source', 'ground', 'foe_ground'):
            self.observe(ref, 'damage', 1)
        self.observe('air', 'damage', 0)
        if effect:
            self.cast_to_grave('exit', target='source')
            if self.mode == 'death':
                self.grave('source')
            else:
                self.cards['source']['zone'] = 'hand'
                self.emit('bind_hand_copy', 'returned', player=self.actor, index=0, value=self.csvid)
                self.observe('returned', 'in_hand', 1)
            for ref in ('ground', 'foe_ground'):
                self.observe(ref, 'damage', 2)
            self.observe('air', 'damage', 0)
        elif self.mode.startswith('echo_'):
            pay = self.mode == 'echo_pay'
            self.echo_attempt(lands, pay=pay, contingent_short=not pay)
            for ref in ('ground', 'foe_ground'):
                self.observe(ref, 'damage', int(not pay))
            self.observe('air', 'damage', 0)

    def shaman(self):
        count = 2 if self.mode == 'two_giants' else 1
        if count == 2:
            self.add('own_giant', self.actor, GIANT, 'battlefield')
        self.add('foe_giant', self.foe, GIANT, 'battlefield')
        non_giant = HOST if self.mode == 'lethal' else WALL
        self.add('ground', self.actor, non_giant, 'battlefield')
        self.add('air', self.foe, AIR, 'battlefield')
        self.paid('source')
        self.enter()
        self.source_stats()
        self.observe('source', 'damage', 0)
        self.observe('foe_giant', 'damage', 0)
        if count == 2:
            self.observe('own_giant', 'damage', 0)
        if self.mode == 'lethal':
            self.grave('ground')
        else:
            self.observe('ground', 'damage', count)
        self.observe('air', 'damage', count)

    def tornado(self):
        if self.mode == 'fliers':
            self.add('own_air', self.actor, AIR, 'battlefield')
            self.add('foe_air', self.foe, SHIVAN, 'battlefield')
        self.add('ground', self.foe, WALL, 'battlefield')
        self.paid('source')
        self.enter()
        self.source_stats()
        self.observe('ground', 'damage', 0)
        if self.mode == 'fliers':
            self.grave('own_air')
            self.grave('foe_air')
        else:
            self.fight_with_blocker(ignore=self.mode == 'ignore_blocker')

    def hellion(self):
        protect = self.mode == 'cannot_prevent'
        self.add('victim', self.actor if protect else self.foe, WALL, 'battlefield')
        if protect:
            self.add('shield', self.actor, SHIELD, 'hand')
            self.cast_to_grave('shield')
        damage = 0 if self.mode == 'zero' else 3
        if self.mode.startswith('echo_'):
            fee = self.life[self.actor] - damage
            lands = self.add_echo_lands(('Forest',) * (fee - int(self.mode == 'echo_short')))
        self.paid('source')
        self.emit('choose_number', player=self.actor, value=damage)
        self.choose('victim')
        self.enter()
        self.source_stats()
        self.observe('victim', 'damage', damage)
        self.life[self.actor] -= damage
        self.emit('assert_life', player=self.actor, value=self.life[self.actor])
        if self.mode.startswith('echo_'):
            pay = self.mode == 'echo_pay'
            self.echo_attempt(lands, pay=pay)

    def ogre(self):
        if self.mode != 'counter':
            artifact = INGOT if self.mode == 'indestructible' else ARTIFACT
            self.add('artifact', self.actor if self.mode == 'own' else self.foe, artifact, 'battlefield')
        self.add('enchantment', self.foe, ENCHANTMENT, 'battlefield')
        self.paid('source')
        self.emit('choose_dialog', player=self.actor, value=int(self.mode != 'counter'))
        if self.mode != 'counter':
            self.choose('artifact')
        self.enter()
        if self.mode in ('artifact', 'own'):
            self.grave('artifact')
        bonus = int(self.mode == 'counter')
        self.source_stats(bonus)
        self.observe('source', 'plus1_counters', bonus)
        self.observe('enchantment', 'in_play', 1)

    def revolutionary(self):
        self.add('field', self.foe, FIELD, 'battlefield')
        self.add('foe_artifact', self.foe, ARTIFACT, 'battlefield')
        owns = self.mode != 'other_only'
        paid = owns and self.mode != 'decline'
        if owns:
            self.add('artifact', self.actor, INGOT if self.mode == 'indestructible' else ARTIFACT, 'battlefield')
        self.paid('source')
        if paid and self.actor == 0:
            self.choose('artifact')
        elif self.mode == 'decline':
            self.emit('choose_cancel_target', player=self.actor)
        self.enter()
        if paid:
            self.grave('artifact')
            self.life[self.foe] -= 2
        bonus = 2 if paid else 0
        self.source_stats(bonus)
        self.observe('source', 'plus1_counters', bonus)
        self.observe('foe_artifact', 'in_play', 1)

    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        actions = {HECATOMB: self.hecatomb, DRAGON: self.dragon, FIREBOMBERS: self.firebombers,
                   CUTPURSE: self.rishadan, FOOTPAD: self.rishadan, YETI: self.yeti,
                   SHAMBLER: self.shambler, SHAMAN: self.shaman, TORNADO: self.tornado,
                   HELLION: self.hellion, OGRE: self.ogre, REVOLUTIONARY: self.revolutionary}
        actions[self.csvid]()
        self.receipts()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name', ''))
    if (not pin or pin['registry']['csvid'] != csvid or csvid not in SOURCES
            or not exact(pin['oracle'], record) or record.get('layout') != 'normal' or record.get('card_faces')):
        raise ValueError('complete pinned entry rider required')
    printed = PRINTED.get(csvid)
    if printed and printed[1:] != (record['mana_cost'], record['type_line'], stats(record) if 'power' in record else None,
                                    tuple(record['keywords']), record['oracle_text']):
        raise ValueError('entry rider source text differs from pilot')
    return dict(csvid=csvid, printed_cost=record['mana_cost'], keywords=list(record['keywords']))


def variants(shape):
    csvid = shape['csvid']
    choices = (MODES if csvid in MODES else FOLLOWERS)[csvid]
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in choices if seat == 0 or (csvid, mode) not in HUMAN_ONLY]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct entry rider variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name', '')) if record else None
    if not pin:
        raise ValueError('complete pinned entry rider required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 entry rider recipe required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or {source['registry']['csvid'] for source in pins['sources'].values()} != set(SOURCES)
            or not pins['witnesses']):
        raise ValueError('complete entry rider source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry']) or row.get('kind') != 'card'
                or row.get('handler') in (None, '0', 'NULL', 'card_dummy') or oracle[name].get('layout') != 'normal'
                or oracle[name].get('card_faces')):
            raise ValueError('pinned entry rider card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered entry rider card required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        builder = Duel if csvid in MODES else RiderDuel
        board = builder(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        recipe = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                      oracle_text=declaration['oracle_text'])
        if csvid in GAPS:
            recipe['untested_components'] = GAPS[csvid]
        cases.append(dict(name=label(csvid, mode, seat), card=name, fixture='empty_main', seed=3308,
                          setup=board.setup, steps=board.steps, _recipe=recipe))
    return cases
