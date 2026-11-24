"""Fixed generic discounts through paid casts, exact scopes and printed riders."""

import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from morph_turn_up_target_recipes import Scene
from multi_type_discounts_recipes import DiscountScene, card_types


FAMILY = 'fixed_generic_cast_reducers'
PROFILE = Path(__file__).parent / 'data/fixed_generic_cast_reducers_wave8_profiles.json'
DEFAULT = {'own_all':'Divination','global_all':'Divination','multicolored':'Cloudblazer',
           'six_creature':'Craw Wurm','instant_sorcery':'Divination','flying':'Wind Drake','colorless':'Maw of Kozilek'}
STOCK = ('Forest','Island','Mountain','Plains')
BASE = ('exact','residual','short','wrong_color','source_cast','source_leaves','opposing_caster',
        'mixed_sources','stacked','countered','paid_replay','tax')
UNIVERSAL = ('artifact','creature','colored_floor','offturn_instant','tapped_source')
EXTRA = {'own_all':UNIVERSAL,'global_all':UNIVERSAL,
         'multicolored':('devoid_negative','mixed_negative','colorless_negative','one_generic','colored_floor','tapped_source'),
         'six_creature':('mixed_negative','high_noncreature','artifact_creature'),
         'instant_sorcery':('instant','offturn_instant','flash_creature','mixed_negative','colored_floor','kindred_instant'),
         'flying':('ground','mixed_negative','ground_with_levitation','artifact_creature','colored_floor','rider_flying','rider_reach'),
         'colorless':('artifact','colored_negative','mixed_negative','devoid_two_symbols','artifact_creature','colored_artifact')}
PROBES = {
    'artifact':'Jalum Tome','creature':'Grizzly Bears','offturn_instant':'Think Twice','instant':'Think Twice',
    'devoid_negative':'Herald of Kozilek','colorless_negative':'Jalum Tome','one_generic':'Centaur Healer',
    'high_noncreature':'Dreamstone Hedron','artifact_creature':'Steel Hellkite','flash_creature':'Ambush Viper',
    'kindred_instant':'Nameless Inversion','ground':'Horned Turtle','ground_with_levitation':'Horned Turtle',
    'colored_negative':'Hill Giant','devoid_two_symbols':'Herald of Kozilek','colored_artifact':'Esperzoa'}
FLOORS = {'multicolored':'Watchwolf','instant_sorcery':'Visions of Beyond'}
MIXED_NEGATIVES = {'multicolored':'Water Elemental','six_creature':'Spined Wurm','instant_sorcery':'Jalum Tome',
                  'flying':'Horned Turtle','colorless':'Hill Giant'}
STATIC = {'Grizzly Bears','Hill Giant','Giant Spider','Suntail Hawk','Craw Wurm','Spined Wurm',
          'Wind Drake','Horned Turtle','Water Elemental','Watchwolf'}
WITNESSES = set(DEFAULT.values())|set(PROBES.values())|set(FLOORS.values())|set(MIXED_NEGATIVES.values())|set(STOCK)|{
    'Counterspell','Boomerang','Sphere of Resistance','Levitation','Giant Spider','Suntail Hawk'}
MODELS = {
    ('Artifact','{5}',(),None,None,'Spells you cast cost {1} less to cast.'): ('own_all',1,False),
    ('Artifact','{2}',(),None,None,'Spells cost {1} less to cast.'): ('global_all',1,True),
    ('Artifact','{4}',(),None,None,'Multicolored spells cost {2} less to cast.'): ('multicolored',2,True),
    ('Creature — Elf','{3}{G}',(),'2','2',
     'Creature spells you cast with mana value 6 or greater cost {2} less to cast.'): ('six_creature',2,False),
    ('Enchantment','{4}{U}',(),None,None,'Instant and sorcery spells cost {2} less to cast.'): ('instant_sorcery',2,True),
    ('Creature — Bird Wizard','{2}{U}',('Flying',),'2','2',
     'Flying\nCreature spells with flying you cast cost {1} less to cast.'): ('flying',1,False),
    ('Creature — Eldrazi Drone','{1}{U}{R}',('Devoid',),'2','4',
     'Devoid (This card has no color.)\nColorless spells you cast cost {1} less to cast.'): ('colorless',1,False),
}


def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    key=(record['type_line'],record['mana_cost'],tuple(record['keywords']),record.get('power'),
         record.get('toughness'),record['oracle_text'])
    if record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces') or key not in MODELS:
        raise ValueError('whole fixed generic spell discount and printed riders required')
    scope,amount,bilateral=MODELS[key]
    return dict(effect='fixed_generic_discount',spells=scope,amount=amount,bilateral=bilateral,cost=cost_from_oracle(record))


def variants(params):
    return [f'{name}_{seat}' for seat in (0,1) for name in (*BASE,*EXTRA[params['spells']])]


def spec(declaration,record):
    params=parameters(record)
    selected=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or not exact(pins()['sources'].get(record['name'],{}).get('oracle'),record)
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),params)):
        raise ValueError('exact whole generic reducer and scope required')
    if (not isinstance(selected,list) or not selected or any(type(item) is not str for item in selected)
            or len(selected)!=len(set(selected)) or set(selected)-set(variants(params))):
        raise ValueError('distinct reviewed fixed reduction variants required')
    return dict(params,family=FAMILY)


def validate_witnesses(oracle,rows):
    data=pins()
    if set(data['witnesses'])!=WITNESSES or len(data['sources'])!=len(MODELS):
        raise ValueError('complete independently pinned cast witness inventory required')
    for name,pin in (data['sources']|data['witnesses']).items():
        row=rows[name]
        if row['kind']!='card' or not exact(row,pin['registry']) or not exact(oracle[name],pin['oracle']):
            raise ValueError(f'Oracle or registry drift for {name}')
        if name not in STATIC and row['handler'] in ('card_dummy','0','NULL',None):
            raise ValueError(f'live cast witness must have a production handler for {name}')
    if [int(oracle[name]['cmc']) for name in ('Spined Wurm','Craw Wurm','Dreamstone Hedron','Steel Hellkite')]!=[5,6,6,6]:
        raise ValueError('exact neighboring mana values and artifact distinctions required')
    if (oracle['Wind Drake']['mana_cost']!=oracle['Horned Turtle']['mana_cost']
            or oracle['Maw of Kozilek']['mana_cost']!=oracle['Hill Giant']['mana_cost']
            or oracle['Maw of Kozilek']['colors'] or not oracle['Hill Giant']['colors']):
        raise ValueError('independent same-price flying and Devoid controls required')


def qualifies(record,scope):
    types=card_types(record)
    if 'Land' in types:
        return False
    if scope in ('own_all','global_all'):
        return True
    if scope=='multicolored':
        return len(record['colors'])>=2
    if scope=='six_creature':
        return 'Creature' in types and record['cmc']>=6
    if scope=='instant_sorcery':
        return bool(types.intersection(('Instant','Sorcery')))
    if scope=='flying':
        return 'Creature' in types and 'Flying' in record['keywords']
    if scope=='colorless':
        return not record['colors']
    raise ValueError(f'unsupported fixed discount predicate {scope}')


def price(record,model,controllers,caster,*,tax=0):
    from recipes import cost_from_oracle

    mana=cost_from_oracle(record)
    matches=sum(model['bilateral'] or controller==caster for controller in controllers)
    reduction=model['amount']*matches if qualifies(record,model['spells']) else 0
    mana[0]=max(0,mana[0]+tax-reduction)
    return mana


class FixedScene(DiscountScene):
    def stats(self,ref):
        return Scene.stats(self,ref)

    def probe_name(self):
        scope=self.model['spells']
        if self.mode=='colored_floor':
            return FLOORS.get(scope,'Suntail Hawk')
        if self.mode=='mixed_negative':
            return MIXED_NEGATIVES[scope]
        return PROBES.get(self.mode,DEFAULT[scope])

    def setup_cards(self):
        mode=self.mode
        if mode=='flash_creature':
            self.active=self.foe
        self.add(self.name,'source',self.owner,'hand' if mode=='source_cast' else 'battlefield',
                 tapped=mode=='tapped_source')
        if mode in ('mixed_sources','mixed_negative','stacked'):
            self.add(self.name,'copy',self.owner if mode=='stacked' else self.foe)
        if mode in ('rider_flying','rider_reach'):
            self.add('Giant Spider' if mode=='rider_reach' else 'Grizzly Bears','blocker',self.foe)
        else:
            self.add(self.probe_name(),'probe',self.actor,'hand')
        if mode=='source_leaves':
            self.add('Boomerang','bounce',self.foe,'hand')
        if mode=='countered':
            self.add('Counterspell','counter',self.foe,'hand')
        if mode=='tax':
            self.add('Sphere of Resistance','taxer',self.foe)
        if mode=='ground_with_levitation':
            self.add('Levitation','levitation',self.owner)
        if mode=='kindred_instant':
            self.add('Hill Giant','victim',self.foe)
        for who in (0,1):
            for index,card in enumerate(STOCK):
                ref=f'library_{who}_{index}'
                self.add(card,ref,who,'library')
                self.library[who].append(ref)

    def source_traits(self,full=False):
        super().source_traits(full)
        if full:
            self.observe_color('source')

    def observe_color(self,ref):
        from recipes import COLORS

        entity=self.entities[ref]
        color=sum(1<<COLORS[symbol] for symbol in self.oracle[entity['name']]['colors'])
        self.emit('assert_matching_permanents',player=entity['controller'],index=self.rows[entity['name']]['csvid'],
                  property='color',value=color)

    def live_price(self,ref):
        entity=self.entities[ref]
        controllers=[item['controller'] for key,item in self.entities.items()
                     if key in ('source','copy') and item['zone']=='battlefield']
        return price(self.oracle[entity['name']],self.model,controllers,entity['owner'],tax=int('taxer' in self.entities))

    def start(self,ref,*,budget=None,allowed=True,response=False):
        entity=self.entities[ref]
        actor=entity['owner']
        fee=self.live_price(ref)
        self.fund(actor,fee if budget is None else budget)
        target={'bounce':'source'}.get(ref)
        if entity['name']=='Nameless Inversion':
            target='victim'
        if allowed and target:
            self.emit('choose_target',player=actor,ref=target)
        self.emit('cast_response' if response else 'cast',ref=ref,value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1+int(response) if allowed else 0)
        self.prop(ref,'in_hand',not allowed)
        if allowed:
            self.pay(actor,fee)
            entity['zone']='stack'
        self.pools()

    def draw_cards(self,who,count):
        for _ in range(count):
            self.leave(self.library[who].pop(0),'hand')

    def finish(self):
        name=self.probe_name()
        self.resolve()
        if card_types(self.oracle[name]).intersection(('Instant','Sorcery')):
            self.leave('probe','graveyard')
            if name in ('Divination','Think Twice','Visions of Beyond'):
                self.draw_cards(self.actor,2 if name=='Divination' else 1)
            if name=='Nameless Inversion':
                self.leave('victim','graveyard')
        else:
            self.leave('probe','battlefield')
            if name=='Cloudblazer':
                self.life[self.actor]+=2
                self.draw_cards(self.actor,2)
            elif name=='Centaur Healer':
                self.life[self.actor]+=3
            self.observe_color('probe')
        self.gates()

    def run_probe(self):
        from recipes import cost_from_oracle

        printed=cost_from_oracle(self.oracle[self.probe_name()])
        mode=self.mode
        budget=self.live_price('probe')
        if mode not in ('exact','source_cast','paid_replay','short','wrong_color','offturn_instant'):
            budget=printed
            budget[0]+=int(mode=='tax')
        allowed=mode not in ('short','wrong_color')
        if mode=='short':
            budget[next(index for index,amount in enumerate(budget) if amount)]-=1
        elif mode=='wrong_color':
            position=next(index for index,amount in enumerate(budget) if index and amount)
            budget[0]+=budget[position]
            budget[position]=0
        if mode=='paid_replay':
            self.replay=len(self.steps)
        self.start('probe',budget=budget,allowed=allowed)
        if not allowed:
            self.gates()
            return
        if mode=='countered':
            self.emit('announce_spell',ref='probe')
            self.start('counter',response=True)
            self.resolve(parent=True)
            self.leave('counter','graveyard')
            self.leave('probe','graveyard')
            self.gates()
            return
        self.finish()

    def flying_fight(self):
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.entities['source']['tapped']=True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked=self.mode=='rider_reach'
        self.emit('block',ref='blocker',other='source',value=int(blocked))
        self.emit('advance_combat')
        self.prop('source','blocked',blocked)
        self.emit('combat_damage')
        self.emit('end_combat')
        if blocked:
            self.leave('source','graveyard')
            self.entities['blocker']['damage']=2
        else:
            self.life[self.foe]-=2
        self.gates()

    def play(self):
        self.setup_cards()
        self.refresh_zero()
        if self.mode=='source_cast':
            self.start('source')
            self.resolve()
            self.leave('source','battlefield')
        if self.mode=='source_leaves':
            self.start('bounce')
            self.resolve()
            self.leave('bounce','graveyard')
            self.leave('source','hand')
        self.gates()
        if self.mode in ('rider_flying','rider_reach'):
            self.flying_fight()
        else:
            self.run_probe()


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or not selected or set(selected)-set(variants(model)):
        raise ValueError('unbound source or invalid fixed reducer variant')
    cases=[]
    for variant in selected:
        mode,seat=variant.rsplit('_',1)
        case=FixedScene(name,model,mode,int(seat),rows,oracle)
        case.play()
        steps=case.steps
        if case.replay is not None:
            cut=case.replay
            tail=steps[cut:]
            steps=steps[:cut]+[step('capture'),*tail,step('record_state'),step('ai_roundtrip'),step('restore'),*tail,step('assert_recorded_state')]
        metadata=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                      untested_components=['alternative/hybrid payments','X on the stack','dedicated colorless mana symbols',
                                           'cost reducers for other non-casting actions','multiplayer'])
        cases.append(dict(name=f'FixedGenericCastReducerRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=1772,
                          setup=case.setup,steps=steps,_recipe=metadata))
    return cases
