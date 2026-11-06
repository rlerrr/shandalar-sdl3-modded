"""Whole-Oracle prevention rules exercised through casting and actual damage."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

# Each entry is a reviewed rule, not a dispatch-handler inference. Extra clauses
# admitted below are inactive on these boards and remain explicit coverage gaps.
RULES = {
 'fog': 'Prevent all combat damage that would be dealt this turn.',
 'players_combat': 'Prevent all combat damage that would be dealt to players this turn.',
 'creatures_players': 'Prevent all damage that creatures would deal to players this turn.',
 'own_creatures': 'Prevent all damage that would be dealt this turn to creatures you control.',
 'creatures': 'Prevent all damage that would be dealt to creatures this turn.',
 'own_team': 'Prevent all damage that would be dealt to you and creatures you control this turn.',
 'you': 'Prevent all damage that would be dealt to you this turn.',
 'you_combat': 'Prevent all combat damage that would be dealt to you this turn.',
 'nongreen': 'Prevent all combat damage that would be dealt by nongreen creatures this turn.',
 'nontrample': 'Prevent all combat damage that would be dealt this turn by creatures without trample.',
 'small': 'Prevent all combat damage that would be dealt this turn by creatures with power 4 or less.',
 'nonhuman': 'Prevent all damage that would be dealt this turn by non-Human sources.',
 'all_creatures': 'Prevent all damage that would be dealt to creatures.',
 'own_noncombat': 'Prevent all noncombat damage that would be dealt to creatures you control.',
 'own_sources': 'Prevent all damage that would be dealt to creatures you control by sources you control.',
 'own_attackers': 'Prevent all combat damage that would be dealt to attacking creatures you control.',
 'own_combat': 'Prevent all combat damage that would be dealt to and dealt by creatures you control.',
 'own_turn': 'During your turn, prevent all damage that would be dealt to you.',
 'armor': 'If a source would deal damage to you, prevent 1 of that damage.',
 'self_all': 'Prevent all damage that would be dealt to this creature.',
 'self_combat': 'Prevent all combat damage that would be dealt to this creature.',
 'self_both': 'Prevent all combat damage that would be dealt to and dealt by this creature.',
 'self_blockers': 'Prevent all combat damage that would be dealt to this creature by creatures blocking it.',
 'self_creatures': 'Prevent all damage that would be dealt to this creature by creatures.',
 'aura_all': 'Prevent all damage that would be dealt by enchanted creature.',
 'aura_both': 'Prevent all combat damage that would be dealt to and dealt by enchanted creature.',
}
GAPS = {
 'flashback': '\nFlashback {2}{G} (You may cast this card from your graveyard for its flashback cost. Then exile it.)',
 'populate': " Populate. (Create a token that's a copy of a creature token you control.)",
 'face_down_look': " You may look at each face-down creature that's attacking or blocking.",
}
KEYWORD_TEXT = {
 (): '', ('Flying',): 'Flying\n',
 ('Defender','Flying'): 'Defender, flying\n',
 ('Defender','Flying','reminder'): "Defender (This creature can't attack.)\nFlying\n",
}
BASE_GAPS = ['multiple blockers', 'damage redirection', 'damage that cannot be prevented',
             'control changes', 'multiplayer', 'independent source keyword mechanics']


def spec(d, r):
 from recipes import cost_from_oracle
 if isinstance(d.get('params'),dict) and d['params'].get('extension')=='replacement':return _replacement_spec(d,r)
 if isinstance(d.get('params'),dict) and d['params'].get('extension')=='modifier':return _modifier_spec(d,r)
 if isinstance(d.get('params'),dict) and d['params'].get('extension')=='entry_shield':return _entry_shield_spec(d,r)
 if isinstance(d.get('params'),dict) and 'extension' in d['params']:return _shield_spec(d,r)
 if (not r or r.get('layout') != 'normal' or r.get('card_faces') or r.get('selected_face')
     or r.get('type_line','').startswith('Token ') or d.get('oracle_text') != r.get('oracle_text')
     or 'variants' not in d):
  raise ValueError('prevention requires a whole normal-card Oracle pin and explicit variants')
 p = d.get('params')
 if not isinstance(p, dict) or set(p) != {'kind','scope','gain','activation_cost','tap','sacrifice','keywords','inactive_clauses'}:
  raise ValueError('explicit prevention kind, scope, costs, keywords and inactive clauses required')
 if (any(not isinstance(p[k],str) for k in ('kind','scope','gain'))
     or p['kind'] not in ('spell','static','self','aura','activation') or p['scope'] not in RULES
     or p['gain'] not in ('none','attackers','creatures','opposing_creatures')
     or not isinstance(p['activation_cost'],str) or type(p['tap']) is not bool or type(p['sacrifice']) is not bool
     or not isinstance(p['keywords'],list) or any(not isinstance(k,str) for k in p['keywords'])
     or not isinstance(p['inactive_clauses'],list) or any(not isinstance(k,str) for k in p['inactive_clauses'])
     or len(set(p['inactive_clauses'])) != len(p['inactive_clauses'])
     or any(x not in GAPS for x in p['inactive_clauses'])):
  raise ValueError('unsupported prevention parameters')
 creature = 'Creature' in r['type_line']
 expected = RULES[p['scope']]
 if p['kind'] == 'spell':
  if r['type_line'] not in ('Instant','Sorcery') or p['scope'] not in ('fog','players_combat','creatures_players','own_creatures','creatures','own_team','you','you_combat','nongreen','nontrample','small','nonhuman'):
   raise ValueError('ordinary reviewed prevention spell required')
 elif p['kind'] == 'activation':
  if not creature or p['scope'] != 'fog' or p['gain'] != 'none' or bool(p['tap']) == bool(p['sacrifice']):
   raise ValueError('reviewed tapping or sacrificing creature Fog required')
  costs = ([p['activation_cost']] if p['activation_cost'] else []) + (['{T}'] if p['tap'] else []) + (['Sacrifice this creature'] if p['sacrifice'] else [])
  expected = ', '.join(costs) + ': ' + expected
  if p['tap']: expected += ' Activate only before the combat damage step.'
 elif p['kind'] == 'aura':
  if r['type_line'] != 'Enchantment — Aura' or p['scope'] not in ('aura_all','aura_both'):
   raise ValueError('ordinary prevention Aura required')
  expected = 'Enchant creature\n' + ('Enchanted creature has flying.\n' if p['keywords']==['Flying'] else '') + expected
 elif p['kind'] == 'self':
  if not creature or not p['scope'].startswith('self_'): raise ValueError('self prevention creature required')
  if r['name'] == 'Cho-Manno, Revolutionary' and p['scope']=='self_all': expected = 'Prevent all damage that would be dealt to Cho-Manno.'
  key=tuple(p['keywords'])
  if key not in KEYWORD_TEXT:raise ValueError('unreviewed prevention creature keywords')
  if r['name']=='Fog Bank':key=('Defender','Flying','reminder')
  expected = KEYWORD_TEXT[key] + expected
 else:
  if r['type_line'] not in ('Artifact','Enchantment') or p['scope'] not in ('all_creatures','own_noncombat','own_sources','own_attackers','own_combat','own_turn','armor'):
   raise ValueError('ordinary continuous prevention permanent required')
 if p['kind']!='activation' and (p['activation_cost'] or p['tap'] or p['sacrifice']):raise ValueError('costs only supported on activations')
 if p['kind'] not in ('self','aura') and p['keywords']:raise ValueError('unreviewed source keywords')
 expected_keywords = p['keywords'] if creature else ['Enchant'] if p['kind']=='aura' else ['Flashback'] if 'flashback' in p['inactive_clauses'] else ['Populate'] if 'populate' in p['inactive_clauses'] else []
 if sorted(r.get('keywords',[]))!=sorted(expected_keywords):raise ValueError('Oracle keyword list disagrees with reviewed clauses')
 if p['gain']=='attackers':expected += ' You gain 1 life for each attacking creature.'
 if p['gain']=='creatures':expected = 'You gain 1 life for each creature on the battlefield. ' + expected
 if p['gain']=='opposing_creatures':expected = 'You gain 1 life for each creature your opponents control. ' + expected
 if p['gain']!='none' and p['kind']!='spell':raise ValueError('life rider requires a spell')
 for gap in p['inactive_clauses']:expected += GAPS[gap]
 if expected!=r['oracle_text']:raise ValueError('whole Oracle prevention rule differs from declared parameters: '+r['name'])
 if p['inactive_clauses'] and ((p['inactive_clauses']==['populate'] and p['scope']!='you_combat') or (p['inactive_clauses']!=['populate'] and (len(p['inactive_clauses'])!=1 or p['scope']!='fog'))):
  raise ValueError('inactive clauses require their reviewed base rule')
 if creature and (any(not re.fullmatch(r'\d+',r.get(k,'')) or int(r[k])>20 for k in ('power','toughness')) or int(r['toughness'])==0):
  raise ValueError('bounded numeric source stats required')
 activation=[0]*6 if not p['activation_cost'] else cost_from_oracle({'mana_cost':p['activation_cost']})
 return dict(p,family='prevention',mana=cost_from_oracle(r),activation_mana=activation,creature=creature,
             power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0)


def variants(p):
 if p.get('extension')=='replacement':return _replacement_variants(p)
 if p.get('extension')=='modifier':return _modifier_variants(p)
 if p.get('extension')=='entry_shield':return _entry_shield_variants(p)
 if p.get('extension')=='shield':return _shield_variants(p)
 if p['kind']=='self':
  vs=['blocked','noncombat','cleanup','cast_payment','insufficient_cast_mana','source_removed','ready_snapshot','resolved_snapshot','bare_zones']
  if 'Defender' not in p['keywords']:vs+=['attacking']
 elif p['kind']=='aura':
  vs=['blocked','unblocked','noncombat','cleanup','source_removed','cast_payment','insufficient_cast_mana','ready_snapshot','resolved_snapshot','bare_zones']
 else:
  vs=['unblocked','blocked','reverse_combat','reverse_blocked','red_source','trample_source','small_source','human_source',
      'noncombat_player','noncombat_friendly','noncombat_enemy','friendly_damage','opposing_turn_damage',
      'cleanup','cast_payment','insufficient_cast_mana','ready_snapshot','resolved_snapshot','bare_zones']
  if p['kind']=='static':
   # Other static predicates need noncombat or own-attacker transition boards;
   # their matching positive witnesses remain, without irrelevant removals.
   if p['scope'] in ('all_creatures','own_combat'):vs+=['source_removed']
   if p['scope'] not in ('all_creatures','own_combat','own_attackers'):vs.remove('cleanup')
  if p['kind']=='activation':
   vs+=['sick','fresh_activation']
   if p['tap']:vs+=['tapped','payment','insufficient_mana','wrong_color','source_lost_pending']
 return [v+'_'+str(owner) for owner in (0,1) for v in vs]


WITNESSES = {
 'Scaled Wurm': ('{7}{G}','Creature — Wurm','7','6','', ['G']),
 'Hill Giant': ('{3}{R}','Creature — Giant','3','3','', ['R']),
 'Grizzly Bears': ('{1}{G}','Creature — Bear','2','2','', ['G']),
 'Air Elemental': ('{3}{U}{U}','Creature — Elemental','4','4','Flying', ['U']),
 'Craw Wurm': ('{4}{G}{G}','Creature — Wurm','6','4','', ['G']),
 'War Mammoth': ('{3}{G}','Creature — Elephant','3','3','Trample', ['G']),
 'Glory Seeker': ('{1}{W}','Creature — Human Soldier','2','2','', ['W']),
 'Lightning Bolt': ('{R}','Instant',None,None,'Lightning Bolt deals 3 damage to any target.', ['R']),
}


def validate_witnesses(o, rows):
 for nm,fields in WITNESSES.items():
  r=o.get(nm,{})
  if (nm not in rows or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
      or tuple(r.get(k) for k in ('mana_cost','type_line','power','toughness','oracle_text','colors'))!=fields
      or r.get('keywords')!=({'Air Elemental':['Flying'],'War Mammoth':['Trample']}.get(nm,[]))):
   raise ValueError('prevention damage witness changed: '+nm)
 # The source-removal control is a real spell, including its printed targeting.
 if 'Disenchant' not in rows or (o.get('Disenchant',{}).get('mana_cost'),o.get('Disenchant',{}).get('oracle_text'))!=('{1}{W}','Destroy target artifact or enchantment.'):
  raise ValueError('prevention removal witness changed')


def expand(name, csvid, d, p, selected, rows, oracle):
 if p.get('extension')=='replacement':return _replacement_expand(name,csvid,d,p,selected,rows,oracle)
 if p.get('extension')=='modifier':return _modifier_expand(name,csvid,d,p,selected,rows,oracle)
 if p.get('extension')=='entry_shield':return _entry_shield_expand(name,csvid,d,p,selected,rows,oracle)
 if p.get('extension')=='shield':return _shield_expand(name,csvid,d,p,selected,rows,oracle)
 from recipes import card, step, observe, cost_from_oracle
 allowed=variants(p)
 if not selected or len(set(selected))!=len(selected) or any(v not in allowed for v in selected):raise ValueError('explicit nonempty unique reviewed prevention variants required')
 validate_witnesses(oracle,rows)
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones'
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
  setup=[];es={};mana=[[20]*6,[20]*6];life=[20,20]
  pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)]
  acts=[]
  def add(nm,ref,player,zone='battlefield',state=None):
   item=card(nm,ref,player,zone)
   if state:item['state']=state
   setup.append(item);r=oracle[nm];creature='Creature' in r['type_line']
   es[ref]=dict(name=nm,player=player,alive=zone=='battlefield',creature=creature,tapped=bool(state and 'tapped' in state),
                power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,damage=0)
   if zone=='hand':hands[player].append(nm)
  def pay(pl,cost):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  def move(ref,zone='graveyard'):
   e=es[ref];e['alive']=False;(hands if zone=='hand' else graves)[e['player']].append(e['name'])
  def cast(ref,target=None,player_target=None):
   e=es[ref];pl=e['player'];seq=[]
   if target:seq.append(step('choose_target',player=pl,ref=target))
   if player_target is not None:seq.append(step('choose_player',player=pl,value=player_target))
   seq += [step('cast',ref=ref,value=1),step('resolve'),step('assert_choices_consumed')]
   pay(pl,cost_from_oracle(oracle[e['name']]));hands[pl].remove(e['name'])
   if oracle[e['name']]['type_line'] in ('Instant','Sorcery'):graves[pl].append(e['name'])
   else:e['alive']=True
   return seq
  def inspect(ref):
   e=es[ref];s=[observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    s.append(observe(ref,'tapped',int(e['tapped'])))
    if e['creature']:s += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness']),observe(ref,'damage',e['damage'])]
   return s
  def hit(ref,n):
   e=es[ref]
   if not e['alive']:return
   e['damage']+=n
   if e['damage']>=e['toughness']:move(ref)
  def gained(attacking=0):
   if p['gain']=='attackers':return attacking
   if p['gain']=='creatures':return sum(e['alive'] and e['creature'] for e in es.values())
   if p['gain']=='opposing_creatures':return sum(e['alive'] and e['creature'] and e['player']!=owner for e in es.values())
   return 0
  kind=p['kind'];scope=p['scope'];self_card=kind=='self';aura=kind=='aura';activation=kind=='activation'
  casting=v in ('cast_payment','insufficient_cast_mana','fresh_activation') or kind in ('spell','aura')
  add(name,'source',owner,'hand' if casting else 'battlefield',['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
  noncombat=v in ('noncombat','noncombat_player','noncombat_friendly','noncombat_enemy','friendly_damage','opposing_turn_damage')
  attacker_owner=owner if v in ('reverse_combat','reverse_blocked') or v=='cleanup' and scope=='own_attackers' else 1-owner
  attack_name={'red_source':'Hill Giant','small_source':'Grizzly Bears','trample_source':'War Mammoth','human_source':'Glory Seeker'}.get(v,'Scaled Wurm')
  if v=='cleanup':attack_name={'nongreen':'Hill Giant','small':'Grizzly Bears'}.get(scope,attack_name)
  if aura:
   add('Scaled Wurm','attacker',1-owner)
   add('Air Elemental' if p['keywords'] else 'Scaled Wurm','blocker',owner)
  elif self_card:
   # Flying defenders are challenged by a flying attacker, so a rejected block
   # never masquerades as a successful prevention result.
   add('Air Elemental' if 'Flying' in p['keywords'] else 'Scaled Wurm','attacker',1-owner)
   add('Air Elemental' if 'Flying' in p['keywords'] else 'Scaled Wurm','blocker',1-owner)
  else:
   add(attack_name,'attacker',attacker_owner)
   add('Scaled Wurm','blocker',1-attacker_owner)
  # A separate, unselected creature/player always remains as a scope control.
  add('Craw Wurm','control',owner)
  if noncombat:
   target='source' if self_card else 'attacker' if aura else 'blocker' if v in ('noncombat_friendly','friendly_damage') else 'attacker'
   if not self_card and not aura and v=='noncombat_friendly' and es[target]['player']!=owner:target='control'
   bolt_owner=owner if v=='friendly_damage' else 1-owner
   add('Lightning Bolt','damage_spell',bolt_owner,'hand')
  if v=='source_removed' and not self_card and not activation:add('Disenchant','removal',owner,'hand')
  if casting:
   if v in ('cast_payment','insufficient_cast_mana'):
    mana[owner]=list(p['mana']) if v=='cast_payment' else [0]*6
    pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   if v=='insufficient_cast_mana':pre += [step('cast',ref='source',value=0),step('assert_stack',value=0),step('assert_choices_consumed')]
   elif kind!='spell':
    pre += cast('source','attacker' if aura else None)+[step('refresh')]
    if self_card or activation:pre += [observe('source','sick',1)]
  enabled=v!='insufficient_cast_mana'
  if activation and p['tap'] and v in ('sick','fresh_activation','cast_payment','tapped'):enabled=False
  def activate():
   seq=[step('require_can_activate',ref='source',value=int(enabled)),step('activate',ref='source',value=int(enabled))]
   if enabled:
    pay(owner,p['activation_mana'])
    if p['tap']:es['source']['tapped']=True
    if p['sacrifice']:move('source')
    seq += [step('assert_stack',value=1)]+inspect('source')
    if v=='source_lost_pending':seq += [step('destroy',ref='source')];move('source')
    seq += [step('resolve'),step('assert_choices_consumed')]
   return seq
  current_turn=owner
  def reduced(amount,src,recipient,combat,attacking_ref=None,enabled_rule=True):
   if not enabled_rule:return amount
   se=es[src];rp=recipient if type(recipient) is int else es[recipient]['player'];creature=type(recipient) is str
   own_creature=creature and rp==owner
   src_oracle=oracle[se['name']]
   prevent=(scope=='fog' and combat or scope=='players_combat' and combat and not creature
       or scope=='creatures_players' and se['creature'] and not creature
       or scope in ('own_creatures','own_noncombat') and own_creature and (scope!='own_noncombat' or not combat)
       or scope in ('creatures','all_creatures') and creature
       or scope=='own_team' and rp==owner or scope=='you' and not creature and rp==owner
       or scope=='you_combat' and combat and not creature and rp==owner
       or scope=='nongreen' and combat and 'G' not in src_oracle['colors']
       or scope=='nontrample' and combat and 'Trample' not in src_oracle['keywords']
       or scope=='small' and combat and se['power']<=4
       or scope=='nonhuman' and 'Human' not in src_oracle['type_line']
       or scope=='own_sources' and own_creature and se['player']==owner
       or scope=='own_attackers' and combat and own_creature and recipient==attacking_ref
       or scope=='own_combat' and combat and (se['player']==owner or own_creature)
       or scope=='own_turn' and not creature and rp==owner and current_turn==owner
       or scope=='self_all' and recipient=='source'
       or scope in ('self_combat','self_both') and combat and recipient=='source'
       or scope=='self_both' and combat and src=='source'
       or scope=='self_blockers' and combat and recipient=='source' and attacking_ref=='source'
       or scope=='self_creatures' and recipient=='source' and se['creature']
       or scope=='aura_all' and src=='attacker'
       or scope=='aura_both' and combat and (src=='attacker' or recipient=='attacker'))
   if prevent:return 0
   if scope=='armor' and not creature and rp==owner:return max(0,amount-1)
   return amount
  if v=='insufficient_cast_mana':pass
  elif noncombat:
   current_turn=1-owner if v=='opposing_turn_damage' else owner
   acts += [step('turn',player=current_turn,value=0)]
   if kind=='spell':acts += cast('source');life[owner]+=gained()
   if activation:acts += activate()
   player_target=v in ('noncombat_player','opposing_turn_damage')
   recipient=owner if player_target else target
   acts += cast('damage_spell',None if player_target else target,owner if player_target else None)
   amount=reduced(3,'damage_spell',recipient,False,enabled_rule=enabled)
   if player_target:life[owner]-=amount
   else:hit(target,amount)
  else:
   current_turn=attacker_owner
   attacking='source' if self_card and v=='attacking' else 'attacker'
   defending='source' if self_card and v!='attacking' else 'blocker'
   current_turn=es[attacking]['player']
   if v=='cleanup':
    if kind=='spell':pre += cast('source');life[owner]+=gained(0)
    if activation:pre += activate()
    pre += [step('cleanup'),step('refresh'),step('turn',player=current_turn,value=0)]
    if kind in ('spell','activation'):enabled=False
   blocked=v not in ('unblocked','reverse_combat','red_source','trample_source','small_source','human_source','cleanup','cast_payment','ready_snapshot','resolved_snapshot','bare_zones','payment','insufficient_mana','wrong_color','sick','fresh_activation','tapped','source_lost_pending') or self_card
   if v=='cleanup' and scope in ('own_creatures','creatures','all_creatures','own_combat','own_attackers'):blocked=True
   acts += [step('turn',player=current_turn,value=0),step('begin_attackers'),step('attack',ref=attacking,value=1),step('finish_attackers')]
   es[attacking]['tapped']=True
   # Sorceries must have been cast during the main phase, before attackers.
   if kind=='spell' and v!='cleanup' and oracle[name]['type_line']=='Sorcery':
    pre += cast('source');life[owner]+=gained(0)
   elif kind=='spell' and v!='cleanup':acts += cast('source');life[owner]+=gained(1)
   if activation and v!='cleanup':
    if v in ('payment','insufficient_mana','wrong_color'):
     pool=list(p['activation_mana']) if v=='payment' else [0]*6
     if v=='wrong_color':pool[0]=sum(p['activation_mana'])
     mana[owner]=pool;acts += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(pool)]
     if v!='payment':enabled=False
    acts += activate()
   if v=='source_removed':
    if self_card:acts += [step('destroy',ref='source')];move('source');enabled=False
    else:acts += cast('removal','source');move('source');enabled=False
   acts += [step('begin_blockers')]
   if blocked and es[defending]['alive']:acts += [step('block',ref=defending,other=attacking,value=1)]
   acts += [step('advance_combat')]
   # Inspect the resolved prevention state before damage; snapshot variants
   # replay the actual damage, including creature deaths and stack decisions.
   ready_cut=len(acts)
   acts += [step('combat_damage'),step('resolve'),step('end_combat'),step('assert_choices_consumed')]
   if blocked and es[defending]['alive']:
    attack_damage=reduced(es[attacking]['power'],attacking,defending,True,attacking,enabled)
    block_damage=reduced(es[defending]['power'],defending,attacking,True,attacking,enabled)
    hit(defending,attack_damage);hit(attacking,block_damage)
   else:life[1-current_turn]-=reduced(es[attacking]['power'],attacking,1-current_turn,True,attacking,enabled)
  # If an Aura's creature died, the Aura follows it to its owner's graveyard.
  if aura and es['source']['alive'] and not es['attacker']['alive']:move('source')
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref in es:checks += inspect(ref)
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in es.values()}
  for pl in (0,1):
   battlefield=[e['name'] for e in es.values() if e['alive'] and e['player']==pl]
   checks += [step('assert_life',player=pl,value=life[pl])]+[step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for zone,names in (('battlefield',battlefield),('hand',hands[pl]),('graveyard',graves[pl]),('exile',exiles[pl]),('library',libs[pl])):
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(names)))
    if zone=='library':checks += [step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  steps=pre+acts+checks
  if v=='ready_snapshot':
   tail=acts[ready_cut:]+checks
   steps=pre+acts[:ready_cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  if v=='cleanup':
   # First prove the rule prevents actual damage on this exact board. Restore
   # that initial board, then run the cleanup transition and its damage probe.
   positive={'nongreen':'red_source','small':'small_source','own_attackers':'reverse_blocked'}.get(scope,
     'blocked' if self_card or scope in ('own_creatures','creatures','all_creatures','own_combat') else 'unblocked')
   witness=expand(name,csvid,d,p,[positive+'_'+str(owner)],rows,oracle)[0]
   if witness['setup']!=setup:raise ValueError('cleanup positive witness must use the identical initial board')
   steps=[step('capture')]+witness['steps']+[step('restore')]+steps
  result.append(dict(name=f'PreventionRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='prevention',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=BASE_GAPS+p['inactive_clauses'])))
 return result


def _shield_pins():
 return json.loads((Path(__file__).parent/'data/prevention_extension_witnesses.json').read_text(encoding='utf8'))


def _shield_spec(d,r):
 from recipes import cost_from_oracle
 if (not r or d.get('version')!=2 or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
     or r.get('type_line','').startswith('Token ') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('shield extension requires whole normal Oracle source and explicit version2 variants')
 profile=_shield_pins()['sources'].get(r.get('name'))
 if not profile or any(r.get(k)!=v for k,v in profile['oracle'].items()):raise ValueError('unreviewed whole shield source record')
 p=d.get('params')
 if p!=profile['params'] or not isinstance(p,dict):raise ValueError('shield component, UI bindings and gaps must match reviewed profile')
 if (p['extension']!='shield' or p['binding'] not in ('packet','recipient','combat_source','packet_combat','field_combat','chosen_combat','legacy_field','recipient_activation') or p['scope'] not in ('any','creature','you','source')
     or not (type(p['amount']) is int and 1<=p['amount']<=12 or p['amount'] in ('all','half_down','all_but_one'))
     or type(p['tap']) is not bool or not isinstance(p['activation_cost'],str)
     or not isinstance(p['gaps'],list) or not isinstance(p['dialogs'],dict)):
  raise ValueError('typed explicit shield parameters required')
 if (any(type(p.get(k,False)) is not bool for k in ('sacrifice','gain_prevented','present_dialogs','exile_prevented','gain_power','prevent_incoming','cast_permission_only','return_source','optional_second_target','red_spent_damage','tap_cost_source','repeat_untap'))
     or type(p.get('pay_life',0)) is not int or not 0<=p.get('pay_life',0)<=5):raise ValueError('typed shield collateral costs and gains required')
 if type(p.get('target_repetitions',1)) is not int or not 1<=p.get('target_repetitions',1)<=2:raise ValueError('bounded explicit source target UI repetitions required')
 creature='Creature' in r['type_line'];spell=r['type_line'].startswith('Instant')
 if not spell and not creature and r['type_line'] not in ('Artifact','Enchantment') and not (p['binding']=='legacy_field' and r['type_line']=='Enchantment — Aura'):raise ValueError('ordinary shield spell or permanent required')
 return dict(copy.deepcopy(p),family='prevention',spell=spell,creature=creature,mana=[0]*6 if r['mana_cost']=='{0}' else cost_from_oracle(r),
   activation_mana=cost_from_oracle(dict(mana_cost=p['activation_cost'])) if p['activation_cost'] else [0]*6,
   power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0)


def _shield_variants(p):
 if p['binding']=='legacy_field':return _legacy_profile_variants(p)
 if p['binding'] in ('packet_combat','field_combat','chosen_combat'):return _combat_extension_variants(p)
 if p['binding']=='combat_source':
  vs=['effect','blocked','without_effect','payment','cast_payment','insufficient_cast_mana','cleanup',
      'ready_snapshot','pending_snapshot','resolved_snapshot','bare_zones']
  if not p['spell']:
   if sum(p['activation_mana']):vs+=['activation_insufficient']
   if not p.get('sacrifice'):vs+=['source_lost_pending']
   if any(p['activation_mana'][1:]):vs+=['activation_wrong_color']
   if p['tap']:vs+=['tapped']
   if p['tap'] and p['creature']:vs+=['sick','fresh_cast']
   elif p['creature']:vs+=['fresh_cast']
  if p.get('target_predicate')=='attacking':vs+=['wrong_role']
  return [v+'_'+str(owner) for owner in (0,1) for v in vs]
 vs=['effect','payment','other_damage','repeat_damage','ready_snapshot','resolved_snapshot','bare_zones','cast_payment']
 if sum(p['mana']):vs+=['insufficient_cast_mana']
 if p['scope']=='any':vs+=['self_player','opponent_player','opponent_creature']
 elif p['scope']=='creature':vs+=['opponent_creature']
 if p['binding'] in ('recipient','recipient_activation') and (p['amount']=='all' or p['amount']>3):vs+=['cleanup','overflow_damage']
 if p.get('pay_life') or p.get('gain_prevented') or p.get('exile_prevented') or p['amount']=='half_down':vs+=['small_damage']
 if p.get('exile_prevented'):vs+=['short_library']
 if p['amount']=='half_down':vs+=['large_damage']
 if p.get('buyback_cost'):vs+=['buyback','buyback_payment','buyback_snapshot']
 if p.get('tap_cost_source'):vs+=['fresh_tap_cost']
 if p.get('alternative_cost'):vs+=['alternative_payment','alternative_snapshot']
 if p.get('repeat_untap'):vs+=['untap_repeat','untap_repeat_snapshot']
 if p['binding']=='recipient_activation':vs+=['tapped','sick','fresh_cast','without_effect']
 # Denied activation cannot be observed inside a live damage callback with
 # the current script API. Passing priority is not a legality assertion.
 if not p['spell'] and not p.get('sacrifice'):vs+=['source_removed']
 return [v+'_'+str(owner) for owner in (0,1) for v in vs]


def _shield_validate(o,rows):
 validate_witnesses(o,rows)
 for name,pin in _shield_pins()['witnesses'].items():
  if name not in rows or o.get(name,{}).get('card_faces') or o.get(name,{}).get('selected_face') or any(o.get(name,{}).get(k)!=v for k,v in pin.items()):
   raise ValueError('whole prevention shield witness changed: '+name)


def _shield_expand(name,csvid,d,p,selected,rows,o):
 if p['binding']=='legacy_field':return _legacy_profile_expand(name,csvid,d,p,selected,rows,o)
 if p['binding'] in ('packet_combat','field_combat','chosen_combat'):return _combat_extension_expand(name,csvid,d,p,selected,rows,o)
 if p['binding']=='combat_source':return _source_combat_expand(name,csvid,d,p,selected,rows,o)
 from recipes import card,step,observe,cost_from_oracle
 if not selected or len(set(selected))!=len(selected) or any(v not in _shield_variants(p) for v in selected):raise ValueError('unique nonempty reviewed shield variants required')
 _shield_validate(o,rows)
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v in ('bare_zones','short_library')
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
  setup=[];entities={};mana=[[20]*6,[20]*6];life=[20,20]
  if v=='short_library':
   libs[owner]=[LIBRARY[0]];setup += [card(LIBRARY[0],'library_short',owner,'library')]
  pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)]
  if p.get('present_dialogs'):pre += [step('present_ai_dialogs',value=1)]
  actions=[]
  def add(nm,ref,pl,zone='battlefield',state=None):
   item=card(nm,ref,pl,zone)
   if state:item['state']=state
   setup.append(item);r=o[nm];creature='Creature' in r['type_line']
   entities[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',creature=creature,tapped=bool(state and 'tapped' in state),
    power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,damage=0)
   if zone=='hand':hands[pl].append(nm)
  def pay(pl,cost):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  def move(ref,zone):
   e=entities[ref];e['alive']=False;(graves if zone=='graveyard' else hands)[e['player']].append(e['name'])
  def cast(ref,target=None,pl_target=None):
   e=entities[ref];pl=e['player'];seq=[]
   if target:seq += [step('choose_target',player=pl,ref=target)]
   if pl_target is not None:seq += [step('choose_player',player=pl,value=pl_target)]
   seq += [step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
   pay(pl,p['mana'] if ref=='source' else cost_from_oracle(o[e['name']]));hands[pl].remove(e['name'])
   if o[e['name']]['type_line']=='Instant':graves[pl].append(e['name'])
   else:e['alive']=True
   return seq
  casting=p['spell'] or v in ('cast_payment','insufficient_cast_mana','fresh_cast','fresh_tap_cost')
  add(name,'source',owner,'hand' if casting else 'battlefield',['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
  if p.get('hand_witness'):add(p['hand_witness'],'hand_control',owner,'hand')
  target_player=p['scope']=='you' or v in ('self_player','opponent_player')
  target_owner=1-owner if v in ('opponent_player','opponent_creature') else owner
  add('Scaled Wurm','recipient',target_owner)
  add('Craw Wurm','control',1-target_owner)
  if v not in ('cast_payment','insufficient_cast_mana') or p['spell']:
   add('Shock' if v=='small_damage' else 'Lava Axe' if v=='large_damage' else 'Lightning Bolt','bolt',1-owner,'hand')
   if v=='large_damage':pre += [step('turn',player=1-owner,value=0)]
   if v in ('repeat_damage','other_damage','cleanup','source_removed','overflow_damage','untap_repeat','untap_repeat_snapshot'):add('Lightning Bolt','second_bolt',1-owner,'hand')
   if v=='overflow_damage':add('Lightning Bolt','third_bolt',1-owner,'hand')
  if v=='source_removed':add('Unsummon' if p['creature'] else 'Disenchant','removal',owner,'hand')
  if v in ('cast_payment','insufficient_cast_mana'):
   pool=list(p['mana']) if v=='cast_payment' else [0]*6;mana[owner]=pool
   pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(pool)]
  elif v in ('payment','buyback_payment','alternative_payment','alternative_snapshot'):
   pool=cost_from_oracle(dict(mana_cost=p['alternative_cost'])) if v in ('alternative_payment','alternative_snapshot') else list(p['mana'] if p['spell'] else p['activation_mana'])
   if v=='buyback_payment':pool=[a+b for a,b in zip(pool,cost_from_oracle(dict(mana_cost=p['buyback_cost'])))]
   mana[owner]=pool
   pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(pool)]
  enabled=v not in ('tapped','sick','fresh_cast','insufficient_cast_mana','without_effect')
  if v=='insufficient_cast_mana':actions += [step('cast',ref='source',value=0),step('assert_stack',value=0)]
  elif casting and not p['spell']:
   actions += cast('source')+[step('refresh')]
   if p['creature']:actions += [observe('source','sick',1)]
  remaining=p['amount']
  def hit(ref,amount):
   e=entities[ref];e['damage']+=amount
   if e['damage']>=e['toughness']:move(ref,'graveyard')
  def shield_cast():
   if p['binding']=='recipient_activation':
    seq=[step('require_can_activate',ref='source',value=1)]+[step('choose_dialog',player=owner,value=n) for n in p['dialogs'][str(owner)]]
    seq += [step('choose_target',player=owner,ref='recipient'),step('activate',ref='source',value=1),step('assert_stack',value=1),observe('recipient','damage',0),step('resolve'),step('assert_choices_consumed')]
    pay(owner,p['activation_mana']);entities['source']['tapped']=p['tap']
    return seq+[observe('source','tapped',int(entities['source']['tapped']))]
   seq=[]
   if target_player:seq += [step('choose_player',player=owner,value=target_owner)]
   else:seq += [step('choose_target',player=owner,ref='recipient')]
   seq += [step('choose_dialog',player=owner,value=n) for n in p['dialogs'][str(owner)]]
   seq += [step('cast',ref='source',value=1),step('assert_stack',value=1),observe('recipient','damage',0),
      step('assert_life',player=target_owner,value=20),step('resolve'),step('assert_choices_consumed')]
   pay(owner,p['mana']);hands[owner].remove(name);graves[owner].append(name)
   return seq
  def bolt(ref='bolt',protect=True,other=False):
   nonlocal remaining
   damage_name=entities[ref]['name'];damage_amount={'Shock':2,'Lightning Bolt':3,'Lava Axe':5}[damage_name]
   recipient='control' if other else 'recipient';pl=1-target_owner if other else target_owner
   seq=([step('choose_player',player=1-owner,value=pl)] if target_player else [step('choose_target',player=1-owner,ref=recipient)])
   seq += [step('cast',ref=ref,value=1),step('assert_stack',value=1)]
   pay(1-owner,cost_from_oracle(o[damage_name]));hands[1-owner].remove(damage_name);graves[1-owner].append(damage_name)
   if protect and enabled and p['binding']=='packet':
    def affordable(nm):
     if nm==name or o[nm]['type_line']!='Instant':return False
     cost=cost_from_oracle(o[nm]);pool=mana[owner]
     return all(pool[c]>=cost[c] for c in range(1,6)) and sum(pool)-sum(cost[1:])>=cost[0]
    # Human fast-effects priority occurs before the spell creates damage if
    # another affordable hand instant exists. Explicitly pass that earlier
    # window, then select prevention only in the real damage window.
    option=p.get('normal_option_cost')
    option_cost=cost_from_oracle(dict(mana_cost=option)) if option else None
    option_available=bool(option_cost and all(mana[owner][c]>=option_cost[c] for c in range(1,6)) and sum(mana[owner])-sum(option_cost[1:])>=option_cost[0])
    if owner==0 and (any(affordable(nm) for nm in hands[owner]) or option_available):seq += [step('choose_priority',player=owner,value=-2)]
    seq += [step('choose_priority',ref='source')]
    dialogs=p['dialogs'][str(owner)]
    if p.get('alternative_cost'):
     alt=cost_from_oracle(dict(mana_cost=p['alternative_cost']));base=p['activation_mana'];pool=mana[owner]
     affordable=lambda cost:all(pool[c]>=cost[c] for c in range(1,6)) and sum(pool)-sum(cost[1:])>=cost[0]
     if not (affordable(alt) and affordable(base)):dialogs=[]
    seq += [step('choose_dialog',player=owner,value=n) for n in dialogs]
    if p.get('tap_cost_source'):seq += [step('choose_target',player=owner,ref='source')]
    seq += [step('choose_damage_player',player=owner,value=pl)] if target_player else [step('choose_damage_target',player=owner,ref=recipient)]
    bought_back=False
    if p.get('buyback_cost'):
     extra=cost_from_oracle(dict(mana_cost=p['buyback_cost']));base=p['mana'];pool=mana[owner]
     affordable_extra=all(pool[c]>=base[c]+extra[c] for c in range(1,6)) and sum(pool)-sum(base)-sum(extra[1:])>=extra[0]
     bought_back=v in ('buyback','buyback_payment','buyback_snapshot')
     if bought_back or affordable_extra:seq += [step('choose_dialog',player=owner,value=0 if bought_back else 1)]
     if bought_back:pay(owner,extra)
    pay(owner,cost_from_oracle(dict(mana_cost=p['alternative_cost'])) if v in ('alternative_payment','alternative_snapshot') else p['mana'] if p['spell'] else p['activation_mana'])
    life[owner]-=p.get('pay_life',0)
    if p['spell']:
     if not bought_back:hands[owner].remove(name);graves[owner].append(name)
    else:
     if p['tap']:entities['source']['tapped']=True
     if p.get('sacrifice'):move('source','graveyard')
   seq += [step('resolve'),step('assert_choices_consumed')]
   prevented=0
   if protect and enabled and not other:
    prevented=damage_amount if remaining=='all' else damage_amount//2 if remaining=='half_down' else min(damage_amount,remaining)
    if type(remaining) is int:remaining-=prevented
   if p.get('exile_prevented'):
    count=min(prevented,len(libs[owner]));exiles[owner].extend(libs[owner][:count]);del libs[owner][:count]
   if p.get('gain_prevented'):life[owner]+=prevented
   amount=damage_amount-prevented
   if target_player:life[pl]-=amount
   else:hit(recipient,amount)
   seq += [step('assert_life',player=pl,value=life[pl])] if target_player else [observe(recipient,'in_play',int(entities[recipient]['alive']))]
   if not target_player and entities[recipient]['alive']:seq += [observe(recipient,'damage',entities[recipient]['damage'])]
   return seq
  cast_only=v in ('cast_payment','insufficient_cast_mana') and not p['spell']
  if v!='insufficient_cast_mana' and not cast_only:
   if p['binding']=='recipient_activation' and not enabled and v!='without_effect':actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_stack',value=0)]
   if p['binding'] in ('recipient','recipient_activation') and enabled:actions += shield_cast()
   actions += bolt()
   if v in ('untap_repeat','untap_repeat_snapshot'):
    # Preserve the first actual effect/cost before the real turn transition clears damage and mana.
    actions += [observe('source','tapped',1),step('assert_stack',value=0)]+[step('assert_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
    actions += [step('set_mana',player=pl,color=c,value=0) for pl in (0,1) for c in range(6)]
    for pl in (1-owner,owner):actions += [step('advance_turn'),step('untap_step',player=pl)]
    actions += [observe('source','tapped',0),step('turn',player=owner,value=0)]
    entities['source']['tapped']=False
    for e in entities.values():e['damage']=0
    mana=[[20]*6,[20]*6]
    actions += [step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)]
    remaining=p['amount'];actions += bolt('second_bolt')+[observe('source','tapped',1)]
   if v=='source_removed':
    actions += cast('removal','source');move('source','hand' if p['creature'] else 'graveyard');enabled=p['binding']=='recipient_activation'
    actions += bolt('second_bolt',protect=p['binding']=='recipient_activation')
   if v in ('repeat_damage','other_damage','cleanup','overflow_damage'):
    if v=='cleanup':
     actions += [step('cleanup'),step('refresh')];remaining=0
     for e in entities.values():e['damage']=0
    actions += bolt('second_bolt',protect=p['binding'] in ('recipient','recipient_activation'),other=v=='other_damage')
    if v=='overflow_damage':actions += bolt('third_bolt',protect=True)
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref,e in entities.items():
   checks += [observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    checks += [observe(ref,'tapped',int(e['tapped']))]
    if e['creature']:checks += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness']),observe(ref,'damage',e['damage'])]
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
  for pl in (0,1):
   battlefield=[e['name'] for e in entities.values() if e['alive'] and e['player']==pl]
   checks += [step('assert_life',player=pl,value=life[pl])]+[step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for zone,names in (('battlefield',battlefield),('hand',hands[pl]),('graveyard',graves[pl]),('library',libs[pl]),('exile',exiles[pl])):
    checks += [step('assert_zone',player=pl,zone=zone,value=len(names))]
    if zone=='library':checks += [step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  steps=pre+actions+checks
  if v in ('ready_snapshot','buyback_snapshot','alternative_snapshot','untap_repeat_snapshot'):steps=pre+[step('capture')]+copy.deepcopy(actions+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions+checks)+[step('assert_recorded_state')]
  if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'PreventionShieldRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='prevention',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps']+
      ['planeswalker targets','damage redirection','unpreventable damage','control changes','multiplayer',
       'activation affordability query during the live damage window','tap and summoning-sickness denial during the live damage window'])))
 return result


def _source_combat_expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 if not selected or len(set(selected))!=len(selected) or any(v not in _shield_variants(p) for v in selected):raise ValueError('reviewed source-combat variants required')
 _shield_validate(o,rows);result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';attacker_owner=1-owner
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
  setup=[];es={};mana=[[20]*6,[20]*6];life=[20,20]
  pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)]
  actions=[];pending_cut=0
  def add(nm,ref,pl,zone='battlefield',state=None):
   item=card(nm,ref,pl,zone)
   if state:item['state']=state
   setup.append(item);r=o[nm];creature='Creature' in r['type_line']
   es[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',creature=creature,tapped=bool(state and 'tapped' in state),
    power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,damage=0)
   if zone=='hand':hands[pl].append(nm)
  def pay(pl,cost):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  def died(ref):
   e=es[ref];e['alive']=False;graves[e['player']].append(e['name'])
  def hit(ref,amount):
   es[ref]['damage']+=amount
   if es[ref]['damage']>=es[ref]['toughness']:died(ref)
  def inspect(ref):
   e=es[ref];seq=[observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    seq += [observe(ref,'tapped',int(e['tapped']))]
    if e['creature']:seq += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness']),observe(ref,'damage',e['damage'])]
   return seq
  casting=p['spell'] or v in ('cast_payment','insufficient_cast_mana','fresh_cast')
  add(name,'source',owner,'hand' if casting else 'battlefield',['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
  add('Scaled Wurm','attacker',attacker_owner);add('Hill Giant','unselected',attacker_owner);add('Scaled Wurm','blocker',owner)
  if v=='cleanup':add('Vitalize','untap',attacker_owner,'hand')
  if v in ('cast_payment','insufficient_cast_mana'):
   mana[owner]=list(p['mana']) if v=='cast_payment' else [0]*6
   pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  if casting and not p['spell'] and v!='insufficient_cast_mana':
   pre += [step('cast',ref='source',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed'),step('refresh')]
   pay(owner,p['mana']);hands[owner].remove(name);es['source']['alive']=True
   if p['creature']:pre += [observe('source','sick',1)]
  cast_only=v=='cast_payment' and not p['spell']
  denied=v in ('tapped','sick','activation_insufficient','activation_wrong_color') or v=='fresh_cast' and p['tap']
  enabled=v not in ('without_effect','insufficient_cast_mana','wrong_role') and not cast_only and not denied
  def declare(blocked=False):
   seq=[step('turn',player=attacker_owner,value=0),step('begin_attackers'),step('attack',ref='attacker',value=1),
        step('attack',ref='unselected',value=1),step('finish_attackers'),step('begin_blockers')]
   es['attacker']['tapped']=es['unselected']['tapped']=True
   if blocked:seq += [step('block',ref='blocker',other='attacker',value=1)]
   return seq+[step('advance_combat')]
  def combat(protect=True,blocked=False):
   seq=[step('combat_damage'),step('resolve'),step('end_combat'),step('assert_choices_consumed')]
   if blocked:
    if not protect:hit('blocker',7)
    hit('attacker',7)
   elif not protect:life[owner]-=7
   life[owner]-=3
   return seq+[step('assert_life',player=owner,value=life[owner])]+inspect('attacker')+inspect('blocker')
  if v=='insufficient_cast_mana':actions += [step('cast',ref='source',value=0),step('assert_stack',value=0)]
  elif v=='wrong_role':actions += [step('can_cast',ref='source',value=0),step('cast',ref='source',value=0),step('assert_choices_consumed')]
  elif not cast_only:
   blocked=v=='blocked';actions += declare(blocked)
   if v in ('payment','activation_insufficient','activation_wrong_color'):
    pool=list(p['mana'] if p['spell'] else p['activation_mana']) if v=='payment' else [0]*6
    if v=='activation_wrong_color':pool[0]=sum(p['activation_mana'])
    mana[owner]=pool;actions += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(pool)]
   if denied:actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_stack',value=0)]
   elif enabled:
    if not p['spell']:actions += [step('require_can_activate',ref='source',value=1)]
    actions += [step('choose_target',player=owner,ref='attacker')]+[step('choose_dialog',player=owner,value=n) for n in p['dialogs'][str(owner)]]
    actions += [step('cast' if p['spell'] else 'activate',ref='source',value=1),step('assert_stack',value=1),step('assert_choices_consumed')]
    pay(owner,p['mana'] if p['spell'] else p['activation_mana'])
    if p['tap']:es['source']['tapped']=True
    if p.get('sacrifice'):died('source')
    if p['spell']:hands[owner].remove(name);graves[owner].append(name)
    actions += inspect('source');pending_cut=len(actions)
    if v=='source_lost_pending':actions += [step('destroy',ref='source')];died('source')
    actions += [step('resolve'),step('assert_choices_consumed')]
   actions += combat(enabled,blocked)
   if v=='cleanup':
    actions += [step('cleanup'),step('refresh'),step('turn',player=attacker_owner,value=0),step('cast',ref='untap',value=1),step('resolve'),step('assert_choices_consumed')]
    pay(attacker_owner,cost_from_oracle(o['Vitalize']));hands[attacker_owner].remove('Vitalize');graves[attacker_owner].append('Vitalize')
    for ref,e in es.items():
     e['damage']=0
     if e['alive'] and e['player']==attacker_owner and e['creature']:e['tapped']=False
    actions += declare()+combat(False)
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref in es:checks += inspect(ref)
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in es.values()}
  for pl in (0,1):
   battlefield=[e['name'] for e in es.values() if e['alive'] and e['player']==pl]
   checks += [step('assert_life',player=pl,value=life[pl])]+[step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for zone,names in (('battlefield',battlefield),('hand',hands[pl]),('graveyard',graves[pl]),('library',libs[pl]),('exile',exiles[pl])):
    checks += [step('assert_zone',player=pl,zone=zone,value=len(names))]
    if zone=='library':checks += [step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=0 if v=='ready_snapshot' else pending_cut;tail=actions[cut:]+checks
   steps=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'PreventionSourceCombatRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='prevention',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps']+
      ['selected creature noncombat damage','multiple blockers','damage redirection','unpreventable damage','control changes','multiplayer'])))
 return result


def _combat_extension_variants(p):
 if p.get('cast_permission_only'):return ['cast_permission_0','cast_permission_1']
 vs=['effect','without_effect','payment','cast_payment','insufficient_cast_mana','cleanup','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot']
 if p['binding']!='packet_combat':vs+=['blocked']
 else:vs.remove('pending_snapshot')
 if p.get('gain_power') or p.get('gain_prevented'):vs+=['growth']
 if p.get('optional_second_target'):vs+=['two_targets']
 if p.get('red_spent_damage'):vs+=['red_spent']
 if p.get('damage_rule')=='ferocious':vs+=['no_ferocious','lose_ferocious']
 if p.get('target_predicate')=='unblocked':
  vs.remove('blocked');vs+=['wrong_role']
 if p.get('prevent_incoming'):vs+=['split_targets']
 if p.get('amount')=='all_but_one':vs+=['one_damage']
 if p.get('alternate_attacker'):vs+=['alternate_source']
 if p.get('target_predicate')=='attacking_flying':vs+=['wrong_role','wrong_ability']
 if not p['spell'] and p['binding']!='packet_combat':
  if sum(p['activation_mana']):vs+=['activation_insufficient']
  if any(p['activation_mana'][1:]):vs+=['activation_wrong_color']
  if p['tap']:vs+=['tapped']
  if p['creature'] and p['tap']:vs+=['sick','fresh_cast']
  elif p['creature']:vs+=['fresh_cast']
  if not p.get('return_source'):vs+=['source_lost_pending']
 return [v+'_'+str(owner) for owner in (0,1) for v in vs]


def _combat_extension_expand(name,csvid,d,p,selected,rows,o):
 import copy
 from collections import Counter
 from recipes import card,step,observe,cost_from_oracle
 from grant_recipes import HAND,GRAVE,EXILE,LIBRARY
 if not selected or len(set(selected))!=len(selected) or any(v not in _combat_extension_variants(p) for v in selected):raise ValueError('explicit reviewed extended combat variants required')
 _shield_validate(o,rows);result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner;bare=v=='bare_zones';binding=p['binding']
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
  setup=[];es={};mana=[[20]*6,[20]*6];life=[20,20]
  pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)]
  actions=[];pending_cut=0;ferocious_state=False
  if p.get('red_spent_damage'):
   mana[owner][4]=1 if v=='red_spent' else 0;pre += [step('set_mana',player=owner,color=4,value=mana[owner][4])]
  def add(nm,ref,pl,zone='battlefield',state=None):
   item=card(nm,ref,pl,zone)
   if state:item['state']=state
   setup.append(item);r=o[nm];creature='Creature' in r['type_line']
   es[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',creature=creature,tapped=bool(state and 'tapped' in state),
    power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,damage=0)
   if zone=='hand':hands[pl].append(nm)
  def pay(pl,cost):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  def died(ref):
   e=es[ref];e['alive']=False;graves[e['player']].append(e['name'])
  def hit(ref,amount):
   es[ref]['damage']+=amount
   if es[ref]['damage']>=es[ref]['toughness']:died(ref)
  def inspect(ref):
   e=es[ref];seq=[observe(ref,'in_play',int(e['alive']))]
   if e['alive']:
    seq += [observe(ref,'tapped',int(e['tapped']))]
    if e['creature']:seq += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness']),observe(ref,'damage',e['damage'])]
   return seq
  casting=p['spell'] or v in ('cast_payment','insufficient_cast_mana','fresh_cast')
  add(name,'source',owner,'hand' if casting else 'battlefield',['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
  attack_name=p['alternate_attacker'] if v=='alternate_source' else 'Squire' if v=='one_damage' else 'Scaled Wurm' if v=='wrong_ability' else p.get('attacker','Scaled Wurm')
  add(attack_name,'attacker',enemy);add('Air Elemental' if v=='wrong_ability' else p.get('unselected','Hill Giant'),'unselected',enemy)
  add('Grizzly Bears' if v=='no_ferocious' else 'Air Elemental' if 'Flying' in o[attack_name]['keywords'] else 'Scaled Wurm','blocker',owner)
  if v=='cleanup':add('Vitalize','untap',enemy,'hand')
  if v=='growth':add('Giant Growth','growth',enemy,'hand')
  if v=='lose_ferocious':add('Shrink','shrink',owner,'hand')
  if v in ('cast_payment','insufficient_cast_mana','cast_permission'):
   mana[owner]=list(p['mana']) if v!='insufficient_cast_mana' else [0]*6
   pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
  if casting and not p['spell'] and v!='insufficient_cast_mana':
   pre += [step('cast',ref='source',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed'),step('refresh')]
   pay(owner,p['mana']);hands[owner].remove(name);es['source']['alive']=True
   if p['creature']:pre += [observe('source','sick',1)]
  cast_only=v=='cast_payment' and not p['spell']
  denied=v in ('tapped','sick','activation_insufficient','activation_wrong_color') or v=='fresh_cast' and p['tap']
  enabled=v not in ('without_effect','insufficient_cast_mana','wrong_role','wrong_ability') and not cast_only and not denied
  def declare(block_main=False,block_other=False):
   seq=[step('turn',player=enemy,value=0),step('begin_attackers')]
   for ref in ('attacker','unselected'):
    if es[ref]['alive']:
     seq += [step('attack',ref=ref,value=1)];es[ref]['tapped']=True;es[ref]['attacking']=True;es[ref]['blocked']=block_main if ref=='attacker' else block_other
   seq += [step('finish_attackers'),step('begin_blockers')]
   if block_main:seq += [step('block',ref='blocker',other='attacker',value=1)]
   if block_other and es['unselected']['alive']:seq += [step('block',ref='blocker',other='unselected',value=1)]
   return seq+[step('advance_combat')]
  def reduced(amount,src,recipient,protect):
   if not protect:return amount
   target_creature=isinstance(recipient,str);own_player=not target_creature and recipient==owner
   prevent=False
   if binding=='chosen_combat':
    prevent=src=='attacker' or p.get('optional_second_target') and v=='two_targets' and src=='unselected' or p.get('prevent_incoming') and recipient==('blocker' if v=='split_targets' else 'attacker')
   elif binding=='packet_combat':prevent=src=='attacker' and own_player
   elif binding=='field_combat':
    rule=p['damage_rule'];flying='Flying' in o[es[src]['name']]['keywords']
    prevent=(own_player and (rule=='you_flying' and flying or rule=='you_nonflying_attacker' and not flying)
      or rule=='unblocked' and es[src].get('attacking') and not es[src].get('blocked')
      or rule=='non_wolves' and not any(t in o[es[src]['name']]['type_line'].split(' — ')[-1].split() for t in ('Wolf','Werewolf'))
      or rule=='ferocious' and (not ferocious_state or es[src]['player']!=owner)
      or rule=='except_chosen' and src!='attacker')
   if prevent:
    prevented=amount-1 if p['amount']=='all_but_one' and amount>0 else amount
    if p.get('gain_prevented'):life[owner]+=prevented
    return amount-prevented
   return amount
  def combat(protect,block_main=False,block_other=False):
   seq=[]
   if binding=='packet_combat' and protect:
    # A blocked control creates a distinct creature recipient, so the sole
    # player packet identifies the selected source without ambiguous choices.
    seq += [step('choose_priority',ref='source')]+[step('choose_dialog',player=owner,value=n) for n in p['dialogs'][str(owner)]]
    seq += [step('choose_damage_player',player=owner,value=owner)]
    pay(owner,p['activation_mana'])
   seq += [step('combat_damage'),step('resolve'),step('end_combat'),step('assert_choices_consumed')]
   amounts=[]
   for ref in ('attacker','unselected'):
    if not es[ref]['alive']:continue
    blocked=block_main if ref=='attacker' else block_other
    if blocked:
     amounts += [('blocker',reduced(es[ref]['power'],ref,'blocker',protect)),(ref,reduced(es['blocker']['power'],'blocker',ref,protect))]
    else:
     amount=reduced(es[ref]['power'],ref,owner,protect);life[owner]-=amount
   for ref,amount in amounts:hit(ref,amount)
   seq += [step('assert_life',player=owner,value=life[owner])]
   for ref in ('attacker','unselected','blocker'):seq += inspect(ref)
   return seq
  if v=='insufficient_cast_mana':actions += [step('cast',ref='source',value=0),step('assert_stack',value=0)]
  elif v=='cast_permission':
   actions += [step('require_can_cast',ref='source',value=1),step('choose_target',player=owner,ref='attacker'),step('cast',ref='source',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
   pay(owner,p['mana']);hands[owner].remove(name);graves[owner].append(name);life[owner]+=es['attacker']['power']
  elif not cast_only:
   block_main=v in ('blocked','no_ferocious','lose_ferocious') or v=='wrong_role' and p.get('target_predicate')=='unblocked';block_other=binding=='packet_combat' or v=='split_targets'
   actions += declare(block_main,block_other)
   if v=='wrong_role':actions += [step('probe_activation_target',ref='source',other='attacker' if p.get('target_predicate')=='unblocked' else 'blocker',index=-1,value=0)]
   if p.get('target_predicate')=='attacking_flying' and v not in ('wrong_role','activation_insufficient','activation_wrong_color','tapped','sick','fresh_cast'):actions += [step('probe_activation_target',ref='source',other='attacker',index=-1,value=int(v!='wrong_ability' and not denied))]
   if v in ('payment','activation_insufficient','activation_wrong_color'):
    pool=list(p['mana'] if p['spell'] else p['activation_mana']) if v=='payment' else [0]*6
    if v=='activation_wrong_color':pool[0]=sum(p['activation_mana'])
    mana[owner]=pool;actions += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(pool)]
   if denied:actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_stack',value=0)]
   elif enabled and binding!='packet_combat':
    if not p['spell']:actions += [step('require_can_activate',ref='source',value=1)]
    actions += [step('choose_dialog',player=owner,value=n) for n in (p['red_spent_dialogs'][str(owner)] if v=='red_spent' else p['dialogs'][str(owner)])]
    if binding=='chosen_combat' or p.get('damage_rule')=='except_chosen':
     actions += [step('choose_target',player=owner,ref='attacker') for _ in range(p.get('target_repetitions',1))]
     if p.get('optional_second_target'):actions += [step('choose_target',player=owner,ref='unselected')] if v=='two_targets' else [step('choose_cancel_target',player=owner)]
     if p.get('prevent_incoming'):actions += [step('choose_target',player=owner,ref='blocker' if v=='split_targets' else 'attacker')]
    actions += [step('cast' if p['spell'] else 'activate',ref='source',value=1),step('assert_stack',value=1),step('assert_choices_consumed')]
    cost=list(p['mana'] if p['spell'] else p['activation_mana'])
    if v=='red_spent':cost[0]-=1;cost[4]+=1
    pay(owner,cost)
    if p['tap']:es['source']['tapped']=True
    if p.get('return_source'):es['source']['alive']=False;hands[owner].append(name)
    if p['spell']:hands[owner].remove(name);graves[owner].append(name)
    actions += inspect('source');pending_cut=len(actions)
    if v=='source_lost_pending':actions += [step('destroy',ref='source')];died('source')
    actions += [step('resolve'),step('assert_choices_consumed')]
    if v=='red_spent':life[enemy]-=es['attacker']['power'];actions += [step('assert_life',player=enemy,value=life[enemy])]
    if p.get('damage_rule')=='ferocious':ferocious_state=any(e['alive'] and e['creature'] and e['player']==owner and e['power']>=4 for e in es.values())
    if p.get('gain_power'):life[owner]+=es['attacker']['power'];actions += [step('assert_life',player=owner,value=life[owner])]
   if v=='lose_ferocious':
    actions += [step('choose_target',player=owner,ref='blocker'),step('cast',ref='shrink',value=1),step('resolve'),step('assert_choices_consumed')]
    pay(owner,cost_from_oracle(o['Shrink']));hands[owner].remove('Shrink');graves[owner].append('Shrink');es['blocker']['power']-=5
    actions += inspect('blocker')
   if v=='growth':
    actions += [step('choose_target',player=enemy,ref='attacker'),step('cast',ref='growth',value=1),step('resolve'),step('assert_choices_consumed')]
    pay(enemy,cost_from_oracle(o['Giant Growth']));hands[enemy].remove('Giant Growth');graves[enemy].append('Giant Growth')
    es['attacker']['power']+=3;es['attacker']['toughness']+=3
    actions += inspect('attacker')
   actions += combat(enabled,block_main,block_other)
   if v=='cleanup':
    actions += [step('cleanup'),step('refresh'),step('turn',player=enemy,value=0),step('cast',ref='untap',value=1),step('resolve'),step('assert_choices_consumed')]
    pay(enemy,cost_from_oracle(o['Vitalize']));hands[enemy].remove('Vitalize');graves[enemy].append('Vitalize')
    for ref,e in es.items():
     e['damage']=0
     if e['alive'] and e['player']==enemy and e['creature']:e['tapped']=False
    actions += declare()+combat(False)
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref in es:checks += inspect(ref)
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in es.values()}
  for pl in (0,1):
   battlefield=[e['name'] for e in es.values() if e['alive'] and e['player']==pl]
   checks += [step('assert_life',player=pl,value=life[pl])]+[step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for zone,names in (('battlefield',battlefield),('hand',hands[pl]),('graveyard',graves[pl]),('library',libs[pl]),('exile',exiles[pl])):
    checks += [step('assert_zone',player=pl,zone=zone,value=len(names))]
    if zone=='library':checks += [step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=pending_cut if v=='pending_snapshot' else 0;tail=actions[cut:]+checks
   steps=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'PreventionExtendedCombatRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='prevention',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps']+
      ['selected creature noncombat damage','multiple blockers','damage redirection','unpreventable damage','control changes','multiplayer'])))
 return result




def _legacy_profile(p):
 return dict(kind=p['legacy_kind'],scope=p['legacy_scope'],gain='none',activation_cost='',tap=False,sacrifice=False,keywords=[],inactive_clauses=list(p['gaps']),mana=list(p['mana']),activation_mana=[0]*6,creature=p['creature'],power=p['power'],toughness=p['toughness'])


def _legacy_profile_variants(p):
 vs=variants(_legacy_profile(p))
 if p.get('selected_components'):vs=[v for v in vs if v.rsplit('_',1)[0] in p['selected_components']]
 return vs


def _legacy_profile_expand(name,csvid,d,p,selected,rows,o):
 if not selected or any(v not in _legacy_profile_variants(p) for v in selected):raise ValueError('reviewed legacy prevention components required')
 _shield_validate(o,rows)
 return expand(name,csvid,d,_legacy_profile(p),selected,rows,o)



def _replacement_spec(d,r):
 from recipes import cost_from_oracle
 if (not r or d.get('version')!=2 or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
     or 'Creature' not in r.get('type_line','') or r.get('type_line','').startswith('Token ') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('replacement prevention requires whole normal creature Oracle and explicit variants')
 profile=_shield_pins()['sources'].get(r['name']);p=d.get('params')
 if not profile or p!=profile['params'] or any(r.get(k)!=v for k,v in profile['oracle'].items()):raise ValueError('unreviewed whole replacement source or parameters')
 if (set(p)!={'extension','replacement','entry_counters','casting_cost','gaps'} or p['extension']!='replacement'
     or p['replacement'] not in ('phantom','minus_counters','draw','zombie_phantom','conditional_phantom','exile_combat') or type(p['entry_counters']) is not int or not 0<=p['entry_counters']<=7
     or not isinstance(p['gaps'],list) or any(not isinstance(x,str) for x in p['gaps'])):raise ValueError('typed exact damage replacement parameters required')
 # Swans explicitly spends white for both printed W/U symbols; no other hybrid conversion is admitted.
 expected_cost=r['mana_cost'].replace('{W/U}','{W}') if p['replacement']=='draw' else r['mana_cost']
 if p['casting_cost']!=expected_cost:raise ValueError('reviewed printed replacement payment required')
 if any(not re.fullmatch(r'\d+',r.get(k,'')) for k in ('power','toughness')):raise ValueError('numeric replacement creature stats required')
 return dict(copy.deepcopy(p),family='prevention',mana=cost_from_oracle(dict(mana_cost=p['casting_cost'])),power=int(r['power']),toughness=int(r['toughness']))


def _replacement_variants(p):
 if p['replacement']=='exile_combat':return [v+'_'+str(owner) for owner in (0,1) for v in ('effect','small_damage','short_library','bare_zones','noncombat','friendly_source','payment','insufficient_cast_mana','ready_snapshot','pending_snapshot','combat_snapshot')]
 vs=['effect','small_damage','repeat_damage','other_damage','friendly_source','cleanup','payment','insufficient_cast_mana','ready_snapshot','pending_snapshot','damaged_snapshot','bare_zones','blocking','combat_snapshot']
 if p['replacement']!='draw':vs+=['exhaustion']
 if p['replacement'] in ('phantom','zombie_phantom'):vs+=['zero_counters']
 if p['replacement']=='zombie_phantom':vs+=['without_battlefield_zombie','without_grave_zombies']
 if p['replacement']=='conditional_phantom':vs+=['without_counter','opponent_land','failed_search','nonland_spell']
 return [v+'_'+str(owner) for owner in (0,1) for v in vs]


def _replacement_expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 if not selected or len(set(selected))!=len(selected) or any(v not in _replacement_variants(p) for v in selected):raise ValueError('unique reviewed replacement variants required')
 _shield_validate(o,rows);result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);mode=p['replacement'];bare=v=='bare_zones' or mode=='exile_combat' and v=='short_library' or mode=='conditional_phantom' and v=='failed_search';combat=v in ('blocking','combat_snapshot') or mode=='exile_combat' and v not in ('noncombat','friendly_source','insufficient_cast_mana')
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
  setup=[];es={};mana=[[20]*6,[20]*6];life=[20,20];plus=p['entry_counters'];minus=0;boost=0;control_boost=0;searched=set()
  if mode=='zombie_phantom':plus=1 if v=='without_grave_zombies' else 2 if v=='without_battlefield_zombie' else 3
  # A single affordable hybrid color avoids an ambiguous payment dialog. The unused blue path is an explicit gap.
  if mode=='draw':mana[owner][2]=0
  if v=='payment':mana[owner]=list(p['mana'])
  if v=='insufficient_cast_mana':mana[owner]=[0]*6
  pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=pl,color=c,value=n) for pl in (0,1) for c,n in enumerate(mana[pl])]
  def add(nm,ref,pl,zone='battlefield'):
   setup.append(card(nm,ref,pl,zone));es[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',damage=0)
   if zone=='hand':hands[pl].append(nm)
  def pay(pl,cost):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  def stats():
   if not es['source']['alive']:return [observe('source','in_play',0)]
   return ([step('require_card',ref='source',property='in_play',value=1)] if mode in ('zombie_phantom','conditional_phantom','exile_combat') else [observe('source','in_play',1)])+[observe('source','plus1_counters',plus),observe('source','minus1_counters',minus),observe('source','damage',es['source']['damage']),
    observe('source','power',p['power']+plus-minus+boost),observe('source','toughness',p['toughness']+plus-minus+boost)]
  def cast_spell(ref,target):
   e=es[ref];pl=e['player'];pay(pl,cost_from_oracle(o[e['name']]));hands[pl].remove(e['name']);graves[pl].append(e['name'])
   return [step('choose_target',player=pl,ref=target),step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
  add(name,'source',owner,'hand');add('Scaled Wurm','control',owner)
  damage_caster=owner if v=='friendly_source' else 1-owner
  count=2 if v in ('repeat_damage','cleanup','damaged_snapshot') or v=='exhaustion' and mode=='conditional_phantom' else p['entry_counters'] if v=='exhaustion' and mode in ('phantom','zombie_phantom') else 3 if v=='exhaustion' else p['entry_counters']+1 if v=='zero_counters' else 1
  if mode=='zombie_phantom':
   if v!='without_battlefield_zombie':add('Scathe Zombies','own_zombie',owner)
   add('Scathe Zombies','opposing_zombie',1-owner)
   for pl,n in ((owner,0 if v=='without_grave_zombies' else 2),(1-owner,1)):
    for i in range(n):setup.append(card('Scathe Zombies',f'zombie_grave_{pl}_{i}',pl,'graveyard'));graves[pl].append('Scathe Zombies')
  if mode=='conditional_phantom' and bare:
   libs[owner]=['Grizzly Bears','Lightning Bolt'] if v=='failed_search' else ['Forest']
   for i,nm in enumerate(libs[owner]):setup.append(card(nm,'search_library_'+str(i),owner,'library'))
  if mode=='conditional_phantom' and v not in ('without_counter','insufficient_cast_mana','nonland_spell'):add('Rampant Growth','land_search',1-owner if v=='opponent_land' else owner,'hand')
  if mode=='conditional_phantom' and v=='nonland_spell':add('Giant Growth','nonland',owner,'hand')
  if mode=='exile_combat' and v=='short_library':libs[owner]=['Forest'];setup.append(card('Forest','short_library',owner,'library'))
  if mode=='draw' and bare:
   libs[damage_caster]=list(LIBRARY)
   for i,nm in enumerate(libs[damage_caster]):setup.append(card(nm,'draw_library_'+str(i),damage_caster,'library'))
  if combat:add('Krosan Colossus' if mode=='minus_counters' else 'Squire' if mode=='exile_combat' and v=='small_damage' else 'Hill Giant','attacker',1-owner)
  else:
   for i in range(count):add('Shock' if v=='small_damage' else 'Lightning Bolt','packet_'+str(i),damage_caster,'hand')
  if v=='other_damage':add('Lightning Bolt','control_packet',1-owner,'hand')
  if v=='zero_counters' or mode=='zombie_phantom' and v=='without_grave_zombies':add('Giant Growth','growth',owner,'hand')
  if v=='insufficient_cast_mana':
   actions=[step('cast',ref='source',value=0),step('assert_stack',value=0),observe('source','in_hand',1)]
   plus=0
  else:
   pay(owner,p['mana']);hands[owner].remove(name);es['source']['alive']=True
   start=[step('cast',ref='source',value=1),step('assert_stack',value=1)]
   resolved=[step('resolve'),step('assert_choices_consumed')]+stats()
   actions=start+resolved
   if v=='payment':actions += [step('assert_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   # Damage spells and Growth have independent real mana after exact source-payment receipts.
   if damage_caster==owner or v=='zero_counters':
    for c in (3,4):mana[owner][c]=20;actions.append(step('set_mana',player=owner,color=c,value=20))
   if v=='zero_counters' or mode=='zombie_phantom' and v=='without_grave_zombies':actions+=cast_spell('growth','source');boost=3;actions+=stats()
   if mode=='conditional_phantom' and v not in ('without_counter','insufficient_cast_mana'):
    if v=='nonland_spell':actions+=cast_spell('nonland','control');control_boost=3;actions+=stats()
    else:
     actor=1-owner if v=='opponent_land' else owner
     if actor!=owner:actions.append(step('turn',player=actor,value=0))
     if v=='payment':
      for c in (0,3):mana[owner][c]=20;actions.append(step('set_mana',player=owner,color=c,value=20))
     pay(actor,cost_from_oracle(o['Rampant Growth']));hands[actor].remove('Rampant Growth');graves[actor].append('Rampant Growth')
     actions += [step('cast',ref='land_search',value=1),step('assert_stack',value=1),observe('source','plus1_counters',0)]
     actions += [step('choose_zone',player=-1,value=-1)] if v=='failed_search' else [step('choose_zone_card',player=actor,value=rows['Forest']['csvid'])]
     actions += [step('resolve'),step('assert_choices_consumed')];searched.add(actor)
     if v!='failed_search':
      libs[actor].remove('Forest');es['new_land']=dict(name='Forest',player=actor,alive=True,damage=0)
      actions += [step('bind_permanent',player=actor,value=rows['Forest']['csvid'],ref='new_land'),observe('new_land','tapped',1)]
      if actor==owner:plus=1
     actions+=stats()
   def packet(i):
    nonlocal plus,minus
    amount=2 if v=='small_damage' else 3;seq=cast_spell('packet_'+str(i),'source')
    if mode in ('phantom','zombie_phantom'):plus=max(0,plus-1)
    elif mode=='minus_counters':minus+=amount
    elif mode=='conditional_phantom':
     if plus:plus-=1
     else:es['source']['damage']+=amount
    elif mode=='exile_combat':es['source']['damage']+=amount
    else:
     if len(libs[damage_caster])<amount:raise ValueError('replacement draw fixture would deck')
     hands[damage_caster]+=libs[damage_caster][:amount];libs[damage_caster]=libs[damage_caster][amount:]
    if p['toughness']+plus-minus+boost<=0 or es['source']['damage']>=p['toughness']+plus-minus+boost:es['source']['alive']=False;graves[owner].append(name)
    return seq+stats()
   if combat:
    cut=len(actions);attacker_name=es['attacker']['name'];incoming=int(o[attacker_name]['power']);outgoing=p['power']+plus-minus+boost
    actions += [step('turn',player=1-owner,value=0),step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='source',other='attacker',value=1),step('advance_combat'),step('combat_damage'),step('resolve'),step('end_combat'),step('assert_choices_consumed')]
    if mode in ('phantom','zombie_phantom'):plus=max(0,plus-1)
    elif mode=='minus_counters':minus+=incoming
    elif mode=='conditional_phantom':
     if plus:plus-=1
     else:es['source']['damage']+=incoming
    elif mode=='exile_combat':
     n=min(incoming,len(libs[owner]));exiles[owner]+=libs[owner][:n];libs[owner]=libs[owner][n:]
    else:
     hands[1-owner]+=libs[1-owner][:incoming];libs[1-owner]=libs[1-owner][incoming:]
    if p['toughness']+plus-minus+boost<=0 or es['source']['damage']>=p['toughness']+plus-minus+boost:es['source']['alive']=False;graves[owner].append(name)
    if mode=='minus_counters':attacker_minus=outgoing;attacker_damage=0
    else:attacker_minus=0;attacker_damage=outgoing
    if attacker_damage>=int(o[attacker_name]['toughness'])-attacker_minus:es['attacker']['alive']=False;graves[1-owner].append(attacker_name)
    if name=='Phantom Nishoba':life[owner]+=outgoing
    actions+=stats()+[step('assert_life',player=owner,value=life[owner])]
   else:
    first=packet(0);actions+=first
   if v=='damaged_snapshot':
    prefix=list(actions);tail=[]
    for i in range(1,count):tail+=packet(i)
    actions=prefix+[step('capture')]+copy.deepcopy(tail)
    replay_tail=tail
   elif not combat:
    for i in range(1,count):
     if v=='cleanup':actions += [step('cleanup')];boost=0;control_boost=0;es['source']['damage']=0
     actions+=packet(i)
   if v=='other_damage':
    actions+=cast_spell('control_packet','control');es['control']['damage']=3
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]+stats()+[observe('control','in_play',1),observe('control','power',7+control_boost),observe('control','toughness',6+control_boost),observe('control','damage',es['control']['damage'])]
  if combat:
   checks += [observe('attacker','in_play',int(es['attacker']['alive']))]
   if es['attacker']['alive']:checks += [observe('attacker','minus1_counters',attacker_minus),observe('attacker','power',int(o[attacker_name]['power'])-attacker_minus),observe('attacker','toughness',int(o[attacker_name]['toughness'])-attacker_minus),observe('attacker','damage',attacker_damage)]
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in es.values()}
  for pl in (0,1):
   checks += [step('assert_life',player=pl,value=life[pl])]+[step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for zone,names in (('battlefield',[e['name'] for e in es.values() if e['alive'] and e['player']==pl]),('hand',hands[pl]),('graveyard',graves[pl]),('library',libs[pl]),('exile',exiles[pl])):
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(names)))
    if zone=='library' and pl not in searched:checks += [step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  steps=pre+actions+checks
  if v=='ready_snapshot':steps=pre+[step('capture')]+copy.deepcopy(actions+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions+checks)+[step('assert_recorded_state')]
  if v=='pending_snapshot':steps=pre+start+[step('capture')]+copy.deepcopy(actions[len(start):]+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions[len(start):]+checks)+[step('assert_recorded_state')]
  if v=='damaged_snapshot':steps=pre+actions+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(replay_tail+checks)+[step('assert_recorded_state')]
  if v=='combat_snapshot':steps=pre+actions[:cut]+[step('capture')]+copy.deepcopy(actions[cut:]+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions[cut:]+checks)+[step('assert_recorded_state')]
  gaps=list(p['gaps'])
  if combat and mode=='minus_counters':gaps=[x for x in gaps if x!='Outgoing infect damage, poison and counter cancellation']+['Poison to players and counter cancellation','Krosan Colossus Morph from face-down state']
  result.append(dict(name=f'PreventionReplacementRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='prevention',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps+([] if combat else ['combat damage'])+['damage that cannot be prevented','redirection','multiple simultaneous packets','control changes','multiplayer'])))
 return result


def _modifier_spec(d,r):
 from recipes import cost_from_oracle
 if (not r or d.get('version')!=2 or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
     or not ('Creature' in r.get('type_line','') or r.get('type_line')=='Enchantment') or r.get('type_line','').startswith('Token ') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('modifier prevention requires whole normal source and explicit variants')
 profile=_shield_pins()['sources'].get(r['name']);p=d.get('params')
 if not profile or p!=profile['params'] or any(r.get(k)!=v for k,v in profile['oracle'].items()):raise ValueError('unreviewed whole modifier source or parameters')
 if (set(p)!={'extension','damage_rule','gaps'} or p['extension']!='modifier' or p['damage_rule'] not in ('spell_one','own_other_growth','own_noncombat_gain','opponent_double_own_half','sliver_one','other_dinosaur_one','five_colors','targeted_self_with_ally')
     or not isinstance(p['gaps'],list) or any(not isinstance(x,str) for x in p['gaps'])):raise ValueError('typed modifier source rule required')
 return dict(copy.deepcopy(p),family='prevention',mana=cost_from_oracle(r),power=int(r.get('power','0')),toughness=int(r.get('toughness','0')))


def _modifier_variants(p):
 vs=['effect','small_damage','repeat_damage','source_damage','opponent_creature','self_player','opponent_player','friendly_source','creature_damage','cleanup','source_removed','payment','insufficient_cast_mana','ready_snapshot','pending_snapshot','damaged_snapshot','bare_zones']
 if p['damage_rule']=='targeted_self_with_ally':vs=[v for v in vs if v not in ('source_damage','source_removed')]+['nonmatching_recipient','without_condition','ally_removed']
 if p['damage_rule']=='sliver_one':vs+=['nonmatching_recipient']
 if p['damage_rule']=='other_dinosaur_one':vs+=['nonmatching_recipient','self_exclusion']
 if p['damage_rule']=='five_colors':vs.remove('source_damage');vs+=['without_condition','opponent_condition','condition_removed']
 else:vs+=['untargeted_spell']
 vs+=['creature_ability']
 if p['damage_rule']=='own_other_growth':vs+=['large_combat']
 return [v+'_'+str(owner) for owner in (0,1) for v in vs]


def _modifier_expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 if not selected or len(set(selected))!=len(selected) or any(v not in _modifier_variants(p) for v in selected):raise ValueError('unique reviewed modifier variants required')
 _shield_validate(o,rows);result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';rule=p['damage_rule']
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
  setup=[];es={};mana=[[20]*6,[20]*6];life=[20,20];enabled=True
  if v=='payment':mana[owner]=list(p['mana'])
  if v=='insufficient_cast_mana':mana[owner]=[0]*6
  pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=pl,color=c,value=n) for pl in (0,1) for c,n in enumerate(mana[pl])]
  def add(nm,ref,pl,zone='battlefield'):
   setup.append(card(nm,ref,pl,zone));r=o[nm];es[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',power=int(r.get('power','0')),toughness=int(r.get('toughness','0')),creature='Creature' in r['type_line'],damage=0,plus=0)
   if zone=='hand':hands[pl].append(nm)
  def add_spell(nm,ref,pl):
   setup.append(card(nm,ref,pl,'hand'));hands[pl].append(nm)
  def pay(pl,cost):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  def inspect():
   seq=[]
   for ref,e in es.items():
    seq.append(observe(ref,'in_play',int(e['alive'])))
    if e['alive'] and e['creature']:seq += [observe(ref,'power',e['power']+e['plus']),observe(ref,'toughness',e['toughness']+e['plus']),observe(ref,'damage',e['damage']),observe(ref,'plus1_counters',e['plus'])]
   return seq+[step('assert_life',player=pl,value=n) for pl,n in enumerate(life)]
  def spell(nm,ref,pl,target=None,target_player=None):
   pay(pl,cost_from_oracle(o[nm]));hands[pl].remove(nm);graves[pl].append(nm)
   choices=[step('choose_target',player=pl,ref=target)] if target else [step('choose_player',player=pl,value=target_player)] if target_player is not None else []
   return choices+[step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
  def hit(amount,target=None,target_player=None,combat=False,targeted_spell=True,spell_source=True):
   recipient=es[target]['player'] if target else target_player;prevented=0
   if enabled:
    if rule=='spell_one' and not combat and spell_source:prevented=min(1,amount)
    elif rule=='own_other_growth' and target and target!='source' and recipient==owner:
     prevented=amount;es[target]['plus']+=amount
    elif rule=='own_noncombat_gain' and not target and recipient==owner and not combat:prevented=amount;life[owner]+=amount
    elif rule=='opponent_double_own_half':
     if recipient==owner:prevented=(amount+1)//2
     else:amount*=2
    elif rule=='sliver_one' and target and 'Sliver' in o[es[target]['name']]['type_line'].split(' — ')[-1].split():prevented=min(1,amount)
    elif rule=='other_dinosaur_one' and target and recipient==owner and 'Dinosaur' in o[es[target]['name']]['type_line'].split(' — ')[-1].split():
     if any(ref!=target and e['alive'] and e['player']==owner and e['name']==name for ref,e in es.items()):prevented=max(0,amount-1)
    elif rule=='five_colors' and not target and recipient==owner:
     colors={c for e in es.values() if e['alive'] and e['player']==owner for c in o[e['name']].get('colors',[])}
     if colors=={'W','U','B','R','G'}:prevented=amount
    elif rule=='targeted_self_with_ally' and target=='source' and not combat and targeted_spell and spell_source:
     if any(ref!='source' and e['alive'] and e['player']==owner and e['creature'] for ref,e in es.items()):prevented=amount
   actual=amount-prevented
   if target:
    e=es[target];e['damage']+=actual
    if e['damage']>=e['toughness']+e['plus']:e['alive']=False;graves[e['player']].append(e['name'])
   else:life[target_player]-=actual
  add(name,'source',owner,'hand')
  friendly_name='Brood Sliver' if rule=='sliver_one' else name if rule=='other_dinosaur_one' else 'Scaled Wurm'
  if v in ('nonmatching_recipient','self_exclusion'):friendly_name='Scaled Wurm'
  add(friendly_name,'friendly',1-owner if rule=='targeted_self_with_ally' and v=='without_condition' else owner)
  add('Brood Sliver' if rule=='sliver_one' else name if rule=='other_dinosaur_one' else 'Scaled Wurm','enemy',1-owner)
  if rule=='five_colors':
   add('Air Elemental','blue_color',owner);add('Hill Giant','red_color',owner)
   add('Sol Ring' if v=='without_condition' else 'Scathe Zombies','black_color',1-owner if v=='opponent_condition' else owner)
  caster=owner if v in ('friendly_source','untargeted_spell') else 1-owner
  target_player=owner if v=='self_player' or rule in ('own_noncombat_gain','five_colors') and v not in ('source_damage','opponent_creature','opponent_player') else 1-owner if v=='opponent_player' else None
  target=None if target_player is not None else 'enemy' if v=='opponent_creature' else 'source' if v in ('source_damage','self_exclusion') or rule=='targeted_self_with_ally' and v!='nonmatching_recipient' else 'friendly'
  count=2 if v in ('repeat_damage','cleanup','source_removed','damaged_snapshot','ally_removed','condition_removed') else 1
  if v=='creature_ability':add('Prodigal Sorcerer','ping_source',caster)
  elif v in ('creature_damage','large_combat'):
   add('Krosan Colossus' if v=='large_combat' else 'Hill Giant','attacker',1-owner)
  elif v=='untargeted_spell':add_spell('Pyroclasm','sweep',caster)
  elif v!='insufficient_cast_mana':
   for i in range(count):add_spell('Shock' if v=='small_damage' else 'Lightning Bolt','packet_'+str(i),caster)
  if v in ('source_removed','ally_removed','condition_removed'):add_spell('Disenchant' if v=='source_removed' and 'Creature' not in o[name]['type_line'] else 'Unsummon','bounce',1-owner)
  if v=='insufficient_cast_mana':actions=[step('cast',ref='source',value=0),step('assert_stack',value=0),observe('source','in_hand',1)]
  else:
   pay(owner,p['mana']);hands[owner].remove(name);es['source']['alive']=True
   start=[step('cast',ref='source',value=1),step('assert_stack',value=1)]
   actions=start+[step('resolve'),step('assert_choices_consumed')]+inspect()
   if v=='payment':actions += [step('assert_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   if caster==owner:mana[owner][4]=20;actions.append(step('set_mana',player=owner,color=4,value=20))
   if v=='creature_ability':
    actions += [observe('ping_source','tapped',0),step('require_can_activate',ref='ping_source',value=1)]
    actions += [step('choose_target',player=caster,ref=target)] if target else [step('choose_player',player=caster,value=target_player)]
    actions += [step('activate',ref='ping_source',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed'),observe('ping_source','tapped',1)]
    hit(1,target,target_player,spell_source=False);actions+=inspect()
   elif v in ('creature_damage','large_combat'):
    actions += [step('turn',player=1-owner,value=0),step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers'),step('begin_blockers')]
    blocked=rule not in ('own_noncombat_gain','five_colors');block_ref='source' if rule=='targeted_self_with_ally' else 'friendly'
    if blocked:actions.append(step('block',ref=block_ref,other='attacker',value=1))
    actions += [step('advance_combat'),step('combat_damage'),step('resolve'),step('end_combat'),step('assert_choices_consumed')]
    if blocked:
     # Both combat packets use powers immediately before damage, before Vigor growth.
     outgoing=es[block_ref]['power']+es[block_ref]['plus'];hit(int(o[es['attacker']['name']]['power']),target=block_ref,combat=True);hit(outgoing,target='attacker',combat=True)
    else:hit(3,target_player=owner,combat=True)
    actions+=inspect()
   elif v=='untargeted_spell':
    actions+=spell('Pyroclasm','sweep',caster)
    for ref,e in list(es.items()):
     if e['alive'] and e['creature']:hit(2,target=ref,targeted_spell=False)
    actions+=inspect()
   else:
    def packet(i):
     seq=spell('Shock' if v=='small_damage' else 'Lightning Bolt','packet_'+str(i),caster,target,target_player)
     hit(2 if v=='small_damage' else 3,target,target_player)
     return seq+inspect()
    actions+=packet(0)
    if v=='damaged_snapshot':prefix=list(actions);tail=packet(1);actions=prefix+[step('capture')]+copy.deepcopy(tail);replay_tail=tail
    else:
     for i in range(1,count):
      if v=='cleanup':
       actions.append(step('cleanup'))
       for e in es.values():e['damage']=0
      if v=='source_removed':
       removal='Unsummon' if es['source']['creature'] else 'Disenchant'
       actions+=spell(removal,'bounce',1-owner,'source');es['source']['alive']=False
       (hands if es['source']['creature'] else graves)[owner].append(name);enabled=False
       actions+=[observe('source','in_play',0)]
      elif v in ('ally_removed','condition_removed'):
       lost='friendly' if v=='ally_removed' else 'black_color'
       actions+=spell('Unsummon','bounce',1-owner,lost);es[lost]['alive']=False;hands[owner].append(es[lost]['name'])
      actions+=packet(i)
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]+inspect()
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in es.values()}|{'Lightning Bolt','Shock','Unsummon'}|({'Pyroclasm'} if v=='untargeted_spell' else set())|({'Disenchant'} if rule=='five_colors' and v=='source_removed' else set())
  for pl in (0,1):
   checks += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for zone,names in (('battlefield',[e['name'] for e in es.values() if e['alive'] and e['player']==pl]),('hand',hands[pl]),('graveyard',graves[pl]),('library',libs[pl]),('exile',exiles[pl])):
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(names)))
    if zone=='library':checks += [step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  steps=pre+actions+checks
  if v=='ready_snapshot':steps=pre+[step('capture')]+copy.deepcopy(actions+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions+checks)+[step('assert_recorded_state')]
  if v=='pending_snapshot':steps=pre+start+[step('capture')]+copy.deepcopy(actions[len(start):]+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions[len(start):]+checks)+[step('assert_recorded_state')]
  if v=='damaged_snapshot':steps=pre+actions+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(replay_tail+checks)+[step('assert_recorded_state')]
  gaps=[x for x in p['gaps'] if not (v=='creature_ability' and x=='Noncombat creature-ability damage')]
  result.append(dict(name=f'PreventionModifierRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='prevention',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps+(['Krosan Colossus Morph from face-down state'] if v=='large_combat' else [])+['planeswalkers','damage that cannot be prevented','redirection','multiple replacement-effect ordering','control changes','multiplayer'])))
 return result


def _entry_shield_spec(d,r):
 from recipes import cost_from_oracle
 if (not r or d.get('version')!=2 or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
     or 'Creature' not in r.get('type_line','') or r.get('type_line','').startswith('Token ') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):raise ValueError('entry shield requires whole normal creature Oracle and explicit variants')
 profile=_shield_pins()['sources'].get(r['name']);p=d.get('params')
 if not profile or p!=profile['params'] or any(r.get(k)!=v for k,v in profile['oracle'].items()):raise ValueError('unreviewed entry shield source or parameters')
 if (set(p)!={'extension','effect','gaps'} or p['extension']!='entry_shield' or p['effect'] not in ('artifact_creatures','own_player_counters','other_creatures_combat')
     or not isinstance(p['gaps'],list) or any(not isinstance(x,str) for x in p['gaps'])):raise ValueError('typed exact entry prevention effect required')
 return dict(copy.deepcopy(p),family='prevention',mana=cost_from_oracle(r))


def _entry_shield_variants(p):
 vs=['effect','payment','insufficient_cast_mana','ready_snapshot','pending_snapshot','resolved_snapshot','bare_zones','cleanup','source_removed','response_cast']
 if p['effect']=='artifact_creatures':vs+=['small_damage','repeat_damage','source_damage','opponent_creature','nonartifact','self_player','opponent_player','friendly_source','late_artifact','type_changed','creature_damage','combat_snapshot']
 elif p['effect']=='own_player_counters':vs+=['small_damage','repeat_damage','source_damage','opponent_player','opponent_creature','nonartifact','friendly_source','creature_damage','combat_snapshot','external_prevention']
 else:vs+=['noncombat','source_damage','small_damage','combat_snapshot']
 return [v+'_'+str(owner) for owner in (0,1) for v in vs]


def _entry_shield_expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 if not selected or len(set(selected))!=len(selected) or any(v not in _entry_shield_variants(p) for v in selected):raise ValueError('unique reviewed entry shield variants required')
 _shield_validate(o,rows);result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner;bare=v=='bare_zones';effect=p['effect']
  combat=v in ('creature_damage','combat_snapshot','external_prevention') or effect=='other_creatures_combat' and v not in ('noncombat','source_damage','response_cast','insufficient_cast_mana')
  initial_turn=enemy if combat or v=='response_cast' else owner
  hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
  libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
  setup=[];es={};spells={};mana=[[20]*6,[20]*6];life=[20,20]
  if v=='payment':mana[owner]=list(p['mana'])
  if v=='insufficient_cast_mana':mana[owner]=[0]*6
  pre=[step('turn',player=initial_turn,value=0)]+[step('set_mana',player=pl,color=c,value=n) for pl in (0,1) for c,n in enumerate(mana[pl])]
  def add(nm,ref,pl,zone='battlefield'):
   setup.append(card(nm,ref,pl,zone));r=o[nm];creature='Creature' in r['type_line']
   es[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',creature=creature,artifact='Artifact' in r['type_line'],power=int(r.get('power','0')),toughness=int(r.get('toughness','0')),damage=0,plus=0,tapped=False)
   if zone=='hand':hands[pl].append(nm)
  def add_spell(nm,ref,pl):setup.append(card(nm,ref,pl,'hand'));hands[pl].append(nm);spells[ref]=(nm,pl)
  def pay(pl,cost):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  def cost(nm):return [0]*6 if o[nm]['mana_cost']=='{0}' else cost_from_oracle(o[nm])
  def inspect():
   seq=[]
   for ref,e in es.items():
    seq += [step('require_card',ref=ref,property='in_play',value=1)] if e['alive'] and e['creature'] else [observe(ref,'in_play',int(e['alive']))]
    if e['alive']:
     seq += [step('assert_matching_permanents',player=e['player'],index=rows[e['name']]['csvid'],property='artifact',value=int(e['artifact'])),observe(ref,'tapped',int(e['tapped']))]
     if e['creature']:seq += [observe(ref,'power',e['power']+e['plus']),observe(ref,'toughness',e['toughness']+e['plus']),observe(ref,'plus1_counters',e['plus']),observe(ref,'damage',e['damage'])]
   return seq+[step('assert_life',player=pl,value=n) for pl,n in enumerate(life)]
  def hit(amount,target=None,pl=None,src=None,is_combat=False,protect=True,external=False):
   prevented=0
   if external and is_combat:prevented=amount
   elif protect:
    if effect=='artifact_creatures' and target and es[target]['creature'] and es[target]['artifact']:prevented=amount
    elif effect=='own_player_counters' and not target and pl==owner:prevented=amount
    elif effect=='other_creatures_combat' and is_combat and src!='source':prevented=amount
   if effect=='own_player_counters' and not target and pl==owner and prevented and es['source']['alive']:es['source']['plus']+=prevented
   if target:
    e=es[target];e['damage']+=amount-prevented
    if e['damage']>=e['toughness']+e['plus']:e['alive']=False;graves[e['player']].append(e['name'])
   else:life[pl]-=amount-prevented
  def ordinary_spell(ref,target=None,pl=None):
   nm,caster=spells[ref];pay(caster,cost(nm));hands[caster].remove(nm);graves[caster].append(nm)
   choices=[step('choose_target',player=caster,ref=target)] if target else [step('choose_player',player=caster,value=pl)] if pl is not None else []
   return choices+[step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
  add(name,'source',owner,'hand');add('Brass Man','artifact',owner);add('Brass Man','enemy_artifact',enemy);add('Scaled Wurm','ordinary',owner)
  if combat:
   add('Squire' if v=='small_damage' else 'Hill Giant','attacker',enemy);add('Scaled Wurm','unblocked',enemy)
  caster=owner if v=='friendly_source' else enemy
  target=None;target_player=None
  if effect=='artifact_creatures':
   target_player=owner if v=='self_player' else enemy if v=='opponent_player' else None
   target=None if target_player is not None else 'source' if v=='source_damage' else 'enemy_artifact' if v=='opponent_creature' else 'ordinary' if v in ('nonartifact','type_changed') else 'late' if v=='late_artifact' else 'artifact'
  elif effect=='own_player_counters':
   target='source' if v=='source_damage' else 'enemy_artifact' if v=='opponent_creature' else 'ordinary' if v=='nonartifact' else None
   target_player=None if target else enemy if v=='opponent_player' else owner
  else:
   target='source' if v=='source_damage' else 'ordinary' if v=='noncombat' else None;target_player=None if target else owner
  if v=='late_artifact':add('Brass Man','late',owner,'hand')
  if v=='type_changed':add('Liquimetal Coating','coating',owner)
  if v=='source_removed':
   add_spell('Unsummon','bounce',enemy)
   if combat:add_spell('Relentless Assault','extra_combat',enemy)
  if v=='external_prevention':add_spell('Fog','external',owner)
  if not combat or v=='external_prevention':
   add_spell('Shock' if v=='small_damage' else 'Lightning Bolt','packet_0',caster)
   if v in ('repeat_damage','cleanup','source_removed'):add_spell('Lightning Bolt','packet_1',caster)
  actions=[];pending_cut=0;combat_cut=0
  def packet(ref,protect=True):
   seq=ordinary_spell(ref,target,target_player);hit(2 if spells[ref][0]=='Shock' else 3,target,target_player,protect=protect)
   return seq+inspect()
  def combat_episode(protect=True,external=False,source_gone=False):
   for ref in ('attacker','unblocked'):es[ref]['tapped']=True
   seq=[step('turn',player=enemy,value=0),step('begin_attackers'),step('attack',ref='attacker',value=1),step('attack',ref='unblocked',value=1),step('finish_attackers'),step('begin_blockers')]
   blocker='ordinary' if source_gone else 'source' if effect=='other_creatures_combat' else 'artifact' if effect=='artifact_creatures' else None
   if blocker:seq.append(step('block',ref=blocker,other='attacker',value=1))
   seq.append(step('advance_combat'))
   if external:seq+=ordinary_spell('external')
   seq += [step('combat_damage'),step('resolve'),step('end_combat'),step('assert_choices_consumed'),step('assert_stack',value=0)]
   if blocker:
    outgoing=es[blocker]['power']+es[blocker]['plus'];hit(int(o[es['attacker']['name']]['power']),blocker,src='attacker',is_combat=True,protect=protect,external=external)
    hit(outgoing,'attacker',src=blocker,is_combat=True,protect=protect,external=external)
   else:hit(int(o[es['attacker']['name']]['power']),pl=owner,src='attacker',is_combat=True,protect=protect,external=external)
   hit(7,pl=owner,src='unblocked',is_combat=True,protect=protect,external=external)
   return seq+inspect()
  def real_next_enemy_turn():
   seq=[step('assert_stack',value=0)]+[step('set_mana',player=pl,color=c,value=0) for pl in (0,1) for c in range(6)]
   # These paths begin in the enemy's actual combat turn. Both prepared untaps run before returning to main priority.
   for pl in (owner,enemy):seq += [step('advance_turn'),step('untap_step',player=pl)]
   for e in es.values():e['damage']=0;e['tapped']=False
   for pl in (0,1):mana[pl]=[20]*6
   return seq+[step('turn',player=enemy,value=0)]+[step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)]
  if v=='insufficient_cast_mana':actions=[step('cast',ref='source',value=0),step('assert_stack',value=0),observe('source','in_hand',1)]
  elif v=='response_cast':
   nm,damage_caster=spells['packet_0'];pay(damage_caster,cost(nm));pay(owner,p['mana'])
   actions=([step('choose_target',player=damage_caster,ref=target)] if target else [step('choose_player',player=damage_caster,value=target_player)])
   actions += [step('cast',ref='packet_0',value=1),step('assert_stack',value=1),step('cast_response',ref='source',value=1),step('assert_stack',value=2),step('assert_zone',player=owner,zone='hand',value=len(hands[owner])),step('resolve_response'),step('assert_stack',value=0),step('assert_choices_consumed')]
   hands[owner].remove(name);hands[damage_caster].remove(nm);graves[damage_caster].append(nm);es['source']['alive']=True
   hit(3,target,target_player);actions+=inspect()
  else:
   pay(owner,p['mana']);actions=[step('cast',ref='source',value=1),step('assert_stack',value=1),step('assert_zone',player=owner,zone='hand',value=len(hands[owner]))];pending_cut=len(actions)
   hands[owner].remove(name);es['source']['alive']=True
   actions += [step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed')]+inspect()
   actions += [step('assert_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
   if v=='late_artifact':
    pay(owner,cost('Brass Man'));hands[owner].remove('Brass Man');es['late']['alive']=True
    actions += [step('cast',ref='late',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]+inspect()
   if v=='type_changed':
    actions += [step('require_can_activate',ref='coating',value=1),step('choose_target',player=owner,ref='ordinary'),step('activate',ref='coating',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
    es['coating']['tapped']=True;es['ordinary']['artifact']=True;actions+=inspect()
   if combat and v!='external_prevention':combat_cut=len(actions);actions+=combat_episode()
   else:actions+=packet('packet_0')
   if v=='repeat_damage':actions+=packet('packet_1')
   if v=='cleanup':
    if combat:actions+=real_next_enemy_turn()+combat_episode(protect=False)
    else:
     actions.append(step('cleanup'))
     for e in es.values():e['damage']=0
     actions+=packet('packet_1',protect=False)
   if v=='source_removed':
    actions+=ordinary_spell('bounce','source');es['source']['alive']=False;es['source']['plus']=0;hands[owner].append(name)
    actions+=[observe('source','in_play',0)]
    if combat:
     actions.append(step('turn',player=enemy,value=0));actions+=ordinary_spell('extra_combat')
     for ref in ('attacker','unblocked'):es[ref]['tapped']=False
     actions+=combat_episode(source_gone=True)
    else:actions+=packet('packet_1')
   if v=='external_prevention':actions+=real_next_enemy_turn()+combat_episode(protect=False,external=True)
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]+inspect()
  universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in es.values()}|{nm for nm,pl in spells.values()}
  for pl in (0,1):
   checks += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
   for zone,names in (('battlefield',[e['name'] for e in es.values() if e['alive'] and e['player']==pl]),('hand',hands[pl]),('graveyard',graves[pl]),('library',libs[pl]),('exile',exiles[pl])):
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(names)))
    if zone=='library':checks += [step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot','combat_snapshot'):
   cut=pending_cut if v=='pending_snapshot' else combat_cut if v=='combat_snapshot' else 0;tail=actions[cut:]+checks
   steps=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'PreventionEntryShieldRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='prevention',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps']+['unpreventable damage','redirection and prevention ordering','control changes','multiplayer'])))
 return result
