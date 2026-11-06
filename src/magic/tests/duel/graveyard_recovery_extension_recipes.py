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
 if isinstance(d.get('params'),dict) and d['params'].get('wave')==6:return wave6_spec(d,r,'graveyard_recovery_extension')
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
 if p.get('_wave6'):return wave6_variants(p)
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
 wave6_validate_witnesses(o,rows)
 if not isinstance(o,dict):raise ValueError('pinned recovery witness context required')
 for n,pin in PINS['cards'].items():
  if n not in rows or any(o.get(n,{}).get(k)!=v for k,v in pin.items()) or o[n].get('card_faces') or o[n].get('selected_face'):raise ValueError('recovery witness Oracle characteristics changed: '+n)
 for n,profile in PROFILES.items():
  if n not in rows or any(o.get(n,{}).get(k)!=v for k,v in profile['source'].items()):raise ValueError('recovery source profile changed: '+n)


def expand(name,csvid,d,p,selected,rows,o):
 if p.get('_wave6'):return wave6_expand(name,csvid,d,p,selected,rows,o)
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


def wave6_spell_spec(d,r,family,p,profile):
 from recipes import cost_from_oracle
 if family!='graveyard_recovery_extension' or r['type_line'].partition(' — ')[0] not in ('Instant','Sorcery') or p.get('wave')!=6 or p.get('schema')!=3 or p.get('scope') not in ('creature','card','land','instant_or_sorcery'):raise ValueError('reviewed whole spell recovery component required')
 if p['selection'] not in ('up_to','x','paired') or p['value_max'] is not None or p['self_destination'] not in ('graveyard','exile') or type(p['discard']) is not bool or type(p['lose_x_life']) is not bool or type(p['draw']) is not int or p['draw'] not in (0,1):raise ValueError('explicit spell companion effects required')
 noun='' if p['scope']=='card' else p['scope']+' '
 if p['selection']=='paired':
  if p['scope']!='instant_or_sorcery' or type(p['quantity']) is not int or p['quantity']!=2:raise ValueError('paired instant/sorcery predicates required')
  clause='Return up to one target instant card and up to one target sorcery card from your graveyard to your hand.'
 elif p['selection']=='x':
  if p['quantity']!='X' or p['scope'] not in ('creature','card'):raise ValueError('printed variable target quantity required')
  clause=f'Return X target {noun}cards from your graveyard to your hand.'
 else:
  if type(p['quantity']) is not int or p['quantity'] not in (2,3):raise ValueError('bounded up-to quantity required')
  clause=f"Return up to { {2:'two',3:'three'}[p['quantity']]} target {noun}cards from your graveyard to your hand."
 if p['discard']:clause=clause[:-1]+', then discard a card.'
 if p['draw']:clause+='\nDraw a card.'
 if p['lose_x_life']:
  if p['selection']!='x':raise ValueError('X life loss requires printed X')
  clause+=' You lose X life.'
 if p['self_destination']=='exile':clause+=' Exile '+r['name']+'.'
 after=p.get('inactive_after_components',[])
 if not isinstance(after,list) or any(type(s) is not str for s in after):raise ValueError('explicit inactive components required')
 if after:clause+='\n'+'\n'.join(after)
 if r['oracle_text']!=clause:raise ValueError('spell quantity, predicate or companions disagree with complete Oracle')
 x_symbols=r['mana_cost'].count('{X}')
 if (p['selection']=='x')!=(x_symbols>0) or x_symbols not in (0,1,2):raise ValueError('printed X symbols disagree with variable component')
 inputs=profile.get('inputs',{})
 if not isinstance(inputs,dict) or set(inputs)-{'ai_discard_ack','quantity_by_selection','double_x_human','ai_x_by_mana_hand','paired_offer_only_eligible'} or any(type(v) is not bool for v in inputs.values()):raise ValueError('unreviewed spell input policy')
 p.update(_wave6=True,family=family,mana=cost_from_oracle(dict(mana_cost=r['mana_cost'].replace('{X}',''))),x_symbols=x_symbols,_inputs=copy.deepcopy(inputs))
 selected=d.get('variants')
 if not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or len(selected)!=len(set(selected)) or set(selected)-set(wave6_spell_variants(p)):raise ValueError('explicit distinct recovery-spell variants required')
 return p


def wave6_spell_variants(p):
 modes=['effect','alternate','payment','insufficient_mana','wrong_color','empty','opponent_only','wrong_type','zero','one_available','duplicates','countered','partial_loss','all_lost','cleanup','repeat','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot']
 if p['selection']=='paired':modes+=['instant_only','sorcery_only']
 if p['selection']=='x':modes+=['quantity_one','quantity_three']
 if p['discard']:modes+=['discard_returned']
 if p['scope']=='card':modes.remove('wrong_type')
 return [f'{v}_{owner}' for owner in (0,1) for v in modes if not (owner==1 and v=='repeat' and (p['discard'] or p['_inputs'].get('ai_x_by_mana_hand')))]


def wave6_spell_expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,cost_from_oracle
 pins=wave6_pins();witnesses=tuple(pins['profiles'][name]['witnesses']);out=[];policy=p['_inputs'];eligible=[n for n in witnesses if wave6_eligible(o[n],p)]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';variable=p['selection']=='x';x=0 if v in ('zero','empty','opponent_only','wrong_type') and variable else 1 if v in ('quantity_one','one_available','all_lost') and variable else 3 if v=='quantity_three' else 2
  maximum=x if variable else p['quantity'];cost=list(p['mana']);cost[0]+=p['x_symbols']*x
  zones={z:[[],[]] for z in ('hand','graveyard','library','exile')}
  for pl in (0,1):
   zones['hand'][pl]=[] if bare else ['Plains','Giant Growth'];zones['graveyard'][pl]=['Forest','Giant Growth','Divination','Seal of Cleansing','Grizzly Bears','Hill Giant','Ornithopter']
   zones['library'][pl]=[] if bare else ['Forest','Divination','Island','Lightning Bolt'];zones['exile'][pl]=[] if bare else ['Mountain']
   for n in eligible[:max(3,maximum)]:
    if n not in zones['graveyard'][pl]:zones['graveyard'][pl].append(n)
  if p['draw'] and bare:zones['library'][owner]=['Forest']
  if p['discard'] and owner==1:
   identity='Hill Giant' if v=='alternate' and p['scope']=='creature' else 'Grizzly Bears'
   if p['scope']=='creature':
    zones['hand'][owner]=[identity,identity];zones['graveyard'][owner]=['Forest','Giant Growth','Divination']+[identity]*3
   else:zones['hand'][owner]=['Plains','Plains']
  if policy.get('ai_x_by_mana_hand') and owner==1:zones['hand'][owner]=['Plains']*max(1,x)
  if v in ('empty','opponent_only'):zones['graveyard'][owner]=[]
  if v=='empty':zones['graveyard'][1-owner]=[]
  if v=='wrong_type':zones['graveyard'][owner]=[n for n in ('Forest','Giant Growth','Divination','Sol Ring','Grizzly Bears') if not wave6_eligible(o[n],p)]
  if v=='one_available':zones['graveyard'][owner]=[n for n in ('Forest','Giant Growth','Sol Ring') if not wave6_eligible(o[n],p)]+[eligible[0]]
  if v=='duplicates':zones['graveyard'][owner]=[eligible[0]]*max(3,maximum)+[n for n in ('Giant Growth',) if not wave6_eligible(o[n],p)]
  if v=='instant_only':zones['graveyard'][owner]=['Giant Growth','Lightning Bolt','Forest']
  if v=='sorcery_only':zones['graveyard'][owner]=['Divination','Forest']
  if v=='discard_returned':zones['hand'][owner]=[]
  if v=='repeat' and variable:
   while sum(wave6_eligible(o[n],p) for n in zones['graveyard'][owner])<2*x:zones['graveyard'][owner].append(eligible[0])
  setup=[];pre=[step('turn',player=owner,value=0)];actions=[];mana=[[10]*6,[10]*6];life=[20,20];uncertain=None
  for pl in (0,1):
   for z in zones:setup.extend(card(n,f's6_{pl}_{z}_{i}',pl,z) for i,n in enumerate(zones[z][pl]))
  def add(n,ref,pl=owner):setup.append(card(n,ref,pl,'hand'));zones['hand'][pl].append(n)
  add(name,'source')
  if v=='repeat':add(name,'source2')
  if v=='countered':add('Counterspell','counter',0)
  if v in ('partial_loss','all_lost'):add('Reclaim','response')
  if v=='repeat':mana[owner]=[30]*6
  if v=='payment' or policy.get('ai_x_by_mana_hand') and owner==1:
   mana[owner]=list(cost)
   if v in ('partial_loss','all_lost'):mana[owner]=[a+b for a,b in zip(mana[owner],cost_from_oracle(o['Reclaim']))]
   if v=='countered' and owner==0:mana[owner]=[a+b for a,b in zip(mana[owner],cost_from_oracle(o['Counterspell']))]
  if v=='insufficient_mana':mana[owner]=[0]*6
  if v=='wrong_color':mana[owner]=[sum(cost),0,0,0,0,0]
  if v in ('repeat','payment','insufficient_mana','wrong_color') or policy.get('ai_x_by_mana_hand') and owner==1:pre.extend(step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner]))
  def pay(n,pl=owner,cost_override=None):mana[pl]=[a-b for a,b in zip(mana[pl],cost_override or cost_from_oracle(o[n]))]
  def targets():
   available=[n for n in zones['graveyard'][owner] if wave6_eligible(o[n],p)]
   if v=='zero':return []
   if p['selection']=='paired':
    result=[]
    for typ in ('Instant','Sorcery'):
     ns=[n for n in available if typ in o[n]['type_line'].partition(' — ')[0].split()]
     if ns:result.append(ns[-1] if v=='alternate' else ns[0])
    return result[:1] if v=='all_lost' else result
   if v=='alternate' and available:available.reverse()
   return available[:1 if v=='all_lost' else maximum]
  def target_inputs(ns):
   q=[]
   if policy.get('double_x_human') and owner==0:q.append(step('choose_x',player=owner,value=x))
   if p['selection']=='paired':
    for typ in ('Instant','Sorcery'):
     n=next((n for n in ns if typ in o[n]['type_line'].partition(' — ')[0].split()),None);offered=any(typ in o[nm]['type_line'].partition(' — ')[0].split() for nm in zones['graveyard'][owner])
     if n:q.append(step('choose_zone_card',player=owner,value=rows[n]['csvid']))
     elif zones['graveyard'][owner] and (offered or not policy.get('paired_offer_only_eligible')):q.append(step('choose_optional_zone',player=owner,value=rows['Giant Growth' if typ=='Instant' else 'Divination']['csvid']))
   else:
    q.extend(step('choose_zone_card',player=owner,value=rows[n]['csvid']) for n in ns)
    limit=min(maximum,len(zones['graveyard'][owner]))
    if policy.get('quantity_by_selection'):limit=min(sum(wave6_eligible(o[n],p) for n in zones['graveyard'][owner]),max(0,sum(mana[owner])-sum(p['mana'])),19)
    if len(ns)<limit:q.append(step('choose_optional_zone',player=owner,value=rows[eligible[0]]['csvid']))
   return q
  def companion_inputs(ns,fizzled):
   if not p['discard']:return []
   pool=list(zones['hand'][owner]);pool.remove(name)
   if v in ('partial_loss','all_lost'):pool.remove('Reclaim')
   if not fizzled:pool+=ns
   if not pool:return []
   if owner==1:return [step('choose_dialog',player=owner,value=0)]
   chosen='Giant Growth' if 'Giant Growth' in pool else pool[0]
   return [step('choose_zone_card',player=owner,value=rows[chosen]['csvid'])]
  def complete(ref):
   nonlocal uncertain
   ns=targets();original=list(ns);lost=ns[0] if v in ('partial_loss','all_lost') and ns else None
   remaining=list(ns)
   if lost:remaining.remove(lost)
   fizzled=bool(original) and not remaining and bool(lost);companions=companion_inputs(remaining,fizzled)
   actions.extend(target_inputs(ns))
   if v=='countered':actions.append(step('choose_priority',player=0,ref='counter'))
   actions.append(step('cast',ref=ref,value=0 if v=='countered' else 1));pay(name,cost_override=cost);zones['hand'][owner].remove(name)
   if v=='countered':pay('Counterspell',0);zones['hand'][0].remove('Counterspell');zones['graveyard'][0].append('Counterspell');zones['graveyard'][owner].append(name);actions.extend([step('resolve'),step('assert_choices_consumed')]);return
   actions.append(step('assert_stack',value=1))
   if lost:
    actions.extend([step('choose_zone_card',player=owner,value=rows[lost]['csvid']),step('cast_response',ref='response',value=1),step('assert_stack',value=2)]);pay('Reclaim');zones['hand'][owner].remove('Reclaim');zones['graveyard'][owner].remove(lost);zones['library'][owner].insert(0,lost);zones['graveyard'][owner].append('Reclaim')
    actions.extend(companions+[step('resolve_response'),step('assert_stack',value=0)])
   else:actions.extend(companions+[step('resolve')])
   actions.append(step('assert_unused_choices',value=len(companions)) if fizzled and companions else step('assert_choices_consumed'))
   if not fizzled:
    for n in remaining:zones['graveyard'][owner].remove(n);zones['hand'][owner].append(n)
    for _ in range(p['draw']):zones['hand'][owner].append(zones['library'][owner].pop(0))
    if p['lose_x_life']:life[owner]-=x
    if p['discard'] and zones['hand'][owner]:
     if owner==1 and len(set(zones['hand'][owner]))>1:
      uncertain=dict(hand=list(zones['hand'][owner]),grave=list(zones['graveyard'][owner]));zones['hand'][owner].pop(0);zones['graveyard'][owner].append(uncertain['hand'][0])
     else:
      chosen='Giant Growth' if 'Giant Growth' in zones['hand'][owner] else zones['hand'][owner][0];zones['hand'][owner].remove(chosen);zones['graveyard'][owner].append(chosen)
   zones['graveyard' if fizzled else p['self_destination']][owner].append(name)
   if uncertain:uncertain['grave'].append(name) if p['self_destination']=='graveyard' else None
  if v in ('insufficient_mana','wrong_color'):actions.extend([step('cast',ref='source',value=0),step('assert_choices_consumed')])
  else:
   complete('source')
   if v=='repeat':complete('source2')
   if v=='cleanup':actions.extend([step('cleanup'),step('resolve'),step('assert_choices_consumed')])
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0)];universe=set(witnesses)|{name}
  for pl in (0,1):
   checks.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=0)])
   if v!='cleanup':checks.append(step('assert_mana_total',player=pl,value=sum(mana[pl])))
   for z in zones:
    values=zones[z][pl];counts=Counter(values);checks.append(step('assert_zone',player=pl,zone=z,value=len(values)))
    if z=='library':checks.extend(step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(values))
    for n in sorted(universe):
     if uncertain and pl==owner and z in ('hand','graveyard') and n in uncertain['hand']:
      bound=uncertain['hand'].count(n) if z=='hand' else uncertain['grave'].count(n)+1;checks.append(step('assert_zone_multiplicity_max',player=pl,zone=z,index=rows[n]['csvid'],value=bound))
     else:checks.append(step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=counts[n]))
   if uncertain and pl==owner:
    for n in sorted(set(uncertain['hand'])):checks.append(step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=uncertain['hand'].count(n)+uncertain['grave'].count(n)))
  seq=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=0 if v=='ready_snapshot' else next(i for i,s in enumerate(actions) if s['op'] in ('resolve','resolve_response'))
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq.extend([step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')])
  gaps=['Other replacement effects, graveyard shroud and multiplayer beyond two players']
  if variable:gaps.append('Variable X tests cover0/1/2/3; larger quantities are untested')
  if policy.get('ai_x_by_mana_hand'):gaps.append('AI1 X controls use exact mana and authored hand counts to bind the native legal choice; AI1 repeat is not emitted')
  if p['discard']:gaps.append('AI1 repeat is not emitted: a legal native discard can discard the second source spell; uncertain distinct-card discards use conservation bounds rather than a guessed identity')
  gaps.extend(p.get('inactive_after_components',[]))
  out.append(dict(name=f'GraveyardRecoveryWave6Recipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family=p['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out


def wave6_activation_response_expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 pins=wave6_pins();witnesses=tuple(pins['profiles'][name]['witnesses']);out=[];c=p['activation'];policy=p['_inputs']
 eligible=[n for n in witnesses if wave6_eligible(o[n],p)]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';selection=p['selection']
  casting=v in ('cast_source','cast_payment','insufficient_cast_mana','fresh_activation','countered_source');cast_only=v in ('cast_source','cast_payment','insufficient_cast_mana','countered_source')
  zones={z:[[],[]] for z in ('hand','graveyard','library','exile')}
  for pl in (0,1):
   zones['hand'][pl]=[] if bare else ['Plains','Giant Growth'];zones['graveyard'][pl]=['Forest','Giant Growth','Divination','Seal of Cleansing','Grizzly Bears','Hill Giant','Sol Ring','Ornithopter','Howling Mine','Mana Vault']
   zones['library'][pl]=[] if bare else ['Forest','Divination','Island','Lightning Bolt'];zones['exile'][pl]=[] if bare else ['Mountain','Giant Growth']
   if bare:zones['graveyard'][pl]=list(eligible[:max(2,p['maximum'])])
   for n in eligible[:max(2,p['maximum'])]:
    if n not in zones['graveyard'][pl]:zones['graveyard'][pl].append(n)
  if c['discard']:
   # AI's actual discard() chooses randomly. Only one legal identity is held.
   zones['hand'][owner]=['Grizzly Bears'] if bare else ['Grizzly Bears','Grizzly Bears'] if owner==1 else ['Plains','Giant Growth','Grizzly Bears']
  if v in ('empty','opponent_only'):zones['graveyard'][owner]=[]
  if v=='empty':zones['graveyard'][1-owner]=[]
  if v=='wrong_type':zones['graveyard'][owner]=[n for n in ('Forest','Giant Growth','Divination','Sol Ring','Grizzly Bears') if not wave6_eligible(o[n],p)]
  if v=='predicate_offer':zones['graveyard'][owner]=['Grizzly Bears','Ornithopter','Forest','Giant Growth','Jace Beleren','Seal of Cleansing','Nameless Inversion']
  if v=='one_available':zones['graveyard'][owner]=['Forest','Giant Growth',eligible[0]]
  if v=='duplicates':zones['graveyard'][owner]=[eligible[0]]*3+['Forest']
  if v=='artifact_only':zones['graveyard'][owner]=['Sol Ring','Ornithopter','Giant Growth']
  if v=='enchantment_only':zones['graveyard'][owner]=['Seal of Cleansing','Holy Strength','Giant Growth']
  if v=='low_artifact':zones['graveyard'][owner]=['Sol Ring','Giant Growth','Seal of Cleansing']
  if v=='empty_hand':zones['hand'][owner]=[]
  if v=='wrong_cost_type':zones['hand'][owner]=['Plains','Giant Growth'];zones['hand'][1-owner].append('Grizzly Bears')
  if v=='alternate' and eligible[-1] not in zones['graveyard'][owner]:zones['graveyard'][owner].append(eligible[-1])
  if v=='alternate' and selection=='all':zones['graveyard'][owner]=['Font of Return','Tortured Existence','Giant Growth']
  source_response=v=='source_bounced_pending';response='Unsummon' if source_response and p['creature'] else 'Boomerang' if source_response else 'Reclaim'
  history='Ornithopter' if selection=='paired' else 'Grizzly Bears'
  if history in zones['graveyard'][owner]:zones['graveyard'][owner].remove(history)
  zones['hand'][owner]=[response,response] if name=='Undertaker' and owner==1 else ['Plains','Hill Giant',response] if c['discard']=='creature' else ['Plains','Giant Growth',response]
  zones['hand'][owner].append(history)
  setup=[];pre=[step('turn',player=owner,value=0)];actions=[];entities=[];mana=[[10]*6,[10]*6];life=[20,20]
  if v=='predicate_offer':pre.append(step('observe_search_filters',value=1))
  for pl in (0,1):
   for z in zones:setup.extend(card(n,f'a6_{pl}_{z}_{i}',pl,z) for i,n in enumerate(zones[z][pl]))
  source=card(name,'source',owner,'hand' if casting else 'battlefield')
  if v=='tapped':source['state']=['tapped']
  if v=='sick':source['state']=['sick']
  if v=='stolen_source' or source_response:source['state']=['opponent_owned']
  setup.append(source)
  if casting:zones['hand'][owner].append(name)
  e=dict(name=name,ref='source',controller=owner,owner=1-owner if v=='stolen_source' or source_response else owner,alive=not casting,tapped=v=='tapped',sick=v=='sick');entities.append(e)
  if c['creature_sacrifice'] and v!='missing_cost':
   for index in range(2 if v=='repeat' else 1):
    pl=1-owner if v=='opponent_cost' else owner;ref=f'cost_creature_{index}';entry=card('Grizzly Bears',ref,pl)
    if v=='stolen_cost':entry['state']=['opponent_owned']
    setup.append(entry);entities.append(dict(name='Grizzly Bears',ref=ref,controller=pl,owner=1-owner if v=='stolen_cost' else pl,alive=True,tapped=False,sick=False))
  def add_hand(n,ref,pl):setup.append(card(n,ref,pl,'hand'));zones['hand'][pl].append(n)
  if v=='countered_source':add_hand('Counterspell','counter',0)
  if v=='repeat':mana[owner]=[30]*6
  if v in ('payment','cast_payment'):mana[owner]=list(p['mana'] if v=='cast_payment' else p['activation_mana'])
  if v in ('insufficient_mana','insufficient_cast_mana'):mana[owner]=[0]*6
  if v=='wrong_color':mana[owner]=[sum(p['activation_mana']),0,0,0,0,0]
  if v in ('repeat','payment','cast_payment','insufficient_mana','insufficient_cast_mana','wrong_color'):pre.extend(step('set_mana',player=owner,color=color,value=value) for color,value in enumerate(mana[owner]))
  def pay(cost,pl=owner):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  if casting:
   if v=='countered_source':actions.append(step('choose_priority',player=0,ref='counter'))
   actions.append(step('cast',ref='source',value=int(v not in ('insufficient_cast_mana','countered_source'))))
   if v!='insufficient_cast_mana':
    pay(p['mana']);zones['hand'][owner].remove(name)
    if v=='countered_source':pay(cost_from_oracle(o['Counterspell']),0);zones['hand'][0].remove('Counterspell');zones['graveyard'][0].append('Counterspell');zones['graveyard'][owner].append(name)
    else:e['alive']=True;e['sick']=p['creature'];actions.append(step('assert_stack',value=1))
    actions.extend([step('resolve'),step('assert_choices_consumed')])
  history_ref=next(x['ref'] for x in setup if x['player']==owner and x['zone']=='hand' and x['name']==history)
  actions.extend([step('cast',ref=history_ref,value=1),step('assert_stack',value=1),step('resolve'),step('destroy',ref=history_ref),step('refresh'),step('assert_choices_consumed')])
  pay([0]*6 if o[history]['mana_cost']=='{0}' else cost_from_oracle(o[history]));zones['hand'][owner].remove(history);zones['graveyard'][owner].append(history)
  def choose_targets():
   available=[n for n in zones['graveyard'][owner] if wave6_eligible(o[n],p)]
   if not source_response and selection!='all':
    return [history]+(['Seal of Cleansing'] if selection=='paired' and v=='partial_target_loss' else ['Hill Giant','Ornithopter'] if selection=='up_to' and v=='partial_target_loss' else [])
   if source_response:return [history]
   if selection=='all':return available
   if v in ('zero','predicate_offer'):return []
   if selection=='paired':
    chosen=[]
    for typ in ('Artifact','Enchantment'):
     candidates=[n for n in zones['graveyard'][owner] if typ in o[n]['type_line'].partition(' — ')[0].split()]
     if candidates:chosen.append(candidates[-1] if v=='alternate' else candidates[0])
    return chosen
   if v=='alternate' and available:
    last=available[-1];available.remove(last);available.insert(0,last)
   return available[:p['maximum']]
  def pending_checks():
   checks=[step('assert_stack',value=1),step('assert_choices_consumed'),step('assert_mana_total',player=owner,value=sum(mana[owner])),step('assert_zone',player=owner,zone='hand',value=len(zones['hand'][owner]))]
   for pl in (0,1):
    for n in sorted(set([name,'Grizzly Bears','Giant Growth'])):checks.append(step('assert_zone_multiplicity',player=pl,zone='graveyard',index=rows[n]['csvid'],value=zones['graveyard'][pl].count(n)))
   if e['alive']:checks.append(observe('source','tapped',int(e['tapped'])))
   return checks
  def activate():
   ns=choose_targets();assert len(ns)==len(set(ns)), 'response cohort requires unique selected identities'
   q=[];cost_card='Hill Giant' if c['discard']=='creature' else response if name=='Undertaker' and owner==1 else 'Giant Growth';sac=next((x for x in entities[1:] if x['alive'] and x['controller']==owner),None)
   hand_input=[step('choose_zone_card',player=owner,value=rows[cost_card]['csvid'])] if c['discard'] and (owner==0 or c['discard']=='creature') else [step('choose_dialog',player=owner,value=0)] if c['discard'] and policy.get('ai_discard_ack') else []
   creature_input=[step('choose_target',player=owner,ref=sac['ref'])] if c['creature_sacrifice'] and not (owner==1 and policy.get('ai_creature_sacrifice_auto')) else []
   if policy.get('discard_before_grave'):q.extend(hand_input)
   if policy.get('creature_before_grave'):q.extend(creature_input)
   if selection=='paired':
    for typ in ('Artifact','Enchantment'):
     n=next((n for n in ns if typ in o[n]['type_line'].partition(' — ')[0].split()),None)
     if n:q.append(step('choose_zone_card',player=owner,value=rows[n]['csvid']))
     elif zones['graveyard'][owner] or policy.get('sacrifice_before_selection'):q.append(step('choose_optional_zone',player=owner,value=rows['Sol Ring' if typ=='Artifact' else 'Seal of Cleansing']['csvid']))
   elif selection!='all':
    q.extend(step('choose_zone_card',player=owner,value=rows[n]['csvid']) for n in ns)
    if selection=='up_to' and len(ns)<min(p['maximum'],len(zones['graveyard'][owner])):q.append(step('choose_optional_zone',player=owner,value=rows[eligible[0]]['csvid']))
   if not policy.get('discard_before_grave'):q.extend(hand_input)
   if not policy.get('creature_before_grave'):q.extend(creature_input)
   offered=list(zones['graveyard'][owner])
   actions.extend([step('require_can_activate',ref='source',value=1)]+q+[step('activate',ref='source',value=1)]);pay(p['activation_mana'])
   if v=='predicate_offer':
    actions.extend([step('assert_offered_count',value=len(offered)),step('assert_offered_selection_required',value=0)])
    actions.extend(step('assert_offered_legal_card',index=rows[n]['csvid'],value=int(wave6_eligible(o[n],p))) for n in dict.fromkeys(offered))
   if c['discard']:zones['hand'][owner].remove(cost_card);zones['graveyard'][owner].append(cost_card)
   if c['creature_sacrifice']:sac['alive']=False;zones['graveyard'][sac['owner']].append(sac['name'])
   if c['tap']:e['tapped']=True
   if c['self_sacrifice']:e['alive']=False;zones['graveyard'][e['owner']].append(name)
   actions.extend(pending_checks())
   if v=='destroy_source_pending':actions.extend([step('destroy',ref='source'),step('refresh')]);e['alive']=False;zones['graveyard'][owner].append(name)
   actions.append(step('bind_hand',ref='actual_response',player=owner,value=rows[response]['csvid']))
   if source_response:actions.append(step('choose_target',player=owner,ref='source'))
   else:
    removed='Seal of Cleansing' if selection=='all' else history
    actions.append(step('choose_zone_card',player=owner,value=rows[removed]['csvid']))
   actions.extend([step('cast',ref='actual_response',value=1),step('assert_stack',value=2),step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed')])
   pay(cost_from_oracle(o[response]));zones['hand'][owner].remove(response);zones['graveyard'][owner].append(response)
   if source_response:e['alive']=False;zones['hand'][e['owner']].append(name)
   else:zones['graveyard'][owner].remove(removed);zones['library'][owner].insert(0,removed)
   if selection=='all':ns=[n for n in zones['graveyard'][owner] if wave6_eligible(o[n],p)]
   elif not source_response:ns.remove(removed)  # The selected physical occurrence left; a same-name cost card cannot replace it.
   for n in ns:
    if n in zones['graveyard'][owner]:zones['graveyard'][owner].remove(n);zones['hand'][owner].append(n)
  denied=v in ('insufficient_mana','wrong_color','tapped','sick','empty_hand','wrong_cost_type','missing_cost','opponent_cost') or v=='fresh_activation' and c['tap'] and p['creature'] or p['minimum'] and not any(wave6_eligible(o[n],p) for n in zones['graveyard'][owner])
  if not cast_only:
   if denied:actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_choices_consumed')])
   else:
    activate()
    if v=='repeat':activate()
   if v=='cleanup':actions.extend([step('cleanup'),step('resolve'),step('assert_choices_consumed')])
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0)]
  for ent in entities:
   if ent['alive']:
    checks.extend([observe(ent['ref'],'csvid',rows[ent['name']]['csvid']),observe(ent['ref'],'in_play',1),observe(ent['ref'],'tapped',int(ent['tapped']))])
    if 'Creature' in o[ent['name']]['type_line']:checks.extend(observe(ent['ref'],k,int(o[ent['name']][k])) for k in ('power','toughness'));checks.extend([observe(ent['ref'],'sick',int(ent['sick'])),observe(ent['ref'],'damage',0),observe(ent['ref'],'plus1_counters',0),observe(ent['ref'],'minus1_counters',0)])
  universe=set(witnesses)|{name,response}
  for pl in (0,1):
   checks.extend([step('assert_life',player=pl,value=life[pl]),step('assert_poison',player=pl,value=0),step('assert_zone',player=pl,zone='battlefield',value=sum(x['alive'] and x['controller']==pl for x in entities))])
   if v!='cleanup':checks.append(step('assert_mana_total',player=pl,value=sum(mana[pl])))
   if v in ('payment','cast_payment','wrong_color','insufficient_mana','insufficient_cast_mana'):checks.extend(step('assert_mana',player=pl,color=color,value=value) for color,value in enumerate(mana[pl]))
   for z in zones:
    values=zones[z][pl];counts=Counter(values);checks.append(step('assert_zone',player=pl,zone=z,value=len(values)))
    checks.extend(step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(values)) if z=='library' else checks.extend(step('assert_zone_contains',player=pl,zone=z,value=rows[n]['csvid']) for n in counts)
    checks.extend(step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=counts[n]) for n in sorted(universe))
  seq=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=0 if v=='ready_snapshot' else next(i for i,s in enumerate(actions) if s['op']=='resolve')
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq.extend([step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')])
  gaps=['Activated-ability countering remains untested; real ordinary-cast source/target responses are exercised','Replacement effects and simultaneous deaths beyond the selected ordinary cost creature','Only two players represented']
  if selection=='paired':gaps.append('Selecting the same artifact-enchantment physical card for both separate target instances')
  out.append(dict(name=f'GraveyardRecoveryWave6Recipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family=p['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out



def wave6_activation_spec(d,r,family,p,profile):
 from recipes import cost_from_oracle
 if family!='graveyard_recovery_extension' or p.get('wave')!=6 or p.get('schema')!=3 or p.get('scope') not in ('creature','enchantment','artifact_or_enchantment'):
  raise ValueError('unreviewed Wave6 activated recovery component')
 selection=p.get('selection');expected={'single':(1,1),'up_to':(0,3),'all':(0,0),'paired':(0,2)}
 if selection not in expected or type(p.get('minimum')) is not int or type(p.get('maximum')) is not int or (p['minimum'],p['maximum'])!=expected[selection]:raise ValueError('printed activated recovery quantity required')
 if (selection=='paired')!=(p['scope']=='artifact_or_enchantment'):raise ValueError('paired recovery requires separate artifact and enchantment predicates')
 if p.get('value_max') is not None or p.get('life_mana_value') is not False:raise ValueError('unreviewed activated recovery companion effect')
 c=p.get('activation')
 if not isinstance(c,dict) or set(c)!={'mana','tap','self_sacrifice','creature_sacrifice','discard'} or any(type(c[k]) is not bool for k in ('tap','self_sacrifice','creature_sacrifice')) or c['discard'] not in ('','card','creature') or type(c['mana']) is not str:raise ValueError('complete printed activation cost required')
 if sum((c['self_sacrifice'],c['creature_sacrifice'],bool(c['discard'])))>1:raise ValueError('one reviewed additional cost required')
 types=set(r['type_line'].partition(' — ')[0].split());creature='Creature' in types
 if not types&{'Creature','Artifact','Enchantment'}:raise ValueError('activated recovery must be a permanent')
 kind='creature' if creature else 'artifact' if 'Artifact' in types else 'enchantment'
 pieces=([c['mana']] if c['mana'] else [])+(['{T}'] if c['tap'] else [])+(['Sacrifice this '+kind] if c['self_sacrifice'] else [])+(['Sacrifice a creature'] if c['creature_sacrifice'] else [])+(['Discard '+('a creature card' if c['discard']=='creature' else 'a card')] if c['discard'] else [])
 noun=p['scope']
 clause={'single':f'Return target {noun} card from your graveyard to your hand.','up_to':f'Return up to three target {noun} cards from your graveyard to your hand.','all':f'Return all {noun} cards from your graveyard to your hand.','paired':'Return up to one target artifact card and up to one target enchantment card from your graveyard to your hand.'}[selection]
 if r['oracle_text']!=', '.join(pieces)+': '+clause:raise ValueError('activation cost or recovery effect disagrees with whole Oracle')
 inputs=profile.get('inputs',{})
 if not isinstance(inputs,dict) or set(inputs)-{'discard_before_grave','creature_before_grave','ai_discard_ack','ai_creature_sacrifice_auto','sacrifice_before_selection'} or any(type(v) is not bool for v in inputs.values()):raise ValueError('unreviewed activated input order')
 if creature and (not str(r.get('power','')).isdigit() or not str(r.get('toughness','')).isdigit() or int(r['toughness'])<1):raise ValueError('bounded activation source body required')
 p.update(_wave6=True,family=family,mana=cost_from_oracle(r),activation_mana=cost_from_oracle(dict(mana_cost=c['mana'])) if c['mana'] else [0]*6,creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,_inputs=copy.deepcopy(inputs))
 selected=d.get('variants')
 if not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or len(selected)!=len(set(selected)) or set(selected)-set(wave6_activation_variants(p)):raise ValueError('explicit distinct activated recovery variants required')
 return p


def wave6_activation_variants(p):
 c=p['activation'];modes=['effect','alternate','payment','insufficient_mana','empty','opponent_only','wrong_type','cleanup','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot','cast_source','cast_payment','insufficient_cast_mana','fresh_activation','countered_source']
 if any(p['activation_mana'][1:]):modes.append('wrong_color')
 if c['tap']:modes.append('tapped')
 if c['tap'] and p['creature']:modes.append('sick')
 if c['self_sacrifice']:modes.append('stolen_source')
 if not (c['tap'] or c['self_sacrifice']):modes.append('repeat')
 if not c['self_sacrifice']:modes.append('destroy_source_pending')
 if c['discard']:modes+=['empty_hand','wrong_cost_type'] if c['discard']=='creature' else ['empty_hand']
 if c['creature_sacrifice']:modes+=['missing_cost','opponent_cost','stolen_cost']
 if p['selection']=='up_to':modes+=['zero','one_available','duplicates','predicate_offer']
 if p['selection']=='paired':modes+=['zero','artifact_only','enchantment_only','low_artifact']
 modes+=['grave_changed'] if p['selection']=='all' else ['lost_target']
 if p['selection'] in ('up_to','paired'):modes.append('partial_target_loss')
 if not c['self_sacrifice']:modes.append('source_bounced_pending')
 return [f'{v}_{pl}' for pl in (0,1) for v in modes]


def wave6_activation_expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 extra=[v for v in selected if v.rsplit('_',1)[0] in ('lost_target','partial_target_loss','grave_changed','source_bounced_pending')]
 out=wave6_activation_response_expand(name,csvid,d,p,extra,rows,o) if extra else []
 selected=[v for v in selected if v not in extra]
 if not selected:return out
 pins=wave6_pins();witnesses=tuple(pins['profiles'][name]['witnesses']);c=p['activation'];policy=p['_inputs']
 eligible=[n for n in witnesses if wave6_eligible(o[n],p)]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';selection=p['selection']
  casting=v in ('cast_source','cast_payment','insufficient_cast_mana','fresh_activation','countered_source');cast_only=v in ('cast_source','cast_payment','insufficient_cast_mana','countered_source')
  zones={z:[[],[]] for z in ('hand','graveyard','library','exile')}
  for pl in (0,1):
   zones['hand'][pl]=[] if bare else ['Plains','Giant Growth'];zones['graveyard'][pl]=['Forest','Giant Growth','Divination','Seal of Cleansing','Grizzly Bears','Hill Giant','Sol Ring','Ornithopter','Howling Mine','Mana Vault']
   zones['library'][pl]=[] if bare else ['Forest','Divination','Island','Lightning Bolt'];zones['exile'][pl]=[] if bare else ['Mountain','Giant Growth']
   if bare:zones['graveyard'][pl]=list(eligible[:max(2,p['maximum'])])
   for n in eligible[:max(2,p['maximum'])]:
    if n not in zones['graveyard'][pl]:zones['graveyard'][pl].append(n)
  if c['discard']:
   # AI's actual discard() chooses randomly. Only one legal identity is held.
   zones['hand'][owner]=['Grizzly Bears'] if bare else ['Grizzly Bears','Grizzly Bears'] if owner==1 else ['Plains','Giant Growth','Grizzly Bears']
  if v in ('empty','opponent_only'):zones['graveyard'][owner]=[]
  if v=='empty':zones['graveyard'][1-owner]=[]
  if v=='wrong_type':zones['graveyard'][owner]=[n for n in ('Forest','Giant Growth','Divination','Sol Ring','Grizzly Bears') if not wave6_eligible(o[n],p)]
  if v=='predicate_offer':zones['graveyard'][owner]=['Grizzly Bears','Ornithopter','Forest','Giant Growth','Jace Beleren','Seal of Cleansing','Nameless Inversion']
  if v=='one_available':zones['graveyard'][owner]=['Forest','Giant Growth',eligible[0]]
  if v=='duplicates':zones['graveyard'][owner]=[eligible[0]]*3+['Forest']
  if v=='artifact_only':zones['graveyard'][owner]=['Sol Ring','Ornithopter','Giant Growth']
  if v=='enchantment_only':zones['graveyard'][owner]=['Seal of Cleansing','Holy Strength','Giant Growth']
  if v=='low_artifact':zones['graveyard'][owner]=['Sol Ring','Giant Growth','Seal of Cleansing']
  if v=='empty_hand':zones['hand'][owner]=[]
  if v=='wrong_cost_type':zones['hand'][owner]=['Plains','Giant Growth'];zones['hand'][1-owner].append('Grizzly Bears')
  if v=='alternate' and eligible[-1] not in zones['graveyard'][owner]:zones['graveyard'][owner].append(eligible[-1])
  if v=='alternate' and selection=='all':zones['graveyard'][owner]=['Font of Return','Tortured Existence','Giant Growth']
  setup=[];pre=[step('turn',player=owner,value=0)];actions=[];entities=[];mana=[[10]*6,[10]*6];life=[20,20]
  if v=='predicate_offer':pre.append(step('observe_search_filters',value=1))
  for pl in (0,1):
   for z in zones:setup.extend(card(n,f'a6_{pl}_{z}_{i}',pl,z) for i,n in enumerate(zones[z][pl]))
  source=card(name,'source',owner,'hand' if casting else 'battlefield')
  if v=='tapped':source['state']=['tapped']
  if v=='sick':source['state']=['sick']
  if v=='stolen_source':source['state']=['opponent_owned']
  setup.append(source)
  if casting:zones['hand'][owner].append(name)
  e=dict(name=name,ref='source',controller=owner,owner=1-owner if v=='stolen_source' else owner,alive=not casting,tapped=v=='tapped',sick=v=='sick');entities.append(e)
  if c['creature_sacrifice'] and v!='missing_cost':
   for index in range(2 if v=='repeat' else 1):
    pl=1-owner if v=='opponent_cost' else owner;ref=f'cost_creature_{index}';entry=card('Grizzly Bears',ref,pl)
    if v=='stolen_cost':entry['state']=['opponent_owned']
    setup.append(entry);entities.append(dict(name='Grizzly Bears',ref=ref,controller=pl,owner=1-owner if v=='stolen_cost' else pl,alive=True,tapped=False,sick=False))
  def add_hand(n,ref,pl):setup.append(card(n,ref,pl,'hand'));zones['hand'][pl].append(n)
  if v=='countered_source':add_hand('Counterspell','counter',0)
  if v=='repeat':mana[owner]=[30]*6
  if v in ('payment','cast_payment'):mana[owner]=list(p['mana'] if v=='cast_payment' else p['activation_mana'])
  if v in ('insufficient_mana','insufficient_cast_mana'):mana[owner]=[0]*6
  if v=='wrong_color':mana[owner]=[sum(p['activation_mana']),0,0,0,0,0]
  if v in ('repeat','payment','cast_payment','insufficient_mana','insufficient_cast_mana','wrong_color'):pre.extend(step('set_mana',player=owner,color=color,value=value) for color,value in enumerate(mana[owner]))
  def pay(cost,pl=owner):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  if casting:
   if v=='countered_source':actions.append(step('choose_priority',player=0,ref='counter'))
   actions.append(step('cast',ref='source',value=int(v not in ('insufficient_cast_mana','countered_source'))))
   if v!='insufficient_cast_mana':
    pay(p['mana']);zones['hand'][owner].remove(name)
    if v=='countered_source':pay(cost_from_oracle(o['Counterspell']),0);zones['hand'][0].remove('Counterspell');zones['graveyard'][0].append('Counterspell');zones['graveyard'][owner].append(name)
    else:e['alive']=True;e['sick']=p['creature'];actions.append(step('assert_stack',value=1))
    actions.extend([step('resolve'),step('assert_choices_consumed')])
  def choose_targets():
   available=[n for n in zones['graveyard'][owner] if wave6_eligible(o[n],p)]
   if selection=='all':return available
   if v in ('zero','predicate_offer'):return []
   if selection=='paired':
    chosen=[]
    for typ in ('Artifact','Enchantment'):
     candidates=[n for n in zones['graveyard'][owner] if typ in o[n]['type_line'].partition(' — ')[0].split()]
     if candidates:chosen.append(candidates[-1] if v=='alternate' else candidates[0])
    return chosen
   if v=='alternate' and available:
    last=available[-1];available.remove(last);available.insert(0,last)
   return available[:p['maximum']]
  def pending_checks():
   checks=[step('assert_stack',value=1),step('assert_choices_consumed'),step('assert_mana_total',player=owner,value=sum(mana[owner])),step('assert_zone',player=owner,zone='hand',value=len(zones['hand'][owner]))]
   for pl in (0,1):
    for n in sorted(set([name,'Grizzly Bears','Giant Growth'])):checks.append(step('assert_zone_multiplicity',player=pl,zone='graveyard',index=rows[n]['csvid'],value=zones['graveyard'][pl].count(n)))
   if e['alive']:checks.append(observe('source','tapped',int(e['tapped'])))
   return checks
  def activate():
   ns=choose_targets();q=[];cost_card='Grizzly Bears' if c['discard']=='creature' or owner==1 or 'Giant Growth' not in zones['hand'][owner] else 'Giant Growth';sac=next((x for x in entities[1:] if x['alive'] and x['controller']==owner),None)
   hand_input=[step('choose_zone_card',player=owner,value=rows[cost_card]['csvid'])] if c['discard'] and (owner==0 or c['discard']=='creature') else [step('choose_dialog',player=owner,value=0)] if c['discard'] and policy.get('ai_discard_ack') else []
   creature_input=[step('choose_target',player=owner,ref=sac['ref'])] if c['creature_sacrifice'] and not (owner==1 and policy.get('ai_creature_sacrifice_auto')) else []
   if policy.get('discard_before_grave'):q.extend(hand_input)
   if policy.get('creature_before_grave'):q.extend(creature_input)
   if selection=='paired':
    for typ in ('Artifact','Enchantment'):
     n=next((n for n in ns if typ in o[n]['type_line'].partition(' — ')[0].split()),None)
     if n:q.append(step('choose_zone_card',player=owner,value=rows[n]['csvid']))
     elif zones['graveyard'][owner] or policy.get('sacrifice_before_selection'):q.append(step('choose_optional_zone',player=owner,value=rows['Sol Ring' if typ=='Artifact' else 'Seal of Cleansing']['csvid']))
   elif selection!='all':
    q.extend(step('choose_zone_card',player=owner,value=rows[n]['csvid']) for n in ns)
    if selection=='up_to' and len(ns)<min(p['maximum'],len(zones['graveyard'][owner])):q.append(step('choose_optional_zone',player=owner,value=rows[eligible[0]]['csvid']))
   if not policy.get('discard_before_grave'):q.extend(hand_input)
   if not policy.get('creature_before_grave'):q.extend(creature_input)
   offered=list(zones['graveyard'][owner])
   actions.extend([step('require_can_activate',ref='source',value=1)]+q+[step('activate',ref='source',value=1)]);pay(p['activation_mana'])
   if v=='predicate_offer':
    actions.extend([step('assert_offered_count',value=len(offered)),step('assert_offered_selection_required',value=0)])
    actions.extend(step('assert_offered_legal_card',index=rows[n]['csvid'],value=int(wave6_eligible(o[n],p))) for n in dict.fromkeys(offered))
   if c['discard']:zones['hand'][owner].remove(cost_card);zones['graveyard'][owner].append(cost_card)
   if c['creature_sacrifice']:sac['alive']=False;zones['graveyard'][sac['owner']].append(sac['name'])
   if c['tap']:e['tapped']=True
   if c['self_sacrifice']:e['alive']=False;zones['graveyard'][e['owner']].append(name)
   actions.extend(pending_checks())
   if v=='destroy_source_pending':actions.extend([step('destroy',ref='source'),step('refresh')]);e['alive']=False;zones['graveyard'][owner].append(name)
   actions.extend([step('resolve'),step('assert_choices_consumed')])
   for n in ns:zones['graveyard'][owner].remove(n);zones['hand'][owner].append(n)
  denied=v in ('insufficient_mana','wrong_color','tapped','sick','empty_hand','wrong_cost_type','missing_cost','opponent_cost') or v=='fresh_activation' and c['tap'] and p['creature'] or p['minimum'] and not any(wave6_eligible(o[n],p) for n in zones['graveyard'][owner])
  if not cast_only:
   if denied:actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_choices_consumed')])
   else:
    activate()
    if v=='repeat':activate()
   if v=='cleanup':actions.extend([step('cleanup'),step('resolve'),step('assert_choices_consumed')])
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0)]
  for ent in entities:
   if ent['alive']:
    checks.extend([observe(ent['ref'],'csvid',rows[ent['name']]['csvid']),observe(ent['ref'],'in_play',1),observe(ent['ref'],'tapped',int(ent['tapped']))])
    if 'Creature' in o[ent['name']]['type_line']:checks.extend(observe(ent['ref'],k,int(o[ent['name']][k])) for k in ('power','toughness'));checks.extend([observe(ent['ref'],'sick',int(ent['sick'])),observe(ent['ref'],'damage',0),observe(ent['ref'],'plus1_counters',0),observe(ent['ref'],'minus1_counters',0)])
  universe=set(witnesses)|{name}
  for pl in (0,1):
   checks.extend([step('assert_life',player=pl,value=life[pl]),step('assert_poison',player=pl,value=0),step('assert_zone',player=pl,zone='battlefield',value=sum(x['alive'] and x['controller']==pl for x in entities))])
   if v!='cleanup':checks.append(step('assert_mana_total',player=pl,value=sum(mana[pl])))
   if v in ('payment','cast_payment','wrong_color','insufficient_mana','insufficient_cast_mana'):checks.extend(step('assert_mana',player=pl,color=color,value=value) for color,value in enumerate(mana[pl]))
   for z in zones:
    values=zones[z][pl];counts=Counter(values);checks.append(step('assert_zone',player=pl,zone=z,value=len(values)))
    checks.extend(step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(values)) if z=='library' else checks.extend(step('assert_zone_contains',player=pl,zone=z,value=rows[n]['csvid']) for n in counts)
    checks.extend(step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=counts[n]) for n in sorted(universe))
  seq=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=0 if v=='ready_snapshot' else next(i for i,s in enumerate(actions) if s['op']=='resolve')
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq.extend([step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')])
  gaps=['Activated-ability countering and grave-target removal in response are untested; cast_response currently requires a pending spell','Replacement effects and simultaneous deaths beyond the selected ordinary cost creature','Only two players represented']
  if selection=='paired':gaps.append('Selecting the same artifact-enchantment physical card for both separate target instances')
  out.append(dict(name=f'GraveyardRecoveryWave6Recipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family=p['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out


# Wave6 uses a separate source/witness universe. No old fixture palette changes.
_WAVE6_INITIAL_WITNESSES=('Grizzly Bears','Hill Giant','Ornithopter','Sol Ring','Mana Vault','Howling Mine','Fellwar Stone','Giant Growth','Divination','Lightning Bolt','Seal of Cleansing','Forest','Plains','Island','Mountain','Counterspell','Reclaim','Unsummon','Cloudshift','Swords to Plowshares')


def wave6_exact(a,b):
 if type(a) is not type(b):return False
 if isinstance(a,dict):return a.keys()==b.keys() and all(wave6_exact(a[k],b[k]) for k in a)
 if isinstance(a,list):return len(a)==len(b) and all(wave6_exact(x,y) for x,y in zip(a,b))
 return a==b


def wave6_pins():
 return json.loads((Path(__file__).parent/'data/graveyard_recovery_wave6_witnesses.json').read_text(encoding='utf8'))


def wave6_eligible(r,p,ignore_value=False):
 types=set(r['type_line'].partition(' — ')[0].split())
 subs=set(r['type_line'].partition(' — ')[2].split());colors=set(r.get('colors',[]));scope=p['scope'];changeling='Changeling' in r.get('keywords',[])
 valid=(True if scope=='card' else bool(types&{'Artifact','Creature','Enchantment','Land','Planeswalker','Battle'}) if scope=='permanent'
  else bool(types&{'Instant','Sorcery'}) if scope=='instant_or_sorcery'
  else bool(types&{'Instant','Sorcery','Creature'}) if scope=='instant_sorcery_creature'
  else bool(types&{'Artifact','Enchantment'}) if scope=='artifact_or_enchantment'
  else bool(subs&{'Aura','Equipment'}) if scope=='aura_equipment'
  else 'Creature' in types and bool(colors&{'U','R'}) if scope=='blue_red_creature'
  else scope.title() in subs or changeling if scope in ('spirit','zombie') else scope.title() in types)
 return valid and (ignore_value or p['value_max'] is None or r['cmc']<=p['value_max'])


def wave6_clause(p,r):
 noun={'card':'','creature':'creature','artifact':'artifact','enchantment':'enchantment','land':'land','permanent':'permanent','instant_sorcery_creature':'instant, sorcery, or creature','aura_equipment':'Aura or Equipment','blue_red_creature':'blue or red creature','spirit':'Spirit','zombie':'Zombie'}[p['scope']]
 noun=(noun+' ') if noun else ''
 if p.get('soulshift'):
  n=p['value_max'];return f'Soulshift {n} (When this creature dies, you may return target Spirit card with mana value {n} or less from your graveyard to your hand.)'
 if p.get('random_recovery'):return f'When this creature enters, return a {noun}card at random from your graveyard to your hand.'
 quantity=('up to two' if p['minimum']==0 else 'two')+' target ' if p['maximum']==2 else 'target '
 text='return '+quantity+noun+'card'+('s' if p['maximum']==2 else '')+(f" with mana value {p['value_max']} or less" if p['value_max'] is not None else '')+' from your graveyard to your hand.'
 if p['mode']=='spell':text=text[0].upper()+text[1:]
 elif p['mode']=='entry':
  text='When this creature enters, '+('if you have four or more creature cards in your graveyard, ' if p.get('minimum_grave_creatures') else '')+('you may ' if p.get('optional') else '')+text
 elif p['mode']=='death':text='When this creature dies, return another target '+noun+'card from your graveyard to your hand.'
 elif p['mode']=='all_battlefield':text=f'Each player returns all {noun}cards from their graveyard to the battlefield.'
 if p['life_mana_value']:text+=" You gain life equal to that card's mana value."
 if p.get('self_destination')=='exile':text+=' Exile '+r['name']+'.'
 if p.get('source_death_destination')=='exile':text+='\nIf this creature would die, exile it instead.'
 return text


def wave6_spec(d,r,family):
 from recipes import cost_from_oracle
 pins=wave6_pins();profile=pins['profiles'].get(r.get('name')) if isinstance(r,dict) else None
 if (not profile or d.get('family')!=family or d.get('version')!=2 or profile['family']!=family
  or not wave6_exact(r,profile['source']) or d.get('oracle_text')!=r['oracle_text'] or not wave6_exact(d.get('params'),profile['params'])
  or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')):
  raise ValueError('Wave6 recovery requires whole source pins and exact opt-in parameters')
 p=copy.deepcopy(profile['params'])
 if p.get('mode')=='spell_extended':return wave6_spell_spec(d,r,family,p,profile)
 if p.get('mode')=='activation':return wave6_activation_spec(d,r,family,p,profile)
 if p['wave']!=6 or p['schema']!=3 or p['scope'] not in ('card','creature','artifact','enchantment','land','permanent','instant_sorcery_creature','aura_equipment','blue_red_creature','spirit','zombie') or p['mode'] not in ('spell','entry','death','all_battlefield'):
  raise ValueError('unreviewed Wave6 recovery mechanism')
 if type(p['minimum']) is not int or type(p['maximum']) is not int or (p['minimum'],p['maximum']) not in ((0,0),(1,1),(0,2),(2,2)):
  raise ValueError('recovery quantity must match the printed exact/up-to boundary')
 if p.get('minimum_grave_creatures') is not None and (type(p['minimum_grave_creatures']) is not int or p['minimum_grave_creatures']!=4):raise ValueError('reviewed creature-grave condition is exactly four')
 for flag in ('life_mana_value','optional','soulshift','random_recovery','legacy_optional_trigger_prompt','legacy_cast_speed_prompt'):
  if flag in p and type(p[flag]) is not bool:raise ValueError('recovery flags require explicit booleans')
 clause=wave6_clause(p,r)
 passive='\n'.join(p['inactive_components'])+('\n' if p['inactive_components'] else '') if 'inactive_components' in p else 'Flying\n' if r.get('keywords')==['Flying'] else ''
 if r['oracle_text']!=passive+clause:raise ValueError('Wave6 parameters disagree with complete operative Oracle text')
 creature='Creature' in r['type_line']
 if creature!=(p['mode'] in ('entry','death')) or not creature and r['type_line'] not in ('Instant','Sorcery'):
  raise ValueError('Wave6 recovery source type disagrees with mechanism')
 if creature and (not str(r.get('power','')).isdigit() or not str(r.get('toughness','')).isdigit() or int(r['toughness'])<1):raise ValueError('bounded surviving Wave6 source body required')
 p.update(_wave6=True,family=family,mana=cost_from_oracle(r),creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0)
 if profile.get('inputs'):
  inputs=profile['inputs']
  if not isinstance(inputs,dict) or set(inputs)-{'optional_empty_selector_ai'} or any(type(v) is not bool for v in inputs.values()):raise ValueError('unreviewed recovery input policy')
  p['_inputs']=copy.deepcopy(inputs)
 selected=d.get('variants')
 if not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or len(selected)!=len(set(selected)) or set(selected)-set(wave6_variants(p)):
  raise ValueError('explicit distinct Wave6 recovery variants required')
 return p


def wave6_variants(p):
 if p.get('mode')=='spell_extended':return wave6_spell_variants(p)
 if p.get('mode')=='activation':return wave6_activation_variants(p)
 modes=['effect','alternate','payment','insufficient_mana','empty','opponent_only','wrong_type','duplicates','cleanup','repeat','ready_snapshot','pending_snapshot','resolved_snapshot','bare_zones','countered']
 if any(p['mana'][1:]):modes.append('wrong_color')
 if p['mode']=='spell':
  modes+=['one_available','partial_loss']
  if p['minimum']<=1:modes.append('all_lost')
  if p['minimum']==0:modes.append('zero')
 if p['mode']=='entry':modes+=['destroy_source']
 if p['mode']=='entry' and p['maximum']==2:modes+=['one_available','zero']
 if p.get('optional'):modes.append('decline')
 if p.get('minimum_grave_creatures'):modes+=['condition_below','condition_boundary','condition_depleted']
 if p['scope']=='aura_equipment':modes+=['type_aura','type_equipment']
 if p['scope']=='blue_red_creature':modes+=['type_blue','type_red']
 if p['scope']=='instant_sorcery_creature':modes+=['type_instant','type_sorcery','type_creature']
 if p['scope']=='permanent':modes+=['type_artifact','type_creature','type_enchantment','type_land','type_planeswalker']
 if p['scope'] in ('spirit','zombie'):modes+=['kindred_card','changeling_card']
 if p['value_max'] is not None:modes+=['wrong_value','value_boundary']
 if p['mode']=='death':modes+=['cast_only','source_only','bounce_source','exile_source','other_death']
 if p['scope']=='card':modes.remove('wrong_type')
 return [f'{v}_{owner}' for owner in (0,1) for v in modes]


def wave6_validate_witnesses(o,rows):
 pins=wave6_pins()
 for n,r in pins['cards'].items():
  if n not in rows or not wave6_exact(o.get(n),r) or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face'):raise ValueError('Wave6 independent witness changed: '+n)
 for n,p in pins['profiles'].items():
  if n not in rows or not wave6_exact(o.get(n),p['source']):raise ValueError('Wave6 source witness changed: '+n)


def wave6_expand(name,csvid,d,p,selected,rows,o):
 """Real source actions with an independent zone/mana/ownership model."""
 if p['mode']=='spell_extended':return wave6_spell_expand(name,csvid,d,p,selected,rows,o)
 if p['mode']=='activation':return wave6_activation_expand(name,csvid,d,p,selected,rows,o)
 from recipes import card,step,observe,cost_from_oracle
 pins=wave6_pins();witnesses=tuple(pins['profiles'][name].get('witnesses',_WAVE6_INITIAL_WITNESSES));out=[]
 eligible=[n for n in witnesses if wave6_eligible(o[n],p)]
 # Include both zero and nonzero MV artifacts without making life gain vacuous.
 if p['life_mana_value']:eligible.sort(key=lambda n:(-o[n]['cmc'],n))
 if len(eligible)<2:raise ValueError('Wave6 needs two independent eligible identities')
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';mode=p['mode']
  zones={z:[[],[]] for z in ('hand','graveyard','library','exile')}
  for pl in (0,1):
   zones['hand'][pl]=[] if bare else ['Plains','Giant Growth']
   zones['graveyard'][pl]=list(eligible[:2]) if bare else ['Forest','Giant Growth','Divination','Seal of Cleansing','Grizzly Bears','Hill Giant','Sol Ring','Ornithopter','Howling Mine','Mana Vault']
   zones['library'][pl]=[] if bare else ['Forest','Divination','Island','Lightning Bolt']
   zones['exile'][pl]=[] if bare else ['Mountain','Giant Growth']
  if 'witnesses' in pins['profiles'][name]:
   for pl in (0,1):
    for n in eligible[:max(2,p['maximum'])]:
     if n not in zones['graveyard'][pl]:zones['graveyard'][pl].append(n)
  if p.get('random_recovery'):
   target=eligible[-1] if v=='alternate' else eligible[0]
   for pl in (0,1):zones['graveyard'][pl]=[n for n in zones['graveyard'][pl] if not wave6_eligible(o[n],p)]+[target,target]
  if p.get('minimum_grave_creatures'):
   while sum('Creature' in o[n]['type_line'] for n in zones['graveyard'][owner])<p['minimum_grave_creatures']+int(v=='repeat'):zones['graveyard'][owner].append('Grizzly Bears')
  if mode=='all_battlefield':
   # Different names per player expose returning the whole pool to the caster.
   zones['graveyard'][owner]=['Sol Ring','Ornithopter','Giant Growth','Forest']
   zones['graveyard'][1-owner]=['Howling Mine','Mana Vault','Hill Giant','Plains']
   if bare:
    zones['graveyard'][owner]=['Sol Ring'];zones['graveyard'][1-owner]=['Howling Mine']
   if p['scope']=='creature':
    zones['graveyard'][owner]=['Grizzly Bears','Ornithopter','Giant Growth','Forest']
    zones['graveyard'][1-owner]=['Hill Giant','Coral Merfolk','Divination','Plains']
    if bare:zones['graveyard'][owner]=['Grizzly Bears'];zones['graveyard'][1-owner]=['Hill Giant']
  if v in ('empty','opponent_only','source_only'):zones['graveyard'][owner]=[]
  if v=='empty':zones['graveyard'][1-owner]=[]
  if v=='wrong_type':zones['graveyard'][owner]=[n for n in ('Forest','Giant Growth','Divination','Seal of Cleansing','Grizzly Bears','Sol Ring') if not wave6_eligible(o[n],p,True)]
  if v=='wrong_value':zones['graveyard'][owner]=[n for n in witnesses if wave6_eligible(o[n],p,True) and not wave6_eligible(o[n],p)]
  if v=='condition_below':zones['graveyard'][owner]=['Grizzly Bears','Hill Giant','Ornithopter','Forest']
  if v=='condition_boundary':zones['graveyard'][owner]=['Grizzly Bears','Grizzly Bears','Hill Giant','Ornithopter','Forest']
  if v=='one_available':zones['graveyard'][owner]=['Forest',eligible[0],'Giant Growth']
  if v=='one_available' and 'witnesses' in pins['profiles'][name]:
   zones['graveyard'][owner]=[n for n in ('Forest','Giant Growth','Divination','Sol Ring','Grizzly Bears') if not wave6_eligible(o[n],p)][:2]+[eligible[0]]
  if v=='repeat' and mode=='spell' and p['minimum']:
   # Each exact-quantity cast needs its own legal original targets.
   while sum(wave6_eligible(o[n],p) for n in zones['graveyard'][owner])<2*p['minimum']:
    zones['graveyard'][owner].append(eligible[0])
  if v=='duplicates':zones['graveyard'][owner]=['Forest',eligible[0],eligible[0],'Giant Growth']
  target=eligible[-1] if v=='alternate' else eligible[0]
  if v.startswith('type_'):
   kind=v[5:];target=next(n for n in eligible if kind.title() in o[n]['type_line'].split() or kind=='blue' and 'U' in o[n]['colors'] or kind=='red' and 'R' in o[n]['colors'])
  if v=='kindred_card':target='Nameless Inversion'
  if v=='changeling_card':target='Mothdust Changeling'
  if v=='value_boundary':target=next(n for n in eligible if o[n]['cmc']==p['value_max'])
  if v=='value_boundary' or v=='alternate' or v.startswith('type_') or v in ('kindred_card','changeling_card'):
   if target not in zones['graveyard'][owner]:zones['graveyard'][owner].append(target)
  if p.get('random_recovery') and v in ('kindred_card','changeling_card'):
   zones['graveyard'][owner]=[n for n in zones['graveyard'][owner] if not wave6_eligible(o[n],p)]+[target,target]
  available=[n for n in zones['graveyard'][owner] if wave6_eligible(o[n],p)]
  if v=='duplicates':chosen=[eligible[0]]*min(p['maximum'] or 1,2)
  else:
   chosen=list(available[:p['maximum']])
   if (mode in ('entry','death') and p['maximum']==1 or 'witnesses' in pins['profiles'][name] and v=='alternate') and target in available:
    remaining=list(available);remaining.remove(target);chosen=[target]+remaining[:max(0,p['maximum']-1)]
  if v=='zero':chosen=[]
  if v=='decline':chosen=[]
  if p.get('minimum_grave_creatures') and sum('Creature' in o[n]['type_line'] for n in zones['graveyard'][owner])<p['minimum_grave_creatures']:chosen=[]
  setup=[];pre=[step('turn',player=owner,value=0)];actions=[]
  for pl in (0,1):
   for z in zones:setup += [card(n,f'w6_{pl}_{z}_{i}',pl,z) for i,n in enumerate(zones[z][pl])]
  def add_hand(n,ref,pl):setup.append(card(n,ref,pl,'hand'));zones['hand'][pl].append(n)
  add_hand(name,'source',owner)
  if v in ('repeat','condition_depleted'):add_hand(name,'source2',owner)
  if v=='countered':add_hand('Counterspell','counter',0)
  if v in ('partial_loss','all_lost'):add_hand('Reclaim','response',owner)
  if v=='bounce_source':add_hand('Unsummon','bounce',owner)
  if v=='exile_source':
   # Real exile uses a simple spell; source death must not be synthesized.
   add_hand('Swords to Plowshares','exile_spell',owner)
  mana=[[10]*6,[10]*6];life=[20,20];entities=[]
  if v in ('repeat','condition_depleted'):mana[owner]=[30]*6
  if v=='payment':mana[owner]=list(p['mana'])
  if v=='insufficient_mana':mana[owner]=[0]*6
  if v=='wrong_color':mana[owner]=[sum(p['mana']),0,0,0,0,0]
  if v in ('repeat','condition_depleted','payment','insufficient_mana','wrong_color'):pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  def pay(n,pl=owner):mana[pl]=[a-b for a,b in zip(mana[pl],cost_from_oracle(o[n]))]
  def choices(ns,zero=False):
   if p.get('random_recovery'):return []
   if v=='decline' and owner==0 and p.get('legacy_optional_trigger_prompt'):return []
   q=[step('choose_zone_card',player=owner,value=rows[n]['csvid']) for n in ns]
   # The actual up-to selector still offers Done when only wrong types remain.
   if zero or (mode=='spell' and p['minimum']==0 or p.get('legacy_fixed_selection_count')) and len(ns)<min(p['maximum'],len(zones['graveyard'][owner])):
    q.append(step('choose_optional_zone',player=owner,value=rows[eligible[0]]['csvid']))
   return q
  def entry_choices(ref,ns):
   if owner==1 and p.get('_inputs',{}).get('optional_empty_selector_ai') and not any(wave6_eligible(o[n],p) for n in zones['graveyard'][owner]) and zones['graveyard'][owner]:
    return [step('choose_optional_zone',player=owner,value=rows[eligible[0]]['csvid'])]
   prefix=[step('choose_trigger',player=owner,ref=ref)] if owner==0 and p.get('legacy_optional_trigger_prompt') and ns else []
   return prefix+choices(ns,v in ('zero','decline') and any(wave6_eligible(o[n],p) for n in zones['graveyard'][owner]))
  def recover(ns):
   for n in ns:
    zones['graveyard'][owner].remove(n);zones['hand'][owner].append(n)
    if p['life_mana_value']:life[owner]+=int(o[n]['cmc'])
  def returned():
   for pl in (0,1):
    for n in list(zones['graveyard'][pl]):
     if wave6_eligible(o[n],p):
      zones['graveyard'][pl].remove(n);entities.append(dict(name=n,owner=pl,controller=pl,ref=f'returned_{len(entities)}',alive=True,returned=True))
  def complete(ref,ns):
   casting=list(ns);is_counter=v=='countered'
   if mode=='entry' and p.get('minimum_grave_creatures') and sum('Creature' in o[n]['type_line'] for n in zones['graveyard'][owner])<p['minimum_grave_creatures']:
    casting=[]
   if mode=='spell':
    if not is_counter:actions.extend(choices(casting,v=='zero'))
    else:actions.extend(choices(casting))
   if owner==0 and p.get('legacy_cast_speed_prompt') and sum(mana[owner])-sum(p['mana'])>=2:
    actions.append(step('choose_dialog',player=owner,value=0))
   if is_counter:actions.append(step('choose_priority',player=0,ref='counter'))
   if not p['creature']:actions.append(step('require_can_cast',ref=ref,value=1))
   actions.append(step('cast',ref=ref,value=0 if is_counter else 1));pay(name);zones['hand'][owner].remove(name)
   if is_counter:
    pay('Counterspell',0);zones['hand'][0].remove('Counterspell');zones['graveyard'][0].append('Counterspell');zones['graveyard'][owner].append(name)
    actions.extend([step('resolve'),step('assert_choices_consumed')]);return
   actions.append(step('assert_stack',value=1))
   if mode=='entry':actions.extend(entry_choices(ref,casting))
   if v in ('partial_loss','all_lost'):
    lost=casting[0]
    actions.extend([step('choose_zone_card',player=owner,value=rows[lost]['csvid']),step('cast_response',ref='response',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_stack',value=0),step('assert_choices_consumed')])
    pay('Reclaim');zones['hand'][owner].remove('Reclaim');zones['graveyard'][owner].append('Reclaim');zones['graveyard'][owner].remove(lost);zones['library'][owner].insert(0,lost)
    casting.remove(lost)
   else:actions.extend([step('resolve'),step('assert_choices_consumed')])
   if mode in ('entry','death'):
    entity=dict(name=name,owner=owner,controller=owner,ref=ref,alive=True,returned=False);entities.append(entity)
    if mode=='entry':recover(casting)
    if mode=='death' and v not in ('cast_only','bounce_source','exile_source','other_death'):
     actions.extend(choices(casting,v=='decline' and bool(available)));actions.extend([step('destroy',ref=ref),step('refresh'),step('resolve'),step('assert_choices_consumed')]);entity['alive']=False;zones['graveyard'][owner].append(name);recover(casting)
   elif mode=='all_battlefield':returned();zones['graveyard'][owner].append(name)
   else:recover(casting);zones[p.get('self_destination','graveyard')][owner].append(name)
  legal=mode!='spell' or len(available)>=p['minimum']
  denied=v in ('insufficient_mana','wrong_color') or not legal
  if denied:
   if not legal:actions.append(step('require_can_cast',ref='source',value=0))
   actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
  else:
   if v=='all_lost':chosen=chosen[:1]
   complete('source',chosen)
   if v in ('repeat','condition_depleted'):
    ns=[n for n in zones['graveyard'][owner] if wave6_eligible(o[n],p) and (mode!='death' or n!=name)][:p['maximum']]
    complete('source2',ns)
   if v=='destroy_source':
    actions.extend([step('destroy',ref='source'),step('refresh')]);entities[0]['alive']=False;zones[p.get('source_death_destination','graveyard')][owner].append(name)
   if v in ('bounce_source','exile_source'):
    spell,ref=('Unsummon','bounce') if v=='bounce_source' else ('Swords to Plowshares','exile_spell')
    actions.extend([step('choose_target',player=owner,ref='source'),step('cast',ref=ref,value=1),step('resolve'),step('assert_choices_consumed')]);pay(spell);zones['hand'][owner].remove(spell);zones['graveyard'][owner].append(spell);entities[0]['alive']=False
    zones['hand' if v=='bounce_source' else 'exile'][owner].append(name)
    if v=='exile_source':life[owner]+=p['power']
   if v=='other_death':
    setup.append(card('Grizzly Bears','other',owner));actions.extend([step('destroy',ref='other'),step('refresh'),step('resolve'),step('assert_choices_consumed')]);zones['graveyard'][owner].append('Grizzly Bears')
   if v=='cleanup':actions.extend([step('cleanup'),step('resolve'),step('assert_choices_consumed')])
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0)]
  copies=Counter()
  for e in entities:
   if not e['alive']:continue
   if e['returned']:
    key=e['controller'],e['name'];checks.append(step('bind_permanent_copy',ref=e['ref'],player=e['controller'],value=rows[e['name']]['csvid'],index=copies[key]));copies[key]+=1
   checks.extend([observe(e['ref'],'in_play',1),observe(e['ref'],'tapped',0)])
   if 'Creature' in o[e['name']]['type_line']:
    checks.extend([observe(e['ref'],key,int(o[e['name']][key])) for key in ('power','toughness')]);checks.extend([observe(e['ref'],'damage',0),observe(e['ref'],'plus1_counters',0),observe(e['ref'],'minus1_counters',0),observe(e['ref'],'sick',1)])
  universe=set(witnesses)|{name}
  for pl in (0,1):
   checks.extend([step('assert_life',player=pl,value=life[pl]),step('assert_poison',player=pl,value=0),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['controller']==pl for e in entities))])
   if v!='cleanup':checks.append(step('assert_mana_total',player=pl,value=sum(mana[pl])))
   if v in ('payment','insufficient_mana','wrong_color'):checks.extend(step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl]))
   for z in zones:
    ns=zones[z][pl];counts=Counter(ns);checks.append(step('assert_zone',player=pl,zone=z,value=len(ns)))
    checks.extend(step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(ns)) if z=='library' else checks.extend(step('assert_zone_contains',player=pl,zone=z,value=rows[n]['csvid']) for n in counts)
    checks.extend(step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=counts[n]) for n in sorted(universe))
  seq=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=0 if v=='ready_snapshot' else next(i for i,s in enumerate(actions) if s['op']=='resolve')
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq.extend([step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')])
  gaps=['Replacement effects, simultaneous deaths and multiplayer beyond two players','Entry/death trigger target loss during stack resolution; ordinary spell target loss is explicit']
  if p.get('inactive_components'):gaps+=['Inactive in these source-action scenarios: '+s for s in p['inactive_components']]
  if p['scope']=='permanent':gaps+=['Battle cards are not represented by the engine witness registry']
  out.append(dict(name=f'GraveyardRecoveryWave6Recipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family=p['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out
