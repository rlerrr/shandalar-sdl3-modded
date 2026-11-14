"""Whole-Oracle own noncreature-cast temporary stats and calibrated compositions."""
import copy,json,re
from pathlib import Path
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from cast_untap_trigger_recipes import pin,exact

FAMILY='cast_temporary_stat_trigger'
DATA=Path(__file__).parent/'data/cast_temporary_stat_trigger_wave8_profiles.json'
PROWESS='Prowess (Whenever you cast a noncreature spell, this creature gets +1/+1 until end of turn.)'
TEAM='Whenever you cast a noncreature spell, creatures you control get +1/+1 until end of turn.'
HEXPROOF="Hexproof (This creature can't be the target of spells or abilities your opponents control.)"
LIFELINK='Whenever you cast a noncreature spell, this creature gains lifelink until end of turn.'
DEATH='When this creature dies, draw a card.'
SHADE='{U}: This creature gets +1/-1 until end of turn.'

def pins():return json.loads(DATA.read_text(encoding='utf8'))
def classify(r):
    from recipes import cost_from_oracle
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or 'Creature' not in r.get('type_line','') or r['type_line'].startswith('Token '):raise ValueError('whole normal primary creature required')
    if any(not re.fullmatch(r'\d+',r.get(k,'')) for k in ('power','toughness')) or int(r['toughness'])<=0:raise ValueError('positive numeric creature base required')
    lines=r.get('oracle_text','').splitlines()
    if len(set(lines))!=len(lines) or sum(s in (PROWESS,TEAM) for s in lines)!=1:raise ValueError('one exact own noncreature stat clause required')
    scope='self' if PROWESS in lines else 'own_creatures';rest=[s for s in lines if s not in (PROWESS,TEAM)];keys=[];secondary=[]
    for s in rest:
        if s in ('Double strike','Flying'):keys.append(s)
        elif s==HEXPROOF:keys.append('Hexproof')
        elif s in (LIFELINK,DEATH,SHADE):secondary.append(s)
        else:raise ValueError('unmodeled complete printed clause')
    if len(secondary)>1 or secondary and scope!='self':raise ValueError('unpiloted secondary composition')
    expected=set(keys)|({'Prowess'} if scope=='self' else set())
    if set(r.get('keywords',[]))!=expected:raise ValueError('complete keyword identity disagrees')
    return dict(scope=scope,power=int(r['power']),toughness=int(r['toughness']),mana=cost_from_oracle(r),keywords=keys,
                lifelink=LIFELINK in lines,death_draw=1 if DEATH in lines else 0,activation=dict(mana=[0,0,1,0,0,0],power=1,toughness=-1) if SHADE in lines else None)
def parameters(p):return {k:copy.deepcopy(p[k]) for k in ('scope','keywords','lifelink','death_draw','activation')}
def variants(p):
    modes=['source_cast','source_payment','source_insufficient_mana','source_wrong_color','fresh_attack',
           'effect','payment','witness_insufficient_mana','witness_wrong_color','two_spells','three_spells','opponent_cast',
           'creature_spell','artifact_creature_spell','instant_spell','sorcery_spell','enchantment_spell','planeswalker_spell',
           'two_sources','source_destroyed_before','source_arrives_late','late_ally','activation_not_cast',
           'cleanup','unblocked','blocked','ready_snapshot','pending_snapshot','resolved_snapshot','countered_witness','bare_zones']
    if 'Double strike' in p['keywords']:modes+=['base_double_strike','boosted_double_strike']
    if 'Hexproof' in p['keywords']:modes+=['targetability','own_bounce']
    if p['lifelink']:modes+=['lifelink_cleanup','base_no_lifelink','two_spell_lifelink']
    if p['death_draw']:modes+=['self_death','boosted_death','other_death','opponent_death','death_snapshot','lethal_combat_death','nonlethal_cleanup','bounce_no_death','countered_source','two_source_deaths']
    if p['activation']:modes+=['ability_value','ability_payment','ability_repeat','ability_lethal','ability_cleanup','ability_insufficient_mana','ability_wrong_color','ability_tapped','ability_sick','ability_source_removed_pending','ability_pending_snapshot','combined_value','combined_cleanup','combined_lethal','combined_pending_snapshot']
    return [f'{m}_{w}' for w in (0,1) for m in modes]
def spec(d,r):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r.get('oracle_text'):raise ValueError('full Oracle and exact family/version required')
    if not exact(pins()['sources'].get(r['name']),pin(r)):raise ValueError('source Oracle pin changed')
    p=classify(r)
    if not exact(d.get('params'),parameters(p)):raise ValueError('parameters must derive from full printed clauses')
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or len(vs)!=len(set(vs)) or set(vs)-set(variants(p)):raise ValueError('unique calibrated variants required')
    return dict(p,family=FAMILY)
def validate_witnesses(o,rows):
    from creature_recipes import oracle_keywords
    data=pins()
    for n,expected in dict(data['witnesses'],**data['sources']).items():
        if n not in rows or rows[n]['handler'] in ('0','NULL') or not exact(pin(o.get(n,{})),expected):raise ValueError('source/witness pin changed: '+n)
        if n in data['sources']:
            if rows[n]['kind']!='card' or rows[n]['handler']=='card_dummy':raise ValueError('implemented normal primary required')
            classify(o[n])
        elif rows[n]['handler']=='card_dummy':oracle_keywords(o[n])
    for n,h in data['handlers'].items():
        if rows[n]['handler']!=h:raise ValueError('input presentation handler changed: '+n)

class StatLedger:
    def __init__(self,name,csvid,d,p,v,rows,o):
        from recipes import card,step
        self.name=name;self.csvid=csvid;self.d=d;self.p=p;self.mode,who=v.rsplit('_',1);self.owner=int(who);self.variant=v;self.rows=rows;self.o=o
        self.e={};self.setup=[];self.actions=[];self.z={z:[[],[]] for z in ('hand','graveyard','exile','library')};self.mana=[[30]*6,[30]*6];self.life=[20,20];self.snapshot=0
        for player in (0,1):
            for zone,base in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY*7)):
                ns=[] if self.mode=='bare_zones' and zone!='library' else list(base);self.z[zone][player]=ns
                self.setup += [card(n,f'zone_{player}_{zone}_{i}',player,zone) for i,n in enumerate(ns)]
        self.source_hand=self.mode in ('source_cast','source_payment','source_insufficient_mana','source_wrong_color','fresh_attack','source_arrives_late','countered_source')
        self.denied_source=self.mode in ('source_insufficient_mana','source_wrong_color');self.actor=1-self.owner if self.mode=='opponent_cast' else self.owner
        self.add(name,'source',self.owner,'hand' if self.source_hand else 'battlefield',tapped=self.mode=='ability_tapped',sick=self.mode=='ability_sick')
        self.add('Grizzly Bears','idle',self.owner);self.add('Grizzly Bears','enemy',1-self.owner);self.add('Darksteel Relic','noncreature',self.owner)
        if self.mode in ('two_sources','two_source_deaths'):self.add(name,'second',self.owner)
        if self.mode=='late_ally':self.add('Grizzly Bears','late',self.owner,'hand')
        if self.mode=='activation_not_cast':self.add('Sol Ring','mana_control',self.owner)
        if self.mode in ('blocked','boosted_double_strike','lethal_combat_death','nonlethal_cleanup'):
            self.add('Suntail Hawk' if self.mode=='nonlethal_cleanup' else 'Grizzly Bears','blocker',1-self.owner)
        if self.mode in ('targetability','own_bounce','bounce_no_death'):
            self.add('Unsummon','own_probe',self.owner,'hand')
        if self.mode=='targetability':
            self.add('Unsummon','enemy_probe',1-self.owner,'hand');self.add('Visara the Dreadful','own_ability',self.owner);self.add('Visara the Dreadful','enemy_ability',1-self.owner)
        self.activation_mode=self.mode.startswith('ability_') or self.mode.startswith('combined_')
        no_witness=self.source_hand and self.mode!='source_arrives_late' or self.activation_mode and not self.mode.startswith('combined_') or self.mode in ('activation_not_cast','targetability','own_bounce','base_double_strike','base_no_lifelink','self_death','other_death','opponent_death','bounce_no_death','two_source_deaths')
        self.count=0 if no_witness else 3 if self.mode=='three_spells' else 2 if self.mode in ('two_spells','two_spell_lifelink') else 1
        witness={'creature_spell':'Grizzly Bears','artifact_creature_spell':'Ornithopter','instant_spell':'Reach Through Mists','sorcery_spell':'Divination','enchantment_spell':'Seal of Cleansing','planeswalker_spell':'Jace Beleren','witness_insufficient_mana':'Reach Through Mists','witness_wrong_color':'Reach Through Mists','payment':'Reach Through Mists'}.get(self.mode,'Darksteel Relic')
        self.witness_name=witness
        for i in range(self.count):self.add(witness,f'witness_{i}',self.actor,'hand')
        if self.mode in ('countered_witness','countered_source'):self.add('Counterspell','response',1-self.owner,'hand')
        if self.mode=='source_payment':self.mana[self.owner]=self.cost(name)
        elif self.mode=='source_insufficient_mana':self.mana[self.owner]=[0]*6
        elif self.mode=='source_wrong_color':self.mana[self.owner]=[sum(self.cost(name)),0,0,0,0,0]
        elif self.mode in ('payment','witness_insufficient_mana','witness_wrong_color'):
            self.mana[self.owner]=self.cost(witness) if self.mode=='payment' else [sum(self.cost(witness)),0,0,0,0,0] if self.mode=='witness_wrong_color' else [0]*6
        elif self.mode in ('ability_payment','ability_insufficient_mana','ability_wrong_color'):
            self.mana[self.owner]=[0,0,1,0,0,0] if self.mode=='ability_payment' else [1,0,0,0,0,0] if self.mode=='ability_wrong_color' else [0]*6
        self.pre=[step('reset_death_history'),step('turn',player=self.owner if self.source_hand else self.actor,value=0)]+[step('set_mana',player=w,color=c,value=n) for w in (0,1) for c,n in enumerate(self.mana[w])]
        self.universe={n for z in self.z.values() for ns in z for n in ns}|{e['name'] for e in self.e.values()}
    def step(self,op,**kw):
        from recipes import step
        self.actions.append(step(op,**kw))
    def add(self,n,ref,w,zone='battlefield',tapped=False,sick=False):
        from recipes import card
        c=card(n,ref,w,zone)
        if zone=='battlefield':c['state']=(['tapped'] if tapped else ['untapped'])+(['sick'] if sick else [])
        self.setup.append(c);r=self.o[n];creature='Creature' in r['type_line'];self.e[ref]=dict(name=n,player=w,alive=zone=='battlefield',pending=False,creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,dp=0,dt=0,damage=0,tapped=tapped,lifelink=False)
        if zone=='hand':self.z['hand'][w].append(n)
    def cost(self,n):
        from recipes import cost_from_oracle
        return [0]*6 if self.o[n]['mana_cost']=='{0}' else cost_from_oracle(self.o[n])
    def spend(self,w,cost):
        self.mana[w]=[a-b for a,b in zip(self.mana[w],cost)]
        if min(self.mana[w])<0:raise ValueError('fixture underfunded real payment')
    def draw(self,w,n):
        if len(self.z['library'][w])<n:raise ValueError('ordered draw fixture underfunded')
        self.z['hand'][w]+=self.z['library'][w][:n];del self.z['library'][w][:n]
    def notify(self,ref):
        spell=self.e[ref];types=set(self.o[spell['name']]['type_line'].partition(' — ')[0].split())
        if 'Creature' in types:return
        sources=[x for x in self.e.values() if x['alive'] and x['name']==self.name and x['player']==spell['player']]
        for source in sources:
            targets=[source] if self.p['scope']=='self' else [x for x in self.e.values() if x['alive'] and x['creature'] and x['player']==source['player']]
            for target in targets:target['dp']+=1;target['dt']+=1
            if self.p['lifelink']:source['lifelink']=True
    def begin(self,ref,denied=False,response=False):
        e=self.e[ref];self.step('cast_response' if response else 'cast',ref=ref,value=int(not denied))
        if denied:self.step('assert_stack',value=0);self.step('assert_choices_consumed');return
        self.spend(e['player'],self.cost(e['name']));self.z['hand'][e['player']].remove(e['name']);e['pending']=True
        self.step('assert_unused_choices',value=0);self.step('require_card',ref=ref,property='in_hand',value=0);self.step('assert_stack',value=2 if response else 1)
        self.step('announce_spell',ref=ref);self.notify(ref);self.actions+=self.checks(stack=2 if response else 1)
    def finish(self,ref,countered=False):
        e=self.e[ref];e['pending']=False;types=set(self.o[e['name']]['type_line'].partition(' — ')[0].split())
        if countered or not types&{'Creature','Artifact','Enchantment','Planeswalker','Land'}:self.z['graveyard'][e['player']].append(e['name'])
        else:e['alive']=True
        if not countered and e['name'] in ('Reach Through Mists','Divination'):self.draw(e['player'],1 if e['name']=='Reach Through Mists' else 2)
    def cast(self,ref,denied=False):
        self.begin(ref,denied)
        if denied:return
        self.snapshot=len(self.actions)
        if self.mode=='countered_witness' and ref=='witness_0' or self.mode=='countered_source' and ref=='source':
            self.begin('response',response=True);self.step('resolve_response');self.finish('response');self.finish(ref,True)
        else:self.step('resolve');self.finish(ref)
        self.actions+=self.checks()
    def kill(self,ref,explicit=False):
        if explicit:self.step('destroy',ref=ref)
        e=self.e[ref]
        if not e['alive']:raise ValueError('cannot kill absent model entity')
        e['alive']=False;self.z['graveyard'][e['player']].append(e['name'])
        if e['name']==self.name and self.p['death_draw']:self.draw(e['player'],self.p['death_draw'])
    def checks(self,stack=0):
        from recipes import step,observe
        out=[step('assert_stack',value=stack),step('assert_choices_consumed')]
        for ref,e in self.e.items():
            if not e['alive']:continue
            out += [observe(ref,'in_play',1),observe(ref,'in_hand',0),observe(ref,'tapped',int(e['tapped']))]
            if e['creature']:out += [observe(ref,'power',e['power']+e['dp']),observe(ref,'toughness',e['toughness']+e['dt']),observe(ref,'damage',e['damage']),observe(ref,'plus1_counters',0)]
            if e['name']=='Jace Beleren':out.append(step('assert_counter',ref=ref,counter_kind='loyalty',value=int(self.o[e['name']]['loyalty'])))
        for w in (0,1):
            out += [step('assert_life',player=w,value=self.life[w]),step('assert_mana_total',player=w,value=sum(self.mana[w])),step('assert_zone',player=w,zone='battlefield',value=sum(e['alive'] and e['player']==w for e in self.e.values()))]
            out += [step('assert_mana',player=w,color=c,value=n) for c,n in enumerate(self.mana[w])]
            out += [step('assert_zone_multiplicity',player=w,zone='battlefield',index=self.rows[n]['csvid'],value=sum(e['alive'] and e['player']==w and e['name']==n for e in self.e.values())) for n in sorted(self.universe)]
            for zone in self.z:
                ns=list(self.z[zone][w])
                if zone=='hand':ns += [e['name'] for e in self.e.values() if e['pending'] and e['player']==w]
                out.append(step('assert_zone',player=w,zone=zone,value=len(ns)))
                out += [step('assert_zone_multiplicity',player=w,zone=zone,index=self.rows[n]['csvid'],value=ns.count(n)) for n in sorted(self.universe)]
                if zone=='library':out += [step('assert_zone_card',player=w,zone=zone,index=i,value=self.rows[n]['csvid']) for i,n in enumerate(ns)]
        return out
    def set_pools(self,pools):
        self.mana=copy.deepcopy(pools)
        for w in (0,1):
            for c,n in enumerate(self.mana[w]):self.step('set_mana',player=w,color=c,value=n)
    def clean(self):
        self.actions+=self.checks();self.set_pools([[0]*6,[0]*6]);self.step('cleanup');self.step('refresh')
        for e in self.e.values():
            if e['alive']:e['dp']=e['dt']=e['damage']=0;e['lifelink']=False
        self.actions+=self.checks()
    def combat(self,fresh=False):
        self.step('turn',player=self.owner,value=0);self.step('begin_attackers');self.step('attack',ref='source',value=int(not fresh));self.e['source']['tapped']=not fresh
        self.step('finish_attackers');self.step('begin_blockers');blocked='blocker' in self.e and not fresh
        if blocked:self.step('block',ref='blocker',other='source',value=1)
        self.step('advance_combat');self.step('combat_damage')
        source=self.e['source'];power=max(0,source['power']+source['dp']);double='Double strike' in self.p['keywords']
        if not fresh:
            if blocked:
                b=self.e['blocker'];b['damage']+=power
                first_killed=double and b['damage']>=b['toughness']+b['dt']
                if not first_killed:
                    source['damage']+=max(0,b['power']+b['dp'])
                    if double:b['damage']+=power
                if source['lifelink']:self.life[self.owner]+=power*(2 if double and not first_killed else 1)
                dead=[ref for ref in ('source','blocker') if self.e[ref]['damage']>=self.e[ref]['toughness']+self.e[ref]['dt']]
                for ref in dead:self.kill(ref)
            else:
                amount=power*(2 if double else 1);self.life[1-self.owner]-=amount
                if source['lifelink']:self.life[self.owner]+=amount
        self.step('end_combat');self.actions+=self.checks()
    def ability(self):
        denied=self.mode in ('ability_insufficient_mana','ability_wrong_color')
        if denied:self.step('require_can_activate',ref='source',value=0);self.step('activate',ref='source',value=0);self.actions+=self.checks();return
        times=4 if self.mode=='combined_lethal' else 3 if self.mode=='ability_lethal' else 2 if self.mode in ('ability_repeat','ability_cleanup','combined_cleanup') else 1
        for i in range(times):
            self.step('require_can_activate',ref='source',value=1);self.step('activate',ref='source',value=1);self.spend(self.owner,self.p['activation']['mana']);self.step('assert_unused_choices',value=0)
            self.actions+=self.checks(stack=1);self.snapshot=len(self.actions)
            if self.mode=='ability_source_removed_pending':self.kill('source',True);self.actions+=self.checks(stack=1)
            self.step('resolve');self.step('refresh')
            if self.e['source']['alive']:
                self.e['source']['dp']+=self.p['activation']['power'];self.e['source']['dt']+=self.p['activation']['toughness']
                if self.e['source']['toughness']+self.e['source']['dt']<=0:self.kill('source')
            self.actions+=self.checks()
        if self.mode in ('ability_cleanup','combined_cleanup'):self.clean()
    def bounce(self):
        self.step('choose_target',player=self.owner,ref='source');self.begin('own_probe');self.step('resolve');self.finish('own_probe')
        self.e['source']['alive']=False;self.z['hand'][self.owner].append(self.name);self.actions+=self.checks()
    def case(self):
        from recipes import step
        if self.source_hand and self.mode!='source_arrives_late':self.cast('source',self.denied_source)
        if self.mode=='source_destroyed_before':self.kill('source',True);self.actions+=self.checks()
        if not self.denied_source:
            for i in range(self.count):self.cast(f'witness_{i}',self.mode in ('witness_insufficient_mana','witness_wrong_color'))
            if self.mode=='source_arrives_late':self.cast('source')
            if self.mode=='late_ally':self.cast('late')
            if self.mode=='activation_not_cast':
                self.step('require_can_activate',ref='mana_control',value=1);self.step('activate',ref='mana_control',value=1);self.e['mana_control']['tapped']=True;self.mana[self.owner][0]+=2;self.actions+=self.checks()
            if self.mode in ('unblocked','blocked','base_double_strike','boosted_double_strike','lifelink_cleanup','base_no_lifelink','two_spell_lifelink','fresh_attack','lethal_combat_death','nonlethal_cleanup'):self.combat(self.mode=='fresh_attack')
            if self.mode in ('cleanup','nonlethal_cleanup'):self.clean()
            if self.mode=='lifelink_cleanup':
                self.clean();self.step('advance_turn');self.step('advance_turn');self.step('untap_step',player=self.owner)
                for e in self.e.values():
                    if e['alive'] and e['player']==self.owner:e['tapped']=False
                self.combat()
            if self.mode=='targetability':
                for side in ('own','enemy'):
                    self.step('can_target_creature',ref=side+'_probe',other='source',value=int(side=='own'))
                    self.step('probe_activation_target',ref=side+'_ability',other='source',index=-1,value=int(side=='own'))
            if self.mode in ('own_bounce','bounce_no_death'):self.bounce()
            if self.mode in ('self_death','boosted_death','death_snapshot','two_source_deaths'):
                if self.mode=='death_snapshot':self.snapshot=len(self.actions)
                self.kill('source',True)
                if self.mode=='two_source_deaths':self.kill('second',True)
                self.actions+=self.checks()
            if self.mode in ('other_death','opponent_death'):
                self.kill('idle' if self.mode=='other_death' else 'enemy',True);self.actions+=self.checks()
            if self.activation_mode:self.ability()
        final=self.checks();seq=self.pre+self.actions+final
        if self.mode in ('ready_snapshot','pending_snapshot','death_snapshot','ability_pending_snapshot','combined_pending_snapshot'):
            cut=0 if self.mode=='ready_snapshot' else self.snapshot;tail=self.actions[cut:]
            seq=self.pre+self.actions[:cut]+[step('capture')]+tail+final+[step('record_state'),step('ai_roundtrip'),step('restore')]+tail+final+[step('assert_recorded_state')]
        if self.mode=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        return dict(name=f'CastTemporaryStatTriggerRecipe_{self.csvid}_{self.variant}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=seq,
                    _recipe=dict(family=FAMILY,version=2,variant=self.variant,params=self.d['params'],oracle_text=self.d['oracle_text'],
                    untested_components=['Independent arbitrary windows between multiple mandatory stat/lifelink triggers, multiplayer and replacement effects.','Normal land-play history action and an external copied-spell source are not claimed; activations and creature/artifact-creature negatives are real actions.','Full automatic turn sequence is not claimed: cleanup/start-turn/untap use real APIs followed by existing first-main fixture selection.']))
def expand(name,csvid,d,p,selected,rows,o):
    validate_witnesses(o,rows)
    if len(set(selected))!=len(selected) or set(selected)-set(variants(p)):raise ValueError('unsupported temporary stat variants')
    return [StatLedger(name,csvid,d,p,v,rows,o).case() for v in selected]
