"""Pinned fixed keyword abilities, proved through real combat and targeting rules."""
import copy,re
from collections import Counter
from creature_recipes import classify
from passive_keyword_creature_recipes import split
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from value_activation_recipes import SPELLS
from target_stats_activation_recipes import CLOAK

SCOPES={'creature':'creature','own_creature':'creature you control','green_creature':'green creature',
 'red_creature':'red creature','black_creature':'black creature','blue_creature':'blue creature',
 'beast_creature':'Beast creature','human':'Human','power_five':'creature with power 5 or greater'}
TARGETS={'creature':'Grizzly Bears','own_creature':'Grizzly Bears','green_creature':'Grizzly Bears',
 'red_creature':'Hill Giant','black_creature':'Scathe Zombies','blue_creature':'Merfolk of the Pearl Trident',
 'beast_creature':'Rumbling Baloth','human':'Elite Vanguard','power_five':'Scaled Wurm'}
STATS={'Grizzly Bears':(2,2),'Hill Giant':(3,3),'Scathe Zombies':(2,2),
 'Merfolk of the Pearl Trident':(1,1),'Rumbling Baloth':(4,4),'Elite Vanguard':(2,1),
 'Scaled Wurm':(7,6),'Suntail Hawk':(1,1),'Air Elemental':(4,4)}
GRANTS=('Flying','First strike','Double strike','Trample','Deathtouch','Lifelink','Vigilance','Haste','Indestructible','Shroud')
REMINDERS={'Lifelink':(' (Damage dealt by the creature also causes its controller to gain that much life.)',),
 'Haste':(' (It can attack and {T} this turn.)',' (It can attack this turn.)'),
 'Deathtouch':(' (Any amount of damage it deals to a creature is enough to destroy it.)',),
 'Shroud':(" (It can't be the target of spells or abilities.)",)}
ROBE=('\n'.join(('Enchant creature (Target a creature as you cast this. This card enters attached to that creature.)',
 "Enchanted creature has shroud. (It can't be the target of spells or abilities.)")))

def spec(d,r):
 from recipes import cost_from_oracle
 if not r or r.get('type_line','').startswith('Token ') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:
  raise ValueError('whole Oracle pin and explicit variants required')
 abilities,plain=split(r);base=classify(plain);p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'keywords','grant','targets','mana_cost','tap'}:
  raise ValueError('explicit source keywords, grant, target scope and costs required')
 if p['keywords']!=base['keywords'] or not isinstance(p['grant'],str) or p['grant'] not in GRANTS:
  raise ValueError('reviewed fixed keyword and unconditional source keywords required')
 if not isinstance(p['targets'],str) or p['targets'] not in SCOPES or type(p['tap']) is not bool or not isinstance(p['mana_cost'],str):
  raise ValueError('reviewed scope, boolean tap cost and structured mana required')
 activation=[0]*6 if p['mana_cost']=='' else cost_from_oracle({'mana_cost':p['mana_cost']})
 cost=', '.join(([p['mana_cost']] if p['mana_cost'] else [])+(['{T}'] if p['tap'] else []))
 if not cost:raise ValueError('nonempty activation cost required')
 clause=f'{cost}: Target {SCOPES[p["targets"]]} gains {p["grant"].lower()} until end of turn.'
 if len(abilities)!=1 or abilities[0] not in {clause+s for s in ('',)+REMINDERS.get(p['grant'],())}:
  raise ValueError('only a single whole-card fixed target keyword ability is reviewed')
 return dict(p,family='target_keyword_activation',activation_mana=activation,mana=base['mana'],
  base_power=base['power'],base_toughness=base['toughness'],legendary='Legendary' in r['type_line'],sacrifice=False)

def variants(p):
 common=['effect','unselected_probe','payment','tapped','sick','fresh_activation','opposing_turn','cast','cast_payment',
  'no_targets','shrouded_targets','cleanup','target_bounced','target_destroyed','target_shrouded_pending',
  'tax','tax_insufficient','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot',
  'source_bounced','source_destroyed']
 if p['targets']!='own_creature':common+=['friendly']
 else:common+=['wrong_controller']
 if p['grant']!='Haste':common+=['expired_probe']
 else:common+=['fresh_without_grant','ready_recipient']
 if sum(p['mana']):common.append('insufficient_cast_mana')
 if sum(p['activation_mana']):common.append('insufficient_mana')
 if any(p['activation_mana'][1:]):common.append('wrong_color')
 if p['targets'] not in ('creature','own_creature'):common.append('wrong_target')
 if p['tap']:
  common+=['repeat_denied']
  if p['grant']!='Shroud':common+=['untap_repeat']
 else:common+=['stacked']
 if not p['legendary']:common+=['two_sources']
 return [v+'_'+str(owner) for owner in (0,1) for v in common]

def validate_witnesses(o,registry):
 for nm,pt in STATS.items():
  r=o.get(nm,{})
  expected='Flying' if nm in ('Suntail Hawk','Air Elemental') else ''
  if nm not in registry or r.get('layout')!='normal' or r.get('card_faces') or 'Creature' not in r.get('type_line','') or (r.get('power'),r.get('toughness'),r.get('oracle_text'))!=(str(pt[0]),str(pt[1]),expected):
   raise ValueError('keyword activation witness changed: '+nm)
 for scope,color in (('green_creature','G'),('red_creature','R'),('black_creature','B'),('blue_creature','U')):
  if o[TARGETS[scope]]['colors']!=[color]:raise ValueError('keyword witness color changed')
 for scope,subtype in (('beast_creature','Beast'),('human','Human')):
  if subtype not in o[TARGETS[scope]]['type_line'].split():raise ValueError('keyword witness subtype changed')
 for nm,fields in dict(SPELLS,**{'Robe of Mirrors':('{U}',ROBE),"Alexi's Cloak":CLOAK}).items():
  if nm not in registry or (o.get(nm,{}).get('mana_cost'),o.get(nm,{}).get('oracle_text'))!=fields:raise ValueError('keyword activation response witness changed: '+nm)

def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones'
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
  mana=[[10]*6,[10]*6];life=[20,20];setup=[];entities={};pre=[step('turn',player=owner,value=0)];actions=[]
  def add(nm,ref,player,zone='battlefield',state=None):
   item=card(nm,ref,player,zone)
   if state:item['state']=state
   setup.append(item);r=oracle[nm];creature='Creature' in r['type_line']
   entities[ref]=dict(name=nm,player=player,alive=zone=='battlefield',tapped=bool(state and 'tapped' in state),creature=creature,
    power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0)
   if zone=='hand':hands[player].append(nm)
  def pay(player,pool):mana[player]=[a-b for a,b in zip(mana[player],pool)]
  def move(ref,zone):
   e=entities[ref];e['alive']=False;(hands if zone=='hand' else graves)[e['player']].append(e['name'])
  def cast(ref,target=None,dialog=None):
   e=entities[ref];seq=([step('choose_target',player=e['player'],ref=target)] if target else [])+[step('cast',ref=ref,value=1)]
   if dialog is not None:seq.append(step('choose_dialog',player=e['player'],value=dialog))
   seq += [step('resolve'),step('assert_choices_consumed')];pay(e['player'],cost_from_oracle(oracle[e['name']]));hands[e['player']].remove(e['name'])
   if 'Instant' in oracle[e['name']]['type_line']:graves[e['player']].append(e['name'])
   else:e['alive']=True
   return seq
  casting=v in ('cast','cast_payment','insufficient_cast_mana','fresh_activation')
  add(name,'source',owner,'hand' if casting else 'battlefield',['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
  target_owner=owner if v=='friendly' or p['targets']=='own_creature' and v!='wrong_controller' else 1-owner
  recipient=TARGETS[p['targets']]
  negative=v in ('no_targets','wrong_target','wrong_controller','shrouded_targets')
  has_recipient=v!='no_targets'
  if has_recipient:
   if v=='wrong_target':recipient='Grizzly Bears' if p['targets']=='red_creature' else 'Hill Giant'
   add(recipient,'recipient',target_owner,state=['sick'] if p['grant']=='Haste' and v!='ready_recipient' else None)
  if not negative:add(TARGETS[p['targets']],'unselected',target_owner,state=['sick'] if p['grant']=='Haste' and v!='ready_recipient' else None)
  two=v=='two_sources'
  if two:add(name,'second',owner)
  response=v in ('source_bounced','target_bounced','target_shrouded_pending','untap_repeat')
  if response:add('Twiddle' if v=='untap_repeat' else "Alexi's Cloak" if v=='target_shrouded_pending' else 'Boomerang','response',owner,'hand')
  if v=='untap_repeat':
   pre.append(step('destroy',ref='unselected'));move('unselected','graveyard')
  if v in ('tax','tax_insufficient'):add('Suppression Field','taxer',1-owner,'hand');pre+= [step('turn',player=1-owner,value=0)]+cast('taxer')+[step('turn',player=owner,value=0)]
  if negative:
   add('Robe of Mirrors','source_robe',owner,'hand');pre+=cast('source_robe','source')
   if v=='shrouded_targets':add('Robe of Mirrors','target_robe',owner,'hand');pre+=cast('target_robe','recipient')
  if casting:
   if v in ('cast_payment','insufficient_cast_mana'):
    mana[owner]=list(p['mana']) if v=='cast_payment' else [0]*6;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   if v=='insufficient_cast_mana':pre += [step('cast',ref='source',value=0),step('assert_choices_consumed'),step('assert_stack',value=0)]
   else:pre+=cast('source')+[step('refresh')]
  if v=='opposing_turn':pre.append(step('turn',player=1-owner,value=0))
  cost=list(p['activation_mana']);cost[0]+=2*int(v in ('tax','tax_insufficient'))
  if v in ('payment','tax','tax_insufficient','insufficient_mana','wrong_color'):
   pool=list(cost) if v in ('payment','tax') else list(p['activation_mana']) if v=='tax_insufficient' else [0]*6
   if v=='wrong_color':
    pool=list(cost);color=next(c for c in range(1,6) if pool[c]);pool[1 if color!=1 else 2]+=pool[color];pool[color]=0
   mana[owner]=pool;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(pool)]
  legal=not negative and v not in ('insufficient_mana','wrong_color','tax_insufficient','insufficient_cast_mana') and not (p['tap'] and (v=='tapped' or v in ('sick','fresh_activation') and 'Haste' not in p['keywords']))
  def inspect(ref):
   e=entities[ref];s=[observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    s += [observe(ref,'tapped',int(e['tapped']))]
    if e['creature']:s += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness'])]
   return s
  def pending(source='source',target='recipient'):
   seq=[step('require_can_activate',ref=source,value=1),step('choose_target',player=owner,ref=target),step('activate',ref=source,value=1),step('assert_choices_consumed')]
   pay(owner,cost)
   if p['sacrifice']:move(source,'graveyard')
   elif p['tap']:entities[source]['tapped']=True
   seq+=inspect(source)
   if entities[target]['alive']:seq+=inspect(target)
   return seq
  changes={}
  def apply(target='recipient'):
   if entities[target]['alive']:changes[target]=True
  ready=0;pending_cut=0
  if v in ('cast','cast_payment','insufficient_cast_mana','fresh_without_grant'):pass
  elif not legal:actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_stack',value=0),step('assert_choices_consumed')]
  else:
   ready=len(actions);actions+=pending()+[step('assert_stack',value=1)];pending_cut=len(actions)
   if v in ('two_sources','stacked'):actions+=pending('second' if two else 'source','recipient')+[step('assert_stack',value=2)]
   if v in ('target_bounced','source_bounced','target_shrouded_pending'):
    target='source' if v=='source_bounced' else 'recipient';actions+=cast('response',target)
    if v!='target_shrouded_pending':move(target,'hand')
   if v in ('source_destroyed','target_destroyed'):
    target='source' if v=='source_destroyed' else 'recipient';actions += [step('destroy',ref=target)];move(target,'graveyard')
   actions += [step('resolve'),step('assert_choices_consumed')]
   fizzle=v in ('target_bounced','target_destroyed','target_shrouded_pending')
   if not fizzle:apply('recipient')
   if v in ('two_sources','stacked'):actions += [step('resolve'),step('assert_choices_consumed')];apply()
   if v=='repeat_denied':actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
   if v=='untap_repeat':
    actions+=cast('response','source',1)+[step('refresh'),observe('source','tapped',0)];entities['source']['tapped']=False
    actions+=pending()+[step('resolve'),step('assert_choices_consumed')];apply()
   actions+=inspect('recipient')
  # Prove the selected keyword through normal combat or actual target/destroy
  # rules. The receipt ledger tracks every death; base P/T alone is insufficient.
  expiry=v in ('cleanup','expired_probe') and p['grant']!='Haste'
  if expiry:actions += [step('cleanup'),step('refresh')]
  tested='recipient' in changes and entities.get('recipient',{}).get('alive')
  probe=tested or v=='fresh_without_grant'
  active=tested and not expiry and v!='unselected_probe'
  probe_ref='unselected' if v=='unselected_probe' else 'recipient'
  if probe:
   e=entities[probe_ref];grant=p['grant'];attacker=probe_ref;victim=None
   if grant=='Indestructible':
    actions += [step('destroy',ref=probe_ref),observe(probe_ref,'in_play',int(active))]
    if not active:move(probe_ref,'graveyard')
   elif grant=='Shroud':
    # A concrete target, rather than aggregate CAN_CAST with other legal targets.
    add('Unsummon','owner_probe',target_owner,'hand');add('Unsummon','enemy_probe',1-target_owner,'hand')
    actions += [step('can_target_creature',ref=ref,other=probe_ref,value=int(not active)) for ref in ('owner_probe','enemy_probe')]
   else:
    blocker=None
    if grant=='Flying':blocker='Grizzly Bears'
    elif grant in ('First strike','Double strike'):
     blocker='Scaled Wurm' if e['power']>=7 else 'Air Elemental' if e['power']>=4 else 'Hill Giant' if e['power']>=3 else 'Grizzly Bears' if e['toughness']>=2 else 'Elite Vanguard'
    elif grant=='Trample':blocker='Suntail Hawk'
    elif grant=='Deathtouch':blocker='Scaled Wurm'
    if blocker:add(blocker,'blocker',1-target_owner)
    actions += [step('turn',player=target_owner,value=0),step('begin_attackers')]
    can_attack=int(grant!='Haste' or active)
    actions += [step('attack',ref=probe_ref,value=can_attack),observe(probe_ref,'attacking',can_attack),observe(probe_ref,'tapped',int(can_attack and not (grant=='Vigilance' and active))),step('finish_attackers'),step('begin_blockers')]
    e['tapped']=bool(can_attack and not (grant=='Vigilance' and active))
    blocks=bool(blocker and not (grant=='Flying' and active))
    if blocker:actions += [step('block',ref='blocker',other=probe_ref,value=int(blocks)),observe('blocker','blocking_count',int(blocks))]
    actions += [step('advance_combat'),step('combat_damage'),step('resolve')]
    damage=e['power']*(2 if grant=='Double strike' and active else 1) if can_attack else 0
    dealt=damage if not blocks else max(0,damage-1) if grant=='Trample' and active else 0
    life[1-target_owner]-=dealt
    if grant=='Lifelink' and active:life[target_owner]+=damage
    if blocks:
     b=entities['blocker'];first=grant in ('First strike','Double strike') and active
     dead_blocker=e['power']>=b['toughness'] or grant=='Deathtouch' and active
     dead_recipient=not(first and dead_blocker) and b['power']>=e['toughness']
     if dead_blocker:move('blocker','graveyard')
     if dead_recipient:move(probe_ref,'graveyard')
     else:actions.append(observe(probe_ref,'damage',0 if first and dead_blocker else b['power']))
    actions += [step('end_combat'),step('assert_choices_consumed')]
  if v=='cleanup' and p['grant']=='Haste':actions += [step('cleanup'),step('refresh')]
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref in entities:checks+=inspect(ref)
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
  for player in (0,1):
   checks += [step('assert_life',player=player,value=life[player]),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
   checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
    checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
    if zone=='library':checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=registry[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=ready if v=='ready_snapshot' else pending_cut;tail=actions[cut:]+checks
   steps=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'TargetKeywordActivationRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='target_keyword_activation',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['blink identity','control changes','additional keywords on affected targets','self-targeting','multiple recipients','Haste expiry after changing turns','source passive keyword mechanics'])))
 return result
