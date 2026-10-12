"""Explicit mana outputs with independently pinned damage/life consequences."""
import copy,re
from collections import Counter
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from mana_creature_recipes import output_pool,ANY_COLOR,WITNESSES,HULK
from permanent_mana_recipes import validate_witnesses as mana_witnesses

SAFE_PASSAGE=('{2}{W}','Prevent all damage that would be dealt to you and creatures you control this turn.')

def spec(d,r):
 from recipes import cost_from_oracle
 if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or
     r.get('type_line') not in ('Land','Artifact') or r.get('keywords') or
     d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('whole normal noncreature land/artifact Oracle pin required')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'production','outputs','colored_damage','fixed_damage','life_payment','opponent_life','enters_tapped'}:
  raise ValueError('explicit outputs, production, entry and each life consequence required')
 if (not isinstance(p['production'],str) or not isinstance(p['outputs'],list) or not p['outputs'] or
     any(type(p[k]) is not int or not 0<=p[k]<=3 for k in ('colored_damage','fixed_damage','life_payment','opponent_life')) or
     type(p['enters_tapped']) is not bool or sum(bool(p[k]) for k in ('colored_damage','fixed_damage','life_payment','opponent_life'))!=1):
  raise ValueError('one explicit bounded life consequence required')
 outputs=p['outputs'];pools=[output_pool(o) for o in outputs]
 if len(set(outputs))!=len(outputs):raise ValueError('unique outputs required')
 form=p['production'];kind='land' if r['type_line']=='Land' else 'artifact'
 lines=[f'This {kind} enters tapped.'] if p['enters_tapped'] else []
 if p['enters_tapped'] and kind!='land':raise ValueError('tapped-entry artifacts unreviewed')
 damage=[0]*len(outputs)
 if form in ('colorless_choice','colorless_any'):
  if outputs[0]!='{C}' or p['fixed_damage'] or p['life_payment']:raise ValueError('ordinary colorless and colored modes required')
  if form=='colorless_choice':
   if len(outputs)!=3 or any(o not in ANY_COLOR for o in outputs[1:]):raise ValueError('two distinct colored alternatives required')
   wording=' or '.join(outputs[1:])
  else:
   if outputs[1:]!=ANY_COLOR:raise ValueError('all five explicit colors required')
   wording='one mana of any color'
  lines+=['{T}: Add {C}.','{T}: Add '+wording+'.'+
    (f' This {kind} deals {p["colored_damage"]} damage to you.' if p['colored_damage'] else f' Each opponent gains {p["opponent_life"]} life.')]
  damage=[0]+[p['colored_damage']]*(len(outputs)-1)
 elif form=='fixed':
  if len(outputs)!=1 or not p['fixed_damage'] or any(p[k] for k in ('colored_damage','life_payment','opponent_life')):
   raise ValueError('one fixed damage-producing output required')
  lines+=['{T}: Add '+outputs[0]+f'. This {kind} deals {p["fixed_damage"]} damage to you.'];damage=[p['fixed_damage']]
 elif form=='any_color':
  if outputs!=ANY_COLOR or not p['life_payment'] or any(p[k] for k in ('colored_damage','fixed_damage','opponent_life')):
   raise ValueError('life cost and all five ordinary outputs required')
  lines+=[f'{{T}}, Pay {p["life_payment"]} life: Add one mana of any color.']
 else:raise ValueError('unreviewed mana production form')
 if r['oracle_text'].splitlines()!=lines:raise ValueError('extra clauses or mismatched complete Oracle text')
 return dict(p,family='mana_consequence',pools=pools,damages=damage,choice=form!='fixed',land=kind=='land',
  mana=None if kind=='land' else cost_from_oracle(r),gains=[0 if i==0 and form.startswith('colorless_') else p['opponent_life'] for i in range(len(outputs))])

def variants(p):
 common=['entry','tapped','sick','opposing_turn','combat','bare_zones','source_bounced','source_destroyed','incidental_tap','resolved_snapshot']
 if not p['land']:common+=['cast','cast_payment','insufficient_cast_mana','fresh_activation']
 result=[v+'_'+str(o) for o in (0,1) for v in common]
 for o in (0,1):
  for i,pool in enumerate(p['pools']):
   vs=['production','payment','repeat_denied','untap_repeat','two_sources','tax','prevention','low_life','ready_snapshot','mana_snapshot','spend_generic','retap_without_mana']
   if any(pool[1:]):vs+=['spend_colored']
   result += [v+'_'+str(i)+'_'+str(o) for v in vs]
 return result

def validate_witnesses(o,rows):
 mana_witnesses(o,rows)
 if 'Safe Passage' not in rows or (o.get('Safe Passage',{}).get('mana_cost'),o.get('Safe Passage',{}).get('oracle_text'))!=SAFE_PASSAGE:
  raise ValueError('mana prevention witness changed')

def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  chunks=variant.rsplit('_',2)
  if len(chunks)==3 and chunks[-2].isdigit():v,index,owner=chunks;index=int(index);owner=int(owner)
  else:v,owner=variant.rsplit('_',1);owner=int(owner);index=0
  bare=v=='bare_zones';casting=v in ('cast','cast_payment','insufficient_cast_mana','fresh_activation')
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exile=[[] if bare else list(EXILE) for _ in (0,1)]
  mana=[[20]*6,[20]*6];life=[20,20];setup=[];entities={};pre=[step('turn',player=owner,value=0)];actions=[]
  def add(nm,ref,player,zone='battlefield',state=None):
   item=card(nm,ref,player,zone)
   if state:item['state']=state
   setup.append(item);r=oracle[nm]
   entities[ref]=dict(name=nm,player=player,alive=zone=='battlefield',tapped=bool(state and 'tapped' in state) or zone=='battlefield' and nm==name and p['enters_tapped'],creature='Creature' in r['type_line'])
   if zone=='hand':hands[player].append(nm)
  def pay(pl,cost):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  def move(ref,zone):
   e=entities[ref];e['alive']=False;(hands if zone=='hand' else graves)[e['player']].append(e['name'])
  def cast(ref,target=None,dialog=None):
   e=entities[ref];s=([step('choose_target',player=e['player'],ref=target)] if target else [])+[step('cast',ref=ref,value=1)]
   if dialog is not None:s.append(step('choose_dialog',player=e['player'],value=dialog))
   s += [step('resolve'),step('assert_choices_consumed')];pay(e['player'],cost_from_oracle(oracle[e['name']]));hands[e['player']].remove(e['name'])
   if 'Instant' in oracle[e['name']]['type_line']:graves[e['player']].append(e['name'])
   else:e['alive']=True
   return s
  def spell(nm,ref,target=None,dialog=None):add(nm,ref,owner,'hand');return cast(ref,target,dialog)
  def untap(ref,label):
   s=spell('Twiddle',label,ref,1);entities[ref]['tapped']=False;return s+[observe(ref,'tapped',0)]
  add(name,'source',owner,'hand' if casting else 'battlefield',['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
  pre += [step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)]
  if v=='two_sources':add(name,'second',owner)
  if v=='tax':pre+=spell('Suppression Field','taxer')
  if casting:
   if v in ('cast_payment','insufficient_cast_mana'):
    mana[owner]=list(p['mana']) if v=='cast_payment' else [0]*6;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   if v=='insufficient_cast_mana':pre += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
   else:pre+=cast('source')+[step('refresh')]
  if not casting and p['enters_tapped'] and v not in ('entry','tapped'):
   pre+=untap('source','entry_untap')
   if v=='two_sources':pre+=untap('second','second_untap')
  if v=='opposing_turn':pre.append(step('turn',player=1-owner,value=0))
  if v=='combat':
   add('Savannah Lions','attacker',1-owner)
   pre += [step('turn',player=1-owner,value=0),step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat')]
   entities['attacker']['tapped']=True
  if v=='prevention':pre+=spell('Safe Passage','prevent')
  if v=='payment':
   mana[owner]=[0]*6;pre += [step('set_mana',player=owner,color=c,value=0) for c in range(6)]
  if v=='low_life':
   life[owner]=max(p['life_payment'],p['damages'][index])+1;pre.append(step('set_life',player=owner,value=life[owner]))
  if v in ('spend_generic','spend_colored'):
   mana[owner]=[6-sum(p['pools'][index]),0,0,0,0,0] if v=='spend_generic' else [0]*6
   pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   if v=='spend_generic':add(HULK[0],'spender',owner,'hand')
   else:
    for c,n in enumerate(p['pools'][index]):
     for j in range(n):add(WITNESSES['{'+'CBUGRW'[c]+'}'][0],f'spender_{c}_{j}',owner,'hand')
  def inspect(ref):
   e=entities[ref];s=[observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    s.append(observe(ref,'tapped',int(e['tapped'])))
    if e['creature']:
     r=oracle[e['name']];s += [observe(ref,'power',int(r['power'])),observe(ref,'toughness',int(r['toughness'])),observe(ref,'damage',0)]
   return s
  def activate(ref='source'):
   s=[step('require_can_activate',ref=ref,value=1)]
   if p['choice']:s.append(step('choose_color',player=owner,value=next(c for c,n in enumerate(p['pools'][index]) if n)))
   s += [step('activate',ref=ref,value=1),step('assert_choices_consumed'),step('assert_stack',value=0)]
   entities[ref]['tapped']=True;mana[owner]=[a+b for a,b in zip(mana[owner],p['pools'][index])]
   life[owner]-=p['life_payment']+(0 if v=='prevention' else p['damages'][index]);life[1-owner]+=p['gains'][index]
   return s+inspect(ref)+[step('assert_life',player=pl,value=life[pl]) for pl in (0,1)]+[step('assert_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  ready=after=0
  if v in ('cast','cast_payment','insufficient_cast_mana'):pass
  elif v in ('entry','tapped'):
   can=not entities['source']['tapped'];actions += [step('require_can_activate',ref='source',value=int(can))]
   if not can:actions += [step('activate',ref='source',value=0),step('assert_choices_consumed')]
  elif v=='incidental_tap':
   actions += spell('Twiddle','incidental','source',0);entities['source']['tapped']=True
  else:
   ready=len(actions);actions += activate();after=len(actions)
   if v=='repeat_denied':actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
   if v=='two_sources':actions += activate('second')
   if v in ('untap_repeat','retap_without_mana'):
    actions += untap('source','repeat_untap')
    if v=='untap_repeat':actions += activate()
    else:actions += spell('Twiddle','retap','source',0);entities['source']['tapped']=True
   if v in ('spend_generic','spend_colored'):
    if v=='spend_generic' and owner==0:actions += [step('choose_mana',player=owner,color=c) for c in range(1,6) for _ in range(p['pools'][index][c])]
    for ref in [ref for ref in entities if ref.startswith('spender')]:actions+=cast(ref)
    if v=='spend_generic':mana[owner]=[0]*6
   if v=='source_bounced':actions += spell('Boomerang','bounce','source');move('source','hand')
   if v=='source_destroyed':actions += [step('destroy',ref='source')];move('source','graveyard')
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref in entities:checks+=inspect(ref)
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
  for pl in (0,1):
   checks += [step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))]
   checks += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for zone,names in (('hand',hands[pl]),('graveyard',graves[pl]),('exile',exile[pl]),('library',libs[pl])):
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(names)))
    if zone=='library':checks += [step('assert_zone_card',player=pl,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=registry[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  steps=pre+actions+checks
  if v in ('ready_snapshot','mana_snapshot'):
   cut=ready if v=='ready_snapshot' else after;tail=actions[cut:]+checks
   steps=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'ManaConsequenceRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='mana_consequence',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['land play permissions','multiplayer','lethal life totals','automatic mana payment source selection','damage redirection','additional clauses or costs'])))
 return result
