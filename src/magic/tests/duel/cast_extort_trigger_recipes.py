"""Whole-Oracle Extort: real own casts, hybrid payments, life loss and passives."""
import copy,json
from pathlib import Path
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from cast_untap_trigger_recipes import pin,exact

FAMILY='cast_extort_trigger'
DATA=Path(__file__).parent/'data/cast_extort_trigger_wave8_profiles.json'
CLAUSE='Extort (Whenever you cast a spell, you may pay {W/B}. If you do, each opponent loses 1 life and you gain that much life.)'

def pins():return json.loads(DATA.read_text(encoding='utf8'))
def classify(r):
    from recipes import cost_from_oracle
    from creature_recipes import oracle_keywords
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or 'Token' in r.get('type_line',''):raise ValueError('normal whole Extort creature required')
    lines=r.get('oracle_text','').splitlines()
    if lines.count(CLAUSE)!=1 or r.get('keywords',[]).count('Extort')!=1:raise ValueError('exact complete Extort clause required')
    rest=copy.deepcopy(r);rest['oracle_text']='\n'.join(s for s in lines if s!=CLAUSE);rest['keywords']=[k for k in r['keywords'] if k!='Extort']
    keys=oracle_keywords(rest)
    if set(keys)-{'Defender','Flying','Vigilance','Lifelink'}:raise ValueError('uncalibrated Extort passive component')
    return dict(mana=cost_from_oracle(r),keywords=keys,power=int(r['power']),toughness=int(r['toughness']),fee='{W/B}',scope='own')
def parameters(p):return {k:copy.deepcopy(p[k]) for k in ('fee','scope','keywords')}
def variants(p):
    modes=['effect','spell_creature','spell_enchantment','spell_instant','spell_sorcery','opponent_cast','repeat_same_turn',
           'fee_white','fee_black','fee_unaffordable','fee_wrong_color','witness_payment','witness_insufficient_mana','witness_wrong_color',
           'source_cast','source_payment','source_insufficient_mana','source_wrong_color','source_removed_before','source_removed_after_announcement',
           'source_arrives_late','countered_witness','two_sources','two_sources_budget_one','two_sources_budget_two','mixed_sources',
           'cleanup','real_turn','bare_zones','empty_library','ready_snapshot','pending_snapshot','resolved_snapshot','combat_clause','fresh_attack']
    if 'Flying' in p['keywords']:modes+=['flying_reach','block_flyer']
    return [f'{m}_{owner}' for owner in (0,1) for m in modes]+['decline_0','both_pay_white_0','both_pay_black_0','payment_cancel_0']
def spec(d,r):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r.get('oracle_text'):raise ValueError('exact Extort family/version/full Oracle required')
    if not exact(pins()['sources'].get(r['name']),pin(r)):raise ValueError('Extort source identity changed')
    p=classify(r)
    if not exact(d.get('params'),parameters(p)):raise ValueError('Extort parameters must derive from complete Oracle')
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or len(set(vs))!=len(vs) or set(vs)-set(variants(p)):raise ValueError('unique supported Extort variants required')
    return dict(p,family=FAMILY)
def validate_witnesses(o,rows):
    from creature_recipes import oracle_keywords
    data=pins()
    for name,expected in dict(data['witnesses'],**data['sources']).items():
        if name not in rows or rows[name]['handler'] in ('NULL','0') or not exact(pin(o.get(name,{})),expected):raise ValueError('Extort source/witness changed: '+name)
        if rows[name]['handler']=='card_dummy':
            if name in data['sources']:raise ValueError('unimplemented Extort source')
            oracle_keywords(o[name])
    for name in data['sources']:
        ui=data['presentations'].get(name)
        if not ui or ui!={'handler':rows[name]['handler'],'acceptance':'optional_trigger_then_hybrid_payment'}:raise ValueError('Extort presentation changed')

class ExtortLedger:
    def __init__(self,name,csvid,d,p,v,rows,o):
        from recipes import card,step
        self.name=name;self.csvid=csvid;self.d=d;self.p=p;self.variant=v;self.mode,owner=v.rsplit('_',1);self.owner=int(owner);self.rows=rows;self.o=o
        self.actor=1-self.owner if self.mode=='opponent_cast' else self.owner
        self.e={};self.setup=[];self.actions=[];self.z={z:[[],[]] for z in ('hand','graveyard','exile','library')};self.life=[20,20];self.mana=[[30,0,30,30,30,30] for _ in (0,1)];self.pending=None
        for who in (0,1):
            for z,base in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY)):
                ns=[] if self.mode=='bare_zones' and z!='library' or self.mode=='empty_library' and z=='library' else list(base)
                self.z[z][who]=ns;self.setup += [card(n,f'zone_{who}_{z}_{i}',who,z) for i,n in enumerate(ns)]
        self.source_hand=self.mode in ('source_cast','source_payment','source_insufficient_mana','source_wrong_color','source_arrives_late','fresh_attack')
        self.denied_source=self.mode in ('source_insufficient_mana','source_wrong_color');self.denied=self.mode in ('witness_insufficient_mana','witness_wrong_color')
        self.add(name,'source',self.owner,'hand' if self.source_hand else 'battlefield')
        # A zero floated pool is not an insufficient generic payment if Ring
        # remains ready.  This control producer is genuinely unavailable.
        self.add('Sol Ring','control_ring',self.owner,tapped=True)
        if self.mode.startswith('two_sources'):self.add(name,'copy',self.owner)
        if self.mode=='mixed_sources':self.add(name,'copy',1-self.owner)
        self.witness={'spell_creature':'Grizzly Bears','spell_enchantment':'Seal of Cleansing','spell_instant':'Reach Through Mists','spell_sorcery':'Divination','witness_wrong_color':'Grizzly Bears'}.get(self.mode,'Sol Ring')
        self.count=3 if self.mode=='repeat_same_turn' else 2 if self.mode=='real_turn' else 1
        for i in range(self.count):self.add(self.witness,f'spell_{i}',self.actor,'hand')
        if self.mode=='countered_witness':self.add('Counterspell','response',1-self.actor,'hand')
        if self.mode=='mixed_sources':self.add('Sol Ring','other_spell',1-self.owner,'hand')
        if self.mode in ('combat_clause','flying_reach') and 'Flying' in p['keywords']:self.add('Giant Spider' if self.mode=='flying_reach' else 'Grizzly Bears','blocker',1-self.owner)
        if self.mode=='block_flyer':self.add('Suntail Hawk','attacker',1-self.owner)
        if self.source_hand:self.mana[self.owner][1]=p['mana'][1]
        if self.mode in ('fee_white','fee_black','fee_unaffordable','fee_wrong_color','witness_payment','both_pay_white','both_pay_black','payment_cancel','two_sources_budget_one','two_sources_budget_two'):
            pools=self.cost(self.witness)
            if self.mode in ('fee_white','two_sources_budget_one'):pools[5]+=1
            elif self.mode=='fee_black':pools[1]+=1
            elif self.mode=='fee_wrong_color':pools[2]+=1
            elif self.mode in ('both_pay_white','both_pay_black','payment_cancel','two_sources_budget_two'):pools[1]+=1;pools[5]+=1
            self.mana[self.owner]=pools
        if self.denied:self.mana[self.actor]=[sum(self.cost(self.witness)),0,0,0,0,0] if self.mode=='witness_wrong_color' else [0]*6
        if self.mode=='source_payment':self.mana[self.owner]=list(p['mana'])
        if self.denied_source:self.mana[self.owner]=[sum(p['mana']),0,0,0,0,0] if self.mode=='source_wrong_color' else [0]*6
        self.pre=[step('turn',player=self.owner if self.source_hand else self.actor,value=0)]+[step('set_mana',player=w,color=c,value=n) for w in (0,1) for c,n in enumerate(self.mana[w])]
        self.universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in self.e.values()}
    def step(self,op,**kw):
        from recipes import step
        self.actions.append(step(op,**kw))
    def add(self,name,ref,who,zone='battlefield',tapped=False):
        from recipes import card
        r=self.o[name];obj=card(name,ref,who,zone)
        if tapped:obj['state']=['tapped']
        self.setup.append(obj)
        if zone=='hand':self.z['hand'][who].append(name)
        creature='Creature' in r['type_line'].partition(' — ')[0].split()
        self.e[ref]=dict(name=name,player=who,alive=zone=='battlefield',hand=zone=='hand',tapped=tapped,creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,damage=0)
    def cost(self,n):
        from recipes import cost_from_oracle
        r=self.o[n]
        if r['layout']!='normal' or 'Land' in r['type_line'].partition(' — ')[0].split():raise ValueError('normal actual Extort spell witness required')
        return cost_from_oracle(r)
    def spend(self,w,cost):
        self.mana[w]=[a-b for a,b in zip(self.mana[w],cost)]
        if min(self.mana[w])<0:raise ValueError('Extort payment underfunded')
    def notify(self,who):
        planned=0
        for ref in ('source','copy'):
            if ref not in self.e or not self.e[ref]['alive'] or self.e[ref]['player']!=who:continue
            pay_color=1 if self.mode in ('fee_black','both_pay_black') else 5
            if not self.mana[who][pay_color]:pay_color=1 if self.mana[who][1] else 5
            affordable=self.mana[who][pay_color]>0
            if who==0:
                if self.mode=='decline' or self.mode=='two_sources_budget_one' and not affordable:continue
                self.step('choose_trigger_source',player=who,ref=ref);planned+=1
            if not affordable:continue
            both=self.mana[who][1]>0 and self.mana[who][5]>0
            if self.mode=='payment_cancel':
                if who!=0 or not both:raise ValueError('actual two-vector human hybrid Cancel menu required')
                self.step('choose_dialog',player=who,value=0);planned+=1;continue
            if who==0 and both:self.step('choose_payment',player=who,value=1<<(4*pay_color));planned+=1
            if who==1 and both:
                # Equal two-instance budget spends both vectors independently
                # of which vector AI chooses first; no interim color receipt.
                if self.mode!='two_sources_budget_two':raise ValueError('AI hybrid alternatives must be constrained')
            cost=[0]*6;cost[pay_color]=1;self.spend(who,cost);self.life[1-who]-=1;self.life[who]+=1
        return planned
    def begin(self,ref,no=False,response=False):
        e=self.e[ref];w=e['player'];self.step('cast_response' if response else 'cast',ref=ref,value=int(not no))
        if no:self.step('assert_stack',value=0);self.step('assert_choices_consumed');return 0
        self.spend(w,self.cost(e['name']));self.z['hand'][w].remove(e['name']);e['hand']=False
        self.step('assert_stack',value=2 if response else 1);self.step('assert_card',ref=ref,property='in_hand',value=0)
        planned=self.notify(w)
        if not response:
            self.step('announce_spell',ref=ref)
            if planned:self.step('assert_unused_choices',value=0)
        return planned
    def finish(self,ref,countered=False):
        e=self.e[ref];r=self.o[e['name']];head=r['type_line'].partition(' — ')[0].split()
        if countered or not set(head)&{'Creature','Artifact','Enchantment'}:self.z['graveyard'][e['player']].append(e['name'])
        else:e['alive']=True
        draws=1 if e['name']=='Reach Through Mists' else 2 if e['name']=='Divination' else 0
        if not countered:
            for _ in range(draws):
                if not self.z['library'][e['player']]:raise ValueError('draw witness library underfunded')
                self.z['hand'][e['player']].append(self.z['library'][e['player']].pop(0))
    def kill(self,ref,explicit=True):
        if explicit:self.step('destroy',ref=ref)
        e=self.e[ref];e['alive']=False;self.z['graveyard'][e['player']].append(e['name'])
    def checks(self):
        from recipes import step,observe
        out=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for ref,e in self.e.items():
            if e['alive']:
                out += [observe(ref,'in_play',1),observe(ref,'in_hand',0),observe(ref,'tapped',int(e['tapped']))]
                if e['creature']:out += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness']),observe(ref,'damage',e['damage']),observe(ref,'plus1_counters',0)]
            elif e['hand']:out.append(observe(ref,'in_hand',1))
        for w in (0,1):
            out += [step('assert_life',player=w,value=self.life[w]),step('assert_mana_total',player=w,value=sum(self.mana[w])),step('assert_zone',player=w,zone='battlefield',value=sum(e['alive'] and e['player']==w for e in self.e.values()))]
            out += [step('assert_mana',player=w,color=c,value=n) for c,n in enumerate(self.mana[w])]
            for zone in self.z:
                ns=self.z[zone][w];out.append(step('assert_zone',player=w,zone=zone,value=len(ns)))
                out += [step('assert_zone_multiplicity',player=w,zone=zone,index=self.rows[n]['csvid'],value=ns.count(n)) for n in sorted(self.universe)]
                if zone=='library':out += [step('assert_zone_card',player=w,zone=zone,index=i,value=self.rows[n]['csvid']) for i,n in enumerate(ns)]
        return out
    def set_pools(self,pools):
        self.mana=copy.deepcopy(pools)
        for w in (0,1):
            for c,n in enumerate(self.mana[w]):self.step('set_mana',player=w,color=c,value=n)
    def zero(self):
        self.actions+=self.checks();self.set_pools([[0]*6,[0]*6])
    def cast_source(self):
        self.step('turn',player=self.owner,value=0);self.begin('source',self.denied_source)
        if self.denied_source:return
        self.step('resolve');self.step('assert_choices_consumed');self.finish('source')
        if self.mode=='source_payment':self.actions+=self.checks();self.set_pools([[30,0,30,30,30,30] for _ in (0,1)])
    def blocked_damage(self,other):
        self.step('combat_damage');source=self.e['source'];opponent=self.e[other]
        source['damage']+=opponent['power'];opponent['damage']+=source['power']
        if 'Lifelink' in self.p['keywords']:self.life[self.owner]+=source['power']
        for ref in ('source',other):
            if self.e[ref]['damage']>=self.e[ref]['toughness']:self.kill(ref,explicit=False)
    def combat(self):
        from recipes import observe
        keys=self.p['keywords'];allowed=self.mode!='fresh_attack' and 'Defender' not in keys
        if self.mode=='block_flyer':
            self.step('turn',player=1-self.owner,value=0);self.step('begin_attackers');self.step('attack',ref='attacker',value=1);self.e['attacker']['tapped']=True
            self.step('finish_attackers');self.step('begin_blockers');self.step('block',ref='source',other='attacker',value=1)
            self.actions.append(observe('source','blocking_count',1));self.step('advance_combat');self.blocked_damage('attacker');self.step('end_combat');return
        self.step('turn',player=self.owner,value=0);self.step('begin_attackers');self.step('attack',ref='source',value=int(allowed))
        self.e['source']['tapped']=bool(allowed and 'Vigilance' not in keys)
        self.actions += [observe('source','attacking',int(allowed)),observe('source','tapped',int(self.e['source']['tapped']))]
        self.step('finish_attackers');self.step('begin_blockers')
        if 'blocker' in self.e:
            legal=self.mode=='flying_reach';self.step('block',ref='blocker',other='source',value=int(legal))
            self.actions.append(observe('blocker','blocking_count',int(legal)))
            if legal:self.step('advance_combat');self.blocked_damage('blocker');self.step('end_combat');return
        self.step('advance_combat');self.step('combat_damage');damage=self.p['power'] if allowed else 0
        self.life[1-self.owner]-=damage
        if 'Lifelink' in keys:self.life[self.owner]+=damage
        self.step('end_combat');self.actions.append(observe('source','attacking',0))
    def case(self):
        from recipes import step
        if self.source_hand and self.mode!='source_arrives_late':self.cast_source()
        if self.mode=='source_removed_before':self.kill('source')
        if not self.denied_source:
            for i in range(self.count):
                if self.mode=='real_turn' and i:
                    self.zero();self.step('advance_turn');self.step('advance_turn');self.actions+=self.checks();self.set_pools([[30,0,30,30,30,30] for _ in (0,1)])
                self.step('turn',player=self.actor,value=0);ref=f'spell_{i}';self.begin(ref,self.denied)
                if self.denied:self.actions+=self.checks();continue
                if self.pending is None:self.pending=len(self.actions)
                if self.mode=='source_removed_after_announcement':self.kill('source')
                if self.mode=='countered_witness':
                    planned=self.begin('response',response=True);self.step('resolve_response')
                    if planned:self.step('assert_unused_choices',value=0)
                    self.step('assert_choices_consumed');self.finish('response');self.finish(ref,True)
                else:self.step('resolve');self.step('assert_choices_consumed');self.finish(ref)
                self.actions+=self.checks()
            if self.mode=='source_arrives_late':self.cast_source()
            if self.mode=='mixed_sources':
                self.step('turn',player=1-self.owner,value=0);self.begin('other_spell');self.step('resolve');self.step('assert_choices_consumed');self.finish('other_spell')
        if self.mode in ('combat_clause','fresh_attack','flying_reach','block_flyer'):self.combat()
        if self.mode=='cleanup':self.zero();self.step('cleanup')
        checks=self.checks();seq=self.pre+self.actions+checks
        if self.mode in ('ready_snapshot','pending_snapshot'):
            cut=0 if self.mode=='ready_snapshot' else self.pending
            seq=self.pre+self.actions[:cut]+[step('capture')]+self.actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+self.actions[cut:]+checks+[step('assert_recorded_state')]
        if self.mode=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        return dict(name=f'CastExtortTriggerRecipe_{self.csvid}_{self.variant}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=seq,
                    _recipe=dict(family=FAMILY,version=2,variant=self.variant,params=self.d['params'],oracle_text=self.d['oracle_text'],untested_components=[
                        'Multiplayer, replacements/prevention of life loss or gain, lethal game-ending boundaries, copied abilities and granted duplicate Extort on one creature.',
                        'Only existing real announcement/response boundaries; no separately pending committed-trigger response window. Actual keyword combat is covered separately from Extort life loss.']))
def expand(name,csvid,d,p,selected,rows,o):
    validate_witnesses(o,rows)
    if len(set(selected))!=len(selected) or set(selected)-set(variants(p)):raise ValueError('unsupported Extort variants')
    return [ExtortLedger(name,csvid,d,p,v,rows,o).case() for v in selected]
