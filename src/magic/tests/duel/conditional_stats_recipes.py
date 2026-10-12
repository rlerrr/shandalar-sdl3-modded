"""Whole-Oracle continuous self-stat bonuses with real zone/count transitions."""
import copy,re
from collections import Counter
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from creature_recipes import REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS

KEYWORDS={'Flying','Reach','Shroud','Haste'}
SPELLS={
 'Sol Ring':('{1}','{T}: Add {C}{C}.'),
 'Twiddle':('{U}','You may tap or untap target artifact, creature, or land.'),
 'Morningtide':('{1}{W}','Exile all graveyards.'),
 'Tremor':('{R}','Tremor deals 1 damage to each creature without flying.'),
}

def spec(d,r):
 from recipes import cost_from_oracle
 if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or
     r.get('type_line','').startswith('Token ') or 'Creature' not in r.get('type_line','') or
     d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):raise ValueError('whole normal creature Oracle pin required')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'keywords','condition','power','toughness'}:raise ValueError('explicit condition, bonus and static keywords required')
 if (not isinstance(p['keywords'],list) or any(not isinstance(k,str) or k not in KEYWORDS for k in p['keywords']) or len(set(p['keywords']))!=len(p['keywords']) or
     p['condition'] not in ('metalcraft','threshold') or any(type(p[k]) is not int or not 0<=p[k]<=7 for k in ('power','toughness')) or not (p['power'] or p['toughness'])):raise ValueError('reviewed static keywords and bounded positive self-stat bonus required')
 for k in ('power','toughness'):
  if not re.fullmatch(r'\d+',r.get(k,'')) or not 0<=int(r[k])<=20:raise ValueError('numeric bounded Oracle characteristics required')
 if int(r['toughness'])==0:raise ValueError('host must survive fixture entry')
 end=('as long as you control three or more artifacts.' if p['condition']=='metalcraft' else 'as long as there are seven or more cards in your graveyard.')
 clause=p['condition'].capitalize()+f' — This creature gets +{p["power"]}/+{p["toughness"]} '+end
 lines=r['oracle_text'].splitlines();keys=[];found=0
 for line in lines:
  if line==clause:found+=1;continue
  text=line
  for reminder in re.findall(r'\([^\n]*?\)',text):
   if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed static keyword reminder')
   text=text.replace(reminder,'')
  parts=[x.strip().capitalize() for x in text.split(',')]
  if any(x not in KEYWORDS for x in parts):raise ValueError('additional clauses or effects require dedicated tests')
  keys+=parts
 if found!=1 or sorted(keys)!=p['keywords']:raise ValueError('explicit condition and keywords differ from whole Oracle text')
 return dict(p,family='conditional_stats',base_power=int(r['power']),base_toughness=int(r['toughness']),mana=cost_from_oracle(r),artifact='Artifact' in r['type_line'])

def counts(p):return (0,1,2,3,4) if p['condition']=='metalcraft' else (0,1,5,6,7,8)
def variants(p):
 vs=[]
 for owner in (0,1):
  for n in counts(p):vs += [f'{v}_{n}_{owner}' for v in ('effect','cast','combat','snapshot','recount')]
  vs += [f'{v}_{owner}' for v in ('opponent_only','offboard','duplicates','increase','decrease','damage_then_decrease','two_sources','cleanup','payment','insufficient_mana','bare_zones')]
  if p['condition']=='threshold':vs += [f'death_increase_{owner}']
 return vs

def validate_witnesses(o,rows):
 for nm,pin in SPELLS.items():
  r=o.get(nm,{})
  if nm not in rows or (r.get('mana_cost'),r.get('oracle_text'))!=pin:raise ValueError('conditional-stat witness changed: '+nm)
 for nm,cost,pt,text in (('Grizzly Bears','{1}{G}',('2','2'),''),('Ornithopter','{0}',('0','2'),'Flying'),('Phyrexian Hulk','{6}',('5','4'),'')):
  r=o.get(nm,{})
  if nm not in rows or (r.get('mana_cost'),(r.get('power'),r.get('toughness')),r.get('oracle_text'))!=(cost,pt,text):raise ValueError('conditional-stat board witness changed: '+nm)

def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  parts=variant.rsplit('_',2);owner=int(parts[-1]);indexed=len(parts)==3 and parts[-2].isdigit();v=parts[0] if indexed else variant.rsplit('_',1)[0];n=int(parts[-2]) if indexed else 0
  bare=v=='bare_zones';casting=v in ('cast','payment','insufficient_mana');metal=p['condition']=='metalcraft';limit=3 if metal else 7
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)];libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exile=[[] if bare else list(EXILE) for _ in (0,1)]
  mana=[[20]*6,[20]*6];life=[20,20];setup=[];entities={};pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)];actions=[]
  if v in ('increase','death_increase'):n=limit-1-int(metal and p['artifact'])
  if v in ('decrease','damage_then_decrease','cleanup','two_sources'):n=limit-int(metal and p['artifact'])
  if v=='duplicates':n=limit
  if not metal:
   grave_n=n if indexed or v in ('increase','decrease','damage_then_decrease','duplicates','death_increase','cleanup','two_sources') else 0
   graves[owner]=[GRAVE[i%len(GRAVE)] for i in range(grave_n)]
   if v=='duplicates':graves[owner]=['Grizzly Bears']*limit
   if v=='opponent_only':graves[1-owner]=['Grizzly Bears']*limit
   if v=='offboard':exile[owner]+=GRAVE*2
  # Start from the empty reusable fixture, then install stocked zones explicitly
  # so boundary graveyard counts are real initial inputs rather than overrides.
  for zone,players in (('hand',hands),('graveyard',graves),('library',libs),('exile',exile)):
   for pl,names in enumerate(players):
    for i,nm in enumerate(names):setup.append(card(nm,f'stock_{pl}_{zone}_{i}',pl,zone))
  def add(nm,ref,pl,zone='battlefield'):
   setup.append(card(nm,ref,pl,zone));r=oracle[nm];entities[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',creature='Creature' in r['type_line'],artifact='Artifact' in r['type_line'],tapped=False,damage=0,power=int(r['power']) if 'Creature' in r['type_line'] else 0,toughness=int(r['toughness']) if 'Creature' in r['type_line'] else 0)
   if zone=='hand':hands[pl].append(nm)
  def active():
   return (sum(e['alive'] and e['artifact'] and e['player']==owner for e in entities.values())>=3 if metal else len(graves[owner])>=7)
  def source_stats():return p['base_power']+p['power']*int(active()),p['base_toughness']+p['toughness']*int(active())
  def inspect(ref):
   e=entities[ref];out=[observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    out.append(observe(ref,'tapped',int(e['tapped'])))
    if e['creature']:
     power,toughness=source_stats() if e['name']==name else (e['power'],e['toughness']);out += [observe(ref,'power',power),observe(ref,'toughness',toughness),observe(ref,'damage',e['damage'])]
   return out
  def move(ref,zone):
   e=entities[ref];e['alive']=False;(hands if zone=='hand' else graves)[e['player']].append(e['name'])
  if metal:
   for i in range(n):add('Sol Ring' if v=='duplicates' or i%3==0 else 'Ornithopter' if i%3==1 else 'Phyrexian Hulk',f'artifact_{i}',owner)
   if v=='opponent_only':
    for i in range(3):add('Sol Ring',f'enemy_{i}',1-owner)
   if v=='offboard':
    for zone in ('hand','graveyard','exile','library'):
     if zone=='hand':add('Sol Ring','outside_hand',owner,'hand')
     else:
      {'graveyard':graves,'exile':exile,'library':libs}[zone][owner].append('Sol Ring');setup.append(card('Sol Ring','outside_'+zone,owner,zone))
  add(name,'source',owner,'hand' if casting else 'battlefield')
  if v=='two_sources':add(name,'second',owner)
  if v in ('increase','death_increase') and not metal:add('Grizzly Bears','qualifier',owner)
  def cast(ref,target=None,dialog=None):
   e=entities[ref];seq=([step('choose_target',player=owner,ref=target)] if target else [])+[step('cast',ref=ref,value=1)]
   if dialog is not None:seq.append(step('choose_dialog',player=owner,value=dialog))
   seq += [step('resolve'),step('assert_choices_consumed')];mana[owner]=[a-b for a,b in zip(mana[owner],cost_from_oracle(oracle[e['name']]))];hands[owner].remove(e['name'])
   if 'Instant' in oracle[e['name']]['type_line'] or 'Sorcery' in oracle[e['name']]['type_line']:graves[owner].append(e['name'])
   else:e['alive']=True
   return seq
  def spell(nm,ref,target=None,dialog=None):add(nm,ref,owner,'hand');return cast(ref,target,dialog)
  def damage_and_check():
   for ref,e in entities.items():
    if e['alive'] and e['creature'] and 'Flying' not in oracle[e['name']]['keywords']:e['damage']+=1
  def state_based():
   for ref,e in list(entities.items()):
    if not e['alive'] or not e['creature']:continue
    toughness=source_stats()[1] if e['name']==name else e['toughness']
    if e['damage']>=toughness:move(ref,'graveyard')
  if casting:
   if v in ('payment','insufficient_mana'):
    mana[owner]=list(p['mana']) if v=='payment' else [0]*6;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   if v=='insufficient_mana':pre += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
   else:pre+=cast('source')+[step('refresh')]
  if v=='damage_then_decrease':actions+=spell('Tremor','damage');damage_and_check();state_based();actions+=inspect('source')
  if v in ('increase','death_increase'):
   actions+=inspect('source')
   if metal:actions+=spell('Sol Ring','new_artifact')
   elif v=='death_increase':actions+=[step('destroy',ref='qualifier')];move('qualifier','graveyard')
   else:actions+=spell('Twiddle','new_card','qualifier',0);entities['qualifier']['tapped']=True
   actions += [step('refresh')]+inspect('source')
  if v in ('decrease','damage_then_decrease'):
   actions+=inspect('source')
   if metal:actions+=[step('destroy',ref='artifact_0')];move('artifact_0','graveyard')
   else:
    actions+=spell('Morningtide','clear_graves')
    # Morningtide's effect exiles the old graveyards; the resolving sorcery
    # itself reaches its owner's graveyard after that effect.
    for pl in (0,1):
     old=list(graves[pl]);
     if pl==owner:old.remove('Morningtide')
     exile[pl]+=old;graves[pl]=['Morningtide'] if pl==owner else []
   actions += [step('refresh')];state_based();actions+=inspect('source')
  if v=='recount':
   for _ in range(3):actions += [step('refresh')]+inspect('source')
  if v=='combat':
   pre += [step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat')]
   entities['source']['tapped']=True;actions += [step('combat_damage'),step('resolve'),step('end_combat')];life[1-owner]-=source_stats()[0]
  if v=='cleanup':actions += [step('cleanup'),step('refresh')]
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref in entities:checks+=inspect(ref)
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
  for pl in (0,1):
   checks += [step('assert_life',player=pl,value=life[pl])]+[step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   battle=[e['name'] for e in entities.values() if e['alive'] and e['player']==pl]
   for zone,names in (('battlefield',battle),('hand',hands[pl]),('graveyard',graves[pl]),('exile',exile[pl]),('library',libs[pl])):
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(names)))
    if zone=='library':checks += [step('assert_zone_card',player=pl,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     count=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=registry[nm]['csvid'],value=count[nm]) for nm in sorted(universe)]
  steps=pre+actions+checks
  if v=='snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'ConditionalStatsRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='conditional_stats',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['independent static keyword behavior','type and control changes','other continuous modifiers','additional triggers or abilities','graveyard replacement effects','multiplayer'])))
 return result
