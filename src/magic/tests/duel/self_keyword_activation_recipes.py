"""Explicit fixed self-keyword abilities, tested through real engine actions."""
import copy
from collections import Counter
from creature_recipes import classify
from passive_keyword_creature_recipes import split
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from value_activation_recipes import SPELLS
from defensive_creature_recipes import SPELLS as RESPONSES

GRANTS=('Flying','First strike','Deathtouch','Lifelink','Vigilance','Reach','Shroud','Hexproof','Indestructible','Menace')
REMINDERS={
 'Shroud':(" (It can't be the target of spells or abilities.)",),
 'Hexproof':(" (It can't be the target of spells or abilities your opponents control.)",),
 'First strike':(' (It deals combat damage before creatures without first strike.)',),
 'Deathtouch':(' (Any amount of damage it deals to a creature is enough to destroy it.)',),
 'Lifelink':(' (Damage dealt by this creature also causes you to gain that much life.)',),
 'Reach':(' (It can block creatures with flying.)',),
 'Indestructible':(' (Damage and effects that say "destroy" don\'t destroy it.)',),
 'Vigilance and lifelink':(" (Attacking doesn't cause it to tap. Damage dealt by it also causes you to gain that much life.)",),
}
ROBE=('Enchant creature (Target a creature as you cast this. This card enters attached to that creature.)\n'
      "Enchanted creature has shroud. (It can't be the target of spells or abilities.)")
WITNESSES={'Grizzly Bears':('2','2',''), 'Llanowar Elves':('1','1','{T}: Add {G}.'),
 'Scaled Wurm':('7','6',''), 'Suntail Hawk':('1','1','Flying'),
 'Giant Spider':('2','4','Reach (This creature can block creatures with flying.)')}

def spec(d,r):
 from recipes import cost_from_oracle
 if not r or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d or r.get('type_line','').startswith('Token '):
  raise ValueError('whole Oracle pin and explicit self-keyword variants required')
 abilities,plain=split(r);base=classify(plain);p=d.get('params')
 if set(base['keywords'])-{'Flying','Defender'}:
  raise ValueError('source keyword combinations need separately reviewed combat expectations')
 if not isinstance(p,dict) or set(p)!={'keywords','grants','mana_cost'} or p['keywords']!=base['keywords']:
  raise ValueError('explicit unconditional source keywords, grants and mana cost required')
 if not isinstance(p['grants'],list) or not p['grants'] or any(not isinstance(k,str) or k not in GRANTS for k in p['grants']) or len(set(p['grants']))!=len(p['grants']):
  raise ValueError('reviewed unique fixed self keywords required')
 if len(p['grants'])>1 and p['grants']!=['Vigilance','Lifelink']:raise ValueError('unreviewed compound grant')
 if not isinstance(p['mana_cost'],str):raise ValueError('structured ordinary activation mana required')
 cost=cost_from_oracle({'mana_cost':p['mana_cost']});phrase=' and '.join(k.lower() for k in p['grants'])
 clause=f'{p["mana_cost"]}: This creature gains {phrase} until end of turn.'
 reminder_key='Vigilance and lifelink' if len(p['grants'])>1 else p['grants'][0]
 if len(abilities)!=1 or abilities[0] not in {clause+s for s in ('',)+REMINDERS.get(reminder_key,())}:
  raise ValueError('only a single whole-card ordinary fixed self-grant ability is reviewed')
 if base['power']==0 and set(p['grants'])&{'First strike','Deathtouch','Lifelink','Menace'}:
  raise ValueError('damage keywords require a positive-power witness')
 return dict(p,family='self_keyword_activation',mana=base['mana'],activation_mana=cost,
  power=base['power'],toughness=base['toughness'],legendary='Legendary' in r['type_line'])

def variants(p):
 v=['effect','payment','tapped','sick','fresh_activation','opposing_turn','cast','cast_payment',
  'inactive_probe','expired_probe','shrouded_source','source_bounced_pending','source_destroyed_pending',
  'stacked','repeat','unaffected_witness','tax','tax_insufficient','bare_zones',
  'ready_snapshot','pending_snapshot','resolved_snapshot']
 if sum(p['mana']):v.append('insufficient_cast_mana')
 if sum(p['activation_mana']):v.append('insufficient_mana')
 if any(p['activation_mana'][1:]):v.append('wrong_color')
 if not p['legendary']:v.append('two_sources')
 if 'Flying' in p['grants']:v.append('reach_blocker')
 if 'Menace' in p['grants']:v.append('two_blockers')
 if 'Indestructible' in p['grants']:v+=['lethal_damage','zero_toughness','exile','bounce_after_resolution']
 return [x+'_'+str(owner) for owner in (0,1) for x in v]

def validate_witnesses(o,registry):
 for nm,fields in WITNESSES.items():
  r=o.get(nm,{})
  if nm not in registry or 'Creature' not in r.get('type_line','') or (r.get('power'),r.get('toughness'),r.get('oracle_text'))!=fields:
   raise ValueError('self-keyword combat witness changed: '+nm)
 spells=dict(SPELLS,**{'Robe of Mirrors':('{U}',ROBE),
  'Grasp of Darkness':('{B}{B}','Target creature gets -4/-4 until end of turn.')})
 for key in ('damage','exile','bounce'):nm,_,mana,text=RESPONSES[key];spells[nm]=(mana,text)
 for nm,fields in spells.items():
  if nm not in registry or (o.get(nm,{}).get('mana_cost'),o.get(nm,{}).get('oracle_text'))!=fields:
   raise ValueError('self-keyword response witness changed: '+nm)

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
    power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,damage=0)
   if zone=='hand':hands[player].append(nm)
  def move(ref,zone):
   e=entities[ref];e['alive']=False;{'hand':hands,'graveyard':graves,'exile':exiles}[zone][e['player']].append(e['name'])
  def pay(player,pool):mana[player]=[a-b for a,b in zip(mana[player],pool)]
  def cast(ref,target=None):
   e=entities[ref];seq=([step('choose_target',player=e['player'],ref=target)] if target else [])+[step('cast',ref=ref,value=1),step('resolve'),step('assert_choices_consumed')]
   pay(e['player'],cost_from_oracle(oracle[e['name']]));hands[e['player']].remove(e['name'])
   if 'Instant' in oracle[e['name']]['type_line'] or 'Sorcery' in oracle[e['name']]['type_line']:graves[e['player']].append(e['name'])
   else:e['alive']=True
   return seq
  casting=v in ('cast','cast_payment','fresh_activation','insufficient_cast_mana')
  add(name,'source',owner,'hand' if casting else 'battlefield',['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
  add('Grizzly Bears','unselected',owner)
  if v=='two_sources':add(name,'second',owner)
  if v=='source_bounced_pending':add('Boomerang','response',owner,'hand')
  if v=='shrouded_source':add('Robe of Mirrors','robe',owner,'hand');pre+=cast('robe','source')
  if v in ('tax','tax_insufficient'):
   add('Suppression Field','taxer',owner,'hand');pre+=cast('taxer')
  if casting:
   if v in ('cast_payment','insufficient_cast_mana'):
    mana[owner]=list(p['mana']) if v=='cast_payment' else [0]*6;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   if v=='insufficient_cast_mana':pre += [step('cast',ref='source',value=0),step('assert_stack',value=0),step('assert_choices_consumed')]
   else:pre+=cast('source')+[step('refresh')]
  if v=='opposing_turn':pre.append(step('turn',player=1-owner,value=0))
  cost=list(p['activation_mana']);cost[0]+=2*int(v in ('tax','tax_insufficient'))
  if v in ('payment','insufficient_mana','wrong_color','tax','tax_insufficient'):
   pool=list(cost) if v in ('payment','tax') else list(p['activation_mana']) if v=='tax_insufficient' else [0]*6
   if v=='wrong_color':
    pool=list(cost);color=next(c for c in range(1,6) if pool[c]);pool[1 if color!=1 else 2]+=pool[color];pool[color]=0
   mana[owner]=pool;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(pool)]
  def inspect(ref):
   e=entities[ref];seq=[observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    seq += [observe(ref,'tapped',int(e['tapped']))]
    if e['creature']:seq += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness']),observe(ref,'damage',e['damage'])]
   return seq
  def pending(ref='source'):
   seq=[step('require_can_activate',ref=ref,value=1),step('activate',ref=ref,value=1),step('assert_choices_consumed')]+inspect(ref)
   pay(owner,cost);return seq
  active=False;pending_cut=0
  if v in ('cast','cast_payment','insufficient_cast_mana','inactive_probe'):pass
  elif v in ('insufficient_mana','wrong_color','tax_insufficient'):
   actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_stack',value=0),step('assert_choices_consumed')]
  else:
   actions+=pending()+[step('assert_stack',value=1)];pending_cut=len(actions)
   if v in ('stacked','two_sources'):actions+=pending('second' if v=='two_sources' else 'source')+[step('assert_stack',value=2)]
   if v=='source_bounced_pending':actions+=cast('response','source');move('source','hand')
   if v=='source_destroyed_pending':actions.append(step('destroy',ref='source'));move('source','graveyard')
   actions += [step('resolve'),step('assert_choices_consumed')];active=entities['source']['alive']
   if v in ('stacked','two_sources'):actions += [step('resolve'),step('assert_choices_consumed')]
   if v=='repeat':actions+=pending()+[step('resolve'),step('assert_choices_consumed')]
  if v=='expired_probe':actions += [step('cleanup'),step('refresh')];active=False
  probe=entities['source']['alive'] and v not in ('cast','cast_payment','fresh_activation','insufficient_cast_mana','insufficient_mana','wrong_color','tax_insufficient')
  probe_ref='unselected' if v=='unaffected_witness' else 'source';e=entities[probe_ref]
  keys=set(p['keywords'] if probe_ref=='source' else [])|set(p['grants'] if active and probe_ref=='source' else [])
  if v=='shrouded_source' and probe_ref=='source':keys.add('Shroud')
  grant=p['grants'][0]
  if probe and v in ('lethal_damage','zero_toughness','exile','bounce_after_resolution'):
   nm='Flame Slash' if v=='lethal_damage' else 'Grasp of Darkness' if v=='zero_toughness' else 'Swords to Plowshares' if v=='exile' else 'Unsummon'
   count=(p['toughness']+3)//4 if v=='lethal_damage' else p['toughness']//4+1 if v=='zero_toughness' else 1
   for i in range(count):
    ref='spell_'+str(i);add(nm,ref,owner,'hand');actions += cast(ref,'source')
   if v=='lethal_damage':e['damage']=4*count
   elif v=='exile':life[owner]+=e['power'];move('source','exile')
   else:move('source','graveyard' if v=='zero_toughness' else 'hand')
  elif probe and grant in ('Shroud','Hexproof'):
   for ref,player in (('owner_probe',owner),('enemy_probe',1-owner)):
    add('Unsummon',ref,player,'hand');legal='Shroud' not in keys and not(player!=e['player'] and 'Hexproof' in keys)
    actions += [step('can_target_creature',ref=ref,other=probe_ref,value=int(legal))]
  elif probe and grant=='Indestructible':
   survives='Indestructible' in keys;actions += [step('destroy',ref=probe_ref),observe(probe_ref,'in_play',int(survives))]
   if not survives:move(probe_ref,'graveyard')
  elif probe:
   defending=grant in ('First strike','Deathtouch','Reach')
   witness='Llanowar Elves' if grant=='First strike' else 'Scaled Wurm' if grant=='Deathtouch' else 'Suntail Hawk' if grant in ('Reach','Menace') else 'Giant Spider' if v=='reach_blocker' else 'Grizzly Bears' if grant=='Flying' else None
   if witness:add(witness,'witness',1-e['player'])
   if v=='two_blockers':add('Suntail Hawk','second_blocker',1-e['player'])
   turn=1-e['player'] if defending else e['player'];attacker='witness' if defending else probe_ref;blocker=probe_ref if defending else 'witness'
   can_attack=defending or not e['tapped'] and 'Defender' not in keys and (v!='sick' or 'Haste' in keys)
   actions += [step('turn',player=turn,value=0),step('begin_attackers'),step('attack',ref=attacker,value=int(can_attack))]
   if defending:entities['witness']['tapped']=True
   if not defending:e['tapped']=bool(e['tapped'] or can_attack and 'Vigilance' not in keys);actions+= [observe(probe_ref,'tapped',int(e['tapped']))]
   actions += [step('finish_attackers'),step('begin_blockers')]
   blocks=False
   if witness and can_attack:
    blocks=not e['tapped'] and ('Flying' in keys or 'Reach' in keys or grant!='Reach') if defending else 'Flying' not in keys or witness in ('Giant Spider','Suntail Hawk')
    actions += [step('block',ref=blocker,other=attacker,value=int(blocks))]
    if v=='two_blockers':actions += [step('block',ref='second_blocker',other=attacker,value=1)]
   actions += [step('advance_combat')]
   if grant=='Menace' and blocks and 'Menace' in keys and v!='two_blockers':blocks=False
   if witness and can_attack:actions += [observe(blocker,'blocking_count',int(blocks))]
   if v=='two_blockers':actions += [observe(probe_ref,'blocked',1),observe('second_blocker','blocking_count',1)]
   else:
    actions += [step('combat_damage'),step('resolve'),step('assert_choices_consumed')]
    if defending:
     w=entities['witness'];first=bool(keys&{'First strike','Double strike'});dead_w=blocks and (e['power']>=w['toughness'] or 'Deathtouch' in keys and e['power']>0)
     dealt=0 if not blocks or first and dead_w else w['power'];dead_e=blocks and dealt>=e['toughness']
     if blocks and 'Lifelink' in keys:life[e['player']]+=e['power']
     if not blocks:life[e['player']]-=w['power']
     if dead_w:move('witness','graveyard')
     else:w['damage']=e['power'] if blocks else 0
     if dead_e:move(probe_ref,'graveyard')
     else:e['damage']=dealt
    elif can_attack:
     if not blocks:
      dealt=e['power']*(2 if 'Double strike' in keys else 1);life[1-e['player']]-=dealt
      if 'Lifelink' in keys:life[e['player']]+=dealt
     else:
      w=entities['witness'];first=bool(keys&{'First strike','Double strike'});dead_w=e['power']>=w['toughness'] or 'Deathtouch' in keys and e['power']>0
      dealt=0 if first and dead_w else w['power'];dead_e=dealt>=e['toughness']
      if 'Lifelink' in keys:life[e['player']]+=e['power']
      if dead_w:move('witness','graveyard')
      else:w['damage']=e['power']*(2 if 'Double strike' in keys else 1)
      if dead_e:move(probe_ref,'graveyard')
      else:e['damage']=dealt
    actions += [step('end_combat'),step('assert_choices_consumed')]
  # An attached Aura goes to its owner's graveyard when its creature leaves.
  if v=='shrouded_source' and not entities['source']['alive']:move('robe','graveyard')
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
   cut=0 if v=='ready_snapshot' else pending_cut;tail=actions[cut:]+checks
   steps=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'SelfKeywordActivationRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='self_keyword_activation',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['control changes','blink identity','additional keyword interactions','source passive keyword mechanics','multiple simultaneous damage assignments'])))
 return result
