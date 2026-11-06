"""Whole-Oracle reanimation components, with real casts and independent zone receipts.

Extra rules are an enumerated, text-pinned component boundary. They never widen
the graveyard/type/value predicate or silently claim coverage of another mode.
"""
import json
import re
from collections import Counter
from pathlib import Path

PINS=json.loads((Path(__file__).parent/'data/reanimation_witnesses.json').read_text(encoding='utf8'))['cards']
WITNESSES=tuple(n for n in PINS if n not in ('Reclaim','Counterspell'))
HAND=('Plains','Giant Growth')
LIBRARY=('Forest','Divination','Island','Lightning Bolt')
EXILE=('Mountain','Giant Growth')
SCOPES={'creature':'creature','artifact_or_creature':'artifact or creature','permanent':'permanent',
 'enchantment':'enchantment','artifact_or_enchantment':'artifact and enchantment',
 'basic_land':'basic land','land':'land','nightstalker_permanent':'Nightstalker permanent','zombie_creature':'Zombie creature'}
EXTRAS={'convoke','beacon_shuffle','indestructible_scry','zombie','swamps','humans','rally_exile','rebound','aura_self','aura_all'}


def eligible(r,p,x=3,ignore=None):
 types=set(r['type_line'].partition(' — ')[0].split());subs=set(r['type_line'].partition(' — ')[2].split())
 scope=p['scope']
 valid={'creature':'Creature' in types,'artifact_or_creature':bool(types&{'Artifact','Creature'}),
 'permanent':bool(types&{'Artifact','Creature','Enchantment','Land','Planeswalker','Battle'}),
 'enchantment':'Enchantment' in types,'artifact_or_enchantment':bool(types&{'Artifact','Enchantment'}),
 'basic_land':{'Basic','Land'}<=types,'land':'Land' in types,
 'nightstalker_permanent':'Nightstalker' in subs and bool(types&{'Artifact','Creature','Enchantment','Land','Planeswalker','Battle'}),
 'zombie_creature':'Creature' in types and ('Zombie' in subs or 'Changeling' in r.get('keywords',[]))}[scope]
 limit=p['limit']
 return (valid or ignore=='type') and (ignore=='value' or limit is None or r['cmc']<=(x if limit=='x_le' else limit))


def extra_text(key,name):
 return {
 'convoke':"Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for {1} or one mana of that creature's color.)",
 'beacon_shuffle':f"Shuffle {name} into its owner's library.",
 'indestructible_scry':"It gains indestructible. If it's your turn, scry 2.",
 'zombie':'That creature is a black Zombie in addition to its other colors and types.',
 'swamps':'Then destroy all Swamps you control.',
 'humans':'then destroy all Humans.',
 'rally_exile':f'Exile those creatures at the beginning of your next upkeep. Exile {name}.',
 'rebound':'Rebound (If you cast this spell from your hand, exile it as it resolves. At the beginning of your next upkeep, you may cast this card from exile without paying its mana cost.)',
 'aura_self':'(Auras with nothing to enchant remain in your graveyard.)',
 'aura_all':'(Auras with nothing to enchant remain in graveyards.)',
 }[key]


def primary_texts(p):
 noun=SCOPES[p['scope']];suffix=' tapped' if p['tapped'] else ''
 if p['mode']=='threshold':
  return {'Return target creature card from your graveyard to your hand.\nThreshold — Return that card from your graveyard to the battlefield instead if there are seven or more cards in your graveyard.'}
 if p['mode']=='single':
  noun+=' card'
  if p['limit'] is not None:noun+=' with mana value '+('X' if p['limit']=='x_le' else str(p['limit']))+' or less'
  grave={'self':'your','opponent':"an opponent's",'any':'a'}[p['graveyard']]
  if p['history']:
   return {f'Choose target {noun} in a graveyard that was put there from the battlefield this turn. Put that card onto the battlefield under your control.'}
  return {f'Put target {noun} from {grave} graveyard onto the battlefield{suffix} under your control.',f'Return target {noun} from {grave} graveyard to the battlefield{suffix}.'}
 noun+=' card' if p['limit'] is not None else ' cards'
 if p['limit'] is not None:noun+=' with mana value X or less'
 each='each' if p['limit'] is not None else 'all'
 grave='all graveyards' if p['graveyard']=='all' else 'your graveyard'
 if p['history']:
  if p['graveyard']=='all':return {'Put onto the battlefield under your control all creature cards in all graveyards that were put there from the battlefield this turn.'}
  return {'Return to the battlefield all permanent cards in your graveyard that were put there from the battlefield this turn.'}
 if p['control']=='owner':return {f"Return all {noun} from {grave} to the battlefield{suffix} under their owners' control."}
 return {f'Return {each} {noun} from {grave} to the battlefield{suffix}.',f'Put all {noun} from {grave} onto the battlefield under your control.'}


def spec(d,r):
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r.get('type_line') not in ('Instant','Sorcery') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('reanimation requires a whole pinned normal instant/sorcery and explicit variants')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'mode','scope','graveyard','control','tapped','limit','history','self_destination','extras','legacy_x_payment_prompt'}:raise ValueError('explicit reanimation predicates, entry and source destinations required')
 if any(not isinstance(p[k],str) for k in ('mode','scope','graveyard','control','self_destination')):raise ValueError('reanimation modes and predicates require strings')
 if p['mode'] not in ('single','mass','threshold') or p['scope'] not in SCOPES or p['graveyard'] not in ('self','opponent','any','all') or p['control'] not in ('self','owner') or p['self_destination'] not in ('graveyard','library_shuffle','exile'):raise ValueError('unreviewed reanimation mode or destination')
 if any(type(p[k]) is not bool for k in ('tapped','history','legacy_x_payment_prompt')) or p['limit'] is not None and not (type(p['limit']) is int and 0<=p['limit']<=8 or p['limit']=='x_le'):raise ValueError('explicit tapped/history, input policy and bounded value limit required')
 if not isinstance(p['extras'],list) or any(not isinstance(k,str) or k not in EXTRAS for k in p['extras']) or p['extras']!=sorted(set(p['extras'])):raise ValueError('explicit ordered reviewed additional component list required')
 if p['mode']!='mass' and (p['graveyard']=='all' or p['control']=='owner') or p['mode']=='mass' and p['graveyard'] not in ('self','all'):raise ValueError('unreviewed graveyard/controller combination')
 if p['mode']=='threshold' and (p['scope']!='creature' or p['graveyard']!='self' or p['limit'] is not None or p['extras'] or p['history'] or p['tapped']):raise ValueError('only ordinary creature threshold reviewed')
 if p['history'] and (p['mode']=='threshold' or p['scope'] not in ('creature','permanent') or p['limit'] is not None or p['extras'] or p['tapped']):raise ValueError('unreviewed historical reanimation predicate')
 if (p['self_destination']=='library_shuffle')!=('beacon_shuffle' in p['extras']) or (p['self_destination']=='exile')!=bool(set(p['extras'])&{'rally_exile','rebound'}):raise ValueError('source destination must match the printed additional component')
 text=r['oracle_text']
 for key in p['extras']:
  part=extra_text(key,r['name'])
  if text.count(part)!=1:raise ValueError('additional component differs from whole pinned Oracle')
  text=text.replace(part,'')
 text=' '.join(text.split())
 # Zombie Apocalypse joins its two sequential instructions with a comma.
 if 'humans' in p['extras'] and text.endswith(','):text=text[:-1]+'.'
 if text not in {' '.join(t.split()) for t in primary_texts(p)}:raise ValueError('reanimation parameters disagree with the complete primary Oracle component')
 keywords={'Convoke'} if 'convoke' in p['extras'] else {'Rebound'} if 'rebound' in p['extras'] else set()
 if 'indestructible_scry' in p['extras']:keywords.add('Scry')
 if p['mode']=='threshold':keywords.add('Threshold')
 if set(r.get('keywords',[]))!=keywords:raise ValueError('unreviewed source keyword component')
 cost=r.get('mana_cost','');variable='{X}' in cost
 if variable!=(p['limit']=='x_le') or variable and cost.count('{X}')!=1:raise ValueError('one X symbol must match the printed value limit')
 if p['legacy_x_payment_prompt'] and not (variable and p['mode']=='single'):raise ValueError('legacy repeated X payment presentation requires a variable single-target spell')
 mana=cost_from_oracle(dict(mana_cost=cost.replace('{X}','')))
 gaps=[]
 if 'convoke' in p['extras']:gaps.append(extra_text('convoke',r['name']))
 if 'indestructible_scry' in p['extras']:gaps.append("If it's your turn, scry 2. (Cases cast on the opponent's turn.)")
 if 'zombie' in p['extras']:gaps.append(extra_text('zombie',r['name']))
 if 'rally_exile' in p['extras']:gaps.append('Exile those creatures at the beginning of your next upkeep.')
 if 'rebound' in p['extras']:gaps.append('Rebound next-upkeep free recast; initial hand-cast exile is checked.')
 if set(p['extras'])&{'aura_self','aura_all'}:gaps.append('Aura attachment and Auras without legal recipients')
 if p['scope'] in ('zombie_creature','nightstalker_permanent'):gaps.append('Changeling and live subtype changes')
 if p['history'] and p['mode']=='single':gaps.append('Selecting among newly dead candidates in both graveyards at once')
 out=dict(p,family='reanimation',mana=mana,variable=variable,instant=r['type_line']=='Instant',untested=gaps)
 selected=d['variants']
 if not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or len(selected)!=len(set(selected)) or set(selected)-set(variants(out)):raise ValueError('explicit distinct supported reanimation variants required')
 if variable and p['mode']=='single' and all(v.endswith('_0') for v in selected):
  out['untested']=gaps+['Controller 1 casting scenarios are pending: native SEH and Wasm input failure; see data/pending_cases/large-batch-1/07567-stir-the-grave-ai.json.']
 return out


def variants(p):
 modes=['effect','payment','insufficient_mana','wrong_color','empty','wrong_type','countered','cleanup','repeat','ready_snapshot','pending_snapshot','resolved_snapshot','bare_zones']
 if p['mode']!='mass':modes+=['alternate','lost_target','death_owner']
 if p['graveyard'] in ('self','opponent'):modes.append('wrong_owner')
 else:modes.append('opponent_graveyard')
 if p['limit'] is not None:modes.append('wrong_value')
 if p['mode']=='threshold':modes += ['below_threshold','threshold_six','threshold_seven']
 if p['history']:modes+=['old_graveyard','noncreature_history']
 if 'indestructible_scry' in p['extras']:modes.remove('death_owner');modes.append('indestructible')
 out=[f'{v}_{owner}' for owner in (0,1) for v in modes]
 if p['variable']:out += [f'{v}_x{x}_{owner}' for owner in (0,1) for x in (0,1,2,5,8) for v in ('effect','payment')]
 return out


def validate_witnesses(o,rows):
 for n,pin in PINS.items():
  if n not in rows or any(o.get(n,{}).get(k)!=v for k,v in pin.items()) or o[n].get('card_faces') or o[n].get('selected_face'):raise ValueError('reanimation witness Oracle characteristics changed: '+n)


def expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);x=3
  if '_x' in v:v,tail=v.rsplit('_x',1);x=int(tail)
  bare=v=='bare_zones';zones={z:[[],[]] for z in ('hand','graveyard','library','exile')}
  for pl in (0,1):
   zones['hand'][pl]=[] if bare else list(HAND);zones['library'][pl]=[] if bare else list(LIBRARY);zones['exile'][pl]=[] if bare else list(EXILE)
   zones['graveyard'][pl]=list(WITNESSES)
  eligible_names=[n for n in WITNESSES if eligible(o[n],p,x)]
  if not eligible_names and p['limit']!='x_le':raise ValueError('reanimation needs an eligible independent witness')
  target=(eligible_names[-1] if v=='alternate' else eligible_names[0]) if eligible_names else None
  if p['legacy_x_payment_prompt'] and v=='payment' and owner==0:
   # The zero-value legal control also makes the exhausted-pool boundary
   # independent of the legacy handler's extra variable payment presentation.
   target=next(n for n in eligible_names if o[n]['cmc']==0)
  grave_owner=1-owner if p['graveyard']=='opponent' or v=='opponent_graveyard' else owner
  if v=='below_threshold':zones['graveyard'][owner]=['Grizzly Bears','Hill Giant','Lightning Bolt','Giant Growth','Divination'];target='Grizzly Bears'
  if v in ('threshold_six','threshold_seven'):
   zones['graveyard'][owner]=['Grizzly Bears','Hill Giant','Lightning Bolt','Giant Growth','Divination','Forest']+(['Plains'] if v=='threshold_seven' else []);target='Grizzly Bears'
  if v=='empty':zones['graveyard']=[[],[]]
  if v in ('wrong_type','wrong_value','noncreature_history'):
   wrong=[n for n in WITNESSES if not eligible(o[n],p,x) and (v!='wrong_value' or eligible(o[n],p,x,'value'))]
   if v=='wrong_type' and p['variable'] and p['mode']=='single':wrong=[n for n in WITNESSES if not eligible(o[n],p,x,'value')]
   if v=='noncreature_history':wrong=[n for n in WITNESSES if 'Creature' not in o[n]['type_line']]
   zones['graveyard']=[list(wrong),list(wrong)]
  if v=='wrong_owner':zones['graveyard'][grave_owner]=[]
  if bare:
   zones['graveyard']=[[],[]];zones['graveyard'][grave_owner]=list(eligible_names[:2])
  setup=[];pre=([step('reset_death_history')] if p['history'] else [])+[step('turn',player=1-owner if 'indestructible_scry' in p['extras'] else owner,value=0)];actions=[];entities=[]
  if p['history']:pre.append(step('can_cast',ref='source',value=int(p['mode']=='mass')))
  history=[]
  if p['history'] and v not in ('empty','wrong_type','wrong_owner','old_graveyard','below_threshold'):
   # Only these cards genuinely died this turn; all other stocked cards are old.
   for pl in (0,1):
    if p['mode']=='single' and pl!=grave_owner:continue
    for nm in (('Sol Ring','Forest') if v=='noncreature_history' else ('Grizzly Bears','Hill Giant','Sol Ring','Forest')):
     zones['graveyard'][pl]=[n for n in zones['graveyard'][pl] if n!=nm]
     ref=f'history_{pl}_{len(history)}';setup.append(card(nm,ref,pl));pre.append(step('destroy',ref=ref));zones['graveyard'][pl].append(nm);history.append((pl,nm))
   if p['scope']=='creature':target='Grizzly Bears'
  if p['history']:pre.append(step('can_cast',ref='source',value=int(p['mode']=='mass' or any(eligible(o[n],p,x) for _,n in history))))
  for pl in (0,1):
   for z in zones:
    names=list(zones[z][pl])
    if z=='graveyard' and p['history']:
     for hpl,nm in history:
      if hpl==pl:names.remove(nm)
    setup += [card(n,f'p{pl}_{z}_{i}',pl,z) for i,n in enumerate(names)]
  def addspell(n,ref,pl):setup.append(card(n,ref,pl,'hand'));zones['hand'][pl].append(n)
  addspell(name,'source',owner)
  # Unrelated battlefield controls distinguish own/all-player secondary sweeps.
  if 'swamps' in p['extras'] or 'humans' in p['extras']:
   for pl in (0,1):
    for nm in (('Swamp','Forest') if 'swamps' in p['extras'] else ('Eager Cadet','Grizzly Bears')):
     ref=f'control_{pl}_{len(entities)}';setup.append(card(nm,ref,pl));entities.append(dict(name=nm,ref=ref,player=pl,owner=pl,alive=True,tapped=False,returned=False))
  mana=[[10]*6,[10]*6];cost=list(p['mana']);cost[0]+=x if p['variable'] else 0
  if v=='repeat':
   mana[owner]=[30]*6
   pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  if v in ('payment','insufficient_mana','wrong_color'):
   mana[owner]=list(cost) if v=='payment' else [0]*6
   if v=='wrong_color':mana[owner][0]=sum(cost)
   pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  def pay(cost,pl):
   # Full-pool generic allocations are free choices; only their total is asserted.
   for c,n in enumerate(cost):mana[pl][c]-=n
  def move_spell(n,pl,dest='graveyard'):
   zones['hand'][pl].remove(n);zones[dest][pl].append(n)
  def legal(pl,n):return eligible(o[n],p,x) and (not p['history'] or (pl,n) in history)
  def recipients():
   pools=range(2) if p['graveyard'] in ('all','any') else [grave_owner]
   return [(pl,n) for pl in pools for n in zones['graveyard'][pl] if legal(pl,n)]
  options=recipients()
  if p['mode']!='mass' and target is not None and (grave_owner,target) not in options and options:target=options[0][1];grave_owner=options[0][0]
  negative=p['mode']!='mass' and not options or v in ('insufficient_mana','wrong_color')
  source_hand=negative
  if v=='countered':addspell('Counterspell','counter',0)
  if v=='lost_target' and not negative:addspell('Reclaim','response',grave_owner)
  if v=='repeat':addspell(name,'source2',owner)
  def choices(n,pl):
   # Historical selectors present their already-filtered dead-card array in a
   # single chooser. Ordinary either-graveyard selectors ask for its player first.
   both=all(any(eligible(o[nm],p,x) for nm in zones['graveyard'][who]) for who in (0,1))
   return ([step('choose_player',player=owner,value=pl)] if p['graveyard']=='any' and not p['history'] and both else [])+[step('choose_zone_card',player=owner,value=rows[n]['csvid'])]
  def returned(pl,n,threshold_active):
   zones['graveyard'][pl].remove(n)
   if p['mode']=='threshold' and not threshold_active:
    zones['hand'][pl].append(n);return
   controller=pl if p['control']=='owner' else owner
   entities.append(dict(name=n,ref=f'returned_{len(entities)}',player=controller,owner=pl,alive=True,tapped=p['tapped'],returned=True))
  def complete(ref,n,pl):
   nonlocal source_hand,actions
   # A legal cast is a rules precondition. Stop this case on a rejected legal
   # source before attempting a response against a nonexistent pending spell.
   actions.append(step('require_can_cast',ref=ref,value=1))
   # Decisions share one ordered queue. Printed and handler-local variable
   # payments happen before the graveyard selector in the real cast path.
   if p['variable']:actions.append(step('choose_x',player=owner,value=x))
   # This legacy single-target X presentation asks for an additional variable payment
   # amount inside its real casting handler. Script the offered amount without
   # changing the Oracle one-X payment receipt. The AI path has no such prompt.
   if p['legacy_x_payment_prompt'] and owner==0:actions.append(step('choose_x',player=owner,value=0 if v=='payment' else x))
   if ref=='source2' and 'convoke' in p['extras']:actions.append(step('choose_dialog',player=owner,value=1))
   if p['mode']!='mass':actions.extend(choices(n,pl))
   if v=='countered' and ref=='source':actions.append(step('choose_priority',player=0,ref='counter'))
   if v=='lost_target':
    if pl==0:actions += [step('choose_priority',player=pl,ref='response'),step('choose_zone_card',player=pl,value=rows[n]['csvid'])]
    pay(cost_from_oracle(o['Reclaim']),pl);move_spell('Reclaim',pl);zones['graveyard'][pl].remove(n);zones['library'][pl].insert(0,n)
   actions.append(step('cast',ref=ref,value=0 if v=='countered' else 1));pay(cost,owner)
   if v!='countered':actions.append(step('assert_stack',value=1))
   if v=='lost_target' and pl==1:
    actions += [step('choose_zone_card',player=pl,value=rows[n]['csvid']),step('cast_response',ref='response',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_choices_consumed')]
   if v=='countered':pay(cost_from_oracle(o['Counterspell']),0);move_spell('Counterspell',0)
   success=v not in ('countered','lost_target')
   threshold_active=len(zones['graveyard'][pl])>=7
   dest=p['self_destination'] if success else 'graveyard'
   move_spell(name,owner,'library' if dest=='library_shuffle' else dest)
   source_hand=False
   actions += [step('resolve'),step('assert_choices_consumed')]
   if success:
    selected_cards=recipients() if p['mode']=='mass' else [(pl,n)]
    for frompl,nm in selected_cards:returned(frompl,nm,threshold_active)
    for e in entities:
     if e['alive'] and ('swamps' in p['extras'] and e['player']==owner and 'Swamp' in o[e['name']]['type_line'] or 'humans' in p['extras'] and 'Human' in o[e['name']]['type_line']):
      e['alive']=False;zones['graveyard'][e['owner']].append(e['name'])
  if negative:
   if v=='wrong_value' and p['variable']:
    actions.append(step('choose_x',player=owner,value=x))
    if p['legacy_x_payment_prompt'] and owner==0:actions.append(step('choose_x',player=owner,value=x))
    if p['mode']=='single':actions.append(step('choose_optional_zone',player=owner,value=rows[zones['graveyard'][grave_owner][0]]['csvid']))
   actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
  else:
   complete('source',target,grave_owner)
   if v=='repeat':
    if p['mode']=='mass':complete('source2',None,grave_owner)
    else:
     rem=recipients()
     if not rem:raise ValueError('repeated single reanimation requires second eligible witness')
     complete('source2',rem[-1][1],rem[-1][0])
   if v in ('death_owner','indestructible'):
    e=next(e for e in entities if e['returned']);actions += [step('bind_permanent',ref=e['ref'],player=e['player'],value=rows[e['name']]['csvid']),step('destroy',ref=e['ref'])]
    if v=='death_owner':e['alive']=False;zones['graveyard'][e['owner']].append(e['name'])
   if v=='cleanup':actions.append(step('cleanup'))
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0),observe('source','in_hand',int(source_hand))]
  copies=Counter()
  for e in entities:
   if not e['alive']:continue
   if e['returned']:
    key=(e['player'],e['name']);checks.append(step('bind_permanent_copy',ref=e['ref'],player=e['player'],value=rows[e['name']]['csvid'],index=copies[key]));copies[key]+=1
   checks += [observe(e['ref'],'in_play',1),observe(e['ref'],'tapped',int(e['tapped']))]
   if 'Creature' in o[e['name']]['type_line']:
    checks += [observe(e['ref'],field,int(o[e['name']][field])) for field in ('power','toughness')]
    checks += [observe(e['ref'],'damage',0),observe(e['ref'],'plus1_counters',0),observe(e['ref'],'minus1_counters',0)]
    if e['returned']:checks.append(observe(e['ref'],'sick',1))
  universe=set(WITNESSES)|set(HAND)|set(LIBRARY)|set(EXILE)|{name,'Reclaim','Counterspell'}
  for pl in (0,1):
   checks += [step('assert_life',player=pl,value=20),step('assert_poison',player=pl,value=0),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities))]
   if v!='cleanup':checks.append(step('assert_mana_total',player=pl,value=sum(mana[pl])))
   if v in ('payment','insufficient_mana','wrong_color'):checks += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for z in zones:
    names=zones[z][pl];counts=Counter(names);checks.append(step('assert_zone',player=pl,zone=z,value=len(names)))
    if z=='library' and p['self_destination']!='library_shuffle':checks += [step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(names)]
    else:checks += [step('assert_zone_contains',player=pl,zone=z,value=rows[n]['csvid']) for n in counts]
    checks += [step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=counts[n]) for n in sorted(universe)]
   checks += [step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=ncount) for n,ncount in Counter(zones['hand'][pl]+zones['graveyard'][pl]).items()]
  seq=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=0 if v=='ready_snapshot' else next(i for i,s in enumerate(actions) if s['op']=='resolve')
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  gaps=p['untested']+['Replacement effects, graveyard shroud, multiplayer, counterspells after targets are selected']
  result.append(dict(name=f'ReanimationRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family='reanimation',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return result
