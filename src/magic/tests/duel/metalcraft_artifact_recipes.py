"""Live artifact thresholds and complete Metalcraft effects."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact


FAMILY = 'metalcraft_artifact'
PROFILE = Path(__file__).parent/'data/metalcraft_artifact_wave8_profiles.json'
CLAUSES = {
    "{U}: Exile this creature. Return it to the battlefield under your control at the beginning of the next end step. Activate only if you control three or more artifacts.":'blink',
    'When this creature enters, if you control three or more artifacts, this creature gets +3/+3 and gains haste until end of turn.':'berserk',
    'This creature has protection from each color as long as you control three or more artifacts.':'protection',
    "Artifacts you control have shroud as long as you control three or more artifacts. (An artifact with shroud can't be the target of spells or abilities.)":'shroud',
    "When this creature enters, if you control three or more artifacts, return target creature to its owner's hand.":'bounce',
    'This artifact is a 5/5 Golem artifact creature as long as you control three or more artifacts.':'animate',
    '{T}: Tap target artifact, creature, or land. Activate only if you control three or more artifacts.':'tap',
}
MODES = {
    'blink':('gain','drop','fee_short','offturn','fresh','source_lost','pending_drop','control_return','replay'),
    'berserk':('pending_gain','pending_drop','late_gain','drop','duration','replay'),
    'bounce':('pending_gain','pending_drop','late_gain','own_target','self_target','stolen_target','replay'),
    'protection':('gain','drop','unprotected','block_color','prevent_damage','colorless','replay'),
    'shroud':('gain','drop','scope','source_removed','noncreature','replay'),
    'animate':('gain','drop','old_attack','fresh_attack','replay'),
    'tap':('gain','drop','target_artifact','target_land','target_creature','sick','pending_drop',
           'invalid_enchantment','target_lost','replay'),
}
PROBES = ('Murder','Unsummon','Giant Growth','Shock','Swords to Plowshares')
SUPPORT = ('Darksteel Relic','Bonesplitter','Sparring Collar')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    if not record or record.get('layout')!='normal' or 'card_faces' in record or 'selected_face' in record:
        raise ValueError('normal whole Metalcraft card required')
    lines = record['oracle_text'].splitlines()
    flying = lines[:1]==['Flying']
    rules = lines[int(flying):]
    prefix = 'Metalcraft — '
    effect = CLAUSES.get(rules[0].removeprefix(prefix)) if len(rules)==1 and rules[0].startswith(prefix) else None
    keys = {'Metalcraft','Flying'} if flying else {'Metalcraft'}
    kind = record['type_line'].split(' — ',1)[0]
    if not effect or set(record['keywords'])!=keys:
        raise ValueError('complete supported printed Metalcraft clause and passives required')
    expected_type = {'animate':'Artifact','protection':'Artifact Creature'}.get(effect,'Creature')
    if kind!=expected_type or flying!=(effect in ('blink','shroud','bounce')):
        raise ValueError('printed type or Flying differs from the supported full clause')
    return dict(effect=effect,flying=flying,artifact='Artifact' in kind,cast_mana=cost_from_oracle(record),
                power=int(record.get('power') or 0),toughness=int(record.get('toughness') or 0))


def variants(model):
    modes = ('cast_below','cast_threshold','unfunded','enemy_scope')+MODES[model['effect']]
    if any(model['cast_mana'][1:]):
        modes += ('wrong_color',)
    if model['flying']:
        modes += ('flying_ground','flying_reach')
    return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]


def spec(declaration,record):
    model = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2 or
            declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),model) or
            type(selected) is not list or not selected or any(type(v) is not str for v in selected) or
            len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('typed complete Metalcraft declaration and explicit variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle, registry):
    for group in ('sources','witnesses'):
        for name,pin in pins()[group].items():
            record,row = oracle.get(name,{}),registry.get(name,{})
            if (not exact(pin,{'oracle':record,'registry':row}) or record.get('layout')!='normal' or
                    row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'implemented normal Metalcraft identity changed: {name}')


class Scene:
    def __init__(self,name,model,mode,owner,registry,oracle):
        self.name,self.model,self.mode,self.owner = name,model,mode,owner
        self.foe,self.registry,self.oracle = 1-owner,registry,oracle
        self.effect = model['effect']
        self.active = self.foe if mode in ('offturn','prevent_damage') else owner
        self.entities,self.setup,self.steps = {},[],[]
        self.life,self.mana = [20,20],[[0]*6 for _ in (0,1)]
        self.boosted = False
        self.source_entered_this_turn = False
        self.replay = None

    def emit(self,op,**fields):
        from recipes import step

        self.steps.append(step(op,**fields))

    def add(self,name,ref,owner,zone='battlefield'):
        from recipes import card

        self.setup.append(card(name,ref,owner,zone))
        o = self.oracle[name]
        kinds = o['type_line'].split(' — ',1)[0].split()
        self.entities[ref] = dict(name=name,owner=owner,controller=owner,zone=zone,tapped=False,
                                  power=int(o.get('power') or 0),toughness=int(o.get('toughness') or 0),damage=0,
                                  kinds=kinds,keywords=o['keywords'])

    def count(self,player):
        return sum(e['controller']==player and e['zone']=='battlefield' and 'Artifact' in e['kinds'] for e in self.entities.values())

    def active_metalcraft(self):
        return self.count(self.entities['source']['controller'])>=3

    def creature(self,ref):
        entity = self.entities[ref]
        return 'Creature' in entity['kinds'] or (ref=='source' and self.effect=='animate' and self.active_metalcraft())

    def stats(self,ref):
        entity = self.entities[ref]
        if ref=='source' and self.effect=='animate' and self.active_metalcraft():
            return 5,5
        bonus = 3 if ref=='source' and self.boosted else 0
        return entity['power']+bonus,entity['toughness']+bonus

    def observe(self,ref,prop,value):
        self.emit('assert_card',ref=ref,property=prop,value=int(value))

    def pools(self):
        for player,amounts in enumerate(self.mana):
            for color,amount in enumerate(amounts):
                self.emit('assert_mana',player=player,color=color,value=amount)

    def receipts(self,stack=0):
        self.emit('assert_stack',value=stack)
        self.emit('assert_choices_consumed')
        self.pools()
        for ref,e in self.entities.items():
            if e['zone']=='stack':
                continue
            # Certarch's grave witness indexes a zone list and shares the
            # victim's active slot number. Its zone receipts are emitted below.
            if self.effect=='tap' and ref=='crypt_grave':
                continue
            alive = e['zone']=='battlefield'
            self.observe(ref,'in_play',alive)
            if alive:
                self.observe(ref,'tapped',e['tapped'])
                if self.creature(ref):
                    power,toughness = self.stats(ref)
                    for prop,value in [('power',power),('toughness',toughness),('damage',e['damage'])]:
                        self.observe(ref,prop,value)
        for player in (0,1):
            self.emit('assert_life',player=player,value=self.life[player])
            for zone in ('hand','battlefield','graveyard','exile','library'):
                if zone=='hand' and any(e['zone']=='stack' for e in self.entities.values()):
                    continue
                names = Counter(e['name'] for e in self.entities.values() if e['zone']==zone and
                                e['controller' if zone=='battlefield' else 'owner']==player)
                self.emit('assert_zone',player=player,zone=zone,value=sum(names.values()))
                for name,n in names.items():
                    self.emit('assert_zone_multiplicity',player=player,zone=zone,index=self.registry[name]['csvid'],value=n)

    def fund(self,player,amounts):
        assert not any(self.mana[player])
        self.mana[player] = list(amounts)
        for color,n in enumerate(amounts):
            if n:
                self.emit('set_mana',player=player,color=color,value=n)
        self.emit('refresh')

    def price(self,ref):
        from recipes import cost_from_oracle

        record = self.oracle[self.entities[ref]['name']]
        return [0]*6 if record['mana_cost']=='{0}' else cost_from_oracle(record)

    def cast(self,ref,target=None,*,parent=None,denied=None):
        entity = self.entities[ref]
        player,fee = entity['owner'],self.price(ref)
        funding = [sum(fee),0,0,0,0,0] if denied=='wrong_color' else [0]*6 if denied else fee
        self.fund(player,funding)
        if parent:
            self.emit('announce_spell',ref=parent)
        if target:
            self.emit('choose_target',player=player,ref=target)
        self.emit('cast_response' if parent else 'cast',ref=ref,value=int(denied is None))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0 if denied else 2 if parent else 1)
        if denied is None:
            entity['zone'] = 'stack'
            self.observe(ref,'in_hand',0)
            self.mana[player] = [0]*6
        self.pools()

    def resolve(self,response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0)

    def play_artifact(self,ref):
        self.cast(ref)
        self.resolve()
        self.entities[ref]['zone'] = 'battlefield'
        self.receipts()

    def leave(self,ref,zone):
        self.entities[ref].update(zone=zone,tapped=False,damage=0)
        if ref=='source':
            self.boosted = False

    def remove(self,ref,spell='removal',zone='graveyard'):
        self.cast(spell,ref)
        self.resolve()
        self.leave(spell,'graveyard')
        self.leave(ref,zone)
        self.receipts()

    def sacrifice(self,pending=False):
        player = self.entities['dropper']['controller']
        self.emit('require_can_activate',ref='dropper',value=1)
        self.emit('choose_player',player=player,value=1-player)
        self.emit('activate',ref='dropper',value=1)
        self.leave('dropper','graveyard')
        self.emit('refresh')
        self.receipts(2 if pending else 1)
        if not pending:
            self.resolve()
            self.finish_sacrifice()
            self.receipts()

    def finish_sacrifice(self):
        enemy = 1-self.entities['dropper']['controller']
        for ref,entity in self.entities.items():
            if entity['owner']==enemy and entity['zone']=='graveyard':
                self.leave(ref,'exile')

    def donate(self,ref):
        self.emit('require_can_activate',ref='trader',value=1)
        self.emit('choose_player',player=self.owner,value=self.foe)
        self.emit('choose_target',player=self.owner,ref=ref)
        self.emit('activate',ref='trader',value=1)
        self.emit('assert_stack',value=1)
        self.resolve()
        self.entities['trader']['tapped'] = True
        entity = self.entities[ref]
        entity['controller'] = self.foe
        self.emit('bind_permanent',ref=ref,player=self.foe,value=self.registry[entity['name']]['csvid'])
        self.receipts()

    def cleanup(self):
        self.emit('cleanup')
        self.emit('refresh')
        self.boosted = False
        for e in self.entities.values():
            e['damage'] = 0

    def combat(self,attacker='source',blocker=None,*,can_attack=True,legal_block=False):
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=int(can_attack))
        self.observe(attacker,'attacking',can_attack)
        self.emit('finish_attackers')
        if not can_attack:
            self.receipts()
            return
        a = self.entities[attacker]
        a['tapped'] = True
        self.emit('begin_blockers')
        if blocker:
            self.emit('block',ref=blocker,other=attacker,value=int(legal_block))
            self.observe(blocker,'blocking_count',legal_block)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if legal_block:
            damage = [(attacker,blocker),(blocker,attacker)]
            deaths = []
            for victim,dealer in damage:
                e,d = self.entities[victim],self.entities[dealer]
                if (victim=='source' and self.effect=='protection' and self.active_metalcraft() and
                        self.oracle[d['name']]['colors']):
                    continue
                e['damage'] += self.stats(dealer)[0]
                if e['damage']>=self.stats(victim)[1] or ('Deathtouch' in d['keywords'] and self.stats(dealer)[0]):
                    deaths.append(victim)
            for victim in deaths:
                self.leave(victim,'graveyard')
        else:
            self.life[1-a['controller']] -= self.stats(attacker)[0]
        self.receipts()

    def artifact_support(self):
        mode = self.mode
        low = mode in ('cast_below','enemy_scope','gain','late_gain','pending_gain','unprotected','old_attack')
        player = self.foe if mode=='control_return' else self.owner
        needed = (2 if low else 3)-int(self.model['artifact'])
        names = list(SUPPORT[:needed])
        if self.effect=='shroud' and mode!='noncreature':
            names[0] = 'Darksteel Myr'
        drop = mode in ('drop','pending_drop') and not (mode=='pending_drop' and self.effect in ('bounce','berserk'))
        if drop:
            names[-1] = "Tormod's Crypt"
        for index,name in enumerate(names):
            label = 'dropper' if drop and index==needed-1 else f'artifact_{index}'
            self.add(name,label,player)
        if drop:
            self.add('Plains','crypt_grave',1-player,'graveyard')
        if mode in ('gain','late_gain','old_attack'):
            self.add('Darksteel Relic','increase',player,'hand')
        if mode=='enemy_scope':
            self.add('Darksteel Relic','enemy_artifact',self.foe)
            self.add('Goblin Tinkerer','own_nonartifact',self.owner)

    def setup_cards(self):
        mode,effect = self.mode,self.effect
        cast_modes = {'cast_below','cast_threshold','unfunded','wrong_color','pending_gain','pending_drop','late_gain','duration',
                      'own_target','self_target','stolen_target','fresh_attack','fresh','sick'}
        initially_cast = (mode in cast_modes and not (mode=='pending_drop' and effect in ('blink','tap'))) or (
            effect in ('berserk','bounce') and mode in ('enemy_scope','replay')) or (effect=='berserk' and mode=='drop')
        self.add(self.name,'source',self.owner,'hand' if initially_cast else 'battlefield')
        self.artifact_support()
        if mode in ('flying_ground','flying_reach','block_color','prevent_damage'):
            self.add('Deadly Recluse' if mode=='flying_reach' else 'Goblin Tinkerer','blocker',self.foe)
        if mode in ('control_return','stolen_target'):
            self.add('Bazaar Trader','trader',self.owner)
        if effect=='bounce' and mode not in ('flying_ground','flying_reach','self_target','unfunded','wrong_color'):
            self.add('Goblin Tinkerer','victim',self.owner if mode in ('own_target','stolen_target') else self.foe)
        if mode=='pending_gain':
            self.add('Shimmer Myr','response',self.owner,'hand')
        if mode=='pending_drop' and effect in ('berserk','bounce'):
            self.add('Shatter','response',self.foe,'hand')
        if effect=='protection':
            for index,name in enumerate(PROBES):
                self.add(name,f'color_probe_{index}',self.foe,'hand')
            self.add('Unsummon','own_probe',self.owner,'hand')
            if mode=='colorless':
                self.add('Rod of Ruin','rod',self.foe)
        if effect in ('shroud','animate'):
            self.add('Murder','own_probe',self.owner,'hand')
            self.add('Murder','foe_probe',self.foe,'hand')
        if effect=='shroud':
            for player in (self.owner,self.foe):
                self.add('Shatter',f'art_probe_{player}',player,'hand')
            if mode=='scope':
                self.add('Bonesplitter','enemy_artifact',self.foe)
                self.add('Darksteel Myr','enemy_creature',self.foe)
        if mode=='source_removed':
            self.add('Murder','removal',self.foe,'hand')
        if effect=='tap':
            names = {'target_artifact':'Bonesplitter','target_land':'Forest'}
            self.add(names.get(mode,'Goblin Tinkerer'),'victim',self.owner if mode=='target_creature' else self.foe)
            if mode=='invalid_enchantment':
                self.add("Furnace of Rath",'invalid',self.foe)

    def enter(self):
        mode,effect = self.mode,self.effect
        if mode=='stolen_target':
            self.donate('victim')
        self.cast('source',denied=mode if mode in ('unfunded','wrong_color') else None)
        if mode in ('unfunded','wrong_color'):
            self.receipts()
            return False
        response = mode in ('pending_gain','pending_drop')
        if mode=='pending_gain':
            self.cast('response',parent='source')
            self.entities['response']['zone'] = 'battlefield'
        elif mode=='pending_drop':
            self.cast('response',f'artifact_{2}',parent='source')
            self.leave('response','graveyard')
            self.leave('artifact_2','graveyard')
        self.entities['source']['zone'] = 'battlefield'
        self.source_entered_this_turn = True
        if mode=='replay' and effect in ('berserk','bounce'):
            self.replay = len(self.steps)
        if effect=='bounce' and self.active_metalcraft():
            victim = 'source' if mode=='self_target' else 'victim'
            self.emit('choose_target',player=self.owner,ref=victim)
            self.leave(victim,'hand')
        self.boosted = effect=='berserk' and self.active_metalcraft()
        self.resolve(response)
        self.receipts()
        return True

    def blink(self):
        mode = self.mode
        if mode=='control_return':
            self.donate('source')
        actor = self.entities['source']['controller']
        if mode=='gain':
            self.play_artifact('increase')
        if mode=='drop':
            self.sacrifice()
        allowed = self.active_metalcraft() and mode!='fee_short'
        self.fund(actor,[0,0,int(mode!='fee_short'),0,0,0])
        self.emit('can_activate',ref='source',value=int(allowed))
        self.emit('activate',ref='source',value=int(allowed))
        if not allowed:
            self.receipts()
            return
        self.mana[actor] = [0]*6
        self.receipts(1)
        if mode=='replay':
            self.replay = len(self.steps)
        if mode=='pending_drop':
            self.sacrifice(pending=True)
        if mode=='source_lost':
            self.emit('destroy',ref='source')
            self.leave('source','graveyard')
        self.resolve()
        if mode=='pending_drop':
            self.finish_sacrifice()
        if mode!='source_lost':
            self.leave('source','exile')
        self.receipts()
        self.cleanup()
        if mode!='source_lost':
            self.entities['source']['zone'] = 'battlefield'
            self.emit('bind_permanent',ref='source',player=actor,value=self.registry[self.name]['csvid'])
        self.receipts()

    def etb(self):
        if self.mode=='late_gain':
            self.play_artifact('increase')
        if self.mode=='drop':
            self.sacrifice()
        if self.effect=='berserk':
            self.combat(can_attack=self.boosted)
            if self.boosted:
                self.cleanup()
                self.receipts()
        else:
            self.receipts()

    def protection(self):
        active = self.active_metalcraft()
        for ref in [f'color_probe_{i}' for i in range(len(PROBES))]+['own_probe']:
            self.emit('can_target_creature',ref=ref,other='source',value=int(not active))
        if self.mode=='unprotected':
            self.remove('source','color_probe_1','hand')
        if self.mode=='block_color':
            self.combat(blocker='blocker')
        if self.mode=='prevent_damage':
            self.combat('blocker','source',legal_block=True)
        if self.mode=='colorless':
            self.fund(self.foe,[3,0,0,0,0,0])
            self.emit('require_can_activate',ref='rod',value=1)
            self.emit('choose_target',player=self.foe,ref='source')
            self.emit('activate',ref='rod',value=1)
            self.mana[self.foe] = [0]*6
            self.entities['rod']['tapped'] = True
            self.receipts(1)
            self.resolve()
            self.entities['source']['damage'] = 1

    def shroud(self):
        active = self.active_metalcraft() and self.entities['source']['zone']=='battlefield'
        for ref in ('own_probe','foe_probe'):
            if self.entities['source']['zone']=='battlefield':
                self.emit('can_target_creature',ref=ref,other='source',value=1)
            if self.mode!='noncreature':
                self.emit('can_target_creature',ref=ref,other='artifact_0',value=int(not active))
            if self.mode=='scope':
                self.emit('can_target_creature',ref=ref,other='enemy_creature',value=1)
        for player in (self.owner,self.foe):
            self.emit('can_cast',ref=f'art_probe_{player}',value=int(not active or self.mode=='scope'))
        if self.mode=='scope':
            self.remove('enemy_artifact',f'art_probe_{self.owner}')

    def animate(self):
        active = self.active_metalcraft()
        for prop,value in [('artifact',1),('subtype_Golem',int(active))]:
            self.emit('assert_matching_permanents',player=self.owner,index=self.registry[self.name]['csvid'],property=prop,value=value)
        for ref in ('own_probe','foe_probe'):
            self.emit('can_target_creature',ref=ref,other='source',value=int(active))
        if self.mode in ('old_attack','fresh_attack'):
            self.combat(can_attack=self.mode=='old_attack')

    def tap(self):
        mode = self.mode
        allowed = self.active_metalcraft() and not self.source_entered_this_turn
        self.emit('can_activate',ref='source',value=int(allowed))
        if not allowed:
            self.receipts()
            return
        if mode=='invalid_enchantment':
            self.emit('probe_activation_target',ref='source',other='invalid',index=-1,value=0)
        self.emit('choose_target',player=self.owner,ref='victim')
        self.emit('activate',ref='source',value=1)
        self.entities['source']['tapped'] = True
        self.receipts(1)
        if mode=='replay':
            self.replay = len(self.steps)
        if mode=='pending_drop':
            self.sacrifice(pending=True)
        if mode=='target_lost':
            self.emit('destroy',ref='victim')
            self.leave('victim','graveyard')
        self.resolve()
        if mode=='pending_drop':
            self.finish_sacrifice()
        if mode!='target_lost':
            self.entities['victim']['tapped'] = True
        self.emit('can_activate',ref='source',value=0)

    def play(self):
        self.setup_cards()
        self.emit('turn',player=self.active,value=0)
        self.emit('present_ai_dialogs',value=1)
        for p in (0,1):
            for color in range(6):
                self.emit('set_mana',player=p,color=color,value=0)
        self.emit('refresh')
        if self.entities['source']['zone']=='hand' and not self.enter():
            return
        if self.mode in ('flying_ground','flying_reach'):
            self.combat(blocker='blocker',legal_block=self.mode=='flying_reach')
            return
        if self.effect in ('berserk','bounce'):
            self.etb()
            return
        if self.effect=='blink':
            self.blink()
            return
        if self.mode in ('gain','old_attack'):
            self.play_artifact('increase')
        if self.mode=='drop':
            self.sacrifice()
        if self.mode=='source_removed':
            self.remove('source')
        if self.mode=='replay' and self.effect!='tap':
            self.replay = len(self.steps)
        {'protection':self.protection,'shroud':self.shroud,'animate':self.animate,'tap':self.tap}[self.effect]()
        self.receipts()


def expand(name,csvid,declaration,model,selected,registry,oracle):
    from recipes import step

    validate_witnesses(oracle,registry)
    if pins()['sources'].get(name,{}).get('registry',{}).get('csvid')!=csvid:
        raise ValueError('unreserved metalcraft source')
    result = []
    for variant in selected:
        mode,owner = variant.rsplit('_',1)
        scene = Scene(name,model,mode,int(owner),registry,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            index = scene.replay
            tail = steps[index:]
            steps = steps[:index]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+tail+[step('assert_recorded_state')]
        gaps = ['Multiplayer']
        if model['effect'] in ('berserk','bounce'):
            gaps.append('Separate priority between actual entry and resolution of the mandatory trigger')
        if model['effect']=='blink':
            gaps.append('Activation after the beginning of the end step')
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                        oracle_text=declaration['oracle_text'],untested_components=gaps)
        result.append(dict(name=f'MetalcraftArtifactRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                           setup=scene.setup,steps=steps,_recipe=metadata))
    return result
