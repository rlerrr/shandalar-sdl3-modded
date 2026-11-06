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
 if isinstance(d.get('params'),dict) and d['params'].get('schema')=='removal_wave6':return wave6_spec(d,r,'target_removal')
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
 if p.get('_wave6'):return wave6_variants(p)
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
 wave6_validate_witnesses(o,rows)


def expand(name,csvid,d,p,selected,rows,o):
 if p.get('_wave6'):return wave6_expand(name,csvid,d,p,selected,rows,o)
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


# The opt-in profile pins and palette are separate from all earlier cohorts.
WAVE6_PINS=json.loads((Path(__file__).parent/'data/removal_wave6_profiles.json').read_text(encoding='utf8'))


def wave6_exact(value,pin):
 if type(value) is not type(pin):return False
 if isinstance(pin,dict):return value.keys()==pin.keys() and all(wave6_exact(value[k],v) for k,v in pin.items())
 if isinstance(pin,list):return len(value)==len(pin) and all(wave6_exact(a,b) for a,b in zip(value,pin))
 return value==pin


def wave6_validate_witnesses(o,rows):
 for name,pin in WAVE6_PINS['cards'].items():
  if name not in rows or any(not wave6_exact(o.get(name,{}).get(k),v) for k,v in pin.items()):raise ValueError('wave6 removal witness changed: '+name)


def wave6_spec(d,r,family):
 from recipes import cost_from_oracle
 p=d.get('params');pin=WAVE6_PINS['profiles'].get(p['profile']) if isinstance(p,dict) and isinstance(p.get('profile'),str) else None
 if not pin or not wave6_exact(p,pin['params']) or type(d.get('version')) is not int or d['version']!=2 or d.get('family')!=family:raise ValueError('reviewed wave6 removal profile required')
 if not r or any(not wave6_exact(r.get(k),v) for k,v in pin['source'].items()) or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or d.get('oracle_text')!=r['oracle_text']:raise ValueError('whole wave6 source pin changed')
 if (p['mode']=='mass')!=(family=='mass_removal'):raise ValueError('wrong removal profile family')
 mana_cost=r['mana_cost'].replace('{X}','');hybrid=p['cost'].get('hybrid')
 if hybrid:mana_cost=mana_cost.replace('{'+('/'.join(hybrid))+'}','{'+hybrid[0]+'}')
 q=dict(p,_wave6=True,family=family,csvid=pin['csvid'],mana=cost_from_oracle(dict(mana_cost=mana_cost)),instant='Instant' in r['type_line'])
 selected=d.get('variants')
 if not isinstance(selected,list) or not selected or any(type(v) is not str for v in selected) or len(selected)!=len(set(selected)) or set(selected)-set(wave6_variants(q)):raise ValueError('explicit nonempty unique wave6 variants required')
 return q


def wave6_matches(r,p,x=2):
 scope=p['scope'];types=set(r['type_line'].partition(' — ')[0].split())
 if scope=='nonland_artifact':matches='Artifact' in types and 'Land' not in types
 elif scope=='artifact_enchantment_or_flying_creature':matches=bool(types&{'Artifact','Enchantment'}) or 'Creature' in types and 'Flying' in r.get('keywords',[])
 elif scope=='multicolored_creature_or_enchantment':matches=bool(types&{'Creature','Enchantment'}) and len(r.get('colors',[]))>=2
 else:matches=eligible(r,dict(scope=scope,controller='any',state='any',limit=None))
 if not matches:return False
 bound=p['limit']
 if bound:
  value=float(r[{'mana_value':'cmc','power':'power','toughness':'toughness'}[bound['field']]]);limit=x if bound['value']=='x' else bound['value']
  return {'le':value<=limit,'ge':value>=limit,'eq':value==limit}[bound['op']]
 return True


def wave6_variants(p):
 if p['mode']=='mass':vs=['effect','payment','insufficient_mana','wrong_color','bare_zones','cleanup','countered','ready_snapshot','pending_snapshot','resolved_snapshot','empty_board','own_only','opponent_only','stolen','tokens']
 else:
  vs=['effect','payment','insufficient_mana','wrong_color','bare_zones','cleanup','countered','ready_snapshot','pending_snapshot','resolved_snapshot','own_target','stolen_target','lost_all','bounced_target']
  if p['scope']!='permanent':vs+=['wrong_type']
  if any(wave6_matches(WAVE6_PINS['cards'][n],p) for n in ('Darksteel Myr','Darksteel Relic','Darksteel Citadel','Predator Ooze','Darksteel Gargoyle') if n in WAVE6_PINS['cards']):vs+=['indestructible_target']
  if any(wave6_matches(WAVE6_PINS['cards'][n],p) for n in ('Grizzly Bears','Wall of Spears','Suntail Hawk')):vs+=['target_hexproof']
  if p['count']=='x' or p['count']>1:vs+=['partial_lost','short_targets']+(['partial_hexproof'] if 'target_hexproof' in vs else [])
  if p['count']=='x' or p['count']>1:vs+=['mixed_controller']
  if p['secondary'].get('token',{}).get('flying'):vs+=['token_flying']
 if any(wave6_matches(WAVE6_PINS['cards'][n],p) for n in ('Drudge Skeletons','Horned Troll','Clay Statue') if n in WAVE6_PINS['cards']):vs+=['regeneration']
 if p.get('controller')=='target_player':vs+=['own_target','target_shrouded']
 if p.get('target_scopes'):vs+=['second_target_lost','overlap_target']
 if p['cost'].get('pay_life'):vs+=['insufficient_life','life_payment']
 if p['secondary'].get('mill'):vs+=['zero_power','large_power','power_pumped','short_library']
 if p['cost'].get('x_selection')=='targets':vs+=['zero_empty']
 if p['secondary'].get('token',{}).get('flying'):vs+=['token_evasion']
 if p['mode']=='mass' and p['secondary'].get('token',{}).get('flying'):vs+=['token_flying']
 if p.get('optional_second'):vs+=['decline_second']
 if p.get('devoid'):vs+=['devoid_protection']
 if p['scope'] in ('artifact_enchantment_or_flying_creature','multicolored_creature_or_enchantment'):vs+=['predicate_'+n for n in ('artifact','enchantment','flying','ground','multicolor','colorless')]
 if p['scope']=='multicolored_creature_or_enchantment':vs+=['predicate_multicolor_artifact']
 if p['scope']=='nonartifact_creature':vs+=['predicate_artifact_creature']
 if p['scope']=='nonblack_creature':vs+=['predicate_black']
 if p['mode']=='target' and p['limit'] and p['limit']['field']=='power' and p['limit']['value']==3:vs+=['limit_below','limit_at','limit_above']
 if p['scope']=='nonland_artifact':vs+=['scope_boundary']
 if p['secondary'].get('discard'):vs+=['empty_discard_hand','one_discard_card','large_discard_hand']
 if p['cost'].get('hybrid'):vs+=['other_hybrid','payment_other_hybrid']
 result=[f'{v}_{pl}' for pl in (0,1) for v in vs]
 if p['variable']:result += [f'{v}_x{x}_{pl}' for pl in (0,1) for x in (0,1,3,5) for v in ('effect','payment')]
 return result


def wave6_expand(name,csvid,d,p,selected,rows,o):
 from collections import Counter
 from recipes import card,step,observe,cost_from_oracle
 if name!=p['profile'] or csvid!=p['csvid']:raise ValueError('wave6 source identity mismatch')
 if not isinstance(selected,list) or not selected or any(type(v) is not str for v in selected) or len(selected)!=len(set(selected)) or set(selected)-set(wave6_variants(p)):raise ValueError('invalid public wave6 selection')
 result=[]
 board=('Memnite','Ornithopter','Grizzly Bears','Scathe Zombies','Wall of Spears','Phyrexian Hulk','Scaled Wurm','Darksteel Myr','Darksteel Relic','Darksteel Citadel','Sol Ring','Seal of Cleansing','Nyxborn Shieldmate','Whip of Erebos','Forest','Jace Beleren')
 for variant in selected:
  branch,owner=variant.rsplit('_',1);owner=int(owner);x=2
  if '_x' in branch:branch,x=branch.rsplit('_x',1);x=int(x)
  if branch=='zero_empty':x=0
  if p['cost'].get('x_selection')=='targets' and branch in ('short_targets','wrong_type'):x=1 if branch=='short_targets' else 0
  bare=branch=='bare_zones';mass=p['mode']=='mass';secondary=p['secondary'];cost=list(p['mana']);cost[0]+=x if p['variable'] else 0
  hybrid=p['cost'].get('hybrid');other=branch in ('other_hybrid','payment_other_hybrid')
  if hybrid and other:
   first={'B':1,'U':2,'G':3,'R':4,'W':5}[hybrid[0]];second={'B':1,'U':2,'G':3,'R':4,'W':5}[hybrid[1]];cost[second]+=cost[first];cost[first]=0
  zones={z:[[] if bare else list(ns),[] if bare else list(ns)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',list(LIBRARY)*4))}
  if branch in ('empty_discard_hand','one_discard_card','large_discard_hand'):zones['hand'][1-owner]=[] if branch=='empty_discard_hand' else ['Forest'] if branch=='one_discard_card' else list(HAND)*2
  if branch=='short_library':zones['library'][1-owner]=zones['library'][1-owner][:2]
  setup=[];entities={};actions=[];pre=[step('turn',player=owner,value=0),step('discard_policy',value=1)];pools=[[30]*6,[30]*6];life=[20,20];token_counts=[0,0];invalid=set();targets=[];regen=None;unknown=[0,0];poison=[0,0];discard_bounds={}
  if branch in ('insufficient_life','life_payment'):
   life[owner]=2 if branch=='insufficient_life' else 4;pre.append(step('set_life',player=owner,value=life[owner]))
  for z,players in zones.items():
   for pl,ns in enumerate(players):setup.extend(card(n,f'p{pl}_{z}_{i}',pl,z) for i,n in enumerate(ns))
  def add(n,ref,pl,zone='battlefield',stolen=False,token=False,ready=False):
   item=card(n,ref,pl,zone)
   if zone=='battlefield':item['state']=(['untapped'] if ready else ['tapped'])+(['opponent_owned'] if stolen else [])+(['token'] if token else [])
   setup.append(item);entities[ref]=dict(name=n,player=pl,owner=1-pl if stolen else pl,alive=zone=='battlefield',token=token,tapped=not ready,power=int(o[n].get('power',0)),toughness=int(o[n].get('toughness',0)))
   if zone!='battlefield':zones[zone][pl].append(n)
  def leave(ref,dest='graveyard'):
   e=entities[ref];e['alive']=False
   if not e['token']:zones[dest][e['owner']].append(e['name'])
  def done(ref):
   e=entities[ref];zones['hand'][e['player']].remove(e['name']);zones['graveyard'][e['owner']].append(e['name'])
  def pay(pl,vector):pools[pl]=[a-b for a,b in zip(pools[pl],vector)]
  add(name,'source',owner,'hand')
  target_owner=owner if branch=='own_target' else 1-owner
  regenerator=next((n for n in ('Drudge Skeletons','Horned Troll','Clay Statue') if n in WAVE6_PINS['cards'] and wave6_matches(o[n],p,x)),None)
  if mass:
   for pl in (0,1):
    if branch=='empty_board' or branch=='own_only' and pl!=owner or branch=='opponent_only' and pl==owner:continue
    for i,n in enumerate(board):add(n,f'board{pl}_{i}',pl,stolen=branch=='stolen' and pl==owner and i%2==0)
    if branch=='scope_boundary':add('Great Furnace',f'artifact_land{pl}',pl)
    if branch=='tokens':add('Grizzly Bears',f'token_control{pl}',pl,token=True)
   if branch=='regeneration':add(regenerator,'regenerator',target_owner)
  else:
   candidates=[n for n in ('Grizzly Bears','Wall of Spears','Seal of Cleansing','Sol Ring','Forest','Scaled Wurm','Suntail Hawk','Watchwolf') if wave6_matches(o[n],p,x)]
   if not candidates:raise ValueError('profile needs an independent positive target')
   victim=candidates[0]
   if branch=='wrong_type':victim=next(n for n in ('Sol Ring','Grizzly Bears','Forest') if not wave6_matches(o[n],p,x))
   if branch=='indestructible_target':victim=next(n for n in ('Darksteel Myr','Darksteel Relic','Darksteel Citadel','Predator Ooze','Darksteel Gargoyle') if wave6_matches(o[n],p,x))
   if branch=='regeneration':victim=regenerator
   if branch in ('large_power','short_library'):victim='Scaled Wurm'
   if branch=='zero_power':victim='Wall of Ice'
   if branch=='devoid_protection':victim='White Knight'
   if branch.startswith('predicate_'):victim={'artifact':'Sol Ring','enchantment':'Fires of Yavimaya','flying':'Air Elemental','ground':'Grizzly Bears','multicolor':'Watchwolf','colorless':'Ornithopter','multicolor_artifact':'Etherwrought Page','artifact_creature':'Wall of Spears','black':'Scathe Zombies'}[branch[10:]]
   if branch.startswith('limit_'):victim={'below':'Grizzly Bears','at':'Watchwolf','above':'Air Elemental'}[branch[6:]]
   count=x if p['count']=='x' else p['count'];count-=int(branch=='short_targets' and p['count']!='x')
   if branch=='decline_second':count=1
   for i in range(count):
    scope=p.get('target_scopes',[p['scope']]*count)[i]
    local=dict(p,scope=scope);target_name=victim if not p.get('target_scopes') else next(n for n in ('Grizzly Bears','Wall of Spears','Seal of Cleansing','Sol Ring','Forest') if wave6_matches(o[n],local,x))
    if p.get('target_scopes') and branch=='wrong_type' and i==0:target_name=next(n for n in ('Sol Ring','Forest') if not wave6_matches(o[n],local,x))
    if p.get('target_scopes') and branch=='indestructible_target':target_name=next((n for n in ('Darksteel Myr','Darksteel Relic','Darksteel Citadel','Predator Ooze','Darksteel Gargoyle') if wave6_matches(o[n],local,x)),target_name)
    if p.get('target_scopes') and branch=='regeneration' and i==0 and wave6_matches(o[regenerator],local,x):target_name=regenerator
    if p.get('target_scopes') and branch=='devoid_protection' and i==0:target_name='White Knight'
    if p.get('target_scopes') and branch.startswith('predicate_') and i==0:target_name=victim
    ref=f'victim{i}';add(target_name,ref,owner if branch=='mixed_controller' and i%2 else target_owner,stolen=branch=='stolen_target');targets.append(ref)
   if p['count']=='x' and branch=='wrong_type':
    # A wrong-type board doesn't prohibit X=0. Preserve the wrong-type
    # permanent controls and cast with zero targets instead of denying cast.
    for i in range(2):add('Grizzly Bears',f'victim{i}',target_owner)
   if branch=='overlap_target':
    # A separately worded target may refer to the same object when it meets
    # both predicates. A creature land supplies both Fumarole predicates.
    shared=next(n for n in ('Dryad Arbor','Whip of Erebos','Wall of Spears','Nyxborn Shieldmate','Darksteel Citadel') if all(wave6_matches(o[n],dict(p,scope=s),x) for s in p['target_scopes'][:2]))
    removed=targets[:2];setup=[item for item in setup if item['ref'] not in removed];entities={k:e for k,e in entities.items() if k not in removed}
    add(shared,'overlap',target_owner);targets=['overlap','overlap']+targets[2:]
   if branch not in ('wrong_type','short_targets'):
    if branch!='zero_empty':add(victim,'unselected',target_owner)
    control=next((n for n in ('Forest','Sol Ring','Grizzly Bears') if not wave6_matches(o[n],p,x)),None)
    if control:add(control,'other_control',owner)
   if branch=='token_flying':add('Suntail Hawk','flyer',1-target_owner,ready=True)
   if branch=='token_evasion':add('Grizzly Bears','ground_blocker',1-target_owner,ready=True)
  if mass and branch in ('token_flying','token_evasion'):add('Suntail Hawk' if branch=='token_flying' else 'Grizzly Bears','flyer' if branch=='token_flying' else 'ground_blocker',1-target_owner,'hand')
  if branch=='countered':add('Counterspell','response',1-owner,'hand')
  if branch=='bounced_target':add('Boomerang','response',1-owner,'hand')
  if branch in ('target_hexproof','partial_hexproof'):add("Ranger's Guile",'response',target_owner,'hand')
  if branch=='power_pumped':add('Giant Growth','response',target_owner,'hand')
  if branch=='target_shrouded':add('Gilded Light','response',target_owner,'hand')
  if hybrid:
   forbidden=hybrid[0] if other else hybrid[1];pools[owner][{'B':1,'U':2,'G':3,'R':4,'W':5}[forbidden]]=0
  if branch in ('payment','payment_other_hybrid'):pools[owner]=list(cost)
  elif branch=='insufficient_mana':pools[owner]=[0]*6
  elif branch=='wrong_color':
   pools[owner]=list(cost)
   for i in range(1,6):pools[owner][0]+=pools[owner][i];pools[owner][i]=0
  for pl in (0,1):pre.extend(step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(pools[pl]))
  predicate_denied=(branch.startswith('predicate_') or branch.startswith('limit_')) and not wave6_matches(o[victim],p,x)
  pre.append(step('refresh'));denied=(branch in ('insufficient_mana','wrong_color','wrong_type','short_targets','insufficient_life') and not(p['count']=='x' and branch in ('wrong_type','short_targets') or p.get('optional_second') and branch=='short_targets')) or predicate_denied;cut=0
  if denied:
   if branch in ('wrong_type','short_targets','insufficient_life') or predicate_denied:actions.append(step('require_can_cast',ref='source',value=0))
   else:actions.append(step('cast',ref='source',value=0))
   if branch=='insufficient_life':actions.append(step('cast',ref='source',value=0))
  else:
   actions.append(step('can_cast' if p['cost'].get('x_selection')=='targets' else 'require_can_cast',ref='source',value=1))
   if p['variable'] and p['cost'].get('x_selection')!='targets':actions.append(step('choose_x',player=owner,value=x))
   if p.get('controller')=='target_player':actions.append(step('choose_player',player=owner,value=target_owner))
   actions.extend(step('choose_target',player=owner,ref=ref) for ref in targets)
   if branch=='decline_second':actions.append(step('choose_cancel_target',player=owner))
   if p['cost'].get('x_selection')=='targets' and branch not in ('payment','zero_empty','short_targets','wrong_type'):actions.append(step('choose_cancel_target',player=owner))
   # Pending spells retain the physical hand-zone entry, but the ordinary
   # in_hand predicate excludes the spell currently on the stack.
   actions.extend([step('cast',ref='source',value=1),step('assert_stack',value=1),observe('source','in_hand',0),step('assert_zone',player=owner,zone='hand',value=len(zones['hand'][owner]))]);pay(owner,cost)
   if p['cost'].get('pay_life'):life[owner]-=p['cost']['pay_life'];actions.append(step('assert_life',player=owner,value=life[owner]))
   actions.extend(step('assert_mana',player=owner,color=i,value=n) for i,n in enumerate(pools[owner]));cut=len(actions)
   if branch in ('lost_all','partial_lost','second_target_lost'):
    for ref in dict.fromkeys(targets if branch=='lost_all' else targets[-1:] if branch=='second_target_lost' else targets[:1]):actions.append(step('destroy',ref=ref));leave(ref);invalid.add(ref)
   if branch in ('bounced_target','target_hexproof','partial_hexproof','power_pumped'):
    ref=targets[0];pl=entities['response']['player'];actions.append(step('choose_target',player=pl,ref=ref));actions.extend([step('cast_response',ref='response',value=1),step('assert_stack',value=2)])
    pay(pl,cost_from_oracle(o[entities['response']['name']]));done('response')
    if branch!='power_pumped':invalid.add(ref)
    if branch=='bounced_target':leave(ref,'hand')
    else:entities[ref]['power']+=3 if branch=='power_pumped' else 1;entities[ref]['toughness']+=3 if branch=='power_pumped' else 1
   if branch=='target_shrouded':actions.extend([step('cast_response',ref='response',value=1),step('assert_stack',value=2)]);pay(target_owner,cost_from_oracle(o['Gilded Light']));done('response')
   if branch=='countered':actions.extend([step('cast_response',ref='response',value=1),step('assert_stack',value=2)]);pay(1-owner,cost_from_oracle(o['Counterspell']));done('response')
   valid=[ref for ref in dict.fromkeys(targets) if ref not in invalid and entities[ref]['alive']];resolved=branch not in ('countered','target_shrouded') and (mass or bool(valid) or p['count']=='x' and x==0)
   if branch=='regeneration':
    ref='regenerator' if mass else targets[0]
    # Single-card destruction finishes its legacy regeneration window after
    # the handler's scry UI. Bind that real UI order without changing the
    # printed final regeneration, scry, zone or payment expectations.
    if not secondary.get('scry'):actions.append(step('choose_regeneration',ref=ref))
    if p['effect']=='destroy' and not p['no_regenerate'] and wave6_matches(o[entities[ref]['name']],p,x):
     regen=ref;entities[ref]['tapped']=True;pools[entities[ref]['player']][0 if entities[ref]['name']=='Clay Statue' else 3 if entities[ref]['name']=='Horned Troll' else 1]-=2 if entities[ref]['name']=='Clay Statue' else 1
   scry_window=min(secondary.get('scry',0),len(zones['library'][owner]))
   if resolved and scry_window:actions.extend(step('choose_zone',player=owner,value=-1) for _ in range(1 if owner else scry_window))
   if branch=='regeneration' and secondary.get('scry'):actions.append(step('choose_regeneration',ref=ref))
   actions.append(step('resolve_response') if branch in ('countered','bounced_target','target_hexproof','partial_hexproof','power_pumped','target_shrouded') else step('resolve'))
   actions.append(step('assert_unused_choices',value=1) if branch=='regeneration' and not regen else step('assert_choices_consumed'));done('source')
   if resolved:
    victims=[ref for ref,e in entities.items() if e['alive'] and wave6_matches(o[e['name']],p,x) and (p.get('controller')!='target_player' or e['player']==target_owner)] if mass else valid
    destroyed=[]
    for ref in victims:
     e=entities[ref];survives=p['effect']=='destroy' and ('Indestructible' in o[e['name']].get('keywords',[]) or ref==regen)
     if secondary.get('controller_life')=='mana_value_all':life[e['player']]+=0 if e['token'] else int(o[e['name']]['cmc'])
     if not survives:destroyed.append(ref);leave(ref,'exile' if p['effect']=='exile' else 'graveyard')
     token=secondary.get('token')
     if token and (not token.get('for_each') or not survives):token_counts[e['player']]+=token['count']
     if secondary.get('poison'):poison[e['player']]+=secondary['poison']
     if secondary.get('mill'):
      amount=min(max(0,e['power']),len(zones['library'][e['player']]));zones['graveyard'][e['player']]+=zones['library'][e['player']][:amount];del zones['library'][e['player']][:amount]
     if secondary.get('discard'):
      pl=e['player'];amount=min(secondary['discard'],len(zones['hand'][pl]))
      if pl==1 and amount:unknown[pl]+=amount;discard_bounds[pl]=dict(hand=Counter(zones['hand'][pl]),graveyard=Counter(zones['graveyard'][pl]))
      else:
       for _ in range(amount):
        n=min(zones['hand'][pl],key=lambda n:rows[n]['csvid']);zones['hand'][pl].remove(n);zones['graveyard'][pl].append(n)
    life[owner]+=secondary.get('gain_per',0)*len(destroyed)-secondary.get('lose_life',0)
   if branch=='cleanup':actions.extend([step('cleanup'),step('refresh')])
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
  for ref,e in entities.items():
   if e['alive']:
    checks.extend([observe(ref,'in_play',1),observe(ref,'tapped',int(e['tapped']))])
    if 'Creature' in o[e['name']]['type_line']:checks.extend(observe(ref,k,e[k]) for k in ('power','toughness'))
  token=secondary.get('token');universe=set(n for players in zones.values() for ns in players for n in ns)|{e['name'] for e in entities.values()}
  for pl in (0,1):
   checks.extend([step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values())+token_counts[pl])]);checks.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(pools[pl]))
   for n in sorted({e['name'] for e in entities.values()}):checks.append(step('assert_zone_multiplicity',player=pl,zone='battlefield',index=rows[n]['csvid'],value=sum(e['alive'] and e['player']==pl and e['name']==n for e in entities.values())))
   for z,players in zones.items():
    change=unknown[pl]*(1 if z=='graveyard' else -1 if z=='hand' else 0);checks.append(step('assert_zone',player=pl,zone=z,value=len(players[pl])+change))
    for n in sorted(universe):
     if pl in discard_bounds and z in ('hand','graveyard'):
      bound=discard_bounds[pl][z][n]+(min(unknown[pl],discard_bounds[pl]['hand'][n]) if z=='graveyard' else 0)
      checks.append(step('assert_zone_multiplicity_max',player=pl,zone=z,index=rows[n]['csvid'],value=bound))
     else:checks.append(step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=players[pl].count(n)))
    if z=='library':
     reorder=bool(owner and pl==owner and secondary.get('scry',0)>1 and not denied and branch not in ('countered','lost_all'))
     checks.extend(step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(players[pl]) if not reorder or i>=secondary['scry'])
   if secondary.get('poison'):checks.append(step('assert_poison',player=pl,value=poison[pl]))
   if pl in discard_bounds:checks.extend(step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=discard_bounds[pl]['hand'][n]+discard_bounds[pl]['graveyard'][n]) for n in sorted(universe))
   if token:
    tid=rows[token['name']]['csvid'];checks.append(step('assert_zone_multiplicity',player=pl,zone='battlefield',index=tid,value=token_counts[pl]))
    if token_counts[pl]:
     properties=dict(token=1,color=sum(1<<{'B':1,'U':2,'G':3,'R':4,'W':5}[c] for c in token['colors']),artifact=0,power=token['power'],toughness=token['toughness'],sick=1,tapped=0)
     properties.update({'subtype_'+s:1 for s in token.get('observed_subtypes',token['subtypes'])})
     checks.extend(step('assert_matching_permanents',player=pl,index=tid,property=k,value=value) for k,value in properties.items())
  if branch=='token_flying' and not denied:
   if mass:
    pl=1-target_owner;checks.extend([step('turn',player=pl,value=0),step('cast',ref='flyer',value=1),step('resolve'),step('advance_turn'),step('advance_turn')])
   checks.extend([step('bind_permanent_copy',ref='created_token',player=target_owner,index=0,value=rows[token['name']]['csvid']),step('turn',player=1-target_owner,value=0),step('begin_attackers'),step('attack',ref='flyer',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='created_token',other='flyer',value=1),observe('created_token','blocking_count',1)])
  if branch=='token_evasion' and not denied:
   if mass:checks.extend([step('turn',player=1-target_owner,value=0),step('cast',ref='ground_blocker',value=1),step('resolve')])
   checks.extend([step('bind_permanent_copy',ref='created_token',player=target_owner,index=0,value=rows[token['name']]['csvid']),step('advance_turn'),step('advance_turn'),step('turn',player=target_owner,value=0),step('begin_attackers'),step('attack',ref='created_token',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='ground_blocker',other='created_token',value=0)])
  steps=pre+actions+checks
  if branch in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
   at=0 if branch=='ready_snapshot' else cut if branch=='pending_snapshot' else len(actions)
   import copy
   tail=actions[at:]+checks
   steps=pre+actions[:at]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  result.append(dict(name=f'RemovalWave6Recipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family=p['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps'])))
 return result
