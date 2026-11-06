"""Whole-Oracle graveyard-to-hand components with independently pinned profiles.

This extension shares the ordinary recovery recipe's type vocabulary and the
established populated-zone fixtures. Explicit profile inputs select a printed
mode; they do not bypass the real cast, mana payment or graveyard validation.
"""
import copy
import json
from collections import Counter
from pathlib import Path
from graveyard_recipes import TYPES
from grant_recipes import HAND,GRAVE,LIBRARY,EXILE

PINS=json.loads((Path(__file__).parent/'data/graveyard_recovery_extension_witnesses.json').read_text(encoding='utf8'))
PROFILES=PINS['profiles']
WITNESSES=tuple(PINS['cards'])
SCOPES={'card','creature','instant_or_sorcery','artifact','enchantment','land','permanent','goblin','zombie','arcane','basic_land','artifact_or_enchantment'}


def eligible(r,p,ignore=None):
 base,_,sub=r['type_line'].partition(' — ');types=set(base.split());subs=set(sub.split())
 scope=p['scope']
 valid={'card':True,'creature':'Creature' in types,'artifact':'Artifact' in types,'enchantment':'Enchantment' in types,
  'land':'Land' in types,'permanent':bool(types&{'Artifact','Creature','Enchantment','Land','Planeswalker','Battle'}),
  'instant_or_sorcery':bool(types&{'Instant','Sorcery'}),'goblin':'Goblin' in subs or 'Changeling' in r.get('keywords',[]),
  'zombie':'Zombie' in subs or 'Changeling' in r.get('keywords',[]),
  'arcane':'Arcane' in subs,'basic_land':{'Basic','Land'}<=types,'artifact_or_enchantment':bool(types&{'Artifact','Enchantment'})}[scope]
 return (valid or ignore=='type') and (ignore=='value' or p['value_max'] is None or r['cmc']<=p['value_max'])


def spec(d,r):
 from recipes import cost_from_oracle
 profile=PROFILES.get(r.get('name')) if isinstance(r,dict) else None
 if (not profile or d.get('family')!='graveyard_recovery_extension' or d.get('version')!=2
  or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
  or any(r.get(k)!=v for k,v in profile['source'].items()) or d.get('oracle_text')!=r.get('oracle_text')
  or d.get('params')!=profile['params']):raise ValueError('graveyard recovery extension requires complete source and explicit reviewed component pins')
 p=copy.deepcopy(profile['params'])
 if p['mode'] not in ('spell','entry','activation') or p['scope'] not in SCOPES:raise ValueError('unreviewed recovery mode or predicate')
 if p['mode']=='spell' and not {'Instant','Sorcery'}&set(r['type_line'].partition(' — ')[0].split()) or p['mode']=='entry' and 'Creature' not in r['type_line']:raise ValueError('reviewed recovery source type required')
 if p['value_max'] is not None and (type(p['value_max']) is not int or not 0<=p['value_max']<=8):raise ValueError('bounded explicit recovery value limit required')
 noun={'card':'','creature':'creature','instant_or_sorcery':'instant or sorcery','artifact':'artifact','enchantment':'enchantment','land':'land','permanent':'permanent','goblin':'Goblin','zombie':'Zombie','arcane':'Arcane','basic_land':'basic land','artifact_or_enchantment':'artifact or enchantment'}[p['scope']]
 clause='Return target '+(noun+' ' if noun else '')+'card'+(f" with mana value {p['value_max']} or less" if p['value_max'] is not None else '')+' from your graveyard to your hand.'
 if clause not in r['oracle_text']:raise ValueError('recovery parameters disagree with the printed component')
 creature='Creature' in r['type_line']
 p.update(family='graveyard_recovery_extension',mana=cost_from_oracle(r),creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0)
 if p['mode']=='activation':
  c=p['activation'];kind='creature' if creature else 'artifact' if 'Artifact' in r['type_line'] else 'enchantment'
  pieces=([c['mana']] if c['mana'] else [])+(['{T}'] if c['tap'] else [])+([f"Pay {c['pay_life']} life"] if c['pay_life'] else [])+(['Sacrifice this '+kind] if c['sacrifice'] else [])+(['Exile this '+kind] if c['exile'] else [])
  if ', '.join(pieces)+': '+clause not in r['oracle_text']:raise ValueError('activation parameters disagree with complete printed recovery cost')
  p['activation_mana']=cost_from_oracle(dict(mana_cost=c['mana'])) if c['mana'] else [0]*6
 selected=d.get('variants');available=variants(p)
 if not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or len(selected)!=len(set(selected)) or set(selected)-set(available):raise ValueError('explicit distinct supported recovery variants required')
 return p


def variants(p):
 if p['mode']=='activation':
  modes=['effect','alternate','payment','insufficient_mana','empty_graveyard','other_graveyard','wrong_type','cleanup','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot','cast_source','cast_payment','insufficient_cast_mana','fresh_activation']
  if any(p['activation_mana'][1:]):modes.append('wrong_color')
  if p['value_max'] is not None:modes += ['wrong_value','value_boundary']
  if p['scope']=='artifact_or_enchantment':modes += ['type_artifact','type_enchantment']
  if p['scope']=='card':modes.remove('wrong_type')
  if p['activation']['tap']:modes += ['tapped','sick']
  if p['activation']['pay_life']:modes += ['insufficient_life','low_life']
  if not (p['activation']['tap'] or p['activation']['sacrifice'] or p['activation']['exile']):modes.append('repeat')
  if not (p['activation']['sacrifice'] or p['activation']['exile']):modes.append('destroy_source_pending')
  return [f'{v}_{owner}' for owner in (0,1) for v in modes]
 modes=['effect','alternate','payment','insufficient_mana','wrong_color','countered','cleanup','repeat','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot']
 if not p['optional_mode']:modes += ['empty_graveyard','other_graveyard','wrong_type']
 if p['value_max'] is not None:modes.append('wrong_value')
 if p['conditional_draw']:modes += ['own_artifact','opponent_artifact']
 if p['mode']=='entry':modes.append('destroy_source')
 if p['scope']=='goblin':modes += ['kindred_card','changeling_card']
 return [f'{v}_{owner}' for owner in (0,1) for v in modes]


def validate_witnesses(o,rows):
 if not isinstance(o,dict):raise ValueError('pinned recovery witness context required')
 for n,pin in PINS['cards'].items():
  if n not in rows or any(o.get(n,{}).get(k)!=v for k,v in pin.items()) or o[n].get('card_faces') or o[n].get('selected_face'):raise ValueError('recovery witness Oracle characteristics changed: '+n)
 for n,profile in PROFILES.items():
  if n not in rows or any(o.get(n,{}).get(k)!=v for k,v in profile['source'].items()):raise ValueError('recovery source profile changed: '+n)


def expand(name,csvid,d,p,selected,rows,o):
 if p['mode']=='activation':return expand_activation(name,csvid,d,p,selected,rows,o)
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 candidates=[n for n in WITNESSES if n not in ('Counterspell',) and eligible(o[n],p)]
 if len(candidates)<2:raise ValueError('recovery component requires two independent eligible witnesses')
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones'
  zones={z:[[],[]] for z in ('hand','graveyard','library','exile')}
  for pl in (0,1):
   zones['hand'][pl]=[] if bare else list(HAND);zones['graveyard'][pl]=list(candidates[:2]) if bare else list(GRAVE)+[n for n in WITNESSES if n!='Counterspell']
   zones['library'][pl]=[] if bare else list(LIBRARY);zones['exile'][pl]=[] if bare else list(EXILE)
  if v in ('empty_graveyard','other_graveyard'):zones['graveyard'][owner]=[]
  if v=='empty_graveyard':zones['graveyard'][1-owner]=[]
  if v=='wrong_type':zones['graveyard'][owner]=[n for n in WITNESSES if not eligible(o[n],p,'value')]
  if v=='wrong_value':zones['graveyard'][owner]=[n for n in WITNESSES if not eligible(o[n],p) and eligible(o[n],p,'value')]
  target=candidates[-1] if v=='alternate' else candidates[0]
  if v=='kindred_card':target='Tarfire'
  if v=='changeling_card':target='Nameless Inversion'
  setup=[];pre=[step('turn',player=owner,value=0)];actions=[];source_alive=False;source_hand=True;source_count=0
  for pl in (0,1):
   for z in zones:
    setup += [card(n,f'p{pl}_{z}_{i}',pl,z) for i,n in enumerate(zones[z][pl])]
  def addspell(n,ref,pl):setup.append(card(n,ref,pl,'hand'));zones['hand'][pl].append(n)
  addspell(name,'source',owner)
  artifact_player=owner if v=='own_artifact' else 1-owner if v=='opponent_artifact' else None
  if artifact_player is not None:setup.append(dict(card('Sol Ring','artifact_control',artifact_player),state=['tapped']))
  mana=[[10]*6,[10]*6]
  if v=='repeat':mana[owner]=[30]*6
  if v=='payment':mana[owner]=list(p['mana'])
  if v in ('insufficient_mana','wrong_color'):mana[owner]=[0]*6
  if v=='wrong_color':mana[owner][0]=sum(p['mana'])
  if v in ('repeat','payment','insufficient_mana','wrong_color'):pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  def pay(n,pl):mana[pl]=[a-b for a,b in zip(mana[pl],cost_from_oracle(o[n]))]
  def receipt(n):zones['graveyard'][owner].remove(n);zones['hand'][owner].append(n)
  def decisions(n,ref):
   # A second uncast splice card exposes an optional splice menu on the
   # first human-controller cast, before the ordinary graveyard selector.
   extra=p['selection'].get('repeat_first_before_zone',[]) if v=='repeat' and ref=='source' and owner==0 else []
   before=p['selection']['before_zone']
   condition=p['selection'].get('before_zone_when')
   if condition:
    q=dict(p,scope=condition['scope'],value_max=None)
    if sum(eligible(o[nm],q) for nm in zones['graveyard'][owner])<condition['minimum'] and owner in condition.get('autoselect_controllers',[0,1]):before=[]
   out=[step('choose_dialog',player=owner,value=x) for x in extra+before]
   out += [step('choose_player',player=owner,value=owner if who=='self' else 1-owner) for who in p['selection'].get('before_zone_players',[])]
   out.append(step('choose_zone_card',player=owner,value=rows[n]['csvid']))
   second_scope=p['selection'].get('decline_second_scope')
   if second_scope:
    remaining=list(zones['graveyard'][owner]);remaining.remove(n);q=dict(p,scope=second_scope,value_max=None)
    fallback=next((nm for nm in remaining if eligible(o[nm],q)),None)
    if fallback:out.append(step('choose_optional_zone',player=owner,value=rows[fallback]['csvid']))
   threshold=p['selection']['after_zone_min_mana']
   after=p['selection']['after_zone'] if threshold is None or sum(mana[owner])-sum(p['mana'])>=threshold else []
   out += [step('choose_dialog',player=owner,value=x) for x in after]
   return out
  has_target=any(eligible(o[n],p) for n in zones['graveyard'][owner])
  denied=v in ('insufficient_mana','wrong_color') or p['mode']=='spell' and not has_target
  if v=='countered':addspell('Counterspell','counter',0)
  if v=='repeat':addspell(name,'source2',owner)
  def cast(ref,n,with_target=True):
   nonlocal source_alive,source_hand,source_count,actions
   # Creature casting uses the engine's default creature path. A raw handler
   # EVENT_CAN_CAST return of zero does not reject that ordinary cast.
   if p['mode']=='spell':actions.append(step('require_can_cast',ref=ref,value=1))
   if with_target and not (v=='countered' and p['mode']=='entry'):actions += decisions(n,ref)
   if v=='countered':actions.append(step('choose_priority',player=0,ref='counter'))
   actions += [step('cast',ref=ref,value=0 if v=='countered' else 1)]
   pay(name,owner);zones['hand'][owner].remove(name);source_hand=False
   if v!='countered':actions.append(step('assert_stack',value=1))
   if v=='countered':
    pay('Counterspell',0);zones['hand'][0].remove('Counterspell');zones['graveyard'][0].append('Counterspell');zones['graveyard'][owner].append(name)
   elif p['mode']=='entry':source_alive=True;source_count+=1
   else:zones[p['self_destination']][owner].append(name)
   actions += [step('resolve'),step('assert_choices_consumed')]
   if v!='countered' and with_target:
    receipt(n)
    if p['conditional_draw'] and artifact_player==owner:zones['hand'][owner].append(zones['library'][owner].pop(0))
  if denied:
   actions += [step('cast',ref='source',value=0)] if v in ('insufficient_mana','wrong_color') else [step('can_cast',ref='source',value=0)]
   actions.append(step('assert_choices_consumed'))
  else:
   cast('source',target,has_target)
   if v=='repeat':
    second=next(n for n in reversed(candidates) if n in zones['graveyard'][owner]);cast('source2',second)
   if v=='destroy_source':actions.append(step('destroy',ref='source'));source_alive=False;source_count-=1;zones['graveyard'][owner].append(name)
   if v=='cleanup':actions.append(step('cleanup'))
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0),observe('source','in_hand',int(source_hand))]
  if source_alive:
   checks += [observe('source','in_play',1),observe('source','power',p['power']),observe('source','toughness',p['toughness']),observe('source','damage',0),observe('source','plus1_counters',0),observe('source','minus1_counters',0)]
  universe=set(WITNESSES)|set(HAND)|set(GRAVE)|set(LIBRARY)|set(EXILE)|{name}
  for pl in (0,1):
   checks += [step('assert_life',player=pl,value=20),step('assert_poison',player=pl,value=0),step('assert_zone',player=pl,zone='battlefield',value=(source_count if pl==owner else 0)+int(pl==artifact_player))]
   if v!='cleanup':checks.append(step('assert_mana_total',player=pl,value=sum(mana[pl])))
   if v in ('payment','insufficient_mana','wrong_color'):checks += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for z in zones:
    names=zones[z][pl];counts=Counter(names);checks.append(step('assert_zone',player=pl,zone=z,value=len(names)))
    checks += [step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(names)] if z=='library' else [step('assert_zone_contains',player=pl,zone=z,value=rows[n]['csvid']) for n in counts]
    checks += [step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=counts[n]) for n in sorted(universe)]
   checks += [step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=count) for n,count in Counter(zones['hand'][pl]+zones['graveyard'][pl]).items()]
  seq=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=0 if v=='ready_snapshot' else next(i for i,s in enumerate(actions) if s['op']=='resolve')
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  gaps=p['additional_components']+['Target removal after selection, graveyard shroud, replacement effects and multiplayer']
  result.append(dict(name=f'GraveyardRecoveryExtensionRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family='graveyard_recovery_extension',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return result


def expand_activation(name,csvid,d,p,selected,rows,o):
 """Ordinary recovery activations; casting and fresh-source controls are real.

 Established tap sources use the same fixture convention as value_activation.
 Separate cast and fresh-activation scenarios exercise their actual hand entry.
 """
 from recipes import card,step,observe
 out=[];cost=p['activation'];candidates=[n for n in WITNESSES if n!='Counterspell' and eligible(o[n],p)]
 if len(candidates)<2:raise ValueError('activated recovery needs independent eligible witnesses')
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones'
  casting=v in ('cast_source','cast_payment','insufficient_cast_mana','fresh_activation')
  cast_only=v in ('cast_source','cast_payment','insufficient_cast_mana')
  zones={z:[[],[]] for z in ('hand','graveyard','library','exile')}
  for pl in (0,1):
   zones['hand'][pl]=[] if bare else list(HAND);zones['graveyard'][pl]=list(candidates[:2]) if bare else list(GRAVE)+[n for n in WITNESSES if n!='Counterspell']
   zones['library'][pl]=[] if bare else list(LIBRARY);zones['exile'][pl]=[] if bare else list(EXILE)
  if v in ('empty_graveyard','other_graveyard'):zones['graveyard'][owner]=[]
  if v=='empty_graveyard':zones['graveyard'][1-owner]=[]
  if v=='wrong_type':zones['graveyard'][owner]=[n for n in WITNESSES if not eligible(o[n],p,'value')]
  if v=='wrong_value':zones['graveyard'][owner]=[n for n in WITNESSES if not eligible(o[n],p) and eligible(o[n],p,'value')]
  target=candidates[-1] if v=='alternate' else next(n for n in candidates if o[n]['cmc']==p['value_max']) if v=='value_boundary' else candidates[0]
  if v.startswith('type_'):target=next(n for n in candidates if v[5:].title() in o[n]['type_line'])
  setup=[]
  for pl in (0,1):
   for z in zones:setup += [card(n,f'p{pl}_{z}_{i}',pl,z) for i,n in enumerate(zones[z][pl])]
  source=card(name,'source',owner,'hand' if casting else 'battlefield')
  if v=='tapped':source['state']=['tapped']
  if v=='sick' and p['creature']:source['state']=['sick']
  setup.append(source)
  if casting:zones['hand'][owner].append(name)
  alive=not casting;tapped=v=='tapped';source_hand=casting;life=[20,20];mana=[[10]*6,[10]*6]
  pre=[step('turn',player=owner,value=0)];actions=[]
  if v=='repeat':mana[owner]=[30]*6
  if v=='payment':mana[owner]=list(p['activation_mana'])
  if v in ('insufficient_mana','wrong_color'):mana[owner]=[0]*6
  if v=='wrong_color':mana[owner][0]=sum(p['activation_mana'])
  if v=='cast_payment':mana[owner]=list(p['mana'])
  if v=='insufficient_cast_mana':mana[owner]=[0]*6
  if v in ('repeat','payment','insufficient_mana','wrong_color','cast_payment','insufficient_cast_mana'):pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  if v in ('insufficient_life','low_life'):
   life[owner]=cost['pay_life']-1 if v=='insufficient_life' else cost['pay_life']+1
   pre.append(step('set_life',player=owner,value=life[owner]))
  if casting:
   actions += [step('cast',ref='source',value=int(v!='insufficient_cast_mana'))]
   if v!='insufficient_cast_mana':
    actions += [step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed'),step('require_card',ref='source',property='in_play',value=1)]
    mana[owner]=[a-b for a,b in zip(mana[owner],p['mana'])];zones['hand'][owner].remove(name);source_hand=False;alive=True
  has_target=any(eligible(o[n],p) for n in zones['graveyard'][owner])
  denied=(v in ('insufficient_mana','wrong_color','insufficient_life','tapped') or v=='sick' and p['creature'] and cost['tap'] or v=='fresh_activation' and p['creature'] and cost['tap'] or not has_target)
  def activate(n):
   nonlocal alive,tapped,actions
   actions += [step('require_can_activate',ref='source',value=1),step('choose_zone_card',player=owner,value=rows[n]['csvid']),step('activate',ref='source',value=1),step('assert_stack',value=1),step('assert_choices_consumed')]
   mana[owner]=[a-b for a,b in zip(mana[owner],p['activation_mana'])];life[owner]-=cost['pay_life']
   if cost['sacrifice'] or cost['exile']:
    alive=False;zones['exile' if cost['exile'] else 'graveyard'][owner].append(name)
   elif cost['tap']:tapped=True
   # Costs and the unaffected hand are observed before resolution, so an
   # activation cannot masquerade as a direct fixture zone move.
   actions += [step('assert_life',player=owner,value=life[owner]),step('assert_zone',player=owner,zone='hand',value=len(zones['hand'][owner]))]
   if cost['tap'] and alive:actions.append(observe('source','tapped',1))
   if v=='destroy_source_pending':actions.append(step('destroy',ref='source'));alive=False;zones['graveyard'][owner].append(name)
   actions += [step('resolve'),step('assert_choices_consumed')]
   zones['graveyard'][owner].remove(n);zones['hand'][owner].append(n)
  if not cast_only:
   if denied:actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_choices_consumed')]
   else:
    activate(target)
    if v=='repeat':activate(next(n for n in reversed(candidates) if n in zones['graveyard'][owner]))
   if v=='cleanup':actions.append(step('cleanup'))
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0)]
  # Sacrifice/exile precedes the effect, so the returned card can legally reuse
  # the old active-card slot. Exact source-name zone receipts remain authoritative.
  if alive or source_hand:checks.append(observe('source','in_hand',int(source_hand)))
  if alive:
   checks += [observe('source','in_play',1),observe('source','tapped',int(tapped))]
   if p['creature']:checks += [observe('source','power',p['power']),observe('source','toughness',p['toughness']),observe('source','damage',0),observe('source','plus1_counters',0),observe('source','minus1_counters',0)]
  universe=set(WITNESSES)|set(HAND)|set(GRAVE)|set(LIBRARY)|set(EXILE)|{name}
  for pl in (0,1):
   checks += [step('assert_life',player=pl,value=life[pl]),step('assert_poison',player=pl,value=0),step('assert_zone',player=pl,zone='battlefield',value=int(alive and pl==owner))]
   if v!='cleanup':checks.append(step('assert_mana_total',player=pl,value=sum(mana[pl])))
   if v in ('payment','insufficient_mana','wrong_color','cast_payment','insufficient_cast_mana'):checks += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for z in zones:
    names=zones[z][pl];counts=Counter(names);checks.append(step('assert_zone',player=pl,zone=z,value=len(names)))
    checks += [step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(names)] if z=='library' else [step('assert_zone_contains',player=pl,zone=z,value=rows[n]['csvid']) for n in counts]
    checks += [step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=counts[n]) for n in sorted(universe)]
   checks += [step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=count) for n,count in Counter(zones['hand'][pl]+zones['graveyard'][pl]).items()]
  seq=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=0 if v=='ready_snapshot' else next(i for i,s in enumerate(actions) if s['op']=='resolve')
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  out.append(dict(name=f'GraveyardRecoveryExtensionRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family=p['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['additional_components']+['Target removal after selection, replacement effects, graveyard shroud, multiplayer and source keyword combat'])))
 return out
