"""Pinned mass destruction/exile/bounce with independent mixed-board receipts."""
import json
import re
from collections import Counter
from pathlib import Path
from creature_recipes import KEYWORDS,REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from removal_recipes import eligible as single_eligible

PINS=json.loads((Path(__file__).parent/'data/mass_removal_witnesses.json').read_text(encoding='utf8'))['cards']
BOARD=tuple(n for n,v in PINS.items() if v.get('board_witness'))
# noun, ordinary predicate, additional required/forbidden types and colors.
SCOPES={
 'permanent':('permanents','permanent',(),(),(),()),
 'nonland_permanent':('nonland permanents','nonland_permanent',(),(),(),()),
 'nonartifact_permanent':('nonartifact permanents','permanent',(),('Artifact',),(),()),
 'creature':('creatures','creature',(),(),(),()),
 'artifact':('artifacts','artifact',(),(),(),()),
 'enchantment':('enchantments','enchantment',(),(),(),()),
 'land':('lands','land',(),(),(),()),
 'nonbasic_land':('nonbasic lands','nonbasic_land',(),(),(),()),
 'artifact_enchantment':('artifacts and enchantments','permanent',('Artifact','Enchantment'),(),(),()),
 'artifact_creature_enchantment':('artifacts, creatures, and enchantments','permanent',('Artifact','Creature','Enchantment'),(),(),()),
 'artifact_creature_land':('artifacts, creatures, and lands','permanent',('Artifact','Creature','Land'),(),(),()),
 'creature_planeswalker':('creatures and planeswalkers','creature_or_planeswalker',(),(),(),()),
 'green_creature':('green creatures','green_creature',(),(),(),()),
 'white_creature':('white creatures','creature',(),(),('W',),()),
 'nonwhite_creature':('nonwhite creatures','nonwhite_creature',(),(),(),()),
 'nonblue_creature':('nonblue creatures','creature',(),(),(),('U',)),
 'aura':('Auras','enchantment',(),(),(),()),
 'nonaura_enchantment':('non-Aura enchantments','enchantment',(),(),(),()),
 'nonwhite_enchantment':('nonwhite enchantments','enchantment',(),(),(),('W',)),
 'green_permanent':('green permanents','permanent',(),(),('G',),()),
 'nonenchanted_creature':("creatures that aren't enchanted",'creature',(),(),(),()),
}


def matches(r,p,*,tapped=False,attacking=False,enchanted=False):
 noun,alias,union,excluded,colors,not_colors=SCOPES[p['scope']]
 q=dict(scope=alias,controller='any',state=p['state'],limit=p['limit'])
 if not single_eligible(r,q,tapped=tapped,attacking=attacking):return False
 types=set(r['type_line'].partition(' — ')[0].split());subs=set(r['type_line'].partition(' — ')[2].split())
 if union and not types&set(union) or types&set(excluded):return False
 if colors and not set(r.get('colors',[]))&set(colors) or set(r.get('colors',[]))&set(not_colors):return False
 if p['scope']=='aura' and 'Aura' not in subs or p['scope']=='nonaura_enchantment' and 'Aura' in subs:return False
 return not (p['scope']=='nonenchanted_creature' and enchanted)


def effects(p,name):
 noun=SCOPES[p['scope']][0]
 if p['state']!='any':noun=p['state']+' '+noun
 if p['limit']:
  b=p['limit'];noun+=' with '+b['field'].replace('_',' ')+f" {b['value']}"+{'eq':'','le':' or less','ge':' or greater'}[b['op']]
 selector={'all':'','self':' you control','opponent':" you don't control",'target_player':' target player controls','target_opponent':' target opponent controls','target_owner':' target player owns'}[p['controller']]
 nouns={noun}
 if p['exclude_self']:nouns={'other '+noun,noun+' except for '+name,noun+' except for '+name.split(',')[0],noun+' except for '+name.split()[0]}
 texts=set()
 for noun in nouns:
  head={'destroy':'Destroy all ','exile':'Exile all ','bounce':'Return all '}[p['effect']]+noun+selector
  if p['effect']=='bounce':head+=" to their owners' hands" if p['controller']=='all' else " to their owner's hand" if p['controller'] in ('self','opponent','target_player','target_opponent') else ' to their hand'
  texts.add(head+'.')
 if p['effect']=='bounce' and p['controller']=='all':texts|={t.replace("their owners' hands","their owner's hand") for t in texts}
 if p['no_regenerate']:texts={t+' '+tail for t in texts for tail in ("They can't be regenerated.","Those creatures can't be regenerated.","Creatures destroyed this way can't be regenerated.")}
 if p['gain_per']:
  what=SCOPES[p['scope']][0].removesuffix('s');n=p['gain_per']
  if p['gain_scope']=='self':tails=['You gain life equal to the number of creatures you controlled that were destroyed this way.']
  else:tails=[f'You gain {n} life for each {what} destroyed this way.',f'You gain {n} life for each permanent destroyed this way.']
  texts={t+' '+tail for t in texts for tail in tails}
 if p['draw_per']:texts={t+' Draw a card for each creature destroyed this way.' for t in texts}
 if p['discard_hand']:texts={t+' You discard your hand.' for t in texts}
 return texts


def spec(d,r):
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('mass removal requires pinned normal whole text and explicit variants')
 p=d.get('params');fields={'mode','effect','scope','controller','state','limit','exclude_self','no_regenerate','gain_per','gain_scope','draw_per','discard_hand','cost','keywords','enters_tapped','uncounterable'}
 if not isinstance(p,dict) or set(p)!=fields:raise ValueError('explicit mass predicate, complete receipts and source costs required')
 if p['mode'] not in ('spell','activation') or p['effect'] not in ('destroy','exile','bounce') or p['scope'] not in SCOPES or p['controller'] not in ('all','self','opponent','target_player','target_opponent','target_owner') or p['state'] not in ('any','tapped','untapped','attacking'):raise ValueError('unreviewed mass scope or mode')
 if any(type(p[k]) is not bool for k in ('exclude_self','no_regenerate','discard_hand','enters_tapped','uncounterable')):raise ValueError('mass flags require explicit booleans')
 if p['no_regenerate'] and p['effect']!='destroy' or p['controller']=='target_owner' and p['effect']!='bounce':raise ValueError('unreviewed mass destination or regeneration clause')
 if type(p['gain_per']) is not int or not 0<=p['gain_per']<=3 or type(p['draw_per']) is not int or p['draw_per'] not in (0,1) or p['gain_scope'] not in ('all','self') or p['gain_scope']=='self' and (p['gain_per']!=1 or p['scope']!='creature'):raise ValueError('bounded destruction-derived receipts required')
 if (p['gain_per'] or p['draw_per']) and p['effect']!='destroy':raise ValueError('secondary receipt requires actual destruction')
 if p['state']!='any' and p['scope']!='creature':raise ValueError('state restriction requires creatures')
 if not isinstance(p['keywords'],list) or any(not isinstance(k,str) for k in p['keywords']):raise ValueError('explicit source keyword list required')
 if 'target_choices' in d and (type(d['target_choices']) is not int or not 0<=d['target_choices']<=2 or not p['controller'].startswith('target_')):raise ValueError('reviewed target presentation count required')
 if p['limit'] is not None:
  b=p['limit']
  if not isinstance(b,dict) or set(b)!={'field','op','value'} or b['field'] not in ('mana_value','power','toughness') or b['op'] not in ('eq','le','ge') or type(b['value']) is not int or not 0<=b['value']<=9:raise ValueError('bounded explicit mass characteristic restriction required')
  if b['field']!='mana_value' and not SCOPES[p['scope']][1].endswith('creature'):raise ValueError('mass power/toughness restriction requires creatures')
 c=p['cost']
 if not isinstance(c,dict) or set(c)!={'mana','tap','self','discard','pay_life'} or not isinstance(c['mana'],str) or type(c['tap']) is not bool or c['self'] not in ('none','sacrifice','exile') or type(c['discard']) is not int or not 0<=c['discard']<=3 or type(c['pay_life']) is not int or not 0<=c['pay_life']<=5:raise ValueError('ordinary explicit mass ability costs required')
 kinds=r['type_line'].partition(' — ')[0].split();creature='Creature' in kinds;kind=next((t.lower() for t in ('Creature','Artifact','Enchantment','Land') if t in kinds),None)
 lines=r['oracle_text'].splitlines();alternatives=[]
 if p['mode']=='spell':
  if not {'Instant','Sorcery'}&set(kinds) or any(c[k] for k in ('mana','tap','discard','pay_life')) or c['self']!='none' or p['keywords'] or p['enters_tapped'] or p['exclude_self']:raise ValueError('ordinary mass instant/sorcery required')
  while lines and re.match(r'^(?:Cycling |Flashback |Miracle |When you cycle this card,)',lines[-1]):alternatives.insert(0,lines.pop())
  if p['uncounterable']:
   if not lines or lines.pop(0)!="This spell can't be countered.":raise ValueError('printed uncounterability required')
  if set(r.get('keywords',[]))-{'Cycling','Flashback','Miracle'}:raise ValueError('unreviewed mass casting mechanic')
  if p['state']=='attacking' and 'Instant' not in kinds:raise ValueError('attacking sweep requires instant timing')
  if ' '.join(lines) not in effects(p,r['name']):raise ValueError('parameters must match the complete ordinary mass spell')
 else:
  if p['uncounterable']:raise ValueError('uncounterability applies to the source spell')
  if not kind or 'Aura' in r['type_line'] or 'Equipment' in r['type_line']:raise ValueError('standalone mass ability source required')
  bits=([c['mana']] if c['mana'] else [])+(['{T}'] if c['tap'] else [])+([f"Pay {c['pay_life']} life"] if c['pay_life'] else [])+(['Discard '+{1:'a card',2:'two cards',3:'three cards'}[c['discard']]] if c['discard'] else [])+([c['self'].capitalize()+' this '+kind] if c['self']!='none' else [])
  if not bits:raise ValueError('nonempty mass activation cost required')
  expected={', '.join(bits)+': '+body for body in effects(p,r['name'])};found=[l for l in lines if l in expected]
  if len(found)!=1:raise ValueError('parameters must match a complete mass activation')
  lines.remove(found[0])
  if p['enters_tapped']:
   clause=f'This {kind} enters tapped.'
   if clause not in lines:raise ValueError('printed tapped entry required')
   lines.remove(clause)
  canonical={k.lower():k for k in KEYWORDS};keys=[]
  for line in lines:
   for reminder in re.findall(r'\([^\n]*?\)',line):
    if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed mass source reminder')
    line=line.replace(reminder,'')
   for word in line.strip().split(','):
    if word.strip().lower() not in canonical:raise ValueError('additional mass source ability requires separate coverage')
    keys.append(canonical[word.strip().lower()])
  if p['keywords']!=sorted(keys) or len(keys)!=len(set(keys)) or set(keys)!=set(r.get('keywords',[])):raise ValueError('mass passive keywords disagree')
 if creature and (any(not re.fullmatch(r'\d+',r.get(k,'')) or not 0<=int(r[k])<=20 for k in ('power','toughness')) or not int(r['toughness'])):raise ValueError('surviving numeric mass source required')
 mana=cost_from_oracle(r);activation=cost_from_oracle(dict(mana_cost=c['mana'])) if c['mana'] else [0]*6
 survives=p['exclude_self'] or not matches(r,p,tapped=c['tap']) or p['effect']=='destroy' and 'Indestructible' in r.get('keywords',[])
 return dict(p,family='mass_removal',mana=mana,activation_mana=activation,creature=creature,instant='Instant' in kinds,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,untested_rules=alternatives,source_survives=survives)


def variants(p):
 vs=['mixed','payment','insufficient_mana','bare_zones','empty_board','own_only','opponent_only','tapped_all','untapped_all','stolen','tokens','aura','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot']
 if any((p['activation_mana'] if p['mode']=='activation' else p['mana'])[1:]):vs.append('wrong_color')
 if p['mode']=='spell':vs+=['countered']+(['opposing_turn'] if p['instant'] else [])
 else:
  vs+=['cast_source','cast_payment','insufficient_cast_mana','fresh_activation','source_destroyed_pending','tax','tax_insufficient']
  if p['cost']['self']=='none' and p['source_survives']:vs+=['repeat_denied'] if p['cost']['tap'] else ['stacked_activations']
  else:vs.remove('source_destroyed_pending')
  if p['cost']['tap']:vs+=['tapped']+(['sick'] if p['creature'] else [])
  if p['cost']['discard']:vs+=['empty_hand_cost','last_discard']
 if p['controller'].startswith('target_'):vs+=['own_target','target_shrouded_pending']
 if p['state']=='attacking':vs+=['own_attackers']
 if p['effect'] in ('destroy','exile','bounce'):vs+=['regeneration']
 return [f'{v}_{owner}' for owner in (0,1) for v in vs]


def validate_witnesses(o,rows):
 for name,fields in PINS.items():
  if name not in rows or any(o.get(name,{}).get(k)!=v for k,v in fields.items() if k!='board_witness'):raise ValueError('mass removal witness changed: '+name)


def expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);c=p['cost'];activation=p['mode']=='activation';bare=v=='bare_zones'
  casting=not activation or v in ('cast_source','cast_payment','insufficient_cast_mana','fresh_activation')
  zones={z:[[] if bare else list(ns),[] if bare else list(ns)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE))};zones['library']=[list(LIBRARY)*16,list(LIBRARY)*16]
  if v=='empty_hand_cost':zones['hand'][owner]=[]
  unknown=[0,0];life=[20,20];mana=[[20]*6,[20]*6];setup=[];entities={};actions=[]
  pre=[step('turn',player=owner,value=0),step('discard_policy',value=2 if v=='last_discard' else 1)]
  for pl in (0,1):
   for z,names in zones.items():setup.extend(card(n,f'p{pl}_{z}_{i}',pl,z) for i,n in enumerate(names[pl]))
  def add(n,ref,pl,zone='battlefield',tapped=False,sick=False,stolen=False,token=False):
   item=card(n,ref,pl,zone)
   if zone=='battlefield':item['state']=(['tapped'] if tapped else ['untapped'])+(['sick'] if sick else [])+(['opponent_owned'] if stolen else [])+(['token'] if token else [])
   setup.append(item);entities[ref]=dict(name=n,player=pl,owner=1-pl if stolen else pl,alive=zone=='battlefield',hand=zone=='hand',tapped=tapped,token=token,attacking=False,parent=None)
   if zone=='hand':zones['hand'][pl].append(n)
  def move(ref,dest):
   e=entities[ref];e['alive']=False;e['hand']=dest=='hand' and not e['token']
   if not e['token']:zones[dest][e['owner']].append(e['name'])
  def hand_spell(ref,dest='graveyard'):
   e=entities[ref];zones['hand'][e['player']].remove(e['name']);e['hand']=False
   if dest=='battlefield':e['alive']=True;e['tapped']=p['enters_tapped'] if ref=='source' else False
   else:zones[dest][e['owner']].append(e['name'])
  def pay(vector,pl):mana[pl]=[a-b for a,b in zip(mana[pl],vector)]
  add(name,'source',owner,'hand' if casting else 'battlefield',tapped=v=='tapped',sick=v=='sick')
  for pl in (0,1):
   if v=='empty_board' or v=='own_only' and pl!=owner or v=='opponent_only' and pl==owner:continue
   for i,n in enumerate(BOARD):
    mana_source='Land' in o[n]['type_line'] or n in ('Sol Ring','Gilded Lotus')
    tapped=mana_source or v=='tapped_all' or v!='untapped_all' and i%2==1
    add(n,f'b{pl}_{i}',pl,tapped=tapped,stolen=v=='stolen')
   if v=='tokens':add('Grizzly Bears',f'token{pl}',pl,token=True)
  if v=='aura':
   for pl in (0,1):
    target=next((ref for ref,e in entities.items() if ref!='source' and e['player']==pl and 'Creature' in o[e['name']]['type_line']),None)
    if target:
     ref=f'aura{pl}';add('Pacifism',ref,pl,'hand');entities[ref]['parent']=target
  if v in ('tax','tax_insufficient'):add('Suppression Field','taxer',1-owner)
  target=owner if v=='own_target' else 1-owner
  negative_own=v=='own_target' and p['controller']=='target_opponent'
  if v=='target_shrouded_pending':add('Gilded Light','shield',target,'hand')
  elif negative_own:add('Gilded Light','shield',1-owner,'hand')
  if v=='countered':add('Counterspell','counter',0,'hand')
  cost=list(p['activation_mana'] if activation else p['mana'])
  if v in ('tax','tax_insufficient'):cost[0]+=2
  if v in ('payment','tax','tax_insufficient'):mana[owner]=list(cost)
  if v=='cast_payment':mana[owner]=list(p['mana'])
  if v in ('insufficient_mana','insufficient_cast_mana'):mana[owner]=[0]*6
  if v=='tax_insufficient':mana[owner][0]-=1
  if v=='wrong_color':
   color=next(i for i in range(1,6) if cost[i]);mana[owner]=list(cost);mana[owner][1 if color!=1 else 2]+=mana[owner][color];mana[owner][color]=0
  for pl in (0,1):
   if f'aura{pl}' in entities:mana[pl]=[a+b for a,b in zip(mana[pl],cost_from_oracle(o['Pacifism']))]
   pre.extend(step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
  pre.append(step('refresh'))
  for pl in (0,1):
   if f'aura{pl}' in entities:
    ref=f'aura{pl}';pre.extend([step('turn',player=pl,value=0),step('choose_target',player=pl,ref=entities[ref]['parent']),step('cast',ref=ref,value=1),step('resolve'),step('assert_choices_consumed')]);pay(cost_from_oracle(o['Pacifism']),pl);hand_spell(ref,'battlefield')
  pre.append(step('turn',player=1-owner if v=='opposing_turn' else owner,value=0))
  if negative_own:
   pre.extend([step('cast',ref='shield',value=1),step('resolve'),step('assert_choices_consumed')]);pay(cost_from_oracle(o['Gilded Light']),1-owner);hand_spell('shield')
  if p['state']=='attacking':
   attacker=owner if v=='own_attackers' else 1-owner;pre.extend([step('turn',player=attacker,value=0),step('begin_attackers')])
   for i,(ref,e) in enumerate(entities.items()):
    if e['alive'] and e['player']==attacker and 'Creature' in o[e['name']]['type_line'] and not e['tapped'] and 'Defender' not in o[e['name']].get('keywords',[]) and i%3 and not any(x['parent']==ref for x in entities.values()):
     pre.append(step('attack',ref=ref,value=1));e['attacking']=True;e['tapped']='Vigilance' not in o[e['name']].get('keywords',[])
   pre.extend([step('finish_attackers'),step('begin_blockers'),step('advance_combat')])
  def target_choice():return [step('choose_player',player=owner,value=target) for _ in range(d.get('target_choices',1))] if p['controller'].startswith('target_') else []
  def selected_entity(ref,e):
   if not e['alive'] or p['exclude_self'] and ref=='source':return False
   who=e['owner'] if p['controller']=='target_owner' else e['player']
   if p['controller']=='self' and who!=owner or p['controller']=='opponent' and who==owner or p['controller'].startswith('target_') and who!=target:return False
   return matches(o[e['name']],p,tapped=e['tapped'],attacking=e['attacking'],enchanted=any(x['alive'] and x['parent']==ref for x in entities.values()))
  def collateral_auras():
   for ref,e in entities.items():
    if e['alive'] and e['parent'] and not entities[e['parent']]['alive']:move(ref,'graveyard')
  def effect(regenerator=None):
   destroyed=[0,0]
   for ref,e in list(entities.items()):
    if not selected_entity(ref,e):continue
    if p['effect']=='destroy' and ('Indestructible' in o[e['name']].get('keywords',[]) or ref==regenerator):continue
    if p['effect']=='destroy':destroyed[e['player']]+=1
    move(ref,{'destroy':'graveyard','exile':'exile','bounce':'hand'}[p['effect']])
   collateral_auras()
   count=destroyed[owner] if p['gain_scope']=='self' else sum(destroyed);life[owner]+=count*p['gain_per']
   for _ in range(sum(destroyed)*p['draw_per']):zones['hand'][owner].append(zones['library'][owner].pop(0))
   if p['discard_hand']:zones['graveyard'][owner].extend(zones['hand'][owner]);zones['hand'][owner]=[];unknown[owner]=0
  def pending_receipts():
   for pl in (0,1):
    actions.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='hand',value=len(zones['hand'][pl])-unknown[pl]),step('assert_zone',player=pl,zone='graveyard',value=len(zones['graveyard'][pl])+unknown[pl]),step('assert_zone',player=pl,zone='exile',value=len(zones['exile'][pl])),step('assert_zone',player=pl,zone='library',value=len(zones['library'][pl]))])
    actions.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
  def activate(previous=0):
   actions.append(step('require_can_activate',ref='source',value=1));actions.extend(target_choice());actions.append(step('activate',ref='source',value=1));pay(cost,owner);life[owner]-=c['pay_life']
   if c['discard']:
    if owner==1:unknown[owner]+=c['discard']
    else:
     for _ in range(c['discard']):
      n=(max if v=='last_discard' else min)(zones['hand'][owner],key=lambda n:rows[n]['csvid']);zones['hand'][owner].remove(n);zones['graveyard'][owner].append(n)
   if c['self']!='none':move('source','graveyard' if c['self']=='sacrifice' else 'exile')
   elif c['tap']:entities['source']['tapped']=True
   actions.append(step('assert_stack',value=previous+1));pending_receipts();return len(actions)
  denied=negative_own or v in ('insufficient_mana','insufficient_cast_mana','wrong_color','tax_insufficient','tapped','empty_hand_cost') or v=='sick' and 'Haste' not in p['keywords'] or not casting and c['discard']>len(zones['hand'][owner])
  cut=0
  if denied:
   if casting:
    if negative_own:actions.append(step('can_cast',ref='source',value=0))
    actions.append(step('cast',ref='source',value=0))
   else:actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
  else:
   if casting:
    if not activation:actions.extend(target_choice())
    if v=='countered':actions.append(step('choose_priority',ref='counter'));pay(cost_from_oracle(o['Counterspell']),0);hand_spell('counter')
    if not activation:actions.append(step('require_can_cast',ref='source',value=1))
    actions.append(step('cast',ref='source',value=0 if v=='countered' and not p['uncounterable'] else 1));pay(p['mana'],owner);hand_spell('source','battlefield' if activation else 'graveyard')
    cut=len(actions)
    if activation or v=='countered' and not p['uncounterable']:actions.extend([step('resolve'),step('assert_choices_consumed')])
   else:
    cut=activate()
    if v=='source_destroyed_pending':actions.append(step('destroy',ref='source'));move('source','graveyard')
    if v=='stacked_activations':activate(1)
   fresh_denied=v=='fresh_activation' and (p['enters_tapped'] or p['creature'] and c['tap'] and 'Haste' not in p['keywords'])
   if v=='fresh_activation':
    if fresh_denied:actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
    else:cut=activate()
   resolving=(v!='countered' or p['uncounterable']) and (not activation or not casting or v=='fresh_activation' and not fresh_denied)
   if resolving:
    protected=v=='target_shrouded_pending'
    if protected:actions.extend([step('cast',ref='shield',value=1),step('assert_stack',value=2)]);pay(cost_from_oracle(o['Gilded Light']),target);hand_spell('shield')
    regenerator=None;attempt=None
    if v=='regeneration':
     ordered=sorted(entities.items(),key=lambda item:item[1]['player']!=owner)
     attempt=next((ref for ref,e in ordered if e['name'] in ('Drudge Skeletons','Horned Troll') and selected_entity(ref,e)),next((ref for ref,e in ordered if e['name']=='Drudge Skeletons'),None))
     if attempt:
      actions.append(step('choose_regeneration',ref=attempt))
      if p['effect']=='destroy' and not p['no_regenerate'] and selected_entity(attempt,entities[attempt]):regenerator=attempt;entities[attempt]['tapped']=True;entities[attempt]['attacking']=False;mana[entities[attempt]['player']][1 if entities[attempt]['name']=='Drudge Skeletons' else 3]-=1
    actions.append(step('resolve'));actions.append(step('assert_unused_choices',value=1) if attempt and not regenerator else step('assert_choices_consumed'))
    if not protected:effect(regenerator)
    if v=='stacked_activations':effect()
   if v=='repeat_denied':actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
   if v=='cleanup':actions.extend([step('cleanup'),step('refresh')])
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref,e in entities.items():
   if e['alive']:
    checks.extend([observe(ref,'in_play',1),observe(ref,'tapped',int(e['tapped']))])
    if 'Creature' in o[e['name']]['type_line']:checks.extend(observe(ref,k,int(o[e['name']][k])) for k in ('power','toughness'))
  all_names=set(PINS)|{name}
  for pl in (0,1):
   checks.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))])
   checks.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
   for nm in sorted(all_names):checks.append(step('assert_zone_multiplicity',player=pl,zone='battlefield',index=rows[nm]['csvid'],value=sum(e['alive'] and e['player']==pl and e['name']==nm for e in entities.values())))
   for zone,names in zones.items():
    n=len(names[pl])+unknown[pl]*(1 if zone=='graveyard' else -1 if zone=='hand' else 0);checks.append(step('assert_zone',player=pl,zone=zone,value=n))
    for nm in sorted(all_names):
     if unknown[pl] and zone=='graveyard':continue
     op='assert_zone_multiplicity_max' if unknown[pl] and zone=='hand' else 'assert_zone_multiplicity';checks.append(step(op,player=pl,zone=zone,index=rows[nm]['csvid'],value=names[pl].count(nm)))
    if zone=='library':checks.extend(step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(names[pl]))
   for nm in sorted(all_names):checks.append(step('assert_hand_graveyard_total',player=pl,index=rows[nm]['csvid'],value=zones['hand'][pl].count(nm)+zones['graveyard'][pl].count(nm)))
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
   at=0 if v=='ready_snapshot' else len(actions) if v=='resolved_snapshot' else cut
   steps=pre+actions[:at]+[step('capture')]+actions[at:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[at:]+checks+[step('assert_recorded_state')]
  result.append(dict(name=f'MassRemovalRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family='mass_removal',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_rules=p['untested_rules'],untested_keywords=p['keywords'],untested_components=['alternative abilities/costs','late arrivals and state changes while pending','triggered/replacement effects','control changes during resolution','multiplayer'])))
 return result
