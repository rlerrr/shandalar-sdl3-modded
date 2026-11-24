"""Paid entry combat modifiers with exact target, payment and combat receipts."""

from functools import lru_cache
import json
from pathlib import Path

from entry_follow_on_damage_and_tap_recipes import CombatBoard, ZERO
from library_search_recipes import wave6_exact as exact
from recipes import cost_from_oracle

FAMILY = 'entry_rider_combat_modifiers'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'

GOBLIN, PEAKS, HOOF, ANGEL=10549,10684,12334,18879
MODELS={
    GOBLIN: ('{1}{R}','Creature — Goblin Scout','2','1',
             "When this creature enters, target creature can't block this turn.",
             ('foe','other_blocks','own','self','countered')),
    PEAKS: ('','Land',None,None,
            'This land enters tapped.\nWhen this land enters, target creature gets +2/+0 until end of turn.\n{T}: Add {R}.',
            ('own','foe','expire','no_creatures','mana')),
    HOOF: ('{5}{G}{G}{G}','Creature — Beast','5','5',
           'Haste\nWhen this creature enters, creatures you control gain trample and get +X/+X until end of turn, where X is the number of creatures you control.',
           ('self_only','team','response_loss','late_creature','countered')),
    ANGEL: ('{4}{W}','Creature — Angel','3','3',
            'Flying\nWhen this creature enters, creatures you control get +1/+1 and gain vigilance until end of turn.',
            ('vigilance','flying','flying_block','late_creature','countered')),
}
WITNESSES={
    'Goblin Tinkerer':('{1}{R}','{R}, {T}: Destroy target artifact. That artifact deals damage equal to its mana value to this creature.','1','2'),
    'Prodigal Sorcerer':('{2}{U}','{T}: This creature deals 1 damage to any target.','1','1'),
    'Serra Angel':('{3}{W}{W}',"Flying\nVigilance (Attacking doesn't cause this creature to tap.)",'4','4'),
    'Fiery Hellhound':('{1}{R}{R}','{R}: This creature gets +1/+0 until end of turn.','2','2'),
    'Essence Scatter':('{1}{U}','Counter target creature spell.'),
    'Unsummon':('{U}',"Return target creature to its owner's hand."),
    'Welding Jar':('{0}','Sacrifice this artifact: Regenerate target artifact.'),
}
SEED=3308

BATTLE_EAGLE, CROSSWAY, CATHAR, ZEALOT = 12543, 11913, 12369, 7844
HAMMER, ACOLYTE, WARLORD = 14110, 9183, 12230
SEACLIFF, GROVE, SPIRES, BRIDGE, FORERUNNERS = 10667, 10696, 15368, 15427, 19614
LANDS = {SEACLIFF, GROVE, SPIRES, BRIDGE}
LAND_MODES = ('own', 'foe', 'expire', 'no_creatures', 'mana')
FORBID = "When this creature enters, target creature can't block this turn."
FOLLOWERS = {
    BATTLE_EAGLE: ('{4}{W}', 'Creature — Bird', '2', '2',
        'Flying\nWhen this creature enters, target creature gets +2/+2 and gains flying until end of turn.',
        ('Flying',), ('own', 'foe', 'expire', 'flying_source', 'countered')),
    CROSSWAY: ('{1}{R}{R}', 'Creature — Vampire', '3', '2', FORBID,
        (), ('foe', 'other_blocks', 'own', 'self', 'countered')),
    CATHAR: ('{2}{R}', 'Creature — Human Knight', '2', '1', 'Haste\n' + FORBID,
        ('Haste',), ('haste', 'other_blocks', 'own', 'expire', 'countered')),
    ZEALOT: ('{1}{R}{R}{W}', 'Creature — Elemental Berserker', '2', '2',
        'When this creature enters, creatures you control get +1/+1 and gain haste until end of turn.',
        (), ('team', 'source_haste', 'fresh_haste', 'late_creature', 'countered')),
    HAMMER: ('{R}', 'Enchantment — Aura', None, None,
        "Enchant creature\nWhen this Aura enters, target creature can't block this turn.\nEnchanted creature gets +1/+1 and has haste.",
        ('Enchant',), ('host_haste', 'separate_target', 'remove_aura', 'expire', 'countered')),
    ACOLYTE: ('{1}{R}{R}', 'Creature — Elemental Shaman', '2', '2',
        "When this creature enters, target creature gets +2/+0 and gains haste until end of turn.\n"
        "Evoke {R} (You may cast this spell for its evoke cost. If you do, it's sacrificed when it enters.)",
        ('Evoke',), ('normal', 'foe', 'self', 'evoke', 'evoke_countered')),
    WARLORD: ('{5}{R}', 'Creature — Vampire Warrior', '4', '4',
        "Haste (This creature can attack and {T} as soon as it comes under your control.)\n"
        "When this creature enters, up to two target creatures can't block this turn.",
        ('Haste',), ('two', 'one', 'decline', 'expire', 'countered')),
    SEACLIFF: ('', 'Land', None, None,
        'This land enters tapped.\nWhen this land enters, target creature gains flying until end of turn.\n{T}: Add {U}.',
        (), LAND_MODES),
    GROVE: ('', 'Land', None, None,
        'This land enters tapped.\nWhen this land enters, target creature gets +1/+1 until end of turn.\n{T}: Add {G}.',
        (), LAND_MODES),
    SPIRES: ('', 'Land', None, None,
        'This land enters tapped.\nWhen this land enters, target creature gets +1/+1 and gains first strike until end of turn.\n{T}: Add {R}.',
        (), LAND_MODES),
    BRIDGE: ('', 'Land', None, None,
        'This land enters tapped.\nWhen this land enters, target creature gets +1/+1 and gains vigilance until end of turn.\n{T}: Add {W}.',
        (), LAND_MODES),
    FORERUNNERS: ('{5}{G}{G}{G}', 'Creature — Boar', '7', '7',
        'Vigilance, trample, haste\nWhen this creature enters, other creatures you control get +2/+2 and gain vigilance and trample until end of turn.',
        ('Vigilance', 'Trample', 'Haste'), ('source_traits', 'others', 'late_creature', 'expire', 'countered')),
}
NEW_WITNESSES = {
    'Counterspell': ('{U}{U}', 'Counter target spell.'),
    'Disenchant': ('{1}{W}', 'Destroy target artifact or enchantment.'),
}
PILOT_KEYWORDS = {HOOF: ('Haste',), ANGEL: ('Flying',)}


def label(csvid,mode,actor):
    return f'EntryRiderCombatModifiersRecipe_{csvid}_{mode}_{actor}'


class Board(CombatBoard):
    def __init__(self,record,csvid,mode,actor,rows,oracle):
        self.csvid,self.mode,self.actor,self.foe=csvid,mode,actor,1-actor
        self.rows,self.oracle=rows,oracle
        self.setup,self.steps,self.cards=[],[],{}
        self.wallet,self.life=[ZERO,ZERO],[20,20]
        self.add('source',record['name'],actor,'hand')
        if mode=='countered':
            self.add('counter','Essence Scatter',self.foe,'hand')
        no_board=csvid==PEAKS and mode in ('no_creatures','mana')
        if not no_board:
            self.add('guard','Serra Angel' if mode=='flying_block' else 'Goblin Tinkerer',self.foe,'battlefield')
        if not no_board and mode!='self_only':
            self.add('ally','Goblin Tinkerer',actor,'battlefield')
        if csvid in (HOOF,ANGEL):
            self.add('artifact','Welding Jar',actor,'battlefield')
            if mode in ('vigilance','team','response_loss'):
                self.add('other_ally','Prodigal Sorcerer',actor,'battlefield')
        if csvid==GOBLIN and mode in ('foe','other_blocks'):
            self.add('reserve','Fiery Hellhound',actor,'battlefield')
        if mode=='other_blocks':
            self.add('other','Prodigal Sorcerer',self.foe,'battlefield')
        if mode=='response_loss':
            self.add('bounce','Unsummon',self.foe,'hand')
        if mode=='late_creature':
            self.add('new','Fiery Hellhound',actor,'hand')

    def pending_response(self, ref, target):
        self.fund(self.foe, cost_from_oracle(self.oracle[self.cards[ref]['name']]))
        self.emit('choose_target', target, player=self.foe)
        self.emit('cast_response', ref, value=1)
        self.emit('assert_stack', value=2)
        self.wallet[self.foe] = ZERO
        self.pools()

    def target(self):
        if self.csvid==GOBLIN:
            return {'own':'ally','self':'source'}.get(self.mode,'guard')
        if self.csvid==PEAKS and self.mode not in ('no_creatures','mana'):
            return 'guard' if self.mode=='foe' else 'ally'
        return None

    def entry(self):
        chosen=self.target()
        if self.csvid==PEAKS:
            if chosen:
                self.emit('choose_target',chosen,player=self.actor)
            self.emit('play_land','source',value=1)
            self.emit('resolve')
        else:
            self.pending('source')
            if self.mode=='countered':
                self.emit('announce_spell','source')
                self.pending('counter',response=True)
                self.emit('resolve_response')
                self.move('source','graveyard')
                self.move('counter','graveyard')
                return False
            if self.mode=='response_loss':
                self.emit('announce_spell','source')
                self.pending_response('bounce','other_ally')
                self.emit('resolve_response')
                self.move('bounce','graveyard')
                self.move('other_ally','hand')
            else:
                if chosen:
                    self.emit('choose_target',chosen,player=self.actor)
                self.emit('resolve')
        self.move('source','battlefield')
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_stack',value=0)
        return True

    def battle(self,attacker,*,attempts=(),blocker=None,attack_stats=None,block_stats=None,trample=False,
               vigilance=False,first_strike=False,block_first_strike=False):
        self.begin(attacker)
        self.prop(attacker,'tapped',int(not vigilance))
        self.emit('begin_blockers')
        for ref,legal in attempts:
            self.emit('block',ref,attacker,value=int(legal))
        self.emit('advance_combat')
        self.prop(attacker,'blocked',int(blocker is not None))
        if blocker:
            self.prop(blocker,'blocking_count',1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        power,toughness=attack_stats or self.stats(attacker)
        if blocker is None:
            self.life[self.foe]-=power
        else:
            b_power,b_toughness=block_stats or self.stats(blocker)
            if trample:
                self.life[self.foe]-=max(0,power-b_toughness)
            retaliation=0 if first_strike and power>=b_toughness else b_power
            outgoing=0 if block_first_strike and b_power>=toughness else power
            for ref,damage,tough in ((attacker,retaliation,toughness),(blocker,outgoing,b_toughness)):
                if damage>=tough:
                    self.move(ref,'graveyard')
                else:
                    self.prop(ref,'damage',damage)
        self.receipt()

    def goblin(self,entered):
        self.prop('guard','tapped',0)
        if entered:
            self.size('source')
            self.prop('source','sick',1)
        denied=entered and self.mode in ('foe','other_blocks')
        attempts=[('guard',not denied)]
        if self.mode=='other_blocks':
            attempts.append(('other',True))
        blocker='other' if self.mode=='other_blocks' else None if denied else 'guard'
        self.battle('ally',attempts=attempts,blocker=blocker)
        if denied and self.mode=='foe':
            self.round_trip_turn()
            self.battle('reserve',attempts=(('guard',True),),blocker='guard')

    def peaks(self):
        chosen=self.target()
        self.prop('source','tapped',1)
        self.emit('can_activate','source',value=0)
        if chosen:
            base=self.stats(chosen)
            raised=(base[0]+2,base[1])
            self.size(chosen,raised)
            self.size('guard' if chosen=='ally' else 'ally')
        self.receipt()
        if self.mode=='mana':
            self.round_trip_turn()
            self.emit('untap_step',player=self.actor)
            self.prop('source','tapped',0)
            self.emit('can_activate','source',value=1)
            self.emit('activate','source',value=1)
            self.wallet[self.actor]=(0,0,0,0,1,0)
            self.prop('source','tapped',1)
            self.emit('can_activate','source',value=0)
        if self.mode=='own':
            self.battle('ally',attack_stats=raised)
        if self.mode=='foe':
            self.battle('ally',attempts=(('guard',True),),blocker='guard',block_stats=raised)
        if self.mode=='expire':
            self.emit('cleanup')
            self.size(chosen)

    def team(self):
        formula=self.csvid==HOOF
        refs=['source']
        for ref in ('ally','other_ally'):
            if ref in self.cards and self.cards[ref]['zone']=='battlefield':
                refs.append(ref)
        amount=len(refs) if formula else 1
        modified={ref:tuple(n+amount for n in self.stats(ref)) for ref in refs}
        for ref, stats in modified.items():
            self.size(ref,stats)
        self.size('guard')
        self.prop('source','sick',int(not formula))
        self.receipt()
        if self.mode=='late_creature':
            self.cast('new',destination='battlefield')
            self.size('new')
            for ref, stats in modified.items():
                self.size(ref,stats)
        elif formula:
            attacker='source' if self.mode=='self_only' else 'ally'
            self.battle(attacker,attempts=(('guard',True),),blocker='guard',
                        attack_stats=modified[attacker],trample=True)
        elif self.mode=='vigilance':
            self.emit('begin_attackers')
            self.emit('attack','source',value=0)
            self.emit('attack','ally',value=1)
            self.prop('ally','attacking',1)
            self.prop('ally','tapped',0)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.life[self.foe]-=modified['ally'][0]
            self.receipt()
            self.round_trip_turn()
            self.size('ally')
            self.battle('ally')
        else:
            self.round_trip_turn()
            self.size('source')
            legal=self.mode=='flying_block'
            self.battle('source',attempts=(('guard',legal),),blocker='guard' if legal else None)
        self.emit('cleanup')
        for ref in refs:
            if self.cards[ref]['zone']=='battlefield':
                self.size(ref)

    def play(self):
        self.emit('turn',player=self.actor)
        self.emit('present_ai_dialogs',value=1)
        for who in (0,1):
            self.fund(who,ZERO)
        entered=self.entry()
        if self.csvid==GOBLIN:
            self.goblin(entered)
        elif self.csvid==PEAKS:
            self.peaks()
        elif entered:
            self.team()
        else:
            self.size('ally')
            self.size('guard')
            self.battle('ally')
        self.receipt()


class FollowOn(Board):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid, self.mode, self.actor, self.foe = csvid, mode, actor, 1-actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet, self.life = [ZERO, ZERO], [20, 20]
        self.add('source', record['name'], actor, 'hand')
        if mode in ('countered', 'evoke_countered'):
            counter = 'Counterspell' if csvid == HAMMER else 'Essence Scatter'
            self.add('counter', counter, self.foe, 'hand')
        if csvid in LANDS and mode in ('no_creatures', 'mana'):
            return
        guard = 'Fiery Hellhound' if csvid == SPIRES and mode == 'own' else 'Goblin Tinkerer'
        if csvid == FORERUNNERS and mode == 'expire':
            guard = 'Prodigal Sorcerer'
        self.add('guard', guard, self.foe, 'battlefield')
        if mode in ('fresh_haste', 'host_haste') or csvid == ACOLYTE and mode in ('normal', 'evoke'):
            self.add('ally', 'Goblin Tinkerer', actor, 'hand')
        else:
            ally = 'Fiery Hellhound' if csvid == FORERUNNERS and mode == 'expire' else 'Goblin Tinkerer'
            self.add('ally', ally, actor, 'battlefield')
        if mode in ('two', 'expire') and csvid == WARLORD:
            self.add('second_guard', 'Fiery Hellhound', self.foe, 'battlefield')
        if mode in ('other_blocks', 'two', 'one', 'separate_target'):
            self.add('other', 'Prodigal Sorcerer', self.foe, 'battlefield')
        if csvid in (ZEALOT, FORERUNNERS):
            self.add('support', 'Prodigal Sorcerer', actor, 'battlefield')
            self.add('artifact', 'Welding Jar', actor, 'battlefield')
        if mode == 'late_creature':
            self.add('new', 'Fiery Hellhound', actor, 'hand')
        if mode == 'remove_aura':
            self.add('removal', 'Disenchant', actor, 'hand')

    def choose(self, ref):
        if ref is None:
            self.emit('choose_cancel_target', player=self.actor)
        else:
            self.emit('choose_target', ref, player=self.actor)

    def enter(self, *choices, aura_target=None, evoke=False):
        if self.csvid in LANDS:
            for ref in choices:
                self.choose(ref)
            self.emit('play_land', 'source', value=1)
        else:
            if aura_target is not None:
                self.choose(aura_target)
            if evoke or self.csvid == ACOLYTE:
                fee = '{R}' if evoke else self.oracle[self.cards['source']['name']]['mana_cost']
                self.fund(self.actor, cost_from_oracle({'mana_cost': fee}))
                if not evoke:
                    self.emit('choose_dialog', player=self.actor, value=0)
                self.emit('cast', 'source', value=1)
                self.emit('assert_stack', value=1)
                self.wallet[self.actor] = ZERO
                self.pools()
            else:
                self.pending('source')
            if self.mode in ('countered', 'evoke_countered'):
                self.emit('announce_spell', 'source')
                self.pending('counter', response=True)
                self.emit('resolve_response')
                self.move('source', 'graveyard')
                self.move('counter', 'graveyard')
                return False
            for ref in choices:
                self.choose(ref)
        self.emit('resolve')
        self.move('source', 'graveyard' if evoke else 'battlefield')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)
        return True

    def clean_size(self, ref):
        self.emit('cleanup')
        if self.cards[ref]['zone'] == 'battlefield':
            self.size(ref)

    def deny_blocks(self):
        mode = self.mode
        if self.csvid == WARLORD:
            choices = {'two': ('guard', 'second_guard'), 'one': ('guard', None),
                       'decline': (None,), 'expire': ('guard', 'second_guard'), 'countered': ()}[mode]
        else:
            choices = ({'own': 'ally', 'self': 'source'}.get(mode, 'guard'),)
        entered = self.enter(*choices)
        if entered:
            self.size('source')
            self.prop('source', 'sick', int(self.csvid == CROSSWAY))
        if mode == 'expire':
            self.round_trip_turn()
        denied = set(choices) if entered and mode != 'expire' else set()
        attacker = 'source' if entered and (self.csvid == WARLORD or mode == 'haste') else 'ally'
        candidates = ('guard', 'second_guard', 'other')
        available = [ref for ref in candidates if ref in self.cards]
        blocker = next((ref for ref in available if ref not in denied), None)
        attempts = tuple((ref, ref not in denied) for ref in available if ref in denied or ref == blocker)
        self.battle(attacker, attempts=attempts, blocker=blocker)

    def land_rider(self):
        mode = self.mode
        target = {'own': 'ally', 'foe': 'guard', 'expire': 'ally'}.get(mode)
        mod, color = {SEACLIFF: ((0, 0), 2), GROVE: ((1, 1), 3),
                      SPIRES: ((1, 1), 4), BRIDGE: ((1, 1), 5)}[self.csvid]
        self.enter(*((target,) if target else ()))
        self.prop('source', 'tapped', 1)
        self.emit('can_activate', 'source', value=0)
        if target:
            raised = tuple(n+d for n, d in zip(self.stats(target), mod))
            self.size(target, raised)
            self.size('guard' if target == 'ally' else 'ally')
        self.receipt()
        if mode == 'own':
            if self.csvid in (SEACLIFF, SPIRES):
                first = self.csvid == SPIRES
                self.battle('ally', attempts=(('guard', first),), blocker='guard' if first else None,
                            attack_stats=raised, first_strike=first)
            else:
                self.battle('ally', attack_stats=raised, vigilance=self.csvid == BRIDGE)
        elif mode == 'foe':
            self.battle('ally', attempts=(('guard', True),), blocker='guard', block_stats=raised,
                        block_first_strike=self.csvid == SPIRES)
        elif mode == 'expire':
            self.round_trip_turn()
            self.size('ally')
            self.battle('ally', attempts=(('guard', True),), blocker='guard')
        elif mode == 'mana':
            self.round_trip_turn()
            self.emit('untap_step', player=self.actor)
            self.prop('source', 'tapped', 0)
            self.emit('can_activate', 'source', value=1)
            self.emit('activate', 'source', value=1)
            self.wallet[self.actor] = tuple(int(i == color) for i in range(6))
            self.prop('source', 'tapped', 1)
            self.emit('can_activate', 'source', value=0)

    def single_boost(self):
        mode = self.mode
        target = 'guard' if mode == 'foe' else 'source' if mode == 'self' else 'ally'
        evoke = mode in ('evoke', 'evoke_countered')
        if self.cards['ally']['zone'] == 'hand':
            self.cast('ally', destination='battlefield')
            self.prop('ally', 'sick', 1)
        if not self.enter(target, evoke=evoke):
            self.size('ally')
            self.size('guard')
            return
        base = self.stats(target)
        stats = (base[0]+2, base[1]+(2 if self.csvid == BATTLE_EAGLE else 0))
        self.size(target, stats)
        if target != 'source' and not evoke:
            self.size('source')
        if mode == 'foe':
            self.size('ally')
        if self.csvid == ACOLYTE and mode in ('normal', 'evoke', 'self'):
            self.prop(target, 'sick', 0)
            self.battle(target, attack_stats=stats)
        if self.csvid == BATTLE_EAGLE and mode == 'own':
            self.battle('ally', attempts=(('guard', False),), attack_stats=stats)
        if mode in ('expire', 'flying_source'):
            self.round_trip_turn()
            self.size('ally')
            attacker = 'source' if mode == 'flying_source' else 'ally'
            self.battle(attacker, attempts=(('guard', attacker == 'ally'),),
                        blocker='guard' if attacker == 'ally' else None)
        self.clean_size(target)

    def hammerhand(self):
        mode = self.mode
        if mode == 'host_haste':
            self.cast('ally', destination='battlefield')
            self.prop('ally', 'sick', 1)
        target = 'other' if mode == 'separate_target' else 'guard'
        if not self.enter(target, aura_target='ally'):
            self.size('ally')
            self.battle('ally', attempts=(('guard', True),), blocker='guard')
            return
        enhanced = tuple(n+1 for n in self.stats('ally'))
        self.size('ally', enhanced)
        self.size('guard')
        self.prop('ally', 'sick', 0)
        if mode == 'remove_aura':
            self.cast('removal', target='source')
            self.move('source', 'graveyard')
            self.size('ally')
            enhanced = self.stats('ally')
        if mode == 'expire':
            self.round_trip_turn()
            self.size('ally', enhanced)
        denied = None if mode == 'expire' else target
        blocker = None if denied == 'guard' else 'guard'
        attempts = tuple((ref, ref == blocker) for ref in ('other', 'guard') if ref in (denied, blocker))
        self.battle('ally', attempts=attempts, blocker=blocker, attack_stats=enhanced)

    def team_rider(self):
        zeal = self.csvid == ZEALOT
        mode = self.mode
        if mode == 'fresh_haste':
            self.cast('ally', destination='battlefield')
            self.prop('ally', 'sick', 1)
        if not self.enter():
            self.size('ally')
            self.size('guard')
            self.battle('ally')
            return
        affected = ('source', 'ally', 'support') if zeal else ('ally', 'support')
        bonus = 1 if zeal else 2
        enhanced = {ref: tuple(n+bonus for n in self.stats(ref)) for ref in affected}
        for ref, stats in enhanced.items():
            self.size(ref, stats)
        self.size('guard')
        self.prop('source', 'sick', 0)
        if not zeal:
            self.size('source')
        if mode == 'late_creature':
            self.cast('new', destination='battlefield')
            self.size('new')
            self.prop('new', 'sick', 1)
            for ref, stats in enhanced.items():
                self.size(ref, stats)
        elif mode == 'expire':
            self.round_trip_turn()
            self.size('ally')
            self.battle('ally', attempts=(('guard', True),), blocker='guard')
        else:
            attacker = 'source' if mode in ('source_haste', 'source_traits') else 'ally'
            self.prop(attacker, 'sick', 0)
            self.battle(attacker, attempts=(('guard', True),), blocker='guard', trample=not zeal,
                        vigilance=not zeal, attack_stats=enhanced.get(attacker))
        self.emit('cleanup')
        for ref in affected:
            if self.cards[ref]['zone'] == 'battlefield':
                self.size(ref)

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            self.fund(who, ZERO)
        if self.csvid in (CROSSWAY, CATHAR, WARLORD):
            self.deny_blocks()
        elif self.csvid in LANDS:
            self.land_rider()
        elif self.csvid in (BATTLE_EAGLE, ACOLYTE):
            self.single_boost()
        elif self.csvid == HAMMER:
            self.hammerhand()
        else:
            self.team_rider()
        self.receipt()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid in MODELS:
        cost, kind, power, tough, rules, modes = MODELS[csvid]
        keywords = PILOT_KEYWORDS.get(csvid, ())
    else:
        cost, kind, power, tough, rules, keywords, modes = FOLLOWERS[csvid]
    fields = ('mana_cost', 'type_line', 'power', 'toughness', 'oracle_text')
    if tuple(record.get(field) for field in fields) != (cost, kind, power, tough, rules):
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
    factory = FollowOn if csvid in FOLLOWERS else Board
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = factory(oracle[name], csvid, mode, int(seat), rows, oracle)
        board.play()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=[])
        cases.append(dict(name=label(csvid, mode, int(seat)), card=name, fixture='empty_main', seed=SEED,
                          setup=board.setup, steps=board.steps, _recipe=metadata))
    return cases


