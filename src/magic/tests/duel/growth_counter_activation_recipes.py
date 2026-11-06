"""Reviewed permanent +1/+1 growth abilities, with costs and persistent receipts."""
import json
import re
from collections import Counter
from pathlib import Path
from creature_recipes import KEYWORDS, REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

FAMILY = 'growth_counter_activation'
PINS = json.loads((Path(__file__).parent/'data/growth_counter_activation_witnesses.json').read_text(encoding='utf8'))
WORDS = {1:'a',2:'two',3:'three',4:'four',5:'five',6:'six',10:'ten'}


def spec(d,r):
 if isinstance(d.get('params'),dict) and d['params'].get('profile')=='selection_growth':return _selection_spec(d,r)
 if isinstance(d.get('params'),dict) and d['params'].get('profile')=='counter_draw':return _counter_draw_spec(d,r)
 if isinstance(d.get('params'),dict) and d['params'].get('profile')=='graft_activation':return _graft_spec(d,r)
 if isinstance(d.get('params'),dict) and d['params'].get('profile')=='resource_growth':return _resource_spec(d,r)
 if isinstance(d.get('params'),dict) and d['params'].get('profile')=='counter_transfer':return _transfer_spec(d,r)
 if isinstance(d.get('params'),dict) and 'profile' in d['params']:return _extended_spec(d,r)
 from recipes import cost_from_oracle
 if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
     or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('growth requires whole pinned normal Oracle text and reviewed variants')
 p=d.get('params')
 fields={'mechanic','amount','mana_cost','tap','sacrifice','pay_life','gain_life','scope','keywords','entry','inactive'}
 if not isinstance(p,dict) or set(p)!=fields:raise ValueError('all growth parameters must be explicit')
 if (p['mechanic'] not in ('ordinary','outlast','monstrosity','adapt') or type(p['amount']) is not int or p['amount'] not in WORDS
     or not isinstance(p['mana_cost'],str) or type(p['tap']) is not bool or p['sacrifice'] not in ('none','creature','another creature','artifact or creature','self')
     or type(p['pay_life']) is not int or not 0<=p['pay_life']<=3 or type(p['gain_life']) is not int or not 0<=p['gain_life']<=2
     or p['scope'] not in ('self','target','artifact_team') or type(p['entry']) is not int or not 0<=p['entry']<=6):
  raise ValueError('bounded ordinary counter ability parameters required')
 kinds=r['type_line'].partition(' — ')[0].split();creature='Creature' in kinds
 if not any(k in kinds for k in ('Creature','Artifact','Enchantment')) or any(k in r['type_line'] for k in ('Aura','Equipment','Land')):raise ValueError('standalone nonland growth source required')
 if p['scope']=='self' and not creature:raise ValueError('self growth requires creature')
 if p['scope']!='self' and p['gain_life']:raise ValueError('reviewed growth life gain requires a nontargeted self ability')
 if p['mechanic']!='ordinary' and (p['scope']!='self' or p['sacrifice']!='none' or p['pay_life'] or p['gain_life']):raise ValueError('keyword growth has no extra costs or effects')
 if p['mechanic']=='outlast' and (not p['tap'] or p['amount']!=1):raise ValueError('outlast is a single tapped counter')
 if p['mechanic'] in ('adapt','monstrosity') and p['tap']:raise ValueError('untapped keyword growth required')
 if p['sacrifice']=='self' and p['scope']!='target':raise ValueError('self sacrifice requires a recipient')
 if p['inactive']!=PINS['inactive'].get(r['name'],[]):raise ValueError('inactive ability gaps require exact reviewed clauses')
 lines=r['oracle_text'].splitlines()
 for clause in p['inactive']:
  if clause not in lines:raise ValueError('inactive gap is absent from Oracle')
  lines.remove(clause)
 if p['entry']:
  clauses={ref+' enters with '+WORDS[p['entry']]+' +1/+1 counters on it.' for ref in ('This creature',r['name'],r['name'].split(',')[0])}
  entry=next((s for s in lines if s in clauses),None)
  if entry is None:raise ValueError('explicit fixed entry counters required')
  lines.remove(entry)
 n=p['amount'];counter=WORDS[n]+' +1/+1 counter'+('s' if n!=1 else '')
 cost=([p['mana_cost']] if p['mana_cost'] else [])+(['{T}'] if p['tap'] else [])+([f'Pay {p["pay_life"]} life'] if p['pay_life'] else [])
 if p['sacrifice']!='none':cost+=['Sacrifice '+('this creature' if p['sacrifice']=='self' else 'a creature' if p['sacrifice']=='creature' else 'an artifact or creature' if p['sacrifice']=='artifact or creature' else p['sacrifice'])]
 if not cost:raise ValueError('growth requires nonempty activation cost')
 recipient={'self':'this creature','target':'target creature','artifact_team':'each artifact creature you control'}[p['scope']]
 effect='Put '+counter+' on '+recipient+(' and you gain '+str(p['gain_life'])+' life' if p['gain_life'] else '')+'.'
 expected=', '.join(cost)+': '+effect
 if p['mechanic']=='outlast':expected=f'Outlast {p["mana_cost"]} ({p["mana_cost"]}, {{T}}: Put a +1/+1 counter on this creature. Outlast only as a sorcery.)'
 elif p['mechanic']=='monstrosity':expected=p['mana_cost']+f': Monstrosity {n}. (If this creature isn\'t monstrous, put '+counter+' on it and it becomes monstrous.)'
 elif p['mechanic']=='adapt':expected=p['mana_cost']+f': Adapt {n}. (If this creature has no +1/+1 counters on it, put '+counter+' on it.)'
 # The only reviewed ordinary timing rider is the sorcery restriction.
 sorcery=p['mechanic']=='outlast'
 if expected not in lines and expected+' Activate only as a sorcery.' in lines:expected+=' Activate only as a sorcery.';sorcery=True
 if expected not in lines:raise ValueError('explicit growth parameters do not match printed activation')
 lines.remove(expected);keys=[];canonical={k.lower():k for k in KEYWORDS}
 for line in lines:
  for reminder in re.findall(r'\([^\n]*?\)',line):
   if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed growth reminder')
   line=line.replace(reminder,'')
  for key in line.strip().split(','):
   if key.strip().lower() not in canonical:raise ValueError('additional active source effect requires coverage')
   keys.append(canonical[key.strip().lower()])
 if p['keywords']!=sorted(keys) or len(keys)!=len(set(keys)) or not set(keys)<=set(r.get('keywords',[])):raise ValueError('explicit passive keyword scope disagrees')
 if p['mechanic']!='ordinary' and p['mechanic'].capitalize() not in r.get('keywords',[]):raise ValueError('structured growth keyword required')
 if creature and any(not re.fullmatch(r'\d+',r.get(k,'')) or not 0<=int(r[k])<=20 for k in ('power','toughness')):raise ValueError('numeric growth source required')
 if creature and int(r['toughness'])+p['entry']<=0:raise ValueError('source must survive entry')
 dialogs=d.get('activation_dialogs',{})
 if not isinstance(dialogs,dict) or any(k not in ('0','1') or type(v) is not int or not 0<=v<=8 for k,v in dialogs.items()):raise ValueError('bounded per-controller mode presentation required')
 return dict(p,family=FAMILY,mana=cost_from_oracle(r),activation_mana=cost_from_oracle(dict(mana_cost=p['mana_cost'])) if p['mana_cost'] else [0]*6,
             creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,sorcery=sorcery)


def variants(p):
 if p.get('profile')=='selection_growth':return _selection_variants(p)
 if p.get('profile')=='counter_draw':return _counter_draw_variants(p)
 if p.get('profile')=='graft_activation':return _graft_variants(p)
 if p.get('profile')=='resource_growth':return _resource_variants(p)
 if p.get('profile')=='counter_transfer':return _transfer_variants(p)
 if p.get('profile'):return _extended_variants(p)
 modes=['effect','payment','bare_zones','cleanup','tapped_source','sick_source','opposing_turn','cast_source','cast_payment','insufficient_cast_mana','fresh_activation','ready_snapshot','pending_snapshot','resolved_snapshot']
 if sum(p['activation_mana']):modes+=['insufficient_mana']
 if any(p['activation_mana'][1:]):modes+=['wrong_color']
 if p['pay_life']:modes+=['insufficient_life']
 if p['sacrifice']!='self':modes+=['source_bounced_pending','source_destroyed_pending']
 if p['tap']:modes+=['repeat_denied','untap_repeat']
 elif p['sacrifice']!='self':modes+=['repeat','stacked_activations']
 if p['sorcery']:modes+=['combat_timing','stack_timing']
 if p['scope']=='target':modes+=['own_target','shroud_target','hexproof_target','target_bounced','target_destroyed']
 if p['scope']=='target' and p['creature'] and p['sacrifice']!='self':modes+=['self_target']
 if p['sacrifice']=='another creature':modes+=['no_fodder','enemy_fodder']
 if p['sacrifice'] in ('creature','another creature','artifact or creature'):modes+=['tapped_fodder','fresh_fodder']
 if p['sacrifice'] in ('creature','artifact or creature'):modes+=['sacrifice_source']
 if p['mechanic'] in ('adapt','monstrosity'):modes+=['preexisting_counter','bounce_recast']
 elif not p['tap'] and p['sacrifice']=='none' and p['scope']=='self':modes+=['bounce_recast']
 return [f'{v}_{owner}' for owner in (0,1) for v in modes]


def validate_witnesses(o,rows):
 _selection_validate_witnesses(o,rows)
 _counter_draw_validate_witnesses(o,rows)
 _graft_validate_witnesses(o,rows)
 _resource_validate_witnesses(o,rows)
 _transfer_validate_witnesses(o,rows)
 for name,record in PINS['cards'].items():
  if name not in rows or any(o.get(name,{}).get(k)!=v for k,v in record.items()):raise ValueError('growth witness changed: '+name)


def expand(name,csvid,d,p,selected,rows,o):
 if p.get('profile')=='selection_growth':return _selection_expand(name,csvid,d,p,selected,rows,o)
 if p.get('profile')=='counter_draw':return _counter_draw_expand(name,csvid,d,p,selected,rows,o)
 if p.get('profile')=='graft_activation':return _graft_expand(name,csvid,d,p,selected,rows,o)
 if p.get('profile')=='resource_growth':return _resource_expand(name,csvid,d,p,selected,rows,o)
 if p.get('profile')=='counter_transfer':return _transfer_expand(name,csvid,d,p,selected,rows,o)
 if p.get('profile'):return _extended_expand(name,csvid,d,p,selected,rows,o)
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';casting=v in ('cast_source','cast_payment','insufficient_cast_mana','fresh_activation','bounce_recast')
  zones={z:[[] if bare else list(ns),[] if bare else list(ns)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE))};zones['library']=[[] if bare else list(LIBRARY),[] if bare else list(LIBRARY)]
  setup=[];entities={};mana=[[30,10,10,10,10,10] for _ in (0,1)];life=[20,20];actions=[];pre=[step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)]
  for pl in (0,1):
   for zone,players in zones.items():setup += [card(n,f'p{pl}_{zone}_{i}',pl,zone) for i,n in enumerate(players[pl])]
  def add(n,ref,pl,zone='battlefield',tapped=False,sick=False):
   item=card(n,ref,pl,zone)
   if zone=='battlefield':item['state']=(['tapped'] if tapped else ['untapped'])+(['sick'] if sick else [])
   setup.append(item);entities[ref]=dict(name=n,player=pl,alive=zone=='battlefield',hand=zone=='hand',tapped=tapped,counters=p['entry'] if n==name and zone=='battlefield' else 0)
   if zone=='hand':zones['hand'][pl].append(n)
  def move(ref,zone):
   e=entities[ref];e['alive']=False;e['hand']=zone=='hand';e['counters']=0;zones[zone][e['player']].append(e['name'])
  def pay(pool,pl):mana[pl]=[a-b for a,b in zip(mana[pl],pool)]
  def cast(ref):
   e=entities[ref];zones['hand'][e['player']].remove(e['name']);e['hand']=False;pay(cost_from_oracle(o[e['name']]),e['player']);e['alive']=True;e['counters']=p['entry'] if ref=='source' else 0
   actions.extend([step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')])
  add(name,'source',owner,'hand' if casting else 'battlefield',tapped=v=='tapped_source',sick=v=='sick_source')
  # Artifact and ordinary creature controls distinguish team membership and controller.
  target_owner=owner if v=='own_target' else 1-owner
  add('Grizzly Bears','recipient',target_owner);add('Hill Giant','control',1-owner);add('Ornithopter','artifact',1-owner if p['sacrifice'] in ('creature','another creature','artifact or creature') else owner);add('Ornithopter','enemy_artifact',1-owner)
  if p['scope']=='artifact_team':add('Ornithopter','artifact_second',owner)
  fodder=p['sacrifice'] in ('creature','another creature','artifact or creature')
  if fodder and v not in ('no_fodder','enemy_fodder','sacrifice_source'):add('Ornithopter','fodder',owner,tapped=v=='tapped_fodder',sick=v=='fresh_fodder')
  if fodder and v in ('repeat','stacked_activations','untap_repeat'):add('Ornithopter','fodder_second',owner)
  if v=='enemy_fodder':add('Ornithopter','fodder',1-owner)
  if v=='untap_repeat':add("Kiora's Follower",'untapper',owner)
  if v=='shroud_target':
   # A native shroud witness avoids adding an Aura entry/attachment effect.
   entities.pop('recipient');setup=[x for x in setup if x['ref']!='recipient'];add('Deadly Insect','recipient',target_owner)
  if v=='hexproof_target':
   entities.pop('recipient');setup=[x for x in setup if x['ref']!='recipient'];add('Gladecover Scout','recipient',target_owner)
  target='recipient' if p['scope']=='target' and v!='self_target' else 'source'
  response=v in ('source_bounced_pending','target_bounced','bounce_recast');response_target='recipient' if v=='target_bounced' else 'source'
  if response:add('Boomerang' if response_target=='source' and not p['creature'] else 'Unsummon','response',owner,'hand')
  if v=='preexisting_counter':add('Hunger of the Howlpack','growth',owner,'hand')
  if v=='stack_timing':add('Giant Growth','pending',owner,'hand')
  if v=='opposing_turn':pre.append(step('turn',player=1-owner,value=0))
  if v=='insufficient_life':life[owner]=p['pay_life']-1;pre.append(step('set_life',player=owner,value=life[owner]))
  if v=='payment':mana[owner]=list(p['activation_mana'])
  if v=='cast_payment':mana[owner]=list(p['mana'])
  if v in ('insufficient_mana','insufficient_cast_mana'):mana[owner]=[0]*6
  if v=='wrong_color':
   mana[owner]=list(p['activation_mana']);color=next(i for i in range(1,6) if mana[owner][i]);mana[owner][0]+=mana[owner][color];mana[owner][color]=0
  for pl in (0,1):pre.extend(step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
  def spell(ref,to,resolve=True):
   e=entities[ref];actions.extend([step('choose_target',player=e['player'],ref=to),step('cast',ref=ref,value=1)]);pay(cost_from_oracle(o[e['name']]),e['player']);zones['hand'][e['player']].remove(e['name']);zones['graveyard'][e['player']].append(e['name']);e['hand']=False
   if resolve:actions.extend([step('resolve'),step('assert_choices_consumed')])
  cast_denied=v=='insufficient_cast_mana'
  if casting:
   if cast_denied:actions.extend([step('cast',ref='source',value=0),observe('source','in_hand',1)])
   else:cast('source')
  if v=='preexisting_counter':spell('growth','source');entities['source']['counters']+=1
  if v=='combat_timing':actions.extend([step('begin_attackers'),step('finish_attackers'),step('begin_blockers'),step('advance_combat')])
  if v=='stack_timing':spell('pending','recipient',False)
  denied=(v in ('insufficient_mana','wrong_color','insufficient_life') or v=='tapped_source' and p['tap'] or v=='sick_source' and p['creature'] and p['tap'] and 'Haste' not in p['keywords']
          or v=='fresh_activation' and p['creature'] and p['tap'] and 'Haste' not in p['keywords'] or p['sorcery'] and v in ('opposing_turn','combat_timing','stack_timing')
          or fodder and v in ('no_fodder','enemy_fodder') and p['sacrifice']=='another creature')
  no_activation=v in ('cast_source','cast_payment','insufficient_cast_mana')
  illegal=v=='shroud_target' or v=='hexproof_target'
  cut=len(actions);resolutions=0
  def activate(fod='fodder',stack=1):
   if p['scope']=='target':actions.append(step('probe_activation_target',ref='source',other=target,index=d.get('activation_dialogs',{}).get(str(owner),-1),value=1))
   actions.append(step('require_can_activate',ref='source',value=1))
   if str(owner) in d.get('activation_dialogs',{}):actions.append(step('choose_dialog',player=owner,value=d['activation_dialogs'][str(owner)]))
   if p['scope']=='target':actions.append(step('choose_target',player=owner,ref=target))
   # Sacrifice helpers bypass the target UI for the AI. Its only cheaper
   # permanents are the explicitly tracked, identical zero-mana fodder.
   if fodder and owner==0:actions.append(step('choose_target',player=owner,ref='source' if v=='sacrifice_source' else fod))
   actions.append(step('activate',ref='source',value=1));pay(p['activation_mana'],owner);life[owner]-=p['pay_life']
   if p['tap']:entities['source']['tapped']=True
   if p['sacrifice']=='self':move('source','graveyard')
   elif fodder:move('source' if v=='sacrifice_source' else fod,'graveyard')
   actions.extend([step('assert_stack',value=stack),step('assert_life',player=owner,value=life[owner])]);actions.extend(step('assert_mana',player=owner,color=i,value=n) for i,n in enumerate(mana[owner]))
  def effect():
   nonlocal resolutions
   resolutions+=1
   refs=[k for k,e in entities.items() if e['alive'] and e['player']==owner and 'Artifact' in o[e['name']]['type_line'] and 'Creature' in o[e['name']]['type_line']] if p['scope']=='artifact_team' else [target]
   for ref in refs:
    e=entities[ref]
    if e['alive'] and not (p['mechanic']=='adapt' and e['counters']) and not (p['mechanic']=='monstrosity' and resolutions>1):e['counters']+=p['amount']
   life[owner]+=p['gain_life']
  if not no_activation:
   if illegal:actions.append(step('probe_activation_target',ref='source',other=target,index=d.get('activation_dialogs',{}).get(str(owner),-1),value=0))
   elif denied:actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
   else:
    activate();cut=len(actions)
    # Observe counters before resolution, making costs and effect distinct.
    if entities[target]['alive']:actions.append(observe(target,'plus1_counters',entities[target]['counters']))
    if v=='stacked_activations':
     if p['sorcery']:actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
     else:activate('fodder_second',2)
    if v in ('source_destroyed_pending','target_destroyed'):
     ref='recipient' if v=='target_destroyed' else 'source';actions.append(step('destroy',ref=ref));move(ref,'graveyard')
    if v in ('source_bounced_pending','target_bounced'):
     spell('response',response_target,False);actions.append(step('assert_stack',value=2));move(response_target,'hand')
    actions.extend([step('resolve'),step('assert_choices_consumed')]);effect()
    if v=='stacked_activations' and not p['sorcery']:effect()
    if v=='repeat_denied':actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
    if v in ('repeat','untap_repeat'):
     if v=='untap_repeat':
      actions.extend([step('choose_target',player=owner,ref='source'),step('activate',ref='untapper',value=1),step('resolve'),step('assert_choices_consumed')]);entities['source']['tapped']=False;entities['untapper']['tapped']=True
     activate('fodder_second');actions.extend([step('resolve'),step('assert_choices_consumed')]);effect()
    if v=='bounce_recast':
     spell('response','source');move('source','hand');actions.append(step('bind_hand',player=owner,value=csvid,ref='source'));cast('source');resolutions=0
     # Re-entry clears both the counters and the monstrous/adapt history.
     activate();actions.extend([step('resolve'),step('assert_choices_consumed')]);effect()
  if v=='stack_timing':actions.extend([step('resolve'),step('assert_choices_consumed')])
  if v=='combat_timing':actions.extend([step('combat_damage'),step('end_combat')])
  if v=='cleanup':actions.extend([step('cleanup'),step('refresh')])
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref,e in entities.items():
   if not e['alive']:continue
   checks.extend([observe(ref,'in_play',1),observe(ref,'tapped',int(e['tapped'])),observe(ref,'plus1_counters',e['counters'])])
   if 'Creature' in o[e['name']]['type_line']:
    bonus=3 if v=='stack_timing' and ref=='recipient' else 0
    checks.extend(observe(ref,k,int(o[e['name']][k])+e['counters']+bonus) for k in ('power','toughness'))
  names={x['name'] for x in setup}
  for pl in (0,1):
   checks.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))]);checks.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
   for nm in sorted(names):checks.append(step('assert_zone_multiplicity',player=pl,zone='battlefield',index=rows[nm]['csvid'],value=sum(e['alive'] and e['player']==pl and e['name']==nm for e in entities.values())))
   for zone,players in zones.items():
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(players[pl])))
    checks.extend(step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=players[pl].count(nm)) for nm in sorted(names))
    if zone=='library':checks.extend(step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(players[pl]))
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
   at=0 if v=='ready_snapshot' else len(actions) if v=='resolved_snapshot' else cut
   steps=pre+actions[:at]+[step('capture')]+actions[at:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[at:]+checks+[step('assert_recorded_state')]
  result.append(dict(name=f'GrowthCounterActivationRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,
                     _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_keywords=p['keywords'],untested_abilities=p['inactive'],untested_components=['source keyword combat','inactive printed abilities','control changes','multiplayer'])))
 return result


def _extended_spec(d,r):
 """Opt-in reviewed profiles leave the original thirty-card contract intact."""
 import copy
 from recipes import cost_from_oracle
 review=PINS.get('profiles',{}).get(r.get('name')) if r else None
 if (not review or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
     or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r.get('oracle_text')
     or any(r.get(k)!=v for k,v in review['oracle'].items())
     or json.dumps(d.get('params'),sort_keys=True)!=json.dumps(review['params'],sort_keys=True) or 'variants' not in d):
  raise ValueError('growth extension requires complete reviewed source identity and explicit profile')
 p=copy.deepcopy(review['params'])
 if p['profile'] not in ('permanent','reinforce','monstrosity','adapt','scavenge','outlast','energy'):raise ValueError('unsupported reviewed growth extension mechanism')
 return dict(p,family=FAMILY,mana=None if 'Land' in r['type_line'] else cost_from_oracle(r),activation_mana=cost_from_oracle(dict(mana_cost=p['cost'])) if p['cost'] else [0]*6,creature='Creature' in r['type_line'],source_is_artifact='Artifact' in r['type_line'])


def _extended_variants(p):
 modes=['effect','payment','bare_zones','cleanup','opposing_turn','ready_snapshot','pending_snapshot','resolved_snapshot']
 if sum(p['activation_mana']):modes+=['insufficient_mana']
 if p['mana'] is not None:modes+=['cast_source','cast_payment','insufficient_cast_mana']
 if any(p['activation_mana'][1:]):modes+=['wrong_color']
 if p['profile']=='reinforce':modes+=['no_creatures','shroud_only','fresh_recipient','repeat_denied']
 elif p['profile']=='scavenge':modes+=['no_creatures','shroud_only','fresh_recipient','repeat_denied','targeting_pending']
 else:
  modes+=([] if p['profile']=='energy' else ['fresh_source'])+['tapped_source']
  if p['sacrifice']!='self':modes+=['source_bounced_pending','source_destroyed_pending']
  if p['sacrifice']=='none' or p['sacrifice'] in ('Human','artifact'):modes+=['untap_repeat' if p['tap'] else 'repeat','stacked_activations']
 if p['target']!='none':modes+=['own_target']+([] if p['profile']=='scavenge' else ['target_bounced','target_destroyed'])
 if p['profile'] not in ('reinforce','scavenge') and p['target']=='creature':
  modes+=['shroud_target','hexproof_target','noncreature_target']
  if p['creature'] and p['sacrifice']!='self':modes+=['self_target']
 if p['tap']:modes+=['sick_source','repeat_denied']
 if p['sacrifice']=='Human':modes+=['tapped_fodder','fresh_fodder','sacrifice_source']
 if p['sacrifice']=='artifact':
  modes+=['tapped_fodder','fresh_fodder']
  modes+=['sacrifice_source'] if p['source_is_artifact'] else ['no_fodder','enemy_fodder']
 if p['sorcery']:modes+=['combat_timing','stack_timing']
 if p['profile']=='monstrosity':modes+=['empty_opponent_hand','large_opponent_hand','preexisting_counter','bounce_recast']
 if p['profile']=='adapt':modes+=['preexisting_counter','bounce_recast']
 if p.get('helper_regressions'):modes+=['query_purity']
 if p['profile']=='outlast':modes+=['grant_source','grant_ally','grant_counterless','grant_opponent','grant_removed','grant_cleanup']
 if p['profile']=='energy':modes+=['blink_repeat']
 if p.get('pay_life'):modes+=['insufficient_life']
 if any(e['effect']=='destroy_monster_target' for e in p['extras']):modes+=['trigger_no_targets','trigger_shroud_only','trigger_hexproof_only','trigger_indestructible']
 if p.get('grave_discount'):
  modes+=['discount_empty','discount_nonmatching','discount_opponent','discount_mixed','discount_zero_generic','discount_excess','discount_excess_overpay']
  if p.get('discount_cast'):modes+=['cast_discount_mixed','cast_discount_zero_generic','cast_discount_opponent']
 if any(e['effect']=='opponents_ground_cant_block' for e in p['extras']):modes+=['monstrous_ground_block','monstrous_flying_block','monstrous_friendly_block','monstrous_block_cleanup','monstrous_fresh_block']
 if p.get('attack_through_defender'):modes+=['monstrous_trample_cleanup']
 if any(e['effect']=='combat_player_sacrifice' for e in p['extras']):modes+=['monstrous_reach_cleanup']
 if p['amount']=='elves_you_control':modes+=['two_extra_elves','changeling_elf']
 if any(e['effect']=='self_per_other_counter_creature' for e in p['extras']):modes+=['countered_ally','many_counters_one_ally']
 for extra in p['extras']:
  if extra['effect']=='monstrous_keywords':
   modes+=['monstrous_'+k.lower().replace(' ','_') for k in extra['keywords']]
   if 'Menace' in extra['keywords']:modes+=['monstrous_menace_pair']
 return [m+'_'+str(owner) for owner in (0,1) for m in modes]


def _extended_expand(name,csvid,d,p,selected,rows,o):
 import copy
 from recipes import card,step,observe,cost_from_oracle
 if not selected or len(selected)!=len(set(selected)) or any(v not in _extended_variants(p) for v in selected):raise ValueError('distinct reviewed growth extension variants required')
 result=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);hand=p['profile']=='reinforce';grave=p['profile']=='scavenge';bare=mode=='bare_zones';cast_only=mode in ('cast_source','cast_payment','insufficient_cast_mana') or mode.startswith('cast_discount_')
  casting=cast_only or mode=='fresh_source' or p['profile']=='energy'
  zones={z:[[] if bare else list(ns),[] if bare else list(ns)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY))}
  if mode=='empty_opponent_hand':zones['hand'][1-owner]=[]
  if mode=='large_opponent_hand':zones['hand'][1-owner]=list(HAND)*2
  if p.get('grave_discount'):
   creature=p['grave_discount']==['Creature'];matching=['Grizzly Bears','Scaled Wurm','Ornithopter'] if creature else ['Giant Growth','Rampant Growth','Lightning Bolt'];nonmatching=['Forest','Sol Ring','Giant Growth' if creature else 'Grizzly Bears']
   grave_mode=mode.removeprefix('cast_') if mode.startswith('cast_discount_') else mode
   if grave_mode=='discount_excess_overpay':grave_mode='discount_excess'
   if grave_mode=='discount_empty':zones['graveyard'][owner]=[]
   if grave_mode=='discount_nonmatching':zones['graveyard'][owner]=nonmatching*3
   if grave_mode=='discount_opponent':zones['graveyard'][owner]=nonmatching;zones['graveyard'][1-owner]=matching*3
   if grave_mode=='discount_mixed':zones['graveyard'][owner]=matching+nonmatching
   if grave_mode in ('discount_zero_generic','discount_excess'):zones['graveyard'][owner]=[matching[0]]*(7 if grave_mode=='discount_zero_generic' else 8)+nonmatching
  entities={};setup=[];mana=[[40,20,20,20,20,20],[40,20,20,20,20,20]];life=[20,20];energy=[0,0];actions=[];pre=([step('prepare_graveyard_abilities')] if grave else [])+[step('reset_death_history'),step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)];cut=0;activations=0
  for pl in (0,1):
   for z,players in zones.items():setup.extend(card(n,f'p{pl}_{z}_{i}',pl,z) for i,n in enumerate(players[pl]))
  def add(n,ref,pl,z='battlefield',tapped=False,sick=False):
   item=card(n,ref,pl,z)
   if z=='battlefield':item['state']=(['tapped'] if tapped else ['untapped'])+(['sick'] if sick else [])
   setup.append(item);entities[ref]=dict(name=n,player=pl,alive=z=='battlefield',tapped=tapped,counters=p['entry'] if ref=='source' and z=='battlefield' else 0,bonus=0,monstrous=False,damage=0,hexproof=False,indestructible=False)
   if z in ('hand','graveyard'):zones[z][pl].append(n)
  def move(ref,z):
   e=entities[ref];e['alive']=False;e['counters']=0;e['bonus']=0;e['monstrous']=False;zones[z][e['player']].append(e['name'])
  def pay(pool,pl):mana[pl]=[a-b for a,b in zip(mana[pl],pool)]
  def source_price(pool,casting=False):
   result=list(pool)
   if p.get('grave_discount') and (not casting or p.get('discount_cast')):result[0]=max(0,result[0]-sum(bool(set(p['grave_discount'])&set(o[n]['type_line'].partition(' — ')[0].split())) for n in zones['graveyard'][owner]))
   return result
  def characteristic(ref,k):
   e=entities[ref];bonus=0
   if ref=='source' and any(x['effect']=='self_per_other_counter_creature' for x in p['extras']):bonus=sum(other!=ref and row['alive'] and row['player']==owner and row['counters']>0 and 'Creature' in o[row['name']]['type_line'] for other,row in entities.items())
   return int(o[e['name']][k])+e['counters']+e['bonus']+bonus
  def keywords(ref):
   e=entities[ref];ks=set(p['keywords'] if ref=='source' else o[e['name']].get('keywords',[]))
   if e['hexproof']:ks.add('Hexproof')
   if e['indestructible']:ks.add('Indestructible')
   if ref=='source' and e['monstrous']:
    for x in p['extras']:
     if x['effect']=='monstrous_keywords':ks.update(x['keywords'])
   return ks
  def stats():
   s=[]
   for ref,e in entities.items():
    if e['alive']:
     s.extend([observe(ref,'in_play',1),observe(ref,'tapped',int(e['tapped'])),observe(ref,'plus1_counters',e['counters'])])
     if 'Creature' in o[e['name']]['type_line']:s.extend(observe(ref,k,characteristic(ref,k)) for k in ('power','toughness'))
     if e['damage']:s.append(observe(ref,'damage',e['damage']))
   return s
  def receipts():
   s=[];universe={x['name'] for x in setup}
   for pl in (0,1):
    s.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))]);s.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
    if p['profile']=='energy':s.append(step('assert_energy',player=pl,value=energy[pl]))
    s.extend(step('assert_zone_multiplicity',player=pl,zone='battlefield',index=rows[n]['csvid'],value=sum(e['alive'] and e['player']==pl and e['name']==n for e in entities.values())) for n in sorted(universe))
    for z,players in zones.items():
     s.append(step('assert_zone',player=pl,zone=z,value=len(players[pl])));s.extend(step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=players[pl].count(n)) for n in sorted(universe))
     if z=='library':s.extend(step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(players[pl]))
   return s
  def block_trial(attacker,blocker,legal):
   return [step('begin_attackers'),step('attack',ref=attacker,value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref=blocker,other=attacker,value=int(legal)),step('advance_combat'),observe(attacker,'blocked',int(legal)),step('remove_from_combat',ref=attacker),step('remove_from_combat',ref=blocker)]
  def spell(n,ref,target,pl=owner,resolve=True,dialog=None):
   add(n,ref,pl,'hand');actions.extend([step('choose_target',player=pl,ref=target),step('cast',ref=ref,value=1)]);pay(cost_from_oracle(o[n]),pl);zones['hand'][pl].remove(n);zones['graveyard'][pl].append(n)
   if dialog is not None:actions.append(step('choose_dialog',player=pl,value=dialog))
   if resolve:actions.extend([step('resolve'),step('assert_choices_consumed')])
  def cast_source():
   actions.extend([step('cast',ref='source',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]);pay(source_price(p['mana'],True),owner);zones['hand'][owner].remove(name);entities['source']['alive']=True;entities['source']['tapped']=False;entities['source']['counters']=p['entry'];entities['source']['monstrous']=False;entities['source']['hexproof']=False;entities['source']['indestructible']=False
   if p['profile']=='energy':energy[owner]+=p['entry_energy']
  add(name,'source',owner,'hand' if hand or casting else 'graveyard' if grave else 'battlefield',tapped=mode=='tapped_source' or p.get('tap_for_mana_denial',False) and mode in ('insufficient_mana','wrong_color'),sick=mode=='sick_source')
  targeted_monster=next((e for e in p['extras'] if e['effect']=='destroy_monster_target'),None)
  isolate_trigger=targeted_monster and targeted_monster['predicate']=='non_gorgon' and mode.startswith('trigger_')
  target_owner=owner if mode=='own_target' or mode in ('grant_ally','grant_counterless','grant_removed','grant_cleanup','countered_ally','many_counters_one_ally') or isolate_trigger else 1-owner
  if mode!='no_creatures':add('Deadly Insect' if mode=='shroud_only' or mode=='shroud_target' else 'Gladecover Scout' if mode=='hexproof_target' else 'Darksteel Relic' if mode=='noncreature_target' else 'Grizzly Bears','recipient',target_owner,'hand' if mode=='fresh_recipient' else 'battlefield',tapped=mode in ('monstrous_menace','monstrous_menace_pair'))
  if mode not in ('no_creatures','shroud_only'):add('Scaled Wurm','unselected',owner if isolate_trigger else 1-owner,tapped=mode in ('monstrous_menace','monstrous_menace_pair'))
  add('Darksteel Relic','noncreature',1-owner if p['sacrifice']=='artifact' else owner)
  if p['sacrifice']=='Human' and mode!='sacrifice_source':add('Eager Cadet','fodder',owner,tapped=mode=='tapped_fodder',sick=mode=='fresh_fodder')
  if p['sacrifice']=='Human' and mode in ('repeat','stacked_activations'):add('Eager Cadet','fodder_second',owner)
  if p['sacrifice']=='artifact' and mode not in ('no_fodder','enemy_fodder','sacrifice_source'):add('Ornithopter','fodder',owner,tapped=mode=='tapped_fodder',sick=mode=='fresh_fodder')
  if p['sacrifice']=='artifact' and mode in ('repeat','stacked_activations','untap_repeat'):add('Ornithopter','fodder_second',owner)
  if mode=='enemy_fodder':add('Ornithopter','fodder',1-owner)
  if mode=='untap_repeat':add("Kiora's Follower",'untapper',owner)
  if p['amount']=='elves_you_control':
   add('Elvish Warrior','opposing_elf',1-owner)
   if mode=='two_extra_elves':add('Elvish Warrior','elf_one',owner);add('Elvish Warrior','elf_two',owner)
   if mode=='changeling_elf':add('Mothdust Changeling','elf_changeling',owner)
  protection=p['profile']=='energy' or any(e['effect']=='monstrous_keywords' and 'Hexproof' in e['keywords'] for e in p['extras'])
  if protection:add('Unsummon','protection_probe',1-owner,'hand')
  ground_mode=mode in ('monstrous_ground_block','monstrous_flying_block','monstrous_friendly_block','monstrous_block_cleanup','monstrous_fresh_block')
  reach_mode=mode in ('monstrous_reach','monstrous_reach_cleanup')
  akros_mode=mode in ('monstrous_trample','monstrous_trample_cleanup') and p.get('attack_through_defender')
  if ground_mode:add('Ambush Viper' if mode=='monstrous_fresh_block' else 'Ornithopter' if mode=='monstrous_flying_block' else 'Scaled Wurm','rule_blocker',1-owner,'hand' if mode=='monstrous_fresh_block' else 'battlefield')
  if reach_mode:add('Suntail Hawk','reach_attacker',1-owner);add('Hunger of the Howlpack','reach_proof_growth',owner,'hand')
  if akros_mode:add('Ornithopter','trample_blocker',1-owner)
  target='source' if mode=='self_target' else 'recipient'
  if mode=='opposing_turn' and p['profile']!='energy':pre.append(step('turn',player=1-owner,value=0))
  if reach_mode or mode=='monstrous_friendly_block':pre.append(step('turn',player=1-owner,value=0))
  if mode=='cast_payment' or mode.startswith('cast_discount_'):mana[owner]=source_price(p['mana'],True)
  if mode=='insufficient_cast_mana':mana[owner]=[0]*6
  if mode=='payment' and p['profile']!='energy' or mode in ('discount_excess','discount_excess_overpay'):mana[owner]=source_price(p['activation_mana'])
  if mode=='discount_excess_overpay':mana[owner][0]=3
  if mode=='insufficient_mana':mana[owner]=[0]*6
  if mode=='insufficient_life':life[owner]=p['pay_life']-1;pre.append(step('set_life',player=owner,value=life[owner]))
  if mode=='wrong_color':
   mana[owner]=source_price(p['activation_mana']);color=next(i for i in range(1,6) if mana[owner][i]);mana[owner][0]+=mana[owner][color];mana[owner][color]=0
  for pl in (0,1):pre.extend(step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
  if mode=='fresh_recipient':
   e=entities['recipient'];actions.extend([step('turn',player=target_owner,value=0),step('cast',ref='recipient',value=1),step('resolve'),step('assert_choices_consumed'),observe('recipient','sick',1),step('turn',player=owner,value=0)])
   pay(cost_from_oracle(o[e['name']]),target_owner);zones['hand'][target_owner].remove(e['name']);e['alive']=True
  if casting:
   if mode=='insufficient_cast_mana':actions.append(step('cast',ref='source',value=0))
   else:cast_source()
  if p['profile']=='energy' and entities['source']['alive']:
   actions.append(observe('source','sick',1))
   if mode=='opposing_turn':actions.append(step('turn',player=1-owner,value=0))
   if mode=='tapped_source':spell('Twiddle','source_tap','source',dialog=0);entities['source']['tapped']=True
  if p['profile']=='energy' and mode=='payment':
   mana[owner]=[0]*6;actions.extend(step('set_mana',player=owner,color=i,value=0) for i in range(6))
  if any(e['effect']=='destroy_non_gorgons' for e in p['extras']):
   for pl in (0,1):add('Gorgon Recluse','gorgon_'+str(pl),pl);add('Darksteel Myr','indestructible_'+str(pl),pl)
   add('Grizzly Bears','friendly_creature',owner)
  if any(e['effect']=='exile_artifacts_enchantments' for e in p['extras']):
   for pl in (0,1):add('Ivory Mask','enchantment_'+str(pl),pl);add('Darksteel Myr','indestructible_'+str(pl),pl)
  if any(e['effect']=='opponent_damage_all' for e in p['extras']):add('Grizzly Bears','friendly_creature',owner)
  if targeted_monster:
   add('Suntail Hawk','friendly_matching',owner)
   if targeted_monster['predicate']=='non_gorgon':add('Gorgon Recluse','enemy_nonmatching',1-owner)
   if mode!='trigger_no_targets':add('Sphinx of Jwar Isle' if mode=='trigger_shroud_only' else 'Aven Fleetwing' if mode=='trigger_hexproof_only' else 'Darksteel Gargoyle' if mode=='trigger_indestructible' else 'Suntail Hawk' if targeted_monster['predicate']=='flying' else 'Hill Giant','monster_victim',1-owner)
  if mode in ('grant_ally','grant_opponent','grant_removed','grant_cleanup','countered_ally','many_counters_one_ally'):
   spell('Hunger of the Howlpack','grant_growth','recipient');entities['recipient']['counters']+=1
   if mode=='many_counters_one_ally':spell('Hunger of the Howlpack','grant_growth_second','recipient');entities['recipient']['counters']+=1
  if mode=='preexisting_counter':spell('Hunger of the Howlpack','growth','source');entities['source']['counters']+=1
  if mode=='combat_timing':actions.extend([step('begin_attackers'),step('finish_attackers'),step('begin_blockers'),step('advance_combat')])
  if mode=='stack_timing':spell('Giant Growth','pending','recipient',resolve=False)
  actions+=stats()
  if protection and entities['source']['alive']:actions.append(step('can_target_creature',ref='protection_probe',other='source',value=1))
  denied=mode in ('insufficient_mana','wrong_color','insufficient_life','no_creatures','shroud_only','no_fodder','enemy_fodder') or p['tap'] and mode=='tapped_source' or p['tap'] and p['creature'] and mode=='sick_source' or p['tap'] and p['creature'] and mode=='fresh_source' and 'Haste' not in p['keywords'] or p['sorcery'] and mode in ('opposing_turn','combat_timing','stack_timing')
  illegal=mode in ('shroud_target','hexproof_target','noncreature_target')
  can='require_can_activate_hand' if hand else 'require_can_activate_graveyard' if grave else 'require_can_activate';activate='activate_hand' if hand else 'activate_graveyard' if grave else 'activate'
  def activation(fodder='fodder',stack=1):
   nonlocal activations
   if not hand and not grave and p['target']=='creature':actions.append(step('probe_activation_target',ref='source',other=target,index=p.get('activation_dialog',-1),value=1))
   actions.append(step(can,ref='source',value=1))
   if str(owner) in p.get('activation_dialogs',{}):actions.append(step('choose_dialog',player=owner,value=p['activation_dialogs'][str(owner)]))
   elif hand or grave or 'activation_dialog' in p and (not p.get('dialog_when_countered') or entities['source']['counters']):actions.append(step('choose_dialog',player=owner,value=p.get('activation_dialog',1)))
   if p['target']=='creature':actions.append(step('choose_target',player=owner,ref=target))
   if p['sacrifice'] in ('Human','artifact') and owner==0:actions.append(step('choose_target',player=owner,ref='source' if mode=='sacrifice_source' else fodder))
   if mode=='query_purity':actions.extend([step(can,ref='source',value=1),step(can,ref='source',value=1),step('assert_stack',value=0)]+receipts())
   if mode in ('discount_excess','discount_excess_overpay'):actions.append(step('choose_x',player=owner,value=3 if mode=='discount_excess_overpay' else 0))
   actions.append(step(activate,ref='source',value=1));pay(source_price(p['activation_mana']),owner)
   if mode in ('discount_excess','discount_excess_overpay'):actions.append(step('assert_unused_choices',value=1))
   if p.get('pay_life'):
    life[owner]-=p['pay_life'];actions.append(step('assert_life',player=owner,value=life[owner]))
   activations+=1
   if p['profile']=='energy':energy[owner]-=p['energy_cost'];actions.append(step('assert_energy',player=owner,value=energy[owner]))
   if p['tap']:entities['source']['tapped']=True
   if p['sacrifice']=='discard_source':zones['hand'][owner].remove(name);zones['graveyard'][owner].append(name)
   elif p['sacrifice']=='exile_source':zones['graveyard'][owner].remove(name);zones['exile'][owner].append(name)
   elif p['sacrifice']=='self':move('source','graveyard')
   elif p['sacrifice'] in ('Human','artifact'):move('source' if mode=='sacrifice_source' else fodder,'graveyard')
   actions.append(step('assert_stack',value=stack));actions.extend(step('assert_mana',player=owner,color=i,value=n) for i,n in enumerate(mana[owner]))
   # Scavenge targets are selected on activation, before priority/responses.
   # Retain this receipt even though the current handler picks at resolution.
   if grave and mode=='targeting_pending':actions.append(step('assert_choices_consumed'))
  def effect():
   legal=p['target']=='none' or entities[target]['alive']
   if not legal:return
   sr=entities['source'];grew=True
   if p['profile']=='monstrosity':grew=sr['alive'] and not sr['monstrous']
   if p['profile']=='adapt':grew=sr['alive'] and not sr['counters']
   if grew:
    rec=target if p['recipient']=='target' else 'source'
    amount=sum(e['alive'] and e['player']==owner and ('Elf' in o[e['name']]['type_line'].partition(' — ')[2].split() or 'Changeling' in o[e['name']].get('keywords',[])) for e in entities.values()) if p['amount']=='elves_you_control' else p['amount']
    if entities[rec]['alive']:entities[rec]['counters']+=amount
    if p['profile']=='monstrosity':sr['monstrous']=True
   for extra in p['extras']:
    if extra['event']=='monstrous' and not grew:continue
    if extra['effect']=='opponent_hand_damage':life[1-owner]-=len(zones['hand'][1-owner])
    elif extra['effect']=='target_shrink':
     e=entities[target];e['bonus']-=extra['amount']
     if int(o[e['name']]['toughness'])+e['counters']+e['bonus']<=0:move(target,'graveyard')
    elif extra['effect']=='source_hexproof_until_cleanup':
     if sr['alive']:sr['hexproof']=True
    elif extra['effect']=='source_indestructible_until_cleanup':
     if sr['alive']:sr['indestructible']=True
    elif extra['effect']=='destroy_non_gorgons':
     for ref,e in entities.items():
      if e['alive'] and 'Creature' in o[e['name']]['type_line'] and 'Gorgon' not in o[e['name']]['type_line'].partition(' — ')[2].split() and 'Indestructible' not in keywords(ref):move(ref,'graveyard')
    elif extra['effect']=='exile_artifacts_enchantments':
     for ref,e in entities.items():
      if e['alive'] and {'Artifact','Enchantment'}&set(o[e['name']]['type_line'].partition(' — ')[0].split()):move(ref,'exile')
    elif extra['effect']=='opponent_damage_all':
     life[1-owner]-=extra['amount']
     for ref,e in entities.items():
      if e['alive'] and e['player']!=owner and 'Creature' in o[e['name']]['type_line']:
       e['damage']+=extra['amount']
       if e['damage']>=characteristic(ref,'toughness') and 'Indestructible' not in keywords(ref):move(ref,'graveyard')
    elif extra['effect']=='destroy_monster_target':
     chosen=monster_target_ref()
     if chosen and 'Indestructible' not in keywords(chosen):move(chosen,'graveyard')
    elif extra['effect'] in ('counter_keyword','monstrous_keywords','self_per_other_counter_creature','opponents_ground_cant_block','combat_player_sacrifice'):pass
    else:raise ValueError('unsupported reviewed growth extra effect')
  def trigger_target_legal(ref):
   e=entities.get(ref)
   if not e or not e['alive'] or e['player']==owner or 'Creature' not in o[e['name']]['type_line'] or {'Shroud','Hexproof'}&keywords(ref):return False
   return 'Flying' in keywords(ref) if targeted_monster['predicate']=='flying' else 'Gorgon' not in o[e['name']]['type_line'].partition(' — ')[2].split()
  def monster_target_ref():return next((ref for ref in ['monster_victim']+list(entities) if trigger_target_legal(ref)),None)
  def queue_monstrous_target():
   if targeted_monster and entities['source']['alive'] and not entities['source']['monstrous']:
    chosen=monster_target_ref()
    if chosen:actions.append(step('choose_target',player=owner,ref=chosen))
  if not cast_only:
   if denied:actions.extend([step(can,ref='source',value=0),step(activate,ref='source',value=0)])
   elif illegal:actions.append(step('probe_activation_target',ref='source',other=target,index=p.get('activation_dialog',-1),value=0))
   else:
    activation();cut=len(actions)
    if p['target']=='creature' and entities[target]['alive']:actions.append(observe(target,'plus1_counters',entities[target]['counters']))
    if mode=='stacked_activations':
     if p['sorcery'] or p['tap'] or p['profile']=='energy':actions.extend([step(can,ref='source',value=0),step(activate,ref='source',value=0)])
     else:activation('fodder_second',2)
    if mode in ('source_bounced_pending','target_bounced'):
     changed='source' if mode=='source_bounced_pending' else target;spell('Boomerang' if changed=='source' and not p['creature'] else 'Unsummon','response',changed,resolve=False);actions.append(step('assert_stack',value=2));move(changed,'hand')
    if mode in ('source_destroyed_pending','target_destroyed'):
     changed='source' if mode=='source_destroyed_pending' else target;actions.append(step('destroy',ref=changed))
     if 'Indestructible' not in keywords(changed):move(changed,'graveyard')
    if mode!='targeting_pending':queue_monstrous_target();actions.extend([step('resolve'),step('assert_choices_consumed')]);effect()
    if mode=='stacked_activations' and not p['sorcery'] and not p['tap'] and p['profile']!='energy':effect()
    if mode=='repeat_denied':actions.extend([step(can,ref='source',value=0),step(activate,ref='source',value=0)])
    if mode in ('repeat','untap_repeat'):
     if p['profile']=='energy':actions.extend([step(can,ref='source',value=0),step(activate,ref='source',value=0)])
     else:
      if mode=='untap_repeat':
       actions.extend([step('choose_target',player=owner,ref='source'),step('activate',ref='untapper',value=1),step('resolve'),step('assert_choices_consumed')]);entities['untapper']['tapped']=True;entities['source']['tapped']=False
      activation('fodder_second');queue_monstrous_target();actions.extend([step('resolve'),step('assert_choices_consumed')]);effect()
    if mode=='bounce_recast':
     spell('Unsummon','bounce','source');move('source','hand');actions.append(step('bind_hand',player=owner,value=csvid,ref='source'));cast_source();activation();queue_monstrous_target();actions.extend([step('resolve'),step('assert_choices_consumed')]);effect()
    if mode=='blink_repeat':
     spell('Cloudshift','blink','source');entities['source']['counters']=0;entities['source']['hexproof']=False;energy[owner]+=p['entry_energy'];actions.append(step('bind_permanent',player=owner,value=csvid,ref='source'));activation();actions.extend([step('resolve'),step('assert_choices_consumed')]);effect()
  if mode=='stack_timing':actions.extend([step('resolve'),step('assert_choices_consumed')]);entities['recipient']['bonus']+=3
  if mode=='combat_timing':actions.extend([step('combat_damage'),step('end_combat')])
  if protection and entities['source']['alive']:actions.append(step('can_target_creature',ref='protection_probe',other='source',value=int('Hexproof' not in keywords('source'))))
  temporary_indestructible=any(e['effect']=='source_indestructible_until_cleanup' for e in p['extras'])
  if temporary_indestructible and activations and entities['source']['alive']:
   actions.extend([step('destroy',ref='source'),step('require_card',ref='source',property='in_play',value=1)])
   actions.append(step('destroy',ref='unselected'));move('unselected','graveyard')
  if mode=='monstrous_indestructible':
   actions.extend([step('destroy',ref='source'),step('require_card',ref='source',property='in_play',value=1),step('destroy',ref='unselected')]);move('unselected','graveyard')
  if mode in ('monstrous_menace','monstrous_menace_pair'):
   add('Savannah Lions','menace_one',1-owner)
   pair=mode.endswith('_pair')
   if pair:add('Savannah Lions','menace_two',1-owner)
   actions.extend([step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='menace_one',other='source',value=int(pair))]);entities['source']['tapped']=True
   if pair:actions.append(step('block',ref='menace_two',other='source',value=1))
   actions.extend([step('advance_combat'),observe('source','blocked',int(pair))])
   if pair:
    actions.extend([observe('menace_one','blocking_count',1),observe('menace_two','blocking_count',1)]+[step('remove_from_combat',ref=ref) for ref in ('source','menace_one','menace_two')]+[step('cleanup')])
   else:actions.extend([step('combat_damage'),step('end_combat')]);life[1-owner]-=characteristic('source','power')
  if ground_mode:
   actions.append(step('refresh'))
   if mode=='monstrous_fresh_block':
    actions.extend([step('cast',ref='rule_blocker',value=1),step('resolve'),step('assert_choices_consumed'),observe('rule_blocker','sick',1)])
    zones['hand'][1-owner].remove('Ambush Viper');pay(cost_from_oracle(o['Ambush Viper']),1-owner);entities['rule_blocker']['alive']=True
   if mode=='monstrous_block_cleanup':actions.extend([step('capture')]+block_trial('source','rule_blocker',False)+[step('restore'),step('cleanup'),step('turn',player=owner,value=0)])
   attacker,blocker=('rule_blocker','source') if mode=='monstrous_friendly_block' else ('source','rule_blocker')
   legal=mode in ('monstrous_flying_block','monstrous_friendly_block','monstrous_block_cleanup')
   actions.extend(block_trial(attacker,blocker,legal)+[step('cleanup')]);entities[attacker]['tapped']=True
  if reach_mode:
   if mode=='monstrous_reach_cleanup':actions.extend([step('cleanup'),step('turn',player=1-owner,value=0)])
   actions.extend([step('refresh'),step('begin_attackers'),step('attack',ref='reach_attacker',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='source',other='reach_attacker',value=1),step('advance_combat'),step('combat_damage'),step('end_combat'),step('resolve'),step('assert_choices_consumed')]);move('reach_attacker','graveyard');entities['source']['damage']=1
  if akros_mode:
   if mode=='monstrous_trample_cleanup':actions.extend([step('cleanup'),step('turn',player=owner,value=0)])
   actions.extend([step('refresh'),step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='trample_blocker',other='source',value=1),step('advance_combat'),step('combat_damage'),step('end_combat')]);entities['source']['tapped']=True;life[1-owner]-=characteristic('source','power')-characteristic('trample_blocker','toughness');move('trample_blocker','graveyard')
  if mode.startswith('grant_'):
   gift=next(e['keyword'] for e in p['extras'] if e['effect']=='counter_keyword')
   actor_ref='source' if mode=='grant_source' else 'recipient';actor=entities[actor_ref]['player']
   if mode=='grant_removed':spell('Unsummon','grant_remove','source');move('source','hand')
   if mode=='grant_cleanup':actions.extend([step('cleanup'),step('turn',player=actor,value=0)])
   if entities[actor_ref]['tapped']:spell('Twiddle','grant_untap',actor_ref,pl=actor,dialog=1);entities[actor_ref]['tapped']=False
   active=entities['source']['alive'] and actor==owner and entities[actor_ref]['counters']>0
   if gift=='Trample':spell('Giant Growth','grant_pump',actor_ref,pl=actor);entities[actor_ref]['bonus']+=3
   power=characteristic(actor_ref,'power');tough=characteristic(actor_ref,'toughness')
   if gift=='Reach':
    add('Suntail Hawk','grant_fighter',1-actor)
    actions.extend([step('turn',player=1-actor,value=0),step('begin_attackers'),step('attack',ref='grant_fighter',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref=actor_ref,other='grant_fighter',value=int(active)),step('advance_combat'),step('combat_damage'),step('end_combat')]);entities['grant_fighter']['tapped']=True
    if active:move('grant_fighter','graveyard');entities[actor_ref]['damage']=1
    else:life[actor]-=1
   else:
    blocker='Grizzly Bears' if gift=='Flying' else 'Savannah Lions' if gift=='First strike' else 'Ornithopter' if gift=='Trample' else 'Scaled Wurm' if gift=='Deathtouch' else None
    if blocker:add(blocker,'grant_fighter',1-actor)
    actions.extend([step('turn',player=actor,value=0),step('begin_attackers'),step('attack',ref=actor_ref,value=1),step('finish_attackers'),step('begin_blockers')]);entities[actor_ref]['tapped']=True
    blocked=blocker is not None and not(gift=='Flying' and active)
    if blocker:actions.append(step('block',ref='grant_fighter',other=actor_ref,value=int(blocked)))
    actions.extend([step('advance_combat'),step('combat_damage')])
    if not blocked:life[1-actor]-=power
    else:
     bt=characteristic('grant_fighter','toughness');bp=characteristic('grant_fighter','power');kill=power>=bt or gift=='Deathtouch' and active
     reciprocal=0 if gift=='First strike' and active and kill else bp
     if gift=='Trample' and active:life[1-actor]-=max(0,power-bt)
     if kill:move('grant_fighter','graveyard')
     else:entities['grant_fighter']['damage']=power
     entities[actor_ref]['damage']=reciprocal
     if reciprocal>=tough:move(actor_ref,'graveyard')
    if gift=='Lifelink' and active:life[actor]+=power
    actions.append(step('end_combat'))
   if entities[actor_ref]['alive']:actions.append(observe(actor_ref,'damage',entities[actor_ref]['damage']))
  if mode=='cleanup':
   actions+=stats();actions.extend([step('cleanup'),step('refresh')])
   for e in entities.values():e['bonus']=0;e['hexproof']=False;e['indestructible']=False;e['damage']=0
   if protection and entities['source']['alive']:actions.append(step('can_target_creature',ref='protection_probe',other='source',value=int('Hexproof' not in keywords('source'))))
   if temporary_indestructible and activations and entities['source']['alive']:actions.append(step('destroy',ref='source'));move('source','graveyard')
  checks=[step('assert_stack',value=1 if mode=='targeting_pending' else 0),step('assert_choices_consumed')]+stats()+receipts()
  steps=pre+actions+checks
  if mode in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
   at=0 if mode=='ready_snapshot' else cut if mode=='pending_snapshot' else len(actions)
   steps=pre+actions[:at]+[step('capture')]+copy.deepcopy(actions[at:]+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions[at:]+checks)+[step('assert_recorded_state')]
  if mode=='grant_removed':
   before=_extended_expand(name,csvid,d,p,['grant_ally_'+str(owner)],rows,o)[0]
   prior={x['ref']:x for x in before['setup']};current={x['ref']:x for x in setup}
   assert all(current.get(ref)==item for ref,item in prior.items())
   extras=[item for ref,item in current.items() if ref not in prior];assert all(item['zone']=='hand' for item in extras)
   proof=copy.deepcopy(before['steps'])
   for action in proof:
    if action['op']=='assert_zone' and action.get('zone')=='hand':action['value']+=sum(x['player']==action['player'] for x in extras)
    if action['op']=='assert_zone_multiplicity' and action.get('zone')=='hand':action['value']+=sum(x['player']==action['player'] and rows[x['name']]['csvid']==action['index'] for x in extras)
   proof.extend(step('assert_zone_multiplicity',player=pl,zone='hand',index=rows[n]['csvid'],value=sum(x['player']==pl and x['name']==n for x in extras)+next((s['value'] for s in before['steps'] if s['op']=='assert_zone_multiplicity' and s.get('zone')=='hand' and s.get('player')==pl and s.get('index')==rows[n]['csvid']),0)) for pl,n in sorted({(x['player'],x['name']) for x in extras}))
   steps=pre+[step('capture')]+proof+[step('restore')]+actions+checks
  if mode in ('monstrous_menace','monstrous_menace_pair'):
   base=[step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='menace_one',other='source',value=1),observe('menace_one','blocking_count',1),step('remove_from_combat',ref='source'),step('remove_from_combat',ref='menace_one')]
   steps=pre+[step('capture')]+base+[step('restore')]+actions+checks
  if mode=='monstrous_indestructible':steps=pre+[step('capture'),step('destroy',ref='source'),step('require_card',ref='source',property='in_play',value=0),step('restore')]+actions+checks
  if ground_mode:
   attacker,blocker=('rule_blocker','source') if mode=='monstrous_friendly_block' else ('source','unselected' if mode=='monstrous_fresh_block' else 'rule_blocker')
   steps=pre+[step('capture')]+block_trial(attacker,blocker,True)+[step('restore')]+actions+checks
  if akros_mode:steps=pre+[step('capture'),step('refresh'),step('begin_attackers'),step('attack',ref='source',value=0),step('restore')]+actions+checks
  if reach_mode:
   proof=[step('choose_target',player=owner,ref='source'),step('cast',ref='reach_proof_growth',value=1),step('resolve'),step('assert_choices_consumed'),observe('source','plus1_counters',1),step('assert_mana',player=owner,color=3,value=19),step('begin_attackers'),step('attack',ref='reach_attacker',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='source',other='reach_attacker',value=0),step('advance_combat'),step('combat_damage'),step('end_combat'),step('resolve'),step('assert_choices_consumed'),step('require_card',ref='source',property='in_play',value=0),step('assert_zone_multiplicity',player=owner,zone='graveyard',index=csvid,value=1),step('assert_life',player=owner,value=19)]
   steps=pre+[step('capture')]+proof+[step('restore')]+actions+checks
  for condition in p.get('inactive_conditions',[]):
   if condition=='no source combat':assert not any(s['op']=='attack' and s.get('ref')=='source' for s in steps)
   elif condition=='no upkeep steps':assert not any(s['op']=='advance_turn' for s in steps)
   elif condition=='source remains in hand until discarded':assert next(x for x in setup if x['ref']=='source')['zone']=='hand' and not any(s['op']=='cast' and s.get('ref')=='source' for s in steps)
   elif condition=='no qualifying spell cast after source entry':
    subjects={x['ref']:x['name'] for x in setup}
    assert next(x for x in setup if x['ref']=='source')['zone']=='hand' and sum(s['op']=='cast' and s.get('ref')=='source' for s in steps)<=1
    assert all(not {'Elemental','Warrior'}&set(o[subjects[s['ref']]]['type_line'].partition(' — ')[2].split()) for s in steps if s['op']=='cast' and s.get('ref')!='source')
   elif condition=='countered other creatures have no activated abilities':assert all(not any(':' in line for line in o[e['name']]['oracle_text'].splitlines()) for ref,e in entities.items() if ref!='source' and e['alive'] and e['counters'] and 'Creature' in o[e['name']]['type_line'])
   elif condition=='secondary damage mode is not selected':assert all(s['value']==0 for s in steps if s['op']=='choose_dialog' and s.get('player')==owner)
   elif condition=='no owned lands can supply mana':assert all(not(e['player']==owner and 'Land' in o[e['name']]['type_line']) for e in entities.values())
   else:raise ValueError('unreviewed inactive growth context')
  result.append(dict(name=f'GrowthCounterActivationExtensionRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_keywords=p['keywords'],untested_abilities=p['inactive'],untested_components=(['pending target-loss responses deferred until activation-time targeting is fixed'] if grave else [])+['source keyword combat','unexercised printed ability modes','control changes','multiplayer'])))
 return result



# Reviewed counter-transfer profiles: old growth bodies remain unchanged.
import copy
TRANSFER_PINS=PINS['transfers']
TRANSFER_HAND=('Giant Growth','Lightning Bolt','Plains')
TRANSFER_GRAVE=('Grizzly Bears','Forest')
TRANSFER_EXILE=('Island',)
TRANSFER_LIBRARY=('Forest','Plains','Lightning Bolt','Grizzly Bears')
def _transfer_step(op,**kw):return dict(op=op,**kw)
def _transfer_observe(ref,prop,value):return _transfer_step('assert_card',ref=ref,property=prop,value=value)
def _transfer_fixed_cost(name,pins=None):
 pins=TRANSFER_PINS if pins is None else pins
 result=[0]*6
 for token in re.findall(r'\{([^}]+)\}',pins['witnesses'][name]['mana_cost']):
  if token.isdigit():result[0]+=int(token)
  elif token in 'BUGRW' and len(token)==1:result['BUGRW'.index(token)+1]+=1
  else:raise ValueError('hybrid source must use its explicitly reviewed vector')
 return result
def _transfer_spec(d,o):
 pin=TRANSFER_PINS['profiles'].get(o.get('name')) if isinstance(o,dict) else None
 if not pin or o!=pin['oracle'] or o.get('layout')!='normal' or o.get('card_faces'):raise ValueError('whole normal Oracle source pin required')
 if set(d)!={'family','version','params','oracle_text','variants'} or d['family']!=FAMILY or type(d['version']) is not int or d['version']!=2:raise ValueError('explicit future family contract required')
 if d['oracle_text']!=o['oracle_text'] or json.dumps(d['params'],sort_keys=True)!=json.dumps(pin['params'],sort_keys=True):raise ValueError('every reviewed transfer parameter is pinned')
 return dict(copy.deepcopy(pin['params']),family=FAMILY,name=o['name'],modes=pin['modes'])
def _transfer_variants(p):return [m+'_'+str(owner) for owner in (0,1) for m in p['modes']]
def _transfer_validate_witnesses(o,rows):
 for n,pin in TRANSFER_PINS['witnesses'].items():
  if o.get(n)!=pin or any(rows.get(n,{}).get(k)!=v for k,v in TRANSFER_PINS['registry'][n].items()):raise ValueError('draft witness changed: '+n)

class _TransferWorld:
 def __init__(self,owner,pins=None):
  self.pins=TRANSFER_PINS if pins is None else pins
  self.owner=owner;self.setup=[];self.steps=[];self.entities={};self.mana=[[30,10,10,10,10,10] for _ in (0,1)]
  self.zones={z:[list(ns),list(ns)] for z,ns in [('hand',TRANSFER_HAND),('graveyard',TRANSFER_GRAVE),('exile',TRANSFER_EXILE),('library',TRANSFER_LIBRARY)]}
  for z,players in self.zones.items():
   for pl in (0,1):
    for i,n in enumerate(players[pl]):self.setup.append(dict(name=n,ref=f'p{pl}_{z}_{i}',player=pl,zone=z))
 def add(self,name,ref,pl,zone='battlefield',tapped=False):
  card=dict(name=name,ref=ref,player=pl,zone=zone)
  if zone=='battlefield':card['state']=['tapped'] if tapped else ['untapped']
  self.setup.append(card);self.entities[ref]=dict(name=name,player=pl,zone=zone,counters=0,tapped=tapped,sick=0)
  if zone!='battlefield':self.zones[zone][pl].append(name)
 def set_mana(self,pl,vector):
  self.mana[pl]=list(vector);self.steps.extend(_transfer_step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(vector))
 def pay(self,pl,vector):
  assert all(a>=b for a,b in zip(self.mana[pl],vector))
  self.mana[pl]=[a-b for a,b in zip(self.mana[pl],vector)]
 def mana_receipts(self):return [_transfer_step('assert_mana',player=pl,color=i,value=n) for pl in (0,1) for i,n in enumerate(self.mana[pl])]
 def move_zone(self,ref,zone):
  e=self.entities[ref];e.update(zone=zone,counters=0,sick=0,tapped=False);self.zones[zone][e['player']].append(e['name'])
 def spell(self,name,target,pl):
  ref='spell_'+str(sum(x['name']==name for x in self.setup));self.add(name,ref,pl,'hand')
  self.steps.extend([_transfer_step('choose_target',player=pl,ref=target),_transfer_step('cast',ref=ref,value=1),_transfer_step('assert_stack',value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')])
  self.pay(pl,_transfer_fixed_cost(name,self.pins));self.zones['hand'][pl].remove(name);self.zones['graveyard'][pl].append(name)
  self.entities[ref]['zone']='graveyard'
  if name=='Hunger of the Howlpack':self.entities[target]['counters']+=1;self.steps.append(_transfer_observe(target,'plus1_counters',self.entities[target]['counters']))
  elif name=='Unsummon':self.move_zone(target,'hand')
  else:raise ValueError('only reviewed draft witness spells')
  self.steps+=self.mana_receipts()
 def grow(self,ref,n):
  assert self.entities[ref]['zone']=='battlefield'
  for _ in range(n):self.spell('Hunger of the Howlpack',ref,1-self.owner)
 def cast_source(self,p,vector):
  self.steps.extend([_transfer_step('cast',ref='source',value=1),_transfer_step('assert_stack',value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')])
  self.pay(self.owner,vector);self.zones['hand'][self.owner].remove(p['name'])
  s=self.entities['source'];s.update(zone='battlefield',counters=p['entry'],sick=1,tapped=False)
  if p['entry_bonus']:
   for ref,e in self.entities.items():
    if ref!='source' and e['zone']=='battlefield' and e['player']==self.owner and e['counters'] and 'Creature' in self.pins['witnesses'][e['name']]['type_line']:e['counters']+=p['entry_bonus']
  self.steps+=self.stats()+self.mana_receipts()+[_transfer_observe('source','sick',1)]
 def cast_creature(self,name,pl):
  self.add(name,'new_creature',pl,'hand');self.steps.extend([_transfer_step('cast',ref='new_creature',value=1),_transfer_step('assert_stack',value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);self.pay(pl,_transfer_fixed_cost(name,self.pins));self.zones['hand'][pl].remove(name);self.entities['new_creature'].update(zone='battlefield',sick=1)
  s=self.entities['source'];r=self.pins['witnesses'][name];src=self.pins['witnesses'][s['name']]
  if pl==self.owner and (int(r['power'])>int(src['power'])+s['counters'] or int(r['toughness'])>int(src['toughness'])+s['counters']):s['counters']+=1
  self.steps+=self.stats()+self.mana_receipts()+[_transfer_observe('new_creature','sick',1)]
 def transfer(self,origin,destination):
  before=sum(e['counters'] for e in self.entities.values() if e['zone']=='battlefield')
  a,b=self.entities[origin],self.entities[destination]
  if origin!=destination and a['zone']==b['zone']=='battlefield' and a['counters']>0:a['counters']-=1;b['counters']+=1
  assert before==sum(e['counters'] for e in self.entities.values() if e['zone']=='battlefield')
 def stats(self):
  out=[]
  for ref,e in self.entities.items():
   if e['zone']=='battlefield':
    out.extend([_transfer_observe(ref,'in_play',1),_transfer_observe(ref,'tapped',int(e['tapped'])),_transfer_observe(ref,'plus1_counters',e['counters'])])
    r=self.pins['witnesses'][e['name']]
    if 'Creature' in r['type_line']:out.extend(_transfer_observe(ref,k,int(r[k])+e['counters']) for k in ('power','toughness'))
   elif e['zone']=='hand':out.append(_transfer_observe(ref,'in_hand',1))
  return out
 def receipts(self):
  out=self.stats()+self.mana_receipts();universe={x['name'] for x in self.setup}
  for pl in (0,1):
   out.extend([_transfer_step('assert_life',player=pl,value=20),_transfer_step('assert_zone',player=pl,zone='battlefield',value=sum(e['zone']=='battlefield' and e['player']==pl for e in self.entities.values()))])
   for n in sorted(universe):out.append(_transfer_step('assert_zone_multiplicity',player=pl,zone='battlefield',index=self.pins['registry'][n]['csvid'],value=sum(e['zone']=='battlefield' and e['player']==pl and e['name']==n for e in self.entities.values())))
   for z,players in self.zones.items():
    out.append(_transfer_step('assert_zone',player=pl,zone=z,value=len(players[pl])))
    out.extend(_transfer_step('assert_zone_multiplicity',player=pl,zone=z,index=self.pins['registry'][n]['csvid'],value=players[pl].count(n)) for n in sorted(universe))
    if z=='library':out.extend(_transfer_step('assert_zone_card',player=pl,zone=z,index=i,value=self.pins['registry'][n]['csvid']) for i,n in enumerate(players[pl]))
  return out

def _transfer_expand(name,csvid,d,p,selected,rows,o):
 _transfer_validate_witnesses(o,rows)
 if csvid!=TRANSFER_PINS['profiles'][name]['csvid'] or not selected or len(set(selected))!=len(selected) or any(v not in _transfer_variants(p) for v in selected):raise ValueError('explicit unique pinned draft selections required')
 out=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);w=_TransferWorld(owner);single=mode=='one_creature';flux=p['direction']=='source_to_target';rootkin=p['direction']=='target_to_source'
  source_cast=not flux or mode in ('fresh_source','cast_source');cast_only=mode in ('cast_source','cast_payment','cast_blue','cast_mixed')
  origin='source' if flux or single else 'origin';destination='source' if rootkin or single else 'recipient'
  if not single:
   donor_player=1-owner if mode in ('opponent_pair','opponent_origin','origin_enemy_probe') else owner
   dest_player=donor_player if p['relation']=='same_controller' else owner if mode=='opponent_origin' else 1-owner
   w.add('Grizzly Bears','origin',donor_player);w.add('Deadly Insect' if mode=='recipient_shroud_probe' else 'Gladecover Scout' if mode=='recipient_hexproof_probe' else 'Darksteel Relic' if mode=='recipient_noncreature_probe' else 'Hill Giant','recipient',dest_player);w.add('Ornithopter','own_control',owner);w.add('Scaled Wurm','enemy_control',1-owner)
   if flux and mode=='repeat':w.add("Kiora's Follower",'untapper',owner)
  w.add('Darksteel Relic','noncreature',owner)
  # Source is the last seeded battlefield creature. Thus Fluxmage cannot evolve
  # from setup entries. Root-Kin is actually cast after all preparatory counters.
  w.add(name,'source',owner,'hand' if source_cast else 'battlefield',tapped=mode=='tapped_source')
  w.steps=[_transfer_step('reset_death_history'),_transfer_step('turn',player=owner,value=0),_transfer_step('present_ai_dialogs',value=1)]
  actor_pool=[30,0,10 if flux else 0,10,0,10 if name=='Daghatar the Adamant' else 0]
  w.set_mana(owner,actor_pool);w.set_mana(1-owner,[30,10,10,10,10,10])
  zero=mode in ('zero_counter','no_counter','origin_zero_probe')
  if not single and mode!='zero_counter':
   w.grow('own_control',0 if mode=='entry_controls' else 1);w.grow('enemy_control',2 if mode=='entry_controls' else 1)
   if not flux:w.grow(origin,0 if zero else 2 if mode=='repeat' else 1)
  if flux and not source_cast and mode not in ('no_counter','evolve_qualifying'):w.grow('source',2 if mode=='repeat' else 1)
  cast_vector=p['cast_vectors'][1 if mode=='cast_blue' else 2 if mode=='cast_mixed' else 0]
  if source_cast:
   if cast_only:w.set_mana(owner,cast_vector)
   w.cast_source(p,cast_vector)
  if flux and mode=='fresh_source':w.grow('source',1)
  if single and name=='Simic Guildmage':w.grow('source',1)
  if mode=='evolve_qualifying':w.cast_creature('Scaled Wurm',owner)
  if mode=='cast_payment':pass
  if mode=='opponent_origin':assert p['relation']=='any'
  if p['graft']:
   assert not any(a['op']=='cast' and a.get('ref')=='new_creature' for a in w.steps), 'graft transfers are dormant, never silently omitted'
  activation_vector=p['activation_vectors'][1 if mode=='payment_black' else 2 if mode=='payment_mixed' else 0]
  if mode in ('payment','payment_black','payment_mixed'):w.set_mana(owner,activation_vector)
  if mode=='insufficient_mana':w.set_mana(owner,[0]*6)
  if mode=='wrong_color':
   excluded={i for v in p['activation_vectors'] for i,n in enumerate(v) if i and n};other=next(i for i in range(1,6) if i not in excluded);pool=[activation_vector[0],0,0,0,0,0];pool[other]=sum(activation_vector[1:]);w.set_mana(owner,pool)
  if mode in ('source_bounced_pending','origin_bounced_pending','recipient_bounced_pending'):w.add('Unsummon','response',1-owner,'hand')
  w.steps+=w.receipts();pending_cut=None;pending_bounce=None
  probe=mode in ('origin_zero_probe','self_target_probe','origin_enemy_probe','recipient_shroud_probe','recipient_hexproof_probe','recipient_noncreature_probe')
  if probe:
   assert rootkin or flux
   candidate='recipient' if flux else 'source' if mode=='self_target_probe' else origin
   w.steps.extend([_transfer_step('require_can_activate',ref='source',value=1),_transfer_step('probe_activation_target',ref='source',other=candidate,index=-1,value=int(mode in ('origin_zero_probe','self_target_probe')))])
  elif not cast_only:
   denied=mode in ('insufficient_mana','wrong_color','one_creature') or flux and mode in ('fresh_source','tapped_source')
   if denied:w.steps.extend([_transfer_step('require_can_activate',ref='source',value=0),_transfer_step('activate',ref='source',value=0)])
   else:
    assert p['relation']!='own_origin' or w.entities[origin]['player']==owner
    assert p['relation']!='same_controller' or (origin!=destination and w.entities[origin]['player']==w.entities[destination]['player'])
    assert p['direction']!='two_targets' or origin!=destination
    def activate():
     w.steps.append(_transfer_step('require_can_activate',ref='source',value=1))
     targets=[destination] if flux else [origin] if rootkin else [origin,destination]
     w.steps.extend(_transfer_step('choose_target',player=owner,ref=ref) for ref in targets)
     w.steps.append(_transfer_step('activate',ref='source',value=1));w.pay(owner,activation_vector)
     if p['tap']:w.entities['source']['tapped']=True
     # Moving a counter is a resolution instruction, never an activation cost.
     w.steps.extend([_transfer_step('assert_stack',value=1),_transfer_observe(origin,'plus1_counters',w.entities[origin]['counters']),_transfer_observe(destination,'plus1_counters',w.entities[destination]['counters'])]+w.mana_receipts())
    activate();pending_cut=len(w.steps)
    if mode in ('source_bounced_pending','origin_bounced_pending','recipient_bounced_pending'):
     changed='source' if mode=='source_bounced_pending' else origin if mode=='origin_bounced_pending' else destination
     # The opponent's real instant leaves hybrid activation choices unambiguous.
     ref='response';w.steps.extend([_transfer_step('choose_target',player=1-owner,ref=changed),_transfer_step('cast',ref=ref,value=1),_transfer_step('assert_stack',value=2)]);w.pay(1-owner,_transfer_fixed_cost('Unsummon'));w.zones['hand'][1-owner].remove('Unsummon');w.zones['graveyard'][1-owner].append('Unsummon');w.entities[ref]['zone']='graveyard';w.move_zone(changed,'hand')
     pending_bounce=changed
    if mode=='source_destroyed_pending':w.steps.append(_transfer_step('destroy',ref='source'));w.move_zone('source','graveyard')
    if mode!='activation_pending':
     w.steps.extend([_transfer_step('resolve'),_transfer_step('assert_choices_consumed')])
     if pending_bounce:
      e=w.entities[pending_bounce];assert w.zones['hand'][e['player']].count(e['name'])==1
      w.steps.append(_transfer_step('bind_hand',ref=pending_bounce,player=e['player'],value=TRANSFER_PINS['registry'][e['name']]['csvid']))
     w.transfer(origin,destination)
    if mode=='repeat':
     if flux:
      w.steps.extend([_transfer_step('choose_target',player=owner,ref='source'),_transfer_step('activate',ref='untapper',value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);w.entities['untapper']['tapped']=True;w.entities['source']['tapped']=False
     activate();w.steps.extend([_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);w.transfer(origin,destination)
  if mode=='cleanup':w.steps+=w.stats()+[_transfer_step('cleanup'),_transfer_step('refresh')]
  checks=[_transfer_step('assert_stack',value=1 if mode=='activation_pending' else 0),_transfer_step('assert_choices_consumed')]+w.receipts()
  if mode=='pending_snapshot':
   assert pending_cut is not None
   tail=w.steps[pending_cut:]+checks
   w.steps=w.steps[:pending_cut]+[_transfer_step('capture')]+copy.deepcopy(tail)+[_transfer_step('record_state'),_transfer_step('ai_roundtrip'),_transfer_step('restore')]+copy.deepcopy(tail)+[_transfer_step('assert_recorded_state')]
  else:w.steps+=checks
  out.append(dict(name=f'GrowthCounterTransferRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=w.steps,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],transfer_model='atomic resolution movement; printed costs',scope_gaps=['counter replacement/prevention','control changes','combat keywords','separate entry-trigger priority windows']+(['optional graft transfer; no other creature enters after source'] if p['graft'] else [])+p['unused_modes'])))
 return out


"""Unapplied resource-growth extension; shares the parameterized counter world."""
RESOURCE_PINS=PINS['resources']

def _resource_spec(d,o):
 pin=RESOURCE_PINS['profiles'].get(o.get('name')) if isinstance(o,dict) else None
 if not pin or o!=pin['oracle'] or o.get('layout')!='normal' or o.get('card_faces'):raise ValueError('whole normal resource-growth Oracle source pin required')
 if set(d)!={'family','version','params','oracle_text','variants'} or d['family']!=FAMILY or type(d['version']) is not int or d['version']!=2:raise ValueError('explicit resource-growth contract required')
 if d['oracle_text']!=o['oracle_text'] or json.dumps(d['params'],sort_keys=True)!=json.dumps(pin['params'],sort_keys=True):raise ValueError('reviewed resource-growth parameters changed')
 return dict(copy.deepcopy(pin['params']),family=FAMILY,name=o['name'],creature='Creature' in o['type_line'])

def _resource_validate_witnesses(o,rows):
 for n,pin in RESOURCE_PINS['witnesses'].items():
  if o.get(n)!=pin or any(rows.get(n,{}).get(k)!=v for k,v in RESOURCE_PINS['registry'][n].items()):raise ValueError('resource-growth witness changed: '+n)

def _resource_variants(p):
 modes=['effect','payment','cleanup','pending_snapshot','source_destroyed_pending','source_bounced_pending','repeat','cast_payment','tapped_source']
 if p['tap']:modes+=['fresh_source']
 if sum(p['cost']):modes+=['insufficient_mana']
 if any(p['cost'][1:]):modes+=['wrong_color']
 if p['kind']=='discard':modes+=['empty_hand','noncreature_hand' if p['creature_discard'] else 'land_discard']
 if p['scope']=='target':modes+=['own_target','target_bounced_pending','target_shroud_probe','target_hexproof_probe','target_noncreature_probe']
 if p['kind']=='battlefield_exile':modes+=['self_target','indestructible_target']
 if p['kind']=='grave_exile':modes+=['own_grave','land_exile','instant_exile','empty_graves','grave_target_lost']
 if p['tap']:modes+=['cast_untap_activation']
 if p.get('once_per_turn'):
  modes.remove('repeat');modes+=['once_used','stacked_denied','opposing_turn','insufficient_life','low_life','next_turn']
 return [m+'_'+str(owner) for owner in (0,1) for m in modes]

class _ResourceWorld(_TransferWorld):
 def __init__(self,owner):super().__init__(owner,RESOURCE_PINS);self.life=[20,20]
 def cast_source(self,p,vector):
  super().cast_source(p,vector)
  if 'Creature' not in self.pins['witnesses'][p['name']]['type_line']:
   assert self.steps[-1]==_transfer_observe('source','sick',1)
   self.steps.pop();self.entities['source']['sick']=0
 def receipts(self):
  return [dict(s,value=self.life[s['player']]) if s['op']=='assert_life' else s for s in super().receipts()]

def _resource_expand(name,csvid,d,p,selected,rows,o):
 _resource_validate_witnesses(o,rows)
 if csvid!=RESOURCE_PINS['profiles'][name]['csvid'] or not selected or len(set(selected))!=len(selected) or any(v not in _resource_variants(p) for v in selected):raise ValueError('unique reviewed resource-growth variants required')
 out=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);w=_ResourceWorld(owner);discard=p['kind']=='discard';exile=p['kind']=='battlefield_exile';grave=p['kind']=='grave_exile'
  # A singleton (or two identical cards for repeat) makes actual random discard
  # deterministic without substituting a chosen-discard cost.
  w.setup=[e for e in w.setup if not(e['zone']=='hand' and e['player']==owner)];w.zones['hand'][owner]=[]
  if grave and mode=='empty_graves':
   w.setup=[e for e in w.setup if e['zone']!='graveyard'];w.zones['graveyard']=[[],[]]
  if grave and mode in ('land_exile','instant_exile'):
   # The AI handler first offers only creatures, then retries any type when
   # none exist. Keep these real noncreature selections reachable in both UIs.
   w.setup=[e for e in w.setup if not(e['zone']=='graveyard' and 'Creature' in o[e['name']]['type_line'])]
   w.zones['graveyard']=[[n for n in ns if 'Creature' not in o[n]['type_line']] for ns in w.zones['graveyard']]
  if grave and mode=='instant_exile':
   w.add('Lightning Bolt','grave_instant',1-owner,'graveyard')
  if grave and mode=='repeat':w.add('Grizzly Bears','second_grave_creature',1-owner,'graveyard')
  target_player=owner if mode=='own_target' else 1-owner
  target_name='Deadly Insect' if mode=='target_shroud_probe' else 'Gladecover Scout' if mode=='target_hexproof_probe' else 'Darksteel Relic' if mode=='target_noncreature_probe' else 'Darksteel Myr' if mode=='indestructible_target' else 'Grizzly Bears'
  w.add(target_name,'recipient',target_player);w.add('Hill Giant','control',owner);w.add('Scaled Wurm','enemy_control',1-owner);w.add('Darksteel Relic','noncreature',owner)
  if p['tap'] and mode=='repeat':w.add("Kiora's Follower",'untapper',owner)
  casting=mode!='tapped_source' and (not p['tap'] or mode in ('fresh_source','cast_payment','cast_untap_activation'))
  w.add(name,'source',owner,'hand' if casting else 'battlefield',tapped=mode=='tapped_source')
  cost_names=[]
  if discard and mode!='empty_hand':
   cost_names=['Forest' if mode in ('land_discard','noncreature_hand') else 'Grizzly Bears']*(2 if mode=='repeat' else 1)
   for i,n in enumerate(cost_names):w.add(n,'cost_'+str(i),owner,'hand')
  w.steps=[_transfer_step('reset_death_history'),_transfer_step('turn',player=owner,value=0),_transfer_step('present_ai_dialogs',value=1)]
  w.set_mana(owner,[40,10,10,10,10,10]);w.set_mana(1-owner,[40,10,10,10,10,10])
  cast_cost=_transfer_fixed_cost(name,RESOURCE_PINS)
  if casting:
   if mode=='cast_payment':w.set_mana(owner,cast_cost)
   w.cast_source(dict(name=name,entry=0,entry_bonus=0),cast_cost)
  if mode=='cast_untap_activation':
   for pl in (0,1):w.set_mana(pl,[0]*6)
   w.steps.extend([_transfer_step('advance_turn'),_transfer_step('untap_step',player=1-owner),_transfer_step('advance_turn'),_transfer_step('untap_step',player=owner),_transfer_step('turn',player=owner,value=0)])
   w.entities['source']['sick']=0;w.steps.append(_transfer_observe('source','sick',0))
  if mode=='payment':w.set_mana(owner,p['cost'])
  if mode=='insufficient_mana':w.set_mana(owner,[0]*6)
  if mode=='wrong_color':w.set_mana(owner,[sum(p['cost']),0,0,0,0,0])
  if mode in ('source_bounced_pending','target_bounced_pending','grave_target_lost'):
   response='Reclaim' if mode=='grave_target_lost' else 'Boomerang' if mode=='source_bounced_pending' and not p['creature'] else 'Unsummon'
   w.add(response,'response',1-owner,'hand')
  if mode in ('insufficient_life','low_life'):
   w.life[owner]=p['pay_life']+(-1 if mode=='insufficient_life' else 1);w.steps.append(_transfer_step('set_life',player=owner,value=w.life[owner]))
  if mode=='opposing_turn' and p.get('own_turn_only'):w.steps.append(_transfer_step('turn',player=1-owner,value=0))
  w.steps+=w.receipts();cut=None
  probe=mode in ('target_shroud_probe','target_hexproof_probe','target_noncreature_probe')
  denied=mode in ('insufficient_mana','wrong_color','empty_hand','noncreature_hand','empty_graves','insufficient_life') or p['tap'] and mode in ('fresh_source','tapped_source') or mode=='opposing_turn' and p.get('own_turn_only')
  if mode=='cast_payment':pass
  elif probe:w.steps.extend([_transfer_step('require_can_activate',ref='source',value=1),_transfer_step('probe_activation_target',ref='source',other='recipient',index=-1,value=0)])
  elif denied:w.steps.extend([_transfer_step('require_can_activate',ref='source',value=0),_transfer_step('activate',ref='source',value=0)])
  else:
   recipient='source' if mode=='self_target' or p['scope']=='self' else 'recipient'
   def activate(iteration=0):
    w.steps.append(_transfer_step('require_can_activate',ref='source',value=1))
    if p['activation_menu'] or owner in p.get('activation_menu_players',[]):w.steps.append(_transfer_step('choose_dialog',player=owner,value=0))
    if p['scope']=='target':w.steps.append(_transfer_step('choose_target',player=owner,ref=recipient))
    if discard and not p['random_discard'] and (owner==0 or p['creature_discard']):w.steps.append(_transfer_step('choose_zone_card',player=owner,value=rows[cost_names[iteration]]['csvid']))
    if grave:
     pl=owner if mode=='own_grave' else 1-owner
     nm='Forest' if mode=='land_exile' else 'Lightning Bolt' if mode=='instant_exile' else 'Grizzly Bears'
     if all(w.zones['graveyard']):w.steps.append(_transfer_step('choose_player',player=owner,value=pl))
     w.steps.append(_transfer_step('choose_zone_card',player=owner,value=rows[nm]['csvid']))
    if p['kind']=='permanent':assert w.entities['source']['sick'] or w.entities['source']['tapped'], 'independent tap mode stays unavailable'
    w.steps.append(_transfer_step('activate',ref='source',value=1));w.pay(owner,p['cost']);w.life[owner]-=p.get('pay_life',0)
    if p['tap']:w.entities['source']['tapped']=True
    if discard:
     nm=cost_names[iteration];w.zones['hand'][owner].remove(nm);w.move_zone('cost_'+str(iteration),'graveyard')
     if iteration==0 and len(cost_names)==2:
      assert cost_names[0]==cost_names[1] and w.zones['hand'][owner].count(cost_names[1])==1
      w.steps.append(_transfer_step('bind_hand',ref='cost_1',player=owner,value=rows[cost_names[1]]['csvid']))
    w.steps.extend([_transfer_step('assert_stack',value=1),_transfer_step('assert_choices_consumed')]+w.receipts())
   def effect(target_legal=True):
    if not target_legal:return
    if grave:
     pl=owner if mode=='own_grave' else 1-owner;nm='Forest' if mode=='land_exile' else 'Lightning Bolt' if mode=='instant_exile' else 'Grizzly Bears'
     w.zones['graveyard'][pl].remove(nm);w.zones['exile'][pl].append(nm)
     if 'Creature' in o[nm]['type_line']:
      w.life[owner]+=1
      if w.entities['source']['zone']=='battlefield':w.entities['source']['counters']+=1
    elif exile:
     w.move_zone(recipient,'exile')
     if w.entities['source']['zone']=='battlefield':w.entities['source']['counters']+=1
    elif w.entities[recipient]['zone']=='battlefield':w.entities[recipient]['counters']+=p.get('amount',1)
   activate();cut=len(w.steps);bounced=None
   if mode=='stacked_denied':w.steps.extend([_transfer_step('require_can_activate',ref='source',value=0),_transfer_step('activate',ref='source',value=0)]);w.steps+=w.receipts()
   if mode in ('source_bounced_pending','target_bounced_pending'):
    bounced='source' if mode=='source_bounced_pending' else recipient
    response='Boomerang' if bounced=='source' and not p['creature'] else 'Unsummon'
    w.steps.extend([_transfer_step('choose_target',player=1-owner,ref=bounced),_transfer_step('cast',ref='response',value=1),_transfer_step('assert_stack',value=2)]);w.pay(1-owner,_transfer_fixed_cost(response,RESOURCE_PINS));w.zones['hand'][1-owner].remove(response);w.zones['graveyard'][1-owner].append(response);w.entities['response']['zone']='graveyard';w.move_zone(bounced,'hand')
   if mode=='source_destroyed_pending':w.steps.append(_transfer_step('destroy',ref='source'));w.move_zone('source','graveyard')
   if mode=='grave_target_lost':
    w.steps.extend([_transfer_step('choose_zone_card',player=1-owner,value=rows['Grizzly Bears']['csvid']),_transfer_step('cast',ref='response',value=1),_transfer_step('assert_stack',value=2)]);w.pay(1-owner,_transfer_fixed_cost('Reclaim',RESOURCE_PINS));w.zones['hand'][1-owner].remove('Reclaim');w.zones['graveyard'][1-owner].append('Reclaim');w.entities['response']['zone']='graveyard';w.zones['graveyard'][1-owner].remove('Grizzly Bears');w.zones['library'][1-owner].insert(0,'Grizzly Bears')
   if p['resolution_menu'] and w.entities[recipient]['zone']=='battlefield':w.steps.append(_transfer_step('choose_dialog',player=owner,value=0))
   w.steps.extend([_transfer_step('resolve'),_transfer_step('assert_choices_consumed')])
   if bounced:w.steps.append(_transfer_step('bind_hand',ref=bounced,player=w.entities[bounced]['player'],value=rows[w.entities[bounced]['name']]['csvid']))
   effect(mode!='grave_target_lost' and (p['scope']!='target' or w.entities[recipient]['zone']=='battlefield'))
   if mode=='once_used':w.steps.extend([_transfer_step('require_can_activate',ref='source',value=0),_transfer_step('activate',ref='source',value=0)]);w.steps+=w.receipts()
   if mode=='next_turn':
    w.steps+=w.receipts()+[_transfer_step('cleanup'),_transfer_step('refresh'),_transfer_step('turn',player=owner,value=0)]
    for pl in (0,1):w.set_mana(pl,[0]*6)
    w.steps.extend([_transfer_step('advance_turn'),_transfer_step('untap_step',player=1-owner),_transfer_step('turn',player=1-owner,value=0),_transfer_step('require_can_activate',ref='source',value=0),_transfer_step('activate',ref='source',value=0)])
    w.steps+=w.receipts()+[_transfer_step('advance_turn'),_transfer_step('untap_step',player=owner),_transfer_step('turn',player=owner,value=0),_transfer_observe('source','sick',0)];w.entities['source']['sick']=0
    activate();w.steps.extend([_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);effect()
   if mode=='repeat':
    w.steps+=w.receipts()
    if p['tap']:
     w.steps.extend([_transfer_step('choose_target',player=owner,ref='source'),_transfer_step('activate',ref='untapper',value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);w.entities['source']['tapped']=False;w.entities['untapper']['tapped']=True
    if exile:
     recipient='enemy_control'
    activate(1)
    if p['resolution_menu']:w.steps.append(_transfer_step('choose_dialog',player=owner,value=0))
    w.steps.extend([_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);effect()
  if mode=='cleanup':w.steps+=w.receipts()+[_transfer_step('cleanup'),_transfer_step('refresh')]
  if p['kind'] in ('permanent','life') and w.entities['source']['zone']=='hand':w.steps.append(_transfer_observe('source','plus1_counters',0))
  checks=[_transfer_step('assert_stack',value=0),_transfer_step('assert_choices_consumed')]+w.receipts()
  if mode=='pending_snapshot':
   assert cut is not None
   tail=w.steps[cut:]+checks;w.steps=w.steps[:cut]+[_transfer_step('capture')]+copy.deepcopy(tail)+[_transfer_step('record_state'),_transfer_step('ai_roundtrip'),_transfer_step('restore')]+copy.deepcopy(tail)+[_transfer_step('assert_recorded_state')]
  else:w.steps+=checks
  for clause in p['conditions']:
   if clause in ('no combat or upkeep step','no damage or regeneration window'):assert not any(s['op'] in ('begin_attackers','combat_damage','upkeep') for s in w.steps)
   elif clause=='no surviving source with an activated-ability exile':assert w.entities['source']['zone']!='battlefield' or all(':' not in o[e['name']]['oracle_text'] for e in w.entities.values() if e['zone']=='exile' and 'Creature' in o[e['name']]['type_line'])
   elif clause=='counter damage mode is unselected':assert all(s['value']==0 for s in w.steps if s['op']=='choose_dialog' and s['player']==owner)
   elif clause=='no life gain this turn':assert all(s['op'] not in ('begin_attackers','combat_damage') for s in w.steps) and max(w.life)<=20
   else:raise ValueError('unreviewed resource condition')
  out.append(dict(name=f'GrowthResourceActivationRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=w.steps,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],scope_gaps=p['unused']+['replacement effects','control changes','multiplayer'])))
 return out



"""Unapplied graft counter-qualified grants with functional recipient actions."""
GRAFT_PINS=PINS['graft_grants']

def _graft_spec(d,o):
 pin=GRAFT_PINS['profiles'].get(o.get('name')) if isinstance(o,dict) else None
 if not pin or o!=pin['oracle'] or o.get('layout')!='normal' or o.get('card_faces'):raise ValueError('whole normal graft source Oracle pin required')
 if set(d)!={'family','version','params','oracle_text','variants'} or d['family']!=FAMILY or type(d['version']) is not int or d['version']!=2:raise ValueError('explicit graft activation contract required')
 if d['oracle_text']!=o['oracle_text'] or json.dumps(d['params'],sort_keys=True)!=json.dumps(pin['params'],sort_keys=True):raise ValueError('reviewed graft activation parameters changed')
 return dict(copy.deepcopy(pin['params']),family=FAMILY,name=o['name'])

def _graft_validate_witnesses(o,rows):
 for n,pin in GRAFT_PINS['witnesses'].items():
  if o.get(n)!=pin or any(rows.get(n,{}).get(k)!=v for k,v in GRAFT_PINS['registry'][n].items()):raise ValueError('graft activation witness changed: '+n)

def _graft_variants(p):
 modes=['effect','payment','insufficient_mana','cleanup','pending_snapshot','source_bounced_pending','source_destroyed_pending','target_bounced_pending','counter_removed_pending','cast_payment','tapped_source','opponent_target','zero_counter_probe','shroud_probe']
 if any(p['cost'][1:]):modes+=['wrong_color']
 if p['effect']=='Untap':modes+=['already_untapped']
 return [m+'_'+str(owner) for owner in (0,1) for m in modes]

class _GraftWorld(_TransferWorld):
 def __init__(self,owner):super().__init__(owner,GRAFT_PINS);self.life=[20,20]
 def stats(self):
  return super().stats()+[_transfer_observe(ref,'damage',e.get('damage',0)) for ref,e in self.entities.items() if e['zone']=='battlefield' and 'Creature' in self.pins['witnesses'][e['name']]['type_line']]
 def receipts(self):return [dict(s,value=self.life[s['player']]) if s['op']=='assert_life' else s for s in super().receipts()]

def _graft_function(w,p,target,active):
 """Actual combat/target/untap behavior; update only observed consequences."""
 e=w.entities[target];actor=e['player'];effect=p['effect']
 if e['zone']!='battlefield':return
 if effect=='Untap':
  w.steps.append(_transfer_observe(target,'tapped',0 if active else int(e['tapped'])))
  if active:e['tapped']=False
  return
 if effect=='Shroud':
  for ref in ('probe_own','probe_enemy'):w.steps.append(_transfer_step('can_target_creature',ref=ref,other=target,value=int(not active)))
  chosen='control' if active else target
  w.steps.extend([_transfer_step('can_target_creature',ref='probe_enemy',other='control',value=1),_transfer_step('choose_target',player=1-w.owner,ref=chosen),_transfer_step('cast',ref='probe_enemy',value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')])
  w.pay(1-w.owner,_transfer_fixed_cost('Unsummon',GRAFT_PINS));w.zones['hand'][1-w.owner].remove('Unsummon');w.zones['graveyard'][1-w.owner].append('Unsummon');w.entities['probe_enemy']['zone']='graveyard';w.move_zone(chosen,'hand')
  w.steps.append(_transfer_step('bind_hand',ref=chosen,player=w.entities[chosen]['player'],value=GRAFT_PINS['registry'][w.entities[chosen]['name']]['csvid']))
  return
 attacker='fighter' if effect=='Reach' else target;blocker=target if effect=='Reach' else 'fighter';attacking_player=w.entities[attacker]['player']
 w.steps.extend([_transfer_step('turn',player=attacking_player,value=0),_transfer_step('begin_attackers'),_transfer_step('attack',ref=attacker,value=1),_transfer_step('finish_attackers'),_transfer_step('begin_blockers')]);w.entities[attacker]['tapped']=True
 blocked=active if effect=='Reach' else not active if effect=='Flying' else True
 w.steps.extend([_transfer_step('block',ref=blocker,other=attacker,value=int(blocked)),_transfer_step('advance_combat'),_transfer_step('combat_damage'),_transfer_step('end_combat')])
 power=lambda ref:int(GRAFT_PINS['witnesses'][w.entities[ref]['name']]['power'])+w.entities[ref]['counters']
 tough=lambda ref:int(GRAFT_PINS['witnesses'][w.entities[ref]['name']]['toughness'])+w.entities[ref]['counters']
 if not blocked:w.life[1-attacking_player]-=power(attacker)
 else:
  ap,bp,at,bt=power(attacker),power(blocker),tough(attacker),tough(blocker)
  if effect=='Trample' and active:w.life[1-attacking_player]-=max(0,ap-bt)
  w.entities[attacker]['damage']=bp;w.entities[blocker]['damage']=ap
  if bp>=at:w.move_zone(attacker,'graveyard')
  if ap>=bt:w.move_zone(blocker,'graveyard')
 w.steps+=w.receipts()

def _graft_expand(name,csvid,d,p,selected,rows,o):
 _graft_validate_witnesses(o,rows)
 if csvid!=GRAFT_PINS['profiles'][name]['csvid'] or not selected or len(set(selected))!=len(selected) or any(v not in _graft_variants(p) for v in selected):raise ValueError('unique pinned graft activation selection required')
 out=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);w=_GraftWorld(owner);actor=1-owner if mode=='opponent_target' else owner;untap=p['effect']=='Untap';probe=mode in ('zero_counter_probe','shroud_probe')
  n='Deadly Insect' if mode=='shroud_probe' else 'Hill Giant'
  w.add(n,'recipient',actor,tapped=untap and mode!='already_untapped');w.add('Grizzly Bears','control',actor);w.add('Scaled Wurm','other_control',1-actor)
  fighter='Suntail Hawk' if p['effect']=='Reach' else 'Ornithopter' if p['effect']=='Trample' else 'Grizzly Bears'
  w.add(fighter,'fighter',1-actor);w.add("Kiora's Follower",'untapper',owner);w.add('Vampire Hexmage','hexmage',1-owner);w.add('Icy Manipulator','tapper',1-owner)
  # Every other creature is already present before the graft source enters.
  w.add(name,'source',owner,'hand')
  w.steps=[_transfer_step('reset_death_history'),_transfer_step('turn',player=owner,value=0),_transfer_step('present_ai_dialogs',value=1)]
  w.set_mana(owner,[40]*6);w.set_mana(1-owner,[40]*6)
  if mode not in ('zero_counter_probe','shroud_probe'):w.grow('recipient',1)
  if mode=='shroud_probe':
   # A real nontargeted spell gives the shrouded recipient a counter, so this
   # proves protection independently of the printed counter requirement.
   w.add("Titania's Boon",'counter_seed',owner,'hand');w.steps.extend([_transfer_step('cast',ref='counter_seed',value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);w.pay(owner,_transfer_fixed_cost("Titania's Boon",GRAFT_PINS));w.zones['hand'][owner].remove("Titania's Boon");w.zones['graveyard'][owner].append("Titania's Boon");w.entities['counter_seed']['zone']='graveyard'
   for e in w.entities.values():
    if e['zone']=='battlefield' and e['player']==owner and 'Creature' in o[e['name']]['type_line']:e['counters']+=1
  cast_cost=_transfer_fixed_cost(name,GRAFT_PINS)
  if mode=='cast_payment':w.set_mana(owner,cast_cost)
  w.cast_source(dict(name=name,entry=p['entry'],entry_bonus=0),cast_cost)
  if mode=='tapped_source':
   w.steps.extend([_transfer_step('choose_target',player=1-owner,ref='source'),_transfer_step('activate',ref='tapper',value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);w.pay(1-owner,[1,0,0,0,0,0]);w.entities['tapper']['tapped']=True;w.entities['source']['tapped']=True
  if p['effect']=='Shroud':w.add('Unsummon','probe_own',owner,'hand');w.add('Unsummon','probe_enemy',1-owner,'hand')
  if mode in ('source_bounced_pending','target_bounced_pending'):w.add('Unsummon','response',1-owner,'hand')
  if mode=='payment':w.set_mana(owner,p['cost'])
  if mode=='insufficient_mana':w.set_mana(owner,[0]*6)
  if mode=='wrong_color':w.set_mana(owner,[sum(p['cost']),0,0,0,0,0])
  w.steps+=w.receipts();cut=None;active=False;bounced=None
  if mode=='cast_payment':pass
  elif probe:w.steps.extend([_transfer_step('require_can_activate',ref='source',value=1),_transfer_step('probe_activation_target',ref='source',other='recipient',index=-1,value=0)])
  elif mode in ('insufficient_mana','wrong_color'):w.steps.extend([_transfer_step('require_can_activate',ref='source',value=0),_transfer_step('activate',ref='source',value=0)])
  else:
   w.steps.extend([_transfer_step('require_can_activate',ref='source',value=1),_transfer_step('choose_target',player=owner,ref='recipient'),_transfer_step('activate',ref='source',value=1),_transfer_step('assert_stack',value=1),_transfer_step('assert_choices_consumed')]);w.pay(owner,p['cost']);w.steps+=w.receipts();cut=len(w.steps)
   if mode in ('source_bounced_pending','target_bounced_pending'):
    bounced='source' if mode=='source_bounced_pending' else 'recipient';w.steps.extend([_transfer_step('choose_target',player=1-owner,ref=bounced),_transfer_step('cast',ref='response',value=1),_transfer_step('assert_stack',value=2)]);w.pay(1-owner,_transfer_fixed_cost('Unsummon',GRAFT_PINS));w.zones['hand'][1-owner].remove('Unsummon');w.zones['graveyard'][1-owner].append('Unsummon');w.entities['response']['zone']='graveyard';w.move_zone(bounced,'hand')
   if mode=='source_destroyed_pending':w.steps.append(_transfer_step('destroy',ref='source'));w.move_zone('source','graveyard')
   if mode=='counter_removed_pending':
    w.steps.extend([_transfer_step('choose_target',player=1-owner,ref='recipient'),_transfer_step('activate',ref='hexmage',value=1),_transfer_step('assert_stack',value=2),_transfer_observe('recipient','plus1_counters',1)]);w.move_zone('hexmage','graveyard');w.entities['recipient']['counters']=0
   w.steps.extend([_transfer_step('resolve'),_transfer_step('assert_choices_consumed')])
   if bounced:w.steps.append(_transfer_step('bind_hand',ref=bounced,player=w.entities[bounced]['player'],value=rows[w.entities[bounced]['name']]['csvid']))
   active=w.entities['recipient']['zone']=='battlefield' and w.entities['recipient']['counters']>0
   if untap and active:w.entities['recipient']['tapped']=False
   if mode=='cleanup':w.steps+=w.receipts()+[_transfer_step('cleanup'),_transfer_step('refresh')];active=untap and active
   _graft_function(w,p,'recipient',active)
  checks=[_transfer_step('assert_stack',value=0),_transfer_step('assert_choices_consumed')]+w.receipts()
  if mode=='pending_snapshot':
   assert cut is not None
   tail=w.steps[cut:]+checks;steps=w.steps[:cut]+[_transfer_step('capture')]+copy.deepcopy(tail)+[_transfer_step('record_state'),_transfer_step('ai_roundtrip'),_transfer_step('restore')]+copy.deepcopy(tail)+[_transfer_step('assert_recorded_state')]
  else:steps=w.steps+checks
  if mode=='cleanup' and not untap:
   before=_graft_expand(name,csvid,d,p,['effect_'+str(owner)],rows,o)[0]
   assert before['setup']==w.setup, 'functional positive and cleanup absence use the same board'
   steps=[_transfer_step('capture')]+copy.deepcopy(before['steps'])+[_transfer_step('restore')]+steps
  assert all('Creature' not in o[w.entities[s['ref']]['name']]['type_line'] for s in steps if s['op']=='cast' and s.get('ref')!='source'), 'no creatures enter after the graft source'
  out.append(dict(name=f'GraftCounterActivationRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],scope_gaps=p['inactive']+['source-self keyword combat','control changes','counter replacement','multiplayer'])))
 return out


"""Unapplied counter-cost draw and entry-mode model; no engine substitutions."""
DRAW_PINS=PINS['counter_draw']

def _counter_draw_spec(d,o):
 pin=DRAW_PINS['profiles'].get(o.get('name')) if isinstance(o,dict) else None
 if not pin or o!=pin['oracle'] or o.get('layout')!='normal' or o.get('card_faces'):raise ValueError('whole normal counter-draw Oracle source pin required')
 if set(d)!={'family','version','params','oracle_text','variants'} or d['family']!=FAMILY or type(d['version']) is not int or d['version']!=2:raise ValueError('explicit counter-draw contract required')
 if d['oracle_text']!=o['oracle_text'] or json.dumps(d['params'],sort_keys=True)!=json.dumps(pin['params'],sort_keys=True):raise ValueError('reviewed counter-draw parameters changed')
 return dict(copy.deepcopy(pin['params']),family=FAMILY,name=o['name'])

def _counter_draw_validate_witnesses(o,rows):
 for n,pin in DRAW_PINS['witnesses'].items():
  if o.get(n)!=pin or any(rows.get(n,{}).get(k)!=v for k,v in DRAW_PINS['registry'][n].items()):raise ValueError('counter-draw witness changed: '+n)

def _counter_draw_variants(p):
 modes=['effect','payment','insufficient_mana','cast_payment','cleanup','pending_snapshot','source_destroyed_pending','repeat','tapped_source','opposing_turn']
 if any(p['cost'][1:]):modes+=['wrong_color']
 if not p['discard']:modes+=['source_bounced_pending','payer_bounced_pending','payer_destroyed_pending']
 if p['entry']=='spell_graves':modes+=['entry_one','entry_zero','entry_many','own_spell_grave','opponent_spell_grave']
 if p['entry']=='graft4':modes+=['one_payer','protected_payer']
 if p['entry']=='other_wizards':modes+=['empty_counters','enemy_counter_only','protected_payer','wizard_entry','nonwizard_entry','enemy_wizard_entry']
 if p['entry']=='optional_mode':modes+=['entry_mode','entry_mode_empty','entry_mode_cleanup','entry_mode_source_loss','entry_mode_twice','entry_mode_enemy']
 return [m+'_'+str(owner) for owner in (0,1) for m in modes]

class _CounterDrawWorld(_TransferWorld):
 def __init__(self,owner):super().__init__(owner,DRAW_PINS)
 def stats(self):
  out=super().stats()
  if 'source' in self.entities and self.entities['source']['zone']=='hand':out.append(_transfer_observe('source','plus1_counters',0))
  return out
 def source_cast(self,p):
  cost=_transfer_fixed_cost(p['name'],DRAW_PINS)
  self.steps.extend([_transfer_step('cast',ref='source',value=1),_transfer_step('assert_stack',value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);self.pay(self.owner,cost);self.zones['hand'][self.owner].remove(p['name'])
  counters=4 if p['entry']=='graft4' else sum('Instant' in self.pins['witnesses'][n]['type_line'] or 'Sorcery' in self.pins['witnesses'][n]['type_line'] for ns in self.zones['graveyard'] for n in ns) if p['entry']=='spell_graves' else 0
  e=self.entities['source'];e.update(zone='battlefield',sick=1,counters=counters)
  if int(self.pins['witnesses'][p['name']]['toughness'])+counters<=0:self.move_zone('source','graveyard')
  self.steps+=self.receipts()
  if e['zone']=='battlefield':self.steps.append(_transfer_observe('source','sick',1))
 def remove_cost(self,ref,n=1):
  e=self.entities[ref];assert e['zone']=='battlefield' and e['player']==self.owner and e['counters']>=n;e['counters']-=n
  if int(self.pins['witnesses'][e['name']]['toughness'])+e['counters']<=0:self.move_zone(ref,'graveyard')
 def draw(self,p):
  assert self.zones['library'][self.owner]
  n=self.zones['library'][self.owner].pop(0);self.zones['hand'][self.owner].append(n)
  if p['discard']:
   assert self.zones['hand'][self.owner]==[n], 'actual AI looting discard remains unambiguous'
   self.zones['hand'][self.owner].remove(n);self.zones['graveyard'][self.owner].append(n)
 def enter_creature(self,ref,n,pl,bonus):
  self.steps.extend([_transfer_step('cast',ref=ref,value=1),_transfer_step('assert_stack',value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);self.pay(pl,_transfer_fixed_cost(n,DRAW_PINS));self.zones['hand'][pl].remove(n);self.entities[ref].update(zone='battlefield',sick=1,counters=bonus);self.steps+=self.receipts()+[_transfer_observe(ref,'sick',1)]

def _counter_draw_expand(name,csvid,d,p,selected,rows,o):
 _counter_draw_validate_witnesses(o,rows)
 if csvid!=DRAW_PINS['profiles'][name]['csvid'] or not selected or len(set(selected))!=len(selected) or any(v not in _counter_draw_variants(p) for v in selected):raise ValueError('unique pinned counter-draw selection required')
 out=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);w=_CounterDrawWorld(owner);global_mode=mode.startswith('entry_mode');protected=mode=='protected_payer';empty=mode in ('empty_counters','enemy_counter_only','entry_mode_empty');academy=p['entry']=='spell_graves';novijen=p['entry']=='graft4'
  if academy:
   w.setup=[x for x in w.setup if not(x['zone']=='hand' and x['player']==owner)];w.zones['hand'][owner]=[]
   spell_seeds=[] if mode=='entry_zero' else [('Lightning Bolt',owner)] if mode=='entry_one' else [('Lightning Bolt',owner),('Reclaim',owner)] if mode=='own_spell_grave' else [('Divination',1-owner),('Giant Growth',1-owner)] if mode=='opponent_spell_grave' else [('Lightning Bolt',owner),('Divination',1-owner)]
   if mode=='entry_many':spell_seeds += [('Reclaim',owner),('Giant Growth',1-owner)]
   for i,(n,pl) in enumerate(spell_seeds):w.add(n,'spell_grave_'+str(i),pl,'graveyard')
  payer_owner=1-owner if mode=='enemy_counter_only' else owner
  w.add('Deadly Insect' if protected else 'Grizzly Bears','payer',payer_owner);w.add('Hill Giant','control',1-owner);w.add('Darksteel Relic','noncreature',owner);w.add('Icy Manipulator','tapper',1-owner)
  w.add(name,'source',owner,'hand')
  entry_case=mode in ('wizard_entry','nonwizard_entry','enemy_wizard_entry') or global_mode
  if entry_case:w.add('Suntail Hawk' if mode=='nonwizard_entry' or global_mode else 'Hapless Researcher','entrant',1-owner if mode=='enemy_wizard_entry' else owner,'hand')
  if global_mode:w.add('Hapless Researcher','enemy_entrant',1-owner,'hand')
  if mode in ('source_bounced_pending','payer_bounced_pending'):w.add('Unsummon','response',1-owner,'hand')
  w.steps=[_transfer_step('reset_death_history'),_transfer_step('turn',player=owner,value=0),_transfer_step('present_ai_dialogs',value=1)];w.set_mana(owner,[40]*6);w.set_mana(1-owner,[40]*6)
  if not academy and not empty and not protected:w.grow('payer',2 if mode=='repeat' or novijen and mode=='one_payer' else 1)
  if mode=='enemy_counter_only':w.grow('payer',1)
  if protected:
   for i in range(2 if novijen else 1):
    ref='counter_seed_'+str(i);w.add("Titania's Boon",ref,owner,'hand');w.steps.extend([_transfer_step('cast',ref=ref,value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);w.pay(owner,_transfer_fixed_cost("Titania's Boon",DRAW_PINS));w.zones['hand'][owner].remove("Titania's Boon");w.zones['graveyard'][owner].append("Titania's Boon");w.entities[ref]['zone']='graveyard';w.entities['payer']['counters']+=1
  if mode=='cast_payment':w.set_mana(owner,_transfer_fixed_cost(name,DRAW_PINS))
  w.source_cast(p)
  if mode=='tapped_source':
   w.steps.extend([_transfer_step('choose_target',player=1-owner,ref='source'),_transfer_step('activate',ref='tapper',value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);w.pay(1-owner,[1,0,0,0,0,0]);w.entities['tapper']['tapped']=True;w.entities['source']['tapped']=True
  if mode in ('wizard_entry','nonwizard_entry','enemy_wizard_entry'):
   pl=1-owner if mode=='enemy_wizard_entry' else owner;w.steps.append(_transfer_step('turn',player=pl,value=0));n=w.entities['entrant']['name'];w.enter_creature('entrant',n,pl,int(pl==owner and 'Wizard' in o[n]['type_line']));w.steps.append(_transfer_step('turn',player=owner,value=0))
  if mode in ('opposing_turn','entry_mode_enemy'):w.steps.append(_transfer_step('turn',player=1-owner,value=0))
  if mode=='payment':w.set_mana(owner,p['cost'])
  if mode=='insufficient_mana':w.set_mana(owner,[0]*6)
  if mode=='wrong_color':w.set_mana(owner,[sum(p['cost']),0,0,0,0,0])
  w.steps+=w.receipts();cut=None
  if mode in ('cast_payment','entry_zero'):pass
  elif mode in ('insufficient_mana','wrong_color','empty_counters','enemy_counter_only'):w.steps.extend([_transfer_step('require_can_activate',ref='source',value=0),_transfer_step('activate',ref='source',value=0)])
  else:
   def activate(iteration=0):
    w.steps.append(_transfer_step('require_can_activate',ref='source',value=1))
    if p['menu'] is not None and mode!='entry_mode_empty':w.steps.append(_transfer_step('choose_dialog',player=owner,value=0 if global_mode else p['menu']))
    refs=[]
    if not global_mode:
     refs=['source']*p['remove'] if academy else ['payer','source'] if novijen and mode not in ('one_payer','protected_payer') and iteration==0 else ['payer']*p['remove']
     if novijen and iteration:refs=['source']*2
     if mode=='wizard_entry':refs=['entrant']
     if p['scope']!='self':w.steps.extend(_transfer_step('choose_target',player=owner,ref=r) for r in refs)
    w.steps.append(_transfer_step('activate',ref='source',value=1));w.pay(owner,p['cost'])
    for ref in refs:w.remove_cost(ref)
    w.steps.extend([_transfer_step('assert_stack',value=1),_transfer_step('assert_choices_consumed')]+w.receipts())
   def resolve():
    if p['discard'] and owner==0:w.steps.append(_transfer_step('choose_zone_card',player=owner,value=rows[w.zones['library'][owner][0]]['csvid']))
    w.steps.extend([_transfer_step('resolve'),_transfer_step('assert_choices_consumed')])
    if not global_mode:w.draw(p)
   activate();cut=len(w.steps)
   if mode in ('source_destroyed_pending','payer_destroyed_pending','entry_mode_source_loss'):
    ref='payer' if mode=='payer_destroyed_pending' else 'source';w.steps.append(_transfer_step('destroy',ref=ref));w.move_zone(ref,'graveyard')
   if mode in ('source_bounced_pending','payer_bounced_pending'):
    ref='source' if mode=='source_bounced_pending' else 'payer';w.steps.extend([_transfer_step('choose_target',player=1-owner,ref=ref),_transfer_step('cast',ref='response',value=1),_transfer_step('assert_stack',value=2)]);w.pay(1-owner,_transfer_fixed_cost('Unsummon',DRAW_PINS));w.zones['hand'][1-owner].remove('Unsummon');w.zones['graveyard'][1-owner].append('Unsummon');w.entities['response']['zone']='graveyard';w.move_zone(ref,'hand')
   resolve()
   if mode in ('source_bounced_pending','payer_bounced_pending'):w.steps.append(_transfer_step('bind_hand',ref=ref,player=w.entities[ref]['player'],value=rows[w.entities[ref]['name']]['csvid']))
   if mode=='repeat':w.steps+=w.receipts();activate(1);resolve()
   if global_mode:
    if mode=='entry_mode_twice':activate(1);resolve()
    if mode=='entry_mode_cleanup':w.steps+=w.receipts()+[_transfer_step('cleanup'),_transfer_step('refresh'),_transfer_step('turn',player=owner,value=0)]
    ref='enemy_entrant' if mode=='entry_mode_enemy' else 'entrant';pl=1-owner if mode=='entry_mode_enemy' else owner
    w.enter_creature(ref,w.entities[ref]['name'],pl,0 if mode in ('entry_mode_cleanup','entry_mode_enemy') else 2 if mode=='entry_mode_twice' else 1)
  if mode=='cleanup':w.steps+=w.receipts()+[_transfer_step('cleanup'),_transfer_step('refresh')]
  checks=[_transfer_step('assert_stack',value=0),_transfer_step('assert_choices_consumed')]+w.receipts()
  if mode=='pending_snapshot':
   assert cut is not None
   tail=w.steps[cut:]+checks;steps=w.steps[:cut]+[_transfer_step('capture')]+copy.deepcopy(tail)+[_transfer_step('record_state'),_transfer_step('ai_roundtrip'),_transfer_step('restore')]+copy.deepcopy(tail)+[_transfer_step('assert_recorded_state')]
  else:steps=w.steps+checks
  if mode in ('entry_mode_cleanup','entry_mode_enemy'):
   proof=_counter_draw_expand(name,csvid,d,p,['entry_mode_'+str(owner)],rows,o)[0];assert proof['setup']==w.setup
   steps=[_transfer_step('capture')]+copy.deepcopy(proof['steps'])+[_transfer_step('restore')]+steps
  if novijen:assert all('Creature' not in o[w.entities[s['ref']]['name']]['type_line'] for s in steps if s['op']=='cast' and s.get('ref')!='source')
  out.append(dict(name=f'CounterCostDrawActivationRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],scope_gaps=p['gaps']+['cost recipient selection under control changes','counter replacement','multiplayer'])))
 return out


"""Final bounded bolster/support/team activation model, using real engine actions."""
SELECTION_PINS=PINS['selection_growth']

def _selection_spec(d,o):
 pin=SELECTION_PINS['profiles'].get(o.get('name')) if isinstance(o,dict) else None
 if not pin or o!=pin['oracle'] or o.get('layout')!='normal' or o.get('card_faces'):raise ValueError('whole normal selection-growth Oracle pin required')
 if set(d)!={'family','version','params','oracle_text','variants'} or d['family']!=FAMILY or type(d['version']) is not int or d['version']!=2:raise ValueError('explicit selection-growth contract required')
 if d['oracle_text']!=o['oracle_text'] or json.dumps(d['params'],sort_keys=True)!=json.dumps(pin['params'],sort_keys=True):raise ValueError('reviewed selection-growth parameters changed')
 return dict(copy.deepcopy(pin['params']),family=FAMILY,name=o['name'])

def _selection_validate_witnesses(o,rows):
 for n,pin in SELECTION_PINS['witnesses'].items():
  if o.get(n)!=pin or any(rows.get(n,{}).get(k)!=v for k,v in SELECTION_PINS['registry'][n].items()):raise ValueError('selection-growth witness changed: '+n)

def _selection_variants(p):
 modes=['effect','payment','insufficient_mana','wrong_color','cast_payment','cleanup','pending_snapshot','tapped_source','opposing_turn','empty_team']
 if not p['sacrifice']:modes+=['repeat','source_destroyed_pending']
 if p['kind']=='bolster':modes+=['tie','protected_choice','preexisting_changes_minimum','response_changes_minimum','source_bounced_pending']
 if p['kind']=='support':modes+=['one_target','opponent_targets','targeting_pending']
 if p['kind']=='team':modes+=['recipient_bounced_pending','preexisting','entry_pay','entry_decline','entry_no_mana','entry_opponent','entry_source_removed']
 return [m+'_'+str(owner) for owner in (0,1) for m in modes]

class _SelectionWorld(_TransferWorld):
 def __init__(self,owner):super().__init__(owner,SELECTION_PINS)
 def cast_source(self,p,vector):
  super().cast_source(p,vector)
  if 'Creature' not in self.pins['witnesses'][p['name']]['type_line']:
   assert self.steps[-1]==_transfer_observe('source','sick',1)
   self.steps.pop();self.entities['source']['sick']=0

def _selection_expand(name,csvid,d,p,selected,rows,o):
 _selection_validate_witnesses(o,rows)
 if csvid!=SELECTION_PINS['profiles'][name]['csvid'] or not selected or len(set(selected))!=len(selected) or any(v not in _selection_variants(p) for v in selected):raise ValueError('unique pinned selection-growth variants required')
 out=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);w=_SelectionWorld(owner);support=p['kind']=='support';bolster=p['kind']=='bolster';team=p['kind']=='team';entry_mode=mode.startswith('entry_');empty=mode=='empty_team'
  if not empty:
   w.add('Deadly Insect' if mode=='protected_choice' else 'Grizzly Bears','recipient',owner)
   w.add('Suntail Hawk' if mode=='protected_choice' else 'Hill Giant','control',owner)
   w.add('Suntail Hawk','enemy_small',1-owner);w.add('Scaled Wurm','enemy_large',1-owner)
   if team:w.add('Ornithopter','artifact_creature',owner)
  elif not support:w.add('Suntail Hawk','enemy_small',1-owner)
  tapper='Scepter of Dominance' if mode=='tapped_source' and 'Creature' not in o[name]['type_line'] else 'Icy Manipulator'
  w.add('Darksteel Relic','noncreature',owner);w.add(tapper,'tapper',1-owner)
  w.add(name,'source',owner,'hand')
  if entry_mode:w.add('Grizzly Bears','entrant',1-owner if mode=='entry_opponent' else owner,'hand')
  response_name='Boomerang' if mode=='source_bounced_pending' else 'Unsummon'
  if mode in ('response_changes_minimum','source_bounced_pending','recipient_bounced_pending'):w.add(response_name,'response',1-owner,'hand')
  w.steps=[_transfer_step('reset_death_history'),_transfer_step('turn',player=owner,value=0),_transfer_step('present_ai_dialogs',value=1)];w.set_mana(owner,[40]*6);w.set_mana(1-owner,[40]*6)
  if mode in ('tie','preexisting_changes_minimum','preexisting') or bolster and mode=='pending_snapshot':w.grow('recipient',2 if mode=='preexisting_changes_minimum' else 1)
  cost=_transfer_fixed_cost(name,SELECTION_PINS)
  if mode=='cast_payment':w.set_mana(owner,cost)
  w.cast_source(dict(name=name,entry=0,entry_bonus=0),cost)
  if mode=='tapped_source':
   w.steps.extend([_transfer_step('choose_target',player=1-owner,ref='source'),_transfer_step('activate',ref='tapper',value=1),_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);w.pay(1-owner,[0,0,0,0,0,1] if tapper=='Scepter of Dominance' else [1,0,0,0,0,0]);w.entities['source']['tapped']=True;w.entities['tapper']['tapped']=True
  if mode=='opposing_turn':w.steps.append(_transfer_step('turn',player=1-owner,value=0))
  if mode=='payment':w.set_mana(owner,p['cost'])
  if mode=='insufficient_mana':w.set_mana(owner,[0]*6)
  if mode=='wrong_color':w.set_mana(owner,[sum(p['cost']),0,0,0,0,0])
  w.steps+=w.receipts();cut=None;bounced=None
  def alive_creatures(pl):return [ref for ref,e in w.entities.items() if e['zone']=='battlefield' and e['player']==pl and 'Creature' in o[e['name']]['type_line']]
  def minimum():
   candidates=alive_creatures(owner)
   if not candidates:return None,[]
   tough=lambda ref:int(o[w.entities[ref]['name']]['toughness'])+w.entities[ref]['counters']
   least=min(map(tough,candidates));tied=[ref for ref in candidates if tough(ref)==least]
   chosen='recipient' if mode=='protected_choice' else 'control' if 'control' in tied else tied[0]
   return chosen,tied
  targets=[] if empty else ['recipient','enemy_large'] if mode=='opponent_targets' else ['recipient'] if mode=='one_target' else ['recipient','control']
  def activate():
   w.steps.append(_transfer_step('require_can_activate',ref='source',value=1))
   if support:
    # Oracle-time target queue stays intact until the actual handler asks.
    w.steps.extend(_transfer_step('choose_target',player=owner,ref=ref) for ref in targets)
    if len(targets)==1:w.steps.append(_transfer_step('choose_cancel_target',player=owner))
   w.steps.append(_transfer_step('activate',ref='source',value=1));w.pay(owner,p['cost'])
   if p['sacrifice']:w.move_zone('source','graveyard')
   w.steps.append(_transfer_step('assert_stack',value=1));w.steps+=w.receipts()
  def resolve():
   chosen,tied=minimum() if bolster else (None,[])
   if bolster and len(tied)>1:w.steps.append(_transfer_step('choose_target',player=owner,ref=chosen))
   w.steps.extend([_transfer_step('resolve'),_transfer_step('assert_choices_consumed')])
   recipients=[chosen] if bolster and chosen else targets if support else alive_creatures(owner) if team else []
   for ref in recipients:
    assert w.entities[ref]['zone']=='battlefield';w.entities[ref]['counters']+=p['amount']
  if entry_mode:
   if mode=='entry_source_removed':w.steps.append(_transfer_step('destroy',ref='source'));w.move_zone('source','graveyard')
   pl=1-owner if mode=='entry_opponent' else owner;w.steps.append(_transfer_step('turn',player=pl,value=0));creature_cost=_transfer_fixed_cost('Grizzly Bears',SELECTION_PINS)
   if mode=='entry_no_mana':w.set_mana(owner,creature_cost)
   w.steps.extend([_transfer_step('cast',ref='entrant',value=1),_transfer_step('assert_stack',value=1)])
   if mode in ('entry_pay','entry_decline'):
    if owner in p['entry_ui']['trigger_players']:w.steps.append(_transfer_step('choose_trigger',player=owner,ref='source'))
    w.steps.append(_transfer_step('choose_dialog',player=owner,value=p['entry_ui']['decline_dialog' if mode=='entry_decline' else 'pay_dialog']))
   w.steps.extend([_transfer_step('resolve'),_transfer_step('assert_choices_consumed')]);w.pay(pl,creature_cost);w.zones['hand'][pl].remove('Grizzly Bears');w.entities['entrant'].update(zone='battlefield',sick=1)
   if mode=='entry_pay':w.pay(owner,[p['entry_pay'],0,0,0,0,0]);w.entities['entrant']['counters']+=1
   w.steps+=w.receipts()+[_transfer_observe('entrant','sick',1)]
  elif mode=='cast_payment':pass
  elif mode in ('insufficient_mana','wrong_color'):w.steps.extend([_transfer_step('require_can_activate',ref='source',value=0),_transfer_step('activate',ref='source',value=0)])
  else:
   activate();cut=len(w.steps)
   if mode=='targeting_pending':
    w.steps.append(_transfer_step('assert_choices_consumed'))
   else:
    if mode=='source_destroyed_pending':w.steps.append(_transfer_step('destroy',ref='source'));w.move_zone('source','graveyard')
    if mode in ('response_changes_minimum','source_bounced_pending','recipient_bounced_pending'):
     bounced='source' if mode=='source_bounced_pending' else 'recipient';w.steps.extend([_transfer_step('choose_target',player=1-owner,ref=bounced),_transfer_step('cast',ref='response',value=1),_transfer_step('assert_stack',value=2)]);w.pay(1-owner,_transfer_fixed_cost(response_name,SELECTION_PINS));w.zones['hand'][1-owner].remove(response_name);w.zones['graveyard'][1-owner].append(response_name);w.entities['response']['zone']='graveyard';w.move_zone(bounced,'hand')
    resolve()
    if bounced:w.steps.append(_transfer_step('bind_hand',ref=bounced,player=w.entities[bounced]['player'],value=rows[w.entities[bounced]['name']]['csvid']))
    if mode=='repeat':w.steps+=w.receipts();activate();resolve()
  if mode=='cleanup':w.steps+=w.receipts()+[_transfer_step('cleanup'),_transfer_step('refresh')]
  checks=[_transfer_step('assert_stack',value=int(mode=='targeting_pending'))]+([] if mode=='targeting_pending' else [_transfer_step('assert_choices_consumed')])+w.receipts()
  if mode=='pending_snapshot':
   assert cut is not None
   tail=w.steps[cut:]+checks;steps=w.steps[:cut]+[_transfer_step('capture')]+copy.deepcopy(tail)+[_transfer_step('record_state'),_transfer_step('ai_roundtrip'),_transfer_step('restore')]+copy.deepcopy(tail)+[_transfer_step('assert_recorded_state')]
  else:steps=w.steps+checks
  if mode=='entry_source_removed':
   proof=_selection_expand(name,csvid,d,p,['entry_pay_'+str(owner)],rows,o)[0];assert proof['setup']==w.setup
   steps=[_transfer_step('capture')]+copy.deepcopy(proof['steps'])+[_transfer_step('restore')]+steps
  out.append(dict(name=f'SelectionGrowthActivationRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],scope_gaps=p['gaps']+['control changes','counter replacement','multiplayer'])))
 return out
