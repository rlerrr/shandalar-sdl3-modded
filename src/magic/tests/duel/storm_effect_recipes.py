"""Complete Storm spell effects through real casts and production copy callbacks."""
import copy,json,re
from pathlib import Path
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from cast_untap_trigger_recipes import pin,exact

FAMILY='storm_effect'
DATA=Path(__file__).parent/'data/storm_effect_wave8_profiles.json'
REMINDERS=('Storm (When you cast this spell, copy it for each spell cast before it this turn.)','Storm (When you cast this spell, copy it for each spell cast before it this turn. You may choose new targets for the copies.)')
WORDS={'one':1,'two':2,'three':3,'four':4}
COLORS={'black':1,'blue':2,'green':3,'red':4,'white':5}
GRAVE_TARGETS=('Grizzly Bears','Wind Drake','Scathe Zombies','Hill Giant','Giant Spider','Goblin Piker')

def pins():return json.loads(DATA.read_text(encoding='utf-8'))
def classify(r):
    from recipes import cost_from_oracle
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r.get('type_line') not in ('Instant','Sorcery'):raise ValueError('normal whole Storm instant/sorcery required')
    lines=r.get('oracle_text','').splitlines()
    if len(lines)!=2 or lines[1] not in REMINDERS or r.get('keywords',[]).count('Storm')!=1:raise ValueError('complete primary effect plus exact Storm clause required')
    s=lines[0];p=dict(kind=None,amount=None,scope=None,destination=None,token=None)
    m=re.fullmatch(r'Target player mills (one|two|three|four) cards?\.',s)
    if m:p.update(kind='mill',amount=WORDS[m[1]],scope='player',destination='graveyard')
    m=re.fullmatch(r'Search your library for a (Dragon permanent|basic land) card, (put it onto the battlefield|reveal that card, put it into your hand), then shuffle\.',s)
    if m:
        if (m[1]=='Dragon permanent')!=(m[2]=='put it onto the battlefield'):raise ValueError('uncalibrated tutor destination')
        p.update(kind='search',amount=1,scope='dragon_permanent' if m[1]=='Dragon permanent' else 'basic_land',destination='battlefield' if m[2]=='put it onto the battlefield' else 'hand')
    m=re.fullmatch(r'Counter target (spell|instant or sorcery spell) unless its controller pays \{([1-9])\}\.',s)
    if m:p.update(kind='tax',amount=int(m[2]),scope='any_spell' if m[1]=='spell' else 'instant_or_sorcery',destination='graveyard')
    if s=="Target creature without flying can't block this turn.":p.update(kind='cannot_block',amount=1,scope='nonflying_creature',destination='battlefield')
    if s=='Return target creature card from your graveyard to your hand.':p.update(kind='return',amount=1,scope='own_creature_card',destination='hand')
    if s=='Destroy target land.':p.update(kind='destroy',amount=1,scope='land',destination='graveyard')
    m=re.fullmatch(r'Create a ([1-9])/([1-9]) (white|blue|black|red|green) ([A-Za-z]+) creature token with flying named (.+)\.',s)
    if m:p.update(kind='token',amount=1,scope='controller',destination='battlefield',token=dict(name=m[5],power=int(m[1]),toughness=int(m[2]),colors=[m[3]],subtypes=[m[4]],keywords=['Flying'],artifact=False))
    if p['kind'] is None:raise ValueError('unreviewed complete Storm primary effect')
    expected={'Storm','Mill'} if p['kind']=='mill' else {'Storm'}
    if set(r['keywords'])!=expected:raise ValueError('operative Storm keywords disagree')
    if (p['kind'] in ('mill','tax','cannot_block','return','destroy'))!=(lines[1]==REMINDERS[1]):raise ValueError('copied-target reminder must match targeted primary')
    return dict(p,mana=cost_from_oracle(r))
def parameters(p):return {k:copy.deepcopy(p[k]) for k in ('kind','amount','scope','destination','token')}
def variants(p):
    modes=['effect','prefix_two','prefix_three','opponent_history','split_history','instant_history','countered_predecessor','previous_turn','payment','insufficient_mana','wrong_color','bare_zones','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot']
    if p['kind']!='tax':modes+=['no_prior','activation_not_cast','countered_original']
    if p['kind']=='mill':modes+=['self_target','split_targets','short_library','empty_target_library']
    if p['kind']=='search':modes+=['no_match','empty_library','duplicates','fail_to_find']
    if p['kind']=='return':modes+=['empty_graveyard','wrong_graveyard','wrong_type']
    if p['kind']=='destroy':modes+=['own_targets','no_land']
    if p['kind']=='cannot_block':modes+=['only_flyers','expiry']
    if p['kind']=='token':modes+=['token_flying_block','token_fresh_attack','token_mana_value_zero']
    if p['kind']=='tax':
        modes+=['tax_paid','tax_partial','tax_exact','tax_zero_budget']
        if p['scope']=='instant_or_sorcery':modes.append('forbidden_parent')
        modes.append('no_parent')
    return [f'{m}_{w}' for w in (0,1) for m in modes]
def spec(d,r):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r.get('oracle_text'):raise ValueError('exact Storm family/version/full Oracle required')
    if not exact(pins()['sources'].get(r['name']),pin(r)):raise ValueError('Storm source identity changed')
    p=classify(r)
    if not exact(d.get('params'),parameters(p)):raise ValueError('Storm parameters must derive from complete Oracle')
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or len(set(vs))!=len(vs) or set(vs)-set(variants(p)):raise ValueError('unique supported Storm variants required')
    return dict(p,family=FAMILY)
def eligible(r,p):
    head,_,tail=r['type_line'].partition(' — ');types=set(head.split())
    return {'Basic','Land'}<=types if p['scope']=='basic_land' else bool(types&{'Artifact','Creature','Enchantment','Land','Planeswalker','Battle'}) and ('Dragon' in tail.split() or 'Changeling' in r.get('keywords',[]))
def validate_witnesses(o,rows):
    from creature_recipes import oracle_keywords
    data=pins();token_names={classify(r)['token']['name'] for r in data['sources'].values() if classify(r)['kind']=='token'}
    for name,players in data.get('tax_dialog_players',{}).items():
        if name not in data['sources'] or classify(data['sources'][name])['kind']!='tax' or not isinstance(players,list) or not players or any(type(w) is not int or w not in (0,1) for w in players) or len(players)!=len(set(players)):
            raise ValueError('invalid source-pinned scripted tax payer presentation')
    for n,expected in dict(data['witnesses'],**data['sources']).items():
        if n not in rows or rows[n]['handler'] in ('NULL','0') or not exact(pin(o.get(n,{})),expected):raise ValueError('Storm source/witness identity changed: '+n)
        if n in data['sources'] and rows[n]['kind']!='card':raise ValueError('Storm source must remain a primary registry card')
        if rows[n]['handler']=='card_dummy':
            if n in data['sources']:raise ValueError('unimplemented Storm source')
            r=o[n]
            if r.get('layout')=='token' and n in token_names:
                if not r['type_line'].startswith('Token Creature'):raise ValueError('whole named creature token witness required')
                oracle_keywords(dict(r,layout='normal',type_line=r['type_line'].removeprefix('Token ')))
            else:oracle_keywords(r)
    if o['Contemplation']['oracle_text']!='Whenever you cast a spell, you gain 1 life.':raise ValueError('real cast-only watcher changed')
    for n,handler in data['handlers'].items():
        if rows[n]['handler']!=handler:raise ValueError('Storm callback presentation changed: '+n)

class StormLedger:
    def __init__(self,name,csvid,d,p,v,rows,o):
        from recipes import card,step
        self.name=name;self.csvid=csvid;self.d=d;self.p=p;self.mode,owner=v.rsplit('_',1);self.owner=int(owner);self.variant=v;self.rows=rows;self.o=o
        self.kind=p['kind'];self.active=1-self.owner if self.kind=='tax' else self.owner
        self.tax_dialog_players=pins().get('tax_dialog_players',{}).get(name,[])
        self.prefix_count=0 if self.mode in ('no_prior','activation_not_cast') else 2 if self.mode in ('prefix_two','opponent_history','split_history') else 3 if self.mode=='prefix_three' else 1
        self.history=0;self.e={};self.setup=[];self.actions=[];self.z={z:[[],[]] for z in ('hand','graveyard','exile','library')};self.mana=[[30]*6,[30]*6];self.life=[20,20];self.shuffles=[0,0];self.shuffled=[False,False];self.pending=None;self.token_count=0;self.effect_count=0;self.binds=[];self.inspected=None
        self.target_player=self.owner if self.mode=='self_target' else 1-self.owner
        self.denied=self.mode in ('insufficient_mana','wrong_color','empty_graveyard','wrong_graveyard','wrong_type','no_land','only_flyers','forbidden_parent','no_parent')
        for who in (0,1):
            for z,base in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY*7)):
                ns=[] if self.mode=='bare_zones' and z!='library' else list(base)
                if z=='graveyard' and self.kind=='return':
                    if who==self.owner:ns=list(GRAVE_TARGETS)+['Forest','Giant Growth']
                    if who==self.owner and self.mode in ('empty_graveyard','wrong_graveyard','wrong_type'):ns=[] if self.mode!='wrong_type' else ['Forest','Giant Growth']
                    if who!=self.owner and self.mode=='wrong_graveyard':ns=list(GRAVE_TARGETS)
                if z=='library' and self.kind=='search' and who==self.owner:
                    ns=['Giant Growth']+(['Volcanic Dragon']*8+['Crib Swap','Forest','Sol Ring','Grizzly Bears'] if p['scope']=='dragon_permanent' else ['Forest']*4+['Island']*4+['Swamp','Plains','Mountain','Sol Ring','Grizzly Bears'])
                    if self.mode=='no_match':ns=['Sol Ring','Grizzly Bears','Giant Growth']
                    if self.mode=='empty_library':ns=[]
                    if self.mode=='duplicates':ns=['Giant Growth']+(['Volcanic Dragon']*2 if p['scope']=='dragon_permanent' else ['Forest']*2)+['Sol Ring']
                if z=='library' and self.kind=='mill' and who==self.target_player and self.mode in ('short_library','empty_target_library'):ns=list(LIBRARY[:2]) if self.mode=='short_library' else []
                self.z[z][who]=ns;self.setup += [card(n,f'zone_{who}_{z}_{i}',who,z) for i,n in enumerate(ns)]
        self.add(name,'source',self.owner,'hand');self.add('Contemplation','watcher',self.owner)
        for i in range(self.prefix_count):
            who=1-self.owner if self.mode=='opponent_history' or self.mode=='split_history' and i%2 else self.owner
            n='Reach Through Mists' if self.kind=='tax' or who!=self.active or self.mode in ('instant_history','countered_predecessor') else 'Sol Ring'
            self.add(n,f'prefix_{i}',who,'hand')
        if self.mode=='countered_predecessor':self.add('Counterspell','prefix_counter',1-self.e['prefix_0']['player'],'hand')
        if self.mode=='countered_original':self.add('Counterspell','source_counter',1-self.owner,'hand')
        if self.mode=='activation_not_cast':self.add('Sol Ring','mana_control',self.owner)
        if self.kind in ('destroy','cannot_block'):
            count=5
            for i in range(count):
                who=self.owner if self.mode=='own_targets' else 1-self.owner
                n='Suntail Hawk' if self.mode=='only_flyers' else 'Grizzly Bears' if self.kind=='cannot_block' else 'Sol Ring' if self.mode=='no_land' else 'Forest'
                self.add(n,f'target_{i}',who)
            if self.kind=='cannot_block' and self.mode!='only_flyers':self.add('Grizzly Bears','attack_probe',self.owner)
        if self.kind=='tax' and self.mode!='no_parent':self.add('Grizzly Bears' if self.mode=='forbidden_parent' else 'Reach Through Mists','parent',1-self.owner,'hand')
        if self.mode=='token_flying_block':self.add('Suntail Hawk','flying_attacker',1-self.owner)
        if self.mode=='token_mana_value_zero':self.add('Gaze of Granite','zero_sweep',self.owner,'hand')
        costs=[[0]*6,[0]*6]
        for ref,e in self.e.items():
            if e['hand'] and ref!='source':costs[e['player']]=[a+b for a,b in zip(costs[e['player']],self.cost(e['name']))]
        if self.mode in ('payment','insufficient_mana','wrong_color'):
            source_cost=list(p['mana']) if self.mode=='payment' else [sum(p['mana']),0,0,0,0,0] if self.mode=='wrong_color' else [0]*6
            self.mana[self.owner]=[a+b for a,b in zip(costs[self.owner],source_cost)]
        if self.kind=='tax':
            other=1-self.owner;budget=(self.prefix_count+2)*p['amount'] if self.mode in ('tax_paid','tax_exact') else p['amount'] if self.mode=='tax_partial' else 0
            self.mana[other]=list(costs[other]);self.mana[other][0]+=budget
        self.pre=[step('turn',player=self.active,value=0)]+[step('set_mana',player=w,color=c,value=n) for w in (0,1) for c,n in enumerate(self.mana[w])]
        if self.kind=='search':self.pre.append(step('observe_search_filters',value=1))
        self.universe={n for z in self.z.values() for ns in z for n in ns}|{e['name'] for e in self.e.values()}
        if self.kind=='token':self.universe.add(p['token']['name'])
    def step(self,op,**kw):
        from recipes import step
        self.actions.append(step(op,**kw))
    def add(self,n,ref,w,zone='battlefield'):
        from recipes import card
        self.setup.append(card(n,ref,w,zone));self.entity(n,ref,w,zone)
        if zone=='hand':self.z['hand'][w].append(n)
    def entity(self,n,ref,w,zone='battlefield',token=False):
        r=self.o[n];creature='Creature' in r['type_line'].partition(' — ')[0].split()
        self.e[ref]=dict(name=n,player=w,alive=zone=='battlefield',hand=zone=='hand',pending=False,tapped=False,creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,damage=0,token=token,cannot_block=False)
    def cost(self,n):
        from recipes import cost_from_oracle
        r=self.o[n];cost=r['mana_cost']
        if n=='Gaze of Granite':cost=cost.replace('{X}','')
        return cost_from_oracle(dict(r,mana_cost=cost))
    def pay(self,w,cost):
        self.mana[w]=[a-b for a,b in zip(self.mana[w],cost)]
        if min(self.mana[w])<0:raise ValueError('real Storm fixture payment underfunded')
    def cast(self,ref,response=False,denied=False):
        e=self.e[ref];self.step('cast_response' if response else 'cast',ref=ref,value=int(not denied))
        if denied:return
        self.pay(e['player'],self.cost(e['name']));self.z['hand'][e['player']].remove(e['name']);e['hand']=False;e['pending']=True
        self.step('assert_unused_choices',value=0);self.step('require_card',ref=ref,property='in_hand',value=0)
    def announced(self,ref):
        e=self.e[ref];self.history+=1
        if e['player']==self.owner:self.life[self.owner]+=1
    def finish_plain(self,ref,countered=False):
        e=self.e[ref];e['pending']=False;n=e['name'];w=e['player'];types=set(self.o[n]['type_line'].partition(' — ')[0].split())
        if countered or not types&{'Creature','Artifact','Enchantment','Land','Planeswalker'}:self.z['graveyard'][w].append(n)
        else:e['alive']=True
        if not countered and n=='Reach Through Mists':
            if not self.z['library'][w]:raise ValueError('prefix draw library underfunded')
            self.z['hand'][w].append(self.z['library'][w].pop(0))
    def kill(self,ref,explicit=False):
        if explicit:self.step('destroy',ref=ref)
        e=self.e[ref];e['alive']=False;e['pending']=False
        if not e['token']:self.z['graveyard'][e['player']].append(e['name'])
    def prefixes(self):
        for i in range(self.prefix_count):
            ref=f'prefix_{i}';self.cast(ref);self.step('assert_stack',value=1);self.step('announce_spell',ref=ref);self.announced(ref)
            if self.mode=='countered_predecessor':
                self.cast('prefix_counter',response=True);self.step('resolve_response');self.announced('prefix_counter');self.finish_plain('prefix_counter');self.finish_plain(ref,True)
            else:self.step('resolve');self.finish_plain(ref)
            self.step('assert_choices_consumed')
        if self.mode=='previous_turn':
            self.zero();self.step('advance_turn');self.step('advance_turn');self.history=0;self.actions+=self.checks();self.mana=[[30]*6,[30]*6]
            if self.kind=='tax':self.mana[1-self.owner]=self.cost(self.e['parent']['name'])
            for who in (0,1):
                for c,n in enumerate(self.mana[who]):self.step('set_mana',player=who,color=c,value=n)
        if self.mode=='activation_not_cast':
            self.step('require_can_activate',ref='mana_control',value=1);self.step('activate',ref='mana_control',value=1);self.mana[self.owner][0]+=2;self.e['mana_control']['tapped']=True;self.step('assert_stack',value=0)
    def target_choice(self,index):
        if self.kind=='mill':self.step('choose_player',player=self.owner,value=self.owner if self.mode=='split_targets' and index%2 else self.target_player);return True
        if self.kind in ('destroy','cannot_block'):self.step('choose_target',player=self.owner,ref=f'target_{index}');return True
        if self.kind=='return':self.step('choose_zone_card',player=self.owner,value=self.rows[GRAVE_TARGETS[index]]['csvid']);return True
        return False
    def one_effect(self,index):
        planned=0;w=self.owner
        if self.kind=='mill':
            who=w if self.mode=='split_targets' and index%2 else self.target_player;ns=self.z['library'][who][:self.p['amount']];del self.z['library'][who][:len(ns)];self.z['graveyard'][who]+=ns
        elif self.kind=='return':
            n=GRAVE_TARGETS[index];self.z['graveyard'][w].remove(n);self.z['hand'][w].append(n)
        elif self.kind=='destroy':self.kill(f'target_{index}')
        elif self.kind=='cannot_block':self.e[f'target_{index}']['cannot_block']=True
        elif self.kind=='token':
            n=self.p['token']['name'];ref=f'token_{self.token_count}';ordinal=self.token_count;self.entity(n,ref,w,token=True);self.token_count+=1
            self.e[ref]['power']=self.p['token']['power'];self.e[ref]['toughness']=self.p['token']['toughness']
            self.binds.append(dict(op='bind_permanent_copy',player=w,value=self.rows[n]['csvid'],index=ordinal,ref=ref))
        elif self.kind=='search':
            ns=self.z['library'][w];self.inspected=list(ns);valid=[n for n in ns if eligible(self.o[n],self.p)]
            n=valid[0] if valid and self.mode!='fail_to_find' else None
            self.step('choose_zone_card' if n else 'choose_optional_zone',player=w,value=self.rows[n or ('Volcanic Dragon' if self.p['scope']=='dragon_permanent' else 'Forest')]['csvid']);planned+=1
            if n:
                ns.remove(n)
                if self.p['destination']=='hand':self.z['hand'][w].append(n)
                else:
                    ordinal=sum(e['alive'] and e['player']==w and e['name']==n for e in self.e.values());ref=f'fetched_{self.effect_count}';self.entity(n,ref,w)
                    self.binds.append(dict(op='bind_permanent_copy',player=w,value=self.rows[n]['csvid'],index=ordinal,ref=ref))
            self.shuffles[w]+=1;self.shuffled[w]=True
        elif self.kind=='tax' and self.e.get('parent',{}).get('pending'):
            who=1-w;amount=self.p['amount']
            if sum(self.mana[who])>=amount:
                if self.mana[who][0]<amount:raise ValueError('tax budgets require exact generic funds')
                # Scripted do_dialog requests an input even for controller1.
                # The calibrated full-payment response path supplies Pay for
                # each real menu; cost receipts remain independent Oracle1.
                if who==0 or self.mode in ('tax_paid','tax_exact') or who in self.tax_dialog_players:self.step('choose_dialog',player=who,value=0);planned+=1
                self.mana[who][0]-=amount
            else:
                self.e['parent']['pending']=False;self.z['graveyard'][who].append(self.e['parent']['name'])
        self.effect_count+=1;return planned
    def flush_binds(self):self.actions+=self.binds;self.binds=[]
    def checks(self,stack=0):
        from recipes import step,observe
        out=[step('assert_stack',value=stack),step('assert_choices_consumed')];seen_tokens=set()
        for ref,e in self.e.items():
            if e['alive']:
                out += [observe(ref,'in_play',1),observe(ref,'in_hand',0),observe(ref,'tapped',int(e['tapped']))]
                if e['creature']:out += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness']),observe(ref,'damage',e['damage']),observe(ref,'plus1_counters',0)]
                if e['token'] and e['name'] not in seen_tokens:
                    seen_tokens.add(e['name']);props=[('token',1),('color',sum(1<<COLORS[c] for c in self.p['token']['colors'])),('artifact',0)]+[('subtype_'+s,1) for s in self.p['token']['subtypes']]
                    out += [step('assert_matching_permanents',player=e['player'],index=self.rows[e['name']]['csvid'],property=prop,value=value) for prop,value in props]
            elif e['hand']:out.append(observe(ref,'in_hand',1))
        for who in (0,1):
            out += [step('assert_life',player=who,value=self.life[who]),step('assert_mana_total',player=who,value=sum(self.mana[who])),step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in self.e.values()))]
            out += [step('assert_mana',player=who,color=c,value=n) for c,n in enumerate(self.mana[who])]
            out += [step('assert_zone_multiplicity',player=who,zone='battlefield',index=self.rows[n]['csvid'],value=sum(e['alive'] and e['player']==who and e['name']==n for e in self.e.values())) for n in sorted(self.universe)]
            for zone in self.z:
                # This legacy zone observer counts active non-IN_PLAY slots,
                # including a pending original.  in_hand still correctly reads0.
                # Keep logical zones unchanged; adapt only the observer's view.
                ns=list(self.z[zone][who])
                if zone=='hand':ns += [e['name'] for e in self.e.values() if e['pending'] and e['player']==who]
                out.append(step('assert_zone',player=who,zone=zone,value=len(ns)))
                out += [step('assert_zone_multiplicity',player=who,zone=zone,index=self.rows[n]['csvid'],value=ns.count(n)) for n in sorted(self.universe)]
                if zone=='library' and not self.shuffled[who]:out += [step('assert_zone_card',player=who,zone=zone,index=i,value=self.rows[n]['csvid']) for i,n in enumerate(ns)]
            if self.kind=='search' and who==self.owner and self.mode!='empty_library':out.append(step('assert_shuffle_count',player=who,value=self.shuffles[who]))
        return out
    def zero(self):
        self.actions+=self.checks()
        for who in (0,1):
            for c in range(6):self.step('set_mana',player=who,color=c,value=0)
        self.mana=[[0]*6,[0]*6]
    def block_probe(self,expired=False):
        from recipes import observe
        self.step('begin_attackers');self.step('attack',ref='attack_probe',value=1);self.e['attack_probe']['tapped']=True;self.step('finish_attackers');self.step('begin_blockers')
        targets=['target_0'] if expired else [f'target_{i}' for i in range(self.copies+1)]
        legal=[ref for ref in targets if not self.e[ref]['cannot_block']]
        if len(legal)>1:raise ValueError('one-blocker combat probe required')
        for ref in targets:
            allowed=ref in legal;self.step('block',ref=ref,other='attack_probe',value=int(allowed));self.actions.append(observe(ref,'blocking_count',int(allowed)))
        self.step('advance_combat');self.step('combat_damage')
        if legal:self.kill('attack_probe');self.kill(legal[0])
        else:self.life[1-self.owner]-=2
        self.step('end_combat')
    def token_controls(self):
        from recipes import observe
        if self.mode=='token_fresh_attack':
            self.step('begin_attackers');self.step('attack',ref='token_0',value=0);self.actions.append(observe('token_0','attacking',0));self.step('finish_attackers');self.step('begin_blockers');self.step('advance_combat');self.step('combat_damage');self.step('end_combat')
        if self.mode=='token_flying_block':
            self.zero();self.step('advance_turn');self.history=0;self.step('begin_attackers');self.step('attack',ref='flying_attacker',value=1);self.e['flying_attacker']['tapped']=True;self.step('finish_attackers');self.step('begin_blockers');self.step('block',ref='token_0',other='flying_attacker',value=1);self.actions.append(observe('token_0','blocking_count',1));self.step('advance_combat');self.step('combat_damage');self.e['token_0']['damage']=1;self.kill('flying_attacker');self.step('end_combat')
        if self.mode=='token_mana_value_zero':
            self.step('choose_x',player=self.owner,value=0);self.cast('zero_sweep');self.step('announce_spell',ref='zero_sweep');self.announced('zero_sweep');self.step('resolve');self.finish_plain('zero_sweep')
            for ref,e in self.e.items():
                if e['alive'] and e['token']:self.kill(ref)
    def case(self):
        from recipes import step
        self.prefixes()
        if self.kind=='tax' and self.mode!='no_parent':
            self.cast('parent');self.step('announce_spell',ref='parent');self.announced('parent');self.step('assert_stack',value=1)
        self.copies=self.history;self.step('assert_mana_total',player=self.owner,value=sum(self.mana[self.owner]))
        if not self.denied and self.kind!='tax':self.step('require_can_cast',ref='source',value=1)
        if not self.denied:self.target_choice(0)
        self.cast('source',response=self.kind=='tax' and self.mode!='no_parent',denied=self.denied)
        if self.denied:
            if self.kind=='tax' and self.mode=='forbidden_parent':self.step('resolve');self.finish_plain('parent')
            elif self.kind=='tax' and 'parent' in self.e:self.step('resolve');self.finish_plain('parent')
        else:
            self.step('assert_stack',value=2 if self.kind=='tax' else 1)
            # Before announcement neither Storm copies nor the primary effect
            # have resolved.  A source CAST callback must not mint the tokens.
            for who in (0,1):self.step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in self.e.values()))
            planned=0
            for i in range(1,self.copies+1):
                planned+=int(self.target_choice(i));planned+=self.one_effect(i)
            self.step('announce_spell',ref='source');self.announced('source')
            if planned:self.step('assert_unused_choices',value=0)
            self.flush_binds();self.actions+=self.checks(stack=1+int(self.e.get('parent',{}).get('pending',False)))
            self.pending=len(self.actions)
            if self.mode=='countered_original':
                self.cast('source_counter',response=True);self.step('resolve_response');self.announced('source_counter');self.finish_plain('source_counter');self.finish_plain('source',True)
            else:
                planned=self.one_effect(0)
                self.step('resolve_response' if self.kind=='tax' and self.e.get('parent',{}).get('pending') else 'resolve')
                if planned:self.step('assert_unused_choices',value=0)
                self.finish_plain('source');self.flush_binds()
            if self.kind=='tax' and self.e.get('parent',{}).get('pending'):self.finish_plain('parent')
            self.actions+=self.checks()
            if self.kind=='search' and self.inspected:
                self.step('assert_forced_zone_choices',value=0)
                for n in sorted(set(self.inspected)):self.step('assert_offered_legal_card',index=self.rows[n]['csvid'],value=int(eligible(self.o[n],self.p)))
            if self.kind=='cannot_block':
                self.block_probe()
                if self.mode=='expiry':
                    self.zero();self.step('advance_turn');self.step('advance_turn');self.history=0;self.step('untap_step',player=self.owner)
                    for e in self.e.values():
                        if e['alive'] and e['player']==self.owner:e['tapped']=False
                        e['cannot_block']=False
                    # Actual untap leaves its real phase; choose the existing
                    # first-main fixture boundary before declaring combat.
                    self.step('turn',player=self.owner,value=0)
                    self.block_probe(expired=True)
            if self.kind=='token':self.token_controls()
        if self.mode=='cleanup':self.zero();self.step('cleanup')
        checks=self.checks();seq=self.pre+self.actions+checks
        if self.mode in ('ready_snapshot','pending_snapshot'):
            cut=0 if self.mode=='ready_snapshot' else self.pending
            seq=self.pre+self.actions[:cut]+[step('capture')]+self.actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+self.actions[cut:]+checks+[step('assert_recorded_state')]
        if self.mode=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        return dict(name=f'StormEffectRecipe_{self.csvid}_{self.variant}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=seq,
                    _recipe=dict(family=FAMILY,version=2,variant=self.variant,params=self.d['params'],oracle_text=self.d['oracle_text'],prior_real_spells=self.copies,
                    untested_components=['Independent between-copy responses/snapshots and arbitrary non-top copy selection; only actual recursive announcement boundary is claimed.',
                    'Same-invalid-target retention across recursive copy windows, land-play history controls without a dedicated normal-play action, multiplayer and replacements.',
                    'Counter-original while reserving mana for a counter-tax parent is not modeled; other primaries retain actual original-countered/copied-effect receipts.']))
def expand(name,csvid,d,p,selected,rows,o):
    validate_witnesses(o,rows)
    if len(set(selected))!=len(selected) or set(selected)-set(variants(p)):raise ValueError('unsupported Storm variants')
    return [StormLedger(name,csvid,d,p,v,rows,o).case() for v in selected]
