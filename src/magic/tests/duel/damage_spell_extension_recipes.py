"""Reviewed fixed damage components using ordinary real-cast damage fixtures."""
import copy
import json
from pathlib import Path
import damage_recipes as targeted
import sweep_recipes as sweeping

PINS=json.loads((Path(__file__).parent/'data/damage_spell_extension_witnesses.json').read_text(encoding='utf8'))
PROFILES=PINS['profiles']

def spec(d,o):
 from recipes import cost_from_oracle
 profile=PROFILES.get(o.get('name')) if isinstance(o,dict) else None
 if (not profile or o.get('layout')!='normal' or o.get('card_faces') or o.get('selected_face')
     or d.get('family')!='damage_spell_extension' or d.get('version')!=2
     or d.get('params')!=profile['params'] or d.get('oracle_text')!=profile['source']['oracle_text']
     or any(o.get(k)!=v for k,v in profile['source'].items())
     or not isinstance(d.get('variants'),list) or not d['variants']):
  raise ValueError('damage extension requires complete source pin, reviewed explicit parameters and variants')
 p=copy.deepcopy(profile['params']);p.update(family='damage_spell_extension',mana=cost_from_oracle(o),instant=o['type_line']=='Instant')
 if p['kind']=='targeted':targeted.spec(p['effect'],p['mana'])
 elif p['kind']=='sweep':sweeping.spec(p['effect'],p['mana'])
 else:raise ValueError('unsupported reviewed damage component')
 available=variants(p)
 if any(not isinstance(v,str) or v not in available for v in d['variants']) or len(d['variants'])!=len(set(d['variants'])):
  raise ValueError('unknown or duplicate damage extension variant')
 return p

def variants(p):
 q=dict(p['effect'],mana=p['mana'])
 if p['kind']=='targeted':
  base=targeted.variants(q,PINS['cards'])
  base+=['pending_snapshot','wrong_payment_color']
  if p['instant'] and 'other_turn' not in base:base+=['other_turn']
 else:
  base=sweeping.variants(q)+['pending_snapshot','wrong_payment_color']
  if p['instant']:base+=['other_turn']
 base=[v for v in base if v not in p['omit_variants']]
 return [v+'_'+str(owner) for owner in (0,1) for v in base]

def validate_witnesses(o,registry):
 targeted.validate_witnesses(o,registry)
 for name,pin in PINS['cards'].items():
  record=o.get(name,{})
  if name not in registry or record.get('card_faces') or record.get('selected_face') or any(record.get(k)!=v for k,v in pin.items()):
   raise ValueError('damage extension witness changed: '+name)

def _pending_snapshot(steps):
 from recipes import step
 capture=next(i for i,s in enumerate(steps) if s['op']=='capture')
 record=next(i for i,s in enumerate(steps) if s['op']=='record_state')
 first=steps[capture+1:record]
 cut=next(i for i,s in enumerate(first) if s['op']=='assert_stack' and s['value']==1)+1
 tail=first[cut:]
 return steps[:capture]+first[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]

def _normal_cast_choices(steps,p,mapped):
 from recipes import step
 out=[]
 negative=mapped in ('insufficient_mana','empty','wrong_type','wrong_color','wrong_flying','wrong_controller','noncombat')
 for s in steps:
  if s['op']=='cast' and s.get('ref')=='source' and not negative:
   at=len(out)
   while at and out[at-1]['op'].startswith('choose_'):at-=1
   before=[] if mapped=='payment' and p.get('skip_before_choices_on_exact_payment') else [step('choose_dialog',player=0,value=n) for n in p['choices_before_target']]
   out[at:at]=before
   if mapped!='payment' or not p['skip_after_choices_on_exact_payment']:
    out.extend(step('choose_dialog',player=0,value=n) for n in p['choices_after_target'])
  out.append(s)
 return out

def expand(name,csvid,d,p,selected,registry,o):
 from recipes import step
 if len(selected)!=len(set(selected)) or any(v not in variants(p) for v in selected):raise ValueError('unknown or duplicate damage extension variant')
 result=[]
 for requested in selected:
  branch,owner=requested.rsplit('_',1);owner=int(owner)
  mapped={'pending_snapshot':'snapshot','wrong_payment_color':'insufficient_mana','other_turn':'survivor' if p['kind']=='targeted' else 'mixed'}.get(branch,branch)
  q=dict(p['effect'],mana=list(p['mana']))
  if p['kind']=='targeted':case=targeted.expand(name,csvid,d,q,[mapped],registry,o)[0]
  else:case=sweeping.expand(name,csvid,d,q,[mapped],registry,o)[0]
  # Ordinary snapshot fixtures reuse step objects on replay. Break that alias
  # before changing controllers or inserting presentation inputs.
  case['setup']=[copy.deepcopy(s) for s in case['setup']]
  steps=[copy.deepcopy(s) for s in case['steps']]
  if p['kind']=='targeted' and any(s['ref']=='recipient' for s in case['setup']):
   controlled=[]
   for s in steps:
    if s['op']=='cast' and s.get('ref')=='source' and s['value']==1:
     at=len(controlled)
     while at and controlled[at-1]['op'].startswith('choose_'):at-=1
     controlled.insert(at,step('assert_card',ref='recipient',property='damage',value=0))
    controlled.append(s)
   steps=controlled
  if p.get('gain_life'):
   gained=0;pending=False
   for s in steps:
    if s['op']=='restore':gained=0;pending=False
    if s['op']=='cast' and s.get('ref')=='source' and s['value']==1:pending=True
    if s['op']=='resolve' and pending:gained+=p['gain_life'];pending=False
    if s['op']=='assert_life' and s['player']==0:s['value']+=gained
  if mapped=='lost_target':
   steps=[s for s in steps if s['op']!='choose_priority' and not (s['op']=='choose_target' and s.get('player')==-1)]
   cut=next(i for i,s in enumerate(steps) if s['op']=='assert_stack' and s['value']==1)+1
   steps[cut:cut]=[step('choose_target',player=0,ref='recipient'),step('cast_response',ref='response',value=1),step('assert_stack',value=2)]
   for s in steps:
    if s['op']=='resolve':s['op']='resolve_response';break
  if mapped in ('regenerate','regeneration_expired'):
   for s in steps:
    if s['op']=='choose_priority':s['op']='choose_regeneration'
  steps=_normal_cast_choices(steps,p,mapped)
  if branch=='wrong_payment_color':
   wrong=list(p['mana']);color=next(i for i in range(1,6) if wrong[i]);wrong[2 if color!=2 else 3]+=wrong[color];wrong[color]=0
   for s in steps:
    if s['op']=='set_mana' and s.get('player')==0:s['value']=wrong[s['color']]
    if s['op']=='assert_mana_total' and s.get('player')==0:s['value']=sum(wrong)
   steps.extend(step('assert_mana',player=0,color=i,value=n) for i,n in enumerate(wrong))
  if branch=='pending_snapshot':steps=_pending_snapshot(steps)
  if branch=='other_turn':steps.insert(0,step('turn',player=1,value=0))
  if branch=='payment':steps.extend(step('assert_mana',player=0,color=i,value=0) for i in range(6))
  if owner:
   for item in case['setup']+steps:
    if item.get('player') in (0,1):item['player']=1-item['player']
    if item.get('op')=='choose_player':item['value']=1-item['value']
   steps.insert(0,step('turn',player=1,value=0))
  case.update(name=f'DamageSpellExtensionRecipe_{csvid}_{requested}',steps=steps)
  gaps=p['inactive_components']+(['planeswalker and battle targets'] if p['kind']=='targeted' and 'player' in p['effect']['targets'] else [])
  case['_recipe']=dict(family='damage_spell_extension',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],
    untested_components=gaps+['damage replacement and prevention','countered source spells and live control changes','multiplayer'])
  result.append(case)
 return result
