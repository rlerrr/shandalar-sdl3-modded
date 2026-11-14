"""Oracle cast-to-untap clauses, including real entry and untap-step restrictions."""
import copy,json,re
from pathlib import Path
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY

FAMILY='cast_untap_trigger'
DATA=Path(__file__).parent/'data/cast_untap_trigger_wave8_profiles.json'
FIELDS=('oracle_id','name','oracle_text','mana_cost','type_line','power','toughness','loyalty','keywords','colors','color_identity','cmc','layout')
COLOR={'black':'B','blue':'U','green':'G','red':'R','white':'W'}
SPELLS={'red':('Seal of Fire','Goblin Piker'),'blue':('Coral Merfolk','Wind Drake'),
        'green':('Grizzly Bears','Elvish Warrior'),'white':('Silvercoat Lion','Seal of Cleansing'),
        'black':('Scathe Zombies','Drudge Skeletons'),'artifact':('Sol Ring','Ornithopter'),
        'any':('Sol Ring','Grizzly Bears'),'historic':('Sol Ring','Isamaru, Hound of Konda')}

def pins():return json.loads(DATA.read_text(encoding='utf8'))
def pin(r):return {k:r[k] for k in FIELDS if k in r}
def exact(a,b):
    if type(a) is not type(b):return False
    if isinstance(a,dict):return a.keys()==b.keys() and all(exact(a[k],b[k]) for k in a)
    if isinstance(a,list):return len(a)==len(b) and all(exact(x,y) for x,y in zip(a,b))
    return a==b

def classify(r):
    from recipes import cost_from_oracle
    from creature_recipes import oracle_keywords
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or 'Token' in r.get('type_line',''):raise ValueError('normal whole untap watcher required')
    types=set(r['type_line'].partition(' — ')[0].split());creature='Creature' in types
    if not types&{'Creature','Artifact','Enchantment'}:raise ValueError('untap watcher permanent required')
    lines=r['oracle_text'].splitlines();triggers=[s for s in lines if s.startswith('Whenever ')]
    if len(triggers)!=1:raise ValueError('one cast untap trigger required')
    m=re.fullmatch(r'Whenever (you cast|a player casts) (a spell|an artifact spell|a historic spell|a (black|blue|green|red|white) spell), (.+)',triggers[0])
    if not m:raise ValueError('unreviewed cast untap trigger')
    scope='own' if m[1]=='you cast' else 'any';predicate='any' if m[2]=='a spell' else 'artifact' if m[2]=='an artifact spell' else 'historic' if m[2]=='a historic spell' else m[3]
    text=m[4];reminder=' (Artifacts, legendaries, and Sagas are historic.)'
    if predicate=='historic':
        if not text.endswith(reminder):raise ValueError('complete historic reminder required')
        text=text[:-len(reminder)]
    paid=re.fullmatch(r'you may pay (\{[1-9]\})\. If you do, untap target permanent\.',text)
    cost=paid[1] if paid else None;optional=bool(paid);recipient=None
    if paid:recipient='permanent'
    elif text=='you may untap target creature.':recipient='creature';optional=True
    elif text=='untap all nonland permanents you control.':recipient='controlled_nonland'
    else:
        names={'this creature',r['name'],r['name'].split()[0].rstrip(',')}
        for name in names:
            if text=='untap '+name+'.':recipient='self'
            if text=='you may untap '+name+'.':recipient='self';optional=True
    if recipient is None:raise ValueError('unreviewed complete untap reward')
    remainder=[s for s in lines if s!=triggers[0]];no_untap=False;enters_tapped=False
    for self_name in ('This creature',r['name'],r['name'].split()[0].rstrip(',')):
        clause=self_name+" doesn't untap during your untap step."
        compound=self_name+" enters tapped and doesn't untap during your untap step."
        if clause in remainder:remainder.remove(clause);no_untap=True
        if compound in remainder:remainder.remove(compound);no_untap=True;enters_tapped=True
    if creature:
        keywords=oracle_keywords(dict(r,oracle_text='\n'.join(remainder)));power=int(r['power']);toughness=int(r['toughness'])
    else:
        if remainder or r['keywords']:raise ValueError('extra untap source clauses require another model')
        keywords=[];power=toughness=None
    if no_untap and recipient!='self':raise ValueError('reviewed self untap restriction required')
    return dict(scope=scope,predicate=predicate,recipient=recipient,optional=optional,trigger_cost=cost,
                trigger_mana=cost_from_oracle(dict(mana_cost=cost)) if cost else [0]*6,
                no_untap=no_untap,enters_tapped=enters_tapped,mana=cost_from_oracle(r),keywords=keywords,power=power,toughness=toughness,legendary='Legendary' in types)

def parameters(p):return {k:copy.deepcopy(p[k]) for k in ('scope','predicate','recipient','optional','trigger_cost','no_untap','enters_tapped')}
def matches(r,predicate):
    head,_,tail=r['type_line'].partition(' — ')
    if 'Land' in head.split():return False
    if predicate=='any':return True
    if predicate=='artifact':return 'Artifact' in head.split()
    if predicate=='historic':return 'Artifact' in head.split() or 'Legendary' in head.split() or 'Saga' in tail.split()
    return COLOR[predicate] in r['colors']

def variants(p):
    modes=['effect','alternate_spell','opponent_cast','repeat_same_turn','witness_payment','witness_insufficient_mana',
           'source_cast','source_payment','source_insufficient_mana','source_removed_before','source_removed_after_announcement',
           'source_arrives_late','countered_witness','cleanup','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot']
    if any(p['mana'][1:]):modes.append('source_wrong_color')
    if p['predicate'] in COLOR:modes.append('witness_wrong_color')
    if p['predicate']!='any':modes.append('nonmatching_spell')
    if p['recipient'] in ('permanent','creature'):modes+=['enemy_target','already_untapped','source_target']
    if p['recipient']=='permanent':modes+=['land_target','artifact_target','enchantment_target','planeswalker_target','functional_target','exact_trigger_payment','trigger_unaffordable']
    if p['recipient']=='controlled_nonland':modes+=['land_creature_excluded','enters_after_trigger','functional_target']
    if p['predicate']=='historic':modes+=['saga_spell','overlap_once']
    if p['no_untap']:modes+=['untap_step_only','untap_step_then_cast','untap_step_snapshot']
    if p['enters_tapped']:modes.append('entry_only')
    if not p['legendary']:modes.append('two_sources')
    result=[f'{mode}_{owner}' for owner in (0,1) for mode in modes]
    if p['optional']:result.append('decline_0')
    return result

def spec(d,r):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r.get('oracle_text'):raise ValueError('exact untap family/version/full Oracle required')
    if not exact(pins()['sources'].get(r['name']),pin(r)):raise ValueError('untap source identity changed')
    p=classify(r)
    if not exact(d.get('params'),parameters(p)):raise ValueError('untap params must derive from whole Oracle')
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or len(set(vs))!=len(vs) or set(vs)-set(variants(p)):raise ValueError('explicit unique supported untap variants required')
    return dict(p,family=FAMILY)

def validate_witnesses(o,rows):
    from creature_recipes import oracle_keywords
    data=pins()
    for name,expected in dict(data['witnesses'],**data['sources']).items():
        if name not in rows or rows[name]['handler'] in ('NULL','0') or not exact(pin(o.get(name,{})),expected):raise ValueError('untap source/witness changed: '+name)
        if rows[name]['handler']=='card_dummy':
            if name in data['sources']:raise ValueError('unimplemented untap source')
            oracle_keywords(o[name])
    for name,ui in data['presentations'].items():
        p=classify(o[name])
        expected='payment_then_target' if p['trigger_cost'] else 'optional_trigger_then_target' if p['recipient']=='creature' else 'optional_trigger'
        if name not in data['sources'] or not p['optional'] or ui['acceptance']!=expected or rows[name]['handler']!=ui['handler']:raise ValueError('untap input presentation changed')


class UntapLedger:
    def __init__(self,name,csvid,d,p,v,rows,o):
        from recipes import card,step
        self.name=name;self.csvid=csvid;self.d=d;self.p=p;self.variant=v;self.mode,owner=v.rsplit('_',1);self.owner=int(owner);self.rows=rows;self.o=o
        self.actor=1-self.owner if self.mode=='opponent_cast' else self.owner;self.turn=self.actor
        self.setup=[];self.e={};self.z={z:[[],[]] for z in ('hand','graveyard','exile','library')};self.mana=[[30]*6,[30]*6];self.life=[20,20];self.actions=[];self.pending=None
        for who in (0,1):
            for zone,base in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY)):
                ns=[] if self.mode=='bare_zones' and zone!='library' else list(base);self.z[zone][who]=ns
                self.setup += [card(n,f'zone_{who}_{zone}_{i}',who,zone) for i,n in enumerate(ns)]
        self.source_hand=self.mode in ('source_cast','source_payment','source_insufficient_mana','source_wrong_color','source_arrives_late','entry_only')
        self.denied_source=self.mode in ('source_insufficient_mana','source_wrong_color');self.denied=self.mode in ('witness_insufficient_mana','witness_wrong_color')
        source_tapped=None if p['enters_tapped'] or self.source_hand else p['recipient'] in ('self','controlled_nonland')
        self.add(name,'source',self.owner,'hand' if self.source_hand else 'battlefield',source_tapped)
        target_mode=self.mode in ('enemy_target','already_untapped','source_target','land_target','artifact_target','enchantment_target','planeswalker_target','functional_target')
        self.add('Grizzly Bears','own_creature',self.owner,tapped=p['recipient']=='controlled_nonland' or p['recipient'] in ('permanent','creature') and not target_mode)
        self.add('Grizzly Bears','enemy_creature',1-self.owner,tapped=True)
        self.add('Sol Ring','own_artifact',self.owner,tapped=p['recipient']=='controlled_nonland' or self.mode in ('artifact_target','functional_target','trigger_unaffordable'))
        if self.mode=='two_sources':self.add(name,'copy',self.owner,tapped=p['recipient']=='self')
        if p['recipient']=='controlled_nonland' or self.mode=='land_target':self.add('Forest','own_land',self.owner,tapped=True)
        if p['recipient']=='controlled_nonland' or self.mode=='enchantment_target':self.add('Seal of Cleansing','own_enchantment',self.owner,tapped=True)
        if p['recipient']=='controlled_nonland' or self.mode=='planeswalker_target':self.add('Chandra, Pyrogenius','own_walker',self.owner,tapped=True)
        if p['recipient']=='controlled_nonland':
            self.add('Dryad Arbor','own_land_creature',self.owner,tapped=True);self.add('Sol Ring','enemy_artifact',1-self.owner,tapped=True)
        if p['no_untap']:
            self.e['own_creature']['tapped']=True;next(x for x in self.setup if x['ref']=='own_creature')['state']=['tapped']
        if self.mode=='source_target' and not self.source_hand and p['recipient'] in ('permanent','creature'):
            self.e['source']['tapped']=True;next(x for x in self.setup if x['ref']=='source')['state']=['tapped']
        if self.mode=='decline' and p['trigger_cost']:self.add('Forest','payment_land',self.owner)
        self.witness=SPELLS[p['predicate']][self.mode=='alternate_spell']
        if self.mode=='saga_spell':self.witness='Song of Freyalise'
        if self.mode=='overlap_once':self.witness='Hope of Ghirapur'
        if self.mode=='enters_after_trigger':self.witness='Marble Diamond'
        if self.mode=='nonmatching_spell':self.witness=next(n for n in ('Sol Ring','Grizzly Bears','Scathe Zombies','Seal of Fire') if not matches(o[n],p['predicate']))
        self.count=3 if self.mode=='repeat_same_turn' else 1
        for i in range(self.count):self.add(self.witness,f'spell_{i}',self.actor,'hand')
        if self.mode=='countered_witness':self.add('Counterspell','response',1-self.actor,'hand')
        if self.mode=='witness_payment':self.mana[self.actor]=self.cost(self.witness)
        if self.denied:self.mana[self.actor]=[sum(self.cost(self.witness))]+[0]*5 if self.mode=='witness_wrong_color' else [0]*6
        if self.mode=='source_payment':self.mana[self.owner]=list(p['mana'])
        if self.denied_source:self.mana[self.owner]=[sum(p['mana'])]+[0]*5 if self.mode=='source_wrong_color' else [0]*6
        if self.mode=='exact_trigger_payment':self.mana[self.owner]=[a+b for a,b in zip(self.cost(self.witness),p['trigger_mana'])]
        if self.mode=='trigger_unaffordable':self.mana[self.owner]=self.cost(self.witness)
        if self.mode=='decline' and p['trigger_cost']:self.mana[self.owner]=self.cost(self.witness)
        # Zero floated mana does not deny a generic cast while a ready Ring can
        # pay it.  AI may also partially fund an unaffordable optional fee.
        denied_cost=p['mana'] if self.denied_source else self.cost(self.witness) if self.denied else None
        block_generic=denied_cost is not None and not any(denied_cost[1:]) and 0<denied_cost[0]<=2
        block_partial_fee=self.mode=='witness_payment' and p['trigger_cost'] and self.owner==1
        if block_generic or block_partial_fee:
            self.e['own_artifact']['tapped']=True;next(x for x in self.setup if x['ref']=='own_artifact')['state']=['tapped']
        self.pre=[step('turn',player=self.owner if self.source_hand else self.actor,value=0)]+[step('set_mana',player=who,color=c,value=v) for who in (0,1) for c,v in enumerate(self.mana[who])]
        if self.source_hand:self.turn=self.owner
        if 'own_walker' in self.e:self.pre.append(step('assert_counter',ref='own_walker',counter_kind='loyalty',value=int(o['Chandra, Pyrogenius']['loyalty'])))
        self.universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in self.e.values()}

    def step(self,op,**kw):
        from recipes import step
        self.actions.append(step(op,**kw))
    def select_turn(self,who):self.step('turn',player=who,value=0);self.turn=who
    def add(self,name,ref,who,zone='battlefield',tapped=False):
        from recipes import card
        r=self.o[name];head=r['type_line'].partition(' — ')[0].split();creature='Creature' in head;obj=card(name,ref,who,zone)
        if tapped:obj['state']=['tapped']
        self.setup.append(obj)
        if zone=='hand':self.z['hand'][who].append(name)
        self.e[ref]=dict(name=name,player=who,alive=zone=='battlefield',hand=zone=='hand',tapped=bool(tapped) or zone=='battlefield' and name==self.name and self.p['enters_tapped'],
                         creature=creature,land='Land' in head,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,
                         loyalty=int(r['loyalty']) if 'Planeswalker' in head else None,no_untap=name==self.name and self.p['no_untap'])
    def cost(self,name):
        from recipes import cost_from_oracle
        r=self.o[name]
        if r['layout'] not in ('normal','saga') or r['layout']=='saga' and 'Saga' not in r['type_line'].partition(' — ')[2].split() or 'Token' in r['type_line'] or 'Land' in r['type_line'].partition(' — ')[0].split():raise ValueError('complete actual spell witness required')
        return [0]*6 if r['mana_cost']=='{0}' else cost_from_oracle(r)
    def spend(self,who,cost):
        self.mana[who]=[a-b for a,b in zip(self.mana[who],cost)]
        if min(self.mana[who])<0:raise ValueError('untap payment underfunded')
    def die(self,ref):
        self.step('destroy',ref=ref);e=self.e[ref];e['alive']=False;self.z['graveyard'][e['player']].append(e['name'])
    def target(self):
        if self.mode=='enemy_target':return 'enemy_creature'
        if self.mode=='source_target':return 'source'
        return {'land_target':'own_land','artifact_target':'own_artifact','enchantment_target':'own_enchantment','planeswalker_target':'own_walker','functional_target':'own_artifact'}.get(self.mode,'own_creature')
    def notify(self,name,who):
        if self.p['scope']=='own' and who!=self.owner or not matches(self.o[name],self.p['predicate']):return 0
        planned=0
        for source in ('source','copy'):
            if source not in self.e or not self.e[source]['alive']:continue
            if self.p['trigger_cost']:
                if self.mode=='decline' or self.mana[self.owner][0]<self.p['trigger_mana'][0]:
                    if self.owner==0:self.step('choose_cancel_mana',player=self.owner);planned+=1
                    continue
                self.spend(self.owner,self.p['trigger_mana'])
            elif self.p['optional'] and self.owner==0:
                if self.mode=='decline':continue
                self.step('choose_trigger_source',player=self.owner,ref=source);planned+=1
            recipient=self.p['recipient']
            if recipient in ('permanent','creature'):
                target=self.target();e=self.e[target]
                if not e['alive'] or recipient=='creature' and not e['creature']:raise ValueError('Oracle-legal untap target required')
                self.step('choose_target',player=self.owner,ref=target);planned+=1;e['tapped']=False
            elif recipient=='self':self.e[source]['tapped']=False
            else:
                for e in self.e.values():
                    if e['alive'] and e['player']==self.owner and not e['land']:e['tapped']=False
        return planned

    def checks(self):
        from recipes import step,observe
        out=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for ref,e in self.e.items():
            if e['alive']:
                out += [observe(ref,'in_play',1),observe(ref,'in_hand',0),observe(ref,'tapped',int(e['tapped']))]
                if e['creature']:out += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness']),observe(ref,'damage',0),observe(ref,'plus1_counters',0)]
                if e['loyalty'] is not None:out.append(step('assert_counter',ref=ref,counter_kind='loyalty',value=e['loyalty']))
            elif e['hand']:out.append(observe(ref,'in_hand',1))
        for who in (0,1):
            out += [step('assert_life',player=who,value=self.life[who]),step('assert_mana_total',player=who,value=sum(self.mana[who])),step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in self.e.values()))]
            for z in self.z:
                ns=self.z[z][who];out.append(step('assert_zone',player=who,zone=z,value=len(ns)))
                out += [step('assert_zone_multiplicity',player=who,zone=z,index=self.rows[n]['csvid'],value=ns.count(n)) for n in sorted(self.universe)]
                if z=='library':out += [step('assert_zone_card',player=who,zone=z,index=i,value=self.rows[n]['csvid']) for i,n in enumerate(ns)]
            if self.mode in ('source_payment','witness_payment','exact_trigger_payment','trigger_unaffordable','decline','source_wrong_color','source_insufficient_mana','witness_wrong_color','witness_insufficient_mana'):
                out += [step('assert_mana',player=who,color=c,value=v) for c,v in enumerate(self.mana[who])]
        return out

    def begin(self,ref,no=False,response=False):
        e=self.e[ref];who=e['player'];name=e['name'];self.step('cast_response' if response else 'cast',ref=ref,value=int(not no))
        if no:self.step('assert_stack',value=0);self.step('assert_choices_consumed');return
        self.spend(who,self.cost(name));self.z['hand'][who].remove(name);e['hand']=False
        self.step('assert_stack',value=2 if response else 1);self.step('assert_card',ref=ref,property='in_hand',value=0)
        planned=self.notify(name,who)
        if not response:
            self.step('announce_spell',ref=ref)
            if planned:self.step('assert_unused_choices',value=0)
        return planned
    def finish(self,ref,countered=False):
        e=self.e[ref];name=e['name'];who=e['player'];head=self.o[name]['type_line'].partition(' — ')[0].split()
        if countered or not set(head)&{'Creature','Artifact','Enchantment','Planeswalker'}:self.z['graveyard'][who].append(name)
        else:
            e['alive']=True;e['tapped']=self.p['enters_tapped'] if ref=='source' else 'This artifact enters tapped.' in self.o[name]['oracle_text'].splitlines()
    def cast_source(self):
        self.select_turn(self.owner);self.begin('source',self.denied_source)
        if self.denied_source:return
        self.step('resolve');self.step('assert_choices_consumed');self.finish('source')
        if self.mode=='source_payment':
            self.actions+=self.checks();self.refill()
    def zero(self):
        for who in (0,1):
            self.step('assert_mana_total',player=who,value=sum(self.mana[who]))
            for c in range(6):self.step('set_mana',player=who,color=c,value=0)
        self.mana=[[0]*6,[0]*6]
    def refill(self):
        self.mana=[[30]*6,[30]*6]
        for who in (0,1):
            for c,v in enumerate(self.mana[who]):self.step('set_mana',player=who,color=c,value=v)
    def actual_untap_cycle(self):
        self.zero()
        if self.mode=='untap_step_snapshot':self.step('capture')
        start=len(self.actions)
        for _ in range(2):
            self.step('advance_turn');self.turn=1-self.turn;self.step('untap_step',player=self.turn)
            for e in self.e.values():
                if e['alive'] and e['player']==self.turn and not e['no_untap']:e['tapped']=False
            self.actions+=self.checks()
        if self.mode=='untap_step_snapshot':
            from recipes import step
            replay=self.actions[start:];self.actions += [step('record_state'),step('ai_roundtrip'),step('restore')]+replay+[step('assert_recorded_state')]
        self.select_turn(self.owner)

    def case(self):
        from recipes import step
        if self.source_hand and self.mode!='source_arrives_late':self.cast_source()
        if self.mode=='source_removed_before':self.die('source')
        if self.mode in ('untap_step_only','untap_step_then_cast','untap_step_snapshot'):self.actual_untap_cycle()
        no_cast=self.mode in ('entry_only','untap_step_only','untap_step_snapshot') or self.denied_source
        if not no_cast:
            if self.mode=='untap_step_then_cast':self.refill()
            for i in range(self.count):
                self.select_turn(self.actor);ref=f'spell_{i}';self.begin(ref,self.denied)
                if self.denied:self.actions+=self.checks();continue
                if self.pending is None:self.pending=len(self.actions)
                if self.mode=='source_removed_after_announcement' and self.e['source']['alive']:self.die('source')
                if self.mode=='countered_witness':
                    planned=self.begin('response',response=True);self.step('resolve_response')
                    if planned:self.step('assert_unused_choices',value=0)
                    self.step('assert_choices_consumed');self.finish('response');self.finish(ref,True)
                else:self.step('resolve');self.step('assert_choices_consumed');self.finish(ref)
                self.actions+=self.checks()
            if self.mode=='source_arrives_late':self.cast_source()
        if self.mode=='functional_target':
            self.step('require_can_activate',ref='own_artifact',value=1);self.step('activate',ref='own_artifact',value=1);self.step('assert_stack',value=0)
            self.e['own_artifact']['tapped']=True;self.mana[self.owner][0]+=2
        if self.mode=='cleanup':self.zero();self.step('cleanup')
        checks=self.checks();seq=self.pre+self.actions+checks
        if self.mode in ('ready_snapshot','pending_snapshot'):
            cut=0 if self.mode=='ready_snapshot' else self.pending
            seq=self.pre+self.actions[:cut]+[step('capture')]+self.actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+self.actions[cut:]+checks+[step('assert_recorded_state')]
        if self.mode=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        return dict(name=f'CastUntapTriggerRecipe_{self.csvid}_{self.variant}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=seq,
                    _recipe=dict(family=FAMILY,version=2,variant=self.variant,params=self.d['params'],oracle_text=self.d['oracle_text'],untested_components=[
                        'Target rejection cells with engine-legal alternatives require a separate legality observer; constrained-board absence probes avoid illegal selections.',
                        'No separately pending target-committed trigger response window is exposed; no synthetic event/history/untap setter.',
                        'Multiplayer, replacements, keyword combat, and color-changing effects; paid pools are checked before explicit zeroing at real untap gates.']))

def expand(name,csvid,d,p,selected,rows,o):
    validate_witnesses(o,rows)
    if len(set(selected))!=len(selected) or set(selected)-set(variants(p)):raise ValueError('unsupported untap variants')
    return [UntapLedger(name,csvid,d,p,v,rows,o).case() for v in selected]
