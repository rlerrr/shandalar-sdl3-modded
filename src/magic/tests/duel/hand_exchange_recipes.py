"""Oracle-pinned draw/discard pipelines with exact or legal-subset receipts."""
import json
import re
from collections import Counter
from pathlib import Path
from creature_recipes import KEYWORDS,REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY

PINS=json.loads((Path(__file__).parent/'data/hand_exchange_witnesses.json').read_text(encoding='utf8'))['cards']
WORDS={1:'a card',2:'two cards',3:'three cards',4:'four cards',5:'five cards',6:'six cards',7:'seven cards'}
KINDS=('draw','discard','mill','gain_life','lose_life')
RECIPIENTS=('self','any','opponent','both','opponents')


def scope(op,p):return p['recipient'] if op['scope']=='recipient' else op['scope']


def phrase(op,subject,explicit=True):
 k=op['kind'];amount=op['amount'];singular=subject!='self'
 verb={'draw':'draw','discard':'discard','mill':'mill','gain_life':'gain','lose_life':'lose'}[k]
 if singular:verb={'gain':'gains','lose':'loses'}.get(verb,verb+'s')
 if k in ('gain_life','lose_life'):tail=f'{amount} life'
 elif amount=='all':tail='your hand' if subject=='self' else 'their hand'
 elif amount=='discarded':tail='that many cards'
 elif amount=='max_discarded':tail='cards equal to the greatest number of cards a player discarded this way'
 elif amount=='discarded_minus_one':tail='that many cards minus one'
 else:tail=WORDS[amount]
 if k=='discard' and op['random']:tail+=' at random'
 if not explicit:return verb+' '+tail
 return {'self':'You ','any':'Target player ','opponent':'Target opponent ','both':'Each player ','opponents':'Each opponent '}[subject]+verb+' '+tail


def effect_texts(p):
 ops=p['operations'];first=ops[0];sub=scope(first,p);full=phrase(first,sub)
 texts={full}
 if sub=='self':texts.add(full.removeprefix('You ')[0].upper()+full.removeprefix('You ')[1:])
 for before,op in zip(ops,ops[1:]):
  sub=scope(op,p);same=scope(before,p)==sub;full=phrase(op,sub)
  sentences=set() if same and sub in ('any','opponent') else {full}
  if sub=='self':sentences.add(full.removeprefix('You ')[0].upper()+full.removeprefix('You ')[1:])
  if same and sub in ('any','opponent'):sentences.add('That player '+phrase(op,sub,False))
  short=phrase(op,sub,False) if same else full[0].lower()+full[1:]
  texts={t+suffix for t in texts for suffix in ([', then '+short,' and '+short]+['. '+s for s in sentences]+['. Then '+s[0].lower()+s[1:] for s in sentences])}
 return {t+'.' for t in texts}


def spec(d,r,*,family='hand_exchange'):
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:
  raise ValueError('hand exchange requires pinned normal whole text and explicit variants')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'mode','recipient','operations','cost','keywords','mana_ability'}:raise ValueError('explicit hand-exchange mode, recipients, ordered operations and costs required')
 if p['mode'] not in ('spell','entry','death','activation') or p['recipient'] not in RECIPIENTS or type(p['mana_ability']) is not bool:raise ValueError('unreviewed hand-exchange mode or recipient')
 c=p['cost']
 if not isinstance(c,dict) or set(c)!={'mana','tap','sacrifice','pay_life','discard'} or not isinstance(c['mana'],str) or any(type(c[k]) is not bool for k in ('tap','sacrifice')) or type(c['pay_life']) is not int or not 0<=c['pay_life']<=5 or type(c['discard']) is not int or not 0<=c['discard']<=3:raise ValueError('ordinary explicit mana/tap/life/self-sacrifice/discard costs required')
 ops=p['operations']
 if not isinstance(ops,list) or not 1<=len(ops)<=4:raise ValueError('one to four ordered hand-exchange operations required')
 for i,op in enumerate(ops):
  if not isinstance(op,dict) or set(op)!={'kind','amount','scope','random'} or op['kind'] not in KINDS or op['scope'] not in ('recipient','self','both') or type(op['random']) is not bool or op['random'] and op['kind']!='discard':raise ValueError('unreviewed hand operation')
  n=op['amount']
  limit=20 if family=='value_flow' and op['kind'] in ('gain_life','lose_life') else 7
  if not (type(n) is int and 1<=n<=limit or op['kind']=='discard' and n=='all' or op['kind']=='draw' and n in ('discarded','max_discarded','discarded_minus_one') and any(s['kind']=='discard' for s in ops[:i])):raise ValueError('bounded fixed or preceding-discard-derived amount required')
 has_discard=c['discard'] or any(op['kind']=='discard' for op in ops)
 if family=='hand_exchange' and not has_discard:raise ValueError('hand-exchange component must actually discard or pay a discard cost')
 if family=='value_flow' and (has_discard or sum(op['amount'] for op in ops if op['kind']=='draw')>7 or c['pay_life']+sum(op['amount'] for op in ops if op['kind']=='lose_life')>20):raise ValueError('value flow requires bounded fixed nondiscard operations safe for repeated-resolution fixtures')
 if p['recipient'] in ('any','opponent') and not any(op['scope']=='recipient' for op in ops):raise ValueError('a targeted recipient must be used')
 if 'target_choices' in d and (type(d['target_choices']) is not int or d['target_choices'] not in (0,1,2) or p['recipient'] not in ('any','opponent')):raise ValueError('target input count must be a reviewed zero to two dialogs for one targeted recipient')
 types=r['type_line'].partition(' — ')[0].split();creature='Creature' in types;land='Land' in types
 kind=next((t.lower() for t in ('Creature','Artifact','Enchantment','Land') if t in types),None)
 if 'Aura' in r['type_line'] or 'Equipment' in r['type_line']:raise ValueError('standalone source required')
 if p['mode']!='activation' and any(c[k] for k in ('mana','tap','sacrifice','pay_life')):raise ValueError('nonactivation has no permanent activation costs')
 if p['mode'] in ('entry','death') and (not creature or c['discard']):raise ValueError('ordinary creature trigger required')
 if p['mode']=='spell' and not {'Instant','Sorcery'}&set(types):raise ValueError('ordinary instant/sorcery required')
 if p['mana_ability'] and (p['mode']!='activation' or creature):raise ValueError('reviewed alternate colorless mana ability requires a noncreature activation source')
 mana=cost_from_oracle(dict(mana_cost=c['mana'])) if c['mana'] else [0]*6
 clauses=effect_texts(p)
 if p['mode']=='activation':
  if kind is None:raise ValueError('activation requires a permanent source')
  pieces=([c['mana']] if c['mana'] else [])+(['{T}'] if c['tap'] else [])+([f"Pay {c['pay_life']} life"] if c['pay_life'] else [])+(['Discard '+WORDS[c['discard']].lower()] if c['discard'] else [])+(['Sacrifice this '+kind] if c['sacrifice'] else [])
  if kind is None or not pieces:raise ValueError('nonempty ordinary permanent activation cost required')
  expected={', '.join(pieces)+': '+s for s in clauses}
 elif p['mode'] in ('entry','death'):
  expected={f'When {ref} '+ending+', '+s[0].lower()+s[1:] for ref in ('this creature',r['name'],r['name'].split(',')[0]) for ending in (('enters','enters the battlefield') if p['mode']=='entry' else ('dies',)) for s in clauses}
 else:
  prefix=f"As an additional cost to cast this spell, discard {WORDS[c['discard']]}. " if c['discard'] else ''
  expected={prefix+s for s in clauses}
 lines=r['oracle_text'].splitlines();alternatives=[]
 while lines and re.match(r'^(?:Flashback |Madness |Cycling |Miracle )',lines[-1]):alternatives.insert(0,lines.pop())
 normalized=lambda text:text.replace('as many cards as they discarded this way','that many cards').replace('all the cards in their hand','their hand').replace('all the cards in your hand','your hand')
 if p['mode']=='spell':
  if normalized(' '.join(lines)) not in expected or p['keywords'] or p['mana_ability']:raise ValueError('parameters must match the complete ordinary spell effect and cost')
  if set(r.get('keywords',[]))-{'Flashback','Madness','Cycling','Miracle'}:raise ValueError('unreviewed casting keyword')
 else:
  if p['mana_ability']:
   if not lines or lines.pop(0)!='{T}: Add {C}.':raise ValueError('alternate mana ability must be the printed colorless tap ability')
  selected=[line for line in lines if normalized(line) in expected]
  if len(selected)!=1:raise ValueError('parameters must match exactly one complete printed trigger/activation')
  lines.remove(selected[0]);keys=[];canonical={k.lower():k for k in KEYWORDS}
  for line in lines:
   for reminder in re.findall(r'\([^\n]*?\)',line):
    if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed passive reminder')
    line=line.replace(reminder,'')
   for k in line.strip().split(','):
    if k.strip().lower() not in canonical:raise ValueError('additional passive, trigger or activated ability requires separate scenarios')
    keys.append(canonical[k.strip().lower()])
  if p['keywords']!=sorted(keys) or len(keys)!=len(set(keys)) or set(keys)-set(r.get('keywords',[])):raise ValueError('explicit source passive keywords disagree')
 target_count=len(re.findall(r'\btarget (?:player|opponent)\b',r['oracle_text'],re.I))
 if target_count!=int(p['recipient'] in ('any','opponent')):raise ValueError('single shared target or nontargeted recipients required')
 if creature and (any(not re.fullmatch(r'\d+',r.get(k,'')) or not 0<=int(r[k])<=20 for k in ('power','toughness')) or not int(r['toughness'])):raise ValueError('surviving numeric source creature required')
 dialogs=d.get('activation_dialogs',{})
 if not isinstance(dialogs,dict) or any(k not in ('0','1') or not isinstance(v,list) or len(v)>4 or any(type(n) is not int or not 0<=n<=16 for n in v) for k,v in dialogs.items()) or dialogs and not p['mana_ability']:raise ValueError('reviewed alternate activation menu annotations required')
 return dict(p,family=family,activation_mana=mana,mana=None if land else cost_from_oracle(r),creature=creature,land=land,legendary='Legendary' in types,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,untested_rules=alternatives,instant='Instant' in types)


def variants(p):
 modes=['effect','payment','bare_zones','empty_hands','one_card','large_hand','uneven_hands','own_empty','opponent_empty','duplicates','last_choice','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot']
 if p['mode']=='activation':
  modes+=['opposing_turn','combat','source_destroyed_pending','tax']
  if not p['legendary']:modes+=['two_sources']
  if p['cost']['tap']:modes+=['tapped']+(['repeat_denied','untap_repeat'] if not p['cost']['sacrifice'] else [])+(['sick','fresh_activation'] if p['creature'] else [])
  elif not p['cost']['sacrifice']:modes+=['repeat','stacked_activations']
  if not p['cost']['tap']:modes.append('tapped_source')
  if p['cost']['sacrifice']:modes.remove('source_destroyed_pending')
  if p['creature'] and 'Shroud' in p['keywords'] and 'untap_repeat' in modes:modes.remove('untap_repeat')
  if not p['land']:modes+=['cast_source','cast_payment','insufficient_cast_mana']
 else:modes+=['countered_cast','insufficient_mana']
 if p['mode']=='entry':modes+=['source_destroyed','bounce_recast','blink_source']
 if p['mode']=='death':modes+=['other_death','source_bounced','source_exiled']
 if 'Shroud' in p['keywords']:modes=[v for v in modes if v not in ('bounce_recast','blink_source','source_bounced','source_exiled')]
 if p['mode']=='spell' and p['instant']:modes+=['opposing_turn','combat']
 if p['recipient']=='any':modes.append('own_target')
 if p['mode']=='spell' and p['recipient'] in ('any','opponent'):modes.append('target_shrouded_pending')
 cost=p['activation_mana'] if p['mode']=='activation' else p['mana']
 if not p['mana_ability']:
  if sum(cost) and p['mode']=='activation':modes.append('insufficient_mana')
  if any(cost[1:]):modes.append('wrong_color')
 if p['cost']['discard']:modes.append('empty_hand_cost')
 if p['cost']['pay_life']:
  modes.append('low_life')
  if p['cost']['pay_life']>1:modes.append('insufficient_life')
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]


def validate_witnesses(o,rows):
 for n,fields in PINS.items():
  if n not in rows or any(o.get(n,{}).get(k)!=v for k,v in fields.items()):raise ValueError('hand-exchange witness changed: '+n)


def expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';mode=p['mode'];c=p['cost'];activation=mode=='activation';land=p['land']
  cast_only=v in ('cast_source','cast_payment','insufficient_cast_mana');casting=not activation or cast_only or v=='fresh_activation'
  hand0=[] if bare or v in ('empty_hands','empty_hand_cost') else list(HAND[:1]) if v=='one_card' else list(HAND)*3 if v=='large_hand' else ['Grizzly Bears']*4 if v=='duplicates' else list(HAND)
  initial_hands=[list(hand0),list(hand0)]
  if v=='uneven_hands':initial_hands[owner]=list(HAND)*2;initial_hands[1-owner]=list(HAND[:1])
  if v=='own_empty':initial_hands[owner]=[]
  if v=='opponent_empty':initial_hands[1-owner]=[]
  if v=='stacked_activations' and c['discard']>2:initial_hands=[list(HAND)*2,list(HAND)*2]
  hands=[list(ns) for ns in initial_hands];possible=[Counter(ns) for ns in initial_hands];hc=[len(ns) for ns in initial_hands];totals=[Counter(ns+([] if bare else list(GRAVE))) for ns in initial_hands]
  target=owner if v=='own_target' or p['recipient']=='self' else 1-owner
  def recipients(sc):return [owner] if sc=='self' else [0,1] if sc=='both' else [1-owner] if sc=='opponents' else [target]
  libs=[list(LIBRARY)*4,list(LIBRARY)*4];exiles=[[] if bare else list(EXILE) for _ in (0,1)];setup=[];entities={};mana=[[20]*6,[20]*6];life=[50,50] if p['family']=='value_flow' else [20,20];last_discard=[0,0]
  if v in ('exact_library','short_library','empty_library'):
   for pl in (0,1):
    moved=sum(op['amount'] for op in p['operations'] if op['kind'] in ('draw','mill') and pl in recipients(scope(op,p)))
    if moved:libs[pl]=libs[pl][:moved if v=='exact_library' else 1 if v=='short_library' else 0]
  actions=[];pre=[step('turn',player=1-owner if v=='opposing_turn' else owner,value=0),step('discard_policy',value=2 if v=='last_choice' else 1)]
  if p['family']=='value_flow':pre.extend(step('set_life',player=pl,value=life[pl]) for pl in (0,1))
  if p['mana_ability']:pre.append(step('present_ai_dialogs',value=1))
  for pl in (0,1):
   for zone,names in (('hand',initial_hands[pl]),('graveyard',[] if bare else GRAVE),('library',libs[pl]),('exile',exiles[pl])):setup += [card(nm,f'p{pl}_{zone}_{i}',pl,zone) for i,nm in enumerate(names)]
  def add(nm,ref,pl,zone='battlefield',tapped=False,sick=False):
   item=card(nm,ref,pl,zone)
   if zone=='battlefield':item['state']=['tapped'] if tapped else ['sick'] if sick else ['untapped']
   setup.append(item);entities[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',hand=zone=='hand',tapped=tapped)
   if zone=='hand':hc[pl]+=1;possible[pl][nm]+=1;totals[pl][nm]+=1;hands[pl].append(nm)
  def take_hand(ref,to_grave=False):
   e=entities[ref];pl=e['player'];nm=e['name'];e['hand']=False;hc[pl]-=1;possible[pl][nm]-=1
   if hands[pl] is not None:hands[pl].remove(nm)
   if not to_grave:totals[pl][nm]-=1
  def leave(ref,dest):
   e=entities[ref];pl=e['player'];nm=e['name'];e['alive']=False;e['hand']=dest=='hand'
   if dest in ('hand','graveyard'):totals[pl][nm]+=1
   if dest=='hand':
    hc[pl]+=1;possible[pl][nm]+=1
    if hands[pl] is not None:hands[pl].append(nm)
   if dest=='exile':exiles[pl].append(nm)
  def discard(pl,n,random=False):
   n=hc[pl] if n=='all' else min(n,hc[pl]);last_discard[pl]=n;hc[pl]-=n
   if hc[pl]==0:hands[pl]=[];possible[pl]=Counter()
   elif n and (random or pl==1 or hands[pl] is None):hands[pl]=None
   elif n:
    for _ in range(n):
     chosen=(max if v=='last_choice' else min)(hands[pl],key=lambda nm:rows[nm]['csvid']);hands[pl].remove(chosen);possible[pl][chosen]-=1
   return n
  def effect():
   for op in p['operations']:
    for pl in recipients(scope(op,p)):
     kind=op['kind'];n=op['amount']
     if kind=='discard':discard(pl,n,op['random']);continue
     if kind=='draw' and isinstance(n,str):n=last_discard[pl] if n=='discarded' else max(last_discard) if n=='max_discarded' else max(0,last_discard[pl]-1)
     if kind in ('draw','mill'):
      for _ in range(min(n,len(libs[pl])) if kind=='mill' else n):
       nm=libs[pl].pop(0);totals[pl][nm]+=1
       if kind=='draw':
        hc[pl]+=1;possible[pl][nm]+=1
        if hands[pl] is not None:hands[pl].append(nm)
     elif kind=='gain_life':life[pl]+=n
     else:life[pl]-=n
  def target_choice():return [step('choose_player',player=owner,value=target) for _ in range(d.get('target_choices',1))] if p['recipient'] in ('any','opponent') else []
  def pay(vector,pl):mana[pl]=[a-b for a,b in zip(mana[pl],vector)]
  def paid_receipts(source_on_stack=False):
   # The legacy active-zone counter includes a pending spell's physical slot.
   # The model transfers it out of hand immediately; creature spells enter play
   # on resolution, while instants/sorceries subsequently enter the graveyard.
   for pl in (0,1):
    actions.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='hand',value=hc[pl]+int(source_on_stack and pl==owner)),step('assert_zone',player=pl,zone='graveyard',value=sum(totals[pl].values())-hc[pl]-int(source_on_stack and mode=='spell' and pl==owner))])
    actions.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
  def helper(nm,ref,pl,target_ref,vector):
   actions.extend([step('choose_target',player=pl,ref=target_ref),step('activate',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]);pay(vector,pl)
  add(name,'source',owner,'hand' if casting else 'battlefield',tapped=v in ('tapped','tapped_source'),sick=v=='sick')
  add('Grizzly Bears','friendly',owner);add('Hill Giant','enemy',1-owner)
  if v=='two_sources':add(name,'second',owner)
  if v=='tax':add('Suppression Field','taxer',1-owner)
  if v=='countered_cast':add('Ertai, Wizard Adept','counter',0)
  if v=='untap_repeat':add("Kiora's Follower",'untapper',owner)
  if v=='bounce_recast':add('Temporal Adept','bouncer',owner)
  if v=='blink_source':add('Eldrazi Displacer','blinker',owner)
  if v=='target_shrouded_pending':add('Gilded Light','shield',target,'hand')
  if v=='source_bounced':add('Unsummon','response',owner,'hand')
  if v=='source_exiled':add('Swords to Plowshares','response',owner,'hand')
  if mode=='death' and v=='pending_snapshot':add('Murder','removal',owner,'hand')
  cost=list(p['activation_mana'] if activation else p['mana'])
  if v=='tax':cost[0]+=2
  if v in ('payment','tax'):mana[owner]=list(cost)
  if v=='cast_payment':mana[owner]=list(p['mana'])
  if v in ('insufficient_mana','insufficient_cast_mana'):mana[owner]=[0]*6
  if v=='wrong_color':
   mana[owner]=list(cost);color=next(i for i in range(1,6) if cost[i]);mana[owner][1 if color!=1 else 2]+=mana[owner][color];mana[owner][color]=0
  if v in ('low_life','insufficient_life'):life[owner]=c['pay_life']+(1 if v=='low_life' else -1);pre.append(step('set_life',player=owner,value=life[owner]))
  for pl in (0,1):pre += [step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl])]
  pre.append(step('refresh'))
  if v=='combat':pre += [step('begin_attackers'),step('attack',ref='friendly',value=1),step('finish_attackers')];entities['friendly']['tapped']=True
  denied=v in ('insufficient_mana','insufficient_cast_mana','wrong_color','empty_hand_cost','insufficient_life','tapped') or v=='sick' and 'Haste' not in p['keywords'] or c['discard']>len(initial_hands[owner])
  def activate(ref='source',pending_before=0):
   actions.append(step('require_can_activate',ref=ref,value=1))
   actions.extend(step('choose_dialog',player=owner,value=n) for n in d.get('activation_dialogs',{}).get(str(owner),[]))
   actions.extend(target_choice())
   actions.append(step('activate',ref=ref,value=1));pay(cost,owner);life[owner]-=c['pay_life']
   if c['discard']:discard(owner,c['discard'])
   if c['sacrifice']:leave(ref,'graveyard')
   elif c['tap']:entities[ref]['tapped']=True
   actions.append(step('assert_stack',value=1+pending_before))
   if not c['sacrifice']:actions.append(observe(ref,'in_play',1))
   paid_receipts()
   return len(actions)
  def cast_source(countered=False):
   if mode=='spell':actions.extend(target_choice())
   if countered:actions.append(step('choose_priority',ref='counter'));pay([2,0,2,0,0,0],0);entities['counter']['tapped']=True
   if mode=='spell':actions.append(step('require_can_cast',ref='source',value=1))
   actions.append(step('cast',ref='source',value=0 if countered else 1));pay(p['mana'],owner)
   take_hand('source',to_grave=countered or mode=='spell')
   if c['discard'] and mode=='spell':discard(owner,c['discard'])
   if not countered:
    actions.append(step('assert_stack',value=1))
    paid_receipts(True)
    if mode=='entry':actions.extend(target_choice())
   cut=len(actions)
   if v=='target_shrouded_pending':
    actions.extend([step('cast',ref='shield',value=1),step('assert_stack',value=2)]);pay(cost_from_oracle(o['Gilded Light']),target);take_hand('shield',True)
   actions.extend([step('resolve'),step('assert_choices_consumed')])
   if not countered and mode!='spell':entities['source']['alive']=True
   if not countered and mode in ('spell','entry') and v!='target_shrouded_pending':effect()
   return cut
  if denied:
   if casting:actions.append(step('cast',ref='source',value=0))
   else:actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
   pending=0
  elif v=='countered_cast':pending=cast_source(True)
  elif activation and not casting:
   pending=activate()
   if v=='source_destroyed_pending':actions.append(step('destroy',ref='source'));leave('source','graveyard')
   if v=='stacked_activations':activate(pending_before=1)
   actions.extend([step('resolve'),step('assert_choices_consumed')]);effect()
   if v=='stacked_activations':effect()
   if v=='repeat_denied':actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
   if v in ('two_sources','repeat','untap_repeat'):
    if v=='untap_repeat':helper("Kiora's Follower",'untapper',owner,'source',[0]*6);entities['untapper']['tapped']=True;entities['source']['tapped']=False
    activate('second' if v=='two_sources' else 'source');actions.extend([step('resolve'),step('assert_choices_consumed')]);effect()
  else:
   pending=cast_source()
   if activation and v=='fresh_activation':
    if c['tap'] and p['creature'] and 'Haste' not in p['keywords']:actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
    else:activate();actions.extend([step('resolve'),step('assert_choices_consumed')]);effect()
   if mode=='entry' and v=='source_destroyed':actions.append(step('destroy',ref='source'));leave('source','graveyard')
   if mode=='entry' and v=='bounce_recast':
    helper('Temporal Adept','bouncer',owner,'source',[0,0,3,0,0,0]);entities['bouncer']['tapped']=True;leave('source','hand')
    actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'));cast_source()
   if mode=='entry' and v=='blink_source':
    actions.extend([step('choose_target',player=owner,ref='source'),step('activate',ref='blinker',value=1),step('assert_stack',value=1)]);pay([3,0,0,0,0,0],owner);actions.extend(target_choice());actions.extend([step('resolve'),step('assert_choices_consumed'),step('bind_permanent',player=owner,value=csvid,ref='source')]);entities['source']['tapped']=True;effect()
   if mode=='death':
    if v in ('source_bounced','source_exiled'):
     actions.extend([step('choose_target',player=owner,ref='source'),step('cast',ref='response',value=1),step('resolve'),step('assert_choices_consumed')]);pay(cost_from_oracle(o[entities['response']['name']]),owner);take_hand('response',True)
     leave('source','hand' if v=='source_bounced' else 'exile')
     if v=='source_bounced':actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'))
     else:life[owner]+=p['power']
    elif v=='pending_snapshot':
     actions.extend([step('choose_target',player=owner,ref='source'),step('cast',ref='removal',value=1),step('assert_stack',value=1)]);pay(cost_from_oracle(o['Murder']),owner);take_hand('removal',True)
     actions.extend(target_choice());pending=len(actions);actions.extend([step('resolve'),step('assert_choices_consumed')]);leave('source','graveyard');effect()
    else:
     if v!='other_death':actions.extend(target_choice())
     ref='friendly' if v=='other_death' else 'source';actions.append(step('destroy',ref=ref));leave(ref,'graveyard');pending=len(actions);actions.extend([step('resolve'),step('assert_choices_consumed')])
     if v!='other_death':effect()
  if v=='cleanup':actions.extend([step('cleanup'),step('refresh')])
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref,e in entities.items():
   if e['alive']:
    checks.extend([observe(ref,'in_play',1),observe(ref,'in_hand',0),observe(ref,'tapped',int(e['tapped']))])
    if 'Creature' in o[e['name']]['type_line']:
     checks.extend(observe(ref,k,int(o[e['name']][k])) for k in ('power','toughness'));checks.extend([observe(ref,'damage',0),observe(ref,'plus1_counters',0),observe(ref,'minus1_counters',0)])
   if e['hand']:checks.append(observe(ref,'in_hand',1))
  all_names=set(o_nm for total in totals for o_nm in total)|{name}|set(PINS)
  for pl in (0,1):
   checks.extend([step('assert_life',player=pl,value=life[pl])]+[step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl])])
   checks.extend([step('assert_zone',player=pl,zone='hand',value=hc[pl]),step('assert_zone',player=pl,zone='graveyard',value=sum(totals[pl].values())-hc[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))])
   for nm in sorted(all_names):
    checks.append(step('assert_hand_graveyard_total',player=pl,index=rows[nm]['csvid'],value=totals[pl][nm]))
    checks.append(step('assert_zone_multiplicity' if hands[pl] is not None else 'assert_zone_multiplicity_max',player=pl,zone='hand',index=rows[nm]['csvid'],value=hands[pl].count(nm) if hands[pl] is not None else possible[pl][nm]))
    checks.append(step('assert_zone_multiplicity',player=pl,zone='exile',index=rows[nm]['csvid'],value=exiles[pl].count(nm)))
   checks.extend([step('assert_zone',player=pl,zone='exile',value=len(exiles[pl])),step('assert_zone',player=pl,zone='library',value=len(libs[pl]))])
   checks.extend(step('assert_zone_card',player=pl,zone='library',index=i,value=rows[nm]['csvid']) for i,nm in enumerate(libs[pl]))
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
   cut=0 if v=='ready_snapshot' else len(actions) if v=='resolved_snapshot' else pending
   steps=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
  family=p['family'];prefix='HandExchangeRecipe' if family=='hand_exchange' else 'ValueFlowRecipe'
  gaps=['random distribution','alternative costs/abilities','replacement and discard triggers','control changes','multiplayer'] if family=='hand_exchange' else ['alternative costs/abilities','draw/life/mill replacement and triggered effects','drawing from an empty library','control changes','multiplayer']
  result.append(dict(name=f'{prefix}_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family=family,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_rules=p['untested_rules'],untested_keywords=p['keywords'],untested_components=gaps)))
 return result
