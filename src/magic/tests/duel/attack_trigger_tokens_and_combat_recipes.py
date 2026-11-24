"""Paid attack-trigger tokens, deployed creatures, and combat rewards."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from mill_death_and_entry_recipes import Duel as PaidBoard, stats
from recipes import cost_from_oracle


FAMILY = 'attack_trigger_tokens_and_combat'
PROFILE = Path(__file__).with_name('data') / 'attack_trigger_tokens_and_combat_wave8_profiles.json'
PREFIX = 'AttackTriggerTokensAndCombatRecipe_'
GEMINI, HERO, KESSIG, WARLEADER = 6894, 11458, 11996, 18953
PRINTED = {
    GEMINI: ('darksteel.c', '{6}', 'Artifact Creature — Construct', (3, 4), (),
             "Whenever this creature attacks, create a colorless Construct artifact creature token named Twin that's attacking. "
             "Its power is equal to this creature's power and its toughness is equal to this creature's toughness. "
             'Sacrifice the token at end of combat.'),
    HERO: ('mirrodin_besieged.c', '{2}{W}{W}', 'Creature — Human Knight', (3, 4), ('Battle Cry',),
           'Battle cry (Whenever this creature attacks, each other attacking creature gets +1/+0 until end of turn.)\n'
           'Whenever this creature attacks, create two 1/1 white Soldier creature tokens that are tapped and attacking.'),
    KESSIG: ('innistrad.c', '{4}{G}', 'Creature — Human Rogue', (3, 4), (),
             "Whenever this creature attacks, create a 2/2 green Wolf creature token that's tapped and attacking "
             'for each creature card in your graveyard.'),
    WARLEADER: ('m19.c', '{2}{W}{W}', 'Creature — Cat Soldier', (4, 4), (),
                'Whenever this creature attacks, create two 1/1 white Cat creature tokens with lifelink '
                'that are tapped and attacking.'),
}
MODES = {
    GEMINI: ('ordinary', 'boosted', 'later_growth', 'blocked_source', 'other_only'),
    HERO: ('ordinary', 'teammate', 'soldiers_blocked', 'late_teammate', 'other_only'),
    KESSIG: ('empty', 'mixed', 'foe_grave', 'own_death', 'next_untap'),
    WARLEADER: ('ordinary', 'blocked_source', 'blocked_cat', 'cat_dies', 'other_only'),
}
TOKENS = {
    GEMINI: ('Twin', 18846, 'Construct', 0, True),
    HERO: ('Soldier', 18822, 'Soldier', 0x20, False),
    KESSIG: ('Wolf', 18856, 'Wolf', 0x08, False),
    WARLEADER: ('Cat', 18704, 'Cat', 0x20, False),
}
PARTNER, WALL = 'Prodigal Sorcerer', 'Dazzling Ramparts'
SENTRY, GHOUL, DRIFT = 'Loyal Sentry', 'Diregraf Ghoul', 'Drift of Phantasms'
DISENCHANT, STONE, BOUNCE = 'Disenchant', 'Millstone', 'Unsummon'
GOBLIN, SERVANT, SHIELD = 'Goblin Tinkerer', 'Servant of the Conduit', 'Silent Gravestone'
GROWTH, BOLT = 'Giant Growth', 'Lightning Bolt'
ICY = 'Icy Manipulator'
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
        self.source_power, self.source_toughness = stats(record)
        self.token_name, self.token_id, self.token_type, self.token_color, self.token_artifact = TOKENS[csvid]
        self.tokens = []
        self.add('source', actor, record['name'], 'hand')
        self.cards['token_identity'] = dict(name=self.token_name, player=actor, zone='absent')
        if mode in ('boosted', 'later_growth'):
            self.add('growth', actor, GROWTH, 'hand')
        if mode in ('teammate', 'other_only'):
            self.add('partner', actor, PARTNER, 'battlefield')
        if mode == 'late_teammate':
            self.add('partner', actor, PARTNER, 'hand')
        if mode in ('blocked_source', 'blocked_cat', 'cat_dies', 'soldiers_blocked'):
            self.add('blocker', self.foe, PARTNER if mode == 'cat_dies' else WALL, 'battlefield')
        if mode == 'soldiers_blocked':
            self.add('second_blocker', self.foe, WALL, 'battlefield')
        if csvid == KESSIG:
            self.kessig_setup()

    def kessig_setup(self):
        self.add('own_land', self.actor, 'Mountain', 'graveyard')
        self.add('own_instant', self.actor, GROWTH, 'graveyard')
        if self.mode in ('mixed', 'next_untap'):
            self.add('own_creature', self.actor, PARTNER, 'graveyard')
        if self.mode == 'mixed':
            self.add('second_creature', self.actor, WALL, 'graveyard')
        if self.mode in ('foe_grave', 'mixed'):
            self.add('foe_creature', self.foe, PARTNER, 'graveyard')
        if self.mode == 'own_death':
            self.add('victim', self.actor, PARTNER, 'battlefield')
            self.add('bolt', self.actor, BOLT, 'hand')

    def cast_to(self, ref, zone, target=None):
        self.paid(ref, target=target)
        self.settle()
        self.cards[ref]['zone'] = zone

    def source_stats(self, power, toughness):
        self.observe('source', 'power', power)
        self.observe('source', 'toughness', toughness)

    def assert_token(self, **properties):
        for prop, value in properties.items():
            self.emit('assert_matching_permanents', prop=prop, player=self.actor,
                      index=self.token_id, value=value)

    def token_count(self, count):
        self.emit('assert_zone_multiplicity', player=self.actor, zone='battlefield', index=self.token_id, value=count)
        self.emit('assert_zone_multiplicity', player=self.foe, zone='battlefield', index=self.token_id, value=0)

    def expect_tokens(self, source_attacks, power, toughness):
        quantity = 0
        if source_attacks:
            if self.csvid == KESSIG:
                quantity = sum(c['player'] == self.actor and c['zone'] == 'graveyard' and
                               self.oracle[c['name']]['type_line'].startswith('Creature')
                               for c in self.cards.values() if c['zone'] == 'graveyard')
            else:
                quantity = 1 if self.csvid == GEMINI else 2
        self.token_count(quantity)
        for index in range(quantity):
            ref = f'token_{index}'
            self.cards[ref] = dict(name=self.token_name, player=self.actor, zone='battlefield')
            self.tokens.append(ref)
        if quantity == 0:
            return 0
        token_power, token_toughness = {GEMINI: (power, toughness), HERO: (2, 1),
                                        KESSIG: (2, 2), WARLEADER: (1, 1)}[self.csvid]
        self.assert_token(token=1, artifact=int(self.token_artifact), color=self.token_color,
                          power=token_power, toughness=token_toughness, attacking=1,
                          tapped=int(self.csvid != GEMINI), **{f'subtype_{self.token_type}': 1})
        return token_power

    def bind_token(self, index):
        self.emit('bind_permanent_copy', self.tokens[index], player=self.actor, index=index, value=self.token_id)
        return self.tokens[index]

    def blockers(self):
        if self.mode == 'blocked_source':
            return [('blocker', 'source')]
        if self.mode in ('blocked_cat', 'cat_dies'):
            return [('blocker', self.bind_token(0))]
        if self.mode == 'soldiers_blocked':
            return [('blocker', self.bind_token(0)), ('second_blocker', self.bind_token(1))]
        return []

    def build(self):
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        self.emit('turn', player=self.actor)
        self.cast_to('source', 'battlefield')
        self.observe('source', 'in_play', 1)
        self.source_stats(self.source_power, self.source_toughness)
        self.token_count(0)
        self.go(self.foe)
        self.go(self.actor)
        self.observe('source', 'sick', 0)
        if self.mode == 'own_death':
            self.cast_to('bolt', 'graveyard', 'victim')
            self.cards['victim']['zone'] = 'graveyard'
            self.observe('victim', 'in_play', 0)
            self.emit('assert_zone_multiplicity', player=self.actor, zone='graveyard',
                      index=self.rows[PARTNER]['csvid'], value=1)

        power, toughness = self.source_power, self.source_toughness
        if self.mode == 'boosted':
            self.cast_to('growth', 'graveyard', 'source')
            power, toughness = power + 3, toughness + 3
        source_attacks = self.mode != 'other_only'
        partner_attacks = self.mode in ('teammate', 'other_only')
        self.emit('begin_attackers')
        if source_attacks:
            self.emit('attack', 'source', value=1)
        if partner_attacks:
            self.emit('attack', 'partner', value=1)
        self.emit('finish_attackers')
        self.settle()
        self.observe('source', 'attacking', int(source_attacks))
        self.observe('source', 'tapped', int(source_attacks))
        self.source_stats(power, toughness)
        token_power = self.expect_tokens(source_attacks, power, toughness)
        if self.mode == 'later_growth':
            self.cast_to('growth', 'graveyard', 'source')
            power, toughness = power + 3, toughness + 3
            self.source_stats(power, toughness)
            self.assert_token(power=token_power, toughness=self.source_toughness)
        partner_power = 1 + int(self.csvid == HERO and source_attacks)
        if partner_attacks:
            self.observe('partner', 'power', partner_power)
        self.emit('assert_life', player=self.actor, value=20)
        self.emit('assert_life', player=self.foe, value=20)

        blocks = self.blockers()
        self.emit('begin_blockers')
        for blocker, attacker in blocks:
            self.emit('block', blocker, attacker, value=1)
        self.emit('advance_combat')
        for _, attacker in blocks:
            self.observe(attacker, 'blocked', 1)
        self.emit('combat_damage')
        self.settle()
        blocked = {attacker for _, attacker in blocks}
        if source_attacks and 'source' not in blocked:
            self.life[self.foe] -= power
        if partner_attacks:
            self.life[self.foe] -= partner_power
        self.life[self.foe] -= token_power * len(set(self.tokens) - blocked)
        if self.csvid == WARLEADER:
            self.life[self.actor] += len(self.tokens)
        if self.mode == 'cat_dies':
            self.cards['blocker']['zone'] = 'graveyard'
            self.cards[self.tokens[0]]['zone'] = 'absent'
            self.observe('blocker', 'in_play', 0)
        surviving_tokens = len(self.tokens) - int(self.mode == 'cat_dies')
        self.token_count(surviving_tokens)
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
        self.emit('end_combat')
        self.settle()
        if self.csvid == GEMINI:
            for token in self.tokens:
                self.cards[token]['zone'] = 'absent'
            surviving_tokens = 0
        self.token_count(surviving_tokens)
        if surviving_tokens:
            self.assert_token(attacking=0, tapped=1)
        if self.mode == 'late_teammate':
            self.cast_to('partner', 'battlefield')
            self.observe('partner', 'power', 1)
        self.receipts()
        if (self.csvid == HERO and source_attacks) or self.mode in ('boosted', 'later_growth', 'next_untap'):
            self.go(self.foe)
            if self.csvid == HERO:
                self.assert_token(power=1)
                if partner_attacks:
                    self.observe('partner', 'power', 1)
            self.source_stats(self.source_power, self.source_toughness)
            if self.mode == 'next_untap':
                self.go(self.actor)
                self.assert_token(tapped=0, sick=0, power=2, toughness=2)
            self.receipts()



PRINTED.update({9406: ('morningtide.c',
        '{2}{W}',
        'Creature — Kithkin Soldier',
        (2, 2),
        ('First strike',),
        'First strike (This creature deals combat damage before creatures without first strike.)\n'
        'Whenever this creature attacks, you may put a Soldier creature card from your hand onto the '
        'battlefield tapped and attacking.'),
 8164: ('guildpact.c',
        '{W}{U}{B}{R}',
        'Creature — Nephilim',
        (2, 2),
        (),
        'Whenever this creature attacks, return target creature card from your graveyard to the battlefield '
        'tapped and attacking.'),
 14550: ('fate_reforged.c',
         '{4}{R}',
         'Creature — Human Warrior',
         (3, 3),
         ('Dash',),
         "Whenever this creature attacks, create a token that's a copy of another target attacking creature "
         "and that's tapped and attacking. Exile the token at end of combat.\n"
         "Dash {2}{R}{R} (You may cast this spell for its dash cost. If you do, it gains haste, and it's "
         "returned from the battlefield to its owner's hand at the beginning of the next end step.)"),
 18318: ('rivals_of_ixalan.c',
         '{1}{R}',
         'Creature — Human Shaman',
         (1, 1),
         ('Ascend',),
         "Ascend (If you control ten or more permanents, you get the city's blessing for the rest of the "
         'game.)\n'
         'Whenever this creature attacks, you may pay {X}{R}. If you do, create X 1/1 red Elemental creature '
         'tokens that are tapped and attacking. At the beginning of the next end step, exile those tokens '
         "unless you have the city's blessing."),
 15869: ('shadows_over_innistrad.c',
         '{1}{W}{U}',
         'Enchantment — Aura',
         None,
         ('Enchant',),
         'Enchant creature\n'
         'Enchanted creature has "Whenever this creature attacks, create a 4/4 white Angel creature token '
         'with flying that\'s tapped and attacking. Exile that token at end of combat."'),
 14109: ('m15.c',
         '{2}{R}',
         'Creature — Goblin Warrior',
         (2, 2),
         (),
         'Other Goblin creatures you control attack each combat if able.\n'
         'At the beginning of combat on your turn, create a 1/1 red Goblin creature token with haste.\n'
         'Whenever this creature attacks, it gets +1/+0 until end of turn for each other attacking Goblin.'),
 13926: ('journey_into_nyx.c',
         '{W}',
         'Sorcery',
         None,
         ('Strive',),
         'Strive — This spell costs {1} more to cast for each target beyond the first.\n'
         'Until end of turn, any number of target creatures each gain "Whenever this creature attacks, '
         'create a 1/1 white Soldier creature token that\'s tapped and attacking."'),
 14054: ('conspiracy.c',
         '{4}{R}{R}',
         'Creature — Dragon',
         (5, 5),
         ('Flying', 'Dethrone'),
         'Flying\n'
         'Dethrone (Whenever this creature attacks the player with the most life or tied for most life, put '
         'a +1/+1 counter on it.)\n'
         "Whenever this creature attacks for the first time each turn, if it's attacking the player with the "
         'most life or tied for most life, untap all attacking creatures. After this phase, there is an '
         'additional combat phase.'),
 10566: ('zendikar.c',
         '{4}{R}{R}',
         'Creature — Dragon',
         (5, 5),
         ('Flying', 'Haste'),
         'Flying, haste\n'
         'Whenever this creature attacks, you may pay {5}{R}{R}. If you do, untap all attacking creatures '
         'and after this phase, there is an additional combat phase.'),
 14326: ('khans_of_tarkir.c',
         '{3}{G}{G}',
         'Creature — Turtle',
         (5, 9),
         ('Landwalk', 'Islandwalk'),
         "Islandwalk (This creature can't be blocked as long as defending player controls an Island.)\n"
         'Whenever this creature attacks, exile it. Return it to the battlefield under your control tapped '
         'and attacking at the beginning of the declare attackers step on your next turn.'),
 16746: ('aether_revolt.c',
         '{3}{R}{R}',
         'Creature — Human Warrior',
         (2, 2),
         ('Haste', 'Double strike'),
         'Double strike, haste\n'
         'Whenever this creature attacks, you get {E}{E} (two energy counters), then you may pay eight {E}. '
         'If you pay, untap all creatures you control, and after this phase, there is an additional combat '
         'phase.')})

CAPTAIN, YORE, FLAMERUSH, SUMMONER, INVOCATION = 9406, 8164, 14550, 18318, 15869
RABBLE, FLEET, SCOURGE, CHARGER, TURTLE, RUNNER = 14109, 13926, 14054, 10566, 14326, 16746
FOLLOWERS = {
    CAPTAIN: ('soldier', 'decline', 'foe_soldier', 'non_soldier', 'first_strike'),
    YORE: ('return', 'foe_creature', 'grave_shroud', 'noncreature'),
    FLAMERUSH: ('copy', 'alone', 'dash', 'original_bounced'),
    SUMMONER: ('unfunded', 'x_one', 'x_two', 'blessing', 'decline'),
    INVOCATION: ('normal', 'air_block', 'foe_host', 'aura_removed'),
    RABBLE: ('others', 'non_goblin', 'other_only'),
    FLEET: ('two', 'zero', 'stop_one', 'other_only', 'expiry'),
    SCOURGE: ('tie', 'foe_ahead', 'own_ahead', 'other_only'),
    CHARGER: ('pay', 'unfunded', 'decline', 'underpaid', 'flying'),
    TURTLE: ('exiled', 'other_only', 'islandwalk'),
    RUNNER: ('below_eight', 'pay', 'decline', 'other_only'),
}
HUMAN_ONLY = {(CAPTAIN, 'decline'), (SUMMONER, 'decline'), (CHARGER, 'decline')}
AI_ONLY = {(CHARGER, 'unfunded'), (CHARGER, 'underpaid')}
TIMING = {
    RABBLE: ('opening_solo', 'opening_sick_goblin', 'opening_foe_turn'),
    TURTLE: ('return_island', 'return_no_island'),
    SCOURGE: ('extra_tie', 'extra_foe_ahead', 'extra_own_ahead'),
    CHARGER: ('extra_pay', 'extra_decline', 'extra_short'),
    RUNNER: ('extra_pay', 'extra_decline'),
}
HUMAN_ONLY.add((CHARGER, 'extra_decline'))
AI_ONLY.add((CHARGER, 'extra_short'))
TOKEN_IDENTITIES = {data[0]: data[1] for data in TOKENS.values()} | {'Angel': 18687, 'Elemental': 18726, 'Goblin': 18746}
WITNESSES = {PARTNER, WALL, SENTRY, GHOUL, DRIFT, DISENCHANT, STONE, BOUNCE,
             GOBLIN, SERVANT, SHIELD, GROWTH, BOLT, ICY, 'Island', 'Mountain'}
GAPS = {
    HERO: ['alternate ordering of the two simultaneous source triggers'],
    FLAMERUSH: ['trigger target legality cannot be inspected independently of the actual selection'],
    SUMMONER: ['permanent City’s Blessing status is observed through token retention, with no direct flag assertion'],
    RABBLE: ['beginning-of-combat token generation and its same-step required attack are outside the raw declaration driver'],
    SCOURGE: ['extra-combat phase transition and multiple same-turn declarations are outside the driver'],
    CHARGER: ['extra-combat phase transition is outside the driver'],
    TURTLE: ['the normal declaration driver omits the preceding opening combat event used by the engine’s delayed return',
             'Islandwalk can only be observed while the creature survives to the blocking phase'],
    RUNNER: ['extra-combat phase transition is outside the driver'],
}


class FollowDuel(Duel):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.active = actor
        self.library = {0: [], 1: []}
        self.add('source', actor, record['name'], 'hand')

    def build(self):
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        self.emit('turn', player=self.actor)
        actions = {CAPTAIN: self.captain, YORE: self.yore, FLAMERUSH: self.flamerush, SUMMONER: self.summoner,
                   INVOCATION: self.invocation, RABBLE: self.rabble, FLEET: self.fleet, SCOURGE: self.scourge,
                   CHARGER: self.charger, TURTLE: self.turtle, RUNNER: self.runner}
        actions[self.csvid]()

    def age(self):
        self.go(1 - self.active)
        self.go(1 - self.active)

    def start(self, attackers):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref, value=1)

    def finish(self):
        self.emit('finish_attackers')
        self.settle()

    def combat(self, damage, block=(), *, observe_before_end=None, scheduled=None):
        self.emit('begin_blockers')
        for blocker, attacker, legal in block:
            self.emit('block', blocker, attacker, value=int(legal))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.settle()
        self.life[1 - self.active] -= damage
        self.emit('assert_life', player=1 - self.active, value=self.life[1 - self.active])
        if observe_before_end:
            observe_before_end()
        if scheduled is None:
            self.emit('end_combat')
        else:
            self.emit('end_combat_step', value=int(scheduled))
        self.settle()

    def bound(self, ref, owner, name, ordinal=0):
        self.emit('bind_permanent_copy', ref, player=owner, index=ordinal, value=self.rows[name]['csvid'])

    def token_receipt(self, owner, name, count, **properties):
        token_id = self.rows[name]['csvid']
        self.emit('assert_zone_multiplicity', player=owner, zone='battlefield', index=token_id, value=count)
        if count:
            for prop, value in properties.items():
                self.emit('assert_matching_permanents', player=owner, index=token_id, prop=prop, value=value)

    def token_add(self, name, owner, count):
        refs = []
        for i in range(count):
            ref = f'created_{i}'
            self.cards[ref] = dict(name=name, player=owner, zone='battlefield')
            refs.append(ref)
        return refs

    def move(self, refs, zone):
        for ref in refs:
            self.cards[ref]['zone'] = zone

    def captain(self):
        if self.mode in ('soldier', 'decline', 'foe_soldier'):
            owner = self.foe if self.mode == 'foe_soldier' else self.actor
            self.add('soldier', owner, SENTRY, 'hand')
        if self.mode in ('soldier', 'non_soldier'):
            self.add('wrong', self.actor, PARTNER, 'hand')
        if self.mode == 'first_strike':
            self.add('blocker', self.foe, GHOUL, 'battlefield')
        self.cast_to('source', 'battlefield')
        self.age()
        self.start(('source',))
        if self.actor == 0 and self.mode in ('soldier', 'non_soldier', 'foe_soldier', 'first_strike'):
            self.emit('choose_trigger_source', 'source', player=self.actor)
        if self.mode == 'soldier':
            self.emit('choose_zone_card', player=self.actor, value=self.rows[SENTRY]['csvid'])
        elif self.mode == 'non_soldier':
            # The optional picker can open even with only an ineligible card in hand.
            self.emit('observe_search_filters', value=1)
            self.emit('choose_optional_zone', player=self.actor, value=self.rows[PARTNER]['csvid'])
        self.finish()
        if self.mode == 'non_soldier':
            self.emit('assert_offered_legal_card', index=self.rows[PARTNER]['csvid'], value=0)
            self.emit('assert_forced_zone_choices', value=0)
            self.emit('observe_search_filters', value=0)
        if self.mode == 'soldier':
            self.move(('soldier',), 'battlefield')
            self.bound('deployed', self.actor, SENTRY)
            self.observe('deployed', 'tapped', 1)
            self.observe('deployed', 'attacking', 1)
        if self.mode == 'first_strike':
            self.combat(0, [('blocker', 'source', True)])
            self.move(('blocker',), 'graveyard')
            self.observe('source', 'in_play', 1)
        else:
            self.combat(2 + int(self.mode == 'soldier'))
        self.receipts()

    def yore(self):
        self.add('land', self.actor, 'Mountain', 'graveyard')
        if self.mode != 'noncreature':
            who = self.foe if self.mode == 'foe_creature' else self.actor
            self.add('creature', who, PARTNER, 'graveyard')
        if self.mode == 'grave_shroud':
            self.add('shield', self.foe, SHIELD, 'battlefield')
        self.cast_to('source', 'battlefield')
        self.age()
        self.start(('source',))
        if self.mode == 'return':
            self.emit('choose_zone_card', player=self.actor, value=self.rows[PARTNER]['csvid'])
        self.finish()
        if self.mode == 'return':
            self.move(('creature',), 'battlefield')
            self.bound('reanimated', self.actor, PARTNER)
            self.observe('reanimated', 'tapped', 1)
            self.observe('reanimated', 'attacking', 1)
        self.combat(2 + int(self.mode == 'return'))
        self.receipts()

    def flamerush(self):
        copied = self.mode != 'alone'
        if copied:
            self.add('partner', self.actor, PARTNER, 'battlefield')
        if self.mode == 'original_bounced':
            self.add('bounce', self.actor, BOUNCE, 'hand')
        if self.mode == 'dash':
            self.mana('set_mana', self.actor, (2, 0, 0, 0, 2, 0))
            self.emit('refresh')
            self.emit('cast', 'source', value=1)
            self.emit('assert_stack', value=1)
            self.mana('assert_mana', self.actor, ZERO)
            self.settle()
            self.move(('source',), 'battlefield')
        else:
            self.cast_to('source', 'battlefield')
            self.age()
        self.start(('source', 'partner') if copied else ('source',))
        if copied:
            self.emit('choose_target', 'partner', player=self.actor)
        self.finish()
        if copied:
            refs = self.token_add(PARTNER, self.actor, 1)
            self.bound('copy', self.actor, PARTNER, ordinal=1)
            for prop, value in (('attacking', 1), ('tapped', 1), ('power', 1), ('toughness', 1), ('color', 4)):
                self.observe('copy', prop, value)
            if self.mode == 'original_bounced':
                self.cast_to('bounce', 'graveyard', 'partner')
                self.move(('partner',), 'hand')
                self.token_receipt(self.actor, PARTNER, 1, token=1, attacking=1, tapped=1, subtype_Wizard=1)
        self.combat(3 + (1 if self.mode == 'original_bounced' else 2 if copied else 0))
        if copied:
            self.move(refs, 'absent')
        self.receipts()
        self.go(self.foe)
        if self.mode == 'dash':
            self.move(('source',), 'hand')
        self.receipts()

    def summoner(self):
        quantity = {'x_one': 1, 'x_two': 2, 'blessing': 2}.get(self.mode, 0)
        if self.mode == 'blessing':
            for i in range(9):
                self.add(f'permanent_{i}', self.actor, STONE, 'battlefield')
        self.cast_to('source', 'battlefield')
        self.age()
        available = (quantity, 0, 0, 0, 1, 0) if quantity else (2, 0, 0, 0, 1, 0) if self.mode == 'decline' else ZERO
        self.mana('set_mana', self.actor, available)
        self.emit('refresh')
        self.start(('source',))
        if quantity and self.actor == 0:
            self.emit('choose_trigger_source', 'source', player=self.actor)
        if quantity:
            self.emit('choose_x', player=self.actor, value=quantity)
        self.finish()
        self.mana('assert_mana', self.actor, ZERO if quantity else available)
        if self.mode == 'decline':
            self.mana('set_mana', self.actor, ZERO)
        refs = self.token_add('Elemental', self.actor, quantity)
        self.token_receipt(self.actor, 'Elemental', quantity, token=1, power=1, toughness=1, color=16,
                           tapped=1, attacking=1, subtype_Elemental=1)
        self.combat(1 + quantity)
        self.token_receipt(self.actor, 'Elemental', quantity)
        self.go(self.foe)
        if self.mode != 'blessing':
            self.move(refs, 'absent')
        self.receipts()

    def invocation(self):
        owner = self.foe if self.mode == 'foe_host' else self.actor
        enemy = 1 - owner
        self.add('host', owner, PARTNER, 'battlefield')
        if self.mode == 'air_block':
            self.add('ground', enemy, WALL, 'battlefield')
            self.add('air', enemy, DRIFT, 'battlefield')
        if self.mode == 'aura_removed':
            self.add('removal', self.actor, DISENCHANT, 'hand')
        self.cast_to('source', 'battlefield', 'host')
        if self.mode == 'aura_removed':
            self.cast_to('removal', 'graveyard', 'source')
            self.move(('source',), 'graveyard')
        if self.active != owner:
            self.go(owner)
        self.start(('host',))
        self.finish()
        quantity = int(self.mode != 'aura_removed')
        refs = self.token_add('Angel', owner, quantity)
        self.token_receipt(owner, 'Angel', quantity, token=1, power=4, toughness=4, color=32,
                           tapped=1, attacking=1, subtype_Angel=1)
        blocks = []
        if self.mode == 'air_block':
            self.bound('angel', owner, 'Angel')
            blocks = [('ground', 'angel', False), ('air', 'angel', True)]
        self.combat(1 + (4 if quantity and not blocks else 0), blocks)
        self.move(refs, 'absent')
        self.receipts()

    def rabble(self):
        if self.mode != 'non_goblin':
            self.add('goblin', self.actor, GOBLIN, 'battlefield')
        self.add('partner', self.actor, PARTNER, 'battlefield')
        self.add('foe_goblin', self.foe, GOBLIN, 'battlefield')
        self.cast_to('source', 'battlefield')
        self.age()
        if self.mode == 'other_only':
            attackers = ('goblin',)
        elif self.mode == 'non_goblin':
            attackers = ('source', 'partner')
        else:
            attackers = ('source', 'goblin', 'partner')
        self.start(attackers)
        self.finish()
        source_power = 3 if self.mode == 'others' else 2
        self.observe('source', 'power', source_power)
        if self.mode != 'non_goblin':
            self.observe('goblin', 'power', 1)
        self.observe('partner', 'power', 1)
        self.combat(sum(source_power if ref == 'source' else 1 for ref in attackers))
        self.go(self.foe)
        self.observe('source', 'power', 2)
        self.receipts()

    def fleet(self):
        self.add('first', self.actor, PARTNER, 'battlefield')
        self.add('second', self.actor, GOBLIN, 'battlefield')
        selected = {'two': ('first', 'second'), 'zero': (), 'stop_one': ('first',),
                    'other_only': ('first',), 'expiry': ('first',)}[self.mode]
        generic = int(self.mode in ('two', 'stop_one', 'zero'))
        self.mana('set_mana', self.actor, (generic, 0, 0, 0, 0, 1))
        self.emit('refresh')
        for target in selected:
            self.emit('choose_target', target, player=self.actor)
        if self.mode in ('stop_one', 'zero'):
            self.emit('choose_done_target', player=self.actor)
        self.emit('cast', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, (generic - int(len(selected) == 2), 0, 0, 0, 0, 0))
        self.settle()
        self.move(('source',), 'graveyard')
        self.mana('set_mana', self.actor, ZERO)
        if self.mode == 'expiry':
            self.age()
        attackers = ('second',) if self.mode == 'other_only' else ('first', 'second')
        self.start(attackers)
        self.finish()
        quantity = sum(ref in selected for ref in attackers) if self.mode != 'expiry' else 0
        self.token_add('Soldier', self.actor, quantity)
        self.token_receipt(self.actor, 'Soldier', quantity, token=1, power=1, toughness=1, color=32,
                           tapped=1, attacking=1, subtype_Soldier=1)
        self.combat(len(attackers) + quantity)
        self.receipts()

    def scourge(self):
        self.add('partner', self.actor, PARTNER, 'battlefield')
        self.add('ground', self.foe, WALL, 'battlefield')
        if self.mode == 'own_ahead':
            self.life[self.actor] = 24
        elif self.mode == 'foe_ahead':
            self.life[self.foe] = 24
        for p in (0, 1):
            self.emit('set_life', player=p, value=self.life[p])
        self.cast_to('source', 'battlefield')
        self.age()
        self.start(('partner',) if self.mode == 'other_only' else ('source', 'partner'))
        self.finish()
        expected = int(self.mode in ('tie', 'foe_ahead'))
        self.observe('source', 'plus1_counters', expected)
        self.observe('source', 'power', 5 + expected)
        self.observe('source', 'toughness', 5 + expected)
        self.observe('source', 'tapped', int(self.mode == 'own_ahead'))
        self.observe('partner', 'tapped', 1 - expected)
        if self.mode == 'other_only':
            self.combat(1)
        else:
            self.combat(6 + expected, [('ground', 'source', False)])
        self.receipts()

    def charger(self):
        self.add('partner', self.actor, PARTNER, 'battlefield')
        if self.mode == 'flying':
            self.add('ground', self.foe, WALL, 'battlefield')
            self.add('air', self.foe, DRIFT, 'battlefield')
        self.cast_to('source', 'battlefield')
        funds = (5, 0, 0, 0, 2, 0) if self.mode in ('pay', 'decline') else (4, 0, 0, 0, 2, 0) if self.mode == 'underpaid' else ZERO
        self.mana('set_mana', self.actor, funds)
        self.emit('refresh')
        self.start(('source', 'partner'))
        if self.mode == 'pay' and self.actor == 0:
            self.emit('choose_trigger_source', 'source', player=self.actor)
        self.finish()
        paid = self.mode == 'pay'
        self.mana('assert_mana', self.actor, ZERO if paid else funds)
        self.observe('source', 'tapped', int(not paid))
        self.observe('partner', 'tapped', int(not paid))
        if self.mode == 'flying':
            self.combat(1, [('ground', 'source', False), ('air', 'source', True)])
            self.move(('air',), 'graveyard')
        else:
            self.combat(6)
        self.receipts()

    def turtle(self):
        self.add('partner', self.actor, PARTNER, 'battlefield')
        if self.mode == 'islandwalk':
            self.add('island', self.foe, 'Island', 'battlefield')
            self.add('wall', self.foe, WALL, 'battlefield')
        self.cast_to('source', 'battlefield')
        self.age()
        if self.mode == 'islandwalk':
            # The self-exile resolves before the blocking step here.
            self.observe('wall', 'in_play', 1)
        attacking = self.mode != 'other_only'
        self.start(('source', 'partner') if attacking else ('partner',))
        self.finish()
        if attacking:
            self.move(('source',), 'exile')
            self.observe('source', 'in_play', 0)
            self.emit('assert_zone_multiplicity', player=self.actor, zone='exile', index=self.csvid, value=1)
        self.combat(1)
        self.go(self.foe)
        self.receipts()

    def runner(self):
        self.add('partner', self.actor, PARTNER, 'battlefield')
        quantity = 3 if self.mode in ('pay', 'decline') else 2 if self.mode == 'below_eight' else 0
        for i in range(quantity):
            ref = f'servant_{i}'
            self.add(ref, self.actor, SERVANT, 'hand')
            self.cast_to(ref, 'battlefield')
        self.cast_to('source', 'battlefield')
        attacking = self.mode != 'other_only'
        self.start(('source', 'partner') if attacking else ('partner',))
        if self.mode in ('pay', 'decline'):
            self.emit('choose_dialog', player=self.actor, value=int(self.mode == 'decline'))
        self.finish()
        self.emit('assert_energy', player=self.actor, value=2 * quantity + 2 * int(attacking) - 8 * int(self.mode == 'pay'))
        self.emit('assert_energy', player=self.foe, value=0)
        self.observe('source', 'tapped', int(attacking and self.mode != 'pay'))
        self.observe('partner', 'tapped', int(self.mode != 'pay'))
        self.combat(5 if attacking else 1)
        self.receipts()


class TimingDuel(FollowDuel):
    def rabble(self):
        if self.mode == 'opening_sick_goblin':
            self.add('sick_goblin', self.actor, GOBLIN, 'hand')
        elif self.mode == 'opening_foe_turn':
            self.add('foe_source', self.foe, self.record['name'], 'hand')
        self.cast_to('source', 'battlefield')
        if self.mode == 'opening_sick_goblin':
            self.cast_to('sick_goblin', 'battlefield')
        if self.mode == 'opening_foe_turn':
            self.go(self.foe)
            self.cast_to('foe_source', 'battlefield')
            self.emit('begin_required_attackers')
            self.token_receipt(self.actor, 'Goblin', 0)
            self.token_add('Goblin', self.foe, 1)
            self.token_receipt(self.foe, 'Goblin', 1, token=1, tapped=0, attacking=0, power=1, toughness=1)
            self.bound('new_goblin', self.foe, 'Goblin')
            self.emit('can_finish_attackers', value=0)
            self.emit('attack', 'new_goblin', value=1)
            self.emit('can_finish_attackers', value=1)
            self.finish()
            self.combat(1, scheduled=False)
            self.receipts()
            return

        self.emit('begin_required_attackers')
        self.token_add('Goblin', self.actor, 1)
        self.token_receipt(self.actor, 'Goblin', 1, token=1, color=16, power=1, toughness=1,
                           attacking=0, tapped=0, subtype_Goblin=1)
        self.bound('new_goblin', self.actor, 'Goblin')
        self.emit('can_finish_attackers', value=0)
        self.emit('attack', 'source', value=0)
        if self.mode == 'opening_sick_goblin':
            self.emit('attack', 'sick_goblin', value=0)
        self.emit('attack', 'new_goblin', value=1)
        self.emit('can_finish_attackers', value=1)
        self.finish()
        self.observe('new_goblin', 'tapped', 1)
        self.combat(1, scheduled=False)
        self.token_receipt(self.actor, 'Goblin', 1, attacking=0, tapped=1)
        self.receipts()

    def turtle(self):
        has_island = self.mode == 'return_island'
        self.add('wall', self.foe, WALL, 'battlefield')
        if has_island:
            self.add('island', self.foe, 'Island', 'battlefield')
        self.cast_to('source', 'battlefield')
        self.age()
        self.start(('source',))
        self.finish()
        self.move(('source',), 'exile')
        self.receipts()
        self.combat(0)
        self.go(self.foe)
        self.emit('begin_required_attackers')
        self.receipts()
        self.finish()
        self.combat(0)
        self.go(self.actor)
        self.receipts()
        self.emit('begin_required_attackers')
        self.move(('source',), 'battlefield')
        self.bound('returned', self.actor, self.record['name'])
        self.observe('returned', 'attacking', 1)
        self.observe('returned', 'tapped', 1)
        self.receipts()
        self.finish()
        self.combat(5 if has_island else 0, [('wall', 'returned', not has_island)])
        self.observe('returned', 'in_play', 1)
        self.receipts()

    def second_attack(self, attackers):
        # The end-combat scheduler already opened the next declare-attacker step.
        for ref in attackers:
            self.emit('attack', ref, value=1)
        self.finish()

    def scourge(self):
        self.add('partner', self.actor, PARTNER, 'battlefield')
        self.add('ground', self.foe, WALL, 'battlefield')
        if self.mode == 'extra_own_ahead':
            self.life[self.actor] = 32
        elif self.mode == 'extra_foe_ahead':
            self.life[self.foe] = 32
        for player in (0, 1):
            self.emit('set_life', player=player, value=self.life[player])
        self.cast_to('source', 'battlefield')
        self.age()
        attackers = ('source', 'partner')
        self.start(attackers)
        self.finish()
        extra = self.mode != 'extra_own_ahead'
        self.observe('source', 'plus1_counters', int(extra))
        for ref in attackers:
            self.observe(ref, 'tapped', int(not extra))
        # The transition itself is the receipt. A second declaration requires
        # the scheduler to have opened that phase.
        self.combat(6 + int(extra), [('ground', 'source', False)], scheduled=extra)
        self.receipts()

    def charger(self):
        self.add('partner', self.actor, PARTNER, 'battlefield')
        self.cast_to('source', 'battlefield')
        paid = self.mode == 'extra_pay'
        pool = (4 if self.mode == 'extra_short' else 5, 0, 0, 0, 2, 0)
        self.mana('set_mana', self.actor, pool)
        self.emit('refresh')
        attackers = ('source', 'partner')
        self.start(attackers)
        if paid and self.actor == 0:
            self.emit('choose_trigger_source', 'source', player=self.actor)
        self.finish()
        self.mana('assert_mana', self.actor, ZERO if paid else pool)
        for ref in attackers:
            self.observe(ref, 'tapped', int(not paid))
        self.combat(6, scheduled=paid)
        self.mana('assert_mana', self.actor, ZERO)
        if paid:
            self.second_attack(attackers)
            for ref in attackers:
                self.observe(ref, 'tapped', 1)
            self.combat(6, scheduled=False)
        self.receipts()

    def runner(self):
        self.add('partner', self.actor, PARTNER, 'battlefield')
        self.add('resting', self.actor, GOBLIN, 'battlefield')
        self.add('tapper', self.actor, ICY, 'hand')
        self.cast_to('tapper', 'battlefield')
        self.mana('set_mana', self.actor, (1, 0, 0, 0, 0, 0))
        self.emit('refresh')
        self.emit('choose_target', 'resting', player=self.actor)
        self.emit('activate', 'tapper', value=1)
        self.settle()
        self.mana('assert_mana', self.actor, ZERO)
        self.observe('resting', 'tapped', 1)
        for i in range(3):
            ref = f'servant_{i}'
            self.add(ref, self.actor, SERVANT, 'hand')
            self.cast_to(ref, 'battlefield')
        self.emit('assert_energy', player=self.actor, value=6)
        self.cast_to('source', 'battlefield')
        attackers = ('source', 'partner')
        self.start(attackers)
        paid = self.mode == 'extra_pay'
        self.emit('choose_dialog', player=self.actor, value=int(not paid))
        self.finish()
        self.emit('assert_energy', player=self.actor, value=0 if paid else 8)
        for ref in (*attackers, 'resting'):
            self.observe(ref, 'tapped', int(not paid))
        self.combat(5, scheduled=paid)
        if paid:
            self.second_attack(attackers)
            self.emit('assert_energy', player=self.actor, value=2)
            self.combat(5, scheduled=False)
        self.receipts()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    expected = PRINTED.get(csvid)
    observed = (record.get('mana_cost'), record.get('type_line'), stats(record) if 'power' in record else None,
                tuple(record.get('keywords', ())), record.get('oracle_text'))
    if expected is None or observed != expected[1:] or record.get('layout') != 'normal' or record.get('card_faces'):
        raise ValueError('complete printed attack-token or combat reward record required')
    return dict(csvid=csvid, printed_cost=expected[1], keywords=list(expected[4]))


def variants(shape):
    csvid = shape['csvid']
    if csvid in MODES:
        return [f'{mode}_{actor}' for actor in (0, 1) for mode in MODES[csvid]]
    return [f'{mode}_{actor}' for actor in (0, 1) for mode in (*FOLLOWERS[csvid], *TIMING.get(csvid, ()))
            if (csvid, mode) not in (AI_ONLY if actor == 0 else HUMAN_ONLY)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed attack-token or combat reward variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name', '')) if record else None
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned attack-token or combat reward card required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 attack-token or combat reward recipe required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses', 'token_identities'} or set(pins['witnesses']) != WITNESSES
            or {p['registry']['csvid'] for p in pins['sources'].values()} != set(PRINTED)
            or set(pins['token_identities']) != set(TOKEN_IDENTITIES)):
        raise ValueError('complete attack-token source, witness, and token identity pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal' or oracle[name].get('card_faces')):
            raise ValueError('pinned attack-token source or witness changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])
    for name, csvid in TOKEN_IDENTITIES.items():
        pin = pins['token_identities'][name]
        if (not exact(rows.get(name), pin['registry']) or not exact(oracle.get(name), pin['available_oracle_token_record'])
                or pin['registry']['csvid'] != csvid):
            raise ValueError('pinned generated token identity changed: ' + name)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered attack-token or combat reward card required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        timing = mode in TIMING.get(csvid, ())
        model = Duel if csvid in MODES else TimingDuel if timing else FollowDuel
        board = model(csvid, oracle[name], int(actor), mode, rows, oracle)
        board.build()
        recipe = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                      oracle_text=declaration['oracle_text'])
        if csvid in GAPS and not timing:
            recipe['untested_components'] = GAPS[csvid]
        cases.append(dict(name=label(csvid, mode, actor), card=name, fixture='empty_main', seed=3012,
                          setup=board.setup, steps=board.steps, _recipe=recipe))
    return cases
