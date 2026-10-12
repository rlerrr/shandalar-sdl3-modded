"""Conditional self keywords checked by legal blocks and actual combat damage.

Reuse the continuous-stat fixture builder for real artifact/graveyard counts.
The combat model below uses pinned Oracle characteristics, never card handlers.
"""
import copy,re
from conditional_stats_recipes import expand as stat_expand, counts, variants as stat_variants
from conditional_stats_recipes import validate_witnesses as stat_witnesses
from creature_recipes import REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS

GRANTS={'Flying','Double strike','Trample'}
STATIC={'Vigilance'}
WITNESSES={
 'Grizzly Bears':('2','2',''),
 'Llanowar Elves':('1','1','{T}: Add {G}.'),
 'Scaled Wurm':('7','6',''),
 'Suntail Hawk':('1','1','Flying'),
 'Giant Spider':('2','4','Reach (This creature can block creatures with flying.)'),
 'Tundra Wolves':('1','1','First strike (This creature deals combat damage before creatures without first strike.)'),
}

def spec(d,r):
 from recipes import cost_from_oracle
 if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or
     r.get('type_line','').startswith('Token ') or 'Creature' not in r.get('type_line','') or
     d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):raise ValueError('whole normal creature Oracle pin required')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'keywords','condition','power','toughness','grant'}:raise ValueError('explicit condition, bonus, grant and static keywords required')
 if (not isinstance(p['keywords'],list) or any(not isinstance(k,str) or k not in STATIC for k in p['keywords']) or len(set(p['keywords']))!=len(p['keywords']) or
     not isinstance(p['grant'],str) or p['grant'] not in GRANTS or p['condition'] not in ('metalcraft','threshold') or
     any(type(p[k]) is not int or not 0<=p[k]<=7 for k in ('power','toughness'))):raise ValueError('reviewed conditional self keyword and bounded bonus required')
 if bool(p['power'])!=bool(p['toughness']):raise ValueError('only pure keywords or positive paired bonuses reviewed')
 for k in ('power','toughness'):
  if not re.fullmatch(r'\d+',r.get(k,'')) or not 1<=int(r[k])<=20:raise ValueError('positive numeric Oracle characteristics required')
 end='you control three or more artifacts' if p['condition']=='metalcraft' else 'there are seven or more cards in your graveyard'
 clause=(f'This creature has {p["grant"].lower()} as long as {end}.' if not p['power'] else
         f'As long as {end}, this creature gets +{p["power"]}/+{p["toughness"]} and has {p["grant"].lower()}.')
 clause=p['condition'].capitalize()+' — '+clause
 keys=[];found=0
 for line in r['oracle_text'].splitlines():
  if line==clause:found+=1;continue
  text=line
  for reminder in re.findall(r'\([^\n]*?\)',text):
   if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed reminder')
   text=text.replace(reminder,'')
  parts=[x.strip().capitalize() for x in text.split(',')]
  if any(x not in STATIC for x in parts):raise ValueError('additional effects require dedicated tests')
  keys+=parts
 if found!=1 or sorted(keys)!=p['keywords']:raise ValueError('parameters differ from whole Oracle text')
 return dict(p,family='conditional_keyword',base_power=int(r['power']),base_toughness=int(r['toughness']),mana=cost_from_oracle(r),artifact='Artifact' in r['type_line'])

def variants(p):
 # Stat-only damage probes assume unconditional flying. Instead check these
 # grants at every count through combat, including real count transitions.
 v=[x for x in stat_variants(p) if not x.startswith(('combat_','damage_then_decrease_'))]
 for owner in (0,1):
  for n in counts(p):
   v += [f'{probe}_{n}_{owner}' for probe in ('unblocked','reach_block','flying_block') if p['grant']=='Flying']
   v += [f'{probe}_{n}_{owner}' for probe in ('unblocked','large_block','first_strike_block') if p['grant']=='Double strike']
   v += [f'{probe}_{n}_{owner}' for probe in ('unblocked','large_block') if p['grant']=='Trample']
 return v

def validate_witnesses(o,rows):
 stat_witnesses(o,rows)
 for name,fields in WITNESSES.items():
  r=o.get(name,{})
  if name not in rows or (r.get('power'),r.get('toughness'),r.get('oracle_text'))!=fields:raise ValueError('conditional-keyword combat witness changed: '+name)

def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import card,step,observe
 result=[];artifact_ids={row['csvid'] for nm,row in registry.items() if 'Artifact' in oracle.get(nm,{}).get('type_line','')}
 for variant in selected:
  parts=variant.rsplit('_',2);owner=int(parts[-1]);indexed=len(parts)==3 and parts[-2].isdigit();v=parts[0] if indexed else variant.rsplit('_',1)[0]
  probe=v if v in ('unblocked','reach_block','flying_block','large_block','first_strike_block') else 'ground_block'
  base_variant='effect_'+parts[-2]+'_'+parts[-1] if probe==v else variant
  snapshot=v=='snapshot'
  if snapshot:base_variant=base_variant.replace('snapshot_','effect_',1)
  c=stat_expand(name,csvid,d,p,[base_variant],registry,oracle)[0]
  # The shared builder finishes with exact state assertions. Keep its legal
  # setup actions and expected zones, then append this family's combat probe.
  cut=max(i for i,x in enumerate(c['steps']) if x==step('assert_stack',value=0))
  pre=c['steps'][:cut];checks=copy.deepcopy(c['steps'][cut:]);combat=[]
  def value(ref,prop):return next(x['value'] for x in checks if x.get('ref')==ref and x.get('property')==prop)
  def change_card(ref,prop,val):
   found=next((x for x in checks if x.get('ref')==ref and x.get('property')==prop),None)
   if found is not None:found['value']=val
   else:checks.append(observe(ref,prop,val))
  def bump(zone,player,nm,delta):
   for x in checks:
    if x['op']=='assert_zone' and x['zone']==zone and x['player']==player:x['value']+=delta
   ident=registry[nm]['csvid']
   found=next((x for x in checks if x['op']=='assert_zone_multiplicity' and x['zone']==zone and x['player']==player and x['index']==ident),None)
   if found:found['value']+=delta
   else:checks.append(step('assert_zone_multiplicity',player=player,zone=zone,index=ident,value=delta))
  def life_loss(pl,amount):
   next(x for x in checks if x['op']=='assert_life' and x['player']==pl)['value']-=amount
  if value('source','in_play'):
   if v in ('cast','payment'):
    # None of the reviewed cards has haste. Exercise the turn gate, rather
    # than overriding the summoning-sickness bit on a just-cast creature.
    pre += [step('begin_attackers'),step('attack',ref='source',value=0),step('advance_turn'),step('advance_turn')]
   active=(sum(x['value'] for x in checks if x['op']=='assert_zone_multiplicity' and x['zone']=='battlefield' and x['player']==owner and
               x['index'] in artifact_ids)>=3 if p['condition']=='metalcraft' else
           next(x['value'] for x in checks if x['op']=='assert_zone' and x['zone']=='graveyard' and x['player']==owner)>=7)
   keys=set(p['keywords'])|({p['grant']} if active else set());power=value('source','power');toughness=value('source','toughness')
   witness=None if probe=='unblocked' else 'Giant Spider' if probe=='reach_block' else 'Suntail Hawk' if probe=='flying_block' else 'Scaled Wurm' if probe=='large_block' else 'Tundra Wolves' if probe=='first_strike_block' else 'Grizzly Bears'
   if witness:
    c['setup'].append(card(witness,'combat_witness',1-owner,'battlefield'));bump('battlefield',1-owner,witness,1)
    # Include explicit absences in all unordered zones, including ownership.
    for pl in (0,1):
     for zone in ('battlefield','hand','graveyard','exile'):bump(zone,pl,witness,0)
   blocks=bool(witness and ('Flying' not in keys or probe in ('reach_block','flying_block')))
   tapped='Vigilance' not in keys
   combat += [step('turn',player=owner,value=0),step('begin_attackers'),step('attack',ref='source',value=1),observe('source','tapped',int(tapped)),step('finish_attackers'),step('begin_blockers')]
   if witness:combat.append(step('block',ref='combat_witness',other='source',value=int(blocks)))
   combat += [step('advance_combat')]
   if witness:combat.append(observe('combat_witness','blocking_count',int(blocks)))
   combat += [step('combat_damage'),step('resolve'),step('assert_choices_consumed')]
   source_damage=value('source','damage');wdamage=0;dead_source=dead_witness=False;dealt=0
   if not blocks:dealt=power*(2 if 'Double strike' in keys else 1)
   else:
    wp=int(oracle[witness]['power']);wt=int(oracle[witness]['toughness']);wfirst=probe=='first_strike_block';double='Double strike' in keys
    if double or wfirst:
     if double:wdamage+=power
     if wfirst:source_damage+=wp
     dead_witness=wdamage>=wt;dead_source=source_damage>=toughness
    # Simultaneous regular combat damage is calculated from survivors of the
    # first-strike step; double strike deals damage in both steps.
    if not dead_source:
     if not dead_witness:wdamage+=power
     if 'Trample' in keys:dealt=max(0,power-max(0,wt-(wdamage-power)))
    if not dead_witness and not wfirst:source_damage+=wp
    dead_witness=wdamage>=wt;dead_source=source_damage>=toughness
   life_loss(1-owner,dealt);change_card('source','tapped',int(tapped))
   if dead_source:
    change_card('source','in_play',0);checks=[x for x in checks if not(x.get('ref')=='source' and x.get('property')!='in_play')];bump('battlefield',owner,name,-1);bump('graveyard',owner,name,1)
   else:change_card('source','damage',source_damage)
   if witness:
    checks += [observe('combat_witness','in_play',int(not dead_witness))]
    if dead_witness:bump('battlefield',1-owner,witness,-1);bump('graveyard',1-owner,witness,1)
    else:checks += [observe('combat_witness','power',int(oracle[witness]['power'])),observe('combat_witness','toughness',int(oracle[witness]['toughness'])),observe('combat_witness','damage',wdamage),observe('combat_witness','tapped',0)]
   combat += [step('end_combat')]
  c['steps']=pre+combat+checks
  if snapshot:
   # Restore the ready board and repeat the actual combat probe after nested AI
   # work, rather than only comparing a post-combat memory image.
   c['steps']=pre+[step('capture')]+combat+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(combat+checks)+[step('assert_recorded_state')]
  c['name']=f'ConditionalKeywordRecipe_{csvid}_{variant}'
  c['_recipe']=dict(family='conditional_keyword',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['defending with the source','control and type changes','other continuous modifiers','combat with multiple blockers','additional triggers or abilities','graveyard replacement effects','multiplayer'])
  result.append(c)
 return result
