"""Real entry costs and linked permanent, modifier and combat receipts."""

from functools import lru_cache
import json
from pathlib import Path

from entry_follow_on_damage_and_tap_recipes import CombatBoard, ZERO
from library_search_recipes import wave6_exact as exact
from recipes import cost_from_oracle, step

FAMILY = 'entry_interaction_combat'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'

AEGIS, STEPPE, SEISMIC, DAWN = 11772, 10829, 15211, 16686
SEED = 3404
MODELS = {
    AEGIS: ('{4}{W}{W}', 'Creature — Angel', '5', '5',
            'Flying (This creature can\'t be blocked except by creatures with flying or reach.)\n'
            'When this creature enters, another target permanent gains indestructible for as long as you control this creature. '
            '(Effects that say "destroy" don\'t destroy it. A creature with indestructible can\'t be destroyed by damage.)',
            ('Flying',), ('protect', 'opposing_artifact', 'source_leaves', 'source_control', 'source_only', 'flying')),
    STEPPE: ('', 'Land', None, None,
             'This land enters tapped.\nWhen this land enters, target creature you control gains protection from the color of your choice until end of turn.\n{T}: Add {W}.',
             (), ('red_block', 'blue_contrast', 'expire', 'foreign_only', 'mana', 'damage')),
    SEISMIC: ('{3}{R}{R}', 'Creature — Elemental', '4', '4',
              "When this creature enters, creatures without flying can't block this turn.",
              (), ('ground', 'flying', 'leaves', 'expire', 'countered')),
    DAWN: ('{4}{W}', 'Creature — Bird', '3', '3',
           'Flying\nWhen this creature enters, creatures you control get +1/+1 and gain vigilance until end of turn.',
           ('Flying',), ('vigilance', 'flying', 'flying_block', 'late_creature', 'countered')),
}
ARENA = 569
LEVIATHAN = 14445
GEISTS = 12168
MOGIS = 13487
PATRIARCH = 8116
WELKIN = 10101
BUSHWHACKER = 15664
PROVISIONER = 15867
HERALD = 16406
AMBITION = 16858
ZEAL = 16862
TRIAL = 17052

FOLLOWERS = {
    ARENA: ('{3}', 'Artifact', None, None,
        "Legendary creatures don't untap during their controllers' untap steps.\nWhen this artifact enters, tap all legendary creatures.",
        (), ('scope', 'untap', 'leaves', 'no_legend')),
    LEVIATHAN: ('{7}{U}{U}', 'Creature — Leviathan', '9', '9',
        "When this creature enters, if you cast it from your hand, tap all nonblue creatures. Those creatures don't untap during their controllers' next untap steps.",
        (), ('hand', 'one_untap', 'reanimate', 'countered')),
    GEISTS: ('{2}{U}{U}', 'Creature — Spirit', '3', '3',
        "Flying\nWhen this creature enters, tap target creature an opponent controls. That creature doesn't untap during its controller's untap step for as long as you control this creature.",
        ('Flying',), ('lock', 'leaves', 'control', 'own_only', 'flying')),
    MOGIS: ('{2}{B}', 'Creature — Human Berserker', '2', '2',
        "When this creature enters, up to X target creatures each gain intimidate and haste until end of turn, where X is your devotion to black. (A creature with intimidate can't be blocked except by artifact creatures and/or creatures that share a color with it. Each {B} in the mana costs of permanents you control counts toward your devotion to black.)",
        (), ('wrong_color', 'same_color', 'artifact', 'two', 'decline')),
    PATRIARCH: ('{4}{B}', 'Creature — Spirit', '4', '3',
        "When this creature enters, if {W} was spent to cast it, target player skips their next combat phase.\nThis creature can't block.",
        (), ('black_fee', 'white_fee', 'target_self', 'countered')),
    WELKIN: ('{4}{W}', 'Creature — Bird Cleric', '2', '2',
        'Flying\nWhen this creature enters, target creature gets +2/+2 and gains flying until end of turn.',
        ('Flying',), ('own', 'foe', 'expire', 'flying_source', 'countered')),
    BUSHWHACKER: ('{2}{R}', 'Creature — Goblin Warrior Ally', '2', '1',
        'Surge {1}{R} (You may cast this spell for its surge cost if you or a teammate has cast another spell this turn.)\nHaste\nWhen this creature enters, if its surge cost was paid, other creatures you control get +1/+0 and gain haste until end of turn.',
        ('Haste', 'Surge'), ('normal', 'surge', 'normal_choice', 'no_prior')),
    PROVISIONER: ('{3}{G}', 'Creature — Human Scout', '3', '3',
        'Trample\nWhen this creature enters, another target Human you control gets +2/+2 until end of turn.',
        ('Trample',), ('human', 'opponent_only', 'non_human', 'trample')),
    HERALD: ('{2}{W}', 'Creature — Human', '3', '2',
        'When this creature enters, target creature you control gets +1/+1 until end of turn.',
        (), ('own', 'self', 'expire', 'countered')),
    AMBITION: ('{2}{B}', 'Enchantment — Aura Cartouche', None, None,
        'Enchant creature you control\nWhen this Aura enters, you may put a -1/-1 counter on target creature.\nEnchanted creature gets +1/+1 and has lifelink.',
        ('Enchant',), ('foe_counter', 'own_counter', 'decline', 'foreign_only')),
    ZEAL: ('{R}', 'Enchantment — Aura Cartouche', None, None,
        "Enchant creature you control\nWhen this Aura enters, target creature can't block this turn.\nEnchanted creature gets +1/+1 and has haste. (It can attack and {T} no matter when it came under your control.)",
        ('Enchant',), ('fresh', 'remove', 'expire', 'foreign_only')),
    TRIAL: ('{2}{W}', 'Enchantment', None, None,
        "When this enchantment enters, creatures you control get +2/+1 and gain vigilance until end of turn.\nWhen a Cartouche you control enters, return this enchantment to its owner's hand.",
        (), ('vigilance', 'own_cartouche', 'foe_cartouche', 'late')),
}


WITNESSES = {
    'Goblin Tinkerer': ('{1}{R}', '{R}, {T}: Destroy target artifact. That artifact deals damage equal to its mana value to this creature.', '1', '2'),
    'Prodigal Sorcerer': ('{2}{U}', '{T}: This creature deals 1 damage to any target.', '1', '1'),
    'Fiery Hellhound': ('{1}{R}{R}', '{R}: This creature gets +1/+0 until end of turn.', '2', '2'),
    'Serra Angel': ('{3}{W}{W}', "Flying\nVigilance (Attacking doesn't cause this creature to tap.)", '4', '4'),
    'Aerie Bowmasters': ('{2}{G}{G}', 'Reach (This creature can block creatures with flying.)\n'
                         'Megamorph {5}{G} (You may cast this card face down as a 2/2 creature for {3}. '
                         'Turn it face up any time for its megamorph cost and put a +1/+1 counter on it.)', '3', '4'),
    'Welding Jar': ('{0}', 'Sacrifice this artifact: Regenerate target artifact.'),
    'Essence Scatter': ('{1}{U}', 'Counter target creature spell.'),
    'Unsummon': ('{U}', "Return target creature to its owner's hand."),
    'Murder': ('{1}{B}{B}', 'Destroy target creature.'),
    'Disenchant': ('{1}{W}', 'Destroy target artifact or enchantment.'),
    'Control Magic': ('{2}{U}{U}', 'Enchant creature\nYou control enchanted creature.'),
    'Pyroclasm': ('{1}{R}', 'Pyroclasm deals 2 damage to each creature.'),
}
NEW_WITNESSES = {
    'Drudge Skeletons': ('{1}{B}', '{B}: Regenerate this creature. (The next time this creature would be destroyed this turn, instead tap it, remove it from combat, and heal all damage on it.)', '1', '1'),
    'Isamaru, Hound of Konda': ('{W}', '', '2', '2'),
    'Sword of the Animist': ('{2}', 'Equipped creature gets +1/+1.\nWhenever equipped creature attacks, '
                             'you may search your library for a basic land card, put it onto the battlefield tapped, then shuffle.\nEquip {2}'),
    'Zombify': ('{3}{B}', 'Return target creature card from your graveyard to the battlefield.'),
    'Ravenous Rats': ('{1}{B}', 'When this creature enters, target opponent discards a card.', '1', '1'),
    'Gold Myr': ('{2}', '{T}: Add {W}.', '1', '1'),
    'Cartouche of Knowledge': ('{1}{U}', 'Enchant creature you control\nWhen this Aura enters, draw a card.\n'
                               'Enchanted creature gets +1/+1 and has flying.'),
    'Forest': ('', '({T}: Add {G}.)'),
}


class Board(CombatBoard):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid, self.mode, self.actor, self.foe = csvid, mode, actor, 1-actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet, self.life = [ZERO, ZERO], [20, 20]
        self.add('source', record['name'], actor, 'hand')
        if csvid == AEGIS:
            self.aegis_setup()
        elif csvid == STEPPE:
            self.steppe_setup()
        else:
            self.add('ally', 'Goblin Tinkerer', actor, 'battlefield')
            guard = 'Serra Angel' if csvid == DAWN and mode == 'flying_block' else 'Goblin Tinkerer'
            self.add('guard', guard, self.foe, 'battlefield')
            if mode == 'countered':
                self.add('counter', 'Essence Scatter', self.foe, 'hand')
            if csvid == SEISMIC:
                self.add('reach', 'Aerie Bowmasters', self.foe, 'battlefield')
                if mode == 'flying':
                    self.add('flyer', 'Serra Angel', self.foe, 'battlefield')
                if mode == 'leaves':
                    self.add('bounce', 'Unsummon', actor, 'hand')
            else:
                self.add('support', 'Prodigal Sorcerer', actor, 'battlefield')
                self.add('artifact', 'Welding Jar', actor, 'battlefield')
                if mode == 'late_creature':
                    self.add('new', 'Fiery Hellhound', actor, 'hand')

    def aegis_setup(self):
        mode = self.mode
        if mode == 'opposing_artifact':
            self.add('artifact', 'Welding Jar', self.foe, 'battlefield')
            self.add('disenchant', 'Disenchant', self.actor, 'hand')
            return
        if mode != 'source_only':
            self.add('ally', 'Goblin Tinkerer', self.actor, 'battlefield')
        if mode == 'flying':
            self.add('guard', 'Goblin Tinkerer', self.foe, 'battlefield')
            return
        self.add('murder', 'Murder', self.actor, 'hand')
        if mode == 'source_leaves':
            self.add('bounce', 'Unsummon', self.actor, 'hand')
        elif mode == 'source_control':
            self.add('control', 'Control Magic', self.foe, 'hand')
        elif mode == 'protect':
            self.add('sweep', 'Pyroclasm', self.actor, 'hand')
            self.add('guard', 'Prodigal Sorcerer', self.foe, 'battlefield')

    def steppe_setup(self):
        if self.mode != 'foreign_only' and self.mode != 'mana':
            self.add('ally', 'Goblin Tinkerer', self.actor, 'battlefield')
        if self.mode != 'mana':
            self.add('guard', 'Goblin Tinkerer', self.foe, 'battlefield')
        if self.mode == 'damage':
            self.add('sweep', 'Pyroclasm', self.actor, 'hand')
            self.add('unprotected', 'Prodigal Sorcerer', self.actor, 'battlefield')

    def pending(self, ref, *, response=False):
        who = self.cards[ref]['player']
        self.fund(who, cost_from_oracle(self.oracle[self.cards[ref]['name']]))
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_stack', value=2 if response else 1)
        self.wallet[who] = ZERO
        self.pools()

    def enter(self, target=None, *, color=None):
        if self.csvid != STEPPE:
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
        if color is not None:
            self.emit('choose_color', player=self.actor, value=color)
        if self.csvid == STEPPE:
            self.emit('play_land', 'source', value=1)
        self.emit('resolve')
        self.move('source', 'battlefield')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)
        return True

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

    def aegis(self):
        mode = self.mode
        target = None if mode == 'source_only' else 'artifact' if mode == 'opposing_artifact' else 'ally'
        self.enter(target)
        self.size('source')
        if mode == 'opposing_artifact':
            self.cast('disenchant', target='artifact')
            self.prop('artifact', 'in_play', 1)
        elif mode == 'flying':
            self.round_trip_turn()
            self.battle('source', attempts=(('guard', False),))
        elif mode == 'source_only':
            self.cast('murder', target='source')
            self.move('source', 'graveyard')
        else:
            if mode == 'source_leaves':
                self.cast('bounce', target='source')
                self.move('source', 'hand')
            elif mode == 'source_control':
                self.emit('advance_turn')
                self.cast('control', target='source', destination='battlefield')
                self.cards['source']['player'] = self.foe
                self.emit('bind_permanent', 'source', player=self.foe, index=-1, value=AEGIS)
                self.prop('source', 'in_play', 1)
                self.emit('advance_turn')
            self.cast('murder', target='ally')
            if mode == 'protect':
                self.size('ally')
                self.cast('sweep')
                self.move('guard', 'graveyard')
                self.size('ally')
            else:
                self.move('ally', 'graveyard')

    def steppe(self):
        mode = self.mode
        own = 'ally' in self.cards
        self.enter('ally' if own else None, color=(2 if mode == 'blue_contrast' else 4) if own else None)
        self.prop('source', 'tapped', 1)
        self.emit('can_activate', 'source', value=0)
        if mode in ('red_block', 'blue_contrast', 'expire'):
            if mode == 'expire':
                self.round_trip_turn()
            legal = mode != 'red_block'
            self.battle('ally', attempts=(('guard', legal),), blocker='guard' if legal else None)
        elif mode == 'damage':
            self.cast('sweep')
            self.move('guard', 'graveyard')
            self.move('unprotected', 'graveyard')
            self.size('ally')
            self.prop('ally', 'damage', 0)
        elif mode == 'mana':
            self.round_trip_turn()
            self.emit('untap_step', player=self.actor)
            self.prop('source', 'tapped', 0)
            self.emit('can_activate', 'source', value=1)
            self.emit('activate', 'source', value=1)
            self.wallet[self.actor] = (0, 0, 0, 0, 0, 1)
            self.prop('source', 'tapped', 1)
            self.emit('can_activate', 'source', value=0)

    def seismic(self):
        entered = self.enter()
        if entered:
            self.size('source')
        if self.mode == 'leaves':
            self.cast('bounce', target='source')
            self.move('source', 'hand')
        if self.mode == 'expire':
            self.round_trip_turn()
        ground_legal = not entered or self.mode == 'expire'
        attempts = [('guard', ground_legal)]
        blocker = 'guard' if ground_legal else None
        if not ground_legal:
            attempts.append(('reach', False))
        if self.mode == 'flying':
            attempts.append(('flyer', True))
            blocker = 'flyer'
        self.battle('ally', attempts=attempts, blocker=blocker)

    def dawn(self):
        if not self.enter():
            self.size('ally')
            self.size('support')
            self.battle('ally')
            return
        affected = ('source', 'ally', 'support')
        buffed = {ref: tuple(n+1 for n in self.stats(ref)) for ref in affected}
        for ref, stats in buffed.items():
            self.size(ref, stats)
        self.size('guard')
        self.prop('source', 'sick', 1)
        if self.mode == 'late_creature':
            self.cast('new', destination='battlefield')
            self.size('new')
            self.prop('new', 'sick', 1)
            for ref, stats in buffed.items():
                self.size(ref, stats)
        elif self.mode == 'vigilance':
            self.battle('ally', stats=buffed['ally'], vigilance=True)
            self.round_trip_turn()
            self.size('ally')
            self.battle('ally')
        else:
            self.round_trip_turn()
            self.size('source')
            legal = self.mode == 'flying_block'
            self.battle('source', attempts=(('guard', legal),), blocker='guard' if legal else None)
        self.emit('cleanup')
        for ref in affected:
            if self.cards[ref]['zone'] == 'battlefield':
                self.size(ref)

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            self.fund(who, ZERO)
        {AEGIS: self.aegis, STEPPE: self.steppe, SEISMIC: self.seismic, DAWN: self.dawn}[self.csvid]()
        self.receipt()


def label(csvid, mode, actor):
    return f'EntryInteractionCombatRecipe_{csvid}_{mode}_{actor}'


class CompanionBoard(Board):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid, self.mode, self.actor, self.foe = csvid, mode, actor, 1-actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet, self.life = [ZERO, ZERO], [20, 20]
        self.add('source', record['name'], actor, 'graveyard' if csvid == LEVIATHAN and mode == 'reanimate' else 'hand')
        if mode == 'countered':
            self.add('counter', 'Essence Scatter', self.foe, 'hand')
        if csvid in (ARENA, LEVIATHAN, GEISTS):
            self.setup_tap_sources()
        elif csvid in (MOGIS, PATRIARCH, WELKIN, BUSHWHACKER, PROVISIONER, HERALD):
            self.setup_combat_sources()
        else:
            self.setup_enchantments()

    def setup_tap_sources(self):
        cid, mode = self.csvid, self.mode
        if cid == ARENA:
            if mode != 'no_legend':
                self.add('own_legend', 'Isamaru, Hound of Konda', self.actor, 'battlefield')
                self.add('foe_legend', 'Isamaru, Hound of Konda', self.foe, 'battlefield')
            self.add('legend_equipment', 'Sword of the Animist', self.actor, 'battlefield')
            self.add('normal', 'Goblin Tinkerer', self.foe, 'battlefield', tapped=mode == 'untap')
            if mode == 'leaves':
                self.add('remove', 'Disenchant', self.actor, 'hand')
        elif cid == LEVIATHAN:
            if mode == 'reanimate':
                self.add('raise', 'Zombify', self.actor, 'hand')
            for seat, prefix in ((self.actor, 'own'), (self.foe, 'foe')):
                self.add(prefix+'_red', 'Goblin Tinkerer', seat, 'battlefield')
                self.add(prefix+'_blue', 'Prodigal Sorcerer', seat, 'battlefield')
            self.add('relic', 'Welding Jar', self.actor, 'battlefield')
        else:
            self.add('ally', 'Goblin Tinkerer', self.actor, 'battlefield')
            if mode != 'own_only':
                self.add('guard', 'Goblin Tinkerer', self.foe, 'battlefield')
            if mode != 'own_only':
                self.add('reserve', 'Fiery Hellhound', self.foe, 'battlefield', tapped=mode != 'flying')
            if mode == 'leaves':
                self.add('bounce', 'Unsummon', self.actor, 'hand')
            elif mode == 'control':
                self.add('control', 'Control Magic', self.foe, 'hand')

    def setup_combat_sources(self):
        cid, mode = self.csvid, self.mode
        ally = 'Prodigal Sorcerer' if cid == PROVISIONER and mode == 'human' else 'Goblin Tinkerer'
        if not (cid == PROVISIONER and mode == 'opponent_only' or cid == HERALD and mode == 'self'):
            self.add('ally', ally, self.actor, 'hand' if cid in (MOGIS, BUSHWHACKER) and mode in (
                'two', 'surge', 'normal_choice') else 'battlefield')
        guard = 'Prodigal Sorcerer' if cid == PROVISIONER and mode != 'trample' else 'Goblin Tinkerer'
        if cid == MOGIS:
            guard = {'same_color': 'Drudge Skeletons', 'artifact': 'Gold Myr', 'two': 'Prodigal Sorcerer'}.get(mode, guard)
            if mode == 'two':
                self.add('devotion', 'Ravenous Rats', self.actor, 'battlefield')
        self.add('guard', guard, self.foe, 'battlefield')

    def setup_enchantments(self):
        cid, mode = self.csvid, self.mode
        if mode != 'foreign_only':
            self.add('ally', 'Goblin Tinkerer', self.actor, 'hand' if cid == ZEAL and mode == 'fresh' else 'battlefield')
        if cid == AMBITION and mode == 'own_counter':
            self.add('counter_recipient', 'Fiery Hellhound', self.actor, 'battlefield')
        if cid == AMBITION and mode == 'foe_counter':
            self.add('victim', 'Prodigal Sorcerer', self.foe, 'battlefield')
        self.add('guard', 'Goblin Tinkerer', self.foe, 'battlefield')
        if cid == ZEAL and mode == 'remove':
            self.add('remove', 'Disenchant', self.actor, 'hand')
        if cid == TRIAL and mode == 'late':
            self.add('new', 'Fiery Hellhound', self.actor, 'hand')
        if cid == TRIAL and mode.endswith('cartouche'):
            owner = self.foe if mode == 'foe_cartouche' else self.actor
            self.add('cartouche', 'Cartouche of Knowledge', owner, 'hand')
            self.add('drawn', 'Forest', owner, 'library')

    def id(self, ref):
        return self.rows[self.cards[ref]['name']]['csvid']

    def choose_target(self, target, *, player=None):
        self.emit('choose_target', target, player=self.actor if player is None else player)

    def finish_entry(self):
        self.emit('resolve')
        self.move('source', 'battlefield')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)

    def paid_source(self, fee, *, menu=None, check_availability=False):
        self.fund(self.actor, fee)
        if check_availability:
            self.emit('can_cast', 'source', value=1)
        if menu is not None:
            self.emit('choose_dialog', player=self.actor, value=menu)
        self.emit('cast', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.wallet[self.actor] = ZERO
        self.pools()

    def observe_tapped(self, refs, tapped):
        for ref in refs:
            self.prop(ref, 'tapped', int(tapped))

    def untap_next(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)

    def arena(self):
        self.enter()
        legends = ('own_legend', 'foe_legend') if self.mode != 'no_legend' else ()
        self.observe_tapped(legends, True)
        self.prop('legend_equipment', 'tapped', 0)
        self.prop('normal', 'tapped', int(self.mode == 'untap'))
        if self.mode == 'leaves':
            self.cast('remove', target='source')
            self.move('source', 'graveyard')
        if self.mode in ('untap', 'leaves'):
            self.untap_next(self.foe)
            self.observe_tapped(('foe_legend',), self.mode == 'untap')
            self.prop('normal', 'tapped', 0)
            self.untap_next(self.actor)
            self.observe_tapped(('own_legend',), self.mode == 'untap')

    def leviathan(self):
        if self.mode == 'reanimate':
            self.emit('choose_zone_card', player=self.actor, value=self.id('source'))
            self.cast('raise')
            self.move('source', 'battlefield')
            self.emit('bind_permanent', 'source', player=self.actor, index=-1, value=LEVIATHAN)
            self.size('source')
            entered_from_hand = False
        else:
            entered_from_hand = self.enter()
            if entered_from_hand:
                self.size('source')
        self.observe_tapped(('own_red', 'foe_red'), entered_from_hand)
        self.observe_tapped(('own_blue', 'foe_blue', 'relic'), False)
        if self.mode == 'one_untap':
            for still_locked in (True, False):
                for player, prefix in ((self.foe, 'foe'), (self.actor, 'own')):
                    self.untap_next(player)
                    self.observe_tapped((prefix+'_red',), still_locked)
                    self.observe_tapped((prefix+'_blue',), False)

    def geists(self):
        self.enter(None if self.mode == 'own_only' else 'guard')
        self.size('source')
        self.prop('ally', 'tapped', 0)
        if self.mode == 'own_only':
            return
        self.prop('guard', 'tapped', 1)
        if self.mode == 'flying':
            self.round_trip_turn()
            self.prop('reserve', 'tapped', 0)
            self.battle('source', attempts=(('reserve', False),))
            return
        if self.mode == 'leaves':
            self.cast('bounce', target='source')
            self.move('source', 'hand')
        self.untap_next(self.foe)
        self.prop('guard', 'tapped', int(self.mode != 'leaves'))
        self.prop('reserve', 'tapped', 0)
        if self.mode == 'control':
            self.cast('control', target='source', destination='battlefield')
            self.cards['source']['player'] = self.foe
            self.emit('bind_permanent', 'source', player=self.foe, index=-1, value=GEISTS)
            self.round_trip_turn()
            self.emit('untap_step', player=self.foe)
            self.prop('guard', 'tapped', 0)

    def mogis(self):
        mode = self.mode
        if mode == 'two':
            self.cast('ally', destination='battlefield')
            self.prop('ally', 'sick', 1)
        self.pending('source')
        if mode == 'decline':
            self.emit('choose_cancel_target', player=self.actor)
        else:
            self.choose_target('source')
            if mode == 'two':
                self.choose_target('ally')
        self.finish_entry()
        self.size('source')
        self.prop('source', 'sick', int(mode == 'decline'))
        blocker_is_legal = mode in ('decline', 'same_color', 'artifact')
        attacker = 'ally' if mode in ('decline', 'two') else 'source'
        self.battle(attacker, attempts=(('guard', blocker_is_legal),), blocker='guard' if blocker_is_legal else None)

    def patriarch(self):
        mode = self.mode
        white = mode in ('white_fee', 'target_self')
        if white:
            self.paid_source((3, 1, 0, 0, 0, 1), menu=1 if self.actor == 0 else None)
            self.emit('choose_player', player=self.actor, value=self.actor if mode == 'target_self' else self.foe)
            self.finish_entry()
        else:
            self.enter()
        if mode == 'target_self':
            self.patriarch_combat('ally', allowed=False)
            return
        self.emit('advance_turn')
        self.patriarch_combat('guard', allowed=not white, reject_source=mode == 'black_fee')
        if white:
            self.round_trip_turn()
            self.patriarch_combat('guard', allowed=True, reject_source=True)

    def patriarch_combat(self, attacker, *, allowed, reject_source=False):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=int(allowed))
        self.prop(attacker, 'attacking', int(allowed))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if reject_source:
            self.emit('block', 'source', attacker, value=0)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        if self.mode == 'white_fee' and not allowed:
            self.steps.append(step('end_combat_step', value=0))
        else:
            self.emit('end_combat')
        if allowed:
            self.life[self.actor] -= self.stats(attacker)[0]

    def targeted_stats(self):
        cid, mode = self.csvid, self.mode
        if cid == PROVISIONER:
            target = 'ally' if mode == 'human' else None
        else:
            target = {'foe': 'guard', 'self': 'source'}.get(mode, 'ally')
        if not self.enter(target):
            self.size('ally')
            self.size('guard')
            return
        if target != 'source':
            self.size('source')
        if target:
            amount = 1 if cid == HERALD else 2
            boosted = tuple(n+amount for n in self.stats(target))
            self.size(target, boosted)
            self.size('ally' if target == 'guard' else 'guard')
        else:
            if 'ally' in self.cards:
                self.size('ally')
            self.size('guard')
        if mode in ('own', 'human'):
            self.battle('ally', attempts=(('guard', False),) if cid == WELKIN else (), stats=boosted)
        elif mode in ('flying_source', 'trample'):
            self.round_trip_turn()
            if mode == 'trample':
                self.battle('source', attempts=(('guard', True),), blocker='guard', trample=True)
            else:
                self.battle('source', attempts=(('guard', False),))
        elif cid == WELKIN and mode == 'expire':
            self.round_trip_turn()
            self.size('ally')
            self.battle('ally', attempts=(('guard', True),), blocker='guard')
        self.emit('cleanup')
        if target and self.cards[target]['zone'] == 'battlefield':
            self.size(target)

    def bushwhacker(self):
        mode = self.mode
        if mode in ('surge', 'normal_choice'):
            self.pending('ally')
            self.emit('announce_spell', 'ally')
            self.emit('resolve')
            self.move('ally', 'battlefield')
            self.prop('ally', 'sick', 1)
        if mode == 'no_prior':
            fee = (1, 0, 0, 0, 1, 0)
            self.fund(self.actor, fee)
            self.emit('cast', 'source', value=0)
            return
        if mode == 'surge':
            self.paid_source((1, 0, 0, 0, 1, 0))
            self.finish_entry()
        elif mode == 'normal_choice':
            self.paid_source((2, 0, 0, 0, 1, 0), menu=1)
            self.finish_entry()
        else:
            self.enter()
        self.size('source')
        self.prop('source', 'sick', 0)
        enhanced = (2, 2) if mode == 'surge' else (1, 2)
        self.size('ally', enhanced)
        self.prop('ally', 'sick', int(mode == 'normal_choice'))
        self.size('guard')
        attacker = 'ally' if mode == 'surge' else 'source'
        self.battle(attacker, stats=enhanced if attacker == 'ally' else None)
        self.emit('cleanup')
        self.size('ally')

    def cartouche(self):
        mode = self.mode
        if mode == 'foreign_only':
            self.fund(self.actor, cost_from_oracle(self.oracle[self.cards['source']['name']]))
            self.emit('can_cast', 'source', value=0)
            self.size('guard')
            return
        if mode == 'fresh':
            self.cast('ally', destination='battlefield')
            self.prop('ally', 'sick', 1)
        self.choose_target('ally')
        ai_declines = self.csvid == AMBITION and self.actor == 1 and mode in ('foe_counter', 'own_counter')
        if ai_declines:
            fee = cost_from_oracle(self.oracle[self.cards['source']['name']])
            self.paid_source(fee, check_availability=True)
        else:
            self.pending('source')
        if self.csvid == ZEAL:
            self.choose_target('guard')
        elif mode != 'decline' and not ai_declines:
            self.emit('choose_trigger', 'source', player=self.actor)
            self.choose_target('victim' if mode == 'foe_counter' else 'counter_recipient')
        self.finish_entry()
        if ai_declines:
            recipient = 'victim' if mode == 'foe_counter' else 'counter_recipient'
            self.emit('assert_counter', recipient, prop='minus1', value=0)
            self.size(recipient)
        elif mode == 'foe_counter':
            self.move('victim', 'graveyard')
        elif mode == 'own_counter':
            self.emit('assert_counter', 'counter_recipient', prop='minus1', value=1)
            self.size('counter_recipient', (1, 1))
        boosted = (2, 3)
        self.size('ally', boosted)
        self.size('guard')
        if mode == 'remove':
            self.cast('remove', target='source')
            self.move('source', 'graveyard')
            boosted = (1, 2)
            self.size('ally', boosted)
        elif mode == 'expire':
            self.round_trip_turn()
            self.size('ally', boosted)
        if self.csvid == ZEAL:
            self.prop('ally', 'sick', 0)
            legal = mode == 'expire'
            self.battle('ally', attempts=(('guard', legal),), blocker='guard' if legal else None, stats=boosted)
        else:
            self.battle('ally', stats=boosted, lifelink=True)

    def trial(self):
        self.enter()
        self.size('ally', (3, 3))
        self.size('guard')
        if self.mode == 'vigilance':
            self.battle('ally', stats=(3, 3), vigilance=True)
            self.round_trip_turn()
            self.size('ally')
            self.battle('ally')
        elif self.mode == 'late':
            self.cast('new', destination='battlefield')
            self.size('new')
            self.size('ally', (3, 3))
        else:
            own = self.mode == 'own_cartouche'
            if not own:
                self.emit('advance_turn')
            host = 'ally' if own else 'guard'
            self.cast('cartouche', target=host, destination='battlefield')
            self.move('drawn', 'hand')
            if own:
                self.move('source', 'hand')
                self.size('ally', (4, 4))
                self.emit('bind_hand', 'source', player=self.actor, index=-1, value=TRIAL)
                self.enter()
                self.size('ally', (6, 5))
            else:
                self.size('guard', (2, 3))
                self.size('ally')

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.fund(player, ZERO)
        actions = {ARENA: self.arena, LEVIATHAN: self.leviathan, GEISTS: self.geists, MOGIS: self.mogis,
                   PATRIARCH: self.patriarch, WELKIN: self.targeted_stats, BUSHWHACKER: self.bushwhacker,
                   PROVISIONER: self.targeted_stats, HERALD: self.targeted_stats, AMBITION: self.cartouche,
                   ZEAL: self.cartouche, TRIAL: self.trial}
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
    pins = profiles()
    witnesses = WITNESSES | NEW_WITNESSES
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != witnesses.keys()
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODELS.keys() | FOLLOWERS.keys()):
        raise ValueError('Exact source and witness scope required')
    for group in pins.values():
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
    factory = CompanionBoard if csvid in FOLLOWERS else Board
    gaps = {PATRIARCH: ['Combat availability is exercised through real declarations. Full automatic phase elision is not separately observed.']}
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


