"""Actual paid counterspell responses and printed creature passives."""

from collections import Counter
import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from global_attack_tax_recipes import Board


FAMILY = 'uncounterable_creature_casts'
PROFILE = Path(__file__).parent / 'data/uncounterable_creature_casts_wave8_profiles.json'
UNCOUNTERABLE = "This spell can't be countered."
DECK = ('Forest', 'Island', 'Mountain', 'Plains')
NORMAL = 'Grizzly Bears'
AURA = 'Spider Umbra'
INSTANT = 'Visions of Beyond'
ENEMY_CREATURES = {'blue': 'Coral Merfolk', 'black': 'Walking Corpse', 'green': NORMAL}
COLOR_SPELLS = {'blue': 'Unsummon', 'black': 'Disfigure', 'green': 'Giant Growth'}
LACES = {'blue': 'Thoughtlace', 'black': 'Deathlace'}
HEX_SHROUD_MODES = ('own_spell','foe_spell','own_ability','foe_ability')
PROTECTION_MODES = ('target_blue','target_black','target_green','block_blue','block_black','block_green',
                    'damage_blue','damage_black','damage_green','aura_blue','aura_black')
COMMON = ('cast', 'short', 'counter', 'exclude', 'ordinary_counter', 'ordinary_exclude', 'replay')
PASSIVES = {'shroud': HEX_SHROUD_MODES, 'blue_black': PROTECTION_MODES, 'blue': PROTECTION_MODES,
            'trample': ('trample','trample_full'), 'flash_reach': PROTECTION_MODES+('flash_response','flash_reach', 'reach'),
            'trample_hexproof': HEX_SHROUD_MODES+('trample','trample_full')}
STATIC_WITNESSES = set(ENEMY_CREATURES.values()) | {'Wall of Stone','Suntail Hawk'}
WITNESSES = set(DECK) | STATIC_WITNESSES | set(COLOR_SPELLS.values()) | set(LACES.values()) | {
    'Counterspell','Exclude','Master Decoy',AURA,INSTANT}
MATCHES = {
    ('Creature — Mongoose','{1}{G}',('Shroud',),'2','1',UNCOUNTERABLE+'\nShroud (This creature can\'t be the target of spells or abilities.)'): 'shroud',
    ('Creature — Elk','{1}{G}{G}',('Protection',),'3','3',UNCOUNTERABLE+'\nProtection from blue and from black (This creature can\'t be blocked, targeted, dealt damage, or enchanted by anything blue or black.)'): 'blue_black',
    ('Creature — Beast','{4}{G}',('Protection',),'3','4',UNCOUNTERABLE+'\nProtection from blue'): 'blue',
    ('Creature — Beast','{3}{G}{G}{G}',('Trample',),'8','8',UNCOUNTERABLE+'\nTrample (This creature can deal excess combat damage to the player or planeswalker it\'s attacking.)'): 'trample',
    ('Creature — Insect','{1}{G}',('Reach','Protection','Flash'),'2','2','Flash\n'+UNCOUNTERABLE+'\nReach, protection from blue'): 'flash_reach',
    ('Creature — Dinosaur','{4}{G}{G}',('Hexproof','Trample'),'7','6',UNCOUNTERABLE+'\nTrample, hexproof'): 'trample_hexproof',
}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    key = (record['type_line'],record['mana_cost'],tuple(record['keywords']),record.get('power'),
           record.get('toughness'),record['oracle_text'])
    if record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces') or key not in MATCHES:
        raise ValueError('supported whole uncounterable creature Oracle required')
    model = MATCHES[key]
    protection = ['blue','black'] if model=='blue_black' else ['blue'] if model in ('blue','flash_reach') else []
    return dict(model=model,cost=cost_from_oracle(record),protection=protection,
                trample='Trample' in record['keywords'],flash='Flash' in record['keywords'])


def variants(params):
    return [f'{mode}_{seat}' for seat in (0,1) for mode in (*COMMON,*PASSIVES[params['model']])]


def spec(declaration,record):
    params=parameters(record)
    chosen=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or not exact(profile()['sources'].get(record['name'],{}).get('oracle'),record)
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),params)):
        raise ValueError('exact pinned source and full Oracle required')
    if (not isinstance(chosen,list) or not chosen or any(type(mode) is not str for mode in chosen)
            or len(chosen)!=len(set(chosen)) or set(chosen)-set(variants(params))):
        raise ValueError('distinct calibrated creature cast variants required')
    return dict(params,family=FAMILY)


def validate_witnesses(oracle,rows):
    pins=profile()
    if set(pins['witnesses'])!=WITNESSES or len(pins['sources'])!=len(MATCHES):
        raise ValueError('complete action and combat witness inventory required')
    for name,pin in (pins['sources']|pins['witnesses']).items():
        row=rows[name]
        if row['kind']!='card' or not exact(row,pin['registry']) or not exact(oracle[name],pin['oracle']):
            raise ValueError(f'Oracle/registry drift for {name}')
        if name not in STATIC_WITNESSES and row['handler'] in ('card_dummy','0','NULL',None):
            raise ValueError(f'live spell or activation witness required for {name}')
    if (oracle['Counterspell']['oracle_text']!='Counter target spell.' or oracle['Exclude']['oracle_text']!=
            'Counter target creature spell.\nDraw a card.'):
        raise ValueError('plain and independently observed counterspell witnesses required')


class UncounterableBoard(Board):
    def __init__(self,name,params,variant,rows,oracle):
        mode,seat=variant.rsplit('_',1)
        self.mode,self.owner,self.enemy=mode,int(seat),1-int(seat)
        self.params,self.model,self.rows,self.oracle=params,params['model'],rows,oracle
        self.name=name
        self.active=self.enemy if mode.startswith('damage_') or mode in ('flash_response','flash_reach','reach') else self.owner
        self.mana,self.life=[[0]*6,[0]*6],[20,20]
        self.setup,self.steps,self.entities=[],[],{}
        self.zones={zone:[[],[]] for zone in ('hand','battlefield','graveyard','library','exile')}
        self.loyalty=self.replay=None
        self.actual_source=NORMAL if mode.startswith('ordinary') else name
        self.add(self.actual_source,'source',self.owner,'hand' if mode in COMMON or mode.startswith('flash_') else 'battlefield')
        if mode in ('counter','exclude','ordinary_counter','ordinary_exclude','replay'):
            self.add('Exclude' if 'exclude' in mode else 'Counterspell','counter',self.enemy,'hand')
        if mode=='flash_response':
            self.add(INSTANT,'parent',self.enemy,'hand')
        if mode.endswith('_spell') or mode.startswith('target_'):
            witness=COLOR_SPELLS[mode.split('_')[1]] if mode.startswith('target_') else COLOR_SPELLS['green']
            self.add(witness,'targeter',self.owner if mode=='own_spell' else self.enemy,'hand')
        if mode.endswith('_ability'):
            self.add('Master Decoy','targeter',self.owner if mode=='own_ability' else self.enemy)
        if mode.startswith(('block_','damage_')):
            self.add(ENEMY_CREATURES[mode.split('_')[1]],'foe',self.enemy)
        if mode in ('trample','trample_full'):
            self.add(NORMAL if mode=='trample' else 'Wall of Stone','foe',self.enemy)
        if mode in ('flash_reach','reach'):
            self.add('Suntail Hawk','foe',self.enemy)
        if mode.startswith('aura_'):
            self.add(AURA,'aura',self.owner,'hand')
            self.add(LACES[mode.split('_')[1]],'lace',self.owner,'hand')
        for who in (0,1):
            for index,card in enumerate(DECK):
                self.add(card,f'library_{who}_{index}',who,'library')
        self.names={entry['name'] for entry in self.entities.values()}|set(DECK)|{name}

    def receipt(self):
        self.emit('assert_stack',value=0)
        self.emit('assert_choices_consumed')
        self.pools()
        for who in (0,1):
            self.emit('assert_life',player=who,value=self.life[who])
            for zone,players in self.zones.items():
                cards=players[who]
                self.emit('assert_zone',player=who,zone=zone,value=len(cards))
                if zone=='library':
                    for index,card in enumerate(cards):
                        self.emit('assert_zone_card',player=who,zone=zone,index=index,value=self.rows[card]['csvid'])
                else:
                    counts=Counter(cards)
                    for card in sorted(self.names):
                        self.emit('assert_zone_multiplicity',player=who,zone=zone,index=self.rows[card]['csvid'],value=counts[card])
        source=self.entities['source']
        if source['zone']=='battlefield':
            self.observe('source','in_play',1)
            for stat,value in zip(('power','toughness'),source['stats']):
                self.observe('source',stat,value)
        elif source['zone']=='hand':
            self.observe('source','in_hand',1)

    def fee(self,ref):
        from recipes import cost_from_oracle

        return cost_from_oracle(self.oracle[self.entities[ref]['name']])

    def start(self,ref,*,target=None,response=False,denied=False):
        who=self.entities[ref]['player']
        fee=self.fee(ref)
        budget=fee.copy()
        if denied:
            budget[3]-=1
        self.fund(who,budget)
        if target:
            self.emit('choose_target',player=who,ref=target)
        self.emit('cast_response' if response else 'cast',ref=ref,value=int(not denied))
        self.observe(ref,'in_hand',denied)
        self.emit('assert_stack',value=0 if denied else 2 if response else 1)
        if not denied:
            self.mana[who]=[0]*6
        self.pools()

    def resolve(self,ref,destination):
        self.emit('resolve')
        self.move(ref,destination)
        self.emit('refresh')

    def draw(self,who):
        self.zones['hand'][who].append(self.zones['library'][who].pop(0))

    def cast_case(self):
        self.start('source',denied=self.mode=='short')
        if self.mode=='short':
            return
        if self.mode=='cast':
            self.resolve('source','battlefield')
            return
        self.emit('announce_spell',ref='source')
        self.emit('assert_stack',value=1)
        # Both counter handlers bind the unique real top creature spell in their cast callback.
        self.start('counter',response=True)
        if self.mode=='replay':
            self.replay=len(self.steps)
        self.emit('resolve_response')
        self.move('counter','graveyard')
        self.move('source','graveyard' if self.mode.startswith('ordinary') else 'battlefield')
        if 'exclude' in self.mode:
            self.draw(self.enemy)
        self.emit('refresh')

    def flash_response(self):
        self.start('parent')
        self.emit('announce_spell',ref='parent')
        self.start('source',response=True)
        self.emit('resolve_response')
        self.move('source','battlefield')
        self.move('parent','graveyard')
        self.draw(self.enemy)
        self.emit('refresh')

    def spell_target(self):
        mode=self.mode
        who=self.entities['targeter']['player']
        color=mode.split('_')[1] if mode.startswith('target_') else 'green'
        allowed=(color not in self.params['protection'] and self.model!='shroud'
                 and (self.model!='trample_hexproof' or who==self.owner))
        self.emit('can_target_creature',ref='targeter',other='source',value=int(allowed))
        self.emit('can_cast',ref='targeter',value=int(allowed))
        if not allowed:
            return
        self.start('targeter',target='source')
        self.resolve('targeter','graveyard')
        if color=='blue':
            self.move('source','hand')
        else:
            delta=-2 if color=='black' else 3
            self.entities['source']['stats']=tuple(value+delta for value in self.entities['source']['stats'])
            if self.entities['source']['stats'][1]<=0:
                self.move('source','graveyard')
            else:
                self.receipt()
                self.emit('cleanup')
                self.emit('refresh')
                self.entities['source']['stats']=tuple(int(self.oracle[self.name][key]) for key in ('power','toughness'))

    def ability_target(self):
        who=self.entities['targeter']['player']
        allowed=self.model=='trample_hexproof' and who==self.owner
        self.fund(who,[0,0,0,0,0,1])
        self.emit('probe_activation_target',ref='targeter',other='source',index=-1,value=int(allowed))
        if allowed:
            self.emit('choose_target',player=who,ref='source')
            self.emit('activate',ref='targeter',value=1)
            self.mana[who]=[0]*6
            self.observe('targeter','tapped',1)
            self.observe('source','tapped',0)
            self.emit('assert_stack',value=1)
            self.emit('resolve')
            self.observe('source','tapped',1)
        else:
            self.observe('targeter','tapped',0)
            self.observe('source','tapped',0)

    def aura_color(self):
        base=self.entities['source']['stats']
        self.start('aura',target='source')
        self.resolve('aura','battlefield')
        self.entities['source']['stats']=tuple(value+1 for value in base)
        self.receipt()
        self.emit('assert_matching_permanents',player=self.owner,index=self.rows[AURA]['csvid'],property='color',value=0x08)
        self.start('lace',target='aura')
        self.resolve('lace','graveyard')
        color=self.mode.split('_')[1]
        if color in self.params['protection']:
            self.move('aura','graveyard')
            self.entities['source']['stats']=base
        else:
            self.emit('assert_matching_permanents',player=self.owner,index=self.rows[AURA]['csvid'],property='color',value=0x02)

    def combat(self):
        attacking=self.active==self.owner
        attacker,blocker=('source','foe') if attacking else ('foe','source')
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=1)
        self.observe(attacker,'tapped',1)
        self.emit('finish_attackers')
        if self.mode=='flash_reach':
            self.start('source')
            self.resolve('source','battlefield')
            self.observe('source','sick',1)
            self.observe('source','tapped',0)
        self.emit('begin_blockers')
        color=self.mode.split('_')[1] if self.mode.startswith(('block_','damage_')) else None
        blocked=not(attacking and color in self.params['protection'])
        self.emit('block',ref=blocker,other=attacker,value=int(blocked))
        self.emit('advance_combat')
        self.observe(attacker,'blocked',blocked)
        self.emit('combat_damage')
        self.emit('end_combat')
        ap,at=self.entities[attacker]['stats']
        bp,bt=self.entities[blocker]['stats']
        if not blocked:
            self.life[self.enemy]-=ap
            return
        if attacking and self.params['trample']:
            self.life[self.enemy]-=max(0,ap-bt)
        if attacking:
            self.combat_hit(attacker,0 if color in self.params['protection'] else bp)
            self.combat_hit(blocker,ap)
        else:
            self.combat_hit(blocker,0 if color in self.params['protection'] else ap)
            self.combat_hit(attacker,bp)

    def combat_hit(self,ref,amount):
        if amount>=self.entities[ref]['stats'][1]:
            self.move(ref,'graveyard')
        else:
            self.observe(ref,'damage',amount)

    def actions(self):
        from recipes import step

        self.emit('turn',player=self.active,value=0)
        self.fund(self.owner,[0]*6)
        if self.mode in COMMON:
            self.cast_case()
        elif self.mode=='flash_response':
            self.flash_response()
        elif self.mode.endswith('_spell') or self.mode.startswith('target_'):
            self.spell_target()
        elif self.mode.endswith('_ability'):
            self.ability_target()
        elif self.mode.startswith('aura_'):
            self.aura_color()
        else:
            self.combat()
        self.receipt()
        if self.replay is not None:
            tail=self.steps[self.replay:]
            self.steps[self.replay:]=[step('capture'),*tail,step('record_state'),step('ai_roundtrip'),
                                     step('restore'),*tail,step('assert_recorded_state')]


def expand(name,csvid,declaration,params,chosen,rows,oracle):
    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or not chosen or set(chosen)-set(variants(params)):
        raise ValueError('unbound uncounterable source or variant')
    result=[]
    for variant in chosen:
        board=UncounterableBoard(name,params,variant,rows,oracle)
        board.actions()
        coverage=['complete priority-menu eligibility enumeration','combat damage assigned to a planeswalker',
                  'multiple or deathtouch blockers','multiplayer and replacement effects']
        metadata=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                      untested_components=coverage)
        result.append(dict(name=f'UncounterableCreatureCastRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                           setup=board.setup,steps=board.steps,_recipe=metadata))
    return result
