"""Fixed token creation at actual permanent deaths and related printed events."""
import copy
import json
import re
from pathlib import Path

from death_counter_trigger_recipes import exact
from death_modular_transfer_recipes import World as ModularWorld


FAMILY = 'death_fixed_token_creation'
COLORS = {'colorless': 0, 'black': 2, 'green': 8}
COUNTS = {'a': 1, 'four': 4}
TRIGGERS = {
    'When this creature dies during combat': 'self_combat',
    'When this creature dies': 'self',
    'When this artifact enters or is put into a graveyard from the battlefield': 'self_artifact_entry',
    'When this land is put into a graveyard from the battlefield': 'self_land',
    'When a creature you control dies': 'controlled_sacrifice',
    'Whenever this creature or another Human creature you control dies': 'controlled_human',
    'Whenever another nontoken creature you control dies': 'controlled_another_nontoken',
}
GAPS = ['independent response/order window between multiple pending death triggers',
        'Dog subtype lacks a named observer, so Mongrel Pack uses the registered Dog identity',
        'land-play priority is exercised through Walking Atlas entry and actual mana use']


def pins():
    return json.loads((Path(__file__).parent/'data/death_fixed_token_creation_wave8_profiles.json').read_text(encoding='utf8'))


def oracle_model(record):
    if not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal fixed death token maker required')
    lines = record.get('oracle_text', '').split('\n')
    keywords = []
    if lines[0] == 'Haste': keywords.append(lines.pop(0))
    mana = lines[0] == '{T}: Add {C}.'
    if mana: lines.pop(0)
    if len(lines) != 1 or record.get('keywords') != keywords:
        raise ValueError('whole reviewed source clauses required')
    head, sep, body = lines[0].partition(', ')
    if head == 'When Tuktuk the Explorer dies' and record['name'] == 'Tuktuk the Explorer': head = 'When this creature dies'
    if not sep or head not in TRIGGERS:
        raise ValueError('reviewed death predicate required')
    trigger = TRIGGERS[head]
    kind = {'self_artifact_entry':'Artifact','self_land':'Land','controlled_sacrifice':'Enchantment'}.get(trigger,'Creature')
    if kind not in record['type_line'].split(' — ')[0].split() or (trigger == 'self_land') != mana:
        raise ValueError('reviewed source kind and printed mana ability required')
    if trigger == 'controlled_sacrifice':
        prefix = 'sacrifice this enchantment. If you do, '
        if not body.startswith(prefix): raise ValueError('printed sacrifice requirement missing')
        body = body.removeprefix(prefix)
    named = re.fullmatch(r'create (Tuktuk the Returned), a legendary 5/5 colorless Goblin Golem artifact creature token\.', body)
    if named and trigger == 'self' and keywords == ['Haste']:
        token = dict(name=named[1], count=1, power=5, toughness=5, color=0, artifact=True,
                     subtypes=['Goblin','Golem'], tapped=False, legendary=True)
    elif trigger == 'controlled_another_nontoken':
        if body != 'create a 3/1 black and red Graveborn creature token with haste.' or keywords:
            raise ValueError('complete Graveborn descriptor with haste required')
        token = dict(name='Graveborn', count=1, power=3, toughness=1, color=18, artifact=False,
                     subtypes=['Graveborn'], tapped=False, legendary=False, haste=True)
    else:
        match = re.fullmatch(r'create (a|four) (tapped )?([12])/([12]) (colorless|black|green) (Dog|Servo|Spirit|Zombie)( artifact)? creature tokens?\.',body)
        if not match or keywords: raise ValueError('complete fixed token descriptor required')
        count,tapped,power,toughness,color,subtype,artifact = match.groups()
        token = dict(name=subtype,count=COUNTS[count],power=int(power),toughness=int(toughness),color=COLORS[color],
                     artifact=bool(artifact),subtypes=[subtype],tapped=bool(tapped),legendary=False)
    return dict(trigger=trigger,kind=kind,keywords=keywords,mana_ability=mana,token=token)


def spec(declaration, record):
    from recipes import cost_from_oracle

    expected = dict(schema=1, **oracle_model(record))
    if (declaration.get('family') != FAMILY or declaration.get('version') != 2 or
            declaration.get('oracle_text') != record.get('oracle_text') or not exact(declaration.get('params'),expected)):
        raise ValueError('typed Oracle-derived death token declaration required')
    mana = [0]*6 if expected['mana_ability'] else cost_from_oracle(record)
    model = dict(expected,family=FAMILY,mana=mana)
    selection = declaration.get('variants')
    if not isinstance(selection,list) or not selection or len(set(selection))!=len(selection) or set(selection)-set(variants(model)):
        raise ValueError('explicit unique supported death token variants required')
    return model


def variants(model):
    modes = ['effect','owned_elsewhere','bounce','exile','kill_created','ready_snapshot','pending_snapshot','resolved_snapshot']
    modes += ['land_mana','land_entry','death_after_mana'] if model['mana_ability'] else ['cast_only','unaffordable','countered']
    modes += {
        'self_combat':['noncombat_death','combat_blocked','other_death'],
        'self':['fresh_haste','legend_once','other_death'],
        'self_artifact_entry':['entry_then_death','other_death'],
        'self_land':['other_death'],
        'controlled_sacrifice':['opponent_death','noncreature_death','simultaneous','source_departed','token_victim','stolen_victim','cost_death'],
        'controlled_human':['own_human','opponent_death','nonhuman','simultaneous','source_departed','token_victim','stolen_victim'],
        'controlled_another_nontoken':['self_death','opponent_death','noncreature_death','source_departed',
                                      'token_victim','stolen_victim','fresh_token_haste','source_simultaneous','repeated_deaths'],
    }[model['trigger']]
    return [f'{mode}_{p}' for p in (0,1) for mode in modes]


def validate_witnesses(oracle, registry):
    data = pins()
    for name, record in data['cards'].items():
        if not exact(record,oracle.get(name)) or not exact(data['registry'][name],registry.get(name)):
            raise ValueError('fixed death token witness changed: '+name)
    for name, source in data['sources'].items():
        row = registry.get(name,{})
        definition = oracle_model(source['oracle'])['token']
        token_name = definition['name']
        if (not exact(source['oracle'],oracle.get(name)) or not exact(source['registry'],row) or
                row.get('kind')!='card' or row.get('handler') in ('NULL','0','card_dummy') or
                not exact(source['token_registry'],registry.get(token_name)) or
                not exact(source['token_oracle_if_available'],oracle.get(token_name))):
            raise ValueError('implemented normal source or runtime token identity changed: '+name)


class World(ModularWorld):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs)
        self.forced_targets = 0
        self.count = 0
        self.token = self.model['token']
        self.token_id = self.registry[self.token['name']]['csvid']

    def token_checks(self):
        from recipes import step

        p = self.controller
        checks = [step('assert_zone_multiplicity',player=q,zone='battlefield',index=self.token_id,
                       value=self.count if q==p else 0) for q in (0,1)]
        if self.count:
            descriptor = {k:self.token[k] for k in ('power','toughness','color','artifact','tapped')}
            descriptor.update(token=1,plus1_counters=0)
            descriptor.update({'subtype_'+name:1 for name in self.token['subtypes'] if name not in ('Dog','Graveborn')})
            checks += [step('assert_matching_permanents',player=p,index=self.token_id,property=key,value=int(value))
                       for key,value in descriptor.items()]
        return checks

    def effects(self):
        return super().effects()+self.token_checks()

    def receipts(self):
        from recipes import step

        checks = super().receipts()+self.effects()
        for receipt in checks:
            if receipt['op']=='assert_zone' and receipt.get('zone')=='battlefield' and receipt.get('player')==self.controller:
                receipt['value'] += self.count
        checks += [step('assert_zone_multiplicity',player=p,zone=z,index=self.token_id,value=0)
                   for p in (0,1) for z in ('hand','graveyard','exile','library')]
        return checks

    def create(self, events=1):
        self.count += events*self.token['count']
        if self.token['legendary']:
            self.count = min(self.count,1)

    def cast_source(self, key='source', pending=False):
        from recipes import step

        p = self.controller
        run = self.fund(p,self.model['mana'])+[step('cast',ref=key,value=1),step('assert_stack',value=1)]
        self.zones['hand'][p].remove(self.name); self.mana[p] = [0]*6
        if not pending:
            self.entities[key]['alive'] = True
            if self.model['trigger']=='self_artifact_entry': self.create()
            run += [step('resolve'),step('assert_choices_consumed')]+self.effects()
        return run

    def death(self, keys, combat=False):
        model = self.model['trigger']; p = self.controller
        if model in ('controlled_sacrifice','controlled_human','controlled_another_nontoken'):
            events = 0
            if self.source['alive']:
                for key in keys:
                    e = self.entities[key]; type_line = self.oracle[e['name']]['type_line']; kind,_,subtypes = type_line.partition(' — ')
                    if e['player']!=p or 'Creature' not in kind.split():
                        continue
                    if model=='controlled_human' and 'Human' not in subtypes.split():
                        continue
                    if model=='controlled_another_nontoken' and (key=='source' or e['token']):
                        continue
                    events += 1
        else:
            events = sum(self.entities[key]['name']==self.name for key in keys)
            if model=='self_combat' and not combat: events = 0
        for key in keys: self.leave(key)
        if model=='controlled_sacrifice':
            events = int(events > 0 and self.source['alive'])
            if events: self.leave('source')
        self.create(events)

    def remove(self, key, spell=None, ref='removal', combat=False, legend=False, caster=None):
        from recipes import step

        kind = self.oracle[self.entities[key]['name']]['type_line'].split(' — ')[0].split()
        spell = spell or ('Stone Rain' if 'Land' in kind else 'Murder' if 'Creature' in kind else 'Naturalize')
        run = self.spell(spell,ref,(key,),caster=caster); cut = len(run)
        if spell == 'Boomerang': self.leave(key,'hand')
        elif spell == 'Scour from Existence': self.leave(key,'exile')
        else: self.death([key],combat=combat)
        if legend: run += [step('choose_target',player=self.controller,ref='old_token')]
        return run+[step('resolve'),step('assert_choices_consumed')]+self.effects(),cut


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step, cost_from_oracle

    validate_witnesses(oracle,registry)
    pin = pins()['sources'].get(name,{})
    if pin.get('registry',{}).get('csvid')!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unreserved fixed death token source or variant')
    result = []
    for variant in selected:
        mode,player = variant.rsplit('_',1); p=int(player); q=1-p
        world=World(name,p,model,registry,oracle)
        kind=model['trigger']
        foreign=mode=='owned_elsewhere'; seeded=foreign or model['mana_ability'] and mode!='land_entry'
        world.add(name,'source',p,'battlefield' if seeded else 'hand',foreign=foreign)
        if kind=='self_artifact_entry' and seeded: world.create()
        action=[]; ready=None; pending=None; key='source'
        needs_victim = (kind=='controlled_sacrifice' or
                        kind=='controlled_human' and mode in ('own_human','opponent_death','nonhuman','simultaneous','source_departed','token_victim','stolen_victim','cost_death') or
                        kind=='controlled_another_nontoken' and mode not in ('self_death','cast_only','unaffordable','countered'))
        if needs_victim:
            key='victim'
            victim = {'noncreature_death':'Howling Mine','nonhuman':'Muck Rats','cost_death':'Fume Spitter'}.get(mode,'Eager Cadet')
            world.add(victim,key,q if mode=='opponent_death' else p,foreign=mode=='stolen_victim',token=mode=='token_victim')
        if mode=='other_death':
            key='victim'; world.add('Muck Rats',key,q)
        if mode in ('simultaneous','repeated_deaths'): world.add('Eager Cadet','second',p)
        if mode=='cost_death': world.add('Runeclaw Bear','cost_target',q)
        if mode=='legend_once': world.add(name,'second_source',p,'hand')
        if mode=='combat_blocked': world.add('Runeclaw Bear','blocker',q)
        if mode=='unaffordable':
            action += [step('cast',ref='source',value=0)]
        elif mode=='countered':
            action += world.cast_source(pending=True)+[step('announce_spell',ref='source')]
            world.add('Counterspell','counter',q,'hand')
            action += world.fund(q,cost_from_oracle(oracle['Counterspell']))+[
                step('cast_response',ref='counter',value=1),step('resolve_response')]
            world.zones['hand'][q].remove('Counterspell'); world.zones['graveyard'][q].append('Counterspell'); world.mana[q]=[0]*6
            world.zones['graveyard'][p].append(name)
        elif mode=='land_entry':
            world.add('Walking Atlas','atlas',p)
            action += [step('activate',ref='atlas',value=1),step('assert_stack',value=1),
                       step('choose_zone_card',player=p,value=csvid),step('resolve'),step('assert_choices_consumed')]
            world.entities['atlas']['tapped']=1; world.source['alive']=True; world.zones['hand'][p].remove(name)
            action += world.effects()
        else:
            if not seeded: action += world.cast_source()
            else: action += world.effects()
            ready=len(action)
            if mode in ('land_mana','death_after_mana'):
                action += [step('require_can_activate',ref='source',value=1),step('activate',ref='source',value=1),
                           step('assert_stack',value=0),step('assert_choices_consumed')]
                world.source['tapped']=1; world.mana[p][0]+=1
                action += world.effects()+[step('require_can_activate',ref='source',value=0)]
            if mode in ('land_mana','cast_only'): pass
            elif mode=='simultaneous':
                action += world.spell('Pyroclasm','sweep',caster=p)
                world.death(['victim','second']+(['source'] if kind=='controlled_human' else []))
                action += [step('resolve'),step('assert_choices_consumed')]+world.effects()
            elif mode=='source_simultaneous':
                action += world.spell('Inferno','sweep',caster=p)
                world.death(['source','victim'])
                world.life = [14,14]
                action += [step('resolve'),step('assert_choices_consumed')]+world.effects()
            elif mode=='cost_death':
                world.death(['victim']); world.entities['cost_target']['minus']=1
                action += [step('choose_target',player=p,ref='cost_target'),step('activate',ref='victim',value=1),
                           step('resolve'),step('assert_choices_consumed')]+world.effects()
            elif mode=='combat_blocked':
                action += world.age()+[step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),
                    step('begin_blockers'),step('block',ref='blocker',other='source',value=1),step('advance_combat'),
                    step('combat_damage'),step('resolve'),step('assert_choices_consumed'),step('end_combat')]
                world.death(['source','blocker'],combat=True); action += world.effects()
            else:
                combat = kind=='self_combat' and mode not in ('noncombat_death','bounce','exile','other_death')
                if combat or mode=='fresh_haste':
                    if combat: action += world.age()
                    action += [step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),step('begin_blockers')]
                    world.source['tapped']=1
                    if mode=='fresh_haste':
                        world.life[q]-=int(oracle[name]['power'])
                        action += [step('advance_combat'),step('combat_damage'),step('resolve'),step('end_combat')]+world.effects()
                if mode=='source_departed':
                    action += world.spell('Boomerang','source_departure',('source',))+[step('resolve')]
                    world.leave('source','hand')
                if model['mana_ability'] and key=='source' and mode not in ('bounce','exile'):
                    action += [step('turn',player=q,value=0)]
                spell = {'bounce':'Boomerang','exile':'Scour from Existence'}.get(mode)
                run,offset=world.remove(key,spell,combat=combat)
                pending=len(action)+offset; action += run
                if combat:
                    action += [step('advance_combat'),step('combat_damage'),step('resolve'),step('end_combat')]
                if mode=='kill_created':
                    action += [step('bind_permanent_copy',player=p,value=world.token_id,index=0,ref='created_token')]
                    # The actual spell kills one generated token. It leaves no card in a graveyard.
                    action += world.spell('Murder','token_removal',())
                    at=next(i for i in range(len(action)-1,-1,-1) if action[i]['op']=='cast')
                    action.insert(at,step('choose_target',player=q,ref='created_token'))
                    world.count-=1
                    action += [step('resolve'),step('assert_choices_consumed')]+world.effects()
                if mode=='legend_once':
                    action += [step('bind_permanent_copy',player=p,value=world.token_id,index=0,ref='old_token')]
                    action += world.cast_source('second_source')
                    run,_=world.remove('second_source',ref='second_removal',legend=True); action += run
                if mode=='repeated_deaths':
                    run,_=world.remove('second',ref='second_removal'); action += run
                if mode=='fresh_token_haste':
                    action += [step('bind_permanent_copy',player=p,value=world.token_id,index=0,ref='haste_token'),
                               step('begin_attackers'),step('attack',ref='haste_token',value=1),step('finish_attackers'),
                               step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('resolve'),
                               step('assert_choices_consumed'),step('end_combat')]
                    world.life[q] -= world.token['power']
                    world.token = dict(world.token,tapped=True)
                    action += world.effects()
        checks=world.receipts(); sequence=world.pre+action+checks
        if mode in ('ready_snapshot','pending_snapshot'):
            cut=ready if mode=='ready_snapshot' else pending
            if cut is None: raise ValueError('real death-token snapshot boundary missing')
            tail=action[cut:]+checks
            sequence=(world.pre+action[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),
                     step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')])
        elif mode=='resolved_snapshot':
            sequence += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        gaps = GAPS
        if kind=='controlled_another_nontoken':
            gaps = GAPS+['Graveborn subtype lacks a named observer, so Sek’Kuar uses its registered Graveborn identity']
        result.append(dict(name=f'DeathFixedTokenCreationRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                           setup=world.setup,steps=sequence,_recipe=dict(family=FAMILY,version=2,variant=variant,
                             params=declaration['params'],oracle_text=declaration['oracle_text'],scope_gaps=gaps)))
    return result
