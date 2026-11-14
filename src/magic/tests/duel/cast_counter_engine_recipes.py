"""Real cast-grown counters and whole printed paid counter-engine abilities."""
import copy,json,re
from pathlib import Path
from cast_untap_trigger_recipes import pin,exact
from cast_temporary_stat_trigger_recipes import StatLedger

FAMILY='cast_counter_engine'
DATA=Path(__file__).parent/'data/cast_counter_engine_wave8_profiles.json'
KINDS={'ki':'ki','lore':'lore','charge':'charge','+1/+1':'plus1'}
NUM={'a':1,'one':1,'two':2,'three':3,'five':5,'X':'X'}
def pins():return json.loads(DATA.read_text(encoding='utf8'))
def mana(raw):
    from recipes import cost_from_oracle
    return [0]*6 if not raw or raw=='{0}' else cost_from_oracle(dict(mana_cost=raw))
def activation(raw):
    cost,text=raw.split(': ',1);fee='';tap=False;remove=0;kind=None
    for part in cost.split(', '):
        if re.fullmatch(r'(?:\{(?:\d+|[WUBRG])\})+',part):
            if fee:raise ValueError('duplicate mana fee')
            fee=part
        elif part=='{T}':
            if tap:raise ValueError('duplicate tap')
            tap=True
        else:
            m=re.fullmatch(r'Remove (a|one|two|three|five|X) (ki|lore|charge|\+1/\+1) counters? from this (?:artifact|creature|enchantment)',part)
            if not m or kind is not None:raise ValueError('unreviewed complete counter cost')
            remove=NUM[m[1]];kind=KINDS[m[2]]
    effect=None;target='none';amount=0;token=None
    if text=='For each counter removed, this creature gets +2/+0 until end of turn.':effect='self_power';amount=2
    elif text=='Add X mana of any one color.':effect='colored_mana';amount='X'
    elif text=='Tap X target creatures.':effect='tap_creatures';target='creatures';amount='X'
    elif text=='Target creature you control gets +1/+1 until end of turn for each lore counter on this artifact.':effect='lore_pt';target='own_creature';amount='lore'
    elif text=='Draw a card.':effect='draw';amount=1
    elif text=='It deals 5 damage to any target.':effect='damage';target='any';amount=5
    elif text=='Add {R}{R}{R}.':effect='red_mana';amount=3
    else:
        m=re.fullmatch(r'Create a (1/1|3/3) colorless (Spirit|Golem artifact) creature token\.',text)
        if m:
            power,toughness=map(int,m[1].split('/'));name=m[2].split()[0];effect='token';amount=1
            token=dict(name=name,power=power,toughness=toughness,color=0,artifact='artifact' in m[2],subtype=name,keywords=[])
    if effect is None or not fee and not tap and kind is None:raise ValueError('whole fixed counter-engine activation required')
    if effect in ('self_power','colored_mana','tap_creatures') and remove!='X':raise ValueError('scaled payoff needs printedXcost')
    if remove=='X' and effect not in ('self_power','colored_mana','tap_creatures'):raise ValueError('unpiloted variable counter cost payoff')
    if effect=='lore_pt' and (remove or kind is not None):raise ValueError('lore read does not remove counters')
    return dict(mana=mana(fee),tap=tap,remove=remove,counter=kind,effect=effect,target=target,amount=amount,token=token)
def classify(r):
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r.get('type_line','').startswith('Token ') or not any(t in r.get('type_line','') for t in ('Artifact','Creature','Enchantment')):raise ValueError('whole normal implemented permanent required')
    if r.get('keywords'):raise ValueError('unmodeled printed passive keywords')
    lines=r.get('oracle_text','').splitlines()
    if len(lines) not in (2,3) or any(': ' not in s for s in lines[1:]):raise ValueError('one complete cast trigger and activation clauses required')
    m=re.fullmatch(r'Whenever you cast (a Spirit or Arcane|a noncreature|an instant or sorcery|an artifact|a red) spell, (if this creature has fewer than three \+1/\+1 counters on it, )?(you may put|put) a (ki|lore|charge|\+1/\+1) counter on this (artifact|creature|enchantment)\.',lines[0])
    if not m:raise ValueError('unreviewed complete cast-counter trigger')
    predicate={'a Spirit or Arcane':'spirit_arcane','a noncreature':'noncreature','an instant or sorcery':'instant_sorcery','an artifact':'artifact','a red':'red'}[m[1]]
    if bool(m[2])!=(m[4]=='+1/+1') or m[2] and predicate!='red':raise ValueError('unpiloted cap predicate')
    acts=[activation(s) for s in lines[1:]]
    if any(a['counter'] not in (None,KINDS[m[4]]) for a in acts):raise ValueError('counter gain/cost mismatch')
    if len(acts)==2 and {a['effect'] for a in acts}!={'draw','damage'}:raise ValueError('unpiloted multi-mode engine')
    creature='Creature' in r['type_line']
    if creature and any(not re.fullmatch(r'\d+',r.get(k,'')) for k in ('power','toughness')):raise ValueError('numeric source base required')
    return dict(predicate=predicate,counter=KINDS[m[4]],optional=m[3]=='you may put',cap=3 if m[2] else None,abilities=acts,mana=mana(r['mana_cost']))
def parameters(p):return {k:copy.deepcopy(p[k]) for k in ('predicate','counter','optional','cap','abilities')}
def variants(p):
    modes=['source_cast','source_payment','source_insufficient_mana','gain','alternate_spell','nonmatching_spell','opponent_cast','two_casts','three_casts','countered_witness','source_arrives_late','source_removed_before','two_sources','ready_snapshot','pending_snapshot','resolved_snapshot','value','payment','activation_pending_snapshot','tapped','sick','source_removed_pending']
    if any(p['mana'][1:]):modes.append('source_wrong_color')
    a=p['abilities'][0];effects={x['effect'] for x in p['abilities']}
    if sum(a['mana']):modes+=['insufficient_mana']
    if any(a['mana'][1:]):modes+=['wrong_color']
    if type(a['remove']) is int and a['remove']>0:modes+=['counter_short']
    if a['remove']=='X':modes+=['zero_x','partial_x','zero_from_two']
    if effects&{'self_power','lore_pt'}:modes+=['cleanup','repeat']
    if a['effect']=='lore_pt':modes+=['lore_changes_pending','own_target','opponent_target_probe']
    if a['effect']=='token':modes+=['token_fresh','token_ground_combat','token_flying_negative']
    if a['effect']=='colored_mana':modes+=['mana_white','mana_black','mana_red','mana_green']
    if 'damage' in effects:modes+=['damage_player','damage_own_player','damage_creature','damage_own_creature','damage_planeswalker','both_modes','damage_target_probe']
    if p['cap']:modes+=['cap_fourth','spend_regrow','permanent_cleanup']
    result=[]
    for who in (0,1):
        for mode in modes:
            if mode=='activation_pending_snapshot' and effects&{'colored_mana','red_mana'}:continue
            if mode=='source_removed_pending' and effects&{'colored_mana','red_mana'}:continue
            if mode in ('partial_x','zero_from_two') and who==1 and a['effect']!='tap_creatures':continue
            result.append(f'{mode}_{who}')
        if p['optional'] and who==0:result.append('decline_0')
    return result
def spec(d,r):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r.get('oracle_text'):raise ValueError('exact family/version and full Oracle required')
    if not exact(pins()['sources'].get(r['name']),pin(r)):raise ValueError('source pin changed')
    p=classify(r)
    if not exact(d.get('params'),parameters(p)):raise ValueError('typed full-clause parameters must derive from Oracle')
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or len(vs)!=len(set(vs)) or set(vs)-set(variants(p)):raise ValueError('unique calibrated variants required')
    return dict(p,family=FAMILY)
def validate_witnesses(o,rows):
    from creature_recipes import oracle_keywords
    data=pins()
    for n,r in dict(data['witnesses'],**data['sources']).items():
        if n not in rows or rows[n]['handler'] in ('0','NULL') or not exact(pin(o.get(n,{})),r):raise ValueError('full source/witness identity changed: '+n)
        if n in data['sources']:
            if rows[n]['kind']!='card' or rows[n]['handler']=='card_dummy':raise ValueError('normal implemented primary required')
            classify(o[n])
        elif rows[n]['handler']=='card_dummy':oracle_keywords(o[n])
    token_names={a['token']['name'] for r in data['sources'].values() for a in classify(r)['abilities'] if a['token']}
    if data['token_identity']!={n:pin(o[n]) for n in token_names}:raise ValueError('independent token identity changed')
    for n,h in data['handlers'].items():
        if rows[n]['handler']!=h:raise ValueError('actual presentation handler changed: '+n)
def matches(r,p):
    head,_,tail=r['type_line'].partition(' — ');types=set(head.split());subtypes=set(tail.split())
    return {'spirit_arcane':bool(subtypes&{'Spirit','Arcane'}) or 'Changeling' in r.get('keywords',[]),'noncreature':'Creature' not in types,'instant_sorcery':bool(types&{'Instant','Sorcery'}),'artifact':'Artifact' in types,'red':'R' in r.get('colors',[])}[p['predicate']]

class CounterLedger(StatLedger):
    def __init__(self,name,csvid,d,p,v,rows,o):
        bridge=dict(scope='self',keywords=[],lifelink=False,death_draw=0,activation=None)
        super().__init__(name,csvid,d,bridge,v,rows,o);self.model=p;self.counters={'source':0};self.token_number=0
        if self.mode not in ('source_payment','source_insufficient_mana','source_wrong_color'):self.mana=[[30]*6,[30]*6]
        for ref in list(self.e):
            if ref.startswith('witness_'):
                e=self.e.pop(ref);self.z['hand'][e['player']].remove(e['name']);self.setup=[x for x in self.setup if x['ref']!=ref]
        if self.mode=='two_sources':self.counters['second']=0
        self.activity=self.mode not in ('source_cast','source_payment','source_insufficient_mana','source_wrong_color','gain','alternate_spell','nonmatching_spell','opponent_cast','two_casts','three_casts','countered_witness','source_arrives_late','source_removed_before','two_sources','ready_snapshot','pending_snapshot','resolved_snapshot','decline')
        a=p['abilities'][0];self.amount=0 if self.mode in ('zero_x','zero_from_two') else 1 if self.mode=='partial_x' else 2
        required=2 if a['remove']=='X' else a['remove'] if a['remove'] else 2
        if self.mode.startswith('damage_'):required=7 if self.mode=='damage_target_probe' else 5
        if self.mode=='both_modes':required=7
        if self.mode in ('cap_fourth','spend_regrow'):required=4 if self.mode=='cap_fourth' else 3
        if self.mode=='counter_short':required=max(0,a['remove']-1)
        if self.mode=='zero_x':required=0
        if self.mode=='partial_x':required=3
        if self.mode=='insufficient_mana' and a['effect']=='colored_mana':required=1
        self.count=required if self.activity else 0 if self.source_hand and self.mode!='source_arrives_late' else 2 if self.mode=='two_casts' else 3 if self.mode=='three_casts' else 1
        default={'spirit_arcane':'Kami of False Hope','noncreature':'Darksteel Relic','instant_sorcery':'Reach Through Mists','artifact':'Darksteel Relic','red':'Raging Goblin'}[p['predicate']]
        alternate={'spirit_arcane':'Reach Through Mists','noncreature':'Seal of Cleansing','instant_sorcery':'Divination','artifact':'Ornithopter','red':'Shock'}[p['predicate']]
        self.witness_name=alternate if self.mode=='alternate_spell' else 'Grizzly Bears' if self.mode=='nonmatching_spell' else default
        for i in range(self.count):self.add(self.witness_name,f'witness_{i}',self.actor,'hand')
        if self.mode=='lore_changes_pending':self.add('Reach Through Mists','pending_extra',self.owner,'hand')
        if self.mode=='spend_regrow':self.add('Raging Goblin','regrowth',self.owner,'hand')
        if self.mode=='damage_planeswalker':self.add('Jace Beleren','walker',1-self.owner)
        if a['effect']=='token':self.add('Suntail Hawk' if self.mode=='token_flying_negative' else 'Grizzly Bears','token_opponent',1-self.owner)
        if self.mode=='tapped':
            self.e['source']['tapped']=True;next(x for x in self.setup if x['ref']=='source')['state']=['tapped']
        if self.mode=='sick':next(x for x in self.setup if x['ref']=='source')['state']=['sick']
        self.universe={n for z in self.z.values() for ns in z for n in ns}|{e['name'] for e in self.e.values()}|({a['token']['name']} if a['token'] else set())
        # Superseding the base generic witness changes no printed source pools.
        self.pre=[x for x in self.pre if x['op'] not in ('set_mana',)]+[dict(op='set_mana',player=w,color=c,value=n) for w in (0,1) for c,n in enumerate(self.mana[w])]
    def checks(self,stack=0):
        from recipes import step
        out=super().checks(stack)
        if self.model['counter']=='plus1':
            for s in out:
                ref=s.get('ref')
                if s['op']=='assert_card' and ref in self.counters and self.e[ref]['alive']:
                    if s['property'] in ('power','toughness'):s['value']+=self.counters[ref]
                    elif s['property']=='plus1_counters':s['value']=self.counters[ref]
        for ref,n in self.counters.items():
            if self.e[ref]['alive']:out.append(step('assert_counter',ref=ref,counter_kind=self.model['counter'],value=n))
        for ref,e in self.e.items():
            if e.get('token') and e['alive']:
                t=e['token_definition'];out += [step('assert_matching_permanents',player=e['player'],index=self.rows[t['name']]['csvid'],property=prop,value=value) for prop,value in [('token',1),('color',t['color']),('artifact',int(t['artifact'])),('subtype_'+t['subtype'],1)]]
        return out
    def notify(self,ref):
        spell=self.e[ref];planned=0
        if not matches(self.o[spell['name']],self.model):return planned
        for source in self.counters:
            if not self.e[source]['alive'] or self.e[source]['player']!=spell['player'] or self.model['cap'] and self.counters[source]>=self.model['cap']:continue
            if self.model['optional']:
                if self.mode=='decline':continue
            # This capped-counter handler presents a real ordering choice for
            # multiple human mandatory sources; the remaining singleton then
            # resolves automatically after the first source has been selected.
            if spell['player']==0 and (self.model['optional'] or self.mode=='two_sources' and self.model['cap'] is not None and not planned):
                self.step('choose_trigger_source',player=0,ref=source);planned+=1
            self.counters[source]+=1
        return planned
    def begin(self,ref,denied=False,response=False):
        e=self.e[ref]
        if e['name']=='Shock' and not denied:self.step('choose_player',player=e['player'],value=1-e['player'])
        self.step('cast_response' if response else 'cast',ref=ref,value=int(not denied))
        if denied:self.step('assert_stack',value=0);self.step('assert_choices_consumed');return
        self.spend(e['player'],self.cost(e['name']));self.z['hand'][e['player']].remove(e['name']);e['pending']=True
        stack=2 if response or getattr(self,'pending_ability',False) else 1
        self.step('assert_unused_choices',value=0);self.step('require_card',ref=ref,property='in_hand',value=0);self.step('assert_stack',value=stack)
        planned=self.notify(ref);self.step('announce_spell',ref=ref)
        if planned:self.step('assert_unused_choices',value=0)
        self.actions+=self.checks(stack=stack)
    def finish(self,ref,countered=False):
        super().finish(ref,countered)
        if not countered and self.e[ref]['name']=='Shock':self.life[1-self.e[ref]['player']]-=2
    def bind_token(self,t):
        ref=f'token_{self.token_number}';ordinal=self.token_number;self.token_number+=1
        self.e[ref]=dict(name=t['name'],player=self.owner,alive=True,pending=False,creature=True,power=t['power'],toughness=t['toughness'],dp=0,dt=0,damage=0,tapped=False,lifelink=False,token=True,token_definition=t)
        self.step('bind_permanent_copy',player=self.owner,value=self.rows[t['name']]['csvid'],index=ordinal,ref=ref)
    def kill(self,ref,explicit=False):
        if self.e[ref].get('token'):
            if explicit:self.step('destroy',ref=ref)
            self.e[ref]['alive']=False
        else:super().kill(ref,explicit)
    def use(self,which=0):
        a=self.model['abilities'][which];effect=a['effect'];available=self.counters['source'];x=self.amount if a['remove']=='X' else a['remove'];fee=a['mana']
        no=self.mode in ('counter_short','insufficient_mana','wrong_color') or self.mode=='tapped' and a['tap']
        self.step('require_can_activate',ref='source',value=int(not no))
        if no:self.step('activate',ref='source',value=0);self.actions+=self.checks();return
        planned=0
        if len(self.model['abilities'])==2:
            self.step('choose_activation_mode',ref='source',player=self.owner,mode_label='Draw a card' if effect=='draw' else 'Deal 5 damage',fallback_color=0,allow_auto_nonmana=True)
        if a['remove']=='X' and effect in ('self_power','colored_mana'):
            if self.owner==0 and (effect=='self_power' or available>1):self.step('choose_number',player=self.owner,value=x);planned+=1
        targets=[]
        if effect=='tap_creatures':
            targets=['enemy','idle'][:x]
            for ref in targets:self.step('choose_target',player=self.owner,ref=ref);planned+=1
            if x<available:self.step('choose_cancel_target',player=self.owner);planned+=1
        if effect=='lore_pt':self.step('choose_target',player=self.owner,ref='idle');planned+=1
        if effect=='colored_mana':
            color={'mana_white':5,'mana_black':1,'mana_red':4,'mana_green':3}.get(self.mode,2)
            self.step('choose_color',player=self.owner,value=color);planned+=1
        else:color=None
        if effect=='damage':
            target='walker' if self.mode=='damage_planeswalker' else 'idle' if self.mode=='damage_own_creature' else 'enemy' if self.mode=='damage_creature' else None
            recipient=self.owner if self.mode=='damage_own_player' else 1-self.owner
            self.step('choose_target',player=self.owner,ref=target) if target else self.step('choose_player',player=self.owner,value=recipient);planned+=1
        self.step('activate',ref='source',value=1)
        if len(self.model['abilities'])==2:self.step('require_activation_mode',value=1)
        self.spend(self.owner,fee);self.counters['source']-=x
        if self.counters['source']<0:raise ValueError('Oracle counter cost underfunded')
        if a['tap']:self.e['source']['tapped']=True
        immediate=effect in ('colored_mana','red_mana')
        if immediate:self.mana[self.owner][color if color is not None else 4]+=x if effect=='colored_mana' else a['amount']
        if planned:self.step('assert_unused_choices',value=0)
        self.actions+=self.checks(stack=0 if immediate else 1);self.snapshot=len(self.actions)
        if self.mode=='source_removed_pending':self.kill('source',True);self.actions+=self.checks(stack=1)
        if self.mode=='lore_changes_pending':
            self.pending_ability=True;self.begin('pending_extra');self.step('resolve');self.finish('pending_extra');self.pending_ability=False
        elif not immediate:self.step('resolve')
        if not immediate:
            if effect=='token':self.bind_token(a['token'])
            elif effect=='self_power' and self.e['source']['alive']:self.e['source']['dp']+=a['amount']*x
            elif effect=='lore_pt':self.e['idle']['dp']+=self.counters['source'];self.e['idle']['dt']+=self.counters['source']
            elif effect=='tap_creatures':
                for ref in targets:self.e[ref]['tapped']=True
            elif effect=='draw':self.draw(self.owner,a['amount'])
            elif effect=='damage':
                if target:
                    if target=='walker':self.kill(target)
                    else:
                        self.e[target]['damage']+=a['amount']
                        if self.e[target]['damage']>=self.e[target]['toughness']+self.e[target]['dt']:self.kill(target)
                else:self.life[recipient]-=a['amount']
        self.step('refresh');self.actions+=self.checks()
    def token_combat(self):
        token='token_0';fresh=self.mode=='token_fresh';negative=self.mode=='token_flying_negative'
        if not fresh:
            self.actions+=self.checks();self.set_pools([[0]*6,[0]*6]);self.step('advance_turn')
            if not negative:
                self.step('advance_turn');self.step('untap_step',player=self.owner)
                for e in self.e.values():
                    if e['alive'] and e['player']==self.owner:e['tapped']=False
            self.step('turn',player=1-self.owner if negative else self.owner,value=0)
        attacker='token_opponent' if negative else token;blocker=token if negative else 'token_opponent'
        self.step('begin_attackers');self.step('attack',ref=attacker,value=int(not fresh));self.e[attacker]['tapped']=not fresh;self.step('finish_attackers');self.step('begin_blockers')
        if not fresh:self.step('block',ref=blocker,other=attacker,value=int(not negative))
        self.step('advance_combat');self.step('combat_damage')
        if negative:self.life[self.owner]-=1
        elif not fresh:
            self.e[blocker]['damage']+=self.e[token]['power'];self.e[token]['damage']+=self.e[blocker]['power']
            for ref in (blocker,token):
                if self.e[ref]['damage']>=self.e[ref]['toughness']:self.kill(ref)
        self.step('end_combat');self.actions+=self.checks()
    def case(self):
        from recipes import step
        if self.source_hand and self.mode!='source_arrives_late':self.cast('source',self.denied_source)
        if self.mode=='source_removed_before':self.kill('source',True);self.actions+=self.checks()
        if not self.denied_source:
            for i in range(self.count):self.cast(f'witness_{i}')
            if self.mode=='source_arrives_late':self.cast('source')
            if self.activity:
                a=self.model['abilities'][0]
                if self.mode in ('payment','insufficient_mana','wrong_color'):
                    pools=copy.deepcopy(self.mana);pools[self.owner]=list(a['mana']) if self.mode=='payment' else [sum(a['mana']),0,0,0,0,0] if self.mode=='wrong_color' else [0]*6
                    self.actions+=self.checks();self.set_pools(pools)
                    if self.mode=='insufficient_mana' and a['effect']=='colored_mana':self.step('refresh')
                if self.mode=='opponent_target_probe':self.step('probe_activation_target',ref='source',other='enemy',index=-1,value=0)
                elif self.mode=='damage_target_probe':self.step('probe_activation_target',ref='source',other='noncreature',index=1,value=0)
                elif self.mode in ('cap_fourth','permanent_cleanup'):pass
                elif self.mode.startswith('damage_'):self.use(1)
                elif self.mode=='both_modes':self.use(0);self.use(1)
                else:self.use(0)
                if self.mode=='spend_regrow':self.cast('regrowth')
                if self.mode=='repeat':
                    self.actions+=self.checks();self.set_pools([[0]*6,[0]*6]);self.step('advance_turn');self.step('advance_turn');self.step('untap_step',player=self.owner);self.step('turn',player=self.owner,value=0)
                    self.e['source']['tapped']=False
                    for e in self.e.values():
                        if e['alive']:e['dp']=e['dt']=e['damage']=0
                    self.set_pools([[30]*6,[30]*6]);self.amount=self.counters['source'];self.use(0)
                if self.mode in ('cleanup','permanent_cleanup'):self.clean()
                if self.mode.startswith('token_'):self.token_combat()
        final=self.checks();seq=self.pre+self.actions+final
        if self.mode in ('ready_snapshot','pending_snapshot','activation_pending_snapshot'):
            cut=0 if self.mode=='ready_snapshot' else self.snapshot;tail=self.actions[cut:]
            seq=self.pre+self.actions[:cut]+[step('capture')]+tail+final+[step('record_state'),step('ai_roundtrip'),step('restore')]+tail+final+[step('assert_recorded_state')]
        if self.mode=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        return dict(name=f'CastCounterEngineRecipe_{self.csvid}_{self.variant}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=self.variant,params=self.d['params'],oracle_text=self.d['oracle_text'],untested_components=['Human numeric choices are explicit; unoffered AI numeric alternatives are not asserted as strategy defects.','Arbitrary separately suspended trigger windows, multiplayer, and replacement effects are unclaimed.','Noncast external-copy input calibration and zero-library draw-loss behavior remain outside this pilot.']))
def expand(name,csvid,d,p,selected,rows,o):
    validate_witnesses(o,rows)
    if len(selected)!=len(set(selected)) or set(selected)-set(variants(p)):raise ValueError('unsupported counter engine variants')
    return [CounterLedger(name,csvid,d,p,v,rows,o).case() for v in selected]
