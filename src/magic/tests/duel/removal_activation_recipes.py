"""Whole-Oracle removal abilities with actual per-candidate target probes."""
import json
import re
from collections import Counter
from pathlib import Path
from creature_recipes import KEYWORDS,REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from removal_recipes import SCOPES as BASIC_SCOPES,eligible as basic_eligible,effect_spec

PINS=json.loads((Path(__file__).parent/'data/removal_activation_witnesses.json').read_text(encoding='utf8'))['cards']
TARGETS=tuple(n for n,r in PINS.items() if r.get('target_witness'))
SCOPES=dict(BASIC_SCOPES)
for word,color in (('white','W'),('blue','U'),('black','B'),('red','R'),('green','G')):
 SCOPES[word+'_creature']=(word+' creature',('Creature',),(),(),(color,),(),())
 SCOPES[word+'_permanent']=(word+' permanent',(),('Artifact','Creature','Enchantment','Land','Planeswalker'),(),(color,),(),())
SCOPES.update({
 'noncreature_artifact':('noncreature artifact',('Artifact',),(),('Creature',),(),(),()),
 'artifact_or_creature':('artifact or creature',(),('Artifact','Creature'),(),(),(),()),
 'enchanted_creature':('enchanted creature',('Creature',),(),(),(),(),()),
 'islandwalk_creature':('creature with islandwalk',('Creature',),(),(),(),(),()),
 'aura':('Aura',('Enchantment',),(),(),(),(),('Aura',)),
 'creature_aura':('Aura attached to a creature',('Enchantment',),(),(),(),(),('Aura',)),
 'land_aura':('Aura attached to a land',('Enchantment',),(),(),(),(),('Aura',)),
 'own_creature_aura':('Aura attached to a creature you control',('Enchantment',),(),(),(),(),('Aura',)),
 'skeleton_vampire_zombie':('Skeleton, Vampire, or Zombie',(),('Creature','Artifact','Enchantment','Land','Planeswalker'),(),(),(),()),
})


def eligible(r,p,owner=0,target_owner=1,*,tapped=False,attacking=False,blocking=False,parent=None,parent_owner=None,enchanted=False):
 if not basic_eligible(r,p['effect'],owner,target_owner,tapped,attacking,blocking,scope_defs=SCOPES):return False
 scope=p['effect']['scope']
 if scope=='enchanted_creature' and not enchanted:return False
 if scope=='islandwalk_creature' and 'Islandwalk' not in r.get('keywords',[]):return False
 if scope in ('creature_aura','land_aura','own_creature_aura'):
  if parent is None:return False
  if ('Land' if scope=='land_aura' else 'Creature') not in parent['type_line']:return False
  if scope=='own_creature_aura' and parent_owner!=owner:return False
 if scope=='skeleton_vampire_zombie' and 'Changeling' not in r.get('keywords',[]) and not set(r['type_line'].partition(' — ')[2].split())&{'Skeleton','Vampire','Zombie'}:return False
 return True


def spec(d,r):
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('removal activation requires pinned normal whole text and explicit variants')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'effect','cost','keywords','mana_ability','enters_tapped'}:raise ValueError('explicit removal effect, complete costs and passive source scope required')
 c=p['cost'];kinds=r['type_line'].partition(' — ')[0].split();kind=next((t.lower() for t in ('Creature','Artifact','Enchantment','Land') if t in kinds),None);creature='Creature' in kinds;land='Land' in kinds
 if not kind or 'Aura' in r['type_line'] or 'Equipment' in r['type_line']:raise ValueError('standalone removal ability source required')
 if not isinstance(c,dict) or set(c)!={'mana','tap','self','discard','pay_life'} or not isinstance(c['mana'],str) or type(c['tap']) is not bool or c['self'] not in ('none','sacrifice','exile') or type(c['discard']) is not int or not 0<=c['discard']<=3 or type(c['pay_life']) is not int or not 0<=c['pay_life']<=5:raise ValueError('ordinary mana/tap/self/discard/life costs required')
 if type(p['mana_ability']) is not bool or type(p['enters_tapped']) is not bool:raise ValueError('explicit mana sibling and entry flags required')
 if p['mana_ability'] and creature:raise ValueError('reviewed noncreature colorless mana sibling required')
 texts,pool=effect_spec(p['effect'],r['name'],scope_defs=SCOPES)
 bits=([c['mana']] if c['mana'] else [])+(['{T}'] if c['tap'] else [])+([f"Pay {c['pay_life']} life"] if c['pay_life'] else [])+(['Discard '+{1:'a card',2:'two cards',3:'three cards'}[c['discard']]] if c['discard'] else [])+([c['self'].capitalize()+' this '+kind] if c['self']!='none' else [])
 if not bits:raise ValueError('nonempty removal activation cost required')
 expected={', '.join(bits)+': '+t for t in texts};lines=r['oracle_text'].splitlines();found=[l for l in lines if l in expected]
 if len(found)!=1:raise ValueError('parameters must match the complete printed removal activation')
 lines.remove(found[0])
 if p['enters_tapped']:
  clause='This '+kind+' enters tapped.'
  if clause not in lines:raise ValueError('printed tapped entry required')
  lines.remove(clause)
 if p['mana_ability']:
  if not lines or lines.pop(0)!='{T}: Add {C}.':raise ValueError('printed colorless mana sibling required')
 canonical={k.lower():k for k in KEYWORDS};keys=[]
 for line in lines:
  for reminder in re.findall(r'\([^\n]*?\)',line):
   if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed source reminder')
   line=line.replace(reminder,'')
  for word in line.strip().split(','):
   if word.strip().lower() not in canonical:raise ValueError('additional source ability needs separate coverage')
   keys.append(canonical[word.strip().lower()])
 if not isinstance(p['keywords'],list) or p['keywords']!=sorted(keys) or len(keys)!=len(set(keys)) or set(keys)!=set(r.get('keywords',[])):raise ValueError('explicit passive keyword scope disagrees')
 if creature and (any(not re.fullmatch(r'\d+',r.get(k,'')) or not 0<=int(r[k])<=20 for k in ('power','toughness')) or not int(r['toughness'])):raise ValueError('surviving numeric source required')
 dialogs=d.get('activation_dialogs',{})
 if not isinstance(dialogs,dict) or any(k not in ('0','1') or type(v) is not int or not 0<=v<=16 for k,v in dialogs.items()) or dialogs and not p['mana_ability']:raise ValueError('reviewed mana/removal mode presentation required')
 return dict(p,family='removal_activation',mana=None if land else cost_from_oracle(r),activation_mana=cost_from_oracle(dict(mana_cost=c['mana'])) if c['mana'] else [0]*6,creature=creature,land=land,legendary='Legendary' in kinds,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,add_pool=pool)


def variants(p):
 vs=['effect','payment','bare_zones','own_target','empty_battlefield','two_targets','cleanup','tapped_source','ready_snapshot','pending_snapshot','resolved_snapshot','opposing_turn','tax']
 vs += ['card_'+str(i) for i in range(len(TARGETS))]
 if not p['mana_ability']:
  vs+=['tax_insufficient']
  if sum(p['activation_mana']):vs+=['insufficient_mana']
  if any(p['activation_mana'][1:]):vs+=['wrong_color']
 if p['cost']['tap']:vs+=['tapped']+(['sick'] if p['creature'] else [])
 if p['cost']['self']=='none':vs+=['source_destroyed_pending','source_bounced_pending']+(['repeat_denied','untap_repeat'] if p['cost']['tap'] else ['repeat','stacked_activations'])
 if p['cost']['discard']:vs+=['empty_hand_cost','last_discard']
 if not p['land']:vs+=['cast_source','cast_payment','insufficient_cast_mana','fresh_activation']
 if p['effect']['state']!='any':vs+=['wrong_state']
 if p['effect']['state'] in ('tapped','untapped'):vs+=['state_changed_pending']
 if p['effect']['state']=='attacking_or_blocking':vs+=['blocking_target']
 if p['effect']['scope']=='enchanted_creature':vs+=['unenchanted']
 if p['effect']['scope']=='own_creature_aura':vs+=['wrong_parent_controller']
 vs+=['target_bounced','target_destroyed']
 if any(eligible(PINS[n],p,target_owner=0 if p['effect']['controller']=='self' else 1,tapped=p['effect']['state']=='tapped',attacking=p['effect']['state'] in ('attacking','attacking_or_blocking'),blocking=p['effect']['state']=='blocking',enchanted=p['effect']['scope']=='enchanted_creature') for n in ('Drudge Skeletons','Horned Troll')):vs+=['regeneration']
 if 'Shroud' in p['keywords'] or 'Hexproof' in p['keywords']:vs=[v for v in vs if v not in ('source_bounced_pending','untap_repeat')]
 return [f'{v}_{owner}' for owner in (0,1) for v in vs]


def validate_witnesses(o,rows):
 for name,fields in PINS.items():
  if name not in rows or any(o.get(name,{}).get(k)!=v for k,v in fields.items() if k!='target_witness'):raise ValueError('removal activation witness changed: '+name)


def expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);c=p['cost'];q=p['effect'];bare=v=='bare_zones'
  casting=v in ('cast_source','cast_payment','insufficient_cast_mana','fresh_activation')
  zones={z:[[] if bare else list(ns),[] if bare else list(ns)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE))};zones['library']=[list(LIBRARY)*4,list(LIBRARY)*4]
  if v=='empty_hand_cost':zones['hand'][owner]=[]
  if v in ('repeat','stacked_activations','untap_repeat') and c['discard']*2>4:zones['hand'][owner]=list(HAND)*2
  unknown=[0,0];mana=[[20]*6,[20]*6];life=[20,20];setup=[];entities={};auras=[];actions=[]
  pre=[step('turn',player=owner,value=0),step('discard_policy',value=2 if v=='last_discard' else 1)]
  if p['mana_ability']:pre.append(step('present_ai_dialogs',value=1))
  for pl in (0,1):
   for z,names in zones.items():setup.extend(card(n,f'p{pl}_{z}_{i}',pl,z) for i,n in enumerate(names[pl]))
  def add(n,ref,pl,zone='battlefield',tapped=False,sick=False,parent=None):
   item=card(n,ref,pl,zone)
   if zone=='battlefield':item['state']=(['tapped'] if tapped else ['untapped'])+(['sick'] if sick else [])
   setup.append(item);entities[ref]=dict(name=n,player=pl,alive=zone=='battlefield',hand=zone=='hand',tapped=tapped,attacking=False,blocking=False,parent=parent)
   if zone=='hand':zones['hand'][pl].append(n)
  def move(ref,dest):
   e=entities[ref];e['alive']=False;e['hand']=dest=='hand';zones[dest][e['player']].append(e['name'])
   for child,ce in entities.items():
    if ce['alive'] and ce['parent']==ref:ce['alive']=False;zones['graveyard'][ce['player']].append(ce['name'])
  def hand_spell(ref,dest='graveyard'):
   e=entities[ref];zones['hand'][e['player']].remove(e['name']);e['hand']=False
   if dest=='battlefield':e['alive']=True
   else:zones[dest][e['player']].append(e['name'])
  def pay(pool,pl):mana[pl]=[a-b for a,b in zip(mana[pl],pool)]
  add(name,'source',owner,'hand' if casting else 'battlefield',tapped=v in ('tapped','tapped_source'),sick=v=='sick')
  target_owner=owner if v=='own_target' or q['controller']=='self' else 1-owner
  def context(n,pl):
   parent=o['Forest'] if n=='Wild Growth' else o['Grizzly Bears'] if n=='Robe of Mirrors' else None
   pp=owner if q['scope']=='own_creature_aura' and v!='wrong_parent_controller' else pl
   return dict(parent=parent,parent_owner=pp,enchanted=q['scope']=='enchanted_creature' and v!='unenchanted' and 'Creature' in o[n]['type_line'])
  def expected(n,pl,tapped=None):
   return eligible(o[n],p,owner,pl,tapped=q['state']=='tapped' if tapped is None else tapped,attacking=q['state'] in ('attacking','attacking_or_blocking'),blocking=q['state']=='blocking' or v=='blocking_target',**context(n,pl))
  candidates=[n for n in TARGETS if expected(n,target_owner)]
  victim=next((n for n in candidates if 'Indestructible' not in o[n].get('keywords',[])),candidates[0] if candidates else 'Grizzly Bears')
  if v.startswith('card_'):victim=TARGETS[int(v[5:])]
  if v=='regeneration':victim=next(n for n in ('Drudge Skeletons','Horned Troll') if expected(n,target_owner))
  def target(n,ref,pl):
   if n in ('Robe of Mirrors','Wild Growth'):
    bearer='Forest' if n=='Wild Growth' else 'Grizzly Bears';parent_owner=owner if q['scope']=='own_creature_aura' and v!='wrong_parent_controller' else pl
    add(bearer,ref+'_bearer',parent_owner,tapped=bearer=='Forest');add(n,ref,pl,'hand',parent=ref+'_bearer');auras.append(ref)
   else:
    mana_source='Land' in o[n]['type_line'] or n in ('Sol Ring','Gilded Lotus')
    tapped=q['state']=='tapped' or mana_source and q['state'] not in ('untapped','attacking','blocking','attacking_or_blocking')
    if v=='wrong_state' and q['state'] in ('tapped','untapped'):tapped=q['state']!='tapped'
    add(n,ref,pl,tapped=tapped)
    if q['scope']=='enchanted_creature' and v!='unenchanted' and 'Creature' in o[n]['type_line']:
     aura=ref+'_aura';add('Pacifism',aura,1-owner,'hand',parent=ref);auras.append(aura)
  if v!='empty_battlefield':target(victim,'recipient',target_owner)
  else:victim=name
  if v in ('two_targets','repeat','stacked_activations','untap_repeat'):target(victim,'second',target_owner)
  if v in ('tax','tax_insufficient'):add('Suppression Field','taxer',1-owner)
  if v=='untap_repeat':add("Kiora's Follower",'untapper',owner)
  response='Boomerang' if v in ('target_bounced','source_bounced_pending') else 'Twiddle' if v=='state_changed_pending' else None
  if response:add(response,'response',1-owner,'hand')
  for ref in auras:
   e=entities[ref];pre.extend([step('turn',player=e['player'],value=0),step('choose_target',player=e['player'],ref=e['parent']),step('cast',ref=ref,value=1),step('resolve'),step('assert_choices_consumed')]);hand_spell(ref,'battlefield')
  pre.append(step('turn',player=1-owner if v=='opposing_turn' else owner,value=0))
  cost=list(p['activation_mana'])
  if v in ('tax','tax_insufficient'):cost[0]+=2
  if v in ('payment','tax','tax_insufficient'):mana[owner]=list(cost)
  if v=='cast_payment':mana[owner]=list(p['mana'])
  if v in ('insufficient_mana','insufficient_cast_mana'):mana[owner]=[0]*6
  if v=='tax_insufficient':mana[owner][0]-=1
  if v=='wrong_color':
   mana[owner]=list(cost);color=next(i for i in range(1,6) if mana[owner][i]);mana[owner][1 if color!=1 else 2]+=mana[owner][color];mana[owner][color]=0
  for pl in (0,1):pre.extend(step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
  pre.append(step('refresh'))
  combat=q['state'] in ('attacking','blocking','attacking_or_blocking')
  if 'recipient' in entities and combat and 'Creature' in o[victim]['type_line'] and v!='wrong_state':
   blocking=q['state']=='blocking' or v=='blocking_target'
   if blocking:add('Scaled Wurm','attacker',1-target_owner);attacker='attacker'
   else:attacker='recipient'
   if blocking or 'Defender' not in o[victim].get('keywords',[]):
    pl=entities[attacker]['player'];pre.extend([step('turn',player=pl,value=0),step('begin_attackers'),step('attack',ref=attacker,value=1),step('finish_attackers'),step('begin_blockers')]);entities[attacker]['attacking']=True;entities[attacker]['tapped']='Vigilance' not in o[entities[attacker]['name']].get('keywords',[])
    if blocking:pre.append(step('block',ref='recipient',other=attacker,value=1));entities['recipient']['blocking']=True
    pre.append(step('advance_combat'))
  def qualifies(ref):
   e=entities[ref];parent=entities.get(e['parent']);enchanted=any(x['alive'] and x['parent']==ref for x in entities.values())
   return e['alive'] and eligible(o[e['name']],p,owner,e['player'],tapped=e['tapped'],attacking=e['attacking'],blocking=e['blocking'],parent=o[parent['name']] if parent else None,parent_owner=parent['player'] if parent else None,enchanted=enchanted)
  ref='recipient' if 'recipient' in entities else 'source';legal=qualifies(ref)
  mode=d.get('activation_dialogs',{}).get(str(owner),-1)
  def probe(target_ref,value):actions.append(step('probe_activation_target',ref='source',other=target_ref,index=mode,value=int(value)))
  def paid_receipts():
   for pl in (0,1):
    actions.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='hand',value=len(zones['hand'][pl])-unknown[pl]),step('assert_zone',player=pl,zone='graveyard',value=len(zones['graveyard'][pl])+unknown[pl]),step('assert_zone',player=pl,zone='exile',value=len(zones['exile'][pl]))])
    actions.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
  def activate(target_ref,stack_before=0):
   if not (v=='payment' and p['mana_ability']):probe(target_ref,True)
   actions.append(step('require_can_activate',ref='source',value=1))
   if mode>=0:actions.append(step('choose_dialog',player=owner,value=mode))
   actions.extend([step('choose_target',player=owner,ref=target_ref),step('activate',ref='source',value=1)]);pay(cost,owner);life[owner]-=c['pay_life']
   if c['discard']:
    if owner==1:unknown[owner]+=c['discard']
    else:
     for _ in range(c['discard']):
      n=(max if v=='last_discard' else min)(zones['hand'][owner],key=lambda n:rows[n]['csvid']);zones['hand'][owner].remove(n);zones['graveyard'][owner].append(n)
   if c['self']!='none':move('source','graveyard' if c['self']=='sacrifice' else 'exile')
   elif c['tap']:entities['source']['tapped']=True
   actions.append(step('assert_stack',value=stack_before+1));paid_receipts();return len(actions)
  def effect(target_ref,regenerate=False):
   e=entities[target_ref]
   if not qualifies(target_ref):return
   if regenerate:e['tapped']=True;e['attacking']=False
   elif not (q['effect']=='destroy' and 'Indestructible' in o[e['name']].get('keywords',[])):move(target_ref,'graveyard' if q['effect']=='destroy' else 'exile')
   life[owner]+=q['gain_life'];life[e['player']]+=q['target_gain_life']-q['target_lose_life']-q['target_damage'];mana[owner]=[a+b for a,b in zip(mana[owner],p['add_pool'])]
   for _ in range(q['draw']):zones['hand'][owner].append(zones['library'][owner].pop(0))
  denied=v in ('insufficient_mana','insufficient_cast_mana','wrong_color','tax_insufficient','tapped','empty_hand_cost') or v=='tapped_source' and c['tap'] or v=='sick' and 'Haste' not in p['keywords'] or not casting and c['discard']>len(zones['hand'][owner])
  cut=0
  if casting:
   actions.append(step('cast',ref='source',value=0 if denied else 1))
   if not denied:
    pay(p['mana'],owner);hand_spell('source','battlefield');entities['source']['tapped']=p['enters_tapped'];actions.extend([step('resolve'),step('assert_choices_consumed')])
    if v=='fresh_activation':
     if p['enters_tapped'] or p['creature'] and c['tap'] and 'Haste' not in p['keywords']:actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
     elif legal:cut=activate(ref);actions.extend([step('resolve'),step('assert_choices_consumed')]);effect(ref)
  elif denied:actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
  elif not legal:
   probe(ref,False)
   if v=='empty_battlefield' and not p['mana_ability']:actions.append(step('require_can_activate',ref='source',value=0))
  else:
   cut=activate(ref)
   if v=='source_destroyed_pending':actions.append(step('destroy',ref='source'));move('source','graveyard')
   if v=='stacked_activations':activate('second',1)
   if response:
    changed='source' if v=='source_bounced_pending' else ref
    actions.extend([step('choose_target',player=1-owner,ref=changed),step('cast',ref='response',value=1)]);pay(cost_from_oracle(o[response]),1-owner);hand_spell('response')
    if response=='Twiddle':actions.append(step('choose_dialog',player=1-owner,value=0 if q['state']=='untapped' else 1));entities[ref]['tapped']=not entities[ref]['tapped']
    else:move(changed,'hand')
   if v=='target_destroyed':actions.append(step('destroy',ref=ref));move(ref,'graveyard')
   regenerate=v=='regeneration' and q['effect']=='destroy' and not q['no_regenerate'];blocked=v=='regeneration' and not regenerate
   if v=='regeneration':actions.append(step('choose_regeneration',ref=ref))
   if regenerate:mana[entities[ref]['player']][1 if victim=='Drudge Skeletons' else 3]-=1
   actions.extend([step('resolve'),step('assert_unused_choices',value=1) if blocked else step('assert_choices_consumed')]);effect(ref,regenerate)
   if v=='stacked_activations':effect('second')
   if v=='repeat_denied':actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
   if v in ('repeat','untap_repeat'):
    if v=='untap_repeat':
     actions.extend([step('choose_target',player=owner,ref='source'),step('activate',ref='untapper',value=1),step('resolve'),step('assert_choices_consumed')]);entities['untapper']['tapped']=True;entities['source']['tapped']=False
    activate('second');actions.extend([step('resolve'),step('assert_choices_consumed')]);effect('second')
  if v=='cleanup':actions.extend([step('cleanup'),step('refresh')])
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for key,e in entities.items():
   if e['alive']:
    checks.extend([observe(key,'in_play',1),observe(key,'tapped',int(e['tapped']))])
    if 'Creature' in o[e['name']]['type_line']:checks.extend(observe(key,k,int(o[e['name']][k])) for k in ('power','toughness'))
  names={item['name'] for item in setup}
  for pl in (0,1):
   checks.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))]);checks.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
   for nm in sorted(names):checks.append(step('assert_zone_multiplicity',player=pl,zone='battlefield',index=rows[nm]['csvid'],value=sum(e['alive'] and e['player']==pl and e['name']==nm for e in entities.values())))
   for zone,names_by_player in zones.items():
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(names_by_player[pl])+unknown[pl]*(1 if zone=='graveyard' else -1 if zone=='hand' else 0)))
    for nm in sorted(names):
     if unknown[pl] and zone=='graveyard':continue
     op='assert_zone_multiplicity_max' if unknown[pl] and zone=='hand' else 'assert_zone_multiplicity';checks.append(step(op,player=pl,zone=zone,index=rows[nm]['csvid'],value=names_by_player[pl].count(nm)))
    if zone=='library':checks.extend(step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(names_by_player[pl]))
   for nm in sorted(names):checks.append(step('assert_hand_graveyard_total',player=pl,index=rows[nm]['csvid'],value=zones['hand'][pl].count(nm)+zones['graveyard'][pl].count(nm)))
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
   at=0 if v=='ready_snapshot' else len(actions) if v=='resolved_snapshot' else cut
   steps=pre+actions[:at]+[step('capture')]+actions[at:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[at:]+checks+[step('assert_recorded_state')]
  result.append(dict(name=f'RemovalActivationRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family='removal_activation',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_keywords=p['keywords'],untested_components=['source keyword combat','alternate abilities and land play','replacement/triggered effects','target protection and live numeric changes','control changes','multiplayer'])))
 return result
