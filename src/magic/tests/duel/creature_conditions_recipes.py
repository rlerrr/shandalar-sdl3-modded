"""Printed entry tapping and land-dependent attack legality, via real combat."""
import copy,re
from collections import Counter
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from creature_recipes import REMINDERS
from permanent_mana_recipes import SPELLS

def spec(d,r):
 from recipes import cost_from_oracle
 if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or 'Creature' not in r.get('type_line','') or
     r.get('type_line','').startswith('Token ') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):raise ValueError('whole normal creature Oracle pin required')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'keywords','enters_tapped','attack_land'}:raise ValueError('explicit static keywords, entry tapping and attack condition required')
 if type(p['enters_tapped']) is not bool or not isinstance(p['keywords'],list) or p['keywords'] not in ([],['Flying'],['Trample'],['Defender']):raise ValueError('reviewed entry state and Flying/Trample/Defender static keywords required')
 land=p['attack_land']
 if land is not None and (not isinstance(land,dict) or set(land)!={'type','scope'} or land['type'] not in ('Island','Mountain') or land['scope'] not in ('defender','battlefield')):raise ValueError('reviewed explicit land type and controller scope required')
 if land is None and not p['enters_tapped']:raise ValueError('at least one reviewed condition required')
 lines=[]
 for line in r['oracle_text'].splitlines():
  text=line
  for reminder in re.findall(r'\([^\n]*?\)',text):
   if reminder not in REMINDERS:raise ValueError('unreviewed reminder')
   text=text.replace(reminder,'').strip()
  lines.append(text)
 expected=list(p['keywords'])
 if p['enters_tapped']:expected.append('This creature enters tapped.')
 if land:
  expected.append("This creature can't attack unless "+('defending player controls an '+land['type'] if land['scope']=='defender' else 'there is a '+land['type']+' on the battlefield')+'.')
 if lines!=expected or sorted(r.get('keywords',[]))!=p['keywords']:raise ValueError('all parameters must match complete Oracle text')
 for k in ('power','toughness'):
  if not re.fullmatch(r'\d+',r.get(k,'')) or not 1<=int(r[k])<=20:raise ValueError('positive bounded numeric Oracle characteristics required')
 return dict(p,family='creature_conditions',power=int(r['power']),toughness=int(r['toughness']),mana=cost_from_oracle(r))

def variants(p):
 common=['entry','cast','cast_no_land','cast_payment','insufficient_mana','cast_snapshot','combat','tapped','sick','fresh_attack','fresh_untapped_attack','untap_attack',
         'block_ground','block_flying','untapped_block_ground','untapped_block_flying','bare_zones','cleanup','ready_snapshot']
 if p['attack_land']:common += ['no_land','own_only','enemy_only','wrong_type','tapped_land','two_lands','offboard','snow_land','dual_land','remove_before_attack','remove_after_attack','add_before_attack','restricted_snapshot']
 return [v+'_'+str(o) for o in (0,1) for v in common]

def validate_witnesses(o,rows):
 for name in ('Twiddle','Boomerang'):
  r=o.get(name,{})
  if name not in rows or (r.get('mana_cost'),r.get('oracle_text'))!=SPELLS[name]:raise ValueError('creature-condition response witness changed: '+name)
 for name,cost,text in (('Spreading Seas','{1}{U}','Enchant land\nWhen this Aura enters, draw a card.\nEnchanted land is an Island.'),('Blood Moon','{2}{R}','Nonbasic lands are Mountains.')):
  if name not in rows or (o.get(name,{}).get('mana_cost'),o.get(name,{}).get('oracle_text'))!=(cost,text):raise ValueError('creature-condition type-change witness changed: '+name)
 for name,pt,text in (('Grizzly Bears',('2','2'),''),('Suntail Hawk',('1','1'),'Flying')):
  r=o.get(name,{})
  if name not in rows or (r.get('power'),r.get('toughness'),r.get('oracle_text'))!=(*pt,text):raise ValueError('creature-condition combat witness changed: '+name)
 for name,type_line in (('Island','Basic Land — Island'),('Mountain','Basic Land — Mountain'),('Forest','Basic Land — Forest'),('Snow-Covered Island','Basic Snow Land — Island'),('Snow-Covered Mountain','Basic Snow Land — Mountain'),('Tropical Island','Land — Forest Island'),('Taiga','Land — Mountain Forest')):
  if name not in rows or o.get(name,{}).get('type_line')!=type_line:raise ValueError('creature-condition land witness changed: '+name)

def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';casting=v in ('cast','cast_no_land','cast_payment','insufficient_mana','cast_snapshot','fresh_attack','fresh_untapped_attack')
  zones={z:[[] if bare else list(ns),[] if bare else list(ns)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY))}
  mana=[[20]*6,[20]*6];life=[20,20];setup=[];entities={};pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)];actions=[]
  def add(nm,ref,pl,zone='battlefield',tapped=False,sick=False):
   item=card(nm,ref,pl,zone)
   if tapped or sick:item['state']=(['tapped'] if tapped else [])+(['sick'] if sick else [])
   setup.append(item);entities[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',tapped=tapped or zone=='battlefield' and nm==name and p['enters_tapped'],sick=sick,damage=0)
   if zone!='battlefield':zones[zone][pl].append(nm)
  def move(ref,zone):
   e=entities[ref];e['alive']=False;zones[zone][e['player']].append(e['name'])
  def cast(ref,target=None,dialog=None):
   e=entities[ref];s=([step('choose_target',player=e['player'],ref=target)] if target else [])+[step('cast',ref=ref,value=1)]
   if dialog is not None:s.append(step('choose_dialog',player=e['player'],value=dialog))
   s += [step('resolve'),step('assert_choices_consumed')];mana[e['player']]=[a-b for a,b in zip(mana[e['player']],cost_from_oracle(oracle[e['name']]))];zones['hand'][e['player']].remove(e['name'])
   if 'Instant' in oracle[e['name']]['type_line']:zones['graveyard'][e['player']].append(e['name'])
   else:
    e['alive']=True;e['tapped']=p['enters_tapped'] if e['name']==name else False;e['sick']=True
    if e['name']=='Spreading Seas':zones['hand'][e['player']].append(zones['library'][e['player']].pop(0))
   return s
  def spell(nm,ref,target=None,dialog=None):add(nm,ref,owner,'hand');return cast(ref,target,dialog)
  def inspect(ref):
   e=entities[ref];s=[observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    s += [observe(ref,'tapped',int(e['tapped']))]
    r=oracle[e['name']]
    if 'Creature' in r['type_line']:s += [observe(ref,'power',int(r['power'])),observe(ref,'toughness',int(r['toughness'])),observe(ref,'damage',e['damage'])]
   return s
  def can_attack():
   src=entities['source']
   if not src['alive'] or src['tapped'] or src['sick'] or 'Defender' in p['keywords']:return False
   cond=p['attack_land']
   return cond is None or any(e['alive'] and ('Land' in oracle[e['name']]['type_line']) and cond['type'] in e.get('land_types',oracle[e['name']]['type_line'].split(' — ')[-1].split()) and (cond['scope']=='battlefield' or e['player']==1-owner) for e in entities.values())
  land=p['attack_land'];base=land['type'] if land else 'Island'
  no_land=v in ('no_land','cast_no_land','restricted_snapshot','bare_zones','block_ground','block_flying','untapped_block_ground','untapped_block_flying','add_before_attack','offboard')
  if land and not no_land:
   nm='Forest' if v=='wrong_type' else 'Snow-Covered '+base if v=='snow_land' else 'Tropical Island' if v=='dual_land' and base=='Island' else 'Taiga' if v=='dual_land' else base
   add(nm,'qualifier',owner if v=='own_only' else 1-owner,tapped=v=='tapped_land')
   if v=='two_lands':add(base,'second_land',1-owner)
  if land and v=='offboard':
   for zone in zones:zones[zone][1-owner].append(base);setup.append(card(base,'offboard_'+zone,1-owner,zone))
  if v=='add_before_attack':add('Forest' if base=='Island' else 'Tropical Island','qualifier',1-owner)
  add(name,'source',owner,'hand' if casting else 'battlefield',tapped=v=='tapped',sick=v=='sick')
  if casting:
   if v in ('cast_payment','insufficient_mana'):
    mana[owner]=list(p['mana']) if v=='cast_payment' else [0]*6;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   if v=='insufficient_mana':actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
   else:actions+=cast('source')+inspect('source')
  if v in ('untap_attack','fresh_untapped_attack','untapped_block_ground','untapped_block_flying'):
   actions += spell('Twiddle','untap','source',1);entities['source']['tapped']=False;actions+=inspect('source')
  if v=='remove_before_attack':actions += spell('Boomerang','remove_land','qualifier');move('qualifier','hand');actions += [step('refresh')]
  if v=='add_before_attack':
   # A legal type-changing spell supplies the condition after fixture load.
   # Neither a fake land cast nor an arbitrary state override is used.
   actions += spell('Spreading Seas','change_type','qualifier') if base=='Island' else spell('Blood Moon','change_type')
   entities['qualifier']['land_types']=[base];actions += [step('refresh')]
  if v=='cleanup':actions += [step('cleanup')]
  attacks=v not in ('entry','cast','cast_no_land','cast_payment','insufficient_mana','cast_snapshot','cleanup') and 'block_' not in v
  if attacks:
   allowed=can_attack();actions += [step('begin_attackers'),step('attack',ref='source',value=int(allowed)),observe('source','attacking',int(allowed)),step('finish_attackers')]
   if allowed:entities['source']['tapped']=True
   if v=='remove_after_attack':actions += spell('Boomerang','remove_land','qualifier');move('qualifier','hand');actions += [step('refresh'),observe('source','attacking',int(allowed))]
   actions += [step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('resolve'),step('end_combat')]
   if allowed:life[1-owner]-=p['power']
  elif 'block_' in v:
   flying=v.endswith('flying');nm='Suntail Hawk' if flying else 'Grizzly Bears';add(nm,'attacker',1-owner);e=entities['attacker'];e['tapped']=True
   allowed=not entities['source']['tapped'] and (not flying or 'Flying' in p['keywords']);actions += [step('turn',player=1-owner,value=0),step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='source',other='attacker',value=int(allowed)),step('advance_combat'),step('combat_damage'),step('resolve'),step('end_combat')]
   ap,at=int(oracle[nm]['power']),int(oracle[nm]['toughness'])
   if not allowed:life[owner]-=ap
   else:
    if p['power']>=at:move('attacker','graveyard')
    else:e['damage']=p['power']
    if ap>=p['toughness']:move('source','graveyard')
    else:entities['source']['damage']=ap
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref in entities:checks+=inspect(ref)
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}|{base}
  for pl in (0,1):
   checks += [step('assert_life',player=pl,value=life[pl])]+[step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   board=[e['name'] for e in entities.values() if e['alive'] and e['player']==pl]
   for zone,names in [('battlefield',board)]+[(z,ns[pl]) for z,ns in zones.items()]:
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(names)))
    if zone=='library':checks += [step('assert_zone_card',player=pl,zone=zone,index=j,value=registry[nm]['csvid']) for j,nm in enumerate(names)]
    else:
     # Battlefield enumeration reports current type-change identities, not
     # printed identities. The qualifier is checked by its stable reference
     # and the total zone count; retain exact identities everywhere else.
     identity_gap={entities['qualifier']['name'],base} if v=='add_before_attack' and zone=='battlefield' and pl==entities['qualifier']['player'] else set()
     multiplicity=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=registry[nm]['csvid'],value=multiplicity[nm]) for nm in sorted(universe-identity_gap)]
  steps=pre+actions+checks
  if v in ('cast_snapshot','ready_snapshot','restricted_snapshot'):steps=pre+[step('capture')]+actions+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions+checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'CreatureConditionsRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='creature_conditions',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['printed identity of type-changed lands','normal land-play timing','other type-changing effects','control changes','multiple blockers','trample overflow','additional clauses or triggers','multiplayer'])))
 return result
