"""Authentic destructive entry, permanent choices, and graveyard riders."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from mill_death_and_entry_recipes import Duel as PaidBoard, stats
from recipes import cost_from_oracle


FAMILY = 'entry_follow_on_destruction'
PROFILE = Path(__file__).with_name('data') / 'entry_follow_on_destruction_wave8_profiles.json'
PREFIX = 'EntryFollowOnDestructionRecipe_'
SPINE, BANE, DEVIL, TRIAL = 11530, 13607, 12503, 17050
PRINTED = {
    SPINE: ('mirrodin_besieged.c', '{7}', 'Artifact', None, (),
            'When this artifact enters, destroy target permanent.\n'
            "When this artifact is put into a graveyard from the battlefield, return it to its owner's hand."),
    BANE: ('commander_2013.c', '{4}{G}{G}', 'Creature — Elemental', (2, 2), (),
           'When this creature enters, destroy all artifacts and enchantments. Put a +1/+1 counter on this creature '
           'for each permanent destroyed this way.'),
    DEVIL: ('avacyn_restored.c', '{R}', 'Creature — Devil', (4, 3), (),
            'When this creature enters, any opponent may have it deal 4 damage to them. '
            'If a player does, sacrifice this creature.'),
    TRIAL: ('amonkhet.c', '{1}{B}', 'Enchantment', None, (),
            'When this enchantment enters, target opponent sacrifices a creature of their choice.\n'
            "When a Cartouche you control enters, return this enchantment to its owner's hand."),
}
MODES = {
    SPINE: ('foe_land', 'self', 'exile', 'owner_return', 'recast'),
    BANE: ('mixed', 'dual_type', 'indestructible', 'untouched', 'artifact_land'),
    DEVIL: ('take', 'decline', 'take_low', 'prevented', 'real_attack'),
    TRIAL: ('choose', 'indestructible', 'no_creatures', 'own_cartouche', 'foe_cartouche', 'owner_return'),
}
HOST, WALL = 'Prodigal Sorcerer', 'Dazzling Ramparts'
ARTIFACT, ENCHANTMENT, WHIP = 'Millstone', 'Fervor', 'Whip of Erebos'
MYR, CITADEL, ARTIFACT_LAND = 'Darksteel Myr', 'Darksteel Citadel', 'Tree of Tales'
OFFER, BREAK, EXILE = 'Harmless Offering', 'Disenchant', 'Revoke Existence'
FIELD, CARTOUCHE = 'Energy Field', 'Cartouche of Knowledge'
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

    def cast_to(self, ref, zone, target=None, rebound_ref=None):
        self.paid(ref, target=target, rebound_ref=rebound_ref)
        self.settle()
        self.cards[ref]['zone'] = zone

    def target(self, seat, ref):
        self.emit('choose_target', ref, player=seat)

    def grave(self, refs):
        for ref in refs:
            self.cards[ref]['zone'] = 'graveyard'
            self.observe(ref, 'in_play', 0)

    def bind(self, ref, who, name, zone):
        self.emit('bind_permanent_copy' if zone == 'battlefield' else 'bind_hand_copy', ref, player=who,
                  index=0, value=self.rows[name]['csvid'])

    def give(self):
        self.cast_to('give', 'graveyard', 'source')
        self.cards['source']['player'] = self.foe
        self.bind('foreign', self.foe, self.record['name'], 'battlefield')
        self.observe('foreign', 'in_play', 1)
        self.receipts()

    def spine(self):
        self.add('spare', self.actor, WALL, 'battlefield')
        self_destroyed = self.mode in ('self', 'recast')
        if not self_destroyed:
            self.add('foe_land', self.foe, 'Island', 'battlefield')
        if self.mode == 'recast':
            self.add('recast_target', self.foe, HOST, 'battlefield')
        if self.mode in ('exile', 'owner_return'):
            self.add('exit', self.actor, EXILE if self.mode == 'exile' else BREAK, 'hand')
        if self.mode == 'owner_return':
            self.add('give', self.actor, OFFER, 'hand')
        self.paid('source')
        if not self_destroyed:
            self.observe('foe_land', 'in_play', 1)
        self.target(self.actor, 'source' if self_destroyed else 'foe_land')
        self.settle()
        if self_destroyed:
            self.cards['source']['zone'] = 'hand'
            self.bind('returned', self.actor, self.record['name'], 'hand')
            self.observe('returned', 'in_hand', 1)
        else:
            self.cards['source']['zone'] = 'battlefield'
            self.grave(('foe_land',))
        self.receipts()
        if self.mode == 'recast':
            self.paid('source', rebound_ref='returned')
            self.target(self.actor, 'recast_target')
            self.settle()
            self.cards['source']['zone'] = 'battlefield'
            self.grave(('recast_target',))
        elif self.mode == 'exile':
            self.cast_to('exit', 'graveyard', 'source')
            self.cards['source']['zone'] = 'exile'
        elif self.mode == 'owner_return':
            self.give()
            self.cast_to('exit', 'graveyard', 'foreign')
            self.cards['source'].update(player=self.actor, zone='hand')
            self.bind('returned', self.actor, self.record['name'], 'hand')
            self.observe('returned', 'in_hand', 1)

    def bane(self):
        self.add('creature', self.actor, HOST, 'battlefield')
        self.add('plain_land', self.foe, 'Island', 'battlefield')
        doomed = []
        if self.mode in ('mixed', 'indestructible'):
            self.add('own_artifact', self.actor, ARTIFACT, 'battlefield')
            self.add('foe_enchantment', self.foe, ENCHANTMENT, 'battlefield')
            doomed = ['own_artifact', 'foe_enchantment']
        elif self.mode == 'dual_type':
            self.add('whip', self.foe, WHIP, 'battlefield')
            doomed = ['whip']
        elif self.mode == 'artifact_land':
            self.add('artifact_land', self.foe, ARTIFACT_LAND, 'battlefield')
            doomed = ['artifact_land']
        if self.mode == 'indestructible':
            self.add('steel_creature', self.actor, MYR, 'battlefield')
            self.add('steel_land', self.foe, CITADEL, 'battlefield')
        self.paid('source')
        for ref in doomed:
            self.observe(ref, 'in_play', 1)
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        self.grave(doomed)
        if self.mode == 'indestructible':
            for ref in ('steel_creature', 'steel_land'):
                self.observe(ref, 'in_play', 1)
        self.observe('source', 'plus1_counters', len(doomed))
        self.observe('source', 'power', 2 + len(doomed))
        self.observe('source', 'toughness', 2 + len(doomed))

    def devil(self):
        if self.mode == 'take_low':
            self.life[self.foe] = 6
            self.emit('set_life', player=self.foe, value=6)
        if self.mode == 'prevented':
            self.add('field', self.foe, FIELD, 'battlefield')
        take = self.mode in ('take', 'take_low', 'prevented')
        self.paid('source')
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
        self.emit('choose_dialog', player=self.foe, value=int(not take))
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        if take:
            self.grave(('source',))
            self.life[self.foe] -= 0 if self.mode == 'prevented' else 4
        else:
            self.observe('source', 'power', 4)
            self.observe('source', 'toughness', 3)
        self.receipts()
        if self.mode == 'real_attack':
            self.go(self.foe)
            self.go(self.actor)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.settle()
            self.life[self.foe] -= 4
            self.emit('end_combat')
            self.settle()

    def trial(self):
        cartouche = self.mode in ('own_cartouche', 'foe_cartouche', 'owner_return')
        who = self.actor if self.mode == 'own_cartouche' else self.foe
        victim = self.mode != 'no_creatures'
        self.add('own_creature', self.actor, HOST, 'battlefield')
        if victim:
            self.add('victim', self.foe, MYR if self.mode == 'indestructible' else HOST, 'battlefield')
        if self.mode == 'choose' and self.foe == 0:
            self.add('choice', self.foe, WALL, 'battlefield')
        if cartouche:
            self.add('cartouche', who, CARTOUCHE, 'hand')
            self.library[who] = ['Mountain', 'Island']
            self.add('drawn', who, 'Mountain', 'library')
            self.add('remaining', who, 'Island', 'library')
            if who == self.foe:
                self.add('cartouche_host', self.foe, WALL, 'hand')
        if self.mode == 'owner_return':
            self.add('give', self.actor, OFFER, 'hand')
        self.paid('source')
        if victim and self.foe == 0:
            self.target(self.foe, 'victim')
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        if victim:
            self.grave(('victim',))
        self.observe('own_creature', 'in_play', 1)
        self.receipts()
        if not cartouche:
            return
        if self.mode == 'owner_return':
            self.give()
        if self.active != who:
            self.go(who)
        host = 'own_creature' if who == self.actor else 'cartouche_host'
        if who == self.foe:
            self.cast_to(host, 'battlefield')
        self.cast_to('cartouche', 'battlefield', host)
        self.cards['drawn']['zone'] = 'hand'
        assert self.library[who].pop(0) == 'Mountain'
        base_power, base_toughness = stats(self.oracle[self.cards[host]['name']])
        self.observe(host, 'power', base_power + 1)
        self.observe(host, 'toughness', base_toughness + 1)
        if self.mode != 'foe_cartouche':
            self.cards['source'].update(player=self.actor, zone='hand')
            self.bind('returned', self.actor, self.record['name'], 'hand')
            self.observe('returned', 'in_hand', 1)
        else:
            self.observe('source', 'in_play', 1)

    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        {SPINE: self.spine, BANE: self.bane, DEVIL: self.devil, TRIAL: self.trial}[self.csvid]()
        self.receipts()


PRINTED.update({9773: ('eventide.c',
        '{2}{R/W}',
        'Creature — Dwarf Shaman',
        (2, 2),
        (),
        'When this creature enters, if you control two or more Mountains, you may destroy target artifact.\n'
        'When this creature enters, if you control two or more Plains, you may destroy target enchantment.'),
 14480: ('commander_2014.c',
         '{5}{B}{B}',
         'Creature — Demon',
         (5, 5),
         ('Flying',),
         'Flying\n'
         'When this creature enters, you may destroy target creature.\n'
         'Whenever a nontoken creature an opponent controls dies, create a tapped 2/2 black Zombie creature '
         'token.'),
 13336: ('m14.c',
         '{3}{B}{B}',
         'Creature — Demon',
         (5, 6),
         ('Flying',),
         'Flying\n'
         'When this creature enters, destroy target non-Demon creature.\n'
         'At the beginning of your upkeep, if there are fewer than six creature cards in your graveyard, '
         'sacrifice a creature.'),
 13803: ('born_of_the_gods.c',
         '{3}{B}{B}',
         'Creature — Harpy',
         (2, 2),
         ('Flying', 'Tribute'),
         'Flying\n'
         'Tribute 2 (As this creature enters, an opponent of your choice may put two +1/+1 counters on '
         'it.)\n'
         "When this creature enters, if tribute wasn't paid, target opponent sacrifices a creature of their "
         'choice.'),
 14880: ('dragons_of_tarkir.c',
         '{5}{B}{B}',
         'Creature — Dragon',
         (5, 6),
         ('Flying',),
         'Flying\n'
         'When this creature enters, if you cast it from your hand and there are five or more other '
         'creatures on the battlefield, destroy all other creatures.'),
 16311: ('kaladesh.c',
         '{3}{W}{W}',
         'Artifact Creature — Construct',
         (4, 5),
         ('Vigilance',),
         'Vigilance\n'
         'When this creature enters, each player chooses an artifact, a creature, an enchantment, and a '
         'planeswalker from among the nonland permanents they control, then sacrifices the rest.'),
 16465: ('kaladesh.c',
         '{4}{B}{B}',
         'Artifact Creature — Construct',
         (5, 4),
         ('Menace',),
         'Menace\n'
         'When this creature enters, you may destroy another target creature. If a creature is destroyed '
         'this way, you gain life equal to its toughness.'),
 19458: ('guilds_of_ravnica.c',
         '{2}{B}',
         'Creature — Human Shaman',
         (3, 2),
         (),
         'When this creature enters, each player sacrifices a creature or planeswalker of their choice. '
         "Each player who can't discards a card."),
 10847: ('worldwake.c',
         '{6}{G}{G}',
         'Creature — Elephant',
         (9, 9),
         (),
         'When this creature enters, you may destroy up to three target noncreature permanents. For each '
         'permanent put into a graveyard this way, its controller creates a 3/3 green Elephant creature '
         'token.'),
 11732: ('commander.c',
         '{7}{B}{B}{B}',
         'Creature — Demon',
         (8, 8),
         (),
         'When this creature enters, if you cast it from your hand, destroy all creatures your opponents '
         'control, then tap all other creatures you control.'),
 6768: ('mirrodin.c',
        '{4}{B}{B}{B}{B}',
        'Creature — Demon',
        (6, 6),
        ('Flying',),
        'Flying\n'
        'When this creature enters, if you cast it from your hand, destroy all nonartifact, nonblack '
        "creatures. They can't be regenerated."),
 4574: ('mercadian_masques.c',
        '{4}{U}',
        'Creature — Human Pirate',
        (3, 2),
        ('Flying',),
        'Flying\n'
        'When this creature enters, each opponent sacrifices a permanent of their choice unless they pay '
        '{3}.\n'
        'This creature can block only creatures with flying.')})

HEDGE, OVERSEER, SHADOWBORN, SHRIKE, REGENT, CATACLYSM = 9773, 14480, 13336, 13803, 14880, 16311
NOXIOUS, PLAGUE, TERASTODON, CACODEMON, REIVER, BRIGAND = 16465, 19458, 10847, 11732, 6768, 4574
FOLLOWERS = {
    HEDGE: ('both', 'mountains', 'plains', 'other_lands', 'typed_lands', 'decline'),
    OVERSEER: ('foe_creature', 'own_creature', 'opponent_token', 'later_death', 'flying'),
    SHADOWBORN: ('non_demon', 'demons_only', 'five_upkeep', 'six_upkeep', 'flying'),
    SHRIKE: ('paid', 'unpaid', 'empty_foe', 'flying'),
    REGENT: ('five', 'four', 'indestructible', 'reanimated', 'flying'),
    CATACLYSM: ('all_types', 'dual_pick', 'vigilance'),
    NOXIOUS: ('foe_creature', 'own_creature', 'indestructible', 'decline', 'no_other', 'menace'),
    PLAGUE: ('both_creatures', 'own_planeswalker', 'foe_planeswalker', 'foe_discards', 'self_only'),
    TERASTODON: ('three', 'decline', 'indestructible', 'one_each'),
    CACODEMON: ('from_hand', 'indestructible', 'reanimated'),
    REIVER: ('mixed', 'no_regen', 'reanimated', 'flying'),
    BRIGAND: ('paid', 'short', 'foe_empty', 'blocking'),
}
HUMAN_ONLY = {(CATACLYSM, 'all_types'), (PLAGUE, 'own_planeswalker')}
PLATEAU, AIR, DRAGON = 'Plateau', 'Drift of Phantasms', 'Shivan Dragon'
KILL, RAISE, DEMON = 'Murder', 'Raise the Alarm', "Demon of Death's Gate"
SQUEE, REANIMATE, PLANESWALKER = 'Squee, Goblin Nabob', 'Zombify', 'Jace Beleren'
UPKEEP_FODDER = 'Anger'
SKELETON, GNOMES, TROLL = 'Drudge Skeletons', 'Bottle Gnomes', 'Albino Troll'
TOKENS = {'Zombie': 18860, 'Soldier': 18822, 'Elephant': 18729}
WITNESSES = {HOST, WALL, ARTIFACT, ENCHANTMENT, WHIP, MYR, CITADEL, ARTIFACT_LAND, OFFER, BREAK, EXILE, FIELD,
             CARTOUCHE, PLATEAU, AIR, DRAGON, KILL, RAISE, DEMON, SQUEE, REANIMATE, PLANESWALKER,
             SKELETON, GNOMES, TROLL, UPKEEP_FODDER, 'Island', 'Mountain', 'Plains', 'Forest'}
GAPS = {
    SHRIKE: ['choice among multiple opponents in multiplayer'],
    CATACLYSM: ['explicit multi-category choices for both boards are exercised with the active human seat'],
    BRIGAND: ['affirmative refusal of an affordable automatic mana payment'],
}


class FollowDuel(Duel):
    def build(self):
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        if self.mode in ('reanimated', 'flying') and self.csvid in (REGENT, CACODEMON, REIVER):
            self.setup[0]['zone'] = self.cards['source']['zone'] = 'graveyard'
        actions = {HEDGE: self.hedge, OVERSEER: self.overseer, SHADOWBORN: self.shadowborn, SHRIKE: self.shrike,
                   REGENT: self.cast_origin, CATACLYSM: self.cataclysm, NOXIOUS: self.noxious, PLAGUE: self.plague,
                   TERASTODON: self.terastodon, CACODEMON: self.cast_origin, REIVER: self.cast_origin, BRIGAND: self.brigand}
        actions[self.csvid]()
        self.receipts()

    def entry(self, deaths=(), *, source_ref='source', observe_stats=True):
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        self.grave(deaths)
        if observe_stats:
            self.source_stats(source_ref)

    def source_stats(self, ref='source', bonus=0):
        power, toughness = stats(self.record)
        self.observe(ref, 'in_play', 1)
        self.observe(ref, 'power', power + bonus)
        self.observe(ref, 'toughness', toughness + bonus)

    def tokens(self, name, seat, count, **properties):
        token_id = TOKENS[name]
        self.emit('assert_zone_multiplicity', player=seat, zone='battlefield', index=token_id, value=count)
        for index in range(count):
            self.cards[f'{name}_{seat}_{index}'] = dict(name=name, player=seat, zone='battlefield')
        if count:
            for prop, value in properties.items():
                self.emit('assert_matching_permanents', player=seat, index=token_id, prop=prop, value=value)

    def flight_witnesses(self):
        self.add('ground', self.foe, WALL, 'battlefield')
        self.add('air', self.foe, AIR, 'battlefield')

    def flight(self, source_ref='source', bonus=0):
        power = stats(self.record)[0] + bonus
        self.go(self.foe)
        self.go(self.actor)
        self.emit('begin_attackers')
        self.emit('attack', source_ref, value=1)
        self.emit('finish_attackers')
        self.settle()
        self.emit('begin_blockers')
        self.emit('block', 'ground', source_ref, value=0)
        self.emit('block', 'air', source_ref, value=1)
        self.emit('advance_combat')
        self.observe(source_ref, 'blocked', 1)
        self.emit('combat_damage')
        self.settle()
        if power >= 5:
            self.grave(('air',))
        else:
            self.observe('air', 'damage', power)
        self.emit('end_combat')
        self.settle()

    def hedge(self):
        mountains = self.mode in ('both', 'mountains', 'typed_lands', 'decline')
        plains = self.mode in ('both', 'plains', 'typed_lands', 'decline')
        if self.mode == 'typed_lands':
            names = (PLATEAU, PLATEAU)
        else:
            names = ('Mountain',) * (1 + mountains) + ('Plains',) * (1 + plains)
        # The same lands establish both subtype gates but cannot autopay the hybrid cost.
        for index, name in enumerate(names):
            ref = f'own_{index}'
            self.add(ref, self.actor, name, 'battlefield')
            self.setup[-1]['state'] = ['tapped']
            self.observe(ref, 'tapped', 1)
        if self.mode == 'other_lands':
            for index, name in enumerate(('Mountain', 'Mountain', 'Plains', 'Plains')):
                self.add(f'foe_{index}', self.foe, name, 'battlefield')
        self.add('artifact', self.foe, ARTIFACT, 'battlefield')
        self.add('enchantment', self.actor, ENCHANTMENT, 'battlefield')
        mana = (2, 0, 0, 0, int(self.actor == 0), int(self.actor == 1))
        self.mana('set_mana', self.actor, mana)
        self.emit('refresh')
        self.emit('cast', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        if self.actor == 0:
            self.emit('choose_trigger', 'source', player=self.actor)
        deaths = []
        for available, ref in ((mountains, 'artifact'), (plains, 'enchantment')):
            if not available:
                continue
            if self.mode == 'decline' and ref == 'artifact':
                self.emit('choose_optional_target', ref, player=self.actor)
            else:
                self.target(self.actor, ref)
                deaths.append(ref)
        self.entry(deaths)

    def overseer(self):
        if self.mode == 'flying':
            self.flight_witnesses()
            target = 'ground'
        elif self.mode == 'opponent_token':
            self.add('raise', self.foe, RAISE, 'hand')
            self.go(self.foe)
            self.cast_to('raise', 'graveyard')
            self.tokens('Soldier', self.foe, 2, token=1, power=1, toughness=1)
            self.bind('soldier', self.foe, 'Soldier', 'battlefield')
            self.go(self.actor)
            target = 'soldier'
        else:
            self.add('victim', self.actor if self.mode == 'own_creature' else self.foe, HOST, 'battlefield')
            target = 'victim'
        if self.mode == 'later_death':
            self.add('kill', self.actor, KILL, 'hand')
        self.paid('source')
        decline = self.mode in ('later_death', 'flying')
        self.emit('choose_optional_target' if decline else 'choose_target', target, player=self.actor)
        permanent_victim = () if decline or self.mode == 'opponent_token' else ('victim',)
        self.entry(permanent_victim)
        if self.mode == 'opponent_token':
            self.cards['Soldier_'+str(self.foe)+'_0']['zone'] = 'absent'
        self.tokens('Zombie', self.actor, int(self.mode == 'foe_creature'), token=1, power=2, toughness=2,
                    tapped=1, color=2, subtype_Zombie=1)
        if self.mode == 'later_death':
            self.cast_to('kill', 'graveyard', 'victim')
            self.grave(('victim',))
            self.tokens('Zombie', self.actor, 1, token=1, power=2, toughness=2, tapped=1, color=2, subtype_Zombie=1)
        if self.mode == 'flying':
            self.flight()
            self.tokens('Zombie', self.actor, 1, token=1, power=2, toughness=2, tapped=1, color=2, subtype_Zombie=1)

    def shadowborn(self):
        self.add('demon', self.foe, DEMON, 'battlefield')
        if self.mode != 'demons_only':
            self.add('victim', self.foe, HOST, 'battlefield')
        if self.mode in ('five_upkeep', 'six_upkeep', 'flying'):
            count = 5 if self.mode == 'five_upkeep' else 6
            for index in range(count):
                self.add(f'grave_{index}', self.actor, HOST, 'graveyard')
            self.add('grave_land', self.actor, 'Forest', 'graveyard')
        if self.mode in ('five_upkeep', 'six_upkeep'):
            # Squee itself can return from the graveyard during this same upkeep.
            fodder = UPKEEP_FODDER if self.mode == 'five_upkeep' else SQUEE
            self.add('sacrifice', self.actor, fodder, 'battlefield')
        if self.mode == 'flying':
            self.flight_witnesses()
        self.paid('source')
        if self.mode != 'demons_only':
            self.target(self.actor, 'victim')
        self.entry(() if self.mode == 'demons_only' else ('victim',))
        if self.mode in ('five_upkeep', 'six_upkeep'):
            self.go(self.foe)
            if self.mode == 'five_upkeep' and self.actor == 0:
                self.target(self.actor, 'sacrifice')
            self.go(self.actor)
            self.emit('assert_unused_choices', value=0)
            if self.mode == 'five_upkeep':
                self.grave(('sacrifice',))
        if self.mode == 'flying':
            self.flight()

    def shrike(self):
        tribute = self.mode in ('paid', 'flying')
        victim = self.mode != 'empty_foe'
        if victim:
            self.add('victim', self.foe, MYR, 'battlefield')
        if self.mode == 'flying':
            self.flight_witnesses()
        self.paid('source')
        self.emit('choose_dialog', player=self.foe, value=int(not tribute))
        if not tribute and victim and self.foe == 0:
            self.target(self.foe, 'victim')
        self.entry(('victim',) if not tribute and victim else (), observe_stats=False)
        count = 2 * tribute
        self.observe('source', 'plus1_counters', count)
        self.source_stats(bonus=count)
        if self.mode == 'flying':
            self.flight(bonus=count)

    def fund_foe(self, reserved):
        self.mana('set_mana', self.foe, reserved)
        self.mana('set_mana', self.actor, cost_from_oracle(self.record))
        self.emit('refresh')
        self.emit('cast', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_hand', 0)
        self.mana('assert_mana', self.actor, ZERO)
        self.mana('assert_mana', self.foe, reserved)

    def cast_origin(self):
        reanimated = self.mode in ('reanimated', 'flying')
        if self.mode == 'flying':
            self.flight_witnesses()
        deaths = []
        if self.csvid == REGENT:
            count = 4 if self.mode == 'four' else 3 if self.mode == 'flying' else 5
            for index in range(count):
                name = MYR if self.mode == 'indestructible' and index == 0 else HOST
                ref = f'other_{index}'
                self.add(ref, self.actor if index % 2 == 0 else self.foe, name, 'battlefield')
                if count == 5 and name != MYR and not reanimated:
                    deaths.append(ref)
        else:
            self.add('own_creature', self.actor, HOST, 'battlefield')
            self.add('foe_creature', self.foe, TROLL if self.mode == 'no_regen' else HOST, 'battlefield')
            if self.csvid == CACODEMON:
                if self.mode == 'indestructible':
                    self.add('protected', self.foe, MYR, 'battlefield')
                    self.add('own_artifact', self.actor, ARTIFACT, 'battlefield')
                if not reanimated:
                    deaths = ['foe_creature']
            else:
                self.add('foe_black', self.foe, SKELETON, 'battlefield')
                self.add('own_artifact_creature', self.actor, GNOMES, 'battlefield')
                if not reanimated:
                    deaths = ['own_creature', 'foe_creature']

        source_ref = 'source'
        if reanimated:
            self.add('revival', self.actor, REANIMATE, 'hand')
            self.emit('choose_zone_card', player=self.actor, value=self.csvid)
            self.cast_to('revival', 'graveyard')
            self.cards['source']['zone'] = 'battlefield'
            self.bind('revived', self.actor, self.record['name'], 'battlefield')
            source_ref = 'revived'
            self.source_stats(source_ref)
        elif self.mode == 'no_regen':
            reserved = (1, 0, 0, 1, 0, 0)
            self.fund_foe(reserved)
            self.emit('choose_regeneration', 'foe_creature')
            self.emit('resolve')
            self.emit('assert_unused_choices', value=1)
            self.emit('assert_choices_consumed')
            self.emit('refresh')
            self.cards['source']['zone'] = 'battlefield'
            self.grave(deaths)
            self.source_stats()
            self.mana('assert_mana', self.foe, reserved)
        else:
            self.paid('source')
            self.entry(deaths)
        if self.csvid == CACODEMON:
            self.observe('own_creature', 'in_play', 1)
            self.observe('own_creature', 'tapped', int(not reanimated))
            self.observe(source_ref, 'tapped', 0)
        if self.mode == 'flying':
            self.flight(source_ref)

    def cataclysm(self):
        if self.mode == 'all_types':
            categories = (('artifact', ARTIFACT), ('creature', HOST), ('enchantment', ENCHANTMENT),
                          ('planeswalker', PLANESWALKER))
            for seat in (self.actor, self.foe):
                for category, name in categories:
                    self.add(f'{category}_{seat}', seat, name, 'battlefield')
                self.add(f'land_{seat}', seat, 'Island', 'battlefield')
                self.add(f'extra_{seat}', seat, MYR, 'battlefield')
        self.paid('source')
        if self.mode == 'all_types':
            for seat in (self.actor, self.foe):
                for category, _ in categories:
                    self.target(seat, f'{category}_{seat}')
            self.entry(('source', 'extra_0', 'extra_1'), observe_stats=False)
        else:
            for _ in range(2):
                self.target(self.actor, 'source')
            self.entry()
        if self.mode == 'vigilance':
            self.go(self.foe)
            self.go(self.actor)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.observe('source', 'tapped', 0)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.settle()
            self.life[self.foe] -= 4
            self.emit('end_combat')
            self.settle()

    def noxious(self):
        if self.mode != 'no_other':
            name = MYR if self.mode == 'indestructible' else WALL
            self.add('victim', self.actor if self.mode == 'own_creature' else self.foe, name, 'battlefield')
        if self.mode == 'menace':
            self.add('other_blocker', self.foe, WALL, 'battlefield')
        self.paid('source')
        if self.actor == 0:
            self.emit('choose_trigger', 'source', player=self.actor)
        destroyed = self.mode in ('foe_creature', 'own_creature')
        if self.mode != 'no_other':
            self.emit('choose_optional_target' if self.mode in ('decline', 'menace') else 'choose_target',
                      'victim', player=self.actor)
        self.entry(('victim',) if destroyed else ())
        self.life[self.actor] += 7 * destroyed
        if self.mode == 'menace':
            self.go(self.foe)
            self.go(self.actor)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            for ref in ('victim', 'other_blocker'):
                self.emit('block', ref, 'source', value=1)
            self.emit('advance_combat')
            self.observe('source', 'blocked', 1)
            if self.actor == 0:
                for _ in range(5):
                    self.target(self.actor, 'victim')
            self.emit('combat_damage')
            self.settle()
            self.emit('end_combat')
            self.settle()

    def plague(self):
        first = 'source' if self.mode == 'self_only' else 'own'
        if first != 'source':
            self.add('own', self.actor, PLANESWALKER if self.mode == 'own_planeswalker' else SQUEE, 'battlefield')
        opponent_discards = self.mode in ('foe_discards', 'self_only')
        if opponent_discards:
            self.add('hand_0', self.foe, 'Island', 'hand')
            self.add('hand_1', self.foe, 'Island', 'hand')
        else:
            self.add('foe', self.foe, PLANESWALKER if self.mode == 'foe_planeswalker' else HOST, 'battlefield')
        if opponent_discards and self.foe == 1:
            self.emit('present_ai_dialogs', value=0)
        self.paid('source')
        if self.actor == 0:
            self.target(self.actor, first)
        if self.foe == 0:
            if opponent_discards:
                self.emit('choose_zone_card', player=self.foe, value=self.rows['Island']['csvid'])
            else:
                self.target(self.foe, 'foe')
        deaths = (first,) if opponent_discards else (first, 'foe')
        self.entry(deaths, observe_stats=first != 'source')
        if opponent_discards:
            self.cards['hand_0']['zone'] = 'graveyard'

    def terastodon(self):
        self.add('own_land', self.actor, 'Island', 'battlefield')
        self.add('foe_artifact', self.foe, ARTIFACT, 'battlefield')
        self.add('foe_enchantment', self.foe, ENCHANTMENT, 'battlefield')
        self.add('creature', self.foe, HOST, 'battlefield')
        if self.mode == 'indestructible':
            self.add('steel', self.foe, CITADEL, 'battlefield')
        planned = {
            'three': ('own_land', 'foe_artifact', 'foe_enchantment'),
            'decline': (), 'one_each': ('own_land', 'foe_artifact'), 'indestructible': ('steel',),
        }[self.mode]
        self.paid('source')
        for ref in planned:
            self.target(self.actor, ref)
        if len(planned) < 3:
            self.emit('choose_done_target', player=self.actor)
        self.entry(tuple(ref for ref in planned if ref != 'steel'))
        for seat in (self.actor, self.foe):
            count = sum(self.cards[ref]['player'] == seat and ref != 'steel' for ref in planned)
            self.tokens('Elephant', seat, count, token=1, power=3, toughness=3, color=8,
                        tapped=0, subtype_Elephant=1)

    def brigand(self):
        pays = self.mode in ('paid', 'blocking')
        if self.mode != 'foe_empty':
            self.add('victim', self.foe, HOST, 'battlefield')
        if self.mode == 'blocking':
            self.add('flying_attacker', self.foe, DRAGON, 'battlefield')
        reserve = (3 if pays else 2, 0, 0, 0, 0, 0)
        self.fund_foe(reserve)
        if self.mode == 'short' and self.foe == 0:
            self.target(self.foe, 'victim')
        self.entry(('victim',) if self.mode == 'short' else ())
        self.mana('assert_mana', self.foe, ZERO if pays else reserve)
        if self.mode == 'blocking':
            self.go(self.foe)
            self.emit('begin_attackers')
            self.emit('attack', 'victim', value=1)
            self.emit('attack', 'flying_attacker', value=1)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            self.emit('block', 'source', 'victim', value=0)
            self.emit('block', 'source', 'flying_attacker', value=1)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.settle()
            self.grave(('source',))
            self.life[self.actor] -= 1
            self.emit('end_combat')
            self.settle()


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
    if (set(pins) != {'sources', 'witnesses', 'tokens'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(PRINTED)
            or set(pins['tokens']) != set(TOKENS)):
        raise ValueError('complete destruction or sacrifice source, witness, and token pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal' or oracle[name].get('card_faces')):
            raise ValueError('pinned destruction or sacrifice card changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])
    for name, pin in pins['tokens'].items():
        if (not exact(oracle.get(name), pin['oracle']) or not exact(rows.get(name), pin['registry'])
                or pin['registry']['csvid'] != TOKENS[name]):
            raise ValueError('pinned generated token identity changed: ' + name)


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
        cases.append(dict(name=label(csvid, mode, actor), card=name, fixture='empty_main', seed=3212,
                          setup=board.setup, steps=board.steps, _recipe=recipe))
    return cases
