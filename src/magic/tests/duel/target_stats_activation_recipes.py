"""Pinned fixed targeted stat abilities: real costs, combat, stack and cleanup."""
import copy,re
from collections import Counter
from creature_recipes import classify
from passive_keyword_creature_recipes import split
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from value_activation_recipes import SPELLS

SCOPES={
 'creature':'creature', 'flying_creature':'creature with flying',
 'green_creature':'green creature', 'dwarf_creature':'Dwarf creature',
 'soldier_creature':'Soldier creature',
 'attacking_creature':'attacking creature', 'blocking_creature':'blocking creature',
 'combat_creature':'attacking or blocking creature', 'attacking_human':'attacking Human',
}
TARGETS={'creature':'Scaled Wurm','flying_creature':'Air Elemental','green_creature':'Scaled Wurm',
 'dwarf_creature':'Dwarven Warriors','soldier_creature':'Elite Vanguard','attacking_creature':'Scaled Wurm',
 'blocking_creature':'Scaled Wurm','combat_creature':'Scaled Wurm','attacking_human':'Elite Vanguard'}
STATS={'Scaled Wurm':(7,6),'Dwarven Warriors':(1,1),'Elite Vanguard':(2,1),
 'Air Elemental':(4,4),'Suntail Hawk':(1,1),
 'Muck Rats':(1,1),'Wall of Stone':(0,8),'Ornithopter':(0,2),'Hill Giant':(3,3)}
ROBE=('\n'.join(('Enchant creature (Target a creature as you cast this. This card enters attached to that creature.)',
 "Enchanted creature has shroud. (It can't be the target of spells or abilities.)")))
CLOAK=('{1}{U}',"Flash\nEnchant creature\nEnchanted creature has shroud. (It can't be the target of spells or abilities.)")

def spec(d,r):
 from recipes import cost_from_oracle
 if not r or r.get('type_line','').startswith('Token ') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:
  raise ValueError('target stat activation requires a whole Oracle pin and explicit variants')
 abilities,plain=split(r);base=classify(plain);p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'keywords','power','toughness','targets','mana_cost','tap','sacrifice'}:
  raise ValueError('explicit source keywords, stat changes, scope and all costs required')
 if p['keywords']!=base['keywords'] or any(type(p[k]) is not int or not -4<=p[k]<=4 for k in ('power','toughness')) or not (p['power'] or p['toughness']):
  raise ValueError('reviewed nonzero bounded signed stat changes and keywords required')
 if not isinstance(p['targets'],str) or p['targets'] not in SCOPES or any(type(p[k]) is not bool for k in ('tap','sacrifice')) or not isinstance(p['mana_cost'],str):
  raise ValueError('reviewed target scope, boolean costs and structured mana required')
 activation=[0]*6 if p['mana_cost']=='' else cost_from_oracle({'mana_cost':p['mana_cost']})
 cost=', '.join(([p['mana_cost']] if p['mana_cost'] else [])+(['{T}'] if p['tap'] else [])+(['Sacrifice this creature'] if p['sacrifice'] else []))
 if not cost:raise ValueError('nonempty activation cost required')
 def signed(n):return (f'{n:+d}',) if n else ('+0','-0')
 expected={f'{cost}: Target {SCOPES[p["targets"]]} gets {a}/{b} until end of turn.' for a in signed(p['power']) for b in signed(p['toughness'])}
 if len(abilities)!=1 or abilities[0] not in expected:
  raise ValueError('only a single whole-card fixed target stat ability is reviewed')
 return dict(p,family='target_stats_activation',activation_mana=activation,mana=base['mana'],
  base_power=base['power'],base_toughness=base['toughness'],legendary='Legendary' in r['type_line'])

def variants(p):
 common=['effect','friendly','payment','tapped','sick','fresh_activation','opposing_turn',
  'cast','cast_payment','no_targets','shrouded_targets','cleanup',
  'target_bounced','target_destroyed','target_shrouded_pending','tax','tax_insufficient',
  'bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot','combat_damage']
 if sum(p['mana']):common.append('insufficient_cast_mana')
 if sum(p['activation_mana']):common.append('insufficient_mana')
 if any(p['activation_mana'][1:]):common.append('wrong_color')
 if p['targets'] not in ('creature','attacking_creature','blocking_creature','combat_creature'):common.append('wrong_target')
 if p['targets'] in ('attacking_creature','blocking_creature','combat_creature','attacking_human'):common+=['wrong_role','removed_from_combat']
 if p['targets']=='combat_creature':common.append('blocking_target')
 if p['power']<0:common.append('negative_power')
 if p['toughness']<0:common.append('lethal')
 if not p['sacrifice']:
  common+=['source_bounced','source_destroyed']
  if p['tap']:common+=['repeat_denied','untap_repeat']
  else:common+=['stacked']
 restricted=p['targets'] in ('attacking_creature','blocking_creature','combat_creature','attacking_human')
 if not p['legendary']:common+=['two_sources']
 if not restricted and (not p['tap'] and not p['sacrifice'] or not p['legendary']):common+=['two_targets']
 return [v+'_'+str(owner) for owner in (0,1) for v in common]

def validate_witnesses(o,registry):
 for nm,pt in STATS.items():
  r=o.get(nm,{})
  if nm not in registry or r.get('layout')!='normal' or r.get('card_faces') or 'Creature' not in r.get('type_line','') or (r.get('power'),r.get('toughness'))!=tuple(str(n) for n in pt):
   raise ValueError('target stat witness characteristics changed: '+nm)
 for nm,word in (('Dwarven Warriors','Dwarf'),('Elite Vanguard','Human'),('Elite Vanguard','Soldier')):
  if word not in o[nm]['type_line'].split():raise ValueError('target stat subtype witness changed: '+nm)
 for nm,text in (('Scaled Wurm',''),('Elite Vanguard',''),('Muck Rats',''),('Hill Giant',''),('Air Elemental','Flying'),('Suntail Hawk','Flying'),('Ornithopter','Flying'),('Wall of Stone',"Defender (This creature can't attack.)"),('Dwarven Warriors',"{T}: Target creature with power 2 or less can't be blocked this turn.")):
  if o[nm]['oracle_text']!=text:raise ValueError('target stat witness rules changed: '+nm)
 if o['Scaled Wurm']['colors']!=['G'] or o['Hill Giant']['colors']!=['R']:raise ValueError('target stat color witness changed')
 for nm,fields in dict(SPELLS,**{'Robe of Mirrors':('{U}',ROBE),"Alexi's Cloak":CLOAK}).items():
  if nm not in registry or (o.get(nm,{}).get('mana_cost'),o.get(nm,{}).get('oracle_text'))!=fields:raise ValueError('target stat response witness changed: '+nm)

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
  target_owner=owner if v=='friendly' else 1-owner
  recipient=TARGETS[p['targets']]
  if v=='negative_power':recipient='Ornithopter' if p['targets']=='flying_creature' or p['targets']=='attacking_creature' else 'Wall of Stone'
  if v=='lethal':recipient='Suntail Hawk' if p['targets']=='flying_creature' else 'Muck Rats'
  restricted_role=p['targets'] in ('attacking_creature','blocking_creature','combat_creature','attacking_human')
  blocking=p['targets']=='blocking_creature' or v=='blocking_target'
  negative=v in ('no_targets','wrong_target','wrong_role','shrouded_targets')
  has_recipient=v!='no_targets'
  if has_recipient:
   if v=='wrong_target':recipient='Hill Giant'
   add(recipient,'recipient',target_owner)
  if not negative:add(TARGETS[p['targets']],'unselected',target_owner)
  two=v in ('two_sources','two_targets') and (v=='two_sources' or p['tap'] or p['sacrifice'])
  if two:add(name,'second',owner)
  if v=='two_targets':add(TARGETS[p['targets']],'other_target',target_owner)
  if blocking and has_recipient and v!='wrong_role':add('Hill Giant','attacker',1-target_owner)
  response=v in ('source_bounced','target_bounced','target_shrouded_pending','untap_repeat')
  if response:add('Twiddle' if v=='untap_repeat' else "Alexi's Cloak" if v=='target_shrouded_pending' else 'Boomerang','response',owner,'hand')
  if v in ('tax','tax_insufficient'):add('Suppression Field','taxer',1-owner,'hand');pre+= [step('turn',player=1-owner,value=0)]+cast('taxer')+[step('turn',player=owner,value=0)]
  if negative and v!='wrong_role':
   add('Robe of Mirrors','source_robe',owner,'hand');pre+=cast('source_robe','source')
   if v=='shrouded_targets':add('Robe of Mirrors','target_robe',owner,'hand');pre+=cast('target_robe','recipient')
  if casting:
   if v in ('cast_payment','insufficient_cast_mana'):
    mana[owner]=list(p['mana']) if v=='cast_payment' else [0]*6;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   if v=='insufficient_cast_mana':pre += [step('cast',ref='source',value=0),step('assert_choices_consumed'),step('assert_stack',value=0)]
   else:pre+=cast('source')+[step('refresh'),observe('source','sick',int('Haste' not in p['keywords']))]
  # Legal declarations establish attacking/blocking restrictions. No raw event
  # injection stands in for phase progression or simultaneous blocker triggers.
  combat=has_recipient and (not negative or v=='wrong_target' and p['targets']=='attacking_human') and (restricted_role or v=='combat_damage')
  if combat:
   attacker='attacker' if blocking else 'recipient';player=entities[attacker]['player']
   pre += [step('turn',player=player,value=0),step('begin_attackers'),step('attack',ref=attacker,value=1),step('finish_attackers'),step('begin_blockers')]
   entities[attacker]['tapped']=True
   if blocking:pre += [step('block',ref='recipient',other=attacker,value=1)]
   pre += [step('advance_combat')]
  elif v=='opposing_turn':pre.append(step('turn',player=1-owner,value=0))
  if v=='two_targets' and restricted_role:
   # Additional independent combat targets need a dedicated declaration board.
   raise ValueError('multiple restricted combat targets are not reviewed')
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
   e=entities[target]
   if not e['alive']:return
   e['power']+=p['power'];e['toughness']+=p['toughness'];changes[target]=changes.get(target,0)+1
   if e['toughness']<=0:move(target,'graveyard')
  ready=0;pending_cut=0
  if v in ('cast','cast_payment','insufficient_cast_mana'):pass
  elif not legal:actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_stack',value=0),step('assert_choices_consumed')]
  else:
   ready=len(actions);actions+=pending()+[step('assert_stack',value=1)];pending_cut=len(actions)
   if v in ('two_sources','stacked','two_targets'):actions+=pending('second' if two else 'source','other_target' if v=='two_targets' else 'recipient')+[step('assert_stack',value=2)]
   if v in ('target_bounced','source_bounced','target_shrouded_pending'):
    target='source' if v=='source_bounced' else 'recipient';actions+=cast('response',target)
    if v!='target_shrouded_pending':move(target,'hand')
   if v in ('source_destroyed','target_destroyed'):
    target='source' if v=='source_destroyed' else 'recipient';actions += [step('destroy',ref=target)];move(target,'graveyard')
   if v=='removed_from_combat':actions += [step('remove_from_combat',ref='recipient')]
   actions += [step('resolve'),step('assert_choices_consumed')]
   fizzle=v in ('target_bounced','target_destroyed','target_shrouded_pending','removed_from_combat')
   if not fizzle:apply('other_target' if v=='two_targets' else 'recipient')
   if v in ('two_sources','stacked','two_targets'):actions += [step('resolve'),step('assert_choices_consumed')];apply()
   if v=='repeat_denied':actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
   if v=='untap_repeat':
    actions+=cast('response','source',1)+[step('refresh'),observe('source','tapped',0)];entities['source']['tapped']=False
    actions+=pending()+[step('resolve'),step('assert_choices_consumed')];apply()
   actions+=inspect('recipient')
   if v=='combat_damage':
    actions += [step('combat_damage')]
    if blocking:
     victim=entities['recipient'];attacker=entities['attacker']
     if max(0,victim['power'])>=attacker['toughness']:move('attacker','graveyard')
     if max(0,attacker['power'])>=victim['toughness']:move('recipient','graveyard')
    else:life[1-target_owner]-=max(0,entities['recipient']['power'])
    actions += [step('resolve'),step('end_combat')]
   if v in ('cleanup','combat_damage'):
    for ref,count in changes.items():
     e=entities[ref]
     if e['alive']:actions+=inspect(ref);e['power']-=count*p['power'];e['toughness']-=count*p['toughness']
    actions += [step('cleanup'),step('refresh')]
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
  result.append(dict(name=f'TargetStatsActivationRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='target_stats_activation',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['blink identity','control changes','multiple restricted combat targets','additional keywords on affected targets','self-targeting'])))
 return result
