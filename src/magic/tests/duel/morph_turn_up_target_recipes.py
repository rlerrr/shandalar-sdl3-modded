"""Paid native Morph actions and whole printed turn-up target effects."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from morph_creature_recipes import FACE_DOWN, MORPH


FAMILY = 'morph_turn_up_target'
PROFILE = Path(__file__).parent/'data/morph_turn_up_target_wave8_profiles.json'
HIDDEN_FEE = (3,0,0,0,0,0)
CLAUSES = {
    'When this creature is turned face up, target creature gets -3/-3 until end of turn.':'shrink',
    'When this creature is turned face up, you may tap or untap target permanent.':'toggle',
    'When this creature is turned face up, destroy target enchantment.':'destroy',
    "When this creature is turned face up, return target creature to its owner's hand.":'bounce',
    'When this creature is turned face up, you may exchange control of target creature you control and target creature an opponent controls.':'exchange',
    'When this creature enters or is turned face up, another target creature you control gets +3/+0 until end of turn.':'pump',
}
BASE = ('hidden','faceup','hidden_choice','hidden_short','flip','flip_wrong','flip_short','offturn',
        'pending_spell','target_changed','replay')
SPECIAL = {
    'shrink':('own_target','nonlethal','self_target'),
    'toggle':('untap','decline','own_land'),
    'destroy':('own_target','no_target','type_probe'),
    'bounce':('own_target','self_target','foreign_owner'),
    'exchange':('decline','decline_second','no_foe','source_swap'),
    'pump':('no_target','eligibility','first_strike','hidden_combat','entry_cleanup'),
}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or 'card_faces' in record or 'selected_face' in record or
            not record.get('type_line','').startswith('Creature — ')):
        raise ValueError('whole normal Morph creature required')
    lines = record['oracle_text'].splitlines()
    matches = [(i,m) for i,line in enumerate(lines) if (m:=MORPH.fullmatch(line))]
    if len(matches)!=1:
        raise ValueError('one complete printed mana-cost Morph clause required')
    index,morph = matches[0]
    remaining = lines[:index]+lines[index+1:]
    first = remaining[:1]==['First strike']
    effects = remaining[int(first):]
    effect = CLAUSES.get(effects[0]) if len(effects)==1 else None
    if not effect or first!=(effect=='pump') or set(record['keywords'])!=({'Morph','First strike'} if first else {'Morph'}):
        raise ValueError('complete supported turn-up effect and native passive required')
    return dict(effect=effect,first_strike=first,cast_mana=cost_from_oracle(record),
                flip_mana=cost_from_oracle(dict(record,mana_cost=morph[1])),
                power=int(record['power']),toughness=int(record['toughness']))


def variants(model):
    return [f'{mode}_{owner}' for owner in (0,1) for mode in BASE+SPECIAL[model['effect']]]


def spec(declaration,record):
    model = parameters(record)
    selected = declaration.get('variants')
    source = pins()['sources'].get(record['name'],{})
    if (not exact(source.get('oracle'),record) or declaration.get('family')!=FAMILY or
            type(declaration.get('version')) is not int or declaration['version']!=2 or
            declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),model) or
            type(selected) is not list or not selected or any(type(v) is not str for v in selected) or
            len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('typed complete Morph declaration and explicit distinct variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(o,rows):
    data = pins()
    if not exact(data['hidden_observer'],rows.get('Face-Down Creature')) or data['hidden_observer']['csvid']!=FACE_DOWN:
        raise ValueError('native hidden characteristic observer changed')
    for group in ('sources','witnesses'):
        for name,pin in data[group].items():
            row,record = rows.get(name,{}),o.get(name,{})
            if (not exact(pin,{'oracle':record,'registry':row}) or record.get('layout')!='normal' or
                    row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'implemented normal Morph identity changed: {name}')


class Scene:
    def __init__(self,name,model,mode,owner,rows,oracle):
        self.name,self.model,self.mode,self.owner = name,model,mode,owner
        self.foe,self.active,self.rows,self.oracle = 1-owner,owner,rows,oracle
        self.effect,self.csvid = model['effect'],rows[name]['csvid']
        self.setup,self.steps,self.entities = [],[],{}
        self.mana,self.life = [[0]*6 for _ in (0,1)],[20,20]
        self.hidden,self.replay = False,None

    def emit(self,op,**fields):
        from recipes import step

        self.steps.append(step(op,**fields))

    def add(self,name,ref,owner,zone='battlefield',tapped=False):
        from recipes import card

        setup = card(name,ref,owner,zone)
        if tapped:
            setup['state'] = ['tapped']
        self.setup.append(setup)
        o = self.oracle[name]
        self.entities[ref] = dict(name=name,owner=owner,controller=owner,zone=zone,tapped=tapped,damage=0,
                                  base=(int(o.get('power') or 0),int(o.get('toughness') or 0)),bonus=(0,0))

    def stats(self,ref):
        e = self.entities[ref]
        base = (2,2) if ref=='source' and self.hidden else e['base']
        return tuple(a+b for a,b in zip(base,e['bonus']))

    def observed_id(self,ref):
        return FACE_DOWN if ref=='source' and self.hidden else self.rows[self.entities[ref]['name']]['csvid']

    def prop(self,ref,prop,value):
        self.emit('assert_card',ref=ref,property=prop,value=int(value))

    def source_traits(self,full=False):
        from recipes import COLORS

        self.prop('source','csvid',self.observed_id('source'))
        for key,n in zip(('power','toughness'),self.stats('source')):
            self.prop('source',key,n)
        if full:
            owner = self.entities['source']['controller']
            color = 0 if self.hidden else sum(1<<COLORS[c] for c in self.oracle[self.name]['colors'])
            for key,n in [('color',color),('artifact',0),('token',0)]:
                self.emit('assert_matching_permanents',player=owner,index=self.observed_id('source'),property=key,value=n)

    def pools(self):
        for player,pool in enumerate(self.mana):
            for color,n in enumerate(pool):
                self.emit('assert_mana',player=player,color=color,value=n)

    def receipts(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0)
        self.pools()
        for ref,e in self.entities.items():
            if e['zone']!='battlefield':
                continue
            self.prop(ref,'in_play',1)
            self.prop(ref,'tapped',e['tapped'])
            if 'Creature' in self.oracle[e['name']]['type_line'].split(' — ',1)[0]:
                power,toughness = self.stats(ref)
                for key,n in [('power',power),('toughness',toughness),('damage',e['damage'])]:
                    self.prop(ref,key,n)
        if self.entities['source']['zone']=='battlefield':
            self.source_traits(full=True)
        for player in (0,1):
            self.emit('assert_life',player=player,value=self.life[player])
            for zone in ('hand','battlefield','graveyard','exile','library'):
                csvids = Counter((self.observed_id(ref) if zone=='battlefield' else self.rows[e['name']]['csvid'])
                                 for ref,e in self.entities.items() if e['zone']==zone and
                                 e['controller' if zone=='battlefield' else 'owner']==player)
                self.emit('assert_zone',player=player,zone=zone,value=sum(csvids.values()))
                for csvid,n in csvids.items():
                    self.emit('assert_zone_multiplicity',player=player,zone=zone,index=csvid,value=n)

    def fund(self,player,budget):
        assert not any(self.mana[player])
        self.mana[player] = list(budget)
        for color,n in enumerate(budget):
            if n:
                self.emit('set_mana',player=player,color=color,value=n)
        self.emit('refresh')

    def pay(self,player,fee):
        self.mana[player] = [have-due for have,due in zip(self.mana[player],fee)]
        assert min(self.mana[player])>=0

    def leave(self,ref,zone):
        e = self.entities[ref]
        e.update(zone=zone,controller=e['owner'],damage=0,tapped=False,bonus=(0,0))
        if ref=='source':
            self.hidden = False

    def resolve(self,parent=False):
        self.emit('resolve_response' if parent else 'resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0)

    def cast_source(self):
        mode = self.mode
        faceup = mode in ('faceup','entry_cleanup')
        fee = self.model['cast_mana'] if faceup else HIDDEN_FEE
        budget = list(fee)
        if mode=='hidden_short':
            budget[0] = 2
        if mode=='hidden_choice':
            budget = list(self.model['cast_mana'])
            budget[0] = max(3,budget[0])
        self.fund(self.owner,budget)
        if mode=='hidden_choice' or (faceup and sum(fee)>=3):
            self.emit('choose_dialog',player=self.owner,value=int(faceup))
        allowed = mode!='hidden_short'
        self.emit('cast',ref='source',value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(allowed))
        if not allowed:
            self.receipts()
            return False
        self.prop('source','in_hand',0)
        self.pay(self.owner,fee)
        self.hidden = not faceup
        self.entities['source']['zone'] = 'stack'
        self.source_traits()
        self.pools()
        if faceup and self.effect=='pump':
            self.emit('choose_target',player=self.owner,ref='victim')
            self.entities['victim']['bonus'] = (3,0)
        self.resolve()
        self.entities['source']['zone'] = 'battlefield'
        self.receipts()
        return True

    def cast_spell(self,ref,target):
        from recipes import cost_from_oracle

        entity = self.entities[ref]
        player = entity['owner']
        fee = cost_from_oracle(self.oracle[entity['name']])
        self.fund(player,fee)
        self.emit('choose_target',player=player,ref=target)
        self.emit('cast',ref=ref,value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1)
        self.pay(player,fee)
        entity['zone'] = 'stack'
        self.pools()

    def bind(self,ref):
        e = self.entities[ref]
        self.emit('bind_permanent',ref=ref,player=e['controller'],value=self.observed_id(ref))

    def donate(self,ref):
        self.emit('require_can_activate',ref='trader',value=1)
        self.emit('choose_player',player=self.owner,value=self.foe)
        self.emit('choose_target',player=self.owner,ref=ref)
        self.emit('activate',ref='trader',value=1)
        self.emit('assert_stack',value=1)
        self.resolve()
        self.entities['trader']['tapped'] = True
        self.entities[ref]['controller'] = self.foe
        self.bind(ref)
        self.receipts()

    def update_before_selection(self):
        if self.effect=='exchange' or self.mode=='foreign_owner':
            self.donate('ally' if self.effect=='exchange' else 'victim')
        elif self.effect=='toggle':
            fee = [1,0,0,0,0,0]
            self.fund(self.owner,fee)
            self.emit('require_can_activate',ref='icy',value=1)
            self.emit('choose_target',player=self.owner,ref='victim')
            self.emit('activate',ref='icy',value=1)
            self.emit('assert_stack',value=1)
            self.pay(self.owner,fee)
            self.resolve()
            self.entities['icy']['tapped'] = self.entities['victim']['tapped'] = True
            self.receipts()
        else:
            self.cast_spell('removal','victim')
            self.resolve()
            self.leave('removal','graveyard')
            self.leave('victim','graveyard')
            self.receipts()

    def scheduled_targets(self):
        mode = self.mode
        if mode in ('no_target','no_foe'):
            return ()
        if mode=='decline':
            return (None,)
        if self.effect=='exchange':
            first = 'source' if mode=='source_swap' else 'fallback' if mode=='target_changed' else 'ally'
            second = None if mode=='decline_second' else 'ally' if mode=='target_changed' else 'victim'
            return first,second
        if mode=='self_target':
            return ('source',)
        return ('fallback' if mode=='target_changed' and self.effect!='toggle' else 'victim',)

    def finish_trigger(self,targets):
        if not targets or None in targets:
            return
        target = targets[0]
        e = self.entities[target]
        if self.effect in ('shrink','pump'):
            e['bonus'] = (-3,-3) if self.effect=='shrink' else (3,0)
            if self.stats(target)[1]<=0:
                self.leave(target,'graveyard')
        elif self.effect=='toggle':
            e['tapped'] = not e['tapped']
        elif self.effect=='destroy':
            self.leave(target,'graveyard')
        elif self.effect=='bounce':
            self.leave(target,'hand')
        elif self.effect=='exchange':
            other = self.entities[targets[1]]
            e['controller'],other['controller'] = other['controller'],e['controller']
            for ref in targets:
                self.bind(ref)

    def flip(self):
        mode = self.mode
        fee = self.model['flip_mana']
        budget = list(fee)
        denied = mode in ('flip_short','flip_wrong')
        if denied:
            budget[next(i for i,n in enumerate(fee) if i and n)] -= 1
            if mode=='flip_wrong':
                assert fee[3]==0
                budget[3] += 1
        self.fund(self.owner,budget)
        if mode=='replay':
            self.replay = len(self.steps)
        self.emit('can_activate',ref='source',value=int(not denied))
        if mode in ('eligibility','type_probe'):
            for ref in (('source','wrong_target') if mode=='eligibility' else ('wrong_target',)):
                self.emit('probe_activation_target',ref='source',other=ref,index=-1,value=0)
            self.emit('probe_activation_target',ref='source',other='victim',index=-1,value=1)
        targets = () if denied else self.scheduled_targets()
        for ref in targets:
            if ref is None:
                self.emit('choose_cancel_target',player=self.owner)
            else:
                self.emit('choose_target',player=self.owner,ref=ref)
        self.emit('activate',ref='source',value=int(not denied))
        self.emit('assert_choices_consumed')
        if denied:
            self.receipts()
            return
        self.pay(self.owner,fee)
        self.hidden = False
        self.emit('refresh')
        self.pools()
        if mode not in ('self_target','source_swap'):
            self.source_traits()
        pending = mode=='pending_spell'
        # Morph is paid before the printed effect. Drain the driver's legacy
        # proxy without asserting it is the rules-visible trigger.
        self.resolve(pending)
        self.finish_trigger(targets)
        if pending:
            self.leave('parent','graveyard')
            self.leave('source','hand')
        self.receipts()

    def advance(self):
        assert not any(map(any,self.mana))
        self.emit('advance_turn')
        self.active = 1-self.active
        for e in self.entities.values():
            e.update(bonus=(0,0),damage=0)

    def cleanup(self):
        self.emit('cleanup')
        self.emit('refresh')
        for e in self.entities.values():
            e.update(bonus=(0,0),damage=0)
        self.receipts()

    def strike_combat(self):
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref='blocker',other='source',value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if self.hidden:
            self.entities['blocker']['damage'] = 2
            self.leave('source','graveyard')
        else:
            self.entities['source']['tapped'] = True
            self.leave('blocker','graveyard')
        self.receipts()

    def setup_cards(self):
        mode,effect = self.mode,self.effect
        self.add(self.name,'source',self.owner,'hand')
        if mode=='pending_spell':
            self.add('Unsummon','parent',self.foe,'hand')
        if effect=='exchange':
            self.add('Goblin Tinkerer','ally',self.owner)
            if mode!='no_foe':
                self.add('Serra Angel','victim',self.foe)
            if mode=='target_changed':
                self.add('Indomitable Archangel','fallback',self.owner)
                self.add('Bazaar Trader','trader',self.owner)
            return
        if mode=='no_target':
            return
        actor = self.owner if mode in ('own_target','own_land','foreign_owner') or effect=='pump' else self.foe
        witness = 'Serra Angel' if mode=='nonlethal' else 'Goblin Tinkerer'
        if effect=='destroy':
            witness = 'Furnace of Rath'
        elif effect=='toggle':
            witness = 'Forest' if mode=='own_land' else 'Bonesplitter'
        self.add(witness,'victim',actor,tapped=mode=='untap')
        if mode=='target_changed':
            if effect=='toggle':
                self.add('Icy Manipulator','icy',self.owner)
            else:
                self.add('Disenchant' if effect=='destroy' else 'Murder','removal',self.foe,'hand')
                self.add('Propaganda' if effect=='destroy' else 'Serra Angel','fallback',actor)
        if mode=='foreign_owner':
            self.add('Bazaar Trader','trader',self.owner)
        if mode in ('eligibility','type_probe'):
            self.add('Goblin Tinkerer','wrong_target',self.foe)
        if mode in ('first_strike','hidden_combat'):
            self.add('Serra Angel','blocker',self.foe)

    def play(self):
        self.setup_cards()
        self.emit('turn',player=self.owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana',player=player,color=color,value=0)
        self.emit('refresh')
        if not self.cast_source() or self.mode in ('hidden','hidden_choice','faceup'):
            return
        if self.mode=='entry_cleanup':
            self.cleanup()
            return
        if self.mode in ('offturn','pending_spell','first_strike','hidden_combat'):
            self.advance()
            if self.mode in ('first_strike','hidden_combat'):
                self.advance()
        if self.mode=='hidden_combat':
            self.strike_combat()
            return
        if self.mode in ('target_changed','foreign_owner'):
            self.update_before_selection()
        if self.mode=='pending_spell':
            self.cast_spell('parent','source')
        self.flip()
        if self.mode=='first_strike':
            self.strike_combat()
        if self.mode in ('nonlethal','offturn','first_strike'):
            self.cleanup()


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if pins()['sources'].get(name,{}).get('registry',{}).get('csvid')!=csvid:
        raise ValueError('unreserved turn-up target source')
    cases = []
    for variant in selected:
        mode,owner = variant.rsplit('_',1)
        scene = Scene(name,model,mode,int(owner),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            tail = steps[at:]
            steps = steps[:at]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+tail+[step('assert_recorded_state')]
        gaps = ['Separate response priority after selection of an actual flip trigger target','Multiplayer']
        if model['effect']=='toggle':
            gaps.append('Separate tap/untap selection and no-change choice after a target is selected')
        if model['effect']=='exchange':
            gaps.append('Refusing the exchange after both targets were selected')
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                        oracle_text=declaration['oracle_text'],covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=gaps)
        cases.append(dict(name=f'MorphTurnUpTargetRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                          setup=scene.setup,steps=steps,_recipe=metadata))
    return cases
