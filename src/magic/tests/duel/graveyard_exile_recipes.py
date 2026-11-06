"""Pinned graveyard exile components, real costs, and owned zone receipts."""
import copy
import json
from collections import Counter
from pathlib import Path
from grant_recipes import HAND,GRAVE,LIBRARY,EXILE

PINS=json.loads((Path(__file__).parent/'data/graveyard_exile_witnesses.json').read_text(encoding='utf8'))
PROFILES=PINS['profiles']
WITNESSES=tuple(PINS['palette'])


def mana_cost(text):
 from recipes import cost_from_oracle
 return [0]*6 if text in ('','{0}') else cost_from_oracle(dict(mana_cost=text))


def eligible(record,p):
 types=set(record['type_line'].partition(' — ')[0].split())
 return {'card':True,'creature':'Creature' in types,'artifact':'Artifact' in types,
  'instant_or_sorcery':bool(types&{'Instant','Sorcery'}),'nonland':'Land' not in types}[p['scope']]


def spec(d,r):
 capacity=isinstance(d.get('params'),dict) and d['params'].get('mode')=='myr_capacity'
 profile=PINS.get('components',{}).get('myr_capacity') if capacity else PROFILES.get(r.get('name')) if isinstance(r,dict) else None
 if (not profile or type(d.get('version')) is not int or d['version']!=2 or d.get('family')!='graveyard_exile'
  or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
  or any(r.get(k)!=v for k,v in profile['source'].items()) or d.get('oracle_text')!=r.get('oracle_text')
  or not isinstance(d.get('params'),dict) or json.dumps(d['params'],sort_keys=True)!=json.dumps(profile['params'],sort_keys=True)):
  raise ValueError('graveyard exile requires complete source pins and explicit reviewed components')
 p=copy.deepcopy(profile['params'])
 if p['mode']=='flashback':
  import re
  grammar={'draw_player': ('Target player draws two cards.', 2), 'mill_player': ('Target player mills three cards.', 3), 'mill_all': ('Each player mills five cards.', 5), 'bounce_owned': ('Return target creature you own to your hand.', 0), 'select_four': ('Look at the top four cards of your library. Put one of them into your hand and the rest into your graveyard.', 0), 'search_instant': ('Search your library for an instant card or a card with flash, reveal it, put it into your hand, then shuffle.', 1), 'search_ambition': ('Search your library for a card and put that card into your hand. If this spell was cast from a graveyard, instead search your library for two cards and put those cards into your hand. Then shuffle.', 2), 'recover_type': ('Choose a permanent type. Return all cards of the chosen type from your graveyard to your hand.', 0), 'blink_controlled': ("Exile target creature you control, then return it to the battlefield under its owner's control.", 0), 'unblockable': ("Target creature can't be blocked this turn.", 0), 'cannot_block': ("Up to two target creatures can't block this turn.", 0), 'sever_names': ('Exile target creature and all other creatures with the same name as that creature.', 0)}
  types=set(r['type_line'].partition(' \u2014 ')[0].split());text=r['oracle_text']
  if p['effect']=='shuffle_grave':
   effect_match=re.fullmatch(r'Target player shuffles up to (two|three) target cards from their graveyard into their library\.',text.splitlines()[0])
   if not effect_match:raise ValueError('reviewed graveyard-to-library target grammar required')
   grammar['shuffle_grave']=(text.splitlines()[0],{'two':2,'three':3}[effect_match[1]])
  if not types&{'Instant','Sorcery'} or 'Flashback' not in r['keywords'] or p['effect'] not in grammar or text.splitlines()[0]!=grammar[p['effect']][0]:raise ValueError('reviewed noncreature Flashback grammar required')
  match=re.search(r'Flashback(?:\s|\u2014)*((?:\{[0-9WUBRG]+\})+)',text)
  life=re.search(r'Flashback[^.\n]*[Pp]ay ([0-9]+) life',text)
  if not match:raise ValueError('ordinary printed Flashback mana cost required')
  p.update(family='graveyard_exile',mana=mana_cost(r['mana_cost']),flashback_mana=mana_cost(match[1]),flashback_life=int(life[1]) if life else 0,sorcery='Sorcery' in types,amount=grammar[p['effect']][1])
  selected=d.get('variants')
  if not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or len(selected)!=len(set(selected)) or set(selected)-set(variants(p)):raise ValueError('explicit distinct reviewed Flashback variants required')
  return p
 if p['mode']=='myr_capacity':
  if r['name']!='Myr Welder':raise ValueError('Myr Welder capacity component required')
  selected=d.get('variants')
  if not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or len(selected)!=len(set(selected)) or set(selected)-set(variants(p)):raise ValueError('explicit distinct reviewed Myr capacity variants required')
  p.update(family='graveyard_exile')
  return p
 if p['mode']=='delve':
  types=set(r['type_line'].partition(' \u2014 ')[0].split());creature='Creature' in types;effect=p.get('spell_effect')
  grammar={'draw_three':'Draw three cards.','destroy_creature':'Destroy target creature.','destroy_nongreen':"Destroy target nongreen creature. It can't be regenerated.",'pump_six':'Target creature gets +6/+6 until end of turn.'}
  if 'Delve' not in r['keywords'] or p.get('exile_scope')!='own_any' or (creature and effect) or (not creature and (not types&{'Instant','Sorcery'} or effect not in grammar or grammar[effect] not in r['oracle_text'].splitlines())):raise ValueError('reviewed own-graveyard Delve component required')
  p.update(family='graveyard_exile',mana=mana_cost(r['mana_cost']),creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0)
  selected=d.get('variants')
  if not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or len(selected)!=len(set(selected)) or set(selected)-set(variants(p)):raise ValueError('explicit distinct reviewed Delve variants required')
  return p
 if p['mode'] not in ('spell','entry','activation','unearth') or p['quantity'] not in ('one','all','upto') or p['scope'] not in ('card','creature','artifact','instant_or_sorcery','nonland') or p['graveyard'] not in ('any','self','opponent','target_player','all','opponents'):raise ValueError('unreviewed graveyard exile mode, count or predicate')
 if p['quantity']=='one' and p['graveyard'] not in ('any','self','opponent') or p['quantity']=='all' and p['graveyard'] not in ('target_player','all','opponents'):raise ValueError('unreviewed exile target structure')
 if p['quantity']=='upto' and (p['graveyard']!='any' or type(p.get('limit')) is not int or not 1<=p['limit']<=4):raise ValueError('bounded reviewed up-to target count required')
 source_types=set(r['type_line'].partition(' — ')[0].split());creature='Creature' in source_types
 if p['mode']=='spell' and not source_types&{'Instant','Sorcery'}:raise ValueError('ordinary spell source required')
 if p['mode']!='spell' and not source_types&{'Artifact','Creature','Enchantment'}:raise ValueError('ordinary permanent source required')
 p.update(family='graveyard_exile',mana=mana_cost(r.get('mana_cost','')),creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0)
 if p['mode'] in ('activation','unearth'):p['activation_mana']=mana_cost(p['activation']['mana'])
 selected=d.get('variants')
 if not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or len(selected)!=len(set(selected)) or set(selected)-set(variants(p)):raise ValueError('explicit distinct reviewed exile variants required')
 return p


def variants(p):
 if p['mode']=='flashback':return _flashback_variants(p)
 if p['mode']=='myr_capacity':return [f'slot{slot}_{owner}' for owner in (0,1) for slot in p['slots']]
 if p['mode']=='delve':return _delve_variants(p)
 if p['mode']=='unearth':return _unearth_variants(p)
 modes=['effect','payment','empty_graveyards','bare_zones','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot']
 if p['quantity'] in ('one','upto'):modes.append('alternate')
 if p['quantity']=='upto':
  modes += ['zero_targets','one_target','max_targets','duplicate_targets']
  if p['mode']=='spell':modes += ['lost_target','partial_lost_target']
 if p['graveyard'] in ('any','target_player'):modes += ['own_target','sole_own_graveyard','sole_opponent_graveyard']
 if p['scope']!='card':modes.append('wrong_type')
 if p['mode'] in ('spell','entry'):
  modes += ['insufficient_mana','countered']
  if any(p['mana'][1:]):modes.append('wrong_color')
  if not p['disable_repeat']:modes.append('repeat')
  if p['mode']=='entry' and not p.get('skip_destroy'):modes.append('destroy_source')
  if p['mode']=='spell' and p['quantity']=='one':modes.append('lost_target')
 else:
  c=p['activation'];modes += ['cast_source','cast_payment','insufficient_cast_mana','fresh_activation']
  if sum(p['activation_mana']):modes.append('insufficient_mana')
  if any(p['activation_mana'][1:]):modes.append('wrong_color')
  if c['tap']:modes.append('tapped')
  if c['tap'] and p['creature']:modes.append('sick')
  if not (c['tap'] or c['sacrifice'] or c['exile']):modes.append('repeat')
  if not (c['sacrifice'] or c['exile']):modes.append('destroy_source_pending')
 if p.get('token'):
  modes.append('destroy_token')
  if p['token'].get('creature',True):modes += ['bounce_token','fresh_token_attack']
 if p.get('activation_exile_count'):modes += ['exact_exile_cost','short_exile_cost','opponent_only_cost']
 if p.get('variable_exile_count'):modes += ['exile_one','exile_two']
 if p.get('cast_exile_count'):modes += ['exact_creature_cost','short_creature_cost','opponent_only_cost']
 if p.get('graveyard_replacement'):modes += ['replacement_own_death','replacement_opponent_death','replacement_removed','replacement_spell']
 return [f'{v}_{owner}' for owner in (0,1) for v in modes]


def validate_witnesses(o,rows):
 if not isinstance(o,dict):raise ValueError('pinned exile witness context required')
 for n,pin in dict(PINS['cards'],**PINS.get('extra_cards',{})).items():
  if n not in rows or any(o.get(n,{}).get(k)!=v for k,v in pin.items()) or o[n].get('card_faces') or o[n].get('selected_face'):raise ValueError('graveyard exile witness changed: '+n)
 for n,p in PROFILES.items():
  if n not in rows or any(o.get(n,{}).get(k)!=v for k,v in p['source'].items()):raise ValueError('graveyard exile source changed: '+n)
 for component in PINS.get('components',{}).values():
  for pin in [component['source']]+list(component['witnesses'].values()):
   n=pin['name']
   if n not in rows or any(o.get(n,{}).get(k)!=v for k,v in pin.items()) or o[n].get('card_faces') or o[n].get('selected_face'):raise ValueError('graveyard exile component witness changed: '+n)


def expand(name,csvid,d,p,selected,rows,o):
 if p['mode']=='flashback':return _expand_flashback(name,csvid,d,p,selected,rows,o)
 if p['mode']=='myr_capacity':return _expand_myr_capacity(name,csvid,d,p,selected,rows,o)
 if p['mode']=='delve':return _expand_delve(name,csvid,d,p,selected,rows,o)
 if p['mode']=='unearth':return _expand_unearth(name,csvid,d,p,selected,rows,o)
 from recipes import card,step,observe
 result=[];candidates=[n for n in WITNESSES if eligible(o[n],p)]
 if p['quantity']=='one' and len(candidates)<2:raise ValueError('two independent eligible exile witnesses required')
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);respond=v in ('lost_target','partial_lost_target');activation=p['mode']=='activation';entry=p['mode']=='entry';bare=v=='bare_zones'
  cost_count=p.get('cast_exile_count',0);ability_cost_count=p.get('activation_exile_count',0)
  casting=not activation or v in ('cast_source','cast_payment','insufficient_cast_mana','fresh_activation')
  cast_only=activation and v in ('cast_source','cast_payment','insufficient_cast_mana')
  target_owner=owner if v=='own_target' or p['graveyard']=='self' else 1-owner
  zones={z:[[],[]] for z in ('hand','graveyard','library','exile')}
  for pl in (0,1):
   zones['hand'][pl]=[] if bare else list(HAND);zones['graveyard'][pl]=list(GRAVE)+list(WITNESSES)
   zones['library'][pl]=list(LIBRARY) if not bare or p['draw'] else [];zones['exile'][pl]=[] if bare else list(EXILE)
  if bare:
   zones['graveyard']=[[],[]]
   if p['quantity']=='all' and p['graveyard']=='all':zones['graveyard']=[list(candidates[:2]),list(candidates[:2])]
   else:zones['graveyard'][target_owner]=list(candidates[:2])
  if cost_count:
   if bare:zones['graveyard'][owner]=list(candidates[:cost_count])
   if v=='repeat':zones['graveyard'][owner] += list(candidates[:cost_count])
   if v in ('exact_creature_cost','short_creature_cost'):zones['graveyard'][owner]=list(candidates[:cost_count-(v=='short_creature_cost')])
   if v=='opponent_only_cost':zones['graveyard'][owner]=[]
  if ability_cost_count:
   if bare:zones['graveyard'][owner]=list(candidates[:max(ability_cost_count,p.get('variable_exile_count',0))])
   if v in ('exact_exile_cost','short_exile_cost'):zones['graveyard'][owner]=list(candidates[:ability_cost_count-(v=='short_exile_cost')])
   if v=='opponent_only_cost':zones['graveyard'][owner]=[]
  if v=='empty_graveyards':zones['graveyard']=[[],[]]
  if v=='wrong_type':zones['graveyard']=[[n for n in WITNESSES if not eligible(o[n],p)] for _ in (0,1)]
  if v=='sole_own_graveyard':zones['graveyard'][1-owner]=[];target_owner=owner
  if v=='sole_opponent_graveyard':zones['graveyard'][owner]=[];target_owner=1-owner
  target=candidates[-1] if v=='alternate' else candidates[0] if candidates else None
  if p['quantity']=='one' and p['graveyard']=='any' and not any(eligible(o[n],p) for n in zones['graveyard'][target_owner]) and any(eligible(o[n],p) for n in zones['graveyard'][1-target_owner]):target_owner=1-target_owner
  setup=[]
  for pl in (0,1):
   for z in zones:setup += [card(n,f'p{pl}_{z}_{i}',pl,z) for i,n in enumerate(zones[z][pl])]
  source=card(name,'source',owner,'hand' if casting else 'battlefield')
  source_tapped=activation and not casting and p['setup_tapped'] or v=='tapped'
  if source_tapped:source['state']=['tapped']
  if v=='sick' and p['creature']:source['state']=['sick']
  setup.append(source)
  if casting:zones['hand'][owner].append(name)
  entities={'source':dict(alive=not casting,hand=casting,tapped=bool(source_tapped))}
  if p.get('battlefield_controls'):
   for pl in (0,1):
    for index,nm in enumerate(('Grizzly Bears','Sol Ring','Seal of Cleansing','Forest')):
     ref=f'zone_control_{pl}_{index}';obj=card(nm,ref,pl);obj['state']=['tapped'];setup.append(obj);entities[ref]=dict(name=nm,player=pl,alive=True,hand=False,tapped=True)
  if v in ('replacement_own_death','replacement_opponent_death','replacement_removed'):
   pl=1-owner if v=='replacement_opponent_death' else owner
   setup.append(card('Grizzly Bears','replacement_control',pl));entities['replacement_control']=dict(name='Grizzly Bears',player=pl,alive=True,hand=False,tapped=False)
  token_instances=[];last_cost_cards=[];chosen_targets=[]
  life=[20,20];mana=[[10]*6,[10]*6];pre=[step('turn',player=owner,value=0)];actions=[]
  current_cost=p['activation_mana'] if activation and not casting else p['mana']
  if v=='repeat':mana[owner]=[30]*6
  if v in ('payment','cast_payment'):mana[owner]=list(p['activation_mana'] if activation and v=='payment' else p['mana'])
  if v in ('insufficient_mana','insufficient_cast_mana','wrong_color'):mana[owner]=[0]*6
  if v=='wrong_color':mana[owner][0]=sum(current_cost)
  if v in ('repeat','payment','cast_payment','insufficient_mana','insufficient_cast_mana','wrong_color'):pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  def pay(cost,pl):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  def add_hand(n,ref,pl):setup.append(card(n,ref,pl,'hand'));zones['hand'][pl].append(n)
  if v=='repeat' and not activation:add_hand(name,'source2',owner);entities['source2']=dict(alive=False,hand=True,tapped=False)
  if v=='countered':add_hand('Counterspell','counter',0)
  if respond:add_hand('Reclaim','response',target_owner)
  if v=='bounce_token':add_hand('Unsummon','token_bounce',owner)
  if v=='replacement_spell':add_hand('Divination','replacement_spell',owner)
  def pools():
   return [owner] if p['graveyard']=='self' else [1-owner] if p['graveyard'] in ('opponent','opponents') else [target_owner] if p['graveyard']=='target_player' else [0,1]
  def legal_cards():return [(pl,n) for pl in pools() for n in zones['graveyard'][pl] if eligible(o[n],p)]
  def choices(n):
   nonlocal chosen_targets
   dialogs=[] if p['inputs'].get('dialogs_require_hand') and not zones['hand'][owner] else p['inputs']['dialogs']
   seq=[step('choose_dialog',player=owner,value=x) for x in dialogs]
   if p['graveyard']=='target_player':seq.append(step('choose_player',player=owner,value=target_owner))
   elif p['quantity'] in ('one','upto'):
    both=all(any(eligible(o[nm],p) for nm in zones['graveyard'][who]) for who in (0,1))
    if p['graveyard']=='any' and (both or p['inputs'].get('always_grave_player')):seq.append(step('choose_player',player=owner,value=target_owner))
   if p['quantity']=='one':seq.append(step('choose_zone_card',player=owner,value=rows[n]['csvid']))
   if p['quantity']=='upto':
    available=[nm for nm in zones['graveyard'][target_owner] if eligible(o[nm],p)]
    if v=='alternate':available=list(reversed(available))
    elif n in available:available.remove(n);available.insert(0,n)
    want=0 if v=='zero_targets' else 1 if v in ('one_target','lost_target') else p['limit']
    if v=='duplicate_targets':
     duplicated=next(nm for nm in available if available.count(nm)>1);available=[duplicated]*2;want=2
    chosen_targets=[(target_owner,nm) for nm in available[:want]]
    seq += [step('choose_zone_card',player=owner,value=rows[nm]['csvid']) for _,nm in chosen_targets]
    if len(chosen_targets)<min(p['limit'],len(zones['graveyard'][target_owner])):seq.append(step('choose_optional_zone',player=owner,value=rows[available[-1]]['csvid']))
   return seq
  def effect(n):
   if p.get('effect_self_exile') and entities['source']['alive']:
    entities['source']['alive']=False;zones['exile'][owner].append(name)
   chosen=list(chosen_targets) if p['quantity']=='upto' else [] if ability_cost_count else legal_cards() if p['quantity']=='all' else [(target_owner,n)]
   amount=0
   if p.get('token') and (chosen or last_cost_cards):
    body=copy.deepcopy(p['token'])
    if body.get('per_cost'):body.update(power=body['per_cost']*len(last_cost_cards),toughness=body['per_cost']*len(last_cost_cards))
    if body.get('selected_stats'):body.update(power=int(o[n]['power']),toughness=int(o[n]['toughness']))
    token_instances.append(body)
   for pl,nm in chosen:
    zones['graveyard'][pl].remove(nm);zones['exile'][pl].append(nm);amount+=1
   life[owner]+=p['life_per']*(len(last_cost_cards) if ability_cost_count else amount)
   if p['life_characteristic'] and chosen:life[owner]+=int(o[chosen[0][1]][p['life_characteristic']])
   if p.get('exile_hands'):
    for pl in pools():zones['exile'][pl].extend(zones['hand'][pl]);zones['hand'][pl]=[]
   if p.get('exile_battlefield'):
    for e in entities.values():
     types=set(o[e.get('name',name)]['type_line'].partition(' — ')[0].split())
     if e['alive'] and (p['exile_battlefield']=='all' or types & set(p['exile_battlefield'])):
      e['alive']=False;zones['exile'][e.get('player',owner)].append(e.get('name',name))
   if p.get('set_life') is not None:life[:]=[p['set_life']]*2
   for _ in range(p['draw']):zones['hand'][owner].append(zones['library'][owner].pop(0))
  has_target=bool(legal_cards()) if p['quantity']=='one' else True
  denied=(bool(ability_cost_count and len([n for n in zones['graveyard'][owner] if eligible(o[n],p)])<ability_cost_count) or bool(cost_count and len([n for n in zones['graveyard'][owner] if eligible(o[n],p)])<cost_count) or v in ('insufficient_mana','wrong_color','insufficient_cast_mana') or p['quantity']=='one' and not has_target and not entry or activation and p['activation']['tap'] and (v=='tapped' or p['creature'] and v in ('sick','fresh_activation')))
  def cast(ref,n,do_effect=True):
   nonlocal actions
   if p['mode']=='spell':actions.append(step('require_can_cast',ref=ref,value=1))
   if cost_count:
    paid_cards=[nm for nm in zones['graveyard'][owner] if eligible(o[nm],p)]
    if n in paid_cards:paid_cards.remove(n);paid_cards.insert(0,n)
    paid_cards=paid_cards[:cost_count]
    actions += [step('choose_zone_card',player=owner,value=rows[nm]['csvid']) for nm in paid_cards]
   if not cost_count and do_effect and (not entry or v!='countered') and (has_target or p['quantity']=='all'):actions += choices(n)
   if v=='countered':actions.append(step('choose_priority',player=0,ref='counter'))
   actions.append(step('cast',ref=ref,value=0 if v=='countered' else 1));pay(p['mana'],owner)
   if v!='countered':actions.append(step('assert_stack',value=1))
   if cost_count:
    for nm in paid_cards:zones['graveyard'][owner].remove(nm);zones['exile'][owner].append(nm)
    actions += [step('assert_zone_multiplicity',player=owner,zone=z,index=rows[nm]['csvid'],value=zones[z][owner].count(nm)) for nm in sorted(set(paid_cards)) for z in ('graveyard','exile')]
   if v=='countered':
    pay(mana_cost(o['Counterspell']['mana_cost']),0);zones['hand'][0].remove('Counterspell');zones['graveyard'][0].append('Counterspell')
   if respond:
    actions += [step('choose_zone_card',player=target_owner,value=rows[n]['csvid']),step('cast_response',ref='response',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_stack',value=0),step('assert_choices_consumed')]
    pay(mana_cost(o['Reclaim']['mana_cost']),target_owner);zones['hand'][target_owner].remove('Reclaim');zones['graveyard'][target_owner].append('Reclaim');zones['graveyard'][target_owner].remove(n);zones['library'][target_owner].insert(0,n)
   if respond and p['quantity']=='upto':chosen_targets.remove((target_owner,n))
   # resolve_response runs the real response and drains its parent as well.
   actions += ([] if respond else [step('resolve')])+[step('assert_choices_consumed')]
   zones['hand'][owner].remove(name);entities[ref]['hand']=False
   if v!='countered' and p['mode']!='spell':entities[ref]['alive']=True;entities[ref]['tapped']=bool(p.get('enters_tapped'))
   if not cost_count and do_effect and v not in ('countered','lost_target') and (has_target or p['quantity']=='all'):effect(n)
   # The source spell is still on the stack while its graveyard effect runs.
   if p['mode']=='spell' or v=='countered':zones['graveyard'][owner].append(name)
  def activate(n):
   nonlocal actions,last_cost_cards
   c=p['activation'];selected_inputs=choices(n)
   if ability_cost_count:
    available=[nm for nm in zones['graveyard'][owner] if eligible(o[nm],p)]
    if n in available:available.remove(n);available.insert(0,n)
    count=1 if v=='exile_one' else 2 if v=='exile_two' else p.get('variable_exile_count',ability_cost_count)
    last_cost_cards=available[:count];selected_inputs=[step('choose_zone_card',player=owner,value=rows[nm]['csvid']) for nm in last_cost_cards]
    if p.get('variable_exile_count'):selected_inputs.append(step('choose_optional_zone',player=owner,value=rows[available[-1]]['csvid']))
   actions += [step('require_can_activate',ref='source',value=1)]+selected_inputs+[step('activate',ref='source',value=1),step('assert_stack',value=1),step('assert_choices_consumed')]
   if ability_cost_count:
    for nm in last_cost_cards:zones['graveyard'][owner].remove(nm);zones['exile'][owner].append(nm)
    actions += [step('assert_zone_multiplicity',player=owner,zone=z,index=rows[nm]['csvid'],value=zones[z][owner].count(nm)) for nm in sorted(set(last_cost_cards)) for z in ('graveyard','exile')]
   pay(p['activation_mana'],owner);life[owner]-=c['pay_life']
   if c['sacrifice'] or c['exile']:
    entities['source']['alive']=False;zones['exile' if c['exile'] else 'graveyard'][owner].append(name)
   elif c['tap']:entities['source']['tapped']=True
   # Costs precede the effect: a sacrificed source can itself be among cards
   # exiled by a subsequent whole-graveyard effect.
   for z in ('graveyard','exile'):actions.append(step('assert_zone_multiplicity',player=owner,zone=z,index=csvid,value=zones[z][owner].count(name)))
   actions.append(step('assert_life',player=owner,value=life[owner]))
   if c['tap'] and entities['source']['alive']:actions += [step('require_card',ref='source',property='in_play',value=1),observe('source','tapped',1)]
   if v=='destroy_source_pending':actions.append(step('destroy',ref='source'));entities['source']['alive']=False;zones['graveyard'][owner].append(name)
   actions += [step('resolve'),step('assert_choices_consumed')];effect(n)
  if activation:
   if casting:
    if v=='insufficient_cast_mana':actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
    else:cast('source',target,False)
   if not cast_only:
    if denied:actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_choices_consumed')]
    else:
     activate(target)
     if v=='repeat':activate(next(n for n in reversed(candidates) if n in zones['graveyard'][target_owner]))
  elif denied:
   actions += [step('cast',ref='source',value=0)] if v in ('insufficient_mana','wrong_color') else [step('cast',ref='source',value=0)] if cost_count else [step('can_cast',ref='source',value=0)]
   actions.append(step('assert_choices_consumed'))
  else:
   cast('source',target)
   if v=='repeat':
    second=next((n for n in reversed(candidates) if n in zones['graveyard'][target_owner]),None) if p['quantity']=='one' else None
    cast('source2',second)
   if v=='destroy_source':actions.append(step('destroy',ref='source'));entities['source']['alive']=False;zones['exile' if p.get('graveyard_replacement') else 'graveyard'][owner].append(name)
   if v in ('replacement_own_death','replacement_opponent_death','replacement_removed'):
    if v=='replacement_removed':actions.append(step('destroy',ref='source'));entities['source']['alive']=False;zones['exile'][owner].append(name)
    actions.append(step('destroy',ref='replacement_control'));e=entities['replacement_control'];e['alive']=False;zones['exile' if v!='replacement_removed' else 'graveyard'][e['player']].append('Grizzly Bears')
   if v=='replacement_spell':
    actions += [step('cast',ref='replacement_spell',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
    pay(mana_cost(o['Divination']['mana_cost']),owner);zones['hand'][owner].remove('Divination');zones['exile'][owner].append('Divination')
    for _ in range(2):zones['hand'][owner].append(zones['library'][owner].pop(0))
  if v in ('destroy_token','bounce_token','fresh_token_attack') and token_instances:
   token_id=rows[p['token']['name']]['csvid'];actions.append(step('bind_permanent_copy',player=owner,value=token_id,index=0,ref='action_token'))
   if v=='destroy_token':actions += [step('destroy',ref='action_token'),step('refresh')];token_instances.pop(0)
   if v=='bounce_token':
    actions += [step('choose_target',player=owner,ref='action_token'),step('cast',ref='token_bounce',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
    pay(mana_cost(o['Unsummon']['mana_cost']),owner);zones['hand'][owner].remove('Unsummon');zones['graveyard'][owner].append('Unsummon');token_instances.pop(0)
   if v=='fresh_token_attack':actions += [step('begin_attackers'),step('attack',ref='action_token',value=0),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('resolve'),step('end_combat')]
  if v=='cleanup':actions.append(step('cleanup'))
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0)]
  for ref,e in entities.items():
   if e['alive'] or e['hand']:checks.append(observe(ref,'in_hand',int(e['hand'])))
   if e['alive']:
    checks += [observe(ref,'in_play',1),observe(ref,'tapped',int(e['tapped']))]
    record=o[e.get('name',name)]
    if 'Creature' in record['type_line']:checks += [observe(ref,'power',int(record['power'])),observe(ref,'toughness',int(record['toughness'])),observe(ref,'damage',0),observe(ref,'plus1_counters',0),observe(ref,'minus1_counters',0)]
  if p.get('token'):
   token_id=rows[p['token']['name']]['csvid']
   for who in (0,1):checks.append(step('assert_zone_multiplicity',player=who,zone='battlefield',index=token_id,value=len(token_instances) if who==owner else 0))
   for index,body in enumerate(token_instances):
    ref='created_token_'+str(index);checks.append(step('bind_permanent_copy',player=owner,value=token_id,index=index,ref=ref))
    props=dict(in_play=1,tapped=int(body.get('tapped',False)))
    if body.get('creature',True):props.update(damage=0,power=body['power'],toughness=body['toughness'],plus1_counters=0,minus1_counters=0)
    matching=dict(token=1,artifact=int(body['artifact']),color=sum(1<<{'B':1,'U':2,'G':3,'R':4,'W':5}[c] for c in body['colors']))
    matching.update({'subtype_'+sub:1 for sub in body['subtypes']})
    checks += [step('assert_matching_permanents',player=owner,index=token_id,property=k,value=value) for k,value in matching.items()]+[observe(ref,k,value) for k,value in props.items()]
  universe=set(PINS['cards'])|set(HAND)|set(GRAVE)|set(LIBRARY)|set(EXILE)|{name}
  for pl in (0,1):
   checks += [step('assert_life',player=pl,value=life[pl]),step('assert_poison',player=pl,value=0),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and pl==e.get('player',owner) for e in entities.values())+(len(token_instances) if pl==owner else 0))]
   if v!='cleanup':checks.append(step('assert_mana_total',player=pl,value=sum(mana[pl])))
   if v in ('payment','cast_payment','insufficient_mana','insufficient_cast_mana','wrong_color'):checks += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for z in zones:
    names=zones[z][pl];counts=Counter(names);checks.append(step('assert_zone',player=pl,zone=z,value=len(names)))
    checks += [step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(names)] if z=='library' else [step('assert_zone_contains',player=pl,zone=z,value=rows[n]['csvid']) for n in counts]
    checks += [step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=counts[n]) for n in sorted(universe)]
   checks += [step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=count) for n,count in Counter(zones['hand'][pl]+zones['graveyard'][pl]).items()]
  seq=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=0 if v=='ready_snapshot' else next(i for i,s in enumerate(actions) if s['op']=='resolve')
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  result.append(dict(name=f'GraveyardExileRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family='graveyard_exile',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['additional_components']+['Graveyard protection, other replacement effects and multiplayer; functional keyword abilities outside the selected graveyard component.'])))
 return result


def _unearth_variants(p):
 modes=['effect','payment','insufficient_mana','wrong_color','bare_zones','cleanup','haste_attack','destroy_source','bounce_source','exile_source','opposing_turn','combat_timing','stack_timing','cannot_repeat','query_purity','ready_snapshot','pending_snapshot','resolved_snapshot','cast_source','cast_payment','insufficient_cast_mana']
 if p.get('entry_effect')=='nonbasic_damage':modes += ['own_target','zero_nonbasic']
 if p.get('entry_effect')=='both_discard':modes += ['last_discard','empty_hands','one_sided_hand']
 if p.get('entry_effect')=='bounce_nonlands':modes += ['empty_battlefield']
 if p.get('entry_effect')=='tutor_creature':modes += ['alternate_tutor','decline_tutor','wrong_type_library','empty_library']
 modes += ['missing_color_'+str(c) for c,n in enumerate(p['activation_mana']) if c and n]
 return [f'{v}_{owner}' for owner in (0,1) for v in modes if not (p.get('entry_effect')=='nonbasic_damage' and v=='own_target' and owner==1)]


def _expand_unearth(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe
 cases=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);bare=mode=='bare_zones';sentinel=(p['inputs'].get('leave_player_sentinel_capacity',0) if mode in ('destroy_source','bounce_source','exile_source','cleanup') else 0);cast_only=mode in ('cast_source','cast_payment','insufficient_cast_mana')
  zones={z:[[] if bare else list(ns),[] if bare else list(ns)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('library',LIBRARY),('exile',EXILE))}
  entry=p.get('entry_effect');entry_target=owner if mode=='own_target' else 1-owner;discard_count=0;board=[];response_owner=0 if p.get('entry_effect')=='both_discard' else owner
  if entry=='both_discard':
   if not bare:zones['hand'][1]=['Giant Growth','Giant Growth']
   if mode=='empty_hands':zones['hand']=[[],[]]
   if mode=='one_sided_hand':zones['hand'][1-owner]=[]
  if entry=='tutor_creature':
   if mode=='alternate_tutor':zones['library'][owner].append('Hill Giant')
   if mode=='wrong_type_library':zones['library'][owner]=['Forest','Plains','Lightning Bolt']
   if mode=='empty_library':zones['library'][owner]=[]
  tutor_target=None;tutor_ran=False;tutor_offered=[]
  setup=[];actions=[];life=[20,20];mana=[[10]*6,[10]*6];alive=False;tapped=False;control=mode=='stack_timing';control_power=2;generic_paid=False
  for who in (0,1):
   for zone in zones:setup += [card(n,f'p{who}_{zone}_{i}',who,zone) for i,n in enumerate(zones[zone][who])]
  if entry=='nonbasic_damage':
   for who in (0,1):
    names=['Forest'] if mode=='zero_nonbasic' else ['Darksteel Citadel','Darksteel Citadel','Forest']
    for index,n in enumerate(names):board.append(dict(name=n,ref=f'entry_land_{who}_{index}',player=who,zone='battlefield',state=['tapped']))
  if entry=='bounce_nonlands' and mode!='empty_battlefield':
   for who in (0,1):
    for index,n in enumerate(('Grizzly Bears','Sol Ring','Seal of Cleansing','Forest')):board.append(dict(name=n,ref=f'entry_control_{who}_{index}',player=who,zone='battlefield',state=['tapped']))
  setup.extend(copy.deepcopy(board))
  for x in board:x['alive']=True
  zone='hand' if cast_only else 'graveyard';setup.append(card(name,'source',owner,zone));zones[zone][owner].append(name)
  pre=([step('prepare_graveyard_abilities')] if not cast_only else [])+[step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)]
  if entry=='tutor_creature':pre.append(step('observe_search_filters',value=1))
  if entry=='both_discard':pre.append(step('discard_policy',value=2 if mode=='last_discard' else 1))
  cost=p['mana'] if cast_only else p['activation_mana']
  if mode in ('payment','cast_payment'):mana[owner]=list(cost)
  if mode=='payment' and p['inputs'].get('unearth_payment_reserve'):mana[owner]=[a+b for a,b in zip(mana[owner],p['inputs']['unearth_payment_reserve'])]
  if mode in ('insufficient_mana','insufficient_cast_mana','wrong_color'):mana[owner]=[0]*6
  if mode=='wrong_color':mana[owner][0]=sum(cost)
  if mode.startswith('missing_color_'):
   color=int(mode.removeprefix('missing_color_'));mana[owner]=list(cost);mana[owner][0]+=mana[owner][color];mana[owner][color]=0
  if mode.startswith('missing_color_') or mode in ('payment','cast_payment','insufficient_mana','insufficient_cast_mana','wrong_color'):pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  def pay(cost,who):
   nonlocal generic_paid
   if cost[0]:generic_paid=True
   mana[who]=[a-b for a,b in zip(mana[who],cost)]
  def extra(n,ref,who=owner):setup.append(card(n,ref,who,'hand'));zones['hand'][who].append(n)
  if entry=='tutor_creature':extra(name,'trigger_identity')
  if mode=='bounce_source':extra('Unsummon','response',response_owner)
  if mode=='exile_source':extra('Swords to Plowshares','response',response_owner)
  if control:extra('Giant Growth','pending');setup.append(card('Grizzly Bears','control',owner))
  if mode=='opposing_turn':actions.append(step('turn',player=1-owner,value=0))
  if mode=='combat_timing':actions += [step('begin_attackers'),step('finish_attackers'),step('begin_blockers'),step('advance_combat')]
  if control:
   actions += [step('choose_target',player=owner,ref='control'),step('cast',ref='pending',value=1),step('assert_stack',value=1)]
   pay(mana_cost(o['Giant Growth']['mana_cost']),owner);zones['hand'][owner].remove('Giant Growth')
  def board_count(who):return sum(x['alive'] and x['player']==who for x in board)
  def entry_inputs():
   nonlocal tutor_target,tutor_offered
   if entry=='tutor_creature':
    tutor_offered=list(zones['library'][owner]);eligible=[nm for nm in tutor_offered if 'Creature' in o[nm]['type_line'].partition(' \u2014 ')[0].split()]
    tutor_target=(eligible[-1] if mode=='alternate_tutor' else eligible[0]) if eligible and mode!='decline_tutor' else None
    out=[step('choose_trigger',player=owner,ref='trigger_identity')] if owner==0 else []
    if tutor_target:out.append(step('choose_zone_card',player=owner,value=rows[tutor_target]['csvid']))
    elif tutor_offered:out.append(step('choose_optional_zone',player=owner,value=rows[tutor_offered[0]]['csvid']))
    return out
   if entry=='nonbasic_damage' and owner==0:return [step('choose_player',player=owner,value=entry_target)]
   # Native AI chooses its discard first, then displays an acknowledgement.
   # Human discard policy handles the separate actual hand-card choice.
   if entry=='both_discard' and any(n!=name for n in zones['hand'][1]):return [step('choose_dialog',player=1,value=0)]
   return []
  def entry_model():
   nonlocal discard_count,tutor_ran
   if entry=='tutor_creature':
    tutor_ran=True
    if tutor_target:zones['library'][owner].remove(tutor_target);zones['graveyard'][owner].append(tutor_target)
   if entry=='nonbasic_damage':life[entry_target]-=sum(x['alive'] and x['player']==entry_target and 'Land' in o[x['name']]['type_line'] and 'Basic' not in o[x['name']]['type_line'] for x in board)
   if entry=='both_discard':
    for who in (owner,1-owner):
     if zones['hand'][who]:
      n=(max if mode=='last_discard' else min)(zones['hand'][who],key=lambda nm:rows[nm]['csvid']);zones['hand'][who].remove(n);zones['graveyard'][who].append(n);discard_count+=int(who==0)
   if entry=='bounce_nonlands':
    for x in board:
     if x['alive'] and 'Land' not in o[x['name']]['type_line']:x['alive']=False;zones['hand'][x['player']].append(x['name'])
  if cast_only:
   denied=mode=='insufficient_cast_mana';actions += [step('cast',ref='source',value=0 if denied else 1),step('assert_choices_consumed')]
   if not denied:
    actions += [step('assert_stack',value=1)]+entry_inputs()+[step('resolve'),step('assert_choices_consumed')];pay(p['mana'],owner);zones['hand'][owner].remove(name);alive=True;entry_model()
    actions.append(step('bind_permanent',player=owner,value=csvid,ref='returned'))
  else:
   denied=mode.startswith('missing_color_') or mode in ('insufficient_mana','wrong_color','opposing_turn','combat_timing','stack_timing')
   actions.append(step('require_can_activate_graveyard',ref='source',value=0 if denied else 1))
   if denied:
    actions += [step('activate_graveyard',ref='source',value=0),step('assert_choices_consumed')]
    if control:actions += [step('resolve'),step('assert_choices_consumed')];zones['graveyard'][owner].append('Giant Growth');control_power=5
    if mode=='combat_timing':actions += [step('combat_damage'),step('resolve'),step('assert_choices_consumed'),step('end_combat')]
   else:
    if mode=='query_purity':
     actions += [step('require_can_activate_graveyard',ref='source',value=1),step('require_can_activate_graveyard',ref='source',value=1),step('assert_stack',value=0),step('assert_zone_multiplicity',player=owner,zone='graveyard',index=csvid,value=1),step('assert_mana_total',player=owner,value=sum(mana[owner]))]
    actions += [step('choose_dialog',player=owner,value=1),step('activate_graveyard',ref='source',value=1),step('assert_stack',value=1)]
    pay(p['activation_mana'],owner)
    # Unearth costs only mana. The printed return is a resolving effect, so
    # physical source ownership remains in graveyard while the ability waits.
    actions += [step('assert_mana_total',player=owner,value=sum(mana[owner])),step('assert_zone_multiplicity',player=owner,zone='graveyard',index=csvid,value=1),step('assert_zone',player=owner,zone='battlefield',value=board_count(owner)),step('assert_zone_multiplicity',player=owner,zone='exile',index=csvid,value=0)]
    actions += entry_inputs()+[step('resolve'),step('assert_choices_consumed'),step('bind_permanent',player=owner,value=csvid,ref='returned')]
    zones['graveyard'][owner].remove(name);alive=True;entry_model()
    if mode=='cannot_repeat':actions += [step('require_can_activate_graveyard',ref='source',value=0),step('activate_graveyard',ref='source',value=0),step('assert_choices_consumed')]
    if mode=='haste_attack':
     actions += [step('begin_attackers'),step('attack',ref='returned',value=1),step('finish_attackers'),step('remove_from_combat',ref='returned'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('resolve'),step('assert_choices_consumed'),step('end_combat')];tapped=True
    if sentinel:actions += [step('assert_zone',player=owner,zone='battlefield',value=1),step('assert_zone',player=1-owner,zone='battlefield',value=0)]
    if mode=='destroy_source':actions += ([step('choose_player',player=owner,value=owner) for _ in range(sentinel)] if sentinel else [])+[step('destroy',ref='returned'),step('refresh')]+([step('assert_unused_choices',value=sentinel)] if sentinel else []);alive=False;zones['exile'][owner].append(name)
    if mode in ('bounce_source','exile_source'):
     n='Unsummon' if mode=='bounce_source' else 'Swords to Plowshares';actions += [step('choose_target',player=response_owner,ref='returned'),step('cast',ref='response',value=1)]+([step('choose_player',player=owner,value=owner) for _ in range(sentinel)] if sentinel else [])+[step('resolve'),step('refresh')]+([step('assert_unused_choices',value=sentinel)] if sentinel else [])+[step('assert_choices_consumed')]
     pay(mana_cost(o[n]['mana_cost']),response_owner);zones['hand'][response_owner].remove(n);zones['graveyard'][response_owner].append(n);alive=False;zones['exile'][owner].append(name)
     if mode=='exile_source':life[owner]+=p['power']
    if mode=='cleanup':actions += ([step('choose_player',player=owner,value=owner) for _ in range(sentinel)] if sentinel else [])+[step('cleanup'),step('resolve')]+([step('assert_unused_choices',value=sentinel)] if sentinel else [])+[step('assert_choices_consumed')];alive=False;zones['exile'][owner].append(name)
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0)]
  if entry=='tutor_creature' and tutor_offered:
   checks += [step('assert_offered_count',value=len(tutor_offered)),step('assert_offered_selection_required',value=0)]+[step('assert_offered_legal_card',index=rows[nm]['csvid'],value=int('Creature' in o[nm]['type_line'].partition(' \u2014 ')[0].split())) for nm in sorted(set(tutor_offered))]
  if entry=='both_discard':checks.append(step('assert_discard_choices',value=discard_count))
  for x in board:
   if x['alive']:checks += [observe(x['ref'],'in_play',1),observe(x['ref'],'tapped',1)]
  if alive:checks += [observe('returned','in_play',1),observe('returned','power',p['power']),observe('returned','toughness',p['toughness']),observe('returned','tapped',int(tapped)),observe('returned','damage',0),observe('returned','plus1_counters',0),observe('returned','minus1_counters',0)]
  if control:checks += [observe('control','in_play',1),observe('control','power',control_power),observe('control','toughness',control_power),observe('control','damage',0)]
  universe=set(PINS['cards'])|{name,'Unsummon','Swords to Plowshares'}
  for who in (0,1):
   checks += [step('assert_life',player=who,value=life[who]),step('assert_poison',player=who,value=0),step('assert_zone',player=who,zone='battlefield',value=board_count(who)+int(who==owner)*(int(alive)+int(control)))]
   if mode!='cleanup':checks.append(step('assert_mana_total',player=who,value=sum(mana[who])))
   if mode!='cleanup' and (not generic_paid or mode in ('payment','cast_payment','insufficient_mana','insufficient_cast_mana','wrong_color')):checks += [step('assert_mana',player=who,color=c,value=n) for c,n in enumerate(mana[who])]
   for zone in zones:
    ns=zones[zone][who];counts=Counter(ns);checks.append(step('assert_zone',player=who,zone=zone,value=len(ns)))
    checks += [step('assert_zone_card',player=who,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(ns)] if zone=='library' and not (tutor_ran and who==owner) else [step('assert_zone_contains',player=who,zone=zone,value=rows[n]['csvid']) for n in counts]
    checks += [step('assert_zone_multiplicity',player=who,zone=zone,index=rows[n]['csvid'],value=counts[n]) for n in sorted(universe)]
  steps=pre+actions+checks
  if mode in ('ready_snapshot','pending_snapshot'):
   cut=0 if mode=='ready_snapshot' else next(i for i,x in enumerate(actions) if x['op']=='resolve')
   steps=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if mode=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  cases.append(dict(name=f'GraveyardExileRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family='graveyard_exile',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['additional_components']+['Responses removing the grave card while Unearth is pending; countering the ability; multiplayer.'])))
 return cases


def _expand_myr_capacity(name,csvid,d,p,selected,rows,o):
 from recipes import step
 import re
 # Use the independent printed mana ability, not an engine-handler constant.
 text=o[p['witness']]['oracle_text'].splitlines()[-1]
 match=re.fullmatch(r'\{T\}: Add ((?:\{C\})+)\.',text)
 if not match:raise ValueError('reviewed colorless mana witness required')
 amount=match[1].count('{C}')
 cases=[]
 for variant in selected:
  position,owner=variant.rsplit('_',1);owner=int(owner);chosen=int(position.removeprefix('slot'))
  artifacts=[p['ordinary_witness']]*p['imprints'];artifacts[chosen]=p['witness']
  setup=[dict(name=name,ref='welder',player=owner,zone='battlefield')]
  setup += [dict(name=n,ref=f'imprint{index}',player=owner,zone='graveyard') for index,n in enumerate(artifacts)]
  actions=[step('turn',player=owner,value=0)]
  actions += [step('set_mana',player=player,color=color,value=0) for player in (0,1) for color in range(6)]
  for index,n in enumerate(artifacts):
   actions += [step('require_can_activate',ref='welder',value=1),step('choose_dialog',player=owner,value=0),
      step('choose_zone_card',player=owner,value=rows[n]['csvid']),
      step('activate',ref='welder',value=1),step('assert_choices_consumed'),step('resolve'),step('assert_choices_consumed'),
      step('assert_zone',player=owner,zone='graveyard',value=p['imprints']-1-index),step('assert_zone',player=owner,zone='exile',value=index+1),
      step('assert_card',ref='welder',property='tapped',value=1)]
   for player in (1-owner,owner):actions += [step('advance_turn'),step('untap_step',player=player)]
   actions += [step('turn',player=owner,value=0),step('assert_card',ref='welder',property='tapped',value=0)]
  actions += [step('require_can_activate',ref='welder',value=1),step('choose_dialog',player=owner,value=p['menu_page_size']),
      step('choose_dialog',player=owner,value=chosen-p['menu_page_size']),step('activate',ref='welder',value=1),
      step('assert_choices_consumed'),step('assert_mana',player=owner,color=0,value=amount),
      step('assert_card',ref='welder',property='tapped',value=1),step('resolve'),step('assert_choices_consumed'),
      step('assert_stack',value=0),step('assert_zone',player=owner,zone='graveyard',value=0),
      step('assert_zone',player=owner,zone='exile',value=p['imprints']),step('assert_zone',player=owner,zone='battlefield',value=1),
      step('assert_zone',player=1-owner,zone='battlefield',value=0)]
  cases.append(dict(name=f'MyrWelderMenuCapacity_slot{chosen}_{owner}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,
   _recipe=dict(family='graveyard_exile',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['additional_components'])))
 return cases


def _delve_variants(p):
 modes=['effect','zero_cost','one_cost','max_cost','normal_payment','payment','no_own_graveyard','empty_graveyards','insufficient_mana','wrong_color','insufficient_exile_cost','opponent_only_cost','alternate_cost','bare_zones','countered','repeat','destroy_source','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot']
 if p.get('disable_repeat'):modes.remove('repeat')
 if not p['creature']:modes.remove('destroy_source')
 if p.get('spell_effect') in ('destroy_creature','destroy_nongreen','pump_six'):modes += ['own_target','no_targets','lost_target']
 if p.get('spell_effect')=='destroy_nongreen':modes.append('wrong_type')
 return [f'{mode}_{who}' for who in (0,1) for mode in modes]


def _expand_delve(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe
 cases=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);bare=mode=='bare_zones';generic=p['mana'][0]
  spell_effect=p.get('spell_effect');target_owner=owner if mode=='own_target' else 1-owner;board=[]
  # Both card types and duplicate physical copies are independent legal costs.
  stock=['Giant Growth','Forest','Grizzly Bears','Lightning Bolt','Sol Ring','Hill Giant','Plains']
  zones={zone:[[] if bare else list(ns),[] if bare else list(ns)] for zone,ns in (('hand',HAND),('graveyard',stock),('library',LIBRARY),('exile',EXILE))}
  if bare and spell_effect=='draw_three':zones['library']=[list(LIBRARY),list(LIBRARY)]
  if bare:zones['graveyard'][owner]=list(stock[:generic])
  if mode in ('no_own_graveyard','opponent_only_cost'):zones['graveyard'][owner]=[]
  if mode=='empty_graveyards':zones['graveyard']=[[],[]]
  if mode=='insufficient_exile_cost':zones['graveyard'][owner]=stock[:generic-1]
  if mode=='alternate_cost':zones['graveyard'][owner]=['Grizzly Bears','Giant Growth','Grizzly Bears']+stock
  paid_count=0 if mode in ('zero_cost','normal_payment','no_own_graveyard','empty_graveyards') else 1 if mode=='one_cost' else min(generic,len(zones['graveyard'][owner]))
  if mode=='effect':paid_count=min(2,generic)
  cost=list(p['mana']);cost[0]-=paid_count
  denied=mode in ('insufficient_mana','wrong_color','insufficient_exile_cost','opponent_only_cost')
  mana=[[10]*6,[10]*6];life=[20,20];setup=[];actions=[];alive=set();tapped=set()
  if mode in ('payment','normal_payment'):mana[owner]=list(cost)
  if mode in ('insufficient_mana','insufficient_exile_cost','opponent_only_cost'):mana[owner]=[0]*6;mana[owner][1:]=p['mana'][1:]
  if mode=='insufficient_mana':mana[owner]=[0]*6
  if mode=='wrong_color':mana[owner]=[sum(p['mana'])]+[0]*5
  if mode=='repeat':mana[owner]=[30]*6
  for who in (0,1):
   for zone in zones:setup += [card(n,f'p{who}_{zone}_{i}',who,zone) for i,n in enumerate(zones[zone][who])]
  setup.append(card(name,'source',owner,'hand'));zones['hand'][owner].append(name)
  if mode=='repeat':setup.append(card(name,'source2',owner,'hand'));zones['hand'][owner].append(name)
  if mode=='countered':setup.append(card('Counterspell','counter',0,'hand'));zones['hand'][0].append('Counterspell')
  if mode=='lost_target':setup.append(card('Unsummon','response',0,'hand'));zones['hand'][0].append('Unsummon')
  if spell_effect in ('destroy_creature','destroy_nongreen','pump_six') and mode!='no_targets':
   for who in (0,1):
    nm='Hill Giant' if spell_effect=='destroy_nongreen' and mode!='wrong_type' else 'Grizzly Bears';ref=f'target_{who}';setup.append(card(nm,ref,who));board.append(dict(name=nm,ref=ref,player=who,alive=True,pump=0))
  def legal_targets():return [x for x in board if x['alive'] and (spell_effect!='destroy_nongreen' or 'G' not in o[x['name']]['colors'])]
  if spell_effect in ('destroy_creature','destroy_nongreen','pump_six') and not legal_targets():denied=True
  pre=[step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)]+[step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  def receipts():
   out=[]
   universe=set(PINS['cards'])|{name,'Counterspell'}
   if mode=='lost_target':universe.add('Unsummon')
   for who in (0,1):
    out += [step('assert_life',player=who,value=life[who]),step('assert_poison',player=who,value=0),step('assert_zone',player=who,zone='battlefield',value=(len(alive) if who==owner else 0)+sum(x['alive'] and x['player']==who for x in board))]
    if mode!='cleanup':out.append(step('assert_mana_total',player=who,value=sum(mana[who])))
    if mode in ('payment','normal_payment','insufficient_mana','wrong_color','insufficient_exile_cost','opponent_only_cost'):out += [step('assert_mana',player=who,color=c,value=n) for c,n in enumerate(mana[who])]
    for zone in zones:
     ns=zones[zone][who];counts=Counter(ns);out.append(step('assert_zone',player=who,zone=zone,value=len(ns)))
     out += [step('assert_zone_card',player=who,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(ns)] if zone=='library' else [step('assert_zone_contains',player=who,zone=zone,value=rows[n]['csvid']) for n in counts]
     out += [step('assert_zone_multiplicity',player=who,zone=zone,index=rows[n]['csvid'],value=counts[n]) for n in sorted(universe)]
   for x in board:
    if x['alive']:
     r=o[x['name']];out += [observe(x['ref'],'in_play',1),observe(x['ref'],'power',int(r['power'])+x['pump']),observe(x['ref'],'toughness',int(r['toughness'])+x['pump']),observe(x['ref'],'tapped',0),observe(x['ref'],'damage',0),observe(x['ref'],'plus1_counters',0),observe(x['ref'],'minus1_counters',0)]
   return out
  def cast(ref,number):
   nonlocal actions
   available=list(zones['graveyard'][owner]);chosen=available[:min(paid_count,len(available))]
   actual_cost=list(p['mana']);actual_cost[0]-=len(chosen)
   # The UI is conditional on ordinary printed-cost affordability; this input
   # declaration follows that handler condition without changing cost receipts.
   if available:
    if all(a>=b for a,b in zip(mana[owner],p['mana'])):actions.append(step('choose_dialog',player=owner,value=1 if mode=='normal_payment' else 0))
    if mode!='normal_payment':
     actions += [step('choose_zone_card',player=owner,value=rows[n]['csvid']) for n in chosen]
     if len(chosen)<min(generic,len(available)):actions.append(step('choose_optional_zone',player=owner,value=rows[available[-1]]['csvid']))
   targeted=legal_targets()
   target=next((x for x in targeted if x['player']==target_owner),targeted[0] if targeted else None)
   if target:actions.append(step('choose_target',player=owner,ref=target['ref']))
   if mode=='countered':actions.append(step('choose_priority',player=0,ref='counter'))
   actions += [step('cast',ref=ref,value=0 if mode=='countered' else 1),step('assert_choices_consumed')]
   mana[owner]=[a-b for a,b in zip(mana[owner],actual_cost)]
   for n in chosen:zones['graveyard'][owner].remove(n);zones['exile'][owner].append(n)
   if mode!='countered':actions += [step('assert_stack',value=1),step('assert_zone',player=owner,zone='hand',value=len(zones['hand'][owner])),step('assert_zone_multiplicity',player=owner,zone='graveyard',index=csvid,value=zones['graveyard'][owner].count(name)),step('assert_zone_multiplicity',player=owner,zone='exile',index=csvid,value=zones['exile'][owner].count(name))]
   # Exiling is part of paying Delve, even if the spell is later countered.
   actions += [step('assert_zone_multiplicity',player=owner,zone=zone,index=rows[n]['csvid'],value=zones[zone][owner].count(n)) for n in sorted(set(chosen)) for zone in ('graveyard','exile')]
   if mode=='countered':
    mana[0]=[a-b for a,b in zip(mana[0],mana_cost(o['Counterspell']['mana_cost']))];zones['hand'][0].remove('Counterspell');zones['graveyard'][0].append('Counterspell')
   actions.append(step('assert_mana_total',player=owner,value=sum(mana[owner])))
   if mode=='lost_target':
    actions += [step('choose_target',player=0,ref=target['ref']),step('cast_response',ref='response',value=1),step('assert_stack',value=2),step('resolve_response')]
    mana[0]=[a-b for a,b in zip(mana[0],mana_cost(o['Unsummon']['mana_cost']))];zones['hand'][0].remove('Unsummon');zones['graveyard'][0].append('Unsummon');target['alive']=False;zones['hand'][target['player']].append(target['name'])
   actions += ([] if mode=='lost_target' else [step('resolve')])+[step('assert_stack',value=0),step('assert_choices_consumed')]
   zones['hand'][owner].remove(name)
   if mode=='countered':zones['graveyard'][owner].append(name)
   elif mode=='lost_target':zones['graveyard'][owner].append(name)
   elif spell_effect:
    if spell_effect=='draw_three':
     for _ in range(3):zones['hand'][owner].append(zones['library'][owner].pop(0))
    if spell_effect in ('destroy_creature','destroy_nongreen'):target['alive']=False;zones['graveyard'][target['player']].append(target['name'])
    if spell_effect=='pump_six':target['pump']+=6
    zones['graveyard'][owner].append(name)
   else:
    alive.add(ref);actions += [observe(ref,'in_play',1),observe(ref,'power',p['power']),observe(ref,'toughness',p['toughness']),observe(ref,'tapped',0),observe(ref,'damage',0),observe(ref,'plus1_counters',0),observe(ref,'minus1_counters',0)]
  if denied:actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
  else:
   cast('source',0)
   if mode=='repeat':cast('source2',1)
   if mode=='destroy_source':actions += [step('destroy',ref='source'),step('refresh')];alive.remove('source');zones['graveyard'][owner].append(name)
   if mode=='cleanup':
    actions += [step('cleanup'),step('resolve'),step('assert_choices_consumed')]
    for x in board:x['pump']=0
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0)]+receipts()
  steps=pre+actions+checks
  if mode in ('ready_snapshot','pending_snapshot'):
   cut=0 if mode=='ready_snapshot' else next(i for i,x in enumerate(actions) if x['op']=='resolve')
   steps=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if mode=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  cases.append(dict(name=f'GraveyardExileRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family='graveyard_exile',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['additional_components']+['Other cost modifiers; responses affecting selected costs while casting; multiplayer.'])))
 return cases


def _flashback_variants(p):
 if p['effect'] in ('blink_controlled','unblockable','cannot_block','sever_names'):return _flashback_board_variants(p)
 modes=['effect','payment','insufficient_mana','wrong_color','bare_zones','short_library','opposing_turn','query_purity','cleanup','ready_snapshot','resolved_snapshot','cast_source','cast_payment','insufficient_cast_mana']
 if p['effect']!='bounce_owned':modes.append('repeat')
 if p['effect'] in ('search_instant','search_ambition'):modes.append('empty_library')
 if p['effect']=='search_ambition':modes.append('one_card_library')
 if p['effect']=='search_instant':modes += ['alternate','decline_search','wrong_type_library','predicate_library']
 if p['effect']=='shuffle_grave':modes += ['opponent_target','zero_targets','one_target','max_targets','duplicate_targets','empty_graveyards']
 if p['effect']=='recover_type':modes += ['type_artifact','type_enchantment','type_land','type_planeswalker','empty_graveyards']
 if p['effect'] in ('draw_player','mill_player'):modes.append('opponent_target')
 if p['effect']=='bounce_owned':modes += ['owned_opponent_control']
 if p['effect'] in ('mill_player','mill_all','select_four'):modes.append('empty_library')
 if p.get('flashback_life'):modes.append('insufficient_life')
 return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]


def _expand_flashback(name,csvid,d,p,selected,rows,o):
 if p['effect'] in ('blink_controlled','unblockable','cannot_block','sever_names'):return _expand_flashback_board(name,csvid,d,p,selected,rows,o)
 from recipes import card,step,observe
 cases=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);bare=mode=='bare_zones';casting=mode in ('cast_source','cast_payment','insufficient_cast_mana');target_owner=1-owner if mode=='opponent_target' else owner
  zones={zone:[[] if bare else list(ns),[] if bare else list(ns)] for zone,ns in (('hand',HAND),('graveyard',GRAVE),('library',LIBRARY),('exile',EXILE))}
  if bare and p['effect']!='bounce_owned':zones['library']=[list(LIBRARY),list(LIBRARY)]
  if mode=='short_library':zones['library']=[list(LIBRARY[:2]),list(LIBRARY[:2])]
  if mode=='empty_library':zones['library']=[[],[]]
  if mode=='one_card_library':zones['library'][owner]=list(LIBRARY[:1])
  if p['effect']=='search_instant' and mode=='alternate':zones['library'][owner]=[p['flash_witness']]+list(LIBRARY)
  if p['effect']=='search_instant' and mode=='predicate_library':zones['library'][owner]=['Divination','Lightning Bolt',p['flash_witness'],'Grizzly Bears','Forest','Sol Ring']
  if p['effect']=='search_instant' and mode=='wrong_type_library':zones['library'][owner]=['Forest','Grizzly Bears','Sol Ring']
  if p['effect']=='recover_type':
   for who in (0,1):zones['graveyard'][who] += ['Ornithopter','Phyrexian Hulk','Seal of Cleansing','Darksteel Citadel','Jace Beleren']
  if mode=='empty_graveyards':zones['graveyard']=[[],[]]
  if mode=='duplicate_targets':zones['graveyard'][target_owner]=['Grizzly Bears','Grizzly Bears']+zones['graveyard'][target_owner]
  setup=[];actions=[];chosen=[];unordered_library=[False,False];mana=[[10]*6,[10]*6];life=[20,20];board=[]
  for who in (0,1):
   for zone in zones:setup += [card(n,f'p{who}_{zone}_{i}',who,zone) for i,n in enumerate(zones[zone][who])]
  source_zone='hand' if casting else 'graveyard'
  # Place the repeated source below the first source: removing the higher
  # physical grave cell preserves the second source's real index reference.
  if mode=='repeat':
   if p['effect'] in ('shuffle_grave','recover_type'):setup.insert(0,card(name,'source2',owner,source_zone));zones[source_zone][owner].insert(0,name)
   else:setup.append(card(name,'source2',owner,source_zone));zones[source_zone][owner].append(name)
  setup.append(card(name,'source',owner,source_zone));zones[source_zone][owner].append(name)
  if p['effect']=='bounce_owned':
   controller=1-owner if mode=='owned_opponent_control' else owner
   obj=card('Grizzly Bears','target',controller)
   if controller!=owner:obj['state']=['opponent_owned']
   setup.append(obj);board.append(dict(name='Grizzly Bears',ref='target',player=controller,owner=owner,alive=True))
   obj=card('Hill Giant','control',1-controller);setup.append(obj);board.append(dict(name='Hill Giant',ref='control',player=1-controller,owner=1-controller,alive=True))
  cost=p['mana'] if casting else p['flashback_mana']
  if mode in ('payment','cast_payment'):mana[owner]=list(cost)
  if mode in ('insufficient_mana','insufficient_cast_mana'):mana[owner]=[0]*6
  if mode=='wrong_color':mana[owner]=[sum(cost)]+[0]*5
  if mode=='insufficient_life':life[owner]=p['flashback_life']-1
  if mode=='repeat':mana[owner]=[30]*6
  pre=([step('prepare_graveyard_abilities')] if not casting else [])+[step('turn',player=1-owner if mode=='opposing_turn' else owner,value=0),step('present_ai_dialogs',value=1)]
  if p['effect'] in ('search_instant','search_ambition'):pre.append(step('observe_search_filters',value=1))
  pre += [step('set_mana',player=owner,color=color,value=n) for color,n in enumerate(mana[owner])]
  if mode=='insufficient_life':pre.append(step('set_life',player=owner,value=life[owner]))
  denied=mode in ('insufficient_mana','insufficient_cast_mana','wrong_color','insufficient_life') or mode=='opposing_turn' and p['sorcery']
  chosen_type={'type_artifact':'Artifact','type_enchantment':'Enchantment','type_land':'Land','type_planeswalker':'Planeswalker'}.get(mode,'Creature')
  def matches(n):return 'Instant' in o[n]['type_line'].partition(' \u2014 ')[0].split() or 'Flash' in o[n].get('keywords',[])
  def inputs():
   nonlocal chosen
   if p['effect'] in ('search_instant','search_ambition'):
    available=[n for n in zones['library'][owner] if p['effect']=='search_ambition' or matches(n)]
    if mode=='alternate' and p['flash_witness'] in available:available.remove(p['flash_witness']);available.insert(0,p['flash_witness'])
    want=0 if mode=='decline_search' else (1 if casting or p['effect']=='search_instant' else 2)
    chosen=available[:want];seq=[step('choose_zone_card',player=owner,value=rows[n]['csvid']) for n in chosen]
    if not chosen and zones['library'][owner]:seq.append(step('choose_optional_zone',player=owner,value=rows[zones['library'][owner][0]]['csvid']))
    return seq
   if p['effect']=='shuffle_grave':
    available=[n for n in zones['graveyard'][target_owner] if n!=name]
    want=0 if mode=='zero_targets' else 1 if mode=='one_target' else p['amount'];chosen=available[:want]
    actual_grave_count=len(zones['graveyard'][target_owner])-int(not casting and target_owner==owner)
    seq=[step('choose_player',player=owner,value=target_owner)]+[step('choose_zone_card',player=owner,value=rows[n]['csvid']) for n in chosen]
    if len(chosen)<min(p['amount'],actual_grave_count):seq.append(step('choose_optional_zone',player=owner,value=rows[(available or ['Forest'])[-1]]['csvid']))
    return seq
   if p['effect']=='recover_type':return [step('choose_dialog',player=owner,value=['Artifact','Creature','Enchantment','Land','Planeswalker'].index(chosen_type))]
   if p['effect'] in ('draw_player','mill_player'):return [step('choose_player',player=owner,value=target_owner)]
   if p['effect']=='bounce_owned':return [step('choose_target',player=owner,ref='target')]
   if p['effect']=='select_four' and zones['library'][owner]:return [step('choose_zone_card',player=owner,value=rows[zones['library'][owner][0]]['csvid'])]
   return []
  def effect():
   if p['effect'] in ('search_instant','search_ambition'):
    for n in chosen:zones['library'][owner].remove(n);zones['hand'][owner].append(n)
    unordered_library[owner]=True
   if p['effect']=='shuffle_grave':
    for n in chosen:zones['graveyard'][target_owner].remove(n);zones['library'][target_owner].append(n)
    if chosen:unordered_library[target_owner]=True
   if p['effect']=='recover_type':
    ns=[n for n in zones['graveyard'][owner] if chosen_type in o[n]['type_line'].partition(' \u2014 ')[0].split()]
    for n in ns:zones['graveyard'][owner].remove(n);zones['hand'][owner].append(n)
   if p['effect']=='draw_player':
    for _ in range(p['amount']):zones['hand'][target_owner].append(zones['library'][target_owner].pop(0))
   if p['effect'] in ('mill_player','mill_all'):
    for who in ((0,1) if p['effect']=='mill_all' else (target_owner,)):
     count=min(p['amount'],len(zones['library'][who]));zones['graveyard'][who].extend(zones['library'][who][:count]);del zones['library'][who][:count]
   if p['effect']=='bounce_owned':board[0]['alive']=False;zones['hand'][owner].append('Grizzly Bears')
   if p['effect']=='select_four':
    looked=zones['library'][owner][:4];del zones['library'][owner][:4]
    if looked:zones['hand'][owner].append(looked.pop(0));zones['graveyard'][owner].extend(looked)
  def cast(ref):
   nonlocal actions
   inspected=list(zones['library'][owner])
   if not casting:actions.append(step('require_can_cast_graveyard',ref=ref,value=0 if denied else 1))
   if mode=='query_purity':actions += [step('require_can_cast_graveyard',ref=ref,value=1),step('assert_stack',value=0),step('assert_mana_total',player=owner,value=sum(mana[owner])),step('assert_life',player=owner,value=life[owner]),step('assert_zone_multiplicity',player=owner,zone='graveyard',index=csvid,value=1)]
   if not denied:actions += inputs()
   actions.append(step('cast' if casting else 'cast_graveyard',ref=ref,value=0 if denied else 1))
   if denied:actions.append(step('assert_choices_consumed'));return
   mana[owner]=[a-b for a,b in zip(mana[owner],cost)]
   if not casting:life[owner]-=p['flashback_life']
   if casting:actions += [step('assert_stack',value=1),step('resolve')]
   # The faithful RulesEngine callback fully resolves a Flashback spell and
   # its proxy before returning; no artificial pending boundary is observable.
   actions += [step('assert_stack',value=0),step('assert_choices_consumed')]
   zones[source_zone][owner].remove(name);effect();zones['graveyard' if casting else 'exile'][owner].append(name)
   if p['effect']=='search_instant' and inspected:
    actions += [step('assert_offered_count',value=len(inspected)),step('assert_offered_selection_required',value=0)]+[step('assert_offered_legal_card',index=rows[n]['csvid'],value=int(matches(n))) for n in sorted(set(inspected))]
   if p['effect']=='search_ambition' and inspected:actions.append(step('assert_offered_selection_required',value=1))
  cast('source')
  if mode=='repeat':cast('source2')
  if mode=='cleanup':actions += [step('cleanup'),step('resolve'),step('assert_choices_consumed')]
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0)]
  for entity in board:
   if entity['alive']:
    record=o[entity['name']];checks += [observe(entity['ref'],'in_play',1),observe(entity['ref'],'power',int(record['power'])),observe(entity['ref'],'toughness',int(record['toughness'])),observe(entity['ref'],'tapped',0),observe(entity['ref'],'damage',0)]
  universe=set(PINS['cards'])|{name}
  if p['effect']=='search_instant':universe.add(p['flash_witness'])
  if p['effect']=='recover_type':universe.add('Jace Beleren')
  for who in (0,1):
   checks += [step('assert_life',player=who,value=life[who]),step('assert_poison',player=who,value=0),step('assert_zone',player=who,zone='battlefield',value=sum(x['alive'] and x['player']==who for x in board))]
   if mode!='cleanup':checks.append(step('assert_mana_total',player=who,value=sum(mana[who])))
   if mode in ('payment','cast_payment','insufficient_mana','insufficient_cast_mana','wrong_color'):checks += [step('assert_mana',player=who,color=color,value=n) for color,n in enumerate(mana[who])]
   for zone in zones:
    ns=zones[zone][who];counts=Counter(ns);checks.append(step('assert_zone',player=who,zone=zone,value=len(ns)))
    checks += [step('assert_zone_card',player=who,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(ns)] if zone=='library' and not unordered_library[who] else [step('assert_zone_contains',player=who,zone=zone,value=rows[n]['csvid']) for n in counts]
    checks += [step('assert_zone_multiplicity',player=who,zone=zone,index=rows[n]['csvid'],value=counts[n]) for n in sorted(universe)]
  steps=pre+actions+checks
  if mode=='ready_snapshot':steps=pre+[step('capture')]+actions+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions+checks+[step('assert_recorded_state')]
  if mode=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  cases.append(dict(name=f'GraveyardExileRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family='graveyard_exile',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['additional_components']+['Responses and countering during the internally resolving Flashback callback; other cost modifiers; multiplayer.'])))
 return cases


def _flashback_board_variants(p):
 modes=['effect','payment','insufficient_mana','wrong_color','bare_zones','opposing_turn','query_purity','cleanup','ready_snapshot','resolved_snapshot','cast_source','cast_payment','insufficient_cast_mana','no_targets','repeat']
 if p['effect']=='blink_controlled':modes += ['owned_opponent','wrong_controller','wrong_type','preexisting_pump','fresh_attack','destroy_returned']
 if p['effect']=='sever_names':modes += ['own_target','owned_opponent_copy']
 if p['effect']=='cannot_block':modes += ['zero_targets','one_target']
 return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]


def _expand_flashback_board(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe
 cases=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);effect=p['effect'];casting=mode in ('cast_source','cast_payment','insufficient_cast_mana');bare=mode=='bare_zones'
  zones={zone:[[] if bare else list(ns),[] if bare else list(ns)] for zone,ns in (('hand',HAND),('graveyard',GRAVE),('library',LIBRARY),('exile',EXILE))}
  setup=[];board=[];actions=[];chosen=[];life=[20,20];mana=[[10]*6,[10]*6]
  for who in (0,1):
   for zone in zones:setup += [card(n,f'p{who}_{zone}_{index}',who,zone) for index,n in enumerate(zones[zone][who])]
  source_zone='hand' if casting else 'graveyard'
  if mode=='repeat':setup.append(card(name,'source2',owner,source_zone));zones[source_zone][owner].append(name)
  setup.append(card(name,'source',owner,source_zone));zones[source_zone][owner].append(name)
  def add(n,ref,controller,original_owner=None,tapped=False):
   original_owner=controller if original_owner is None else original_owner
   obj=card(n,ref,controller);flags=(['opponent_owned'] if original_owner!=controller else [])+(['tapped'] if tapped else [])
   if flags:obj['state']=flags
   setup.append(obj);board.append(dict(name=n,key=ref,ref=ref,player=controller,owner=original_owner,alive=True,tapped=tapped,damage=0,pump=0,unblockable=False,cannot_block=False,sick=None))
  if mode!='no_targets':
   if effect=='blink_controlled':
    n='Forest' if mode=='wrong_type' else 'Grizzly Bears';controller=1-owner if mode=='wrong_controller' else owner;original_owner=1-owner if mode=='owned_opponent' else owner
    add(n,'target',controller,original_owner,tapped=True);add('Forest' if mode=='wrong_type' else 'Hill Giant','control',1-owner,tapped=mode=='wrong_type')
   if effect=='unblockable':add('Grizzly Bears','attacker',owner);add('Hill Giant','blocker0',1-owner)
   if effect=='cannot_block':add('Grizzly Bears','attacker',owner);add('Hill Giant','blocker0',1-owner);add('Hill Giant','blocker1',1-owner)
   if effect=='sever_names':
    add('Grizzly Bears','target0',1-owner);add('Grizzly Bears','target1',owner,1-owner if mode=='owned_opponent_copy' else owner);add('Grizzly Bears','target2',1-owner);add('Hill Giant','control0',owner);add('Hill Giant','control1',1-owner)
  cost=p['mana'] if casting else p['flashback_mana']
  if mode in ('payment','cast_payment'):mana[owner]=list(cost)
  if mode in ('insufficient_mana','insufficient_cast_mana'):mana[owner]=[0]*6
  if mode=='wrong_color':mana[owner]=[sum(cost)]+[0]*5
  if mode=='repeat':mana[owner]=[30]*6
  pre=([step('prepare_graveyard_abilities')] if not casting else [])+[step('turn',player=1-owner if mode=='opposing_turn' else owner,value=0),step('present_ai_dialogs',value=1)]+[step('set_mana',player=owner,color=color,value=n) for color,n in enumerate(mana[owner])]
  def legal():
   xs=[x for x in board if x['alive'] and 'Creature' in o[x['name']]['type_line'].partition(' \u2014 ')[0].split()]
   return [x for x in xs if x['player']==owner] if effect=='blink_controlled' else xs
  denied=mode in ('insufficient_mana','insufficient_cast_mana','wrong_color') or mode=='opposing_turn' and p['sorcery'] or effect!='cannot_block' and not legal()
  def pay(spent,who=owner):mana[who]=[a-b for a,b in zip(mana[who],spent)]
  if mode=='preexisting_pump':
   actions += [step('choose_target',player=owner,ref='target'),step('cast',ref=f'p{owner}_hand_{HAND.index("Giant Growth")}',value=1),step('resolve'),step('assert_choices_consumed')]
   pay(mana_cost(o['Giant Growth']['mana_cost']));zones['hand'][owner].remove('Giant Growth');zones['graveyard'][owner].append('Giant Growth');board[0]['pump']=3
  def inputs():
   nonlocal chosen
   xs=legal()
   if effect=='cannot_block':
    available=[x for x in xs if x['key'].startswith('blocker')];want=0 if mode=='zero_targets' else 1 if mode=='one_target' else 2;chosen=available[:want]
    return [step('choose_target',player=owner,ref=x['ref']) for x in chosen]+([step('choose_cancel_target',player=owner)] if len(chosen)<2 and xs else [])
   if effect=='unblockable':chosen=[next(x for x in xs if x['key']=='attacker')]
   elif effect=='sever_names':chosen=[next((x for x in xs if x['player']==(owner if mode=='own_target' else 1-owner)),xs[0])]
   else:chosen=[xs[0]]
   return [step('choose_target',player=owner,ref=x['ref']) for x in chosen]
  def apply():
   if effect=='blink_controlled':
    x=chosen[0];x.update(player=x['owner'],tapped=False,damage=0,pump=0,sick=1,ref='blink_returned')
    actions.append(step('bind_permanent',player=x['owner'],value=rows[x['name']]['csvid'],ref=x['ref']))
   if effect=='unblockable':chosen[0]['unblockable']=True
   if effect=='cannot_block':
    for x in chosen:x['cannot_block']=True
   if effect=='sever_names':
    target_name=chosen[0]['name']
    for x in board:
     if x['alive'] and x['name']==target_name:x['alive']=False;zones['exile'][x['owner']].append(x['name'])
  def cast(ref):
   nonlocal actions
   if not casting:actions.append(step('require_can_cast_graveyard',ref=ref,value=0 if denied else 1))
   if mode=='query_purity':actions += [step('require_can_cast_graveyard',ref=ref,value=1),step('assert_stack',value=0),step('assert_zone_multiplicity',player=owner,zone='graveyard',index=csvid,value=1),step('assert_mana_total',player=owner,value=sum(mana[owner]))]
   if not denied:actions += inputs()
   actions.append(step('cast' if casting else 'cast_graveyard',ref=ref,value=0 if denied else 1))
   if denied:actions.append(step('assert_choices_consumed'));return
   if casting:actions += [step('assert_stack',value=1),step('resolve')]
   actions += [step('assert_stack',value=0),step('assert_choices_consumed')];pay(cost)
   zones[source_zone][owner].remove(name);apply();zones['graveyard' if casting else 'exile'][owner].append(name)
  cast('source')
  if mode=='repeat':cast('source2')
  if mode=='cleanup':
   actions += [step('cleanup'),step('resolve'),step('assert_choices_consumed')]
   for x in board:x['unblockable']=False;x['cannot_block']=False
  if not denied and effect=='blink_controlled':
   x=board[0]
   if mode=='fresh_attack':actions += [step('begin_attackers'),step('attack',ref=x['ref'],value=0),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('resolve'),step('end_combat')]
   if mode=='destroy_returned':actions += [step('destroy',ref=x['ref']),step('refresh')];x['alive']=False;zones['graveyard'][x['owner']].append(x['name'])
  if not denied and effect in ('unblockable','cannot_block') and board:
   attacker=next(x for x in board if x['key']=='attacker');blockers=[x for x in board if x['key'].startswith('blocker')];blocking=None
   if mode=='cleanup':actions.append(step('turn',player=owner,value=0))
   actions += [step('begin_attackers'),step('attack',ref=attacker['ref'],value=1),step('finish_attackers'),step('begin_blockers')];attacker['tapped']=True
   for blocker in blockers:
    allowed=not attacker['unblockable'] and not blocker['cannot_block'];actions.append(step('block',ref=blocker['ref'],other=attacker['ref'],value=int(allowed)))
    if allowed:blocking=blocker;break
   actions += [step('advance_combat'),step('combat_damage'),step('resolve'),step('assert_choices_consumed'),step('end_combat')]
   if blocking:attacker['alive']=False;zones['graveyard'][attacker['owner']].append(attacker['name']);blocking['damage']=int(o[attacker['name']]['power'])
   else:life[1-owner]-=int(o[attacker['name']]['power'])
  checks=[step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0)]
  for x in board:
   if x['alive']:
    checks += [observe(x['ref'],'in_play',1),observe(x['ref'],'tapped',int(x['tapped']))]
    if 'Creature' in o[x['name']]['type_line']:checks += [observe(x['ref'],'power',int(o[x['name']]['power'])+x['pump']),observe(x['ref'],'toughness',int(o[x['name']]['toughness'])+x['pump']),observe(x['ref'],'damage',x['damage']),observe(x['ref'],'plus1_counters',0),observe(x['ref'],'minus1_counters',0)]
    if x['sick'] is not None:checks.append(observe(x['ref'],'sick',x['sick']))
  universe=set(PINS['cards'])|{name}
  for who in (0,1):
   checks += [step('assert_life',player=who,value=life[who]),step('assert_poison',player=who,value=0),step('assert_zone',player=who,zone='battlefield',value=sum(x['alive'] and x['player']==who for x in board))]
   if mode!='cleanup':checks.append(step('assert_mana_total',player=who,value=sum(mana[who])))
   if mode in ('payment','cast_payment','insufficient_mana','insufficient_cast_mana','wrong_color'):checks += [step('assert_mana',player=who,color=color,value=n) for color,n in enumerate(mana[who])]
   for zone in zones:
    ns=zones[zone][who];counts=Counter(ns);checks.append(step('assert_zone',player=who,zone=zone,value=len(ns)))
    checks += [step('assert_zone_card',player=who,zone=zone,index=index,value=rows[n]['csvid']) for index,n in enumerate(ns)] if zone=='library' else [step('assert_zone_contains',player=who,zone=zone,value=rows[n]['csvid']) for n in counts]
    checks += [step('assert_zone_multiplicity',player=who,zone=zone,index=rows[n]['csvid'],value=counts[n]) for n in sorted(universe)]
  steps=pre+actions+checks
  if mode=='ready_snapshot':steps=pre+[step('capture')]+actions+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions+checks+[step('assert_recorded_state')]
  if mode=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  cases.append(dict(name=f'GraveyardExileRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family='graveyard_exile',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['additional_components']+['Responses and countering inside the atomic Flashback callback; other replacement and targeting-protection effects; multiplayer.'])))
 return cases
