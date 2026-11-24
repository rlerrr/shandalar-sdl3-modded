"""Paid entry combat and exact secondary ability riders."""

from functools import lru_cache
import json
from pathlib import Path

from entry_follow_on_damage_and_tap_recipes import CombatBoard, ZERO, ZONES
from library_search_recipes import wave6_exact as exact
from recipes import cost_from_oracle, step

PROFILE = Path(__file__).parent / 'data' / 'remaining_entry_combat_wave8_profiles.json'

FURY, HUSSAR, MOON, DUELIST = 9540, 8286, 15904, 16017
SEED = 3500
FAMILY = 'remaining_entry_combat'
MODELS = {
    FURY: ('{3}{R}{R}', 'Creature — Giant Warrior', '3', '3',
           'When this creature enters, other creatures you control gain "{T}: This creature deals 2 damage to any target" until end of turn.\n'
           "Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.)",
           ('Persist',), ('player', 'creature', 'persist', 'expired', 'sick', 'countered')),
    HUSSAR: ('{3}{W}{U}', 'Creature — Human Knight', '4', '3',
             'Flying\nWhen this creature enters, untap all creatures you control.\n'
             'Forecast — Tap two untapped white and/or blue creatures you control, Reveal this card from your hand: Draw a card. '
             '(Activate only during your upkeep and only once each turn.)',
             ('Flying', 'Forecast'), ('untap_scope', 'flying', 'forecast', 'forecast_once_reset', 'only_one', 'foreign_upkeep')),
    MOON: ('{5}{U}{U}', 'Creature — Drake', '5', '5',
           'Flying\nWhen this creature enters, target creature gains flying until end of turn.\n'
           '{4}{U}{U}, Exile this card from your graveyard: Creatures you control gain flying until end of turn.',
           ('Flying',), ('own', 'source_flight', 'grave_team', 'grave_expire', 'grave_short')),
    DUELIST: ('{3}{R}', 'Creature — Vampire Warrior', '3', '2',
              "Haste\nWhen this creature enters, target creature can't block this turn.",
              ('Haste',), ('haste', 'other_blocks', 'own_target', 'expire', 'countered')),
}

WITNESSES = {
    'Essence Scatter': ('{1}{U}', 'Counter target creature spell.'),
    'Fiery Hellhound': ('{1}{R}{R}', '{R}: This creature gets +1/+0 until end of turn.', '2', '2'),
    'Forest': ('', '({T}: Add {G}.)'),
    'Goblin Guide': ('{R}', "Haste\nWhenever this creature attacks, defending player reveals the top card of their library. If it's a land card, that player puts it into their hand.", '2', '2'),
    'Goblin Tinkerer': ('{1}{R}', '{R}, {T}: Destroy target artifact. That artifact deals damage equal to its mana value to this creature.', '1', '2'),
    "Healer's Hawk": ('{W}', 'Flying\nLifelink (Damage dealt by this creature also causes you to gain that much life.)', '1', '1'),
    'Island': ('', '({T}: Add {U}.)'),
    'Murder': ('{1}{B}{B}', 'Destroy target creature.'),
    'Prodigal Sorcerer': ('{2}{U}', '{T}: This creature deals 1 damage to any target.', '1', '1'),
    'Serra Angel': ('{3}{W}{W}', "Flying\nVigilance (Attacking doesn't cause this creature to tap.)", '4', '4'),
    'Welding Jar': ('{0}', 'Sacrifice this artifact: Regenerate target artifact.'),
}


PONTIFF = 8100
EXARCH = 11698
GATEKEEPER = 13238
VENGEANCE = 14511
HORNET = 16704
MORAY = 16796
GIDEON = 16918
PROTECTOR = 16621
OFFICER = 19650
WAR = 6835
BELL = 14382

FOLLOWERS = {
    PONTIFF: ('{1}{W}{B}', 'Creature — Human Cleric', '1', '1',
        "Haunt (When this creature dies, exile it haunting target creature.)\nWhen this creature enters or the creature it haunts dies, choose one —\n• Creatures you control get +1/+1 until end of turn.\n• Creatures you don't control get -1/-1 until end of turn.",
        ('Haunt',), ('pump', 'weaken', 'haunt')),
    EXARCH: ('{3}{R}', 'Creature — Phyrexian Cleric', '2', '2',
        'When this creature enters, choose one —\n• Target creature gets +2/+0 until end of turn.\n• Target creature gets -0/-2 until end of turn.',
        (), ('pump', 'shrink', 'expire')),
    GATEKEEPER: ('{3}{B}', 'Creature — Zombie Soldier', '2', '4',
        'When this creature enters, if you control two or more Gates, target creature an opponent controls gets -2/-2 until end of turn.',
        (), ('two', 'one', 'foreign')),
    VENGEANCE: ('{B}{B}', 'Enchantment — Aura', None, None,
        'Enchant creature\nWhen this Aura enters, put a +1/+1 counter on target creature you control.\nEnchanted creature gets -1/-1.',
        ('Enchant',), ('opponent_host', 'self_host', 'leaves')),
    HORNET: ('{3}{B}', 'Creature — Insect', '2', '3',
        'Flying\nWhen this creature enters, if you control a creature with a +1/+1 counter on it, creatures your opponents control get -1/-1 until end of turn.',
        ('Flying',), ('eligible', 'opponent_only', 'flying')),
    MORAY: ('{3}{U}', 'Creature — Fish', '0', '5',
        'When this creature enters, you get {E}{E}{E}{E} (four energy counters).\nPay {E}: This creature gets +2/-2 until end of turn.',
        (), ('twice', 'third_dies', 'countered')),
    GIDEON: ('{4}{W}', 'Enchantment', None, None,
        'When this enchantment enters, you may search your library and/or graveyard for a card named Gideon, Martial Paragon, reveal it, and put it into your hand. If you search your library this way, shuffle.\nCreatures you control get +1/+1.',
        (), ('scope', 'library', 'graveyard', 'leaves')),
    PROTECTOR: ('{10}{G}', 'Creature — Avatar', '10', '10',
        'This spell costs {1} less to cast for each creature your opponents control.\nWhen this creature enters, put a +1/+1 counter on each other creature you control.',
        (), ('scope', 'no_foes', 'crowd')),
    OFFICER: ('{2}{W}', 'Creature — Human Soldier', '3', '2',
        'When this creature enters, target creature you control gets +1/+1 until end of turn.',
        (), ('ally', 'self', 'countered')),
    WAR: ('{R}{R}{R}', 'Creature — Elemental', '1', '1',
        'When this creature enters, sacrifice it unless an opponent was dealt damage this turn.\nWhenever an opponent is dealt damage, put that many +1/+1 counters on this creature.',
        (), ('no_damage', 'foe_damage', 'self_damage')),
    BELL: ('{1}{U}', 'Enchantment — Aura', None, None,
        'Enchant creature\nWhen this Aura enters, tap enchanted creature.\nEnchanted creature doesn\'t untap during its controller\'s untap step.\nEnchanted creature has "{6}: Untap this creature."',
        ('Enchant',), ('lock', 'leaves', 'pay', 'short')),
}
NEW_WITNESSES = {
    'Shock': ('{R}', 'Shock deals 2 damage to any target.'),
    'Disenchant': ('{1}{W}', 'Destroy target artifact or enchantment.'),
    'Burst of Strength': ('{G}', 'Put a +1/+1 counter on target creature and untap it.'),
    'Azorius Guildgate': ('', 'This land enters tapped.\n{T}: Add {W} or {U}.'),
    'Selesnya Guildgate': ('', 'This land enters tapped.\n{T}: Add {G} or {W}.'),
    'Gideon, Martial Paragon': ('{4}{W}', '+2: Untap all creatures you control. Those creatures get +1/+1 until end of turn.\n'
                                '0: Until end of turn, Gideon becomes a 5/5 Human Soldier creature with indestructible that\'s still a planeswalker. '
                                'Prevent all damage that would be dealt to him this turn.\n'
                                '−10: Creatures you control get +2/+2 until end of turn. Tap all creatures your opponents control.'),
}


class Board(CombatBoard):
    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('can_activate_granted_legacy', 'activate_granted_legacy'):
            self.steps.append(step(op, ref=ref, index=index, value=value))
            return
        super().emit(op, ref, other, prop, player=player, value=value, zone=zone, index=index, color=color)

    def pending(self, ref, *, response=False):
        who = self.cards[ref]['player']
        self.fund(who, cost_from_oracle(self.oracle[self.cards[ref]['name']]))
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_stack', value=2 if response else 1)
        self.wallet[who] = ZERO
        self.pools()

    def enter(self, target=None):
        self.pending('source')
        if self.mode == 'countered':
            self.emit('announce_spell', 'source')
            self.pending('counter', response=True)
            self.emit('resolve_response')
            self.move('counter', 'graveyard')
            self.move('source', 'graveyard')
            return False
        if target:
            self.emit('choose_target', target, player=self.actor)
        self.finish_entry()
        return True

    def finish_entry(self):
        self.emit('resolve')
        self.move('source', 'battlefield')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)

    def battle(self, attacker, *, attempts=(), blocker=None, stats=None, vigilance=False, trample=False, lifelink=False):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.prop(attacker, 'attacking', 1)
        self.prop(attacker, 'tapped', int(not vigilance))
        self.emit('finish_attackers')
        self.emit('resolve')
        self.emit('begin_blockers')
        for candidate, allowed in attempts:
            self.emit('block', candidate, attacker, value=int(allowed))
        self.emit('advance_combat')
        self.prop(attacker, 'blocked', int(blocker is not None))
        if blocker is not None:
            self.prop(blocker, 'blocking_count', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        power, tough = stats or self.stats(attacker)
        if blocker is None:
            self.life[self.foe] -= power
        else:
            opposing_power, opposing_tough = self.stats(blocker)
            if trample:
                self.life[self.foe] -= max(0, power-opposing_tough)
            for ref, damage, endurance in ((attacker, opposing_power, tough), (blocker, power, opposing_tough)):
                if damage >= endurance:
                    self.move(ref, 'graveyard')
                else:
                    self.prop(ref, 'damage', damage)
        if lifelink:
            self.life[self.actor] += power
        self.receipt()

    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid, self.mode, self.actor, self.foe = csvid, mode, actor, 1-actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet, self.life = [ZERO, ZERO], [20, 20]
        source_zone = 'graveyard' if mode.startswith('grave_') else 'hand'
        self.add('source', record['name'], actor, source_zone)
        if mode == 'countered':
            self.add('counter', 'Essence Scatter', self.foe, 'hand')
        if csvid == FURY:
            self.fury_setup()
        elif csvid == HUSSAR:
            self.hussar_setup()
        else:
            self.add('ally', 'Goblin Tinkerer', actor, 'battlefield')
            self.add('guard', 'Goblin Tinkerer', self.foe, 'battlefield')
            if csvid == MOON:
                self.add('reserve', 'Fiery Hellhound', actor, 'battlefield')
            if mode == 'other_blocks':
                self.add('other', 'Prodigal Sorcerer', self.foe, 'battlefield')

    def fury_setup(self):
        mode = self.mode
        self.add('ally', 'Goblin Tinkerer', self.actor, 'hand' if mode == 'sick' else 'battlefield')
        self.add('guard', 'Fiery Hellhound', self.foe, 'battlefield')
        self.add('reserve', 'Prodigal Sorcerer', self.actor, 'battlefield', tapped=mode == 'player')
        self.add('artifact', 'Welding Jar', self.actor, 'battlefield')
        if mode == 'persist':
            self.add('first_kill', 'Murder', self.actor, 'hand')
            self.add('last_kill', 'Murder', self.actor, 'hand')
        elif mode == 'expired':
            self.add('late', 'Goblin Guide', self.actor, 'hand')

    def hussar_setup(self):
        mode = self.mode
        if mode in ('untap_scope', 'flying'):
            self.add('red', 'Goblin Tinkerer', self.actor, 'battlefield', tapped=True)
            self.add('blue', 'Prodigal Sorcerer', self.actor, 'battlefield', tapped=True)
            self.add('artifact', 'Welding Jar', self.actor, 'battlefield', tapped=True)
            self.add('guard', 'Fiery Hellhound', self.foe, 'battlefield', tapped=mode == 'untap_scope')
            return
        self.add('first', 'Prodigal Sorcerer', self.actor, 'battlefield')
        if mode != 'only_one':
            self.add('second', "Healer's Hawk", self.actor, 'battlefield')
        if mode == 'forecast_once_reset':
            self.add('spare_white', 'Serra Angel', self.actor, 'battlefield')
            self.add('spare_blue', 'Prodigal Sorcerer', self.actor, 'battlefield')
        self.add('red', 'Goblin Tinkerer', self.actor, 'battlefield')
        self.add('foreign', 'Serra Angel', self.foe, 'battlefield')
        self.add('first_draw', 'Forest', self.actor, 'library')
        if mode == 'forecast_once_reset':
            self.add('second_draw', 'Island', self.actor, 'library')

    def grant(self, ref, value, *, activate=False):
        op = 'activate_granted_legacy' if activate else 'can_activate_granted_legacy'
        self.emit(op, ref, index=FURY, value=value)

    def ping(self, ref, *, creature=None):
        self.grant(ref, 1)
        if creature:
            self.emit('choose_target', creature, player=self.actor)
        else:
            self.emit('choose_player', player=self.actor, value=self.foe)
        self.grant(ref, 1, activate=True)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.prop(ref, 'tapped', 1)
        self.pools()
        self.emit('resolve')
        if creature:
            self.move(creature, 'graveyard')
        else:
            self.life[self.foe] -= 2
        self.grant(ref, 0)
        self.receipt()

    def fury(self):
        mode = self.mode
        if mode == 'sick':
            self.cast('ally', destination='battlefield')
            self.prop('ally', 'sick', 1)
        self.grant('ally', 0)
        if not self.enter():
            self.grant('ally', 0)
            return
        self.size('source')
        for ref in ('source', 'guard', 'artifact'):
            self.grant(ref, 0)
        if mode in ('player', 'creature'):
            self.grant('reserve', int(mode != 'player'))
            self.ping('ally', creature='guard' if mode == 'creature' else None)
        elif mode == 'persist':
            self.grant('ally', 1)
            self.round_trip_turn()
            self.grant('ally', 0)
            self.cast('first_kill', target='source')
            self.emit('bind_permanent', 'source', player=self.actor, index=-1, value=FURY)
            self.emit('assert_counter', 'source', prop='minus1', value=1)
            self.size('source', (2, 2))
            self.ping('ally')
            self.cast('last_kill', target='source')
            self.move('source', 'graveyard')
        elif mode == 'expired':
            self.cast('late', destination='battlefield')
            self.prop('late', 'sick', 0)
            self.grant('late', 0)
            self.grant('ally', 1)
            self.round_trip_turn()
            self.grant('ally', 0)
            self.grant('ally', 0, activate=True)
        else:
            self.grant('ally', 0)
            self.grant('ally', 0, activate=True)
            self.prop('ally', 'tapped', 0)
            self.ping('reserve')

    def upkeep(self, player):
        self.emit('untap_step', player=player)
        self.emit('upkeep_step', player=player)

    def forecast(self, draw):
        self.emit('can_activate_hand', 'source', value=1)
        self.emit('choose_dialog', player=self.actor, value=1)
        for ref in ('first', 'second'):
            self.emit('choose_target', ref, player=self.actor)
        self.emit('activate_hand', 'source', value=1)
        # A one-creature underpayment stays an ordinary assertion, so actual
        # tap and draw outcomes remain observable and strict plans end cleanly.
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.prop('source', 'in_hand', 1)
        self.prop('first', 'tapped', 1)
        self.prop('second', 'tapped', 1)
        for untouched in ('spare_white', 'spare_blue', 'red', 'foreign'):
            if untouched in self.cards:
                self.prop(untouched, 'tapped', 0)
        self.emit('resolve')
        self.move(draw, 'hand')
        self.receipt()

    def hussar(self):
        mode = self.mode
        if mode in ('untap_scope', 'flying'):
            self.enter()
            self.size('source')
            for ref in ('red', 'blue'):
                self.prop(ref, 'tapped', 0)
            self.prop('guard', 'tapped', int(mode == 'untap_scope'))
            self.prop('artifact', 'tapped', 1)
            if mode == 'flying':
                self.round_trip_turn()
                self.battle('source', attempts=(('guard', False),))
            return
        self.emit('can_activate_hand', 'source', value=0)
        self.emit('advance_turn')
        turn_owner = self.foe if mode == 'foreign_upkeep' else self.actor
        self.upkeep(turn_owner)
        if mode in ('only_one', 'foreign_upkeep'):
            self.emit('can_activate_hand', 'source', value=0)
            self.prop('first', 'tapped', 0)
            return
        self.forecast('first_draw')
        if mode == 'forecast_once_reset':
            self.emit('can_activate_hand', 'source', value=0)
            self.round_trip_turn()
            self.upkeep(self.actor)
            self.prop('first', 'tapped', 0)
            self.prop('second', 'tapped', 0)
            self.forecast('second_draw')

    def grant_flight(self):
        self.emit('prepare_graveyard_abilities')
        short = self.mode == 'grave_short'
        self.fund(self.actor, (3 if short else 4, 0, 2, 0, 0, 0))
        self.emit('can_activate_graveyard', 'source', value=int(not short))
        if not short:
            self.emit('choose_dialog', player=self.actor, value=1)
        self.emit('activate_graveyard', 'source', value=int(not short))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(not short))
        if short:
            return False
        self.wallet[self.actor] = ZERO
        self.move('source', 'exile')
        self.pools()
        self.emit('assert_zone_multiplicity', player=self.actor, zone=ZONES['exile'], index=MOON, value=1)
        self.emit('assert_zone_multiplicity', player=self.actor, zone=ZONES['graveyard'], index=MOON, value=0)
        self.emit('resolve')
        return True

    def moon(self):
        mode = self.mode
        if mode.startswith('grave_'):
            granted = self.grant_flight()
            if mode == 'grave_team':
                self.emit('begin_attackers')
                for attacker in ('ally', 'reserve'):
                    self.emit('attack', attacker, value=1)
                    self.prop(attacker, 'attacking', 1)
                self.emit('finish_attackers')
                self.emit('begin_blockers')
                for attacker in ('ally', 'reserve'):
                    self.emit('block', 'guard', attacker, value=0)
                self.emit('advance_combat')
                self.emit('combat_damage')
                self.emit('resolve')
                self.emit('end_combat')
                self.life[self.foe] -= self.stats('ally')[0] + self.stats('reserve')[0]
            else:
                if granted:
                    self.round_trip_turn()
                else:
                    self.receipt()
                    self.fund(self.actor, ZERO)
                self.battle('ally', attempts=(('guard', True),), blocker='guard')
            return
        self.enter('ally' if mode == 'own' else 'guard')
        self.size('source')
        if mode == 'own':
            self.battle('ally', attempts=(('guard', False),))
        else:
            self.round_trip_turn()
            self.battle('source', attempts=(('guard', False),))

    def duelist(self):
        mode = self.mode
        entered = self.enter('ally' if mode == 'own_target' else 'guard')
        if entered:
            self.size('source')
            self.prop('source', 'sick', 0)
        if mode == 'expire':
            self.round_trip_turn()
        denied = entered and mode not in ('own_target', 'expire')
        attempts = [('guard', not denied)]
        blocker = None if denied else 'guard'
        if mode == 'other_blocks':
            attempts.append(('other', True))
            blocker = 'other'
        self.battle('source' if entered else 'ally', attempts=attempts, blocker=blocker)

    def play(self):
        owner_upkeep = self.csvid == HUSSAR and self.mode in ('forecast', 'forecast_once_reset', 'only_one')
        self.emit('turn', player=self.foe if owner_upkeep else self.actor)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.fund(player, ZERO)
        {FURY: self.fury, HUSSAR: self.hussar, MOON: self.moon, DUELIST: self.duelist}[self.csvid]()
        self.receipt()

def label(csvid, mode, actor):
    return f'RemainingEntryCombatRecipe_{csvid}_{mode}_{actor}'


class Follower(Board):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid, self.mode, self.actor, self.foe = csvid, mode, actor, 1-actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet, self.life = [ZERO, ZERO], [20, 20]
        self.add('source', record['name'], actor, 'hand')
        if csvid in (PONTIFF, EXARCH, GATEKEEPER, VENGEANCE, HORNET, GIDEON, PROTECTOR, OFFICER):
            self.add('ally', 'Goblin Tinkerer', actor, 'battlefield')
        if csvid != PROTECTOR or mode != 'no_foes':
            guard = ('Prodigal Sorcerer' if csvid == HORNET and mode != 'flying'
                     else 'Serra Angel' if csvid == EXARCH and mode == 'expire' else 'Goblin Tinkerer')
            self.add('guard', guard, self.foe, 'battlefield')
        if mode == 'countered':
            self.add('counter', 'Essence Scatter', self.foe, 'hand')
        if csvid == PONTIFF and mode == 'haunt':
            self.add('host', 'Fiery Hellhound', actor, 'battlefield')
            self.add('first_kill', 'Murder', actor, 'hand')
            self.add('last_kill', 'Murder', actor, 'hand')
        elif csvid == GATEKEEPER:
            self.add('first_gate', 'Azorius Guildgate', actor, 'battlefield', tapped=True)
            if mode != 'one':
                self.add('second_gate', 'Selesnya Guildgate', actor if mode == 'two' else self.foe, 'battlefield', tapped=True)
        elif csvid == HORNET and mode != 'flying':
            self.add('growth', 'Burst of Strength', actor, 'hand')
            self.add('other', 'Fiery Hellhound', self.foe, 'battlefield')
        elif csvid == GIDEON and mode in ('library', 'graveyard'):
            self.add('walker', 'Gideon, Martial Paragon', actor, mode)
            if mode == 'library':
                self.add('decoy', 'Forest', actor, 'library')
        elif csvid == PROTECTOR:
            self.add('second', 'Prodigal Sorcerer', actor, 'battlefield')
            self.add('artifact', 'Welding Jar', self.foe, 'battlefield')
            if mode == 'crowd':
                self.add('extra_foe', 'Fiery Hellhound', self.foe, 'battlefield')
                self.add('third_foe', 'Prodigal Sorcerer', self.foe, 'battlefield')
        elif csvid == WAR and mode != 'no_damage':
            self.add('setup_shock', 'Shock', actor, 'hand')
            if mode == 'foe_damage':
                self.add('later_shock', 'Shock', actor, 'hand')
        elif csvid == BELL:
            self.add('host', 'Fiery Hellhound', actor if mode in ('pay', 'short') else self.foe, 'battlefield')
        if mode == 'leaves':
            self.add('remove', 'Disenchant', actor, 'hand')

    def target(self, ref):
        self.emit('choose_target', ref, player=self.actor)

    def dialog(self, index):
        self.emit('choose_dialog', player=self.actor, value=index)

    def points(self, ref, power, toughness):
        p, t = self.stats(ref)
        self.size(ref, (p+power, t+toughness))

    def counter(self, ref, value):
        self.emit('assert_counter', ref, prop='plus1', value=value)

    def remove_aura(self):
        self.cast('remove', target='source')
        self.move('source', 'graveyard')

    def pontiff(self):
        mode = self.mode
        self.pending('source')
        self.dialog(0 if mode == 'weaken' else 1)
        self.finish_entry()
        if mode == 'weaken':
            self.size('ally')
            self.points('guard', -1, -1)
            self.size('source')
            self.emit('cleanup')
            self.size('guard')
            return
        self.points('ally', 1, 1)
        self.points('source', 1, 1)
        self.size('guard')
        if mode == 'haunt':
            self.fund(self.actor, cost_from_oracle(self.oracle['Murder']))
            self.target('source')
            self.emit('cast', 'first_kill', value=1)
            self.emit('assert_stack', value=1)
            self.wallet[self.actor] = ZERO
            self.pools()
            self.target('host')
            self.emit('resolve')
            self.emit('assert_unused_choices', value=0)
            self.move('first_kill', 'graveyard')
            self.move('source', 'exile')
            self.receipt()
            self.emit('cleanup')
            self.size('ally')
            self.fund(self.actor, cost_from_oracle(self.oracle['Murder']))
            self.target('host')
            self.emit('cast', 'last_kill', value=1)
            self.emit('assert_stack', value=1)
            self.wallet[self.actor] = ZERO
            self.pools()
            self.dialog(0)
            self.emit('resolve')
            self.emit('assert_unused_choices', value=0)
            self.move('host', 'graveyard')
            self.move('last_kill', 'graveyard')
            self.points('guard', -1, -1)
            self.size('ally')
        else:
            self.battle('ally', stats=(2, 3))

    def exarch(self):
        mode = self.mode
        self.pending('source')
        self.dialog(int(mode != 'pump'))
        self.target('ally' if mode == 'pump' else 'guard')
        self.finish_entry()
        if mode == 'pump':
            self.points('ally', 2, 0)
            self.size('guard')
            self.battle('ally', stats=(3, 2))
        else:
            self.size('ally')
            if mode == 'expire':
                self.points('guard', 0, -2)
            else:
                self.move('guard', 'graveyard')
        self.emit('cleanup')
        self.size('ally')
        if mode == 'expire':
            self.size('guard')

    def gatekeeper(self):
        active = self.mode == 'two'
        self.enter('guard' if active else None)
        self.size('source')
        self.size('ally')
        if active:
            self.move('guard', 'graveyard')
        else:
            self.size('guard')

    def vengeance(self):
        host = 'ally' if self.mode == 'self_host' else 'guard'
        self.target(host)
        self.pending('source')
        self.target('ally')
        self.finish_entry()
        self.counter('ally', 1)
        self.points('ally', int(host != 'ally'), int(host != 'ally'))
        self.points('guard', -int(host == 'guard'), -int(host == 'guard'))
        if self.mode == 'leaves':
            self.remove_aura()
            self.size('guard')
            self.counter('ally', 1)
            self.points('ally', 1, 1)

    def hornet(self):
        mode = self.mode
        if mode != 'flying':
            buffed = 'ally' if mode == 'eligible' else 'other'
            self.cast('growth', target=buffed)
            self.counter(buffed, 1)
        self.enter()
        self.size('source')
        if mode == 'eligible':
            self.move('guard', 'graveyard')
            self.points('other', -1, -1)
            self.points('ally', 1, 1)
            self.emit('cleanup')
            self.size('other')
        elif mode == 'opponent_only':
            self.size('guard')
            self.points('other', 1, 1)
        else:
            self.round_trip_turn()
            self.battle('source', attempts=(('guard', False),))

    def moray(self):
        if not self.enter():
            self.emit('assert_energy', player=self.actor, value=0)
            return
        self.emit('assert_energy', player=self.actor, value=4)
        self.emit('assert_energy', player=self.foe, value=0)
        self.size('source')
        for i in range(1, 4 if self.mode == 'third_dies' else 3):
            self.emit('can_activate', 'source', value=1)
            self.emit('activate', 'source', value=1)
            self.emit('assert_stack', value=1)
            self.emit('assert_energy', player=self.actor, value=4-i)
            self.size('source', (2*(i-1), 5-2*(i-1)))
            self.emit('resolve')
            self.emit('refresh')
            if i < 3:
                self.size('source', (2*i, 5-2*i))
            else:
                self.move('source', 'graveyard')
        if self.mode == 'twice':
            self.emit('cleanup')
            self.size('source')
            self.emit('assert_energy', player=self.actor, value=2)

    def gideon(self):
        self.pending('source')
        found = self.mode in ('library', 'graveyard')
        if found:
            self.emit('choose_zone_card', player=self.actor, value=self.rows[self.cards['walker']['name']]['csvid'])
        self.finish_entry()
        if found:
            self.move('walker', 'hand')
        self.steps.append(step('assert_shuffle_count', player=self.actor, value=int(self.mode == 'library')))
        self.points('ally', 1, 1)
        self.size('guard')
        if self.mode == 'leaves':
            self.remove_aura()
            self.size('ally')
        elif self.mode == 'scope':
            self.emit('cleanup')
            self.points('ally', 1, 1)

    def protector(self):
        count = {'scope': 1, 'no_foes': 0, 'crowd': 3}[self.mode]
        self.fund(self.actor, (10-count, 0, 0, 1, 0, 0))
        self.emit('cast', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.wallet[self.actor] = ZERO
        self.pools()
        self.finish_entry()
        for ref in ('ally', 'second'):
            self.counter(ref, 1)
            self.points(ref, 1, 1)
        self.counter('source', 0)
        self.size('source')
        if count:
            self.counter('guard', 0)
            self.size('guard')

    def officer(self):
        target = 'source' if self.mode == 'self' else 'ally'
        if not self.enter(target):
            self.size('ally')
            return
        self.points(target, 1, 1)
        self.size('guard')
        if self.mode == 'ally':
            self.battle('ally', stats=(2, 3))
        self.emit('cleanup')
        self.size(target)

    def shock(self, ref, player):
        self.fund(self.actor, cost_from_oracle(self.oracle['Shock']))
        self.emit('choose_player', player=self.actor, value=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.wallet[self.actor] = ZERO
        self.pools()
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.move(ref, 'graveyard')
        self.life[player] -= 2

    def war(self):
        mode = self.mode
        if mode != 'no_damage':
            self.shock('setup_shock', self.foe if mode == 'foe_damage' else self.actor)
        self.enter()
        if mode == 'foe_damage':
            self.counter('source', 0)
            self.size('source')
            self.shock('later_shock', self.foe)
            self.counter('source', 2)
            self.size('source', (3, 3))
        else:
            self.move('source', 'graveyard')

    def bell(self):
        self.target('host')
        self.enter()
        self.prop('host', 'tapped', 1)
        self.prop('guard', 'tapped', 0)
        if self.mode in ('lock', 'leaves'):
            if self.mode == 'leaves':
                self.remove_aura()
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.prop('host', 'tapped', int(self.mode == 'lock'))
            return
        pays = self.mode == 'pay'
        self.fund(self.actor, (6 if pays else 5, 0, 0, 0, 0, 0))
        self.emit('can_activate', 'source', value=int(pays))
        self.emit('activate', 'source', value=int(pays))
        self.emit('assert_stack', value=int(pays))
        if pays:
            self.wallet[self.actor] = ZERO
        self.pools()
        self.prop('host', 'tapped', 1)
        self.emit('resolve')
        self.prop('host', 'tapped', int(not pays))

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.fund(player, ZERO)
        actions = {PONTIFF: self.pontiff, EXARCH: self.exarch, GATEKEEPER: self.gatekeeper,
                   VENGEANCE: self.vengeance, HORNET: self.hornet, MORAY: self.moray, GIDEON: self.gideon,
                   PROTECTOR: self.protector, OFFICER: self.officer, WAR: self.war, BELL: self.bell}
        actions[self.csvid]()
        self.receipt()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    cost, kind, power, toughness, rules, keywords, modes = (MODELS | FOLLOWERS)[csvid]
    fields = ('mana_cost', 'type_line', 'power', 'toughness', 'oracle_text')
    if tuple(record.get(field) for field in fields) != (cost, kind, power, toughness, rules):
        raise ValueError('Exact cost, type, stats and all entry clauses required')
    if record.get('layout') != 'normal' or record.get('card_faces') or set(record.get('keywords', ())) != set(keywords):
        raise ValueError('Complete pinned normal entry source required')
    return dict(csvid=csvid, cost=cost, choices=[f'{mode}_{seat}' for mode in modes for seat in (0, 1)])


def variants(model):
    return list(model['choices'])


def validate_witnesses(oracle, rows):
    pinned = profiles()
    witnesses = WITNESSES | NEW_WITNESSES
    if (set(pinned) != {'sources', 'witnesses'} or set(pinned['witnesses']) != witnesses.keys()
            or {pin['registry']['csvid'] for pin in pinned['sources'].values()} != MODELS.keys() | FOLLOWERS.keys()):
        raise ValueError('Exact source and witness scope required')
    for group in pinned.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, dict(oracle=record, registry=row)) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed implemented entry source or witness: {name}')
            if name in witnesses:
                expected = witnesses[name]
                fields = ('mana_cost', 'oracle_text', 'power', 'toughness')[:len(expected)]
                if tuple(record[k] for k in fields) != expected:
                    raise ValueError(f'Changed witness rules: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned entry primary required')
    model = classify(record, pin['registry']['csvid'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(chosen, list) or not chosen
            or any(type(v) is not str for v in chosen) or len(chosen) != len(set(chosen)) or set(chosen)-set(variants(model))):
        raise ValueError('Exact scoped recipe and distinct variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected)-set(variants(model)):
        raise ValueError('Exact primary and bounded variants required')
    factory = Follower if csvid in FOLLOWERS else Board
    gaps = {
        HUSSAR: ['Forecast uses its actual named hand source. Public visibility to the opponent is unobserved.'],
        GIDEON: ['Search selection, hand destination and shuffle are observed. Public reveal is not observed separately.'],
        BELL: ['Paid untap uses the production Aura callback with a host controlled by its caster. '
               'The cross-controller legacy has no attachment coordinate for the test selector.'],
    }
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = factory(oracle[name], csvid, mode, int(seat), rows, oracle)
        board.play()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=gaps.get(csvid, []))
        cases.append(dict(name=label(csvid, mode, int(seat)), card=name, fixture='empty_main', seed=SEED,
                          setup=board.setup, steps=board.steps, _recipe=metadata))
    return cases

