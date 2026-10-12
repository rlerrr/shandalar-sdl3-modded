"""Fixed targeted damage abilities, independently pinned to whole Oracle text."""
import copy
from collections import Counter
from creature_recipes import classify
from passive_keyword_creature_recipes import split
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
from target_stats_activation_recipes import validate_witnesses as stat_witnesses
from tap_damage_creature_recipes import validate_witnesses as damage_witnesses

SCOPES={'any':'any target','creature':'target creature','flying_creature':'target creature with flying',
 'combat_creature':'target attacking or blocking creature','attacking_flying':'target attacking creature with flying',
 'player':'target player or planeswalker','opponent':'target opponent or planeswalker'}

def spec(d,r):
 from recipes import cost_from_oracle
 if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or
     r.get('type_line','').startswith('Token ') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('damage ability requires a whole normal-card Oracle pin and explicit variants')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'keywords','damage','targets','mana_cost','tap','sacrifice','pay_life','damage_you','damage_self','controller_life_loss','tap_target'}:
  raise ValueError('explicit source keywords, damage, scope, all costs and collateral effects required')
 if (not isinstance(p['targets'],str) or p['targets'] not in SCOPES or not isinstance(p['mana_cost'],str) or
     any(type(p[k]) is not bool for k in ('tap','sacrifice','tap_target')) or
     any(type(p[k]) is not int or not 0<=p[k]<=12 for k in ('damage','pay_life','damage_you','damage_self','controller_life_loss')) or
     not p['damage'] or p['pay_life']>5 or sum(bool(p[k]) for k in ('damage_you','damage_self','controller_life_loss','tap_target'))>1):
  raise ValueError('bounded damage, explicit ordinary costs, and one reviewed collateral effect required')
 creature='Creature' in r['type_line']
 if creature:
  clauses,plain=split(r);base=classify(plain);kind='creature'
 else:
  if r['type_line'] not in ('Artifact','Enchantment'):raise ValueError('ordinary source permanent required')
  clauses=r['oracle_text'].splitlines();base=dict(keywords=[],mana=cost_from_oracle(r),power=0,toughness=0);kind=r['type_line'].lower()
 if p['keywords']!=base['keywords'] or any(k not in ('Flying','Defender','Haste') for k in base['keywords']):
  raise ValueError('only independently reviewed Flying/Defender/Haste sources supported')
 activation=[0]*6 if not p['mana_cost'] else cost_from_oracle({'mana_cost':p['mana_cost']})
 costs=([p['mana_cost']] if p['mana_cost'] else [])+(['{T}'] if p['tap'] else [])+([f'Pay {p["pay_life"]} life'] if p['pay_life'] else [])+([f'Sacrifice this {kind}'] if p['sacrifice'] else [])
 if not costs:raise ValueError('nonempty cost required')
 suffix='.'
 if p['damage_you']:suffix=f' and {p["damage_you"]} damage to you.'
 if p['damage_self']:suffix=f' and {p["damage_self"]} damage to itself.'
 if p['controller_life_loss']:suffix=f' and that creature\'s controller loses {p["controller_life_loss"]} life.'
 if p['tap_target']:suffix='. Tap that creature.'
 if (p['controller_life_loss'] or p['tap_target']) and p['targets'] in ('any','player','opponent'):
  raise ValueError('creature-only collateral effects required')
 refs=('It',) if p['sacrifice'] else (f'This {kind}',r['name'],r['name'].split(',')[0])
 expected={', '.join(costs)+f': {ref} deals {p["damage"]} damage to {SCOPES[p["targets"]]}{suffix}' for ref in refs}
 if len(clauses)!=1 or clauses[0] not in expected:raise ValueError('only one whole fixed damage ability reviewed')
 return dict(p,family='damage_activation',activation_mana=activation,mana=base['mana'],creature=creature,
  base_power=base['power'],base_toughness=base['toughness'],legendary='Legendary' in r['type_line'])

def variants(p):
 vs=['effect','payment','tapped','opposing_turn','cast','cast_payment','insufficient_cast_mana','cleanup',
     'tax','tax_insufficient','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot','prevention']
 if p['creature']:vs+=['sick','fresh_activation']
 if sum(p['activation_mana']):vs+=['insufficient_mana']
 if any(p['activation_mana'][1:]):vs+=['wrong_color']
 if p['pay_life']:vs+=['insufficient_life','low_life']
 player_only=p['targets'] in ('player','opponent')
 if p['targets'] in ('any','player'):vs+=['self_player']
 if p['targets']=='any':vs+=['opponent_player']
 if player_only or p['targets']=='any':vs+=['player_shrouded_pending']
 if not player_only:
  vs+=['friendly','lethal','indestructible','target_bounced','target_destroyed','target_shrouded_pending','combat_damage']
  if p['targets']!='any':vs+=['no_targets','shrouded_targets']
  if p['targets'] in ('flying_creature','attacking_flying'):vs+=['wrong_target']
  if p['targets'] in ('combat_creature','attacking_flying'):vs+=['wrong_role','removed_from_combat']
  if p['targets']=='combat_creature':vs+=['blocking_target','declared_blocker_death']
  if p['creature'] and p['targets'] in ('any','creature'):vs+=['self_target']
 if not p['sacrifice']:
  vs+=['source_bounced','source_destroyed']
  if p['damage_self']<p['base_toughness'] or not p['creature']:
   vs+=['repeat_denied','untap_repeat'] if p['tap'] else ['stacked']
 if not p['legendary']:vs+=['two_sources']
 return [v+'_'+str(o) for o in (0,1) for v in vs]

def validate_witnesses(o,rows):
 stat_witnesses(o,rows);damage_witnesses(o,rows)
 for nm,cost,text in (
  ("Alexi's Cloak",'{1}{U}',"Flash\nEnchant creature\nEnchanted creature has shroud. (It can't be the target of spells or abilities.)"),
  ('Safe Passage','{2}{W}','Prevent all damage that would be dealt to you and creatures you control this turn.'),
  ('Gilded Light','{1}{W}',"You gain shroud until end of turn. (You can't be the target of spells or abilities.)\nCycling {2} ({2}, Discard this card: Draw a card.)")):
  if nm not in rows or (o.get(nm,{}).get('mana_cost'),o.get(nm,{}).get('oracle_text'))!=(cost,text):raise ValueError('damage response witness changed: '+nm)
 for nm,pt,text in (
  ('Mahamoti Djinn',('5','6'),"Flying (This creature can't be blocked except by creatures with flying or reach.)"),
  ('Darksteel Gargoyle',('3','3'),'Flying\nIndestructible (Damage and effects that say "destroy" don\'t destroy this creature.)'),
  ('Krosan Cloudscraper',('13','13'),'At the beginning of your upkeep, sacrifice this creature unless you pay {G}{G}.\nMorph {7}{G}{G} (You may cast this card face down as a 2/2 creature for {3}. Turn it face up any time for its morph cost.)')):
  r=o.get(nm,{})
  if nm not in rows or (r.get('power'),r.get('toughness'))!=pt or r.get('oracle_text')!=text:raise ValueError('damage survivor witness changed: '+nm)

def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones'
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
  mana=[[20]*6,[20]*6];life=[20,20];setup=[];entities={};pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)];actions=[]
  def add(nm,ref,player,zone='battlefield',state=None):
   item=card(nm,ref,player,zone)
   if state:item['state']=state
   setup.append(item);r=oracle[nm];creature='Creature' in r['type_line']
   entities[ref]=dict(name=nm,player=player,alive=zone=='battlefield',tapped=bool(state and 'tapped' in state),creature=creature,damage=0,
    power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,indestructible=nm in ('Darksteel Myr','Darksteel Gargoyle'))
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
  player_target=p['targets'] in ('player','opponent') or v in ('self_player','opponent_player','player_shrouded_pending')
  target_owner=owner if v in ('friendly','self_player','self_target') else 1-owner
  flying=p['targets'] in ('flying_creature','attacking_flying')
  recipient='Mahamoti Djinn' if flying else 'Krosan Cloudscraper' if p['damage']>=6 else 'Scaled Wurm'
  if v in ('lethal','declared_blocker_death'):recipient='Suntail Hawk' if flying else 'Muck Rats'
  if v=='indestructible':recipient='Darksteel Gargoyle' if flying else 'Darksteel Myr'
  if v=='wrong_target':recipient='Scaled Wurm'
  negative=v in ('no_targets','wrong_target','wrong_role','shrouded_targets')
  if not player_target and v not in ('no_targets','self_target'):add(recipient,'recipient',target_owner)
  if not negative:add('Mahamoti Djinn' if flying else 'Scaled Wurm','unselected',1-owner)
  if v=='two_sources':add(name,'second',owner)
  blocking=v in ('blocking_target','declared_blocker_death')
  if blocking:add('Hill Giant','attacker',1-target_owner)
  if v in ('source_bounced','target_bounced','target_shrouded_pending','untap_repeat'):
   add('Twiddle' if v=='untap_repeat' else "Alexi's Cloak" if v=='target_shrouded_pending' else 'Boomerang','response',owner,'hand')
  if v=='prevention':add('Safe Passage','response',target_owner,'hand')
  if v=='player_shrouded_pending':add('Gilded Light','response',target_owner,'hand')
  if v in ('tax','tax_insufficient'):
   add('Suppression Field','taxer',1-owner,'hand');pre += [step('turn',player=1-owner,value=0)]+cast('taxer')+[step('turn',player=owner,value=0)]
  if negative and p['creature']:
   add('Robe of Mirrors','source_robe',owner,'hand');pre+=cast('source_robe','source')
  if v=='shrouded_targets':add('Robe of Mirrors','target_robe',owner,'hand');pre+=cast('target_robe','recipient')
  if casting:
   if v in ('cast_payment','insufficient_cast_mana'):
    mana[owner]=list(p['mana']) if v=='cast_payment' else [0]*6;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   if v=='insufficient_cast_mana':pre += [step('cast',ref='source',value=0),step('assert_choices_consumed'),step('assert_stack',value=0)]
   else:
    pre+=cast('source')+[step('refresh')]
    # Haste is checked by actual activation availability, not raw sick flags.
    if p['creature'] and 'Haste' not in p['keywords']:pre+=[observe('source','sick',1)]
  restricted=p['targets'] in ('combat_creature','attacking_flying')
  combat=(not negative or restricted and v in ('shrouded_targets','wrong_target')) and not player_target and v!='self_target' and (restricted or v=='combat_damage')
  if combat:
   attacker='attacker' if blocking else 'recipient';pl=entities[attacker]['player']
   pre += [step('turn',player=pl,value=0),step('begin_attackers'),step('attack',ref=attacker,value=1),step('finish_attackers'),step('begin_blockers')]
   entities[attacker]['tapped']=True
   if blocking:pre += [step('block',ref='recipient',other=attacker,value=1)]
   pre += [step('advance_combat')]
  if v=='opposing_turn' and not restricted:pre.append(step('turn',player=1-owner,value=0))
  cost=list(p['activation_mana']);cost[0]+=2*int(v in ('tax','tax_insufficient'))
  if v in ('payment','tax','tax_insufficient','insufficient_mana','wrong_color'):
   pool=list(cost) if v in ('payment','tax') else list(p['activation_mana']) if v=='tax_insufficient' else [0]*6
   if v=='wrong_color':
    pool=list(cost);color=next(c for c in range(1,6) if pool[c]);pool[1 if color!=1 else 2]+=pool[color];pool[color]=0
   mana[owner]=pool;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(pool)]
  if v in ('insufficient_life','low_life'):
   life[owner]=p['pay_life']-1 if v=='insufficient_life' else p['pay_life']+1;pre+=[step('set_life',player=owner,value=life[owner])]
  legal=not negative and v not in ('insufficient_mana','wrong_color','tax_insufficient','insufficient_cast_mana','insufficient_life') and not (p['tap'] and (v=='tapped' or p['creature'] and v in ('sick','fresh_activation') and 'Haste' not in p['keywords']))
  chosen='source' if v=='self_target' else 'recipient'
  def inspect(ref):
   e=entities[ref];s=[observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    s += [observe(ref,'tapped',int(e['tapped']))]
    if e['creature']:s += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness']),observe(ref,'damage',e['damage'])]
   return s
  def pending(source='source'):
   choices=[step('choose_player',player=owner,value=target_owner)] if player_target else [step('choose_target',player=owner,ref=chosen)]
   seq=[step('require_can_activate',ref=source,value=1)]+choices+[step('activate',ref=source,value=1),step('assert_choices_consumed')]
   pay(owner,cost);life[owner]-=p['pay_life']
   if p['tap']:entities[source]['tapped']=True
   if p['sacrifice']:move(source,'graveyard')
   seq+=inspect(source)
   if not player_target:seq+=inspect(chosen)
   return seq+[step('assert_life',player=pl,value=life[pl]) for pl in (0,1)]
  def hit(ref,amount):
   e=entities[ref]
   if not e['alive']:return
   e['damage']+=amount
   if e['damage']>=e['toughness'] and not e['indestructible']:move(ref,'graveyard')
  def apply(source='source'):
   prevent=v=='prevention'
   if player_target:
    if not prevent:life[target_owner]-=p['damage']
   else:
    if not prevent:hit(chosen,p['damage'])
    life[target_owner]-=p['controller_life_loss']
    if p['tap_target'] and entities[chosen]['alive']:entities[chosen]['tapped']=True
   life[owner]-=0 if prevent and target_owner==owner else p['damage_you']
   if entities[source]['alive'] and p['damage_self'] and not (prevent and target_owner==owner):hit(source,p['damage_self'])
  ready=pending_cut=0
  if v in ('cast','cast_payment','insufficient_cast_mana'):pass
  elif not legal:actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_stack',value=0),step('assert_choices_consumed')]
  else:
   ready=len(actions);actions+=pending()+[step('assert_stack',value=1)];pending_cut=len(actions)
   if v in ('two_sources','stacked'):actions+=pending('second' if v=='two_sources' else 'source')+[step('assert_stack',value=2)]
   if v in ('target_bounced','source_bounced','target_shrouded_pending','prevention','player_shrouded_pending'):
    target='source' if v=='source_bounced' else 'recipient'
    actions+=cast('response',target if v in ('target_bounced','source_bounced','target_shrouded_pending') else None)
    if v in ('target_bounced','source_bounced'):move(target,'hand')
   if v in ('source_destroyed','target_destroyed'):
    target='source' if v=='source_destroyed' else 'recipient';actions += [step('destroy',ref=target)];move(target,'graveyard')
   if v=='removed_from_combat':actions+=[step('remove_from_combat',ref='recipient')]
   actions += [step('resolve'),step('assert_choices_consumed')]
   if v not in ('target_bounced','target_destroyed','target_shrouded_pending','player_shrouded_pending','removed_from_combat'):
    if v in ('two_sources','stacked'):apply('second' if v=='two_sources' else 'source')
    apply()
   if v=='repeat_denied':actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
   if v=='untap_repeat':
    actions+=cast('response','source',1);entities['source']['tapped']=False
    actions+=pending()+[step('resolve'),step('assert_choices_consumed')];apply()
   if not player_target:actions+=inspect(chosen)
   if v in ('combat_damage','declared_blocker_death'):
    actions += [step('combat_damage'),step('resolve'),step('end_combat')]
    if blocking:
     # A blocker killed by the ability still leaves the attacker blocked.
     if entities['recipient']['alive']:
      power=entities['recipient']['power'];hit('recipient',entities['attacker']['power']);hit('attacker',power)
    elif entities['recipient']['alive']:life[1-target_owner]-=entities['recipient']['power']
   if v=='cleanup':
    actions += [step('cleanup'),step('refresh')]
    for e in entities.values():e['damage']=0
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref in entities:checks+=inspect(ref)
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
  for pl in (0,1):
   checks += [step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))]
   checks += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for zone,names in (('hand',hands[pl]),('graveyard',graves[pl]),('exile',exiles[pl]),('library',libs[pl])):
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(names)))
    if zone=='library':checks += [step('assert_zone_card',player=pl,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=registry[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=ready if v=='ready_snapshot' else pending_cut;tail=actions[cut:]+checks
   steps=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'DamageActivationRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='damage_activation',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['planeswalker targeting','regeneration','color protection','damage redirection','source static keywords except haste availability','changing target characteristics while pending','alternate modes','additional costs or effects'])))
 return result
