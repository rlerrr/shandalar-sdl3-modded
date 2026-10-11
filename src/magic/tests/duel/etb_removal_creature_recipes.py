"""Pinned single-target entry removal through real casts, targeting and re-entry."""
import re
from collections import Counter
from creature_recipes import KEYWORDS, REMINDERS
from counter_damage_creature_recipes import MECHANICS
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

TARGETS={'artifact':('Ornithopter','Phyrexian Hulk'),
         'enchantment':('Seal of Cleansing','Seal of Primordium'),
         'creature':('Hill Giant','Craw Wurm'), 'land':('Forest','Plains')}
SPELLS={'Unsummon':('{U}',"Return target creature to its owner's hand."),
        'Cloudshift':('{W}','Exile target creature you control, then return that card to the battlefield under your control.'),
        'Essence Scatter':('{1}{U}','Counter target creature spell.')}

def spec(d,r):
 from recipes import cost_from_oracle
 if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
     or 'Creature' not in r.get('type_line','') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('entry removal requires a pinned normal creature and explicit variants')
 priority=d.get('trigger_priority',False)
 if type(priority) is not bool:raise ValueError('trigger_priority requires a boolean input annotation')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'effect','targets','optional','keywords'}:raise ValueError('explicit entry effect, targets, optionality and keywords required')
 if p['effect'] not in ('destroy','bounce') or type(p['optional']) is not bool or p['targets'] not in (['artifact'],['enchantment'],['artifact','enchantment'],['artifact','enchantment','land'],['creature']):raise ValueError('unreviewed entry removal scope')
 if priority and not p['optional']:raise ValueError('priority selection requires an optional entry trigger')
 noun={'artifact':'artifact','enchantment':'enchantment','artifact,enchantment':'artifact or enchantment','artifact,enchantment,land':'artifact, enchantment, or land','creature':'creature'}[','.join(p['targets'])]
 clause=('destroy target '+noun+'.' if p['effect']=='destroy' else "return target "+noun+" to its owner's hand.")
 expected={f"When {ref} enters{suffix}, "+('you may ' if p['optional'] else '')+clause for ref in ('this creature',r['name'],r['name'].split(',')[0]) for suffix in ('',' the battlefield')}
 keys=[];rules=[];abilities=[];canonical={k.lower():k for k in KEYWORDS+('Hexproof','Shroud')+tuple(MECHANICS)}
 for line in r['oracle_text'].splitlines():
  if line in expected:rules.append(line);continue
  if ':' in line and re.match(r'^(?:\{|Sacrifice |Discard |Remove |Exile |Pay |Tap |Untap |Return )',line):abilities.append(line);continue
  text=line
  for reminder in re.findall(r'\([^\n]*?\)',line):
   if reminder not in REMINDERS|EXTRA_REMINDERS|set(MECHANICS.values()):raise ValueError('unreviewed entry removal reminder')
   text=text.replace(reminder,'')
  for word in text.strip().split(','):
   if word.strip().lower() not in canonical:raise ValueError('unreviewed entry removal passive or triggered clause')
   keys.append(canonical[word.strip().lower()])
 if set(keys)&{'Shroud','Hexproof'}:raise ValueError('entry source protection needs dedicated re-entry actions')
 if len(rules)!=1 or p['keywords']!=sorted(keys) or len(keys)!=len(set(keys)) or set(keys)!=set(r.get('keywords',[])):raise ValueError('entry removal parameters disagree with whole passive scope')
 for k in ('power','toughness'):
  if not re.fullmatch(r'\d+',r.get(k,'')) or not 0<=int(r[k])<=10:raise ValueError('bounded numeric entry source stats required')
 if int(r['toughness'])==0:raise ValueError('entry removal source must survive entry')
 return dict(p,family='etb_removal_creature',power=int(r['power']),toughness=int(r['toughness']),mana=cost_from_oracle(r),remaining_abilities=abilities)

def variants(p):
 modes=['entry','own_target','payment','insufficient_mana','no_other_targets','wrong_type','countered','cleanup_once','other_creature',
        'bounce_only','bounce_recast','blink','destroy_source','ready_snapshot','pending_snapshot','resolved_snapshot','blink_snapshot','bare_zones']
 if p['optional']:modes.append('decline')
 if any(p['mana'][1:]):modes.append('wrong_color')
 modes += ['target_'+t for t in p['targets']]
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]

def validate_witnesses(o,registry):
 for category,names in TARGETS.items():
  for nm in names:
   r=o.get(nm,{})
   if nm not in registry or r.get('layout')!='normal' or category.title() not in r.get('type_line',''):raise ValueError('entry removal target witness changed: '+nm)
 for nm,values in SPELLS.items():
  r=o.get(nm,{})
  if nm not in registry or (r.get('mana_cost'),r.get('oracle_text'))!=values:raise ValueError('entry removal spell witness changed: '+nm)
 for nm,text in [('Seal of Cleansing','Sacrifice this enchantment: Destroy target artifact or enchantment.'),('Seal of Primordium','Sacrifice this enchantment: Destroy target artifact or enchantment.')]:
  if o[nm].get('oracle_text')!=text:raise ValueError('entry removal witness has new passive behavior')

def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';full=not bare
  hands=[list(HAND) if full else [] for _ in (0,1)];graves=[list(GRAVE) if full else [] for _ in (0,1)]
  libs=[list(LIBRARY) if full else [] for _ in (0,1)];exiles=[list(EXILE) if full else [] for _ in (0,1)]
  mana=[[10]*6,[10]*6];setup=[];entities={};actions=[];pre=[step('turn',player=owner,value=0)]
  def add(nm,ref,player,zone='battlefield'):
   setup.append(card(nm,ref,player,zone))
   if zone=='battlefield' and 'Land' in oracle[nm]['type_line']:setup[-1]['state']=['tapped']
   if zone=='hand':hands[player].append(nm)
   entities[ref]=dict(name=nm,player=player,alive=zone=='battlefield',hand=zone=='hand')
  add(name,'source',owner,'hand')
  category=v[len('target_'):] if v.startswith('target_') else p['targets'][0]
  victim,alternate=TARGETS[category];target_owner=owner if v=='own_target' else 1-owner
  no_other=v in ('no_other_targets','wrong_type')
  if not no_other:
   add(victim,'recipient',target_owner);add(alternate,'unselected',target_owner)
  if v=='wrong_type':
   invalid=next(t for t in TARGETS if t not in p['targets']);add(TARGETS[invalid][0],'invalid',1-owner)
  if v in ('blink','blink_snapshot','bounce_recast'):add(alternate,'second_target',1-target_owner)
  if v in ('payment','insufficient_mana','wrong_color'):
   mana[owner]=list(p['mana']) if v!='insufficient_mana' else [0]*6
   if v=='wrong_color':
    c=next(c for c in range(1,6) if mana[owner][c]);mana[owner][1 if c!=1 else 2]+=mana[owner][c];mana[owner][c]=0
   pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  def pay(cost,player):mana[player]=[a-b for a,b in zip(mana[player],cost)]
  def remove(ref):
   e=entities[ref];e['alive']=False;e['hand']=p['effect']=='bounce'
   (hands if e['hand'] else graves)[e['player']].append(e['name'])
  def entry_choice(ref='recipient',decline=False):
   priority=d.get('trigger_priority',False) and owner==0
   if decline:return [] if priority else [step('choose_cancel_target',player=owner)]
   return ([step('choose_trigger',player=owner,ref='source')] if priority else [])+[step('choose_dialog',player=owner,value=n) for n in d.get('resolve_dialogs',[])]+[step('choose_target',player=owner,ref=ref)]
  def entry(ref='source',target='recipient',decline=False,has_target=True):
   out=[step('cast',ref=ref,value=1),step('assert_stack',value=1)]+(entry_choice(target,decline) if has_target else [])+[step('resolve'),step('assert_choices_consumed')]
   pay(p['mana'],owner);hands[owner].remove(name);entities[ref]['alive']=True;entities[ref]['hand']=False
   if has_target and not decline:
    remove(target)
    if p['effect']=='bounce':out += [step('bind_hand',player=entities[target]['player'],value=registry[entities[target]['name']]['csvid'],ref=target)]
   return out
  def spell(nm,ref,target,caster):
   setup.append(card(nm,ref,caster,'hand'));pay(cost_from_oracle(oracle[nm]),caster);graves[caster].append(nm)
   return [step('choose_target',player=caster,ref=target),step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
  if v in ('insufficient_mana','wrong_color'):
   actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
  elif v=='countered':
   setup.append(card('Essence Scatter','counter',0,'hand'));pay(p['mana'],owner);pay(cost_from_oracle(oracle['Essence Scatter']),0);hands[owner].remove(name);graves[owner].append(name);graves[0].append('Essence Scatter');entities['source']['hand']=False
   actions += [step('choose_priority',player=0,ref='counter'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
  else:
   self_target=no_other and category=='creature'
   actions += entry(target='source' if self_target else 'recipient',decline=v=='decline' or self_target and p['optional'],has_target=not no_other or self_target)
   if self_target and not p['optional'] and p['effect']=='bounce':actions += [step('bind_hand',player=owner,value=csvid,ref='source')]
   if v in ('bounce_only','bounce_recast'):
    actions += spell('Unsummon','bounce','source',0);entities['source']['alive']=False;entities['source']['hand']=True;hands[owner].append(name)
    actions += [step('bind_hand',player=owner,value=csvid,ref='source')]
    if v=='bounce_recast':actions += entry(target='second_target')
   if v in ('blink','blink_snapshot'):
    # The re-entry target is chosen during Cloudshift resolution, after its
    # own cast target. Both are queued through the normal host decision path.
    setup.append(card('Cloudshift','blink',owner,'hand'));pay(cost_from_oracle(oracle['Cloudshift']),owner);graves[owner].append('Cloudshift')
    actions += [step('choose_target',player=owner,ref='source'),step('cast',ref='blink',value=1),step('assert_stack',value=1)]+entry_choice('second_target')+[step('resolve'),step('assert_choices_consumed'),step('bind_permanent',player=owner,value=csvid,ref='source')];remove('second_target')
    if p['effect']=='bounce':actions += [step('bind_hand',player=entities['second_target']['player'],value=registry[entities['second_target']['name']]['csvid'],ref='second_target')]
   if v=='destroy_source':actions += [step('destroy',ref='source')];entities['source']['alive']=False;graves[owner].append(name)
   if v=='cleanup_once':actions.append(step('cleanup'))
   if v=='other_creature':
    add('Grizzly Bears','other',owner,'hand');pay(cost_from_oracle(oracle['Grizzly Bears']),owner);hands[owner].remove('Grizzly Bears');entities['other']['alive']=True;entities['other']['hand']=False
    actions += [step('cast',ref='other',value=1),step('resolve'),step('assert_choices_consumed')]
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref,e in entities.items():
   checks += [observe(ref,'in_play',int(e['alive'])),observe(ref,'in_hand',int(e['hand']))]
   r=oracle[e['name']]
   if e['alive'] and 'Creature' in r['type_line']:checks += [observe(ref,'power',int(r['power'])),observe(ref,'toughness',int(r['toughness'])),observe(ref,'damage',0),observe(ref,'plus1_counters',0),observe(ref,'minus1_counters',0)]
  for player in (0,1):
   checks += [step('assert_life',player=player,value=20),step('assert_poison',player=player,value=0),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
   if v!='cleanup_once':
    checks += [step('assert_mana_total',player=player,value=sum(mana[player]))]
    if v in ('payment','insufficient_mana','wrong_color'):checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
    checks += [step('assert_zone',player=player,zone=zone,value=len(names))]
    checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(names if zone=='library' else dict.fromkeys(names))]
   checks += [step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=n) for nm,n in Counter(hands[player]+graves[player]).items()]
  seq=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot','blink_snapshot'):
   cut=0 if v=='ready_snapshot' else max(i for i,s in enumerate(actions) if s['op']=='resolve')
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  result.append(dict(name=f'EtbRemovalCreatureRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=seq,_recipe=dict(family=p['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['remaining_abilities']+p['keywords']+['target leaves after trigger targeting','trigger ordering','alternative costs'])))
 return result
