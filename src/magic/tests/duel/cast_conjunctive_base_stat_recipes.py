"""Whole-Oracle two-color casts set base stats, preserving additive layers."""
import copy,re,json
from pathlib import Path
from cast_untap_trigger_recipes import pin,exact
from cast_temporary_stat_trigger_recipes import StatLedger
from defensive_creature_recipes import reviewed_payment

FAMILY='cast_conjunctive_base_stat'
DATA=Path(__file__).parent/'data/cast_conjunctive_base_stat_wave8_profiles.json'
COLOR={'white':'W','blue':'U','black':'B','red':'R','green':'G'}
MONO={'W':'Silvercoat Lion','U':'Coral Merfolk','B':'Scathe Zombies','R':'Raging Goblin','G':'Grizzly Bears'}
MATCH={frozenset('RW'):'Lightning Helix',frozenset('WB'):'Mourning Thrull',frozenset('UR'):'Crag Puca',frozenset('GU'):"Gaea's Skyfolk",frozenset('BG'):'Shambling Shell'}
PATTERN=re.compile(r"Whenever you cast a spell that's both (white|blue|black|red|green) and (white|blue|black|red|green), this creature has base power and toughness (\d+)/(\d+) until end of turn and (gains (first strike|flying|trample|wither) until end of turn\.(?: \(It deals damage to creatures in the form of -1/-1 counters\.\))?|can't be blocked this turn\.)")
def pins():return json.loads(DATA.read_text(encoding='utf8'))
def classify(r):
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or not r.get('type_line','').startswith('Creature ') or r.get('keywords'):raise ValueError('whole normal numeric creature with one printed trigger required')
    if any(not re.fullmatch(r'\d+',r.get(k,'')) for k in ('power','toughness')) or int(r['toughness'])<1:raise ValueError('numeric surviving base required')
    m=PATTERN.fullmatch(r.get('oracle_text',''))
    if not m or m[1]==m[2]:raise ValueError('complete two-distinct-color conjunctive base-setting clause required')
    if not 0<=int(m[3])<=20 or not 1<=int(m[4])<=20:raise ValueError('bounded surviving transformed base required')
    grant=m[6] or 'unblockable'
    if (grant=='wither')!=m[5].endswith('(It deals damage to creatures in the form of -1/-1 counters.)'):raise ValueError('whole Wither reminder required')
    pairs=re.findall(r'\{([WUBRG])/([WUBRG])\}',r.get('mana_cost',''))
    if len(pairs)!=1 or set(pairs[0])!={COLOR[m[1]],COLOR[m[2]]}:raise ValueError('one printed matching hybrid pair required')
    mana,_=reviewed_payment(r,dict(payment_colors=[pairs[0][0]]))
    return dict(colors=[COLOR[m[1]],COLOR[m[2]]],base_power=int(m[3]),base_toughness=int(m[4]),grant=grant,hybrid_pair=list(pairs[0]),mana=mana)
def parameters(p):return {k:copy.deepcopy(p[k]) for k in ('colors','base_power','base_toughness','grant','hybrid_pair')}
def variants(p):
    modes=['source_cast','source_payment','source_other_payment','source_generic_payment','source_insufficient_mana','source_wrong_color','countered_source','fresh_attack','matching_spell','same_card_spell','same_card_other_payment','first_color','second_color','separate_colors','unrelated_multicolor','colorless_spell','extra_colors','opponent_cast','two_sources','two_casts','countered_witness','source_arrives_late','source_removed_before','source_removed_after','growth_before','growth_after','counter_before','counter_after','growth_counter_before','growth_counter_after','repeat_after_growth','counter_cleanup','cleanup','source_bounced','bounce_recast','unblocked','blocked_ground','blocked_flyer','blocked_reach','blocked_large','base_blocked','combat_cleanup','large_combat_cleanup','expiry_combat','ready_snapshot','pending_snapshot','combat_snapshot','resolved_snapshot']
    return [f'{m}_{w}' for w in (0,1) for m in modes]
def spec(d,r):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r.get('oracle_text'):raise ValueError('exact full Oracle/family/version pin required')
    if not exact(pins()['sources'].get(r['name']),pin(r)):raise ValueError('source pin changed')
    p=classify(r)
    if not exact(d.get('params'),parameters(p)):raise ValueError('typed parameters must derive from Oracle')
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or len(vs)!=len(set(vs)) or set(vs)-set(variants(p)):raise ValueError('unique supported variants required')
    return dict(p,family=FAMILY)
def validate_witnesses(o,rows):
    from creature_recipes import oracle_keywords
    data=pins()
    for n,r in dict(data['witnesses'],**data['sources']).items():
        if n not in rows or rows[n]['handler'] in ('0','NULL') or not exact(pin(o.get(n,{})),r):raise ValueError('whole source/witness changed: '+n)
        if n in data['sources']:
            if rows[n]['kind']!='card' or rows[n]['handler']=='card_dummy':raise ValueError('implemented normal primary required')
            classify(o[n])
        elif rows[n]['handler']=='card_dummy':
            try:oracle_keywords(o[n])
            except ValueError:
                from defensive_creature_recipes import spec as defensive_spec
                static=defensive_spec(dict(oracle_text=o[n]['oracle_text'],variants=['cast'],params=dict(keywords=sorted(o[n]['keywords']))),o[n])
                if static['remaining_abilities']:raise ValueError('dummy witness has nonstatic clauses')
    for n,h in data['handlers'].items():
        if rows[n]['handler']!=h:raise ValueError('input handler pin changed: '+n)
def matches(r,p):return set(p['colors'])<=set(r.get('colors',[]))

class BaseLedger(StatLedger):
    def __init__(self,name,csvid,d,p,v,rows,o):
        mode,who=v.rsplit('_',1);self.model=p
        base='source_payment_'+who if mode in ('source_other_payment','source_generic_payment') else v
        super().__init__(name,csvid,d,dict(scope='self',keywords=[],lifelink=False,death_draw=0,activation=None),base,rows,o)
        self.mode=mode;self.variant=v;self.active=set();self.plus={'source':0,'idle':0};self.minus={};self.original=(int(o[name]['power']),int(o[name]['toughness']));self.combat_cut=0
        self.watchers=['source']+(['second'] if mode=='two_sources' else [])
        if mode=='two_sources':self.plus['second']=0
        for ref in list(self.e):
            if ref.startswith('witness_'):
                e=self.e.pop(ref);self.z['hand'][e['player']].remove(e['name']);self.setup=[x for x in self.setup if x['ref']!=ref]
        self.mana=[[30]*6,[30]*6]
        if self.source_hand:
            self.mana[self.owner]=self.cost(name)
            if mode=='source_insufficient_mana':self.mana[self.owner]=[0]*6
            elif mode=='source_wrong_color':
                unused=next(c for c in 'BUGRW' if c not in p['hybrid_pair']);self.mana[self.owner]=[1,0,0,0,0,0];self.mana[self.owner]['CBUGRW'.index(unused)]=1
            elif mode=='source_generic_payment':
                unused=next(c for c in 'BUGRW' if c not in p['hybrid_pair']);self.mana[self.owner][0]=0;self.mana[self.owner]['CBUGRW'.index(unused)]+=1
        self.count=0 if self.source_hand and mode!='source_arrives_late' or mode=='base_blocked' else 2 if mode in ('two_casts','repeat_after_growth','separate_colors') else 1
        default=MATCH[frozenset(p['colors'])]
        for i in range(self.count):
            spell=name if mode in ('same_card_spell','same_card_other_payment') else MONO[p['colors'][0]] if mode=='first_color' else MONO[p['colors'][1]] if mode=='second_color' else MONO[p['colors'][i]] if mode=='separate_colors' else 'Watchwolf' if mode=='unrelated_multicolor' else 'Darksteel Relic' if mode=='colorless_spell' else 'Fusion Elemental' if mode=='extra_colors' else default
            self.add(spell,f'witness_{i}',self.actor,'hand')
        if 'growth' in mode:self.add('Giant Growth','growth',self.owner,'hand')
        if 'counter' in mode and mode not in ('countered_source','countered_witness'):self.add('Travel Preparations','counter_spell',self.owner,'hand')
        if mode in ('source_bounced','bounce_recast'):self.add('Unsummon','bounce',1-self.owner,'hand')
        combat_modes={'fresh_attack','unblocked','blocked_ground','blocked_flyer','blocked_reach','blocked_large','base_blocked','combat_cleanup','large_combat_cleanup','expiry_combat','combat_snapshot'}
        if mode in combat_modes:
            blocker='Suntail Hawk' if mode=='blocked_flyer' else 'Giant Spider' if mode=='blocked_reach' else 'Wall of Denial' if mode in ('blocked_large','large_combat_cleanup') or mode=='expiry_combat' and p['grant']=='wither' else 'Phyrexian Walker' if mode=='expiry_combat' and p['grant']=='trample' else 'Grizzly Bears'
            self.add(blocker,'blocker',1-self.owner)
        self.pre=[x for x in self.pre if x['op']!='set_mana']+[dict(op='set_mana',player=w,color=c,value=n) for w in (0,1) for c,n in enumerate(self.mana[w])]
        self.universe={n for z in self.z.values() for ns in z for n in ns}|{e['name'] for e in self.e.values()}
    def cost(self,n):
        r=self.o[n];pairs=re.findall(r'\{([WUBRG])/([WUBRG])\}',r.get('mana_cost',''))
        if pairs:
            other=self.mode in ('source_other_payment','same_card_other_payment');return reviewed_payment(r,dict(payment_colors=[pair[int(other)] for pair in pairs]))[0]
        return super().cost(n)
    def spend(self,w,cost):
        for c in range(1,6):self.mana[w][c]-=cost[c]
        if min(self.mana[w][1:])<0:raise ValueError('colored cost underfunded')
        generic=cost[0]
        for c in range(6):
            take=min(generic,self.mana[w][c]);self.mana[w][c]-=take;generic-=take
        if generic:raise ValueError('generic cost underfunded')
    def checks(self,stack=0):
        out=super().checks(stack)
        for s in out:
            ref=s.get('ref')
            if s['op']=='assert_card' and ref in self.plus:
                if s['property'] in ('power','toughness'):s['value']+=self.plus[ref]
                elif s['property']=='plus1_counters':s['value']=self.plus[ref]
        from recipes import observe,step
        for ref,e in self.e.items():
            if e['alive'] and e['creature']:out.append(observe(ref,'minus1_counters',self.minus.get(ref,0)))
        out += [step('assert_poison',player=w,value=0) for w in (0,1)]
        return out
    def notify(self,ref):
        spell=self.e[ref]
        if not matches(self.o[spell['name']],self.model):return 0
        live=[r for r in self.watchers if self.e[r]['alive'] and self.e[r]['player']==spell['player']]
        planned=0
        if spell['player']==0 and len(live)>1:self.step('choose_trigger_source',player=0,ref=live[0]);planned=1
        for source in live:self.e[source]['power']=self.model['base_power'];self.e[source]['toughness']=self.model['base_toughness'];self.active.add(source)
        return planned
    def begin(self,ref,denied=False,response=False):
        e=self.e[ref];cost=self.cost(e['name']);hybrid='/' in self.o[e['name']].get('mana_cost','')
        if not denied and (hybrid or any(self.mana[e['player']][c]<cost[c] for c in range(1,6)) or sum(self.mana[e['player']])<sum(cost)):
            self.actions+=self.checks(1 if response else 0)
            pool=cost if hybrid else [30]*6
            if ref=='source' and self.mode=='source_generic_payment':pool=list(self.mana[self.owner])
            pools=copy.deepcopy(self.mana);pools[e['player']]=list(pool);self.set_pools(pools);self.step('refresh')
        if e['name']=='Lightning Helix':self.step('choose_player',player=e['player'],value=1-e['player'])
        elif e['name']=='Giant Growth':self.step('choose_target',player=e['player'],ref='source')
        elif e['name']=='Travel Preparations':
            self.step('choose_target',player=e['player'],ref='source');self.step('choose_target',player=e['player'],ref='idle')
        elif e['name']=='Unsummon':self.step('choose_target',player=e['player'],ref='source')
        if ref=='source' and self.mode=='source_generic_payment' and e['player']==0:
            generic_color=next(c for c in 'BUGRW' if c not in self.model['hybrid_pair'])
            self.step('choose_mana',player=0,color='CBUGRW'.index(generic_color))
        self.step('cast_response' if response else 'cast',ref=ref,value=int(not denied))
        if denied:self.step('assert_stack',value=0);self.step('assert_choices_consumed');return
        self.spend(e['player'],cost);self.z['hand'][e['player']].remove(e['name']);e['pending']=True
        self.step('require_card',ref=ref,property='in_hand',value=0);planned=self.notify(ref);self.step('announce_spell',ref=ref)
        if planned:self.step('assert_unused_choices',value=0)
        self.actions+=self.checks(2 if response else 1)
    def finish(self,ref,countered=False):
        super().finish(ref,countered)
        if countered:return
        e=self.e[ref]
        if e['name']=='Lightning Helix':self.life[e['player']]+=3;self.life[1-e['player']]-=3
        elif e['name']=='Giant Growth':self.e['source']['dp']+=3;self.e['source']['dt']+=3
        elif e['name']=='Travel Preparations':self.plus['source']+=1;self.plus['idle']+=1
        elif e['name']=='Unsummon':
            source=self.e['source'];source['alive']=False;self.z['hand'][source['player']].append(source['name']);source['power'],source['toughness']=self.original;source['dp']=source['dt']=source['damage']=0;self.plus['source']=0;self.active.discard('source')
    def clean(self):
        self.actions+=self.checks();self.set_pools([[0]*6,[0]*6]);self.step('refresh');self.step('cleanup');self.step('refresh')
        self.active.clear()
        for ref in self.watchers:
            if self.e[ref]['alive']:self.e[ref]['power'],self.e[ref]['toughness']=self.original
        for e in self.e.values():
            if e['alive']:e['dp']=e['dt']=e['damage']=0
        self.actions+=self.checks()
    def value(self,ref,key):return self.e[ref][key]+self.e[ref]['dp' if key=='power' else 'dt']+self.plus.get(ref,0)
    def combat(self):
        fresh=self.mode=='fresh_attack';grant=self.model['grant'] if 'source' in self.active else None
        self.step('turn',player=self.owner,value=0);self.step('begin_attackers');self.step('attack',ref='source',value=int(not fresh));self.e['source']['tapped']=not fresh;self.step('finish_attackers');self.actions+=self.checks();self.combat_cut=len(self.actions);self.step('begin_blockers')
        blocked=not fresh and self.mode!='unblocked' and grant!='unblockable' and (grant!='flying' or 'Flying' in self.o[self.e['blocker']['name']].get('keywords',[]) or 'Reach' in self.o[self.e['blocker']['name']].get('keywords',[]))
        if not fresh and self.mode!='unblocked':self.step('block',ref='blocker',other='source',value=int(blocked))
        self.step('advance_combat');self.step('combat_damage')
        if not fresh:
            p=max(0,self.value('source','power'))
            if not blocked:self.life[1-self.owner]-=p
            else:
                b=self.e['blocker'];back=max(0,self.value('blocker','power'));amount=min(p,max(0,self.value('blocker','toughness')-b['damage'])) if grant=='trample' else p
                if grant=='trample':self.life[1-self.owner]-=p-amount
                if grant=='wither':
                    self.minus['blocker']=self.minus.get('blocker',0)+amount;b['power']-=amount;b['toughness']-=amount
                else:b['damage']+=amount
                dead=b['damage']>=self.value('blocker','toughness')
                if grant!='first strike' or not dead:self.e['source']['damage']+=back
                if dead:self.kill('blocker')
                if self.e['source']['damage']>=self.value('source','toughness'):self.kill('source')
        self.step('end_combat');self.actions+=self.checks()
    def case(self):
        from recipes import step
        source_only=self.source_hand and self.mode!='source_arrives_late'
        if source_only:self.cast('source',self.denied_source)
        if self.mode=='source_removed_before':self.kill('source',True);self.actions+=self.checks()
        if not source_only:
            if self.mode in ('growth_before','growth_counter_before'):self.cast('growth')
            if self.mode in ('counter_before','growth_counter_before','counter_cleanup'):self.cast('counter_spell')
            for i in range(self.count):
                self.cast(f'witness_{i}')
                if self.mode=='repeat_after_growth' and i==0:self.cast('growth')
            if self.mode in ('growth_after','growth_counter_after'):self.cast('growth')
            if self.mode in ('counter_after','growth_counter_after'):self.cast('counter_spell')
            if self.mode=='source_arrives_late':self.cast('source')
            if self.mode=='source_removed_after':self.kill('source',True);self.actions+=self.checks()
            if self.mode in ('source_bounced','bounce_recast'):
                self.cast('bounce');self.step('bind_hand_copy',player=self.owner,value=self.csvid,index=0,ref='source');self.step('require_card',ref='source',property='in_hand',value=1)
                if self.mode=='bounce_recast':self.cast('source')
            if self.mode in ('cleanup','counter_cleanup','expiry_combat'):
                self.clean()
                if self.mode=='expiry_combat':
                    self.step('advance_turn');self.step('advance_turn');self.step('untap_step',player=self.owner);self.step('turn',player=self.owner,value=0)
                    for e in self.e.values():
                        if e['alive'] and e['player']==self.owner:e['tapped']=False
                    self.combat()
            if self.mode in ('unblocked','blocked_ground','blocked_flyer','blocked_reach','blocked_large','base_blocked','combat_cleanup','large_combat_cleanup','combat_snapshot'):self.combat()
            if self.mode in ('combat_cleanup','large_combat_cleanup'):self.clean()
        elif self.mode=='fresh_attack' and not self.denied_source:self.combat()
        final=self.checks();seq=self.pre+self.actions+final
        if self.mode in ('ready_snapshot','pending_snapshot','combat_snapshot'):
            cut=0 if self.mode=='ready_snapshot' else self.combat_cut if self.mode=='combat_snapshot' else self.snapshot;tail=self.actions[cut:];seq=self.pre+self.actions[:cut]+[step('capture')]+tail+final+[step('record_state'),step('ai_roundtrip'),step('restore')]+tail+final+[step('assert_recorded_state')]
        if self.mode=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        return dict(name=f'CastConjunctiveBaseStatRecipe_{self.csvid}_{self.variant}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=self.variant,params=self.d['params'],oracle_text=self.d['oracle_text'],untested_components=['No calibrated external noncast-copy/retarget ledger or arbitrary separately suspended trigger boundary.','No multiplayer/replacement or additional unprinted keyword/layer composition is claimed.','Hybrid alternatives use actual constrained legal pools, not an assumed AI payment preference.']))
def expand(name,csvid,d,p,selected,rows,o):
    validate_witnesses(o,rows)
    if len(selected)!=len(set(selected)) or set(selected)-set(variants(p)):raise ValueError('unsupported conjunctive variants')
    return [BaseLedger(name,csvid,d,p,v,rows,o).case() for v in selected]
