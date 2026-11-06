"""Whole-Oracle life-spell components adapting reviewed value-flow receipts."""
import copy,json
from pathlib import Path
from hand_exchange_recipes import expand as pipeline_expand,validate_witnesses as pipeline_witnesses
from grant_recipes import HAND,GRAVE

PINS=json.loads((Path(__file__).parent/'data/life_spell_extension_witnesses.json').read_text(encoding='utf8'))
FAMILY='life_spell_extension'

def spec(d,o):
 from recipes import cost_from_oracle
 review=PINS['cards'].get(o.get('name')) if o else None
 if not review or o.get('layout')!='normal' or o.get('selected_face') or o.get('card_faces') or o.get('type_line','').partition(' — ')[0] not in ('Instant','Sorcery'):
  raise ValueError('life spell component requires a reviewed normal instant/sorcery')
 keys=('name','oracle_id','oracle_text','mana_cost','type_line','layout','keywords','colors')
 if any(o.get(k)!=review['oracle'].get(k) for k in keys) or d.get('oracle_text')!=o['oracle_text']:
  raise ValueError('life spell complete Oracle pin changed')
 if type(d.get('version')) is not int or d['version']!=2 or 'variants' not in d or json.dumps(d.get('params'),sort_keys=True)!=json.dumps(review['params'],sort_keys=True):
  raise ValueError('life spell requires explicit reviewed version2 parameters and variants')
 return dict(copy.deepcopy(review['params']),family=FAMILY,mana=cost_from_oracle(o),instant=o['type_line'].startswith('Instant'))

def validate_witnesses(o,rows):
 pipeline_witnesses(o,rows)
 for name,pin in PINS['witnesses'].items():
  if name not in rows or any(o.get(name,{}).get(k)!=v for k,v in pin.items()):raise ValueError('life spell witness changed: '+name)

def variants(p):
 modes=['effect','payment','insufficient_mana','wrong_color','bare_zones','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot','countered_cast']
 if p['instant']:modes+=['opposing_turn','combat']
 if p['recipient']=='any':modes+=['own_target']
 if p['recipient'] in ('any','opponent'):modes+=['target_shrouded_pending']
 if 'storm copies' in p['gaps']:modes.remove('target_shrouded_pending')
 quantities={op['quantity'] for op in p['operations']}
 if quantities&{'own_creatures','all_creatures','own_lands','own_plains'}:modes+=['quantity_zero','quantity_high']
 if quantities&{'grave_cards','grave_creatures'}:modes+=['grave_extra']
 if 'hand_cards' in quantities:modes+=['one_card','large_hand','own_empty','opponent_empty']
 if any(op['kind']=='draw' for op in p['operations']):modes+=['exact_library']
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]

def _quantity(kind,v,p,o):
 if kind=='fixed':return 1
 if kind=='grave_cards':return 0 if v=='bare_zones' else len(GRAVE)+(3 if v=='grave_extra' else 0)
 if kind=='grave_creatures':return 0 if v=='bare_zones' else sum('Creature' in o[n]['type_line'] for n in GRAVE)+(3 if v=='grave_extra' else 0)
 if kind=='hand_cards':return 0 if v in ('bare_zones','own_empty') else 1 if v=='one_card' else len(HAND)*3 if v=='large_hand' else len(HAND)
 if kind=='own_creatures':return 0 if v=='quantity_zero' else 3 if v=='quantity_high' else 1
 if kind=='all_creatures':return 0 if v=='quantity_zero' else 4 if v=='quantity_high' else 2
 if kind=='opponent_creatures':return 1
 if kind=='own_lands':return 0 if v=='quantity_zero' else len(p['lands'])+(2 if v=='quantity_high' else 0)
 if kind=='own_plains':return 0 if v=='quantity_zero' else sum('Plains' in o[n]['type_line'].partition(' — ')[2].split() for n in p['lands'])+(2 if v=='quantity_high' else 0)
 raise ValueError('unreviewed life quantity')

def expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe
 validate_witnesses(o,rows)
 allowed=variants(p)
 if not selected or len(selected)!=len(set(selected)) or any(v not in allowed for v in selected):raise ValueError('explicit distinct reviewed life variants required')
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner)
  basev='effect' if v in ('quantity_zero','quantity_high','grave_extra') else v
  quantities={op['quantity'] for op in p['operations']}
  internal=dict(family='value_flow',mode='spell',recipient=p['recipient'],operations=[dict(kind=op['kind'],scope=op['scope'],amount=op['amount']*_quantity(op['quantity'],v,p,o),random=False) for op in p['operations']],
   cost=dict(mana='',tap=False,sacrifice=False,pay_life=0,discard=0),keywords=[],mana_ability=False,activation_mana=[0]*6,mana=p['mana'],creature=False,land=False,legendary=False,power=0,toughness=0,untested_rules=p['gaps'],instant=p['instant'])
  c=pipeline_expand(name,csvid,dict(d,target_choices=p.get('target_choices',1)),internal,[basev+'_'+str(owner)],rows,o)[0]
  added=[];removed=set();land_refs=[]
  if v=='quantity_zero' and quantities&{'own_creatures','all_creatures'}:
   removed={'friendly','enemy'} if any(op['quantity']=='all_creatures' for op in p['operations']) else {'friendly'}
   c['setup']=[x for x in c['setup'] if x['ref'] not in removed]
  if v=='quantity_high' and quantities&{'own_creatures','all_creatures'}:
   for i in range(2):
    ref='quantity_'+str(i);c['setup'].append(dict(card('Grizzly Bears',ref,owner,'battlefield'),state=['untapped']));added.append(ref)
  if v=='grave_extra':
   for i in range(3):c['setup'].append(card('Grizzly Bears','grave_extra_'+str(i),owner,'graveyard'))
  lands=list(p.get('lands',[]))
  if v=='quantity_zero':lands=[] if 'own_lands' in quantities else [n for n in lands if 'Plains' not in o[n]['type_line']] if 'own_plains' in quantities else lands
  if v=='quantity_high' and quantities&{'own_lands','own_plains'}:lands+=['Plains' if 'own_plains' in quantities else 'Forest']*2
  for i,nm in enumerate(lands):
   ref='quantity_land_'+str(i);c['setup'].append(dict(card(nm,ref,owner,'battlefield'),state=['tapped']));land_refs.append(ref)
  if quantities&{'own_creatures','all_creatures','opponent_creatures'}:
   # A persistent noncreature distinguishes creature quantities from counting
   # every permanent, including when no qualifying creature remains.
   ref='quantity_type_control';c['setup'].append(dict(card('Darksteel Relic',ref,owner,'battlefield'),state=['tapped']));land_refs.append(ref)
  steps=[];raw=copy.deepcopy(c['steps'])
  # Queue source menus before source targets, never before response decisions.
  inserts={}
  for i,original in enumerate(raw):
   s=copy.deepcopy(original)
   if s['op']=='cast' and s.get('ref')=='source' and s.get('value')==1 or s['op']=='cast' and s.get('ref')=='source' and v=='countered_cast':
    j=i-1
    if j>=0 and raw[j]['op']=='require_can_cast':j-=1
    while j>=0 and raw[j]['op'].startswith('choose_'):j-=1
    inserts.setdefault(j+1,[]).extend(step('choose_dialog',player=owner,value=n) for n in p['cast_before'])
    if p['cast_after'] and v!='payment':
     after=next((k for k in range(j+1,i) if raw[k]['op'] in ('choose_priority','require_can_cast')),i)
     inserts.setdefault(after,[]).extend(step('choose_dialog',player=owner,value=n) for n in p['cast_after'])
  shield_pending=False
  for i,original in enumerate(raw):
   s=copy.deepcopy(original)
   steps+=inserts.get(i,[])
   if s.get('ref') in removed:continue
   # The AI's alternate-payment helper always spends white if available.
   # This reviewed component pays4GG, so white is unavailable on both sides.
   if 'no white mana spent' in p['inactive'] and s['op'] in ('set_mana','assert_mana') and s.get('player')==owner and s.get('color')==5:s['value']=0
   if s['op']=='assert_zone' and s.get('zone')=='battlefield':
    s['value']+=(len(added)+len(land_refs))*int(s['player']==owner)-sum(1 for ref in removed if (ref=='friendly')==(s['player']==owner))
   if v=='grave_extra' and s.get('player')==owner:
    if s['op']=='assert_zone' and s.get('zone')=='graveyard':s['value']+=3
    if s['op']=='assert_hand_graveyard_total' and s.get('index')==rows['Grizzly Bears']['csvid']:s['value']+=3
   if s['op']=='cast' and s.get('ref')=='shield':
    steps.append(dict(s,op='cast_response'));shield_pending=True
    continue
   if s['op']=='resolve' and shield_pending:
    steps += [step('resolve_response'),step('assert_stack',value=0)];shield_pending=False
    continue
   steps.append(s)
  # Added quantity controls have fixed Oracle stats and never leave the board.
  for ref in added:steps += [observe(ref,'in_play',1),observe(ref,'tapped',0),observe(ref,'power',2),observe(ref,'toughness',2),observe(ref,'damage',0)]
  for ref in land_refs:steps += [observe(ref,'in_play',1),observe(ref,'tapped',1)]
  active=v not in ('insufficient_mana','wrong_color','countered_cast','target_shrouded_pending')
  target=owner if v=='own_target' or p['recipient']=='self' else 1-owner
  for pl in (0,1):
   steps += [step('assert_energy',player=pl,value=p.get('energy',0) if active and pl==owner else 0),step('assert_poison',player=pl,value=p.get('poison',0) if active and pl==target else 0)]
  c['steps']=steps;c['name']=f'LifeSpellExtensionRecipe_{csvid}_{variant}'
  c['_recipe']=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps']+['life and draw replacement effects','multiplayer','combat damage followups'],inactive_conditions=p['inactive'])
  result.append(c)
 return result
