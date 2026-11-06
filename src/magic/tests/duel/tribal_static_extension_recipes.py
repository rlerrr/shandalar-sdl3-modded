"""Pinned continuous tribal/color keyword lords with functional recipient checks."""
import copy
import json
from pathlib import Path
from collections import Counter
from static_stats_recipes import matches
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

FAMILY='tribal_static_extension'
REVIEWS=json.loads((Path(__file__).parent/'data/tribal_static_extension_witnesses.json').read_text(encoding='utf8'))
SUPPORTED={'Flying','First strike','Double strike','Vigilance','Haste','Trample','Deathtouch','Lifelink','Hexproof','Shroud','Indestructible'}


def spec(d,r):
 from recipes import cost_from_oracle
 review=REVIEWS['sources'].get(r.get('name')) if r else None
 if (not review or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
     or any(r.get(k)!=v for k,v in review['oracle'].items()) or d.get('oracle_text')!=r['oracle_text']
     or type(d.get('version')) is not int or d.get('version')!=2
     or json.dumps(d.get('params'),sort_keys=True)!=json.dumps(review['params'],sort_keys=True) or 'variants' not in d):
  raise ValueError('tribal static source requires complete reviewed Oracle and explicit parameters')
 p=copy.deepcopy(review['params'])
 if not p['clauses'] or any(set(q)!={'scope','other','state','predicate','value','power','toughness','keywords'} or not set(q['keywords'])<=SUPPORTED for q in p['clauses']):raise ValueError('reviewed ordinary continuous clauses required')
 if not int(r['toughness']):raise ValueError('surviving numeric source required')
 return dict(p,family=FAMILY,mana=cost_from_oracle(r),legendary='Legendary' in r['type_line'],grant_keywords=sorted(set(k for q in p['clauses'] for k in q['keywords'])))


def variants(p):
 modes=['effect','cast','payment','insufficient_mana','pending_snapshot','ready_snapshot','resolved_snapshot','cleanup','tapped_source','sick_source','source_bounce','bounce_recast','source_destroyed','opponent_recipient']
 if any(q['predicate']!='all' for q in p['clauses']):modes+=['nonmatching']
 if any(q['predicate']=='subtypes' for q in p['clauses']):modes+=['changeling']
 if set(p['grant_keywords'])&{'Hexproof','Shroud'}:modes+=['owner_targeting']
 if 'Haste' in p['grant_keywords']:modes+=['late_recipient']
 if any(p['mana'][1:]):modes+=['wrong_color']
 if not p['legendary']:modes+=['two_sources','remove_one_source','opposing_source']
 modes += ['keyword_'+k.replace(' ','_') for k in p['grant_keywords'][1:]]
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]


def validate_witnesses(o,rows):
 for n,pin in REVIEWS['witnesses'].items():
  if n not in rows or any(o.get(n,{}).get(k)!=v for k,v in pin.items()):raise ValueError('tribal static witness changed: '+n)


def expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 if not selected or len(set(selected))!=len(selected) or any(v not in variants(p) for v in selected):raise ValueError('explicit distinct supported tribal static variants required')
 validate_witnesses(o,rows);out=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner)
  casting=mode in ('cast','payment','insufficient_mana','wrong_color','pending_snapshot')
  zones={z:[list(ns),list(ns)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY))}
  mana=[[20]*6,[20]*6];life=[20,20];entities={};setup=[];sources=[];actions=[];pre=[step('turn',player=owner,value=0)];cut=0
  for pl in (0,1):
   for z,players in zones.items():setup.extend(card(n,f'p{pl}_{z}_{i}',pl,z) for i,n in enumerate(players[pl]))
  def add(n,ref,pl,zone='battlefield',tapped=False,sick=False):
   item=card(n,ref,pl,zone)
   if zone=='battlefield':item['state']=(['tapped'] if tapped else ['untapped'])+(['sick'] if sick else [])
   setup.append(item);entities[ref]=dict(name=n,player=pl,alive=zone=='battlefield',tapped=tapped,extra=0,damage=0,attacking=False)
   if zone=='hand':zones['hand'][pl].append(n)
  def applies(q,ref,sr):
   e=entities[ref]
   return matches(q,o[e['name']],entities[sr]['player'],e['player'],ref,sr,tapped=e['tapped'])
  def keys(ref):
   e=entities[ref];ks=set(p['source_keywords'] if e['name']==name else o[e['name']].get('keywords',[]))
   for sr in sources:
    for q in p['clauses']:
     if applies(q,ref,sr):ks.update(q['keywords'])
   return ks
  def value(ref,prop):
   e=entities[ref]
   return int(o[e['name']][prop])+e['extra']+sum(q[prop] for sr in sources for q in p['clauses'] if applies(q,ref,sr))
  def move(ref,z):
   e=entities[ref];e['alive']=False;e['attacking']=False;zones[z][e['player']].append(e['name'])
   if ref in sources:sources.remove(ref)
  def pay(n,pl):mana[pl]=[a-b for a,b in zip(mana[pl],cost_from_oracle(o[n]))]
  def cast(ref,target=None):
   e=entities[ref]
   if target is not None:actions.append(step('choose_target',player=e['player'],ref=target))
   actions.extend([step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]);pay(e['name'],e['player']);zones['hand'][e['player']].remove(e['name'])
   if 'Creature' in o[e['name']]['type_line']:e['alive']=True;e['tapped']=False
   else:zones['graveyard'][e['player']].append(e['name'])
  def spell(n,ref,target,pl=owner):add(n,ref,pl,'hand');cast(ref,target)
  def stats():
   checks=[]
   for ref,e in entities.items():
    if e['alive']:
     checks.extend([observe(ref,'in_play',1),observe(ref,'tapped',int(e['tapped'])),observe(ref,'damage',e['damage'])])
     checks.extend(observe(ref,k,value(ref,k)) for k in ('power','toughness'))
   return checks
  def receipts():
   checks=[];universe={x['name'] for x in setup}
   for pl in (0,1):
    checks.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))]);checks.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
    for n in sorted(universe):checks.append(step('assert_zone_multiplicity',player=pl,zone='battlefield',index=rows[n]['csvid'],value=sum(e['alive'] and e['player']==pl and e['name']==n for e in entities.values())))
    for z,players in zones.items():
     checks.append(step('assert_zone',player=pl,zone=z,value=len(players[pl])))
     checks.extend(step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=players[pl].count(n)) for n in sorted(universe))
     if z=='library':checks.extend(step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(players[pl]))
   return checks
  add(name,'source',owner,'hand' if casting else 'battlefield',tapped=mode=='tapped_source',sick=mode=='sick_source')
  if not casting:sources.append('source')
  for pl in (owner,1-owner):
   add(p['matching'],'match_'+str(pl),pl,'hand' if mode=='late_recipient' and pl==owner else 'battlefield')
   add(p['nonmatching'],'nonmatch_'+str(pl),pl)
   add('Mothdust Changeling','changeling_'+str(pl),pl)
  if mode in ('two_sources','remove_one_source','opposing_source'):
   add(name,'second',1-owner if mode=='opposing_source' else owner);sources.append('second')
  if mode in ('payment','insufficient_mana','wrong_color'):
   mana[owner]=list(p['mana']) if mode!='insufficient_mana' else [0]*6
   if mode=='wrong_color':
    i=next(i for i in range(1,6) if mana[owner][i]);mana[owner][0]+=mana[owner][i];mana[owner][i]=0
  for pl in (0,1):pre.extend(step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
  actions+=stats()
  if casting:
   allowed=mode not in ('insufficient_mana','wrong_color');actions.append(step('cast',ref='source',value=int(allowed)))
   if allowed:
    pay(name,owner);zones['hand'][owner].remove(name);actions.append(step('assert_stack',value=1));actions+=stats();cut=len(actions)
    actions.extend([step('resolve'),step('assert_choices_consumed')]);entities['source']['alive']=True;sources.append('source')
  if mode in ('source_bounce','bounce_recast','remove_one_source'):
   if mode=='remove_one_source' and 'Shroud' in keys('source'):
    actions.append(step('destroy',ref='source'));move('source','graveyard')
   else:spell('Unsummon','bounce','source');move('source','hand')
   actions+=stats()
   if mode=='bounce_recast':
    actions.append(step('bind_hand',player=owner,value=csvid,ref='source'));cast('source');sources.append('source')
  if mode=='source_destroyed':
   protected='Indestructible' in keys('source');actions.append(step('destroy',ref='source'))
   if not protected:move('source','graveyard')
  if mode=='late_recipient':cast('match_'+str(owner))
  if mode=='cleanup':actions.append(step('cleanup'))
  actions+=stats()
  for pl in (0,1):actions.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
  # The actual source cost is already checked above. Provide separate explicit
  # resources for witness casts and combat pumps so exact source payment remains
  # isolated even in a zero-pool negative casting fixture.
  for pl in (0,1):
   mana[pl]=[20]*6;actions.extend(step('set_mana',player=pl,color=i,value=20) for i in range(6))
  target='nonmatch_'+str(owner) if mode=='nonmatching' else 'match_'+str(1-owner) if mode=='opponent_recipient' else 'changeling_'+str(owner) if mode=='changeling' else 'match_'+str(owner)
  actor=entities[target]['player'];keyword=mode[len('keyword_'):].replace('_',' ') if mode.startswith('keyword_') else p['grant_keywords'][0]
  active=keyword in keys(target)
  if keyword=='Haste':
   # Bounce and really recast the witness; a fixture sickness flag is not a cast.
   spell('Unsummon','fresh_bounce',target,actor);move(target,'hand')
   # Populated hands can already contain the same vanilla creature. Bind the
   # returned copy explicitly rather than requiring a unique card identity.
   actions.append(step('bind_hand_copy',player=actor,value=rows[entities[target]['name']]['csvid'],index=zones['hand'][actor].count(entities[target]['name'])-1,ref=target));cast(target)
   active='Haste' in keys(target)
  if keyword in ('Hexproof','Shroud'):
   probe_actor=actor if mode=='owner_targeting' else 1-actor
   add('Unsummon','probe',probe_actor,'hand')
   def legal(ref):return 'Shroud' not in keys(ref) and not ('Hexproof' in keys(ref) and probe_actor!=entities[ref]['player'])
   control='match_'+str(actor) if target=='nonmatch_'+str(actor) else 'nonmatch_'+str(actor)
   actions.extend([step('can_target_creature',ref='probe',other=target,value=int(legal(target))),step('can_target_creature',ref='probe',other=control,value=int(legal(control)))])
   if legal(target):cast('probe',target);move(target,'hand')
  elif keyword=='Indestructible':
   spell('Murder','kill',target,1-actor)
   if not active:move(target,'graveyard')
   control='match_'+str(actor) if target=='nonmatch_'+str(actor) else 'nonmatch_'+str(actor);safe='Indestructible' in keys(control);spell('Murder','control_kill',control,1-actor)
   if not safe:move(control,'graveyard')
  else:
   if keyword in ('Trample','First strike','Deathtouch'):
    spell('Giant Growth','pump',target,actor);entities[target]['extra']=3
   power=value(target,'power');tough=value(target,'toughness');ks=keys(target);allowed=active if keyword=='Haste' else True
   blocker=None
   if keyword in ('Flying','Trample','First strike','Deathtouch'):
    bn='Grizzly Bears' if keyword=='Flying' else 'Ornithopter' if keyword=='Trample' else 'Savannah Lions' if keyword=='First strike' else 'Scaled Wurm'
    add(bn,'blocker',1-actor);blocker='blocker'
   actions.extend([step('turn',player=actor,value=0),step('begin_attackers'),step('attack',ref=target,value=int(allowed)),observe(target,'attacking',int(allowed))]);entities[target]['attacking']=bool(allowed)
   if allowed:entities[target]['tapped']='Vigilance' not in ks
   actions.extend([observe(target,'tapped',int(entities[target]['tapped'])),step('finish_attackers'),step('begin_blockers')])
   blocked=bool(blocker and not (keyword=='Flying' and active))
   if blocker:actions.append(step('block',ref=blocker,other=target,value=int(blocked)))
   actions.extend([step('advance_combat'),step('combat_damage')])
   damage=power*(2 if 'Double strike' in ks else 1) if allowed else 0
   if blocked:
    bt=value(blocker,'toughness');bp=value(blocker,'power');first=bool(ks&{'First strike','Double strike'});kills=power>=bt or 'Deathtouch' in ks
    life[1-actor]-=max(0,damage-bt) if 'Trample' in ks else 0
    dealt=0 if first and kills else bp;entities[target]['damage']=dealt
    if kills:move(blocker,'graveyard')
    else:entities[blocker]['damage']=damage
    if dealt>=tough and 'Indestructible' not in ks:move(target,'graveyard')
   else:life[1-actor]-=damage
   if 'Lifelink' in ks:life[actor]+=damage
   actions.append(step('end_combat'))
   for e in entities.values():e['attacking']=False
   if entities[target]['alive']:actions.append(observe(target,'damage',entities[target]['damage']))
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]+stats()+receipts()
  steps=pre+actions+checks
  if mode in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
   at=0 if mode=='ready_snapshot' else cut if mode=='pending_snapshot' else len(actions)
   steps=pre+actions[:at]+[step('capture')]+copy.deepcopy(actions[at:]+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions[at:]+checks)+[step('assert_recorded_state')]
  if mode in ('source_bounce','source_destroyed','remove_one_source'):
   # First prove the live grant on this board, then restore the complete state
   # before removing its source. A missing implementation must not pass a test
   # that checks only the absence of a grant after source departure.
   before=expand(name,csvid,d,p,[('two_sources' if mode=='remove_one_source' else 'effect')+'_'+str(owner)],rows,o)[0]
   prior={x['ref']:x for x in before['setup']};current={x['ref']:x for x in setup}
   assert all(current.get(ref)==item for ref,item in prior.items())
   extras=[item for ref,item in current.items() if ref not in prior]
   assert all(item['zone']=='hand' for item in extras)
   proof=copy.deepcopy(before['steps'])
   for action in proof:
    if action['op']=='assert_zone' and action.get('zone')=='hand':action['value']+=sum(x['player']==action['player'] for x in extras)
    if action['op']=='assert_zone_multiplicity' and action.get('zone')=='hand':action['value']+=sum(x['player']==action['player'] and rows[x['name']]['csvid']==action['index'] for x in extras)
   proof.extend(step('assert_zone_multiplicity',player=pl,zone='hand',index=rows[n]['csvid'],value=sum(x['player']==pl and x['name']==n for x in extras)+next((s['value'] for s in before['steps'] if s['op']=='assert_zone_multiplicity' and s.get('zone')=='hand' and s.get('player')==pl and s.get('index')==rows[n]['csvid']),0)) for pl,n in sorted({(x['player'],x['name']) for x in extras}))
   steps=pre+[step('capture')]+proof+[step('restore')]+actions+checks
  out.append(dict(name=f'TribalStaticExtensionRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p.get('gaps',[])+['source keyword combat','source activated abilities','additional combat interactions','live type/color/control changes','multiplayer'],untested_abilities=p['inactive'])))
 return out
