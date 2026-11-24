"""Instant-speed mana and draw through real casting, payment, and resolution."""
import copy
import json
import re
from pathlib import Path

from death_counter_extended_recipes import World
from death_counter_trigger_recipes import exact

FAMILY = 'egg_mana_draw'
PROFILE = Path(__file__).parent / 'data/egg_mana_draw_wave8_profiles.json'
COLORS = {'B':1,'U':2,'G':3,'R':4,'W':5}
# Representative colored spells have no target selection. The real basic in
# each fixture keeps the payment action legal even when the Egg is refused.
PAYMENT = {2:('Island','Reach Through Mists'),3:('Forest','Fog'),4:('Mountain','Rite of Flame')}
MODES = ('exact_two','funded','off_turn','respond_to_spell','cast_then_activate','unfunded','tapped',
         'foreign_owned','last_card','empty_library','two_eggs','pending_snapshot','ordinary_spell_payment')
GAPS = ['both actors exercise real spell payment; the direct human payment-source attempt requires the human selector',
        'independent ability countering requires its own activation-stack targeting control']


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or
            record.get('type_line')!='Artifact' or record.get('mana_cost')!='{1}' or record.get('keywords')!=[]):
        raise ValueError('normal whole one-mana artifact required')
    if record['oracle_text'] == '{1}, {T}, Sacrifice this artifact: Add one mana of any color. Draw a card. (Activate only as an instant.)':
        return dict(schema=1,activation_generic=1,tap=True,sacrifice=True,output_colors=list(COLORS.values()),draw=1,timing='instant')
    ability = re.fullmatch(r'\{2\}, \{T\}, Sacrifice this artifact: Add \{([WUBRG])\}\{([WUBRG])\}\. Draw a card\. \(Activate only as an instant\.\)',
                           record.get('oracle_text',''))
    if not ability or ability[1]==ability[2]:
        raise ValueError('complete instant-speed Egg cost, colors, and draw required')
    output = [0]*6
    for symbol in ability.groups(): output[COLORS[symbol]] += 1
    return dict(schema=1,activation_generic=2,tap=True,sacrifice=True,output=output,draw=1,timing='instant')


def spec(declaration,record):
    from recipes import cost_from_oracle

    expected = parameters(record)
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2 or
            declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),expected)):
        raise ValueError('typed complete pinned Egg declaration required')
    model = dict(expected,family=FAMILY,mana=cost_from_oracle(record))
    selected = declaration.get('variants')
    if not isinstance(selected,list) or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('distinct reviewed Egg variants required')
    return model


def variants(model):
    if 'output_colors' in model:
        modes = ['exact_one' if mode=='exact_two' else mode for mode in MODES]
        modes.extend('color_'+symbol for symbol in COLORS)
        return [f'{mode}_{p}' for p in (0,1) for mode in modes]
    return [f'{mode}_{p}' for p in (0,1) for mode in MODES]+['payment_window_probe_0']


def validate_witnesses(oracle,registry):
    data = profiles()
    for name,pin in data['witnesses'].items():
        if not exact(oracle.get(name),pin['oracle']) or not exact(registry.get(name),pin['registry']):
            raise ValueError('Egg witness pin changed: '+name)
    for name,pin in data['sources'].items():
        row = registry.get(name,{})
        if (not exact(oracle.get(name),pin['oracle']) or not exact(row,pin['registry']) or
                row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
            raise ValueError('implemented normal Egg source changed: '+name)
        parameters(oracle[name])


class EggWorld(World):
    def __init__(self,name,controller,model,registry,oracle,mode):
        from recipes import card, cost_from_oracle, step

        super().__init__(name,controller,model,registry,oracle)
        self.mode = mode
        self.enemy = 1-controller
        zone = 'hand' if mode=='cast_then_activate' else 'battlefield'
        self.add(name,'source',controller,zone,foreign=mode=='foreign_owned')
        if mode=='tapped':
            self.setup[-1]['state'] = ['tapped']
            self.entities['source']['tapped'] = 1
        if mode=='two_eggs':
            self.add(name,'second',controller)
        if mode=='respond_to_spell':
            self.add('Reach Through Mists','parent',self.enemy,'hand')
        if mode in ('ordinary_spell_payment','payment_window_probe'):
            payment_color = next(color for color in PAYMENT if model['output'][color])
            land,spell = PAYMENT[payment_color]
            self.add(land,'ordinary_mana_source',controller)
            self.add(spell,'spender',controller,'hand')
            assert cost_from_oracle(oracle[spell])==[int(c==payment_color) for c in range(6)]
        if 'selected_color' in model:
            self.pre.append(step('present_ai_dialogs',value=1))
        if mode in ('last_card','empty_library'):
            self.setup = [item for item in self.setup if item['player']!=controller or item['zone']!='library']
            self.zones['library'][controller] = [] if mode=='empty_library' else ['Forest']
            if mode=='last_card':
                self.setup.append(card('Forest','final_library_card',controller,'library'))
        if mode in ('off_turn','respond_to_spell'):
            self.pre[1] = step('turn',player=self.enemy,value=0)

    def effects(self):
        return [receipt for key,e in self.entities.items() if e['alive'] for receipt in self.stats(key)]+self.mana_checks()

    def receipts(self):
        return super().receipts()+self.effects()

    def draw(self,player):
        library = self.zones['library'][player]
        if library:
            self.zones['hand'][player].append(library.pop(0))
        else:
            # The host's real empty-library loss callback sets this sentinel.
            self.life[player] = -99

    def cast_egg(self):
        from recipes import step

        player = self.controller
        run = self.fund(player,self.model['mana'])+[step('cast',ref='source',value=1),step('assert_stack',value=1),
               step('assert_unused_choices',value=0),step('resolve')]
        self.zones['hand'][player].remove(self.name)
        self.mana[player] = [0]*6
        self.source['alive'] = True
        return run+self.receipts()

    def cast_parent(self):
        from recipes import cost_from_oracle, step

        enemy = self.enemy
        parent = self.entities['parent']['name']
        run = self.fund(enemy,cost_from_oracle(self.oracle[parent]))+[step('cast',ref='parent',value=1),
               step('assert_stack',value=1),step('announce_spell',ref='parent'),step('assert_unused_choices',value=0)]
        self.zones['hand'][enemy].remove(parent)
        self.mana[enemy] = [0]*6
        return run

    def activate_egg(self,ref,stack,legal):
        from recipes import step

        player = self.controller
        run = [step('can_activate',ref=ref,value=int(legal))]
        if legal and 'selected_color' in self.model:
            run.append(step('choose_color',player=player,value=self.model['selected_color']))
        # Probe Sphere availability without attempting a color choice at zero mana.
        if 'selected_color' not in self.model or self.mode!='unfunded':
            run.append(step('activate',ref=ref,value=int(legal)))
        if 'selected_color' not in self.model:
            run.append(step('assert_unused_choices',value=0))
        run.append(step('assert_stack',value=stack))
        if legal:
            self.mana[player][0] -= self.model['activation_generic']
            self.leave(ref)
        source_id = self.registry[self.name]['csvid']
        for who in (0,1):
            for zone in ('graveyard','battlefield'):
                n = (self.zones[zone][who].count(self.name) if zone=='graveyard' else
                     sum(e['alive'] and e['player']==who and e['name']==self.name for e in self.entities.values()))
                run.append(step('assert_zone_multiplicity',player=who,zone=zone,index=source_id,value=n))
            run.append(step('assert_zone',player=who,zone='library',value=len(self.zones['library'][who])))
        return run+self.mana_checks()

    def resolve_eggs(self,count):
        from recipes import step

        p = self.controller
        for _ in range(count):
            self.mana[p] = [a+b for a,b in zip(self.mana[p],self.model['output'])]
            self.draw(p)
        if self.mode=='respond_to_spell':
            self.zones['graveyard'][self.enemy].append(self.entities['parent']['name'])
            self.draw(self.enemy)
        return [step('resolve'),step('assert_unused_choices',value=0)]+self.receipts()

    def spell_payment(self):
        from recipes import step

        p = self.controller
        sequence = self.fund(p,[4,0,0,0,0,0])
        if self.mode=='payment_window_probe':
            # Attempt the actual source click during the ordinary payment loop.
            # The engine still applies its normal eligibility check. The next
            # click pays with a genuinely available basic if the Egg is denied.
            sequence.append(step('choose_mana_source',ref='source'))
        if p==0:
            sequence.append(step('choose_mana_source',ref='ordinary_mana_source'))
        sequence += [step('cast',ref='spender',value=1),step('assert_unused_choices',value=0),
                     step('assert_stack',value=1),step('assert_zone',player=p,zone='library',value=len(self.zones['library'][p]))]
        self.entities['ordinary_mana_source']['tapped'] = 1
        sequence += self.effects()
        spender = self.entities['spender']['name']
        self.zones['hand'][p].remove(spender)
        self.zones['graveyard'][p].append(spender)
        if spender=='Reach Through Mists':
            self.draw(p)
        elif spender=='Rite of Flame':
            self.mana[p][4] += 2
        return sequence+[step('resolve'),step('assert_unused_choices',value=0)]+self.receipts()


def expand(name,csvid,declaration,model,selected,registry,oracle):
    from recipes import step

    validate_witnesses(oracle,registry)
    pin = profiles()['sources'].get(name,{})
    if pin.get('registry',{}).get('csvid')!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unreserved Egg source or variant')
    result = []
    for variant in selected:
        mode,player = variant.rsplit('_',1)
        case_model = model
        sphere = 'output_colors' in model
        if sphere:
            color = COLORS[mode.removeprefix('color_')] if mode.startswith('color_') else COLORS['G']
            case_model = dict(model,selected_color=color,output=[int(i==color) for i in range(6)])
        w = EggWorld(name,int(player),case_model,registry,oracle,mode)
        actions = []
        if mode in ('ordinary_spell_payment','payment_window_probe'):
            actions += w.spell_payment()
        else:
            if mode=='cast_then_activate':
                actions += w.cast_egg()
            elif mode=='respond_to_spell':
                actions += w.cast_parent()
            fee = model['activation_generic']
            funding = {'exact_two':fee,'exact_one':fee,'unfunded':fee-1}.get(mode,4)
            actions += w.fund(w.controller,[funding,0,0,0,0,0])
            if sphere and mode=='unfunded':
                actions.append(step('refresh'))
            legal = mode not in ('unfunded','tapped')
            pending = 2 if mode=='respond_to_spell' else int(legal)
            actions += w.activate_egg('source',pending,legal)
            count = int(legal)
            if mode=='two_eggs':
                actions += w.activate_egg('second',2,True)
                count += 1
            tail = w.resolve_eggs(count)
            if mode=='pending_snapshot':
                actions += [step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]
                tail += [step('assert_recorded_state')]
            actions += tail
        gaps = GAPS if not sphere else ['explicit attempt to use the Sphere during payment','ability countering']
        result.append(dict(name=f'EggManaDrawRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                           setup=w.setup,steps=w.pre+actions,_recipe=dict(family=FAMILY,version=2,variant=variant,
                           params=declaration['params'],oracle_text=declaration['oracle_text'],scope_gaps=gaps)))
    return result
