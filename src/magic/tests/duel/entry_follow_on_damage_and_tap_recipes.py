"""Paid entry triggers and real scope, damage, control and untap receipts."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step

FAMILY = 'entry_follow_on_damage_and_tap'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
ZONES = {'hand':0,'battlefield':1,'library':2,'graveyard':3,'exile':4}
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
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        if op == 'play_land':
            self.steps.append(step(op, ref=ref, value=value))
            return
        if op == 'choose_mana_source':
            self.steps.append(step(op, ref=ref))
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

    def cast(self, ref, *, target=None, destination='graveyard'):
        card = self.cards[ref]
        who = card['player']
        self.fund(who, cost_from_oracle(self.oracle[card['name']]))
        if target is not None:
            self.emit('choose_target', target, player=who)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.wallet[who] = ZERO
        self.pools()
        self.emit('resolve')
        self.move(ref, destination)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def pending(self, ref, *, response=False):
        card = self.cards[ref]
        who = card['player']
        self.fund(who,cost_from_oracle(self.oracle[card['name']]))
        self.emit('cast_response' if response else 'cast',ref,value=1)
        self.emit('assert_stack',value=2 if response else 1)
        self.wallet[who] = ZERO
        self.pools()

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
            if self.actor == 0:
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

HELLKITE, MAGE, KESSIG, SPIRES=12629,13352,12406,10835
MODELS={
    HELLKITE: ('{3}{R}{R}','Creature — Dragon','5','5',
               'Flying\nHaste (This creature can attack and {T} as soon as it comes under your control.)\n'
               'When this creature enters, it deals 1 damage to each creature with flying your opponents control. Tap those creatures.',
               ('scope','lethal','haste','countered')),
    MAGE: ('{U}{U}','Creature — Merfolk Wizard','2','2',
           "When this creature enters, tap target red or green creature an opponent controls. That creature doesn't untap during its controller's untap step for as long as you control this creature.",
           ('red','green','wrong_color','own_only','leaves','loses_control','countered')),
    KESSIG: ('{2}{R}','Creature — Human Warrior','3','1',
             'When this creature enters, it deals damage to target player or planeswalker equal to the number of Humans you control.',
             ('self_count','three_humans','self_target','support_lost','planeswalker','countered')),
    SPIRES: ('','Land',None,None,
             "This land enters tapped.\nWhen this land enters, target creature can't block this turn.\n{T}: Add {R}.",
             ('forbid','other_blocks','own_target','no_creatures','mana')),
}
WITNESSES={
    'Goblin Tinkerer':('{1}{R}','{R}, {T}: Destroy target artifact. That artifact deals damage equal to its mana value to this creature.','1','2'),
    'Prodigal Sorcerer':('{2}{U}','{T}: This creature deals 1 damage to any target.','1','1'),
    'Royal Assassin':('{1}{B}{B}','{T}: Destroy target tapped creature.','1','1'),
    'Serra Angel':('{3}{W}{W}',"Flying\nVigilance (Attacking doesn't cause this creature to tap.)",'4','4'),
    "Healer's Hawk":('{W}','Flying\nLifelink (Damage dealt by this creature also causes you to gain that much life.)','1','1'),
    'Elvish Mystic':('{G}','{T}: Add {G}.','1','1'),
    'Fiery Hellhound':('{1}{R}{R}','{R}: This creature gets +1/+0 until end of turn.','2','2'),
    'Essence Scatter':('{1}{U}','Counter target creature spell.'),
    'Control Magic':('{2}{U}{U}','Enchant creature\nYou control enchanted creature.'),
    'Unsummon':('{U}',"Return target creature to its owner's hand."),
    'Welding Jar':('{0}','Sacrifice this artifact: Regenerate target artifact.'),
}
SEED=3212


def label(csvid,mode,actor):
    return f'EntryFollowOnDamageAndTapRecipe_{csvid}_{mode}_{actor}'


class Board(CombatBoard):
    def __init__(self,record,csvid,mode,actor,rows,oracle):
        self.csvid,self.mode,self.actor,self.foe=csvid,mode,actor,1-actor
        self.rows,self.oracle=rows,oracle
        self.setup,self.steps,self.cards=[],[],{}
        self.wallet,self.life=[ZERO,ZERO],[20,20]
        self.add('source',record['name'],actor,'hand')
        if mode=='countered':
            self.add('counter','Essence Scatter',self.foe,'hand')
        if csvid==HELLKITE:
            self.add('own_flyer','Serra Angel',actor,'battlefield')
            self.add('flyer','Serra Angel',self.foe,'battlefield')
            self.add('ground','Goblin Tinkerer',self.foe,'battlefield')
            self.add('relic','Welding Jar',self.foe,'battlefield')
            if mode=='lethal':
                self.add('small_flyer',"Healer's Hawk",self.foe,'battlefield')
        if csvid==MAGE:
            name='Elvish Mystic' if mode=='green' else 'Prodigal Sorcerer' if mode in ('wrong_color','own_only') else 'Fiery Hellhound'
            self.add('guard',name,self.foe,'battlefield')
            self.add('own_red','Goblin Tinkerer',actor,'battlefield')
            self.add('wrong_color','Royal Assassin',self.foe,'battlefield')
            if mode=='loses_control':
                self.add('control','Control Magic',self.foe,'hand')
        if csvid==KESSIG:
            self.add('nonhuman','Goblin Tinkerer',actor,'battlefield')
            self.add('foe_human','Prodigal Sorcerer',self.foe,'battlefield')
            if mode not in ('self_count','planeswalker'):
                self.add('human_0','Prodigal Sorcerer',actor,'battlefield')
                self.add('human_1','Royal Assassin',actor,'battlefield')
            if mode=='support_lost':
                self.add('bounce','Unsummon',self.foe,'hand')
            if mode=='planeswalker':
                self.add('walker','Jace Beleren',self.foe,'hand')
        if csvid==SPIRES and mode not in ('no_creatures','mana'):
            self.add('attacker','Goblin Tinkerer',actor,'battlefield')
            self.add('reserve','Fiery Hellhound',actor,'battlefield')
            self.add('guard','Goblin Tinkerer',self.foe,'battlefield')
            if mode=='other_blocks':
                self.add('other','Prodigal Sorcerer',self.foe,'battlefield')

    def pending_response(self,ref,target):
        self.fund(self.foe,cost_from_oracle(self.oracle[self.cards[ref]['name']]))
        self.emit('choose_target',target,player=self.foe)
        self.emit('cast_response',ref,value=1)
        self.emit('assert_stack',value=2)
        self.wallet[self.foe]=ZERO
        self.pools()

    def enter(self):
        self.pending('source')
        if self.mode=='countered':
            self.emit('announce_spell','source')
            self.pending('counter',response=True)
            self.emit('resolve_response')
            self.move('source','graveyard')
            self.move('counter','graveyard')
            return False
        response=self.mode=='support_lost'
        if response:
            self.emit('announce_spell','source')
            self.pending_response('bounce','human_0')
        if self.csvid==MAGE and self.mode not in ('wrong_color','own_only'):
            self.emit('choose_target','guard',player=self.actor)
        if self.csvid==KESSIG:
            if self.mode=='planeswalker':
                self.emit('choose_target','walker',player=self.actor)
            else:
                self.emit('choose_player',player=self.actor,value=self.actor if self.mode=='self_target' else self.foe)
        self.emit('resolve_response' if response else 'resolve')
        if response:
            self.move('bounce','graveyard')
            self.move('human_0','hand')
        self.move('source','battlefield')
        self.emit('assert_unused_choices',value=0)
        self.size('source')
        return True

    def hellkite(self,entered):
        for ref in ('own_flyer','flyer','ground'):
            self.prop(ref,'damage',int(entered and ref=='flyer'))
            self.prop(ref,'tapped',int(entered and ref=='flyer'))
        self.prop('relic','tapped',0)
        if entered and self.mode=='lethal':
            self.move('small_flyer','graveyard')
        self.receipt()
        if entered and self.mode=='haste':
            self.prop('source','sick',0)
            self.begin('source')
            self.combat('source',attempts=(('flyer',False),('ground',False)))

    def tidebinder(self,entered):
        locked=entered and self.mode not in ('wrong_color','own_only')
        self.prop('guard','tapped',int(locked))
        self.prop('own_red','tapped',0)
        self.prop('wrong_color','tapped',0)
        self.receipt()
        if not locked:
            return
        if self.mode=='loses_control':
            self.emit('advance_turn')
            self.cast('control',target='source',destination='battlefield')
            self.cards['source']['player']=self.foe
            self.emit('bind_permanent','source',player=self.foe,index=-1,value=MAGE)
            self.size('source')
            self.receipt()
            self.round_trip_turn()
            self.emit('untap_step',player=self.foe)
            self.prop('guard','tapped',0)
            return
        self.emit('advance_turn')
        self.emit('untap_step',player=self.foe)
        self.prop('guard','tapped',1)
        self.prop('wrong_color','tapped',0)
        self.round_trip_turn()
        if self.mode=='leaves':
            self.emit('destroy','source')
            self.move('source','graveyard')
        self.emit('untap_step',player=self.foe)
        self.prop('guard','tapped',int(self.mode!='leaves'))

    def humans(self):
        amount=0
        for card in self.cards.values():
            if card['player']==self.actor and card['zone']=='battlefield':
                _,_,subtypes=self.oracle[card['name']]['type_line'].partition(' — ')
                amount+=int('Human' in subtypes.split())
        if self.mode=='planeswalker':
            self.emit('assert_counter','walker',prop='loyalty',value=3-amount)
        else:
            recipient=self.actor if self.mode=='self_target' else self.foe
            self.life[recipient]-=amount

    def spires(self):
        selected=None
        if self.mode not in ('no_creatures','mana'):
            selected='attacker' if self.mode=='own_target' else 'guard'
            self.emit('choose_target',selected,player=self.actor)
        self.emit('play_land','source',value=1)
        self.move('source','battlefield')
        self.emit('resolve')
        self.emit('assert_unused_choices',value=0)
        self.prop('source','tapped',1)
        self.emit('can_activate','source',value=0)
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
        if selected is None:
            return
        self.begin('attacker')
        legal=self.mode=='own_target'
        attempts=[('guard',legal)]
        if self.mode=='other_blocks':
            attempts.append(('other',True))
        blocker='other' if self.mode=='other_blocks' else 'guard' if legal else None
        self.combat('attacker',attempts=attempts,blocked=blocker)
        if self.mode=='forbid':
            self.round_trip_turn()
            self.begin('reserve')
            self.combat('reserve',attempts=(('guard',True),),blocked='guard')

    def play(self):
        self.emit('turn',player=self.foe if self.mode=='planeswalker' else self.actor)
        self.emit('present_ai_dialogs',value=1)
        for who in (0,1):
            self.fund(who,ZERO)
        if self.mode=='planeswalker':
            self.cast('walker',destination='battlefield')
            self.emit('assert_counter','walker',prop='loyalty',value=3)
            self.emit('advance_turn')
        if self.csvid==SPIRES:
            self.spires()
        else:
            entered=self.enter()
            if self.csvid==HELLKITE:
                self.hellkite(entered)
            elif self.csvid==MAGE:
                self.tidebinder(entered)
            elif entered:
                self.humans()
        self.receipt()


FOUNDRY, SPAWN, STONEHORN, CASCADE, FOG, TRIAL = 12947, 13980, 11851, 15442, 16100, 17054
CHAIN, VOLLEY, HYDRA, FIRE, MARSH, AEGIS = 18417, 19049, 19130, 19699, 10632, 14922
FOLLOWERS = {
    12947: ('{4}{R}{W}', 'Creature — Elemental Soldier', '4', '4', 'When this creature enters, it deals damage to any target equal to the number of creatures you control.\n{R}: This creature gets +1/+0 until end of turn.\n{W}: This creature gets +0/+1 until end of turn.', (), ('self_count', 'creature_count', 'red_pump', 'white_pump', 'countered')),
    13980: ('{5}{R}{R}', 'Creature — Dragon', '5', '5', 'Flying\nWhen this creature enters, it deals damage to any target equal to the number of Mountains you control.', ('Flying',), ('mountains', 'zero', 'flying', 'lethal')),
    11851: ('{3}{W}', 'Creature — Rhino Soldier', '1', '4', 'When this creature enters, target opponent skips their next combat phase.', (), ('lock', 'source_removed', 'countered')),
    15442: ('', 'Land', None, None, "This land enters tapped.\nWhen this land enters, target creature an opponent controls doesn't untap during its controller's next untap step.\n{T}: Add {U}.", (), ('skip', 'does_not_tap', 'source_lost', 'mana')),
    16100: ('{1}{U}', 'Creature — Spirit', '1', '3', "Skulk (This creature can't be blocked by creatures with greater power.)\nWhen this creature enters, target creature an opponent controls doesn't untap during its controller's next untap step.", ('Skulk',), ('skip', 'does_not_tap', 'skulk', 'equal_power')),
    17054: ('{2}{R}', 'Enchantment', None, None, "When this enchantment enters, it deals 3 damage to any target.\nWhen a Cartouche you control enters, return this enchantment to its owner's hand.", (), ('player', 'lethal', 'own_cartouche', 'foe_cartouche')),
    18417: ('{R}{R}{R}', 'Creature — Goblin Warrior', '3', '3', 'First strike\nWhen this creature enters, it deals 1 damage to each opponent and each creature and planeswalker they control.', ('First strike',), ('scope', 'planeswalker', 'first_strike', 'countered')),
    19049: ('{3}{R}', 'Creature — Goblin Warrior', '4', '2', 'When this creature enters, it deals damage to target creature an opponent controls equal to the number of Goblins you control.', (), ('self_count', 'three_goblins', 'lethal', 'no_legal')),
    19130: ('{5}{G}', 'Creature — Lizard Hydra', '4', '4', 'Reach\nWhen this creature enters, destroy all creatures with flying. Put a +1/+1 counter on this creature for each creature destroyed this way.', ('Reach',), ('scope', 'indestructible', 'none', 'reach')),
    19699: ('{B}{B}{R}{R}', 'Creature — Human Rogue', '4', '3', 'When this creature enters, it deals 2 damage to target opponent and 2 damage to up to one target creature or planeswalker.', (), ('decline', 'lethal', 'planeswalker', 'own_target')),
    10632: ('', 'Land', None, None, 'This land enters tapped.\nWhen this land enters, target player loses 1 life.\n{T}: Add {B}.', (), ('foe', 'self', 'mana')),
    14922: ('{W}', 'Enchantment — Aura', None, None, 'Enchant creature\nWhen this Aura enters, tap target creature an opponent controls.\nEnchanted creature gets +1/+3.', ('Enchant',), ('scope', 'attach_enemy', 'no_enemy', 'detach')),
}
NEW_WITNESSES={
    'Mountain': ('','({T}: Add {R}.)'),
    'Forest': ('','({T}: Add {G}.)'),
    'Taiga': ('','({T}: Add {R} or {G}.)'),
    'Gladecover Scout': ('{G}',"Hexproof (This creature can't be the target of spells or abilities your opponents control.)",'1','1'),
    'Darksteel Gargoyle': ('{7}',"Flying\nIndestructible (Damage and effects that say \"destroy\" don't destroy this creature.)",'3','3'),
    'Cartouche of Knowledge': ('{1}{U}','Enchant creature you control\nWhen this Aura enters, draw a card.\nEnchanted creature gets +1/+1 and has flying.'),
}
LANDS={CASCADE,MARSH}
PILOT_KEYWORDS={HELLKITE:('Flying','Haste')}


class FollowOn(Board):
    def __init__(self,record,csvid,mode,actor,rows,oracle):
        self.csvid,self.mode,self.actor,self.foe=csvid,mode,actor,1-actor
        self.rows,self.oracle=rows,oracle
        self.setup,self.steps,self.cards=[],[],{}
        self.wallet,self.life=[ZERO,ZERO],[20,20]
        self.add('source',record['name'],actor,'hand')
        self.declare_witnesses()

    def declare_witnesses(self):
        actor,foe,mode,source=self.actor,self.foe,self.mode,self.csvid
        if mode=='countered':
            self.add('counter','Essence Scatter',foe,'hand')
        if source==FOUNDRY and mode=='creature_count':
            self.add('ally','Goblin Tinkerer',actor,'battlefield')
            self.add('ally_2','Prodigal Sorcerer',actor,'battlefield')
            self.add('guard','Serra Angel',foe,'battlefield')
        if source==SPAWN:
            self.add('wrong_land','Forest',actor,'battlefield')
            self.add('opposing_mountain','Mountain',foe,'battlefield')
            if mode!='zero':
                self.add('mountain','Mountain',actor,'battlefield')
                self.add('nonbasic_mountain','Taiga',actor,'battlefield')
            if mode=='flying':
                self.add('guard','Fiery Hellhound',foe,'battlefield')
            if mode=='lethal':
                self.add('guard','Goblin Tinkerer',foe,'battlefield')
        if source==STONEHORN:
            self.add('guard','Goblin Tinkerer',foe,'battlefield')
            self.add('ally','Fiery Hellhound',actor,'battlefield')
        if source in (CASCADE,FOG):
            target='Prodigal Sorcerer' if mode=='equal_power' else 'Fiery Hellhound'
            self.add('guard',target,foe,'battlefield',tapped=mode in ('skip','source_lost','mana'))
            self.add('reserve','Goblin Tinkerer',foe,'battlefield',tapped=mode in ('skip','source_lost','mana'))
        if source==TRIAL:
            self.add('guard','Goblin Tinkerer',foe,'battlefield')
            if mode.endswith('cartouche'):
                drawer=actor if mode=='own_cartouche' else foe
                self.add('cartouche','Cartouche of Knowledge',drawer,'hand')
                self.add('drawn','Forest',drawer,'library')
                self.add('host','Fiery Hellhound',drawer,'battlefield')
        if source==CHAIN:
            self.add('ally','Prodigal Sorcerer',actor,'battlefield')
            self.add('guard','Goblin Tinkerer',foe,'battlefield')
            if mode=='scope':
                self.add('small','Prodigal Sorcerer',foe,'battlefield')
        if source==VOLLEY:
            if mode!='no_legal':
                self.add('foe_goblin','Goblin Tinkerer',foe,'battlefield')
            self.add('non_goblin','Prodigal Sorcerer',actor,'battlefield')
            if mode in ('three_goblins','lethal'):
                self.add('ally_0','Goblin Tinkerer',actor,'battlefield')
                self.add('ally_1','Goblin Tinkerer',actor,'battlefield')
            self.add('guard','Gladecover Scout' if mode=='no_legal' else 'Goblin Tinkerer' if mode=='lethal' else 'Serra Angel',foe,'battlefield')
        if source==HYDRA:
            self.add('ground','Goblin Tinkerer',foe,'battlefield')
            if mode in ('scope','indestructible'):
                self.add('own_flyer','Serra Angel',actor,'battlefield')
                self.add('foe_flyer','Serra Angel',foe,'battlefield')
            if mode=='indestructible':
                self.add('gargoyle','Darksteel Gargoyle',foe,'battlefield')
            if mode=='reach':
                self.add('wing','Serra Angel',foe,'hand')
        if source==FIRE:
            if mode=='lethal':
                self.add('guard','Prodigal Sorcerer',foe,'battlefield')
            if mode=='own_target':
                self.add('ally','Goblin Tinkerer',actor,'battlefield')
        if source==AEGIS:
            host_controller=foe if mode=='attach_enemy' else actor
            self.add('host','Fiery Hellhound',host_controller,'battlefield')
            if mode!='no_enemy':
                self.add('guard','Goblin Tinkerer',foe,'battlefield')
            if mode=='detach':
                self.add('bounce','Unsummon',foe,'hand')
        if mode=='planeswalker':
            self.add('walker','Jace Beleren',foe,'hand')

    def pay_anchor(self,ref='source',target=None):
        if target is None:
            self.pending(ref)
            return
        spell=self.cards[ref]
        who=spell['player']
        self.fund(who,cost_from_oracle(self.oracle[spell['name']]))
        self.emit('choose_target',target,player=who)
        self.emit('cast',ref,value=1)
        self.emit('assert_stack',value=1)
        self.wallet[who]=ZERO
        self.pools()

    def target_inputs(self):
        actor,foe,mode,source=self.actor,self.foe,self.mode,self.csvid

        def pick(ref):
            self.emit('choose_target',ref,player=actor)

        def target_player(player):
            self.emit('choose_player',player=actor,value=player)

        if source in (FOUNDRY,SPAWN):
            named=(source==FOUNDRY and mode=='creature_count') or (source==SPAWN and mode=='lethal')
            pick('guard') if named else target_player(foe)
        elif source in (CASCADE,FOG):
            pick('guard')
        elif source==TRIAL:
            pick('guard') if mode=='lethal' else target_player(foe)
        elif source==VOLLEY and mode!='no_legal':
            pick('guard')
        elif source==FIRE:
            target_player(foe)
            if mode=='decline':
                self.emit('choose_cancel_target',player=actor)
            else:
                pick({'lethal':'guard','planeswalker':'walker','own_target':'ally'}[mode])
        elif source==MARSH:
            target_player(actor if mode=='self' else foe)
        elif source==AEGIS and mode!='no_enemy':
            pick('guard')

    def entry(self):
        if self.csvid in LANDS:
            self.target_inputs()
            self.emit('play_land','source',value=1)
        else:
            self.pay_anchor(target='host' if self.csvid==AEGIS else None)
            if self.mode=='countered':
                self.emit('announce_spell','source')
                self.pending('counter',response=True)
                self.emit('resolve_response')
                self.move('source','graveyard')
                self.move('counter','graveyard')
                self.receipt()
                return False
            self.target_inputs()
        self.emit('resolve')
        self.move('source','battlefield')
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_stack',value=0)
        return True

    def subtype_count(self,subtype):
        return sum(card['zone']=='battlefield' and card['player']==self.actor and subtype in
                   self.oracle[card['name']]['type_line'].partition(' — ')[2].split() for card in self.cards.values())

    def creature_damage(self,ref,amount):
        if amount>=self.stats(ref)[1]:
            self.move(ref,'graveyard')
        else:
            self.size(ref)
            self.prop(ref,'damage',amount)

    def foundry(self):
        amount=sum(c['zone']=='battlefield' and c['player']==self.actor and
                   'Creature' in self.oracle[c['name']]['type_line'].partition(' — ')[0].split()
                   for c in self.cards.values())
        if self.mode=='creature_count':
            self.creature_damage('guard',amount)
        else:
            self.life[self.foe]-=amount
        self.size('source')
        self.receipt()
        if self.mode in ('red_pump','white_pump'):
            red=self.mode=='red_pump'
            fee=(0,0,0,0,int(red),int(not red))
            self.fund(self.actor,fee)
            self.emit('can_activate','source',value=1)
            self.emit('activate','source',value=1)
            self.emit('assert_stack',value=1)
            self.wallet[self.actor]=ZERO
            self.emit('resolve')
            self.emit('assert_unused_choices',value=0)
            self.size('source',(5,4) if red else (4,5))
            self.receipt()
            self.emit('cleanup')
            self.size('source')

    def spawn(self):
        amount=self.subtype_count('Mountain')
        if self.mode=='lethal':
            self.creature_damage('guard',amount)
        else:
            self.life[self.foe]-=amount
        self.size('source')
        self.receipt()
        if self.mode=='flying':
            self.round_trip_turn()
            self.begin('source')
            self.combat('source',attempts=(('guard',False),))

    def stonehorn(self,entered):
        self.emit('begin_attackers')
        self.emit('attack','ally',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[self.foe]-=2
        if self.mode=='source_removed':
            self.emit('destroy','source')
            self.move('source','graveyard')
        self.emit('advance_turn')
        self.emit('begin_attackers')
        self.emit('attack','guard',value=int(not entered))
        self.prop('guard','attacking',int(not entered))
        self.emit('finish_attackers')
        if not entered:
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.life[self.actor]-=1

    def mana_land(self,color):
        self.prop('source','tapped',1)
        self.emit('can_activate','source',value=0)
        self.receipt()
        self.round_trip_turn()
        self.emit('untap_step',player=self.actor)
        self.prop('source','tapped',0)
        self.emit('can_activate','source',value=1)
        self.emit('activate','source',value=1)
        pool=[0]*6
        pool[color]=1
        self.wallet[self.actor]=tuple(pool)
        self.prop('source','tapped',1)
        self.emit('can_activate','source',value=0)

    def tap_lock(self):
        land=self.csvid==CASCADE
        if land:
            self.prop('source','tapped',1)
            self.emit('can_activate','source',value=0)
        else:
            self.size('source')
        sleep=self.mode in ('skip','source_lost','mana')
        self.prop('guard','tapped',int(sleep))
        self.prop('reserve','tapped',int(sleep))
        self.receipt()
        if self.mode=='mana':
            self.mana_land(2)
            return
        if self.mode=='source_lost':
            self.emit('destroy','source')
            self.move('source','graveyard')
        if self.mode in ('skip','source_lost','does_not_tap'):
            self.emit('advance_turn')
            self.emit('untap_step',player=self.foe)
            self.prop('guard','tapped',int(sleep))
            self.prop('reserve','tapped',0)
            self.round_trip_turn()
            self.emit('untap_step',player=self.foe)
            self.prop('guard','tapped',0)
        else:
            self.round_trip_turn()
            self.begin('source')
            allowed=self.mode=='equal_power'
            self.combat('source',attempts=(('guard',allowed),),blocked='guard' if allowed else None)

    def trial(self):
        if self.mode=='lethal':
            self.move('guard','graveyard')
        else:
            self.life[self.foe]-=3
        self.receipt()
        if not self.mode.endswith('cartouche'):
            return
        own=self.mode=='own_cartouche'
        if not own:
            self.emit('advance_turn')
        self.cast('cartouche',target='host',destination='battlefield')
        self.move('drawn','hand')
        self.size('host',(3,3))
        if own:
            self.move('source','hand')
        self.receipt()
        if own:
            self.emit('bind_hand','source',player=self.actor,index=-1,value=TRIAL)
            self.entry()
            self.life[self.foe]-=3

    def chain(self):
        self.life[self.foe]-=1
        self.prop('ally','damage',0)
        self.prop('source','damage',0)
        self.creature_damage('guard',1)
        if self.mode=='scope':
            self.move('small','graveyard')
        if self.mode=='planeswalker':
            self.emit('assert_counter','walker',prop='loyalty',value=2)
        self.receipt()
        if self.mode=='first_strike':
            self.round_trip_turn()
            self.begin('source')
            self.emit('begin_blockers')
            self.emit('block','guard','source',value=1)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.move('guard','graveyard')
            self.prop('source','damage',0)

    def volley(self):
        self.size('source')
        if self.mode=='no_legal':
            self.prop('guard','damage',0)
        else:
            self.creature_damage('guard',self.subtype_count('Goblin'))
            self.prop('foe_goblin','damage',0)
        self.prop('non_goblin','damage',0)

    def hydra(self):
        count=0
        if self.mode in ('scope','indestructible'):
            for ref in ('own_flyer','foe_flyer'):
                self.move(ref,'graveyard')
                count+=1
        self.emit('assert_counter','source',prop='plus1',value=count)
        self.size('source',(4+count,4+count))
        self.size('ground')
        self.receipt()
        if self.mode=='reach':
            self.emit('advance_turn')
            self.cast('wing',destination='battlefield')
            self.round_trip_turn()
            self.emit('begin_attackers')
            self.emit('attack','wing',value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block','source','wing',value=1)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.move('source','graveyard')
            self.move('wing','graveyard')

    def fire(self):
        self.life[self.foe]-=2
        if self.mode=='planeswalker':
            self.emit('assert_counter','walker',prop='loyalty',value=1)
        if self.mode=='lethal':
            self.move('guard','graveyard')
        if self.mode=='own_target':
            self.move('ally','graveyard')

    def marsh(self):
        recipient=self.actor if self.mode=='self' else self.foe
        self.life[recipient]-=1
        self.prop('source','tapped',1)
        self.emit('can_activate','source',value=0)
        self.receipt()
        if self.mode=='mana':
            self.mana_land(1)

    def aegis(self):
        self.size('host',(3,5))
        self.prop('host','tapped',0)
        if self.mode!='no_enemy':
            self.prop('guard','tapped',1)
        self.receipt()
        if self.mode=='detach':
            self.cast('bounce',target='host')
            self.move('host','hand')
            self.move('source','graveyard')

    def play(self):
        self.emit('turn',player=self.foe if self.mode=='planeswalker' else self.actor)
        self.emit('present_ai_dialogs',value=1)
        for who in (0,1):
            self.fund(who,ZERO)
        if self.mode=='planeswalker':
            self.cast('walker',destination='battlefield')
            self.emit('assert_counter','walker',prop='loyalty',value=3)
            self.emit('advance_turn')
        entered=self.entry()
        actions={FOUNDRY:self.foundry,SPAWN:self.spawn,CASCADE:self.tap_lock,FOG:self.tap_lock,
                 TRIAL:self.trial,CHAIN:self.chain,VOLLEY:self.volley,HYDRA:self.hydra,FIRE:self.fire,
                 MARSH:self.marsh,AEGIS:self.aegis}
        if self.csvid==STONEHORN:
            self.stonehorn(entered)
        elif entered:
            actions[self.csvid]()
        self.receipt()


@lru_cache
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid in MODELS:
        cost,kind,power,tough,rules,modes=MODELS[csvid]
        keywords=PILOT_KEYWORDS.get(csvid,())
    else:
        cost,kind,power,tough,rules,keywords,modes=FOLLOWERS[csvid]
    fields=('mana_cost','type_line','power','toughness','oracle_text')
    if tuple(record.get(field) for field in fields)!=(cost,kind,power,tough,rules):
        raise ValueError('Exact cost, type, stats and all entry clauses required')
    if record.get('layout')!='normal' or record.get('card_faces') or set(record.get('keywords',()))!=set(keywords):
        raise ValueError('Complete pinned normal entry source required')
    return dict(csvid=csvid,cost=cost,choices=[f'{mode}_{seat}' for mode in modes for seat in (0,1)])


def variants(model):
    return list(model['choices'])


def validate_witnesses(oracle,rows):
    pins=profiles()
    expects=WITNESSES | NEW_WITNESSES
    if (set(pins)!={'sources','witnesses'} or set(pins['witnesses'])!=expects.keys()|{'Jace Beleren'}
            or {pin['registry']['csvid'] for pin in pins['sources'].values()}!=MODELS.keys()|FOLLOWERS.keys()):
        raise ValueError('Exact source and witness scope required')
    for group in pins.values():
        for name,pin in group.items():
            record,row=oracle.get(name,{}),rows.get(name,{})
            if (not exact(pin,dict(oracle=record,registry=row)) or record.get('layout')!='normal'
                    or row.get('kind')!='card' or row.get('handler') in (None,'0','NULL','card_dummy')):
                raise ValueError(f'Changed implemented entry source or witness: {name}')
            if name in expects:
                fields=('mana_cost','oracle_text','power','toughness')[:len(expects[name])]
                if tuple(record[k] for k in fields)!=expects[name]:
                    raise ValueError(f'Changed witness rules: {name}')


def spec(declaration,record):
    pin=profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record,pin['oracle']):
        raise ValueError('Complete pinned entry primary required')
    model=classify(record,pin['registry']['csvid'])
    chosen=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not exact(declaration.get('params'),model) or not isinstance(chosen,list) or not chosen
            or any(type(v) is not str for v in chosen) or len(chosen)!=len(set(chosen)) or set(chosen)-set(variants(model))):
        raise ValueError('Exact scoped recipe and distinct variants required')
    return dict(model,family=FAMILY)


def expand(name,csvid,declaration,model,selected,rows,oracle):
    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or model['csvid']!=csvid or set(selected)-set(variants(model)):
        raise ValueError('Exact primary and bounded variants required')
    gaps={STONEHORN:['The bounded combat driver exercises the actual attack veto. Automatic skipping of the whole phase and consumption before a later combat are unobserved.']}
    factory=FollowOn if csvid in FOLLOWERS else Board
    cases=[]
    for variant in selected:
        mode,seat=variant.rsplit('_',1)
        board=factory(oracle[name],csvid,mode,int(seat),rows,oracle)
        board.play()
        metadata=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                      oracle_text=declaration['oracle_text'],covered_clauses=declaration['oracle_text'].splitlines(),
                      untested_components=gaps.get(csvid,[]))
        cases.append(dict(name=label(csvid,mode,int(seat)),card=name,fixture='empty_main',seed=SEED,
                          setup=board.setup,steps=board.steps,_recipe=metadata))
    return cases
