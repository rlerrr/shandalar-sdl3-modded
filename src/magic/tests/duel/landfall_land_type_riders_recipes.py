"""Observe subtype-specific Landfall through actual land plays and paid spells."""
from collections import Counter
import json
from pathlib import Path

from landfall_case_support import LandfallBoard, landfall_lines
from library_search_recipes import wave6_exact
from recipes import cost_from_oracle


FAMILY='landfall_land_type_riders'
PROFILE=Path(__file__).parent/'data/landfall_land_type_riders_wave8_profiles.json'
LANDFALL='Landfall — Whenever a land you control enters, '
RULES={
    ('Flying',LANDFALL+'this creature deals 1 damage to any target. If that land is a Mountain, this creature deals 2 damage instead.'):
        ('damage','Mountain','Taiga',('planeswalker','creature_damage','rejected_land')),
    ('Flying',LANDFALL+'you may return target nonland permanent card from your graveyard to your hand. '
                       'If that land is a Plains, you may return that nonland permanent card to the battlefield instead.'):
        ('return','Plains','Savannah',('matched_hand','no_legal_grave','noncreature')),
    ('Flying',LANDFALL+'tap target creature an opponent controls. '
                       "If that land is an Island, that creature doesn't untap during its controller's next untap step."):
        ('freeze','Island','Tropical Island',('no_foe','source_left','already_tapped')),
    ('Flying',LANDFALL+'other creatures you control get +1/+0 until end of turn. '
                       'If that land is a Swamp, those creatures get +2/+0 until end of turn instead.'):
        ('team','Swamp','Bayou',('expires','combat_boost','rejected_land')),
    ("Searing Blaze deals 1 damage to target player or planeswalker and 1 damage to target creature that player or that planeswalker's controller controls.",
     'Landfall — If you had a land enter the battlefield under your control this turn, Searing Blaze deals 3 damage to that player or planeswalker and 3 damage to that creature instead.'):
        ('blaze','Mountain','Taiga',('no_land','matched','different_land','foreign','next_turn','planeswalker',
                                   'self_pair','no_pair','vanished_creature','rejected_land')),
}
COMMON=('ordinary','matched','both_subtypes','foreign','before_source','flying_ground','flying_reach')
DUALS={'Taiga','Savannah','Tropical Island','Bayou'}
LANDS={'Forest','Mountain','Plains','Island','Swamp',*DUALS}
DECK=('Forest','Mountain','Plains','Island')
BEAR='Grizzly Bears'
REACH='Wall of Vines'
GOBLIN="Mons's Goblin Raiders"
RECOVER="Feldon's Cane"
WALKER='Jace Beleren'
BOUNCE='Unsummon'
DUMMIES={BEAR,REACH,GOBLIN,'Hill Giant'}
WITNESSES={*DUMMIES,*LANDS,RECOVER,WALKER,BOUNCE,'Fog'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    lines=tuple(landfall_lines(record))
    if lines not in RULES:
        raise ValueError('exact whole Landfall clauses required')
    effect,subtype,dual,modes=RULES[lines]
    spell=effect=='blaze'
    if (set(record['keywords'])!=({'Landfall'} if spell else {'Landfall','Flying'})
            or spell and record['type_line']!='Instant'
            or not spell and 'Creature' not in record['type_line']):
        raise ValueError('wrong complete source type or keywords')
    return dict(effect=effect,subtype=subtype,dual=dual,modes=list(modes),
                cast_price=cost_from_oracle(record),
                power=None if spell else int(record['power']),
                toughness=None if spell else int(record['toughness']))


def variants(shape):
    modes=tuple(shape['modes'])
    if shape['effect']!='blaze':
        modes=COMMON+modes
    outcomes=[f'{mode}_{seat}' for mode in modes for seat in (0,1)]
    return outcomes+(['decline_0'] if shape['effect']=='return' else [])


def spec(declaration,record):
    pinned=profiles()['sources'].get(record.get('name') if record else None)
    if not pinned or not wave6_exact(pinned['oracle'],record):
        raise ValueError('whole exact pinned Landfall Oracle record required')
    shape=classify(record)
    allowed=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not wave6_exact(declaration.get('params'),shape) or not isinstance(allowed,list)
            or not allowed or len(set(allowed))!=len(allowed) or set(allowed)-set(variants(shape))):
        raise ValueError('exact Landfall model and distinct reviewed scenarios required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    profile=profiles()
    if set(profile)!={'sources','witnesses'} or set(profile['witnesses'])!=WITNESSES:
        raise ValueError('missing independent native and Oracle witnesses')
    for group,pins in profile.items():
        for name,pin in pins.items():
            row,record=rows[name],oracle[name]
            if (row!=pin['registry'] or not wave6_exact(record,pin['oracle']) or row['kind']!='card'
                    or row['handler'] in (None,'NULL','0') or row['handler']=='card_dummy' and name not in DUMMIES):
                raise ValueError('native witness or whole Oracle changed '+name)
            if group=='sources':
                classify(record)


class Board(LandfallBoard):
    def build(self):
        self.effect=self.shape['effect']
        self.life=[20,20]
        self.lock=False
        self.recovered=False
        self.entered=False
        for seat in (0,1):
            for i,name in enumerate(DECK):
                self.add(name,f'library_{seat}_{i}',seat,'library')
        mode=self.mode
        if mode not in ('no_land','no_pair','flying_ground','flying_reach'):
            land=('Forest' if mode in ('ordinary','different_land') else self.shape['dual'] if mode=='both_subtypes' or
                  self.effect=='freeze' and mode=='source_left' else self.shape['subtype'])
            self.add(land,'land',self.foe if mode=='foreign' else self.owner,'hand')
        if mode=='rejected_land':
            self.add('Forest','second_land',self.owner,'hand')
        if mode in ('flying_ground','flying_reach'):
            self.add(REACH if mode=='flying_reach' else BEAR,'blocker',self.foe)
        if mode=='planeswalker':
            self.add(WALKER,'walker',self.foe)
            self.loyalty=int(self.oracle[WALKER]['loyalty'])
        if self.effect in ('team','freeze','blaze') and mode!='no_pair':
            self.add(BEAR,'ally',self.owner)
        if self.effect=='team':
            self.add(GOBLIN,'other_ally',self.owner)
            self.add(BEAR,'enemy',self.foe)
            self.add(RECOVER,'noncreature',self.owner)
        if self.effect in ('freeze','blaze') or mode=='creature_damage':
            if mode not in ('no_foe','no_pair','flying_ground','flying_reach'):
                self.add(BEAR,'victim',self.foe,tapped=mode=='already_tapped')
        if mode in ('source_left','vanished_creature'):
            self.add(BOUNCE,'bounce',self.foe,'hand')
        if self.effect=='return':
            if mode!='no_legal_grave':
                self.add(RECOVER if mode=='noncreature' else BEAR,'recovery',self.owner,'graveyard')
            self.add('Forest','invalid_land',self.owner,'graveyard')
            self.add('Fog','invalid_spell',self.owner,'graveyard')
            self.add('Hill Giant','foreign_grave',self.foe,'graveyard')

    def card_fact(self,ref,prop,value):
        self.emit('assert_card',ref=ref,property=prop,value=int(value))

    def facts(self):
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0)
        self.observe_mana()
        for seat in (0,1):
            self.emit('assert_life',player=seat,value=self.life[seat])
            for zone in ('battlefield','hand','graveyard','library','exile'):
                ids=[self.rows[item['name']]['csvid'] for item in self.cards.values() if item['player']==seat and item['zone']==zone]
                self.emit('assert_zone',player=seat,zone=zone,value=len(ids))
                if zone=='library':
                    for index,csvid in enumerate(ids):
                        self.emit('assert_zone_card',player=seat,zone=zone,index=index,value=csvid)
                else:
                    for csvid,n in sorted(Counter(ids).items()):
                        self.emit('assert_zone_multiplicity',player=seat,zone=zone,index=csvid,value=n)
        for ref,item in self.cards.items():
            if item['zone']!='battlefield':
                continue
            record=self.oracle[item['name']]
            if ref=='walker':
                self.emit('assert_counter',ref=ref,counter_kind='loyalty',value=self.loyalty)
            elif 'Creature' in record['type_line']:
                if ref=='recovery' and self.recovered:
                    self.emit('bind_permanent',ref=ref,player=self.owner,value=self.rows[item['name']]['csvid'])
                self.card_fact(ref,'power',int(record['power'])+item['boost'])
                self.card_fact(ref,'toughness',int(record['toughness']))
                self.card_fact(ref,'tapped',item['tapped'])
                self.card_fact(ref,'damage',item['damage'])

    def advance_turn(self):
        super().advance_turn()
        if self.lock and self.active==self.foe:
            victim=self.cards['victim']
            if victim['zone']=='battlefield':
                victim['tapped']=True
            self.lock=False
        self.entered=False

    def typed(self):
        subtype=self.oracle[self.cards['land']['name']]['type_line'].partition(' — ')[2].split()
        return self.shape['subtype'] in subtype

    def recovery_destination(self):
        return 'battlefield' if self.typed() and self.mode!='matched_hand' else 'hand'

    def trigger_choices(self,_):
        if self.effect=='damage':
            ref={'planeswalker':'walker','creature_damage':'victim'}.get(self.mode)
            if ref is None:
                self.emit('choose_player',player=self.owner,value=self.foe)
            else:
                self.emit('choose_target',ref=ref,player=self.owner)
        elif self.effect=='freeze' and 'victim' in self.cards:
            self.emit('choose_target',ref='victim',player=self.owner)
        elif self.effect=='return' and self.mode!='decline':
            if self.owner==0:
                self.emit('choose_trigger_source',ref='source',player=self.owner)
            if 'recovery' not in self.cards:
                return
            if self.typed():
                self.emit('choose_dialog',player=self.owner,value=int(self.recovery_destination()=='battlefield'))
            self.emit('choose_zone_card',player=self.owner,value=self.rows[self.cards['recovery']['name']]['csvid'])

    def hurt(self,ref,points):
        item=self.cards[ref]
        if item['zone']!='battlefield':
            return
        item['damage']+=points
        if item['damage']>=int(self.oracle[item['name']]['toughness']):
            item['zone']='graveyard'

    def reward(self):
        amount=1+int(self.typed())
        if self.effect=='damage':
            if self.mode=='planeswalker':
                self.loyalty-=amount
            elif self.mode=='creature_damage':
                self.hurt('victim',amount)
            else:
                self.life[self.foe]-=amount
        elif self.effect=='freeze' and 'victim' in self.cards:
            self.cards['victim']['tapped']=True
            self.lock=self.typed()
        elif self.effect=='team':
            for ref in ('ally','other_ally'):
                self.cards[ref]['boost']+=amount
        elif self.effect=='return' and self.mode!='decline' and 'recovery' in self.cards:
            self.cards['recovery']['zone']=self.recovery_destination()
            self.recovered=self.recovery_destination()=='battlefield'

    def complete_combat(self,attacker,blocker=None,*,blocks=False):
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=1)
        self.cards[attacker]['tapped']=True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker is not None:
            self.emit('block',ref=blocker,other=attacker,value=int(blocks))
        self.emit('advance_combat')
        self.card_fact(attacker,'blocked',blocks)
        self.emit('combat_damage')
        creature=self.cards[attacker]
        power=int(self.oracle[creature['name']]['power'])+creature['boost']
        if blocks:
            adversary=self.cards[blocker]
            block_power=int(self.oracle[adversary['name']]['power'])+adversary['boost']
            self.hurt(blocker,power)
            self.hurt(attacker,block_power)
        else:
            self.life[self.foe]-=power
        self.emit('resolve')
        self.emit('end_combat')
        self.facts()

    def bounce(self,recipient,*,response=False):
        self.cast('bounce',target=recipient,response=response)
        self.emit('resolve_response' if response else 'resolve')
        self.cards['bounce']['zone']='graveyard'
        self.cards[recipient]['zone']='hand'

    def landfall(self):
        mode=self.mode
        if mode=='before_source':
            self.emit('turn',player=self.owner,value=0)
            for seat in (0,1):
                self.fund(seat,[0]*6)
            self.play_land()
        self.play_source()
        self.facts()
        if mode=='before_source':
            return
        if mode in ('flying_ground','flying_reach'):
            self.advance_turn()
            self.advance_turn()
            self.complete_combat('source','blocker',blocks=mode=='flying_reach')
            return
        if mode=='foreign':
            self.advance_turn()
        self.play_land(choices=self.trigger_choices if mode!='foreign' else None)
        if mode!='foreign':
            self.reward()
        self.facts()
        if mode=='source_left':
            self.bounce('source')
            self.facts()
        if mode=='rejected_land':
            self.play_land('second_land',allowed=False)
            self.facts()
        if self.effect=='freeze' and mode!='no_foe':
            for _ in range(3 if self.active==self.owner else 2):
                self.advance_turn()
                self.facts()
        elif mode=='expires':
            self.advance_turn()
            self.facts()
        elif mode=='combat_boost':
            self.complete_combat('ally')

    def blaze(self):
        self.emit('turn',player=self.owner,value=0)
        for seat in (0,1):
            self.fund(seat,[0]*6)
        mode=self.mode
        self.facts()
        if mode=='foreign':
            self.advance_turn()
        if 'land' in self.cards:
            self.play_land()
            self.entered=self.active==self.owner
            self.facts()
        if mode=='next_turn':
            self.advance_turn()
        if mode=='rejected_land':
            self.play_land('second_land',allowed=False)
        if mode=='no_pair':
            self.fund(self.owner,self.shape['cast_price'])
            self.emit('require_can_cast',ref='source',value=0)
            self.emit('cast',ref='source',value=0)
            self.facts()
            return
        victim='ally' if mode=='self_pair' else 'victim'
        seat=self.owner if mode=='self_pair' else self.foe
        if mode=='planeswalker':
            self.emit('choose_target',player=self.owner,ref='walker')
        else:
            self.emit('choose_player',player=self.owner,value=seat)
        self.cast('source',target=victim)
        if mode=='vanished_creature':
            self.emit('announce_spell',ref='source')
            self.bounce(victim,response=True)
        else:
            self.emit('resolve')
        self.cards['source']['zone']='graveyard'
        points=3 if self.entered else 1
        if mode=='planeswalker':
            self.loyalty-=points
            if self.loyalty<=0:
                self.cards['walker']['zone']='graveyard'
        else:
            self.life[seat]-=points
        self.hurt(victim,points)
        self.facts()

    def run(self):
        if self.effect=='blaze':
            self.blaze()
        else:
            self.landfall()


def build(name,csvid,declaration,shape,variant,rows,oracle):
    mode,seat=variant.rsplit('_',1)
    world=Board(name,shape,mode,int(seat),rows,oracle)
    world.run()
    return dict(name=f'LandfallLandTypeRidersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=2298,
                setup=world.setup,steps=world.steps,
                _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                             oracle_text=declaration['oracle_text'],
                             untested_components=['Multiplayer, hacked basic types, and off-turn land-entry replacements.']))


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid']!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(shape)):
        raise ValueError('distinct reviewed Landfall cases require the exact native source')
    validate_witnesses(oracle,rows)
    return [build(name,csvid,declaration,shape,variant,rows,oracle) for variant in selected]
