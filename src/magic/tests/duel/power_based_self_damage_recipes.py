"""Targeted self-damage using the creature's power and characteristics at resolution."""
import copy
import json
from pathlib import Path

from death_counter_extended_recipes import World
from death_counter_trigger_recipes import exact

FAMILY = 'power_based_self_damage'
PROFILE = Path(__file__).parent / 'data/power_based_self_damage_wave8_profiles.json'
ORACLE_TEXT = 'Target creature deals damage to itself equal to its power.'
MODES = ('survives','lethal','own_target','lifelink','foreign_lifelink','deathtouch','indestructible','own_color_protection',
         'zero_power','power_up_pending','negative_power_pending','target_bounced','target_hexproof','countered',
         'no_creature','shroud_only','insufficient_mana','pending_snapshot')
TARGETS = {'lethal':'Runeclaw Bear','lifelink':'Child of Night','foreign_lifelink':'Child of Night',
           'deathtouch':'Deadly Recluse','indestructible':'Darksteel Sentinel','zero_power':'Wall of Wood',
           'no_creature':'Howling Mine','shroud_only':'Calcite Snapper'}
RESPONSES = {'power_up_pending':('Giant Growth',3), 'negative_power_pending':('Disfigure',-2),
             'target_hexproof':("Ranger's Guile",1), 'target_bounced':('Unsummon',0), 'countered':('Counterspell',0)}
GAPS = ['hand-sorcery timing denial requires the actual priority action gate; the low-level test cast is not that gate',
        'arbitrary damage replacement and multiplayer']


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or
            record.get('type_line') not in ('Instant','Sorcery') or record.get('keywords')!=[] or
            record.get('oracle_text')!=ORACLE_TEXT or 'U' in record.get('colors',[])):
        raise ValueError('whole reviewed nonblue one-target power-based self-damage spell required')
    return dict(schema=1,target='creature',amount='power_on_resolution',damage_source='target',
                timing=record['type_line'].lower())


def spec(declaration,record):
    from recipes import cost_from_oracle

    expected = parameters(record)
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2 or
            declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),expected)):
        raise ValueError('typed complete self-damage declaration required')
    model = dict(expected,family=FAMILY,mana=cost_from_oracle(record))
    selected = declaration.get('variants')
    if not isinstance(selected,list) or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('explicit distinct supported self-damage variants required')
    return model


def variants(model):
    modes = MODES+('off_turn','instant_response') if model['timing']=='instant' else MODES
    return [f'{mode}_{p}' for p in (0,1) for mode in modes]


def validate_witnesses(oracle,registry):
    data = profiles()
    for name,pin in data['witnesses'].items():
        row = registry.get(name,{})
        if not exact(oracle.get(name),pin['oracle']) or not exact(row,pin['registry']):
            raise ValueError('self-damage witness pin changed: '+name)
        if row.get('handler')=='card_dummy' and name not in data['characteristics_only']:
            raise ValueError('an action witness must have an implemented handler: '+name)
    for name,pin in data['sources'].items():
        row = registry.get(name,{})
        if (not exact(oracle.get(name),pin['oracle']) or not exact(row,pin['registry']) or
                row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
            raise ValueError('implemented normal self-damage spell changed: '+name)
        parameters(oracle[name])
    target = oracle['Horned Turtle']
    if target['colors']!=['U'] or target['power']!='1' or target['toughness']!='4':
        raise ValueError('the pending negative-power and protected-source fixtures require a blue 1/4')


class SelfDamageWorld(World):
    def __init__(self,name,controller,model,registry,oracle,mode):
        from recipes import step

        super().__init__(name,controller,model,registry,oracle)
        self.mode = mode
        self.enemy = 1-controller
        self.target_controller = controller if mode=='own_target' else self.enemy
        self.add(name,'source',controller,'hand')
        self.add(TARGETS.get(mode,'Horned Turtle'),'victim',self.target_controller,foreign=mode=='foreign_lifelink')
        if mode not in ('no_creature','shroud_only'):
            self.add('Eager Cadet','alternate',1-self.target_controller)
        if mode in RESPONSES:
            if mode=='target_hexproof':
                self.add('Runeclaw Bear','defender_alternate',self.target_controller)
            self.add(RESPONSES[mode][0],'response',self.enemy,'hand')
        if mode=='own_color_protection':
            self.add('Blue Ward','ward',controller,'hand')
        if mode=='instant_response':
            self.add('Reach Through Mists','parent',self.enemy,'hand')
        if mode in ('off_turn','instant_response'):
            self.pre[1] = step('turn',player=self.enemy,value=0)

    def effects(self):
        return [s for key,e in self.entities.items() if e['alive'] for s in self.stats(key)]+self.mana_checks()

    def receipts(self):
        return super().receipts()+self.effects()

    def cast_card(self,key,target=None,response=False):
        from recipes import cost_from_oracle, step

        card = self.entities[key]
        p = card['player']
        cost = cost_from_oracle(self.oracle[card['name']])
        run = self.fund(p,cost)
        if target:
            run.append(step('choose_target',player=p,ref=target))
        run += [step('cast_response' if response else 'cast',ref=key,value=1),step('assert_stack',value=2 if response else 1),
                step('assert_unused_choices',value=0)]
        self.zones['hand'][p].remove(card['name'])
        self.mana[p] = [0]*6
        return run

    def finish(self,key):
        card = self.entities[key]
        self.zones['graveyard'][card['owner']].append(card['name'])

    def add_protection(self):
        from recipes import step

        run = self.cast_card('ward','victim')+[step('resolve'),step('assert_unused_choices',value=0)]
        self.entities['ward'].update(alive=True,attached='victim')
        # The blue creature now has protection from its own blue damage.
        # The actual nonblue self-damage spell remains a valid targeter.
        return run+[step('can_target_creature',ref='source',other='victim',value=1)]+self.receipts()

    def response(self):
        from recipes import step

        mode = self.mode
        run = [step('announce_spell',ref='source')]
        run += self.cast_card('response',None if mode=='countered' else 'victim',response=True)
        run += [step('resolve_response'),step('assert_unused_choices',value=0)]
        self.finish('response')
        if mode=='target_bounced':
            self.leave('victim','hand')
        else:
            boost = RESPONSES[mode][1]
            self.entities['victim'].update(bp=boost,bt=boost)
        checks = self.effects()
        if mode=='power_up_pending':
            # The response helper drains the original spell too. Its damage
            # is covered by the exact final receipt in resolve_source().
            checks = [s for s in checks if (s.get('ref'),s.get('property'))!=('victim','damage')]
        return run+checks

    def damage(self):
        victim = self.entities['victim']
        if not victim['alive'] or self.mode in ('target_hexproof','own_color_protection'):
            return
        record = self.oracle[victim['name']]
        amount = max(0,int(record['power'])+victim['bp'])
        victim['damage'] += amount
        keys = record['keywords']
        if 'Lifelink' in keys:
            self.life[victim['player']] += amount
        lethal = victim['damage']>=int(record['toughness'])+victim['bt'] or amount>0 and 'Deathtouch' in keys
        if lethal and 'Indestructible' not in keys:
            self.leave('victim')

    def resolve_source(self,response=False):
        from recipes import step

        self.damage()
        self.finish('source')
        run = [step('resolve_response' if response else 'resolve'),step('assert_unused_choices',value=0)]
        if response:
            # resolve_response preserves the parent context while draining
            # both spells. Only review the parent outcome after that drain.
            self.finish('parent')
            self.zones['hand'][self.enemy].append(self.zones['library'][self.enemy].pop(0))
        return run+self.receipts()


def expand(name,csvid,declaration,model,selected,registry,oracle):
    from recipes import step

    validate_witnesses(oracle,registry)
    pin = profiles()['sources'].get(name,{})
    if pin.get('registry',{}).get('csvid')!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unreserved power-based self-damage source or variant')
    result = []
    for variant in selected:
        mode,player = variant.rsplit('_',1)
        w = SelfDamageWorld(name,int(player),model,registry,oracle,mode)
        if mode in ('no_creature','shroud_only','insufficient_mana'):
            valid_target = mode=='insufficient_mana'
            actions = w.fund(w.controller,[0]*6 if valid_target else model['mana'])
            if not valid_target:
                actions += [step('require_can_cast',ref='source',value=0)]
            actions += [step('cast',ref='source',value=0),step('assert_unused_choices',value=0)]+w.receipts()
        else:
            actions = w.add_protection() if mode=='own_color_protection' else []
            is_response = mode=='instant_response'
            if is_response:
                actions += w.cast_card('parent')+[step('announce_spell',ref='parent')]
            actions += w.cast_card('source','victim',response=is_response)+w.effects()
            if mode in RESPONSES:
                actions += w.response()
            if mode=='countered':
                w.finish('source')
                tail = [step('resolve')]+w.receipts()
            else:
                tail = w.resolve_source(response=is_response)
            if mode=='pending_snapshot':
                actions += [step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]
                tail += [step('assert_recorded_state')]
            actions += tail
        result.append(dict(name=f'PowerBasedSelfDamageRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                           setup=w.setup,steps=w.pre+actions,_recipe=dict(family=FAMILY,version=2,variant=variant,
                           params=declaration['params'],oracle_text=declaration['oracle_text'],scope_gaps=GAPS)))
    return result
