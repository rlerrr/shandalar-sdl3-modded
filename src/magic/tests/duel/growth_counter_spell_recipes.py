"""Pinned fixed +1/+1 counter spells; counters are always earned by real casts."""
from collections import Counter
from grant_recipes import HAND, EXILE, LIBRARY
import copy
import json
import re
from pathlib import Path

EXTENSION_PINS=json.loads((Path(__file__).parent/'data/growth_counter_spell_extension_witnesses.json').read_text(encoding='utf8'))
LIFECYCLE_PINS=dict(profiles=EXTENSION_PINS['lifecycle_profiles'],witnesses=EXTENSION_PINS['cards'])
COMBAT_PINS=EXTENSION_PINS.get('combat_profiles',{})
ACTION_PINS=EXTENSION_PINS.get('action_profiles',{})

WORDS={1:'a',2:'two',3:'three',4:'four',5:'five'}
TAILS={
 'none':'',
 'cycling':'\nCycling {4}{G}{G} ({4}{G}{G}, Discard this card: Draw a card.)\nWhen you cycle this card, you may put four +1/+1 counters on target creature.',
 'flashback_white':'\nFlashback {1}{W} (You may cast this card from your graveyard for its flashback cost. Then exile it.)',
 'graveyard_double':' If this spell was cast from a graveyard, put ten +1/+1 counters on that creature instead.\nFlashback {5}{G}{G} (You may cast this card from your graveyard for its flashback cost. Then exile it.)',
 'awaken':'\nAwaken 4—{6}{G} (If you cast this spell for {6}{G}, also put four +1/+1 counters on target land you control and it becomes a 0/0 Elemental creature with haste. It\'s still a land.)',
 'reach':' That creature gains reach until end of turn. (It can block creatures with flying.)',
}
SPELLS={
 'Counterspell':('{U}{U}','Counter target spell.'),
 'Unsummon':('{U}',"Return target creature to its owner's hand."),
 'Giant Growth':('{G}','Target creature gets +3/+3 until end of turn.'),
 'Burst of Strength':('{G}','Put a +1/+1 counter on target creature and untap it.'),
 'Arbor Armament':('{G}','Put a +1/+1 counter on target creature. That creature gains reach until end of turn. (It can block creatures with flying.)'),
 "Ranger's Guile":('{G}',"Target creature you control gets +1/+1 and gains hexproof until end of turn. (It can't be the target of spells or abilities your opponents control.)"),
}
WITNESSES={'Savannah Lions':('2','1','Creature — Cat',''),
 'Grizzly Bears':('2','2','Creature — Bear',''),
 'Scaled Wurm':('7','6','Creature — Wurm',''),
 'Sol Ring':(None,None,'Artifact','{T}: Add {C}{C}.')}

def spec(d,o):
 if isinstance(d.get('params'),dict) and d['params'].get('extension')=='reviewed_counter_actions':return _growth_action_spec(d,o)
 if isinstance(d.get('params'),dict) and d['params'].get('extension')=='reviewed_counter_combat':return _growth_combat_spec(d,o)
 if isinstance(d.get('params'),dict) and d['params'].get('extension')=='reviewed_lifecycle':return _growth_lifecycle_spec(d,o)
 if isinstance(d.get('params'),dict) and 'extension' in d['params']:return _growth_extension_spec(d,o)
 from recipes import cost_from_oracle
 if (not o or o.get('layout')!='normal' or o.get('card_faces') or o.get('selected_face')
     or o.get('type_line') not in ('Instant','Sorcery') or d.get('oracle_text')!=o.get('oracle_text') or 'variants' not in d):
  raise ValueError('growth requires pinned whole text of a normal instant/sorcery and explicit variants')
 p=d.get('params')
 keys={'scope','amount','draw','untap','condition','followup','tail'}
 if not isinstance(p,dict) or set(p)!=keys:raise ValueError('explicit complete growth parameters required')
 if p['scope'] not in ('single','permanent','two','repeat','up_to_two','support','team','bolster'):raise ValueError('unsupported counter recipient scope')
 if type(p['amount']) is not int or p['amount'] not in WORDS:raise ValueError('bounded fixed counter amount required')
 if type(p['draw']) is not int or p['draw'] not in (0,1) or type(p['untap']) is not bool:raise ValueError('explicit draw and untap required')
 if p['condition'] not in ('none','morbid','delirium') or p['followup'] not in ('none','countered_team','untap_countered_team') or p['tail'] not in TAILS:raise ValueError('unreviewed conditional or additional component')
 n=p['amount'];counter=f"{WORDS[n]} +1/+1 counter{'s' if n!=1 else ''}"
 stems={
  'single':f'Put {counter} on target creature'+(' and untap it' if p['untap'] else '')+'.',
  'permanent':f'Put {counter} on target permanent.',
  'two':f'Put {counter} on target creature and {counter} on another target creature.',
  'repeat':f'Put {counter} on target creature.\nPut {counter} on target creature.',
  'up_to_two':f'Put {counter} on each of up to two target creatures.',
  'support':'Support 2. (Put a +1/+1 counter on each of up to two target creatures.)',
  'team':f'Put {counter} on each creature you control.',
  'bolster':f"Bolster {n}. (Choose a creature with the least toughness among creatures you control and put {counter} on it.)",
 }
 text=stems[p['scope']]
 if p['scope']!='single' and p['untap']:raise ValueError('untap only accepted in reviewed targeted stem')
 if p['scope'] in ('support','up_to_two','repeat') and n!=1:raise ValueError('reviewed multi-recipient form requires one counter')
 if p['condition']!='none':
  if p['scope']!='single' or p['untap'] or n!={'morbid':1,'delirium':2}[p['condition']]:raise ValueError('conditional amount must match reviewed stem')
  text+='\n'+{'morbid':'Morbid — Put three +1/+1 counters on that creature instead if a creature died this turn.',
   'delirium':'Delirium — Put three +1/+1 counters on that creature instead if there are four or more card types among cards in your graveyard.'}[p['condition']]
 if p['followup']=='countered_team':
  if p['scope']=='single' and n==1:text=text[:-1]+', then put a +1/+1 counter on each creature you control with a +1/+1 counter on it.'
  elif p['scope']=='bolster' and n==1:text='Bolster 1, then put a +1/+1 counter on each creature you control with a +1/+1 counter on it. (To bolster 1, choose a creature with the least toughness among creatures you control and put a +1/+1 counter on it.)'
  else:raise ValueError('unreviewed sequential counter effect')
 if p['followup']=='untap_countered_team':
  if p['scope']!='support':raise ValueError('unreviewed team untap')
  text+='\nUntap each creature you control with a +1/+1 counter on it.'
 if p['draw']:text+='\nDraw a card.'
 text+=TAILS[p['tail']]
 if o['oracle_text']!=text:raise ValueError('growth parameters do not match complete pinned Oracle text')
 compatible={'none':True,'cycling':p['scope']=='team' and n==4,'flashback_white':p['scope']=='up_to_two',
  'graveyard_double':p['scope']=='single' and n==5,'awaken':p['scope']=='permanent' and n==2,'reach':p['scope']=='single' and n==1}
 if not compatible[p['tail']]:raise ValueError('additional component incompatible with reviewed fixed cast')
 return dict(p,family='growth_counter_spell',mana=cost_from_oracle(o),instant=o['type_line']=='Instant',remaining_components=[TAILS[p['tail']].strip()] if p['tail']!='none' else [])

def variants(p):
 if p.get('_counter_action'):return _growth_action_variants(p)
 if p.get('_combat_counter'):return _growth_combat_variants(p)
 if p.get('_lifecycle'):return _growth_lifecycle_variants(p)
 if p.get('_extension'):return _growth_extension_variants(p)
 modes=['effect','payment','insufficient_mana','wrong_color','two_copies','cleanup','pump','pending_snapshot','resolved_snapshot','countered','preexisting']
 if p['instant']:modes+=['opposing_turn','stacked_spells']
 if p['scope'] not in ('team','bolster'):modes+=['opponent','lost_target','bounce_target','bounce_after','protected_target']
 if p['scope'] in ('two','repeat','up_to_two','support'):modes+=['partial_target']
 if p['scope'] in ('up_to_two','support'):modes+=['one_target']
 if p['scope']=='repeat':modes+=['same_target']
 if p['scope'] in ('up_to_two','support','team','bolster'):modes+=['empty']
 if p['scope']=='single':modes+=['illegal_target']
 if p['scope']=='permanent':modes+=['artifact_target']
 if p['condition']!='none':modes+=['condition_met','condition_snapshot']
 if p['scope'] in ('support','up_to_two'):
  # These coverage components need the selector's optional Done input, or a
  # response after targets have been bound. A separate pending receipt proves
  # that binding happened at cast time without selecting an illegal fallback.
  modes=[v for v in modes if v not in ('countered','lost_target','bounce_target','protected_target','partial_target','one_target')]
  modes+=['selection_pending']
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]

def validate_witnesses(o,registry):
 for name,fields in SPELLS.items():
  r=o.get(name,{})
  if name not in registry or r.get('layout')!='normal' or (r.get('mana_cost'),r.get('oracle_text'))!=fields:raise ValueError('growth spell witness changed: '+name)
 for name,fields in WITNESSES.items():
  r=o.get(name,{})
  if name not in registry or r.get('layout')!='normal' or tuple(r.get(k) for k in ('power','toughness','type_line','oracle_text'))!=fields:raise ValueError('growth permanent witness changed: '+name)
 for name,pin in EXTENSION_PINS['cards'].items():
  r=o.get(name,{})
  if name not in registry or r.get('card_faces') or r.get('selected_face') or any(r.get(k)!=value for k,value in pin.items()):raise ValueError('growth extension witness changed: '+name)
 for identity in EXTENSION_PINS.get('internal_identities',[]):
  row=registry.get(identity['name'],{})
  if any(row.get(k)!=v for k,v in identity.items()):raise ValueError('growth internal observation identity changed: '+identity['name'])

def expand(name,csvid,d,p,selected,registry,o):
 if p.get('_counter_action'):return _growth_action_expand(name,csvid,d,p,selected,registry,o)
 if p.get('_combat_counter'):return _growth_combat_expand(name,csvid,d,p,selected,registry,o)
 if p.get('_lifecycle'):return _growth_lifecycle_expand(name,csvid,d,p,selected,registry,o)
 if p.get('_extension'):return _growth_extension_expand(name,csvid,d,p,selected,registry,o)
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  if variant not in variants(p):raise ValueError('unknown growth variant '+variant)
  v,owner=variant.rsplit('_',1);owner=int(owner);scope=p['scope'];multi=scope in ('two','repeat','up_to_two','support')
  hands=[list(HAND),list(HAND)];graves=[['Grizzly Bears','Giant Growth','Forest'] for _ in (0,1)]
  exiles=[list(EXILE),list(EXILE)];libs=[list(LIBRARY),list(LIBRARY)];mana=[[10]*6,[10]*6]
  setup=[];entities={};pre=[step('turn',player=1-owner if v=='opposing_turn' else owner,value=0)];run=[]
  def add(nm,ref,player,zone='battlefield',tapped=False):
   item=card(nm,ref,player,zone)
   if tapped:item['state']=['tapped']
   setup.append(item)
   if zone=='hand':hands[player].append(nm)
   if zone=='battlefield':entities[ref]=dict(name=nm,player=player,alive=True,counters=0,tapped=tapped,power=int(o[nm].get('power','0') or '0'),toughness=int(o[nm].get('toughness','0') or '0'),creature='Creature' in o[nm]['type_line'],boost=0,protected=False)
  for player in (0,1):
   if v=='condition_met' or v=='condition_snapshot':
    if p['condition']=='delirium' and player==owner:graves[player].append('Sol Ring')
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
    setup.extend(card(nm,f'{zone}_{player}_{i}',player,zone) for i,nm in enumerate(names))
  add(name,'source',owner,'hand')
  if v in ('two_copies','stacked_spells'):add(name,'second',owner,'hand')
  if v not in ('empty','illegal_target'):
   add('Savannah Lions','first',1-owner if v in ('opponent','protected_target') else owner,tapped=True)
   add('Grizzly Bears','second_target',owner,tapped=True)
   add('Scaled Wurm','unselected',owner,tapped=True)
   add('Grizzly Bears','opposing',1-owner,tapped=True)
  add('Sol Ring','artifact',owner)
  targets=[] if v in ('empty','illegal_target') else ['first']
  if multi and v not in ('empty','one_target'):targets+=['first' if v=='same_target' else 'second_target']
  if v=='artifact_target':targets=['artifact']
  if v=='illegal_target':targets=['artifact']
  if v in ('two_copies','stacked_spells'):mana[owner][0]=max(10,2*p['mana'][0])
  if v=='payment':mana[owner]=list(p['mana'])
  if v=='insufficient_mana':mana[owner]=[0]*6
  if v=='wrong_color':
   mana[owner]=list(p['mana']);c=next(i for i in range(1,6) if mana[owner][i]);mana[owner][1 if c!=1 else 2]+=mana[owner][c];mana[owner][c]=0
  for player in (0,1):pre.extend(step('set_mana',player=player,color=i,value=n) for i,n in enumerate(mana[player]))
  def pay(cost,player=owner):mana[player]=[a-b for a,b in zip(mana[player],cost)]
  def stats():
   seq=[]
   for ref,e in entities.items():
    seq.append(observe(ref,'in_play',int(e['alive'])))
    if e['alive']:
     seq += [observe(ref,'plus1_counters',e['counters']),observe(ref,'tapped',int(e['tapped']))]
     if e['creature']:seq.extend(observe(ref,k,e[k]+e['counters']+e['boost']) for k in ('power','toughness'))
   return seq
  def choose():
   if scope in ('team','bolster'):return []
   seq=[step('choose_target',player=owner,ref=t) for t in targets]
   if scope in ('support','up_to_two') and len(targets)==1:seq.append(step('choose_cancel_target',player=owner))
   return seq
  def bolster_ref():
   candidates=[ref for ref,e in entities.items() if e['alive'] and e['creature'] and e['player']==owner]
   return min(candidates,key=lambda ref:entities[ref]['toughness']+entities[ref]['counters']+entities[ref]['boost']) if candidates else None
  def resolve_choices():
   ref=bolster_ref() if scope=='bolster' else None
   if not ref:return []
   e=entities[ref];least=e['toughness']+e['counters']+e['boost']
   tied=sum(other['alive'] and other['creature'] and other['player']==owner and other['toughness']+other['counters']+other['boost']==least for other in entities.values())
   # A unique least-toughness creature requires no player choice.
   return [step('choose_target',player=owner,ref=ref)] if tied>1 else []
  def cast(ref='source'):
   pay(p['mana']);hands[owner].remove(name)
   return choose()+[step('cast',ref=ref,value=1),step('assert_stack',value=1 if ref=='source' or v!='stacked_spells' else 2)]
  def effect():
   live=[t for t in targets if entities[t]['alive'] and not entities[t]['protected']]
   if targets and not live and scope not in ('team','bolster'):
    graves[owner].append(name);return
   recipients=([ref for ref,e in entities.items() if e['alive'] and e['creature'] and e['player']==owner] if scope=='team' else [bolster_ref()] if scope=='bolster' and bolster_ref() else live)
   amount=3 if p['condition']!='none' and v in ('condition_met','condition_snapshot') else p['amount']
   for ref in recipients:entities[ref]['counters']+=amount
   if p['untap']:
    for ref in recipients:entities[ref]['tapped']=False
   if p['followup']!='none':
    for e in entities.values():
     if e['alive'] and e['creature'] and e['player']==owner and e['counters']:
      if p['followup']=='countered_team':e['counters']+=1
      else:e['tapped']=False
   # A spell with all its targets illegal does not draw its secondary card.
   if p['draw'] and (live or scope in ('team','bolster') or not targets):hands[owner].extend(libs[owner][:p['draw']]);del libs[owner][:p['draw']]
   graves[owner].append(name)
  def auxiliary(nm,ref,target,caster=owner,pending=0):
   add(nm,ref,caster,'hand');hands[caster].remove(nm);graves[caster].append(nm);pay(cost_from_oracle(o[nm]),caster)
   return [step('choose_target',player=caster,ref=target),step('cast',ref=ref,value=1),step('assert_stack',value=pending+1),step('resolve'),step('assert_choices_consumed')]
  if v=='preexisting':
   run+=auxiliary('Arbor Armament','seed','unselected');entities['unselected']['counters']=1;run+=stats()
  if p['condition']=='morbid' and v in ('condition_met','condition_snapshot'):
   run.append(step('destroy',ref='opposing'));entities['opposing']['alive']=False;graves[1-owner].append('Grizzly Bears')
  cut=len(run)
  if v in ('insufficient_mana','wrong_color'):
   run += [step('cast',ref='source',value=0),observe('source','in_hand',1)]
  elif v=='illegal_target':
   run += [step('can_cast',ref='source',value=0),observe('source','in_hand',1)]
  elif v=='selection_pending':
   run+=cast()+stats()
  elif v=='countered':
   add('Counterspell','response',0,'hand');hands[0].remove('Counterspell');graves[0].append('Counterspell');pay(cost_from_oracle(o['Counterspell']),0)
   hands[owner].remove(name);graves[owner].append(name);pay(p['mana'])
   run+=choose()+[step('choose_priority',ref='response'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
  else:
   run+=cast()+stats();cut=len(run)
   if v in ('lost_target','partial_target'):
    ref='second_target' if v=='partial_target' else 'first';run.append(step('destroy',ref=ref));entities[ref]['alive']=False;graves[entities[ref]['player']].append(entities[ref]['name'])
   if v=='bounce_target':
    run+=auxiliary('Unsummon','response','first',caster=1-owner,pending=1);e=entities['first'];e['alive']=False;hands[e['player']].append(e['name'])
   if v=='protected_target':
    run+=auxiliary("Ranger's Guile",'response','first',caster=1-owner,pending=1);entities['first']['protected']=True;entities['first']['boost']=1
   if v=='stacked_spells':run+=cast('second')+resolve_choices();effect()
   run += resolve_choices()+[step('resolve'),step('assert_choices_consumed')];effect();run+=stats()
   if v=='two_copies':run+=cast('second')+resolve_choices()+[step('resolve'),step('assert_choices_consumed')];effect();run+=stats()
   if v=='pump':
    run+=auxiliary('Giant Growth','pump','first');entities['first']['boost']=3;run+=stats();run.append(step('cleanup'));entities['first']['boost']=0;run+=stats()
   if v=='cleanup':run += [step('cleanup')]+stats()+[step('cleanup')]+stats()
   if v=='bounce_after':
    run+=auxiliary('Unsummon','response','first',caster=1-owner);e=entities['first'];e['alive']=False;hands[e['player']].append(e['name'])
    run += [step('bind_hand',player=e['player'],value=registry[e['name']]['csvid'],ref='returned'),observe('returned','plus1_counters',0),observe('returned','in_hand',1)]
   if v=='resolved_snapshot':cut=len(run)
  checks=stats()+[step('assert_stack',value=int(v=='selection_pending')),step('assert_choices_consumed')]
  for player in (0,1):
   checks += [step('assert_life',player=player,value=20),step('assert_mana_total',player=player,value=sum(mana[player])),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
   if v=='payment':checks.extend(step('assert_mana',player=player,color=i,value=n) for i,n in enumerate(mana[player]))
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
    # The active-zone hand inspector includes cards currently on the stack.
    # Other receipts are still exact while this dedicated cast is pending.
    if v=='selection_pending' and zone=='hand':continue
    checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
    checks.extend(step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' else step('assert_zone_multiplicity',player=player,zone=zone,index=registry[nm]['csvid'],value=count) for i,(nm,count) in enumerate([(nm,1) for nm in names] if zone=='library' else Counter(names).items()))
   if v!='selection_pending':checks.extend(step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=n) for nm,n in Counter(hands[player]+graves[player]).items())
  actions=pre+run+checks
  if v in ('pending_snapshot','resolved_snapshot','condition_snapshot'):
   actions=pre+run[:cut]+[step('capture')]+run[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+run[cut:]+checks+[step('assert_recorded_state')]
  result.append(dict(name=f'GrowthCounterSpellRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,
    _recipe=dict(family='growth_counter_spell',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['remaining_components']+(['optional one-target Done selection and responses after targets are bound; cast-time binding has a separate pending receipt'] if scope in ('support','up_to_two') else []))))
 return result


def _growth_extension_spec(d,o):
 from recipes import cost_from_oracle
 profile=EXTENSION_PINS['profiles'].get(o.get('name')) if isinstance(o,dict) else None
 exact=lambda value:json.dumps(value,sort_keys=True,ensure_ascii=False,separators=(',',':'))
 if (not profile or d.get('family')!='growth_counter_spell' or d.get('version')!=2
     or o.get('layout')!='normal' or o.get('card_faces') or o.get('selected_face')
     or d.get('oracle_text')!=profile['source']['oracle_text']
     or any(o.get(k)!=value for k,value in profile['source'].items())
     or exact(d.get('params'))!=exact(profile['params'])
     or not isinstance(d.get('variants'),list) or not d['variants']):
  raise ValueError('growth extension requires exact reviewed parameters, complete source pin and explicit variants')
 p=copy.deepcopy(profile['params'])
 mana_source=dict(o,mana_cost=o['mana_cost'].replace('{X}','')) if p.get('variable') else o
 p.update(family='growth_counter_spell',_extension=True,mana=cost_from_oracle(mana_source),instant=o['type_line']=='Instant')
 available=_growth_extension_variants(p)
 if any(not isinstance(v,str) or v not in available for v in d['variants']) or len(d['variants'])!=len(set(d['variants'])):
  raise ValueError('unknown or duplicate reviewed growth variant')
 return p


def _growth_extension_variants(p):
 if p['mechanic']=='linked':return _growth_linked_variants(p)
 if p['mechanic']=='normal_component':
  modes=['effect','payment','insufficient_mana','wrong_color','ready_snapshot','pending_snapshot']
  if p['component_family']=='hand_exchange':modes+=['cleanup','countered']
  elif p['component_family']!='draw_cards':modes+=['friendly','empty','wrong_target','lost_target']
  return [v+'_'+str(owner) for owner in (0,1) for v in modes]
 modes=['effect','payment','insufficient_mana','wrong_color','cleanup','ready_snapshot','pending_snapshot','two_copies','countered']
 if p['instant']:modes+=['opposing_turn']
 mechanic=p['mechanic']
 if mechanic in ('fixed','distribute','mill_counters','double'):
  modes+=['opponent','preexisting','lost_target']
 if mechanic=='fixed' and len(p['amounts'])>1:modes+=['partial_target','selection_pending']
 if mechanic=='distribute':modes+=['one_target','partial_target']
 if mechanic=='double':modes+=['multi_target','counterless']
 if mechanic=='fight':modes+=['fight_both_die','fight_first_dies','no_counter','wrong_controller','lost_target']
 if mechanic=='mill_counters':modes+=['empty_grave','empty_library','short_library']
 if mechanic=='draw_indestructible':modes+=['empty_counters','multiple_counters','protection_expiry']
 if mechanic=='allocated':modes+=['one_target','opponent','preexisting','lost_target','partial_target','selection_pending']
 if mechanic=='team':modes+=['opponent','preexisting','selection_pending']
 if mechanic=='fixed' and p['strive_cost']:modes+=['multi_target']
 return [v+'_'+str(owner) for owner in (0,1) for v in modes if v+'_'+str(owner) not in p.get('deferred_variants',[])]


def _growth_extension_expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 if any(not isinstance(v,str) or v not in _growth_extension_variants(p) for v in selected) or len(selected)!=len(set(selected)):
  raise ValueError('unknown or duplicate reviewed growth variant')
 if p['mechanic']=='normal_component':return _growth_normal_component_expand(name,csvid,d,p,selected,rows,o)
 if p['mechanic']=='linked':return _growth_linked_expand(name,csvid,d,p,selected,rows,o)
 results=[]
 for requested in selected:
  v,owner=requested.rsplit('_',1);owner=int(owner);m=p['mechanic']
  hands=[list(HAND),list(HAND)];graves=[['Grizzly Bears','Giant Growth','Forest'] for _ in (0,1)]
  exiles=[list(EXILE),list(EXILE)];libs=[list(LIBRARY),list(LIBRARY)];mana=[[10]*6,[10]*6];life=[20,20]
  if v=='empty_grave':graves[owner]=[]
  if v=='empty_library':libs[owner]=[]
  if v=='short_library':libs[owner]=['Forest','Plains']
  setup=[];entities={};spells={};run=[]
  actor=1-owner if v=='opposing_turn' else owner
  pre=[step('turn',player=actor,value=0),step('reset_death_history')]
  for player in (0,1):
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
    setup.extend(card(nm,f'{zone}_{player}_{i}',player,zone) for i,nm in enumerate(names))
  def reserve(nm,ref,player):
   setup.append(card(nm,ref,player,'hand'));hands[player].append(nm);spells[ref]=(nm,player)
  def add(nm,ref,player):
   setup.append(card(nm,ref,player));r=o[nm]
   entities[ref]=dict(name=nm,player=player,alive=True,counters=0,power=int(r.get('power','0') or '0'),toughness=int(r.get('toughness','0') or '0'),damage=0,indestructible=False,tapped=nm=='Sol Ring',creature='Creature' in r['type_line'])
   if nm=='Sol Ring':setup[-1]['state']=['tapped']
  reserve(name,'source',owner)
  if v=='two_copies':reserve(name,'source_second',owner)
  first='Scaled Wurm' if m=='fight' and v!='fight_both_die' and v!='fight_first_dies' else 'Grizzly Bears'
  enemy='Hill Giant' if v=='fight_both_die' else 'Scaled Wurm' if v=='fight_first_dies' else 'Grizzly Bears'
  add(first,'first',1-owner if v=='opponent' else owner)
  add('Savannah Lions','second',owner);add('Hill Giant','third',owner)
  add('Wall of Stone' if first=='Scaled Wurm' else 'Scaled Wurm','unselected',owner)
  add(enemy,'enemy',1-owner);add('Sol Ring','artifact',owner)
  if m=='fight' and v=='two_copies':add('Hill Giant','enemy_second',1-owner)
  targets=(['first','enemy'] if m=='fight' else [] if m=='draw_indestructible' else
           ['first','second'] if m=='double' and v=='multi_target' else
           ['first'] if v=='one_target' else ['first','second','third'][:len(p['amounts'])])
  if m=='allocated':targets=(['first']*len(p['amounts']) if v=='one_target' else list(p['target_roles']))
  if m=='team':targets=[]
  if m=='fixed' and p['strive_cost'] and v=='multi_target':targets=['first','second']
  seeds=[]
  if m=='fight':
   if v=='wrong_controller':seeds=['enemy']
   elif v!='no_counter':seeds=['first']
  elif m=='double' and v!='counterless':seeds=['first']+(['second'] if v=='multi_target' else [])
  elif m=='draw_indestructible' and v!='empty_counters':seeds=['first','second','enemy']+(['first'] if v=='multiple_counters' else [])
  elif v=='preexisting':seeds=['first']
  for i,ref in enumerate(seeds):reserve('Arbor Armament',f'seed_{i}',owner)
  if v=='lost_target':reserve('Unsummon','response',1-owner)
  if v=='countered':reserve('Counterspell','counter',1-owner)
  if m=='draw_indestructible':
   for ref in ('murder_marked','murder_unmarked','murder_enemy','murder_expired'):reserve('Murder',ref,1-owner)
  def pay(cost,player):mana[player]=[a-b for a,b in zip(mana[player],cost)]
  def pool(vector):
   mana[owner]=list(vector)
   return [step('set_mana',player=owner,color=i,value=n) for i,n in enumerate(vector)]
  def stats():
   seq=[]
   for ref,e in entities.items():
    seq.append(observe(ref,'in_play',int(e['alive'])))
    if e['alive']:
     seq += [observe(ref,'plus1_counters',e['counters']),observe(ref,'damage',e['damage']),observe(ref,'tapped',int(e['tapped']))]
     if e['creature']:seq += [observe(ref,k,e[k]+e['counters']) for k in ('power','toughness')]
   return seq
  def receipt():
   return [step('assert_mana',player=player,color=i,value=n) for player in (0,1) for i,n in enumerate(mana[player])]
  def die(ref):
   e=entities[ref];e['alive']=False;graves[e['player']].append(e['name'])
  def aux(ref,target):
   nm,player=spells[ref];pay(cost_from_oracle(o[nm]),player);hands[player].remove(nm);graves[player].append(nm)
   return [step('require_card',ref=target,property='in_play',value=1),step('choose_target',player=player,ref=target),step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]+receipt()
  for i,ref in enumerate(seeds):
   run+=aux(f'seed_{i}',ref);entities[ref]['counters']+=1;run+=stats()
  cost=list(p['mana'])
  if p['strive_cost'] and len(targets)>1:
   extra=cost_from_oracle(dict(mana_cost=p['strive_cost']));cost=[a+(len(targets)-1)*b for a,b in zip(cost,extra)]
  if v=='insufficient_mana':run+=pool([0]*6)
  elif v=='wrong_color':
   wrong=list(cost);color=next(i for i in range(1,6) if wrong[i]);wrong[1 if color!=1 else 2]+=wrong[color];wrong[color]=0;run+=pool(wrong)
  elif v=='payment' or p['strive_cost']:run+=pool(cost)
  ready_cut=len(run)
  def choose():
   seq=[step('choose_dialog',player=owner,value=n) for n in p['mode_choices']]
   if m=='team':return seq+[step('choose_player',player=owner,value=1-owner if v=='opponent' else owner)]
   seq.extend(step('choose_target',player=owner,ref=ref) for ref in targets)
   if m=='distribute' and len(targets)==1:seq.append(step('choose_cancel_target',player=owner))
   return seq
  def cast(ref):
   pay(cost,owner);hands[owner].remove(name)
   return choose()+[step('cast',ref=ref,value=1),step('assert_stack',value=1)]
  def fight():
   if not all(entities[ref]['alive'] for ref in targets):return
   a,b=(entities[ref] for ref in targets);ap=a['power']+a['counters'];bp=b['power']+b['counters']
   a['damage']+=max(0,bp);b['damage']+=max(0,ap)
   for ref in targets:
    e=entities[ref]
    if e['damage']>=e['toughness']+e['counters'] and not e['indestructible']:die(ref)
  def effect():
   live=[ref for ref in targets if entities[ref]['alive']]
   if targets and not live:graves[owner].append(name);return
   if m in ('fixed','distribute','allocated'):
    amounts=([sum(p['amounts'])] if m=='distribute' and len(targets)==1 else p['amounts'])
    if m=='fixed' and p['strive_cost']:amounts=[p['amounts'][0]]*len(targets)
    for ref,n in zip(targets,amounts):
     if entities[ref]['alive']:entities[ref]['counters']+=n
   elif m=='team':
    recipient=1-owner if v=='opponent' else owner
    for e in entities.values():
     if e['alive'] and e['creature'] and e['player']==recipient:e['counters']+=p['amounts'][0]
   elif m=='double':
    for ref in live:entities[ref]['counters']*=2
   elif m=='fight':fight()
   elif m=='mill_counters':
    milled=libs[owner][:3];del libs[owner][:3];graves[owner].extend(milled)
    count=sum('Creature' in o[nm]['type_line'] for nm in graves[owner]);entities[targets[0]]['counters']+=count
   elif m=='draw_indestructible':
    recipients=[e for e in entities.values() if e['alive'] and e['creature'] and e['player']==owner and e['counters']>0]
    for e in recipients:e['indestructible']=True
    hands[owner].extend(libs[owner][:len(recipients)]);del libs[owner][:len(recipients)]
   graves[owner].append(name)
  negative=v in ('insufficient_mana','wrong_color','no_counter','wrong_controller')
  if negative:
   run += [step('cast' if v in ('insufficient_mana','wrong_color') else 'can_cast',ref='source',value=0),step('assert_stack',value=0),observe('source','in_hand',1)]
  else:
   run+=cast('source')+stats();pending_cut=len(run)
   if v=='selection_pending':pass
   elif v=='countered':
    nm,player=spells['counter'];hands[player].remove(nm);graves[player].append(nm);graves[owner].append(name);pay(cost_from_oracle(o[nm]),player)
    run += [step('cast_response',ref='counter',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_choices_consumed')]
   else:
    if v=='lost_target':
     nm,player=spells['response'];hands[player].remove(nm);graves[player].append(nm);pay(cost_from_oracle(o[nm]),player);e=entities['first'];e['alive']=False;hands[e['player']].append(e['name'])
     run += [step('choose_target',player=player,ref='first'),step('cast_response',ref='response',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_choices_consumed')]
     effect()
    else:
     if v=='partial_target':run.append(step('destroy',ref='second'));die('second')
     run += [step('resolve'),step('assert_choices_consumed')];effect()
    run += stats()+receipt()
    if v=='two_copies':
     if m=='fight':targets[1]='enemy_second'
     if p['strive_cost']:run+=pool(cost)
     run+=cast('source_second')+[step('resolve'),step('assert_choices_consumed')];effect();run+=stats()+receipt()
    if m=='draw_indestructible':
     if v!='empty_counters':
      run+=aux('murder_marked','first')+[step('require_card',ref='first',property='in_play',value=1)]+stats()
      run+=aux('murder_unmarked','unselected');die('unselected');run+=stats()
      run+=aux('murder_enemy','enemy');die('enemy');run+=stats()
     if v=='protection_expiry':
      run.append(step('cleanup'))
      for e in entities.values():e['indestructible']=False;e['damage']=0
      run+=stats()+aux('murder_expired','first');die('first');run+=stats()
    if v=='cleanup':
     run.append(step('cleanup'))
     for e in entities.values():e['damage']=0;e['indestructible']=False
     run+=stats()
  checks=stats()+receipt()+[step('assert_stack',value=int(v=='selection_pending')),step('assert_choices_consumed')]
  for player in (0,1):
   checks += [step('assert_life',player=player,value=life[player]),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('library',libs[player]),('exile',exiles[player])):
    if v=='selection_pending' and zone=='hand':continue
    checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
    if zone=='library':checks.extend(step('assert_zone_card',player=player,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(names))
    else:checks.extend(step('assert_zone_multiplicity',player=player,zone=zone,index=rows[nm]['csvid'],value=count) for nm,count in Counter(names).items())
   if v!='selection_pending':checks.extend(step('assert_hand_graveyard_total',player=player,index=rows[nm]['csvid'],value=count) for nm,count in Counter(hands[player]+graves[player]).items())
  actions=pre+run+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=ready_cut if v=='ready_snapshot' else pending_cut
   tail=run[cut:]+checks
   actions=pre+run[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  results.append(dict(name=f'GrowthCounterSpellExtensionRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,
    _recipe=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components']+['multiplayer','replacement effects and control changes'])))
 return results


def _growth_normal_component_expand(name,csvid,d,p,selected,rows,o):
 """Run the explicitly selected normal-cost component through its usual family.

 The complete original source, including Awaken, was checked before reaching
 this adapter. The component record describes only the chosen ordinary cast;
 all fixtures deliberately have no controlled battlefield land.
 """
 from recipes import expand as public_expand,step
 result=[];family=p['component_family']
 for requested in selected:
  branch,owner=requested.rsplit('_',1);owner=int(owner)
  component=copy.deepcopy(o[name]);component.update(oracle_text=p['component_text'],keywords=[])
  if family=='hand_exchange':
   mapped='countered_cast' if branch=='countered' else branch
   child=dict(family=family,version=2,params=p['component_params'],oracle_text=p['component_text'],variants=[mapped+'_'+str(owner)],target_choices=1)
  else:
   mapped={'effect':'ordered' if family=='draw_cards' else 'opponent','ready_snapshot':'snapshot','pending_snapshot':'snapshot','wrong_color':'insufficient_mana'}.get(branch,branch)
   child=dict(family=family,version=2,params=p['component_params'],oracle_text=p['component_text'],variants=[mapped])
  case=public_expand(name,csvid,child,component,rows,oracle_context=o)[0]
  case['setup']=[copy.deepcopy(a) for a in case['setup']]
  actions=[copy.deepcopy(a) for a in case['steps']]
  if family!='hand_exchange':
   annotated=[]
   for i,a in enumerate(actions):
    annotated.append(a)
    if a['op']=='cast' and a.get('value')==1 and (i+1==len(actions) or actions[i+1]!=step('assert_stack',value=1)):
     annotated.append(step('assert_stack',value=1))
    if a['op']=='resolve':annotated.append(step('assert_mana_total',player=0,value=0 if branch=='payment' else 60-sum(p['mana'])))
   actions=annotated
   if branch=='wrong_color':
    wrong=list(p['mana']);color=next(i for i in range(1,6) if wrong[i]);wrong[1 if color!=1 else 2]+=wrong[color];wrong[color]=0
    for a in actions:
     if a['op']=='set_mana' and a.get('player')==0:a['value']=wrong[a['color']]
     if a['op']=='assert_mana_total' and a.get('player')==0:a['value']=sum(wrong)
    actions.extend(step('assert_mana',player=0,color=i,value=n) for i,n in enumerate(wrong))
   if branch=='pending_snapshot':
    capture=next(i for i,a in enumerate(actions) if a['op']=='capture');record=next(i for i,a in enumerate(actions) if a['op']=='record_state')
    body=actions[capture+1:record];cut=next(i for i,a in enumerate(body) if a['op']=='assert_stack' and a['value']==1)+1
    tail=body[cut:]
    actions=actions[:capture]+body[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
   if owner:
    for a in case['setup']+actions:
     if a.get('player') in (0,1):a['player']=1-a['player']
     if a.get('op')=='choose_player':a['value']=1-a['value']
    actions.insert(0,step('turn',player=owner,value=0))
  case.update(name=f'GrowthCounterSpellExtensionRecipe_{csvid}_{requested}',steps=actions)
  case['_recipe']=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components']+['multiplayer','replacement effects and control changes'])
  result.append(case)
 return result


def _growth_linked_variants(p):
 if p['linked_kind']=='reanimate':return _growth_reanimate_variants(p)
 if p['linked_kind']=='manifest':return _growth_manifest_variants(p)
 modes=['effect','payment','insufficient_mana','wrong_color','preexisting','cleanup','ready_snapshot','pending_snapshot','two_copies','countered']
 kind=p['linked_kind']
 if p['instant'] and p.get('target_predicate')!='combat':modes+=['opposing_turn']
 if kind=='counter_fight':modes+=['fight_both_die','fight_first_dies','lost_target','lost_own','separate_counter','selection_pending']
 if kind=='destroy_bolster':modes+=['friendly','lost_target','wrong_predicate','selection_pending']
 if kind=='sacrifice_bolster':modes+=['self_target','no_enchantment','empty_team','selection_pending']
 if kind=='tutor_bolster':modes+=['empty_library','no_basic_land','fail_to_find','empty_team']
 if kind=='team_life':modes+=['empty_team']
 if kind=='tempting_team':modes+=['decline','empty_team']
 if kind=='support_damage':modes=[v for v in modes if v not in ('countered','two_copies')]+['lethal','selection_pending']
 if kind=='temporary_allocation':modes=[v for v in modes if v not in ('insufficient_mana','wrong_color')]+['one_target','lost_target','partial_target','selection_pending','preexisting_cleanup']
 if kind=='exile_counter':modes+=['opponent','lost_target','lost_own','wrong_predicate','selection_pending','own_only']
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]


def _growth_linked_expand(name,csvid,d,p,selected,rows,o):
 """Model sequential printed effects, deriving every receipt from Oracle pins."""
 from recipes import card,step,observe,cost_from_oracle
 if p['linked_kind']=='reanimate':return _growth_reanimate_expand(name,csvid,d,p,selected,rows,o)
 if p['linked_kind']=='manifest':return _growth_manifest_expand(name,csvid,d,p,selected,rows,o)
 result=[];kind=p['linked_kind'];combat=p.get('target_predicate')=='combat'
 for requested in selected:
  v,owner=requested.rsplit('_',1);owner=int(owner)
  setup=[];es={};spells={};run=[];hands=[list(HAND),list(HAND)]
  graves=[['Grizzly Bears','Giant Growth','Forest'] for _ in (0,1)]
  exiles=[list(EXILE),list(EXILE)];libs=[list(LIBRARY),list(LIBRARY)]
  mana=[[10]*6,[10]*6];life=[20,20];fetched=0;shuffled=False;last_offered=[];temporary=Counter()
  if kind=='tutor_bolster':libs[owner].append('Wasteland')
  if v=='empty_library':libs[owner]=[]
  if v=='no_basic_land':libs[owner]=['Giant Growth','Grizzly Bears','Lightning Bolt']
  target_player=owner if v=='self_target' else 1-owner
  actor=(owner if v=='friendly' else 1-owner) if combat and v!='wrong_predicate' else 1-owner if v=='opposing_turn' else owner
  pre=[step('turn',player=actor,value=0),step('reset_death_history')]
  if kind=='tutor_bolster':pre.append(step('observe_search_filters',value=1))
  def reserve(nm,ref,pl):
   setup.append(card(nm,ref,pl,'hand'));hands[pl].append(nm);spells[ref]=(nm,pl)
  def add(nm,ref,pl,tapped=False):
   item=card(nm,ref,pl)
   if tapped:item['state']=['tapped']
   setup.append(item);r=o[nm]
   es[ref]=dict(name=nm,player=pl,alive=True,counters=0,power=int(r.get('power','0') or '0'),toughness=int(r.get('toughness','0') or '0'),damage=0,tapped=tapped,creature='Creature' in r['type_line'])
  reserve(name,'source',owner)
  if v=='two_copies':reserve(name,'source_second',owner)
  if v!='empty_team':
   first='Scaled Wurm' if kind=='counter_fight' and v not in ('fight_both_die','fight_first_dies') else 'Grizzly Bears'
   first_owner=1-owner if kind=='exile_counter' and v=='opponent' else owner
   second_owner=1-owner if kind=='exile_counter' and v in ('opponent','wrong_predicate') else owner
   add(first,'first',first_owner);add('Savannah Lions','second',second_owner)
   if not (kind=='exile_counter' and v in ('wrong_predicate','own_only')):
    add('Hill Giant','third',owner);add('Wall of Stone' if first=='Scaled Wurm' else 'Scaled Wurm','unselected',owner)
  enemy='Hill Giant' if v=='fight_both_die' else 'Scaled Wurm' if v=='fight_first_dies' or kind=='support_damage' else 'Suntail Hawk' if p.get('target_predicate')=='flying' and v!='wrong_predicate' else 'Grizzly Bears'
  if kind=='support_damage':enemy='Scaled Wurm' if v=='lethal' else 'Wall of Stone'
  if not (kind=='exile_counter' and v in ('wrong_predicate','own_only')):add(enemy,'enemy',owner if v=='friendly' else 1-owner)
  if kind=='exile_counter' and v not in ('wrong_predicate','own_only'):add('Hill Giant','enemy_other',1-owner)
  if kind in ('counter_fight','destroy_bolster') and v=='two_copies':add('Suntail Hawk' if p.get('target_predicate')=='flying' else 'Hill Giant','enemy_second',1-owner)
  add('Sol Ring','artifact',owner,tapped=True)
  if kind=='sacrifice_bolster' and v!='no_enchantment':add('Greed','offered_enchantment',target_player)
  if v in ('preexisting','preexisting_cleanup'):reserve('Arbor Armament','seed',owner)
  if v in ('lost_target','lost_own'):reserve('Unsummon','response',1-owner)
  if v=='countered':reserve('Counterspell','counter',1-owner)
  for pl in (0,1):
   for zone,names in (('hand',list(HAND)),('graveyard',graves[pl]),('library',libs[pl]),('exile',exiles[pl])):
    setup.extend(card(nm,f'{zone}_{pl}_{i}',pl,zone) for i,nm in enumerate(names))
  target='first' if kind in ('temporary_allocation','exile_counter') else 'enemy';counter_target='second' if v=='separate_counter' or kind=='exile_counter' else 'first'
  def stats():
   seq=[]
   for ref,e in es.items():
    seq.append(observe(ref,'in_play',int(e['alive'])))
    if e['alive']:
     seq.extend(observe(ref,prop,int(e[prop])) for prop in ('damage','tapped'))
     seq.append(observe(ref,'plus1_counters',e['counters']))
     if e['creature']:seq.extend(observe(ref,prop,e[prop]+e['counters']) for prop in ('power','toughness'))
   if kind=='tutor_bolster':
    seq.append(step('assert_zone_multiplicity',player=owner,zone='battlefield',index=rows['Forest']['csvid'],value=fetched))
    if fetched:
     seq.extend(step('assert_matching_permanents',player=owner,index=rows['Forest']['csvid'],property=prop,value=n) for prop,n in (('tapped',1),('plus1_counters',0)))
   return seq
  def receipts():return [step('assert_mana',player=pl,color=i,value=n) for pl in (0,1) for i,n in enumerate(mana[pl])]
  def pay(cost,pl):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  def pool(cost):
   mana[owner]=list(cost)
   return [step('set_mana',player=owner,color=i,value=n) for i,n in enumerate(cost)]
  def die(ref):
   e=es[ref];e['alive']=False;graves[e['player']].append(e['name'])
  def consume(ref):
   nm,pl=spells[ref];hands[pl].remove(nm);pay(cost_from_oracle(o[nm]),pl)
  if v in ('preexisting','preexisting_cleanup'):
   consume('seed');graves[owner].append('Arbor Armament')
   seed_target='unselected' if kind=='support_damage' else 'second'
   run+=[step('choose_target',player=owner,ref=seed_target),step('cast',ref='seed',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
   es[seed_target]['counters']+=1;run+=stats()+receipts()
  if combat and v!='wrong_predicate':
   attackers=['enemy']+(['enemy_second'] if v=='two_copies' else [])
   run+=[step('begin_attackers')]+[step('attack',ref=ref,value=1) for ref in attackers]+[step('finish_attackers'),step('begin_blockers'),step('advance_combat')]
   for ref in attackers:es[ref]['tapped']=True
  if v=='payment' or kind=='temporary_allocation':run+=pool(p['mana'])
  if v=='insufficient_mana':run+=pool([0]*6)
  if v=='wrong_color':
   wrong=list(p['mana']);color=next(i for i in range(1,6) if wrong[i]);wrong[1 if color!=1 else 2]+=wrong[color];wrong[color]=0;run+=pool(wrong)
  ready_cut=len(run)
  def choose():
   if kind=='counter_fight':return [step('choose_dialog',player=owner,value=n) for n in p['mode_choices']]+[step('choose_target',player=owner,ref=ref) for ref in (counter_target,'first',target)]
   if kind=='destroy_bolster':return [step('choose_target',player=owner,ref=target)]
   if kind=='sacrifice_bolster':return [step('choose_player',player=owner,value=target_player)]
   if kind=='support_damage':return [step('choose_target',player=owner,ref=ref) for ref in (target,'first','second')]
   if kind=='temporary_allocation':return [step('choose_target',player=owner,ref=ref) for ref in (['first']*3 if v=='one_target' else ['first','first','second'])]+[step('choose_dialog',player=owner,value=p['normal_cost_choice'])]
   if kind=='exile_counter':return [step('choose_target',player=owner,ref=ref) for ref in (target,counter_target)]
   return []
  def cast(ref):
   consume(ref)
   return ([step('require_can_cast',ref=ref,value=1)] if kind=='exile_counter' and v=='own_only' else [])+choose()+[step('cast',ref=ref,value=1),step('assert_stack',value=1)]
  def bolster():
   candidates=[ref for ref,e in es.items() if e['alive'] and e['creature'] and e['player']==owner]
   if not candidates:return []
   low=min(es[ref]['toughness']+es[ref]['counters'] for ref in candidates)
   tied=[ref for ref in candidates if es[ref]['toughness']+es[ref]['counters']==low];ref=tied[0]
   es[ref]['counters']+=p['amount']
   return [step('choose_target',player=owner,ref=ref)] if len(tied)>1 else []
  def effect():
   nonlocal fetched,shuffled,last_offered
   seq=[]
   if kind=='counter_fight':
    if es[counter_target]['alive']:es[counter_target]['counters']+=1
    if es['first']['alive'] and es[target]['alive']:
     a,b=es['first'],es[target];ap=a['power']+a['counters'];bp=b['power']+b['counters']
     a['damage']+=bp;b['damage']+=ap
     for ref in ('first',target):
      e=es[ref]
      if e['damage']>=e['toughness']+e['counters']:die(ref)
   elif kind=='destroy_bolster':
    if es[target]['alive']:die(target);seq+=bolster()
   elif kind=='sacrifice_bolster':
    offered=es.get('offered_enchantment')
    if offered and offered['alive']:
     if target_player==0:seq.append(step('choose_target',player=0,ref='offered_enchantment'))
     die('offered_enchantment')
    seq+=bolster()
   elif kind=='tutor_bolster':
    last_offered=list(libs[owner])
    if 'Forest' in libs[owner] and v!='fail_to_find':
     seq.append(step('choose_zone_card',player=owner,value=rows['Forest']['csvid']));libs[owner].remove('Forest');fetched+=1
    else:seq.append(step('choose_optional_zone',player=owner,value=rows['Forest']['csvid']))
    shuffled=True;seq+=bolster()
   elif kind=='team_life':
    seq.append(step('choose_dialog',player=1-owner,value=p['resolution_mode']))
    for e in es.values():
     if e['alive'] and e['creature'] and e['player']==owner:e['counters']+=p['amount']
    life[owner]+=p['gain_life']
   elif kind=='tempting_team':
    seq.append(step('choose_dialog',player=1-owner,value=1 if v=='decline' else 0))
    for e in es.values():
     if e['alive'] and e['creature']:
      if e['player']==owner:e['counters']+=1 if v=='decline' else 2
      elif v!='decline':e['counters']+=1
   elif kind=='support_damage':
    # Both selected support targets are queued at cast time. A separate pending
    # receipt exposes handlers which defer consuming those decisions.
    for ref in ('first','second'):
     if es[ref]['alive']:es[ref]['counters']+=1
    if es[target]['alive']:
     es[target]['damage']+=sum(e['power']+e['counters'] for e in es.values() if e['alive'] and e['creature'] and e['player']==owner and e['counters'])
     if es[target]['damage']>=es[target]['toughness']+es[target]['counters']:die(target)
   elif kind=='temporary_allocation':
    for ref in (['first']*3 if v=='one_target' else ['first','first','second']):
     if es[ref]['alive']:es[ref]['counters']+=1;temporary[ref]+=1
   elif kind=='exile_counter':
    if es[target]['alive']:
     e=es[target];e['alive']=False;exiles[e['player']].append(e['name'])
    if es[counter_target]['alive']:es[counter_target]['counters']+=p['amount']
   graves[owner].append(name)
   return seq
  def search_checks():
   if kind!='tutor_bolster':return []
   return [step('assert_forced_zone_choices',value=0),step('assert_offered_count',value=len(last_offered))]+[step('assert_offered_legal_card',index=rows[nm]['csvid'],value=int('Basic Land' in o[nm]['type_line'])) for nm in sorted(set(last_offered))]
  denied=v in ('insufficient_mana','wrong_color','wrong_predicate')
  pending_cut=0
  if denied:
   run+=[step('can_cast' if v=='wrong_predicate' else 'cast',ref='source',value=0),observe('source','in_hand',1),step('assert_stack',value=0)]
  else:
   run+=cast('source')+stats()+receipts();pending_cut=len(run)
   if v=='selection_pending':pass
   elif v=='countered':
    consume('counter');graves[1-owner].append('Counterspell');graves[owner].append(name)
    run+=[step('cast_response',ref='counter',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_choices_consumed')]
   else:
    if v in ('lost_target','lost_own'):
     ref=(counter_target if kind=='exile_counter' else 'first') if v=='lost_own' else target;consume('response');graves[1-owner].append('Unsummon');e=es[ref];e['alive']=False;hands[e['player']].append(e['name'])
     run+=[step('choose_target',player=1-owner,ref=ref),step('cast_response',ref='response',value=1),step('assert_stack',value=2)]+effect()+[step('resolve_response'),step('assert_choices_consumed')]
    else:
     if v=='partial_target':run.append(step('destroy',ref='second'));die('second')
     run+=effect()+[step('resolve'),step('assert_choices_consumed')]
    run+=stats()+receipts()+search_checks()
    if v=='two_copies':
     if kind in ('counter_fight','destroy_bolster'):target='enemy_second'
     if kind=='exile_counter':target='third'
     if kind=='temporary_allocation':run+=pool(p['mana'])
     run+=cast('source_second')+effect()+[step('resolve'),step('assert_choices_consumed')]+stats()+receipts()+search_checks()
    if v in ('cleanup','preexisting_cleanup'):
     run.append(step('cleanup'))
     for e in es.values():e['damage']=0
     if kind=='temporary_allocation':
      for ref,n in temporary.items():
       if es[ref]['alive']:es[ref]['counters']-=n
     run+=stats()
  checks=stats()+receipts()+[step('assert_stack',value=int(v=='selection_pending')),step('assert_choices_consumed')]
  for pl in (0,1):
   checks+=[step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in es.values())+(fetched if pl==owner else 0))]
   for zone,names in (('hand',hands[pl]),('graveyard',graves[pl]),('exile',exiles[pl]),('library',libs[pl])):
    if v=='selection_pending' and zone=='hand':continue
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(names)))
    if zone=='library' and not (shuffled and pl==owner):checks.extend(step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(names))
    else:checks.extend(step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=n) for nm,n in Counter(names).items())
   if v!='selection_pending':checks.extend(step('assert_hand_graveyard_total',player=pl,index=rows[nm]['csvid'],value=n) for nm,n in Counter(hands[pl]+graves[pl]).items())
  actions=pre+run+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=ready_cut if v=='ready_snapshot' else pending_cut;tail=run[cut:]+checks
   actions=pre+run[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  result.append(dict(name=f'GrowthCounterSpellLinkedRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,_recipe=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components']+['multiplayer','replacement effects and live control changes'])))
 return result


def _growth_reanimate_variants(p):
 modes=['effect','payment','insufficient_mana','wrong_color','cleanup','ready_snapshot','pending_snapshot','two_copies','countered','empty','wrong_type','lost_target','death_owner','selection_pending']
 modes+=['green_payment','green_snapshot','wrong_owner'] if p['condition']=='green_payment' else ['condition_met','condition_snapshot','opponent_graveyard','opponent_death']
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]


def _growth_reanimate_expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for requested in selected:
  v,owner=requested.rsplit('_',1);owner=int(owner);mastery=p['condition']=='spell_mastery'
  grave_owner=1-owner if v in ('opponent_graveyard','opponent_death','wrong_owner') else owner
  hands=[list(HAND),list(HAND)];graves=[['Grizzly Bears','Giant Growth','Forest'] for _ in (0,1)]
  libs=[list(LIBRARY),list(LIBRARY)];exiles=[list(EXILE),list(EXILE)];mana=[[10]*6,[10]*6]
  targets=['Elvish Warrior']+(['Craw Wurm'] if v=='two_copies' else [])
  graves[grave_owner].extend(targets)
  if v in ('empty','wrong_type'):
   graves=[[nm for nm in ns if 'Creature' not in o[nm]['type_line']] for ns in graves]
   if v=='wrong_type':
    for ns in graves:ns.append('Sol Ring')
  if v=='wrong_owner':graves[owner]=[nm for nm in graves[owner] if 'Creature' not in o[nm]['type_line']]
  setup=[];spells={};returned=[];run=[];kept_counters=0
  pre=[step('turn',player=owner,value=0),step('reset_death_history')]
  def reserve(nm,ref,pl):
   setup.append(card(nm,ref,pl,'hand'));hands[pl].append(nm);spells[ref]=(nm,pl)
  reserve(name,'source',owner)
  if v=='two_copies':reserve(name,'source_second',owner)
  seed=v in ('condition_met','condition_snapshot')
  if seed:reserve('Arbor Armament','seed',owner)
  if v=='countered':reserve('Counterspell','counter',1-owner)
  if v=='lost_target':reserve('Reclaim','response',grave_owner)
  if v in ('death_owner','opponent_death'):reserve('Murder','murder',1-owner)
  setup.append(card('Savannah Lions','kept',owner));setup.append(card('Hill Giant','enemy',1-owner))
  item=card('Sol Ring','artifact',owner);item['state']=['tapped'];setup.append(item)
  for pl in (0,1):
   for zone,ns in (('hand',list(HAND)),('graveyard',graves[pl]),('library',libs[pl]),('exile',exiles[pl])):
    setup.extend(card(nm,f'{zone}_{pl}_{i}',pl,zone) for i,nm in enumerate(ns))
  def pay(cost,pl):mana[pl]=[a-b for a,b in zip(mana[pl],cost)]
  def consume(ref):
   nm,pl=spells[ref];hands[pl].remove(nm);pay(cost_from_oracle(o[nm]),pl)
  def receipts():return [step('assert_mana',player=pl,color=i,value=n) for pl in (0,1) for i,n in enumerate(mana[pl])]
  def stats():
   seq=[observe('kept','in_play',1),observe('kept','plus1_counters',kept_counters),observe('kept','power',2+kept_counters),observe('kept','toughness',1+kept_counters),observe('enemy','in_play',1),observe('enemy','plus1_counters',0),observe('artifact','in_play',1),observe('artifact','tapped',1)]
   for e in returned:
    seq.append(observe(e['ref'],'in_play',int(e['alive'])))
    if e['alive']:
     seq.extend(observe(e['ref'],prop,n) for prop,n in (('plus1_counters',e['counters']),('power',int(o[e['name']]['power'])+e['counters']),('toughness',int(o[e['name']]['toughness'])+e['counters']),('damage',0),('tapped',0)))
   return seq
  if seed:
   consume('seed');graves[owner].append('Arbor Armament');kept_counters=1
   run+=[step('choose_target',player=owner,ref='kept'),step('cast',ref='seed',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]+stats()+receipts()
  green=v in ('green_payment','green_snapshot')
  cost=list(p['mana'])
  if green:cost[0]-=1;cost[3]+=1
  def pool(vector):
   mana[owner]=list(vector)
   return [step('set_mana',player=owner,color=i,value=n) for i,n in enumerate(vector)]
  actual_pool=[0]*6 if v=='insufficient_mana' else list(cost)
  if v=='wrong_color':actual_pool=[sum(cost),0,0,0,0,0]
  run+=pool(actual_pool);ready_cut=len(run)
  def choose(nm):
   seq=[step('choose_dialog',player=owner,value=1)] if green and owner==0 else []
   if mastery and all(any('Creature' in o[n]['type_line'] for n in graves[pl]) for pl in (0,1)):seq.append(step('choose_player',player=owner,value=grave_owner))
   return seq+[step('choose_zone_card',player=owner,value=rows[nm]['csvid'])]
  def cast(ref,nm):
   hands[owner].remove(name);pay(cost,owner)
   return [step('require_can_cast',ref=ref,value=1)]+choose(nm)+[step('cast',ref=ref,value=1),step('assert_stack',value=1)]
  def effect(nm):
   if nm not in graves[grave_owner]:graves[owner].append(name);return []
   counters=2 if mastery and sum(o[n]['type_line'] in ('Instant','Sorcery') for n in graves[owner])>=2 else 1 if green and not mastery else 0
   graves[grave_owner].remove(nm);graves[owner].append(name)
   ref='returned_'+str(len(returned));returned.append(dict(name=nm,ref=ref,alive=True,counters=counters,owner=grave_owner))
   return [step('bind_permanent',player=owner,value=rows[nm]['csvid'],ref=ref)]
  denied=v in ('insufficient_mana','wrong_color','wrong_owner','empty','wrong_type')
  pending_cut=0
  if denied:run+=[step('can_cast' if v in ('wrong_owner','empty','wrong_type') else 'cast',ref='source',value=0),observe('source','in_hand',1),step('assert_stack',value=0)]
  else:
   run+=cast('source',targets[0])+stats()+receipts();pending_cut=len(run)
   if v=='selection_pending':pass
   elif v=='countered':
    consume('counter');graves[1-owner].append('Counterspell');graves[owner].append(name)
    run+=[step('cast_response',ref='counter',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_choices_consumed')]
   else:
    if v=='lost_target':
     # The source paid an exact pool. Fund its same-owner response only after
     # that payment, preserving Vigor Mortis's genuinely non-green source cast.
     mana[grave_owner][3]=1;run.append(step('set_mana',player=grave_owner,color=3,value=1))
     consume('response');graves[grave_owner].remove(targets[0]);graves[grave_owner].append('Reclaim');libs[grave_owner].insert(0,targets[0])
     run+=[step('choose_zone_card',player=grave_owner,value=rows[targets[0]]['csvid']),step('cast_response',ref='response',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_choices_consumed')]+effect(targets[0])
    else:run+=[step('resolve'),step('assert_choices_consumed')]+effect(targets[0])
    run+=stats()+receipts()
    if v=='two_copies':run+=pool(cost)+cast('source_second',targets[1])+[step('resolve'),step('assert_choices_consumed')]+effect(targets[1])+stats()+receipts()
    if v in ('death_owner','opponent_death'):
     consume('murder');graves[1-owner].append('Murder');e=returned[0];e['alive']=False;graves[e['owner']].append(e['name'])
     run+=[step('choose_target',player=1-owner,ref=e['ref']),step('cast',ref='murder',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]+stats()+receipts()
    if v=='cleanup':run+=[step('cleanup')]+stats()
  checks=stats()+receipts()+[step('assert_stack',value=int(v=='selection_pending')),step('assert_choices_consumed')]
  for pl in (0,1):
   checks+=[step('assert_life',player=pl,value=20),step('assert_zone',player=pl,zone='battlefield',value=(2+sum(e['alive'] for e in returned)) if pl==owner else 1)]
   for zone,ns in (('hand',hands[pl]),('graveyard',graves[pl]),('library',libs[pl]),('exile',exiles[pl])):
    if v=='selection_pending' and zone=='hand':continue
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(ns)))
    if zone=='library':checks.extend(step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(ns))
    else:checks.extend(step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=n) for nm,n in Counter(ns).items())
   if v!='selection_pending':checks.extend(step('assert_hand_graveyard_total',player=pl,index=rows[nm]['csvid'],value=n) for nm,n in Counter(hands[pl]+graves[pl]).items())
  actions=pre+run+checks
  if v in ('ready_snapshot','pending_snapshot','green_snapshot','condition_snapshot'):
   cut=pending_cut if v=='pending_snapshot' else ready_cut;tail=run[cut:]+checks
   actions=pre+run[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  result.append(dict(name=f'GrowthCounterSpellLinkedRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,_recipe=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components']+['multiplayer','replacement effects and graveyard shroud'])))
 return result


def _growth_manifest_variants(p):
 modes=['effect','payment','insufficient_mana','wrong_color','cleanup','ready_snapshot','pending_snapshot','two_copies','countered','empty_library','creature_top','artifact_top','preexisting']
 if p['variable']:modes+=['zero','large_x']
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]


def _growth_manifest_expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for requested in selected:
  v,owner=requested.rsplit('_',1);owner=int(owner);x=0 if v=='zero' else 5 if v=='large_x' else 3
  amount=x if p['variable'] else p['amount'];cost=list(p['mana'])
  if p['variable']:cost[0]+=x
  hands=[list(HAND),list(HAND)];graves=[['Grizzly Bears','Giant Growth','Forest'] for _ in (0,1)]
  libs=[list(LIBRARY),list(LIBRARY)];exiles=[list(EXILE),list(EXILE)];mana=[[10]*6,[10]*6]
  if v=='empty_library':libs[owner]=[]
  if v=='creature_top':libs[owner]=['Elvish Warrior']+libs[owner]
  if v=='artifact_top':libs[owner]=['Sol Ring']+libs[owner]
  setup=[];run=[];faces=[];kept=0
  pre=[step('turn',player=owner,value=0),step('reset_death_history')]
  def reserve(nm,ref,pl):setup.append(card(nm,ref,pl,'hand'));hands[pl].append(nm)
  reserve(name,'source',owner)
  if v=='two_copies':reserve(name,'source_second',owner)
  if v=='countered':reserve('Counterspell','counter',1-owner)
  if v=='preexisting':reserve('Arbor Armament','seed',owner)
  setup.extend([card('Grizzly Bears','kept',owner),card('Hill Giant','enemy',1-owner)])
  item=card('Sol Ring','artifact',owner);item['state']=['tapped'];setup.append(item)
  for pl in (0,1):
   for zone,ns in (('hand',list(HAND)),('graveyard',graves[pl]),('library',libs[pl]),('exile',exiles[pl])):setup.extend(card(nm,f'{zone}_{pl}_{i}',pl,zone) for i,nm in enumerate(ns))
  def pay(vector,pl):mana[pl]=[a-b for a,b in zip(mana[pl],vector)]
  def receipts():return [step('assert_mana',player=pl,color=i,value=n) for pl in (0,1) for i,n in enumerate(mana[pl])]
  def stats():
   seq=[observe('kept','in_play',1),observe('kept','plus1_counters',kept),observe('kept','power',2+kept),observe('kept','toughness',2+kept),observe('enemy','in_play',1),observe('enemy','plus1_counters',0),observe('artifact','in_play',1),observe('artifact','tapped',1)]
   for face in faces:
    seq.extend(observe(face['ref'],prop,n) for prop,n in (('in_play',1),('plus1_counters',amount),('power',2+amount),('toughness',2+amount),('damage',0),('tapped',0)))
   if faces:seq.extend(step('assert_matching_permanents',player=owner,index=p['manifest_csvid'],property=prop,value=0) for prop in ('color','artifact','token'))
   return seq
  if v=='preexisting':
   hands[owner].remove('Arbor Armament');graves[owner].append('Arbor Armament');pay(cost_from_oracle(o['Arbor Armament']),owner);kept=1
   run+=[step('choose_target',player=owner,ref='kept'),step('cast',ref='seed',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]+stats()+receipts()
  pool=list(cost) if v=='payment' else [0]*6 if v=='insufficient_mana' else None
  if v=='wrong_color':
   pool=list(cost);color=next(i for i in range(1,6) if pool[i]);pool[1 if color!=1 else 2]+=pool[color];pool[color]=0
  if pool is not None:
   mana[owner]=pool;run.extend(step('set_mana',player=owner,color=i,value=n) for i,n in enumerate(pool))
  ready_cut=len(run)
  def cast(ref):
   hands[owner].remove(name);pay(cost,owner)
   return ([step('choose_x',player=owner,value=x)] if p['variable'] else [])+[step('cast',ref=ref,value=1),step('assert_stack',value=1)]
  def effect():
   seq=[]
   if libs[owner]:
    nm=libs[owner].pop(0);ref='manifested_'+str(len(faces));faces.append(dict(name=nm,ref=ref))
    # All still-face-down copies have the same independently expected stats.
    # Rebind every copy after allocation without assuming an active-card slot.
    seq.append(step('assert_zone_multiplicity',player=owner,zone='battlefield',index=p['manifest_csvid'],value=len(faces)))
    seq.extend(step('bind_permanent_copy',player=owner,index=i,value=p['manifest_csvid'],ref=face['ref']) for i,face in enumerate(faces))
   graves[owner].append(name)
   return seq
  if v in ('insufficient_mana','wrong_color'):run+=[step('cast',ref='source',value=0),step('assert_stack',value=0),observe('source','in_hand',1)]
  else:
   run+=cast('source')+stats()+receipts();pending_cut=len(run)
   if v=='countered':
    hands[1-owner].remove('Counterspell');graves[1-owner].append('Counterspell');pay(cost_from_oracle(o['Counterspell']),1-owner);graves[owner].append(name)
    run+=[step('cast_response',ref='counter',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_choices_consumed')]
   else:
    run+=[step('resolve'),step('assert_choices_consumed')]+effect()+stats()+receipts()
    if v=='two_copies':run+=cast('source_second')+[step('resolve'),step('assert_choices_consumed')]+effect()+stats()+receipts()
    if v=='cleanup':run+=[step('cleanup')]+stats()
  checks=stats()+receipts()+[step('assert_stack',value=0),step('assert_choices_consumed')]
  for pl in (0,1):
   checks+=[step('assert_life',player=pl,value=20),step('assert_zone',player=pl,zone='battlefield',value=2+len(faces) if pl==owner else 1)]
   for zone,ns in (('hand',hands[pl]),('graveyard',graves[pl]),('library',libs[pl]),('exile',exiles[pl])):
    checks.append(step('assert_zone',player=pl,zone=zone,value=len(ns)))
    if zone=='library':checks.extend(step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(ns))
    else:checks.extend(step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[nm]['csvid'],value=n) for nm,n in Counter(ns).items())
   checks.extend(step('assert_hand_graveyard_total',player=pl,index=rows[nm]['csvid'],value=n) for nm,n in Counter(hands[pl]+graves[pl]).items())
  actions=pre+run+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=ready_cut if v=='ready_snapshot' else pending_cut;tail=run[cut:]+checks
   actions=pre+run[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  result.append(dict(name=f'GrowthCounterSpellLinkedRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,_recipe=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components']+['multiplayer','replacement effects and live control changes'])))
 return result


# Reviewed lifecycle/protection branch; earlier generation paths stay unchanged.
LIFECYCLE_HAND = ('Plains', 'Giant Growth', 'Lightning Bolt')
LIFECYCLE_GRAVE = ('Forest', 'Giant Growth')
LIFECYCLE_LIBRARY = ('Forest', 'Plains', 'Grizzly Bears', 'Mountain', 'Island', 'Lightning Bolt')
LIFECYCLE_EXILE = ('Mountain', 'Giant Growth')
LIFECYCLE_COLORS = {'B': 1, 'U': 2, 'G': 3, 'R': 4, 'W': 5}

def _lifecycle_step(op, **values):
    return dict(op=op, **values)

def _lifecycle_card(name, ref, player, zone='battlefield', tapped=False):
    item = dict(name=name, ref=ref, player=player, zone=zone)
    if tapped:
        item['state'] = ['tapped']
    return item

def _lifecycle_observe(ref, prop, value):
    return _lifecycle_step('assert_card', ref=ref, property=prop, value=value)

def _lifecycle_cost(record):
    text = record['mana_cost']
    symbols = re.findall('\\{(\\d+|[BUGRW])\\}', text)
    if ''.join(('{' + s + '}' for s in symbols)) != text:
        raise ValueError('draft requires a fixed ordinary mana cost')
    out = [0] * 6
    for symbol in symbols:
        out[0 if symbol.isdigit() else LIFECYCLE_COLORS[symbol]] += int(symbol) if symbol.isdigit() else 1
    return out

def _lifecycle_exact(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':'))

def _growth_lifecycle_spec(d, o):
    profile = LIFECYCLE_PINS['profiles'].get(o.get('name')) if isinstance(o, dict) else None
    if not profile or set(d) != {'family', 'version', 'params', 'oracle_text', 'variants'} or d['family'] != 'growth_counter_spell' or (type(d['version']) is not int) or (d['version'] != 2) or (o.get('layout') != 'normal') or o.get('card_faces') or o.get('selected_face') or any((o.get(k) != v for k, v in profile['source'].items())) or (d['oracle_text'] != profile['source']['oracle_text']) or (_lifecycle_exact(d['params']) != _lifecycle_exact(profile['params'])):
        raise ValueError('draft requires exact full source and reviewed lifecycle parameters')
    p = copy.deepcopy(profile['params'])
    p.update(family='growth_counter_spell', _lifecycle=True, mana=_lifecycle_cost(o))
    selected = d['variants']
    if not isinstance(selected, list) or not selected or len(selected) != len(set(selected)) or any((v not in _growth_lifecycle_variants(p) for v in selected)):
        raise ValueError('explicit distinct supported draft variants required')
    return p

def _growth_lifecycle_variants(p):
    modes = ['effect', 'payment', 'insufficient_mana', 'wrong_color', 'preexisting', 'cleanup', 'ready_snapshot', 'pending_snapshot', 'countered', 'lost_target', 'selection_pending', 'opposing_turn', 'wrong_type', 'empty']
    modes += ['wrong_controller'] if p['controller'] == 'self' else ['opponent']
    if p['mode'] == 'blink':
        modes += ['non_spirit', 'non_spirit_preexisting', 'repeat']
    if p['mode'] == 'undying':
        modes += ['bounce_instead', 'repeat_grant', 'returned_cleanup']
    if p['mode'] == 'protection':
        modes += ['repeat']
    if p['mode'] in ('blink', 'undying', 'delayed'):
        modes += ['foreign_owner']
    return [v + '_' + str(owner) for owner in (0, 1) for v in modes]

def _lifecycle_validate(o, registry):
    for name, pin in LIFECYCLE_PINS['witnesses'].items():
        record = o.get(name, {})
        if name not in registry or record.get('card_faces') or record.get('selected_face') or any((record.get(k) != v for k, v in pin.items())):
            raise ValueError('draft witness changed: ' + name)
    for name, profile in LIFECYCLE_PINS['profiles'].items():
        if name not in registry or registry[name]['csvid'] != profile['csvid']:
            raise ValueError('draft source registry identity changed: ' + name)

def _growth_lifecycle_expand(name, csvid, d, p, selected, registry, o):
    if not selected or len(selected) != len(set(selected)) or set(selected) - set(d['variants']) or any((v not in _growth_lifecycle_variants(p) for v in selected)):
        raise ValueError('unsupported or undeclared draft selection')
    _lifecycle_validate(o, registry)
    if csvid != LIFECYCLE_PINS['profiles'][name]['csvid']:
        raise ValueError('draft source identity mismatch')
    results = []
    for requested in selected:
        v, owner = requested.rsplit('_', 1)
        owner = int(owner)
        mode = p['mode']
        actor = 1 - owner if v == 'opposing_turn' else owner
        target_owner = 1 - owner if v in ('opponent', 'wrong_controller') else owner
        setup = []
        entities = {}
        spells = {}
        run = []
        hands = [list(LIFECYCLE_HAND), list(LIFECYCLE_HAND)]
        graves = [list(LIFECYCLE_GRAVE), list(LIFECYCLE_GRAVE)]
        libraries = [list(LIFECYCLE_LIBRARY), list(LIFECYCLE_LIBRARY)]
        exiles = [list(LIFECYCLE_EXILE), list(LIFECYCLE_EXILE)]
        mana = [[10] * 6, [10] * 6]
        life = [20, 20]
        granted = False
        protected = False
        returned = 0
        return_checks = []
        pre = [_lifecycle_step('turn', player=actor, value=0), _lifecycle_step('reset_death_history')]

        def add(nm, ref, pl, tapped=False, foreign_owned=False):
            item = _lifecycle_card(nm, ref, pl, tapped=tapped)
            if foreign_owned:
                item.setdefault('state', []).append('opponent_owned')
            setup.append(item)
            r = o[nm]
            entities[ref] = dict(name=nm, ref=ref, player=pl, owner=1 - pl if foreign_owned else pl, alive=True, exiled=False, counters=0, boost=0, damage=0, tapped=tapped, sick=False, creature='Creature' in r['type_line'])

        def reserve(nm, ref, pl):
            setup.append(_lifecycle_card(nm, ref, pl, 'hand'))
            hands[pl].append(nm)
            spells[ref] = (nm, pl)
        reserve(name, 'source', owner)
        if v in ('repeat', 'repeat_grant'):
            reserve(name, 'source_second', owner)
        if v == 'countered':
            reserve('Counterspell', 'counter', 1 - owner)
        if v in ('lost_target', 'bounce_instead'):
            reserve('Unsummon', 'response', 1 - owner)
        seeded = v in ('preexisting', 'non_spirit_preexisting')
        if seeded:
            reserve('Arbor Armament', 'seed', owner)
        if mode == 'undying' and v not in ('insufficient_mana', 'wrong_color', 'wrong_type', 'empty', 'selection_pending', 'lost_target', 'bounce_instead'):
            reserve('Murder', 'murder', 1 - target_owner)
        if mode == 'blink' and v == 'repeat':
            reserve('Giant Growth', 'pump', owner)
        if mode == 'protection':
            reserve('Lightning Bolt', 'red_probe', 1 - owner)
            reserve('Murder', 'black_probe', 1 - owner)
        if v != 'empty':
            first = 'Sol Ring' if v == 'wrong_type' else 'Kami of Ancient Law' if mode == 'blink' and v not in ('non_spirit', 'non_spirit_preexisting') else 'Grizzly Bears'
            add(first, 'first', target_owner, tapped=mode in ('blink', 'delayed'), foreign_owned=v == 'foreign_owner')
        if v not in ('empty', 'wrong_type', 'wrong_controller'):
            add('Scaled Wurm', 'kept', owner)
        if v not in ('empty', 'wrong_type'):
            add('Hill Giant', 'enemy', 1 - owner)
        add('Sol Ring', 'artifact', owner, tapped=True)
        for pl in (0, 1):
            for zone, ns in (('hand', list(LIFECYCLE_HAND)), ('graveyard', graves[pl]), ('library', libraries[pl]), ('exile', exiles[pl])):
                setup.extend((_lifecycle_card(nm, f'{zone}_{pl}_{i}', pl, zone) for i, nm in enumerate(ns)))

        def fund(pl, vector):
            mana[pl] = list(vector)
            return [_lifecycle_step('set_mana', player=pl, color=i, value=n) for i, n in enumerate(vector)]

        def pay(pl, vector):
            mana[pl] = [a - b for a, b in zip(mana[pl], vector)]

        def consume(ref):
            nm, pl = spells[ref]
            hands[pl].remove(nm)
            pay(pl, _lifecycle_cost(o[nm]))
            return (nm, pl)

        def receipts():
            return [_lifecycle_step('assert_mana', player=pl, color=i, value=n) for pl in (0, 1) for i, n in enumerate(mana[pl])]

        def stats():
            seq = []
            for e in entities.values():
                seq.append(_lifecycle_step('assert_zone_multiplicity', player=e['player'], zone='battlefield', index=registry[e['name']]['csvid'], value=sum((other['alive'] and other['player'] == e['player'] and (other['name'] == e['name']) for other in entities.values()))))
                if e['alive']:
                    seq.extend((_lifecycle_observe(e['ref'], prop, int(value)) for prop, value in (('in_play', 1), ('plus1_counters', e['counters']), ('tapped', e['tapped']), ('damage', e['damage']), ('sick', e['sick']))))
                    if e['creature']:
                        seq.extend((_lifecycle_observe(e['ref'], prop, int(o[e['name']][prop]) + e['counters'] + e['boost']) for prop in ('power', 'toughness')))
            return seq

        def auxiliary(ref, target):
            nm, pl = spells[ref]
            seq = fund(pl, _lifecycle_cost(o[nm]))
            consume(ref)
            graves[pl].append(nm)
            return seq + [_lifecycle_step('require_card', ref=target, property='in_play', value=1), _lifecycle_step('choose_target', player=pl, ref=target), _lifecycle_step('cast', ref=ref, value=1), _lifecycle_step('assert_stack', value=1), _lifecycle_step('resolve'), _lifecycle_step('assert_choices_consumed')] + receipts()

        def die(key):
            e = entities[key]
            e['alive'] = False
            graves[e['owner']].append(e['name'])

        def reenter(counter):
            nonlocal returned
            e = entities['first']
            returned += 1
            e.update(alive=True, exiled=False, counters=counter, boost=0, damage=0, tapped=False, sick=True, player=e['owner'], ref='returned_' + str(returned))
            return [_lifecycle_step('bind_permanent', player=e['player'], value=registry[e['name']]['csvid'], ref=e['ref'])]
        if seeded:
            run += auxiliary('seed', 'first')
            entities['first']['counters'] += 1
            run += stats()
        source_pool = list(p['mana'])
        if v == 'insufficient_mana':
            source_pool = [0] * 6
        if v == 'wrong_color':
            c = next((i for i in range(1, 6) if source_pool[i]))
            source_pool[1 if c != 1 else 2] += source_pool[c]
            source_pool[c] = 0
        run += fund(owner, source_pool)
        ready_cut = len(run)

        def cast(ref):
            consume(ref)
            return [_lifecycle_step('require_can_cast', ref=ref, value=1), _lifecycle_step('choose_target', player=owner, ref=entities['first']['ref']), _lifecycle_step('cast', ref=ref, value=1), _lifecycle_step('assert_stack', value=1)]

        def primary():
            nonlocal granted, protected
            e = entities['first']
            seq = []
            if not e['alive']:
                graves[owner].append(name)
                return seq
            if mode == 'blink':
                seq += reenter(int('Spirit' in o[e['name']]['type_line']))
            elif mode == 'delayed':
                e['alive'] = False
                e['exiled'] = True
                exiles[e['owner']].append(e['name'])
            elif mode == 'undying':
                granted = True
            else:
                e['counters'] += 1
                protected = True
            graves[owner].append(name)
            return seq

        def resolve_primary():
            choices = [_lifecycle_step('choose_color', player=owner, value=p['protection_color'])] if mode == 'protection' else []
            return choices + [_lifecycle_step('resolve'), _lifecycle_step('assert_choices_consumed')] + primary() + stats() + receipts()
        negative = v in ('insufficient_mana', 'wrong_color', 'wrong_type', 'empty', 'wrong_controller')
        pending_cut = 0
        if negative:
            run += [_lifecycle_step('cast' if v in ('insufficient_mana', 'wrong_color') else 'can_cast', ref='source', value=0), _lifecycle_observe('source', 'in_hand', 1), _lifecycle_step('assert_stack', value=0)]
        else:
            run += cast('source') + stats() + receipts()
            pending_cut = len(run)
            if v == 'selection_pending':
                pass
            elif v == 'countered':
                consume('counter')
                graves[1 - owner].append('Counterspell')
                graves[owner].append(name)
                run += [_lifecycle_step('cast_response', ref='counter', value=1), _lifecycle_step('assert_stack', value=2), _lifecycle_step('resolve_response'), _lifecycle_step('assert_choices_consumed')] + stats() + receipts()
            elif v == 'lost_target':
                nm, pl = spells['response']
                run += fund(pl, _lifecycle_cost(o[nm]))
                consume('response')
                graves[pl].append(nm)
                e = entities['first']
                e['alive'] = False
                hands[e['owner']].append(e['name'])
                graves[owner].append(name)
                run += [_lifecycle_step('choose_target', player=pl, ref=e['ref']), _lifecycle_step('cast_response', ref='response', value=1), _lifecycle_step('assert_stack', value=2), _lifecycle_step('resolve_response'), _lifecycle_step('assert_choices_consumed')] + stats() + receipts()
            else:
                run += resolve_primary()
                if v in ('repeat', 'repeat_grant'):
                    if mode == 'blink':
                        run += auxiliary('pump', entities['first']['ref'])
                        entities['first']['boost'] = 3
                        run += stats()
                    run += fund(owner, p['mana']) + cast('source_second') + resolve_primary()
            if v not in ('selection_pending', 'lost_target'):
                if mode == 'delayed' and v != 'countered':
                    e = entities['first']
                    run += [_lifecycle_step('assert_zone_multiplicity', player=e['owner'], zone='exile', index=registry[e['name']]['csvid'], value=1), _lifecycle_step('cleanup'), _lifecycle_step('assert_choices_consumed')]
                    exiles[e['owner']].remove(e['name'])
                    run += reenter(1) + stats() + receipts()
                if v == 'cleanup':
                    run += [_lifecycle_step('cleanup'), _lifecycle_step('turn', player=actor, value=0)]
                    granted = False
                    protected = False
                    for e in entities.values():
                        e['boost'] = 0
                        e['damage'] = 0
                    run += stats()
                if mode == 'undying':
                    e = entities['first']
                    if v == 'bounce_instead':
                        run += auxiliary('response', e['ref'])
                        e['alive'] = False
                        hands[e['owner']].append(e['name'])
                        run += stats()
                    else:
                        counter_at_death = e['counters']
                        run += auxiliary('murder', e['ref'])
                        die('first')
                        if granted and counter_at_death == 0:
                            graves[e['owner']].remove(e['name'])
                            run += reenter(1)
                            granted = False
                        run += stats() + receipts()
                        if v == 'returned_cleanup':
                            run += [_lifecycle_step('cleanup')] + stats() + receipts()
                if mode == 'protection':
                    e = entities['first']
                    run += [_lifecycle_step('require_card', ref=e['ref'], property='in_play', value=1), _lifecycle_step('can_target_creature', ref='red_probe', other=e['ref'], value=0 if protected else 1), _lifecycle_step('can_target_creature', ref='black_probe', other=e['ref'], value=1)]
                    attacking = 'enemy' if actor != owner else 'first'
                    blocking = 'first' if actor != owner else 'enemy'
                    a = entities[attacking]
                    b = entities[blocking]
                    block_legal = actor != owner or not protected
                    run += [_lifecycle_step('begin_attackers'), _lifecycle_step('attack', ref=a['ref'], value=1), _lifecycle_step('finish_attackers'), _lifecycle_step('begin_blockers'), _lifecycle_step('block', ref=b['ref'], other=a['ref'], value=int(block_legal)), _lifecycle_step('advance_combat'), _lifecycle_step('combat_damage'), _lifecycle_step('resolve'), _lifecycle_step('end_combat'), _lifecycle_step('assert_choices_consumed')]
                    a['tapped'] = True
                    if block_legal:
                        ap = int(o[a['name']]['power']) + a['counters']
                        bp = int(o[b['name']]['power']) + b['counters']
                        a['damage'] += bp
                        if not (protected and blocking == 'first'):
                            b['damage'] += ap
                        for key in (attacking, blocking):
                            c = entities[key]
                            if c['damage'] >= int(o[c['name']]['toughness']) + c['counters']:
                                die(key)
                    else:
                        life[1 - actor] -= int(o[a['name']]['power']) + a['counters']
                    run += stats() + receipts()
        checks = stats() + receipts() + [_lifecycle_step('assert_stack', value=int(v == 'selection_pending')), _lifecycle_step('assert_choices_consumed')]
        for pl in (0, 1):
            checks += [_lifecycle_step('assert_life', player=pl, value=life[pl]), _lifecycle_step('assert_zone', player=pl, zone='battlefield', value=sum((e['alive'] and e['player'] == pl for e in entities.values())))]
            for zone, ns in (('hand', hands[pl]), ('graveyard', graves[pl]), ('library', libraries[pl]), ('exile', exiles[pl])):
                if v == 'selection_pending' and zone == 'hand':
                    continue
                checks.append(_lifecycle_step('assert_zone', player=pl, zone=zone, value=len(ns)))
                if zone == 'library':
                    checks.extend((_lifecycle_step('assert_zone_card', player=pl, zone=zone, index=i, value=registry[nm]['csvid']) for i, nm in enumerate(ns)))
                else:
                    checks.extend((_lifecycle_step('assert_zone_multiplicity', player=pl, zone=zone, index=registry[nm]['csvid'], value=n) for nm, n in Counter(ns).items()))
            if v != 'selection_pending':
                checks.extend((_lifecycle_step('assert_hand_graveyard_total', player=pl, index=registry[nm]['csvid'], value=n) for nm, n in Counter(hands[pl] + graves[pl]).items()))
        actions = pre + run + checks
        if v in ('ready_snapshot', 'pending_snapshot'):
            cut = ready_cut if v == 'ready_snapshot' else pending_cut
            tail = run[cut:] + checks
            actions = pre + run[:cut] + [_lifecycle_step('capture')] + tail + [_lifecycle_step('record_state'), _lifecycle_step('ai_roundtrip'), _lifecycle_step('restore')] + copy.deepcopy(tail) + [_lifecycle_step('assert_recorded_state')]
        results.append(dict(name=f'GrowthCounterSpellLifecycleRecipe_{csvid}_{requested}', card=name, fixture='empty_main', seed=123, setup=setup, steps=actions, _recipe=dict(family='growth_counter_spell', version=2, variant=requested, params=d['params'], oracle_text=d['oracle_text'], untested_components=p['inactive_components'] + ['multiplayer', 'live controller changes after fixture setup'])))
    return results


def _growth_combat_spec(d,o):
 profile=COMBAT_PINS.get(o.get('name')) if isinstance(o,dict) else None
 if (not profile or set(d)!={'family','version','params','oracle_text','variants'}
     or d['family']!='growth_counter_spell' or type(d['version']) is not int or d['version']!=2
     or o.get('layout')!='normal' or o.get('card_faces') or o.get('selected_face')
     or any(o.get(k)!=v for k,v in profile['source'].items())
     or d['oracle_text']!=profile['source']['oracle_text'] or _lifecycle_exact(d['params'])!=_lifecycle_exact(profile['params'])):
  raise ValueError('counter combat requires complete reviewed source and parameters')
 p=copy.deepcopy(profile['params']);p.update(family='growth_counter_spell',_combat_counter=True,mana=_lifecycle_cost(o),instant=o['type_line']=='Instant')
 selected=d['variants']
 if not isinstance(selected,list) or not selected or len(selected)!=len(set(selected)) or any(v not in _growth_combat_variants(p) for v in selected):raise ValueError('explicit supported counter-combat variants required')
 return p


def _growth_combat_variants(p):
 modes=['effect','payment','insufficient_mana','wrong_color','preexisting','cleanup','ready_snapshot','pending_snapshot','countered']
 m=p['mode']
 if m=='converge_trample':modes+=['green_only','two_colors','three_colors','four_colors','opponent','lost_target','two_copies','selection_pending']
 if m=='bolster_hand_trample':modes+=['empty_hand','one_card','large_hand','two_copies']
 if m=='addendum_team':modes+=['opposing_turn','combat','two_copies']
 if m=='shield_counters':modes+=['opponent','opposing_turn','combat','expired','lost_target','selection_pending']
 if m in ('counterless_fog','opponent_fog'):modes+=['reverse_combat','unblocked','noncombat']
 if m in ('counterless_fog','opponent_fog'):modes.remove('preexisting')
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]


class _CounterCombatBoard:
 """Expected state plus ordinary script actions, shared by reviewed riders."""
 def __init__(self,name,owner,actor,rows,oracle,hand_size=None):
  self.name=name;self.owner=owner;self.rows=rows;self.o=oracle;self.setup=[];self.e={};self.spells={}
  self.hands=[list(LIFECYCLE_HAND),list(LIFECYCLE_HAND)]
  if hand_size is not None:self.hands[owner]=(list(LIFECYCLE_HAND)*4)[:hand_size]
  self.initial_hands=copy.deepcopy(self.hands);self.graves=[list(LIFECYCLE_GRAVE),list(LIFECYCLE_GRAVE)]
  self.libs=[list(LIFECYCLE_LIBRARY),list(LIFECYCLE_LIBRARY)];self.exiles=[list(LIFECYCLE_EXILE),list(LIFECYCLE_EXILE)]
  self.mana=[[10]*6,[10]*6];self.life=[20,20];self.pre=[_lifecycle_step('turn',player=actor,value=0),_lifecycle_step('reset_death_history')]
 def add(self,nm,ref,pl,tapped=False):
  self.setup.append(_lifecycle_card(nm,ref,pl,tapped=tapped));r=self.o[nm]
  self.e[ref]=dict(name=nm,player=pl,owner=pl,alive=True,power=int(r.get('power','0') or '0'),toughness=int(r.get('toughness','0') or '0'),counters=0,damage=0,tapped=tapped,creature='Creature' in r['type_line'],trample=False,vigilance=False,indestructible=False)
 def reserve(self,nm,ref,pl):self.setup.append(_lifecycle_card(nm,ref,pl,'hand'));self.hands[pl].append(nm);self.spells[ref]=(nm,pl)
 def populate(self):
  for pl in (0,1):
   for zone,ns in (('hand',self.initial_hands[pl]),('graveyard',self.graves[pl]),('library',self.libs[pl]),('exile',self.exiles[pl])):
    self.setup.extend(_lifecycle_card(nm,f'{zone}_{pl}_{i}',pl,zone) for i,nm in enumerate(ns))
 def fund(self,pl,vector):
  self.mana[pl]=list(vector);return [_lifecycle_step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(vector)]
 def consume(self,ref,vector=None):
  nm,pl=self.spells[ref];self.hands[pl].remove(nm);self.mana[pl]=[a-b for a,b in zip(self.mana[pl],vector if vector is not None else _lifecycle_cost(self.o[nm]))];return nm,pl
 def receipt(self):return [_lifecycle_step('assert_mana',player=pl,color=i,value=n) for pl in (0,1) for i,n in enumerate(self.mana[pl])]
 def stats(self):
  out=[];seen=set()
  for ref,e in self.e.items():
   key=e['player'],e['name']
   if key not in seen:
    seen.add(key);out.append(_lifecycle_step('assert_zone_multiplicity',player=e['player'],zone='battlefield',index=self.rows[e['name']]['csvid'],value=sum(other['alive'] and (other['player'],other['name'])==key for other in self.e.values())))
   if e['alive']:
    out.extend(_lifecycle_observe(ref,prop,int(n)) for prop,n in (('in_play',1),('plus1_counters',e['counters']),('damage',e['damage']),('tapped',e['tapped'])))
    if e['creature']:out.extend(_lifecycle_observe(ref,prop,e[prop]+e['counters']) for prop in ('power','toughness'))
  return out
 def die(self,ref):e=self.e[ref];e['alive']=False;self.graves[e['owner']].append(e['name'])
 def hit(self,ref,n):
  e=self.e[ref];e['damage']+=n
  if e['damage']>=e['toughness']+e['counters'] and not e['indestructible']:self.die(ref)
 def aux(self,ref,target):
  nm,pl=self.spells[ref];seq=self.fund(pl,_lifecycle_cost(self.o[nm]));self.consume(ref);self.graves[pl].append(nm)
  return seq+[_lifecycle_step('require_card',ref=target,property='in_play',value=1),_lifecycle_step('choose_target',player=pl,ref=target),_lifecycle_step('cast',ref=ref,value=1),_lifecycle_step('assert_stack',value=1),_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed')]+self.receipt()
 def expire(self):
  for e in self.e.values():e.update(damage=0,trample=False,vigilance=False,indestructible=False)
 def checks(self,pending=False):
  out=self.stats()+self.receipt()+[_lifecycle_step('assert_stack',value=int(pending)),_lifecycle_step('assert_choices_consumed')]
  for pl in (0,1):
   out.extend([_lifecycle_step('assert_life',player=pl,value=self.life[pl]),_lifecycle_step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in self.e.values()))])
   for zone,ns in (('hand',self.hands[pl]),('graveyard',self.graves[pl]),('library',self.libs[pl]),('exile',self.exiles[pl])):
    if pending and zone=='hand':continue
    out.append(_lifecycle_step('assert_zone',player=pl,zone=zone,value=len(ns)))
    if zone=='library':out.extend(_lifecycle_step('assert_zone_card',player=pl,zone=zone,index=i,value=self.rows[nm]['csvid']) for i,nm in enumerate(ns))
    else:out.extend(_lifecycle_step('assert_zone_multiplicity',player=pl,zone=zone,index=self.rows[nm]['csvid'],value=n) for nm,n in Counter(ns).items())
   if not pending:out.extend(_lifecycle_step('assert_hand_graveyard_total',player=pl,index=self.rows[nm]['csvid'],value=n) for nm,n in Counter(self.hands[pl]+self.graves[pl]).items())
  return out


def _growth_combat_expand(name,csvid,d,p,selected,rows,o):
 if not selected or len(selected)!=len(set(selected)) or set(selected)-set(d['variants']) or any(v not in _growth_combat_variants(p) for v in selected):raise ValueError('unsupported counter combat selection')
 if csvid!=COMBAT_PINS[name]['csvid']:raise ValueError('counter combat identity changed')
 _lifecycle_validate(o,rows)
 if p['mode'] in ('shield_counters','counterless_fog','opponent_fog'):return _growth_prevention_expand(name,csvid,d,p,selected,rows,o)
 return _growth_rider_expand(name,csvid,d,p,selected,rows,o)


def _growth_rider_expand(name,csvid,d,p,selected,rows,o):
 results=[];mode=p['mode']
 for requested in selected:
  v,owner=requested.rsplit('_',1);owner=int(owner);actor=1-owner if v=='opponent' or v=='opposing_turn' else owner
  hand_size={'empty_hand':0,'one_card':1,'large_hand':6}.get(v)
  b=_CounterCombatBoard(name,owner,actor,rows,o,hand_size);run=[]
  b.reserve(name,'source',owner)
  if v=='two_copies':b.reserve(name,'source_second',owner)
  if v=='countered':b.reserve('Counterspell','counter',1-owner)
  if v=='lost_target':b.reserve('Unsummon','response',1-owner)
  if v=='preexisting':b.reserve('Arbor Armament','seed',owner)
  if mode=='addendum_team':
   for ref in ('murder_first','murder_second','murder_enemy'):b.reserve('Murder',ref,1-owner)
  creature_owner=actor if mode=='converge_trample' else owner
  b.add('Grizzly Bears','first',creature_owner);b.add('Savannah Lions','second',creature_owner);b.add('Scaled Wurm','unselected',owner);b.add('Sol Ring','artifact',owner,True)
  for i in range(3):b.add('Suntail Hawk','blocker_'+str(i),1-actor)
  b.add('Hill Giant','enemy',1-owner);b.populate()
  if v=='preexisting':
   run+=b.aux('seed','unselected');b.e['unselected']['counters']+=1;run+=b.stats()
  if mode=='addendum_team' and v=='combat':
   run+=[_lifecycle_step('begin_attackers'),_lifecycle_step('attack',ref='first',value=1),_lifecycle_step('finish_attackers')];b.e['first']['tapped']=True
  payment=list(p['mana'])
  if mode=='converge_trample':payment={'green_only':[0,0,0,4,0,0],'two_colors':[0,0,0,2,2,0],'three_colors':[0,0,1,2,1,0],'four_colors':[0,1,1,1,1,0]}.get(v,payment)
  pool=list(payment)
  if v=='insufficient_mana':pool=[0]*6
  if v=='wrong_color':c=next(i for i in range(1,6) if pool[i]);pool[1 if c!=1 else 2]+=pool[c];pool[c]=0
  run+=b.fund(owner,pool);ready_cut=len(run)
  def cast(ref):
   b.consume(ref,payment)
   choices=[]
   if mode=='converge_trample' and owner==0 and v in ('green_only','two_colors','three_colors','four_colors'):
    choices=[_lifecycle_step('choose_mana',player=owner,color=c) for c in range(1,6) for _ in range(payment[c]-int(c==3))]
   return [_lifecycle_step('require_can_cast',ref=ref,value=1)]+choices+([_lifecycle_step('choose_target',player=owner,ref='first')] if mode=='converge_trample' else [])+[_lifecycle_step('cast',ref=ref,value=1),_lifecycle_step('assert_stack',value=1)]
  def effect():
   seq=[]
   if mode=='converge_trample':
    if b.e['first']['alive']:b.e['first']['counters']+=sum(n>0 for n in payment[1:]);b.e['first']['trample']=True
   elif mode=='bolster_hand_trample':
    amount=len(b.hands[owner]);candidates=[ref for ref,e in b.e.items() if e['alive'] and e['creature'] and e['player']==owner]
    if amount and candidates:
     low=min(b.e[ref]['toughness']+b.e[ref]['counters'] for ref in candidates);tied=[ref for ref in candidates if b.e[ref]['toughness']+b.e[ref]['counters']==low];ref=tied[0]
     if len(tied)>1:seq.append(_lifecycle_step('choose_target',player=owner,ref=ref))
     b.e[ref]['counters']+=amount
    for e in b.e.values():
     if e['alive'] and e['creature'] and e['player']==owner and e['counters']:e['trample']=True
   else:
    for e in b.e.values():
     if e['alive'] and e['creature'] and e['player']==owner:
      e['indestructible']=True
      if v not in ('opposing_turn','combat'):e['counters']+=1;e['vigilance']=True
   b.graves[owner].append(name);return seq
  if v in ('insufficient_mana','wrong_color'):run+=[_lifecycle_step('cast',ref='source',value=0),_lifecycle_observe('source','in_hand',1)]
  else:
   run+=cast('source')+b.stats()+b.receipt();pending_cut=len(run)
   if v=='selection_pending':pass
   else:
    if v=='countered':
     run+=b.fund(1-owner,_lifecycle_cost(o['Counterspell']));b.consume('counter');b.graves[1-owner].append('Counterspell');b.graves[owner].append(name)
     run+=[_lifecycle_step('cast_response',ref='counter',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')]
    elif v=='lost_target':
     run+=b.fund(1-owner,_lifecycle_cost(o['Unsummon']));b.consume('response');b.graves[1-owner].append('Unsummon');e=b.e['first'];e['alive']=False;b.hands[e['owner']].append(e['name']);b.graves[owner].append(name)
     run+=[_lifecycle_step('choose_target',player=1-owner,ref='first'),_lifecycle_step('cast_response',ref='response',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')]
    else:run+=effect()+[_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed')]
    run+=b.stats()+b.receipt()
    if v=='two_copies':run+=b.fund(owner,payment)+cast('source_second')+effect()+[_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed')]+b.stats()+b.receipt()
    if v=='cleanup':run+=[_lifecycle_step('cleanup'),_lifecycle_step('turn',player=actor,value=0)];b.expire();run+=b.stats()
    if mode=='addendum_team':
     for ref,target in (('murder_first','first'),('murder_second','second'),('murder_enemy','enemy')):
      run+=b.aux(ref,target)
      if not b.e[target]['indestructible']:b.die(target)
      run+=b.stats()
     if b.e['first']['alive'] and actor==owner:
      if v!='combat':run+=[_lifecycle_step('require_card',ref='first',property='in_play',value=1),_lifecycle_step('begin_attackers'),_lifecycle_step('attack',ref='first',value=1),_lifecycle_step('finish_attackers')];b.e['first']['tapped']=not b.e['first']['vigilance']
      run+=[_lifecycle_step('begin_blockers'),_lifecycle_step('advance_combat'),_lifecycle_step('combat_damage'),_lifecycle_step('resolve'),_lifecycle_step('end_combat'),_lifecycle_step('assert_choices_consumed')];b.life[1-owner]-=b.e['first']['power']+b.e['first']['counters'];run+=b.stats()
    else:
     marked=[ref for ref,e in b.e.items() if e['alive'] and e['creature'] and e['player']==actor and e['counters']]
     control=next((ref for ref,e in b.e.items() if e['alive'] and e['creature'] and e['player']==actor and not e['counters']),None)
     attackers=marked+([control] if control else [])
     run+=[_lifecycle_step('require_card',ref=ref,property='in_play',value=1) for ref in attackers]+[_lifecycle_step('begin_attackers')]+[_lifecycle_step('attack',ref=ref,value=1) for ref in attackers]+[_lifecycle_step('finish_attackers'),_lifecycle_step('begin_blockers')]
     for i,ref in enumerate(attackers):run.append(_lifecycle_step('block',ref='blocker_'+str(i),other=ref,value=1))
     run+=[_lifecycle_step('advance_combat'),_lifecycle_step('combat_damage'),_lifecycle_step('resolve'),_lifecycle_step('end_combat'),_lifecycle_step('assert_choices_consumed')]
     for i,ref in enumerate(attackers):
      a=b.e[ref];block='blocker_'+str(i);a['tapped']=True;power=a['power']+a['counters'];assigned=min(power,1) if a['trample'] else power
      if a['trample']:b.life[1-actor]-=max(0,power-1)
      b.hit(block,assigned);b.hit(ref,1)
     run+=b.stats()+b.receipt()
  checks=b.checks(v=='selection_pending');actions=b.pre+run+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=ready_cut if v=='ready_snapshot' else pending_cut;tail=run[cut:]+checks
   actions=b.pre+run[:cut]+[_lifecycle_step('capture')]+tail+[_lifecycle_step('record_state'),_lifecycle_step('ai_roundtrip'),_lifecycle_step('restore')]+copy.deepcopy(tail)+[_lifecycle_step('assert_recorded_state')]
  results.append(dict(name=f'GrowthCounterSpellCombatRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=b.setup,steps=actions,_recipe=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components']+['multiplayer','live control changes'])))
 return results


def _growth_prevention_expand(name,csvid,d,p,selected,rows,o):
 results=[];mode=p['mode'];shield=mode=='shield_counters'
 for requested in selected:
  v,owner=requested.rsplit('_',1);owner=int(owner)
  actor=1-owner if v in ('opposing_turn','combat','reverse_combat') else owner
  b=_CounterCombatBoard(name,owner,actor,rows,o);run=[]
  b.reserve(name,'source',owner)
  if v=='countered':b.reserve('Counterspell','counter',1-owner)
  if v=='lost_target':b.reserve('Unsummon','response',1-owner)
  if shield:
   target_player=1-owner if v=='opponent' else owner
   b.add('Grizzly Bears','first',target_player);b.add('Scaled Wurm','unselected',owner);b.add('Hill Giant','enemy',1-owner)
   for nm,ref in (('Lightning Bolt','bolt'),('Shock','shock'),('Lightning Bolt','control_bolt')):b.reserve(nm,ref,owner)
   if v=='preexisting':b.reserve('Arbor Armament','seed',owner)
  else:
   b.add('Grizzly Bears','attack_a',actor);b.add('Hill Giant','attack_b',actor)
   b.add('Grizzly Bears','block_a',1-actor);b.add('Scaled Wurm','block_b',1-actor)
   for ref in ('seed_attack','seed_block'):b.reserve('Arbor Armament',ref,owner)
   if v=='noncombat':b.reserve('Lightning Bolt','bolt',owner)
  b.add('Sol Ring','artifact',owner,True);b.populate()
  seeds=[('seed','first')] if shield and v=='preexisting' else ([] if shield else [('seed_attack','attack_b'),('seed_block','block_a')])
  for ref,target in seeds:
   run+=b.aux(ref,target);b.e[target]['counters']+=1;run+=b.stats()
  payment=list(p['mana']);pool=list(payment)
  if v=='insufficient_mana':pool=[0]*6
  if v=='wrong_color':c=next(i for i in range(1,6) if pool[i]);pool[1 if c!=1 else 2]+=pool[c];pool[c]=0
  run+=b.fund(owner,pool);ready_cut=len(run)
  if v in ('insufficient_mana','wrong_color'):run+=[_lifecycle_step('cast',ref='source',value=0),_lifecycle_observe('source','in_hand',1)]
  else:
   b.consume('source');run+=[_lifecycle_step('require_can_cast',ref='source',value=1)]
   if shield:run.append(_lifecycle_step('choose_target',player=owner,ref='first'))
   run+=[_lifecycle_step('cast',ref='source',value=1),_lifecycle_step('assert_stack',value=1)]+b.stats()+b.receipt();pending_cut=len(run)
   if v!='selection_pending':
    active=v not in ('countered','lost_target')
    b.graves[owner].append(name)
    if v=='countered':
     run+=b.fund(1-owner,_lifecycle_cost(o['Counterspell']));b.consume('counter');b.graves[1-owner].append('Counterspell')
     run+=[_lifecycle_step('cast_response',ref='counter',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')]
    elif v=='lost_target':
     run+=b.fund(1-owner,_lifecycle_cost(o['Unsummon']));b.consume('response');b.graves[1-owner].append('Unsummon');b.e['first']['alive']=False;b.hands[b.e['first']['owner']].append('Grizzly Bears')
     run+=[_lifecycle_step('choose_target',player=1-owner,ref='first'),_lifecycle_step('cast_response',ref='response',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')]
    else:run+=[_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed')]
    run+=b.stats()+b.receipt()
    if v in ('expired','cleanup'):
     run+=[_lifecycle_step('cleanup'),_lifecycle_step('turn',player=actor,value=0)];b.expire();active=False;run+=b.stats()
    if shield:
     remaining=3 if active else 0
     if v=='combat':
      run+=[_lifecycle_step('begin_attackers'),_lifecycle_step('attack',ref='enemy',value=1),_lifecycle_step('finish_attackers'),_lifecycle_step('begin_blockers'),_lifecycle_step('block',ref='first',other='enemy',value=1),_lifecycle_step('advance_combat'),_lifecycle_step('combat_damage'),_lifecycle_step('resolve'),_lifecycle_step('end_combat'),_lifecycle_step('assert_choices_consumed')]
      own_power=b.e['first']['power']+b.e['first']['counters'];prevented=min(3,remaining);b.e['first']['counters']+=prevented;b.e['enemy']['tapped']=True;b.hit('first',3-prevented);b.hit('enemy',own_power);run+=b.stats()
     elif b.e['first']['alive']:
      for ref,damage in (('bolt',3),('shock',2)):
       if not b.e['first']['alive']:break
       run+=b.aux(ref,'first');prevented=min(damage,remaining);remaining-=prevented;b.e['first']['counters']+=prevented;b.hit('first',damage-prevented);run+=b.stats()
     run+=b.aux('control_bolt','unselected');b.hit('unselected',3);run+=b.stats()
    elif v=='noncombat':
     run+=b.aux('bolt','block_b');b.hit('block_b',3);run+=b.stats()
    else:
     attackers=('attack_a','attack_b');run+=[_lifecycle_step('begin_attackers')]+[_lifecycle_step('attack',ref=ref,value=1) for ref in attackers]+[_lifecycle_step('finish_attackers'),_lifecycle_step('begin_blockers')]
     if v!='unblocked':run+=[_lifecycle_step('block',ref='block_a',other='attack_a',value=1),_lifecycle_step('block',ref='block_b',other='attack_b',value=1)]
     run+=[_lifecycle_step('advance_combat'),_lifecycle_step('combat_damage'),_lifecycle_step('resolve'),_lifecycle_step('end_combat'),_lifecycle_step('assert_choices_consumed')]
     def dealt(ref):
      e=b.e[ref]
      prevented=active and ((mode=='counterless_fog' and not e['counters']) or (mode=='opponent_fog' and e['player']==1-owner))
      return 0 if prevented else e['power']+e['counters']
     assignments=[('attack_a','block_a'),('attack_b','block_b')]
     damage=[(a,z,dealt(a),dealt(z)) for a,z in assignments]
     for a,z,ad,zd in damage:
      b.e[a]['tapped']=True
      if v=='unblocked':b.life[1-actor]-=ad
      else:b.hit(z,ad);b.hit(a,zd)
     run+=b.stats()
  checks=b.checks(v=='selection_pending');actions=b.pre+run+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=ready_cut if v=='ready_snapshot' else pending_cut;tail=run[cut:]+checks
   actions=b.pre+run[:cut]+[_lifecycle_step('capture')]+tail+[_lifecycle_step('record_state'),_lifecycle_step('ai_roundtrip'),_lifecycle_step('restore')]+copy.deepcopy(tail)+[_lifecycle_step('assert_recorded_state')]
  results.append(dict(name=f'GrowthCounterSpellCombatRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=b.setup,steps=actions,_recipe=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components']+['multiplayer','live control changes'])))
 return results


def _growth_action_spec(d,o):
 profile=ACTION_PINS.get(o.get('name')) if isinstance(o,dict) else None
 if (not profile or set(d)!={'family','version','params','oracle_text','variants'} or d['family']!='growth_counter_spell'
     or type(d['version']) is not int or d['version']!=2 or o.get('card_faces') or o.get('selected_face')
     or any(o.get(k)!=v for k,v in profile['source'].items()) or d['oracle_text']!=profile['source']['oracle_text']
     or _lifecycle_exact(d['params'])!=_lifecycle_exact(profile['params'])):raise ValueError('whole reviewed counter action required')
 base=[0,0,0,1,0,0] if profile['params']['mode']=='transfer' else _lifecycle_cost(dict(mana_cost=o['mana_cost'].replace('{X}','')))
 p=copy.deepcopy(profile['params']);p.update(family='growth_counter_spell',_counter_action=True,mana=base)
 if not isinstance(d['variants'],list) or not d['variants'] or len(d['variants'])!=len(set(d['variants'])) or any(v not in _growth_action_variants(p) for v in d['variants']):raise ValueError('explicit supported counter actions required')
 return p

def _growth_action_variants(p):
 if p['mode']=='grave_single_allocation':return _growth_spoils_variants(p)
 if p['mode'] in ('next_creature','attack_lifelink'):return _growth_followup_variants(p)
 if p['mode']=='untap_lock':return _growth_rush_variants(p)
 if p['mode'] in ('mutiny','support_control'):return _growth_control_variants(p)
 modes=['effect','payment','insufficient_mana','wrong_color','cleanup','ready_snapshot','pending_snapshot','countered','selection_pending','lost_target','two_copies','preexisting']
 if p['mode']=='transfer':modes+=['opponent','blue_payment','empty_donor','wrong_controller']
 elif p['mode']=='thrive':modes+=['one_target','three_targets','opponent','partial_target','zero_targets']
 elif p['mode']=='multikicker':modes+=['kicked','x_one','x_five','opponent','partial_target']
 elif p['mode']=='artifact_animation':modes+=['artifact_creature','vehicle','x_zero','x_one','x_five','wrong_controller','wrong_type']
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]+(['partial_amount_0','zero_amount_0'] if p['mode']=='transfer' else [])

def _growth_action_expand(name,csvid,d,p,selected,rows,o):
 _lifecycle_validate(o,rows)
 if csvid!=ACTION_PINS[name]['csvid'] or not selected or set(selected)-set(d['variants']) or len(selected)!=len(set(selected)) or any(v not in _growth_action_variants(p) for v in selected):raise ValueError('undeclared counter action')
 if p['mode']=='grave_single_allocation':return _growth_spoils_expand(name,csvid,d,p,selected,rows,o)
 if p['mode'] in ('next_creature','attack_lifelink'):return _growth_followup_expand(name,csvid,d,p,selected,rows,o)
 if p['mode']=='untap_lock':return _growth_rush_expand(name,csvid,d,p,selected,rows,o)
 if p['mode'] in ('mutiny','support_control'):return _growth_control_expand(name,csvid,d,p,selected,rows,o)
 results=[];mode=p['mode']
 for requested in selected:
  v,owner=requested.rsplit('_',1);owner=int(owner);b=_CounterCombatBoard(name,owner,owner,rows,o);run=[]
  if mode=='thrive' and v=='zero_targets':
   b.add('Sol Ring','artifact',owner,True);b.reserve(name,'source',owner);b.populate()
   run+=b.fund(owner,p['mana'])+[_lifecycle_step('can_cast',ref='source',value=1),_lifecycle_observe('source','in_hand',1)]
   results.append(dict(name=f'GrowthCounterSpellActionRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=b.setup,steps=b.pre+run+b.checks(),_recipe=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components']+['This legal X=0 no-target permission probe ends before casting to avoid a failed-precondition cascade; other selected cases perform real productive casts.'])))
   continue
  recipient=1-owner if v in ('opponent','wrong_controller') else owner
  artifact=mode=='artifact_animation';transfer=mode=='transfer';x={'x_zero':0,'x_one':1,'x_five':5}.get(v,3)
  targets=['first','second'] if mode=='thrive' and v!='one_target' else ['first']
  if mode=='thrive' and v=='three_targets':targets.append('third')
  if mode=='multikicker' and v in ('kicked','partial_target'):targets.append('second')
  if transfer:targets=['first','second']
  first_name=('Phyrexian Hulk' if v in ('artifact_creature','preexisting') else 'Sky Skiff' if v=='vehicle' else 'Grizzly Bears' if v=='wrong_type' else 'Darksteel Relic') if artifact else 'Grizzly Bears'
  b.add(first_name,'first',recipient);b.add('Savannah Lions','second',1-owner if v=='wrong_controller' else recipient);b.add('Scaled Wurm','third',owner);b.add('Hill Giant','enemy',1-owner)
  if not artifact:b.add('Sol Ring','artifact',owner,True)
  if transfer and v=='wrong_controller':
   b.e['first']['player']=b.e['first']['owner']=owner
   next(a for a in b.setup if a['ref']=='first')['player']=owner
   for ref in ('third','enemy'):del b.e[ref]
   b.setup=[a for a in b.setup if a['ref'] not in ('third','enemy')]
  b.reserve(name,'source',owner)
  if v=='two_copies':b.reserve(name,'source_second',owner)
  if v=='countered':b.reserve('Counterspell','counter',1-owner)
  if v=='lost_target':b.reserve('Unsummon','response',1-owner) if not artifact or v=='artifact_creature' else b.reserve('Disperse','response',1-owner)
  seeds=[]
  if transfer:
   seeds=[] if v=='empty_donor' else ['first']*3
   seeds+=['second']*(2 if v=='preexisting' else 1)
  elif v=='preexisting':seeds=['first']
  for i,target in enumerate(seeds):b.reserve('Arbor Armament','seed_'+str(i),owner)
  b.populate()
  for i,target in enumerate(seeds):run+=b.aux('seed_'+str(i),target);b.e[target]['counters']+=1;run+=b.stats()
  payment=list(p['mana'])
  if v=='blue_payment':payment=[0,0,1,0,0,0]
  if not transfer:payment[0]+=len(targets) if mode=='thrive' else x+(len(targets)-1 if mode=='multikicker' else 0)
  pool=list(payment)
  if v=='insufficient_mana':pool=[0]*6
  if v=='wrong_color':pool=[sum(payment),0,0,0,0,0]
  run+=b.fund(owner,pool);ready_cut=len(run)
  negative=v in ('insufficient_mana','wrong_color','wrong_controller','wrong_type')
  def cast(ref,chosen):
   b.consume(ref,payment);seq=[_lifecycle_step('require_can_cast',ref=ref,value=1)]
   if mode in ('multikicker','artifact_animation'):seq.append(_lifecycle_step('choose_x',player=owner,value=x))
   if mode=='multikicker':
    seq.append(_lifecycle_step('choose_target',player=owner,ref=chosen[0]))
    for target in chosen[1:]:seq.extend([_lifecycle_step('choose_dialog',player=owner,value=0),_lifecycle_step('choose_target',player=owner,ref=target)])
   else:seq.extend(_lifecycle_step('choose_target',player=owner,ref=target) for target in chosen)
   return seq+[_lifecycle_step('cast',ref=ref,value=1),_lifecycle_step('assert_stack',value=1)]
  def effect(chosen):
   seq=[]
   if transfer:
    if all(b.e[t]['alive'] for t in chosen):
     amount={'partial_amount':2,'zero_amount':0}.get(v,b.e[chosen[0]]['counters'])
     if b.e[chosen[0]]['counters'] and owner==0:seq.append(_lifecycle_step('choose_number',player=owner,value=amount))
     b.e[chosen[0]]['counters']-=amount;b.e[chosen[1]]['counters']+=amount
   elif artifact:
    e=b.e['first']
    if e['alive']:
     e['counters']+=x
     if v!='vehicle' and not e['creature']:e.update(creature=True,power=0,toughness=0,animated=True)
     if e['creature'] and e['toughness']+e['counters']==0:b.die('first')
   else:
    for t in chosen:
     if b.e[t]['alive']:b.e[t]['counters']+=1 if mode=='thrive' else x
   b.graves[owner].append(name);return seq
  if negative:
   run+=[_lifecycle_step('cast' if v in ('insufficient_mana','wrong_color') else 'can_cast',ref='source',value=0)]
  else:
   run+=cast('source',targets)+b.stats()+b.receipt();pending_cut=len(run)
   if v!='selection_pending':
    if v=='countered':
     run+=b.fund(1-owner,_lifecycle_cost(o['Counterspell']));b.consume('counter');b.graves[1-owner].append('Counterspell');b.graves[owner].append(name)
     run+=[_lifecycle_step('cast_response',ref='counter',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')]
    elif v=='lost_target':
     response=b.spells['response'][0];run+=b.fund(1-owner,_lifecycle_cost(o[response]));b.consume('response');b.graves[1-owner].append(response);e=b.e['first'];e['alive']=False;b.hands[e['owner']].append(e['name'])
     run+=[_lifecycle_step('choose_target',player=1-owner,ref='first'),_lifecycle_step('cast_response',ref='response',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')];effect(targets)
    else:
     if v=='partial_target':run.append(_lifecycle_step('destroy',ref='second'));b.die('second')
     run+=effect(targets)+[_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed')]
    run+=b.stats()+b.receipt()
    if v=='two_copies':
     chosen=list(reversed(targets)) if transfer else targets
     run+=b.fund(owner,payment)+cast('source_second',chosen)+effect(chosen)+[_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed')]+b.stats()+b.receipt()
    if v=='cleanup':run.append(_lifecycle_step('cleanup'));b.expire();run+=b.stats()
    if artifact and b.e['first']['alive']:
     run.extend(_lifecycle_step('assert_matching_permanents',player=owner,index=rows[first_name]['csvid'],property=prop,value=n) for prop,n in [('artifact',1),('token',0),('subtype_Construct',int(b.e['first'].get('animated',False)))])
     if b.e['first']['creature']:
      run+=[_lifecycle_step('turn',player=owner,value=0),_lifecycle_step('begin_attackers'),_lifecycle_step('attack',ref='first',value=1),_lifecycle_step('finish_attackers'),_lifecycle_step('begin_blockers'),_lifecycle_step('advance_combat'),_lifecycle_step('combat_damage'),_lifecycle_step('resolve'),_lifecycle_step('end_combat')];b.e['first']['tapped']=True;b.life[1-owner]-=b.e['first']['power']+b.e['first']['counters'];run+=b.stats()
     elif v=='vehicle':run+=[_lifecycle_step('begin_attackers'),_lifecycle_step('attack',ref='first',value=0)]
  checks=b.checks(v=='selection_pending');actions=b.pre+run+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=ready_cut if v=='ready_snapshot' else pending_cut;tail=run[cut:]+checks
   actions=b.pre+run[:cut]+[_lifecycle_step('capture')]+tail+[_lifecycle_step('record_state'),_lifecycle_step('ai_roundtrip'),_lifecycle_step('restore')]+copy.deepcopy(tail)+[_lifecycle_step('assert_recorded_state')]
  results.append(dict(name=f'GrowthCounterSpellActionRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=b.setup,steps=actions,_recipe=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components'])))
 return results


def _growth_rush_variants(p):
 return [v+'_'+str(owner) for owner in (0,1) for v in ('next_step','target_self','payment','insufficient_mana','wrong_color','countered','lost_target','ready_snapshot','pending_snapshot','locked_snapshot')]

def _growth_rush_expand(name,csvid,d,p,selected,rows,o):
 results=[]
 for requested in selected:
  v,owner=requested.rsplit('_',1);owner=int(owner);recipient=owner if v=='target_self' else 1-owner
  b=_CounterCombatBoard(name,owner,owner,rows,o);run=[]
  b.add('Grizzly Bears','first',recipient);b.add('Savannah Lions','unselected',recipient,True);b.add('Scaled Wurm','other_control',1-recipient,True)
  b.reserve(name,'source',owner)
  if v=='countered':b.reserve('Counterspell','counter',1-owner)
  if v=='lost_target':b.reserve('Unsummon','response',1-owner)
  b.populate()
  run+=b.fund(0,[0]*6)+b.fund(1,[0]*6)
  payment=list(p['mana']);pool=[0]*6 if v=='insufficient_mana' else ([1,0,0,0,0,0] if v=='wrong_color' else payment)
  run+=b.fund(owner,pool);ready_cut=len(run)
  if v in ('insufficient_mana','wrong_color'):run+=[_lifecycle_step('cast',ref='source',value=0)]
  else:
   b.consume('source');run+=[_lifecycle_step('require_can_cast',ref='source',value=1),_lifecycle_step('choose_target',player=owner,ref='first'),_lifecycle_step('cast',ref='source',value=1),_lifecycle_step('assert_stack',value=1)]+b.stats()+b.receipt();pending_cut=len(run)
   b.graves[owner].append(name);locked=v not in ('countered','lost_target')
   if v=='countered':
    run+=b.fund(1-owner,_lifecycle_cost(o['Counterspell']));b.consume('counter');b.graves[1-owner].append('Counterspell')
    run+=[_lifecycle_step('cast_response',ref='counter',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')]
   elif v=='lost_target':
    run+=b.fund(1-owner,_lifecycle_cost(o['Unsummon']));b.consume('response');b.graves[1-owner].append('Unsummon');b.e['first']['alive']=False;b.hands[recipient].append('Grizzly Bears')
    run+=[_lifecycle_step('choose_target',player=1-owner,ref='first'),_lifecycle_step('cast_response',ref='response',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')]
   else:run+=[_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed')];b.e['first']['tapped']=True
   run+=b.stats()+b.receipt();current=owner;visits=[0,0];locked_cut=None
   while visits[recipient]<2 or min(visits)<1:
    current=1-current;visits[current]+=1
    run+=[_lifecycle_step('advance_turn'),_lifecycle_step('untap_step',player=current)]
    for ref,e in b.e.items():
     if e['alive'] and e['player']==current and not (ref=='first' and locked and visits[current]==1):e['tapped']=False
    run+=b.stats()+b.receipt()+[_lifecycle_step('assert_stack',value=0),_lifecycle_step('assert_choices_consumed')]
    if current==recipient and visits[current]==1:locked_cut=len(run)
  checks=b.checks();actions=b.pre+run+checks
  if v in ('ready_snapshot','pending_snapshot','locked_snapshot'):
   cut={'ready_snapshot':ready_cut,'pending_snapshot':pending_cut,'locked_snapshot':locked_cut}[v];tail=run[cut:]+checks
   actions=b.pre+run[:cut]+[_lifecycle_step('capture')]+tail+[_lifecycle_step('record_state'),_lifecycle_step('ai_roundtrip'),_lifecycle_step('restore')]+copy.deepcopy(tail)+[_lifecycle_step('assert_recorded_state')]
  results.append(dict(name=f'GrowthCounterSpellUntapRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=b.setup,steps=actions,_recipe=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components'])))
 return results


def _growth_control_variants(p):
 modes=['effect','payment','insufficient_mana','wrong_color','cleanup','ready_snapshot','pending_snapshot','selection_pending','target_self','foreign_owner','death_before_return']
 if p['mode']=='mutiny':modes+=['countered','lost_target','preexisting']

 return [v+'_'+str(owner) for owner in (0,1) for v in modes]

def _growth_control_expand(name,csvid,d,p,selected,rows,o):
 results=[];support=p['mode']=='support_control'
 for requested in selected:
  v,owner=requested.rsplit('_',1);owner=int(owner);original=owner if v=='target_self' else 1-owner
  b=_CounterCombatBoard(name,owner,owner,rows,o);run=[]
  b.add('Hill Giant','first',original,True);b.add('Grizzly Bears','second',owner);b.add('Savannah Lions','third',owner);b.add('Scaled Wurm','unselected',1-owner);b.add('Sol Ring','artifact',owner,True)
  if v=='foreign_owner':b.e['first']['owner']=owner;next(a for a in b.setup if a['ref']=='first')['state'].append('opponent_owned')
  b.reserve(name,'source',owner)
  if v=='countered':b.reserve('Counterspell','counter',1-owner)
  if v=='lost_target':b.reserve('Unsummon','response',1-owner)
  if v=='preexisting':b.reserve('Arbor Armament','seed',owner)
  if v=='death_before_return':b.reserve('Murder','murder',1-owner)
  b.populate()
  if v=='preexisting':run+=b.aux('seed','first');b.e['first']['counters']+=1;run+=b.stats()
  payment=list(p['mana']);pool=list(payment)
  if v=='insufficient_mana':pool=[0]*6
  if v=='wrong_color':pool=[sum(payment),0,0,0,0,0]
  run+=b.fund(owner,pool);ready_cut=len(run)
  if v in ('insufficient_mana','wrong_color'):run.append(_lifecycle_step('cast',ref='source',value=0))
  else:
   b.consume('source');run+=[_lifecycle_step('require_can_cast',ref='source',value=1),_lifecycle_step('choose_target',player=owner,ref='first')]
   support_targets=['first','second'] if v=='support_stolen' else ['second','third']
   if support:run.extend(_lifecycle_step('choose_target',player=owner,ref=t) for t in support_targets)
   run+=[_lifecycle_step('cast',ref='source',value=1),_lifecycle_step('assert_stack',value=1)]+b.stats()+b.receipt();pending_cut=len(run)
   if v!='selection_pending':
    b.graves[owner].append(name)
    if v=='countered':
     run+=b.fund(1-owner,_lifecycle_cost(o['Counterspell']));b.consume('counter');b.graves[1-owner].append('Counterspell')
     run+=[_lifecycle_step('cast_response',ref='counter',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')]
    elif v=='lost_target':
     run+=b.fund(1-owner,_lifecycle_cost(o['Unsummon']));b.consume('response');b.graves[1-owner].append('Unsummon');b.e['first']['alive']=False;b.hands[b.e['first']['owner']].append('Hill Giant')
     run+=[_lifecycle_step('choose_target',player=1-owner,ref='first'),_lifecycle_step('cast_response',ref='response',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')]
    else:
     if support:
      for target in support_targets:b.e[target]['counters']+=1
     else:b.e['first']['counters']+=1
     e=b.e.pop('first');e.update(player=owner,tapped=False);b.e['stolen']=e
     run+=[_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed'),_lifecycle_step('bind_permanent',player=owner,value=rows['Hill Giant']['csvid'],ref='stolen')]+b.stats()+b.receipt()
     if v=='death_before_return':
      run+=b.aux('murder','stolen');b.die('stolen');run+=b.stats()
     else:
      run+=[_lifecycle_step('require_card',ref='stolen',property='in_play',value=1),_lifecycle_step('begin_attackers'),_lifecycle_step('attack',ref='stolen',value=1),_lifecycle_step('finish_attackers'),_lifecycle_step('begin_blockers'),_lifecycle_step('advance_combat'),_lifecycle_step('combat_damage'),_lifecycle_step('resolve'),_lifecycle_step('end_combat')]
      b.e['stolen']['tapped']=True;b.life[1-owner]-=e['power']+e['counters'];run+=b.stats()
     run.append(_lifecycle_step('cleanup'));b.expire()
     if b.e['stolen']['alive']:
      e=b.e.pop('stolen');e['player']=original;b.e['returned']=e
      run.append(_lifecycle_step('bind_permanent',player=original,value=rows['Hill Giant']['csvid'],ref='returned'))
     run+=b.stats()
  checks=b.checks(v=='selection_pending');actions=b.pre+run+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=ready_cut if v=='ready_snapshot' else pending_cut;tail=run[cut:]+checks
   actions=b.pre+run[:cut]+[_lifecycle_step('capture')]+tail+[_lifecycle_step('record_state'),_lifecycle_step('ai_roundtrip'),_lifecycle_step('restore')]+copy.deepcopy(tail)+[_lifecycle_step('assert_recorded_state')]
  results.append(dict(name=f'GrowthCounterSpellControlRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=b.setup,steps=actions,_recipe=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components'])))
 return results


def _growth_spoils_variants(p):
 return [v+'_'+str(owner) for owner in (0,1) for v in ('effect','payment','insufficient_mana','wrong_color','preexisting','cleanup','ready_snapshot','pending_snapshot','selection_pending','countered','lost_target','two_copies','opponent','more_graves','grave_response')]

def _growth_spoils_expand(name,csvid,d,p,selected,rows,o):
 results=[]
 for requested in selected:
  v,owner=requested.rsplit('_',1);owner=int(owner);recipient=1-owner if v=='opponent' else owner
  b=_CounterCombatBoard(name,owner,owner,rows,o);run=[]
  b.graves[1-owner].extend(['Grizzly Bears','Sol Ring','Phyrexian Hulk']);b.graves[owner].extend(['Hill Giant','Darksteel Relic'])
  if v=='more_graves':b.graves[1-owner].extend(['Savannah Lions','Darksteel Relic'])
  b.add('Grizzly Bears','first',recipient);b.add('Savannah Lions','unselected',owner);b.add('Hill Giant','enemy',1-owner);b.add('Sol Ring','artifact',owner,True)
  b.reserve(name,'source',owner)
  if v=='two_copies':b.reserve(name,'source_second',owner)
  if v=='preexisting':b.reserve('Arbor Armament','seed',owner)
  if v=='countered':b.reserve('Counterspell','counter',1-owner)
  if v=='lost_target':b.reserve('Unsummon','response',1-owner)
  if v=='grave_response':b.reserve('Murder','response',owner)
  b.populate()
  if v=='preexisting':run+=b.aux('seed','first');b.e['first']['counters']+=1;run+=b.stats()
  x=sum('Artifact' in o[n]['type_line'] or 'Creature' in o[n]['type_line'] for n in b.graves[1-owner]);payment=list(p['mana']);payment[0]+=x;pool=list(payment)
  if v=='insufficient_mana':pool=[0]*6
  if v=='wrong_color':pool=[sum(payment),0,0,0,0,0]
  run+=b.fund(owner,pool);ready_cut=len(run)
  def cast(ref):
   b.consume(ref,payment)
   return [_lifecycle_step('require_can_cast',ref=ref,value=1),_lifecycle_step('choose_target',player=owner,ref='first'),_lifecycle_step('choose_cancel_target',player=owner),_lifecycle_step('cast',ref=ref,value=1),_lifecycle_step('assert_stack',value=1)]
  def effect():
   if b.e['first']['alive']:b.e['first']['counters']+=x
   b.graves[owner].append(name)
  if v in ('insufficient_mana','wrong_color'):run.append(_lifecycle_step('cast',ref='source',value=0))
  else:
   run+=cast('source')+b.stats()+b.receipt();pending_cut=len(run)
   if v!='selection_pending':
    if v=='countered':
     run+=b.fund(1-owner,_lifecycle_cost(o['Counterspell']));b.consume('counter');b.graves[1-owner].append('Counterspell');b.graves[owner].append(name)
     run+=[_lifecycle_step('cast_response',ref='counter',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')]
    elif v in ('lost_target','grave_response'):
     nm,pl=b.spells['response'];run+=b.fund(pl,_lifecycle_cost(o[nm]));b.consume('response');b.graves[pl].append(nm)
     target='first' if v=='lost_target' else 'enemy'
     if v=='lost_target':e=b.e['first'];e['alive']=False;b.hands[e['owner']].append(e['name'])
     else:b.die('enemy')
     run+=[_lifecycle_step('choose_target',player=pl,ref=target),_lifecycle_step('cast_response',ref='response',value=1),_lifecycle_step('assert_stack',value=2)]
     if v=='grave_response':run.append(_lifecycle_step('choose_dialog',player=owner,value=0))
     run+=[_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')];effect()
    else:run+=[_lifecycle_step('choose_dialog',player=owner,value=0),_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed')];effect()
    run+=b.stats()+b.receipt()
    if v=='two_copies':run+=b.fund(owner,payment)+cast('source_second')+[_lifecycle_step('choose_dialog',player=owner,value=0),_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed')];effect();run+=b.stats()+b.receipt()
    if v=='cleanup':run.append(_lifecycle_step('cleanup'));b.expire();run+=b.stats()
  checks=b.checks(v=='selection_pending');actions=b.pre+run+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=ready_cut if v=='ready_snapshot' else pending_cut;tail=run[cut:]+checks
   actions=b.pre+run[:cut]+[_lifecycle_step('capture')]+tail+[_lifecycle_step('record_state'),_lifecycle_step('ai_roundtrip'),_lifecycle_step('restore')]+copy.deepcopy(tail)+[_lifecycle_step('assert_recorded_state')]
  results.append(dict(name=f'GrowthCounterSpellGraveAllocationRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=b.setup,steps=actions,_recipe=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components'])))
 return results


def _growth_followup_variants(p):
 modes=['effect','payment','insufficient_mana','wrong_color','cleanup','ready_snapshot','pending_snapshot','countered']
 modes+=['expired','intervening','creature_counter_pending'] if p['mode']=='next_creature' else ['unblocked','noncombat','two_copies']
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]

def _growth_followup_expand(name,csvid,d,p,selected,rows,o):
 results=[];summon=p['mode']=='next_creature'
 for requested in selected:
  v,owner=requested.rsplit('_',1);owner=int(owner);b=_CounterCombatBoard(name,owner,owner,rows,o);run=[]
  b.add('Scaled Wurm','unselected',owner);b.add('Hill Giant','enemy',1-owner);b.add('Sol Ring','artifact',owner,True)
  if not summon:b.add('Grizzly Bears','first',owner)
  b.reserve(name,'source',owner)
  if v=='two_copies':b.reserve(name,'source_second',owner)
  if v in ('countered','creature_counter_pending'):b.reserve('Counterspell','counter',1-owner)
  if v in ('intervening','noncombat'):b.reserve('Shock','shock',owner)
  if summon:
   b.reserve('Grizzly Bears','creature_first',owner);b.reserve('Savannah Lions','creature_second',owner)
  b.populate();payment=list(p['mana']);pool=list(payment)
  if v=='insufficient_mana':pool=[0]*6
  if v=='wrong_color':pool=[sum(payment),0,0,0,0,0]
  run+=b.fund(owner,pool);ready_cut=len(run)
  if v in ('insufficient_mana','wrong_color'):run.append(_lifecycle_step('cast',ref='source',value=0))
  else:
   b.consume('source');run+=[_lifecycle_step('require_can_cast',ref='source',value=1),_lifecycle_step('cast',ref='source',value=1),_lifecycle_step('assert_stack',value=1)]+b.stats()+b.receipt();pending_cut=len(run)
   b.graves[owner].append(name);active=summon or v!='countered'
   if v=='countered':
    run+=b.fund(1-owner,_lifecycle_cost(o['Counterspell']));b.consume('counter');b.graves[1-owner].append('Counterspell')
    run+=[_lifecycle_step('cast_response',ref='counter',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('resolve_response'),_lifecycle_step('assert_choices_consumed')]
   else:run+=[_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed')]
   run+=b.stats()+b.receipt()
   if v=='two_copies':
    run+=b.fund(owner,payment);b.consume('source_second');b.graves[owner].append(name)
    run+=[_lifecycle_step('cast',ref='source_second',value=1),_lifecycle_step('assert_stack',value=1),_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed')]+b.receipt()
   if v=='expired' or (v=='cleanup' and not summon):
    run+=[_lifecycle_step('cleanup'),_lifecycle_step('turn',player=owner,value=0)];b.expire();active=False;run+=b.stats()
   if v in ('intervening','noncombat'):
    run+=b.aux('shock','enemy');b.hit('enemy',2);run+=b.stats()+[_lifecycle_step('assert_life',player=owner,value=20),_lifecycle_step('assert_life',player=1-owner,value=20)]
   if summon:
    nm='Grizzly Bears';cost=_lifecycle_cost(o[nm]);run+=b.fund(owner,cost);b.consume('creature_first')
    run+=[_lifecycle_step('cast',ref='creature_first',value=1),_lifecycle_step('assert_stack',value=1)]+b.receipt()
    if v=='creature_counter_pending':
     run+=b.fund(1-owner,_lifecycle_cost(o['Counterspell']));b.consume('counter')
     run+=[_lifecycle_step('cast_response',ref='counter',value=1),_lifecycle_step('assert_stack',value=2),_lifecycle_step('assert_choices_consumed')]
    else:
     for nm,source,ref,counters in [('Grizzly Bears','creature_first','entered_first',int(active)),('Savannah Lions','creature_second','entered_second',0)]:
      if source=='creature_second':
       run+=b.fund(owner,_lifecycle_cost(o[nm]));b.consume(source);run+=[_lifecycle_step('cast',ref=source,value=1),_lifecycle_step('assert_stack',value=1)]
      b.e[ref]=dict(name=nm,player=owner,owner=owner,alive=True,power=int(o[nm]['power']),toughness=int(o[nm]['toughness']),counters=counters,damage=0,tapped=False,creature=True,trample=False,vigilance=False,indestructible=False)
      run+=[_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed'),_lifecycle_step('bind_permanent',player=owner,value=rows[nm]['csvid'],ref=ref),_lifecycle_observe(ref,'sick',1)]+b.stats()+b.receipt()
     if v=='cleanup':run.append(_lifecycle_step('cleanup'));b.expire();run+=b.stats()
   elif v!='noncombat':
    attackers=['first','unselected'] if v=='unblocked' else ['first']
    run+=[_lifecycle_step('begin_attackers')]+[_lifecycle_step('attack',ref=ref,value=1) for ref in attackers]+[_lifecycle_step('finish_attackers'),_lifecycle_step('resolve'),_lifecycle_step('assert_choices_consumed'),_lifecycle_step('begin_blockers')]
    if v!='unblocked':run.append(_lifecycle_step('block',ref='enemy',other='first',value=1))
    run+=[_lifecycle_step('advance_combat'),_lifecycle_step('combat_damage'),_lifecycle_step('resolve'),_lifecycle_step('end_combat'),_lifecycle_step('assert_choices_consumed')]
    for ref in attackers:
     power=b.e[ref]['power'];b.e[ref]['tapped']=True
     if active:b.life[owner]+=power
     if v=='unblocked':b.life[1-owner]-=power
     else:b.hit('enemy',power);b.hit(ref,b.e['enemy']['power'])
    run+=b.stats()
  pending=v=='creature_counter_pending'
  checks=b.checks(pending)
  if pending:checks=[dict(a,value=2) if a['op']=='assert_stack' else a for a in checks]
  actions=b.pre+run+checks
  if v in ('ready_snapshot','pending_snapshot'):
   cut=ready_cut if v=='ready_snapshot' else pending_cut;tail=run[cut:]+checks
   actions=b.pre+run[:cut]+[_lifecycle_step('capture')]+tail+[_lifecycle_step('record_state'),_lifecycle_step('ai_roundtrip'),_lifecycle_step('restore')]+copy.deepcopy(tail)+[_lifecycle_step('assert_recorded_state')]
  results.append(dict(name=f'GrowthCounterSpellFollowupRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=b.setup,steps=actions,_recipe=dict(family='growth_counter_spell',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['inactive_components'])))
 return results
