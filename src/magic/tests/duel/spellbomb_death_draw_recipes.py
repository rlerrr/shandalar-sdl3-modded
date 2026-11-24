"""Scars Spellbomb actions and optional paid draw from their own departure."""
import copy
import json
import re
from pathlib import Path

from death_counter_extended_recipes import World
from death_counter_trigger_recipes import exact

FAMILY = 'spellbomb_death_draw'
PROFILE = Path(__file__).parent / 'data/spellbomb_death_draw_wave8_profiles.json'
BASICS = {'B':(1,'Swamp'),'U':(2,'Island'),'G':(3,'Forest'),'R':(4,'Mountain'),'W':(5,'Plains')}
EFFECTS = {
    'Target creature gains flying until end of turn.': ('flying',0,'U'),
    'Search your library for a basic land card, reveal it, put it into your hand, then shuffle.': ('basic_search',2,'G'),
    "Exile target player's graveyard.": ('grave_exile',0,'B'),
    'Create a 1/1 colorless Myr artifact creature token.': ('myr',1,'W'),
    "Target creature can't block this turn.": ('no_block',0,'R'),
}
COMMON = ('activate_pay','activate_unfunded','activate_wrong_color','destroy_pay','destroy_wrong_color',
          'exile','bounce','cast_then_activate','tapped','foreign_owned_destroy','pending_snapshot')
EXTRA = {'flying':('effect_enemy','expired','no_target'), 'no_block':('effect_enemy','expired','no_target'),
         'basic_search':('no_basic',), 'grave_exile':('self_target','empty_target'), 'myr':('activation_unaffordable',)}
REMOVAL = {'destroy_pay':('Naturalize','graveyard'), 'destroy_wrong_color':('Naturalize','graveyard'),
           'destroy_decline':('Naturalize','graveyard'), 'foreign_owned_destroy':('Naturalize','graveyard'),
           'exile':('Scour from Existence','exile'), 'bounce':('Boomerang','hand')}
PAY_CANCELED = {'activate_decline','destroy_decline'}
PAY_NO_SOURCE = {'activate_unfunded','activate_wrong_color','destroy_wrong_color'}
SCOPE_GAPS = ['independent priority window between the death trigger and original ability',
              'AI voluntary decline requires an actual independently observed selector',
              'Horizon public reveal presentation has no observer; the basic identity and destination are checked']


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or
            record.get('type_line')!='Artifact' or record.get('mana_cost')!='{1}' or record.get('keywords')!=[]):
        raise ValueError('normal whole one-mana Spellbomb required')
    prefix,sep,death = record.get('oracle_text','').partition('\n')
    active = re.fullmatch(r'(?:\{([12])\}, )?\{T\}, Sacrifice this artifact: (.+)',prefix)
    payment = re.fullmatch(r'When this artifact is put into a graveyard from the battlefield, you may pay \{([WUBRG])\}\. If you do, draw a card\.',death)
    if not sep or not active or not payment or active[2] not in EFFECTS:
        raise ValueError('complete reviewed activation and optional death draw required')
    effect,fee,color = EFFECTS[active[2]]
    if fee!=int(active[1] or 0) or color!=payment[1]:
        raise ValueError('activation fee and paying color must match its whole effect')
    return dict(schema=1,active_effect=effect,activation_generic=fee,tap=True,sacrifice=True,draw_color=color)


def spec(declaration,record):
    from recipes import cost_from_oracle

    expected = parameters(record)
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2 or
            declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),expected)):
        raise ValueError('typed complete Spellbomb declaration required')
    model = dict(expected,family=FAMILY,mana=cost_from_oracle(record))
    selected = declaration.get('variants')
    if not isinstance(selected,list) or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('distinct supported Spellbomb variants required')
    return model


def variants(model):
    effect = model['active_effect']
    stems = COMMON+EXTRA[effect]
    caps = ['activate_decline_0','destroy_decline_0']
    if effect=='basic_search':
        caps.append('search_decline_0')
    return [f'{mode}_{p}' for p in (0,1) for mode in stems]+caps


def validate_witnesses(oracle,registry):
    data = profiles()
    pins = data['witnesses']
    for name,pin in pins.items():
        if not exact(oracle.get(name),pin['oracle']) or not exact(registry.get(name),pin['registry']):
            raise ValueError('Spellbomb fixture identity changed: '+name)
    token = pins['Myr']['oracle']
    if (token['type_line']!='Token Artifact Creature — Myr' or token['power']!='1' or token['toughness']!='1' or
            token['colors'] or pins['Myr']['registry']['csvid']!=18792):
        raise ValueError('the registered colorless 1/1 Myr artifact token is required')
    for name,pin in data['sources'].items():
        row = registry.get(name,{})
        if (not exact(oracle.get(name),pin['oracle']) or not exact(row,pin['registry']) or
                row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
            raise ValueError('implemented normal Spellbomb source changed: '+name)
        parameters(oracle[name])


class SpellbombWorld(World):
    def __init__(self,name,controller,model,registry,oracle,mode):
        from recipes import card, step

        super().__init__(name,controller,model,registry,oracle)
        self.mode = mode
        self.effect = model['active_effect']
        self.enemy = 1-controller
        self.active_player = self.enemy if mode=='effect_enemy' else controller
        self.tokens = 0
        self.grave_target = controller if mode=='self_target' else self.enemy
        self.add(name,'source',controller,'hand' if mode=='cast_then_activate' else 'battlefield',
                 foreign=mode=='foreign_owned_destroy')
        if mode=='tapped':
            self.setup[-1]['state'] = ['tapped']
            self.source['tapped'] = 1
        if self.effect in ('flying','no_block') and mode!='no_target':
            self.add('Hill Giant','host',self.active_player)
            self.add('Runeclaw Bear','blocker',1-self.active_player)
        if self.effect=='basic_search':
            # Put the death-draw witness above the distinct basic.
            # Radiant Fountain is a nonbasic land and must stay in the library.
            deck = ['Hill Giant','Radiant Fountain','Giant Growth']
            if mode!='no_basic':
                deck.insert(1,'Forest')
            self.setup = [c for c in self.setup if c['player']!=controller or c['zone']!='library']
            self.zones['library'][controller] = deck
            self.setup += [card(n,f'search_library_{i}',controller,'library') for i,n in enumerate(deck)]
        if mode=='empty_target':
            self.setup = [c for c in self.setup if c['player']!=self.enemy or c['zone']!='graveyard']
            self.zones['graveyard'][self.enemy] = []
        # Keep correct payment capacity separate from the generic activation
        # pool, so affordability is observable even when generic costs are paid.
        self.has_payer = mode not in PAY_NO_SOURCE and mode!='activation_unaffordable'
        if self.has_payer:
            self.add(BASICS[model['draw_color']][1],'draw_land',controller)
        if mode in REMOVAL:
            self.add(REMOVAL[mode][0],'removal',self.enemy,'hand')
        self.pre[1] = step('turn',player=self.active_player,value=0)

    def effects(self):
        return [s for key,e in self.entities.items() if e['alive'] for s in self.stats(key)]+self.mana_checks()

    def receipts(self):
        from recipes import step

        checks = super().receipts()+self.effects()
        if self.effect!='myr':
            return checks
        tid = self.registry['Myr']['csvid']
        for s in checks:
            if s['op']=='assert_zone' and s.get('zone')=='battlefield' and s['player']==self.controller:
                s['value'] += self.tokens
        for p in (0,1):
            for zone in ('battlefield','graveyard','exile','hand','library'):
                checks.append(step('assert_zone_multiplicity',player=p,zone=zone,index=tid,
                                   value=self.tokens if p==self.controller and zone=='battlefield' else 0))
        if self.tokens:
            expected = {'token':1,'artifact':1,'color':0,'power':1,'toughness':1,'subtype_Myr':1,'tapped':0}
            checks += [step('assert_matching_permanents',player=self.controller,index=tid,property=k,value=v)
                       for k,v in expected.items()]
        return checks

    def cast_source(self):
        from recipes import step

        p = self.controller
        run = self.fund(p,self.model['mana'])+[step('cast',ref='source',value=1),step('assert_stack',value=1),
                                               step('resolve'),step('assert_unused_choices',value=0)]
        self.zones['hand'][p].remove(self.name)
        self.mana[p] = [0]*6
        self.source['alive'] = True
        return run+self.receipts()

    def death_payment(self):
        from recipes import step

        p = self.controller
        if not self.has_payer:
            return []
        if self.mode in PAY_CANCELED:
            return [step('choose_cancel_mana',player=p)]
        self.entities['draw_land']['tapped'] = 1
        self.zones['hand'][p].append(self.zones['library'][p].pop(0))
        return [step('choose_mana_source',ref='draw_land')] if p==0 else []

    def activate(self):
        from recipes import step

        p = self.controller
        fee = self.model['activation_generic']
        wrong = self.mode=='activate_wrong_color'
        pool = [0 if self.mode=='activation_unaffordable' else fee,0,0,0,0,0]
        if wrong:
            needed = BASICS[self.model['draw_color']][0]
            pool[next(color for color in range(1,6) if color!=needed)] = 1
        run = self.fund(p,pool)
        allowed = self.mode not in ('tapped','no_target','activation_unaffordable')
        run.append(step('can_activate',ref='source',value=int(allowed)))
        if allowed:
            if self.effect in ('flying','no_block'):
                ref = 'host' if self.effect=='flying' else 'blocker'
                run.append(step('choose_target',player=p,ref=ref))
            elif self.effect=='grave_exile':
                run.append(step('choose_player',player=p,value=self.grave_target))
            run += self.death_payment()
            self.mana[p][0] -= fee
            self.leave('source')
        run.append(step('activate',ref='source',value=int(allowed)))
        return run,allowed

    def removal(self):
        from recipes import cost_from_oracle, step

        p = self.controller
        spell,destination = REMOVAL[self.mode]
        run = []
        if self.mode=='destroy_wrong_color':
            needed = BASICS[self.model['draw_color']][0]
            pool = [0]*6
            pool[next(color for color in range(1,6) if color!=needed)] = 1
            run += self.fund(p,pool)
        run += self.fund(self.enemy,cost_from_oracle(self.oracle[spell]))
        run += [step('choose_target',player=self.enemy,ref='source'),step('cast',ref='removal',value=1),
                step('assert_stack',value=1),step('assert_unused_choices',value=0)]
        if destination=='graveyard':
            run += self.death_payment()
        self.leave('source',destination)
        self.zones['hand'][self.enemy].remove(spell)
        self.zones['graveyard'][self.enemy].append(spell)
        self.mana[self.enemy] = [0]*6
        return run+[step('resolve'),step('assert_unused_choices',value=0)]

    def resolve(self,allowed):
        from recipes import step

        run = []
        p = self.controller
        if allowed and self.effect=='basic_search':
            deck = self.zones['library'][p]
            basic = next((n for n in deck if self.oracle[n]['type_line'].startswith('Basic Land ')),None)
            run.append(step('observe_search_filters',value=1))
            if basic and self.mode!='search_decline':
                run.append(step('choose_zone_card',player=p,value=self.registry[basic]['csvid']))
                deck.remove(basic)
                self.zones['hand'][p].append(basic)
            elif deck:
                run.append(step('choose_optional_zone',player=p,value=self.registry[basic or deck[0]]['csvid']))
            self.shuffles[p] += 1
        elif allowed and self.effect=='grave_exile':
            target = self.grave_target
            self.zones['exile'][target] += self.zones['graveyard'][target]
            self.zones['graveyard'][target] = []
        elif allowed and self.effect=='myr':
            self.tokens += 1
        run += [step('resolve'),step('assert_unused_choices',value=0)]
        if allowed and self.effect in ('flying','no_block'):
            if self.mode=='expired':
                run += self.age()
            run += self.combat()
        return run

    def combat(self):
        from recipes import observe, step

        # Flight makes the selected ground attacker airborne. Panic makes
        # the otherwise legal ground blocker unavailable. Real expiry makes
        # the same block possible in the next turn.
        can_block = self.mode=='expired'
        self.entities['host']['tapped'] = 1
        if can_block:
            self.entities['host']['damage'] = 2
            self.leave('blocker')
        else:
            self.life[1-self.active_player] -= 3
        return [step('begin_attackers'),step('attack',ref='host',value=1),step('finish_attackers'),step('begin_blockers'),
                step('block',ref='blocker',other='host',value=int(can_block)),step('advance_combat'),
                observe('host','blocked',int(can_block)),step('combat_damage'),step('resolve'),
                step('assert_unused_choices',value=0),step('end_combat')]


def expand(name,csvid,declaration,model,selected,registry,oracle):
    from recipes import step

    validate_witnesses(oracle,registry)
    pin = profiles()['sources'].get(name,{})
    if pin.get('registry',{}).get('csvid')!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unreserved Spellbomb source or variant')
    result = []
    for variant in selected:
        mode,player = variant.rsplit('_',1)
        w = SpellbombWorld(name,int(player),model,registry,oracle,mode)
        actions = []
        if mode in REMOVAL:
            actions += w.removal()+w.receipts()
        else:
            if mode=='cast_then_activate':
                actions += w.cast_source()
            stimulus,allowed = w.activate()
            actions += stimulus
            follow = w.resolve(allowed)+w.receipts()
            if mode=='pending_snapshot':
                actions += [step('capture')]+copy.deepcopy(follow)+[step('record_state'),step('ai_roundtrip'),step('restore')]
                follow += [step('assert_recorded_state')]
            actions += follow
        result.append(dict(name=f'SpellbombDeathDrawRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                           setup=w.setup,steps=w.pre+actions,_recipe=dict(family=FAMILY,version=2,variant=variant,
                           params=declaration['params'],oracle_text=declaration['oracle_text'],scope_gaps=SCOPE_GAPS)))
    return result
