"""Freeze four Oracle-pinned entry choice and life pilots without launching a runtime."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'entry_interaction_choice_and_life'
PROFILE = Path(__file__).with_name('data') / 'entry_interaction_choice_and_life_wave8_profiles.json'

FANATIC, PHOENIX, EXARCH, GUIDE = 13703,13713,11619,14507
PREFIX, SEED = 'EntryInteractionChoiceAndLifeRecipe',3404
MODES = {
    FANATIC: ('tribute_paid','trample','blink_declined'),
    PHOENIX: ('tribute_paid','declined_death','declined_flight','borrowed_death'),
    EXARCH: ('gain','drain','protected_opponent','alternate_reentry'),
    GUIDE: ('counter','find_snow','fail_to_find','draw_found'),
}
HAND,BATTLE,LIBRARY,GRAVE = range(4)
ZERO = (0,)*6
BEAR,CAD,BLINK,MURDER,CONTROL = 'Grizzly Bears','Eager Cadet','Cloudshift','Murder','Control Magic'
LEYLINE,SNOW,PLAINS,DUAL = 'Leyline of Sanctity','Snow-Covered Forest','Plains','Tropical Island'
WITNESSES = {BEAR,CAD,BLINK,MURDER,CONTROL,LEYLINE,SNOW,PLAINS,DUAL}


def label(csvid,mode,seat):
    return f'{PREFIX}_{csvid}_{mode}_{seat}'


class Board:
    def __init__(self, row, mode, owner, rows, oracle):
        self.id,self.name = row['csvid'],row['name']
        self.mode,self.owner,self.foe,self.active = mode,owner,1-owner,owner
        self.rows,self.oracle = rows,oracle
        self.setup,self.steps,self.cards = [],[],{}
        self.mana,self.life = [ZERO,ZERO],[20,20]
        self.add('source',self.name,owner,HAND)
        self.prepare()

    def add(self, ref, name, owner, zone=BATTLE, *, tapped=False):
        assert ref not in self.cards
        self.setup.append((ref,owner,zone,self.rows[name]['csvid'],int(tapped)))
        self.cards[ref] = [name,owner,zone]

    def prepare(self):
        p,q,m = self.owner,self.foe,self.mode
        if self.id == FANATIC:
            if m=='tribute_paid':
                self.add('attacker',CAD,p)
            elif m=='trample':
                self.add('blocker',BEAR,q)
            elif m=='blink_declined':
                self.add('blink',BLINK,p,HAND)
        elif self.id == PHOENIX:
            if m=='declined_flight':
                self.add('blocker',BEAR,q)
            else:
                self.add('removal',MURDER,p if m=='borrowed_death' else q,HAND)
                if m=='borrowed_death':
                    self.add('steal',CONTROL,q,HAND)
                elif m=='tribute_paid':
                    self.add('attacker',CAD,p)
        elif self.id == EXARCH:
            if m=='protected_opponent':
                self.add('protected',LEYLINE,q)
            elif m=='alternate_reentry':
                self.add('blink',BLINK,p,HAND)
        elif self.id == GUIDE:
            deck = (BEAR,DUAL) if m=='fail_to_find' else (BEAR,DUAL,SNOW,PLAINS)
            for name,ref in zip(deck,('library_bear','library_dual','library_snow','library_plains')):
                self.add(ref,name,p,LIBRARY)

    def emit(self, op, ref='', other='', *, prop='', player=0, value=0, zone=HAND, index=0, color=0):
        self.steps.append((op,ref,other,prop,player,value,zone,index,color))

    def see(self, ref, prop, value):
        self.emit('assert_card',ref,prop=prop,value=value)

    def stats(self, ref, amount):
        for prop in ('power','toughness'):
            self.see(ref,prop,amount)

    def pay_setup(self, player, pool):
        self.mana[player] = tuple(pool)
        for color,count in enumerate(pool):
            self.emit('set_mana',player=player,color=color,value=count)
        self.emit('refresh')

    def wallet(self, player):
        for color,count in enumerate(self.mana[player]):
            self.emit('assert_mana',player=player,color=color,value=count)

    def state(self):
        self.emit('assert_stack')
        for player in (0,1):
            self.wallet(player)
            self.emit('assert_life',player=player,value=self.life[player])
            for zone in (HAND,BATTLE,LIBRARY,GRAVE):
                counts = Counter(name for name,p,z in self.cards.values() if (p,z)==(player,zone))
                self.emit('assert_zone',player=player,zone=zone,value=counts.total())
                for name,count in sorted(counts.items()):
                    self.emit('assert_zone_multiplicity',player=player,zone=zone,value=count,index=self.rows[name]['csvid'])

    def settle(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices')
        self.emit('assert_stack')

    def cast(self, ref, *, target=None, decisions=(), resolution=(), budget=None, leftover=ZERO):
        name,actor,zone = self.cards[ref]
        assert zone==HAND
        self.pay_setup(actor,cost_from_oracle(self.oracle[name]) if budget is None else budget)
        if target:
            self.emit('choose_target',target,player=actor)
        self.emit('cast',ref,value=1)
        self.emit('assert_unused_choices')
        self.emit('assert_stack',value=1)
        self.mana[actor] = tuple(leftover)
        self.wallet(actor)
        for op,player,value in decisions:
            self.emit(op,player=player,value=value)
        for op,selected,player,value in resolution:
            self.emit(op,selected,player=player,value=value)
        self.settle()
        self.cards[ref][2] = GRAVE if self.oracle[name]['type_line']=='Instant' else BATTLE

    def bind(self, ref, controller):
        name,_,zone = self.cards[ref]
        assert zone==BATTLE
        self.cards[ref][1] = controller
        self.emit('bind_permanent_copy',ref,player=controller,value=self.rows[name]['csvid'])

    def visit(self, actor, *, through_draw=False):
        assert actor==1-self.active and self.mana==[ZERO,ZERO]
        self.emit('advance_turn')
        self.emit('untap_step',player=actor)
        if through_draw:
            self.emit('upkeep_step',player=actor)
            self.emit('draw_step',player=actor)
            self.settle()
        else:
            self.emit('turn',player=actor)
        self.active=actor

    def combat(self, attacker, *, denied='', block=None, hit, blocker_dies=False):
        self.emit('begin_attackers')
        if denied:
            self.emit('attack',denied,value=0)
        self.emit('attack',attacker,value=1)
        self.see(attacker,'tapped',1)
        self.emit('finish_attackers')
        self.settle()
        self.emit('begin_blockers')
        if block is not None:
            self.emit('block','blocker',attacker,value=block)
        self.emit('advance_combat')
        self.settle()
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')
        if blocker_dies:
            self.cards['blocker'][2]=GRAVE
        self.life[1-self.active]-=hit
        self.state()

    def tribute(self, paid):
        return (('choose_dialog',self.foe,int(not paid)),)

    def fanatic(self):
        p,m = self.owner,self.mode
        paid = m in ('tribute_paid','blink_declined')
        self.emit('present_ai_dialogs',value=1)
        self.cast('source',decisions=self.tribute(paid))
        self.stats('source',4)
        self.see('source','plus1_counters',int(paid))
        self.state()
        if m=='tribute_paid':
            self.combat('attacker',denied='source',hit=1)
        elif m=='trample':
            self.combat('source',block=1,hit=2,blocker_dies=True)
            self.see('source','damage',2)
        else:
            self.cast('blink',target='source',decisions=self.tribute(False))
            self.bind('source',p)
            self.stats('source',4)
            self.see('source','plus1_counters',0)
            self.combat('source',hit=4)
        self.emit('cleanup')
        self.settle()
        self.stats('source',4 if m=='tribute_paid' else 3)
        self.see('source','plus1_counters',int(m=='tribute_paid'))
        self.see('source','damage',0)
        self.state()

    def phoenix(self):
        p,q,m = self.owner,self.foe,self.mode
        paid = m=='tribute_paid'
        self.emit('present_ai_dialogs',value=1)
        self.cast('source',decisions=self.tribute(paid))
        self.stats('source',5 if paid else 3)
        self.see('source','plus1_counters',2 if paid else 0)
        self.state()
        if m=='declined_flight':
            self.combat('source',block=0,hit=3)
            return
        if m=='tribute_paid':
            self.combat('attacker',denied='source',hit=1)
        elif m=='borrowed_death':
            self.visit(q)
            self.cast('steal',target='source')
            self.bind('source',q)
            self.state()
        self.cast('removal',target='source')
        self.cards['source'][1:] = [p,GRAVE if paid else HAND]
        if m=='borrowed_death':
            self.cards['steal'][2]=GRAVE
        self.state()

    def exarch(self):
        p,q,m = self.owner,self.foe,self.mode
        if m!='protected_opponent':
            self.emit('present_ai_dialogs',value=1)
        # The sole legal mode auto-chooses for Human but is still presented to AI.
        choices = () if m=='protected_opponent' and p==0 else (('choose_dialog',p,int(m=='drain')),)
        self.cast('source',decisions=choices)
        self.stats('source',2)
        if m=='drain':
            self.life[q] -= 2
        else:
            self.life[p] += 2
        self.state()
        if m=='alternate_reentry':
            self.cast('blink',target='source',decisions=(('choose_dialog',p,1),))
            self.bind('source',p)
            self.life[q] -= 2
            self.stats('source',2)
            self.state()

    def guide(self):
        p,m = self.owner,self.mode
        counter = m=='counter'
        self.emit('present_ai_dialogs',value=1)
        decisions = (('choose_dialog',p,int(not counter)),)
        found = m in ('find_snow','draw_found')
        if found:
            self.emit('observe_search_filters',value=1)
            decisions += (('choose_zone_card',p,self.rows[SNOW]['csvid']),)
        elif m=='fail_to_find':
            decisions += (('choose_optional_zone',p,self.rows[DUAL]['csvid']),)
        self.cast('source',decisions=decisions)
        self.see('source','plus1_counters',int(counter))
        self.stats('source',2 if counter else 1)
        self.emit('assert_shuffle_count',player=p,value=int(not counter))
        if found:
            for name,legal in ((SNOW,1),(PLAINS,1),(BEAR,0),(DUAL,0)):
                self.emit('assert_offered_legal_card',index=self.rows[name]['csvid'],value=legal)
            self.emit('observe_search_filters',value=0)
            self.emit('assert_zone_card',player=p,zone=LIBRARY,value=self.rows[SNOW]['csvid'])
        self.state()
        if m=='draw_found':
            self.visit(self.foe)
            self.visit(p,through_draw=True)
            self.cards['library_snow'][2] = HAND
            self.state()

    def run(self):
        self.emit('turn',player=self.owner)
        self.pay_setup(self.foe,ZERO)
        self.pay_setup(self.owner,ZERO)
        {FANATIC:self.fanatic,PHOENIX:self.phoenix,EXARCH:self.exarch,GUIDE:self.guide}[self.id]()


ORACLE_BONES, BRUTE, TYRANT, OUTCAST = 13772,13823,15703,14619
SENTINEL, SWORDSMITH, JOKUL, SELKIE = 3001,6798,8391,9866
PEEMA, CONSULATE, REVOLUTIONARY, MARE = 16765,16326,16747,19004
FOLLOW_MODES = {
    ORACLE_BONES: ('tribute_paid','free_fog','decline_spell'),
    BRUTE: ('tribute_paid','trample'),
    TYRANT: ('normal_flying','surged_creature','surged_player','no_cast_before'),
    OUTCAST: ('counter','spirit'),
    SENTINEL: ('paid_fetch','no_extra_mana','fail_to_find'),
    SWORDSMITH: ('zero_x','paid_x','decline_search'),
    JOKUL: ('short_lands','island_untaps','opponent_island'),
    SELKIE: ('both_types','forests_only','foe_types'),
    PEEMA: ('self_power','greatest_power','force_block'),
    CONSULATE: ('entry_energy','prevent_packet','unprevented'),
    REVOLUTIONARY: ('permanent_on_entry','permanent_on_death','player_energy','trample'),
    MARE: ('own_spell','foe_spell','foe_ability','red_block','nonred_block'),
}
ALL_MODES = MODES | FOLLOW_MODES
HUMAN_ONLY = {(SWORDSMITH,'decline_search')}
FOG, MEMNITE, SPIRIT, FOREST = 'Fog','Memnite','Spirit','Forest'
ISLAND, MOUNTAIN, SWAMP = 'Island','Mountain','Swamp'
BONE_SAW, BONESPLITTER, MORNINGSTAR, RELIC = 'Bone Saw','Bonesplitter','Vulshok Morningstar','Darksteel Relic'
WURM, GIANT, PIKER, SPIDER = 'Craw Wurm','Hill Giant','Goblin Piker','Giant Spider'
HUNGER, PUZZLE, GROWTH, ICY = 'Hunger of the Howlpack',"Woodweaver's Puzzleknot",'Giant Growth','Icy Manipulator'
WITNESSES |= {FOG,MEMNITE,SPIRIT,FOREST,ISLAND,MOUNTAIN,SWAMP,BONE_SAW,BONESPLITTER,MORNINGSTAR,
              RELIC,WURM,GIANT,PIKER,SPIDER,HUNGER,PUZZLE,GROWTH,ICY}
GAPS = {
    GUIDE: ['No stable reveal-count observer. Actual inspected legality, shuffle, exact top card and later draw are observed.'],
    TYRANT: ['Two-player duels have no teammate. Surge requires a real earlier same-turn cast by the source’s controller.'],
    SENTINEL: ['The test pays once from exact remaining mana. Recruited Sentinels have no mana left for another search.'],
    SWORDSMITH: ['The Human can decline the optional ETB outright; the AI normally accepts it. Both seats exercise real X payment and the Equipment threshold.'],
    JOKUL: ['The duel input cannot directly inspect the temporary sacrifice marking before all five legal selections commit. It observes all five graveyard moves and the actual non-untap/Island behavior.'],
    CONSULATE: ['The native damage-window API can select the current single-source damage packet. It cannot independently schedule two damage packets from the same source in the same turn after that prevention choice.'],
}


class FollowBoard(Board):
    def prepare(self):
        p,q,m = self.owner,self.foe,self.mode
        if self.id in (ORACLE_BONES,BRUTE):
            if self.id==ORACLE_BONES:
                self.add('fog',FOG,p,HAND)
            elif m=='tribute_paid':
                self.add('attacker',CAD,p)
            else:
                self.add('blocker',BEAR,q)
        elif self.id==TYRANT:
            if m.startswith('surged'):
                self.add('previous',MEMNITE,p,HAND)
            if m!='no_cast_before':
                self.add('blocker',BEAR,q)
        elif self.id==OUTCAST and m=='spirit':
            self.add('blocker',BEAR,q)
        elif self.id in (SENTINEL,SWORDSMITH):
            deck = ((BEAR,self.name,FOREST) if m!='fail_to_find' else (BEAR,FOREST)) if self.id==SENTINEL else (
                    RELIC,BONE_SAW,BONESPLITTER,MORNINGSTAR)
            for i,name in enumerate(deck):
                self.add(f'deck_{i}',name,p,LIBRARY)
        elif self.id==JOKUL:
            lands = (FOREST,PLAINS,MOUNTAIN,SWAMP,DUAL)
            for i,name in enumerate(lands[:4] if m=='short_lands' else lands):
                self.add(f'sac_{i}',name,p,tapped=True)
            self.add('island',ISLAND,q if m=='opponent_island' else p,HAND)
            if m=='island_untaps':
                self.add('blocker',BEAR,q)
        elif self.id==SELKIE:
            names = (FOREST,DUAL,ISLAND) if m!='forests_only' else (FOREST,DUAL)
            for i,name in enumerate(names):
                self.add(f'land_{i}',name,q if m=='foe_types' else p,tapped=True)
            self.add('victim',BEAR,q,tapped=True)
            self.add('untapped_witness',GIANT,q)
        elif self.id==PEEMA:
            self.add('biggest',WURM,p if m=='greatest_power' else q)
            if m=='force_block':
                self.add('attacker',BEAR,p)
        elif self.id==CONSULATE and m!='entry_energy':
            self.add('attacker',BEAR,q)
        elif self.id==REVOLUTIONARY:
            if m.startswith('permanent_'):
                self.add('witness',BEAR,p)
                self.add('grow',HUNGER,p,HAND)
            if m=='permanent_on_death':
                self.add('murder',MURDER,q,HAND)
            elif m=='player_energy':
                self.add('seed',PUZZLE,p,HAND)
            elif m=='trample':
                self.add('blocker',CAD,q)
        elif self.id==MARE:
            if m.endswith('_spell'):
                self.add('grow',GROWTH,p if m=='own_spell' else q,HAND)
            elif m=='foe_ability':
                self.add('icy',ICY,q)
            elif m in ('red_block','nonred_block'):
                self.add('blocker',PIKER if m=='red_block' else BEAR,q)

    def attack(self, attacker, *, denied=None):
        self.emit('begin_attackers')
        if denied:
            self.emit('attack',denied,value=0)
        self.emit('attack',attacker,value=1)
        self.see(attacker,'tapped',1)
        self.emit('finish_attackers')
        self.settle()

    def finish_damage(self, hit, *, dead=()):
        self.emit('combat_damage')
        self.settle()
        self.emit('end_combat')
        self.life[1-self.active]-=hit
        for ref in dead:
            self.cards[ref][2]=GRAVE
        self.state()

    def new_tribute(self):
        p,m = self.owner,self.mode
        paid = m=='tribute_paid'
        self.emit('present_ai_dialogs',value=1)
        follow = ()
        if self.id==ORACLE_BONES and not paid:
            op = 'choose_target' if m=='free_fog' else 'choose_optional_target'
            follow = ((op,'fog',p,0),)
        self.cast('source',decisions=self.tribute(paid),resolution=follow)
        if self.id==ORACLE_BONES:
            self.see('source','plus1_counters',2 if paid else 0)
            self.see('source','power',5 if paid else 3)
            self.see('source','toughness',3 if paid else 1)
            if m=='free_fog':
                self.cards['fog'][2]=GRAVE
            if m=='decline_spell':
                self.emit('assert_forced_target_choices')
            self.state()
            self.combat('source',hit=5 if paid else 0 if m=='free_fog' else 3)
        else:
            self.stats('source',8 if paid else 5)
            self.see('source','plus1_counters',3 if paid else 0)
            self.state()
            if paid:
                self.combat('attacker',denied='source',hit=1)
            else:
                self.combat('source',block=1,hit=3,blocker_dies=True)

    def tyrant(self):
        p,q,m = self.owner,self.foe,self.mode
        if m=='no_cast_before':
            self.pay_setup(p,(3,0,0,0,2,0))
            self.emit('cast','source',value=0)
            self.state()
            return
        surged = m.startswith('surged')
        if surged:
            self.cast('previous',budget=ZERO)
        choice = (('choose_target','blocker',p,0),) if m=='surged_creature' else (
                 (('choose_player','',p,q),) if m=='surged_player' else ())
        self.cast('source',budget=(3,0,0,0,2,0) if surged else None,resolution=choice)
        if m=='surged_creature':
            self.cards['blocker'][2]=GRAVE
        elif m=='surged_player':
            self.life[q]-=3
        self.see('source','power',5)
        self.see('source','toughness',4)
        self.state()
        if m=='normal_flying':
            self.visit(q)
            self.visit(p)
            self.combat('source',block=0,hit=5)

    def outcast(self):
        p,q,m = self.owner,self.foe,self.mode
        spirit = m=='spirit'
        self.emit('present_ai_dialogs',value=1)
        self.cast('source',decisions=(('choose_dialog',p,int(spirit)),))
        self.see('source','plus1_counters',int(not spirit))
        self.see('source','power',2 if spirit else 3)
        self.see('source','toughness',1 if spirit else 2)
        if spirit:
            csvid = self.rows[SPIRIT]['csvid']
            self.emit('bind_permanent_copy','spirit',player=p,value=csvid)
            self.cards['spirit'] = [SPIRIT,p,BATTLE]
            for prop,value in (('token',1),('color',32),('subtype_Spirit',1),('power',1),('toughness',1)):
                self.emit('assert_matching_permanents',prop=prop,player=p,value=value,index=csvid)
        self.state()
        if spirit:
            self.visit(q)
            self.visit(p)
            self.combat('spirit',block=0,hit=1)

    def paid_search(self):
        p,m = self.owner,self.mode
        sentinel = self.id==SENTINEL
        limits = {'zero_x':0,'paid_x':1}
        enabled = m not in ('no_extra_mana','decline_search')
        if sentinel:
            base = (2,0,0,1,0,0)
            extra = (1,0,0,1,0,0) if enabled else ZERO
            found = 'deck_1' if m=='paid_fetch' else None
            expected_filter = {ref:int(card[0]==self.name)
                               for ref,card in self.cards.items() if card[2]==LIBRARY}
        else:
            base = (3,0,0,0,0,1)
            extra = (limits.get(m,0),0,0,0,0,0)
            found = {'zero_x':'deck_1','paid_x':'deck_2'}.get(m)
            expected_filter = {'deck_0':0,'deck_1':1,'deck_2':int(m=='paid_x'),'deck_3':0}
        resolution = (('choose_trigger','source',p,0),) if p==0 and enabled else ()
        if enabled and not sentinel:
            resolution += (('choose_x','',p,limits[m]),)
        if found:
            self.emit('observe_search_filters',value=1)
            resolution += (('choose_zone_card','',p,self.rows[self.cards[found][0]]['csvid']),)
        elif m=='fail_to_find':
            resolution += (('choose_optional_zone','',p,self.rows[BEAR]['csvid']),)
        self.cast('source',budget=tuple(a+b for a,b in zip(base,extra)),leftover=extra,
                  resolution=resolution)
        self.mana[p] = ZERO
        self.emit('assert_shuffle_count',player=p,value=int(enabled))
        if found:
            for ref,eligible in expected_filter.items():
                self.emit('assert_offered_legal_card',index=self.rows[self.cards[ref][0]]['csvid'],value=eligible)
            self.emit('observe_search_filters',value=0)
            name = self.cards[found][0]
            self.cards[found][2] = BATTLE
            self.emit('bind_permanent_copy',found,player=p,index=int(sentinel),value=self.rows[name]['csvid'])
            self.see(found,'in_play',1)
        self.see('source','power',2)
        self.see('source','toughness',3)
        self.state()

    def jokul(self):
        p,q,m = self.owner,self.foe,self.mode
        short = m=='short_lands'
        lands = tuple(ref for ref in self.cards if ref.startswith('sac_'))
        resolution = tuple(('choose_target',ref,p,0) for ref in lands) if p==0 and not short else ()
        self.cast('source',resolution=resolution)
        if short:
            self.cards['source'][2]=GRAVE
            self.state()
            return
        for ref in lands:
            self.cards[ref][2]=GRAVE
        self.stats('source',12)
        self.see('source','tapped',1)
        self.state()
        self.visit(q)
        if m=='opponent_island':
            self.emit('play_land','island',value=1)
            self.cards['island'][2]=BATTLE
            self.settle()
            self.see('source','tapped',1)
        self.visit(p)
        self.see('source','tapped',1)
        if m=='island_untaps':
            if p==0:
                self.emit('choose_trigger','source',player=p)
            self.emit('play_land','island',value=1)
            self.cards['island'][2]=BATTLE
            self.settle()
            self.see('source','tapped',0)
            self.combat('source',block=1,hit=10,blocker_dies=True)
        self.state()

    def selkie(self):
        p,m = self.owner,self.mode
        gain = m!='foe_types'
        bounce = m=='both_types'
        resolution = (('choose_trigger','source',p,0),) if p==0 and gain else ()
        if bounce:
            resolution += (('choose_target','victim',p,0),)
        if p==0 and gain:
            resolution += (('choose_dialog','',p,0),)
        self.cast('source',budget=(2,0,0,1,0,0),resolution=resolution)
        if gain:
            self.life[p]+=3
        if bounce:
            self.cards['victim'][2]=HAND
        self.stats('source',2)
        self.state()

    def peema(self):
        p,q,m = self.owner,self.foe,self.mode
        self.cast('source')
        self.emit('assert_energy',player=p,value=6 if m=='greatest_power' else 3)
        self.emit('assert_energy',player=q,value=0)
        self.state()
        if m!='force_block':
            return
        self.emit('choose_target','biggest',player=p)
        self.emit('activate','source',value=1)
        self.settle()
        self.emit('assert_energy',player=p,value=0)
        self.attack('attacker')
        self.emit('begin_blockers')
        self.emit('choose_target','attacker',player=q)
        self.emit('advance_combat')
        self.emit('assert_unused_choices')
        self.see('attacker','blocked',1)
        self.see('biggest','blocking_count',1)
        self.finish_damage(0,dead=('attacker',))

    def consulate(self):
        p,q,m = self.owner,self.foe,self.mode
        self.cast('source')
        self.emit('assert_energy',player=p,value=4)
        self.emit('assert_energy',player=q,value=0)
        self.emit('can_activate','source',value=0)
        self.state()
        if m=='entry_energy':
            return
        self.visit(q)
        self.attack('attacker')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        if m=='prevent_packet':
            self.emit('choose_priority','source')
            self.emit('choose_damage_player',player=p,value=p)
        self.finish_damage(0 if m=='prevent_packet' else 2)
        self.emit('assert_energy',player=p,value=2 if m=='prevent_packet' else 4)

    def revolutionary(self):
        p,q,m = self.owner,self.foe,self.mode
        if m.startswith('permanent_'):
            self.cast('grow',target='witness')
            self.see('witness','plus1_counters',1)
            target = (('choose_target','witness',p,0),)
        elif m=='player_energy':
            self.cast('seed')
            self.life[p]+=3
            self.emit('assert_energy',player=p,value=3)
            target = (('choose_player','',p,p),)
        else:
            target = (('choose_target','blocker',p,0),)
        self.cast('source',resolution=target)
        if m.startswith('permanent_'):
            self.see('witness','plus1_counters',2)
            self.stats('witness',4)
        elif m=='player_energy':
            self.emit('assert_energy',player=p,value=4)
        self.stats('source',3)
        self.state()
        if m=='permanent_on_death':
            self.cast('murder',target='source',resolution=target)
            self.cards['source'][2]=GRAVE
            self.see('witness','plus1_counters',3)
            self.stats('witness',5)
            self.state()
        elif m=='trample':
            self.visit(q)
            self.visit(p)
            self.combat('source',block=1,hit=2,blocker_dies=True)

    def mare(self):
        p,q,m = self.owner,self.foe,self.mode
        self.cast('source')
        self.life[p]+=3
        self.state()
        if m.endswith('_spell'):
            self.cast('grow',target='source')
            self.life[p]+=3*int(m=='foe_spell')
            self.see('source','power',5)
            self.see('source','toughness',6)
        elif m=='foe_ability':
            self.visit(q)
            self.pay_setup(q,(1,0,0,0,0,0))
            self.emit('choose_target','source',player=q)
            self.emit('activate','icy',value=1)
            self.settle()
            self.mana[q] = ZERO
            self.life[p]+=3
            self.see('source','tapped',1)
        elif m in ('red_block','nonred_block'):
            self.visit(q)
            self.visit(p)
            legal = m=='nonred_block'
            self.combat('source',block=int(legal),hit=0 if legal else 2,blocker_dies=legal)
        self.state()

    def run(self):
        self.emit('turn',player=self.owner)
        self.pay_setup(self.foe,ZERO)
        self.pay_setup(self.owner,ZERO)
        actions = {ORACLE_BONES:self.new_tribute,BRUTE:self.new_tribute,TYRANT:self.tyrant,
                   OUTCAST:self.outcast,SENTINEL:self.paid_search,SWORDSMITH:self.paid_search,JOKUL:self.jokul,
                   SELKIE:self.selkie,PEEMA:self.peema,CONSULATE:self.consulate,REVOLUTIONARY:self.revolutionary,MARE:self.mare}
        actions[self.id]()


ZONES = ('hand','battlefield','library','graveyard')
ZONE_OPS = {'assert_zone','assert_zone_multiplicity','assert_zone_card'}
INDEXED = {'assert_zone_multiplicity','assert_zone_card','assert_offered_legal_card',
           'assert_matching_permanents','bind_permanent_copy'}
SEATED = {'turn','untap_step','upkeep_step','draw_step','choose_target','choose_optional_target',
          'choose_dialog','choose_trigger','choose_x','choose_player','choose_zone_card','choose_optional_zone',
          'choose_damage_player','set_mana','assert_mana','assert_life','assert_energy','assert_shuffle_count',
          'bind_permanent_copy','assert_matching_permanents'} | ZONE_OPS
VALUED = {'cast','activate','can_activate','play_land','choose_dialog','choose_player','choose_damage_player',
          'choose_x','choose_zone_card','choose_optional_zone','present_ai_dialogs','observe_search_filters',
          'assert_offered_legal_card','assert_shuffle_count','assert_unused_choices','assert_forced_target_choices',
          'set_mana','assert_mana','assert_life','assert_energy','assert_stack','assert_card',
          'assert_matching_permanents','bind_permanent_copy','attack','block'} | ZONE_OPS


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
        raise ValueError('whole pinned normal entry interaction primary required')
    return dict(csvid=csvid,mana_cost=record['mana_cost'])


def variants(shape):
    card = shape['csvid']
    return [f'{mode}_{seat}' for mode in ALL_MODES[card]
            for seat in ((0,) if (card,mode) in HUMAN_ONLY else (0,1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(shape))):
        raise ValueError('distinct supported entry interaction variants required')


def spec(declaration,record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned entry interaction primary required')
    shape = classify(record,pin['registry']['csvid'])
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not exact(declaration.get('params'),shape)):
        raise ValueError('exact declared entry interaction parameters required')
    validate_variants(shape,declaration.get('variants'))
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    pinned = profiles()
    if (set(pinned)!={'sources','witnesses'} or set(pinned['witnesses'])!=WITNESSES
            or {pin['registry']['csvid'] for pin in pinned['sources'].values()}!=ALL_MODES.keys()):
        raise ValueError('complete entry interaction source and witness pins required')
    for group,entries in pinned.items():
        for name,pin in entries.items():
            record,row = oracle.get(name),rows.get(name,{})
            if not exact(record,pin['oracle']) or not exact(row,pin['registry']):
                raise ValueError('entry interaction source or witness changed: '+name)
            if group=='sources':
                if row.get('kind')!='card' or row.get('handler') in (None,'0','NULL','card_dummy'):
                    raise ValueError('implemented entry interaction primary required')
                classify(record,row['csvid'])
            elif name==SPIRIT and (row['csvid']!=18829 or record['layout']!='token'):
                raise ValueError('exact physical Spirit token required')


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid']!=csvid or shape['csvid']!=csvid:
        raise ValueError('exact registered entry interaction source required')
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

