"""Pinned fixed death damage, observed through the ordinary death pipeline."""
import copy
import json
import re
from collections import Counter
from pathlib import Path
from defensive_creature_recipes import reviewed_payment
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

FAMILY='death_damage_creature'
INACTIVE="This creature can't be blocked by creatures with power 2 or less."


def spec(d,r):
 from recipes import cost_from_oracle
 if (not isinstance(r,dict) or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
     or 'Creature' not in r.get('type_line','') or r.get('keywords') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('fixed death damage requires a whole pinned normal creature and explicit variants')
 p=d.get('params')
 if (not isinstance(p,dict) or not {'amount','scope','optional','inactive'}<=set(p)
     or set(p)-{'amount','scope','optional','inactive','payment_colors'}
     or type(p['amount']) is not int or not 1<=p['amount']<=6
     or p['scope'] not in ('any','self','opponent') or type(p['optional']) is not bool
     or p['inactive'] not in ([],[INACTIVE])):
  raise ValueError('explicit bounded death amount, scope, optionality and reviewed inactive clauses required')
 if p['optional'] and p['scope']!='any':raise ValueError('optional fixed death damage supports any target only')
 target={'any':'any target','self':'you','opponent':'target opponent or planeswalker'}[p['scope']]
 middle='you may have it deal ' if p['optional'] else 'it deals '
 death=f'When this creature dies, {middle}{p["amount"]} damage to {target}.'
 if r['oracle_text']!='\n'.join(p['inactive']+[death]):raise ValueError('death damage parameters must match the complete pinned Oracle text')
 for field in ('power','toughness'):
  if not re.fullmatch(r'\d+',r.get(field,'')) or not 0<=int(r[field])<=6:raise ValueError('bounded numeric death source required')
 if not int(r['toughness']):raise ValueError('death damage source must survive entry')
 mana,hybrids=reviewed_payment(r,p)
 return dict(p,family=FAMILY,power=int(r['power']),toughness=int(r['toughness']),mana=mana,hybrids=hybrids)


def variants(p):
 modes=['cast_only','destroy','murder','exile','bounce','bounce_recast','blink_only','blink_destroy','other_death',
        'cleanup_once','two_copies','countered','payment','insufficient_mana','ready_snapshot','death_snapshot',
        'resolved_snapshot','bare_zones','combat_death','tapped_death']
 if any(p['mana'][1:]):modes.append('wrong_color')
 if p['hybrids']:modes.append('alternate_payment')
 if p['toughness']<=3:modes.append('lethal_damage')
 if p['toughness']<=4:modes.append('zero_toughness')
 if p['scope']=='any':modes += ['opponent_player','self_player','friendly_target','lethal_target','indestructible_target']
 if p['optional']:modes.append('decline')
 return [f'{mode}_{owner}' for owner in (0,1) for mode in modes if mode!='decline' or owner==0]


def validate_witnesses(oracle,registry):
 pins=json.loads((Path(__file__).parent/'data/death_damage_witnesses.json').read_text(encoding='utf8'))
 if pins.get('version')!=1:raise ValueError('unsupported death damage witnesses')
 for name,record in pins['cards'].items():
  if name not in registry or any(oracle.get(name,{}).get(k)!=v for k,v in record.items()):raise ValueError('death damage witness changed: '+name)


def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);bare=mode=='bare_zones'
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  exiles=[[] if bare else list(EXILE) for _ in (0,1)];libraries=[list(LIBRARY) for _ in (0,1)]
  setup=[];entities={};mana=[[30,10,10,10,10,10],[30,10,10,10,10,10]];life=[20,20]
  pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=player,color=0,value=30) for player in (0,1)];actions=[];cast_mana=list(p['mana'])
  if mode=='alternate_payment':
   other=dict(d['params'],payment_colors=[pair[1] if c==pair[0] else pair[0] for pair,c in zip(p['hybrids'],p['payment_colors'])])
   cast_mana,_=reviewed_payment(oracle[name],other)
  for player in (0,1):
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libraries[player])):
    setup += [card(nm,f'p{player}_{zone}_{i}',player,zone) for i,nm in enumerate(names)]
  def add(nm,ref,player,zone='hand'):
   setup.append(card(nm,ref,player,zone));entities[ref]=dict(name=nm,player=player,alive=zone=='battlefield',hand=zone=='hand',damage=0,tapped=0)
   if zone=='hand':hands[player].append(nm)
  add(name,'source',owner)
  if mode in ('payment','alternate_payment','insufficient_mana','wrong_color'):
   mana[owner]=list(cast_mana) if mode in ('payment','alternate_payment') else [0]*6
   if mode=='wrong_color':
    excluded={c for pair in p['hybrids'] for c in pair}|{c for c,n in zip(('B','U','G','R','W'),p['mana'][1:]) if n}
    unused=next(c for c in ('B','U','G','R','W') if c not in excluded)
    mana[owner][0]=cast_mana[0];mana[owner][{'B':1,'U':2,'G':3,'R':4,'W':5}[unused]]=sum(cast_mana[1:])
   pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  target_ref=None;recipient=None;decline=mode=='decline'
  if p['scope']=='self':recipient=owner
  elif p['scope']=='opponent' or mode=='opponent_player':recipient=1-owner
  elif mode=='self_player':recipient=owner
  else:
   target_ref='victim';target_owner=owner if mode=='friendly_target' else 1-owner
   target_name='Eager Cadet' if mode=='lethal_target' else 'Darksteel Myr' if mode=='indestructible_target' else 'Scaled Wurm'
   add(target_name,target_ref,target_owner,'battlefield')
  add('Grizzly Bears','unselected',1-owner,'battlefield')
  def pay(nm,caster):
   vector=cast_mana if nm==name else cost_from_oracle(oracle[nm]);mana[caster]=[a-b for a,b in zip(mana[caster],vector)]
  def hybrid_choice(ref):
   if not p['hybrids'] or owner!=0 or mode in ('payment','alternate_payment'):return []
   return [step('choose_payment',player=owner,value=sum(n<<(4*c) for c,n in enumerate(cast_mana)))]
  def cast(ref):
   e=entities[ref];pay(e['name'],e['player']);hands[e['player']].remove(e['name']);e['hand']=False;e['alive']=True
   return (hybrid_choice(ref) if e['name']==name else [])+[step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
  def death_decisions(ref='source'):
   if p['scope']=='self':return []
   # Goblin Arsonist presents its optional damage as a cancelable target picker.
   # No optional priority action is offered by this handler.
   if decline:return [step('choose_cancel_target',player=owner)]
   return [step('choose_target',player=owner,ref=target_ref)] if target_ref else [step('choose_player',player=owner,value=recipient)]
  def died(ref='source'):
   e=entities[ref];e['alive']=False;graves[e['player']].append(e['name'])
   if decline:return
   if recipient is not None:life[recipient]-=p['amount']
   elif target_ref:
    e=entities[target_ref];e['damage']+=p['amount']
    if e['name']!='Darksteel Myr' and e['damage']>=int(oracle[e['name']]['toughness']):
     e['alive']=False;graves[e['player']].append(e['name'])
  def removal(nm,ref='removal',target='source',caster=None):
   caster=1-owner if caster is None else caster
   add(nm,ref,caster);pay(nm,caster);hands[caster].remove(nm);entities[ref]['hand']=False;graves[caster].append(nm)
   return [step('choose_target',player=caster,ref=target),step('cast',ref=ref,value=1),step('assert_stack',value=1)]
  if mode in ('insufficient_mana','wrong_color'):
   actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
  elif mode=='countered':
   add('Essence Scatter','counter',0);pay(name,owner);pay('Essence Scatter',0)
   hands[owner].remove(name);hands[0].remove('Essence Scatter');graves[owner].append(name);graves[0].append('Essence Scatter');entities['source']['hand']=entities['counter']['hand']=False
   actions += hybrid_choice('source')+[step('choose_priority',player=0,ref='counter'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
  else:
   actions += cast('source')+[step('assert_life',player=owner,value=20),step('assert_life',player=1-owner,value=20)]
   if mode=='two_copies':add(name,'second',owner);actions += cast('second')
   if mode=='other_death':
    add('Eager Cadet','other',owner,'battlefield');actions += [step('destroy',ref='other')];entities['other']['alive']=False;graves[owner].append('Eager Cadet')
   elif mode in ('bounce','bounce_recast'):
    actions += removal('Unsummon')+[step('resolve'),step('bind_hand',player=owner,value=csvid,ref='source')]
    entities['source']['alive']=False;entities['source']['hand']=True;hands[owner].append(name)
    if mode=='bounce_recast':actions += cast('source')+death_decisions()+[step('destroy',ref='source')];died()
   elif mode in ('blink_only','blink_destroy'):
    actions += removal('Cloudshift',caster=owner)+[step('resolve'),step('bind_permanent',player=owner,value=csvid,ref='source')]
    if mode=='blink_destroy':actions += death_decisions()+[step('destroy',ref='source')];died()
   elif mode=='exile':
    actions += removal('Swords to Plowshares')+[step('resolve')];entities['source']['alive']=False;exiles[owner].append(name);life[owner]+=p['power']
   elif mode in ('murder','death_snapshot','lethal_damage','zero_toughness','lethal_target'):
    spell='Lightning Bolt' if mode=='lethal_damage' else 'Grasp of Darkness' if mode=='zero_toughness' else 'Murder'
    actions += removal(spell)+death_decisions()+[step('resolve')];died()
   elif mode=='combat_death':
    add('Scaled Wurm','attacker',1-owner,'battlefield')
    actions += [step('turn',player=1-owner,value=1),step('attack',ref='attacker',value=1),step('block',ref='source',other='attacker',value=1),step('advance_combat')]+death_decisions()+[step('combat_damage')]
    died();entities['attacker']['damage']=p['power'];entities['attacker']['tapped']=1
    if p['power']>=6:entities['attacker']['alive']=False;graves[1-owner].append('Scaled Wurm')
   elif mode not in ('cast_only','payment','alternate_payment'):
    if mode=='tapped_death':
     actions += removal('Twiddle',ref='tap')+[step('choose_dialog',player=1-owner,value=0),step('resolve'),observe('source','tapped',1)]
    if mode=='two_copies':
     actions += removal('Murder',ref='first_removal')+death_decisions()+[step('resolve')];died()
     actions += removal('Murder',ref='second_removal',target='second')+death_decisions('second')+[step('resolve')];died('second')
    else:actions += death_decisions()+[step('destroy',ref='source')];died()
    if mode=='cleanup_once':
     # Cleanup clears marked damage. Observe the positive effect first so a
     # no-op handler cannot pass merely because the final expected damage is 0.
     actions += [observe(target_ref,'damage',p['amount'])] if target_ref else [step('assert_life',player=recipient,value=life[recipient])]
     actions.append(step('cleanup'))
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref,e in entities.items():
   checks += [observe(ref,'in_play',int(e['alive'])),observe(ref,'in_hand',int(e['hand']))]
   if e['alive']:
    r=oracle[e['name']];checks += [observe(ref,'power',int(r['power'])),observe(ref,'toughness',int(r['toughness'])),observe(ref,'damage',0 if mode=='cleanup_once' else e['damage']),observe(ref,'plus1_counters',0),observe(ref,'minus1_counters',0)]
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{name,'Scaled Wurm','Grizzly Bears','Eager Cadet','Darksteel Myr','Murder','Swords to Plowshares','Unsummon','Cloudshift','Essence Scatter','Lightning Bolt','Grasp of Darkness','Twiddle'}
  for player in (0,1):
   checks += [step('assert_life',player=player,value=life[player]),step('assert_poison',player=player,value=0),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
   if mode!='cleanup_once':checks += [step('assert_mana_total',player=player,value=sum(mana[player]))]
   if mode in ('payment','alternate_payment','insufficient_mana','wrong_color'):checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libraries[player])):
    checks += [step('assert_zone',player=player,zone=zone,value=len(names))]
    if zone=='library':checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=registry[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  seq=pre+actions+checks
  if mode in ('ready_snapshot','death_snapshot'):
   cut=0 if mode=='ready_snapshot' else max(i for i,s in enumerate(actions) if s['op']=='resolve')
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions[cut:])+copy.deepcopy(checks)+[step('assert_recorded_state')]
  elif mode=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  result.append(dict(name=f'DeathDamageCreatureRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive']+['planeswalker targets','sacrifice costs','control changes','trigger ordering','damage replacement/prevention','multiplayer'])))
 return result
