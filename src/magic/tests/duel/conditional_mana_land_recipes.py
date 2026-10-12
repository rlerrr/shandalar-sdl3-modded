"""Conditional land entry tested on ordered, real-engine board fixtures."""
import copy
from collections import Counter
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from mana_creature_recipes import output_pool,ANY_COLOR,WITNESSES,HULK
from permanent_mana_recipes import validate_witnesses as mana_witnesses

BASIC=('Plains','Island','Swamp','Mountain','Forest')
DUALS={frozenset(types):name for types,name in (
 (('Plains','Island'),'Tundra'),(('Island','Swamp'),'Underground Sea'),(('Swamp','Mountain'),'Badlands'),
 (('Mountain','Forest'),'Taiga'),(('Forest','Plains'),'Savannah'),(('Plains','Swamp'),'Scrubland'),
 (('Island','Mountain'),'Volcanic Island'),(('Swamp','Forest'),'Bayou'),(('Mountain','Plains'),'Plateau'),
 (('Forest','Island'),'Tropical Island'))}

def spec(d,r):
 if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r.get('keywords') or
     d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('whole normal single-face land Oracle pin and explicit variants required')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'outputs','condition','limit','types'}:
  raise ValueError('explicit outputs, entry condition, threshold and qualifying types required')
 if (not isinstance(p['outputs'],list) or len(p['outputs'])!=2 or any(not isinstance(o,str) for o in p['outputs']) or len(set(p['outputs']))!=2 or
     any(o not in ANY_COLOR for o in p['outputs']) or not isinstance(p['condition'],str) or
     type(p['limit']) is not int or not isinstance(p['types'],list) or any(not isinstance(t,str) for t in p['types'])):
  raise ValueError('two distinct ordinary colored outputs and typed entry parameters required')
 mana='{T}: Add '+' or '.join(p['outputs'])+'.';kind=p['condition']
 if kind=='at_most_other_lands':
  if p['limit']!=2 or p['types']!=[] or r.get('type_line')!='Land':raise ValueError('reviewed fast-land threshold required')
  lines=['This land enters tapped unless you control two or fewer other lands.',mana]
 elif kind=='at_least_basics':
  types=r.get('type_line','').removeprefix('Land — ').split()
  if p['limit']!=2 or p['types']!=[] or len(types)!=2 or frozenset(types) not in DUALS or r.get('type_line')!='Land — '+' '.join(types):raise ValueError('reviewed nonbasic land with two basic land types required')
  lines=['('+mana+')','This land enters tapped unless you control two or more basic lands.']
 elif kind=='land_types':
  if p['limit']!=0 or len(p['types'])!=2 or len(set(p['types']))!=2 or any(t not in BASIC for t in p['types']) or r.get('type_line')!='Land':raise ValueError('two explicit qualifying land subtypes required')
  words=[('an ' if t=='Island' else 'a ')+t for t in p['types']]
  lines=['This land enters tapped unless you control '+' or '.join(words)+'.',mana]
 else:raise ValueError('unreviewed entry predicate')
 if r['oracle_text'].splitlines()!=lines:raise ValueError('extra clauses or mismatched complete Oracle text')
 return dict(p,family='conditional_mana_land',pools=[output_pool(o) for o in p['outputs']])

def boards(p):
 base={'none':([],[],False),'nonlands':(['Sol Ring','Grizzly Bears',HULK[0]],[],False),
  'offboard':([],[],False)}
 if p['condition']=='at_most_other_lands':
  base.update({f'lands_{n}':(['Wasteland']*n,[],False) for n in range(1,5)})
  base.update(two_basics=(['Forest','Island'],[],False),three_basics=(['Forest','Island','Swamp'],[],False),
   opponent_only=([],['Wasteland']*3,False),late_lands=(['Wasteland']*3,[],True))
 elif p['condition']=='at_least_basics':
  base.update(one_basic=(['Forest'],[],False),two_basics=(['Forest','Island'],[],False),
   three_basics=(['Forest','Island','Swamp'],[],False),same_basics=(['Forest','Forest'],[],False),
   nonbasic_types=(['Tundra','Bayou'],[],False),mixed=(['Forest','Tundra'],[],False),
   opponent_only=([],['Forest','Island'],False),late_basics=(['Forest','Island'],[],True))
 else:
  a,b=p['types'];wrong=next(t for t in BASIC if t not in (a,b))
  base.update(first_type=([a],[],False),second_type=([b],[],False),both_types=([a,b],[],False),
   nonbasic_type=([DUALS[frozenset((a,b))]],[],False),wrong_type=([wrong],[],False),
   untyped_lands=(['Wasteland']*3,[],False),opponent_only=([],[a],False),late_type=([a],[],True))
 return base

def ready(p,names,oracle):
 lands=[oracle[n] for n in names if 'Land' in oracle[n]['type_line']]
 if p['condition']=='at_most_other_lands':return len(lands)<=p['limit']
 if p['condition']=='at_least_basics':return sum(r['type_line'].startswith('Basic Land') for r in lands)>=p['limit']
 return any(any(t in r['type_line'].split(' — ')[-1].split() for t in p['types']) for r in lands)

def default_board(p):return {'at_most_other_lands':'none','at_least_basics':'two_basics','land_types':'first_type'}[p['condition']]

def variants(p):
 result=[]
 for owner in (0,1):
  for board in boards(p):
   result += [f'entry_{board}_{owner}',f'entry_snapshot_{board}_{owner}']
   result += [f'production_{board}_{i}_{owner}' for i in range(2)]
  result += [f'{v}_{owner}' for v in ('tapped','sick','opposing_turn','combat','bare_zones','source_bounced','source_destroyed','incidental_tap','resolved_snapshot')]
  if p['condition']!='at_most_other_lands':result.append(f'qualifiers_removed_{owner}')
  if p['condition']=='at_most_other_lands':result += [f'{v}_{owner}' for v in ('remove_excess','sequential_boundary')]
  for i in range(2):
   result += [f'{v}_{i}_{owner}' for v in ('payment','repeat_denied','untap_repeat','two_sources','tax','ready_snapshot','mana_snapshot','spend_colored','spend_generic')]
 return result

def validate_witnesses(o,rows):
 mana_witnesses(o,rows)
 symbols=dict(zip(BASIC,('W','U','B','R','G')))
 for name in BASIC:
  r=o.get(name,{})
  if name not in rows or (r.get('type_line'),r.get('oracle_text'))!=('Basic Land — '+name,'({T}: Add {'+symbols[name]+'}.)'):raise ValueError('basic-land witness changed: '+name)
 for types,name in DUALS.items():
  r=o.get(name,{});line=r.get('type_line','')
  if name not in rows or not line.startswith('Land — ') or frozenset(line.split(' — ')[1].split())!=types:raise ValueError('nonbasic typed-land witness changed: '+name)
  order=line.split(' — ')[1].split();text='({T}: Add {'+symbols[order[0]]+'} or {'+symbols[order[1]]+'}.)'
  if r.get('oracle_text')!=text:raise ValueError('dual-land rules witness changed: '+name)
 pins={'Wasteland':('Land','{T}: Add {C}.\n{T}, Sacrifice this land: Destroy target nonbasic land.'),
  'Sol Ring':('Artifact','{T}: Add {C}{C}.'),'Grizzly Bears':('Creature — Bear','')}
 for name,(type_line,text) in pins.items():
  r=o.get(name,{})
  if name not in rows or (r.get('type_line'),r.get('oracle_text'))!=(type_line,text):raise ValueError('entry-board witness changed: '+name)

def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import card,step,observe,cost_from_oracle
 result=[];board_map=boards(p)
 for variant in selected:
  owner=int(variant.rsplit('_',1)[1]);raw=variant.rsplit('_',1)[0];index=0;board=default_board(p)
  if raw.startswith('entry_snapshot_'):v='entry_snapshot';board=raw.removeprefix('entry_snapshot_')
  elif raw.startswith('entry_'):v='entry';board=raw.removeprefix('entry_')
  elif raw.startswith('production_'):v='production';board,i=raw.removeprefix('production_').rsplit('_',1);index=int(i)
  elif raw.rsplit('_',1)[-1].isdigit():v,i=raw.rsplit('_',1);index=int(i)
  else:v=raw
  if v=='remove_excess':board='lands_3'
  if v=='sequential_boundary':board='two_basics'
  own,enemy,late=board_map[board];bare=v=='bare_zones'
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exile=[[] if bare else list(EXILE) for _ in (0,1)]
  mana=[[20]*6,[20]*6];setup=[];entities={};pre=[step('turn',player=owner,value=0)];actions=[]
  def add(nm,ref,pl,zone='battlefield',tapped=False):
   item=card(nm,ref,pl,zone)
   if tapped:item['state']=['tapped']
   setup.append(item)
   r=oracle[nm];entities[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',tapped=tapped,creature='Creature' in r['type_line'])
   if zone!='battlefield':{'hand':hands,'graveyard':graves,'library':libs,'exile':exile}[zone][pl].append(nm)
  initial_ready=ready(p,[] if late else own,oracle)
  if late:add(name,'source',owner,tapped=not initial_ready)
  for pl,names in ((owner,own),(1-owner,enemy)):
   for i,nm in enumerate(names):add(nm,f'board_{pl}_{i}',pl)
  if board=='offboard':
   nm=p['types'][0] if p['condition']=='land_types' else 'Forest'
   for zone in ('hand','graveyard','exile','library'):add(nm,'outside_'+zone,owner,zone)
  if not late:add(name,'source',owner,tapped=v=='tapped' or not initial_ready)
  if v=='sick':next(c for c in setup if c['ref']=='source')['state']=['sick']
  if v in ('two_sources','sequential_boundary'):
   second_ready=ready(p,own+[name],oracle);add(name,'second',owner,tapped=not second_ready)
  # Entry flags are expectations, not overrides: ordinary cases leave source
  # fixture state unset so the engine's replacement effect decides tapping.
  for c in setup:
   if c['name']==name and v!='tapped' and 'state' in c and c['state']==['tapped']:c.pop('state')
  pre += [step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)]
  def pay(pl,cost):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  def move(ref,zone):
   e=entities[ref];e['alive']=False;(hands if zone=='hand' else graves)[e['player']].append(e['name'])
  def spell(nm,ref,target=None,dialog=None):
   add(nm,ref,owner,'hand');seq=([step('choose_target',player=owner,ref=target)] if target else [])+[step('cast',ref=ref,value=1)]
   if dialog is not None:seq.append(step('choose_dialog',player=owner,value=dialog))
   seq += [step('resolve'),step('assert_choices_consumed')];pay(owner,cost_from_oracle(oracle[nm]));hands[owner].remove(nm)
   if 'Instant' in oracle[nm]['type_line']:graves[owner].append(nm)
   else:entities[ref]['alive']=True
   return seq
  def untap(ref,label):
   seq=spell('Twiddle',label,ref,1);entities[ref]['tapped']=False;return seq+[observe(ref,'tapped',0)]
  def inspect(ref):
   e=entities[ref];s=[observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    s.append(observe(ref,'tapped',int(e['tapped'])))
    if e['creature']:
     r=oracle[e['name']];s += [observe(ref,'power',int(r['power'])),observe(ref,'toughness',int(r['toughness'])),observe(ref,'damage',0)]
   return s
  pre += [observe('source','tapped',int(entities['source']['tapped'])),step('require_can_activate',ref='source',value=int(not entities['source']['tapped']))]
  if v not in ('entry','entry_snapshot','tapped','remove_excess') and entities['source']['tapped']:pre+=untap('source','entry_untap')
  if v=='opposing_turn':pre.append(step('turn',player=1-owner,value=0))
  if v=='combat':
   add('Savannah Lions','attacker',1-owner);pre += [step('turn',player=1-owner,value=0),step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat')];entities['attacker']['tapped']=True
  if v=='tax':pre+=spell('Suppression Field','field')
  if v=='payment':
   mana[owner]=[0]*6;pre += [step('set_mana',player=owner,color=c,value=0) for c in range(6)]
  if v in ('spend_generic','spend_colored'):
   mana[owner]=[5,0,0,0,0,0] if v=='spend_generic' else [0]*6;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   color=next(c for c,n in enumerate(p['pools'][index]) if n);nm=HULK[0] if v=='spend_generic' else WITNESSES[p['outputs'][index]][0];add(nm,'spender',owner,'hand')
  def activate(ref='source'):
   seq=[step('require_can_activate',ref=ref,value=1),step('choose_color',player=owner,value=next(c for c,n in enumerate(p['pools'][index]) if n)),step('activate',ref=ref,value=1),step('assert_choices_consumed'),step('assert_stack',value=0)]
   entities[ref]['tapped']=True;mana[owner]=[a+b for a,b in zip(mana[owner],p['pools'][index])]
   return seq+inspect(ref)+[step('assert_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  ready_cut=after=0
  if v in ('entry','entry_snapshot'):pass
  elif v=='tapped':actions += [step('activate',ref='source',value=0),step('assert_choices_consumed')]
  elif v in ('qualifiers_removed','remove_excess'):
   refs=[ref for ref,e in entities.items() if ref.startswith('board_') and e['player']==owner and 'Land' in oracle[e['name']]['type_line']]
   if v=='remove_excess':refs=refs[:1]
   for ref in refs:actions+=[step('destroy',ref=ref)];move(ref,'graveyard')
   actions += [step('refresh'),step('require_can_activate',ref='source',value=int(not entities['source']['tapped']))]
  elif v=='incidental_tap':actions += spell('Twiddle','incidental','source',0);entities['source']['tapped']=True
  elif v=='sequential_boundary':actions += inspect('second')+[step('require_can_activate',ref='second',value=0)]
  else:
   ready_cut=len(actions);actions+=activate();after=len(actions)
   if v=='repeat_denied':actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
   if v=='untap_repeat':actions += untap('source','repeat_untap')+activate()
   if v=='two_sources':actions += activate('second')
   if v in ('spend_generic','spend_colored'):
    if v=='spend_generic' and owner==0:actions.append(step('choose_mana',player=owner,color=next(c for c,n in enumerate(p['pools'][index]) if n)))
    actions += [step('cast',ref='spender',value=1),step('resolve'),step('assert_choices_consumed')];hands[owner].remove(entities['spender']['name']);entities['spender']['alive']=True;mana[owner]=[0]*6
   if v=='source_bounced':actions += spell('Boomerang','bounce','source');move('source','hand')
   if v=='source_destroyed':actions += [step('destroy',ref='source')];move('source','graveyard')
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref,e in entities.items():
   if ref.startswith('outside_'):continue
   checks+=inspect(ref)
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
  for pl in (0,1):
   checks += [step('assert_life',player=pl,value=20)]+[step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   battle=[e['name'] for e in entities.values() if e['alive'] and e['player']==pl]
   for zone,names in (('battlefield',battle),('hand',hands[pl]),('graveyard',graves[pl]),('exile',exile[pl]),('library',libs[pl])):
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(names)))
    if zone=='library':checks += [step('assert_zone_card',player=pl,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=registry[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  steps=pre+actions+checks
  if v in ('ready_snapshot','mana_snapshot'):
   cut=ready_cut if v=='ready_snapshot' else after;tail=actions[cut:]+checks
   steps=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if v in ('entry_snapshot','resolved_snapshot'):steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'ConditionalManaLandRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='conditional_mana_land',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['land play permissions','re-entry and control changes','type-changing effects','other entry replacement effects','automatic mana payment source selection','multiplayer'])))
 return result
