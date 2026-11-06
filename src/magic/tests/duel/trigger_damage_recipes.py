"""Pinned creature entry/death damage, through actual casting and death processing."""
import json,re
from collections import Counter
from pathlib import Path
from creature_recipes import KEYWORDS,REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS,reviewed_payment
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY

PINS=json.loads((Path(__file__).parent/'data/trigger_damage_witnesses.json').read_text(encoding='utf8'))['cards']
BOARD=('Savannah Lions','Scaled Wurm','Suntail Hawk','Air Elemental','Phyrexian Hulk','Darksteel Myr','Darksteel Gargoyle','Nyxborn Shieldmate','Dryad Arbor','Sol Ring','Forest')
SCOPES={'any':'any target','creature':'target creature','flying_creature':'target creature with flying','player':'target player or planeswalker','opponent':'target opponent or planeswalker','self':'you','all_creatures':'each creature','other_creatures':'each other creature','ground_creatures':'each creature without flying','flying_creatures':'each creature with flying','opposing_flying_creatures':'each creature with flying your opponents control','all_creatures_players':'each creature and each player','flying_creatures_players':'each creature with flying and each player','opposing_creatures_planeswalkers':'each opponent and each creature and planeswalker they control','opponents':'each opponent'}
TARGETED={'any','creature','flying_creature','player','opponent'}


def spec(d,r):
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or 'Creature' not in r.get('type_line','') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('whole pinned normal creature and explicit variants required')
 p=d.get('params');required={'when','amount','targets','damage_you','tap_affected','keywords','inactive_rules'}
 if not isinstance(p,dict) or not required<=set(p) or set(p)-required-{'payment_colors'}:raise ValueError('explicit trigger, damage, target scope, collateral damage and remaining rules required')
 if p['when'] not in ('entry','death') or type(p['amount']) is not int or not 1<=p['amount']<=6 or p['targets'] not in SCOPES or type(p['damage_you']) is not int or p['damage_you'] not in (0,1) or type(p['tap_affected']) is not bool:raise ValueError('reviewed bounded ordinary trigger required')
 if p['damage_you'] and p['targets']!='creature' or p['tap_affected'] and p['targets']!='opposing_flying_creatures':raise ValueError('collateral damage/tapping requires the reviewed creature scope')
 if not isinstance(p['keywords'],list) or p['keywords']!=sorted(set(p['keywords'])) or set(p['keywords'])-{'Flying','Reach','Haste','Trample','First strike'}:raise ValueError('reviewed passive source keywords required')
 if not isinstance(p['inactive_rules'],list) or len(set(p['inactive_rules']))!=len(p['inactive_rules']):raise ValueError('explicit distinct inactive rules required')
 metadata=set(p['keywords']);inactive=set(p['inactive_rules'])
 for line in inactive:
  if not isinstance(line,str):raise ValueError('inactive rule must be printed text')
  if re.fullmatch(r'Echo (?:\{[1-9WUBRGC]\})+ \(At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost\.\)',line):metadata.add('Echo')
  elif re.fullmatch(r'Cycling (?:\{(?:[1-9WUBRGC]|[WUBRG]/[WUBRG])\})+ \(.+Discard this card: Draw a card\.\)',line):metadata.add('Cycling')
  elif re.fullmatch(r'(?:\{[1-9WUBRGC]\})+: Return this card from your graveyard to your hand\.',line):pass
  elif re.fullmatch(r'(?:\{[1-9WUBRGC]\})+: This creature gets \+\d+/\+\d+ until end of turn and deals \d+ damage to you\.',line):pass
  elif line in ("You may have this creature assign its combat damage as though it weren't blocked.","This creature can't be blocked by creatures with power 2 or less."):pass
  else:raise ValueError('unreviewed inactive rule')
 tails=' and 1 damage to you.' if p['damage_you'] else '.'
 if p['tap_affected']:tails+=' Tap those creatures.'
 expected={f'When {ref} '+('enters'+suffix if p['when']=='entry' else 'dies')+f', {actor} deals {p["amount"]} damage to {SCOPES[p["targets"]]}{tails}' for ref in ('this creature',r['name'],r['name'].split(',')[0]) for suffix in ('',' the battlefield') for actor in ('it',r['name'],r['name'].split(',')[0])}
 found=[];keys=[];seen_inactive=[];canonical={k.lower():k for k in KEYWORDS}
 for line in r['oracle_text'].splitlines():
  if line in expected:found.append(line);continue
  if line in inactive:seen_inactive.append(line);continue
  text=line
  for reminder in re.findall(r'\([^\n]*?\)',line):
   if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed passive reminder')
   text=text.replace(reminder,'')
  for word in text.strip().split(','):
   if word.strip().lower() not in canonical:raise ValueError('unreviewed extra trigger or passive clause')
   keys.append(canonical[word.strip().lower()])
 if len(found)!=1 or sorted(keys)!=p['keywords'] or set(seen_inactive)!=inactive or set(r.get('keywords',[]))!=metadata:raise ValueError('trigger parameters must match complete Oracle text and metadata')
 for k in ('power','toughness'):
  if not re.fullmatch(r'\d+',r.get(k,'')) or not 0<=int(r[k])<=10:raise ValueError('bounded numeric source stats required')
 if not int(r['toughness']):raise ValueError('source must survive ordinary entry')
 mana,hybrid=reviewed_payment(r,p)
 return dict(p,family='trigger_damage',mana=mana,power=int(r['power']),toughness=int(r['toughness']),legendary='Legendary' in r['type_line'],hybrid=bool(hybrid))


def variants(p):
 vs=['effect','payment','insufficient_mana','bare_zones','cleanup','countered','bounce','blink','bounce_recast','destroy','exile','other_creature','other_death','ready_snapshot','pending_snapshot','resolved_snapshot','repeat','only_source','tapped_source']
 if any(p['mana'][1:]):vs.append('wrong_color')
 if p['targets'] in ('any','creature','flying_creature'):vs+=['friendly_target','lethal','indestructible','target_removed_before_entry'] if p['when']=='entry' else ['friendly_target','lethal','indestructible']
 if p['targets'] in ('any','player'):vs+=['own_player','opponent_player']
 if p['targets']=='any' or p['targets']=='creature' or p['targets']=='flying_creature' and 'Flying' in p['keywords']:
  if p['when']=='entry':vs.append('self_target')
 if not p['legendary']:vs.append('two_sources')
 return [v+'_'+str(owner) for owner in (0,1) for v in vs]


def validate_witnesses(o,rows):
 for nm,pin in PINS.items():
  if nm not in rows or any(o.get(nm,{}).get(k)!=v for k,v in pin.items()):raise ValueError('trigger damage witness changed: '+nm)


def creature_affected(scope,r,controller,owner,ref,source):
 if 'Creature' not in r['type_line']:return False
 flying='Flying' in r.get('keywords',[])
 if scope=='other_creatures' and ref==source:return False
 if scope in ('flying_creatures','flying_creatures_players','opposing_flying_creatures') and not flying:return False
 if scope=='ground_creatures' and flying:return False
 if scope in ('opposing_flying_creatures','opposing_creatures_planeswalkers') and controller==owner:return False
 return scope in ('all_creatures','other_creatures','ground_creatures','flying_creatures','all_creatures_players','flying_creatures_players','opposing_flying_creatures','opposing_creatures_planeswalkers')


def expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 out=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';targeted=p['targets'] in TARGETED;creature_target=p['targets'] in ('any','creature','flying_creature') and v not in ('own_player','opponent_player')
  zones={z:[list(vals) if not bare else [] for _ in (0,1)] for z,vals in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY))}
  setup=[];entities={};mana=[[20]*6,[20]*6];life=[50,50];pre=[step('turn',player=owner,value=0)];actions=[];pending_cut=None
  for player in (0,1):
   pre += [step('set_life',player=player,value=50)]+[step('set_mana',player=player,color=c,value=20) for c in range(6)]
   for zone,by_player in zones.items():setup += [card(nm,f'zone_{player}_{zone}_{i}',player,zone) for i,nm in enumerate(by_player[player])]
  def add(nm,ref,player,zone='battlefield'):
   setup.append(card(nm,ref,player,zone));r=o[nm];tapped=zone=='battlefield' and nm in ('Dryad Arbor','Sol Ring','Forest')
   if tapped:setup[-1]['state']=['tapped']
   entities[ref]=dict(name=nm,player=player,alive=zone=='battlefield',hand=zone=='hand',damage=0,tapped=int(tapped))
   if zone=='hand':zones['hand'][player].append(nm)
  def leave(ref,zone):
   e=entities[ref];e['alive']=False;e['hand']=zone=='hand';e['damage']=0;e['tapped']=0;zones[zone][e['player']].append(e['name'])
  def pay(nm,caster):
   cost=p['mana'] if nm==name else cost_from_oracle(o[nm]);mana[caster]=[a-b for a,b in zip(mana[caster],cost)]
  add(name,'source',owner,'hand')
  recipient=None
  if v!='only_source':
   if creature_target:
    nm='Suntail Hawk' if v=='lethal' and p['targets']=='flying_creature' else 'Savannah Lions' if v=='lethal' else 'Darksteel Gargoyle' if v in ('indestructible','repeat','two_sources','bounce_recast','blink') else 'Air Elemental' if p['targets']=='flying_creature' else 'Scaled Wurm'
    add(nm,'recipient',owner if v=='friendly_target' else 1-owner);recipient='recipient'
    add('Darksteel Gargoyle','unselected',owner)
    if v=='target_removed_before_entry':add('Darksteel Gargoyle','replacement',1-owner)
   elif not targeted and p['targets'] not in ('self','opponents'):
    for player in (0,1):
     for i,nm in enumerate(BOARD):add(nm,f'board_{player}_{i}',player)
  elif creature_target and p['when']=='entry' and (p['targets']!='flying_creature' or 'Flying' in p['keywords']):recipient='source'
  if v=='self_target':recipient='source'
  target_player=owner if v=='own_player' else 1-owner
  def choices(source='source',target=None):
   target=target or recipient
   if not targeted:return []
   if creature_target:return [step('choose_target',player=owner,ref=target)] if target else []
   return [] if d.get('implicit_player_target') else [step('choose_player',player=owner,value=target_player)]
  def effect(source='source',target=None):
   target=target or recipient;scope=p['targets'];hit=[]
   if targeted:
    if creature_target:
     if not target:return
     hit=[target]
    else:life[target_player]-=p['amount']
   elif scope=='self':life[owner]-=p['amount']
   elif scope=='opponents':life[1-owner]-=p['amount']
   else:
    hit=[ref for ref,e in entities.items() if e['alive'] and creature_affected(scope,o[e['name']],e['player'],owner,ref,source)]
    if scope in ('all_creatures_players','flying_creatures_players'):life[0]-=p['amount'];life[1]-=p['amount']
    if scope=='opposing_creatures_planeswalkers':life[1-owner]-=p['amount']
   life[owner]-=p['damage_you']
   for ref in hit:
    e=entities[ref]
    if not e['alive']:continue
    e['damage']+=p['amount'];r=o[e['name']]
    if p['tap_affected']:e['tapped']=1
    if e['damage']>=int(r['toughness']) and 'Indestructible' not in r.get('keywords',[]):leave(ref,'graveyard')
  def cast(source='source',trigger=True):
   e=entities[source];pay(name,owner);zones['hand'][owner].remove(name);e['hand']=False;e['alive']=True;e['damage']=0;e['tapped']=0
   seq=[step('cast',ref=source,value=1),step('assert_stack',value=1)]
   if p['when']=='entry':
    seq+=choices(source)
    if trigger:effect(source)
   return seq+[step('resolve'),step('assert_choices_consumed')]
  def die(source='source',method='destroy'):
   seq=[]
   if p['when']=='death':seq+=choices(source)
   seq += [step('destroy',ref=source),step('resolve'),step('assert_choices_consumed')];leave(source,'graveyard')
   if p['when']=='death':effect(source)
   return seq
  def helper(nm,ref,target,caster):
   add(nm,ref,caster,'hand');pay(nm,caster);zones['hand'][caster].remove(nm);zones['graveyard'][caster].append(nm);entities[ref]['hand']=False
   return [step('choose_target',player=caster,ref=target),step('cast',ref=ref,value=1),step('assert_stack',value=1)]
  if v in ('payment','insufficient_mana','wrong_color'):
   mana[owner]=list(p['mana']) if v!='insufficient_mana' else [0]*6
   if v=='wrong_color':
    # Remove all allowed printed colors; same total mana cannot pay the base cost.
    unused=next(c for c in range(1,6) if not p['mana'][c] and all('BWURG'[c-1] not in pair for pair in re.findall(r'\{([WUBRG]/[WUBRG])\}',o[name]['mana_cost'])))
    mana[owner]=[sum(p['mana']),0,0,0,0,0];mana[owner][unused]=mana[owner][0];mana[owner][0]=0
   pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  if v in ('insufficient_mana','wrong_color'):actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
  elif v=='countered':
   add('Essence Scatter','counter',0,'hand');pay(name,owner);pay('Essence Scatter',0);zones['hand'][owner].remove(name);zones['graveyard'][owner].append(name);zones['hand'][0].remove('Essence Scatter');zones['graveyard'][0].append('Essence Scatter');entities['source']['hand']=entities['counter']['hand']=False
   actions += [step('choose_priority',ref='counter'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
  else:
   start=cast();pending_cut=next(i for i,s in enumerate(start) if s['op'].startswith('choose_') or s['op']=='resolve')
   if v=='target_removed_before_entry':
    # Move the initially available creature before the entry trigger exists;
    # a different legal creature is selected when the source actually enters.
    i=next(i for i,s in enumerate(start) if s['op'].startswith('choose_'))
    before=helper('Unsummon','target_bounce','recipient',1-owner)+[step('assert_stack',value=2)]
    before=[s for s in before if not (s['op']=='assert_stack' and s['value']==1)]
    start[i:i]=before;leave('recipient','hand')
    for s in start[i+len(before):]:
     if s['op']=='choose_target':s['ref']='replacement'
    # Reconstruct the expected damage: the initial prediction is undone.
    e=entities['recipient'];e['damage']=0
    # The default recipient survives this single entry damage in this variant.
    entities['replacement']['damage']=p['amount']
   actions+=start
   if p['when']=='death' and v not in ('bounce','blink','exile','other_creature','other_death','only_source'):
    pending_cut=len(actions);actions+=die()
   if v=='tapped_source' and entities['source']['alive']:
    actions+=helper('Twiddle','tap','source',1-owner)+[step('choose_dialog',player=1-owner,value=0),step('resolve'),step('assert_choices_consumed')];entities['source']['tapped']=1
   if v in ('bounce','bounce_recast') and entities['source']['alive']:
    actions+=helper('Unsummon','bounce','source',1-owner)+[step('resolve'),step('assert_choices_consumed'),step('bind_hand',player=owner,value=csvid,ref='source')];leave('source','hand')
    if v=='bounce_recast':actions+=cast()
   if v=='blink' and entities['source']['alive']:
    actions+=helper('Cloudshift','blink','source',owner)
    if p['when']=='entry':actions+=choices();entities['source']['damage']=0;effect()
    actions += [step('resolve'),step('assert_choices_consumed'),step('bind_permanent',player=owner,value=csvid,ref='source')]
   if v=='destroy' and entities['source']['alive']:actions+=die()
   if v=='exile' and entities['source']['alive']:
    actions+=helper('Swords to Plowshares','exile','source',1-owner)+[step('resolve'),step('assert_choices_consumed')];leave('source','exile');life[owner]+=p['power']
   if v in ('other_creature','other_death'):
    add('Grizzly Bears','other',owner,'hand');pay('Grizzly Bears',owner);zones['hand'][owner].remove('Grizzly Bears');entities['other']['hand']=False;entities['other']['alive']=True
    actions += [step('cast',ref='other',value=1),step('resolve'),step('assert_choices_consumed')]
    if v=='other_death':actions += [step('destroy',ref='other')];leave('other','graveyard')
   if v=='two_sources':
    add(name,'second',owner,'hand');actions+=cast('second')
    if p['when']=='death':actions+=die('second')
   if v=='repeat' and p['when']=='entry' and entities['source']['alive']:
    actions+=helper('Cloudshift','repeat_blink','source',owner)+choices()+[step('resolve'),step('assert_choices_consumed'),step('bind_permanent',player=owner,value=csvid,ref='source')];entities['source']['damage']=0;effect()
   if v=='cleanup':
    actions.append(step('cleanup'))
    for e in entities.values():e['damage']=0
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref,e in entities.items():
   checks += [observe(ref,'in_play',int(e['alive'])),observe(ref,'in_hand',int(e['hand']))]
   if e['alive']:
    r=o[e['name']]
    if 'Creature' in r['type_line']:checks += [observe(ref,'power',int(r['power'])),observe(ref,'toughness',int(r['toughness'])),observe(ref,'damage',e['damage']),observe(ref,'plus1_counters',0),observe(ref,'minus1_counters',0)]
    checks += [observe(ref,'tapped',e['tapped'])]
  universe=set(PINS)|{name}|set(HAND+GRAVE+EXILE+LIBRARY)
  for player in (0,1):
   checks += [step('assert_life',player=player,value=life[player]),step('assert_poison',player=player,value=0),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
   if v!='cleanup':checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
   for zone,by_player in zones.items():
    names=by_player[player];checks.append(step('assert_zone',player=player,zone=zone,value=len(names)));counts=Counter(names)
    if zone=='library':checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(names)]
    else:checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=rows[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  seq=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=0 if v=='ready_snapshot' else pending_cut
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  out.append(dict(name=f'TriggerDamageRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family='trigger_damage',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['keywords']+p['inactive_rules']+['planeswalker targets','control changes','simultaneous trigger ordering','target leaves after trigger targeting','replacement/prevention effects','combat-trigger interactions','multiplayer'])))
 return out
