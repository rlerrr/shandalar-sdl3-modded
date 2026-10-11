"""Pinned prowess and unconditional landfall bonuses, exercised by real spells."""
import re
from collections import Counter
from creature_recipes import oracle_keywords
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

FAMILY = 'trigger_pump_creature'
WITNESSES = {
 'Darksteel Relic': ('{0}', 'Indestructible (Effects that say "destroy" don\'t destroy this artifact.)'),
 'Rampant Growth': ('{1}{G}', 'Search your library for a basic land card, put that card onto the battlefield tapped, then shuffle.'),
 "Nature's Lore": ('{1}{G}', 'Search your library for a Forest card, put that card onto the battlefield, then shuffle.'),
 'Giant Growth': ('{G}', 'Target creature gets +3/+3 until end of turn.'),
}

def spec(d, r):
 from recipes import cost_from_oracle
 if not r or r.get('layout') != 'normal' or d.get('oracle_text') != r.get('oracle_text') or 'variants' not in d:
  raise ValueError('pinned normal whole text and explicit variants required')
 p = d.get('params')
 if not isinstance(p, dict) or set(p) != {'trigger', 'power', 'toughness', 'keywords'}:
  raise ValueError('explicit trigger, bonus and passive keywords required')
 if p['trigger'] not in ('prowess', 'landfall') or any(type(p[k]) is not int or not 1 <= p[k] <= 2 for k in ('power', 'toughness')):
  raise ValueError('reviewed trigger and bounded positive bonus required')
 lines = r['oracle_text'].splitlines()
 if p['trigger'].capitalize() not in r.get('keywords', []):raise ValueError('structured Oracle trigger keyword required')
 if p['trigger'] == 'prowess':
  if (p['power'], p['toughness']) != (1, 1):raise ValueError('prowess is exactly +1/+1')
  clauses = {'Prowess', 'Prowess (Whenever you cast a noncreature spell, this creature gets +1/+1 until end of turn.)'}
 else:
  clauses = {f"Landfall — Whenever a land you control enters, {ref} gets +{p['power']}/+{p['toughness']} until end of turn." for ref in ('this creature', r['name'])}
 if sum(s in clauses for s in lines) != 1:raise ValueError('one unconditional self-pump trigger required')
 stripped = dict(r, oracle_text='\n'.join(s for s in lines if s not in clauses), keywords=[k for k in r.get('keywords', []) if k != p['trigger'].capitalize()])
 keys = oracle_keywords(stripped)
 if p['keywords'] != keys or set(keys) & {'Menace', 'Defender', 'Deathtouch', 'Double strike', 'Lifelink'}:
  raise ValueError('reviewed passive keyword set required')
 return dict(family=FAMILY, trigger=p['trigger'], bonus_power=p['power'], bonus_toughness=p['toughness'], keywords=keys, power=int(r['power']), toughness=int(r['toughness']), mana=cost_from_oracle(r), legendary='Legendary' in r['type_line'])

def variants(p):
 vs = ['cast', 'payment', 'insufficient_mana', 'fresh_attack', 'effect', 'two_triggers', 'three_triggers', 'opponent_trigger', 'creature_spell', 'alternate_spell', 'tapped', 'sick', 'source_arrives_late', 'source_destroyed_before', 'unblocked', 'blocked', 'cleanup', 'ready_snapshot', 'pending_snapshot', 'resolved_snapshot', 'bare_zones']
 if not p['legendary']:vs += ['two_sources']
 if any(p['mana'][1:]):vs += ['wrong_color']
 if p['trigger'] == 'landfall':vs += ['nonland_spell', 'failed_search']
 return [v + '_' + str(owner) for owner in (0, 1) for v in vs]

def validate_witnesses(o, registry):
 from attack_pump_recipes import validate_witnesses as creatures
 creatures(o, registry)
 for name, fields in WITNESSES.items():
  r = o.get(name, {})
  if name not in registry or r.get('layout') != 'normal' or (r.get('mana_cost'), r.get('oracle_text')) != fields:
   raise ValueError('trigger pump witness changed: ' + name)

def expand(name, csvid, d, p, selected, registry, oracle):
 from recipes import card, step, observe, cost_from_oracle
 result = []
 for variant in selected:
  v, owner = variant.rsplit('_', 1); owner = int(owner); bare = v == 'bare_zones'
  casted = v in ('cast', 'payment', 'insufficient_mana', 'wrong_color', 'fresh_attack', 'source_arrives_late')
  illegal = v in ('insufficient_mana', 'wrong_color')
  hands = [[] if bare else list(HAND) for _ in (0, 1)]
  graves = [[] if bare else list(GRAVE) for _ in (0, 1)]
  exiles = [[] if bare else list(EXILE) for _ in (0, 1)]
  libs = [['Forest', 'Plains', 'Island'] if bare else list(LIBRARY) for _ in (0, 1)]
  if v == 'failed_search':libs[owner] = ['Grizzly Bears', 'Lightning Bolt']
  mana = [[10]*6, [10]*6]; life = [20, 20]; setup = []; entities = {}; pre = []; actions = []; searched = set()
  for player in (0, 1):
   for zone, names in (('hand', hands[player]), ('graveyard', graves[player]), ('exile', exiles[player]), ('library', libs[player])):
    setup += [card(nm, f'{zone}_{player}_{i}', player, zone) for i, nm in enumerate(names)]
  def add(nm, ref, player, zone='battlefield', state=None):
   c = card(nm, ref, player, zone)
   if state:c['state'] = state
   setup.append(c)
   if zone == 'hand':hands[player].append(nm)
   r = oracle[nm]
   entities[ref] = dict(name=nm, player=player, alive=zone=='battlefield', hand=zone=='hand', power=int(r['power']) if 'Creature' in r['type_line'] else None, toughness=int(r['toughness']) if 'Creature' in r['type_line'] else None, tapped=int(bool(state and 'tapped' in state)), damage=0, attacking=0)
  add(name, 'source', owner, 'hand' if casted else 'battlefield', ['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
  add('Hill Giant', 'idle', owner); add('Grizzly Bears', 'enemy', 1-owner)
  if v=='two_sources':add(name, 'copy', owner)
  if v=='blocked':add('Ornithopter', 'blocker', 1-owner)
  actor = 1-owner if v=='opponent_trigger' else owner
  positive = 'Darksteel Relic' if p['trigger']=='prowess' else 'Rampant Growth'
  spell = 'Grizzly Bears' if v=='creature_spell' else 'Darksteel Relic' if v=='nonland_spell' else ('Giant Growth' if p['trigger']=='prowess' else "Nature's Lore") if v=='alternate_spell' else positive
  count = 3 if v=='three_triggers' else 2 if v=='two_triggers' else 1
  triggering = v not in ('cast', 'payment', 'insufficient_mana', 'wrong_color', 'fresh_attack')
  if triggering:
   for i in range(count):add(spell, f'spell_{i}', actor, 'hand')
  pre += [step('turn', player=actor, value=0)]
  if v=='payment':mana[owner] = list(p['mana'])
  if v=='insufficient_mana':mana[owner] = [0]*6
  if v=='wrong_color':
   mana[owner] = list(p['mana']); c = next(c for c in range(1, 6) if mana[owner][c]); mana[owner][1 if c!=1 else 2] += mana[owner][c]; mana[owner][c] = 0
  for player in (0, 1):pre += [step('set_mana', player=player, color=c, value=n) for c,n in enumerate(mana[player])]
  pre += [step('refresh')]
  def stats():
   seq = []
   for ref,e in entities.items():
    if ref.startswith('spell_'):continue
    seq += [observe(ref, 'in_play', int(e['alive'])), observe(ref, 'in_hand', int(e['hand']))]
    if e['alive']:seq += [observe(ref,k,int(e[k])) for k in ('power','toughness','damage','tapped','attacking')]
   return seq
  def pay(player, r):
   cost = [0]*6 if r['mana_cost']=='{0}' else cost_from_oracle(r)
   mana[player] = [a-b for a,b in zip(mana[player], cost)]
  def cast_source():
   actions.extend([step('cast', ref='source', value=int(not illegal)), step('assert_choices_consumed')])
   if not illegal:
    pay(owner, oracle[name]); hands[owner].remove(name); entities['source'].update(alive=True, hand=False)
    actions.extend([step('resolve'), step('assert_choices_consumed')] + stats())
  if casted and v!='source_arrives_late':cast_source()
  if v=='source_destroyed_before':
   actions += [step('destroy', ref='source')]; entities['source'].update(alive=False, hand=False); graves[owner].append(name)
  pending_cut = 0
  if triggering:
   for i in range(count):
    ref = f'spell_{i}'
    if spell=='Giant Growth':actions += [step('choose_target', player=actor, ref='idle')]
    actions += [step('cast', ref=ref, value=1), step('assert_choices_consumed')]
    pending_cut = len(actions) if i==0 else pending_cut
    if spell in ('Rampant Growth', "Nature's Lore"):
     land = None if v=='failed_search' else next(nm for nm in libs[actor] if ('Basic Land' in oracle[nm]['type_line'] if spell=='Rampant Growth' else 'Forest' in oracle[nm]['type_line']))
     actions += [step('choose_zone', player=-1, value=-1)] if land is None else [step('choose_zone_card', player=-1, value=registry[land]['csvid'])]
    actions += [step('resolve'), step('assert_choices_consumed')]
    pay(actor, oracle[spell]); hands[actor].remove(spell); entities[ref].update(hand=False)
    if spell in ('Rampant Growth', "Nature's Lore"):
     searched.add(actor)
     if v!='failed_search':
      libs[actor].remove(land); entities[ref].update(alive=True, name=land, tapped=int(spell=='Rampant Growth'))
     graves[actor].append(spell)
    elif spell=='Giant Growth':
     entities['idle']['power'] += 3; entities['idle']['toughness'] += 3; graves[actor].append(spell)
    else:entities[ref]['alive'] = True
    valid = actor==owner and v not in ('creature_spell','nonland_spell','failed_search') and v!='source_arrives_late'
    if valid:
     for key in ('source','copy'):
      if key in entities and entities[key]['alive']:
       entities[key]['power'] += p['bonus_power']; entities[key]['toughness'] += p['bonus_toughness']
    actions += stats()
   if v=='source_arrives_late':cast_source()
  if v in ('unblocked','blocked','cleanup','fresh_attack'):
   allowed = v!='fresh_attack' or 'Haste' in p['keywords']
   actions += [step('begin_attackers'), step('attack', ref='source', value=int(allowed)), step('finish_attackers')]
   if allowed:entities['source']['attacking']=1; entities['source']['tapped']=int('Vigilance' not in p['keywords'])
   actions += stats() + [step('begin_blockers')]
   if v=='blocked':actions += [step('block', ref='blocker', other='source', value=1)]
   actions += [step('advance_combat'), step('combat_damage'), step('end_combat')]
   power = entities['source']['power']
   if allowed:
    if v=='blocked':
     if power>=2:entities['blocker']['alive']=False; graves[1-owner].append('Ornithopter')
     else:entities['blocker']['damage']=power
     if 'Trample' in p['keywords']:life[1-owner] -= max(0,power-2)
    else:life[1-owner] -= power
   entities['source']['attacking']=0
  if v=='cleanup':
   actions += stats() + [step('cleanup')]
   for e in entities.values():
    if e['alive'] and e['power'] is not None:e.update(power=int(oracle[e['name']]['power']), toughness=int(oracle[e['name']]['toughness']), damage=0)
  checks = stats() + [step('assert_stack', value=0), step('assert_choices_consumed')]
  for player in (0,1):
   checks += [step('assert_life',player=player,value=life[player]), step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
    checks += [step('assert_zone',player=player,zone=zone,value=len(names))]
    checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' and player not in searched else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(names if zone=='library' and player not in searched else dict.fromkeys(names))]
   checks += [step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=n) for nm,n in Counter(hands[player]+graves[player]).items()]
   if v!='cleanup':checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
   for ref,e in entities.items():
    if ref.startswith('spell_') and e['alive'] and e['player']==player:
     checks += [step('assert_zone_contains',player=player,zone='battlefield',value=registry[e['name']]['csvid'])]
  seq = pre + actions + checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut = 0 if v=='ready_snapshot' else pending_cut
   seq = pre + actions[:cut] + [step('capture')] + actions[cut:] + checks + [step('record_state'),step('ai_roundtrip'),step('restore')] + actions[cut:] + checks + [step('assert_recorded_state')]
  if v=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  result.append(dict(name=f'TriggerPumpRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['countering triggering spells','trigger ordering with unrelated abilities','land play action','multiplayer'])))
 return result
