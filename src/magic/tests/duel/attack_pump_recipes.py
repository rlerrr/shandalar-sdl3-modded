"""Whole-text attacking self bonuses through complete combat declarations."""
import re
from collections import Counter
from creature_recipes import oracle_keywords
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
WITNESSES={'Hill Giant':('3','3',[],''),'Grizzly Bears':('2','2',[],''),'Ornithopter':('0','2',['Flying'],'Flying')}

def spec(d,r):
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('pinned whole text and explicit variants required')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'power','toughness','keywords'}:raise ValueError('explicit attacking bonus and keywords required')
 if type(p['power']) is not int or type(p['toughness']) is not int or not 0<=p['power']<=5 or not 0<=p['toughness']<=3 or not (p['power'] or p['toughness']):raise ValueError('bounded nonzero attacking bonus required')
 ps=('+0','-0') if p['power']==0 else (f"+{p['power']}",);ts=('+0','-0') if p['toughness']==0 else (f"+{p['toughness']}",)
 clauses={f'Whenever {ref} attacks, it gets {a}/{b} until end of turn.' for ref in ('this creature',r['name']) for a in ps for b in ts}
 lines=r.get('oracle_text','').splitlines();trigger=[s for s in lines if s in clauses]
 if len(trigger)!=1:raise ValueError('one unconditional attacking-self trigger required')
 stripped=dict(r,oracle_text='\n'.join(s for s in lines if s not in clauses));keys=oracle_keywords(stripped)
 if p['keywords']!=keys or set(keys)&{'Menace','Defender','Deathtouch','Double strike','Lifelink'}:raise ValueError('reviewed whole-text passive keywords required')
 return dict(family='attack_pump_creature',bonus_power=p['power'],bonus_toughness=p['toughness'],keywords=keys,power=int(r['power']),toughness=int(r['toughness']),mana=cost_from_oracle(r),legendary='Legendary' in r['type_line'])

def variants(p):
 vs=['cast','payment','insufficient_mana','fresh_attack','attack','attack_pair','other_attack','defend','tapped','sick','blocked','unblocked','cleanup','remove_from_combat','source_destroyed_after','source_bounced_after','ready_snapshot','declaration_snapshot','resolved_snapshot','bare_zones']
 if not p['legendary']:vs+=['two_sources']
 if any(p['mana'][1:]):vs+=['wrong_color']
 return [v+'_'+str(owner) for owner in (0,1) for v in vs]

def validate_witnesses(o,registry):
 for name,fields in WITNESSES.items():
  r=o.get(name,{})
  if name not in registry or r.get('layout')!='normal' or tuple(r.get(k) for k in ('power','toughness','keywords','oracle_text'))!=fields:raise ValueError('attack pump witness changed: '+name)
 r=o.get('Unsummon',{})
 if 'Unsummon' not in registry or (r.get('mana_cost'),r.get('oracle_text'))!=('{U}',"Return target creature to its owner's hand."):raise ValueError('attack pump bounce witness changed')

def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import card,step,observe
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';casted=v in ('cast','payment','insufficient_mana','fresh_attack','wrong_color');illegal=v in ('insufficient_mana','wrong_color')
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)];libs=[[] if bare else list(LIBRARY) for _ in (0,1)]
  mana=[[10]*6,[10]*6];life=[20,20];setup=[];entities={};pre=[];actions=[]
  def add(nm,ref,player,zone='battlefield',state=None):
   item=card(nm,ref,player,zone)
   if state:item['state']=state
   setup.append(item)
   if zone=='hand':hands[player].append(nm)
   r=oracle[nm];entities[ref]=dict(name=nm,player=player,alive=zone=='battlefield',hand=zone=='hand',power=int(r['power']),toughness=int(r['toughness']),damage=0,tapped=bool(state and 'tapped' in state),attacking=False,blocking=0)
  for player in (0,1):
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):setup += [card(nm,f'{zone}_{player}_{i}',player,zone) for i,nm in enumerate(names)]
  add(name,'source',owner,'hand' if casted else 'battlefield',['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
  add('Hill Giant','idle',owner);add('Grizzly Bears','enemy',1-owner)
  if v=='blocked':add('Ornithopter','blocker',1-owner)
  if v=='two_sources':add(name,'copy',owner)
  if v=='source_bounced_after':setup.append(card('Unsummon','response',0,'hand'));hands[0].append('Unsummon')
  actor=1-owner if v=='defend' else owner
  pre+=[step('turn',player=actor,value=0)]
  if v=='payment':mana[owner]=list(p['mana'])
  if v=='insufficient_mana':mana[owner]=[0]*6
  if v=='wrong_color':
   mana[owner]=list(p['mana']);c=next(c for c in range(1,6) if mana[owner][c]);mana[owner][1 if c!=1 else 2]+=mana[owner][c];mana[owner][c]=0
  for player in (0,1):pre += [step('set_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
  pre += [step('refresh')]
  def stats():
   seq=[]
   for ref,e in entities.items():
    seq += [observe(ref,'in_play',int(e['alive'])),observe(ref,'in_hand',int(e['hand']))]
    if e['alive']:seq += [observe(ref,k,int(e[k])) for k in ('power','toughness','damage','tapped','attacking')]+[observe(ref,'blocking_count',e['blocking'])]
   return seq
  def die(ref):e=entities[ref];e['alive']=False;e['hand']=False;graves[e['player']].append(e['name'])
  if casted:
   actions += [step('cast',ref='source',value=int(not illegal)),step('assert_choices_consumed')]
   if not illegal:
    mana[owner]=[a-b for a,b in zip(mana[owner],p['mana'])];hands[owner].remove(name);entities['source']['alive']=True;entities['source']['hand']=False
    actions += [step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]+stats()
  combat=not casted or v=='fresh_attack';declaration_cut=0
  if combat:
   actions += [step('begin_attackers')]
   attackers=[]
   if v=='defend':attackers=['enemy']
   elif v=='other_attack':attackers=['idle']
   else:
    allowed=v not in ('tapped','sick') and (v!='fresh_attack' or 'Haste' in p['keywords'])
    actions += [step('attack',ref='source',value=int(allowed))]
    if allowed:attackers=['source']
    if v in ('attack_pair','tapped','sick'):attackers+=['idle']
    if v=='two_sources':attackers+=['copy']
   for ref in attackers:
    if ref!='source':actions += [step('attack',ref=ref,value=1)]
    e=entities[ref];e['attacking']=True;e['tapped']='Vigilance' not in p['keywords'] if ref in ('source','copy') else True
   actions += stats();declaration_cut=len(actions)
   actions += [step('finish_attackers')]
   for ref in attackers:
    if ref in ('source','copy'):entities[ref]['power']+=p['bonus_power'];entities[ref]['toughness']+=p['bonus_toughness']
   actions += stats()
   if v=='source_destroyed_after':actions += [step('destroy',ref='source')];die('source')
   if v=='source_bounced_after':
    actions += [step('choose_target',player=0,ref='source'),step('cast',ref='response',value=1),step('resolve'),step('assert_choices_consumed'),step('bind_hand',player=owner,value=csvid,ref='source')]
    hands[0].remove('Unsummon');graves[0].append('Unsummon');mana[0][2]-=1;hands[owner].append(name);entities['source']['alive']=False;entities['source']['hand']=True
   if v=='remove_from_combat':actions += [step('remove_from_combat',ref='source')];entities['source']['attacking']=False;attackers=[]
   actions += [step('begin_blockers')]
   if v=='blocked':actions += [step('block',ref='blocker',other='source',value=1)];entities['blocker']['blocking']=1
   if v=='defend':actions += [step('block',ref='source',other='enemy',value=1)];entities['source']['blocking']=1
   actions += [step('advance_combat')]+stats()
   if v in ('blocked','unblocked','cleanup','remove_from_combat','source_destroyed_after','source_bounced_after'):
    power=entities['source']['power']
    if v=='blocked':
     if power>=2:die('blocker')
     else:entities['blocker']['damage']=power
     if 'Trample' in p['keywords']:life[1-owner]-=max(0,power-2)
    elif v in ('unblocked','cleanup'):life[1-owner]-=power
    actions += [step('combat_damage'),step('end_combat')]
    for e in entities.values():e['attacking']=False;e['blocking']=0
   if v=='cleanup':
    actions += stats()+[step('cleanup')]
    for e in entities.values():
     if e['alive']:r=oracle[e['name']];e['power']=int(r['power']);e['toughness']=int(r['toughness']);e['damage']=0
  checks=stats()+[step('assert_choices_consumed'),step('assert_stack',value=0)]
  for player in (0,1):
   checks += [step('assert_life',player=player,value=life[player]),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
    checks += [step('assert_zone',player=player,zone=zone,value=len(names))]
    checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(names if zone=='library' else dict.fromkeys(names))]
   checks += [step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=count) for nm,count in Counter(hands[player]+graves[player]).items()]
   if v!='cleanup':checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
  seq=pre+actions+checks
  if v in ('ready_snapshot','declaration_snapshot'):
   cut=0 if v=='ready_snapshot' else declaration_cut
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  result.append(dict(name=f'AttackPumpRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family='attack_pump_creature',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['multiple combat phases','multiplayer','Flash timing'])))
 return result
