"""Provoke, untap, protecting abilities and actual cast eligibility."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step

FAMILY = 'attack_follow_on_requirements_and_legality'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = (0,) * 6


class CombatBoard:
    def add(self, ref, name, player, zone, *, tapped=False):
        assert ref not in self.cards and player in (0, 1)
        self.cards[ref] = dict(name=name, player=player, zone=zone)
        flags = (1 if tapped else 8) if zone == 'battlefield' else 0
        entry = card(name, ref, player, zone)
        if flags:
            entry['state'] = ['tapped' if tapped else 'untapped']
        self.setup.append(entry)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('choose_trigger_source', 'untap_step'):
            self.steps.append(step(op, **({'ref': ref, 'player': player} if ref else {'player': player})))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        values = dict(ref=ref, other=other, property=prop, player=player, value=value,
                      zone=next(name for name, code in ZONES.items() if code == zone), index=index, color=color)
        self.steps.append(step(op, **{key: item for key, item in values.items() if item != ''}))

    def prop(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def fund(self, player, pool):
        self.wallet[player] = tuple(pool)
        for color, amount in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=amount)
        self.emit('refresh')

    def pools(self):
        for who, pool in enumerate(self.wallet):
            self.emit('assert_life', player=who, value=self.life[who])
            for color, amount in enumerate(pool):
                self.emit('assert_mana', player=who, color=color, value=amount)

    def move(self, ref, zone):
        self.cards[ref]['zone'] = zone

    def receipt(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.pools()
        for who in (0, 1):
            for zone, code in ZONES.items():
                names = [card['name'] for card in self.cards.values() if card['zone'] == zone and card['player'] == who]
                self.emit('assert_zone', player=who, zone=code, value=len(names))
                for name in sorted(set(names)):
                    self.emit('assert_zone_multiplicity', player=who, zone=code,
                              index=self.rows[name]['csvid'], value=names.count(name))

    def cast(self, ref, *, target=None, destination='graveyard', resolve_choices=()):
        card = self.cards[ref]
        who = card['player']
        self.fund(who, cost_from_oracle(self.oracle[card['name']]))
        if target is not None:
            self.emit('choose_target', target, player=who)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.wallet[who] = ZERO
        self.pools()
        for op, player, value in resolve_choices:
            self.emit(op, player=player, value=value)
        self.emit('resolve')
        self.move(ref, destination)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def stats(self, ref):
        r = self.oracle[self.cards[ref]['name']]
        return int(r['power']), int(r['toughness'])

    def size(self, ref, stats=None):
        self.prop(ref, 'in_play', 1)
        power, tough = self.stats(ref) if stats is None else stats
        self.prop(ref, 'power', power)
        self.prop(ref, 'toughness', tough)

    def round_trip_turn(self):
        self.emit('advance_turn')
        self.emit('advance_turn')

    def combat(self, attacker, *, attempts=(), blocked=None, actual_stats=None, after_advance=None):
        self.emit('begin_blockers')
        for blocker, allowed in attempts:
            self.emit('block', blocker, attacker, value=int(allowed))
        self.emit('advance_combat')
        self.prop(attacker, 'blocked', int(blocked is not None))
        if after_advance:
            after_advance()
        if blocked is not None:
            self.prop(blocked, 'blocking_count', 1)
        a_pt = self.stats(attacker)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if blocked is None:
            self.life[self.foe] -= a_pt[0]
        else:
            b_pt = actual_stats or self.stats(blocked)
            for ref, received, toughness in ((attacker, b_pt[0], a_pt[1]), (blocked, a_pt[0], b_pt[1])):
                if received >= toughness:
                    self.move(ref, 'graveyard')
                else:
                    self.prop(ref, 'damage', received)
        self.receipt()

ELITE, BEARS, PILEDRIVER, SWARM = 6365, 1650, 6156, 6607
PLAN = {
    ELITE: ('{W}', 'Creature — Human Soldier', '1', '1',
            'Provoke (Whenever this creature attacks, you may have target creature defending player controls untap and block it if able.)\n'
            '{1}{W}: Prevent all combat damage that would be dealt to and dealt by this creature this turn.',
            ('provoke', 'decline', 'paid_prevention', 'short', 'noncombat')),
    BEARS: ('{1}{G}', 'Creature — Bear Spirit', '3', '3',
            "Whenever this creature attacks, if defending player controls no black nontoken permanents, it doesn't untap during your next untap step.",
            ('empty', 'black_noncreature', 'own_black', 'black_token', 'black_removed', 'idle')),
    PILEDRIVER: ('{1}{R}', 'Creature — Goblin Warrior', '1', '2',
            "Protection from blue (This creature can't be blocked, targeted, dealt damage, or enchanted by anything blue.)\n"
            'Whenever this creature attacks, it gets +2/+0 until end of turn for each other attacking Goblin.',
            ('blue_block', 'blue_target', 'other_colors', 'goblin_count', 'idle_goblin')),
    SWARM: ('{G}', 'Creature — Insect', '0', '1',
            "Flying\nWhenever this creature attacks, defending player can't cast spells this turn.",
            ('spell_lock', 'own_spell', 'flyer_blocks', 'idle')),
}
WITNESSES = {
    'Goblin Tinkerer': ('{1}{R}', '{R}, {T}: Destroy target artifact. That artifact deals damage equal to its mana value to this creature.', '1', '2'),
    'Prodigal Sorcerer': ('{2}{U}', '{T}: This creature deals 1 damage to any target.', '1', '1'),
    'Serra Angel': ('{3}{W}{W}', "Flying\nVigilance (Attacking doesn't cause this creature to tap.)", '4', '4'),
    'Fiery Hellhound': ('{1}{R}{R}', '{R}: This creature gets +1/+0 until end of turn.', '2', '2'),
    'Phyrexian Arena': ('{1}{B}{B}', 'At the beginning of your upkeep, you draw a card and lose 1 life.'),
    'Doomed Dissenter': ('{1}{B}', 'When this creature dies, create a 2/2 black Zombie creature token.', '1', '1'),
    'Royal Assassin': ('{1}{B}{B}', '{T}: Destroy target tapped creature.', '1', '1'),
    'Lightning Bolt': ('{R}', 'Lightning Bolt deals 3 damage to any target.'),
    'Reach Through Mists': ('{U}', 'Draw a card.'),
    'Unsummon': ('{U}', "Return target creature to its owner's hand."),
    'Forest': ('', '({T}: Add {G}.)'),
    'Plains': ('', '({T}: Add {W}.)'),
}
SEED = 3062
WALLET = (1,0,0,0,0,1)


def label(csvid, mode, actor):
    return f'AttackFollowOnRequirementsAndLegalityRecipe_{csvid}_{mode}_{actor}'


class Board(CombatBoard):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid, self.mode, self.actor, self.foe = csvid, mode, actor, 1-actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet, self.life = [ZERO, ZERO], [20, 20]
        self.add('source', record['name'], actor, 'hand')
        if csvid == ELITE:
            self.add('guard', 'Goblin Tinkerer', self.foe, 'battlefield', tapped=True)
            if mode == 'noncombat':
                self.add('bolt', 'Lightning Bolt', self.foe, 'hand')
        if csvid == BEARS:
            if mode in ('black_noncreature', 'own_black'):
                self.add('black_card', 'Phyrexian Arena', self.foe if mode == 'black_noncreature' else actor, 'battlefield')
            if mode == 'black_token':
                self.add('dissenter', 'Doomed Dissenter', self.foe, 'battlefield')
            if mode == 'black_removed':
                self.add('black_card', 'Royal Assassin', self.foe, 'battlefield')
            if mode == 'idle':
                self.add('other', 'Goblin Tinkerer', actor, 'battlefield')
        if csvid == PILEDRIVER:
            if mode == 'blue_target':
                self.add('bounce', 'Unsummon', self.foe, 'hand')
            if mode == 'blue_block':
                self.add('guard', 'Prodigal Sorcerer', self.foe, 'battlefield')
            if mode == 'other_colors':
                self.add('guard', 'Goblin Tinkerer', self.foe, 'battlefield')
            if mode in ('goblin_count', 'idle_goblin'):
                self.add('goblin_0', 'Goblin Tinkerer', actor, 'battlefield')
                self.add('goblin_1', 'Goblin Tinkerer', actor, 'battlefield')
                self.add('non_goblin', 'Fiery Hellhound', actor, 'battlefield')
                self.add('foe_goblin', 'Goblin Tinkerer', self.foe, 'battlefield')
        if csvid == SWARM:
            if mode == 'flyer_blocks':
                self.add('guard', 'Serra Angel', self.foe, 'battlefield')
            else:
                self.add('guard', 'Goblin Tinkerer', self.foe, 'battlefield')
                self.add('foe_spell', 'Reach Through Mists', self.foe, 'hand')
                self.add('foe_draw', 'Forest', self.foe, 'library')
                if mode == 'own_spell':
                    self.add('own_spell', 'Reach Through Mists', actor, 'hand')
                    self.add('own_draw', 'Plains', actor, 'library')
                if mode == 'idle':
                    self.add('other', 'Fiery Hellhound', actor, 'battlefield')

    def declare(self, refs, provoke=None, *, decline=False):
        self.emit('begin_attackers')
        for ref in refs:
            self.emit('attack', ref, value=1)
            self.prop(ref, 'attacking', 1)
        if provoke:
            if self.actor == 0:
                self.emit('choose_trigger_source', 'source', player=self.actor)
            self.emit('choose_target', provoke, player=self.actor)
        if decline and self.actor == 1:
            self.emit('choose_cancel_target', player=self.actor)
        self.emit('finish_attackers')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)

    def activate_prevention(self, payable=True):
        self.fund(self.actor, WALLET if payable else (0,0,0,0,0,1))
        self.emit('can_activate', 'source', value=int(payable))
        if payable:
            self.emit('activate', 'source', value=1)
            self.wallet[self.actor] = ZERO
            self.emit('assert_stack', value=1)
            self.pools()
            self.emit('resolve')

    def elite(self):
        if self.mode == 'noncombat':
            self.activate_prevention()
            self.fund(self.foe, cost_from_oracle(self.oracle['Lightning Bolt']))
            self.emit('choose_target', 'source', player=self.foe)
            self.emit('cast', 'bolt', value=1)
            self.emit('assert_stack', value=1)
            self.wallet[self.foe] = ZERO
            self.emit('resolve')
            self.move('source', 'graveyard')
            self.move('bolt', 'graveyard')
            return
        declined = self.mode == 'decline'
        self.declare(['source'], None if declined else 'guard', decline=declined)
        self.prop('guard', 'tapped', int(declined))
        if self.mode in ('paid_prevention','short'):
            self.activate_prevention(self.mode == 'paid_prevention')
            if self.mode == 'short':
                self.pools()
                self.fund(self.actor, ZERO)
        if declined:
            self.combat('source', attempts=(('guard', False),))
        elif self.mode != 'paid_prevention':
            self.combat('source', blocked='guard')
        else:
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.prop('source', 'blocked', 1)
            self.prop('guard', 'blocking_count', 1)
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            for ref in ('source','guard'):
                self.size(ref)
                self.prop(ref, 'damage', 0)
            self.emit('cleanup')

    def untap_again(self):
        self.emit('advance_turn')
        self.emit('advance_turn')
        self.emit('untap_step', player=self.actor)

    def bears(self):
        if self.mode == 'black_token':
            self.emit('destroy', 'dissenter')
            self.emit('resolve')
            self.move('dissenter', 'graveyard')
            self.cards['zombie'] = dict(name='Zombie', player=self.foe, zone='battlefield')
            csvid = self.rows['Zombie']['csvid']
            for prop, expected in (('power',2),('toughness',2),('color',2),('token',1)):
                self.emit('assert_matching_permanents', player=self.foe, index=csvid, prop=prop, value=expected)
            self.receipt()
        attacker = 'other' if self.mode == 'idle' else 'source'
        self.declare([attacker])
        if self.mode == 'black_removed':
            self.emit('destroy', 'black_card')
            self.move('black_card', 'graveyard')
        self.combat(attacker)
        self.untap_again()
        skip = self.mode not in ('black_noncreature','black_removed','idle')
        self.prop('source', 'tapped', int(skip))
        if skip:
            self.untap_again()
            self.prop('source', 'tapped', 0)

    def piledriver(self):
        if self.mode == 'blue_target':
            self.fund(self.foe, cost_from_oracle(self.oracle['Unsummon']))
            self.emit('can_target_creature', 'bounce', 'source', value=0)
            self.emit('cast', 'bounce', value=0)
            self.size('source')
            return
        if self.mode in ('blue_block','other_colors'):
            self.declare(['source'])
            allowed = self.mode == 'other_colors'
            self.combat('source', attempts=(('guard', allowed),), blocked='guard' if allowed else None)
            return
        attackers = ['source','non_goblin']
        if self.mode == 'goblin_count':
            attackers += ['goblin_0','goblin_1']
        self.declare(attackers)
        extra = 4 if self.mode == 'goblin_count' else 0
        self.size('source', (1+extra,2))
        self.size('goblin_0')
        self.size('goblin_1')
        self.size('non_goblin')
        self.size('foe_goblin')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[self.foe] -= extra + sum(self.stats(ref)[0] for ref in attackers)
        self.emit('cleanup')
        self.size('source')

    def draw_witness(self, spell, drawn):
        self.cast(spell)
        self.move(drawn, 'hand')
        self.receipt()

    def swarm(self):
        if self.mode == 'flyer_blocks':
            self.declare(['source'])
            self.combat('source', attempts=(('guard',True),), blocked='guard')
            return
        attacker = 'other' if self.mode == 'idle' else 'source'
        self.declare([attacker])
        if self.mode == 'idle':
            self.draw_witness('foe_spell','foe_draw')
            self.combat(attacker)
            return
        fee = cost_from_oracle(self.oracle['Reach Through Mists'])
        self.fund(self.foe, fee)
        self.emit('cast', 'foe_spell', value=0)
        self.receipt()
        self.fund(self.foe, ZERO)
        if self.mode == 'own_spell':
            self.draw_witness('own_spell','own_draw')
        self.combat(attacker, attempts=(('guard',False),))
        self.emit('advance_turn')
        self.draw_witness('foe_spell','foe_draw')

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for who in (0,1):
            self.fund(who, ZERO)
        self.cast('source', destination='battlefield')
        self.size('source')
        self.round_trip_turn()
        self.prop('source', 'sick', 0)
        {ELITE: self.elite, BEARS: self.bears, PILEDRIVER: self.piledriver, SWARM: self.swarm}[self.csvid]()
        self.receipt()


BRONTO, FERAL, GRAPPLER, LOWLAND, ICATIAN, FORCE = 6344, 6376, 6393, 6410, 1803, 8671
FOLLOWERS = {
    6344: ('{4}{G}{G}', 'Creature — Beast', '5', '3', 'Trample\nProvoke (Whenever this creature attacks, you may have target creature defending player controls untap and block it if able.)', ('Trample', 'Provoke'), ('provoke', 'decline', 'trample')),
    6376: ('{4}{G}{G}', 'Creature — Beast', '3', '3', 'Amplify 2 (As this creature enters, put two +1/+1 counters on it for each Beast card you reveal in your hand.)\nProvoke (Whenever this creature attacks, you may have target creature defending player controls untap and block it if able.)', ('Amplify', 'Provoke'), ('reveal_zero', 'reveal_one', 'reveal_two')),
    6393: ('{R}', 'Creature — Goblin', '1', '1', 'Provoke (Whenever this creature attacks, you may have target creature defending player controls untap and block it if able.)', ('Provoke',), ('provoke', 'decline', 'hexproof')),
    6410: ('{4}{W}', 'Creature — Human Soldier', '2', '2', 'First strike\nProvoke (Whenever this creature attacks, you may have target creature defending player controls untap and block it if able.)', ('First strike', 'Provoke'), ('provoke', 'decline', 'first_strike')),
    1803: ('{3}{W}', 'Creature — Human Soldier', '1', '1', "First strike; banding (Any creatures with banding, and up to one without, can attack in a band. Bands are blocked as a group. If any creatures with banding you control are blocking or being blocked by a creature, you divide that creature's combat damage, not its controller, among any of the creatures it's being blocked by or is blocking.)\nWhenever this creature attacks, all creatures banded with it gain first strike until end of turn.", ('First strike', 'Banding'), ('first_strike', 'unbanded')),
    8671: ('{3}{G}{G}', 'Creature — Elemental Spirit', '8', '8', "Trample\nWhenever this creature attacks, if defending player controls no black permanents, it doesn't untap during your next untap step.", ('Trample',), ('empty', 'black_noncreature', 'own_black', 'black_token', 'trample')),
}
NEW_WITNESSES = {
    'Gladecover Scout': ('{G}', "Hexproof (This creature can't be the target of spells or abilities your opponents control.)", '1', '1'),
    'Krosan Tusker': ('{5}{G}{G}', 'Cycling {2}{G} ({2}{G}, Discard this card: Draw a card.)\nWhen you cycle this card, you may search your library for a basic land card, reveal that card, put it into your hand, then shuffle. (Do this before you draw.)'),
    'Thragtusk': ('{4}{G}', 'When this creature enters, you gain 5 life.\nWhen this creature leaves the battlefield, create a 3/3 green Beast creature token.'),
}
PILOT_KEYWORDS = {ELITE: ('Provoke',), PILEDRIVER: ('Protection',), SWARM: ('Flying',)}


class FollowOn(Board):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid, self.mode, self.actor, self.foe = csvid, mode, actor, 1-actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet, self.life = [ZERO, ZERO], [20,20]
        self.add('source', record['name'], actor, 'hand')
        if csvid == FORCE:
            if mode in ('black_noncreature','own_black'):
                self.add('black_card', 'Phyrexian Arena', self.foe if mode == 'black_noncreature' else actor, 'battlefield')
            if mode == 'black_token':
                self.add('dissenter', 'Doomed Dissenter', self.foe, 'battlefield')
            if mode == 'trample':
                self.add('guard', 'Goblin Tinkerer', self.foe, 'battlefield')
            return
        guard = 'Gladecover Scout' if mode == 'hexproof' else 'Goblin Tinkerer'
        if csvid == ICATIAN:
            guard = 'Prodigal Sorcerer'
        if csvid == LOWLAND and mode == 'provoke':
            guard = 'Serra Angel'
        self.add('guard', guard, self.foe, 'battlefield', tapped=csvid != ICATIAN)
        if csvid == BRONTO and mode == 'provoke':
            self.add('alternative', 'Prodigal Sorcerer', self.foe, 'battlefield')
        if csvid == ICATIAN and mode == 'unbanded':
            self.add('ally', 'Goblin Tinkerer', actor, 'battlefield')
        if csvid == FERAL:
            self.add('non_beast', 'Forest', actor, 'hand')
            if mode in ('reveal_one','reveal_two'):
                self.add('beast_0', 'Krosan Tusker', actor, 'hand')
            if mode == 'reveal_two':
                self.add('beast_1', 'Thragtusk', actor, 'hand')

    def trample(self, *, legal_block=False):
        self.emit('begin_blockers')
        if legal_block:
            self.emit('block', 'guard', 'source', value=1)
        self.emit('advance_combat')
        if 'alternative' in self.cards:
            self.prop('alternative', 'blocking_count', 0)
        self.prop('source', 'blocked', 1)
        self.prop('guard', 'blocking_count', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[self.foe] -= self.stats('source')[0] - self.stats('guard')[1]
        self.prop('source', 'damage', 1)
        self.move('guard', 'graveyard')
        self.receipt()

    def provoke(self):
        declined = self.mode == 'decline'
        legal = not declined and self.mode != 'hexproof'
        cancel_offer = declined or (self.mode == 'hexproof' and self.actor == 1)
        self.declare(['source'], 'guard' if legal else None, decline=cancel_offer)
        self.prop('guard', 'tapped', int(not legal))
        if not legal:
            self.combat('source', attempts=(('guard', False),))
        elif self.csvid == BRONTO:
            self.trample()
        elif self.csvid == LOWLAND:
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.prop('source', 'blocked', 1)
            self.prop('guard', 'blocking_count', 1)
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            if self.mode == 'first_strike':
                self.size('source')
                self.prop('source', 'damage', 0)
                self.move('guard', 'graveyard')
            else:
                self.move('source', 'graveyard')
                self.prop('guard', 'damage', 2)
        elif self.csvid == FERAL:
            power = self.stats('source')[0] + self.amplify_count
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.prop('source', 'blocked', 1)
            self.prop('guard', 'blocking_count', 1)
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.size('source', (power,power))
            self.prop('source', 'damage', 1)
            self.move('guard', 'graveyard')
        else:
            self.combat('source', blocked='guard')

    @property
    def amplify_count(self):
        return {'reveal_zero': 0, 'reveal_one': 2, 'reveal_two':4}[self.mode]

    def feral_cast(self):
        choices = [('choose_dialog', 0, 0)] if self.actor == 0 else []
        if self.amplify_count:
            # The production Auto mode reveals all matching hand cards, then
            # both controller paths show the actual disclosure to seat zero.
            choices.append(('choose_zone', 0, -1))
        self.cast('source', destination='battlefield', resolve_choices=choices)
        self.emit('assert_counter', 'source', prop='plus1', value=self.amplify_count)
        size = 3+self.amplify_count
        self.size('source', (size,size))
        self.receipt()

    def icatian(self):
        unbanded = self.mode == 'unbanded'
        self.declare(['source','ally'] if unbanded else ['source'])
        self.emit('begin_blockers')
        self.emit('block', 'guard', 'ally' if unbanded else 'source', value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if unbanded:
            self.life[self.foe] -= 1
        self.prop('ally' if unbanded else 'source', 'damage', int(unbanded))
        self.size('source')
        self.move('guard', 'graveyard')

    def force(self):
        if self.mode == 'black_token':
            self.emit('destroy', 'dissenter')
            self.emit('resolve')
            self.move('dissenter', 'graveyard')
            self.cards['zombie'] = dict(name='Zombie',player=self.foe,zone='battlefield')
            csvid = self.rows['Zombie']['csvid']
            for prop, value in (('color',2),('token',1)):
                self.emit('assert_matching_permanents', player=self.foe, index=csvid, prop=prop, value=value)
            self.receipt()
        self.declare(['source'])
        if self.mode == 'trample':
            self.trample(legal_block=True)
        else:
            self.combat('source')
        self.untap_again()
        skip = self.mode not in ('black_noncreature','black_token')
        self.prop('source', 'tapped', int(skip))
        if skip:
            self.untap_again()
            self.prop('source', 'tapped', 0)

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for actor in (0,1):
            self.fund(actor, ZERO)
        if self.csvid == FERAL:
            self.feral_cast()
        else:
            self.cast('source', destination='battlefield')
            self.size('source')
        self.round_trip_turn()
        self.prop('source', 'sick', 0)
        if self.csvid == ICATIAN:
            self.icatian()
        elif self.csvid == FORCE:
            self.force()
        else:
            self.provoke()
        self.receipt()


@lru_cache
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid in PLAN:
        cost, kind, power, toughness, rules, modes = PLAN[csvid]
        keywords = PILOT_KEYWORDS.get(csvid, ())
    else:
        cost, kind, power, toughness, rules, keywords, modes = FOLLOWERS[csvid]
    fields = ('mana_cost','type_line','power','toughness','oracle_text')
    if tuple(record.get(field) for field in fields) != (cost,kind,power,toughness,rules):
        raise ValueError('Exact paid cost, creature stats and entire attack Oracle required')
    if record.get('layout') != 'normal' or record.get('card_faces') or set(record.get('keywords', ())) != set(keywords):
        raise ValueError('Exact whole single-face record required')
    return dict(csvid=csvid,cost=cost,choices=[f'{mode}_{seat}' for mode in modes for seat in (0,1)])


def variants(params):
    return list(params['choices'])


def validate_witnesses(oracle, rows):
    pins = profiles()
    expects = WITNESSES | NEW_WITNESSES
    if (set(pins) != {'sources','witnesses'} or set(pins['witnesses']) != expects.keys() | {'Zombie'}
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != PLAN.keys() | FOLLOWERS.keys()):
        raise ValueError('Exact pinned source and witness scope required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            layout = 'token' if name == 'Zombie' else 'normal'
            if (not exact(pin,dict(oracle=record,registry=row)) or record.get('layout') != layout
                    or row.get('kind') != 'card' or row.get('handler') in (None,'0','NULL','card_dummy')):
                raise ValueError(f'Changed implemented attack record: {name}')
            if name in expects:
                fields = ('mana_cost','oracle_text','power','toughness')[:len(expects[name])]
                if tuple(record[k] for k in fields) != expects[name]:
                    raise ValueError(f'Changed witness Oracle clauses: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record,pin['oracle']):
        raise ValueError('Exact complete source Oracle required')
    model = classify(record,pin['registry']['csvid'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'),model) or not isinstance(selected,list) or not selected
            or any(type(variant) is not str for variant in selected) or len(selected) != len(set(selected))
            or set(selected)-set(variants(model))):
        raise ValueError('Exact scoped family and distinct attack variants required')
    return dict(model,family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle,rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected)-set(variants(model)):
        raise ValueError('Exact attack source and variants required')
    gaps = {
        ICATIAN: ['Actual band declaration is unavailable in the driver. First strike and unbanded combat are exercised.'],
        FERAL: ['The real automatic reveal path is used. Selecting a subset of eligible Beasts in Manual mode is unobserved.'],
        PILEDRIVER: ['Blue targeting and blocking are exercised. Blue enchantment and untargeted damage are unobserved.'],
    }
    cases = []
    factory = FollowOn if csvid in FOLLOWERS else Board
    for variant in selected:
        mode, seat = variant.rsplit('_',1)
        board = factory(oracle[name],csvid,mode,int(seat),rows,oracle)
        board.play()
        covered = declaration['oracle_text'].splitlines()
        if csvid == ICATIAN:
            covered = ['First strike']
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                        oracle_text=declaration['oracle_text'],covered_clauses=covered,untested_components=gaps.get(csvid,[]))
        cases.append(dict(name=label(csvid,mode,int(seat)),card=name,fixture='empty_main',seed=SEED,
                          setup=board.setup,steps=board.steps,_recipe=metadata))
    return cases
