"""Oracle-derived attack events and value receipts through real declarations."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

FAMILY = 'attack_value_trigger'
WORDS = {'a':1,'one':1,'two':2,'three':3,'four':4,'five':5,'six':6,'seven':7,'eight':8,'nine':9,'ten':10}


def _pins():
 return json.loads((Path(__file__).parent/'data/attack_value_trigger_wave8_witnesses.json').read_text(encoding='utf8'))


def classify(r):
 from creature_recipes import oracle_keywords
 from recipes import cost_from_oracle
 if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
     or 'Creature' not in r.get('type_line','') or r['type_line'].startswith('Token ')):
  raise ValueError('normal standalone creature attack trigger required')
 if '{E}' in r.get('oracle_text',''):return _classify_energy(r)
 if re.search(r'attacks, you may pay \{[RWC]\}',r.get('oracle_text','')):return _classify_paid(r)
 lines=r.get('oracle_text','').splitlines();references={'this creature',r['name'],r['name'].split(',')[0]}
 source='(?:'+'|'.join(re.escape(ref) for ref in sorted(references))+')'
 trigger=None;effect=None;amount=power=toughness=0;other=False;line=None;extra={};passive={};passive_lines=[]
 for candidate in lines:
  if candidate=='This creature attacks each combat if able.':passive['mandatory_attack']=True;passive_lines.append(candidate)
  if candidate=="This creature can't be blocked except by creatures with flying or reach.":passive['evasion']='flying_or_reach';passive_lines.append(candidate)
  if candidate=='Devoid (This card has no color.)':
   if 'Devoid' not in r.get('keywords',[]) or r.get('colors')!=[]:raise ValueError('whole printed Devoid colorlessness required')
   passive['devoid']=True;passive_lines.append(candidate)
 for candidate in lines:
  prefix=rf'Whenever {source} attacks, '
  match=re.fullmatch(prefix+r'(?:each opponent|defending player) loses ([1-9]\d?) life\.',candidate)
  subtype_loss=re.fullmatch(r'Whenever a ([A-Z][A-Za-z-]+) you control attacks, defending player loses ([1-9]\d?) life\.',candidate)
  if match:trigger,effect,amount='self','life_loss',int(match[1])
  elif subtype_loss:trigger,effect,amount='own_subtype','life_loss',int(subtype_loss[2]);extra={'subtype':subtype_loss[1]}
  elif re.fullmatch(r"Whenever a creature you control attacks, this creature deals 1 damage to the player or planeswalker it's attacking\.",candidate):
   trigger,effect,amount='own','damage',1
  elif re.fullmatch(r"Whenever a creature attacks you or a planeswalker you control, that creature's controller mills (?:two|[1-9]\d?) cards\.",candidate):
   trigger,effect,amount='defense','mill',2 if 'mills two ' in candidate else int(re.search(r'mills (\d+)',candidate)[1])
  else:
   match=re.fullmatch(prefix+r'defending player mills (one|two|three|four|five|six|seven|eight|nine|ten|[1-9]\d?) cards?\.',candidate)
   if match:trigger,effect,amount='self','mill',WORDS.get(match[1],int(match[1]) if match[1].isdigit() else 0)
   else:
    match=re.fullmatch(prefix+r'(?:each )?(other )?attacking creatures? gets? \+([0-9]+)/\+([0-9]+) until end of turn\.',candidate)
    if match:trigger,effect,power,toughness,other='self','pump',int(match[2]),int(match[3]),bool(match[1])
    elif candidate=="Battle cry (Whenever this creature attacks, each other attacking creature gets +1/+0 until end of turn.)":
     trigger,effect,power,toughness,other='self','pump',1,0,True
    elif re.fullmatch(prefix+r'tap target creature defending player controls\.',candidate):trigger,effect='self','tap'
    elif re.fullmatch(prefix+r'untap each other creature you control\.',candidate):
     trigger,effect,other='self','untap',True
    elif re.fullmatch(prefix+r'put a \+1/\+1 counter on each attacking creature\.',candidate):
     trigger,effect,amount='self','counter',1;extra={'recipient':'attacking_team'}
    elif re.fullmatch(prefix+r'it gets \+X/\+0 until end of turn, where X is the number of attacking creatures you control\.',candidate):
     trigger,effect,power='self','self_pump',1;extra={'quantity':'attackers'}
    else:
     scaled=re.fullmatch(prefix+r'it gets \+([1-5])/\+([0-5]) until end of turn for each (untapped creature you control|land you control|other attacking ([A-Z][A-Za-z-]+))\.',candidate)
     team=re.fullmatch(prefix+r'(?:([A-Z][A-Za-z-]+) )?creatures you control get \+([0-5])/\+([0-5]) until end of turn\.',candidate)
     if scaled:
      trigger,effect,power,toughness='self','self_pump',int(scaled[1]),int(scaled[2])
      extra={'quantity':'untapped_creatures' if scaled[3]=='untapped creature you control' else 'lands' if scaled[3]=='land you control' else 'other_attacking_subtype'}
      if scaled[4]:extra['subtype']=scaled[4]
     elif team:
      trigger,effect,power,toughness='self','pump',int(team[2]),int(team[3]);extra={'recipient':'own_team'}
      if team[1]:extra['subtype']=team[1]
     else:continue
  if line is not None:raise ValueError('one reviewed complete attack mechanic required')
  line=candidate
 if line is None:raise ValueError('unsupported attack mechanic: retain for adjacent-model inventory')
 # Structured Mill/Battle Cry keywords describe the removed trigger, rather
 # than passive mechanics. The remaining text must be ordinary keywords.
 stripped=dict(r,oracle_text='\n'.join(x for x in lines if x!=line and x not in passive_lines),keywords=[k for k in r.get('keywords',[]) if k not in ('Mill','Battle Cry') and not (k=='Devoid' and passive.get('devoid'))])
 keywords=oracle_keywords(stripped)
 if set(keywords)&{'Double strike','Deathtouch','Lifelink','Defender','Menace'}:
  raise ValueError('attack keyword needs an additional combat model')
 if not 0<=amount<=20 or not 0<=power<=5 or not 0<=toughness<=5:raise ValueError('bounded attack value required')
 if effect in ('pump','self_pump') and not (power or toughness):raise ValueError('nonzero printed attack bonus required')
 cost_from_oracle(r)
 return dict(trigger=trigger,effect=effect,amount=amount,power=power,toughness=toughness,other=other,keywords=keywords,**extra,**passive)


def spec(d,r):
 from recipes import cost_from_oracle
 if (d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2
     or not r or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('whole pinned version2 attack-value declaration required')
 expected=classify(r);p=d.get('params')
 if (type(p) is not dict or set(p)!=set(expected) or p!=expected
     or any(type(p[k]) is not int for k in ('amount','power','toughness'))
     or type(p['other']) is not bool or type(p['keywords']) is not list
     or any(type(x) is not str for x in p['keywords'])
     or any(type(p[k]) is not str for k in ('trigger','effect'))
     or any(type(p[k]) is not str for k in ('quantity','subtype','recipient','evasion') if k in p)
     or any(type(p[k]) is not bool for k in ('mandatory_attack','devoid') if k in p)):
  raise ValueError('typed parameters must match the complete attack wording')
 if (any(type(p[k]) is not int for k in ('entry_energy','energy_cost','payment_amount') if k in p)
     or 'grants_trample' in p and type(p['grants_trample']) is not bool
     or 'requires_colorless' in p and type(p['requires_colorless']) is not bool
     or any(type(p[k]) is not str for k in ('target_effect','target_scope','payment_symbol') if k in p)):
  raise ValueError('typed printed energy parameters required')
 if p.get('subtype') and p['subtype'] not in _pins().get('subtype_witnesses',{}):
  raise ValueError('attack subtype needs independently pinned matching witnesses')
 ui=_pins().get('source_ui',{}).get(r['name'],{})
 if any(r.get(k)!=value for k,value in ui.get('oracle',{}).items()):raise ValueError('reviewed attack-trigger UI source changed')
 priority=ui.get('trigger_priority_players',[])
 if (type(priority) is not list or any(type(player) is not int or player not in (0,1) for player in priority)
     or len(set(priority))!=len(priority)):
  raise ValueError('reviewed trigger presentation annotation required')
 aggregation=ui.get('trigger_priority_aggregation','per_trigger')
 if aggregation not in ('per_source','per_trigger'):raise ValueError('reviewed trigger priority aggregation required')
 model=dict(copy.deepcopy(p),family=FAMILY,mana=cost_from_oracle(r),source_power=int(r['power']),source_toughness=int(r['toughness']),legendary='Legendary' in r['type_line'],trigger_priority_players=list(priority),trigger_priority_aggregation=aggregation,source_artifact='Artifact' in r['type_line'])
 if p['effect']=='paid_target':
  gate=ui.get('payment_gate');cancel=ui.get('cancellation_players')
  if gate not in ('affordability_checked','unconditional') or type(cancel) is not list or cancel!=[0] or any(type(x) is not int for x in cancel):
   raise ValueError('reviewed resolving-payment gate and human cancellation presentation required')
  vector=[0]*6;vector[{'C':0,'R':4,'W':5}[p['payment_symbol']]]=p['payment_amount']
  model.update(entry_energy=0,attack_mana=vector,payment_gate=gate,cancellation_players=list(cancel))
 return model


def variants(p):
 if p['effect']=='paid_target':return _paid_variants(p)
 if p['effect'].startswith('energy_'):return _energy_variants(p)
 values=['effect','attack_pair','other_attack','opponent_attack','no_attack','tapped','sick','source_absent',
         'cast','payment','insufficient_mana','fresh_attack','unblocked','cleanup','source_destroyed_after',
         'ready_snapshot','declaration_snapshot','resolved_snapshot','bare_zones']
 if 'First strike' not in p['keywords']:values+=['blocked']
 if not p['legendary']:values+=['two_sources']
 if p['effect']=='mill':values+=['short_library','empty_library']
 if p['effect'] in ('life_loss','damage'):values+=['prevention_control']
 if p['effect']=='tap':values+=['no_eligible_target','already_tapped']
 if 'quantity' in p or p.get('recipient')=='own_team' or p['effect']=='untap':values+=['no_qualifiers','extra_qualifier','opponent_qualifier']
 if p['trigger']=='own_subtype':values+=['nonmatching_attacker','extra_qualifier','opponent_qualifier']
 if p.get('evasion') or p.get('devoid') and 'Flying' in p['keywords']:values+=['ground_block_denied','reach_block_allowed']
 return [value+'_'+str(owner) for owner in (0,1) for value in values
         if not (value=='two_sources' and owner in p.get('trigger_priority_players',[]))]


def validate_witnesses(o,rows):
 pins=_pins()
 for nm,pin in pins['witnesses'].items():
  r=o.get(nm,{})
  if (nm not in rows or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
      or any(r.get(k)!=value for k,value in pin.items())):
   raise ValueError('whole attack-value witness changed: '+nm)
 for subtype,name in pins.get('subtype_witnesses',{}).items():
  if name not in pins['witnesses'] or not _has_subtype(o[name],subtype):
   raise ValueError('printed subtype witness must independently qualify: '+subtype)
  # Battlefield setup performs real entry before libraries are seeded. Matching
  # witnesses must not introduce unmodeled triggers or combat keywords.
  if re.search(r'\b(?:When|Whenever|At the beginning)\b',o[name]['oracle_text']) or set(o[name].get('keywords',[]))-{'Changeling'}:
   raise ValueError('subtype witness needs trigger-free ordinary combat: '+name)
 for nm,pin in pins.get('tokens',{}).items():
  r=o.get(nm,{})
  if (nm not in rows or r.get('layout')!='token' or not r.get('type_line','').startswith('Token ')
      or r.get('card_faces') or r.get('selected_face') or any(r.get(k)!=value for k,value in pin.items())):
   raise ValueError('whole independently pinned attack token changed: '+nm)


def _has_subtype(record,subtype):
 return subtype in record.get('type_line','').split(' — ')[-1].split() or 'Changeling' in record.get('keywords',[])


def expand(name,csvid,d,p,selected,rows,o):
 if p['effect']=='paid_target':return _expand_paid(name,csvid,d,p,selected,rows,o)
 if p['effect'].startswith('energy_'):return _expand_energy(name,csvid,d,p,selected,rows,o)
 from recipes import card,step,observe,cost_from_oracle
 if not selected or len(set(selected))!=len(selected) or any(v not in variants(p) for v in selected):
  raise ValueError('explicit unique reviewed attack-value variants required')
 validate_witnesses(o,rows);result=[]
 for variant in selected:
  value,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner;bare=value=='bare_zones'
  defensive=p['trigger']=='defense';actor=enemy if defensive else owner
  if value=='other_attack':actor=owner
  if value=='opponent_attack':actor=enemy
  casted=value in ('cast','payment','insufficient_mana','fresh_attack');illegal=value=='insufficient_mana'
  cast_only=value in ('cast','payment','insufficient_mana')
  setup=[];entities={};hands=[[],[]];graves=[[],[]];exiles=[[],[]];libraries=[[],[]];life=[20,20];mana=[[0]*6,[0]*6];lands=[0,0]
  pre=[];actions=[];sources=['source'];attackers=[];declaration_cut=0
  def add(nm,ref,player,zone='battlefield',state=None):
   item=card(nm,ref,player,zone)
   if state:item['state']=state
   setup.append(item)
   if zone=='battlefield' and 'Land' in o[nm]['type_line']:lands[player]+=1
   if zone in ('hand','graveyard','exile','library'):
    {'hand':hands,'graveyard':graves,'exile':exiles,'library':libraries}[zone][player].append(nm)
   if 'Creature' in o[nm]['type_line'] and (zone=='battlefield' or ref in ('source','copy')):
    entities[ref]=dict(name=nm,player=player,zone=zone,inspectable=zone not in ('graveyard','library','exile'),power=int(o[nm]['power']),toughness=int(o[nm]['toughness']),damage=0,
                       tapped=bool(state and 'tapped' in state),attacking=False,blocking_count=0,plus1_counters=0)
  def pool(player,counts):
   mana[player]=list(counts)
   return [step('set_mana',player=player,color=color,value=count) for color,count in enumerate(counts)]
  def move(ref,zone):
   e=entities[ref];e['zone']=zone;e['attacking']=False;e['blocking_count']=0
   if zone in ('hand','graveyard','exile'):{'hand':hands,'graveyard':graves,'exile':exiles}[zone][e['player']].append(e['name'])
  def stats():
   seq=[]
   for ref,e in entities.items():
    if not e['inspectable']:continue
    seq += [observe(ref,'in_play',int(e['zone']=='battlefield'))]
    if e['zone']=='battlefield':seq += [observe(ref,k,int(e[k])) for k in ('power','toughness','damage','tapped','attacking','blocking_count') if k!='blocking_count' or not e.get('historical_blocking_assignment')]
    if e['zone']=='battlefield' and p['effect']=='counter':seq += [observe(ref,'plus1_counters',e['plus1_counters'])]
   source_present=any(entities[ref]['zone']=='battlefield' for ref in sources)
   if p.get('devoid') and source_present:
    seq += [step('assert_matching_permanents',player=owner,index=csvid,property='color',value=0),step('assert_matching_permanents',player=owner,index=rows['Hill Giant']['csvid'],property='color',value=16)]
   if p.get('evasion') and source_present:
    seq += [step('assert_matching_permanents',player=owner,index=csvid,property='artifact',value=int(p['source_artifact']))]
   if p['trigger']=='own_subtype' and p['subtype'] in ('Sliver','Goblin','Elf','Kithkin','Human'):
    checked=set()
    for e in entities.values():
     key=(e['player'],rows[e['name']]['csvid'])
     if e['zone']=='battlefield' and key not in checked:
      checked.add(key);seq += [step('assert_matching_permanents',player=e['player'],index=key[1],property='subtype_'+p['subtype'],value=int(_has_subtype(o[e['name']],p['subtype'])))]
   return seq
  source_zone='graveyard' if value=='source_absent' else 'hand' if casted else 'battlefield'
  source_tapped=value=='tapped' or p.get('mandatory_attack') and value in ('no_attack','other_attack')
  add(name,'source',owner,source_zone,['tapped'] if source_tapped else ['sick'] if value=='sick' else None)
  if value=='two_sources':add(name,'copy',owner);sources.append('copy')
  matching=_pins().get('subtype_witnesses',{}).get(p.get('subtype'))
  ally=matching if matching and value!='no_qualifiers' else 'Grizzly Bears'
  idle=matching if matching and p.get('recipient')=='own_team' and value!='no_qualifiers' else 'Hill Giant'
  without_others=value=='no_qualifiers' and (p['effect']=='untap' or p.get('quantity') in ('attackers','untapped_creatures','other_attacking_subtype') or p.get('recipient')=='own_team' and not p.get('subtype'))
  if not without_others:
   add(ally,'ally',owner);add(idle,'idle',owner,'battlefield',['tapped'] if p['effect']=='untap' else None)
  if p.get('recipient')=='own_team' and not without_others:add('Hill Giant','nonmatching' if p.get('subtype') else 'own_control',owner)
  if value=='extra_qualifier' and p.get('quantity')!='lands':
   add('Woodland Changeling' if matching else 'Grizzly Bears','extra_qualifier',owner)
  if value=='opponent_qualifier' and p.get('quantity')!='lands':
   add(matching or 'Grizzly Bears','opponent_qualifier',enemy,'battlefield',['tapped'] if p['effect']=='untap' else None)
  if p.get('quantity')=='lands':
   if value!='no_qualifiers':
    add('Forest','land_1',owner);add('Island','land_2',owner)
   add('Mountain','opponent_land',enemy)
   if value=='extra_qualifier':add('Plains','extra_land',owner)
   if value=='opponent_qualifier':add('Swamp','extra_opponent_land',enemy)
  if value!='no_eligible_target':
   add(matching or 'Grizzly Bears','enemy',enemy,'battlefield',['tapped'] if value=='already_tapped' else None)
   add('Craw Wurm','enemy_idle',enemy,'battlefield',['tapped'] if p['effect']=='untap' else None)
  add('Sol Ring','artifact_control',enemy)
  if value in ('blocked','reach_block_allowed'):add('Giant Spider' if value=='reach_block_allowed' else 'Air Elemental','blocker',1-actor)
  if not bare:
   for player in (0,1):
    add('Forest',f'hand_{player}',player,'hand');add('Lightning Bolt',f'grave_{player}',player,'graveyard');add('Island',f'exile_{player}',player,'exile')
    cards=['Forest','Island','Plains','Swamp','Mountain','Grizzly Bears']*2
    if value=='short_library':cards=cards[:3]
    if value=='empty_library':cards=[]
    for index,nm in enumerate(cards):add(nm,f'library_{player}_{index}',player,'library')
  if value=='prevention_control':add('Safe Passage','shield',1-actor,'hand')
  pre=[step('turn',player=owner if cast_only else actor,value=0)]+pool(0,[0]*6)+pool(1,[0]*6)+[step('refresh')]
  if casted:
   actions += pool(owner,[0]*6 if illegal else p['mana'])+[step('cast',ref='source',value=int(not illegal)),step('assert_choices_consumed')]
   if not illegal:
    actions += [step('assert_stack',value=1),observe('source','in_play',0),observe('source','in_hand',0),step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed')]
    mana[owner]=[0]*6;hands[owner].remove(name);entities['source']['zone']='battlefield'
   actions += stats()
  if not cast_only:
   if value=='prevention_control':
    shield_owner=1-actor;cost=cost_from_oracle(o['Safe Passage'])
    actions += pool(shield_owner,cost)+[step('cast',ref='shield',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
    hands[shield_owner].remove('Safe Passage');graves[shield_owner].append('Safe Passage');mana[shield_owner]=[0]*6
   actions += [step('begin_attackers')]
   if value=='no_attack':chosen=[]
   elif actor==owner:
    chosen=['idle'] if value=='nonmatching_attacker' else ['ally'] if value in ('other_attack','source_absent') else ['source']
    if value in ('attack_pair','tapped','sick'):chosen+=['ally']
    if value=='two_sources':chosen+=['copy']
    if (p['effect'] in ('pump','untap','counter') or p.get('quantity')=='other_attacking_subtype') and 'ally' in entities and 'ally' not in chosen:chosen+=['ally']
    if p['effect']=='pump' and value=='attack_pair':chosen+=['idle']
    if value=='extra_qualifier' and 'extra_qualifier' in entities and p.get('quantity')!='untapped_creatures':chosen+=['extra_qualifier']
   else:
    chosen=['enemy']
    if value=='attack_pair':chosen+=['enemy_idle']
   for ref in chosen:
    e=entities[ref];is_source=ref in sources
    allowed=e['zone']=='battlefield' and not e['tapped'] and not (is_source and value=='sick' and 'Haste' not in p['keywords']) and not (is_source and value=='fresh_attack' and 'Haste' not in p['keywords'])
    actions += [step('attack',ref=ref,value=int(allowed))]
    if allowed:
     attackers.append(ref);e['attacking']=True;e['tapped']=not (is_source and 'Vigilance' in p['keywords'])
   actions += stats()+[step('assert_life',player=pl,value=life[pl]) for pl in (0,1)]
   declaration_cut=len(actions)
   active_sources=[ref for ref in sources if entities[ref]['zone']=='battlefield']
   count=0
   for source_ref in active_sources:
    triggers=sum((ref==source_ref if p['trigger']=='self' else entities[ref]['player']==owner and (p['trigger']!='own_subtype' or _has_subtype(o[entities[ref]['name']],p['subtype'])) if p['trigger'] in ('own','own_subtype') else entities[ref]['player']==enemy) for ref in attackers)
    count+=triggers
    if owner in p.get('trigger_priority_players',[]):
     priority_count=int(bool(triggers)) if p['trigger_priority_aggregation']=='per_source' else triggers
     actions += [step('choose_trigger',player=owner,ref=source_ref) for _ in range(priority_count)]
    if p['effect']=='pump' and triggers:
     recipients=entities if p.get('recipient')=='own_team' else attackers
     for ref in recipients:
      e=entities[ref]
      if (e['zone']=='battlefield' and e['player']==owner and (not p['other'] or ref!=source_ref)
          and (not p.get('subtype') or _has_subtype(o[e['name']],p['subtype']))):
       entities[ref]['power']+=p['power']*triggers;entities[ref]['toughness']+=p['toughness']*triggers
    if p['effect']=='self_pump' and triggers:
     if p['quantity']=='attackers':quantity=sum(entities[ref]['player']==owner for ref in attackers)
     elif p['quantity']=='untapped_creatures':quantity=sum(e['zone']=='battlefield' and e['player']==owner and not e['tapped'] for e in entities.values())
     elif p['quantity']=='lands':quantity=lands[owner]
     elif p['quantity']=='other_attacking_subtype':quantity=sum(ref!=source_ref and entities[ref]['player']==owner and _has_subtype(o[entities[ref]['name']],p['subtype']) for ref in attackers)
     else:raise ValueError('unreviewed attack-pump quantity')
     entities[source_ref]['power']+=p['power']*quantity*triggers;entities[source_ref]['toughness']+=p['toughness']*quantity*triggers
    if p['effect']=='untap' and triggers:
     for ref,e in entities.items():
      if ref!=source_ref and e['zone']=='battlefield' and e['player']==owner:e['tapped']=False
    if p['effect']=='counter' and triggers:
     for ref in attackers:
      if entities[ref]['player']==owner:
       amount=p['amount']*triggers;entities[ref]['plus1_counters']+=amount
       entities[ref]['power']+=amount;entities[ref]['toughness']+=amount
   if p['effect']=='tap' and count and value!='no_eligible_target':
    for _ in range(count):actions += [step('choose_target',player=owner,ref='enemy')]
    entities['enemy']['tapped']=True
   actions += [step('finish_attackers'),step('assert_choices_consumed'),step('assert_stack',value=0)]
   if p['effect'] in ('life_loss','damage'):
    amount=p['amount']*count
    if p['effect']=='damage' and value=='prevention_control':amount=0
    life[enemy]-=amount
   if p['effect']=='mill':
    target=actor if p['trigger']=='defense' else enemy;amount=min(p['amount']*count,len(libraries[target]))
    graves[target].extend(libraries[target][:amount]);del libraries[target][:amount]
   actions += stats()+[step('assert_life',player=pl,value=life[pl]) for pl in (0,1)]
   if value=='source_destroyed_after' and entities['source']['zone']=='battlefield':
    actions += [step('destroy',ref='source')];move('source','graveyard');attackers=[ref for ref in attackers if ref!='source']
   if value in ('unblocked','blocked','cleanup','ground_block_denied','reach_block_allowed'):
    actions += [step('begin_blockers')]
    if value in ('blocked','reach_block_allowed') and attackers:
     attacked=attackers[0];actions += [step('block',ref='blocker',other=attacked,value=1)];entities['blocker']['blocking_count']=1
    if value=='ground_block_denied' and attackers:actions += [step('block',ref='enemy',other=attackers[0],value=0)]
    surviving_pair=False
    if value in ('blocked','reach_block_allowed') and attackers:
     attacking=entities[attackers[0]];blocking=entities['blocker']
     surviving_pair=attacking['toughness']>blocking['power'] and blocking['toughness']>attacking['power']
    actions += [step('advance_combat')]
    if surviving_pair:actions += [observe('blocker','blocking_count',1)]
    actions += [step('combat_damage'),step('end_combat')]
    for ref in attackers:
     e=entities[ref]
     if value in ('blocked','reach_block_allowed') and ref==attackers[0]:
      blocker=entities['blocker'];e['damage']+=blocker['power'];blocker['damage']+=e['power']
      if ref in sources and 'Trample' in p['keywords']:life[1-actor]-=max(0,e['power']-blocker['toughness'])
      if e['damage']>=e['toughness']:move(ref,'graveyard')
      if blocker['damage']>=blocker['toughness']:move('blocker','graveyard')
     else:life[1-actor]-=e['power']
    for e in entities.values():e['attacking']=False;e['blocking_count']=0
    # This observer counts retained blocker assignments even after combat.
    # Assert its live value above; post-combat rules are shown by cleared
    # attacking state, real damage and zone receipts instead of stale indexes.
    if surviving_pair:entities['blocker']['historical_blocking_assignment']=True
    if value=='cleanup':
     actions += [step('cleanup'),step('refresh')]
     for e in entities.values():e['power']=int(o[e['name']]['power'])+e['plus1_counters'];e['toughness']=int(o[e['name']]['toughness'])+e['plus1_counters'];e['damage']=0
  checks=stats()+[step('assert_stack',value=0),step('assert_choices_consumed')]
  universe={item['name'] for item in setup}
  for player in (0,1):
   battlefield=[e['name'] for e in entities.values() if e['zone']=='battlefield' and e['player']==player]+[item['name'] for item in setup if item['zone']=='battlefield' and item['player']==player and 'Creature' not in o[item['name']]['type_line']]
   checks += [step('assert_life',player=player,value=life[player])]+[step('assert_mana',player=player,color=color,value=amount) for color,amount in enumerate(mana[player])]
   for zone,names in (('battlefield',battlefield),('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libraries[player])):
    checks += [step('assert_zone',player=player,zone=zone,value=len(names))]
    if zone=='library':checks += [step('assert_zone_card',player=player,zone=zone,index=index,value=rows[nm]['csvid']) for index,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=rows[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  seq=pre+actions+checks
  if value in ('ready_snapshot','declaration_snapshot'):
   cut=declaration_cut if value=='declaration_snapshot' else 0;tail=actions[cut:]+checks
   seq=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if value=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'AttackValueTriggerRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,
   _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['planeswalker attacks','multiplayer','multiple combat phases','independent passive keyword mechanics']+(['duplicate human trigger-presentation sources'] if p.get('trigger_priority_players') else [])+(['forced attack-declaration UI enforcement; authored declarations are legal'] if p.get('mandatory_attack') else []))))
 return result


def _classify_energy(r):
 from creature_recipes import oracle_keywords
 from recipes import cost_from_oracle
 lines=r['oracle_text'].splitlines()
 entry='When this creature enters, you get {E}{E} (two energy counters).'
 if lines.count(entry)!=1:raise ValueError('reviewed complete entry-energy clause required')
 refs={'this creature',r['name'],r['name'].split(',')[0]}
 source='(?:'+'|'.join(re.escape(x) for x in sorted(refs))+')'
 prefix=rf'Whenever {source} attacks, you may pay '
 candidates=[]
 for line in lines:
  if re.fullmatch(prefix+r'\{E\}\{E\}\. If you do, put a \+1/\+1 counter on it\.',line):
   candidates.append((line,'energy_counter',2,1,0,0,False))
  if re.fullmatch(prefix+r'\{E\}\{E\}\. If you do, it gets \+2/\+2 until end of turn\.',line):
   candidates.append((line,'energy_pump',2,0,2,2,False))
  if re.fullmatch(prefix+r'\{E\}\. If you do, it gets \+1/\+1 and gains trample until end of turn\.',line):
   candidates.append((line,'energy_pump',1,0,1,1,True))
  if re.fullmatch(prefix+r'\{E\}\{E\}\. If you do, create a 1/1 colorless Servo artifact creature token\.',line):
   candidates.append((line,'energy_servo',2,1,0,0,False))
  if re.fullmatch(prefix+r'\{E\}\. If you do, another target attacking creature gains (indestructible|flying) until end of turn\.',line):
   candidates.append((line,'energy_target',1,0,0,0,False))
  if re.fullmatch(prefix+r"\{E\}\. If you do, target creature can't block this turn\.",line):
   candidates.append((line,'energy_target',1,0,0,0,False))
 if len(candidates)!=1:raise ValueError('reviewed complete self-attack energy effect required')
 line,effect,cost,amount,power,toughness,trample=candidates[0]
 remaining='\n'.join(x for x in lines if x not in (entry,line))
 # The common creature parser predates this exact ordinary-keyword reminder.
 remaining=remaining.replace('Double strike (This creature deals both first-strike and regular combat damage.)','Double strike')
 keywords=oracle_keywords(dict(r,oracle_text=remaining))
 allowed={'Flying','Vigilance'} if effect=='energy_servo' else {'Flying'} if effect=='energy_target' else {'Trample','Double strike'}
 if set(keywords)-allowed:raise ValueError('energy combat keyword needs its own receipts')
 cost_from_oracle(r)
 extra={}
 if effect=='energy_target':
  grant=re.search(r'another target attacking creature gains (indestructible|flying)',line)
  extra=dict(target_effect=grant[1] if grant else 'cannot_block',target_scope='other_attacking' if grant else 'creature')
 return dict(trigger='self',effect=effect,amount=amount,power=power,toughness=toughness,other=False,
             keywords=keywords,entry_energy=2,energy_cost=cost,grants_trample=trample,**extra)


def _energy_variants(p):
 if p['effect'] in ('energy_servo','energy_target'):return _resource_variants(p)
 values=['entry','insufficient_mana','wrong_color','fresh_attack','effect','no_attack','other_attack','opponent_attack',
         'tapped','source_removed_before','unblocked','blocked','cleanup','source_destroyed_after','depleted','repeat',
         'two_sources','ready_snapshot','declaration_snapshot','resolved_snapshot','bare_zones']
 if p['grants_trample']:values+=['trample_expiry']
 return [value+'_'+str(owner) for owner in (0,1) for value in values]+['decline_0','decline_then_accept_0']


def _expand_energy(name,csvid,d,p,selected,rows,o):
 if p['effect'] in ('energy_servo','energy_target'):return _expand_resources(name,csvid,d,p,selected,rows,o)
 from recipes import card,step,observe,cost_from_oracle
 if not selected or len(set(selected))!=len(selected) or any(v not in _energy_variants(p) for v in selected):
  raise ValueError('explicit unique reviewed energy-attack variants required')
 validate_witnesses(o,rows);result=[]
 for variant in selected:
  value,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner
  setup=[];actions=[];entities={};zones={z:[[],[]] for z in ('battlefield','hand','graveyard','exile','library')}
  energy=[0,0];life=[20,20];mana=[[0]*6,[0]*6];current=owner;ready_cut=declaration_cut=0
  def add(nm,ref,pl,zone='battlefield',source=False):
   setup.append(card(nm,ref,pl,zone));zones[zone][pl].append(nm)
   if 'Creature' in o[nm]['type_line']:
    entities[ref]=dict(name=nm,player=pl,zone=zone,source=source,sick=False,tapped=False,attacking=False,
                       damage=0,counters=0,pump=0,pump_t=0,trample=False)
  def move(ref,zone):
   e=entities[ref];zones[e['zone']][e['player']].remove(e['name']);zones[zone][e['player']].append(e['name']);e['zone']=zone
   e['attacking']=False;e['tapped']=False
  def power(ref):
   e=entities[ref];return int(o[e['name']]['power'])+e['counters']+e['pump']
  def toughness(ref):
   e=entities[ref];return int(o[e['name']]['toughness'])+e['counters']+e['pump_t']
  def receipts():
   seq=[step('assert_stack',value=0),step('assert_choices_consumed')]
   for ref,e in entities.items():
    # Zone refs cease being instances after a card dies; use exact zone receipts.
    if e['zone']!='battlefield':continue
    seq += [observe(ref,'in_play',1),observe(ref,'power',power(ref)),observe(ref,'toughness',toughness(ref))]
    seq += [observe(ref,key,int(e[key])) for key in ('sick','tapped','attacking','damage')]
    seq += [observe(ref,'plus1_counters',e['counters'])]
   universe=sorted({item['name'] for item in setup})
   for pl in (0,1):
    seq += [step('assert_life',player=pl,value=life[pl]),step('assert_energy',player=pl,value=energy[pl])]
    seq += [step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl])]
    for zone,players in zones.items():
     seq += [step('assert_zone',player=pl,zone=zone,value=len(players[pl]))]
     if zone=='library':seq += [step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(players[pl])]
     else:seq += [step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=players[pl].count(nm)) for nm in universe]
   return seq
  def pool(pl,counts):
   mana[pl]=list(counts);actions.extend(step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(counts))
  def cast_source(ref,denied=None):
   price=list(p['mana'])
   if denied=='insufficient_mana':price=[0]*6
   if denied=='wrong_color':
    color=next(i for i in range(1,6) if price[i]);price[color]-=1;price[0]+=1
   pool(owner,price);actions.extend([step('cast',ref=ref,value=int(denied is None)),step('assert_choices_consumed')])
   if denied is not None:actions.extend(receipts());return
   mana[owner]=[0]*6
   actions.extend([step('assert_stack',value=1),observe(ref,'in_play',0),observe(ref,'in_hand',0)])
   actions.extend(step('assert_mana',player=owner,color=i,value=0) for i in range(6))
   actions.extend([step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed')])
   move(ref,'battlefield');entities[ref]['sick']=True;energy[owner]+=p['entry_energy'];actions.extend(receipts())
  def reset_temporary():
   for e in entities.values():e['pump']=e['pump_t']=e['damage']=0;e['trample']=False;e['attacking']=False
  def next_turn():
   nonlocal current
   if any(any(counts) for counts in mana):raise ValueError('real turn gate requires spent pools')
   actions.append(step('advance_turn'));reset_temporary();current=1-current
   actions.append(step('untap_step',player=current))
   # The real untap wrapper leaves its phase selected. Return to first main
   # for this already-current player; only advance_turn aged the creatures.
   actions.append(step('turn',player=current,value=0))
   for e in entities.values():
    if e['zone']=='battlefield' and e['player']==current:e['tapped']=False;e['sick']=False
   actions.extend(receipts())
  def age():next_turn();next_turn()
  def declaration(chosen,decline=False):
   nonlocal declaration_cut
   attackers=[];actions.append(step('begin_attackers'))
   for ref in chosen:
    e=entities[ref];legal=e['zone']=='battlefield' and e['player']==current and not e['tapped'] and not e['sick']
    actions.append(step('attack',ref=ref,value=int(legal)))
    if legal:attackers.append(ref);e['attacking']=True;e['tapped']=True
   actions.extend(receipts());declaration_cut=len(actions)
   for ref in attackers:
    e=entities[ref]
    if not e['source'] or energy[owner]<p['energy_cost'] or decline:continue
    # This is the player's optional trigger menu, not an invented energy setter.
    # Identical sources use the existing timestamp-qualified source selector.
    if owner==0:actions.append(step('choose_trigger_source',player=owner,ref=ref))
    energy[owner]-=p['energy_cost']
    if p['effect']=='energy_counter':e['counters']+=p['amount']
    else:e['pump']+=p['power'];e['pump_t']+=p['toughness'];e['trample']=p['grants_trample']
   actions.extend([step('finish_attackers'),step('assert_choices_consumed')]);actions.extend(receipts());return attackers
  def combat(attackers,blocked=False):
   actions.append(step('begin_blockers'));blocked_ref=attackers[0] if blocked and attackers else None
   blocker='blocker' if 'blocker' in entities else 'enemy'
   if blocked_ref:
    actions.extend([step('block',ref=blocker,other=blocked_ref,value=1),observe(blocker,'blocking_count',1)])
   actions.extend([step('advance_combat'),step('combat_damage'),step('end_combat')])
   for ref in attackers:
    e=entities[ref]
    if e['zone']!='battlefield':continue
    ap=power(ref);double=e['source'] and 'Double strike' in p['keywords']
    trample=e['source'] and ('Trample' in p['keywords'] or e['trample'])
    if ref!=blocked_ref:life[1-current]-=ap*(2 if double else 1);continue
    bp=power(blocker);bt=toughness(blocker);b=entities[blocker]
    if double and ap>=bt:
     b['damage']+=ap;move(blocker,'graveyard')
     if trample:life[1-current]-=max(0,ap-bt)+ap
    else:
     e['damage']+=bp;b['damage']+=ap*(2 if double else 1)
     if trample:life[1-current]-=max(0,ap*(2 if double else 1)-bt)
     if e['damage']>=toughness(ref):move(ref,'graveyard')
     if b['damage']>=toughness(blocker):move(blocker,'graveyard')
   for e in entities.values():e['attacking']=False
   actions.extend(receipts())
  add(name,'source',owner,'hand',source=True)
  if value in ('two_sources','repeat'):add(name,'copy',owner,'hand',source=True)
  add('Grizzly Bears','ally',owner);add('Hill Giant','idle',owner)
  add('Grizzly Bears','enemy',enemy);add('Craw Wurm','enemy_idle',enemy);add('Sol Ring','artifact_control',enemy)
  if value=='blocked' and 'Double strike' in p['keywords']:add('Hill Giant','blocker',enemy)
  if value=='tapped':add('Twiddle','tap_spell',owner,'hand')
  if value!='bare_zones':
   for pl in (0,1):
    add('Forest',f'hand_{pl}',pl,'hand');add('Lightning Bolt',f'grave_{pl}',pl,'graveyard');add('Island',f'exile_{pl}',pl,'exile')
    for i,nm in enumerate(('Forest','Island','Plains','Swamp','Mountain','Grizzly Bears')):add(nm,f'library_{pl}_{i}',pl,'library')
  pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=pl,color=i,value=0) for pl in (0,1) for i in range(6)]+[step('refresh')]
  actions.extend(receipts());cast_source('source',value if value in ('insufficient_mana','wrong_color') else None)
  if value not in ('entry','insufficient_mana','wrong_color'):
   if value=='two_sources':cast_source('copy')
   if value!='fresh_attack':age()
   if value=='opponent_attack':next_turn()
   if value=='tapped':
    pool(owner,cost_from_oracle(o['Twiddle']))
    actions.extend([step('choose_target',player=owner,ref='source'),step('cast',ref='tap_spell',value=1),step('choose_dialog',player=owner,value=0),step('resolve'),step('assert_choices_consumed')])
    mana[owner]=[0]*6;zones['hand'][owner].remove('Twiddle');zones['graveyard'][owner].append('Twiddle');entities['source']['tapped']=True;actions.extend(receipts())
   if value=='source_removed_before':actions.append(step('destroy',ref='source'));move('source','graveyard');actions.extend(receipts())
   ready_cut=len(actions)
   chosen=[] if value=='no_attack' else ['enemy'] if value=='opponent_attack' else ['ally'] if value in ('other_attack','source_removed_before') else ['source','copy'] if value=='two_sources' else ['source']
   attackers=declaration(chosen,decline=value in ('decline','decline_then_accept'))
   if value=='source_destroyed_after':actions.append(step('destroy',ref='source'));move('source','graveyard');attackers=[];actions.extend(receipts())
   if value in ('unblocked','blocked','cleanup','depleted','repeat','decline_then_accept','trample_expiry'):
    combat(attackers,blocked=value=='blocked')
    if value=='cleanup':actions.append(step('cleanup'));reset_temporary();actions.extend(receipts())
    if value in ('depleted','trample_expiry'):
     # Cost1 naturally takes two paid attacks; cost2 exhausts the entry grant once.
     if p['energy_cost']==1:age();combat(declaration(['source']))
     age();attackers=declaration(['source'])
     if value=='trample_expiry':combat(attackers,blocked=True)
    if value=='repeat':cast_source('copy');age();declaration(['source'])
    if value=='decline_then_accept':age();declaration(['source'])
  checks=receipts();seq=pre+actions+checks
  if value in ('ready_snapshot','declaration_snapshot'):
   cut=ready_cut if value=='ready_snapshot' else declaration_cut;tail=actions[cut:]+checks
   seq=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if value=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
  result.append(dict(name=f'AttackValueTriggerRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,
   _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],
                energy_model='actual paid entry and player-owned energy; real turns and attack payment',
                untested_components=['planeswalker attacks','multiplayer','control changes','replacement effects','AI optional-decline policy','partial positive energy below cost2','separate response window inside attack-trigger resolution'])))
 return result


def _resource_variants(p):
 values=['entry','insufficient_mana','wrong_color','fresh_attack','effect','no_attack','other_attack','opponent_attack',
         'tapped','source_removed_before','unblocked','cleanup','source_destroyed_after','depleted','repeat','two_sources',
         'ready_snapshot','declaration_snapshot','resolved_snapshot','bare_zones']
 if p['effect']=='energy_servo':values+=['token_ready_attack','token_block','token_destroy','token_bounce','token_exile']
 else:
  values+=['effect_blocked','expiry','target_alternate']
  if p['target_scope']=='other_attacking':values+=['no_eligible_target']
  else:values+=['target_own']
 if 'Flying' in p['keywords']:values+=['ground_block_denied','reach_block_allowed','flying_block_allowed']
 return [v+'_'+str(owner) for owner in (0,1) for v in values]+['decline_0','decline_then_accept_0']


class _AttackResourceWorld:
 """Player resource, chosen recipient and created-token ledgers; no state setters."""
 def __init__(self,name,p,owner,rows,o):
  self.name=name;self.p=p;self.owner=owner;self.current=owner;self.rows=rows;self.o=o
  self.setup=[];self.steps=[];self.entities={};self.tokens=[];self.bound=set()
  self.zones={z:[[],[]] for z in ('battlefield','hand','graveyard','exile','library')}
  self.energy=[0,0];self.life=[20,20];self.mana=[[0]*6,[0]*6]
 def emit(self,op,**kw):
  from recipes import step
  self.steps.append(step(op,**kw))
 def entity(self,nm,ref,pl,zone,source=False,token=False):
  r=self.o[nm]
  self.entities[ref]=dict(name=nm,player=pl,zone=zone,source=source,token=token,power=int(r['power']),toughness=int(r['toughness']),
                          sick=token,tapped=False,attacking=False,damage=0,keys=set(self.p['keywords'] if source else r.get('keywords',[])),grants=set())
 def add(self,nm,ref,pl,zone='battlefield',source=False):
  from recipes import card
  self.setup.append(card(nm,ref,pl,zone));self.zones[zone][pl].append(nm)
  if 'Creature' in self.o[nm]['type_line']:self.entity(nm,ref,pl,zone,source=source)
 def leave(self,ref,zone):
  e=self.entities[ref];self.zones[e['zone']][e['player']].remove(e['name'])
  if not e['token']:self.zones[zone][e['player']].append(e['name'])
  e['zone']='ceased' if e['token'] else zone;e['attacking']=False;e['tapped']=False
 def arrive(self,ref):
  e=self.entities[ref];self.zones[e['zone']][e['player']].remove(e['name']);self.zones['battlefield'][e['player']].append(e['name'])
  e['zone']='battlefield';e['sick']=True;e['tapped']=False
 def create_servo(self):
  for _ in range(self.p['amount']):
   ref=f'servo_{len(self.tokens)}';self.tokens.append(ref)
   self.entity('Servo',ref,self.owner,'battlefield',token=True);self.zones['battlefield'][self.owner].append('Servo')
 def has(self,ref,key):
  e=self.entities[ref];return key.lower() in {x.lower() for x in e['keys']|e['grants']}
 def receipts(self):
  for ref in self.tokens:
   e=self.entities[ref]
   if ref not in self.bound and e['zone']=='battlefield':
    live=[r for r in self.tokens if self.entities[r]['zone']=='battlefield']
    self.emit('bind_permanent_copy',player=e['player'],value=self.rows['Servo']['csvid'],index=live.index(ref),ref=ref);self.bound.add(ref)
  self.emit('assert_stack',value=0);self.emit('assert_choices_consumed')
  for ref,e in self.entities.items():
   if e['zone']!='battlefield':continue
   for prop in ('in_play','power','toughness','sick','tapped','attacking','damage','plus1_counters'):
    self.emit('assert_card',ref=ref,property=prop,value=1 if prop=='in_play' else 0 if prop=='plus1_counters' else int(e[prop]))
  if self.p['effect']=='energy_servo' and 'Servo' in self.zones['battlefield'][self.owner]:
   for prop,val in (('token',1),('artifact',1),('color',0),('subtype_Servo',1)):
    self.emit('assert_matching_permanents',player=self.owner,index=self.rows['Servo']['csvid'],property=prop,value=val)
  universe={c['name'] for c in self.setup}|({'Servo'} if self.p['effect']=='energy_servo' else set())
  for pl in (0,1):
   self.emit('assert_life',player=pl,value=self.life[pl]);self.emit('assert_energy',player=pl,value=self.energy[pl])
   for color,n in enumerate(self.mana[pl]):self.emit('assert_mana',player=pl,color=color,value=n)
   for zone,players in self.zones.items():
    self.emit('assert_zone',player=pl,zone=zone,value=len(players[pl]))
    if zone=='library':
     for i,nm in enumerate(players[pl]):self.emit('assert_zone_card',player=pl,zone=zone,index=i,value=self.rows[nm]['csvid'])
    else:
     for nm in sorted(universe):self.emit('assert_zone_multiplicity',player=pl,zone=zone,index=self.rows[nm]['csvid'],value=players[pl].count(nm))
 def pool(self,pl,counts):
  self.mana[pl]=list(counts)
  for color,n in enumerate(counts):self.emit('set_mana',player=pl,color=color,value=n)
 def cast_source(self,ref,denied=None):
  price=list(self.p['mana'])
  if denied=='insufficient_mana':price=[0]*6
  if denied=='wrong_color':
   color=next(i for i in range(1,6) if price[i]);price[color]-=1;price[0]+=1
  self.pool(self.owner,price);self.emit('cast',ref=ref,value=int(denied is None));self.emit('assert_choices_consumed')
  if denied is not None:self.receipts();return
  self.mana[self.owner]=[0]*6;self.emit('assert_stack',value=1)
  self.emit('assert_card',ref=ref,property='in_hand',value=0);self.emit('assert_card',ref=ref,property='in_play',value=0)
  for color in range(6):self.emit('assert_mana',player=self.owner,color=color,value=0)
  self.emit('resolve');self.arrive(ref);self.energy[self.owner]+=self.p['entry_energy'];self.receipts()
 def expire(self):
  for e in self.entities.values():e['damage']=0;e['attacking']=False;e['grants']=set()
 def next_turn(self):
  if any(any(x) for x in self.mana):raise ValueError('real turn gate requires spent mana')
  self.emit('advance_turn');self.expire();self.current=1-self.current
  self.emit('untap_step',player=self.current);self.emit('turn',player=self.current,value=0)
  for e in self.entities.values():
   if e['zone']=='battlefield' and e['player']==self.current:e['tapped']=False;e['sick']=False
  self.receipts()
 def age(self):self.next_turn();self.next_turn()
 def spell(self,nm,ref,pl,target,dialog=None):
  from recipes import cost_from_oracle
  self.pool(pl,cost_from_oracle(self.o[nm]));self.emit('choose_target',player=pl,ref=target);self.emit('cast',ref=ref,value=1)
  self.emit('assert_stack',value=1)
  if dialog is not None:self.emit('choose_dialog',player=pl,value=dialog)
  self.emit('resolve');self.emit('assert_choices_consumed');self.mana[pl]=[0]*6
  self.zones['hand'][pl].remove(nm);self.zones['graveyard'][pl].append(nm)
 def declaration(self,chosen,decline=False,target='ally',probe_no_target=False):
  self.emit('begin_attackers');attackers=[]
  for ref in chosen:
   e=self.entities[ref];legal=e['zone']=='battlefield' and e['player']==self.current and not e['tapped'] and not e['sick']
   self.emit('attack',ref=ref,value=int(legal))
   if legal:attackers.append(ref);e['attacking']=True;e['tapped']=not self.has(ref,'Vigilance')
  self.receipts();cut=len(self.steps)
  paid=[]
  for ref in attackers:
   e=self.entities[ref]
   if not e['source'] or self.energy[self.owner]<self.p['energy_cost'] or decline:continue
   recipient=target if ref=='source' else 'source'
   eligible=self.p['effect']=='energy_servo' or self.p['target_scope']=='creature' or recipient in attackers and recipient!=ref
   if not eligible:
    # A targeted trigger without a legal other attacker cannot be put on stack.
    # Human probe must stay unused; AI has no optional menu to fabricate.
    if probe_no_target and self.owner==0:self.emit('choose_trigger_source',player=self.owner,ref=ref)
    continue
   if self.owner==0:self.emit('choose_trigger_source',player=self.owner,ref=ref)
   if self.p['effect']=='energy_target':self.emit('choose_target',player=self.owner,ref=recipient)
   paid.append((ref,recipient));self.energy[self.owner]-=self.p['energy_cost']
  self.emit('finish_attackers')
  if probe_no_target and self.owner==0:self.emit('assert_unused_choices',value=1)
  else:self.emit('assert_choices_consumed')
  for ref,recipient in paid:
   if self.p['effect']=='energy_servo':self.create_servo()
   else:self.entities[recipient]['grants'].add(self.p['target_effect'])
  self.receipts();return attackers,cut
 def combat(self,attackers,pairs=()):
  self.emit('begin_blockers');blocked={}
  for blocker,attacker in pairs:
   b=self.entities[blocker];a=self.entities[attacker]
   legal=b['zone']=='battlefield' and a['zone']=='battlefield' and b['player']!=a['player'] and not b['tapped'] and not self.has(blocker,'cannot_block') and (not self.has(attacker,'Flying') or self.has(blocker,'Flying') or self.has(blocker,'Reach'))
   self.emit('block',ref=blocker,other=attacker,value=int(legal))
   if legal:
    if attacker in blocked:raise ValueError('one real blocker per attacker in reviewed resource model')
    blocked[attacker]=blocker;self.emit('assert_card',ref=blocker,property='blocking_count',value=1)
  self.emit('advance_combat');self.emit('combat_damage');self.emit('end_combat')
  for ref in attackers:
   a=self.entities[ref]
   if a['zone']!='battlefield':continue
   if ref not in blocked:self.life[1-self.current]-=a['power'];continue
   blocker=blocked[ref];b=self.entities[blocker];a['damage']+=b['power'];b['damage']+=a['power']
   if a['damage']>=a['toughness'] and not self.has(ref,'indestructible'):self.leave(ref,'graveyard')
   if b['damage']>=b['toughness'] and not self.has(blocker,'indestructible'):self.leave(blocker,'graveyard')
  for e in self.entities.values():e['attacking']=False
  self.receipts()


def _expand_resources(name,csvid,d,p,selected,rows,o):
 from recipes import step
 if not selected or len(set(selected))!=len(selected) or any(v not in _resource_variants(p) for v in selected):
  raise ValueError('explicit unique reviewed token/target energy variants required')
 validate_witnesses(o,rows)
 if p['effect']=='energy_servo':
  r=o.get('Servo',{})
  if (r.get('layout')!='token' or r.get('type_line')!='Token Artifact Creature — Servo' or r.get('power')!='1' or r.get('toughness')!='1'
      or r.get('colors')!=[] or r.get('keywords')!=[] or r.get('oracle_text')!='' or 'Servo' not in _pins().get('tokens',{})):
   raise ValueError('independent ordinary1/1 colorless artifact Servo token witness required')
 result=[]
 for variant in selected:
  value,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner
  w=_AttackResourceWorld(name,p,owner,rows,o)
  w.add(name,'source',owner,'hand',source=True)
  if value in ('two_sources','repeat'):w.add(name,'copy',owner,'hand',source=True)
  w.add('Grizzly Bears','ally',owner);w.add('Hill Giant','idle',owner)
  w.add('Grizzly Bears','enemy',enemy);w.add('Craw Wurm','enemy_idle',enemy);w.add('Sol Ring','artifact_control',enemy)
  w.add('Hill Giant','giant',enemy);w.add('Giant Spider','reach',enemy);w.add('Air Elemental','flying',enemy)
  if value=='tapped':w.add('Twiddle','tap_spell',owner,'hand')
  if value=='token_bounce':w.add('Unsummon','bounce',owner,'hand')
  if value=='token_exile':w.add('Swords to Plowshares','exile_spell',owner,'hand')
  if value=='expiry' and p.get('target_effect')=='indestructible':w.add('Lightning Bolt','expiry_bolt',enemy,'hand')
  if value!='bare_zones':
   for pl in (0,1):
    w.add('Forest',f'hand_{pl}',pl,'hand');w.add('Lightning Bolt',f'grave_{pl}',pl,'graveyard');w.add('Island',f'exile_{pl}',pl,'exile')
    for i,nm in enumerate(('Forest','Island','Plains','Swamp','Mountain','Grizzly Bears')):w.add(nm,f'library_{pl}_{i}',pl,'library')
  w.emit('turn',player=owner,value=0);w.pool(0,[0]*6);w.pool(1,[0]*6);w.emit('refresh');w.receipts()
  w.cast_source('source',value if value in ('insufficient_mana','wrong_color') else None)
  ready_cut=declaration_cut=0
  if value not in ('entry','insufficient_mana','wrong_color'):
   if value=='two_sources':w.cast_source('copy')
   if value!='fresh_attack':w.age()
   if value=='opponent_attack':w.next_turn()
   if value=='tapped':w.spell('Twiddle','tap_spell',owner,'source',dialog=0);w.entities['source']['tapped']=True;w.receipts()
   if value=='source_removed_before':w.emit('destroy',ref='source');w.leave('source','graveyard');w.receipts()
   ready_cut=len(w.steps)
   chosen=[] if value=='no_attack' else ['enemy'] if value=='opponent_attack' else ['ally'] if value in ('other_attack','source_removed_before') else ['source','copy'] if value=='two_sources' else ['source']
   if (p.get('target_scope')=='other_attacking' and value not in ('no_attack','other_attack','opponent_attack','no_eligible_target','source_removed_before')
       and 'ally' not in chosen):chosen.append('ally')
   if value=='target_alternate':chosen+=['idle'] if p['target_scope']=='other_attacking' else []
   if value=='target_own' and 'ally' not in chosen:chosen.append('ally')
   target=('idle' if p['target_scope']=='other_attacking' else 'enemy_idle') if value=='target_alternate' else 'ally' if p.get('target_scope')=='other_attacking' or value=='target_own' else 'enemy'
   attackers,declaration_cut=w.declaration(chosen,decline=value in ('decline','decline_then_accept'),target=target,probe_no_target=value=='no_eligible_target')
   if value=='source_destroyed_after':w.emit('destroy',ref='source');w.leave('source','graveyard');w.receipts()
   if value in ('unblocked','cleanup','depleted','repeat','decline_then_accept','token_ready_attack','token_block','expiry','effect_blocked','target_alternate','target_own','ground_block_denied','reach_block_allowed','flying_block_allowed'):
    pairs=[]
    if value in ('effect_blocked','expiry','target_alternate','target_own') and p['effect']=='energy_target':
     if p['target_effect']=='indestructible':pairs=[('giant',target)]
     elif p['target_effect']=='flying':pairs=[('enemy',target)]
     else:pairs=[(target,'source')]+([('enemy_idle' if target=='enemy' else 'enemy','source')] if value in ('effect_blocked','target_alternate','target_own') else [])
    if value in ('ground_block_denied','reach_block_allowed','flying_block_allowed'):
     pairs=[({'ground_block_denied':'enemy','reach_block_allowed':'reach','flying_block_allowed':'flying'}[value],'source')]
    w.combat(attackers,pairs)
    if value=='cleanup':w.emit('cleanup');w.expire();w.receipts()
    if value in ('depleted','token_ready_attack'):
     if value=='depleted' and p['energy_cost']==1:
      w.age();again=['source','ally'] if p.get('target_scope')=='other_attacking' else ['source'];w.combat(w.declaration(again,target=target)[0])
     w.age();again=['servo_0'] if value=='token_ready_attack' else ['source','ally'] if p.get('target_scope')=='other_attacking' else ['source']
     a,_=w.declaration(again,target=target)
     if value=='token_ready_attack':w.combat(a,[('enemy','servo_0')])
    if value=='token_block':
     w.next_turn();a,_=w.declaration(['enemy']);w.combat(a,[('servo_0','enemy')])
    if value=='repeat':w.cast_source('copy');w.age();w.declaration(['source','ally'] if p.get('target_scope')=='other_attacking' else ['source'],target=target)
    if value=='decline_then_accept':w.age();w.declaration(['source','ally'] if p.get('target_scope')=='other_attacking' else ['source'],target=target)
    if value=='expiry':
     if p['target_effect']=='indestructible':
      w.next_turn();w.spell('Lightning Bolt','expiry_bolt',enemy,target);w.entities[target]['damage']+=3
      if w.entities[target]['damage']>=w.entities[target]['toughness']:w.leave(target,'graveyard')
      w.receipts()
     elif p['target_effect']=='flying':w.age();a,_=w.declaration(['ally']);w.combat(a,[('enemy','ally')])
     else:w.age();a,_=w.declaration(['source'],target='enemy_idle');w.combat(a,[('enemy','source')])
   if value in ('token_destroy','token_bounce','token_exile'):
    if value=='token_destroy':w.emit('destroy',ref='servo_0');w.emit('refresh');w.leave('servo_0','graveyard')
    elif value=='token_bounce':w.spell('Unsummon','bounce',owner,'servo_0');w.leave('servo_0','hand')
    else:w.spell('Swords to Plowshares','exile_spell',owner,'servo_0');w.life[owner]+=1;w.leave('servo_0','exile')
    w.receipts()
  w.receipts();seq=w.steps
  if value in ('ready_snapshot','declaration_snapshot'):
   cut=ready_cut if value=='ready_snapshot' else declaration_cut;tail=seq[cut:]
   seq=seq[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if value=='resolved_snapshot':
   start=len(w.steps);w.receipts();checks=w.steps[start:];seq=w.steps[:start]+[step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+checks+[step('assert_recorded_state')]
  result.append(dict(name=f'AttackValueTriggerRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=seq,
   _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],
    resource_model='real paid entry/energy/turns; created-token and chosen-recipient ledgers',
    untested_components=['AI optional-decline policy','exact rejected-target-filter introspection','responses inside attack-trigger resolution','control changes','replacement effects','multiplayer'])))
 return result


def _classify_paid(r):
 from creature_recipes import oracle_keywords
 from recipes import cost_from_oracle
 lines=r['oracle_text'].splitlines();refs={'this creature',r['name'],r['name'].split(',')[0]}
 source='(?:'+'|'.join(re.escape(x) for x in sorted(refs))+')'
 candidates=[]
 for line in lines:
  match=re.fullmatch(rf'Whenever {source} attacks, you may pay \{{([RWC])\}}\. If you do, (target creature can\x27t block this turn\.|tap target creature an opponent controls\.|another target creature gains flying until end of turn\.)(?: \(\{{C\}} represents colorless mana\.\))?',line)
  if match:
   symbol,clause=match.groups()
   if ('({C} represents colorless mana.)' in line)!=(symbol=='C'):raise ValueError('whole true-colorless reminder required')
   effect,scope=('cannot_block','creature') if clause.startswith('target creature') else ('tap','opponent_creature') if clause.startswith('tap ') else ('flying','another_creature')
   candidates.append((line,symbol,effect,scope))
 if len(candidates)!=1:raise ValueError('one whole reviewed paid attack-target clause required')
 line,symbol,effect,scope=candidates[0];passive=[];extra={}
 if 'Devoid (This card has no color.)' in lines:
  if r.get('colors')!=[] or 'Devoid' not in r.get('keywords',[]):raise ValueError('whole printed Devoid color required')
  passive=['Devoid (This card has no color.)'];extra['devoid']=True
 stripped=dict(r,oracle_text='\n'.join(x for x in lines if x!=line and x not in passive),keywords=[k for k in r.get('keywords',[]) if k!='Devoid'])
 keywords=oracle_keywords(stripped)
 if set(keywords)-{'Flying'}:raise ValueError('paid attack passive needs separate combat receipts')
 cost_from_oracle(r)
 return dict(trigger='self',effect='paid_target',amount=0,power=0,toughness=0,other=False,keywords=keywords,
             payment_symbol=symbol,payment_amount=1,requires_colorless=symbol=='C',target_effect=effect,target_scope=scope,**extra)


def _paid_variants(p):
 values=['entry','insufficient_cast_mana','wrong_cast_color','fresh_attack','effect','insufficient_payment','wrong_payment_color',
         'no_attack','other_attack','opponent_attack','tapped','source_removed_before','unblocked','effect_blocked','expiry',
         'target_alternate','two_sources','ready_snapshot','declaration_snapshot','resolved_snapshot','bare_zones',
         'source_destroyed_after','payment_extra']
 if p['requires_colorless']:
  values.remove('wrong_payment_color');values+=['colored_instead_C','idle_recipient','opponent_recipient','no_eligible_target']
 if p['target_scope']=='creature':values+=['own_recipient','self_recipient']
 if 'Flying' in p['keywords']:values+=['ground_block_denied','reach_block_allowed','flying_block_allowed']
 return [v+'_'+str(owner) for owner in (0,1) for v in values]+['payment_cancel_0','trigger_decline_0']


class _PaidAttackWorld(_AttackResourceWorld):
 """Reuse the resource/recipient ledger with actual resolving mana payments."""
 def receipts(self):
  super().receipts()
  if self.p.get('devoid') and any(e['source'] and e['zone']=='battlefield' for e in self.entities.values()):
   self.emit('assert_matching_permanents',player=self.owner,index=self.rows[self.name]['csvid'],property='color',value=0)
   if 'idle' in self.entities and self.entities['idle']['zone']=='battlefield':
    self.emit('assert_matching_permanents',player=self.owner,index=self.rows['Hill Giant']['csvid'],property='color',value=16)
 def legal_recipients(self,source):
  return [ref for ref,e in self.entities.items() if e['zone']=='battlefield'
          and (self.p['target_scope']!='another_creature' or ref!=source)
          and (self.p['target_scope']!='opponent_creature' or e['player']!=self.owner)]
 def declaration(self,chosen,mode='effect',target='enemy'):
  self.emit('begin_attackers');attackers=[]
  for ref in chosen:
   e=self.entities[ref];legal=e['zone']=='battlefield' and e['player']==self.current and not e['tapped'] and not e['sick']
   self.emit('attack',ref=ref,value=int(legal))
   if legal:attackers.append(ref);e['attacking']=True;e['tapped']=not self.has(ref,'Vigilance')
  self.receipts();cut=len(self.steps);paid=[];unused=0;color=self.p['attack_mana'].index(1)
  for ref in attackers:
   e=self.entities[ref]
   if not e['source'] or mode=='trigger_decline':continue
   recipients=self.legal_recipients(ref)
   if not recipients:
    if self.owner in self.p['trigger_priority_players']:self.emit('choose_trigger_source',player=self.owner,ref=ref);unused+=1
    continue
   if self.owner in self.p['trigger_priority_players']:self.emit('choose_trigger_source',player=self.owner,ref=ref)
   affordable=self.mana[self.owner][color]>=1
   if mode=='payment_cancel':self.emit('choose_cancel_mana',player=self.owner);continue
   if mode=='colored_instead_C':
    # Accept the outer trigger and the actual mana-selection input. Consuming
    # that input is compatible with rejecting/refunding an invalid payment;
    # retaining colored mana and producing no effect are Oracle expectations.
    if self.owner==0:self.emit('choose_mana',player=self.owner,color=4)
    continue
   if not affordable:
    if self.owner==0 and self.p['payment_gate']=='unconditional':self.emit('choose_cancel_mana',player=self.owner)
    continue
   recipient=target if ref=='source' else ('source' if self.p['target_scope']=='another_creature' else target)
   if recipient not in recipients:raise ValueError('actual paid attack recipient must meet complete Oracle permission')
   self.emit('choose_target',player=self.owner,ref=recipient);paid.append(recipient);self.mana[self.owner][color]-=1
  self.emit('finish_attackers')
  if unused:self.emit('assert_unused_choices',value=unused)
  else:self.emit('assert_choices_consumed')
  for ref in paid:
   if self.p['target_effect']=='tap':self.entities[ref]['tapped']=True
   else:self.entities[ref]['grants'].add(self.p['target_effect'])
  self.receipts();return attackers,cut


def _expand_paid(name,csvid,d,p,selected,rows,o):
 from recipes import step
 if not selected or len(set(selected))!=len(selected) or any(v not in _paid_variants(p) for v in selected):
  raise ValueError('explicit unique paid-mana attack variants required')
 validate_witnesses(o,rows);result=[]
 for variant in selected:
  value,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner
  w=_PaidAttackWorld(name,p,owner,rows,o);w.add(name,'source',owner,'hand',source=True)
  if value=='two_sources':w.add(name,'copy',owner,'hand',source=True)
  if value!='no_eligible_target':
   w.add('Grizzly Bears','ally',owner);w.add('Hill Giant','idle',owner)
   w.add('Grizzly Bears','enemy',enemy);w.add('Craw Wurm','enemy_idle',enemy)
   w.add('Giant Spider','reach',enemy);w.add('Air Elemental','flying',enemy)
  w.add('Sol Ring','artifact_control',enemy)
  if value=='payment_cancel':
   w.add({'R':'Mountain','W':'Plains','C':'Sol Ring'}[p['payment_symbol']],'payment_source',owner)
  if value=='tapped':w.add('Twiddle','tap_spell',owner,'hand')
  if value!='bare_zones':
   for pl in (0,1):
    w.add('Forest',f'hand_{pl}',pl,'hand');w.add('Lightning Bolt',f'grave_{pl}',pl,'graveyard');w.add('Island',f'exile_{pl}',pl,'exile')
    for i,nm in enumerate(('Forest','Island','Plains','Swamp','Mountain','Grizzly Bears')):w.add(nm,f'library_{pl}_{i}',pl,'library')
  w.emit('turn',player=owner,value=0);w.pool(0,[0]*6);w.pool(1,[0]*6);w.emit('refresh');w.receipts()
  denied='insufficient_mana' if value=='insufficient_cast_mana' else 'wrong_color' if value=='wrong_cast_color' else None
  w.cast_source('source',denied);ready_cut=declaration_cut=0
  if value not in ('entry','insufficient_cast_mana','wrong_cast_color'):
   if value=='two_sources':w.cast_source('copy')
   if value!='fresh_attack':w.age()
   if value=='opponent_attack':w.next_turn()
   if value=='tapped':w.spell('Twiddle','tap_spell',owner,'source',dialog=0);w.entities['source']['tapped']=True;w.receipts()
   if value=='source_removed_before':w.emit('destroy',ref='source');w.leave('source','graveyard');w.receipts()
   funding=list(p['attack_mana'])
   if value in ('insufficient_payment','payment_cancel'):funding=[0]*6
   if value in ('wrong_payment_color','colored_instead_C'):
    funding=[0]*6;funding[4 if p['payment_symbol']!='R' else 5]=1
   if value in ('payment_extra','two_sources'):funding=[n*2 for n in funding]
   w.pool(owner,funding)
   if value=='payment_cancel':
    w.emit('require_can_activate',ref='payment_source',value=1);w.emit('assert_card',ref='payment_source',property='tapped',value=0)
   ready_cut=len(w.steps)
   chosen=[] if value=='no_attack' else ['enemy'] if value=='opponent_attack' else ['ally'] if value in ('other_attack','source_removed_before') else ['source','copy'] if value=='two_sources' else ['source']
   if p['target_effect']=='flying' and value not in ('no_attack','other_attack','opponent_attack','source_removed_before','idle_recipient','opponent_recipient','no_eligible_target') and 'ally' not in chosen:chosen.append('ally')
   target='source' if value=='self_recipient' else 'ally' if value=='own_recipient' or p['target_effect']=='flying' and value!='opponent_recipient' else 'enemy_idle' if value=='target_alternate' else 'enemy'
   if value=='target_alternate' and p['target_scope']=='another_creature':target='idle'
   if value=='no_eligible_target':target='source'
   attackers,declaration_cut=w.declaration(chosen,mode=value,target=target)
   if value=='payment_cancel':w.emit('assert_card',ref='payment_source',property='tapped',value=0)
   if value=='source_destroyed_after':w.emit('destroy',ref='source');w.leave('source','graveyard');w.receipts()
   if value in ('unblocked','effect_blocked','expiry','opponent_recipient','own_recipient','self_recipient','ground_block_denied','reach_block_allowed','flying_block_allowed'):
    pairs=[]
    if value in ('effect_blocked','expiry'):
     pairs=[('enemy','ally' if p['target_effect']=='flying' else 'source')]
     if value=='effect_blocked' and p['target_effect']=='cannot_block':pairs+=[('enemy_idle','source')]
    if value=='opponent_recipient':pairs=[('enemy','source')]
    if value in ('own_recipient','self_recipient'):pairs=[('enemy','source')]
    if value in ('ground_block_denied','reach_block_allowed','flying_block_allowed'):
     pairs=[({'ground_block_denied':'enemy','reach_block_allowed':'reach','flying_block_allowed':'flying'}[value],'source')]
    w.combat(attackers,pairs)
    if value=='expiry':
     if p['target_effect']=='tap':
      w.emit('cleanup');w.expire();w.receipts();w.next_turn();a,_=w.declaration(['enemy']);w.combat(a)
     elif p['target_effect']=='flying':w.age();a,_=w.declaration(['ally']);w.combat(a,[('enemy','ally')])
     else:w.age();w.pool(owner,p['attack_mana']);a,_=w.declaration(['source'],target='enemy_idle');w.combat(a,[('enemy','source')])
  w.receipts();seq=w.steps
  if value in ('ready_snapshot','declaration_snapshot'):
   cut=ready_cut if value=='ready_snapshot' else declaration_cut;tail=seq[cut:]
   seq=seq[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if value=='resolved_snapshot':
   start=len(w.steps);w.receipts();checks=w.steps[start:];seq=w.steps[:start]+[step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+checks+[step('assert_recorded_state')]
  result.append(dict(name=f'AttackValueTriggerRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=seq,
   _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],
    resource_model='real cast/turns and resolving R/W/trueC payment; chosen-recipient ledger',
    untested_components=['AI optional-decline and payment-cancellation policy','manual positive mana-source selection','exact rejected-target-filter introspection','responses inside attack-trigger resolution','control changes','replacement effects','multiplayer'])))
 return result
