"""Paid destructive entry interactions and linked permanent effects."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from mill_death_and_entry_recipes import Duel as PaidBoard, stats


FAMILY = 'entry_interaction_destruction'
PROFILE = Path(__file__).with_name('data') / 'entry_interaction_destruction_wave8_profiles.json'
GRAVEN, WURM, PROWLER, MARE = 8063, 11476, 16705, 18979
PRINTED = {
    GRAVEN: ('guildpact.c', '{4}{W}{W}', 'Creature — Gargoyle', (4, 4), ('Flying', 'Haunt'),
             'Flying\nHaunt (When this creature dies, exile it haunting target creature.)\n'
             'When this creature enters or the creature it haunts dies, each other creature has base power and toughness 1/1 until end of turn.'),
    WURM: ('mirrodin_besieged.c', '{3}{B}{B}{B}', 'Creature — Phyrexian Wurm', (6, 5), (),
           'When this creature enters, creatures your opponents control get -2/-2 until end of turn.\n'
           'Whenever a creature an opponent controls dies, that player loses 2 life.'),
    PROWLER: ('aether_revolt.c', '{B}', 'Creature — Human Rogue', (1, 1), (),
              'When this creature enters, you may have target creature get -1/-1 until end of turn.'),
    MARE: ('m19.c', '{1}{B}{B}', 'Creature — Nightmare Horse', (2, 2), (),
           "This creature can't be blocked by white creatures.\n"
           'When this creature enters, creatures your opponents control get -1/-1 until end of turn.'),
}
MODES = {
    GRAVEN: ('scope', 'counter_layer', 'haunt_death', 'haunt_bounce', 'no_carrier', 'flying'),
    WURM: ('scope', 'expiry', 'later_foe', 'later_own', 'source_gone'),
    PROWLER: ('opposing', 'own', 'decline', 'self', 'expiry'),
    MARE: ('scope', 'expiry', 'colored_block', 'white_only'),
}
SMALL, WALL, AIR, DRAGON = 'Prodigal Sorcerer', 'Dazzling Ramparts', 'Drift of Phantasms', 'Shivan Dragon'
MYR, SHRUB, MULTI = 'Darksteel Myr', 'Nimble Mongoose', 'Qasali Pridemage'
SPACER, COUNTER, KILL, BOUNCE = 'Millstone', 'Burst of Strength', 'Murder', 'Unsummon'
ZERO = (0,) * 6


def label(csvid, mode, actor):
    return f'EntryInteractionDestructionRecipe_{csvid}_{mode}_{actor}'


class Duel(PaidBoard):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.active = actor
        self.library = {0: [], 1: []}
        self.add('source', actor, record['name'], 'hand')

    def enter(self, *, survives=True):
        self.settle()
        if survives:
            self.cards['source']['zone'] = 'battlefield'
            self.pose('source', *stats(self.record))
        else:
            self.grave('source')

    def choose(self, ref):
        self.emit('choose_target', ref, player=self.actor)

    def grave(self, ref):
        self.cards[ref]['zone'] = 'graveyard'
        self.observe(ref, 'in_play', 0)

    def pose(self, ref, power, toughness):
        self.observe(ref, 'in_play', 1)
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def move_spell(self, spell, target, *, after_cast=None):
        self.paid(spell, target=target)
        if after_cast:
            after_cast()
        self.settle()
        self.cards[spell]['zone'] = 'graveyard'

    def new_own_turn(self):
        self.go(self.foe)
        self.go(self.actor)

    def blocked_attack(self, probes, *, defender_damage, promised_life, source_damage=0):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.settle()
        self.emit('begin_blockers')
        for ref, allowed in probes:
            self.emit('block', ref, 'source', value=int(allowed))
        self.emit('advance_combat')
        self.observe('source', 'blocked', int(any(allowed for _, allowed in probes)))
        self.emit('combat_damage')
        self.settle()
        for ref, damage in defender_damage:
            self.observe(ref, 'damage', damage)
        if source_damage:
            self.observe('source', 'damage', source_damage)
        self.life[self.foe] -= promised_life
        self.emit('end_combat')
        self.settle()

    def graven(self):
        if self.mode == 'no_carrier':
            self.add('kill_source', self.actor, KILL, 'hand')
            self.paid('source')
            self.enter()
            self.move_spell('kill_source', 'source')
            self.grave('source')
            return
        # Occupy separate slots before the real 1/1 witnesses for each player.
        for seat in (0, 1):
            self.add(f'spacer_{seat}', seat, SPACER, 'battlefield')
            self.add(f'spacer_b_{seat}', seat, SPACER, 'battlefield')
        self.add('own', self.actor, WALL, 'battlefield')
        self.add('foe', self.foe, DRAGON, 'battlefield')
        if self.mode == 'flying':
            self.add('ground', self.foe, WALL, 'battlefield')
            self.add('air', self.foe, AIR, 'battlefield')
        if self.mode == 'counter_layer':
            self.add('counter_spell', self.actor, COUNTER, 'hand')
            self.move_spell('counter_spell', 'own')
            self.observe('own', 'plus1_counters', 1)
            self.pose('own', 1, 8)
        if self.mode in ('haunt_death', 'haunt_bounce'):
            self.add('kill_source', self.actor, KILL, 'hand')
            self.add('carrier', self.foe, SMALL, 'battlefield')
            self.add('exit_carrier', self.actor, KILL if self.mode == 'haunt_death' else BOUNCE, 'hand')
        self.paid('source')
        self.enter()
        boosted = self.mode == 'counter_layer'
        self.pose('own', 1 + boosted, 1 + boosted)
        self.pose('foe', 1, 1)
        if self.mode in ('scope', 'counter_layer', 'flying'):
            self.new_own_turn()
            self.pose('own', int(boosted), 7 + boosted)
            self.pose('foe', 5, 5)
            if self.mode == 'flying':
                self.blocked_attack((('ground', False), ('air', True)), defender_damage=(('air', 4),), promised_life=0)
        else:
            self.move_spell('kill_source', 'source', after_cast=lambda: self.choose('carrier'))
            self.cards['source']['zone'] = 'exile'
            self.observe('source', 'in_play', 0)
            self.receipts()
            self.new_own_turn()
            self.pose('own', 0, 7)
            self.pose('foe', 5, 5)
            self.move_spell('exit_carrier', 'carrier')
            if self.mode == 'haunt_death':
                self.grave('carrier')
                self.pose('own', 1, 1)
                self.pose('foe', 1, 1)
            else:
                self.cards['carrier']['zone'] = 'hand'
                self.observe('carrier', 'in_play', 0)
                self.pose('own', 0, 7)
                self.pose('foe', 5, 5)

    def wurm(self):
        self.add('own', self.actor, WALL, 'battlefield')
        self.add('foe', self.foe, DRAGON, 'battlefield')
        if self.mode == 'scope':
            self.add('ally_small', self.actor, SMALL, 'battlefield')
            self.add('foe_small', self.foe, SMALL, 'battlefield')
            self.add('indestructible', self.foe, MYR, 'battlefield')
        if self.mode in ('later_foe', 'later_own', 'source_gone'):
            self.add('removal', self.actor, KILL, 'hand')
            if self.mode == 'source_gone':
                self.add('second_removal', self.actor, KILL, 'hand')
        self.paid('source')
        self.emit('assert_life', player=self.foe, value=20)
        self.enter()
        self.pose('own', 0, 7)
        self.pose('foe', 3, 3)
        if self.mode == 'scope':
            self.pose('ally_small', 1, 1)
            self.grave('foe_small')
            self.grave('indestructible')
            self.life[self.foe] -= 4
        elif self.mode == 'expiry':
            self.new_own_turn()
            self.pose('foe', 5, 5)
        elif self.mode != 'scope':
            self.new_own_turn()
            self.pose('foe', 5, 5)
            if self.mode == 'source_gone':
                self.move_spell('removal', 'source')
                self.grave('source')
                self.move_spell('second_removal', 'foe')
                self.grave('foe')
            else:
                victim = 'foe' if self.mode == 'later_foe' else 'own'
                self.move_spell('removal', victim)
                self.grave(victim)
                if victim == 'foe':
                    self.life[self.foe] -= 2

    def prowler(self):
        own = self.mode == 'own'
        self_target = self.mode == 'self'
        large = self.mode in ('expiry', 'decline')
        self.add('witness', self.actor, WALL, 'battlefield')
        self.add('victim', self.actor if own else self.foe, SHRUB if self_target else DRAGON if large else SMALL, 'battlefield')
        self.paid('source')
        if self.mode == 'decline':
            self.emit('choose_optional_target', 'victim', player=self.actor)
        else:
            self.choose('source' if self_target else 'victim')
        self.enter(survives=not self_target)
        self.pose('witness', 0, 7)
        if self_target:
            self.pose('victim', 1, 1)
        elif self.mode == 'decline':
            self.pose('victim', 5, 5)
        elif self.mode == 'expiry':
            self.pose('victim', 4, 4)
            self.new_own_turn()
            self.pose('victim', 5, 5)
        else:
            self.grave('victim')

    def mare(self):
        self.add('own', self.actor, SMALL, 'battlefield')
        self.add('foe', self.foe, WALL, 'battlefield')
        combat = self.mode in ('colored_block', 'white_only')
        if self.mode == 'scope':
            self.add('weak', self.foe, MYR, 'battlefield')
        if combat:
            self.add('multicolor', self.foe, MULTI, 'battlefield')
        if self.mode == 'colored_block':
            self.add('legal_blocker', self.foe, AIR, 'battlefield')
        self.paid('source')
        self.enter()
        self.pose('own', 1, 1)
        self.pose('foe', -1, 6)
        if self.mode == 'scope':
            self.grave('weak')
        else:
            self.new_own_turn()
            self.pose('foe', 0, 7)
        if combat:
            allowed = self.mode == 'colored_block'
            self.pose('multicolor', 2, 2)
            probes = (('foe', False), ('multicolor', False))
            if allowed:
                probes += (('legal_blocker', True),)
            self.blocked_attack(probes, defender_damage=(('legal_blocker', 2),) if allowed else (),
                                promised_life=0 if allowed else 2)

    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        {GRAVEN: self.graven, WURM: self.wurm, PROWLER: self.prowler, MARE: self.mare}[self.csvid]()
        self.receipts()


CREMATOR, DOOM, SINGULAR, PYRO, LIMITED, SPIKE = 9502, 13617, 2701, 6157, 3507, 3567
SULEIMAN, TYRANT, HOODED, EYEBLIGHT, DEMON, WARFIRE = 2785, 12495, 14570, 15125, 16342, 17065
SOURCES = {csvid: printed[0] for csvid, printed in PRINTED.items()} | {
    CREMATOR: 'shadowmoor.c', DOOM: 'commander_2013.c', SINGULAR: 'visions.c', PYRO: 'onslaught.c',
    LIMITED: 'exodus.c', SPIKE: 'exodus.c', SULEIMAN: 'visions.c', TYRANT: 'avacyn_restored.c',
    HOODED: 'fate_reforged.c', EYEBLIGHT: 'origins.c', DEMON: 'kaladesh.c', WARFIRE: 'amonkhet.c',
}
FOLLOWERS = {
    CREMATOR: ('player', 'walker', 'noncreature', 'empty', 'random_equal_power'),
    DOOM: ('marked', 'short', 'same_mark', 'indestructible'),
    SINGULAR: ('initial_duplicates', 'later', 'basic_land', 'shield'),
    PYRO: ('both', 'source_removed', 'kindred'),
    LIMITED: ('over_five', 'threshold', 'below', 'removed'),
    SPIKE: ('alone', 'one_each', 'multiple'),
    SULEIMAN: ('both_types', 'later_djinn', 'later_efreet', 'shield'),
    TYRANT: ('lands', 'nonlands', 'hexproof', 'empty'),
    HOODED: ('counter', 'damaged_foe', 'damaged_ally', 'indestructible', 'after_cleanup'),
    EYEBLIGHT: ('opposing', 'owner_only', 'expiry'),
    DEMON: ('four_deaths', 'three_energy', 'own_grave', 'foe_grave', 'wrong_color', 'flying'),
    WARFIRE: ('mixed_types', 'foe_grave', 'own_only', 'lethal'),
}
GAPS = {
    CREMATOR: ['different-power random branches with exact surviving hand identity'],
    DOOM: ['direct doom counter inspection; paid destruction observes the two selected locations'],
    SINGULAR: ['name changes between entry and trigger resolution'],
    SPIKE: ['+1/+1 counters on a noncreature permanent'],
    TYRANT: ['mixed land-first versus nonland-first random identities; homogeneous lands and nonlands verify stop and continuation'],
    HOODED: ['direct observation of its damaged-target candidate list'],
    DEMON: ['reanimation ownership after a later control change'],
}
JACE, DJINN, EFREET = 'Jace, Memory Adept', 'Cloud Djinn', 'Serendib Efreet'
GOBLIN, KINDRED = 'Goblin Balloon Brigade', 'Boggart Shenanigans'
WRAP, NATURALIZE, SHOCK, SORCERY = 'Wrap in Vigor', 'Naturalize', 'Shock', 'Lava Spike'
HEXPROOF = 'Witchbane Orb'


class InteractionDuel(Duel):
    def entry(self, ref='source', *, survives=True, stats_expected=None):
        self.settle()
        if not survives:
            self.grave(ref)
            return
        self.cards[ref]['zone'] = 'battlefield'
        self.observe(ref, 'in_play', 1)
        if stats_expected is not None:
            self.pose(ref, *stats_expected)

    def tapped_land(self, ref, seat, name='Island'):
        self.add(ref, seat, name, 'battlefield')
        self.setup[-1]['state'] = ['tapped']

    def cremator(self):
        positive = self.mode in ('player', 'walker', 'random_equal_power')
        mixed = self.mode == 'random_equal_power'
        if self.mode != 'empty':
            self.add('discard', self.actor, DRAGON if positive else 'Forest', 'hand')
        if mixed:
            self.add('second', self.actor, DJINN, 'hand')
        if self.mode == 'walker':
            self.add('walker', self.foe, JACE, 'battlefield')
            self.emit('assert_counter', 'walker', counter_kind='loyalty', value=4)
        self.paid('source')
        if self.mode == 'walker':
            self.choose('walker')
        elif positive:
            self.emit('choose_player', player=self.actor, value=self.foe)
        self.enter()
        if self.mode != 'empty' and not mixed:
            self.cards['discard']['zone'] = 'graveyard'
        if self.mode == 'walker':
            self.observe('walker', 'in_play', 0)
            self.cards['walker']['zone'] = 'graveyard'
        elif positive:
            self.life[self.foe] -= 5

    def doom(self):
        self.add('own', self.actor, MYR if self.mode == 'indestructible' else SMALL, 'battlefield')
        self.add('foe', self.foe, SPACER, 'battlefield')
        self.add('unmarked', self.foe, WALL, 'battlefield')
        self.tapped_land('land', self.actor)
        self.paid('source')
        self.emit('choose_target', 'own', player=self.actor)
        self.emit('choose_target', 'own' if self.mode == 'same_mark' else 'foe', player=self.foe)
        self.entry()
        self.observe('own', 'in_play', 1)
        self.observe('foe', 'in_play', 1)
        reserve = (1 if self.mode == 'short' else 2, 0, 0, 0, 0, 0)
        self.mana('set_mana', self.actor, reserve)
        self.emit('refresh')
        allowed = self.mode != 'short'
        self.emit('can_activate', 'source', value=int(allowed))
        if allowed:
            self.emit('activate', 'source', value=1)
            self.emit('assert_stack', value=1)
            self.mana('assert_mana', self.actor, ZERO)
            self.grave('source')
            self.settle()
            if self.mode != 'indestructible':
                self.grave('own')
            if self.mode != 'same_mark':
                self.grave('foe')
        self.observe('unmarked', 'in_play', 1)
        self.observe('land', 'in_play', 1)

    def singularity(self):
        if self.mode == 'shield':
            self.add('shield', self.actor, WRAP, 'hand')
        if self.mode == 'basic_land':
            self.tapped_land('first', self.actor)
            self.tapped_land('second', self.foe)
            self.add('visitor', self.actor, 'Island', 'hand')
        else:
            duplicate = self.mode in ('initial_duplicates', 'shield')
            self.add('first', self.actor, SMALL, 'battlefield')
            if duplicate:
                self.add('second', self.foe, SMALL, 'battlefield')
            else:
                self.add('visitor', self.actor, SMALL, 'hand')
        self.add('unrelated', self.actor, WALL, 'battlefield')
        if self.mode == 'shield':
            self.move_spell('shield', None)
        self.paid('source')
        self.entry()
        if self.mode in ('initial_duplicates', 'shield'):
            self.grave('first')
            self.grave('second')
        elif self.mode == 'basic_land':
            self.emit('play_land', 'visitor', value=1)
            self.settle()
            self.cards['visitor']['zone'] = 'battlefield'
            for ref in ('first', 'second', 'visitor'):
                self.observe(ref, 'in_play', 1)
        else:
            self.paid('visitor')
            self.entry('visitor', stats_expected=(1, 1))
            self.grave('first')
        self.observe('unrelated', 'in_play', 1)

    def pyro(self):
        self.add('own', self.actor, GOBLIN, 'battlefield')
        self.add('unrelated', self.foe, SMALL, 'battlefield')
        if self.mode == 'kindred':
            self.add('kindred', self.foe, KINDRED, 'battlefield')
        else:
            self.add('foe', self.foe, GOBLIN, 'battlefield')
        if self.mode == 'source_removed':
            self.add('removal', self.actor, KILL, 'hand')
        self.paid('source')
        self.entry(stats_expected=(5, 2))
        self.pose('own', 4, 1)
        if self.mode != 'kindred':
            self.pose('foe', 4, 1)
        self.pose('unrelated', 1, 1)
        if self.mode == 'source_removed':
            self.move_spell('removal', 'source')
            self.grave('source')
            self.pose('own', 4, 1)
        self.go(self.foe)
        if self.mode == 'source_removed':
            self.pose('own', 1, 1)
            self.pose('foe', 1, 1)
        else:
            self.grave('source')
            self.grave('own')
            self.grave('kindred' if self.mode == 'kindred' else 'foe')
        self.pose('unrelated', 1, 1)

    def limited(self):
        trimmed = self.mode == 'over_five'
        counts = (6, 7) if trimmed else (4, 5) if self.mode == 'below' else (5, 5)
        for seat, count in enumerate(counts):
            for index in range(count):
                self.tapped_land(f'land_{seat}_{index}', seat)
        self.add('visitor', self.actor, 'Island', 'hand')
        if self.mode == 'removed':
            self.add('removal', self.actor, NATURALIZE, 'hand')
        self.paid('source')
        if trimmed:
            self.emit('choose_target', 'land_0_5', player=0)
        self.entry()
        if trimmed:
            self.grave('land_0_5')
            # The indistinguishable Islands have the same receipts for every legal AI choice.
            for index in (5, 6):
                self.cards[f'land_1_{index}']['zone'] = 'graveyard'
        allowed = self.mode == 'below'
        self.emit('play_land', 'visitor', value=int(allowed))
        if self.mode == 'removed':
            self.observe('visitor', 'in_hand', 1)
            self.move_spell('removal', 'source')
            self.grave('source')
            self.emit('play_land', 'visitor', value=1)
            allowed = True
        if allowed:
            self.cards['visitor']['zone'] = 'battlefield'
            self.observe('visitor', 'in_play', 1)
        else:
            self.observe('visitor', 'in_hand', 1)

    def spike(self):
        count = 0
        if self.mode != 'alone':
            self.add('own', self.actor, WALL, 'battlefield')
            self.add('foe', self.foe, DRAGON, 'battlefield')
            targets = ('own', 'foe') + (('own',) if self.mode == 'multiple' else ())
            for index, target in enumerate(targets):
                ref = f'counter_{index}'
                self.add(ref, self.actor, COUNTER, 'hand')
                self.move_spell(ref, target)
            count = len(targets)
            self.observe('own', 'plus1_counters', count - 1)
            self.observe('foe', 'plus1_counters', 1)
        self.paid('source')
        self.entry(stats_expected=(count + 1, count + 1))
        self.observe('source', 'plus1_counters', count + 1)
        if count:
            self.observe('own', 'plus1_counters', 0)
            self.observe('foe', 'plus1_counters', 0)
            self.pose('own', 0, 7)
            self.pose('foe', 5, 5)

    def suleiman(self):
        self.add('spared', self.foe, SMALL, 'battlefield')
        later = self.mode.startswith('later_')
        if later:
            self.add('visitor', self.actor, DJINN if self.mode == 'later_djinn' else EFREET, 'hand')
        else:
            self.add('djinn', self.actor, DJINN, 'battlefield')
            self.add('efreet', self.foe, EFREET, 'battlefield')
        if self.mode == 'shield':
            self.add('shield', self.actor, WRAP, 'hand')
            self.move_spell('shield', None)
        self.paid('source')
        self.entry()
        if later:
            self.paid('visitor')
            self.entry('visitor', survives=False)
        else:
            self.grave('djinn')
            self.grave('efreet')
        self.observe('spared', 'in_play', 1)

    def tyrant(self):
        self.add('own', self.actor, SPACER, 'battlefield')
        if self.mode == 'lands':
            for index in range(3):
                self.tapped_land(f'land_{index}', self.foe)
        elif self.mode == 'nonlands':
            self.add('artifact', self.foe, SPACER, 'battlefield')
            self.add('indestructible', self.foe, MYR, 'battlefield')
            self.add('creature', self.foe, SMALL, 'battlefield')
        elif self.mode == 'hexproof':
            self.add('protection', self.foe, HEXPROOF, 'battlefield')
            self.add('creature', self.foe, SMALL, 'battlefield')
        self.paid('source')
        # Two-player production has only one possible opponent; it performs the actual legality and RNG checks.
        self.enter()
        if self.mode == 'lands':
            self.cards['land_0']['zone'] = 'graveyard'
        elif self.mode == 'nonlands':
            for ref in ('artifact', 'indestructible', 'creature'):
                self.grave(ref)
        self.observe('own', 'in_play', 1)

    def hooded(self):
        marked = self.mode != 'counter'
        targeted = self.mode.startswith('damaged_') or self.mode == 'indestructible'
        victim = MYR if self.mode == 'indestructible' else WALL
        self.add('victim', self.actor if self.mode == 'damaged_ally' else self.foe, victim, 'battlefield')
        self.add('unmarked', self.foe, WALL, 'battlefield')
        if marked:
            self.add('damage_spell', self.actor, SHOCK, 'hand')
            self.move_spell('damage_spell', 'victim')
            self.observe('victim', 'in_play', 1)
            self.observe('victim', 'damage', 2)
            if self.mode == 'after_cleanup':
                self.new_own_turn()
                self.observe('victim', 'damage', 0)
        self.paid('source')
        self.emit('choose_dialog', player=self.actor, value=int(targeted))
        if targeted:
            self.choose('victim')
        self.entry(stats_expected=(1, 2) if targeted else (2, 3))
        self.observe('source', 'plus1_counters', int(not targeted))
        if targeted and self.mode != 'indestructible':
            self.grave('victim')
        else:
            self.observe('victim', 'in_play', 1)
        self.observe('unmarked', 'in_play', 1)

    def eyeblight(self):
        self.add('own', self.actor, SMALL, 'battlefield')
        has_target = self.mode != 'owner_only'
        if has_target:
            self.add('foe', self.foe, DRAGON if self.mode == 'expiry' else MYR, 'battlefield')
        self.paid('source')
        if has_target:
            self.choose('foe')
        self.enter()
        self.pose('own', 1, 1)
        if self.mode == 'opposing':
            self.grave('foe')
        elif self.mode == 'expiry':
            self.pose('foe', 4, 4)
            self.new_own_turn()
            self.pose('foe', 5, 5)

    def demon(self):
        if self.mode == 'flying':
            self.add('ground', self.foe, WALL, 'battlefield')
            self.add('air', self.foe, AIR, 'battlefield')
            self.paid('source')
            self.enter()
            self.new_own_turn()
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            self.emit('block', 'ground', 'source', value=0)
            self.emit('block', 'air', 'source', value=1)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.settle()
            self.grave('air')
            self.emit('assert_energy', player=self.actor, value=1)
            self.emit('end_combat')
            self.settle()
            return
        count = 3 if self.mode == 'three_energy' else 4
        dead = []
        for side, n in ((self.actor, 2), (self.foe, count - 2)):
            for slot in range(n):
                ref = f'creature_{side}_{slot}'
                self.add(ref, side, SMALL, 'battlefield')
                dead.append(ref)
        self.add('wall', self.actor, WALL, 'battlefield')
        self.add('foe_wall', self.foe, WALL, 'battlefield')
        self.paid('source')
        self.enter()
        self.pose('wall', -2, 5)
        self.pose('foe_wall', -2, 5)
        for ref in dead:
            self.grave(ref)
        self.emit('assert_energy', player=self.actor, value=count)
        self.emit('assert_energy', player=self.foe, value=0)
        if self.mode == 'four_deaths':
            return
        correct_mana = self.mode != 'wrong_color'
        reserve = (2, int(correct_mana), int(not correct_mana), 0, 0, 0)
        self.mana('set_mana', self.actor, reserve)
        self.emit('refresh')
        allowed = count == 4 and correct_mana
        self.emit('can_activate', 'source', value=int(allowed))
        if not allowed:
            self.mana('assert_mana', self.actor, reserve)
            return
        target_seat = self.foe if self.mode == 'foe_grave' else self.actor
        index = self.rows[SMALL]['csvid']
        self.emit('choose_player', player=self.actor, value=target_seat)
        self.emit('choose_zone_card', player=self.actor, value=index)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_energy', player=self.actor, value=0)
        self.mana('assert_mana', self.actor, ZERO)
        self.settle()
        self.cards[f'creature_{target_seat}_0'].update(player=self.actor, zone='battlefield')
        self.emit('bind_permanent_copy', 'returned', player=self.actor, index=0, value=index)
        self.pose('returned', 1, 1)
        self.observe('returned', 'tapped', 1)

    def warfire(self):
        self.add('own', self.actor, WALL, 'battlefield')
        target = self.mode != 'own_only'
        if target:
            self.add('foe', self.foe, SMALL if self.mode == 'lethal' else WALL, 'battlefield')
        seeds = {self.actor: (SMALL, 'Forest'), self.foe: (SHOCK, SHOCK, SORCERY)}
        if self.mode in ('mixed_types', 'own_only', 'lethal'):
            seeds[self.actor] += (SHOCK, SORCERY)
        for seat, names in seeds.items():
            for index, name in enumerate(names):
                self.add(f'grave_{seat}_{index}', seat, name, 'graveyard')
        self.paid('source')
        if target:
            self.choose('foe')
        self.enter()
        self.observe('own', 'damage', 0)
        if self.mode == 'lethal':
            self.grave('foe')
        elif target:
            self.observe('foe', 'damage', 2 if self.mode == 'mixed_types' else 0)

    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        actions = {CREMATOR: self.cremator, DOOM: self.doom, SINGULAR: self.singularity, PYRO: self.pyro,
                   LIMITED: self.limited, SPIKE: self.spike, SULEIMAN: self.suleiman, TYRANT: self.tyrant,
                   HOODED: self.hooded, EYEBLIGHT: self.eyeblight, DEMON: self.demon, WARFIRE: self.warfire}
        actions[self.csvid]()
        if self.csvid == CREMATOR and self.mode == 'random_equal_power':
            # Different five-power cards produce the same Oracle result regardless of the random choice.
            self.emit('assert_stack', value=0)
            for seat, counts in ((self.actor, (1, 1, 0, 1, 0)), (self.foe, (0, 0, 0, 0, 0))):
                self.emit('assert_life', player=seat, value=self.life[seat])
                for zone, count in zip(('hand', 'battlefield', 'library', 'graveyard', 'exile'), counts):
                    self.emit('assert_zone', player=seat, zone=zone, value=count)
        else:
            self.receipts()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name', ''))
    if (not pin or pin['registry']['csvid'] != csvid or csvid not in SOURCES
            or not exact(pin['oracle'], record) or record.get('layout') != 'normal' or record.get('card_faces')):
        raise ValueError('complete pinned entry interaction required')
    printed = PRINTED.get(csvid)
    if printed and printed[1:] != (record['mana_cost'], record['type_line'], stats(record) if 'power' in record else None,
                                    tuple(record['keywords']), record['oracle_text']):
        raise ValueError('entry interaction source text differs from pilot')
    return dict(csvid=csvid, printed_cost=record['mana_cost'], keywords=list(record['keywords']))


def variants(shape):
    csvid = shape['csvid']
    choices = (MODES if csvid in MODES else FOLLOWERS)[csvid]
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in choices]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct entry interaction variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name', '')) if record else None
    if not pin:
        raise ValueError('complete pinned entry interaction required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 entry interaction recipe required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or {source['registry']['csvid'] for source in pins['sources'].values()} != set(SOURCES)
            or not pins['witnesses']):
        raise ValueError('complete entry interaction source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry']) or row.get('kind') != 'card'
                or row.get('handler') in (None, '0', 'NULL', 'card_dummy') or oracle[name].get('layout') != 'normal'
                or oracle[name].get('card_faces')):
            raise ValueError('pinned entry interaction card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered entry interaction card required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        builder = Duel if csvid in MODES else InteractionDuel
        board = builder(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        recipe = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                      oracle_text=declaration['oracle_text'])
        if csvid in GAPS:
            recipe['untested_components'] = GAPS[csvid]
        cases.append(dict(name=label(csvid, mode, seat), card=name, fixture='empty_main', seed=3404,
                          setup=board.setup, steps=board.steps, _recipe=recipe))
    return cases
