"""Explicit two-color hybrid payments layered onto reviewed creature recipes."""
import copy
import itertools
from defensive_creature_recipes import reviewed_payment, mirror_cast_cases

FAMILIES = {'creature_keywords','self_pump_creature','evasion_creature','mana_creature',
            'regenerate_creature','etb_value_creature','blocking_creature','tap_damage_creature',
            'combat_keyword_creature','lord_creature','entry_counter_creature','counter_damage_creature'}
EXTRA = ('hybrid_payment_other','hybrid_payment_mixed')
MIRRORS = ('cast','payment','insufficient_mana','snapshot','fresh_attack')

def applies(d):
 return isinstance(d,dict) and isinstance(d.get('family'),str) and d['family'] in FAMILIES and isinstance(d.get('params'),dict) and 'payment_colors' in d['params']

def normalize(d,r,colors=None):
 if d.get('version')!=2 or not isinstance(r,dict):raise ValueError('hybrid payments require a reviewed version-two Oracle record')
 params=dict(d['params'])
 if colors is not None:params['payment_colors']=colors
 mana,pairs=reviewed_payment(r,params)
 if not pairs:raise ValueError('explicit hybrid symbols required')
 clean=dict(d,params={k:v for k,v in params.items() if k!='payment_colors'})
 symbols=('B','U','G','R','W')
 cost=(f'{{{mana[0]}}}' if mana[0] else '')+''.join('{'+c+'}' for c,n in zip(symbols,mana[1:]) for _ in range(n))
 return clean,dict(r,mana_cost=cost),mana,pairs

def specification(d,r):
 from recipes import explicit_spec
 clean,record,mana,pairs=normalize(d,r)
 result=explicit_spec(clean,record)
 result['_hybrid']=dict(mana=mana,pairs=pairs,colors=d['params']['payment_colors'])
 return result

def available(pools,vector):
 return all(pools[c]>=vector[c] for c in range(1,6)) and sum(pools)>=sum(vector)

def script_payments(case,d,r):
 from recipes import step
 _,_,mana,pairs=normalize(d,r)
 source=next(c for c in case['setup'] if c['ref']=='source')
 if source['player']!=0:return
 subjects={c['ref'] for c in case['setup'] if c['name']==source['name'] and c['player']==0}
 vectors=set()
 for choices in itertools.product(*pairs):
  _,_,vector,_=normalize(d,r,list(choices));vectors.add(tuple(vector))
 pools=[[10]*6,[10]*6];saved=None;out=[]
 for action in case['steps']:
  op=action['op']
  if op=='set_mana':pools[action.get('player',0)][action['color']]=action['value']
  elif op=='capture':saved=copy.deepcopy(pools)
  elif op=='restore' and saved is not None:pools=copy.deepcopy(saved)
  # A countered creature still pays its casting cost despite cast returning 0.
  # Repeated-copy scenarios can cast the same card through another setup ref.
  if op=='cast' and action.get('ref') in subjects and available(pools[0],mana):
   if sum(available(pools[0],v) for v in vectors)>1:
    choice=step('choose_payment',player=0,value=sum(n<<(4*c) for c,n in enumerate(mana)))
    # ETB target decisions may have been queued before casting. Payment happens
    # before resolution; spell cast-target-first scripts keep their own order.
    cut=len(out)
    if not d.get('cast_target_first'):
     while cut and out[cut-1]['op'].startswith('choose_'):cut-=1
    out.insert(cut,choice)
   pools[0]=[a-b for a,b in zip(pools[0],mana)]
  out.append(dict(action))
 case['steps']=out

def invalid_payment(case,d,r):
 """A negative payment must exclude both alternatives, not the chosen color."""
 if not case['_recipe']['variant'].startswith('wrong_color'):return
 from recipes import COLORS
 _,_,mana,pairs=normalize(d,r)
 excluded={c for pair in pairs for c in pair}
 excluded.update(c for c,n in zip(('B','U','G','R','W'),mana[1:]) if n)
 unused=next((c for c in ('B','U','G','R','W') if c not in excluded),None)
 if unused is None:raise ValueError('hybrid wrong-color payment requires an unused color witness')
 pools=[mana[0],0,0,0,0,0];pools[COLORS[unused]]=sum(mana[1:])
 source=next(c for c in case['setup'] if c['ref']=='source')
 case['steps']=[dict(s) for s in case['steps']]
 for s in case['steps']:
  if s['op'] in ('set_mana','assert_mana') and s.get('player',0)==source['player']:s['value']=pools[s['color']]

def expand(name,csvid,d,r,registry,oracle_context):
 from recipes import _expand_plain
 clean,record,_,pairs=normalize(d,r)
 selected=d.get('variants')
 if not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or len(set(selected))!=len(selected):
  raise ValueError('explicit unique hybrid recipe variants required')
 mirrors={v+'_1':v for v in MIRRORS} if d['family']=='creature_keywords' else {}
 base=[v for v in selected if v not in EXTRA and v not in mirrors]
 for variant in selected:
  if variant in mirrors and mirrors[variant] not in base:base.append(mirrors[variant])
 cases=_expand_plain(name,csvid,dict(clean,variants=base),record,registry,oracle_context) if base else []
 result=[c for c in cases if c['_recipe']['variant'] in selected]
 if mirrors:result+=mirror_cast_cases(cases,selected)
 for variant in EXTRA:
  if variant not in selected:continue
  original=d['params']['payment_colors']
  colors=[(pair[1] if current==pair[0] else pair[0]) if variant==EXTRA[0] or i%2 else current for i,(pair,current) in enumerate(zip(pairs,original))]
  alt,alt_record,_,_=normalize(d,r,colors)
  generated=_expand_plain(name,csvid,dict(alt,variants=['payment']),alt_record,registry,oracle_context)
  for case in generated:
   old=case['_recipe']['variant'];case['name']=case['name'].removesuffix('_'+old)+'_'+variant
   case['_recipe'].update(variant=variant,payment_colors=colors)
  result+=generated
 for case in result:
  case['_recipe']['params']=d['params'];case['_recipe']['oracle_text']=d['oracle_text']
  if case['_recipe']['variant'] not in EXTRA:
   invalid_payment(case,d,r)
   script_payments(case,d,r)
  if d['family']=='creature_keywords' and case['fixture']=='empty_main':
   from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
   from recipes import step
   # Pure creatures cannot alter these zones. Retain useful alternatives on
   # the shared board while preserving the case's explicit extra cards.
   zones={'hand':HAND,'graveyard':GRAVE,'exile':EXILE,'library':LIBRARY}
   case['fixture']='stocked_zones';case['steps']=[dict(s) for s in case['steps']]
   for action in case['steps']:
    if action['op']=='assert_zone' and action['zone'] in zones:action['value']+=len(zones[action['zone']])
   for player in (0,1):
    for zone,names in zones.items():
     case['steps'] += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(names if zone=='library' else dict.fromkeys(names))]
 return result
