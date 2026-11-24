"""Freeze real entry-trigger control diagnostics for four pinned cards."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'entry_rider_control'
PROFILE = Path(__file__).with_name('data') / 'entry_rider_control_wave8_profiles.json'

DRAKE, THIEF, SOWER, CONSCRIPTS = 3777,11827,9264,12514
PREFIX, SEED = 'EntryRiderControlRecipe',3308
MODES = {
    DRAKE: ('exchange','decline','protected','noncreature'),
    THIEF: ('persist','dies','stolen_source','nonartifact'),
    SOWER: ('flying','dies','stolen_source','protected'),
    CONSCRIPTS: ('creature','land','fresh_own','source_dies'),
}
HAND, BATTLE, LIBRARY, GRAVE, EXILE = range(5)
ZERO = (0,)*6
BEAR, GIANT, LAND = 'Grizzly Bears','Hill Giant','Forest'
ARTIFACT, PROTECTED = 'Darksteel Relic','Silhana Ledgewalker'
MURDER, BOUNCE, STEAL, GROWTH = 'Murder','Unsummon','Control Magic','Giant Growth'
WITNESSES = {BEAR,GIANT,LAND,ARTIFACT,PROTECTED,MURDER,BOUNCE,STEAL,GROWTH}


def label(csvid,mode,actor):
    return f'{PREFIX}_{csvid}_{mode}_{actor}'


class Board:
    def __init__(self, row, mode, actor, rows, oracle):
        self.id,self.name = row['csvid'],row['name']
        self.mode,self.actor,self.foe,self.active = mode,actor,1-actor,actor
        self.rows,self.oracle = rows,oracle
        self.setup,self.steps,self.cards,self.owners = [],[],{},{}
        self.pool,self.life = [ZERO,ZERO],[20,20]
        self.add('source',self.name,actor,HAND)
        self.witnesses()

    def add(self, ref, name, owner, zone=BATTLE, *, tapped=False):
        assert ref not in self.cards
        self.setup.append((ref,owner,zone,self.rows[name]['csvid'],int(tapped)))
        self.cards[ref] = [name,owner,zone]
        self.owners[ref] = owner

    def witnesses(self):
        p,q,m = self.actor,self.foe,self.mode
        if self.id == DRAKE:
            target = {'protected':PROTECTED,'noncreature':ARTIFACT}.get(m,GIANT)
            self.add('victim',target,q)
            self.add('own_creature',BEAR,p)
            if m=='exchange':
                self.add('remove',BOUNCE,p,HAND)
        elif self.id == THIEF:
            victim = BEAR if m=='nonartifact' else ARTIFACT
            self.add('victim',victim,q,tapped=(m=='persist'))
        elif self.id == SOWER:
            self.add('victim',PROTECTED if m=='protected' else GIANT,q)
            self.add('guard',BEAR,self.actor if m=='protected' else q)
        elif self.id == CONSCRIPTS:
            if m=='fresh_own':
                self.add('victim',BEAR,p,HAND)
            else:
                self.add('victim',LAND if m=='land' else BEAR,q,tapped=True)
            if m=='land':
                self.add('spend',GROWTH,p,HAND)
        if m in ('dies','source_dies'):
            self.add('remove',MURDER,q,HAND)
        elif m=='stolen_source':
            self.add('steal',STEAL,q,HAND)

    def emit(self, op, ref='', other='', *, prop='', player=0, value=0, zone=HAND, index=0, color=0):
        self.steps.append((op,ref,other,prop,player,value,zone,index,color))

    def see(self, ref, prop, value):
        self.emit('assert_card',ref,prop=prop,value=value)

    def set_pool(self, player, pool):
        self.pool[player] = tuple(pool)
        for color,count in enumerate(pool):
            self.emit('set_mana',player=player,color=color,value=count)
        self.emit('refresh')

    def mana(self, player):
        for color,count in enumerate(self.pool[player]):
            self.emit('assert_mana',player=player,color=color,value=count)

    def state(self):
        self.emit('assert_stack')
        for player in (0,1):
            self.mana(player)
            self.emit('assert_life',player=player,value=self.life[player])
            for zone in (HAND,BATTLE,GRAVE):
                counts = Counter(name for name,p,z in self.cards.values() if (p,z)==(player,zone))
                self.emit('assert_zone',player=player,zone=zone,value=counts.total())
                for name,count in sorted(counts.items()):
                    self.emit('assert_zone_multiplicity',player=player,zone=zone,index=self.rows[name]['csvid'],value=count)
        for ref,(_,_,where) in self.cards.items():
            self.see(ref,'in_play',int(where==BATTLE))

    def stats(self, ref):
        record = self.oracle[self.cards[ref][0]]
        for prop in ('power','toughness'):
            self.see(ref,prop,int(record[prop]))

    def bind(self, ref, actor):
        name,_,zone = self.cards[ref]
        assert zone==BATTLE
        self.cards[ref][1] = actor
        self.emit('bind_permanent_copy',ref,player=actor,value=self.rows[name]['csvid'],index=0)

    def put_in_owned_zone(self, ref, zone):
        self.cards[ref][1:] = [self.owners[ref],zone]

    def settle(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices')
        self.emit('assert_stack')

    def cast(self, ref, *, target=None, entry=None, decline=False, actual_pool=False, choices=()):
        name,p,zone = self.cards[ref]
        assert zone==HAND
        cost = tuple(cost_from_oracle(self.oracle[name]))
        if actual_pool:
            assert self.pool[p]==cost
        else:
            self.set_pool(p,cost)
        if target is not None:
            self.emit('choose_target',target,player=p)
        self.emit('cast',ref,value=1)
        self.emit('assert_unused_choices')
        self.emit('assert_stack',value=1)
        self.pool[p] = ZERO
        self.mana(p)
        for op,choice,seat,value in choices:
            self.emit(op,choice,player=seat,value=value)
        if entry is not None:
            self.emit('choose_optional_target' if decline else 'choose_target',entry,player=p)
        self.settle()
        self.cards[ref][2] = GRAVE if self.oracle[name]['type_line']=='Instant' else BATTLE
        if decline:
            self.emit('assert_forced_target_choices',value=0)

    def visit(self, actor):
        assert actor==1-self.active and self.pool==[ZERO,ZERO]
        self.emit('advance_turn')
        self.emit('untap_step',player=actor)
        self.emit('turn',player=actor)
        self.active = actor

    def combat(self, refs, *, impossible_block=None, blocked_by=None, hit):
        self.emit('begin_attackers')
        for ref in refs:
            self.emit('attack',ref,value=1)
            self.see(ref,'attacking',1)
            self.see(ref,'tapped',1)
        self.emit('finish_attackers')
        self.settle()
        self.emit('begin_blockers')
        if impossible_block:
            self.emit('block',impossible_block,refs[0],value=0)
        if blocked_by:
            self.emit('block',blocked_by,refs[0],value=1)
        self.emit('advance_combat')
        self.settle()
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')
        self.life[1-self.active] -= hit
        self.state()

    def drake(self):
        select = 'victim' if self.mode in ('exchange','decline') else None
        self.cast('source',entry=select,decline=self.mode=='decline')
        if self.mode!='exchange':
            self.put_in_owned_zone('source',GRAVE)
            self.state()
            return
        self.bind('source',self.foe)
        self.bind('victim',self.actor)
        self.stats('source')
        self.stats('victim')
        self.state()
        self.visit(self.foe)
        self.combat(('source',),impossible_block='victim',hit=3)
        self.cast('remove',target='source')
        self.put_in_owned_zone('source',HAND)
        self.state()

    def dependent(self):
        p,q,m = self.actor,self.foe,self.mode
        target = None if m=='nonartifact' else 'guard' if m=='protected' else 'victim'
        self.cast('source',entry=target)
        self.stats('source')
        if m=='protected':
            self.emit('can_target_creature','source','victim',value=0)
            self.emit('can_target_creature','source','guard',value=1)
        if target=='victim':
            self.bind('victim',p)
        self.state()
        if m=='dies':
            self.cast('remove',target='source')
            self.put_in_owned_zone('source',GRAVE)
            self.bind('victim',q)
            self.state()
        elif m=='stolen_source':
            self.visit(q)
            self.cast('steal',target='source')
            self.bind('source',q)
            if self.id==THIEF:
                self.bind('victim',q)
            self.state()
        elif m=='persist':
            self.see('victim','tapped',1)
            self.visit(q)
            self.state()
            self.visit(p)
            self.see('victim','tapped',0)
            self.state()
        elif m=='flying':
            self.visit(q)
            self.visit(p)
            self.combat(('source',),impossible_block='guard',hit=2)

    def conscripts(self):
        p,q,m = self.actor,self.foe,self.mode
        if m=='fresh_own':
            self.cast('victim')
            self.see('victim','sick',1)
        self.cast('source',entry='victim')
        if m!='fresh_own':
            self.bind('victim',p)
        self.see('victim','tapped',0)
        self.stats('source')
        self.state()
        if m=='land':
            self.emit('activate','victim',value=1)
            self.settle()
            self.pool[p] = (0,0,0,1,0,0)
            self.mana(p)
            self.see('victim','tapped',1)
            self.cast('spend',target='source',actual_pool=True)
            self.see('source','power',6)
        elif m=='source_dies':
            self.cast('remove',target='source')
            self.put_in_owned_zone('source',GRAVE)
            self.state()
            self.combat(('victim',),hit=2)
        else:
            self.combat(('source','victim'),hit=5)
        self.emit('cleanup')
        self.settle()
        if m!='fresh_own':
            self.bind('victim',q)
        if m=='land':
            self.stats('source')
        self.state()

    def run(self):
        self.emit('turn',player=self.actor)
        self.set_pool(self.foe,ZERO)
        self.set_pool(self.actor,ZERO)
        {DRAKE:self.drake,THIEF:self.dependent,SOWER:self.dependent,CONSCRIPTS:self.conscripts}[self.id]()


MANTICORE, MOLTEN, SIREN, GATES = 10890,13011,13805,13225
SPAWNBROKER, PLOTTER, BATTLEMASTER, VICTOR = 7964,8155,6829,15121
FUGUE, JEWEL, HORSE, AUTHORITY = 16606,19082,13371,13604
FOLLOW_MODES = {
    MANTICORE: ('borrow','flying','only_friendly'),
    MOLTEN: ('borrow','decline'),
    SIREN: ('tribute_paid','tribute_declined','permanent'),
    GATES: ('two_gates','one_gate','foreign_gate'),
    SPAWNBROKER: ('equal_power','greater_power','decline'),
    PLOTTER: ('land_exchange','no_enemy_land','no_own_land'),
    BATTLEMASTER: ('both_equipment','pre_equipped','no_equipment'),
    VICTOR: ('power_two','power_three','only_friendly'),
    FUGUE: ('entry','upkeep','noncreature'),
    JEWEL: ('draw','mana','blocked'),
    HORSE: ('controller_upkeep','defender'),
    AUTHORITY: ('entry_artifact','decline','foreign_upkeep_target','own_upkeep_target'),
}
ALL_MODES = MODES | FOLLOW_MODES
TEMPORARY = {MANTICORE,MOLTEN,GATES,VICTOR}
AZORIUS, ORZHOV = 'Azorius Guildgate','Orzhov Guildgate'
ISLAND, PLAINS, MOUNTAIN = 'Island','Plains','Mountain'
PIKER, SOLDIER = 'Goblin Piker','Soldier'
BONES, MORNINGSTAR, ANTHEM = 'Bonesplitter','Vulshok Morningstar','Glorious Anthem'
DEFENDER = 'Wall of Stone'
WITNESSES |= {AZORIUS,ORZHOV,ISLAND,PLAINS,MOUNTAIN,PIKER,SOLDIER,BONES,MORNINGSTAR,ANTHEM,DEFENDER}
GAPS = {
    DRAKE: ['The atomic ETB target callback does not expose a point where a chosen creature can independently become illegal before that trigger resolves.'],
    MOLTEN: ['Duel fixtures have one opponent. They exercise up to one from that opponent, including choosing no creature.'],
    SIREN: ['Duel fixtures have one opponent to offer tribute.'],
    FUGUE: ['Noncreature enchant legality is checked before any target input. Attached creatures exercise entry, cleanup and real upkeep.'],
    JEWEL: ['The native theft callback requires EVENT_PHASE_CHANGED in PHASE_AFTER_BLOCKING. The existing advance_combat wrapper does not dispatch that event at this boundary, so unblocked theft, opponent draws and untap are unobserved.'],
    HORSE: ['Duel fixtures have one opponent to receive the Horse and one opponent of its later controller to receive Soldiers.'],
}


class FollowBoard(Board):
    def witnesses(self):
        p,q,m = self.actor,self.foe,self.mode
        if self.id in TEMPORARY:
            self.temporary_setup(p,q,m)
        elif self.id == SIREN:
            self.add('victim',GIANT,q)
            self.add('guard',BEAR,q)
            if m=='permanent':
                self.add('removal',MURDER,q,HAND)
        elif self.id in (SPAWNBROKER,PLOTTER):
            own = LAND if self.id==PLOTTER and m!='no_own_land' else BEAR
            if self.id==PLOTTER:
                enemy = ISLAND if m!='no_enemy_land' else GIANT
            else:
                enemy = GIANT if m=='greater_power' else PIKER
            self.add('trade',own,p,tapped=self.id==PLOTTER and own==LAND)
            self.add('victim',enemy,q,tapped=self.id==PLOTTER and enemy==ISLAND)
        elif self.id == BATTLEMASTER:
            if m!='no_equipment':
                self.add('owned_gear',BONES,p)
                self.add('foe_gear',MORNINGSTAR,q)
            if m=='pre_equipped':
                self.add('formerly_equipped',GIANT,q)
            self.add('relic',ARTIFACT,q)
        elif self.id == FUGUE:
            self.add('victim',LAND if m=='noncreature' else BEAR,q,tapped=True)
        elif self.id == JEWEL:
            for ref,name in zip(('library_a','library_b','library_c'),(ISLAND,PLAINS,MOUNTAIN)):
                self.add(ref,name,p,LIBRARY)
            if m=='blocked':
                self.add('attacker',BEAR,q)
                self.add('blocker',DEFENDER,p)
                for ref,name in zip(('foe_library_a','foe_library_b','foe_library_c'),(LAND,ISLAND,PLAINS)):
                    self.add(ref,name,q,LIBRARY)
        elif self.id == HORSE:
            # Dormant library witness preserves the existing modeled hand receipts.
            self.setup.append(('horse_bounce_library',p,LIBRARY,self.rows[BOUNCE]['csvid'],0))
            if m=='defender':
                self.add('attacker',BEAR,q)
        elif self.id == AUTHORITY:
            self.add('entry_target',ARTIFACT,q)
            self.add('scope_land',LAND,q,tapped=True)
            if m.endswith('upkeep_target'):
                self.add('upkeep_target',ANTHEM,p if m=='own_upkeep_target' else q)

    def temporary_setup(self, p, q, mode):
        name = GIANT if mode=='power_three' else BEAR
        target_seat = p if mode=='only_friendly' else q
        self.add('victim',name,target_seat,tapped=True)
        if mode=='flying':
            self.add('guard',PIKER,q)
        if self.id==GATES:
            self.add('gate_a',AZORIUS,p,tapped=True)
            if mode!='one_gate':
                self.add('gate_b',ORZHOV,p if mode=='two_gates' else q,tapped=True)

    def state(self):
        if self.id not in (JEWEL,AUTHORITY):
            return super().state()
        self.emit('assert_stack')
        extra = LIBRARY if self.id==JEWEL else EXILE
        for player in (0,1):
            self.mana(player)
            self.emit('assert_life',player=player,value=self.life[player])
            for zone in (HAND,BATTLE,GRAVE,extra):
                counts = Counter(name for name,p,z in self.cards.values() if (p,z)==(player,zone))
                self.emit('assert_zone',player=player,zone=zone,value=counts.total())
                for name,count in sorted(counts.items()):
                    self.emit('assert_zone_multiplicity',player=player,zone=zone,value=count,index=self.rows[name]['csvid'])

    def upkeep(self, actor, *, choices=()):
        assert actor==1-self.active and self.pool==[ZERO,ZERO]
        self.emit('advance_turn')
        self.emit('untap_step',player=actor)
        for op,ref,value in choices:
            self.emit(op,ref,player=actor,value=value)
        self.emit('upkeep_step',player=actor)
        self.settle()
        self.emit('turn',player=actor)
        self.active = actor

    def triggered(self, choices=(), *, target=None, decline=False):
        self.cast('source',target=target,entry=None,choices=choices,decline=decline)

    def return_borrowed(self):
        self.emit('cleanup')
        self.settle()
        self.bind('victim',self.foe)
        self.state()

    def temporary(self):
        m,p,q = self.mode,self.actor,self.foe
        can_steal = m in ('borrow','flying','two_gates','power_two')
        take_optional = self.id==MOLTEN and p==0 and m in ('borrow','decline')
        choices = (('choose_trigger','source',p,0),) if take_optional else ()
        if m=='decline':
            choices += (('choose_optional_target','victim',p,0),)
        elif can_steal:
            choices += (('choose_target','victim',p,0),)
        self.triggered(choices,decline=m=='decline')
        self.stats('source')
        if not can_steal:
            if 'victim' in self.cards:
                self.see('victim','tapped',1)
            self.state()
            if m=='decline':
                self.combat(('source',),hit=6)
            return
        self.bind('victim',p)
        self.see('victim','tapped',0)
        self.state()
        attack = ('source','victim') if self.id==MOLTEN else ('victim',)
        self.combat(attack,hit=8 if self.id==MOLTEN else 2)
        self.return_borrowed()
        if m=='flying':
            self.visit(q)
            self.visit(p)
            self.combat(('source',),impossible_block='guard',hit=5)

    def siren(self):
        p,q = self.actor,self.foe
        paid = self.mode=='tribute_paid'
        self.emit('present_ai_dialogs',value=1)
        choices = (('choose_dialog','',q,int(not paid)),)
        if not paid:
            choices += (('choose_target','victim',p,0),)
        self.triggered(choices)
        self.see('source','plus1_counters',3 if paid else 0)
        for prop in ('power','toughness'):
            self.see('source',prop,4 if paid else 1)
        if not paid:
            self.bind('victim',p)
        self.state()
        if self.mode=='permanent':
            self.cast('removal',target='source')
            self.put_in_owned_zone('source',GRAVE)
            self.state()
        else:
            self.visit(q)
            self.visit(p)
            self.combat(('source',),impossible_block='guard',hit=4 if paid else 1)

    def exchange_other(self):
        p,q,m = self.actor,self.foe,self.mode
        exchange = m in ('equal_power','land_exchange')
        if exchange:
            choices = (('choose_target','trade',p,0),('choose_target','victim',p,0))
        elif m=='greater_power':
            choices = (('choose_target','trade',p,0),)
        elif m=='decline':
            choices = (('choose_optional_target','trade',p,0),)
        else:
            choices = ()
        self.triggered(choices,decline=m=='decline')
        if exchange:
            self.bind('trade',q)
            self.bind('victim',p)
            self.state()
            self.visit(q)
        self.stats('source')
        self.state()

    def battlemaster(self):
        m,p,q = self.mode,self.actor,self.foe
        if m=='pre_equipped':
            self.emit('turn',player=q)
            self.active = q
            self.set_pool(q,(2,0,0,0,0,0))
            self.emit('choose_target','formerly_equipped',player=q)
            self.emit('activate','foe_gear',value=1)
            self.settle()
            self.pool[q] = ZERO
            self.mana(q)
            self.see('formerly_equipped','power',5)
            self.see('formerly_equipped','toughness',5)
            self.visit(p)
        self.cast('source')
        bonus = m!='no_equipment'
        self.see('source','power',6 if bonus else 2)
        self.see('source','toughness',4 if bonus else 2)
        if m=='pre_equipped':
            self.stats('formerly_equipped')
        self.state()
        self.combat(('source',),hit=6 if bonus else 2)

    def fugue(self):
        p,q,m = self.actor,self.foe,self.mode
        if m=='noncreature':
            self.set_pool(p,cost_from_oracle(self.oracle[self.name]))
            self.emit('can_cast','source',value=1)
            self.see('victim','in_play',1)
            self.see('source','in_hand',1)
            self.state()
            return
        self.triggered(target='victim')
        self.bind('victim',p)
        self.see('victim','tapped',0)
        self.state()
        self.combat(('victim',),hit=2)
        self.return_borrowed()
        if m=='upkeep':
            self.upkeep(q)
            self.state()
            self.upkeep(p)
            self.bind('victim',p)
            self.see('victim','tapped',0)
            self.state()
            self.combat(('victim',),hit=2)
            self.return_borrowed()

    def jewel(self):
        p,q,m = self.actor,self.foe,self.mode
        self.cast('source')
        for ref in ('library_a','library_b','library_c'):
            self.put_in_owned_zone(ref,HAND)
        self.state()
        if m=='mana':
            color = 3 if p==0 else 4
            self.emit('choose_color',player=p,value=color)
            self.emit('activate','source',value=1)
            self.settle()
            self.pool[p] = tuple(3 if index==color else 0 for index in range(6))
            self.see('source','tapped',1)
            self.emit('can_activate','source',value=0)
            self.state()
        elif m=='blocked':
            self.visit(q)
            self.combat(('attacker',),blocked_by='blocker',hit=0)

    def horse(self):
        p,q = self.actor,self.foe
        self.cast('source')
        self.bind('source',q)
        self.stats('source')
        self.state()
        self.upkeep(q)
        self.emit('bind_permanent_copy','soldier',player=p,value=self.rows[SOLDIER]['csvid'])
        self.cards['soldier'] = [SOLDIER,p,BATTLE]
        for prop,value in (('token',1),('color',32),('power',1),('toughness',1),('subtype_Soldier',1)):
            self.emit('assert_matching_permanents',prop=prop,player=p,value=value,index=self.rows[SOLDIER]['csvid'])
        self.state()
        if self.mode=='defender':
            self.emit('begin_attackers')
            self.emit('attack','source',value=0)
            self.emit('attack','attacker',value=1)
            self.emit('finish_attackers')
            self.settle()
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.settle()
            self.emit('combat_damage')
            self.settle()
            self.emit('end_combat')
            self.life[p] -= 2
        else:
            self.upkeep(p)
        self.state()
        self.bounce_horse()

    def bounce_horse(self):
        p,q = self.actor,self.foe
        if self.active==p:
            self.visit(q)
        self.emit('advance_turn')
        self.emit('untap_step',player=p)
        self.emit('upkeep_step',player=p)
        self.emit('draw_step',player=p)
        self.settle()
        self.active=p
        self.emit('turn',player=p)
        self.emit('bind_hand_copy','horse_bounce',player=p,value=self.rows[BOUNCE]['csvid'])
        self.cards['horse_bounce']=[BOUNCE,p,HAND]
        self.cast('horse_bounce',target='source')
        self.put_in_owned_zone('source',HAND)
        self.state()
        self.upkeep(q)
        self.state()

    def authority(self):
        p,q,m = self.actor,self.foe,self.mode
        optional = m=='decline'
        self.cast('source',entry='entry_target',decline=optional)
        if not optional:
            self.put_in_owned_zone('entry_target',EXILE)
        self.state()
        if m.endswith('upkeep_target'):
            self.upkeep(q)
            self.state()
            choice = (('choose_trigger','source',0),) if p==0 else ()
            self.upkeep(p,choices=(*choice,('choose_target','upkeep_target',0)))
            self.put_in_owned_zone('upkeep_target',EXILE)
            if m=='foreign_upkeep_target':
                self.bind('source',q)
            self.state()

    def run(self):
        self.emit('turn',player=self.actor)
        self.set_pool(self.foe,ZERO)
        self.set_pool(self.actor,ZERO)
        actions = {SIREN:self.siren,SPAWNBROKER:self.exchange_other,PLOTTER:self.exchange_other,
                   BATTLEMASTER:self.battlemaster,FUGUE:self.fugue,JEWEL:self.jewel,HORSE:self.horse,
                   AUTHORITY:self.authority}
        actions.get(self.id,self.temporary)()


ZONES = ('hand','battlefield','library','graveyard','exile')
ZONE_OPS = {'assert_zone','assert_zone_multiplicity'}
INDEXED = {'assert_zone_multiplicity','assert_matching_permanents','bind_permanent_copy','bind_hand_copy'}
SEATED = {'turn','untap_step','upkeep_step','draw_step','bind_hand_copy','choose_target','choose_optional_target','choose_dialog',
          'choose_trigger','choose_color','set_mana','assert_mana','assert_life','bind_permanent_copy',
          'assert_matching_permanents'} | ZONE_OPS
VALUED = {'cast','can_cast','activate','can_activate','can_target_creature','choose_dialog','choose_color',
          'present_ai_dialogs','set_mana','assert_mana','assert_life','assert_stack','assert_card',
          'assert_unused_choices','assert_forced_target_choices','assert_matching_permanents',
          'bind_permanent_copy','bind_hand_copy','attack','block'} | ZONE_OPS


def operation(row):
    op,ref,other,prop,player,value,zone,index,color = row
    action = dict(op=op)
    if ref:
        action['ref'] = ref
    if other:
        action['other'] = other
    if prop:
        action['property'] = prop
    if op in SEATED:
        action['player'] = player
    if op in VALUED:
        action['value'] = value
    if op in ZONE_OPS:
        action['zone'] = ZONES[zone]
    if op in INDEXED:
        action['index'] = index
    if op in ('set_mana','assert_mana'):
        action['color'] = color
    return action


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (not pin or not exact(record,pin['oracle']) or csvid not in ALL_MODES
            or pin['registry']['csvid'] != csvid or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole pinned normal entry control primary required')
    return dict(csvid=csvid,mana=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in ALL_MODES[shape['csvid']] for seat in (0,1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(shape))):
        raise ValueError('distinct supported entry control variants required')


def spec(declaration,record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned entry control primary required')
    shape = classify(record,pin['registry']['csvid'])
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not exact(declaration.get('params'),shape)):
        raise ValueError('exact declared entry control parameters required')
    validate_variants(shape,declaration.get('variants'))
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    pinned = profiles()
    if (set(pinned)!={'sources','witnesses'} or set(pinned['witnesses'])!=WITNESSES
            or {pin['registry']['csvid'] for pin in pinned['sources'].values()}!=ALL_MODES.keys()):
        raise ValueError('complete entry control source and witness pins required')
    for group,entries in pinned.items():
        for name,pin in entries.items():
            record,row = oracle.get(name),rows.get(name,{})
            if not exact(record,pin['oracle']) or not exact(row,pin['registry']):
                raise ValueError('entry control source or witness changed: '+name)
            if group=='sources':
                if row.get('kind')!='card' or row.get('handler') in (None,'0','NULL','card_dummy'):
                    raise ValueError('implemented entry control primary required')
                classify(record,row['csvid'])
            elif name==SOLDIER and (row['csvid']!=18822 or record['layout']!='token'):
                raise ValueError('exact physical Horse Soldier template required')


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid']!=csvid or shape['csvid']!=csvid:
        raise ValueError('exact registered entry control source required')
    validate_variants(shape,selected)
    validate_witnesses(oracle,rows)
    result = []
    reverse = {rows[n]['csvid']:n for n in WITNESSES|{name}}
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        board_type = FollowBoard if csvid in FOLLOW_MODES else Board
        game = board_type(rows[name],mode,int(seat),rows,oracle)
        game.run()
        setup = []
        for ref,actor,zone,card,tapped in game.setup:
            item = dict(name=reverse[card],ref=ref,player=actor,zone=ZONES[zone])
            if tapped:
                item['state'] = ['tapped']
            setup.append(item)
        result.append(dict(name=label(csvid,mode,seat),card=name,fixture='empty_main',seed=SEED,
                           setup=setup,steps=[operation(t) for t in game.steps],
                           _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                                        oracle_text=declaration['oracle_text'],untested_components=GAPS.get(csvid,[]))))
    return result

