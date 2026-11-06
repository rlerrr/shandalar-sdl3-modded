"""Whole-Oracle-pinned extensions of ordinary single-target entry removal.

The existing recipe owns real casting, payment, re-entry and zone receipts.
Reviewed profiles add explicit inactive clauses and target predicates while
retaining independently pinned witnesses and complete printed source text.
"""
import copy
import json
import re
from pathlib import Path
from collections import Counter
import etb_removal_creature_recipes as ordinary
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

PINS=json.loads((Path(__file__).parent/'data/etb_removal_extension_witnesses.json').read_text(encoding='utf8'))
PROFILES=PINS['profiles']


def _eligible(name,p,o):
 r=o[name];line=r['type_line'];pred=p['predicate']
 if not any(t.title() in line for t in p['targets']):return False
 return (pred is None
  or pred=='nonartifact_nonblack' and 'Artifact' not in line and 'B' not in r['colors']
  or pred=='nonblack' and 'B' not in r['colors']
  or pred=='noncreature' and 'Creature' not in line
  or pred=='flying' and 'Flying' in r['keywords']
  or pred=='equipment' and 'Equipment' in line
  or pred=='angel' and re.search(r'\bAngel\b',line) is not None
  or pred=='goblin' and re.search(r'\bGoblin\b',line) is not None
  or pred=='vampire_werewolf_zombie' and re.search(r'\b(?:Vampire|Werewolf|Zombie)\b',line) is not None
  or pred=='nonelf_unequal_stats' and re.search(r'\bElf\b',line) is None and r.get('power')!=r.get('toughness'))


def spec(d,r):
 from recipes import cost_from_oracle
 profile=PROFILES.get(r.get('name')) if isinstance(r,dict) else None
 if (not profile or r.get('card_faces') or r.get('selected_face') or d.get('family')!='etb_removal_extension' or d.get('version')!=2
     or d.get('params')!=profile['params'] or d.get('oracle_text')!=profile['source']['oracle_text']
     or any(r.get(k)!=v for k,v in profile['source'].items()) or not isinstance(d.get('variants'),list) or not d['variants']):
  raise ValueError('entry removal extension requires whole reviewed source and explicit parameters/variants')
 p=copy.deepcopy(profile['params']);p.update(family='etb_removal_extension',power=int(r['power']),toughness=int(r['toughness']),mana=cost_from_oracle(r),remaining_abilities=list(p['inactive_components']))
 available=variants(p)
 if any(not isinstance(v,str) for v in d['variants']) or len(set(d['variants']))!=len(d['variants']) or any(v not in available for v in d['variants']):raise ValueError('unknown or duplicate entry removal extension variant')
 return p


def variants(p):
 modes=['entry','own_target','payment','insufficient_mana','no_other_targets','wrong_type','countered','cleanup_once','other_creature',
        'bounce_only','bounce_recast','blink','destroy_source','ready_snapshot','pending_snapshot','resolved_snapshot','blink_snapshot','bare_zones']
 if len(p['targets'])==len(ordinary.TARGETS):modes.remove('wrong_type')
 if p['optional']:modes.append('decline')
 if p['opponent_only']:modes.remove('own_target');modes.append('own_only')
 if any(p['mana'][1:]):modes.append('wrong_color')
 modes += ['target_'+t for t in p['targets']]+['invalid_'+n for n in p['negative_witnesses']]
 modes=[v for v in modes if v not in p['excluded_variants']]
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]


def validate_witnesses(o,registry):
 if not isinstance(o,dict):raise ValueError('entry removal extension requires witness Oracle context')
 ordinary.validate_witnesses(o,registry)
 for name,pin in PINS['cards'].items():
  if name not in registry or any(o.get(name,{}).get(k)!=v for k,v in pin.items()):raise ValueError('entry removal extension witness changed: '+name)
 for name,profile in PROFILES.items():
  p=profile['params']
  positive=[n for t in p['targets'] for n in p['positive_witnesses'].get(t,ordinary.TARGETS[t])]
  if any(not _eligible(n,p,o) for n in positive) or any(_eligible(n,p,o) for n in p['negative_witnesses'].values()):raise ValueError('entry removal extension witness predicate disagrees with reviewed Oracle')
  if p['predicate'] and _eligible(name,p,o):raise ValueError('entry removal extension denied-target source must itself be ineligible')


def _no_target_case(case,p,branch,owner,rows,o):
 """Turn a reviewed positive board into a populated denied-target board.

 Remove both opposing target witnesses and their expected destination receipt.
 A predicate/control witness is present but must remain on the battlefield.
 """
 from recipes import card,step,observe
 removed={c['ref']:c for c in case['setup'] if c['ref'] in ('recipient','unselected')}
 victim=removed['recipient'];victim_id=rows[victim['name']]['csvid'];target_owner=victim['player'];zone='hand' if p['effect']=='bounce' else 'graveyard'
 case['setup']=[c for c in case['setup'] if c['ref'] not in removed]
 invalid=None;invalid_owner=1-owner
 if branch=='own_only':invalid=victim['name'];invalid_owner=owner
 elif branch.startswith('invalid_'):invalid=p['negative_witnesses'][branch[8:]]
 elif branch=='wrong_type':
  invalid=next(n for names in ordinary.TARGETS.values() for n in names if not any(t.title() in o[n]['type_line'] for t in p['targets']))
 if invalid:case['setup'].append(card(invalid,'invalid',invalid_owner))
 seen=set();out=[]
 for s in case['steps']:
  if s.get('ref') in removed:continue
  if id(s) not in seen:
   seen.add(id(s))
   if s['op']=='assert_zone' and s['player']==target_owner:
    if s['zone'] in ('battlefield',zone):s['value']-=1
   if invalid and s['op']=='assert_zone' and s['zone']=='battlefield' and s['player']==invalid_owner:s['value']+=1
   if s['op']=='assert_hand_graveyard_total' and s['player']==target_owner and s['index']==victim_id:s['value']-=1
  if s['op']=='assert_zone_contains' and s['player']==target_owner and s['zone']==zone and s['value']==victim_id:
   stock=HAND if zone=='hand' else GRAVE
   if victim['name'] not in stock:continue
  out.append(s)
 case['steps']=out
 if invalid:
  checks=[observe('invalid','in_play',1),observe('invalid','in_hand',0),step('assert_zone_contains',player=invalid_owner,zone='battlefield',value=rows[invalid]['csvid'])]
  if 'Creature' in o[invalid]['type_line']:checks += [observe('invalid',k,int(o[invalid][k])) for k in ('power','toughness')]+[observe('invalid','damage',0)]
  case['steps']+=checks
 return case


def _normal_cost_choices(case,p,branch,owner):
 from recipes import step
 if not p['cast_dialogs'] or branch in ('insufficient_mana','wrong_color'):return
 if branch=='payment' and p['alternate_mana']:
  alternate=p['alternate_mana'];pool=p['mana']
  if any(a>b for a,b in zip(alternate[1:],pool[1:])) or sum(pool)-sum(alternate[1:])<alternate[0]:return
 out=[]
 # Countered sources choose their casting mode before the queued response.
 if branch=='countered':out += [step('choose_dialog',player=owner,value=n) for n in p['cast_dialogs']]
 for s in case['steps']:
  if s['op']=='cast' and s.get('ref')=='source' and branch!='countered':out += [step('choose_dialog',player=owner,value=n) for n in p['cast_dialogs']]
  out.append(s)
 case['steps']=out


def _reserve_repeat_generic(case,p,branch,owner):
 from recipes import step
 if branch!='bounce_recast' or 2*p['mana'][0]<=10:return
 reserve=2*p['mana'][0];delta=reserve-10
 case['steps'].insert(0,step('set_mana',player=owner,color=0,value=reserve))
 seen=set()
 for s in case['steps']:
  if id(s) in seen:continue
  seen.add(id(s))
  if s['op']=='assert_mana_total' and s['player']==owner:s['value']+=delta


def _rebuild_zone_receipts(case,rows):
 """Apply the common recipe's reviewed entity outcomes to the adapted board.

 This keeps ownership/identity substitutions independent of stock duplicates
 and checks every zone multiplicity after re-entry and snapshot replay.
 """
 from recipes import step
 full=case['fixture']=='stocked_zones'
 zones=[{z:list(names) if full else [] for z,names in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY))} for _ in (0,1)]
 for z in zones:z['battlefield']=[]
 states={}
 for s in case['steps']:
  if s['op']=='assert_card' and s['property'] in ('in_play','in_hand'):states.setdefault(s['ref'],{})[s['property']]=s['value']
 for c in case['setup']:
  state=states.get(c['ref'])
  if state:
   zone='battlefield' if state.get('in_play') else 'hand' if state.get('in_hand') else 'graveyard'
  else:zone='graveyard' if c['zone']=='hand' else c['zone']
  zones[c['player']][zone].append(c['name'])
 universe=sorted(set(HAND+GRAVE+EXILE+LIBRARY)|{c['name'] for c in case['setup']})
 checks=[]
 for player in (0,1):
  for zone,names in zones[player].items():
   checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
   if zone=='library':checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(names)]
   else:
    counts=Counter(names);checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=rows[n]['csvid'],value=counts[n]) for n in universe]
  counts=Counter(zones[player]['hand']+zones[player]['graveyard'])
  checks += [step('assert_hand_graveyard_total',player=player,index=rows[n]['csvid'],value=counts[n]) for n in universe]
 out=[]
 for s in case['steps']:
  if s['op'] in ('assert_zone','assert_zone_contains','assert_zone_card','assert_hand_graveyard_total'):continue
  if s['op'] in ('record_state','assert_recorded_state'):out += copy.deepcopy(checks)
  out.append(s)
 case['steps']=out if out and out[-1]['op']=='assert_recorded_state' else out+checks


def _replace_positive_witnesses(case,p,branch,rows,o):
 category=branch[7:] if branch.startswith('target_') else p['targets'][0]
 names=p['positive_witnesses'].get(category)
 changed={}
 for c in case['setup']:
  if names and c['ref'] in ('recipient','unselected','second_target'):
   c['name']=names[0 if c['ref']=='recipient' else 1];changed[c['ref']]=c['name']
  if branch=='wrong_type' and c['ref']=='invalid':
   c['name']=next(n for pair in ordinary.TARGETS.values() for n in pair if not any(t.title() in o[n]['type_line'] for t in p['targets']))
   changed[c['ref']]=c['name']
 out=[]
 for s in case['steps']:
  name=changed.get(s.get('ref'))
  if name:
   if s['op']=='bind_hand':s['value']=rows[name]['csvid']
   if s['op']=='assert_card' and s['property'] in ('power','toughness','damage','plus1_counters','minus1_counters'):
    if 'Creature' not in o[name]['type_line']:continue
    if s['property'] in ('power','toughness'):s['value']=int(o[name][s['property']])
  out.append(s)
 case['steps']=out


def expand(name,csvid,d,p,selected,registry,oracle_context):
 if (not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected)
     or len(set(selected))!=len(selected) or any(v not in variants(p) for v in selected)):
  raise ValueError('entry removal extension expansion requires unique reviewed selected variants')
 result=[]
 for requested in selected:
  branch,owner=requested.rsplit('_',1);owner=int(owner)
  deny=(branch.startswith('invalid_') or branch=='own_only' or (branch in ('no_other_targets','wrong_type') and (p['predicate'] or p['opponent_only'])))
  mapped='entry' if deny else branch
  q={k:copy.deepcopy(p[k]) for k in ('effect','targets','optional','keywords','power','toughness','mana')};q.update(family='etb_removal_creature',remaining_abilities=p['remaining_abilities'])
  declaration=dict(d,trigger_priority=p['trigger_priority'],resolve_dialogs=p['resolve_dialogs'])
  case=ordinary.expand(name,csvid,declaration,q,[mapped+'_'+str(owner)],registry,oracle_context)[0]
  _replace_positive_witnesses(case,p,mapped,registry,oracle_context)
  if deny:case=_no_target_case(case,p,branch,owner,registry,oracle_context)
  if p['opponent_only']:
   for c in case['setup']:
    if c['ref']=='second_target':c['player']=1-owner
   for s in case['steps']:
    if s['op']=='bind_hand' and s.get('ref')=='second_target':s['player']=1-owner
  _normal_cost_choices(case,p,branch,owner)
  _reserve_repeat_generic(case,p,branch,owner)
  _rebuild_zone_receipts(case,registry)
  case['name']=f'EtbRemovalExtensionRecipe_{csvid}_{requested}'
  case['_recipe']=dict(family='etb_removal_extension',version=2,variant=requested,params=copy.deepcopy(d['params']),oracle_text=d['oracle_text'],untested_components=list(p['inactive_components'])+['Pending-target departure, regeneration/replacement, protection, trigger ordering, control changes and multiplayer.'])
  result.append(case)
 return result
