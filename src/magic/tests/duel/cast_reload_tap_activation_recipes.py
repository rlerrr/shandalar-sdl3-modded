"""Whole printed tap abilities reloaded by actual qualifying spell casts."""
import copy,json,re
from pathlib import Path
from cast_untap_trigger_recipes import pin,exact
from cast_temporary_stat_trigger_recipes import StatLedger

FAMILY='cast_reload_tap_activation'
DATA=Path(__file__).parent/'data/cast_reload_tap_activation_wave8_profiles.json'
PASSIVES={'Defender','Flying','Flash','Devoid (This card has no color.)'}
SPELLS={'any':('Darksteel Relic','Grizzly Bears'),'instant_sorcery':('Reach Through Mists','Divination'),'noncreature':('Darksteel Relic','Reach Through Mists'),'black':('Scathe Zombies','Drudge Skeletons'),'blue':('Coral Merfolk','Wind Drake'),'multicolor':('Watchwolf','Centaur Healer'),'colorless':('Darksteel Relic','Ornithopter'),'Wizard':('Prodigal Sorcerer','Archivist')}

def pins():return json.loads(DATA.read_text(encoding='utf8'))
def mana(raw):
    from recipes import cost_from_oracle
    return [0]*6 if not raw or raw=='{0}' else cost_from_oracle(dict(mana_cost=raw))
def classify(r):
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or not r.get('type_line','').startswith('Creature '):raise ValueError('whole normal creature required')
    if any(not re.fullmatch(r'\d+',r.get(k,'')) for k in ('power','toughness')) or int(r['toughness'])<1:raise ValueError('numeric surviving creature required')
    lines=r.get('oracle_text','').splitlines();passives=[x for x in lines if x in PASSIVES];body=[x for x in lines if x not in PASSIVES]
    if len(passives)!=len(set(passives)) or len(body)!=2:raise ValueError('all printed passive/trigger/activation clauses required')
    keywords=sorted('Devoid' if x.startswith('Devoid ') else x for x in passives)
    if keywords!=sorted(r.get('keywords',[])):raise ValueError('keyword text mismatch')
    trigger=next((x for x in body if x.startswith('Whenever ')),None);ability=next((x for x in body if ': ' in x),None)
    m=re.fullmatch(r'Whenever you cast (a spell|an instant or sorcery spell|a noncreature spell|a black spell|a blue spell|a multicolored spell|a colorless spell|a Wizard spell), (you may )?untap this creature\.',trigger or '')
    if not m:raise ValueError('complete own cast self-untap trigger required')
    predicate={'a spell':'any','an instant or sorcery spell':'instant_sorcery','a noncreature spell':'noncreature','a black spell':'black','a blue spell':'blue','a multicolored spell':'multicolor','a colorless spell':'colorless','a Wizard spell':'Wizard'}[m[1]]
    a=re.fullmatch(r'(\{U\}, )?\{T\}: (.+)',ability or '')
    if not a:raise ValueError('whole tap cost required')
    effects={'This creature deals 1 damage to any target.':'damage_any','This creature deals 1 damage to each opponent.':'damage_opponents','Target creature gets +1/+1 until end of turn.':'target_pt','Target creature gains flying until end of turn.':'target_flying','Target player exiles a card from their graveyard.':'grave_exile','Draw a card, then discard a card.':'loot'}
    if a[2] not in effects:raise ValueError('unreviewed whole activation effect')
    return dict(predicate=predicate,optional=bool(m[2]),keywords=keywords,effect=effects[a[2]],fee=mana('{U}' if a[1] else ''),mana=mana(r['mana_cost']))
def parameters(p):return {k:copy.deepcopy(p[k]) for k in ('predicate','optional','keywords','effect','fee')}
def variants(p):
    modes=['source_cast','source_payment','source_insufficient_mana','source_wrong_color','gain','alternate_spell','opponent_cast','two_sources','two_casts','countered_witness','source_arrives_late','source_removed_before','activation_only','reload','loop_two','tapped_activation','sick_activation','fresh_activation','real_untap_cycle','opposing_turn','payment','source_removed_pending','activation_pending_snapshot','ready_snapshot','pending_snapshot','resolved_snapshot','cleanup','source_attack','block_ground','block_flying']
    if p['predicate']!='any':modes+=['nonmatching_spell']
    if sum(p['fee']):modes+=['insufficient_mana','wrong_color']
    if p['effect']=='damage_any':modes+=['own_player','own_creature','enemy_creature','self_target','planeswalker','illegal_target_probe','target_removed_pending','lethal_repeat']
    if p['effect'] in ('target_pt','target_flying'):modes+=['own_creature','enemy_creature','self_target','illegal_target_probe','target_removed_pending','target_combat']
    if p['effect']=='grave_exile':modes+=['own_graveyard','enemy_graveyard','empty_graveyard','duplicate_graveyard']
    if p['effect']=='loot':modes+=['empty_hand','one_card_library']
    if 'Flying' in p['keywords']:modes+=['flying_ground_block','flying_flyer_block','flying_reach_block']
    if 'Flash' in p['keywords']:modes+=['source_flash']
    result=[f'{m}_{w}' for w in (0,1) for m in modes]
    if p['optional']:result+=['decline_0']
    return result
def spec(d,r):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r.get('oracle_text'):raise ValueError('full Oracle/family/version pin required')
    if not exact(pins()['sources'].get(r['name']),pin(r)):raise ValueError('source identity changed')
    p=classify(r)
    if not exact(d.get('params'),parameters(p)):raise ValueError('typed parameters must derive from every Oracle clause')
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or len(vs)!=len(set(vs)) or set(vs)-set(variants(p)):raise ValueError('unique calibrated variants required')
    return dict(p,family=FAMILY)
def validate_witnesses(o,rows):
    from creature_recipes import oracle_keywords
    data=pins()
    for n,r in dict(data['witnesses'],**data['sources']).items():
        if n not in rows or not exact(pin(o.get(n,{})),r) or rows[n]['handler'] in ('0','NULL'):raise ValueError('full implemented/source or reviewed static witness identity changed: '+n)
        if n in data['sources']:
            if rows[n]['kind']!='card' or rows[n]['handler']=='card_dummy':raise ValueError('implemented normal primary required')
            classify(o[n])
        elif rows[n]['handler']=='card_dummy':oracle_keywords(o[n])
    for n,h in data['handlers'].items():
        if rows[n]['handler']!=h:raise ValueError('pinned handler presentation changed: '+n)
    if set(data['mandatory_multi_source_choice'])!=set(data['sources']) or any(type(v) is not bool for v in data['mandatory_multi_source_choice'].values()):raise ValueError('explicit source-bound presentation schema required')
def matches(r,p):
    head,_,tail=r['type_line'].partition(' — ');types=set(head.split());colors=set(r.get('colors',[]))
    return {'any':True,'instant_sorcery':bool(types&{'Instant','Sorcery'}),'noncreature':'Creature' not in types,'black':'B' in colors,'blue':'U' in colors,'multicolor':len(colors)>1,'colorless':not colors,'Wizard':'Wizard' in tail.split() or 'Changeling' in r.get('keywords',[])}[p['predicate']]

class ReloadLedger(StatLedger):
    def __init__(self,name,csvid,d,p,v,rows,o):
        mode,who=v.rsplit('_',1);base=('source_cast_'+who) if mode in ('fresh_activation','source_flash') else v
        super().__init__(name,csvid,d,dict(scope='self',keywords=p['keywords'],lifelink=False,death_draw=0,activation=None),base,rows,o)
        self.mode=mode;self.variant=v;self.model=p;self.granted=set();self.loyalty={};self.discards=0;self.mana=[[30]*6,[30]*6]
        for ref in list(self.e):
            if ref.startswith('witness_'):
                e=self.e.pop(ref);self.z['hand'][e['player']].remove(e['name']);self.setup=[x for x in self.setup if x['ref']!=ref]
        self.watchers=['source']+(['second'] if mode=='two_sources' else [])
        self.watch_only=mode in ('gain','alternate_spell','nonmatching_spell','opponent_cast','two_sources','two_casts','countered_witness','source_arrives_late','source_removed_before','decline','pending_snapshot','ready_snapshot','resolved_snapshot')
        self.loop=mode in ('reload','loop_two','lethal_repeat')
        self.count=2 if mode in ('two_casts','loop_two') else 1 if self.watch_only or self.loop else 0
        if mode in ('source_cast','source_payment','source_insufficient_mana','source_wrong_color','fresh_activation','source_flash'):self.count=0
        self.witness_name=SPELLS[p['predicate']][int(mode=='alternate_spell')]
        if mode=='nonmatching_spell':self.witness_name='Grizzly Bears' if p['predicate'] in ('instant_sorcery','noncreature','Wizard') else 'Raging Goblin'
        if mode=='opponent_cast':self.actor=1-self.owner
        if mode=='source_payment':self.mana[self.owner]=self.cost(name)
        elif mode=='source_insufficient_mana':self.mana[self.owner]=[0]*6
        elif mode=='source_wrong_color':self.mana[self.owner]=[sum(self.cost(name)),0,0,0,0,0]
        if p['effect']=='loot':
            # Any actual AI discard is observationally identical; no AI card
            # preference is assumed, and later casts rebind a live hand copy.
            self.replace_zone('hand',self.owner,[self.witness_name]*max(2,self.count))
            self.replace_zone('library',self.owner,[self.witness_name]*(1 if mode=='one_card_library' else 12))
            if self.source_hand:self.z['hand'][self.owner].append(name)
            if mode=='empty_hand':self.replace_zone('hand',self.owner,[])
            for i in range(self.count):
                if self.actor!=self.owner:self.add(self.witness_name,f'witness_{i}',self.actor,'hand')
                else:self.e[f'witness_{i}']=dict(name=self.witness_name,player=self.actor,alive=False,pending=False,creature='Creature' in o[self.witness_name]['type_line'],power=int(o[self.witness_name].get('power',0)),toughness=int(o[self.witness_name].get('toughness',0)),dp=0,dt=0,damage=0,tapped=False,lifelink=False)
        else:
            for i in range(self.count):self.add(self.witness_name,f'witness_{i}',self.actor,'hand')
        if self.watch_only:
            for ref in self.watchers:
                if self.e[ref]['alive']:
                    self.e[ref]['tapped']=True;next(x for x in self.setup if x['ref']==ref)['state']=['tapped']
        if mode=='tapped_activation':self.e['source']['tapped']=True;next(x for x in self.setup if x['ref']=='source')['state']=['tapped']
        if mode=='sick_activation':next(x for x in self.setup if x['ref']=='source')['state']=['sick']
        if mode=='planeswalker':self.add('Jace Beleren','walker',1-self.owner);self.loyalty['walker']=int(o['Jace Beleren']['loyalty'])
        if mode in ('block_flying','flying_flyer_block'):self.add('Suntail Hawk','combat_other',1-self.owner)
        elif mode=='flying_reach_block':self.add('Giant Spider','combat_other',1-self.owner)
        elif mode in ('source_attack','block_ground','flying_ground_block','target_combat') or mode=='cleanup' and p['effect']=='target_flying':self.add('Grizzly Bears','combat_other',1-self.owner)
        if mode=='source_flash':self.add('Grizzly Bears','flash_parent',1-self.owner,'hand')
        self.recipient=self.owner if mode=='own_graveyard' else 1-self.owner
        if p['effect']=='grave_exile':self.replace_zone('graveyard',self.recipient,[] if mode=='empty_graveyard' else ['Grizzly Bears']*(2 if mode=='duplicate_graveyard' else 1))
        self.pre=[x for x in self.pre if x['op']!='set_mana']+[dict(op='set_mana',player=w,color=c,value=n) for w in (0,1) for c,n in enumerate(self.mana[w])]
        if p['effect']=='loot':self.pre.append(dict(op='discard_policy',value=1))
        self.universe={n for z in self.z.values() for ns in z for n in ns}|{e['name'] for e in self.e.values()}
    def replace_zone(self,zone,w,ns):
        from recipes import card
        self.setup=[x for x in self.setup if not(x['zone']==zone and x['player']==w) or x['ref']=='source']
        self.z[zone][w]=list(ns);self.setup += [card(n,f'custom_{zone}_{w}_{i}',w,zone) for i,n in enumerate(ns)]
    def checks(self,stack=0):
        out=super().checks(stack)
        for s in out:
            if s['op']=='assert_counter' and s.get('ref') in self.loyalty:s['value']=self.loyalty[s['ref']]
        if 'Devoid' in self.model['keywords']:
            from recipes import step
            if any(self.e[ref]['alive'] for ref in self.watchers):out.append(step('assert_matching_permanents',player=self.owner,index=self.csvid,property='color',value=0))
        if self.model['effect']=='loot':
            from recipes import step
            out.append(step('assert_discard_choices',value=self.discards))
        return out
    def notify(self,ref):
        spell=self.e[ref]
        if not matches(self.o[spell['name']],self.model):return 0
        live=[r for r in self.watchers if self.e[r]['alive'] and self.e[r]['player']==spell['player']];planned=0
        for source in live:
            if self.model['optional'] and self.mode=='decline':continue
            choice=self.model['optional'] or len(live)>1 and pins()['mandatory_multi_source_choice'][self.name] and not planned
            if spell['player']==0 and choice:self.step('choose_trigger_source',player=0,ref=source);planned+=1
            self.e[source]['tapped']=False
        return planned
    def begin(self,ref,denied=False,response=False):
        e=self.e[ref]
        if self.model['effect']=='loot' and ref.startswith('witness_'):self.step('bind_hand_copy',player=e['player'],value=self.rows[e['name']]['csvid'],index=0,ref=ref)
        self.step('cast_response' if response else 'cast',ref=ref,value=int(not denied))
        if denied:self.step('assert_stack',value=0);self.step('assert_choices_consumed');return
        self.spend(e['player'],self.cost(e['name']));self.z['hand'][e['player']].remove(e['name']);e['pending']=True
        self.step('require_card',ref=ref,property='in_hand',value=0);planned=self.notify(ref);self.step('announce_spell',ref=ref)
        if planned:self.step('assert_unused_choices',value=0)
        self.actions+=self.checks(2 if response else 1)
    def finish(self,ref,countered=False):
        super().finish(ref,countered)
        if not countered and self.e[ref]['name']=='Centaur Healer':self.life[self.e[ref]['player']]+=3
    def use(self):
        effect=self.model['effect'];denied=self.mode in ('tapped_activation','sick_activation','fresh_activation','insufficient_mana','wrong_color')
        self.step('require_can_activate',ref='source',value=int(not denied))
        if denied:self.step('activate',ref='source',value=0);self.actions+=self.checks();return
        target='source' if self.mode=='self_target' else 'idle' if self.mode=='own_creature' or effect in ('target_pt','target_flying') and self.mode not in ('enemy_creature','target_removed_pending') else 'enemy'
        if self.mode=='planeswalker':target='walker'
        if effect in ('target_pt','target_flying') or effect=='damage_any' and self.mode in ('own_creature','enemy_creature','self_target','planeswalker','target_removed_pending','lethal_repeat','cleanup'):self.step('choose_target',player=self.owner,ref=target)
        elif effect=='damage_any':self.step('choose_player',player=self.owner,value=self.owner if self.mode=='own_player' else 1-self.owner)
        elif effect=='grave_exile':self.step('choose_player',player=self.owner,value=self.recipient)
        self.step('activate',ref='source',value=1);self.spend(self.owner,self.model['fee']);self.e['source']['tapped']=True
        self.step('assert_unused_choices',value=0);self.actions+=self.checks(1);self.snapshot=len(self.actions)
        if self.mode=='source_removed_pending':self.kill('source',True);self.actions+=self.checks(1)
        if self.mode=='target_removed_pending':self.kill(target,True);self.actions+=self.checks(1)
        if effect=='grave_exile' and self.z['graveyard'][self.recipient]:self.step('choose_zone_card',player=self.recipient,value=self.rows[self.z['graveyard'][self.recipient][0]]['csvid'])
        if effect=='loot' and self.owner==1:self.step('choose_dialog',player=1,value=0)
        self.step('resolve')
        if effect=='damage_opponents':self.life[1-self.owner]-=1
        elif effect=='damage_any':
            creature_target=self.mode in ('own_creature','enemy_creature','self_target','planeswalker','target_removed_pending','lethal_repeat','cleanup')
            if not creature_target:self.life[self.owner if self.mode=='own_player' else 1-self.owner]-=1
            elif self.e[target]['alive']:
                if target in self.loyalty:self.loyalty[target]-=1
                else:
                    self.e[target]['damage']+=1
                    if self.e[target]['damage']>=self.e[target]['toughness']+self.e[target]['dt']:self.kill(target)
        elif effect=='target_pt' and self.e[target]['alive']:self.e[target]['dp']+=1;self.e[target]['dt']+=1
        elif effect=='target_flying' and self.e[target]['alive']:self.granted.add(target)
        elif effect=='grave_exile' and self.z['graveyard'][self.recipient]:self.z['exile'][self.recipient].append(self.z['graveyard'][self.recipient].pop(0))
        elif effect=='loot':
            self.draw(self.owner,1);n=min(self.z['hand'][self.owner],key=lambda n:self.rows[n]['csvid']);self.z['hand'][self.owner].remove(n);self.z['graveyard'][self.owner].append(n);self.discards+=int(self.owner==0)
        self.step('refresh');self.actions+=self.checks()
    def combat(self,target=False):
        defensive=self.mode in ('block_ground','block_flying');attacker='combat_other' if defensive else 'idle' if target else 'source';blocker='source' if defensive else 'combat_other'
        self.step('turn',player=1-self.owner if defensive else self.owner,value=0);self.step('begin_attackers')
        can_attack=not(attacker=='source' and 'Defender' in self.model['keywords']);self.step('attack',ref=attacker,value=int(can_attack));self.e[attacker]['tapped']=can_attack
        self.step('finish_attackers');self.step('begin_blockers')
        flying=attacker in self.granted or attacker=='source' and 'Flying' in self.model['keywords'] or self.e[attacker]['name']=='Suntail Hawk'
        blocker_flying=blocker=='source' and 'Flying' in self.model['keywords'] or self.e[blocker]['name']=='Suntail Hawk';reach=self.e[blocker]['name']=='Giant Spider'
        blocks=can_attack and (not flying or blocker_flying or reach)
        if can_attack:self.step('block',ref=blocker,other=attacker,value=int(blocks))
        self.step('advance_combat');self.step('combat_damage')
        if blocks:
            for a,b in ((attacker,blocker),(blocker,attacker)):self.e[b]['damage']+=max(0,self.e[a]['power']+self.e[a]['dp'])
            for ref in (attacker,blocker):
                if self.e[ref]['damage']>=self.e[ref]['toughness']+self.e[ref]['dt']:self.kill(ref)
        elif can_attack:self.life[self.e[blocker]['player']]-=max(0,self.e[attacker]['power']+self.e[attacker]['dp'])
        self.step('end_combat');self.actions+=self.checks()
    def case(self):
        from recipes import step
        source_only=self.mode in ('source_cast','source_payment','source_insufficient_mana','source_wrong_color')
        combat_mode=self.mode in ('source_attack','block_ground','block_flying','flying_ground_block','flying_flyer_block','flying_reach_block')
        if self.mode=='source_flash':
            self.step('turn',player=1-self.owner,value=0);self.begin('flash_parent');self.begin('source',response=True);self.step('resolve_response');self.finish('source');self.finish('flash_parent');self.actions+=self.checks()
        elif self.source_hand and self.mode!='source_arrives_late':self.cast('source',self.denied_source)
        if self.mode=='source_removed_before':self.kill('source',True);self.actions+=self.checks()
        if not self.denied_source and not source_only and self.mode!='source_flash':
            if combat_mode:self.combat()
            elif self.watch_only:
                for i in range(self.count):self.cast(f'witness_{i}')
                if self.mode=='source_arrives_late':self.cast('source')
            else:
                if self.mode in ('payment','insufficient_mana','wrong_color'):
                    pools=copy.deepcopy(self.mana);pools[self.owner]=list(self.model['fee']) if self.mode=='payment' else [sum(self.model['fee']),0,0,0,0,0] if self.mode=='wrong_color' else [0]*6;self.actions+=self.checks();self.set_pools(pools);self.step('refresh')
                if self.mode=='opposing_turn':self.step('turn',player=1-self.owner,value=0)
                if self.mode=='illegal_target_probe':self.step('probe_activation_target',ref='source',other='noncreature',index=-1,value=0);self.actions+=self.checks()
                else:self.use()
                if self.loop:
                    for i in range(self.count):
                        self.step('require_can_activate',ref='source',value=0);self.cast(f'witness_{i}');self.use()
                if self.mode=='real_untap_cycle':
                    self.actions+=self.checks();self.set_pools([[0]*6,[0]*6]);self.step('refresh');self.step('advance_turn');self.step('advance_turn');self.step('untap_step',player=self.owner);self.step('turn',player=self.owner,value=0)
                    for e in self.e.values():
                        if e['alive']:e['dp']=e['dt']=e['damage']=0;e['tapped']=False if e['player']==self.owner else e['tapped']
                    self.granted.clear();self.set_pools([[30]*6,[30]*6]);self.step('refresh');self.actions+=self.checks();self.use()
                if self.mode=='cleanup':
                    self.granted.clear();self.clean()
                    if self.model['effect']=='target_flying':self.combat(target=True)
                if self.mode=='target_combat':self.combat(target=True)
        final=self.checks();seq=self.pre+self.actions+final
        if self.mode in ('ready_snapshot','pending_snapshot','activation_pending_snapshot'):
            cut=0 if self.mode=='ready_snapshot' else self.snapshot;tail=self.actions[cut:];seq=self.pre+self.actions[:cut]+[step('capture')]+tail+final+[step('record_state'),step('ai_roundtrip'),step('restore')]+tail+final+[step('assert_recorded_state')]
        if self.mode=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        return dict(name=f'CastReloadTapActivationRecipe_{self.csvid}_{self.variant}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=self.variant,params=self.d['params'],oracle_text=self.d['oracle_text'],untested_components=['No arbitrary trigger suspension, multiplayer, replacement effects, or free creature cast above a pending activation.','AI strategic optional decline is not claimed; constrained identical loot choices preserve exact receipts without assuming preference.','Empty-library draw-loss and noncast external-copy trigger calibration remain separate gaps.']))
def expand(name,csvid,d,p,selected,rows,o):
    validate_witnesses(o,rows)
    if len(selected)!=len(set(selected)) or set(selected)-set(variants(p)):raise ValueError('unsupported reload variants')
    return [ReloadLedger(name,csvid,d,p,v,rows,o).case() for v in selected]
