"""Targeted attack triggers with exact combat, scope and printed riders."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step

FAMILY = 'attack_trigger_block_and_target'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'

TUSKER, GROTAG, NEFASHU, WARKITE = 14222, 10769, 6556, 18330
MODELS = {
    TUSKER: ('{2}{G}{U}{R}', 'Creature — Elephant Warrior', '6', '4',
             'Whenever this creature attacks, target creature defending player controls blocks it this combat if able.',
             ('force', 'named_only', 'tapped', 'hexproof', 'ally_attacks')),
    GROTAG: ('{4}{R}', 'Creature — Lizard', '3', '3',
             "Whenever this creature attacks, target creature can't block this turn.",
             ('forbid', 'another', 'own', 'ally_attacks')),
    NEFASHU: ('{4}{B}{B}', 'Creature — Zombie Mutant', '5', '3',
              'Whenever this creature attacks, up to five target creatures each get -1/-1 until end of turn.',
              ('zero', 'one', 'five', 'own', 'ally_attacks')),
    WARKITE: ('{1}{U}', 'Creature — Human Pirate', '2', '1',
              'Flying\nWhenever this creature attacks, target creature defending player controls loses all abilities and has base power and toughness 0/1 until end of turn.',
              ('lose_flying', 'layer', 'lose_activation', 'hexproof', 'ally_attacks')),
}
WITNESSES = {
    'Goblin Tinkerer': ('{1}{R}', '{R}, {T}: Destroy target artifact. That artifact deals damage equal to its mana value to this creature.', '1', '2'),
    'Prodigal Sorcerer': ('{2}{U}', '{T}: This creature deals 1 damage to any target.', '1', '1'),
    'Serra Angel': ('{3}{W}{W}', "Flying\nVigilance (Attacking doesn't cause this creature to tap.)", '4', '4'),
    'Fiery Hellhound': ('{1}{R}{R}', '{R}: This creature gets +1/+0 until end of turn.', '2', '2'),
    'Elvish Mystic': ('{G}', '{T}: Add {G}.', '1', '1'),
    'Boreal Druid': ('{G}', '{T}: Add {C}.', '1', '1'),
    'Gladecover Scout': ('{G}', "Hexproof (This creature can't be the target of spells or abilities your opponents control.)", '1', '1'),
    'Giant Growth': ('{G}', 'Target creature gets +3/+3 until end of turn.'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = (0,) * 6
SEED = 3012


def label(csvid, mode, actor):
    return f'AttackTriggerBlockAndTargetRecipe_{csvid}_{mode}_{actor}'


class Board:
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid, self.mode, self.actor, self.foe = csvid, mode, actor, 1-actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet, self.life = [ZERO, ZERO], [20, 20]
        self.add('source', record['name'], actor, 'hand')
        self.targeted = []
        self.add('ally', 'Serra Angel' if csvid == WARKITE and mode == 'lose_flying' else 'Goblin Tinkerer',
                 actor, 'battlefield')
        enemy = 'Gladecover Scout' if mode == 'hexproof' else 'Goblin Tinkerer'
        if csvid == WARKITE and mode in ('lose_flying', 'layer'):
            enemy = 'Serra Angel'
        if csvid == WARKITE and mode == 'lose_activation':
            enemy = 'Prodigal Sorcerer'
        if csvid == NEFASHU and mode in ('one', 'five'):
            enemy = 'Elvish Mystic'
        self.add('guard', enemy, self.foe, 'battlefield', tapped=mode == 'tapped')
        if mode == 'named_only':
            self.add('other', 'Serra Angel', self.foe, 'battlefield')
        if mode == 'another':
            self.add('other', 'Prodigal Sorcerer', self.foe, 'battlefield')
        if mode == 'five':
            for index, name in enumerate(('Goblin Tinkerer','Fiery Hellhound','Boreal Druid','Serra Angel','Prodigal Sorcerer')):
                self.add(f'other_{index}', name, self.foe, 'battlefield')
        if mode == 'layer':
            self.add('growth', 'Giant Growth', self.actor, 'hand')
        for who in (0, 1):
            self.fund(who, ZERO)

    def add(self, ref, name, player, zone, *, tapped=False):
        assert ref not in self.cards and player in (0, 1)
        self.cards[ref] = dict(name=name, player=player, zone=zone)
        flags = (1 if tapped else 8) if zone == 'battlefield' else 0
        entry = card(name, ref, player, zone)
        if flags:
            entry['state'] = ['tapped' if tapped else 'untapped']
        self.setup.append(entry)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        if op == 'choose_trigger_source':
            self.steps.append(step(op, ref=ref, player=player))
            return
        if op == 'untap_step':
            self.steps.append(step(op, player=player))
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

    def cast(self, ref, *, target=None, destination='graveyard', payment=None, dialog=None, resolve_targets=()):
        card = self.cards[ref]
        who = card['player']
        self.fund(who, payment or cost_from_oracle(self.oracle[card['name']]))
        if dialog is not None:
            self.emit('choose_dialog', player=who, value=dialog)
        if target is not None:
            self.emit('choose_target', target, player=who)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.wallet[who] = ZERO
        self.pools()
        for resolve_target in resolve_targets:
            self.emit('choose_target', resolve_target, player=who)
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

    def begin(self, ref, targets=None, *, cancel=False):
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        self.prop(ref, 'attacking', 1)
        if targets is not None:
            if self.actor == 0 and self.csvid == NEFASHU:
                self.emit('choose_trigger_source', ref, player=self.actor)
            for target in targets:
                self.emit('choose_target', target, player=self.actor)
            if cancel:
                self.emit('choose_cancel_target', player=self.actor)
            self.targeted.extend(targets)
        self.emit('finish_attackers')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)

    def combat(self, attacker, *, attempts=(), blocked=None, actual_stats=None, attacker_stats=None, after_advance=None):
        self.emit('begin_blockers')
        for blocker, allowed in attempts:
            self.emit('block', blocker, attacker, value=int(allowed))
        self.emit('advance_combat')
        self.prop(attacker, 'blocked', int(blocked is not None))
        if after_advance:
            after_advance()
        if blocked is not None:
            self.prop(blocked, 'blocking_count', 1)
        a_pt = attacker_stats or self.stats(attacker)
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

    def stay_idle(self):
        self.begin('ally')
        self.prop('source', 'attacking', 0)
        self.combat('ally', attempts=(('guard', True),), blocked='guard')

    def tusker(self):
        blocked = self.mode in ('force', 'named_only')
        if self.mode == 'hexproof':
            self.emit('can_target_creature', 'source', 'guard', value=0)
        self.begin('source', [] if self.mode == 'hexproof' else ['guard'])
        attempts = (('guard', False),) if self.mode == 'tapped' else ()
        def check_named():
            self.prop('guard', 'blocking_count', int(blocked))
            if self.mode == 'named_only':
                self.prop('other', 'blocking_count', 0)
        self.combat('source', attempts=attempts, blocked='guard' if blocked else None, after_advance=check_named)

    def grotag(self):
        targets = ['ally'] if self.mode == 'own' else ['guard']
        self.begin('source', targets)
        attempts = [('guard', self.mode == 'own')]
        if self.mode == 'another':
            attempts.append(('other', True))
        blocker = {'own': 'guard', 'another': 'other'}.get(self.mode)
        self.combat('source', attempts=attempts, blocked=blocker)
        if self.mode == 'forbid':
            self.round_trip_turn()
            self.begin('ally')
            self.combat('ally', attempts=(('guard', True),), blocked='guard')

    def nefashu(self):
        targets = {'zero': [], 'own': ['ally'], 'one': ['guard'],
                   'five': ['guard','other_0','other_1','other_2','other_3']}[self.mode]
        self.begin('source', targets, cancel=len(targets) < 5)
        def after_shrink():
            for ref in targets:
                power, toughness = self.stats(ref)
                if toughness == 1:
                    self.move(ref, 'graveyard')
                    self.prop(ref, 'in_play', 0)
                else:
                    self.size(ref, (power-1, toughness-1))
            for ref in sorted(self.cards.keys() - {'source', *targets}):
                self.size(ref)
        self.combat('source', after_advance=after_shrink)
        self.emit('cleanup')
        for ref in targets:
            if self.cards[ref]['zone'] == 'battlefield':
                self.size(ref)

    def warkite(self):
        if self.mode == 'layer':
            self.cast('growth', target='guard')
            self.size('guard', (7, 7))
        if self.mode == 'lose_activation':
            self.emit('can_activate', 'guard', value=1)
        if self.mode == 'hexproof':
            self.emit('can_target_creature', 'source', 'guard', value=0)
        self.begin('source', [] if self.mode == 'hexproof' else ['guard'])
        if self.mode != 'hexproof':
            self.size('guard', (3, 4) if self.mode == 'layer' else (0, 1))
        if self.mode == 'lose_activation':
            self.emit('can_activate', 'guard', value=0)
        self.combat('source', attempts=(('guard', False),))
        if self.mode == 'lose_flying':
            self.round_trip_turn()
            self.size('guard')
            self.begin('ally')
            self.combat('ally', attempts=(('guard', True),), blocked='guard')
        else:
            self.emit('cleanup')
            self.size('guard')
            if self.mode == 'lose_activation':
                self.emit('can_activate', 'guard', value=1)

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        self.cast('source', destination='battlefield')
        self.size('source')
        self.prop('source', 'sick', 1)
        self.round_trip_turn()
        self.prop('source', 'sick', 0)
        if self.mode == 'ally_attacks':
            self.stay_idle()
        else:
            effects = {TUSKER: self.tusker, GROTAG: self.grotag, NEFASHU: self.nefashu, WARKITE: self.warkite}
            effects[self.csvid]()
        self.receipt()


KROVOD, MARDU, HEEL, HERO, TORRENT, BEETLE = 12667, 14321, 14560, 11459, 14645, 16882
HUMILIATOR, GRASP, UMBRA, TOUCH, FERRETS = 19129, 15140, 19113, 12379, 2216
FOLLOWERS = {
    12667: ('{4}{W}', 'Creature — Beast', '2', '5', 'Whenever this creature attacks, another target attacking creature gets +0/+2 until end of turn. Untap that creature.', (), ('pair', 'solo', 'ally_attacks')),
    14321: ('{2}{R}{W}{B}', 'Creature — Orc Warrior', '5', '4', "Whenever this creature attacks, target creature can't block this turn.", (), ('forbid', 'another', 'own', 'ally_attacks')),
    14560: ('{3}{R}', 'Creature — Goblin Berserker', '3', '2', "Whenever this creature attacks, target creature can't block this turn.\nDash {2}{R} (You may cast this spell for its dash cost. If you do, it gains haste, and it's returned from the battlefield to its owner's hand at the beginning of the next end step.)", ('Dash',), ('dash', 'dash_other', 'normal', 'normal_fresh')),
    11459: ('{2}{R}{R}', 'Creature — Human Knight', '4', '2', "Haste\nBattle cry (Whenever this creature attacks, each other attacking creature gets +1/+0 until end of turn.)\nWhenever this creature attacks, creatures with power 1 or less can't block this turn.", ('Battle Cry', 'Haste'), ('haste', 'battle_cry', 'block_power')),
    14645: ('{4}{U}', 'Creature — Elemental', '3', '5', 'Flying\nWhenever this creature attacks, tap all creatures defending player controls.\n{3}{B/G}{B/G}: Put this card from exile onto the battlefield tapped. Activate only as a sorcery.', ('Flying',), ('sweep', 'ally_attacks')),
    16882: ('{3}{B}{G}', 'Creature — Insect', '4', '5', 'When this creature enters, put a -1/-1 counter on target creature you control.\nWhenever this creature attacks, remove a -1/-1 counter from target creature you control and put a -1/-1 counter on up to one target creature defending player controls.', (), ('transfer', 'lethal', 'only_own')),
    19129: ('{3}{U}', 'Creature — Vedalken Wizard', '3', '4', 'Metalcraft — Whenever this creature attacks, if you control three or more artifacts, creatures your opponents control lose all abilities and have base power and toughness 1/1 until end of turn.', ('Metalcraft',), ('metalcraft', 'short', 'opposing_artifacts')),
    15140: ('{1}{W}', 'Enchantment — Aura', None, None, 'Enchant creature\nEnchanted creature gets +1/+1 and has "Whenever this creature attacks, tap target creature defending player controls."', ('Enchant',), ('tap', 'ally_attacks', 'hexproof')),
    19113: ('{3}{U}{U}', 'Enchantment — Aura', None, None, 'Enchant creature\nEnchanted creature has base power and toughness 8/8 and has "Whenever this creature attacks, you may tap target creature with power 8 or less."\nUmbra armor (If enchanted creature would be destroyed, instead remove all damage from it and destroy this Aura.)', ('Umbra armor', 'Enchant'), ('tap', 'decline', 'armor')),
    12379: ('{1}{U}', 'Enchantment — Aura', None, None, 'Enchant creature\nEnchanted creature has "Whenever this creature attacks, you may tap or untap target permanent."', ('Enchant',), ('tap', 'untap', 'decline', 'ally_attacks')),
    2216: ('{G}', 'Creature — Ferret', '1', '1', "Whenever this creature attacks, it gets +0/+2 until end of turn.\nAt end of combat, tap all creatures that blocked this creature this turn. They don't untap during their controller's next untap step.", (), ('blocker', 'lethal', 'unblocked')),
}
NEW_WITNESSES = {
    'Swamp': ('', '({T}: Add {B}.)'),
    'Welding Jar': ('{0}', 'Sacrifice this artifact: Regenerate target artifact.'),
    'Darksteel Relic': ('{0}', 'Indestructible (Effects that say "destroy" don\'t destroy this artifact.)'),
    "Traveler's Amulet": ('{1}', '{1}, Sacrifice this artifact: Search your library for a basic land card, reveal it, put it into your hand, then shuffle.'),
    'Murder': ('{1}{B}{B}', 'Destroy target creature.'),
    'Krosan Cloudscraper': ('{7}{G}{G}{G}', 'At the beginning of your upkeep, sacrifice this creature unless you pay {G}{G}.\nMorph {7}{G}{G} (You may cast this card face down as a 2/2 creature for {3}. Turn it face up any time for its morph cost.)', '13', '13'),
}
AURAS = {GRASP, UMBRA, TOUCH}


class FollowOn(Board):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid, self.mode, self.actor, self.foe = csvid, mode, actor, 1-actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet, self.life = [ZERO, ZERO], [20, 20]
        self.targeted = []
        self.add('source', record['name'], actor, 'hand')
        self.add('ally', 'Goblin Tinkerer', actor, 'battlefield')
        guard = {'lethal': 'Serra Angel' if csvid == FERRETS else 'Elvish Mystic',
                 'hexproof': 'Gladecover Scout', 'tap': 'Goblin Tinkerer'}.get(mode, 'Goblin Tinkerer')
        if csvid == BEETLE and mode == 'only_own':
            guard = 'Gladecover Scout'
        if csvid == HUMILIATOR:
            guard = 'Prodigal Sorcerer'
        self.add('guard', guard, self.foe, 'battlefield')
        if csvid == MARDU and mode == 'another' or csvid == HEEL and mode == 'dash_other':
            self.add('other', 'Prodigal Sorcerer', self.foe, 'battlefield')
        if csvid in (HERO, TORRENT) and mode in ('block_power', 'sweep'):
            self.add('other', 'Fiery Hellhound' if csvid == HERO else 'Gladecover Scout', self.foe, 'battlefield')
        if csvid == TORRENT:
            self.add('land', 'Swamp', self.foe, 'battlefield')
        if csvid == HUMILIATOR:
            self.add('other', 'Serra Angel', self.foe, 'battlefield')
            artifacts = ('Darksteel Relic', "Traveler's Amulet", 'Welding Jar')
            for i, name in enumerate(artifacts):
                owner = self.actor if mode == 'metalcraft' or (mode == 'short' and i < 2) else self.foe
                self.add(f'artifact_{i}', name, owner, 'battlefield')
        if csvid in AURAS:
            self.add('host', 'Fiery Hellhound', self.actor, 'battlefield')
        if csvid == UMBRA and mode == 'tap':
            self.add('large', 'Krosan Cloudscraper', self.foe, 'battlefield')
        if csvid == UMBRA and mode == 'armor':
            self.add('murder', 'Murder', self.foe, 'hand')
        if csvid == TOUCH and mode in ('tap', 'untap'):
            self.add('land', 'Swamp', self.foe, 'battlefield', tapped=mode == 'untap')

    def begin_group(self, attackers, targets=(), *, optional_source=None, extra=(), decline=False):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref, value=1)
            self.prop(ref, 'attacking', 1)
        if optional_source and self.actor == 0 and not decline:
            self.emit('choose_trigger_source', optional_source, player=self.actor)
        for target in targets:
            self.emit('choose_target', target, player=self.actor)
        for op, value in extra:
            self.emit(op, player=self.actor, value=value)
        self.emit('finish_attackers')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)

    def krovod(self):
        if self.mode == 'ally_attacks':
            self.stay_idle()
            return
        if self.mode == 'solo':
            self.begin_group(['source'])
            self.prop('source', 'tapped', 1)
            self.size('source')
            self.combat('source')
            return
        self.begin_group(['source', 'ally'], ['ally'])
        self.prop('ally', 'attacking', 1)
        self.prop('ally', 'tapped', 0)
        self.prop('source', 'tapped', 1)
        self.size('ally', (1, 4))
        self.emit('begin_blockers')
        self.emit('block', 'guard', 'ally', value=1)
        self.emit('advance_combat')
        self.prop('source', 'blocked', 0)
        self.prop('ally', 'blocked', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[self.foe] -= 2
        self.prop('ally', 'damage', 1)
        self.prop('guard', 'damage', 1)
        self.emit('cleanup')
        self.size('ally')

    def heel(self):
        if self.mode == 'normal_fresh':
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.emit('finish_attackers')
            self.receipt()
            return
        different = self.mode == 'dash_other'
        self.begin_group(['source'], ['guard'])
        attempts = [('guard', False)] + ([('other', True)] if different else [])
        self.combat('source', attempts=attempts, blocked='other' if different else None)
        self.emit('cleanup')
        if self.mode.startswith('dash'):
            self.move('source', 'hand')
        else:
            self.prop('source', 'in_play', 1)

    def hero(self):
        with_ally = self.mode == 'battle_cry'
        self.begin_group(['source', 'ally'] if with_ally else ['source'])
        if with_ally:
            self.size('ally', (2, 2))
            self.size('source')
            self.life[self.foe] -= 2
        if self.mode == 'block_power':
            self.combat('source', attempts=(('guard', False), ('other', True)), blocked='other')
        else:
            self.combat('source', attempts=(('guard', False),))
            self.emit('cleanup')
            self.size('ally')

    def torrent(self):
        if self.mode == 'ally_attacks':
            self.stay_idle()
            return
        self.begin_group(['source'])
        self.prop('guard', 'tapped', 1)
        self.prop('other', 'tapped', 1)
        self.prop('ally', 'tapped', 0)
        self.prop('land', 'tapped', 0)
        self.combat('source', attempts=(('guard', False), ('other', False)))

    def beetle(self):
        self.emit('assert_counter', 'ally', prop='minus1', value=1)
        self.size('ally', (0,1))
        targets = ['ally'] if self.mode == 'only_own' else ['ally', 'guard']
        self.begin_group(['source'], targets)
        self.emit('assert_counter', 'ally', prop='minus1', value=0)
        self.size('ally')
        def count_target():
            if self.mode == 'lethal':
                self.move('guard', 'graveyard')
                self.prop('guard', 'in_play', 0)
            else:
                self.emit('assert_counter', 'guard', prop='minus1', value=int(self.mode == 'transfer'))
                self.size('guard', (0,1) if self.mode == 'transfer' else None)
        self.combat('source', after_advance=count_target)
        self.emit('cleanup')
        if self.mode == 'transfer':
            self.emit('assert_counter', 'guard', prop='minus1', value=1)

    def humiliator(self):
        self.begin_group(['source'])
        enabled = self.mode == 'metalcraft'
        self.size('guard')
        self.size('other', (1,1) if enabled else None)
        self.size('ally')
        self.emit('can_activate', 'guard', value=int(not enabled))
        self.combat('source')
        self.emit('cleanup')
        self.size('other')
        self.emit('can_activate', 'guard', value=1)

    def aura_attack(self):
        host_stats = (3,3) if self.csvid == GRASP else (8,8) if self.csvid == UMBRA else (2,2)
        self.size('host', host_stats)
        if self.mode == 'ally_attacks':
            self.begin_group(['ally'])
            self.prop('guard', 'tapped', 0)
            self.combat('ally')
            return
        if self.mode == 'armor':
            self.cast('murder', target='host')
            self.move('source', 'graveyard')
            self.size('host')
            self.prop('host', 'damage', 0)
            self.begin_group(['host'])
            self.prop('guard', 'tapped', 0)
            self.combat('host')
            return
        declined = self.mode == 'decline'
        target = 'land' if self.csvid == TOUCH else 'guard'
        available = not declined and self.mode != 'hexproof'
        if not declined:
            chooser = ['choose_dialog', int(self.mode == 'untap')]
            extra = [tuple(chooser)] if self.csvid == TOUCH and available else []
        else:
            extra = [('choose_cancel_target', 0)] if self.actor == 1 else []
        self.begin_group(['host'], [target] if available else [],
                         optional_source='source', decline=declined, extra=extra)
        if self.mode == 'hexproof':
            self.prop('guard', 'tapped', 0)
        elif self.csvid == TOUCH and available:
            self.prop('land', 'tapped', int(self.mode == 'tap'))
        else:
            self.prop('guard', 'tapped', int(available))
        if 'large' in self.cards:
            self.prop('large', 'tapped', 0)
        self.combat('host', attacker_stats=host_stats)

    def ferrets(self):
        self.begin_group(['source'])
        self.size('source', (1,3))
        blocked = self.mode != 'unblocked'
        self.combat('source', attempts=(('guard', True),) if blocked else (),
                    blocked='guard' if blocked else None, attacker_stats=(1,3))
        if self.mode == 'lethal':
            self.prop('guard', 'tapped', 0)
            return
        self.prop('guard', 'tapped', int(blocked))
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.prop('guard', 'tapped', int(blocked))
        self.size('source')
        if blocked:
            self.emit('advance_turn')
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.prop('guard', 'tapped', 0)

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for who in (0,1):
            self.fund(who, ZERO)
        alt = self.csvid == HEEL and self.mode.startswith('dash')
        target = 'host' if self.csvid in AURAS else None
        self.cast('source', target=target, destination='battlefield',
                  payment=cost_from_oracle({'mana_cost': '{2}{R}'}) if alt else None,
                  dialog=1 if self.csvid == HEEL and not alt else None,
                  resolve_targets=('ally',) if self.csvid == BEETLE else ())
        if self.csvid not in AURAS:
            self.size('source')
        if self.csvid not in (HERO, *AURAS) and not alt and self.mode != 'normal_fresh':
            self.round_trip_turn()
        actions = {KROVOD: self.krovod, MARDU: self.grotag, HEEL: self.heel, HERO: self.hero,
                   TORRENT: self.torrent, BEETLE: self.beetle, HUMILIATOR: self.humiliator,
                   GRASP: self.aura_attack, UMBRA: self.aura_attack, TOUCH: self.aura_attack, FERRETS: self.ferrets}
        if self.csvid == MARDU and self.mode == 'ally_attacks':
            self.stay_idle()
        else:
            actions[self.csvid]()
        self.receipt()


@lru_cache
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid in MODELS:
        fee, kind, power, tough, rules, modes = MODELS[csvid]
        keywords = ('Flying',) if csvid == WARKITE else ()
    else:
        fee, kind, power, tough, rules, keywords, modes = FOLLOWERS[csvid]
    names = ('mana_cost','type_line','power','toughness','oracle_text')
    if tuple(record.get(name) for name in names) != (fee, kind, power, tough, rules):
        raise ValueError('Exact type, stats, costs and entire attack trigger clauses required')
    if record.get('layout') != 'normal' or record.get('card_faces') or set(record.get('keywords', ())) != set(keywords):
        raise ValueError('Whole single-face pinned attack-trigger record required')
    return dict(csvid=csvid, cost=fee, choices=[f'{mode}_{seat}' for mode in modes for seat in (0,1)])


def variants(model):
    return list(model['choices'])


def validate_witnesses(oracle, rows):
    pins = profiles()
    expects = WITNESSES | NEW_WITNESSES
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != expects.keys()
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODELS.keys() | FOLLOWERS.keys()):
        raise ValueError('Exact primary and witness scope required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, dict(oracle=record, registry=row)) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed implemented attack source or witness: {name}')
            if name in expects:
                fields = ('mana_cost', 'oracle_text', 'power', 'toughness')[:len(expects[name])]
                if tuple(record[k] for k in fields) != expects[name]:
                    raise ValueError(f'Unexpected attack witness clause: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned primary required')
    model = classify(record, pin['registry']['csvid'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(selected, list) or not selected
            or any(type(variant) is not str for variant in selected) or len(selected) != len(set(selected))
            or set(selected) - set(variants(model))):
        raise ValueError('Exact scoped family and distinct attack variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact attack primary and scoped choices required')
    gaps = {
        TORRENT: ['The live rules-engine action that returns Torrent Elemental from exile is outside the current duel driver.'],
        UMBRA: ['The fixture keeps the above-threshold creature untouched and chooses a legal target. An illegal target attempt is unobserved.'],
        GRASP: ['Aura and host share a controller in these fixtures.'],
        TOUCH: ['Aura and host share a controller in these fixtures.'],
    }
    cases = []
    source = FollowOn if csvid in FOLLOWERS else Board
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        board = source(oracle[name], csvid, mode, int(actor), rows, oracle)
        board.play()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=gaps.get(csvid, []))
        if csvid == TORRENT:
            metadata['covered_clauses'] = metadata['covered_clauses'][:2]
        cases.append(dict(name=label(csvid, mode, int(actor)), card=name, fixture='empty_main', seed=SEED,
                          setup=board.setup, steps=board.steps, _recipe=metadata))
    return cases

