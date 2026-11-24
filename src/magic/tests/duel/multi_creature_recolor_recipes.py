"""Real one-or-more target selection and end-of-turn creature recoloring."""
import copy
import json
import re
from pathlib import Path

from death_counter_extended_recipes import World
from death_counter_trigger_recipes import exact

FAMILY = 'multi_creature_recolor'
PROFILE = Path(__file__).parent / 'data/multi_creature_recolor_wave8_profiles.json'
COLOR_NAMES = {'black':'B','blue':'U','green':'G','red':'R','white':'W'}
COLOR_BITS = {c:1<<n for n,c in enumerate('BUGRW',1)}
MODES = ('sole_target','one_and_stop','two_and_stop','all_three','eleven','partial_hexproof','all_hexproof','cleanup',
         'hibernation','hibernation_expired','colorless_target','no_creature','shroud_only','insufficient_mana',
         'off_turn','pending_snapshot','countered')
TWO_TARGETS = {'two_and_stop','partial_hexproof','pending_snapshot'}
DENIED = {'no_creature','shroud_only','insufficient_mana'}
RESPONSES = {'partial_hexproof':"Ranger's Guile",'all_hexproof':"Ranger's Guile",'countered':'Counterspell'}
MANY_CREATURES = ('Horned Turtle','Hill Giant','Elvish Mystic','Giant Spider','Tidehollow Strix',
                  'Scathe Zombies','Ornithopter','Llanowar Elves','Runeclaw Bear','Muck Rats')
GAPS = ['more than eleven distinct targets and multiplayer',
        'an independently observed priority window between a response and its parent resolution']


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or
            record.get('type_line')!='Instant' or record.get('keywords')!=[]):
        raise ValueError('whole normal recoloring instant required')
    match = re.fullmatch(r'One or more target creatures become (black|blue|green|red|white) until end of turn\.',record.get('oracle_text',''))
    if not match:
        raise ValueError('exact one-or-more target, color replacement and duration required')
    color = COLOR_NAMES[match[1]]
    if record['mana_cost']!='{'+color+'}' or record.get('colors')!=[color]:
        raise ValueError('the reviewed recolor instant has the corresponding one-mana color')
    return dict(schema=1,target='creature',minimum=1,color=color,color_operation='replace',duration='until_end_of_turn')


def spec(declaration,record):
    from recipes import cost_from_oracle

    expected = parameters(record)
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2 or
            declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),expected)):
        raise ValueError('typed complete recoloring declaration required')
    model = dict(expected,family=FAMILY,mana=cost_from_oracle(record))
    selected = declaration.get('variants')
    if not isinstance(selected,list) or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('explicit distinct supported recoloring variants required')
    return model


def variants(model):
    return [f'{mode}_{p}' for p in (0,1) for mode in MODES]


def validate_witnesses(oracle,registry):
    data = profiles()
    for name,pin in data['witnesses'].items():
        row = registry.get(name,{})
        if not exact(oracle.get(name),pin['oracle']) or not exact(row,pin['registry']):
            raise ValueError('recoloring witness identity changed: '+name)
        if row.get('handler')=='card_dummy' and name not in data['characteristics_only']:
            raise ValueError('action witnesses must have implemented handlers: '+name)
    for name,pin in data['sources'].items():
        row = registry.get(name,{})
        if (not exact(oracle.get(name),pin['oracle']) or not exact(row,pin['registry']) or
                row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
            raise ValueError('implemented recoloring source changed: '+name)
        parameters(oracle[name])
    if oracle['Hibernation']['oracle_text']!="Return all green permanents to their owners' hands.":
        raise ValueError('the actual downstream green-permanent effect changed')


class RecolorWorld(World):
    def __init__(self,name,controller,model,registry,oracle,mode):
        from recipes import step

        super().__init__(name,controller,model,registry,oracle)
        self.mode = mode
        self.enemy = 1-controller
        self.new_color = COLOR_BITS[model['color']]
        self.add(name,'source',controller,'hand')
        self.creatures = []
        self.selected = []
        self.stop = None
        if mode=='eleven':
            names = (*MANY_CREATURES,'Savannah Lions' if model['color']!='W' else 'Grizzly Bears')
            for i,n in enumerate(names):
                ref = f'choice_{i}'
                self.creature(n,ref,controller if i%2 else self.enemy)
                self.selected.append(ref)
        elif mode=='no_creature':
            self.add('Howling Mine','inanimate',self.enemy)
        elif mode=='shroud_only':
            self.creature('Calcite Snapper','protected',self.enemy)
        else:
            first = 'Hill Giant' if model['color']=='U' else 'Horned Turtle'
            sentinel = 'Grizzly Bears' if model['color']=='W' else 'Eager Cadet'
            if mode in ('hibernation','hibernation_expired'):
                first = 'Horned Turtle' if model['color']=='G' else 'Grizzly Bears'
                sentinel = 'Eager Cadet' if model['color']=='G' else 'Elvish Mystic'
            elif mode=='colorless_target':
                first = 'Ornithopter'
            self.creature(first,'first',self.enemy)
            self.selected.append('first')
            if mode=='sole_target':
                self.add('Howling Mine','inanimate',controller)
            else:
                self.creature('Tidehollow Strix','second',controller)
                self.creature(sentinel,'untargeted',self.enemy)
                if mode in TWO_TARGETS or mode=='all_three':
                    self.selected.append('second')
                if mode=='all_three':
                    self.selected.append('untargeted')
                elif mode not in DENIED:
                    self.stop = 'untargeted' if mode in TWO_TARGETS else 'second'
        if mode in RESPONSES:
            self.add(RESPONSES[mode],'response',self.enemy,'hand')
        if mode in ('hibernation','hibernation_expired'):
            self.add('Hibernation','color_check',self.enemy,'hand')
        if mode=='off_turn':
            self.pre[1] = step('turn',player=self.enemy,value=0)

    def creature(self,name,ref,player):
        self.add(name,ref,player)
        intrinsic = sum(COLOR_BITS[c] for c in self.oracle[name]['colors'])
        self.entities[ref].update(color=intrinsic,intrinsic=intrinsic)
        self.creatures.append(ref)

    def effects(self):
        from recipes import step

        checks = [s for key,e in self.entities.items() if e['alive'] for s in self.stats(key)]
        for ref in self.creatures:
            e = self.entities[ref]
            if e['alive']:
                csvid = self.registry[e['name']]['csvid']
                checks.extend([step('assert_matching_permanents',player=e['player'],index=csvid,property='color',value=e['color']),
                               step('assert_matching_permanents',player=e['player'],index=csvid,property='artifact',
                                    value=int('Artifact' in self.oracle[e['name']]['type_line']))])
        return checks+self.mana_checks()

    def receipts(self):
        from recipes import step

        return super().receipts()+self.effects()+[step('assert_forced_target_choices',value=0)]

    def payment(self,key,choices=(),response=False):
        from recipes import cost_from_oracle, step

        e = self.entities[key]
        name,p = e['name'],e['player']
        run = self.fund(p,cost_from_oracle(self.oracle[name]))+list(choices)
        run += [step('cast_response' if response else 'cast',ref=key,value=1),step('assert_stack',value=2 if response else 1),
                step('assert_unused_choices',value=0),step('assert_forced_target_choices',value=0)]
        self.zones['hand'][p].remove(name)
        self.mana[p] = [0]*6
        return run

    def cast_source(self):
        from recipes import step

        p = self.controller
        if self.mode in DENIED:
            insufficient = self.mode=='insufficient_mana'
            run = self.fund(p,[0]*6 if insufficient else self.model['mana'])
            if not insufficient:
                run += [step('require_can_cast',ref='source',value=0)]
            return run+[step('cast',ref='source',value=0),step('assert_unused_choices',value=0)]
        choices = [step('choose_target',player=p,ref=ref) for ref in self.selected]
        if self.stop:
            choices.append(step('choose_optional_target',player=p,ref=self.stop))
        return self.payment('source',choices)+self.effects()

    def retire(self,key):
        e = self.entities[key]
        self.zones['graveyard'][e['owner']].append(e['name'])

    def resolve(self):
        from recipes import step

        response = self.mode in RESPONSES
        actions = []
        legal = self.selected.copy()
        if response:
            actions.append(step('announce_spell',ref='source'))
            countered = self.mode=='countered'
            choices = [] if countered else [step('choose_target',player=self.enemy,ref='first')]
            actions += self.payment('response',choices,response=True)
            self.retire('response')
            if countered:
                legal = []
            else:
                self.entities['first'].update(bp=1,bt=1)
                legal.remove('first')
        for ref in legal:
            self.entities[ref]['color'] = self.new_color
        self.retire('source')
        actions += [step('resolve_response' if response else 'resolve'),step('assert_unused_choices',value=0)]+self.receipts()
        if self.mode in ('cleanup','hibernation_expired'):
            for ref in self.creatures:
                self.entities[ref]['color'] = self.entities[ref]['intrinsic']
            if self.mode=='hibernation_expired':
                actions += self.age(own=False)
            else:
                actions += [step('cleanup'),step('refresh')]+self.receipts()
        if self.mode in ('hibernation','hibernation_expired'):
            actions += self.payment('color_check')
            self.retire('color_check')
            for ref in self.creatures:
                if self.entities[ref]['color']&COLOR_BITS['G']:
                    self.leave(ref,'hand')
            actions += [step('resolve'),step('assert_unused_choices',value=0)]+self.receipts()
        return actions


def expand(name,csvid,declaration,model,selected,registry,oracle):
    from recipes import step

    validate_witnesses(oracle,registry)
    pin = profiles()['sources'].get(name,{})
    if pin.get('registry',{}).get('csvid')!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unreserved multi-creature recoloring source or variant')
    result = []
    for variant in selected:
        mode,player = variant.rsplit('_',1)
        w = RecolorWorld(name,int(player),model,registry,oracle,mode)
        actions = w.cast_source()
        follow = w.receipts() if mode in DENIED else w.resolve()
        if mode=='pending_snapshot':
            actions += [step('capture')]+copy.deepcopy(follow)+[step('record_state'),step('ai_roundtrip'),step('restore')]
            follow += [step('assert_recorded_state')]
        actions += follow
        result.append(dict(name=f'MultiCreatureRecolorRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                           setup=w.setup,steps=w.pre+actions,_recipe=dict(family=FAMILY,version=2,variant=variant,
                           params=declaration['params'],oracle_text=declaration['oracle_text'],scope_gaps=GAPS)))
    return result
