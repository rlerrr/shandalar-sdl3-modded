"""Whole Oracle draw-event rewards driven by real spells and abilities."""
import copy
import json
import re
from pathlib import Path
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

FAMILY='draw_value_trigger'
DATA=Path(__file__).parent/'data/draw_value_trigger_wave8_profiles.json'
PIN_FIELDS=('oracle_id','name','oracle_text','mana_cost','type_line','power','toughness','keywords','colors','color_identity','cmc','layout')
WORDS={'a':1,'one':1,'two':2,'three':3}
COLORS={'black':'B','blue':'U','green':'G','red':'R','white':'W'}

def pins():return json.loads(DATA.read_text(encoding='utf8'))
def pin(r):return {k:r[k] for k in PIN_FIELDS if k in r}
def exact(a,b):
    if type(a) is not type(b):return False
    if isinstance(a,dict):return a.keys()==b.keys() and all(exact(a[k],b[k]) for k in a)
    if isinstance(a,list):return len(a)==len(b) and all(exact(x,y) for x,y in zip(a,b))
    return a==b

def classify(r):
    from recipes import cost_from_oracle
    from creature_recipes import oracle_keywords
    if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
            or 'Token' in r.get('type_line','')):raise ValueError('normal whole draw watcher required')
    types=set(r['type_line'].partition(' — ')[0].split())
    if not types&{'Creature','Enchantment','Artifact'}:raise ValueError('permanent draw watcher required')
    lines=r['oracle_text'].splitlines();triggers=[s for s in lines if s.startswith('Whenever ')]
    if len(triggers)!=1:raise ValueError('one complete draw-event trigger required')
    m=re.fullmatch(r'Whenever (you draw|an opponent draws|a player draws) a card, (.+)',triggers[0])
    if not m:raise ValueError('unreviewed draw-event grammar')
    scope={'you draw':'own','an opponent draws':'opponent','a player draws':'any'}[m[1]];text=m[2];condition=None
    conditional=re.fullmatch(r'if you control a (black|blue|green|red|white) permanent, (.+)',text)
    if conditional:condition={'controlled_color':COLORS[conditional[1]]};text=conditional[2]
    paid=re.fullmatch(r'you may pay ((?:\{[0-9WUBRG]+\})+)\. If you do, (.+)',text)
    trigger_cost=paid[1] if paid else None
    if paid:text=paid[2]
    effect=recipient=target_filter=None;amount=None;optional=bool(paid)
    m=re.fullmatch(r'you gain ([1-9]) life\.',text)
    if m:effect='gain';recipient='source';amount=int(m[1])
    m=re.fullmatch(r'put (a|one|two|three) \+1/\+1 counters? on this creature\.',text)
    if m:effect='counters';recipient='source';amount=WORDS[m[1]]
    m=re.fullmatch(r'(you may )?draw (a|two|three) cards?\.',text)
    if m:effect='draw';recipient='source';amount=WORDS[m[2]];optional=optional or bool(m[1])
    m=re.fullmatch(r'(you may have )?target player mill (a|two|three) cards?\.',text)
    if m:effect='mill';recipient='target';target_filter='player';amount=WORDS[m[2]];optional=bool(m[1])
    m=re.fullmatch(r'each opponent mills (a|one|two|three) cards?\.',text)
    if m:effect='mill';recipient='opponents';amount=WORDS[m[1]]
    damage=re.fullmatch(r'(you may have )?(this creature|this enchantment) (deals|deal) ([1-3]) damage to that player\.',text)
    if damage:
        if bool(damage[1])!=(damage[3]=='deal'):raise ValueError('complete optional damage wording required')
        effect='damage';recipient='drawer';amount=int(damage[4]);optional=bool(damage[1])
    if effect is None:raise ValueError('unreviewed draw-event reward')
    if effect=='draw' and scope!='opponent':raise ValueError('unbounded recursive own-draw reward requires separate model')
    if paid and (effect!='draw' or amount!=1 or trigger_cost!='{1}'):raise ValueError('reviewed per-draw optional payment required')
    creature='Creature' in types;remainder='\n'.join(s for s in lines if s!=triggers[0]);operative={'Mill'} if effect=='mill' else set()
    if creature:
        keywords=oracle_keywords(dict(r,oracle_text=remainder,keywords=[k for k in r['keywords'] if k not in operative]));power=int(r['power']);toughness=int(r['toughness'])
    else:
        if remainder or set(r['keywords'])-operative:raise ValueError('additional draw watcher clauses require another component')
        keywords=[];power=toughness=None
    if effect=='counters' and not creature:raise ValueError('self counters require creature source')
    return dict(scope=scope,effect=effect,recipient=recipient,amount=amount,optional=optional,condition=condition,target_filter=target_filter,
                trigger_cost=trigger_cost,trigger_mana=cost_from_oracle(dict(mana_cost=trigger_cost)) if paid else [0]*6,
                mana=cost_from_oracle(r),keywords=keywords,power=power,toughness=toughness,legendary='Legendary' in types)

def parameters(p):return {k:copy.deepcopy(p[k]) for k in ('scope','effect','recipient','amount','optional','condition','target_filter','trigger_cost')}

def variants(p):
    modes=['effect','alternate_spell','other_drawer','two_operations','repeat_same_turn','activation_draw','not_drawn_to_hand',
           'cast_without_draw','countered_witness','witness_payment','witness_insufficient_mana','witness_wrong_color',
           'source_cast','source_payment','source_insufficient_mana','source_removed_before','source_removed_pending','source_arrives_late',
           'source_removed_after_draw','cleanup','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot']
    if any(p['mana'][1:]):modes.append('source_wrong_color')
    if not p['legendary']:modes.append('two_sources')
    if p['effect']=='mill':modes+=['short_mill_library','empty_mill_library']
    if p['recipient']=='target':modes+=['target_self','interleaved_self_two']
    if p['condition']:modes+=['no_red_permanent','enemy_red_only','red_removed_before_draw']
    if p['trigger_cost']:modes+=['trigger_payment','trigger_unaffordable']
    return [f'{m}_{o}' for o in (0,1) for m in modes]+(['decline_0'] if p['optional'] else [])+(['payment_decline_0'] if p['trigger_cost'] else [])

def spec(d,r):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r.get('oracle_text'):
        raise ValueError('exact draw family/version/full Oracle required')
    if not exact(pins()['sources'].get(r['name']),pin(r)):raise ValueError('draw source identity changed')
    p=classify(r)
    if not exact(d.get('params'),parameters(p)):raise ValueError('draw parameters must derive from complete Oracle')
    selected=d.get('variants')
    if not isinstance(selected,list) or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(p)):raise ValueError('distinct supported draw variants required')
    return dict(p,family=FAMILY)

def validate_witnesses(o,rows):
    from creature_recipes import oracle_keywords
    data=pins()
    for name,expected in dict(data['witnesses'],**data['sources']).items():
        if name not in rows or not exact(pin(o.get(name,{})),expected) or rows[name]['handler'] in ('NULL','0'):
            raise ValueError('draw source/witness identity changed: '+name)
        if rows[name]['handler']=='card_dummy':
            if name in data['sources']:raise ValueError('unimplemented draw source')
            oracle_keywords(o[name])
    for name,ui in data['presentations'].items():
        if name not in data['sources'] or rows[name]['handler']!=ui['handler'] or ui['human_acceptance']!='optional_trigger':
            raise ValueError('draw trigger presentation identity changed')
        if not classify(o[name])['optional']:raise ValueError('optional draw trigger presentation required')


class DrawLedger:
    def __init__(self,name,csvid,d,p,variant,rows,o):
        from recipes import card
        self.name=name;self.csvid=csvid;self.d=d;self.p=p;self.variant=variant;self.mode,owner=variant.rsplit('_',1);self.owner=int(owner);self.rows=rows;self.o=o
        self.drawer=1-self.owner if p['scope']=='opponent' else self.owner
        if self.mode=='other_drawer':self.drawer=1-self.drawer
        self.setup=[];self.entities={};self.zones={z:[[],[]] for z in ('hand','graveyard','exile','library')};self.mana=[[30]*6,[30]*6];self.life=[20,20];self.actions=[];self.pending_cut=None
        self.target=self.owner if self.mode in ('target_self','interleaved_self_two') else 1-self.owner
        for who in (0,1):
            for z,base in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY)):
                ns=list(LIBRARY)*4 if z=='library' else [] if self.mode=='bare_zones' else list(base)
                if z=='library' and who==self.target and self.mode in ('short_mill_library','empty_mill_library'):ns=['Forest'] if self.mode=='short_mill_library' else []
                if z=='library' and who==self.owner and self.mode=='interleaved_self_two':ns=['Forest','Plains','Giant Growth','Lightning Bolt','Island']
                self.zones[z][who]=ns;self.setup += [card(n,f'zone_{who}_{z}_{i}',who,z) for i,n in enumerate(ns)]
        self.source_hand=self.mode in ('source_cast','source_payment','source_insufficient_mana','source_wrong_color','source_arrives_late')
        self.denied_source=self.mode in ('source_insufficient_mana','source_wrong_color')
        self.add(name,'source',self.owner,'hand' if self.source_hand else 'battlefield');self.add('Grizzly Bears','idle',self.owner);self.add('Grizzly Bears','enemy',1-self.owner);self.add('Sol Ring','mana_control',self.owner,tapped=True)
        if self.mode=='two_sources':self.add(name,'copy',self.owner)
        if p['condition'] and self.mode!='no_red_permanent':self.add('Seal of Fire','red_control',1-self.owner if self.mode=='enemy_red_only' else self.owner)
        if self.mode=='payment_decline':self.add('Forest','payment_land',self.owner)
        self.witness='Divination' if self.mode in ('alternate_spell','interleaved_self_two') or self.mode=='trigger_payment' else 'Reach Through Mists'
        if self.mode=='not_drawn_to_hand':self.witness='Raise Dead'
        if self.mode=='cast_without_draw':self.witness='Sol Ring'
        self.operations=3 if self.mode=='repeat_same_turn' else 2 if self.mode=='two_operations' else 1
        if self.mode=='activation_draw':self.add('Jayemdae Tome','tome',self.drawer)
        else:
            for i in range(self.operations):self.add(self.witness,f'spell_{i}',self.drawer,'hand')
        if self.mode=='countered_witness':self.add('Counterspell','response',1-self.drawer,'hand')
        if self.mode=='witness_payment':self.mana[self.drawer]=[4,0,0,0,0,0] if self.mode=='activation_draw' else self.cost(self.witness)
        if self.mode=='witness_insufficient_mana':self.mana[self.drawer]=[0]*6
        if self.mode=='witness_wrong_color':self.mana[self.drawer]=[sum(self.cost(self.witness))]+[0]*5
        if self.mode=='source_payment':self.mana[self.owner]=list(p['mana'])
        if self.denied_source:self.mana[self.owner]=[sum(p['mana'])]+[0]*5 if self.mode=='source_wrong_color' else [0]*6
        if self.mode=='trigger_payment':self.mana[self.owner]=[2,0,0,0,0,0];self.mana[self.drawer]=self.cost(self.witness)
        if self.mode in ('trigger_unaffordable','payment_decline'):self.mana[self.owner]=[0]*6
        from recipes import step
        self.pre=[step('turn',player=self.owner if self.source_hand else self.drawer,value=0)]+[step('set_mana',player=who,color=c,value=v) for who in (0,1) for c,v in enumerate(self.mana[who])]
        self.universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in self.entities.values()}

    def step(self,op,**kw):
        from recipes import step
        self.actions.append(step(op,**kw))
    def add(self,name,ref,who,zone='battlefield',tapped=False):
        from recipes import card
        r=self.o[name];creature='Creature' in r['type_line'].partition(' — ')[0].split();obj=card(name,ref,who,zone)
        if tapped:obj['state']=['tapped']
        self.setup.append(obj)
        if zone=='hand':self.zones['hand'][who].append(name)
        self.entities[ref]=dict(name=name,player=who,alive=zone=='battlefield',hand=zone=='hand',creature=creature,tapped=tapped,
                                power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,counters=0)
    def cost(self,name):
        from recipes import cost_from_oracle
        r=self.o[name]
        if r['layout']!='normal' or 'Token' in r['type_line']:raise ValueError('normal printed draw-action witness required')
        return [0]*6 if r['mana_cost']=='{0}' else cost_from_oracle(r)
    def spend(self,who,cost):
        self.mana[who]=[a-b for a,b in zip(self.mana[who],cost)]
        if min(self.mana[who])<0:raise ValueError('draw fixture payment underfunded')
    def die(self,ref):
        e=self.entities[ref];self.step('destroy',ref=ref);e['alive']=False;self.zones['graveyard'][e['player']].append(e['name'])
    def condition(self):
        if not self.p['condition']:return True
        color=self.p['condition']['controlled_color']
        return any(e['alive'] and e['player']==self.owner and color in self.o[e['name']]['colors'] for e in self.entities.values())

    def draw(self,drawer,n,depth=0):
        if depth>4:raise ValueError('unreviewed recursive draw source composition')
        for _ in range(n):
            if not self.zones['library'][drawer]:raise ValueError('successful draw fixture must be stocked; game-loss modeling is separate')
            card=self.zones['library'][drawer].pop(0);self.zones['hand'][drawer].append(card)
            if self.p['scope']=='own' and drawer!=self.owner or self.p['scope']=='opponent' and drawer==self.owner:continue
            if not self.condition():continue
            for ref in ('source','copy'):
                if ref not in self.entities or not self.entities[ref]['alive']:continue
                if self.p['optional'] and self.owner==0:
                    if self.mode=='decline':continue
                    self.step('choose_trigger_source',player=self.owner,ref=ref)
                if self.p['trigger_cost']:
                    if self.mode=='payment_decline':self.step('choose_cancel_mana',player=self.owner);continue
                    if self.mana[self.owner][0]<1:
                        if self.mode=='trigger_unaffordable' and self.owner==0:self.step('choose_cancel_mana',player=self.owner)
                        continue
                    self.spend(self.owner,self.p['trigger_mana'])
                effect=self.p['effect'];amount=self.p['amount']
                if effect=='gain':self.life[self.owner]+=amount
                elif effect=='damage':self.life[drawer]-=amount
                elif effect=='counters':
                    e=self.entities[ref];e['counters']+=amount;e['power']+=amount;e['toughness']+=amount
                elif effect=='draw':self.draw(self.owner,amount,depth+1)
                else:
                    who=self.target if self.p['recipient']=='target' else 1-self.owner
                    if self.p['recipient']=='target':self.step('choose_player',player=self.owner,value=who)
                    take=min(amount,len(self.zones['library'][who]));self.zones['graveyard'][who]+=self.zones['library'][who][:take];del self.zones['library'][who][:take]

    def checks(self,pending=False):
        from recipes import step,observe
        out=[step('assert_stack',value=1 if pending else 0)]
        if not pending:out.append(step('assert_choices_consumed'))
        for ref,e in self.entities.items():
            if e['alive']:
                out += [observe(ref,'in_play',1),observe(ref,'in_hand',0),observe(ref,'tapped',int(e['tapped']))]
                if e['creature']:out += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness']),observe(ref,'plus1_counters',e['counters']),observe(ref,'damage',0)]
            elif e['hand'] and not pending:out.append(observe(ref,'in_hand',1))
        for who in (0,1):
            out += [step('assert_life',player=who,value=self.life[who]),step('assert_mana_total',player=who,value=sum(self.mana[who]))]
            if not pending:out.append(step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in self.entities.values())))
            for z in ('library',) if pending else self.zones:
                ns=self.zones[z][who];out.append(step('assert_zone',player=who,zone=z,value=len(ns)))
                out += [step('assert_zone_multiplicity',player=who,zone=z,index=self.rows[n]['csvid'],value=ns.count(n)) for n in sorted(self.universe)]
                if z=='library':out += [step('assert_zone_card',player=who,zone=z,index=i,value=self.rows[n]['csvid']) for i,n in enumerate(ns)]
            if self.mode in ('source_payment','witness_payment','trigger_payment','trigger_unaffordable','payment_decline','witness_insufficient_mana','witness_wrong_color','source_insufficient_mana','source_wrong_color'):
                out += [step('assert_mana',player=who,color=c,value=v) for c,v in enumerate(self.mana[who])]
        return out

    def cast_source(self):
        self.step('turn',player=self.owner,value=0);self.step('cast',ref='source',value=int(not self.denied_source))
        if self.denied_source:self.step('assert_stack',value=0);self.step('assert_choices_consumed');return
        self.spend(self.owner,self.p['mana']);self.entities['source']['hand']=False;self.zones['hand'][self.owner].remove(self.name)
        self.step('assert_stack',value=1);self.step('announce_spell',ref='source');self.step('resolve');self.step('assert_choices_consumed');self.entities['source']['alive']=True
        if self.mode=='source_payment':
            self.actions+=self.checks();self.mana[self.owner]=[30]*6
            for c,v in enumerate(self.mana[self.owner]):self.step('set_mana',player=self.owner,color=c,value=v)

    def operation(self,i):
        from recipes import step
        activation=self.mode=='activation_draw';no=self.mode in ('witness_insufficient_mana','witness_wrong_color')
        self.step('turn',player=self.drawer,value=0)
        if activation:
            self.step('require_can_activate',ref='tome',value=1);self.step('activate',ref='tome',value=1);self.spend(self.drawer,[4,0,0,0,0,0]);self.entities['tome']['tapped']=True
        else:
            ref=f'spell_{i}';e=self.entities[ref]
            if self.mode=='not_drawn_to_hand':self.step('choose_zone_card',player=self.drawer,value=self.rows['Grizzly Bears']['csvid'])
            self.step('cast',ref=ref,value=int(not no))
            if no:self.step('assert_stack',value=0);self.step('assert_choices_consumed');return
            self.spend(self.drawer,self.cost(self.witness));e['hand']=False;self.zones['hand'][self.drawer].remove(self.witness)
            self.step('assert_card',ref=ref,property='in_hand',value=0);self.step('announce_spell',ref=ref)
        self.actions+=self.checks(pending=True)
        if self.pending_cut is None:self.pending_cut=len(self.actions)
        if self.mode=='source_removed_pending':self.die('source')
        if self.mode=='countered_witness':
            who=1-self.drawer;self.step('cast_response',ref='response',value=1);self.spend(who,self.cost('Counterspell'));self.zones['hand'][who].remove('Counterspell');self.entities['response']['hand']=False
            self.step('assert_stack',value=2);self.step('resolve_response');self.step('assert_choices_consumed');self.zones['graveyard'][who].append('Counterspell');self.zones['graveyard'][self.drawer].append(self.witness)
        else:
            if self.mode=='not_drawn_to_hand':
                self.zones['graveyard'][self.drawer].remove('Grizzly Bears');self.zones['hand'][self.drawer].append('Grizzly Bears')
            elif self.mode!='cast_without_draw':self.draw(self.drawer,2 if self.witness=='Divination' else 1)
            self.step('resolve');self.step('assert_unused_choices',value=0);self.step('assert_choices_consumed')
            if not activation:
                if self.mode=='cast_without_draw':self.entities[ref]['alive']=True
                else:self.zones['graveyard'][self.drawer].append(self.witness)
        self.actions+=self.checks()

    def case(self):
        from recipes import step
        if self.source_hand and self.mode!='source_arrives_late':self.cast_source()
        if self.mode=='source_removed_before':self.die('source')
        if self.mode=='red_removed_before_draw':self.die('red_control')
        if not self.denied_source:
            for i in range(self.operations):self.operation(i)
            if self.mode=='source_arrives_late':self.cast_source()
        if self.mode=='source_removed_after_draw':self.die('source')
        if self.mode=='cleanup':
            for who in (0,1):self.step('assert_mana_total',player=who,value=sum(self.mana[who]))
            for who in (0,1):
                for c in range(6):self.step('set_mana',player=who,color=c,value=0)
            self.mana=[[0]*6,[0]*6];self.step('cleanup')
        checks=self.checks();seq=self.pre+self.actions+checks
        if self.mode in ('ready_snapshot','pending_snapshot'):
            cut=0 if self.mode=='ready_snapshot' else self.pending_cut
            seq=self.pre+self.actions[:cut]+[step('capture')]+self.actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+self.actions[cut:]+checks+[step('assert_recorded_state')]
        if self.mode=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        return dict(name=f'DrawValueTriggerRecipe_{self.csvid}_{self.variant}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=seq,
                    _recipe=dict(family=FAMILY,version=2,variant=self.variant,params=self.d['params'],oracle_text=self.d['oracle_text'],untested_components=[
                        'Ordinary keyword combat/flash timing beyond normal source casting, replacements, multiplayer and opposing optional-draw recursion.',
                        'No separately pending window between one actual draw and its synchronously resolving draw trigger; no synthetic response timing.',
                        'Successful draw libraries are stocked; failed-draw game outcomes need a separate observer. Empty/short mill destinations are exercised.',
                        'Cleanup does not model phase-boundary mana expiry; paid pools precede explicit zeroing.']))

def expand(name,csvid,d,p,selected,rows,o):
    validate_witnesses(o,rows)
    if len(set(selected))!=len(selected) or set(selected)-set(variants(p)):raise ValueError('unsupported draw variants')
    return [DrawLedger(name,csvid,d,p,v,rows,o).case() for v in selected]
