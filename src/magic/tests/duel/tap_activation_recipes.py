"""Whole-Oracle fixed tap/untap abilities; real costs, targeting and combat."""
import copy
from collections import Counter
from creature_recipes import classify
from passive_keyword_creature_recipes import split
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY
from target_stats_activation_recipes import ROBE, validate_witnesses as stat_witnesses

SCOPES = {
 'creature':'creature', 'flying_creature':'creature with flying',
 'nonflying_creature':'creature without flying', 'green_creature':'green creature',
 'nonhuman_creature':'non-Human creature', 'small_power_2':'creature with power 2 or less',
 'small_power_3':'creature with power 3 or less', 'small_toughness_2':'creature with toughness 2 or less',
 'wall':'Wall', 'artifact':'artifact', 'artifact_creature':'artifact creature',
 'enemy_artifact':"artifact you don't control", 'land':'land', 'forest':'Forest', 'gate':'Gate',
 'artifact_creature_land':'artifact, creature, or land', 'permanent':'permanent',
}
TARGETS = {
 'creature':'Grizzly Bears', 'flying_creature':'Air Elemental',
 'nonflying_creature':'Grizzly Bears', 'green_creature':'Scaled Wurm',
 'nonhuman_creature':'Grizzly Bears', 'small_power_2':'Grizzly Bears',
 'small_power_3':'Hill Giant', 'small_toughness_2':'Grizzly Bears',
 'wall':'Wall of Stone', 'artifact':'Ornithopter', 'artifact_creature':'Ornithopter',
 'enemy_artifact':'Ornithopter', 'land':'Forest', 'forest':'Forest', 'gate':'Selesnya Guildgate',
 'artifact_creature_land':'Grizzly Bears', 'permanent':'Grizzly Bears',
}
WRONG = {
 'flying_creature':'Grizzly Bears', 'nonflying_creature':'Air Elemental',
 'green_creature':'Hill Giant', 'nonhuman_creature':'Elite Vanguard',
 'small_power_2':'Hill Giant', 'small_power_3':'Scaled Wurm',
 'small_toughness_2':'Hill Giant', 'wall':'Grizzly Bears',
 'artifact':'Grizzly Bears', 'artifact_creature':'Sol Ring', 'enemy_artifact':'Grizzly Bears',
 'land':'Grizzly Bears', 'forest':'Island', 'gate':'Forest',
}

def spec(d, r):
 from recipes import cost_from_oracle
 if (not r or r.get('layout','normal')!='normal' or r.get('card_faces') or
     r.get('type_line','').startswith('Token ') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('tap ability requires a whole normal-card Oracle pin and explicit variants')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'keywords','effect','targets','mana_cost','tap'}:
  raise ValueError('explicit source keywords, effect, target scope and costs required')
 if (not isinstance(p['effect'],str) or not isinstance(p['targets'],str) or
     p['effect'] not in ('tap','untap') or p['targets'] not in SCOPES or
     type(p['tap']) is not bool or not isinstance(p['mana_cost'],str)):
  raise ValueError('reviewed effect, target scope and boolean tap cost required')
 creature='Creature' in r.get('type_line','')
 if creature:
  abilities,plain=split(r);base=classify(plain)
 else:
  if r.get('type_line') not in ('Artifact','Enchantment') or p['keywords']!=[]:
   raise ValueError('only ordinary artifacts/enchantments or keyword creatures reviewed')
  abilities=r['oracle_text'].splitlines();base=dict(keywords=[],mana=cost_from_oracle(r),power=0,toughness=0)
 if p['keywords']!=base['keywords'] or any(k not in ('Flying','Defender') for k in base['keywords']):
  raise ValueError('only reviewed Flying/Defender source keywords supported')
 activation=[0]*6 if not p['mana_cost'] else cost_from_oracle({'mana_cost':p['mana_cost']})
 cost=', '.join(([p['mana_cost']] if p['mana_cost'] else [])+(['{T}'] if p['tap'] else []))
 if not cost or abilities!=[f'{cost}: {p["effect"].capitalize()} target {SCOPES[p["targets"]]}.']:
  raise ValueError('single complete fixed tap/untap clause required; extra abilities deferred')
 return dict(p,family='tap_activation',activation_mana=activation,mana=base['mana'],
  creature=creature,source_type=r['type_line'],base_power=base['power'],base_toughness=base['toughness'])

def can_hide_source(p):
 # Robe protects any creature source. An ordinary noncreature source is not
 # eligible for these restricted scopes. Do not invent absence of legal self targets.
 return p['creature'] or p['targets'] not in ('artifact','artifact_creature_land','permanent')

def variants(p):
 vs=['effect','payment','tapped','opposing_turn','cast','cast_payment','cleanup',
     'target_bounced','target_destroyed','source_bounced','source_destroyed',
     'tax','tax_insufficient','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot',
     'already_in_state','two_sources','two_targets']
 if p['creature']:vs+=['sick','fresh_activation']
 if p['targets']!='enemy_artifact':vs+=['friendly']
 else:vs+=['wrong_controller']
 if sum(p['mana']):vs+=['insufficient_cast_mana']
 if sum(p['activation_mana']):vs+=['insufficient_mana']
 if any(p['activation_mana'][1:]):vs+=['wrong_color']
 if can_hide_source(p):
  vs+=['no_targets']
  if p['targets'] in WRONG:vs+=['wrong_target']
  if p['targets'] not in ('land','forest','gate'):vs+=['shrouded_targets']
 if p['targets'] not in ('land','forest','gate'):vs+=['target_shrouded_pending','combat_block','declared_blocker']
 if p['targets'] not in ('land','forest','gate','wall'):vs+=['combat_attack','declared_attacker']
 if p['tap']:vs+=['repeat_denied','untap_repeat']
 else:vs+=['stacked']
 if p['creature'] and p['targets'] in ('creature','artifact_creature_land','permanent'):vs+=['self_target']
 if not p['creature'] and p['targets'] in ('artifact','artifact_creature_land','permanent'):vs+=['self_target']
 if p['targets'] in ('land','forest','artifact','artifact_creature_land','permanent','enemy_artifact'):vs+=['mana_source']
 if p['targets'] in ('artifact_creature_land','permanent'):vs+=['artifact_target','land_target']
 return [v+'_'+str(owner) for owner in (0,1) for v in vs]

def validate_witnesses(o,registry):
 stat_witnesses(o,registry)
 pins={
  'Grizzly Bears':('Creature — Bear','{1}{G}','', '2','2'),
  'Forest':('Basic Land — Forest','','({T}: Add {G}.)',None,None),
  'Island':('Basic Land — Island','','({T}: Add {U}.)',None,None),
  'Selesnya Guildgate':('Land — Gate','','This land enters tapped.\n{T}: Add {G} or {W}.',None,None),
  'Sol Ring':('Artifact','{1}','{T}: Add {C}{C}.',None,None),
 }
 for nm,pin in pins.items():
  r=o.get(nm,{})
  if nm not in registry or tuple(r.get(k) for k in ('type_line','mana_cost','oracle_text','power','toughness'))!=pin:
   raise ValueError('tap/untap witness changed: '+nm)

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
  target_owner=owner if v in ('friendly','wrong_controller') or v=='mana_source' and p['targets']!='enemy_artifact' else 1-owner
  recipient=TARGETS[p['targets']]
  if v=='wrong_target':recipient=WRONG[p['targets']]
  if v in ('mana_source','artifact_target'):recipient='Forest' if p['targets'] in ('land','forest') else 'Sol Ring'
  if v=='land_target':recipient='Forest'
  target_initial=p['effect']=='untap'
  if v=='already_in_state':target_initial=not target_initial
  if v in ('declared_attacker','declared_blocker'):target_initial=False
  negative=v in ('no_targets','wrong_target','wrong_controller','shrouded_targets')
  if v!='no_targets':add(recipient,'recipient',target_owner,state=['tapped'] if target_initial else None)
  if not negative:add(TARGETS[p['targets']],'unselected',target_owner,state=['tapped'] if target_initial else None)
  if v in ('two_sources','two_targets'):add(name,'second',owner)
  if v=='two_targets':add(TARGETS[p['targets']],'other_target',target_owner,state=['tapped'] if target_initial else None)
  if v in ('combat_block','declared_blocker'):add('Hill Giant','attacker',1-target_owner)
  if v in ('source_bounced','target_bounced','target_shrouded_pending','untap_repeat'):
   add('Twiddle' if v=='untap_repeat' else "Alexi's Cloak" if v=='target_shrouded_pending' else 'Boomerang','response',owner,'hand')
  if v=='already_in_state' and p['targets']=='gate':
   # Gates enter tapped through the real put-into-play path. Establish ready
   # targets by legal spells, not an absent fixture tap flag.
   for target in ('recipient','unselected'):
    ref='ready_'+target;add('Twiddle',ref,owner,'hand');pre+=cast(ref,target,1)
  if v=='untap_repeat' and p['targets']=='gate':add('Twiddle','retap_target',owner,'hand')
  if v in ('tax','tax_insufficient'):
   add('Suppression Field','taxer',1-owner,'hand');pre += [step('turn',player=1-owner,value=0)]+cast('taxer')+[step('turn',player=owner,value=0)]
  if negative and p['creature']:
   add('Robe of Mirrors','source_robe',owner,'hand');pre+=cast('source_robe','source')
  if v=='shrouded_targets':
   add('Robe of Mirrors','target_robe',owner,'hand');pre+=cast('target_robe','recipient')
  if casting:
   if v in ('cast_payment','insufficient_cast_mana'):
    mana[owner]=list(p['mana']) if v=='cast_payment' else [0]*6;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   if v=='insufficient_cast_mana':pre += [step('cast',ref='source',value=0),step('assert_choices_consumed'),step('assert_stack',value=0)]
   else:
    pre+=cast('source')+[step('refresh')]
    if p['creature']:pre+=[observe('source','sick',1)]
  if v=='opposing_turn':pre.append(step('turn',player=1-owner,value=0))
  if v in ('declared_attacker','declared_blocker'):
   attacker='recipient' if v=='declared_attacker' else 'attacker';player=entities[attacker]['player']
   pre += [step('turn',player=player,value=0),step('begin_attackers'),step('attack',ref=attacker,value=1),step('finish_attackers'),step('begin_blockers')]
   entities[attacker]['tapped']=True
   if v=='declared_blocker':pre += [step('block',ref='recipient',other=attacker,value=1)]
   pre += [step('advance_combat')]
  cost=list(p['activation_mana']);cost[0]+=2*int(v in ('tax','tax_insufficient'))
  if v in ('payment','tax','tax_insufficient','insufficient_mana','wrong_color'):
   pool=list(cost) if v in ('payment','tax') else list(p['activation_mana']) if v=='tax_insufficient' else [0]*6
   if v=='wrong_color':
    pool=list(cost);color=next(c for c in range(1,6) if pool[c]);pool[1 if color!=1 else 2]+=pool[color];pool[color]=0
   mana[owner]=pool;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(pool)]
  legal=not negative and v not in ('insufficient_mana','wrong_color','tax_insufficient','insufficient_cast_mana') and not (p['tap'] and (v=='tapped' or p['creature'] and v in ('sick','fresh_activation')))
  def inspect(ref):
   e=entities[ref];s=[observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    s += [observe(ref,'tapped',int(e['tapped']))]
    if e['creature']:s += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness'])]
   return s
  def pending(source='source',target='recipient'):
   seq=[step('require_can_activate',ref=source,value=1),step('choose_target',player=owner,ref=target),step('activate',ref=source,value=1),step('assert_choices_consumed')]
   pay(owner,cost)
   if p['tap']:entities[source]['tapped']=True
   return seq+inspect(source)+inspect(target)
  def apply(target='recipient'):
   if entities[target]['alive']:entities[target]['tapped']=p['effect']=='tap'
  ready=pending_cut=0
  if v in ('cast','cast_payment','insufficient_cast_mana'):pass
  elif not legal:actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_stack',value=0),step('assert_choices_consumed')]
  else:
   chosen='source' if v=='self_target' else 'recipient'
   ready=len(actions);actions+=pending(target=chosen)+[step('assert_stack',value=1)];pending_cut=len(actions)
   if v in ('two_sources','stacked','two_targets'):actions+=pending('second' if v!='stacked' else 'source','other_target' if v=='two_targets' else 'recipient')+[step('assert_stack',value=2)]
   if v in ('target_bounced','source_bounced','target_shrouded_pending'):
    target='source' if v=='source_bounced' else 'recipient';actions+=cast('response',target)
    if v!='target_shrouded_pending':move(target,'hand')
   if v in ('source_destroyed','target_destroyed'):
    target='source' if v=='source_destroyed' else 'recipient';actions += [step('destroy',ref=target)];move(target,'graveyard')
   actions += [step('resolve'),step('assert_choices_consumed')]
   if v not in ('target_bounced','target_destroyed','target_shrouded_pending'):apply(chosen)
   if v in ('two_sources','stacked','two_targets'):apply('other_target' if v=='two_targets' else 'recipient')
   if v=='repeat_denied':actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
   if v=='untap_repeat':
    actions+=cast('response','source',1)+[step('refresh'),observe('source','tapped',0)];entities['source']['tapped']=False
    if p['targets']=='gate':actions+=cast('retap_target','recipient',0);entities['recipient']['tapped']=True
    actions+=pending()+[step('resolve'),step('assert_choices_consumed')];apply()
   actions+=inspect(chosen)
   if v in ('combat_attack','combat_block'):
    attacker='recipient' if v=='combat_attack' else 'attacker';player=entities[attacker]['player']
    actions += [step('turn',player=player,value=0),step('begin_attackers'),step('attack',ref=attacker,value=int(not entities[attacker]['tapped']))]
    if not entities[attacker]['tapped']:entities[attacker]['tapped']=True
    actions += [step('finish_attackers'),step('begin_blockers')]
    if v=='combat_block':actions += [step('block',ref='recipient',other='attacker',value=int(not entities['recipient']['tapped']))]
    actions += [step('advance_combat')]
   if v in ('combat_attack','declared_attacker','combat_block','declared_blocker'):
    actions += [step('combat_damage'),step('resolve'),step('end_combat')]
    if v in ('combat_attack','declared_attacker'):
     if v=='declared_attacker' or p['effect']=='untap':life[1-target_owner]-=entities['recipient']['power']
    elif v=='declared_blocker' or p['effect']=='untap':
     if entities['recipient']['power']>=entities['attacker']['toughness']:move('attacker','graveyard')
     if entities['attacker']['power']>=entities['recipient']['toughness']:move('recipient','graveyard')
    else:life[target_owner]-=entities['attacker']['power']
   if v=='mana_source':
    actions += [step('require_can_activate',ref='recipient',value=int(p['effect']=='untap'))]
    if p['effect']=='untap':
     actions += [step('activate',ref='recipient',value=1),step('assert_stack',value=0),step('assert_choices_consumed')];entities['recipient']['tapped']=True
     mana[target_owner][3 if recipient=='Forest' else 0]+=1 if recipient=='Forest' else 2
   if v=='cleanup':actions += [step('cleanup'),step('refresh')]
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
  result.append(dict(name=f'TapActivationRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='tap_activation',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],
    untested_components=['alternate mana modes','extra effects','changing power/toughness restrictions while pending','control changes','noncreature target shroud','static source keywords','self targeting in restricted scopes'])))
 return result
