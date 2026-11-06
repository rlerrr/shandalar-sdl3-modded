"""Reviewed Aura components layered over the established combat grant witnesses."""
import copy
import json
from pathlib import Path

from grant_recipes import spec as grant_spec, variants as grant_variants
from grant_recipes import expand as grant_expand, validate_witnesses as grant_witnesses

PIN_PATH=Path(__file__).parent/'data/aura_keyword_extension_witnesses.json'


def pins():
 return json.loads(PIN_PATH.read_text(encoding='utf8'))


def spec(d,r):
 from recipes import cost_from_oracle
 if (not r or d.get('version')!=2 or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
     or r.get('type_line') not in ('Enchantment — Aura','Legendary Enchantment — Aura') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('whole pinned normal Aura and explicit version2 variants required')
 profile=pins()['sources'].get(r['name'])
 if not profile or any(r.get(k)!=v for k,v in profile['oracle'].items()):raise ValueError('unreviewed or changed whole Aura record')
 if d.get('params')!=profile['params']:raise ValueError('Aura component and inactive clauses must match the reviewed profile')
 p=copy.deepcopy(d['params'])
 if (any(type(p[k]) is not int for k in ('power','toughness','draw')) or type(p['return_aura']) is not bool
     or not isinstance(p['keywords'],list) or not isinstance(p['gaps'],list)):
  raise ValueError('typed explicit Aura component parameters required')
 inner=dict(power=p['power'],toughness=p['toughness'],keywords=p['keywords'],controller=p['controller'])
 for k in ('draw','return_aura'):
  if p[k]:inner[k]=p[k]
 inner=grant_spec(inner,cost_from_oracle(r),'aura_creature')
 return dict(family='aura_keyword_extension',component=inner,gaps=p['gaps'],excluded=profile.get('excluded',[]))


def variants(p):
 return [v+'_'+str(owner) for owner in (0,1) for v in grant_variants(p['component']) if v not in p['excluded']]


def validate_witnesses(o,rows):
 grant_witnesses(o,rows)
 for name,pin in pins()['witnesses'].items():
  if name not in rows or o.get(name,{}).get('card_faces') or o.get(name,{}).get('selected_face') or any(o.get(name,{}).get(k)!=v for k,v in pin.items()):raise ValueError('whole Aura combat witness changed: '+name)


def expand(name,csvid,d,p,selected,rows,o):
 allowed=variants(p)
 if not selected or len(set(selected))!=len(selected) or any(v not in allowed for v in selected):raise ValueError('nonempty unique reviewed Aura variants required')
 validate_witnesses(o,rows)
 result=[]
 for variant in selected:
  base,owner=variant.rsplit('_',1);owner=int(owner)
  inner_d=dict(d,params={k:v for k,v in p['component'].items() if k not in ('family','mana')})
  c=grant_expand(name,csvid,inner_d,p['component'],[base],rows,o)[0]
  if name=='Indrik Umbra' and base=='opponent_unblocked':
   # The Aura's must-block component is outside these witnesses. With an
   # opposing host, the caster's independent control is now the defender;
   # make it unavailable as a blocker, just like the other tapped control.
   next(x for x in c['setup'] if x['ref']=='unselected')['state']=['tapped']
   for item in c['steps']:
    if item.get('op')=='assert_card' and item.get('ref')=='unselected' and item.get('property')=='tapped':item['value']=1
  if owner:
   seen=set()
   for item in c['setup']+c['steps']:
    if id(item) in seen:continue
    seen.add(id(item))
    if item.get('player') in (0,1):item['player']=1-item['player']
    if item.get('op')=='choose_player':item['value']=1-item['value']
  if base=='lost_target':
   # The legacy expander's nested response assumes the human caster. Use the
   # existing explicit response context for either actual controller instead.
   steps=[x for x in c['steps'] if x['op']!='choose_priority' and not (x['op']=='choose_target' and x.get('player')==-1)]
   pending=steps.index(dict(op='assert_stack',value=1))+1
   steps[pending:pending]=[dict(op='choose_target',player=owner,ref='recipient'),
    dict(op='cast_response',ref='response',value=1),dict(op='assert_stack',value=2)]
   resolution=next(i for i,x in enumerate(steps) if x['op']=='resolve')
   steps[resolution:resolution+1]=[dict(op='resolve_response'),dict(op='assert_stack',value=0)]
   c['steps']=steps
  c['steps'].insert(0,dict(op='turn',player=owner,value=0))
  c['name']=f'AuraKeywordExtensionRecipe_{csvid}_{variant}'
  c['_recipe']=dict(family='aura_keyword_extension',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],
    untested_components=p['gaps']+['multiple blockers','control changes','layer interactions','copy effects'])
  result.append(c)
 return result
