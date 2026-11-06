"""Oracle-pinned creature entry recovery using stocked zones and real casts."""
import re
from collections import Counter
from creature_recipes import KEYWORDS, REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY
from etb_removal_creature_recipes import SPELLS

TYPES=('artifact','creature','enchantment','instant','land','sorcery','planeswalker')
WITNESSES=('Grizzly Bears','Hill Giant','Giant Growth','Lightning Bolt','Divination','Mind Rot',
 'Sol Ring','Ornithopter','Seal of Cleansing','Seal of Primordium','Forest','Plains',
 'Nyx-Fleece Ram','Archetype of Courage','Jace Beleren')

def eligible(r,p):
 present={t for t in TYPES if t.title() in r['type_line']}
 return (not present.intersection(p['exclude_types']) and
  (p['targets']==['card'] or (all(t in present for t in p['targets']) if p['type_match']=='all' else any(t in present for t in p['targets']))))

def spec(d,r):
 if isinstance(d.get('params'),dict) and d['params'].get('wave')==6:
  from graveyard_recovery_extension_recipes import wave6_spec
  return wave6_spec(d,r,'etb_recovery_creature')
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or 'Creature' not in r.get('type_line','') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:
  raise ValueError('entry recovery requires a whole pinned normal creature and explicit variants')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'targets','type_match','exclude_types','optional','keywords'}:raise ValueError('explicit entry recovery types, exclusions, optionality and keywords required')
 scopes={('creature',):'creature',('artifact',):'artifact',('enchantment',):'enchantment',('instant',):'instant',('sorcery',):'sorcery',('land',):'land',('instant','sorcery'):'instant or sorcery',('creature','enchantment'):'creature or enchantment',('artifact','creature'):'artifact or creature',('instant','sorcery','creature'):'instant, sorcery, or creature',('card',):''}
 if not isinstance(p['targets'],list) or any(not isinstance(t,str) for t in p['targets']) or tuple(p['targets']) not in scopes or p['type_match'] not in ('any','all') or type(p['optional']) is not bool:raise ValueError('unreviewed recovery scope')
 if p['type_match']=='all':
  if p['targets']!=['creature','enchantment'] or p['exclude_types']:raise ValueError('only enchantment creature intersection reviewed')
  noun='enchantment creature'
 elif p['exclude_types']:
  if p['targets']!=['card'] or p['exclude_types']!=['creature','land']:raise ValueError('only noncreature nonland exclusion reviewed')
  noun='noncreature, nonland'
 else:noun=scopes[tuple(p['targets'])]
 if not isinstance(p['exclude_types'],list):raise ValueError('explicit type exclusion array required')
 priority=d.get('trigger_priority',False)
 if type(priority) is not bool or priority and not p['optional']:raise ValueError('priority input annotation requires an optional trigger')
 clause=('you may ' if p['optional'] else '')+'return target '+(noun+' ' if noun else '')+'card from your graveyard to your hand.'
 expected={f'When {ref} enters{suffix}, {clause}' for ref in ('this creature',r['name'],r['name'].split(',')[0]) for suffix in ('',' the battlefield')}
 canonical={k.lower():k for k in KEYWORDS+('Hexproof',)};keys=[];triggers=[];abilities=[]
 for line in r['oracle_text'].splitlines():
  if line in expected:triggers.append(line);continue
  if ':' in line and re.match(r'^(?:\{|Sacrifice |Discard |Remove |Exile |Pay |Tap |Untap |Return )',line):abilities.append(line);continue
  text=line
  for reminder in re.findall(r'\([^\n]*?\)',line):
   if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed entry recovery reminder')
   text=text.replace(reminder,'')
  for word in text.strip().split(','):
   if word.strip().lower() not in canonical:raise ValueError('unreviewed additional entry recovery clause')
   keys.append(canonical[word.strip().lower()])
 if len(triggers)!=1 or p['keywords']!=sorted(keys) or len(keys)!=len(set(keys)) or set(keys)!=set(r.get('keywords',[])):raise ValueError('entry recovery parameters disagree with whole passive scope')
 for k in ('power','toughness'):
  if not re.fullmatch(r'\d+',r.get(k,'')) or not 0<=int(r[k])<=10:raise ValueError('bounded numeric entry recovery source required')
 if int(r['toughness'])==0:raise ValueError('entry recovery source must survive entry')
 return dict(p,family='etb_recovery_creature',power=int(r['power']),toughness=int(r['toughness']),mana=cost_from_oracle(r),remaining_abilities=abilities)

def variants(p):
 if p.get('_wave6'):
  from graveyard_recovery_extension_recipes import wave6_variants
  return wave6_variants(p)
 modes=['entry','alternate','payment','insufficient_mana','empty_graveyard','other_graveyard','countered','cleanup_once','other_creature','bounce_only','bounce_recast','blink','destroy_source','ready_snapshot','pending_snapshot','resolved_snapshot','blink_snapshot','bare_zones','graveyard_shroud']
 if p['targets']!=['card'] or p['exclude_types']:modes.append('wrong_type')
 if p['optional']:modes.append('decline')
 if any(p['mana'][1:]):modes.append('wrong_color')
 if p['type_match']=='any':modes += ['type_'+t for t in (TYPES if p['targets']==['card'] else p['targets']) if t not in p['exclude_types']]
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]

def validate_witnesses(o,registry):
 from graveyard_recovery_extension_recipes import wave6_validate_witnesses
 wave6_validate_witnesses(o,registry)
 for nm in WITNESSES:
  r=o.get(nm,{})
  if nm not in registry or r.get('layout')!='normal' or not any(t.title() in r.get('type_line','') for t in TYPES):raise ValueError('entry recovery witness changed: '+nm)
 for nm,values in SPELLS.items():
  r=o.get(nm,{})
  if nm not in registry or (r.get('mana_cost'),r.get('oracle_text'))!=values:raise ValueError('entry recovery response witness changed: '+nm)
 if 'Ground Seal' not in registry or o.get('Ground Seal',{}).get('oracle_text')!="When this enchantment enters, draw a card.\nCards in graveyards can't be the targets of spells or abilities.":raise ValueError('graveyard protection witness changed')

def expand(name,csvid,d,p,selected,registry,oracle):
 if p.get('_wave6'):
  from graveyard_recovery_extension_recipes import wave6_expand
  return wave6_expand(name,csvid,d,p,selected,registry,oracle)
 from recipes import card,step,observe,cost_from_oracle
 result=[];candidates=[nm for nm in WITNESSES if eligible(oracle[nm],p)]
 if len(candidates)<2:raise ValueError('entry recovery needs two independently eligible witnesses')
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones'
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE)+list(WITNESSES) for _ in (0,1)]
  libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
  if bare:graves[owner]=list(candidates[:2])
  if v in ('empty_graveyard','other_graveyard'):graves[owner]=[]
  if v=='empty_graveyard':graves[1-owner]=[]
  if v=='wrong_type':graves[owner]=[nm for nm in WITNESSES if not eligible(oracle[nm],p)]
  if v=='graveyard_shroud':
   # Ground Seal is installed through its legal entry path, so its draw is real.
   hands[owner].append(libs[owner].pop(0))
  mana=[[10]*6,[10]*6];setup=[];pre=[step('turn',player=owner,value=0)];actions=[];alive=False;source_hand=True;extra_alive=False
  for player in (0,1):
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('library',libs[player]),('exile',exiles[player])):
    # Ground Seal's entry draw was accounted above, but happens during setup.
    original=list(names)
    if v=='graveyard_shroud' and player==owner and zone=='hand':original=original[:-1]
    if v=='graveyard_shroud' and player==owner and zone=='library':original=[LIBRARY[0]]+original
    setup += [card(nm,f'p{player}_{zone}_{i}',player,zone) for i,nm in enumerate(original)]
  if v=='graveyard_shroud':setup.append(card('Ground Seal','seal',owner))
  setup.append(card(name,'source',owner,'hand'));hands[owner].append(name)
  target=candidates[-1] if v=='alternate' else candidates[0]
  if v.startswith('type_'):target=next(nm for nm in candidates if v[5:].title() in oracle[nm]['type_line'])
  if v in ('payment','insufficient_mana','wrong_color'):
   mana[owner]=list(p['mana']) if v!='insufficient_mana' else [0]*6
   if v=='wrong_color':
    unused=next(c for c in range(1,6) if not p['mana'][c]);mana[owner]=[p['mana'][0],0,0,0,0,0];mana[owner][unused]=sum(p['mana'][1:])
   pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  def pay(nm,caster=owner):mana[caster]=[a-b for a,b in zip(mana[caster],cost_from_oracle(oracle[nm]))]
  def choice(nm,decline=False):
   if decline and d.get('trigger_priority',False) and owner==0:return []
   return ([step('choose_trigger',player=owner,ref='source')] if d.get('trigger_priority',False) and owner==0 else [])+([step('choose_optional_zone',player=owner,value=registry[nm]['csvid'])] if decline else [step('choose_zone_card',player=owner,value=registry[nm]['csvid'])])
  def receipt(nm):graves[owner].remove(nm);hands[owner].append(nm)
  def entry(nm,has_target=True,decline=False):
   nonlocal alive,source_hand
   pay(name);hands[owner].remove(name);alive=True;source_hand=False
   if has_target and not decline:receipt(nm)
   return [step('cast',ref='source',value=1),step('assert_stack',value=1)]+(choice(nm,decline) if has_target else [])+[step('resolve'),step('assert_choices_consumed')]
  def spell(nm,ref):
   setup.append(card(nm,ref,owner,'hand'));pay(nm);graves[owner].append(nm)
   return [step('choose_target',player=owner,ref='source'),step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
  if v in ('insufficient_mana','wrong_color'):actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
  elif v=='countered':
   setup.append(card('Essence Scatter','counter',0,'hand'));pay(name);pay('Essence Scatter',0);hands[owner].remove(name);graves[owner].append(name);graves[0].append('Essence Scatter');source_hand=False
   actions += [step('choose_priority',player=0,ref='counter'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
  else:
   has_target=any(eligible(oracle[nm],p) for nm in graves[owner]) and v!='graveyard_shroud'
   actions += entry(target,has_target,v=='decline')
   if v in ('bounce_only','bounce_recast'):
    actions += spell('Unsummon','bounce')+[step('bind_hand',player=owner,value=csvid,ref='source')];alive=False;source_hand=True;hands[owner].append(name)
    if v=='bounce_recast':actions += entry(candidates[1])
   if v in ('blink','blink_snapshot'):
    setup.append(card('Cloudshift','blink',owner,'hand'));pay('Cloudshift');graves[owner].append('Cloudshift')
    actions += [step('choose_target',player=owner,ref='source'),step('cast',ref='blink',value=1),step('assert_stack',value=1)]+choice(candidates[1])+[step('resolve'),step('assert_choices_consumed'),step('bind_permanent',player=owner,value=csvid,ref='source')];receipt(candidates[1])
   if v=='destroy_source':actions.append(step('destroy',ref='source'));alive=False;graves[owner].append(name)
   if v=='cleanup_once':actions.append(step('cleanup'))
   if v=='other_creature':
    setup.append(card('Grizzly Bears','other',owner,'hand'));pay('Grizzly Bears');extra_alive=True
    actions += [step('cast',ref='other',value=1),step('resolve'),step('assert_choices_consumed')]
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0),observe('source','in_play',int(alive)),observe('source','in_hand',int(source_hand))]
  if alive:checks += [observe('source','power',p['power']),observe('source','toughness',p['toughness']),observe('source','damage',0),observe('source','plus1_counters',0),observe('source','minus1_counters',0)]
  for player in (0,1):
   checks += [step('assert_life',player=player,value=20),step('assert_poison',player=player,value=0),step('assert_zone',player=player,zone='battlefield',value=int(player==owner)*(int(alive)+int(extra_alive)+int(v=='graveyard_shroud')))]
   if v!='cleanup_once':
    checks += [step('assert_mana_total',player=player,value=sum(mana[player]))]
    if v in ('payment','insufficient_mana','wrong_color'):checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
    checks += [step('assert_zone',player=player,zone=zone,value=len(names))]
    checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(names if zone=='library' else dict.fromkeys(names))]
    if zone in ('hand','graveyard'):
     universe=set(WITNESSES)|set(GRAVE)|set(HAND)|{name,'Unsummon','Cloudshift','Essence Scatter'}
     counts=Counter(names)
     checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=registry[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
   checks += [step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=n) for nm,n in Counter(hands[player]+graves[player]).items()]
  seq=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot','blink_snapshot'):
   cut=0 if v=='ready_snapshot' else max(i for i,s in enumerate(actions) if s['op']=='resolve')
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  result.append(dict(name=f'EtbRecoveryCreatureRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family=p['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['remaining_abilities']+p['keywords']+['target leaves after targeting','trigger ordering','alternative costs'])))
 return result
