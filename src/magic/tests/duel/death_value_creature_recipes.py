"""Whole-Oracle fixed death receipts, driven by real removal and combat."""
import re
from collections import Counter
from creature_recipes import KEYWORDS, REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY
from etb_removal_creature_recipes import SPELLS

RESPONSES=dict(SPELLS, **{
 'Murder':('{1}{B}{B}','Destroy target creature.'),
 'Swords to Plowshares':('{W}', 'Exile target creature. Its controller gains life equal to its power.'),
 'Lightning Bolt':('{R}','Lightning Bolt deals 3 damage to any target.'),
 'Grasp of Darkness':('{B}{B}','Target creature gets -4/-4 until end of turn.'),
 'Twiddle':('{U}','You may tap or untap target artifact, creature, or land.'),
})

def spec(d,r):
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or 'Creature' not in r.get('type_line','') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:
  raise ValueError('death receipt requires a whole pinned normal creature and explicit variants')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'draw','gain_life','optional','keywords'}:raise ValueError('explicit death draw/life, optionality and keywords required')
 if type(p['optional']) is not bool or type(p['draw']) is not int or type(p['gain_life']) is not int or not 0<=p['draw']<=3 or not 0<=p['gain_life']<=10 or bool(p['draw'])==bool(p['gain_life']):raise ValueError('one bounded fixed death receipt required')
 text=('draw '+{1:'a card',2:'two cards',3:'three cards'}[p['draw']] if p['draw'] else 'gain '+str(p['gain_life'])+' life')
 clause=('you may '+text if p['optional'] else text if p['draw'] else 'you '+text)+'.'
 expected={f'When {ref} dies, {clause}' for ref in ('this creature',r['name'],r['name'].split(',')[0])}
 keys=[];rules=[];canonical={k.lower():k for k in KEYWORDS}
 for line in r['oracle_text'].splitlines():
  if line in expected:rules.append(line);continue
  passive=line
  for reminder in re.findall(r'\([^\n]*?\)',line):
   if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed death receipt reminder')
   passive=passive.replace(reminder,'')
  for word in passive.strip().split(','):
   if word.strip().lower() not in canonical:raise ValueError('unreviewed additional death receipt clause')
   keys.append(canonical[word.strip().lower()])
 if len(rules)!=1 or p['keywords']!=sorted(keys) or len(keys)!=len(set(keys)) or set(keys)!=set(r.get('keywords',[])) or set(keys)&{'First strike','Double strike','Deathtouch','Menace'}:raise ValueError('death receipt whole-text parameters disagree')
 for k in ('power','toughness'):
  if not re.fullmatch(r'\d+',r.get(k,'')) or not 0<=int(r[k])<=5:raise ValueError('bounded numeric death receipt source required')
 if not int(r['toughness']):raise ValueError('death receipt source must survive casting')
 return dict(p,family='death_value_creature',power=int(r['power']),toughness=int(r['toughness']),mana=cost_from_oracle(r))

def variants(p):
 modes=['cast_only','destroy','murder','exile','bounce','blink_only','blink_destroy','other_death','cleanup_once','two_copies','countered','payment','insufficient_mana','ready_snapshot','death_snapshot','resolved_snapshot','bare_zones','injured','short_library','combat_death','tapped_death']
 if p['toughness']<=3:modes += ['lethal_damage','nonlethal_damage'] if p['toughness']>1 else ['lethal_damage']
 if p['toughness']<=4:modes.append('zero_toughness')
 if p['optional']:modes.append('decline')
 if any(p['mana'][1:]):modes.append('wrong_color')
 # Player 1 is the real engine's AI. Its accepted optional trigger is a policy
 # choice; lack of a human decline prompt there is not a rules failure.
 return [v+'_'+str(owner) for owner in (0,1) for v in modes if v!='decline' or owner==0]

def validate_witnesses(o,registry):
 for nm,values in RESPONSES.items():
  r=o.get(nm,{})
  if nm not in registry or (r.get('mana_cost'),r.get('oracle_text'))!=values:raise ValueError('death response witness changed: '+nm)
 for nm,pt in [('Grizzly Bears',('2','2')),('Scaled Wurm',('7','6')),('Eager Cadet',('1','1'))]:
  r=o.get(nm,{})
  if nm not in registry or (r.get('power'),r.get('toughness'))!=pt or r.get('oracle_text')!='':raise ValueError('death combat witness changed: '+nm)

def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones'
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  libs=[list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
  if v=='short_library':libs[owner]=libs[owner][:p['draw'] or 1]
  setup=[];entities={};mana=[[10]*6,[10]*6];life=[20,20];pre=[step('turn',player=owner,value=0)];actions=[]
  for player in (0,1):
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('library',libs[player]),('exile',exiles[player])):setup += [card(nm,f'p{player}_{zone}_{i}',player,zone) for i,nm in enumerate(names)]
  def add(nm,ref,player,zone='hand'):
   setup.append(card(nm,ref,player,zone));entities[ref]=dict(name=nm,player=player,alive=zone=='battlefield',hand=zone=='hand',damage=0,tapped=0)
   if zone=='hand':hands[player].append(nm)
  add(name,'source',owner)
  if v=='injured':life[owner]=7;pre.append(step('set_life',player=owner,value=7))
  if v in ('payment','insufficient_mana','wrong_color'):
   mana[owner]=list(p['mana']) if v!='insufficient_mana' else [0]*6
   if v=='wrong_color':
    unused=next(c for c in range(1,6) if not p['mana'][c]);mana[owner]=[p['mana'][0],0,0,0,0,0];mana[owner][unused]=sum(p['mana'][1:])
   pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  def pay(nm,caster):mana[caster]=[a-b for a,b in zip(mana[caster],cost_from_oracle(oracle[nm]))]
  def cast(ref):
   e=entities[ref];pay(e['name'],e['player']);hands[e['player']].remove(e['name']);e['hand']=False;e['alive']=True
   return [step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
  def decisions(ref='source',decline=False):
   # Optional death triggers use normal priority selection. Pure AI takes them.
   return [step('choose_trigger',player=0,ref=ref)] if p['optional'] and owner==0 and not decline else []
  def died(ref='source',decline=False):
   e=entities[ref];e['alive']=False;graves[e['player']].append(e['name'])
   if decline:return
   life[owner]+=p['gain_life'];drawn=libs[owner][:p['draw']];assert len(drawn)==p['draw'];hands[owner]+=drawn;libs[owner]=libs[owner][p['draw']:]
  def remove_spell(nm,ref='removal',target='source'):
   caster=1-owner;add(nm,ref,caster);pay(nm,caster);hands[caster].remove(nm);entities[ref]['hand']=False;graves[caster].append(nm)
   return [step('choose_target',player=caster,ref=target),step('cast',ref=ref,value=1),step('assert_stack',value=1)]
  if v in ('insufficient_mana','wrong_color'):actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
  elif v=='countered':
   add('Essence Scatter','counter',0);pay(name,owner);pay('Essence Scatter',0);hands[owner].remove(name);hands[0].remove('Essence Scatter');graves[owner].append(name);graves[0].append('Essence Scatter');entities['source']['hand']=entities['counter']['hand']=False
   actions += [step('choose_priority',player=0,ref='counter'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
  else:
   actions += cast('source')
   # Observe the absence of a death receipt while the creature is alive.
   actions += [step('assert_life',player=owner,value=life[owner]),step('assert_zone',player=owner,zone='library',value=len(libs[owner]))]
   if v=='two_copies':add(name,'second',owner);actions += cast('second')
   if v=='other_death':
    add('Grizzly Bears','other',owner);actions += cast('other')+[step('destroy',ref='other')];entities['other']['alive']=False;graves[owner].append('Grizzly Bears')
   elif v in ('bounce','blink_only','blink_destroy'):
    nm='Unsummon' if v=='bounce' else 'Cloudshift'
    if nm=='Cloudshift':
     add(nm,'blink',owner);pay(nm,owner);hands[owner].remove(nm);graves[owner].append(nm);entities['blink']['hand']=False
     actions += [step('choose_target',player=owner,ref='source'),step('cast',ref='blink',value=1),step('resolve'),step('bind_permanent',player=owner,value=csvid,ref='source')]
    else:
     actions += remove_spell(nm)+[step('resolve'),step('bind_hand',player=owner,value=csvid,ref='source')];entities['source']['alive']=False;entities['source']['hand']=True;hands[owner].append(name)
    if v=='blink_destroy':actions += decisions()+[step('destroy',ref='source')];died()
   elif v=='exile':
    actions += remove_spell('Swords to Plowshares')+[step('resolve')];entities['source']['alive']=False;exiles[owner].append(name);life[owner]+=p['power']
   elif v in ('murder','death_snapshot','lethal_damage','zero_toughness','nonlethal_damage'):
    nm='Lightning Bolt' if v=='lethal_damage' else 'Grasp of Darkness' if v=='zero_toughness' else 'Murder'
    if v=='nonlethal_damage':
     # A single point of real combat damage must not produce a death receipt.
     add('Eager Cadet','attacker',1-owner,'battlefield')
     actions += [step('turn',player=1-owner,value=1),step('attack',ref='attacker',value=1),step('block',ref='source',other='attacker',value=1),step('advance_combat'),step('combat_damage')]
     entities['source']['damage']=1;entities['attacker']['tapped']=1
     if p['power']>=1:entities['attacker']['alive']=False;graves[1-owner].append('Eager Cadet')
    else:actions += remove_spell(nm)+decisions()+[step('resolve')];died()
   elif v=='combat_death':
    add('Scaled Wurm','attacker',1-owner,'battlefield');actions += [step('turn',player=1-owner,value=1),step('attack',ref='attacker',value=1),step('block',ref='source',other='attacker',value=1),step('advance_combat')]+decisions()+[step('combat_damage')];died();entities['attacker']['damage']=p['power'];entities['attacker']['tapped']=1
   elif v not in ('cast_only','payment'):
    if v=='tapped_death':
     # A real Twiddle provides the tapped state before destruction.
     add('Twiddle','tap',1-owner);pay('Twiddle',1-owner);hands[1-owner].remove('Twiddle');graves[1-owner].append('Twiddle');entities['tap']['hand']=False
     actions += [step('choose_target',player=1-owner,ref='source'),step('cast',ref='tap',value=1),step('choose_dialog',player=1-owner,value=0),step('resolve'),observe('source','tapped',1)]
    decline=v=='decline';actions += decisions(decline=decline)+[step('destroy',ref='source')];died(decline=decline)
    if v=='two_copies':actions += decisions('second')+[step('destroy',ref='second')];died('second')
    if v=='cleanup_once':actions.append(step('cleanup'))
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref,e in entities.items():
   checks += [observe(ref,'in_play',int(e['alive'])),observe(ref,'in_hand',int(e['hand']))]
   if e['alive']:
    r=oracle[e['name']];checks += [observe(ref,'power',int(r['power'])),observe(ref,'toughness',int(r['toughness'])),observe(ref,'damage',e['damage']),observe(ref,'plus1_counters',0),observe(ref,'minus1_counters',0)]
  for player in (0,1):
   checks += [step('assert_life',player=player,value=life[player]),step('assert_poison',player=player,value=0),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
   if v!='cleanup_once':checks += [step('assert_mana_total',player=player,value=sum(mana[player]))]
   if v in ('payment','insufficient_mana','wrong_color'):checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
    checks += [step('assert_zone',player=player,zone=zone,value=len(names))]
    if zone=='library':checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     universe=set(list(HAND+GRAVE+EXILE+LIBRARY)+[name]+list(RESPONSES)+['Grizzly Bears','Scaled Wurm','Eager Cadet']);counts=Counter(names)
     checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=registry[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  seq=pre+actions+checks
  if v in ('ready_snapshot','death_snapshot'):
   cut=0 if v=='ready_snapshot' else max(i for i,s in enumerate(actions) if s['op']=='resolve')
   seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  if v=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
  result.append(dict(name=f'DeathValueCreatureRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,_recipe=dict(family=p['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['keywords']+['sacrifice costs','control changes','trigger ordering','empty-library draw loss','replacement effects'])))
 return result
