"""Paid Level Up with independently observed resource and combat riders."""

import copy
import json
from pathlib import Path

from level_up_case_support import LevelUpBoard, classify_leveler
from library_search_recipes import wave6_exact


FAMILY = 'level_resource_riders'
PROFILE = Path(__file__).parent / 'data/level_resource_riders_wave8_profiles.json'
PREVENT = ('If a source would deal damage to you or a creature you control, prevent {} of that damage.')
RIDERS = {
    (('Flying',),('Flying','Other Merfolk creatures you control get +1/+1.')):
        ('merfolk_lord','coralhelm_commander',['Flying','Level Up']),
    ((PREVENT.format(1),),(PREVENT.format(2),)):
        ('prevent','hedron_field_purists',['Level Up']),
    (('{T}: Add {G}{G}.',),('Elves you control have "{T}: Add {G}{G}."',)):
        ('elf_mana','joraga_treespeaker',['Level Up']),
    (('Other creatures you control get +1/+1.',),('Other creatures you control get +2/+2.',)):
        ('lord','kabira_vindicator',['Level Up']),
    ((),("At the beginning of each end step, if it's not your turn, take an extra turn after this one.",)):
        ('time','lighthouse_chronologist',['Level Up']),
    ((),('Whenever this creature attacks, it deals 6 damage to each creature defending player controls.',)):
        ('sweep','lord_of_shatterskull_pass',['Level Up']),
}
SCENES = {
    'merfolk_lord':('before_tier','fly','fly_blocked','lord_removed','replay'),
    'prevent':('base','one_per_packet','two_per_packet','combat','walker','replay'),
    'elf_mana':('self_mana','mana_proxy','mana_bearer','foreign_only','fresh','replay'),
    'lord':('before_tier','tier_one','tier_two','lord_removed','replay'),
    'time':('before_tier','turn_grant','own_end','removed_timer','replay'),
    'sweep':('before_tier','attack_trigger','ally_attacks','source_blocks','replay'),
}
COMMON = ('short','opponent_timing')
WITNESSES = {'Grizzly Bears','Merfolk of the Pearl Trident','Horned Turtle','Wind Drake','Lightning Bolt',
             'Jace Beleren','Elvish Warrior','Naturalize','Expedition Map','Dazzling Ramparts','Island'}
DUMMIES = {'Grizzly Bears','Merfolk of the Pearl Trident','Horned Turtle','Wind Drake','Elvish Warrior'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def _rider(shape):
    return RIDERS.get((tuple(shape['first_riders']),tuple(shape['second_riders'])))


def classify(record):
    shape=classify_leveler(record)
    rider=_rider(shape)
    if not rider or record['keywords']!=rider[2]:
        raise ValueError('all printed resource rider clauses and keywords required')
    return dict(shape,rider=rider[0])


def handler(record):
    return 'manalinkeh_card_'+_rider(classify(record))[1]


def variants(shape):
    return [f'{scene}_{seat}' for scene in COMMON+SCENES[shape['rider']] for seat in (0,1)]


def spec(declaration,record):
    shape=classify(record)
    pin=profiles()['sources'].get(record['name'])
    chosen=declaration.get('variants')
    if (not pin or not wave6_exact(pin['oracle'],record) or declaration.get('family')!=FAMILY
            or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not wave6_exact(declaration.get('params'),shape)
            or type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(set(chosen))!=len(chosen) or set(chosen)-set(variants(shape))):
        raise ValueError('exact printed Level Up bands, costs, riders and valid variants required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    data=profiles()
    if data.get('version')!=1 or not data.get('sources') or set(data.get('witnesses',()))!=WITNESSES:
        raise ValueError('complete reviewed leveler witnesses required')
    for group in ('sources','witnesses'):
        for name,pin in data[group].items():
            row,record=rows[name],oracle[name]
            if (not wave6_exact(pin,dict(oracle=record,registry=row)) or row['kind']!='card'
                    or record['layout']!=('leveler' if group=='sources' else 'normal')
                    or row['handler'] in (None,'NULL','0') or row['handler']=='card_dummy' and name not in DUMMIES):
                raise ValueError(f'changed leveler or witness: {name}')
            if group=='sources' and row['handler']!=handler(record):
                raise ValueError(f'changed reviewed leveler handler: {name}')


class Riders(LevelUpBoard):
    def __init__(self,name,shape,mode,owner,rows,oracle):
        self.life=[20,20]
        self.active=owner
        super().__init__(name,shape,mode,owner,rows,oracle)

    def build(self):
        mode,rider,owner,foe=self.mode,self.shape['rider'],self.owner,self.foe
        if mode in COMMON:
            return
        if rider in ('merfolk_lord','lord'):
            self.add('Merfolk of the Pearl Trident','ally',owner)
            self.add('Merfolk of the Pearl Trident','foreign',foe)
            self.add('Grizzly Bears','different',owner)
            if rider=='merfolk_lord' and mode in ('before_tier','fly','fly_blocked'):
                self.add('Wind Drake' if mode=='fly_blocked' else 'Horned Turtle','blocker',foe)
        if rider=='prevent':
            self.add('Horned Turtle','ally',owner)
            self.add('Horned Turtle','foreign',foe)
            if mode=='walker':
                self.add('Jace Beleren','walker',owner,'hand')
            if mode=='combat':
                self.add('Grizzly Bears','attacker',foe)
            else:
                recipients={'walker':('walker',),'base':('owner','ally','foreign'),
                            'one_per_packet':('owner','owner','ally','foreign'),
                            'two_per_packet':('owner','ally','foreign'),'replay':('owner','ally')}[mode]
                for n,_ in enumerate(recipients):
                    self.add('Lightning Bolt',f'bolt_{n}',foe,'hand')
                self.recipients=recipients
        if rider=='elf_mana':
            self.add('Grizzly Bears','different',owner)
            self.add('Elvish Warrior','foreign',foe)
            if mode in ('mana_proxy','mana_bearer','replay'):
                self.add('Elvish Warrior','ally',owner)
            if mode in ('self_mana','mana_proxy','replay'):
                self.add('Naturalize','spend',owner,'hand')
                self.add('Expedition Map','artifact',foe)
        if rider=='sweep':
            self.add('Grizzly Bears','ally',owner)
            self.add('Grizzly Bears','enemy',foe)
            self.add('Dazzling Ramparts','wall',foe)
            self.add('Island','land',foe)

    def obs(self,ref,prop,value):
        self.emit('assert_card',ref=ref,property=prop,value=value)

    def facts(self):
        ready=self.cards['source']['zone']=='battlefield'
        if ready:
            self.observe_level()
        self.emit('assert_stack',value=0)
        self.emit('assert_unused_choices',value=0)
        self.observe_mana()
        rider=self.shape['rider']
        for ref,card in self.cards.items():
            where=card['zone']
            if where=='battlefield':
                self.obs(ref,'tapped',int(card['tapped']))
                if ref!='source' and self.oracle[card['name']].get('power') is not None:
                    self.obs(ref,'in_play',1)
                    own=card['player']==self.owner
                    merfolk='Merfolk' in self.oracle[card['name']]['type_line'].partition(' — ')[2].split()
                    boost=0
                    if ready and own:
                        if rider=='merfolk_lord' and merfolk:
                            boost=int(self.level>=self.shape['second'][0])
                        if rider=='lord':
                            boost=int(self.level>=self.shape['first'][0])+int(self.level>=self.shape['second'][0])
                    for field in ('power','toughness'):
                        self.obs(ref,field,int(self.oracle[card['name']][field])+boost)
                if self.oracle[card['name']].get('power') is not None:
                    self.obs(ref,'damage',card['damage'])
            elif where=='graveyard':
                self.emit('assert_zone_contains',player=card['player'],zone='graveyard',
                          value=self.rows[card['name']]['csvid'])
        for seat in (0,1):
            self.emit('assert_life',player=seat,value=self.life[seat])
            for zone in ('battlefield','hand','graveyard'):
                self.emit('assert_zone',player=seat,zone=zone,value=sum(
                    p['zone']==zone and p['player']==seat for p in self.cards.values()))

    def next_turn(self,expected,*,scheduled=False):
        if any(map(any,self.pools)):
            raise ValueError('actual turn change requires fully spent fixture pools')
        if scheduled:
            self.emit('advance_scheduled_turn',value=expected)
        else:
            if expected!=1-self.active:
                raise ValueError('ordinary opposite turn required')
            self.advance_turn()
        self.active=expected
        self.emit('untap_step',player=self.active)
        if not scheduled:
            self.emit('turn',player=self.active,value=0)
        for unit in self.cards.values():
            unit['damage']=0
            if unit['zone']=='battlefield' and unit['player']==self.active:
                unit['tapped']=False

    def remove_source(self):
        self.emit('destroy',ref='source')
        self.cards['source']['zone']='graveyard'
        self.emit('refresh')
        self.facts()

    def denied_level(self):
        self.level_to(0)
        full=list(self.shape['activation'])
        if self.mode=='opponent_timing':
            self.next_turn(self.foe)
        else:
            color=next(i for i in range(1,6) if full[i]) if any(full[1:]) else 0
            full[color]-=1
        self.fund(self.owner,full)
        self.emit('can_activate',ref='source',value=0)
        self.emit('activate',ref='source',value=0)
        self.facts()

    def duel(self,attacker,blocker=None,*,legal_block=True):
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=1)
        self.cards[attacker]['tapped']=True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block',ref=blocker,other=attacker,value=int(legal_block))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.facts()

    def lord(self):
        mode,owner=self.mode,self.owner
        kind=self.shape['rider']
        lower,upper=self.shape['first'][0],self.shape['second'][0]
        target={'before_tier':lower-1,'fly':lower,'fly_blocked':upper,'tier_one':lower}.get(mode,upper)
        self.level_to(target)
        self.facts()
        if mode=='lord_removed':
            self.remove_source()
        if kind=='merfolk_lord' and mode in ('before_tier','fly','fly_blocked'):
            self.next_turn(self.foe)
            self.next_turn(owner)
            if mode=='fly':
                self.life[self.foe]-=self.source_stats()[0]
            else:
                victim=self.cards['blocker']
                body=self.oracle[victim['name']]
                victim['damage']=self.source_stats()[0]
                self.cards['source']['damage']=int(body['power'])
                if victim['damage']>=int(body['toughness']):
                    victim['zone']='graveyard'
            self.duel('source','blocker',legal_block=mode!='fly')

    def bolt(self,index,recipient,prevention):
        ref=f'bolt_{index}'
        if recipient=='owner':
            self.emit('choose_player',player=self.foe,value=self.owner)
        self.cast(ref,target=None if recipient=='owner' else recipient)
        self.emit('resolve')
        self.cards[ref]['zone']='graveyard'
        if recipient=='owner':
            self.life[self.owner]-=3-prevention
        elif recipient=='walker':
            self.cards['walker']['zone']='graveyard'
        else:
            target=self.cards[recipient]
            target['damage']+=3-(prevention if target['player']==self.owner else 0)
        self.facts()

    def prevent(self):
        mode=self.mode
        first,second=self.shape['first'][0],self.shape['second'][0]
        target={'base':0,'one_per_packet':first}.get(mode,second)
        self.level_to(target)
        reduction=int(target>=first)+int(target>=second)
        if mode=='combat':
            self.next_turn(self.foe)
            self.life[self.owner]-=max(0,2-reduction)
            self.duel('attacker')
            return
        if mode=='walker':
            self.cast('walker')
            self.emit('resolve')
            self.cards['walker']['zone']='battlefield'
            self.emit('assert_counter',ref='walker',counter_kind='loyalty',value=3)
        for index,recipient in enumerate(self.recipients):
            self.bolt(index,recipient,reduction)

    def produce_green(self,donor):
        self.emit('require_can_activate',ref='source',value=1)
        if donor!='source':
            self.emit('choose_target',player=self.owner,ref=donor)
        self.emit('activate',ref='source',value=1)
        self.cards[donor]['tapped']=True
        self.pools[self.owner]=[0,0,0,2,0,0]
        self.facts()
        if self.owner==0:
            # Assign Naturalize's generic unit from the two real floating green.
            self.emit('choose_mana',player=0,color=3)
        self.emit('choose_target',player=self.owner,ref='artifact')
        self.emit('cast',ref='spend',value=1)
        self.emit('assert_stack',value=1)
        self.pools[self.owner]=[0]*6
        self.observe_mana()
        self.emit('resolve')
        self.cards['spend']['zone']='graveyard'
        self.cards['artifact']['zone']='graveyard'
        self.facts()

    def elf_mana(self):
        mode=self.mode
        if mode in ('self_mana','fresh'):
            self.level_to(self.shape['first'][0])
            self.next_turn(self.foe)
            if mode=='fresh':
                self.emit('can_activate',ref='source',value=0)
                self.facts()
                return
            self.next_turn(self.owner)
            self.next_turn(self.foe)
            self.produce_green('source')
            return
        self.level_to(self.shape['second'][0])
        self.next_turn(self.foe)
        if mode=='foreign_only':
            self.emit('can_activate',ref='source',value=0)
        elif mode=='mana_bearer':
            self.emit('can_activate',ref='ally',value=1)
            self.emit('can_activate',ref='foreign',value=0)
            self.emit('can_activate',ref='different',value=0)
        else:
            self.produce_green('ally')
            self.emit('can_activate',ref='source',value=0)
        self.facts()

    def time(self):
        mode,owner,foe=self.mode,self.owner,self.foe
        upper=self.shape['second'][0]
        self.level_to(upper-int(mode=='before_tier'))
        self.next_turn(foe,scheduled=True)
        self.facts()
        if mode=='own_end':
            return
        if mode=='removed_timer':
            self.remove_source()
        self.next_turn(owner,scheduled=True)
        self.facts()
        if mode in ('turn_grant','replay'):
            self.next_turn(owner,scheduled=True)
            self.facts()
        self.next_turn(foe,scheduled=True)
        self.facts()

    def sweep(self):
        mode=self.mode
        upper=self.shape['second'][0]
        self.level_to(upper-int(mode=='before_tier'))
        self.next_turn(self.foe)
        if mode=='source_blocks':
            self.cards['source']['damage']=2
            self.cards['enemy'].update(damage=6,zone='graveyard')
            self.duel('enemy','source')
            return
        self.next_turn(self.owner)
        attacker='ally' if mode=='ally_attacks' else 'source'
        life_change=2 if attacker=='ally' else self.source_stats()[0]
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=1)
        self.cards[attacker]['tapped']=True
        self.emit('finish_attackers')
        if attacker=='source' and self.level>=upper:
            self.cards['enemy']['zone']='graveyard'
            self.cards['wall']['damage']=6
        self.emit('refresh')
        self.facts()
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.life[self.foe]-=life_change
        self.emit('end_combat')
        self.facts()

    def run_rider(self):
        if self.mode in COMMON:
            self.denied_level()
            return
        rider=self.shape['rider']
        method='lord' if rider=='merfolk_lord' else rider
        getattr(self,method)()


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    from recipes import step

    if rows[name]['csvid']!=csvid or not selected or set(selected)-set(variants(shape)):
        raise ValueError('exact reviewed resource-level scenario required')
    validate_witnesses(oracle,rows)
    gaps=['Paid resolutions and exact derived tier transitions observe level count.']
    if shape['rider']=='elf_mana':
        gaps.append('The legacy actual mana route activates Joraga as the provider; direct bearer availability is checked separately.')
    if shape['rider']=='time':
        gaps.append('Scheduled turns use the real end-turn trigger and scheduler without independent end-step priority.')
    cases=[]
    for variant in selected:
        mode,seat=variant.rsplit('_',1)
        game=Riders(name,shape,mode,int(seat),rows,oracle)
        game.play_source()
        cut=len(game.steps)
        game.run_rider()
        if mode=='replay':
            tail=game.steps[cut:]
            game.steps=game.steps[:cut]+[step('capture'),*tail,step('record_state'),step('ai_roundtrip'),step('restore'),
                                        *copy.deepcopy(tail),step('assert_recorded_state')]
        cases.append(dict(name=f'LevelResourceRidersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                          setup=game.setup,steps=game.steps,_recipe=dict(family=FAMILY,version=2,variant=variant,
                          params=declaration['params'],oracle_text=declaration['oracle_text'],untested_components=gaps)))
    return cases
