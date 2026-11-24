"""Freeze four Oracle-pinned Aura and Equipment entry diagnostics."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'remaining_entry_attachments'
PROFILE = Path(__file__).with_name('data') / 'remaining_entry_attachments_wave8_profiles.json'

RESEARCHER, OUTFITTER, SLEDGE, CUTLASS = 3688,10588,11504,17478
PREFIX, SEED = 'RemainingEntryAttachmentsRecipe',3500
MODES = {
    RESEARCHER: ('attach','decline','wrong_aura','blink'),
    OUTFITTER: ('attach','move','decline','foreign_equipment'),
    SLEDGE: ('entry_combat','sacrifice_reattach','offturn','foreign_creature'),
    CUTLASS: ('pirate','changeling','paid_reattach','foreign_pirate'),
}
HAND,BATTLE,LIBRARY,GRAVE = range(4)
ZERO = (0,)*6
BEAR,GIANT,PIRATE,CHANGELING = 'Grizzly Bears','Hill Giant','Fathom Fleet Firebrand','Amoeboid Changeling'
HOLY,UNHOLY,WILD,BLINK = 'Holy Strength','Unholy Strength','Wild Growth','Cloudshift'
BONES,RELIC = 'Bonesplitter','Darksteel Relic'
WITNESSES = {BEAR,GIANT,PIRATE,CHANGELING,HOLY,UNHOLY,WILD,BLINK,BONES,RELIC}


def label(csvid,mode,p):
    return f'{PREFIX}_{csvid}_{mode}_{p}'


class Board:
    def __init__(self, row, mode, actor, rows, oracle):
        self.id,self.name = row['csvid'],row['name']
        self.mode,self.actor,self.foe,self.active = mode,actor,1-actor,actor
        self.rows,self.oracle = rows,oracle
        self.setup,self.steps,self.cards = [],[],{}
        self.mana,self.life = [ZERO,ZERO],[20,20]
        self.add('source',self.name,actor,HAND)
        self.witnesses()

    def add(self, ref, name, player, zone=BATTLE, *, tapped=False):
        assert ref not in self.cards
        self.setup.append((ref,player,zone,self.rows[name]['csvid'],int(tapped)))
        self.cards[ref] = [name,player,zone]

    def witnesses(self):
        p,q,m = self.actor,self.foe,self.mode
        if self.id==RESEARCHER:
            if m!='wrong_aura':
                self.add('chosen',HOLY,p,HAND)
            self.add('wrong',WILD,p,HAND)
            self.add('enemy_aura',HOLY if m=='wrong_aura' else UNHOLY,q,HAND)
            self.add('own_bear',BEAR,p)
            if m=='blink':
                self.add('later',UNHOLY,p,HAND)
                self.add('blink',BLINK,p,HAND)
        elif self.id==OUTFITTER:
            self.add('gear',BONES,q if m=='foreign_equipment' else p)
            self.add('plain_artifact',RELIC,p)
            self.add('own_bear',BEAR,p)
            self.add('enemy_giant',GIANT,q)
        elif self.id==SLEDGE:
            self.add('first',BEAR,q if m=='foreign_creature' else p)
            if m in ('sacrifice_reattach','offturn'):
                self.add('second',GIANT,p)
                self.add('fodder',RELIC,p)
            if m=='foreign_creature':
                self.add('own_artifact',RELIC,p)
        else:
            self.add('pirate',CHANGELING if m=='changeling' else PIRATE,q if m=='foreign_pirate' else p)
            self.add('nonpirate',BEAR,p)

    def emit(self, op, ref='', other='', *, prop='', player=0, value=0, zone=HAND, index=0, color=0):
        self.steps.append((op,ref,other,prop,player,value,zone,index,color))

    def see(self, ref, prop, value):
        self.emit('assert_card',ref,prop=prop,value=value)

    def stats(self, ref, power_bonus=0, toughness_bonus=0):
        record = self.oracle[self.cards[ref][0]]
        self.see(ref,'power',int(record['power'])+power_bonus)
        self.see(ref,'toughness',int(record['toughness'])+toughness_bonus)

    def funds(self, p, pool=ZERO):
        self.mana[p] = tuple(pool)
        for color,count in enumerate(pool):
            self.emit('set_mana',player=p,color=color,value=count)
        self.emit('refresh')

    def wallet(self, p):
        for color,count in enumerate(self.mana[p]):
            self.emit('assert_mana',player=p,color=color,value=count)

    def state(self):
        self.emit('assert_stack')
        for p in (0,1):
            self.wallet(p)
            self.emit('assert_life',player=p,value=self.life[p])
            for zone in (HAND,BATTLE,GRAVE):
                counts = Counter(name for name,player,z in self.cards.values() if (player,z)==(p,zone))
                self.emit('assert_zone',player=p,zone=zone,value=counts.total())
                for name,count in sorted(counts.items()):
                    self.emit('assert_zone_multiplicity',player=p,zone=zone,value=count,index=self.rows[name]['csvid'])

    def settle(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices')
        self.emit('assert_stack')

    def cast(self, ref, *, target=None, choices=(), initial_choices=(), budget=None, existing_mana=False):
        name,actor,zone = self.cards[ref]
        assert zone==HAND
        cost = tuple(cost_from_oracle(self.oracle[name]) if budget is None else budget)
        if existing_mana:
            assert self.mana[actor]==cost
        else:
            self.funds(actor,cost)
        for op,selected,player,value in initial_choices:
            self.emit(op,selected,player=player,value=value)
        if target:
            self.emit('choose_target',target,player=actor)
        self.emit('cast',ref,value=1)
        self.emit('assert_unused_choices')
        self.emit('assert_stack',value=1)
        self.mana[actor]=ZERO
        self.wallet(actor)
        for op,selected,player,value in choices:
            self.emit(op,selected,player=player,value=value)
        self.settle()
        self.cards[ref][2] = GRAVE if self.oracle[name]['type_line'] in ('Instant','Sorcery') else BATTLE

    def ready_turn(self, player):
        assert player==1-self.active and self.mana==[ZERO,ZERO]
        self.emit('advance_turn')
        self.emit('untap_step',player=player)
        self.emit('turn',player=player)
        self.active = player

    def equip(self, ref, recipient, *, fee=0, sacrifice=None, actor=None):
        p = self.actor if actor is None else actor
        if fee:
            self.funds(p,(fee,0,0,0,0,0))
        self.emit('can_activate',ref,value=1)
        if sacrifice and p==0:
            self.emit('choose_target',sacrifice,player=p)
        self.emit('choose_target',recipient,player=p)
        self.emit('activate',ref,value=1)
        self.mana[p] = ZERO
        self.wallet(p)
        self.emit('assert_stack',value=1)
        self.emit('assert_unused_choices')
        if sacrifice:
            self.cards[sacrifice][2]=GRAVE
            self.emit('assert_zone_multiplicity',player=p,zone=GRAVE,
                      value=1,index=self.rows[self.cards[sacrifice][0]]['csvid'])
        self.settle()

    def combat(self, attacker, hit, *, denied_blocker=None):
        self.emit('begin_attackers')
        self.emit('attack',attacker,value=1)
        self.see(attacker,'tapped',1)
        self.emit('finish_attackers')
        self.settle()
        self.emit('begin_blockers')
        if denied_blocker:
            self.emit('block',denied_blocker,attacker,value=0)
        self.emit('advance_combat')
        self.settle()
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')
        self.life[1-self.active]-=hit
        self.state()

    def researcher(self):
        p,m = self.actor,self.mode
        choices = ()
        if m!='wrong_aura':
            if p==0:
                choices = (('choose_trigger','source',p,0),)
            op = 'choose_optional_zone' if m=='decline' else 'choose_zone_card'
            choices += ((op,'',p,self.rows[HOLY]['csvid']),)
        self.cast('source',choices=choices)
        if m in ('attach','blink'):
            self.cards['chosen'][2]=BATTLE
            self.stats('source',1,2)
        else:
            self.stats('source')
        if m=='decline':
            self.emit('assert_forced_zone_choices')
        self.stats('own_bear')
        self.state()
        if m=='blink':
            choices = (('choose_trigger','source',p,0),) if p==0 else ()
            choices += (('choose_zone_card','',p,self.rows[UNHOLY]['csvid']),)
            self.cast('blink',target='source',choices=choices)
            self.emit('bind_permanent_copy','source',player=p,value=self.id)
            self.cards['chosen'][2]=GRAVE
            self.cards['later'][2]=BATTLE
            self.stats('source',2,1)
            self.stats('own_bear')
            self.state()

    def outfitter(self):
        p,m = self.actor,self.mode
        if m=='move':
            self.equip('gear','own_bear',fee=1)
            self.stats('own_bear',2,0)
        choices = ()
        if m!='foreign_equipment':
            if p==0:
                choices = (('choose_trigger','source',p,0),)
            choices += (('choose_target','gear',p,0),)
            op = 'choose_optional_target' if m=='decline' else 'choose_target'
            choices += ((op,'source',p,0),)
        self.cast('source',choices=choices)
        if m=='decline':
            self.emit('assert_forced_target_choices')
        self.stats('source',2 if m in ('attach','move') else 0,0)
        self.stats('own_bear')
        self.stats('enemy_giant')
        self.state()

    def sledge(self):
        p,m = self.actor,self.mode
        attached = m!='foreign_creature'
        self.cast('source',choices=(('choose_target','first',p,0),) if attached else ())
        self.stats('first',3 if attached else 0,int(attached))
        if 'second' in self.cards:
            self.stats('second')
        self.state()
        if m=='sacrifice_reattach':
            self.equip('source','second',sacrifice='fodder')
            self.stats('first')
            self.stats('second',3,1)
            self.state()
        elif m=='entry_combat':
            # The Equipment itself is a legal artifact sacrifice cost.
            self.emit('can_activate','source',value=1)
            self.combat('first',hit=5)
        elif m=='offturn':
            self.ready_turn(self.foe)
            self.emit('can_activate','source',value=0)
            self.stats('first',3,1)
            self.stats('second')
            self.state()

    def cutlass(self):
        p,m = self.actor,self.mode
        attached = m!='foreign_pirate'
        self.cast('source',choices=(('choose_target','pirate',p,0),) if attached else ())
        self.stats('pirate',2 if attached else 0,int(attached))
        self.stats('nonpirate')
        self.state()
        if m=='paid_reattach':
            self.funds(p,(1,0,0,0,0,0))
            self.emit('can_activate','source',value=0)
            self.wallet(p)
            self.equip('source','nonpirate',fee=2)
            self.stats('pirate')
            self.stats('nonpirate',2,1)
            self.combat('nonpirate',hit=4)

    def run(self):
        self.emit('turn',player=self.actor)
        self.funds(self.foe)
        self.funds(self.actor)
        {RESEARCHER:self.researcher,OUTFITTER:self.outfitter,SLEDGE:self.sledge,CUTLASS:self.cutlass}[self.id]()


MAGE, GIANT_AURA, CARRY, COCOON = 7766,14076,6863,598
EPHARA, REFLECTION, BARRIER, KELP = 13694,12403,5251,949
GOLD, REINS, BLADEMASTER = 836,11406,19098
FOLLOW_MODES = {
    MAGE: ('library_attach','fail_to_find'),
    GIANT_AURA: ('grave','hand','library','decline'),
    CARRY: ('unattach','release','only_artifact'),
    COCOON: ('countdown','foreign_creature'),
    EPHARA: ('return','foreign_creature','flight'),
    REFLECTION: ('existing','new_entrant','removed'),
    BARRIER: ('own_creature','foreign_creature','player'),
    KELP: ('last_attack','idle'),
    GOLD: ('zero','two','source_removed'),
    REINS: ('foreign_land','release_creature','own_land'),
    BLADEMASTER: ('both','subset','foreign_equipment'),
}
ALL_MODES = MODES | FOLLOW_MODES
HUMAN_ONLY = {(GIANT_AURA,'decline')}
CAD, FOREST, PLAINS, MEMNITE = 'Eager Cadet','Forest','Plains','Memnite'
DISENCHANT, BOLT, HAUNTING, SPIRIT, GROWTH = 'Disenchant','Lightning Bolt','Midnight Haunting','Spirit','Giant Growth'
WITNESSES |= {CAD,FOREST,PLAINS,MEMNITE,DISENCHANT,BOLT,HAUNTING,SPIRIT,GROWTH}

GAPS = {
    MAGE: ['The legacy ETB callback selects and attaches atomically. No available priority step can remove the already-entered Mage before that search while preserving its trigger.'],
    GIANT_AURA: ['Production searches graveyard, hand and library in that order. Each printed zone is independently exercised; the Human also declines before searching.'],
    COCOON: ['No direct pupa observer. Three blocked own untaps, the fourth-upkeep sacrifice and the permanent combat result prove the countdown.'],
    GOLD: ['No direct sleep observer. Real X payment, exact consecutive untap/upkeep boundaries, and removal of the source distinguish zero and multiple counters.'],
}


class FollowBoard(Board):
    def witnesses(self):
        p,q,m = self.actor,self.foe,self.mode
        if self.id in (MAGE,GIANT_AURA):
            where = {'grave':GRAVE,'hand':HAND,'decline':HAND}.get(m,LIBRARY)
            if m!='fail_to_find':
                self.add('aura',HOLY,p,where)
            self.add('wrong',WILD,p,LIBRARY)
            self.add('non_aura',RELIC,p,LIBRARY)
        elif self.id==CARRY:
            self.add('foil',RELIC,q)
            if m!='only_artifact':
                self.add('gear',BONES,q)
                self.add('foe_host',GIANT,q)
                self.add('own_host',BEAR,p)
            if m=='release':
                self.add('remove',DISENCHANT,q,HAND)
        elif self.id==COCOON:
            self.add('host',BEAR,q if m=='foreign_creature' else p)
            if m=='countdown':
                self.add('guard',GIANT,q)
        elif self.id==EPHARA:
            self.add('host',BEAR,p)
            self.add('guard',GIANT,q)
            if m!='flight':
                self.add('entrant',CAD,q if m=='foreign_creature' else p,HAND)
        elif self.id==REFLECTION:
            self.add('host',GIANT,q)
            self.add('ours',CAD,p)
            self.add('their_cadet',CAD,q)
            self.add('relic',RELIC,p)
            if m=='existing':
                self.add('make_tokens',HAUNTING,p,HAND)
            else:
                self.add('entrant',MEMNITE,p,HAND)
            if m=='removed':
                self.add('remove',DISENCHANT,q,HAND)
        elif self.id==BARRIER:
            self.add('drawn',PLAINS,p,LIBRARY)
            self.add('recipient',GIANT,q if m=='foreign_creature' else p)
            self.add('bolt',BOLT,p if m=='foreign_creature' else q,HAND)
        elif self.id in (KELP,GOLD):
            self.add('host',BEAR,q)
            if m=='source_removed':
                self.add('remove',DISENCHANT,p,HAND)
        elif self.id==REINS:
            own = m=='own_land'
            self.add('host',BEAR if m=='release_creature' else FOREST,p if own else q,tapped=True)
            if m!='release_creature':
                self.add('own_bear',BEAR,p)
                self.add('spend',GROWTH,p,HAND)
            else:
                self.add('remove',DISENCHANT,q,HAND)
        elif self.id==BLADEMASTER:
            self.add('ally',BEAR,p)
            self.add('guard',GIANT,q)
            seat = q if m=='foreign_equipment' else p
            self.add('gear',BONES,seat)
            self.add('aura',HOLY,p,HAND)
            self.add('nonattachment',RELIC,p)

    def library(self):
        for p in (0,1):
            counts = Counter(name for name,player,zone in self.cards.values() if (player,zone)==(p,LIBRARY))
            self.emit('assert_zone',player=p,zone=LIBRARY,value=counts.total())
            for name,number in sorted(counts.items()):
                self.emit('assert_zone_multiplicity',player=p,zone=LIBRARY,value=number,index=self.rows[name]['csvid'])

    def bound(self, ref, player, *, ordinal=0):
        name,_,zone = self.cards[ref]
        assert zone==BATTLE
        self.cards[ref][1] = player
        self.emit('bind_permanent_copy',ref,player=player,value=self.rows[name]['csvid'],index=ordinal)

    def upkeep_to(self, actor):
        assert actor==1-self.active and self.mana==[ZERO,ZERO]
        self.emit('advance_turn')
        self.emit('untap_step',player=actor)
        self.emit('upkeep_step',player=actor)
        self.settle()
        self.emit('turn',player=actor)
        self.active=actor

    def search_aura(self):
        p,m = self.actor,self.mode
        found = m not in ('fail_to_find','decline')
        do_search = m!='decline'
        choices = (('choose_trigger','source',p,0),) if self.id==GIANT_AURA and p==0 and do_search else ()
        if found:
            choices += (('choose_zone_card','',p,self.rows[HOLY]['csvid']),)
        elif do_search:
            choices += (('choose_optional_zone','',p,self.rows[WILD]['csvid']),)
        self.cast('source',choices=choices)
        if found:
            self.cards['aura'][2]=BATTLE
        self.stats('source',int(found),2*int(found))
        self.emit('assert_shuffle_count',player=p,value=int(do_search and m in ('library','library_attach','fail_to_find')))
        self.state()
        self.library()

    def carry_away(self):
        p,q,m = self.actor,self.foe,self.mode
        if m=='only_artifact':
            self.funds(p,cost_from_oracle(self.oracle[self.name]))
            self.emit('can_cast','source',value=0)
            self.state()
            return
        self.emit('turn',player=q)
        self.active=q
        self.equip('gear','foe_host',fee=1,actor=q)
        self.stats('foe_host',2,0)
        self.ready_turn(p)
        self.cast('source',target='gear')
        self.bound('gear',p)
        self.stats('foe_host')
        self.stats('own_host')
        self.state()
        if m=='release':
            self.equip('gear','own_host',fee=1)
            self.stats('own_host',2,0)
            self.ready_turn(q)
            self.cast('remove',target='source')
            self.cards['source'][2]=GRAVE
            self.bound('gear',q)
            self.stats('own_host',2,0)
            self.stats('foe_host')
            self.state()

    def cocoon(self):
        p,q,m = self.actor,self.foe,self.mode
        if m=='foreign_creature':
            self.funds(p,cost_from_oracle(self.oracle[self.name]))
            self.emit('can_cast','source',value=0)
            self.see('host','tapped',0)
            self.state()
            return
        self.cast('source',target='host')
        self.see('host','tapped',1)
        self.state()
        for number in range(1,5):
            self.upkeep_to(q)
            self.upkeep_to(p)
            self.see('host','tapped',int(number<=3))
            if number<4:
                self.see('host','plus1_counters',0)
                self.see('source','in_play',1)
        self.cards['source'][2]=GRAVE
        self.see('host','plus1_counters',1)
        self.stats('host',1,1)
        self.state()
        self.ready_turn(q)
        self.ready_turn(p)
        self.combat('host',hit=3,denied_blocker='guard')

    def ephara(self):
        p,q,m = self.actor,self.foe,self.mode
        self.cast('source',target='host')
        self.stats('host',1,1)
        self.see('host','plus1_counters',1)
        self.state()
        if m=='flight':
            self.combat('host',hit=3,denied_blocker='guard')
            return
        if m=='foreign_creature':
            self.ready_turn(q)
        choices = (('choose_trigger','source',p,0),) if p==0 and m=='return' else ()
        self.cast('entrant',choices=choices)
        if m=='return':
            self.cards['source'][2]=HAND
        self.stats('host',1,1)
        self.see('host','plus1_counters',1)
        self.state()

    def reflection(self):
        p,q,m = self.actor,self.foe,self.mode
        if m=='existing':
            self.cast('make_tokens')
            for idx in (0,1):
                ref = f'spirit_{idx}'
                self.cards[ref]=[SPIRIT,p,BATTLE]
                self.bound(ref,p,ordinal=idx)
        self.cast('source',target='host')
        self.cards['ours'][0]=GIANT
        self.stats('ours')
        self.stats('their_cadet')
        if m=='existing':
            for idx in (0,1):
                self.stats(f'spirit_{idx}')
        self.state()
        if m=='removed':
            self.cast('remove',target='source')
            self.cards['source'][2]=GRAVE
        if m!='existing':
            self.cast('entrant',budget=ZERO)
            if m!='removed':
                self.cards['entrant'][0]=GIANT
            self.stats('entrant')
            self.stats('ours')
            self.state()

    def barrier(self):
        p,q,m = self.actor,self.foe,self.mode
        self.cast('source')
        self.cards['drawn'][2]=HAND
        self.state()
        self.library()
        if m=='player':
            self.cast('bolt',initial_choices=(('choose_player','',q,p),))
            self.life[p]-=3
        else:
            self.cast('bolt',target='recipient')
            if m=='own_creature':
                self.see('recipient','in_play',1)
                self.see('recipient','damage',2)
            else:
                self.cards['recipient'][2]=GRAVE
                self.see('recipient','in_play',0)
        self.state()

    def kelp(self):
        p,q,m = self.actor,self.foe,self.mode
        self.cast('source',target='host')
        self.see('host','tapped',1)
        self.ready_turn(q)
        self.see('host','tapped',0)
        if m=='last_attack':
            self.combat('host',hit=2)
        self.ready_turn(p)
        self.ready_turn(q)
        self.see('host','tapped',int(m=='last_attack'))
        self.ready_turn(p)
        self.ready_turn(q)
        self.see('host','tapped',0)
        self.state()

    def gold(self):
        p,q,m = self.actor,self.foe,self.mode
        x = 0 if m=='zero' else 2
        self.cast('source',target='host',budget=(x,0,2,0,0,0),initial_choices=(('choose_x','',p,x),))
        self.see('host','tapped',1)
        self.state()
        if m=='source_removed':
            self.cast('remove',target='source')
            self.cards['source'][2]=GRAVE
        for step in range(1,4 if m=='two' else 2):
            self.upkeep_to(q)
            self.see('host','tapped',int(m=='two' and step<=2))
            if step<3 and m=='two':
                self.upkeep_to(p)
                self.see('host','tapped',1)
        self.state()

    def reins(self):
        p,q,m = self.actor,self.foe,self.mode
        self.cast('source',target='host')
        if m!='own_land':
            self.bound('host',p)
        self.see('host','tapped',0)
        self.state()
        if m=='release_creature':
            self.ready_turn(q)
            self.cast('remove',target='source')
            self.cards['source'][2]=GRAVE
            self.bound('host',q)
            self.stats('host')
        else:
            self.emit('activate','host',value=1)
            self.settle()
            self.mana[p]=(0,0,0,1,0,0)
            self.wallet(p)
            self.see('host','tapped',1)
            self.cast('spend',target='own_bear',existing_mana=True)
            self.stats('own_bear',3,3)
        self.state()

    def blademaster(self):
        p,q,m = self.actor,self.foe,self.mode
        carry = m!='foreign_equipment'
        if carry:
            self.cast('aura',target='ally')
            self.equip('gear','ally',fee=1)
            self.stats('ally',3,2)
        choices = (('choose_trigger','source',p,0),) if p==0 and carry else ()
        if carry:
            choices += (('choose_target','gear',p,0),)
            choices += ((('choose_target','aura',p,0),) if m=='both' else (('choose_done_target','',p,0),))
        self.cast('source',choices=choices)
        if m=='both':
            self.stats('source',3,2)
            self.stats('ally',2,2)
        elif m=='subset':
            self.stats('source',2,0)
            self.stats('ally',2,3)
        else:
            self.stats('source')
            self.stats('ally')
        self.stats('guard')
        self.state()
        self.ready_turn(q)
        self.ready_turn(p)
        self.combat('source',hit={'both':12,'subset':10,'foreign_equipment':6}[m],denied_blocker='guard')

    def run(self):
        self.emit('turn',player=self.actor)
        self.funds(self.foe)
        self.funds(self.actor)
        modes = {MAGE:self.search_aura,GIANT_AURA:self.search_aura,CARRY:self.carry_away,COCOON:self.cocoon,
                 EPHARA:self.ephara,REFLECTION:self.reflection,BARRIER:self.barrier,KELP:self.kelp,
                 GOLD:self.gold,REINS:self.reins,BLADEMASTER:self.blademaster}
        modes[self.id]()


ZONES = ('hand','battlefield','library','graveyard')
ZONE_OPS = {'assert_zone','assert_zone_multiplicity'}
INDEXED = {'assert_zone_multiplicity','bind_permanent_copy'}
SEATED = {'turn','untap_step','upkeep_step','choose_target','choose_optional_target','choose_done_target',
          'choose_trigger','choose_zone_card','choose_optional_zone','choose_player','choose_x',
          'set_mana','assert_mana','assert_life','assert_shuffle_count','bind_permanent_copy'} | ZONE_OPS
VALUED = {'cast','can_cast','can_activate','activate','choose_zone_card','choose_optional_zone','choose_player','choose_x',
          'assert_unused_choices','assert_forced_target_choices','assert_forced_zone_choices','assert_shuffle_count',
          'assert_stack','assert_card','bind_permanent_copy','set_mana','assert_mana','assert_life','attack','block'} | ZONE_OPS


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
            or pin['registry']['csvid']!=csvid or record.get('layout')!='normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole pinned normal attachment primary required')
    return dict(csvid=csvid,mana_cost=record['mana_cost'])


def variants(shape):
    card = shape['csvid']
    return [f'{mode}_{p}' for mode in ALL_MODES[card] for p in ((0,) if (card,mode) in HUMAN_ONLY else (0,1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(shape))):
        raise ValueError('distinct supported attachment variants required')


def spec(declaration,record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned attachment primary required')
    shape = classify(record,pin['registry']['csvid'])
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not exact(declaration.get('params'),shape)):
        raise ValueError('exact declared attachment parameters required')
    validate_variants(shape,declaration.get('variants'))
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    pinned = profiles()
    if (set(pinned)!={'sources','witnesses'} or set(pinned['witnesses'])!=WITNESSES
            or {pin['registry']['csvid'] for pin in pinned['sources'].values()}!=ALL_MODES.keys()):
        raise ValueError('complete attachment source and witness pins required')
    for group,entries in pinned.items():
        for name,pin in entries.items():
            record,row = oracle.get(name),rows.get(name,{})
            if not exact(record,pin['oracle']) or not exact(row,pin['registry']):
                raise ValueError('attachment source or witness changed: '+name)
            if group=='sources':
                if row.get('kind')!='card' or row.get('handler') in (None,'0','NULL','card_dummy'):
                    raise ValueError('implemented attachment primary required')
                classify(record,row['csvid'])
            elif name==SPIRIT and (row['csvid']!=18829 or record['layout']!='token'):
                raise ValueError('exact Spirit token required')


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid']!=csvid or shape['csvid']!=csvid:
        raise ValueError('exact registered attachment source required')
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

