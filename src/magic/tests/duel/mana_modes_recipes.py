"""Multiple printed tap-for-mana modes, including hybrid input payments."""
import copy,re
from collections import Counter
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from mana_creature_recipes import ANY_COLOR,WITNESSES,HULK
from permanent_mana_recipes import validate_witnesses

SYMBOLS='CBUGRW'

def pool(text):
 if not isinstance(text,str) or not re.fullmatch(r'(?:\{[CBUGRW]\}){1,5}',text):raise ValueError('one to five explicit ordinary output symbols required')
 return [text.count('{'+c+'}') for c in SYMBOLS]

def inputs(cost):
 if cost=='{T}':return [[0]*6]
 if not isinstance(cost,str):raise ValueError('structured tap input cost required')
 m=re.fullmatch(r'\{([1-5]|[BUGRW]/[BUGRW])\}, \{T\}',cost)
 if not m:raise ValueError('reviewed generic or two-color hybrid input and tap required')
 if m[1].isdigit():return [[int(m[1]),0,0,0,0,0]]
 colors=m[1].split('/')
 if colors[0]==colors[1]:raise ValueError('distinct ordinary hybrid colors required')
 return [[int(c==color) for c in SYMBOLS] for color in colors]

def spec(d,r):
 from recipes import cost_from_oracle
 if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r.get('type_line') not in ('Land','Artifact') or
     r.get('keywords') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):raise ValueError('whole normal noncreature mana permanent Oracle pin required')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'modes'} or not isinstance(p['modes'],list) or not 2<=len(p['modes'])<=3:raise ValueError('two or three explicit printed mana modes required')
 modes=[];lines=[]
 for mode in p['modes']:
  if not isinstance(mode,dict) or set(mode)!={'cost','production','outputs','choices','fallback_choices'}:raise ValueError('each mode needs a cost, production, outputs and decision scripts')
  outputs=mode['outputs'];form=mode['production'];cost=mode['cost'];paid=inputs(cost)
  if not isinstance(outputs,list) or not outputs or any(not isinstance(o,str) for o in outputs) or len(set(outputs))!=len(outputs):raise ValueError('unique explicit outputs required')
  pools=[pool(o) for o in outputs]
  if form=='fixed' and len(outputs)==1:wording=outputs[0]
  elif form=='choice' and 2<=len(outputs)<=3:wording=' or '.join(outputs) if len(outputs)==2 else ', '.join(outputs[:-1])+', or '+outputs[-1]
  elif form=='any_color' and outputs==ANY_COLOR:wording='one mana of any color'
  else:raise ValueError('reviewed fixed, choice or any-color output form required')
  choices=mode['choices']
  if not isinstance(choices,list) or len(choices)!=len(outputs):raise ValueError('one explicit decision script per output required')
  def validate_script(script,index):
   if not isinstance(script,list) or len(script)>3:raise ValueError('bounded mode decision script required')
   for choice in script:
    if (not isinstance(choice,dict) or set(choice)!={'kind','value'} or choice['kind'] not in ('color','dialog') or type(choice['value']) is not int or
        not 0<=choice['value']<=(5 if choice['kind']=='color' else 16)):raise ValueError('reviewed color or dialog choice required')
    if choice['kind']=='color' and not pools[index][choice['value']]:raise ValueError('selected color must be in the declared output')
  for index,players in enumerate(choices):
   if not isinstance(players,list) or len(players)!=2:raise ValueError('both controllers need explicit scripts')
   for payments in players:
    if not isinstance(payments,list) or len(payments)!=len(paid):raise ValueError('every legal input payment needs a script')
    for script in payments:validate_script(script,index)
  fallback=mode['fallback_choices']
  if not isinstance(fallback,list) or len(fallback)!=2:raise ValueError('both controllers need a fallback script')
  for script in fallback:validate_script(script,0)
  lines.append(cost+': Add '+wording+'.');modes.append(dict(mode,pools=pools,inputs=paid))
 if r['oracle_text'].splitlines()!=lines or len(set(lines))!=len(lines):raise ValueError('all printed modes must exactly match whole Oracle text')
 if modes[0]['cost']!='{T}' or modes[0]['outputs']!=['{C}']:raise ValueError('first reviewed mode must be the free colorless fallback')
 return dict(p,family='mana_modes',modes=modes,land=r['type_line']=='Land',mana=None if r['type_line']=='Land' else cost_from_oracle(r))

def variants(p):
 result=[]
 for owner in (0,1):
  result += [v+'_'+str(owner) for v in ('entry','tapped','sick','opposing_turn','combat','bare_zones','zero_pool_fallback','source_bounced','source_destroyed','incidental_tap','resolved_snapshot')]
  if not p['land']:result += [v+'_'+str(owner) for v in ('cast','cast_payment','insufficient_cast_mana','fresh_activation')]
  for m,mode in enumerate(p['modes']):
   for i in range(len(mode['outputs'])):
    for payment in range(len(mode['inputs'])):
     vs=['production','payment','repeat_denied','untap_repeat','tax','ready_snapshot','mana_snapshot','spend_generic']
     if any(mode['pools'][i][1:]):vs+=['spend_colored']
     result += [f'{v}_{m}_{i}_{payment}_{owner}' for v in vs]
 return result

def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  parts=variant.rsplit('_',4);indexed=len(parts)==5 and all(x.isdigit() for x in parts[1:])
  if indexed:v,m,i,payment,owner=parts; m,i,payment,owner=map(int,(m,i,payment,owner))
  else:v,owner=variant.rsplit('_',1);owner=int(owner);m=i=payment=0
  mode=p['modes'][m];output=mode['pools'][i];paid=mode['inputs'][payment];bare=v=='bare_zones';casting=v in ('cast','cast_payment','insufficient_cast_mana','fresh_activation')
  zones={z:[[] if bare else list(ns),[] if bare else list(ns)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY))}
  mana=[[20]*6,[20]*6];life=[20,20];setup=[];entities={};pre=[step('turn',player=owner,value=0)];actions=[]
  def add(nm,ref,pl,zone='battlefield',state=None):
   item=card(nm,ref,pl,zone)
   if state:item['state']=state
   setup.append(item);entities[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',tapped=bool(state and 'tapped' in state))
   if zone!='battlefield':zones[zone][pl].append(nm)
  def set_pool(value):
   mana[owner]=list(value);return [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(value)]+[step('refresh')]
  def inspect(ref):
   e=entities[ref];s=[observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    s += [observe(ref,'tapped',int(e['tapped']))]
    r=oracle[e['name']]
    if 'Creature' in r.get('type_line',''):s += [observe(ref,'power',int(r['power'])),observe(ref,'toughness',int(r['toughness'])),observe(ref,'damage',0)]
   return s
  def cast(ref,target=None,dialog=None):
   e=entities[ref];s=([step('choose_target',player=e['player'],ref=target)] if target else [])+[step('cast',ref=ref,value=1)]
   if dialog is not None:s.append(step('choose_dialog',player=e['player'],value=dialog))
   s += [step('resolve'),step('assert_choices_consumed')];mana[e['player']]=[a-b for a,b in zip(mana[e['player']],cost_from_oracle(oracle[e['name']]))];zones['hand'][e['player']].remove(e['name'])
   if 'Instant' in oracle[e['name']]['type_line']:zones['graveyard'][e['player']].append(e['name'])
   else:e['alive']=True
   return s
  def spell(nm,ref,target=None,dialog=None):add(nm,ref,owner,'hand');return cast(ref,target,dialog)
  add(name,'source',owner,'hand' if casting else 'battlefield',['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
  pre += [step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)]
  if casting:
   if v in ('cast_payment','insufficient_cast_mana'):pre += set_pool(p['mana'] if v=='cast_payment' else [0]*6)
   if v=='insufficient_cast_mana':pre += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
   else:pre+=cast('source')+[step('refresh')]
  if v=='opposing_turn':pre.append(step('turn',player=1-owner,value=0))
  if v=='combat':
   add('Savannah Lions','attacker',1-owner);entities['attacker']['tapped']=True
   pre += [step('turn',player=1-owner,value=0),step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat')]
  if v=='tax':pre += spell('Suppression Field','taxer')
  reserve=list(paid)
  if v=='production':reserve=[x+2 for x in reserve] if sum(paid)==0 else [x*3 for x in reserve]
  if v=='spend_generic':reserve[0]+=6-sum(output);add(HULK[0],'spender',owner,'hand')
  if v=='spend_colored':
   for c,n in enumerate(output):
    for j in range(n):add(WITNESSES['{'+SYMBOLS[c]+'}'][0],f'spender_{c}_{j}',owner,'hand')
  bind_ai_free=m==0 and owner==1 and len(p['modes'][1]['inputs'])==2 and v not in ('entry','cast','cast_payment','insufficient_cast_mana','tapped','incidental_tap')
  if bind_ai_free:
   # This handler's AI path chooses filtering whenever colored funding exists.
   # Bind the requested free mode by supplying only colorless funding; C cannot
   # pay its hybrid input. Human cases still choose C with both modes available.
   reserve=[reserve[0],0,0,0,0,0] if indexed else [20,0,0,0,0,0]
  if indexed or v=='zero_pool_fallback' or bind_ai_free:pre += set_pool(reserve)
  if v=='zero_pool_fallback':pre += set_pool([0]*6)
  def activate():
   s=[step('require_can_activate',ref='source',value=1)]
   funded=any(sum(mana[owner])>=sum(cost) and all(mana[owner][c]>=cost[c] for c in range(1,6)) for cost in p['modes'][1]['inputs'])
   script=mode['fallback_choices'][owner] if m==0 and not funded else mode['choices'][i][owner][payment]
   s += [step('choose_'+choice['kind'],player=owner,value=choice['value']) for choice in script]
   s += [step('activate',ref='source',value=1),step('assert_choices_consumed'),step('assert_stack',value=0)]
   entities['source']['tapped']=True;mana[owner]=[a-b+c for a,b,c in zip(mana[owner],paid,output)]
   return s+inspect('source')+[step('assert_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  ready=after=0
  if v in ('entry','cast','cast_payment','insufficient_cast_mana'):actions+=inspect('source')
  elif v=='tapped':actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_choices_consumed')]
  elif v=='incidental_tap':actions += spell('Twiddle','tap','source',0);entities['source']['tapped']=True;actions += [step('require_can_activate',ref='source',value=0)]
  else:
   ready=len(actions);actions+=activate();after=len(actions)
   if v in ('repeat_denied','zero_pool_fallback'):actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
   if v=='untap_repeat':
    actions+=set_pool([0,0,1,0,0,0])+spell('Twiddle','untap','source',1);entities['source']['tapped']=False
    actions += [observe('source','tapped',0)]+set_pool(paid)+activate()
   if v in ('spend_generic','spend_colored'):
    if v=='spend_generic' and owner==0:actions += [step('choose_mana',player=owner,color=c) for c in range(1,6) for _ in range(output[c])]
    for ref in [ref for ref in entities if ref.startswith('spender')]:actions += cast(ref)
    if v=='spend_generic':mana[owner]=[0]*6
   if v=='source_bounced':
    if mana[owner][2]<2:actions+=set_pool([20]*6)
    actions += spell('Boomerang','bounce','source');entities['source']['alive']=False;zones['hand'][owner].append(name)
   if v=='source_destroyed':actions += [step('destroy',ref='source')];entities['source']['alive']=False;zones['graveyard'][owner].append(name)
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref in entities:checks+=inspect(ref)
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
  for pl in (0,1):
   checks += [step('assert_life',player=pl,value=life[pl])]+[step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   board=[e['name'] for e in entities.values() if e['alive'] and e['player']==pl]
   for zone,names in [('battlefield',board)]+[(z,players[pl]) for z,players in zones.items()]:
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(names)))
    if zone=='library':checks += [step('assert_zone_card',player=pl,zone=zone,index=j,value=registry[nm]['csvid']) for j,nm in enumerate(names)]
    else:
     multiplicity=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=registry[nm]['csvid'],value=multiplicity[nm]) for nm in sorted(universe)]
  steps=pre+actions+checks
  if v in ('ready_snapshot','mana_snapshot'):
   cut=ready if v=='ready_snapshot' else after;tail=actions[cut:]+checks
   steps=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'ManaModesRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='mana_modes',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['land play permissions','mode-specific eligibility query','input supplied by automatic source selection','AI selection of free C with colored funding available','hybrid payment with both input colors simultaneously available','multiple-source chaining','multiplayer','additional clauses or costs'])))
 return result


# Wave-six opt-in branches keep every old declaration on its original path.
_wave6_original_spec = spec
_wave6_original_variants = variants
_wave6_original_expand = expand
_wave6_original_validate = validate_witnesses

def spec(d, o):
    from mana_spell_extension_recipes import applies, spec as extension_spec
    return extension_spec(d, o) if applies(d) else _wave6_original_spec(d, o)

def variants(p):
    if p.get('wave') == 6:
        from mana_spell_extension_recipes import variants as extension_variants
        return extension_variants(p)
    return _wave6_original_variants(p)

def validate_witnesses(o, registry):
    _wave6_original_validate(o, registry)

def expand(name, csvid, d, p, selected, registry, o):
    if p.get('wave') == 6:
        from mana_spell_extension_recipes import expand as extension_expand
        return extension_expand(name, csvid, d, p, selected, registry, o)
    return _wave6_original_expand(name, csvid, d, p, selected, registry, o)
