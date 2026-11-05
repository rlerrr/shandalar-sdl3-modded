"""Whole-Oracle variable draw/discard/mill/life spells using normal X payment."""
import re
from hand_exchange_recipes import expand as pipeline,validate_witnesses,WORDS

KINDS=('draw','discard','mill','gain_life','lose_life')
RECIPIENTS=('self','any','opponent','both','opponents')
VALUES=(0,1,2,3,5,8)


def scope(op,p):return p['recipient'] if op['scope']=='recipient' else op['scope']


def phrase(op,subject,explicit=True):
 k=op['kind'];n=op['amount'];verb={'draw':'draw','discard':'discard','mill':'mill','gain_life':'gain','lose_life':'lose'}[k]
 if subject!='self':verb={'gain':'gains','lose':'loses'}.get(verb,verb+'s')
 if n=='life_lost':tail='life equal to the life lost this way'
 elif k in ('gain_life','lose_life'):tail={'X':'X','2X':'two times X','X+3':'X plus 3'}.get(n,str(n))+' life'
 else:tail='X cards' if n=='X' else WORDS[n]
 if k=='discard' and op['random']:tail+=' at random'
 if not explicit:return verb+' '+tail
 return {'self':'You ','any':'Target player ','opponent':'Target opponent ','both':'Each player ','opponents':'Each opponent '}[subject]+verb+' '+tail


def effect_texts(p):
 texts={phrase(p['operations'][0],scope(p['operations'][0],p))}
 if scope(p['operations'][0],p)=='self':texts|={t.removeprefix('You ')[0].upper()+t.removeprefix('You ')[1:] for t in tuple(texts)}
 for before,op in zip(p['operations'],p['operations'][1:]):
  sub=scope(op,p);same=scope(before,p)==sub;full=phrase(op,sub)
  sentences=set() if same and sub in ('any','opponent') else {full}
  if sub=='self':sentences.add(full.removeprefix('You ')[0].upper()+full.removeprefix('You ')[1:])
  if same and sub in ('any','opponent'):sentences.add('That player '+phrase(op,sub,False))
  short=phrase(op,sub,False) if same else full[0].lower()+full[1:]
  texts={t+suffix for t in texts for suffix in ([', then '+short,' and '+short]+['. '+s for s in sentences]+['. Then '+s[0].lower()+s[1:] for s in sentences])}
 return {t+'.' for t in texts}


def spec(d,r):
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('normal whole-Oracle variable spell and explicit variants required')
 if r['type_line'] not in ('Instant','Sorcery') or set(r.get('keywords',[]))-{'Mill'}:raise ValueError('ordinary variable instant/sorcery without extra casting mechanics required')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'recipient','operations'} or p['recipient'] not in RECIPIENTS or not isinstance(p['operations'],list) or not 1<=len(p['operations'])<=4:raise ValueError('explicit recipient and ordered variable operations required')
 variable=False
 for i,op in enumerate(p['operations']):
  if not isinstance(op,dict) or set(op)!={'kind','amount','scope','random'} or op['kind'] not in KINDS or op['scope'] not in ('recipient','self','both') or type(op['random']) is not bool or op['random'] and op['kind']!='discard':raise ValueError('ordinary variable value operation required')
  n=op['amount'];life=op['kind'] in ('gain_life','lose_life')
  if not (type(n) is int and 1<=n<=3 or isinstance(n,str) and (n=='X' or life and n in ('2X','X+3') or op['kind']=='gain_life' and n=='life_lost' and any(s['kind']=='lose_life' for s in p['operations'][:i]))):raise ValueError('reviewed fixed or X-derived amount required')
  variable|=isinstance(n,str) and n!='life_lost'
 if 'Mill' in r.get('keywords',[]) and not any(op['kind']=='mill' for op in p['operations']):raise ValueError('Mill metadata requires a printed mill operation')
 if not variable:raise ValueError('variable value must actually depend on X')
 targeted=p['recipient'] in ('any','opponent')
 if targeted!=bool(re.search(r'\btarget (?:player|opponent)\b',r['oracle_text'],re.I)) or targeted and not any(op['scope']=='recipient' for op in p['operations']):raise ValueError('single shared player target must be used')
 if ' '.join(r['oracle_text'].splitlines()) not in effect_texts(p):raise ValueError('variable parameters must match the complete printed operation pipeline')
 cost=r['mana_cost']
 if cost.count('{X}')!=1 or not re.fullmatch(r'(?:\{(?:X|[1-9][0-9]?|[WUBRGC])\})+',cost):raise ValueError('one X symbol and ordinary fixed base mana required')
 base=cost_from_oracle(dict(mana_cost=cost.replace('{X}','')))
 return dict(p,family='variable_value',base_mana=base,instant=r['type_line']=='Instant')


def variants(p):
 vs=[(v,x) for x in VALUES for v in ('effect','payment')]
 vs += [(v,x) for x in (0,5) for v in ('ready_snapshot','pending_snapshot','resolved_snapshot')]
 extra=['bare_zones','cleanup','countered_cast','insufficient_mana']
 if any(p['base_mana'][1:]):extra+=['wrong_color']
 if p['instant']:extra+=['opposing_turn','combat']
 if p['recipient']=='any':extra+=['own_target']
 if p['recipient'] in ('any','opponent'):extra+=['target_shrouded_pending']
 if any(op['kind'] in ('draw','mill') for op in p['operations']):extra+=['exact_library']
 if any(op['kind']=='mill' for op in p['operations']) and not any(op['kind']=='draw' for op in p['operations']):extra+=['short_library','empty_library']
 if any(op['kind']=='discard' for op in p['operations']):extra+=['last_choice','empty_hands','duplicates','one_card','uneven_hands']
 vs += [(v,3) for v in extra]
 return [f'{v}_x{x}_{owner}' for owner in (0,1) for v,x in vs]


def amounts(p,x):
 out=[];lost=0
 for op in p['operations']:
  n=op['amount'];n={'X':x,'2X':2*x,'X+3':x+3,'life_lost':lost}.get(n,n) if isinstance(n,str) else n
  if op['kind']=='lose_life':lost+=n*(2 if scope(op,p)=='both' else 1)
  out.append(dict(op,amount=n))
 return out


def expand(name,csvid,d,p,selected,rows,o):
 from recipes import step
 result=[]
 for variant in selected:
  v,tail=variant.rsplit('_x',1);x,owner=map(int,tail.split('_'));concrete=amounts(p,x);mana=list(p['base_mana']);mana[0]+=x
  q=dict(mode='spell',recipient=p['recipient'],operations=concrete,cost=dict(mana='',tap=False,sacrifice=False,pay_life=0,discard=0),keywords=[],mana_ability=False,family='value_flow',activation_mana=[0]*6,mana=mana,creature=False,land=False,legendary=False,power=0,toughness=0,untested_rules=[],instant=p['instant'])
  c=pipeline(name,csvid,d,q,[f'{v}_{owner}'],rows,o)[0]
  # The choice belongs to the cast's normal variable-mana boundary. Fixed
  # costs and helper responses must not consume it; replay gets its own copy.
  steps=[]
  for s in c['steps']:
   if s['op']=='cast' and s.get('ref')=='source' and v not in ('insufficient_mana','wrong_color'):
    at=len(steps)
    while at and (steps[at-1]['op'].startswith('choose_') or steps[at-1]['op']=='require_can_cast'):at-=1
    steps.insert(at,step('choose_x',player=owner,value=x))
   steps.append(s)
  c['steps']=steps;c['name']=f'VariableValueRecipe_{csvid}_{variant}'
  c['_recipe']=dict(family='variable_value',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],x=x,untested_components=['alternative costs/abilities','replacement and triggered effects','drawing beyond an empty library','multiplayer'])
  result.append(c)
 return result
