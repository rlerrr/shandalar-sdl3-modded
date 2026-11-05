"""Pinned single-target destruction/exile, with independent target predicates."""
import json
import re
from pathlib import Path
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

PINS=json.loads((Path(__file__).parent/'data/removal_witnesses.json').read_text(encoding='utf8'))['cards']
PERMANENTS={'Artifact','Creature','Enchantment','Land','Planeswalker','Battle'}
COLORS={'white':'W','blue':'U','black':'B','red':'R','green':'G'}
SCOPES={
 'permanent':('permanent',(),tuple(PERMANENTS),(),(),(),()),
 'nonland_permanent':('nonland permanent',(),tuple(PERMANENTS),('Land',),(),(),()),
 'noncreature_permanent':('noncreature permanent',(),tuple(PERMANENTS),('Creature',),(),(),()),
 'artifact':('artifact',('Artifact',),(),(),(),(),()),
 'enchantment':('enchantment',('Enchantment',),(),(),(),(),()),
 'creature':('creature',('Creature',),(),(),(),(),()),
 'land':('land',('Land',),(),(),(),(),()),
 'artifact_or_enchantment':('artifact or enchantment',(),('Artifact','Enchantment'),(),(),(),()),
 'artifact_or_land':('artifact or land',(),('Artifact','Land'),(),(),(),()),
 'artifact_creature_or_planeswalker':('artifact, creature, or planeswalker',(),('Artifact','Creature','Planeswalker'),(),(),(),()),
 'creature_or_enchantment':('creature or enchantment',(),('Creature','Enchantment'),(),(),(),()),
 'creature_or_planeswalker':('creature or planeswalker',(),('Creature','Planeswalker'),(),(),(),()),
 'spirit_or_enchantment':('Spirit or enchantment',(),tuple(PERMANENTS),(),(),(),()),
 'artifact_creature':('artifact creature',('Artifact','Creature'),(),(),(),(),()),
 'nonartifact_creature':('nonartifact creature',('Creature',),(),('Artifact',),(),(),()),
 'nonartifact_nonblack_creature':('nonartifact, nonblack creature',('Creature',),(),('Artifact',),(),('B',),()),
 'nonblack_creature':('nonblack creature',('Creature',),(),(),(),('B',),()),
 'nonwhite_creature':('nonwhite creature',('Creature',),(),(),(),('W',),()),
 'nonwhite_nonblack_creature':('nonwhite, nonblack creature',('Creature',),(),(),(),('W','B'),()),
 'green_creature':('green creature',('Creature',),(),(),('G',),(),()),
 'nonwhite_permanent':('nonwhite permanent',(),tuple(PERMANENTS),(),(),('W',),()),
 'white_permanent':('white permanent',(),tuple(PERMANENTS),(),('W',),(),()),
 'black_or_red_permanent':('black or red permanent',(),tuple(PERMANENTS),(),('B','R'),(),()),
 'human_creature':('Human creature',('Creature',),(),(),(),(),('Human',)),
 'spirit':('Spirit',(),tuple(PERMANENTS),(),(),(),('Spirit',)),
 'wall':('Wall',(),tuple(PERMANENTS),(),(),(),('Wall',)),
 'equipment':('Equipment',('Artifact',),(),(),(),(),('Equipment',)),
 'mountain':('Mountain',('Land',),(),(),(),(),('Mountain',)),
 'nonspirit_creature':('non-Spirit creature',('Creature',),(),(),(),(),()),
 'nonzombie_creature':('non-Zombie creature',('Creature',),(),(),(),(),()),
 'nonelf_creature':('non-Elf creature',('Creature',),(),(),(),(),()),
 'nonmerfolk_creature':('non-Merfolk creature',('Creature',),(),(),(),(),()),
 'nonlegendary_creature':('nonlegendary creature',('Creature',),(),(),(),(),()),
 'monocolored_creature':('monocolored creature',('Creature',),(),(),(),(),()),
 'creature_or_vehicle':('creature or Vehicle',(),('Creature','Artifact'),(),(),(),()),
 'colored_creature':("creature that's one or more colors",('Creature',),(),(),tuple(COLORS.values()),(),()),
 'nonbasic_land':('nonbasic land',('Land',),(),(),(),(),()),
 'flying_creature':('creature with flying',('Creature',),(),(),(),(),()),
 'nonflying_creature':('creature without flying',('Creature',),(),(),(),(),()),
}
RESPONSES={'Giant Growth','Lightning Bolt','Counterspell','Twiddle','Boomerang','Unsummon','Cloudshift','Blessed Breath','Vines of Vastwood','Pacifism'}
TARGETS=tuple(n for n in PINS if n not in RESPONSES)
STATES={'any':'','tapped':'tapped ','untapped':'untapped ','attacking':'attacking ',
        'blocking':'blocking ','attacking_or_blocking':'attacking or blocking '}


def eligible(r,p,owner=0,target_owner=1,tapped=False,attacking=False,blocking=False,*,scope_defs=SCOPES):
 noun,required,union,excluded,colors,not_colors,subtypes=scope_defs[p['scope']]
 types=set(r['type_line'].partition(' — ')[0].split());subs=set(r['type_line'].partition(' — ')[2].split())
 if 'Creature' in types and 'Changeling' in r.get('keywords',[]):subs|={'Human','Spirit','Zombie','Wall','Elf','Merfolk'}
 if not (set(required)<=types and (not union or types&set(union)) and not types&set(excluded)
         and (not colors or set(r.get('colors',[]))&set(colors)) and not set(r.get('colors',[]))&set(not_colors)
         and set(subtypes)<=subs):return False
 if p['scope']=='nonspirit_creature' and 'Spirit' in subs or p['scope']=='nonzombie_creature' and 'Zombie' in subs:return False
 if p['scope']=='spirit_or_enchantment' and not ('Spirit' in subs or 'Enchantment' in types):return False
 if p['scope']=='creature_or_vehicle' and not ('Creature' in types or 'Vehicle' in subs):return False
 if p['scope']=='nonelf_creature' and 'Elf' in subs or p['scope']=='nonmerfolk_creature' and 'Merfolk' in subs:return False
 if p['scope']=='nonlegendary_creature' and 'Legendary' in types:return False
 if p['scope']=='monocolored_creature' and len(r.get('colors',[]))!=1:return False
 if p['scope']=='nonbasic_land' and 'Basic' in types:return False
 if p['scope']=='flying_creature' and 'Flying' not in r.get('keywords',[]):return False
 if p['scope']=='nonflying_creature' and 'Flying' in r.get('keywords',[]):return False
 if p['controller']=='self' and target_owner!=owner or p['controller']=='opponent' and target_owner==owner:return False
 if not {'any':True,'tapped':tapped,'untapped':not tapped,'attacking':attacking,
         'blocking':blocking,'attacking_or_blocking':attacking or blocking}[p['state']]:return False
 if p['limit']:
  b=p['limit'];value=r.get({'mana_value':'cmc','power':'power','toughness':'toughness'}[b['field']])
  if value is None or not re.fullmatch(r'\d+(?:\.0)?',str(value)):return False
  value=float(value)
  if not {'eq':value==b['value'],'le':value<=b['value'],'ge':value>=b['value']}[b['op']]:return False
 return True


def effect_spec(p,name,*,scope_defs=SCOPES):
 required={'effect','scope','state','controller','limit','no_regenerate','draw','gain_life','target_gain_life','target_lose_life','target_damage','add_mana'}
 if not isinstance(p,dict) or set(p)!=required:raise ValueError('explicit removal predicate and complete secondary receipts required')
 if p['effect'] not in ('destroy','exile') or p['scope'] not in scope_defs or p['state'] not in STATES or p['controller'] not in ('any','self','opponent') or type(p['no_regenerate']) is not bool:
  raise ValueError('unreviewed removal effect, scope, state or controller')
 if p['effect']=='exile' and p['no_regenerate']:raise ValueError('exile has no regeneration clause')
 if any(type(p[k]) is not int or not 0<=p[k]<=(2 if k=='draw' else 10) for k in ('draw','gain_life','target_gain_life','target_lose_life','target_damage')):raise ValueError('bounded fixed removal receipts required')
 if p['limit'] is not None:
  b=p['limit']
  if not isinstance(b,dict) or set(b)!={'field','op','value'} or b['field'] not in ('power','toughness','mana_value') or b['op'] not in ('eq','le','ge') or type(b['value']) is not int or not 0<=b['value']<=9:raise ValueError('unreviewed fixed target characteristic limit')
  if b['field']!='mana_value' and 'Creature' not in scope_defs[p['scope']][1]:raise ValueError('power/toughness bounds require creature-only scope')
 noun=STATES[p['state']]+scope_defs[p['scope']][0]
 if p['state']!='any' and 'Creature' not in scope_defs[p['scope']][1]:raise ValueError('state restrictions require creature scope')
 if p['limit']:
  b=p['limit'];noun+=' with '+b['field'].replace('_',' ')+f" {b['value']}"+{'eq':'','le':' or less','ge':' or greater'}[b['op']]
 noun+={'any':'','self':' you control','opponent':' an opponent controls'}[p['controller']]
 primary=p['effect'].capitalize()+' target '+noun+'.'
 if p['no_regenerate']:primary+=" It can't be regenerated."
 if p['target_gain_life']:primary+=f" Its controller gains {p['target_gain_life']} life."
 if p['target_lose_life']:primary+=f" Its controller loses {p['target_lose_life']} life."
 if p['target_damage']:primary+=f" {name} deals {p['target_damage']} damage to that "+('artifact' if p['scope']=='artifact' else 'land' if p['scope'] in ('land','mountain','nonbasic_land') else 'permanent')+"'s controller."
 if p['gain_life']:primary+=f" You gain {p['gain_life']} life."
 mana_output=[0]*6
 if not isinstance(p['add_mana'],str) or p['add_mana'] and not re.fullmatch(r'(?:\{[BUGRWC]\}){1,5}',p['add_mana']):raise ValueError('ordinary fixed secondary mana output required')
 if p['add_mana']:
  primary+=' Add '+p['add_mana']+'.'
  for c in re.findall(r'\{([BUGRWC])\}',p['add_mana']):mana_output[{'C':0,'B':1,'U':2,'G':3,'R':4,'W':5}[c]]+=1
 if p['draw']:primary+=' Draw '+{1:'a card',2:'two cards'}[p['draw']]+'.'
 alternatives={primary}
 if p['state']!='any' and scope_defs[p['scope']][0]!='creature':alternatives.add(primary.replace(STATES[p['state']]+scope_defs[p['scope']][0],scope_defs[p['scope']][0].replace('creature',STATES[p['state']]+'creature')))
 if p['controller']=='opponent':alternatives.add(primary.replace(' an opponent controls'," you don't control"))
 return alternatives,mana_output


def spec(d,r):
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or not {'Instant','Sorcery'}&set(r.get('type_line','').partition(' — ')[0].split()) or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:
  raise ValueError('removal requires a pinned normal instant/sorcery and explicit variants')
 p=d.get('params')
 alternatives,mana_output=effect_spec(p,r.get('name',''))
 # Line breaks do not alter an ordinary sequential spell effect.
 text=r['oracle_text'];lines=text.splitlines();remaining=[]
 while lines and re.match(r'^(?:Cycling |Flashback |Splice onto Arcane |Madness )',lines[-1]):remaining.insert(0,lines.pop())
 if ' '.join(lines) not in alternatives:raise ValueError('parameters must match the complete ordinary spell effect')
 if any(k not in ('Cycling','Flashback','Splice','Madness') for k in r.get('keywords',[])):raise ValueError('unreviewed casting keyword or extra ability')
 if p['state'] in ('attacking','blocking','attacking_or_blocking') and not r['type_line'].startswith('Instant'):raise ValueError('combat removal requires reviewed instant timing')
 result=dict(p,family='target_removal',mana=cost_from_oracle(r),add_pool=mana_output,untested_rules=remaining)
 if not any(eligible(PINS[n],result,target_owner=0 if p['controller']=='self' else 1,tapped=p['state']=='tapped',attacking=p['state'] in ('attacking','attacking_or_blocking'),blocking=p['state']=='blocking') for n in TARGETS):raise ValueError('removal predicate has no matching pinned target')
 return result


def variants(p):
 modes=['effect','payment','insufficient_mana','empty_battlefield','own_target','two_targets','bare_zones',
        'cleanup','lost_target','bounced_target','countered','ready_snapshot','pending_snapshot','resolved_snapshot']
 modes += ['card_'+str(i) for i in range(len(TARGETS))]
 if any(p['mana'][1:]):modes.append('wrong_color')
 if p['state']!='any':modes.append('wrong_state')
 if p['state'] in ('tapped','untapped'):modes.append('state_changed_pending')
 if p['state']=='attacking_or_blocking':modes.append('blocking_target')
 if p['state']=='any' and any('regenerate' in PINS[n]['oracle_text'].lower() and eligible(PINS[n],p) for n in ('Drudge Skeletons','Horned Troll')):modes.append('regeneration')
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]


def validate_witnesses(o,rows):
 for name,fields in PINS.items():
  if name not in rows or any(o.get(name,{}).get(k)!=v for k,v in fields.items()):raise ValueError('removal witness changed: '+name)


def expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 result=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones'
  zones={z:[[] if bare else list(ns),[] if bare else list(ns)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY))}
  zones['library']=[list(LIBRARY),list(LIBRARY)]
  setup=[];entities={};mana=[[20]*6,[20]*6];life=[20,20];pre=[step('turn',player=owner,value=0)];actions=[]
  for player in (0,1):
   for z,ns in zones.items():setup += [card(nm,f'p{player}_{z}_{i}',player,z) for i,nm in enumerate(ns[player])]
  def add(nm,ref,player,zone='battlefield',tapped=False):
   item=card(nm,ref,player,zone)
   if zone=='battlefield':item['state']=['tapped'] if tapped else ['untapped']
   setup.append(item);entities[ref]=dict(name=nm,player=player,alive=zone=='battlefield',hand=zone=='hand',tapped=tapped,attacking=False,blocking=False)
   if zone!='battlefield':zones[zone][player].append(nm)
  def move(ref,dest):
   e=entities[ref];e['alive']=False;e['hand']=dest=='hand';zones[dest][e['player']].append(e['name'])
  def pay(nm,player):mana[player]=[a-b for a,b in zip(mana[player],cost_from_oracle(o[nm]))]
  def qualifies(nm,player):return eligible(o[nm],p,owner,player,tapped=p['state']=='tapped',attacking=p['state'] in ('attacking','attacking_or_blocking'),blocking=p['state']=='blocking' or v=='blocking_target')
  add(name,'source',owner,'hand')
  target_owner=owner if v=='own_target' or p['controller']=='self' else 1-owner
  candidates=[nm for nm in TARGETS if qualifies(nm,target_owner)]
  # Own-target permission is a negative test for opponent-only removal.
  victim=next((nm for nm in TARGETS if qualifies(nm,1-owner)),None) if not candidates else next((nm for nm in candidates if 'Indestructible' not in o[nm].get('keywords',[])),candidates[0])
  victim=victim or next(nm for nm in TARGETS if qualifies(nm,owner))
  if v.startswith('card_'):victim=TARGETS[int(v[5:])]
  if v=='regeneration':victim=next(n for n in ('Drudge Skeletons','Horned Troll') if qualifies(n,target_owner))
  if v!='empty_battlefield':
   mana_source='Land' in o[victim]['type_line'] or victim in ('Sol Ring','Gilded Lotus')
   add(victim,'recipient',target_owner,tapped=p['state']=='tapped' or mana_source)
  if v=='two_targets':add(victim,'unselected',target_owner,tapped=entities['recipient']['tapped'])
  if v in ('payment','insufficient_mana','wrong_color'):
   mana[owner]=[0]*6 if v=='insufficient_mana' else list(p['mana'])
   if v=='wrong_color':
    c=next(c for c in range(1,6) if mana[owner][c]);mana[owner][1 if c!=1 else 2]+=mana[owner][c];mana[owner][c]=0
  for player in (0,1):pre += [step('set_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
  pre.append(step('refresh'))
  combat=p['state'] in ('attacking','blocking','attacking_or_blocking')
  if 'recipient' in entities and combat and 'Creature' in o[victim]['type_line'] and (p['state']=='blocking' or v=='blocking_target' or 'Defender' not in o[victim].get('keywords',[])) and v!='wrong_state':
   blocking=p['state']=='blocking' or v=='blocking_target'
   if blocking:add('Scaled Wurm','attacker',1-target_owner);attacker='attacker'
   else:attacker='recipient'
   pl=entities[attacker]['player'];pre += [step('turn',player=pl,value=0),step('begin_attackers'),step('attack',ref=attacker,value=1),step('finish_attackers'),step('begin_blockers')]
   entities[attacker]['attacking']=True;entities[attacker]['tapped']='Vigilance' not in o[entities[attacker]['name']].get('keywords',[])
   if blocking:pre.append(step('block',ref='recipient',other=attacker,value=1));entities['recipient']['blocking']=True
   pre.append(step('advance_combat'))
  if v=='wrong_state' and p['state'] in ('tapped','untapped'):
   entities['recipient']['tapped']=p['state']!='tapped';setup[-1]['state']=['tapped'] if entities['recipient']['tapped'] else ['untapped']
  e=entities.get('recipient');legal=e is not None and eligible(o[victim],p,owner,target_owner,e['tapped'],e['attacking'],e['blocking'])
  denial=v in ('insufficient_mana','wrong_color') or not legal
  if denial:
   if v in ('insufficient_mana','wrong_color'):actions.append(step('cast',ref='source',value=0))
   else:actions.append(step('can_cast',ref='source',value=0))
  else:
   if v=='countered':add('Counterspell','counter',0,'hand');pay('Counterspell',0);zones['hand'][0].remove('Counterspell');zones['graveyard'][0].append('Counterspell');entities['counter']['hand']=False
   actions += [step('require_can_cast',ref='source',value=1),step('choose_target',player=owner,ref='recipient')]
   if v=='countered':actions.append(step('choose_priority',ref='counter'))
   actions.append(step('cast',ref='source',value=0 if v=='countered' else 1))
   pay(name,owner);zones['hand'][owner].remove(name);zones['graveyard'][owner].append(name);entities['source']['hand']=False
   if v!='countered':actions += [step('assert_stack',value=1),observe('recipient','in_play',1)]
   cut=len(actions)
   if v=='lost_target':actions.append(step('destroy',ref='recipient'));move('recipient','graveyard')
   if v=='bounced_target':
    add('Boomerang','response',owner,'hand');actions += [step('choose_target',player=owner,ref='recipient'),step('cast',ref='response',value=1)]
    pay('Boomerang',owner);zones['hand'][owner].remove('Boomerang');zones['graveyard'][owner].append('Boomerang');entities['response']['hand']=False;move('recipient','hand')
   changed=v=='state_changed_pending'
   if changed:
    add('Twiddle','response',owner,'hand');actions += [step('choose_target',player=owner,ref='recipient'),step('cast',ref='response',value=1),step('choose_dialog',player=owner,value=0 if p['state']=='untapped' else 1)]
    pay('Twiddle',owner);zones['hand'][owner].remove('Twiddle');zones['graveyard'][owner].append('Twiddle');entities['response']['hand']=False;entities['recipient']['tapped']=not entities['recipient']['tapped']
   regenerate=v=='regeneration' and p['effect']=='destroy' and not p['no_regenerate']
   blocked_regeneration=v=='regeneration' and not regenerate
   if blocked_regeneration:actions.append(step('choose_regeneration',ref='recipient'))
   if regenerate:
    actions.append(step('choose_regeneration',ref='recipient'));mana[target_owner][1 if victim=='Drudge Skeletons' else 3]-=1;entities['recipient']['tapped']=True;entities['recipient']['attacking']=False
   actions += [step('resolve'),step('assert_unused_choices',value=1) if blocked_regeneration else step('assert_choices_consumed'),step('refresh')]
   if v=='bounced_target':actions.append(step('bind_hand_copy',player=target_owner,value=rows[victim]['csvid'],index=zones['hand'][target_owner].count(victim)-1,ref='recipient'))
   effect=v not in ('countered','lost_target','bounced_target') and not changed
   indestructible='Indestructible' in o[victim].get('keywords',[]) and p['effect']=='destroy'
   if effect:
    if not (regenerate or indestructible):move('recipient','graveyard' if p['effect']=='destroy' else 'exile')
    life[owner]+=p['gain_life'];life[target_owner]+=p['target_gain_life']-p['target_lose_life']-p['target_damage']
    mana[owner]=[a+b for a,b in zip(mana[owner],p['add_pool'])]
    for _ in range(p['draw']):zones['hand'][owner].append(zones['library'][owner].pop(0))
   if v=='cleanup':actions += [step('cleanup'),step('refresh')]
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref,e in entities.items():
   # A vacated target's battlefield slot can be reused by a response spell.
   if e['alive']:checks += [observe(ref,'in_play',1),observe(ref,'tapped',int(e['tapped']))]
   elif ref=='recipient' and v!='bounced_target':checks.append(observe(ref,'in_play',0))
   if e['hand']:checks.append(observe(ref,'in_hand',1))
   if e['alive'] and 'Creature' in o[e['name']]['type_line']:
    for k in ('power','toughness'):checks.append(observe(ref,k,int(o[e['name']][k])))
    checks += [observe(ref,'damage',0),observe(ref,'plus1_counters',0),observe(ref,'minus1_counters',0)]
  for player in (0,1):
   checks += [step('assert_life',player=player,value=life[player])]+[step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
   checks.append(step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values())))
   for z,ns in zones.items():
    checks.append(step('assert_zone',player=player,zone=z,value=len(ns[player])))
    for nm in sorted(set(ns[player])|{name,victim,'Counterspell','Twiddle','Boomerang'}):checks.append(step('assert_zone_multiplicity',player=player,zone=z,index=rows[nm]['csvid'],value=ns[player].count(nm)))
    if z=='library':checks += [step('assert_zone_card',player=player,zone=z,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(ns[player])]
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
   at=0 if v=='ready_snapshot' else len(actions) if v=='resolved_snapshot' else cut
   steps=pre+actions[:at]+[step('capture')]+actions[at:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[at:]+checks+[step('assert_recorded_state')]
  result.append(dict(name=f'TargetRemovalRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,
   _recipe=dict(family='target_removal',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_rules=p['untested_rules'],
    untested_components=['alternative source abilities','additional targets','replacement effects','control changes','multiplayer'])))
 return result
