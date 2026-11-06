"""Whole-Oracle reviewed tap/untap spells; actual casting and frozen cohorts.

The review table pins additional wording as well as the exercised component.
Inactive conditions are verified by the constructed board, never by handlers.
"""
import copy
import json
from pathlib import Path

REVIEW=json.loads((Path(__file__).parent/'data/tap_spell_reviews.json').read_text(encoding='utf8'))
SINGLE={'creature','land','permanent','artifact_creature_land'}
HAND=('Grizzly Bears','Plains')
GRAVE=('Grizzly Bears','Forest')
EXILE=('Island',)
LIBRARY=('Forest','Plains','Grizzly Bears','Forest','Mountain','Island')
ARMY=('Grizzly Bears','Hill Giant','Merfolk of the Pearl Trident','Savannah Lions','Kami of Ancient Law','Sol Ring','Forest','Darksteel Relic','Island')

def spec(d,o):
 from recipes import cost_from_oracle
 entry=(REVIEW['cards'].get(o.get('name')) or REVIEW.get('advanced_cards',{}).get(o.get('name')) or REVIEW.get('extension_cards',{}).get(o.get('name')) or REVIEW.get('next_untap_cards',{}).get(o.get('name')) or REVIEW.get('mechanic_cards',{}).get(o.get('name')) or REVIEW.get('quantity_cards',{}).get(o.get('name'))) if o else None
 if o and isinstance(d.get('params'),dict) and d['params'].get('schema')==9:entry=REVIEW.get('cost_followups',{}).get(o.get('name'))
 if not entry or o.get('layout')!='normal' or o.get('card_faces') or o.get('selected_face') or o.get('type_line','').partition(' — ')[0] not in ('Instant','Sorcery'):
  raise ValueError('tap spell requires a reviewed ordinary instant/sorcery')
 keys=('name','oracle_id','oracle_text','mana_cost','type_line','keywords','layout','colors')
 if any(o.get(k)!=entry['oracle'].get(k) for k in keys) or d.get('oracle_text')!=o['oracle_text']:
  raise ValueError('tap spell whole Oracle review changed')
 if type(d.get('version')) is not int or d.get('version')!=2 or 'variants' not in d or json.dumps(d.get('params'),sort_keys=True)!=json.dumps(entry['params'],sort_keys=True):
  raise ValueError('tap spell requires explicit reviewed version 2 parameters and variants')
 p=copy.deepcopy(entry['params'])
 if p.get('schema')==8:return dict(p,family='tap_spell',quantity8=True)
 if p.get('schema')==5:return dict(p,family='tap_spell',next_untap5=True,source_name=o['name'])
 if p.get('schema')==6:
  reviewed=_spec3(dict(p,schema=4),o);reviewed['variable']=True;return reviewed
 if p.get('schema') in (7,9):return _spec3(dict(p,schema=4),o)
 if p.get('schema') in (3,4):return _spec3(p,o)
 return dict(p,family='tap_spell',mana=cost_from_oracle(o),instant=o['type_line'].startswith('Instant'),single=p['scope'] in SINGLE)

def variants(p):
 if p.get('quantity8'):return _variants8(p)
 if p.get('next_untap5'):return _variants5(p)
 if p.get('advanced3'):return _variants3(p)
 names=['effect','payment','insufficient_mana','wrong_color','bare_zones','already_in_state','cleanup','two_copies',
        'ready_snapshot','pending_snapshot','resolved_snapshot','countered','combat_attack']
 if p['instant']:names+=['opposing_turn']
 if p['single']:
  names+=['lost_target','bounced_target']
  if p['controller']!='opponent':names+=['own_target']
  # Whole-card cast permission cannot isolate one mode of a modal/overload spell.
  if not p['cast_before'] and p['scope'] not in ('permanent','artifact_creature_land'):names+=['wrong_type']
  if not p['optional'] and not p['cast_before']:names+=['no_targets']
  if p['count']>1:names+=['one_target'] if p['optional'] else []
  if p['optional']:names+=['zero_targets']
  if p['scope'] in ('permanent','artifact_creature_land'):names+=['artifact_target','land_target']
  if p['scope']!='land' and p['count']==1:names+=['target_shrouded_pending']
 else:names+=['empty_battlefield','removed_before','late_entry']
 if 'no spell previously cast for surge' in p['inactive']:names.remove('two_copies')
 if p['scope'] in ('land','lands'):names.remove('combat_attack')
 if p['scope'] in ('lands','white_creatures'):names.remove('late_entry')
 return [v+'_'+str(owner) for owner in (0,1) for v in names]

def validate_witnesses(o,rows):
 for name,pin in {**REVIEW['witnesses'],**REVIEW.get('advanced_witnesses',{}),**REVIEW.get('extension_witnesses',{}),**REVIEW.get('next_untap_witnesses',{}),**REVIEW.get('mechanic_witnesses',{}),**REVIEW.get('quantity_witnesses',{})}.items():
  if name not in rows or any(o.get(name,{}).get(k)!=v for k,v in pin.items()):
   raise ValueError('tap spell witness changed: '+name)

def matches(record,p,owner,controller):
 if p['controller']=='self' and controller!=owner or p['controller']=='opponent' and controller==owner:return False
 types=set(record['type_line'].partition(' — ')[0].split());scope=p['scope']
 if scope in ('creature','creatures','player_creatures'):return 'Creature' in types
 if scope in ('land','lands'):return 'Land' in types
 if scope=='blue_creatures':return 'Creature' in types and 'U' in record.get('colors',[])
 if scope=='white_creatures':return 'Creature' in types and 'W' in record.get('colors',[])
 if scope=='nonspirit_creatures':return 'Creature' in types and 'Spirit' not in record['type_line'].partition(' — ')[2].split()
 if scope=='nonland_permanents':return 'Land' not in types
 if scope=='artifact_creature_land':return bool(types&{'Artifact','Creature','Land'})
 return scope=='permanent'

def expand(name,csvid,d,p,selected,rows,o):
 if p.get('quantity8'):return _expand8(name,csvid,d,p,selected,rows,o)
 if p.get('next_untap5'):return _expand5(name,csvid,d,p,selected,rows,o)
 if p.get('advanced3'):return _expand3(name,csvid,d,p,selected,rows,o)
 from recipes import card,step,observe,cost_from_oracle
 validate_witnesses(o,rows)
 supported=variants(p)
 if not selected or len(selected)!=len(set(selected)) or any(x not in supported for x in selected):raise ValueError('explicit distinct tap spell variants required')
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';empty=v=='empty_battlefield'
  zones={z:[[] if bare else list(ns) for _ in (0,1)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY))}
  # Draw components always have a real, pinned library, including bare-zone cases.
  if p['draw']:zones['library']=[list(LIBRARY),list(LIBRARY)]
  setup=[];entities={};mana=[[20]*6,[20]*6];pre=[step('turn',player=owner,value=0)];actions=[]
  for player in (0,1):
   for z,ns in zones.items():setup += [card(nm,f'{z}_{player}_{i}',player,z) for i,nm in enumerate(ns[player])]
  def add(nm,ref,pl,zone='battlefield',tapped=False):
   item=card(nm,ref,pl,zone)
   if zone=='battlefield':item['state']=['tapped' if tapped else 'untapped']
   setup.append(item);entities[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',tapped=tapped)
   if zone!='battlefield':zones[zone][pl].append(nm)
  def pay(nm,pl):mana[pl]=[a-b for a,b in zip(mana[pl],cost_from_oracle(o[nm]))]
  def moved(ref,z):
   e=entities[ref];e['alive']=False;zones[z][e['player']].append(e['name'])
  add(name,'source',owner,'hand')
  target_owner=owner if v in ('own_target','wrong_controller') or p['controller']=='self' or v=='late_entry' and p['scope']=='player_creatures' else 1-owner
  recipient='Forest' if p['scope'] in ('land','lands') else 'Merfolk of the Pearl Trident' if p['scope']=='blue_creatures' else 'Savannah Lions' if p['scope']=='white_creatures' else 'Grizzly Bears'
  if v=='artifact_target':recipient='Sol Ring'
  if v=='land_target':recipient='Forest'
  if v=='wrong_type':recipient='Grizzly Bears' if p['scope']=='land' else 'Forest'
  desired=p['effect']=='tap' or p['effect']=='tap_opponent_untap_self' and target_owner!=owner
  initial=desired if v=='already_in_state' else not desired
  selected_refs=[]
  if p['single']:
   count=1 if v=='one_target' else p['count']
   if v in ('zero_targets','no_targets'):count=0
   for i in range(count):
    ref='recipient' if i==0 else f'recipient_{i}';add(recipient,ref,target_owner,tapped=initial)
    if not (v=='wrong_type' and p['optional']):selected_refs.append(ref)
   if v not in ('wrong_type','wrong_controller','no_targets'):
    add(recipient,'unselected',target_owner,tapped=initial)
    add('Grizzly Bears' if p['scope']=='land' else 'Forest','type_control',1-target_owner,tapped=initial)
  elif not empty:
   for pl in (0,1):
    for i,nm in enumerate(ARMY):
     # Each controller gets a mixed ready/tapped board; first matching member
     # starts opposite the effect so every successful cast has a positive receipt.
     match=matches(o[nm],p,owner,pl)
     tap=p['effect']=='tap' or p['effect']=='tap_opponent_untap_self' and pl!=owner
     state=tap if v=='already_in_state' else not tap if match and i%2==0 else bool(i%2)
     if not match:
      if nm=='Forest':state=True
      if nm in ('Island','Darksteel Relic'):state=False
     ref=f'army_{pl}_{i}';add(nm,ref,pl,tapped=state)
   if p['scope']=='player_creatures':selected_refs=[f'army_{target_owner}_{i}' for i,nm in enumerate(ARMY) if 'Creature' in o[nm]['type_line']]
  probe=selected_refs[0] if selected_refs else next((ref for ref,e in entities.items() if e['alive'] and matches(o[e['name']],p,owner,e['player'])),None)
  # Inactive mechanics are established by the fixture, independently of handlers.
  if 'fewer than three artifacts' in p['inactive']:
   assert all(sum(e['alive'] and e['player']==pl and 'Artifact' in o[e['name']]['type_line'] for e in entities.values())<3 for pl in (0,1))
  if 'no Samurai creatures' in p['inactive']:assert not any(e['alive'] and 'Samurai' in o[e['name']]['type_line'] for e in entities.values())
  if 'fewer than two instant/sorcery cards in graveyard' in p['inactive']:assert not any({'Instant','Sorcery'}&set(o[nm]['type_line'].split()) for nm in zones['graveyard'][owner])
  if 'fewer than two Islands controlled' in p['inactive']:assert sum(e['alive'] and e['player']==owner and 'Island' in o[e['name']]['type_line'].partition(' — ')[2].split() for e in entities.values())<2
  if v=='two_copies':add(name,'second',owner,'hand');add('Twiddle','reset',owner,'hand')
  if v=='countered':add('Counterspell','counter',0,'hand')
  if v=='bounced_target':add('Boomerang','response',owner,'hand')
  if v=='target_shrouded_pending':add("Alexi's Cloak",'response',owner,'hand')
  late_name='Diregraf Ghoul' if p['effect']=='untap' else 'Merfolk of the Pearl Trident' if p['scope']=='blue_creatures' else 'Grizzly Bears'
  if v=='late_entry':add(late_name,'late',owner,'hand')
  # A pool receipt is insufficient if ready lands/artifacts can supply the
  # missing color or generic amount. Keep all casting resources explicit.
  if v in ('payment','insufficient_mana','wrong_color'):
   for item in setup:
    e=entities.get(item['ref'])
    if e and e['alive'] and e['player']==owner and e['name'] in ('Sol Ring','Forest','Island'):
     e['tapped']=True;item['state']=['tapped']
  if v=='payment':
   mana[owner]=list(p['mana'])
   # A reserved generic mana makes an erroneous ordinary-mode entwine charge
   # observable without an impossible-payment UI. Correct casting must leave
   # this reserve untouched, so repairing the card makes the same case pass.
   if name=="Dream's Grip":mana[owner][0]=1
  if v=='insufficient_mana':mana[owner]=[0]*6
  if v=='wrong_color':
   mana[owner]=list(p['mana']);c=next(i for i in range(1,6) if mana[owner][i]);mana[owner][1 if c!=1 else 2]+=mana[owner][c];mana[owner][c]=0
  for pl in (0,1):pre += [step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl])]
  if v=='opposing_turn':pre += [step('turn',player=1-owner,value=0)]
  pre += [step('refresh')]
  def inspect():
   seq=[]
   for ref,e in entities.items():
    if e['alive']:
     seq += [observe(ref,'in_play',1),observe(ref,'tapped',int(e['tapped']))]
     if 'Creature' in o[e['name']]['type_line']:seq += [observe(ref,k,int(o[e['name']][k])) for k in ('power','toughness')]+[observe(ref,'damage',0)]
   return seq
  def choices(include_resolve=True):
   # The ordinary-cost receipt leaves no mana for optional overload/buyback;
   # those menus are offered only when their extra payment is affordable.
   before=[] if v=='payment' and name=='Blustersquall' else p['cast_before']
   after=[] if v=='payment' and name=='Mind Games' else p['cast_after']
   seq=[step('choose_dialog',player=owner,value=n) for n in before]
   if p['single']:
    seq += [step('choose_target',player=owner,ref=ref) for ref in selected_refs]
    if p['optional'] and len(selected_refs)<p['count'] and v!='wrong_type':seq += [step('choose_cancel_target',player=owner)]
   if p['scope']=='player_creatures':seq += [step('choose_player',player=owner,value=target_owner)]
   seq += [step('choose_dialog',player=owner,value=n) for n in after+(p['resolve_dialogs'] if include_resolve else [])]
   return seq
  def cast(ref='source'):
   pay(name,owner);zones['hand'][owner].remove(name)
   return [step('require_can_cast',ref=ref,value=1)]+choices()+[step('cast',ref=ref,value=1),step('assert_stack',value=1)]+inspect()
  def affected(ref,e):
   if not e['alive']:return False
   if v=='target_shrouded_pending' and ref=='recipient':return False
   if p['single']:return ref in selected_refs
   if p['scope']=='player_creatures':return e['player']==target_owner and 'Creature' in o[e['name']]['type_line']
   return matches(o[e['name']],p,owner,e['player'])
  def resolve():
   for ref,e in entities.items():
    if affected(ref,e):e['tapped']=p['effect']=='tap' or p['effect']=='tap_opponent_untap_self' and e['player']!=owner
   if not p['single'] or any(entities[ref]['alive'] and not (v=='target_shrouded_pending' and ref=='recipient') for ref in selected_refs):
    for _ in range(p['draw']):zones['hand'][owner].append(zones['library'][owner].pop(0))
   zones['graveyard'][owner].append(name)
   return [step('resolve'),step('assert_choices_consumed'),step('refresh')]+inspect()
  illegal=v in ('insufficient_mana','wrong_color','wrong_controller','no_targets') or v=='wrong_type' and not p['optional'] or v=='own_target' and p['controller']=='opponent'
  cut=0
  if illegal:
   actions += [step('cast' if v in ('insufficient_mana','wrong_color') else 'can_cast',ref='source',value=0),step('assert_choices_consumed')]
  elif v=='countered':
   actions += choices(False)+[step('choose_priority',ref='counter'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
   pay(name,owner);pay('Counterspell',0);zones['hand'][owner].remove(name);zones['hand'][0].remove('Counterspell');zones['graveyard'][owner].append(name);zones['graveyard'][0].append('Counterspell')
  else:
   actions+=cast();cut=len(actions)
   if v in ('lost_target','removed_before'):
    victim=selected_refs[0] if p['single'] else probe
    if victim:actions += [step('destroy',ref=victim)];moved(victim,'graveyard')
   if v=='bounced_target':
    victim=selected_refs[0];actions += [step('choose_target',player=owner,ref=victim),step('cast',ref='response',value=1),step('resolve'),step('assert_choices_consumed')]
    pay('Boomerang',owner);zones['hand'][owner].remove('Boomerang');zones['graveyard'][owner].append('Boomerang');moved(victim,'hand')
   if v=='target_shrouded_pending':
    actions += [step('choose_target',player=owner,ref='recipient'),step('cast',ref='response',value=1),step('resolve'),step('assert_choices_consumed')]
    pay("Alexi's Cloak",owner);zones['hand'][owner].remove("Alexi's Cloak");entities['response']['alive']=True
   actions+=resolve()
   if v=='two_copies':
    if probe:
     reset=not entities[probe]['tapped'];actions += [step('choose_target',player=owner,ref=probe),step('cast',ref='reset',value=1),step('choose_dialog',player=owner,value=0 if reset else 1),step('resolve'),step('assert_choices_consumed')]
     pay('Twiddle',owner);zones['hand'][owner].remove('Twiddle');zones['graveyard'][owner].append('Twiddle');entities[probe]['tapped']=reset;actions+=inspect()
    actions+=cast('second')+resolve()
   if v=='late_entry':
    actions += [step('cast',ref='late',value=1),step('resolve'),step('assert_choices_consumed')];pay(late_name,owner);zones['hand'][owner].remove(late_name);entities['late']['alive']=True;entities['late']['tapped']=late_name=='Diregraf Ghoul'
   if v=='combat_attack' and probe and entities[probe]['alive'] and 'Creature' in o[entities[probe]['name']]['type_line']:
    e=entities[probe];legal=not e['tapped'];actions += [step('turn',player=e['player'],value=0),step('begin_attackers'),step('attack',ref=probe,value=int(legal)),step('finish_attackers'),step('begin_blockers'),step('advance_combat')]
    if legal:e['tapped']=True
   if v=='cleanup':actions += [step('cleanup'),step('refresh')]
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]+inspect()
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
  for pl in (0,1):
   checks += [step('assert_life',player=pl,value=20),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))]
   if v!='cleanup':checks += [step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl])]
   for z,ns in zones.items():
    checks += [step('assert_zone',player=pl,zone=z,value=len(ns[pl]))]
    checks += [step('assert_zone_multiplicity',player=pl,zone=z,index=rows[nm]['csvid'],value=ns[pl].count(nm)) for nm in sorted(universe)]
    if z=='library':checks += [step('assert_zone_card',player=pl,zone=z,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(ns[pl])]
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
   at=0 if v=='ready_snapshot' else cut if v=='pending_snapshot' else len(actions)
   tail=actions[at:]+checks
   steps=pre+actions[:at]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  result.append(dict(name=f'TapSpellRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='tap_spell',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps']+['next untap steps','control changes','multiplayer'],inactive_conditions=p['inactive'])))
 return result


# Schema3 is a separate reviewed model. Legacy declarations above deliberately
# retain their previous variants, fixture construction and decoded step bodies.
def _spec3(p,o):
 from recipes import cost_from_oracle
 raw=p['payment_mana'] or o['mana_cost']
 variable='{X}' in raw
 mana=cost_from_oracle(dict(mana_cost=raw.replace('{X}','{0}')))
 return dict(p,family='tap_spell',advanced3=True,source_name=o['name'],mana=mana,variable=variable,instant=o['type_line'].startswith('Instant'))

def _variants3(p):
 if p.get('convoke_only'):
  modes=['convoke_payment','convoke_countered','convoke_ready_snapshot','convoke_pending_snapshot','convoke_foreign_owned','convoke_tapped','convoke_wrong_controller','convoke_noncreature']
  if p['instant']:modes+=['convoke_opposing_turn']
  if p.get('indestructible_grant'):modes+=['convoke_protected_destroy']
  return [mode+'_'+str(owner) for owner in (0,1) for mode in modes]
 modes=['effect','payment','insufficient_mana','wrong_color','bare_zones','cleanup','two_copies','ready_snapshot','pending_snapshot','resolved_snapshot','countered']
 if p['instant']:modes+=['opposing_turn']
 single=p['kind']!='mass'
 if single:
  modes+=['lost_target','bounced_target']
  if p['controller']=='any':modes+=['own_target']
  if p['state']=='any':modes+=['already_in_state']
  else:modes+=['wrong_state']
  if p['count']!='x' and not any(p['dialogs'].values()):
   if not p['optional']:modes+=['no_targets']
   if p['scope']!='permanent':modes+=['wrong_type']
   if p['controller']!='any':modes+=['wrong_controller']
  if p['optional']:modes+=['one_target','zero_targets']
  if p['optional'] and (p['token'] or p.get('schema')==4 and p['scry']):modes+=['only_target_lost']
  if p['scope'] in ('permanent','artifact_creature_land','nonland_permanent','artifact_or_creature','artifact'):modes+=['artifact_target','mana_source']
  if p['scope'] in ('permanent','artifact_creature_land'):modes+=['land_target']
 else:modes+=['empty_battlefield']
 if p['kind']=='theft' or p.get('mass_control'):modes+=['stolen_destroyed','stolen_bounced']
 if p['mana_gain']=='target_cmc' or p['controller_damage']=='target_power':modes+=['large_target']
 if p['scope'] not in ('land','own_lands'):modes+=['combat_attack']
 if p['power_limit'] is not None:modes+=['wrong_power']
 if p.get('flying_damage'):modes+=['flying_target','flying_cleanup']
 if 'Trample' in p.get('keywords',[]):modes+=['trample_block']
 if p.get('schema')==4:
  if p.get('timing')=='opponent_after_upkeep':modes.remove('opposing_turn');modes+=['own_turn_denied']
  if p['scope']=='own_lands':modes+=['mana_source']
  if p.get('target_player') and not p.get('player_opponent'):modes+=['target_self']
  if p.get('modal_types'):modes += ['mode_'+scope+'_'+effect for scope in ('artifact','creature','land') for effect in ('tap','untap') if (scope,effect)!=('creature','tap')]
  if p['scry']:modes+=['scry_bottom']
  if p.get('modal_power'):modes+=['small_creatures']
  if p.get('radiance'):modes+=['colorless_target']
  if p.get('combat_keywords'):modes+=['flying_block','keyword_cleanup']
  if p.get('combat_prevention'):
   modes=[mode for mode in modes if mode not in ('combat_attack','cleanup')]
   if p['effect'] in ('preserve','tap_blockers') and 'already_in_state' in modes:modes.remove('already_in_state')
   modes+=['prevent_unblocked','prevent_blocked','prevent_expired']
   if (p['kind']=='mass' or p['state']=='attacking') and 'opposing_turn' in modes:modes.remove('opposing_turn')
  if p.get('any_number'):modes+=['three_targets']
  if p.get('initial_ready') and p['kind']=='target':
   if 'already_in_state' in modes:modes.remove('already_in_state')
   modes+=['tapped_target']
  if p['scope']=='none' and 'combat_attack' in modes:modes.remove('combat_attack')
  if p.get('tap_cost'):modes+=['cost_tapped','cost_wrong_controller','cost_noncreature','cost_too_few']
  if p.get('sweep_destroy') and 'combat_attack' in modes:modes.remove('combat_attack')
  if p['optional'] and p['count']==1:modes.remove('one_target')
 if p.get('strive_cost'):modes=[mode for mode in modes if mode not in ('one_target','zero_targets')]
 if p.get('fixed_damage') or p.get('target_remove'):
  modes=[mode for mode in modes if mode not in ('combat_attack','already_in_state')]
 if p.get('any_damage_target'):
  modes=[mode for mode in modes if mode not in ('no_targets','wrong_type')]
  modes+=['opponent_player','self_player','large_target','indestructible_target']
 if p['scope']=='artifact_or_enchantment':modes+=['artifact_target','enchantment_target','wrong_creature']
 if p.get('target_remove') and p['state']=='attacking_or_blocking':modes+=['blocking_target']
 if p.get('indestructible_grant'):modes+=['protected_destroy','expired_destroy','lethal_damage','zero_toughness']
 if p.get('regeneration_shield'):
  modes+=['protected_destroy','shield_consumed','expired_destroy','lethal_damage','zero_toughness','damaged_shield','cannot_regenerate','regeneration_combat']
  if p.get('deathtouch_grant'):modes+=['deathtouch_combat','expired_deathtouch']
  if p.get('spell_mastery'):modes+=['mastery_active','mastery_own_target','opponent_mastery','mastery_all_targets_lost']
 if p['variable']:
  result=[v+'_x2_'+str(owner) for owner in (0,1) for v in modes if v!='scry_bottom' or owner==0]
  result += [v+'_x'+str(x)+'_'+str(owner) for owner in (0,1) for x in (0,1,3) for v in ('effect','payment')]
  if p['count']=='x':result += ['no_targets_x0_'+str(owner) for owner in (0,1)]
  return result
 return [v+'_'+str(owner) for owner in (0,1) for v in modes if v!='scry_bottom' or owner==0]

def _expand3(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 import re
 validate_witnesses(o,rows)
 if not isinstance(selected,list) or not selected or len(set(selected))!=len(selected) or any(v not in _variants3(p) for v in selected):raise ValueError('explicit distinct schema3 tap variants required')
 output=[];base_profile=p
 for variant in selected:
  p=copy.deepcopy(base_profile)
  raw,owner=variant.rsplit('_',1);owner=int(owner)
  parsed=re.fullmatch(r'(.+)_x([0-3])',raw) if p['variable'] else None
  v=parsed[1] if parsed else raw;x=int(parsed[2]) if parsed else 0
  convoke_mode=raw[8:] if p.get('convoke_only') and raw.startswith('convoke_') else None
  if convoke_mode:v={'foreign_owned':'effect','tapped':'insufficient_mana','wrong_controller':'insufficient_mana','noncreature':'insufficient_mana'}.get(convoke_mode,convoke_mode)
  if p.get('schema')==4 and p.get('modal_types'):
   mode=re.fullmatch(r'mode_(artifact|creature|land)_(tap|untap)',v)
   kind,effect=mode.groups() if mode else ('creature','tap')
   p['scope']='player_'+kind+'s';p['effect']=effect;p['resolve_before']=[{'land':0,'creature':1,'artifact':2}[kind],0 if effect=='tap' else 1]
  if p.get('modal_power'):
   p['power_filter']=['le',3] if v=='small_creatures' else ['ge',4];p['resolve_before']=[1 if v=='small_creatures' else 0]
  cost=list(p['mana'])
  if p.get('strive_cost'):cost=[a+max(0,x-1)*b for a,b in zip(cost,p['strive_cost'])]
  else:cost[0]+=x
  if convoke_mode:
   for resource in p['convoke_resources']:
    colors=o[resource]['colors'];assert len(colors)==1
    color='BUGRW'.index(colors[0])+1
    if cost[color]:cost[color]-=1
    else:assert cost[0]>0;cost[0]-=1
  bare=v=='bare_zones';single=p['kind']!='mass';theft=p['kind']=='theft' or bool(p.get('mass_control'))
  zones={z:[[] if bare else list(ns) for _ in (0,1)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY))}
  if p['draw'] or p['scry'] or p.get('draw_both'):zones['library']=[list(LIBRARY)*3,list(LIBRARY)*3]
  if p.get('spell_mastery') and v in ('mastery_active','mastery_own_target','mastery_all_targets_lost','opponent_mastery'):
   zones['graveyard'][1-owner if v=='opponent_mastery' else owner] += ['Lightning Bolt','Giant Growth']
  setup=[];entities={};mana=[[60,20,20,20,20,20] for _ in (0,1)];life=[20,20];tokens=[0,0]
  if p.get('start_life') is not None:life[owner]=p['start_life']
  actor=1-owner if v=='opposing_turn' or v=='combat_attack' and p['controller']=='opponent' and not theft or p.get('timing')=='opponent_after_upkeep' and v!='own_turn_denied' else owner
  if p.get('regeneration_shield') and v in ('regeneration_combat','deathtouch_combat','expired_deathtouch'):actor=1-owner
  pre=[step('turn',player=actor,value=0)];actions=[]
  for pl in (0,1):
   for z,ns in zones.items():setup += [card(nm,f'{z}_{pl}_{i}',pl,z) for i,nm in enumerate(ns[pl])]
  def add(nm,ref,pl,zone='battlefield',tapped=False,sick=False,foreign=False,token=False):
   item=card(nm,ref,pl,zone)
   if zone=='battlefield':item['state']=['tapped' if tapped else 'untapped']+(['sick'] if sick else [])+(['opponent_owned'] if foreign else [])+(['token'] if token else [])
   setup.append(item);r=o[nm];creature='Creature' in r['type_line']
   entities[ref]=dict(name=nm,controller=pl,owner=1-pl if foreign else pl,alive=zone=='battlefield',hand=zone=='hand',tapped=tapped,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,damage=0,creature=creature,attacking=False,blocking=False,prevent_by=False,prevent_to=False,token=token)
   if zone!='battlefield':zones[zone][pl].append(nm)
  def move(ref,z):
   e=entities[ref];e['alive']=False;e['hand']=z=='hand';zones[z][e['owner']].append(e['name'])
  def pay(vector,pl=owner):mana[pl]=[a-b for a,b in zip(mana[pl],vector)]
  add(name,'source',owner,'hand')
  target_owner=owner if p['controller']=='self' or v=='own_target' or v=='combat_attack' and p['controller']=='any' and not theft else 1-owner
  if v=='wrong_controller':target_owner=1-owner if p['controller']=='self' else owner
  if p.get('spell_mastery') and v=='mastery_own_target':target_owner=owner
  if p.get('target_player') and v=='target_self':target_owner=owner
  target_name='Phyrexian Hulk' if p['scope']=='artifact' else 'Forest' if p['scope']=='land' else 'Grizzly Bears'
  if p.get('target_default'):target_name=p['target_default']
  if v in ('artifact_target','mana_source'):target_name='Sol Ring'
  if v=='land_target':target_name='Forest'
  if v in ('large_target','wrong_power'):target_name='Scaled Wurm'
  if p.get('regeneration_shield') and v=='damaged_shield':target_name='Scaled Wurm'
  if v in ('flying_target','flying_cleanup'):target_name='Air Elemental'
  if v=='colorless_target':target_name='Phyrexian Hulk'
  if v=='indestructible_target':target_name='Darksteel Myr'
  if v=='enchantment_target':target_name='Omniscience'
  if v=='wrong_creature':target_name='Grizzly Bears'
  if v=='wrong_type':target_name='Grizzly Bears' if p['scope']=='land' else 'Omniscience' if p['scope']=='artifact_creature_land' else 'Forest'
  desired=p['effect']=='tap';initial=False if p['state']=='untapped' else not desired
  if v=='already_in_state':initial=desired
  if p.get('initial_ready'):initial=False
  if v in ('wrong_state','tapped_target'):initial=True
  chosen=[];count=x if p['count']=='x' else p['count']
  if v=='three_targets':count=3
  if v in ('one_target','only_target_lost'):count=min(1,count)
  if v in ('zero_targets','no_targets'):count=0
  damage_player=p.get('any_damage_target') and v in ('opponent_player','self_player')
  if damage_player:
   count=0;target_owner=owner if v=='self_player' else 1-owner
  if single:
   for i in range(count):
    ref='recipient' if i==0 else f'recipient_{i}'
    candidate=['Grizzly Bears','Hill Giant','Scaled Wurm'][i] if p.get('distinct_targets') and target_name=='Grizzly Bears' else target_name
    add(candidate,ref,target_owner,tapped=initial,sick=theft)
    if not (v=='wrong_type' and p['optional'] or p.get('schema')==4 and v=='wrong_controller' and p['optional']):chosen.append(ref)
   negative=v in ('wrong_type','no_targets','wrong_state','wrong_controller','wrong_power','wrong_creature')
   if not negative:
    if p.get('x_input') in ('target_count','mana_then_count'):
     # This UI chooses targets, then pays their count as X. A nontargetable
     # creature supplies an unchanged control without forcing an extra choice.
     add('Deadly Insect','unselected',1-owner,tapped=initial)
    else:
     add('Hill Giant' if target_name=='Grizzly Bears' else 'Sol Ring' if target_name=='Phyrexian Hulk' else target_name,'unselected',target_owner,tapped=initial)
     add('Darksteel Relic','type_control',owner,tapped=True)
  elif p.get('tap_cost'):
   if v!='empty_battlefield':
    for i,nm in enumerate(['Grizzly Bears','Hill Giant','Runeclaw Bear','Deadly Insect'][:3 if v=='cost_too_few' else 4]):
     if v=='cost_noncreature' and i==3:nm='Darksteel Relic'
     add(nm,'cost_'+str(i),1-owner if v=='cost_wrong_controller' and i==3 else owner,tapped=v=='cost_tapped' and i==3,sick=True)
    add('Centaur Courser','cost_enemy',1-owner);add('Darksteel Relic','cost_type_control',owner,tapped=True)
  elif v!='empty_battlefield' and p.get('mass_control'):
   for pl in (0,1):
    army=['Grizzly Bears','Hill Giant','Air Elemental'] if pl==owner else ['Runeclaw Bear','Centaur Courser','Rumbling Baloth']
    for i,nm in enumerate(army):add(nm,f'army_{pl}_{i}',pl,tapped=i!=1,sick=True)
    add('Darksteel Relic',f'artifact_{pl}',pl,tapped=True);add('Forest',f'land_{pl}',pl,tapped=True)
  elif v!='empty_battlefield':
   for pl in (0,1):
    add('Grizzly Bears',f'army_{pl}_0',pl,tapped=not (p.get('initial_ready') and pl==owner))
    add('Hill Giant',f'army_{pl}_1',pl,tapped=False)
    add('Darksteel Relic',f'artifact_{pl}',pl,tapped=True)
    add('Forest',f'land_{pl}',pl,tapped=False)
    if p.get('schema')==4:
     entities[f'land_{pl}']['tapped']=True;next(item for item in setup if item['ref']==f'land_{pl}')['state']=['tapped']
     add('Island',f'land_other_{pl}',pl,tapped=False);add('Sol Ring',f'artifact_other_{pl}',pl,tapped=False)
  if p.get('radiance') and v not in ('wrong_type','no_targets'):
   add('Grizzly Bears','radiance_own',owner,tapped=True);add('Phyrexian Hulk' if target_name!='Phyrexian Hulk' else 'Juggernaut','radiance_colorless',owner,tapped=True)
  if p.get('nontoken_controls') and v!='empty_battlefield':
   for pl in (0,1):
    add('Eldrazi Scion',f'kept_token_{pl}',pl,tapped=False,token=True)
    add('Darksteel Myr',f'indestructible_{pl}',pl,tapped=True)
    add('Grizzly Bears',f'foreign_{pl}',pl,tapped=True,foreign=True)
  if p.get('spell_mastery') and v in ('mastery_active','mastery_own_target','mastery_all_targets_lost','opponent_mastery'):
   add('Hill Giant','mastery_own',owner);add('Centaur Courser','mastery_foreign',owner,foreign=True)
   if v=='mastery_own_target':add('Hill Giant','mastery_enemy',1-owner)
  convoke_refs=[]
  if convoke_mode:
   # All other owned creatures remain unavailable, so exactly these two
   # resources determine legality, credit and AI auto-convoke selection.
   for item in setup:
    e=entities.get(item['ref'])
    if e and e['alive'] and e['controller']==owner and e['creature']:
     e['tapped']=True;item['state']=['tapped']+[flag for flag in item.get('state',[]) if flag not in ('tapped','untapped')]
   for i,resource in enumerate(p['convoke_resources']):
    if convoke_mode=='noncreature' and i==0:resource=p['convoke_noncreature']
    ref='convoke_resource_'+str(i);convoke_refs.append(ref)
    add(resource,ref,1-owner if convoke_mode=='wrong_controller' and i==0 else owner,tapped=convoke_mode=='tapped' and i==0,sick='Creature' in o[resource]['type_line'],foreign=convoke_mode=='foreign_owned')
  probe=chosen[0] if chosen else next((ref for ref,e in entities.items() if e['alive'] and e['controller']==owner and e['creature']),None)
  if p['scope']=='own_lands' and v=='mana_source':probe=f'land_{owner}'
  if p.get('mass_control'):
   probe=next((ref for ref,e in entities.items() if e['alive'] and e['controller']!=owner and e['creature'] and (not p.get('power_filter') or (e['power']>=p['power_filter'][1] if p['power_filter'][0]=='ge' else e['power']<=p['power_filter'][1]))),None)
  if 'no controlled creature on successful casts to encode' in p['inactive'] and v!='wrong_controller':assert not any(e['alive'] and e['controller']==owner and e['creature'] for e in entities.values())
  if 'no controlled creature has power 4 or greater' in p['inactive']:assert not any(e['alive'] and e['controller']==owner and e['creature'] and e['power']>=4 for e in entities.values())
  if 'no creature blocks during the turn' in p['inactive']:assert v!='trample_block'
  if v=='trample_block':add('Merfolk of the Pearl Trident','blocker',1-owner)
  if v in ('flying_block','keyword_cleanup'):add('Merfolk of the Pearl Trident','blocker',1-owner)
  combat_active=bool(p.get('combat_prevention')) and v not in ('wrong_type','no_targets','wrong_state','wrong_controller','insufficient_mana','wrong_color','empty_battlefield') and (not single or p['state']=='attacking' or v.startswith('prevent_'))
  combat_attackers=[];combat_blocker=None
  if combat_active:
   if single:combat_attackers=list(chosen)
   else:
    add('Serra Angel','fog_attacker',1-owner);combat_attackers=['fog_attacker']
   if combat_attackers:
    attacker_owner=entities[combat_attackers[0]]['controller'];pre[0]=step('turn',player=attacker_owner,value=0)
    blocked=v=='prevent_blocked' or p.get('must_blocked') and v not in ('prevent_unblocked','prevent_expired')
    if blocked:
     combat_blocker='fog_blocker';add('Giant Spider' if not single else 'Merfolk of the Pearl Trident',combat_blocker,1-attacker_owner)
    for ref in combat_attackers:
     entities[ref]['tapped']=False;next(item for item in setup if item['ref']==ref)['state']=['untapped']
  if v=='prevent_expired':add('Twiddle','expiry_reset',owner,'hand')
  if p.get('indestructible_grant') and v in ('lethal_damage','zero_toughness'):add('Lightning Bolt' if v=='lethal_damage' else 'Last Gasp','protection_probe',owner,'hand')
  if p.get('regeneration_shield'):
   if v in ('lethal_damage','zero_toughness','damaged_shield','cannot_regenerate'):
    add({'lethal_damage':'Lightning Bolt','zero_toughness':'Last Gasp','damaged_shield':'Lightning Bolt','cannot_regenerate':'Terminate'}[v],'regeneration_probe',owner,'hand')
   if v=='expired_destroy':add(name,'expiry_second',owner,'hand')
   if v in ('regeneration_combat','deathtouch_combat','expired_deathtouch'):add('Scaled Wurm','regeneration_blocker',owner)
   if v=='expired_deathtouch':add('Scaled Wurm','expiry_blocker',owner);add('Twiddle','regen_reset',owner,'hand')
  if v=='two_copies':
   add(name,'second',owner,'hand')
   if p.get('sweep_destroy'):add('Grizzly Bears','sweep_later',owner,'hand')
   elif p.get('set_life') is not None:pass
   elif p.get('tap_cost'):add('Mobilize','cost_reset',owner,'hand')
   elif not p['return_tap'] and not p.get('repeat_new_target'):add('Twiddle','reset',owner,'hand')
  if v=='countered':add('Counterspell','counter',0,'hand')
  if v in ('bounced_target','stolen_bounced'):add('Boomerang','response',owner,'hand')
  # Optional casting resources are explicit too: normal convoke declines
  # tapping, and failed payment cannot be funded by those creatures.
  if p.get('no_ready_artifacts'):
   for item in setup:
    e=entities.get(item['ref'])
    if e and e['alive'] and e['controller']==owner and 'Artifact' in o[e['name']]['type_line']:e['tapped']=True;item['state']=['tapped']
  if p.get('convoke_normal') and v in ('insufficient_mana','wrong_color') and not convoke_mode:
   for item in setup:
    e=entities.get(item['ref'])
    if e and e['alive'] and e['controller']==owner and e['creature']:e['tapped']=True;item['state']=['tapped']+[flag for flag in item.get('state',[]) if flag not in ('tapped','untapped')]
  if p.get('conspire_inactive'):
   assert sum(e['alive'] and e['controller']==owner and e['creature'] and not e['tapped'] and bool(set(o[e['name']]['colors'])&set(o[name]['colors'])) for e in entities.values())<2
  # Never let ready battlefield mana pay the missing color or generic cost.
  if v in ('payment','insufficient_mana','wrong_color'):
   for item in setup:
    e=entities.get(item['ref'])
    if e and e['alive'] and e['controller']==owner and e['name'] in ('Forest','Sol Ring','Island'):
     e['tapped']=True;item['state']=['tapped']
  if v=='payment':mana[owner]=list(cost)
  if v=='insufficient_mana':mana[owner]=[0]*6
  if v=='wrong_color':
   mana[owner]=list(cost);color=next(i for i in range(1,6) if mana[owner][i]);mana[owner][1 if color!=1 else 2]+=mana[owner][color];mana[owner][color]=0
  if convoke_mode:
   mana[owner]=list(cost)
   if v=='countered' and owner==0:mana[owner][2]+=2
  if p.get('no_white'):mana[owner][5]=0
  for pl in (0,1):pre += [step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl])]
  if p.get('start_life') is not None:pre += [step('set_life',player=owner,value=p['start_life'])]
  pre += [step('refresh')]
  def declare_combat(attackers,blocker=None):
   seq=[step('begin_attackers')]
   for ref in attackers:
    seq += [step('attack',ref=ref,value=1)];e=entities[ref];e['tapped']='Vigilance' not in o[e['name']]['keywords'];e['attacking']=True
   seq += [step('finish_attackers'),step('begin_blockers')]
   if blocker:
    seq += [step('block',ref=blocker,other=attackers[0],value=1)];entities[blocker]['blocking']=True
   return seq+[step('advance_combat')]
  if combat_active and combat_attackers:pre += declare_combat(combat_attackers,combat_blocker)
  if p.get('target_remove') and p['state']=='attacking_or_blocking' and v not in ('wrong_state','no_targets','wrong_type','wrong_controller','wrong_power','wrong_creature') and chosen:
   participants=list(chosen)+(['unselected'] if 'unselected' in entities and entities['unselected']['creature'] else [])
   for ref in participants:
    entities[ref]['tapped']=False;next(item for item in setup if item['ref']==ref)['state']=['untapped']
   if v=='blocking_target':
    add('Scaled Wurm','combat_attacker',1-target_owner)
    pre += [step('turn',player=1-target_owner,value=0),step('begin_attackers'),step('attack',ref='combat_attacker',value=1),step('finish_attackers'),step('begin_blockers')]
    entities['combat_attacker']['tapped']=True;entities['combat_attacker']['attacking']=True
    for ref in participants:pre.append(step('block',ref=ref,other='combat_attacker',value=1));entities[ref]['blocking']=True
    pre.append(step('advance_combat'))
   else:
    pre.append(step('turn',player=target_owner,value=0));pre += declare_combat(participants)
  def bind(ref):
   e=entities[ref];return [step('bind_permanent_copy',ref=ref,player=e['controller'],value=rows[e['name']]['csvid'],index=0)]
  def inspect():
   seq=[]
   for ref,e in entities.items():
    if not e['alive']:continue
    seq += [observe(ref,'in_play',1),observe(ref,'tapped',int(e['tapped']))]
    if p.get('nontoken_controls') and ref.startswith('kept_token_'):seq.append(step('assert_matching_permanents',player=e['controller'],index=rows[e['name']]['csvid'],property='token',value=1))
    if convoke_mode and ref in convoke_refs and e['creature']:seq.append(observe(ref,'sick',1))
    if e['creature']:seq += [observe(ref,k,e[k]) for k in ('power','toughness','damage')]
   return seq
  def receipts():
   seq=[];universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
   for pl in (0,1):
    seq += [step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['controller']==pl for e in entities.values())+tokens[pl])]
    if v not in ('cleanup','flying_cleanup'):seq += [step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl])]
    if p['token']:
     token=p['token'];tid=rows[token['name']]['csvid']
     seq += [step('assert_zone_multiplicity',player=pl,zone='battlefield',index=tid,value=tokens[pl])]
     if tokens[pl]:seq += [step('assert_matching_permanents',player=pl,index=tid,property=k,value=value) for k,value in token['properties'].items()]
    for z,ns in zones.items():
     seq.append(step('assert_zone',player=pl,zone=z,value=len(ns[pl])))
     seq += [step('assert_zone_multiplicity',player=pl,zone=z,index=rows[nm]['csvid'],value=ns[pl].count(nm)) for nm in sorted(universe)]
     if z=='library':seq += [step('assert_zone_card',player=pl,zone=z,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(ns[pl])]
   return seq
  def choices():
   seq=[step('choose_x',player=owner,value=x)] if p['variable'] and p.get('x_input')!='target_count' else []
   menus=p['dialogs'][str(owner)]
   if p['menu']=='extra2' and v=='payment':menus=[]
   seq += [step('choose_dialog',player=owner,value=n) for n in menus]
   if p.get('convoke_normal') and not convoke_mode and any(e['alive'] and e['controller']==owner and e['creature'] and not e['tapped'] for e in entities.values()):seq += [step('choose_dialog',player=owner,value=1)]
   if convoke_mode and owner==0:seq += [step('choose_target',player=owner,ref=ref) for ref in convoke_refs]
   if p.get('tap_cost'):seq += [step('choose_target',player=owner,ref='cost_'+str(i)) for i in range(4)]
   seq += [step('choose_target',player=owner,ref=ref) for ref in chosen]
   if damage_player:seq += [step('choose_player',player=owner,value=target_owner)]
   if p.get('target_player') and not p.get('player_opponent'):seq += [step('choose_player',player=owner,value=target_owner)]
   if p.get('target_ui')=='two_picks':
    cancels=1 if v=='wrong_type' else max(0,2-len(chosen))
    seq += [step('choose_cancel_target',player=owner) for _ in range(cancels)]
   elif p.get('target_ui')=='until_done' and v!='wrong_type':
    # The AI-only off-turn UI filters Energy Arc's selections to attackers.
    # Once every such candidate was selected, its loop ends without Done.
    remaining=any(e['alive'] and e['creature'] and ref not in chosen and 'Shroud' not in o[e['name']]['keywords'] and (owner!=1 or pre[0]['player']==owner or e['attacking']) for ref,e in entities.items())
    if remaining:seq += [step('choose_cancel_target',player=owner)]
   elif p['optional'] and len(chosen)<(x if p['count']=='x' else p['count']) and v!='wrong_type' and not (p.get('schema')==4 and v=='wrong_controller'):seq += [step('choose_cancel_target',player=owner)]
   after=[] if p.get('after_extra_mana') and v=='payment' else p['after']
   seq += [step('choose_dialog',player=owner,value=n) for n in after]
   return seq
  def paid_cost():
   if convoke_mode:
    for ref in convoke_refs:entities[ref]['tapped']=True
   if p.get('tap_cost'):
    for i in range(4):entities['cost_'+str(i)]['tapped']=True
  def cast(ref='source'):
   paid_cost();pay(cost);return [step('require_can_cast',ref=ref,value=1)]+choices()+[step('cast',ref=ref,value=1),step('assert_stack',value=1)]+inspect()
  def valid(ref):
   e=entities[ref]
   return e['alive'] and (p['state']!='untapped' or not e['tapped']) and (p['state']!='attacking' or e['attacking']) and (p['state']!='attacking_or_blocking' or e['attacking'] or e['blocking']) and (p['power_limit'] is None or e['power']<=p['power_limit'])
  def active():return not chosen or any(valid(ref) for ref in chosen)
  def resolve_choices(exclude=()):
   legal=not chosen or any(valid(ref) for ref in chosen if ref not in exclude)
   seq=[step('choose_dialog',player=owner,value=n) for n in p.get('resolve_before',[])]
   if p['scry'] and legal:
    if owner==0:seq+=[step('choose_zone',player=owner,value=0 if v=='scry_bottom' else -1)]
    else:seq+=[step('choose_optional_zone',player=owner,value=rows[zones['library'][owner][0]]['csvid'])]
   return seq
  def mass_match(e):
   if not e['alive']:return False
   if p['scope']=='none':return False
   if p['scope']=='own_lands':return e['controller']==owner and 'Land' in o[e['name']]['type_line']
   if p['scope']=='own_creatures':return e['controller']==owner and e['creature']
   if p['scope']=='nonland_permanents':return 'Land' not in o[e['name']]['type_line']
   if p.get('power_filter') and e['creature']:return e['power']>=p['power_filter'][1] if p['power_filter'][0]=='ge' else e['power']<=p['power_filter'][1]
   if p['scope'].startswith('player_'):
    return e['controller']==target_owner and p['scope'][7:-1].capitalize() in o[e['name']]['type_line']
   return e['creature']
  def apply(ref='source'):
   legal=[ref for ref in chosen if valid(ref)];active=not chosen or bool(legal)
   mastery=p.get('spell_mastery') and sum(bool({'Instant','Sorcery'}&set(o[n]['type_line'].partition(' — ')[0].split())) for n in zones['graveyard'][owner])>=2
   rebound=p['source_destination']=='exile' and active
   zones['hand'][owner].remove(name);zones['exile' if rebound else 'graveyard'][owner].append(name);entities[ref]['hand']=False
   rebinding=[]
   selected=legal if single else [ref for ref,e in entities.items() if mass_match(e)]
   if p.get('radiance'):
    colors=set().union(*(set(o[entities[ref]['name']]['colors']) for ref in legal)) if legal else set()
    selected=[ref for ref,e in entities.items() if e['alive'] and e['creature'] and (ref in legal or colors&set(o[e['name']]['colors']))]
   if p['effect']=='tap_blockers' and single and legal and combat_blocker:selected += [combat_blocker]
   for target in selected:
    e=entities[target]
    if p.get('sweep_destroy'):
     if p.get('nontoken_controls') and (e['token'] or 'Indestructible' in o[e['name']]['keywords']):continue
     move(target,'graveyard');continue
    if p.get('target_remove'):
     destination=p['target_remove']
     if destination=='destroy' and 'Indestructible' in o[e['name']]['keywords']:continue
     move(target,{'destroy':'graveyard','exile':'exile','library_top':'library'}[destination])
     if destination=='library_top':zones['library'][e['owner']].insert(0,zones['library'][e['owner']].pop())
     continue
    if p.get('fixed_damage'):
     e['damage']+=p['fixed_damage']
     if e['damage']>=e['toughness'] and 'Indestructible' not in o[e['name']]['keywords']:move(target,'graveyard')
    if theft:
     e['controller']=1-e['controller'] if p.get('mass_control')=='swap' else owner;rebinding += bind(target)
    if p['effect']=='tap_blockers':
     if e['blocking']:e['tapped']=True
    elif p['effect']!='preserve':e['tapped']=not e['tapped'] if p['effect']=='invert' else p['effect']=='tap' or p['effect']=='tap_opponent_untap_self' and e['controller']!=owner
    e['power']+=x if p['stats'][0]=='x' else p['stats'][0];e['toughness']+=p['stats'][1]
    if p.get('flying_damage') and 'Flying' in o[e['name']]['keywords']:e['damage']+=p['flying_damage']
    if p['controller_damage']=='target_power':life[e['controller']]-=e['power']
    if p['mana_gain']=='target_cmc':mana[owner][0]+=int(o[e['name']]['cmc'])
   if active:
    if p.get('regeneration_shield'):
     for target in legal:entities[target]['shield']=True
     if mastery:
      for e in entities.values():
       if e['alive'] and e['creature'] and e['controller']==owner:e['shield']=True
     if p.get('spell_mastery') and v not in ('mastery_active','mastery_own_target','mastery_all_targets_lost'):assert not mastery
    if damage_player:life[target_owner]-=p['fixed_damage']
    if 'life above 5 at every resolution' in p['inactive']:assert life[owner]>5
    if p.get('combat_prevention'):
     protected=[ref for ref,e in entities.items() if e['alive'] and e['creature']] if p['combat_prevention']=='all' else selected
     for target in protected:
      entities[target]['prevent_by']=True;entities[target]['prevent_to']=p['combat_prevention'] in ('all','both')
    if p['token']:tokens[owner]+=1
    life[owner]-=x if p['life_loss']=='x' else p['life_loss']
    life[owner]+=p.get('gain_life',0)
    if p.get('set_life') is not None:life[owner]=p['set_life']
    for _ in range(p['draw']):zones['hand'][owner].append(zones['library'][owner].pop(0))
    for pl in (0,1):
     for _ in range(p.get('draw_both',0)):zones['hand'][pl].append(zones['library'][pl].pop(0))
    if p['scry'] and v=='scry_bottom' and owner==0:zones['library'][owner].append(zones['library'][owner].pop(0))
   return rebinding
  def finish(ref='source'):
   steps=resolve_choices()+[step('resolve'),step('assert_choices_consumed'),step('refresh')];steps+=apply(ref);return steps+inspect()+receipts()
  def regenerate_destroy(ref):
   seq=[step('destroy',ref=ref)];e=entities[ref]
   if e.get('shield'):
    e['shield']=False;e['tapped']=True;e['damage']=0;e['attacking']=False;e['blocking']=False
   else:move(ref,'graveyard')
   return seq+inspect()+receipts()
  def combat_result(attackers,blocker=None):
   steps=[step('combat_damage'),step('end_combat')]
   for i,ref in enumerate(attackers):
    e=entities[ref]
    if not e['alive']:continue
    if i==0 and blocker and entities[blocker]['alive']:
     other=entities[blocker]
     damage=0 if e['prevent_by'] or other['prevent_to'] else e['power'];incoming=0 if other['prevent_by'] or e['prevent_to'] else other['power']
     other['damage']+=damage;e['damage']+=incoming
     if other['damage']>=other['toughness']:move(blocker,'graveyard')
     if e['damage']>=e['toughness']:move(ref,'graveyard')
    elif not e['prevent_by']:life[1-e['controller']]-=max(0,e['power'])
   for e in entities.values():e['attacking']=False;e['blocking']=False
   return steps+inspect()+receipts()
  if p.get('regeneration_shield') and v=='damaged_shield':
   actions += [step('choose_target',player=owner,ref=probe),step('cast',ref='regeneration_probe',value=1),step('resolve'),step('assert_choices_consumed')]
   pay(cost_from_oracle(o['Lightning Bolt']));zones['hand'][owner].remove('Lightning Bolt');zones['graveyard'][owner].append('Lightning Bolt');entities[probe]['damage']=3;actions+=inspect()+receipts()
  illegal=v in ('insufficient_mana','wrong_color','wrong_state','wrong_controller','wrong_power','wrong_creature') or v=='wrong_type' and not p['optional'] or v=='no_targets' and p['count']!='x'
  if p.get('schema')==4 and p['optional'] and v=='wrong_controller':illegal=False
  if v=='own_turn_denied':illegal=True
  if p.get('tap_cost') and v in ('empty_battlefield','cost_tapped','cost_wrong_controller','cost_noncreature','cost_too_few'):illegal=True
  cut=0
  if illegal:
   actions += [step('cast' if v in ('insufficient_mana','wrong_color') else 'can_cast',ref='source',value=0),step('assert_choices_consumed')]
  elif v=='countered':
   if p.get('regeneration_shield'):actions.append(step('require_can_cast',ref='source',value=1))
   actions += choices()+[step('choose_priority',ref='counter'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
   paid_cost();pay(cost);pay([0,0,2,0,0,0],0)
   zones['hand'][owner].remove(name);zones['graveyard'][owner].append(name)
   zones['hand'][0].remove('Counterspell');zones['graveyard'][0].append('Counterspell')
  else:
   actions+=cast();cut=len(actions)
   if v in ('lost_target','only_target_lost','mastery_all_targets_lost'):actions.append(step('destroy',ref=chosen[0]));move(chosen[0],'graveyard')
   if v=='bounced_target':
    actions += [step('choose_target',player=owner,ref=chosen[0]),step('cast_response',ref='response',value=1),step('assert_stack',value=2)]+resolve_choices(exclude=(chosen[0],))+[step('resolve_response'),step('assert_stack',value=0),step('assert_choices_consumed')]
    pay([0,0,2,0,0,0]);zones['hand'][owner].remove('Boomerang');zones['graveyard'][owner].append('Boomerang');move(chosen[0],'hand');actions+=apply()+inspect()+receipts()
   else:actions+=finish()
   if p.get('regeneration_shield'):
    if v in ('protected_destroy','shield_consumed','expired_destroy','damaged_shield'):
     actions+=regenerate_destroy(probe)
     if v=='protected_destroy':actions+=regenerate_destroy('unselected')
     if v=='shield_consumed':actions+=regenerate_destroy(probe)
     if v=='expired_destroy':
      # First prove protection, then install a fresh unconsumed shield before
      # cleanup.  Consuming the first shield cannot masquerade as expiry.
      actions+=cast('expiry_second')+finish('expiry_second')+[step('cleanup'),step('refresh')];entities[probe]['shield']=False;actions+=regenerate_destroy(probe)
    if v in ('lethal_damage','zero_toughness','cannot_regenerate'):
     witness={'lethal_damage':'Lightning Bolt','zero_toughness':'Last Gasp','cannot_regenerate':'Terminate'}[v]
     actions += [step('choose_target',player=owner,ref=probe),step('cast',ref='regeneration_probe',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
     pay(cost_from_oracle(o[witness]));zones['hand'][owner].remove(witness);zones['graveyard'][owner].append(witness)
     if v=='lethal_damage':entities[probe].update(shield=False,tapped=True,damage=0)
     else:move(probe,'graveyard')
     actions+=inspect()+receipts()
    if v in ('mastery_active','mastery_own_target','mastery_all_targets_lost','opponent_mastery'):
     if v!='mastery_all_targets_lost':actions+=regenerate_destroy(probe)
     if v=='mastery_own_target':actions+=regenerate_destroy(probe)
     for target in ('mastery_own','mastery_foreign','unselected')+ (('mastery_enemy',) if v=='mastery_own_target' else ()):
      actions+=regenerate_destroy(target)
    if v in ('regeneration_combat','deathtouch_combat','expired_deathtouch'):
     e=entities[probe];actions+=declare_combat([probe],'regeneration_blocker')+[step('combat_damage')];e.update(shield=False,tapped=True,damage=0,attacking=False,blocking=False)
     if p.get('deathtouch_grant'):move('regeneration_blocker','graveyard')
     else:entities['regeneration_blocker']['damage']+=e['power']
     actions += [step('assert_card',ref=probe,property='attacking',value=0)]+inspect()+receipts()+[step('end_combat')]
     if v=='expired_deathtouch':
      actions += [step('cleanup'),step('refresh'),step('turn',player=e['controller'],value=0),step('choose_target',player=owner,ref=probe),step('cast',ref='regen_reset',value=1),step('choose_dialog',player=owner,value=1),step('resolve'),step('assert_choices_consumed')]
      pay(cost_from_oracle(o['Twiddle']));zones['hand'][owner].remove('Twiddle');zones['graveyard'][owner].append('Twiddle');e['tapped']=False
      actions+=declare_combat([probe],'expiry_blocker')+[step('combat_damage'),step('end_combat')];entities['expiry_blocker']['damage']+=e['power'];move(probe,'graveyard');actions+=inspect()+receipts()
   if p.get('indestructible_grant') and v in ('protected_destroy','expired_destroy','lethal_damage','zero_toughness'):
    if v in ('protected_destroy','expired_destroy'):
     actions.append(step('destroy',ref=probe));actions+=inspect()+receipts()
     if v=='protected_destroy':actions.append(step('destroy',ref='unselected'));move('unselected','graveyard')
     else:
      actions += [step('cleanup'),step('refresh'),step('destroy',ref=probe)];move(probe,'graveyard')
    else:
     witness='Lightning Bolt' if v=='lethal_damage' else 'Last Gasp'
     actions += [step('choose_target',player=owner,ref=probe),step('cast',ref='protection_probe',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
     pay(cost_from_oracle(o[witness]));zones['hand'][owner].remove(witness);zones['graveyard'][owner].append(witness)
     if v=='lethal_damage':entities[probe]['damage']+=3
     else:move(probe,'graveyard')
    actions+=inspect()+receipts()
   if v=='two_copies':
    # Reset one surviving target with a real spell, keeping the second effect
    # observable even for spells with no cumulative stat bonus.
    if p.get('sweep_destroy'):
     actions += [step('cast',ref='sweep_later',value=1),step('resolve'),step('assert_choices_consumed'),step('refresh')]
     pay([1,0,0,1,0,0]);zones['hand'][owner].remove('Grizzly Bears');entities['sweep_later']['alive']=True;entities['sweep_later']['hand']=False;actions+=inspect()+receipts()
    elif p.get('set_life') is not None:
     life[owner]=p['start_life'];actions += [step('set_life',player=owner,value=p['start_life']),step('assert_life',player=owner,value=p['start_life'])]
    elif p.get('tap_cost'):
     actions += [step('cast',ref='cost_reset',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed'),step('refresh')]
     pay([0,0,0,1,0,0]);zones['hand'][owner].remove('Mobilize');zones['graveyard'][owner].append('Mobilize')
     for e in entities.values():
      if e['alive'] and e['controller']==owner and e['creature']:e['tapped']=False
     actions+=inspect()+receipts()
    elif p['return_tap']:
     # The first temporary theft makes this opponent-only target illegal for
     # a second cast. End that effect, observe its mandatory return tap, and
     # retain the exact remaining pools: this helper dispatches end phases
     # without performing a full turn transition or clearing floating mana.
     actions += [step('cleanup'),step('resolve'),step('refresh')]
     for target in chosen:
      entities[target]['controller']=entities[target]['owner'];entities[target]['tapped']=True;actions+=bind(target)
     actions+=inspect()+[step('assert_mana',player=pl,color=i,value=n) for pl in (0,1) for i,n in enumerate(mana[pl])]
     actions += [step('turn',player=owner,value=0),step('refresh')]
    elif p.get('repeat_new_target'):chosen[:]=['unselected']
    elif probe:
     reset=not entities[probe]['tapped'];actions += [step('choose_target',player=owner,ref=probe),step('cast',ref='reset',value=1),step('choose_dialog',player=owner,value=0 if reset else 1),step('resolve'),step('assert_choices_consumed')]
     pay([0,0,1,0,0,0]);zones['hand'][owner].remove('Twiddle');zones['graveyard'][owner].append('Twiddle');entities[probe]['tapped']=reset;actions+=inspect()
    actions+=cast('second')+finish('second')
   if p.get('regeneration_shield') and v=='two_copies':
    actions+=regenerate_destroy('recipient')+regenerate_destroy('unselected')
   if v=='stolen_destroyed':actions.append(step('destroy',ref=probe));move(probe,'graveyard')
   if v=='stolen_bounced':
    actions += [step('choose_target',player=owner,ref=probe),step('cast',ref='response',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
    pay([0,0,2,0,0,0]);zones['hand'][owner].remove('Boomerang');zones['graveyard'][owner].append('Boomerang');move(probe,'hand')
   if v=='mana_source' and probe:
    e=entities[probe];can=not e['tapped'];actions += [step('require_can_activate',ref=probe,value=int(can)),step('activate',ref=probe,value=int(can)),step('assert_stack',value=0),step('assert_choices_consumed')]
    if can:e['tapped']=True;mana[e['controller']][3 if e['name']=='Forest' else 0]+=1 if e['name']=='Forest' else 2
   if v=='trample_block' and probe:
    e=entities[probe];actions += [step('begin_attackers'),step('attack',ref=probe,value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='blocker',other=probe,value=1),step('advance_combat'),step('combat_damage'),step('end_combat')]
    e['tapped']=True;e['damage']+=1;life[1-owner]-=max(0,e['power']-1);move('blocker','graveyard')
   if v=='combat_attack' and probe:
    e=entities[probe];can=not e['tapped'];actions += [step('begin_attackers'),step('attack',ref=probe,value=int(can)),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('end_combat')]
    if can:e['tapped']=True;life[1-e['controller']]-=max(0,e['power'])*(2 if 'Double strike' in p.get('combat_keywords',[]) else 1)
   if v in ('flying_block','keyword_cleanup') and probe:
    if v=='keyword_cleanup':actions += [step('cleanup'),step('refresh'),step('turn',player=owner,value=0)]
    e=entities[probe];blocks=v=='keyword_cleanup'
    actions += [step('begin_attackers'),step('attack',ref=probe,value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='blocker',other=probe,value=int(blocks)),step('advance_combat'),step('combat_damage'),step('end_combat')]
    e['tapped']=True
    if blocks:e['damage']+=1;move('blocker','graveyard')
    else:life[1-owner]-=2*e['power']
   if v in ('cleanup','flying_cleanup'):
    actions += [step('cleanup')]+([step('resolve')] if p['return_tap'] else [])+[step('refresh')]
    for ref,e in entities.items():
     if not e['alive']:continue
     if e['creature']:e['power']=int(o[e['name']]['power']);e['toughness']=int(o[e['name']]['toughness']);e['damage']=0
     if theft and (ref in chosen or p.get('mass_control')):
      e['controller']=e['owner'];actions+=bind(ref)
      if p['return_tap']:e['tapped']=True
  if p.get('regeneration_shield') and v=='countered':actions+=regenerate_destroy(probe)
  if combat_active and combat_attackers:
   actions+=combat_result(combat_attackers,combat_blocker)
   if v=='prevent_expired':
    actions += [step('cleanup'),step('refresh')]
    for e in entities.values():e['prevent_by']=e['prevent_to']=False;e['damage']=0
    ref=combat_attackers[0]
    actions += [step('turn',player=entities[ref]['controller'],value=0),step('choose_target',player=owner,ref=ref),step('cast',ref='expiry_reset',value=1),step('choose_dialog',player=owner,value=1),step('resolve'),step('assert_choices_consumed')]
    pay([0,0,1,0,0,0]);zones['hand'][owner].remove('Twiddle');zones['graveyard'][owner].append('Twiddle');entities[ref]['tapped']=False
    actions+=declare_combat([ref])+combat_result([ref])
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]+inspect()+receipts()
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
   at=0 if v=='ready_snapshot' else cut if v=='pending_snapshot' else len(actions)
   tail=actions[at:]+checks
   steps=pre+actions[:at]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  output.append(dict(name=f'TapSpell3Recipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family='tap_spell',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps']+['untap-step dispatch','control-changing responses','multiplayer'],inactive_conditions=p['inactive'])))
 return output


# Schema5 uses an explicit real untap-step operation. Older reviewed branches stay unchanged.
def _variants5(p):
 name=p['source_name']
 modes=['next_step','countered','insufficient_mana','wrong_color','ready_snapshot','pending_snapshot','locked_snapshot']
 if p['model']=='player_next_untap':modes+=['artifact_function']
 if p['recipient']=='any':modes+=['target_self']
 if p['model']=='selected_next_untap' and p['count']==2:modes+=['split_controllers']
 if p.get('persistent_theft'):modes+=['artifact_target','artifact_creature_target','enchantment_target','own_target','foreign_target','lost_target','bounced_target','stolen_destroyed','stolen_bounced','stolen_sick','retained_attack','wrong_land','wrong_planeswalker']
 if p.get('exile_top'):modes+=['small_library','empty_library']
 return [mode+'_'+str(owner) for owner in (0,1) for mode in modes]

def _expand5(name,csvid,d,p,selected,rows,o):
 validate_witnesses(o,rows)
 if not isinstance(selected,list) or not selected or len(set(selected))!=len(selected) or any(v not in _variants5(p) for v in selected):raise ValueError('explicit distinct schema5 tap variants required')
 result=[]
 for variant in selected:
  c=_next_untap_case5(name,csvid,variant,p,rows,o)
  diagnostic=c.pop('_draft')
  c['_recipe']=dict(family='tap_spell',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=diagnostic['gaps'],untap_model_checkpoints=diagnostic['untap_checkpoints'],checkpoint_index_basis=diagnostic.get('checkpoint_index_basis','no future untap in payment-negative control'))
  result.append(c)
 return result

def _next_untap_case5(name,csvid,variant,p,rows,o):
 from recipes import step,cost_from_oracle
 ORACLE=o;IDS={n:r['csvid'] for n,r in rows.items()}
 HAND=['Plains'];GRAVE=['Hill Giant'];EXILE=['Plains'];LIBRARY=['Forest','Island','Grizzly Bears']
 def cost(n):return cost_from_oracle(o[n])
 v,owner=variant.rsplit('_',1);owner=int(owner)
 recipient=owner if v=='target_self' or p['recipient']=='self' else 1-owner
 actor=recipient if name=='Telekinesis' else owner
 setup=[];entities={};zones={z:[list(cards),list(cards)] for z,cards in [('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY)]}
 if p.get('exile_top'):zones['library'][owner]=['Forest']*(0 if v=='empty_library' else 2 if v=='small_library' else 4)
 mana=[[0]*6 for _ in (0,1)];mana[owner]=cost(name);life=[20,20]
 countered=v=='countered';invalid=p.get('persistent_theft') and v in ('wrong_land','wrong_planeswalker');denied=v in ('insufficient_mana','wrong_color') or invalid
 if countered:mana[0]=[x+y for x,y in zip(mana[0],cost('Counterspell'))]
 if p.get('persistent_theft') and v in ('bounced_target','stolen_bounced'):mana[owner]=[x+y for x,y in zip(mana[owner],cost('Boomerang'))]
 if v=='insufficient_mana':mana[owner]=[0]*6
 if v=='wrong_color':
  color=next(i for i in range(1,6) if mana[owner][i]);mana[owner][1 if color!=1 else 2]+=mana[owner][color];mana[owner][color]=0
 pre=[step('turn',player=actor,value=0)];actions=[];locks={};original_locks={};checkpoints=[];visits=[0,0]
 for player in (0,1):
  for zone,cards in zones.items():
   setup.extend(dict(name=card,ref=f'{zone}_{player}_{i}',player=player,zone=zone) for i,card in enumerate(cards[player]))
 def add(card,ref,controller,zone='battlefield',tapped=False,foreign=False):
  item=dict(name=card,ref=ref,player=controller,zone=zone)
  if zone=='battlefield':item['state']=['tapped' if tapped else 'untapped']+(['opponent_owned'] if foreign else [])
  setup.append(item);r=ORACLE[card];creature='Creature' in r['type_line']
  entities[ref]=dict(name=card,controller=controller,owner=1-controller if foreign else controller,alive=zone=='battlefield',tapped=tapped,creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,damage=0)
  if zone!='battlefield':zones[zone][controller].append(card)
 for player in (0,1):
  if invalid:continue
  for ref,card,tapped,foreign in [('creature','Grizzly Bears',True,False),('ready_creature','Hill Giant',False,False),('artifact_creature','Phyrexian Hulk',True,False),('land','Forest',True,False),('ready_land','Island',False,False),('artifact','Sol Ring',True,False),('ready_artifact','Darksteel Relic',False,False),('foreign_creature','Grizzly Bears',True,True),('foreign_land','Forest',True,True)]:
   add(card,f'{ref}_{player}',player,tapped=tapped,foreign=foreign)
 add(name,'source',owner,'hand')
 selected=[]
 if invalid:add('Forest' if v=='wrong_land' else 'Jace Beleren','invalid_target',1-owner,tapped=True)
 if p.get('persistent_theft') and not invalid:
  target_name={'artifact_target':'Mind Stone','artifact_creature_target':'Obsianus Golem','enchantment_target':'Omniscience'}.get(v,'Air Elemental')
  add(target_name,'theft_target',owner if v=='own_target' else 1-owner,tapped=v!='stolen_sick',foreign=v=='foreign_target');selected=['theft_target']
 if name=='Frost Breath':
  selected=[f'creature_{owner if v=="split_controllers" else recipient}',f'artifact_creature_{recipient}']
 if name=='Telekinesis':
  add('Serra Angel','selected_angel',recipient,tapped=False,foreign=True);selected=['selected_angel']
 for ref in selected:
  if p.get('persistent_theft'):continue
  entities[ref]['tapped']=False;next(x for x in setup if x['ref']==ref)['state']=['untapped']+(['opponent_owned'] if entities[ref]['owner']!=entities[ref]['controller'] else [])
 if countered:add('Counterspell','counter',0,'hand')
 if v=='artifact_function':add('Mind Stone','functional_stone',recipient,'hand')
 if p.get('persistent_theft') and v in ('bounced_target','stolen_bounced'):add('Boomerang','theft_response',owner,'hand')
 if p.get('own_land_lock') and not denied:add('Eon Hub','upkeep_control',owner,tapped=True)
 if p.get('exile_top'):assert all('Land' in o[n]['type_line'].partition(' — ')[0].split() for n in zones['library'][owner])
 if denied:
  # A ready Island could otherwise fund the missing printed U.
  for ref,e in entities.items():
   if e['alive'] and e['controller']==owner and e['name'] in ('Forest','Island','Sol Ring'):
    e['tapped']=True;next(x for x in setup if x['ref']==ref)['state']=['tapped']+(['opponent_owned'] if e['owner']!=e['controller'] else [])
 for player in (0,1):pre += [step('set_mana',player=player,color=color,value=value) for color,value in enumerate(mana[player])]
 pre += [step('refresh')]
 def inspect():
  result=[]
  for ref,e in entities.items():
   if not e['alive']:continue
   result += [step('assert_card',ref=ref,property='in_play',value=1),step('assert_card',ref=ref,property='tapped',value=int(e['tapped']))]
   if e['creature']:result += [step('assert_card',ref=ref,property=k,value=e[k]) for k in ('power','toughness','damage')]
  return result
 def receipts():
  result=[];universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
  for player in (0,1):
   bf=[e['name'] for e in entities.values() if e['alive'] and e['controller']==player]
   result += [step('assert_life',player=player,value=life[player]),step('assert_zone',player=player,zone='battlefield',value=len(bf))]
   result += [step('assert_zone_multiplicity',player=player,zone='battlefield',index=IDS[n],value=bf.count(n)) for n in sorted(universe)]
   result += [step('assert_mana',player=player,color=color,value=value) for color,value in enumerate(mana[player])]
   for zone,cards in zones.items():
    result += [step('assert_zone',player=player,zone=zone,value=len(cards[player]))]
    result += [step('assert_zone_multiplicity',player=player,zone=zone,index=IDS[n],value=cards[player].count(n)) for n in sorted(universe)]
    if zone=='library':result += [step('assert_zone_card',player=player,zone=zone,index=i,value=IDS[n]) for i,n in enumerate(cards[player])]
  return result
 def pay(card,player):mana[player]=[x-y for x,y in zip(mana[player],cost(card))]
 def choices():
  if name=='Misstep':return [step('choose_player',player=owner,value=recipient)]
  return [step('choose_target',player=owner,ref=ref) for ref in selected]
 if name=='Telekinesis' and not denied:
  actions += [step('begin_attackers'),step('attack',ref='selected_angel',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat')]
  # The vigilant attacker really attacks without tapping, then the spell must tap it.
  actions+=inspect()
 if denied:
  actions += [step('can_cast' if invalid else 'cast',ref='source',value=0),step('assert_choices_consumed')]+inspect()+receipts();pending_cut=None
 elif countered:
  actions += [step('require_can_cast',ref='source',value=1)]+choices()+[step('choose_priority',ref='counter'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed'),step('assert_stack',value=0)]
  pay(name,owner);pay('Counterspell',0);zones['hand'][owner].remove(name);zones['hand'][0].remove('Counterspell');zones['graveyard'][owner].append(name);zones['graveyard'][0].append('Counterspell');pending_cut=None
 else:
  actions += [step('require_can_cast',ref='source',value=1)]+choices()+[step('cast',ref='source',value=1),step('assert_stack',value=1),step('assert_card',ref='source',property='in_hand',value=0)]
  pay(name,owner);actions+=inspect()+receipts();pending_cut=len(actions)
  if p.get('persistent_theft') and v=='bounced_target':
   actions += [step('choose_target',player=owner,ref='theft_target'),step('cast_response',ref='theft_response',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_choices_consumed'),step('assert_stack',value=0),step('refresh')]
   pay('Boomerang',owner);zones['hand'][owner].remove('Boomerang');zones['graveyard'][owner].append('Boomerang');e=entities['theft_target'];e['alive']=False;zones['hand'][e['owner']].append(e['name'])
  else:
   if p.get('persistent_theft') and v=='lost_target':
    actions.append(step('destroy',ref='theft_target'));e=entities['theft_target'];e['alive']=False;zones['graveyard'][e['owner']].append(e['name'])
   if p.get('exile_top') and zones['library'][owner]:actions.append(step('choose_zone',player=0,value=-1))
   actions += [step('resolve'),step('assert_choices_consumed'),step('assert_stack',value=0),step('refresh')]
  zones['hand'][owner].remove(name);zones['graveyard'][owner].append(name)
  active=not p.get('persistent_theft') or entities['theft_target']['alive']
  if active and p.get('persistent_theft'):
   e=entities['theft_target'];e['controller']=owner;actions += [step('bind_permanent_copy',ref='theft_target',player=owner,value=IDS[e['name']],index=0)]
  if active and p.get('exile_top'):
   cards=zones['library'][owner][:p['exile_top']];zones['library'][owner]=zones['library'][owner][len(cards):];zones['exile'][owner]+=cards
  if not active:selected_for_lock=[]
  elif p['model']=='player_next_untap':
   selected_for_lock=[ref for ref,e in entities.items() if e['alive'] and e['controller']==recipient and any(t in ORACLE[e['name']]['type_line'].partition(' — ')[0].split() for t in p['types'])]
  else:selected_for_lock=selected
  for ref in selected_for_lock:
   locks[ref]=p['steps'];original_locks[ref]=p['steps']
   if p['tap']:entities[ref]['tapped']=True
  actions+=inspect()+receipts()
  if p.get('persistent_theft') and v in ('stolen_destroyed','stolen_bounced','foreign_target'):
   e=entities['theft_target'];e['alive']=False
   if v=='stolen_bounced':
    actions += [step('choose_target',player=owner,ref='theft_target'),step('cast',ref='theft_response',value=1),step('resolve'),step('assert_choices_consumed')];pay('Boomerang',owner);zones['hand'][owner].remove('Boomerang');zones['graveyard'][owner].append('Boomerang');zones['hand'][e['owner']].append(e['name'])
   else:actions.append(step('destroy',ref='theft_target'));zones['graveyard'][e['owner']].append(e['name'])
   actions+=inspect()+receipts()
  if p.get('persistent_theft') and v=='stolen_sick':
   actions += [step('begin_attackers'),step('attack',ref='theft_target',value=0),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('end_combat')]+inspect()+receipts()
 if name=='Telekinesis' and not denied:
  actions += [step('combat_damage'),step('end_combat')]
  if countered:life[1-recipient]-=4
  actions+=inspect()+receipts()
 if denied:
  # Cost controls stop here. The unspent wrong-color pool must not be
  # silently cleared or carried into a future real untap/mana-burn phase.
  return dict(name=f'TapSpell5Recipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=pre+actions+[step('assert_stack',value=0)]+inspect()+receipts(),
   _draft=dict(status='requires_explicit_untap_step',future_ops=[],variant=variant,caster=owner,recipient=recipient,source_profile=p,untap_checkpoints=[],gaps=['payment-negative control only; no phase transition attempted']))
 current=actor;locked_cut=None;function_used=False
 # Each original locked permanent must be seen beyond its full skipped-step
 # duration; the other controller also gets a real, unaffected untap step.
 while True:
  actions += [step('advance_turn')];current=1-current;visits[current]+=1
  assert mana==[[0]*6,[0]*6]
  op_index=len(actions);actions += [step('untap_step',player=current)]
  blocked=[]
  for ref,e in entities.items():
   if not e['alive'] or e['controller']!=current:continue
   if locks.get(ref,0)>0:
    if e['tapped']:blocked.append(ref)
    locks[ref]-=1
   else:e['tapped']=False
  actions += [step('assert_choices_consumed'),step('assert_stack',value=0)]+inspect()+receipts()
  checkpoints.append(dict(kind='untap',action_index=op_index,player=current,ordinal=visits[current],blocked=blocked,expected_tapped={ref:int(e['tapped']) for ref,e in entities.items() if e['alive']},floating_mana=copy.deepcopy(mana)))
  if blocked and locked_cut is None:locked_cut=len(actions)
  if v=='artifact_function' and current==recipient and not function_used:
   ref=f'artifact_{recipient}';assert not entities[ref]['tapped']
   actions += [step('turn',player=recipient,value=0),step('require_can_activate',ref=ref,value=1),step('activate',ref=ref,value=1),step('assert_choices_consumed'),step('assert_stack',value=0)]
   entities[ref]['tapped']=True;mana[recipient][0]+=2;actions+=inspect()+receipts()
   actions += [step('cast',ref='functional_stone',value=1),step('resolve'),step('assert_choices_consumed'),step('assert_stack',value=0)]
   pay('Mind Stone',recipient);zones['hand'][recipient].remove('Mind Stone');entities['functional_stone']['alive']=True;actions+=inspect()+receipts();function_used=True
  if p.get('persistent_theft') and v=='retained_attack' and current==owner and not function_used:
   actions += [step('turn',player=owner,value=0),step('begin_attackers'),step('attack',ref='theft_target',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('end_combat')]
   entities['theft_target']['tapped']=True;life[1-owner]-=entities['theft_target']['power'];actions+=inspect()+receipts();function_used=True
  if all(visits[e['controller']]>original_locks[ref] for ref,e in entities.items() if ref in original_locks) and min(visits)>=1:break
  assert sum(visits)<=8
 checks=[step('assert_stack',value=0),step('assert_choices_consumed')]+inspect()+receipts()
 if v in ('ready_snapshot','pending_snapshot','locked_snapshot'):
  cut={'ready_snapshot':0,'pending_snapshot':pending_cut,'locked_snapshot':locked_cut}[v];assert cut is not None
  tail=actions[cut:]+checks
  result=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
 else:result=pre+actions+checks
 return dict(name=f'TapSpell5Recipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=result,
  _draft=dict(status='requires_explicit_untap_step',future_ops=['untap_step'],variant=variant,caster=owner,recipient=recipient,source_profile=p,untap_checkpoints=checkpoints,
   checkpoint_index_basis='zero-based unsnapshotted actions, excluding prelude; snapshot expansion duplicates selected tail',
   gaps=['late entering permanents and controller changes require separate rules/engine review','upkeep/draw triggers','phasing and skip-untap effects','optional or mandatory untap-trigger UI outside the vanilla smoke cohort']+(['Hazoret free-cast branches: caster library contains only lands, whose exile cannot be cast as spells'] if p.get('exile_top') else [])+(['normal intervening-upkeep expiry is untested; Eon Hub supplies the skipped-upkeep context; no draw or full turn progression claimed'] if p.get('own_land_lock') else [])))


"""Schema8: reviewed qualified search and own-graveyard target quantities."""
def _variants8(p):
 modes=['effect','payment','insufficient_mana','wrong_color','decline','empty_zone','no_match','one_match','duplicates','cleanup','two_copies','ready_snapshot','pending_snapshot','resolved_snapshot','countered']
 if p['instant']:modes+=['opposing_turn']
 if p['count']==2:modes+=['one_selected']
 if p['zone']=='graveyard':modes+=['only_target_lost','partial_target_lost']
 else:modes+=['found_destroyed','found_bounced']
 if p['scope']=='basic_land':modes+=['snow_basic','nonbasic_control','tapped_entry']
 if p['scope']=='creature' and p['zone']=='library':modes+=['sick_entry']
 if p['scope']=='artifact':modes+=['artifact_function']
 if p['variable']:return [f'{mode}_x{x}_{owner}' for owner in (0,1) for mode in modes for x in ([0,1,2,3] if mode in ('effect','payment','no_match') else [2])]
 return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]


def _quantity_eligible8(record,p,x):
 types=set(record['type_line'].partition(' — ')[0].split())
 return ({'Basic','Land'}<=types if p['scope']=='basic_land' else p['scope'].title() in types) and (not p['variable'] or record['cmc']<=x)


def _expand8(name,csvid,d,p,selected,rows,o):
 from recipes import step,cost_from_oracle
 validate_witnesses(o,rows)
 if not isinstance(selected,list) or not selected or len(selected)!=len(set(selected)) or set(selected)-set(_variants8(p)):raise ValueError('explicit distinct schema8 variants required')
 return [_quantity_case8(name,csvid,d,p,variant,rows,o) for variant in selected]


def _quantity_case8(name,csvid,d,p,variant,rows,o):
 from recipes import step,cost_from_oracle
 v,owner=variant.rsplit('_',1);owner=int(owner);x=0
 if p['variable']:v,x=v.rsplit('_x',1);x=int(x)
 zones={z:[list(cards),list(cards)] for z,cards in [('hand',['Plains']),('graveyard',['Scathe Zombies','Grizzly Bears','Hill Giant','Forest','Sol Ring']),('exile',['Island']),('library',['Forest','Island','Plains','Mountain','Grizzly Bears','Merfolk of the Pearl Trident','Ornithopter','Scathe Zombies','Sol Ring','Mind Stone','Darksteel Citadel','Darksteel Ingot','Seal of Fire'])]}
 zone=p['zone'];original_order=[True,True];setup=[];entities={};mana=[[0]*6,[0]*6];shuffled=False
 preferences={'creature':['Ornithopter','Merfolk of the Pearl Trident','Grizzly Bears','Scathe Zombies'],'artifact':['Darksteel Citadel','Sol Ring','Mind Stone','Darksteel Ingot'],'basic_land':['Forest','Island','Plains','Mountain']}
 if p['variable']:primary=preferences[p['scope']][x]
 else:primary='Scathe Zombies' if zone=='graveyard' else 'Forest'
 desired=[primary]
 if p['count']==2:desired=['Scathe Zombies','Grizzly Bears'] if zone=='graveyard' else ['Forest','Island']
 if v=='one_selected':desired=desired[:1]
 if v=='only_target_lost':desired=desired[:1]
 if v in ('decline','empty_zone','no_match'):desired=[]
 if v=='empty_zone':zones[zone][owner]=[]
 if v=='no_match':zones[zone][owner]=['Seal of Fire','Lightning Bolt','Tundra']
 if v=='one_match':zones[zone][owner]=['Seal of Fire',primary,'Tundra'];desired=[primary]
 if v=='duplicates':zones[zone][owner]=['Seal of Fire',primary,primary,'Tundra'];desired=[primary]*p['count']
 if v=='snow_basic':zones[zone][owner]=['Snow-Covered Forest','Tundra','Darksteel Citadel'];desired=['Snow-Covered Forest']
 if v=='nonbasic_control':zones[zone][owner]=['Tundra','Darksteel Citadel','Seal of Fire'];desired=[]
 if v=='artifact_function':desired=['Sol Ring']
 if v=='sick_entry':desired=['Grizzly Bears']
 printed=cost_from_oracle(dict(o[name],mana_cost=o[name]['mana_cost'].replace('{X}','')));cost=list(printed);cost[0]+=x
 mana[owner]=[a*2 if v=='two_copies' else a for a in cost]
 if v=='effect':mana[owner][0]+=17
 if v=='countered':mana[0]=[a+b for a,b in zip(mana[0],cost_from_oracle(o['Counterspell']))]
 response='Reclaim' if v in ('only_target_lost','partial_target_lost') else 'Boomerang' if v=='found_bounced' else 'Twiddle' if v=='two_copies' and zone=='library' and p['scope'] in ('artifact','creature') else None
 if response:mana[owner]=[a+b for a,b in zip(mana[owner],cost_from_oracle(o[response]))]
 if v=='artifact_function':mana[owner][0]+=0
 if v=='insufficient_mana':mana[owner]=[0]*6
 if v=='wrong_color':
  c=next(c for c in range(1,6) if cost[c]);mana[owner][0]+=mana[owner][c];mana[owner][c]=0
 for player in (0,1):
  for z,cards in zones.items():setup.extend(dict(name=nm,ref=f'{z}_{player}_{i}',player=player,zone=z) for i,nm in enumerate(cards[player]))
 def add(nm,ref,pl,z='battlefield',tapped=False):
  item=dict(name=nm,ref=ref,player=pl,zone=z)
  if z=='battlefield':item['state']=['tapped' if tapped else 'untapped']
  setup.append(item)
  if z=='battlefield':entities[ref]=dict(name=nm,player=pl,tapped=tapped,alive=True,sick=False)
  else:zones[z][pl].append(nm)
 for player in (0,1):
  for nm,ref in [('Hill Giant','creature'),('Darksteel Relic','artifact'),('Mountain','land')]:add(nm,f'{ref}_{player}',player,tapped=True)
 add(name,'source',owner,'hand')
 if v=='two_copies':add(name,'second',owner,'hand')
 if v=='countered':add('Counterspell','counter',0,'hand')
 if response:add(response,'response',owner,'hand')
 if v=='artifact_function':add('Mind Stone','functional_stone',owner,'hand')
 universe={nm for cards in zones.values() for side in cards for nm in side}|{e['name'] for e in entities.values()}|{name}
 pre=[step('turn',player=1-owner if v=='opposing_turn' else owner,value=0),step('observe_search_filters',value=1)]+[step('set_mana',player=pl,color=c,value=value) for pl in (0,1) for c,value in enumerate(mana[pl])]+[step('refresh')];actions=[];pending_cut=None
 def pay(amount,pl):
  mana[pl]=[a-b for a,b in zip(mana[pl],amount)];assert min(mana[pl])>=0
 def receipt():
  seq=[]
  for ref,e in entities.items():
   if not e['alive']:continue
   seq += [step('assert_card',ref=ref,property='in_play',value=1),step('assert_card',ref=ref,property='tapped',value=int(e['tapped']))]
   record=o[e['name']]
   if 'Creature' in record['type_line']:
    seq += [step('assert_card',ref=ref,property=k,value=int(record[k])) for k in ('power','toughness')]+[step('assert_card',ref=ref,property='damage',value=0)]
    if e['sick']:seq.append(step('assert_card',ref=ref,property='sick',value=1))
  for pl in (0,1):
   seq += [step('assert_life',player=pl,value=20),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))]
   seq += [step('assert_zone_multiplicity',player=pl,zone='battlefield',index=rows[nm]['csvid'],value=sum(e['alive'] and e['player']==pl and e['name']==nm for e in entities.values())) for nm in sorted(universe)]
   seq += [step('assert_mana',player=pl,color=c,value=value) for c,value in enumerate(mana[pl])]
   for z,cards in zones.items():
    ns=cards[pl];seq += [step('assert_zone',player=pl,zone=z,value=len(ns))]+[step('assert_zone_multiplicity',player=pl,zone=z,index=rows[nm]['csvid'],value=ns.count(nm)) for nm in sorted(universe)]
    if z=='library' and original_order[pl]:seq += [step('assert_zone_card',player=pl,zone=z,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(ns)]
  return seq
 def selection(wanted,available):
  seq=[step('choose_zone_card',player=owner,value=rows[nm]['csvid']) for nm in wanted]
  remaining=list(available)
  for nm in wanted:remaining.remove(nm)
  legal=[nm for nm in remaining if _quantity_eligible8(o[nm],p,x)]
  if len(wanted)<p['count'] and remaining:seq.append(step('choose_optional_zone',player=owner,value=rows[(legal or remaining)[0]]['csvid']))
  return seq
 def finish_source(ref):zones['hand'][owner].remove(name);zones['graveyard'][owner].append(name)
 def perform(ref,wanted):
  nonlocal pending_cut
  selected=list(wanted);available=list(zones[zone][owner]);seq=[step('require_can_cast',ref=ref,value=1)]
  if p['variable']:seq.append(step('choose_x',player=owner,value=x))
  if p.get('normal_dialog') is not None:seq.append(step('choose_dialog',player=owner,value=p['normal_dialog']))
  if zone=='graveyard':seq+=selection(selected,available)
  if v=='countered':
   seq += [step('choose_priority',ref='counter'),step('cast',ref=ref,value=0),step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed')];pay(cost,owner);pay(cost_from_oracle(o['Counterspell']),0);finish_source(ref);zones['hand'][0].remove('Counterspell');zones['graveyard'][0].append('Counterspell');return seq+receipt()
  seq += [step('cast',ref=ref,value=1),step('assert_stack',value=1),step('assert_card',ref=ref,property='in_hand',value=0)];pay(cost,owner);seq+=receipt()
  if pending_cut is None:pending_cut=len(actions)+len(seq)
  active=True
  if v in ('only_target_lost','partial_target_lost'):
   removed=selected[0];seq += [step('choose_zone_card',player=owner,value=rows[removed]['csvid']),step('cast_response',ref='response',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_stack',value=0),step('assert_choices_consumed')];pay(cost_from_oracle(o['Reclaim']),owner);zones['hand'][owner].remove('Reclaim');zones['graveyard'][owner].append('Reclaim');zones['graveyard'][owner].remove(removed);zones['library'][owner].insert(0,removed);selected=selected[1:];active=bool(selected)
  else:
   if zone=='library':seq+=selection(selected,available)
   seq += [step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed')]
  finish_source(ref)
  if zone=='library' and len(wanted)==p['count'] and v!='countered':
   inspected=list(available)
   for nm in wanted[:-1]:inspected.remove(nm)
   seq += [step('assert_offered_count',value=len(inspected)),step('assert_offered_selection_required',value=0)]
   seq += [step('assert_offered_legal_card',index=rows[nm]['csvid'],value=int(_quantity_eligible8(o[nm],p,x))) for nm in sorted(set(inspected))]
  if zone=='graveyard' and len(wanted)==p['count'] and v not in ('countered','only_target_lost','partial_target_lost'):
   seq += [step('assert_offered_count',value=len(available)),step('assert_offered_legal_card',index=rows[wanted[-1]]['csvid'],value=1)]
   seq += [step('assert_offered_legal_card',index=rows[nm]['csvid'],value=0) for nm in sorted(set(available)) if not _quantity_eligible8(o[nm],p,x)]
  if active:
   if zone=='library':original_order[owner]=False
   for nm in selected:
    zones[zone][owner].remove(nm)
    if p['destination']=='hand':zones['hand'][owner].append(nm)
    else:
     ref_found=f'found_{len(entities)}';idx=sum(e['alive'] and e['player']==owner and e['name']==nm for e in entities.values());entities[ref_found]=dict(name=nm,player=owner,tapped=p['destination']=='battlefield_tapped',alive=True,sick='Creature' in o[nm]['type_line']);seq += [step('bind_permanent_copy',player=owner,value=rows[nm]['csvid'],index=idx,ref=ref_found)]
  seq += [step('refresh')]+receipt()
  return seq
 denied=v in ('insufficient_mana','wrong_color')
 if denied:actions += [step('cast',ref='source',value=0),step('assert_stack',value=0),step('assert_choices_consumed')]+receipt()
 else:
  actions+=perform('source',desired)
  if v=='two_copies':
   if response:
    found=next(ref for ref,e in entities.items() if ref.startswith('found_'));actions += [step('choose_target',player=owner,ref=found),step('cast',ref='response',value=1),step('choose_dialog',player=owner,value=0),step('resolve'),step('assert_choices_consumed')];pay(cost_from_oracle(o['Twiddle']),owner);zones['hand'][owner].remove('Twiddle');zones['graveyard'][owner].append('Twiddle');entities[found]['tapped']=True;actions+=receipt()
   remaining=[nm for nm in zones[zone][owner] if _quantity_eligible8(o[nm],p,x)];actions+=perform('second',remaining[:p['count']])
  if v in ('found_destroyed','found_bounced'):
   found=next(ref for ref in entities if ref.startswith('found_'));e=entities[found]
   if v=='found_destroyed':actions.append(step('destroy',ref=found));dest='graveyard'
   else:
    actions += [step('choose_target',player=owner,ref=found),step('cast',ref='response',value=1),step('resolve'),step('assert_choices_consumed')];pay(cost_from_oracle(o['Boomerang']),owner);zones['hand'][owner].remove('Boomerang');zones['graveyard'][owner].append('Boomerang');dest='hand'
   e['alive']=False;zones[dest][owner].append(e['name']);actions+=receipt()
  if v=='tapped_entry':
   for found in [ref for ref in entities if ref.startswith('found_')]:actions.append(step('activate',ref=found,value=0))
   actions+=receipt()
  if v=='sick_entry':
   found=next(ref for ref in entities if ref.startswith('found_'));actions += [step('begin_attackers'),step('attack',ref=found,value=0),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('end_combat')]+receipt()
  if v=='artifact_function':
   found=next(ref for ref in entities if ref.startswith('found_'));actions += [step('activate',ref=found,value=1),step('assert_stack',value=0),step('assert_choices_consumed')];entities[found]['tapped']=True;mana[owner][0]+=2;actions+=receipt();actions += [step('cast',ref='functional_stone',value=1),step('resolve'),step('assert_choices_consumed')];pay(cost_from_oracle(o['Mind Stone']),owner);zones['hand'][owner].remove('Mind Stone');entities['functional_stone']=dict(name='Mind Stone',player=owner,tapped=False,alive=True,sick=False);actions+=receipt()
  if v=='cleanup':actions += [step('cleanup'),step('refresh')]+receipt()
 checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0)]+receipt();seq=pre+actions+checks
 if v in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
  cut=0 if v=='ready_snapshot' else pending_cut if v=='pending_snapshot' else len(actions);assert cut is not None;tail=actions[cut:]+checks;seq=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
 return dict(name=f'TapSpell8Recipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family='tap_spell',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps']))
