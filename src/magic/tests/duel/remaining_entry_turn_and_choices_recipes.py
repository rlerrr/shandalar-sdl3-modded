"""Paid entry effects involving player choices, attachments and turn scheduling."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from mill_death_and_entry_recipes import Duel as PaidBoard, stats
from recipes import cost_from_oracle


FAMILY = 'remaining_entry_turn_and_choices'
PROFILE = Path(__file__).with_name('data') / 'remaining_entry_turn_and_choices_wave8_profiles.json'
WURM, DRUID, EATER, FIEND = 3699, 10385, 6882, 4742
PRINTED = {
    WURM: ('urza_saga.c', '{3}{G}', 'Creature — Wurm', (6, 6), ('Trample',),
           "Trample\nWhen this creature enters, any player may sacrifice a land of their choice. "
           "If a player does, put this creature on top of its owner's library."),
    DRUID: ('m10.c', '{2}{G}', 'Creature — Human Druid', (1, 1), (),
            "When this creature enters, target Forest becomes a 4/5 green Treefolk creature for as long as "
            "this creature remains on the battlefield. It's still a land."),
    EATER: ('darksteel.c', '{4}', 'Artifact Creature — Leviathan', (9, 8), ('Flying', 'Trample'),
            'Flying, trample\nWhen this creature enters, you skip your next two turns.'),
    FIEND: ('nemesis.c', '{3}{B}', 'Creature — Phyrexian Horror Mercenary', (2, 2), (),
            'When this creature enters, each player loses 3 life.\n'
            '{3}, {T}: Search your library for a Mercenary permanent card with mana value 3 or less, '
            'put it onto the battlefield, then shuffle.'),
}
MODES = {
    WURM: ('owner_sacrifice', 'opponent_sacrifice', 'both_decline', 'no_land_combat', 'draw_recast'),
    DRUID: ('own_mana', 'foreign_nonbasic', 'attack', 'source_bounce', 'source_death', 'no_forest'),
    EATER: ('schedule', 'removed', 'countered', 'combat'),
    FIEND: ('sick', 'pay', 'alternate', 'decline', 'short', 'prevention'),
}
GROUND, AIR, SPACER = 'Dazzling Ramparts', 'Drift of Phantasms', 'Millstone'
COUNTER, KILL, BOUNCE, DRAW, SHIELD = 'Counterspell', 'Murder', 'Unsummon', 'Reach Through Mists', 'Safe Passage'
DUAL, MATCH, EARLY, COSTLY, ELSE = 'Tropical Island', 'Rathi Intimidator', 'Rampart Crawler', 'Cateran Kidnappers', 'Prodigal Sorcerer'
ZERO = (0,) * 6


def label(csvid, mode, actor):
    return f'RemainingEntryTurnAndChoicesRecipe_{csvid}_{mode}_{actor}'


class Duel(PaidBoard):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.active = actor
        self.library = {0: [], 1: []}
        self.shuffled = False
        self.add('source', actor, record['name'], 'hand')

    def add_land(self, ref, seat, name, *, tapped=True):
        self.add(ref, seat, name, 'battlefield')
        if tapped:
            self.setup[-1]['state'] = ['tapped']

    def seeds(self, names):
        for index, name in enumerate(names):
            self.add(f'library_{index}', self.actor, name, 'library')
        self.library[self.actor] = list(names)

    def pose(self, ref, power, toughness):
        self.observe(ref, 'in_play', 1)
        self.observe(ref, 'power', power)
        self.observe(ref, 'toughness', toughness)

    def spell_to_grave(self, ref, *, target=None):
        self.paid(ref, target=target)
        self.settle()
        self.cards[ref]['zone'] = 'graveyard'

    def grave(self, ref):
        self.cards[ref]['zone'] = 'graveyard'
        self.observe(ref, 'in_play', 0)

    def enter(self, ref='source'):
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        self.pose(ref, *stats(self.record))

    def trample(self, *, flying=False, excess=None):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.settle()
        self.emit('begin_blockers')
        if flying:
            self.emit('block', 'ground', 'source', value=0)
        self.emit('block', 'blocker', 'source', value=1)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 1)
        self.emit('combat_damage')
        self.settle()
        self.grave('blocker')
        self.life[self.foe] -= (4 if flying else 1) if excess is None else excess
        self.emit('end_combat')
        self.settle()

    def wurm(self):
        turn_recast = self.mode == 'draw_recast'
        sacrificer = (self.actor if self.mode == 'owner_sacrifice' else self.foe
                      if self.mode in ('opponent_sacrifice', 'draw_recast') else None)
        seen = []
        if self.mode != 'no_land_combat':
            for who, name in ((self.actor, 'Forest'), (self.foe, 'Island')):
                self.add_land(f'land_{who}', who, name)
                seen.append(who)
            if not turn_recast and sacrificer == 0:
                self.add_land('spare', 0, 'Plains')
        else:
            self.add('blocker', self.foe, AIR, 'battlefield')
        self.seeds(('Plains', 'Forest'))
        if turn_recast:
            self.add('draw_spell', self.actor, DRAW, 'hand')
        self.paid('source')
        for who in seen:
            self.emit('choose_dialog', player=who, value=int(who != sacrificer))
            if who == sacrificer:
                if who == 0:
                    self.emit('choose_target', f'land_{who}', player=who)
                break
        if sacrificer is None:
            self.enter()
            if self.mode == 'no_land_combat':
                self.go(self.foe)
                self.go(self.actor)
                self.trample(excess=1)
            return
        self.settle()
        self.cards['source']['zone'] = 'library'
        self.library[self.actor].insert(0, self.record['name'])
        self.observe('source', 'in_play', 0)
        self.grave(f'land_{sacrificer}')
        self.receipts()
        if turn_recast:
            self.spell_to_grave('draw_spell')
            assert self.library[self.actor].pop(0) == self.record['name']
            self.cards['source']['zone'] = 'hand'
            self.emit('bind_hand_copy', 'recast', player=self.actor, index=0, value=self.csvid)
            self.observe('recast', 'in_hand', 1)
            self.paid('source', rebound_ref='recast')
            # The opponent paid the first time and now has no land. The owner declines.
            self.emit('choose_dialog', player=self.actor, value=1)
            self.enter('recast')

    def animated(self, owner, ref, name):
        self.pose(ref, 4, 5)
        self.emit('can_target_creature', 'probe', ref, value=1)
        csvid = self.rows[name]['csvid']
        self.emit('assert_matching_permanents', player=owner, index=csvid, prop='color', value=8)
        self.emit('assert_matching_permanents', player=owner, index=csvid, prop='subtype_Treefolk', value=1)

    def druid(self):
        foreign = self.mode == 'foreign_nonbasic'
        no_forest = self.mode == 'no_forest'
        owner = self.foe if foreign else self.actor
        name = DUAL if foreign else 'Plains' if no_forest else 'Forest'
        self.add('probe', self.actor, SPACER, 'battlefield')
        self.add_land('land', owner, name)
        self.add_land('nonforest', self.foe, 'Mountain', tapped=False)
        mover = {'source_bounce': BOUNCE, 'source_death': KILL}.get(self.mode)
        if mover:
            self.add('removal', self.actor, mover, 'hand')
        self.paid('source')
        if not no_forest:
            self.emit('choose_target', 'land', player=self.actor)
        self.enter()
        self.emit('can_target_creature', 'probe', 'nonforest', value=0)
        if no_forest:
            self.emit('can_target_creature', 'probe', 'land', value=0)
            return
        self.animated(owner, 'land', name)
        if mover:
            self.spell_to_grave('removal', target='source')
            if mover == KILL:
                self.grave('source')
            else:
                self.cards['source']['zone'] = 'hand'
                self.emit('bind_hand_copy', 'bounced', player=self.actor, index=0, value=self.csvid)
                self.observe('bounced', 'in_hand', 1)
            self.observe('land', 'in_play', 1)
            self.emit('can_target_creature', 'probe', 'land', value=0)
            self.emit('assert_matching_permanents', player=owner, index=self.rows[name]['csvid'], prop='subtype_Treefolk', value=0)
        elif self.mode in ('own_mana', 'attack'):
            self.go(self.foe)
            self.go(self.actor)
            self.animated(owner, 'land', name)
            self.observe('land', 'tapped', 0)
            if self.mode == 'own_mana':
                self.emit('can_activate', 'land', value=1)
                self.emit('activate', 'land', value=1)
                self.emit('assert_stack', value=0)
                self.mana('assert_mana', self.actor, (0, 0, 0, 1, 0, 0))
                self.observe('land', 'tapped', 1)
            else:
                self.emit('begin_attackers')
                self.emit('attack', 'land', value=1)
                self.emit('finish_attackers')
                self.settle()
                self.emit('begin_blockers')
                self.emit('advance_combat')
                self.emit('combat_damage')
                self.settle()
                self.life[self.foe] -= 4
                self.emit('end_combat')
                self.settle()

    def eater(self):
        if self.mode == 'combat':
            self.add('ground', self.foe, GROUND, 'battlefield')
            self.add('blocker', self.foe, AIR, 'battlefield')
        elif self.mode == 'removed':
            self.add('removal', self.actor, KILL, 'hand')
        elif self.mode == 'countered':
            self.add('counter', self.foe, COUNTER, 'hand')
        self.paid('source')
        if self.mode == 'countered':
            self.emit('announce_spell', 'source')
            self.mana('set_mana', self.foe, cost_from_oracle(self.oracle[COUNTER]))
            self.emit('refresh')
            self.emit('cast_response', 'counter', value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=2)
            self.mana('assert_mana', self.foe, ZERO)
            self.emit('resolve_response')
            self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
            for seat in (self.foe, self.actor, self.foe, self.actor):
                self.emit('advance_scheduled_turn', value=seat)
            return
        self.enter()
        self.emit('assert_matching_permanents', player=self.actor, index=self.csvid, prop='artifact', value=1)
        if self.mode == 'removed':
            self.spell_to_grave('removal', target='source')
            self.grave('source')
        for seat in (self.foe, self.foe, self.foe, self.actor):
            self.emit('advance_scheduled_turn', value=seat)
        if self.mode == 'combat':
            self.emit('untap_step', player=self.actor)
            self.emit('upkeep_step', player=self.actor)
            # Begin combat in the actual active controller's first main phase after upkeep.
            self.emit('turn', player=self.actor)
            self.trample(flying=True)

    def fiend(self):
        tutoring = self.mode != 'prevention'
        allowed = self.mode in ('pay', 'alternate', 'decline')
        if tutoring:
            self.seeds((ELSE, MATCH, COSTLY, EARLY))
        else:
            self.add('shield', self.actor, SHIELD, 'hand')
            self.spell_to_grave('shield')
        self.paid('source')
        self.enter()
        self.life = [17, 17]
        self.receipts()
        if not tutoring:
            return
        if self.mode != 'sick':
            self.go(self.foe)
            self.go(self.actor)
        funds = (2 if self.mode == 'short' else 3, 0, 0, 0, 0, 0)
        self.mana('set_mana', self.actor, funds)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=int(allowed))
        if not allowed:
            self.mana('assert_mana', self.actor, funds)
            self.emit('assert_shuffle_count', player=self.actor, value=0)
            return
        self.emit('observe_search_filters', value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.observe('source', 'tapped', 1)
        name = EARLY if self.mode == 'alternate' else MATCH
        csvid = self.rows[name]['csvid']
        self.emit('choose_optional_zone' if self.mode == 'decline' else 'choose_zone_card', player=self.actor, value=csvid)
        self.settle()
        self.shuffled = True
        self.emit('assert_shuffle_count', player=self.actor, value=1)
        self.emit('assert_shuffle_count', player=self.foe, value=0)
        for name_ in self.library[self.actor]:
            self.emit('assert_offered_legal_card', player=self.actor, index=self.rows[name_]['csvid'],
                      value=int(name_ in (MATCH, EARLY)))
        if self.mode != 'decline':
            self.cards[f'library_{self.library[self.actor].index(name)}']['zone'] = 'battlefield'
            self.library[self.actor].remove(name)
            self.emit('bind_permanent_copy', 'fetched', player=self.actor, index=0, value=csvid)
            self.pose('fetched', *stats(self.oracle[name]))
            self.observe('fetched', 'tapped', 0)

    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        {WURM: self.wurm, DRUID: self.druid, EATER: self.eater, FIEND: self.fiend}[self.csvid]()
        index = len(self.steps)
        self.receipts()
        if self.shuffled:
            self.steps[index:] = [step for step in self.steps[index:] if step['op'] != 'assert_zone_card']


WUMPUS, DEVIL, NOGGLE, MELANCHOLY, THIRST, CLOAK = 8849, 8294, 9838, 8805, 2635, 5155
COURIER, REBEL, SPY, SPARK, KONG = 16308, 16772, 16773, 17860, 17884
SOURCES = {csvid: printed[0] for csvid, printed in PRINTED.items()} | {
    WUMPUS: 'urza_saga.c', DEVIL: 'dissension.c', NOGGLE: 'eventide.c',
    MELANCHOLY: 'planar_chaos.c', THIRST: 'mirage.c', CLOAK: 'conspiracy.c',
    COURIER: 'conspiracy-take-the-crown.c', REBEL: 'aether_revolt.c', SPY: 'aether_revolt.c',
    SPARK: 'unglued_unhinged.c', KONG: 'unglued_unhinged.c',
}
FOLLOWERS = {
    WUMPUS: MODES[WURM], DEVIL: ('black', 'red', 'paid_x', 'x_without_black', 'fear'),
    NOGGLE: ('dual_types', 'islands', 'mountains_walker', 'opponent_lands', 'decline'),
    MELANCHOLY: ('pay', 'short', 'decline'), THIRST: ('pay', 'short', 'decline'),
    CLOAK: ('landwalk', 'wrong_land', 'removed', 'islandwalk'),
    COURIER: ('monarch_draw', 'same_defender', 'lost'),
    REBEL: ('own_host', 'foe_only', 'leaves', 'control_loss'),
    SPY: ('own_host', 'foe_only', 'leaves', 'control_loss'),
    SPARK: ('entry_sacrifice', 'natural', 'point_loss', 'point_match', 'countered'),
    KONG: ('sweep', 'token', 'foe_apes', 'trample'),
}
GAPS = {
    COURIER: ['attack different defending players requires multiplayer; same-defender real combat is covered'],
    SPARK: ['the seat-one network dice path waits on a remote result unavailable in this duel driver'],
    NOGGLE: ['declining the damage trigger independently while accepting both tap targets'],
}
SNOW_ISLAND, VOLCANIC, RETURN_AURA = 'Snow-Covered Island', 'Volcanic Island', 'Boomerang'
SMALL, RELIC, DONATE, KIRD, WALKER = 'Prodigal Sorcerer', 'Darksteel Relic', 'Donate', 'Kird Ape', 'Jace, Memory Adept'
TOKEN, TOKENS = 'Ape', {'Ape': 18689}


def totals(seed):
    rolls = []
    for _ in range(8):
        seed = (seed ^ (seed << 13)) & 0xffffffff
        seed ^= seed >> 17
        seed = (seed ^ (seed << 5)) & 0xffffffff
        rolls.append((seed & 0x7fffffff) % 6 + 1)
    return tuple(a + b for a, b in zip(rolls[::2], rolls[1::2]))


def spark_seed(mode):
    if mode == 'countered':
        return 3500
    for seed in range(1, 10000):
        entry, next_roll, third, _ = totals(seed)
        point = entry not in (2, 3, 7, 11, 12)
        selected = {'entry_sacrifice': entry in (2, 3, 12), 'natural': entry in (7, 11) and next_roll == 7,
                    'point_loss': point and next_roll == 7, 'point_match': point and next_roll == entry and third == 7}
        if selected[mode]:
            return seed
    raise ValueError('no deterministic dice seed for ' + mode)


class ChoiceDuel(Duel):
    def entry_with_input_receipt(self, leftover=0):
        self.emit('resolve')
        self.emit('assert_unused_choices', value=leftover)
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards['source']['zone'] = 'battlefield'
        self.observe('source', 'in_play', 1)

    def counter_source(self):
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle[COUNTER]))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2)
        self.mana('assert_mana', self.foe, ZERO)
        self.emit('resolve_response')
        self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'

    def devil(self):
        black = self.mode in ('black', 'paid_x', 'fear')
        pays_x = self.mode in ('paid_x', 'x_without_black')
        if pays_x:
            self.add('recipient', self.actor, GROUND, 'battlefield')
        if self.mode == 'fear':
            self.add('invalid', self.foe, GROUND, 'battlefield')
            self.add('blocker', self.foe, RELIC, 'battlefield')
            self.add('artifact_creature', self.foe, 'Darksteel Myr', 'battlefield')
        self.mana('set_mana', self.actor, (int(not black), int(black), 0, 0, 1, 0))
        self.emit('refresh')
        if black and self.actor == 0:
            self.emit('choose_mana', player=self.actor, color=1)
        self.emit('cast', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        if pays_x:
            self.mana('set_mana', self.actor, (2, 0, 0, 0, 0, 0))
            self.emit('choose_x', player=self.actor, value=2)
            self.emit('choose_target', 'recipient', player=self.actor)
        if black:
            self.enter()
        else:
            self.settle()
            self.grave('source')
        if pays_x:
            self.mana('assert_mana', self.actor, ZERO)
            self.pose('recipient', 2, 7)
            if self.mode == 'paid_x' or self.actor == 1:
                # Retain the precise spent-pool assertion before the independent cleanup check.
                self.emit('set_mana', player=self.actor, color=0, value=0)
            self.go(self.foe)
            self.go(self.actor)
            self.pose('recipient', 0, 7)
        if self.mode == 'fear':
            self.go(self.foe)
            self.go(self.actor)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            self.emit('block', 'invalid', 'source', value=0)
            self.emit('block', 'artifact_creature', 'source', value=1)
            self.emit('advance_combat')
            self.observe('source', 'blocked', 1)
            self.emit('combat_damage')
            self.settle()
            self.observe('artifact_creature', 'in_play', 1)
            self.emit('end_combat')
            self.settle()

    def noggle(self):
        islands = self.mode in ('dual_types', 'islands', 'decline')
        mountains = self.mode in ('dual_types', 'mountains_walker')
        if islands or mountains or self.mode == 'opponent_lands':
            names = {'dual_types': (VOLCANIC, VOLCANIC), 'islands': ('Island', SNOW_ISLAND),
                     'decline': ('Island', SNOW_ISLAND), 'mountains_walker': ('Mountain', 'Mountain'),
                     'opponent_lands': (VOLCANIC, VOLCANIC)}[self.mode]
            for index, name in enumerate(names):
                self.add_land(f'land_{index}', self.foe if self.mode == 'opponent_lands' else self.actor, name)
        self.add('tapper1', self.actor, GROUND, 'battlefield')
        self.add('tapper2', self.foe, SPACER, 'battlefield')
        if self.mode == 'mountains_walker':
            self.add('walker', self.foe, WALKER, 'battlefield')
            self.emit('assert_counter', 'walker', counter_kind='loyalty', value=4)
        mana = (2, 0, int(self.actor == 0), 0, int(self.actor == 1), 0)
        self.mana('set_mana', self.actor, mana)
        self.emit('refresh')
        self.emit('cast', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        if self.actor == 0:
            self.emit('choose_trigger', 'source', player=self.actor)
        if islands:
            if self.mode == 'decline':
                self.emit('choose_optional_target', 'tapper1', player=self.actor)
            else:
                self.emit('choose_target', 'tapper1', player=self.actor)
                self.emit('choose_target', 'tapper2', player=self.actor)
        if mountains:
            if self.mode == 'mountains_walker':
                self.emit('choose_target', 'walker', player=self.actor)
            else:
                self.emit('choose_player', player=self.actor, value=self.foe)
        self.enter()
        for ref in ('tapper1', 'tapper2'):
            self.observe(ref, 'tapped', int(islands and self.mode != 'decline'))
        if self.mode == 'mountains_walker':
            self.emit('assert_counter', 'walker', counter_kind='loyalty', value=2)
        elif mountains:
            self.life[self.foe] -= 2

    def upkeep_aura(self):
        self.add('host', self.foe, GROUND, 'battlefield')
        plane = 'Swamp' if self.csvid == MELANCHOLY else 'Island'
        pay = self.mode == 'pay'
        funded = self.mode != 'short'
        self.add_land('fee', self.actor, plane if funded else ('Island' if plane == 'Swamp' else 'Swamp'))
        self.paid('source', target='host')
        self.entry_with_input_receipt()
        self.observe('host', 'tapped', 1)
        self.go(self.foe)
        self.observe('host', 'tapped', 1)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.actor)
        self.observe('fee', 'tapped', 0)
        if funded:
            self.emit('choose_dialog', player=self.actor, value=int(not pay))
        if pay and self.actor == 0:
            self.emit('choose_mana_source', 'fee')
        self.emit('upkeep_step', player=self.actor)
        self.emit('assert_unused_choices', value=0)
        self.observe('fee', 'tapped', int(pay))
        self.mana('assert_mana', self.actor, ZERO)
        if not pay:
            self.grave('source')
        self.emit('turn', player=self.actor)
        self.active = self.actor
        self.go(self.foe)
        self.observe('host', 'tapped', int(pay))

    def cloak(self):
        self.add('host', self.actor, SMALL, 'battlefield')
        self.add('blocker', self.foe, GROUND, 'battlefield')
        self.add_land('defender_land', self.foe, 'Island' if self.mode != 'landwalk' else 'Forest')
        self.seeds(('Mountain', 'Plains'))
        if self.mode == 'removed':
            self.add('removal', self.actor, RETURN_AURA, 'hand')
        self.paid('source', target='host')
        self.emit('choose_color', player=self.actor, value=2 if self.mode in ('islandwalk', 'removed') else 3)
        self.entry_with_input_receipt()
        self.library[self.actor].pop(0)
        self.cards['library_0']['zone'] = 'hand'
        self.receipts()
        if self.mode == 'removed':
            self.spell_to_grave('removal', target='source')
            self.cards['source']['zone'] = 'hand'
            self.emit('bind_hand_copy', 'returned', player=self.actor, index=0, value=self.csvid)
            self.observe('returned', 'in_hand', 1)
        allowed = self.mode in ('wrong_land', 'removed')
        self.emit('begin_attackers')
        self.emit('attack', 'host', value=1)
        self.emit('finish_attackers')
        self.settle()
        self.emit('begin_blockers')
        self.emit('block', 'blocker', 'host', value=int(allowed))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.settle()
        if allowed:
            self.observe('blocker', 'damage', 1)
        else:
            self.life[self.foe] -= 1
        self.emit('end_combat')
        self.settle()

    def monarch_draw(self, seat):
        ref = self.draw_refs[seat].pop(0)
        self.library[seat].pop(0)
        self.cards[ref]['zone'] = 'hand'

    def courier(self):
        self.draw_refs = {seat: [] for seat in (0, 1)}
        for seat in (0, 1):
            for index, name in enumerate(('Forest', 'Island')):
                ref = f'crown_{seat}_{index}'
                self.add(ref, seat, name, 'library')
                self.library[seat].append(name)
                self.draw_refs[seat].append(ref)
        if self.mode == 'same_defender':
            self.add('ally', self.actor, SMALL, 'battlefield')
            self.add('blocker', self.foe, GROUND, 'battlefield')
        elif self.mode == 'lost':
            self.add('opposing_attacker', self.foe, SMALL, 'battlefield')
        self.paid('source')
        self.enter()
        self.go(self.foe)
        self.monarch_draw(self.actor)
        self.receipts()
        if self.mode == 'monarch_draw':
            return
        if self.mode == 'same_defender':
            self.go(self.actor)
        attackers = ('source', 'ally') if self.mode == 'same_defender' else ('opposing_attacker',)
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref, value=1)
        self.emit('finish_attackers')
        self.settle()
        self.emit('begin_blockers')
        if self.mode == 'same_defender':
            self.emit('block', 'blocker', 'source', value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.settle()
        self.life[self.foe if self.mode == 'same_defender' else self.actor] -= 1
        self.emit('end_combat')
        self.settle()
        if self.mode == 'same_defender':
            self.observe('blocker', 'damage', 3)
        else:
            self.go(self.actor)
            self.monarch_draw(self.foe)

    def quicksmith(self):
        self.add('host', self.foe if self.mode == 'foe_only' else self.actor, SPACER, 'battlefield')
        if self.mode == 'leaves':
            self.add('removal', self.actor, KILL, 'hand')
        elif self.mode == 'control_loss':
            self.add('give', self.actor, DONATE, 'hand')
        if self.csvid == SPY:
            self.seeds(('Plains', 'Forest'))
        self.paid('source')
        self.emit('choose_target', 'host', player=self.actor)
        self.entry_with_input_receipt(int(self.mode == 'foe_only'))
        if self.mode == 'leaves':
            self.spell_to_grave('removal', target='source')
            self.grave('source')
        elif self.mode == 'control_loss':
            self.mana('set_mana', self.actor, cost_from_oracle(self.oracle[DONATE]))
            self.emit('refresh')
            self.emit('choose_player', player=self.actor, value=self.foe)
            self.emit('choose_target', 'source', player=self.actor)
            self.emit('cast', 'give', value=1)
            self.settle()
            self.cards['give']['zone'] = 'graveyard'
            self.cards['source']['player'] = self.foe
        allowed = self.mode == 'own_host'
        if self.mode == 'foe_only':
            # The strict bridge is same-controller only. Target eligibility is observed above.
            self.emit('can_activate', 'host', value=0)
        else:
            self.emit('can_activate_granted_legacy', 'host', index=self.csvid, value=int(allowed))
        if allowed:
            if self.csvid == REBEL:
                self.emit('choose_player', player=self.actor, value=self.foe)
            self.emit('activate_granted_legacy', 'host', index=self.csvid, value=1)
            self.emit('assert_stack', value=1)
            self.observe('host', 'tapped', 1)
            self.settle()
            self.emit('can_activate_granted_legacy', 'host', index=self.csvid, value=0)
            if self.csvid == REBEL:
                self.life[self.foe] -= 2
            else:
                self.library[self.actor].pop(0)
                self.cards['library_0']['zone'] = 'hand'

    def spark(self):
        if self.mode == 'countered':
            self.add('counter', self.foe, COUNTER, 'hand')
        self.paid('source')
        if self.mode == 'countered':
            self.counter_source()
            return
        if self.mode == 'entry_sacrifice':
            self.settle()
            self.grave('source')
            return
        self.enter()
        self.go(self.foe)
        self.go(self.actor)
        if self.mode == 'point_loss':
            self.grave('source')
            return
        self.observe('source', 'in_play', 1)
        self.go(self.foe)
        self.go(self.actor)
        self.observe('source', 'in_play', 1)

    def kong(self):
        if self.mode == 'sweep':
            for seat in (0, 1):
                self.add(f'artifact_{seat}', seat, SPACER, 'battlefield')
            self.add('indestructible', self.foe, RELIC, 'battlefield')
        elif self.mode == 'token':
            self.add('ape', self.actor, KIRD, 'battlefield')
        elif self.mode == 'foe_apes':
            self.add('ape', self.foe, KIRD, 'battlefield')
        elif self.mode == 'trample':
            self.add('blocker', self.foe, AIR, 'battlefield')
        self.paid('source')
        self.enter()
        if self.mode == 'sweep':
            for seat in (0, 1):
                self.grave(f'artifact_{seat}')
            self.observe('indestructible', 'in_play', 1)
        elif self.mode in ('token', 'foe_apes'):
            self.observe('source', 'tapped', 0)
            allowed = self.mode == 'token'
            self.emit('can_activate', 'source', value=int(allowed))
            if allowed:
                for ref in ('source', 'ape'):
                    self.emit('choose_target', ref, player=self.actor)
                self.emit('activate', 'source', value=1)
                self.emit('assert_stack', value=1)
                for ref in ('source', 'ape'):
                    self.observe(ref, 'tapped', 1)
                self.settle()
                self.cards['generated'] = dict(name=TOKEN, player=self.actor, zone='battlefield')
                for prop, value in (('token', 1), ('subtype_Ape', 1), ('color', 8), ('power', 1), ('toughness', 1)):
                    self.emit('assert_matching_permanents', player=self.actor, index=TOKENS[TOKEN], prop=prop, value=value)
        else:
            self.go(self.foe)
            self.go(self.actor)
            self.trample(excess=3)

    def build(self):
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        actions = {WUMPUS: self.wurm, DEVIL: self.devil, NOGGLE: self.noggle,
                   MELANCHOLY: self.upkeep_aura, THIRST: self.upkeep_aura, CLOAK: self.cloak,
                   COURIER: self.courier, REBEL: self.quicksmith, SPY: self.quicksmith,
                   SPARK: self.spark, KONG: self.kong}
        actions[self.csvid]()
        self.receipts()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name', ''))
    if (not pin or pin['registry']['csvid'] != csvid or csvid not in SOURCES
            or not exact(pin['oracle'], record) or record.get('layout') != 'normal' or record.get('card_faces')):
        raise ValueError('complete pinned remaining entry choice required')
    printed = PRINTED.get(csvid)
    if printed and printed[1:] != (record['mana_cost'], record['type_line'], stats(record) if 'power' in record else None,
                                    tuple(record['keywords']), record['oracle_text']):
        raise ValueError('remaining entry choice source text differs from pilot')
    return dict(csvid=csvid, printed_cost=record['mana_cost'], keywords=list(record['keywords']))


def variants(shape):
    csvid = shape['csvid']
    choices = (MODES if csvid in MODES else FOLLOWERS)[csvid]
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in choices
            if csvid != SPARK or seat == 0 or mode == 'countered']


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct remaining entry choice variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name', '')) if record else None
    if not pin:
        raise ValueError('complete pinned remaining entry choice required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 remaining entry choice recipe required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses', 'tokens'} or set(pins['tokens']) != set(TOKENS) or {source['registry']['csvid'] for source in pins['sources'].values()} != set(SOURCES)
            or not pins['witnesses']):
        raise ValueError('complete remaining entry choice source and witness pins required')
    for name, pin in pins['tokens'].items():
        if not exact(oracle.get(name), pin['oracle']) or not exact(rows.get(name), pin['registry']) or pin['registry']['csvid'] != TOKENS[name]:
            raise ValueError('pinned Ape token changed')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry']) or row.get('kind') != 'card'
                or row.get('handler') in (None, '0', 'NULL', 'card_dummy') or oracle[name].get('layout') != 'normal'
                or oracle[name].get('card_faces')):
            raise ValueError('pinned remaining entry choice card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered remaining entry choice card required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        builder = Duel if csvid in MODES else ChoiceDuel
        board = builder(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        recipe = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                      oracle_text=declaration['oracle_text'])
        if csvid in GAPS:
            recipe['untested_components'] = GAPS[csvid]
        seed = spark_seed(mode) if csvid == SPARK else 3500
        cases.append(dict(name=label(csvid, mode, seat), card=name, fixture='empty_main', seed=seed,
                          setup=board.setup, steps=board.steps, _recipe=recipe))
    return cases
