"""Fail-closed whole-text specifications for fixed token makers."""
import copy,json,re
from pathlib import Path
from creature_recipes import KEYWORDS as SOURCE_KEYWORDS, REMINDERS
from token_creation_recipes import COLORS, WITNESSES

EXTRA_WITNESSES={
 'Murder':('{1}{B}{B}','Destroy target creature.'),
 'Lightning Bolt':('{R}','Lightning Bolt deals 3 damage to any target.'),
 'Scaled Wurm':('{7}{G}','','7','6'),
 'Twiddle':('{U}','You may tap or untap target artifact, creature, or land.'),
}
TOKEN_KEYWORDS={'Flying','Trample','Haste','Vigilance','Deathtouch','Reach','Lifelink'}
SUBTYPES={'Angel','Bear','Bird','Boar','Cat','Centaur','Citizen','Dragon','Drake','Eldrazi','Elemental','Elf','Goblin','Horror','Human','Illusion','Insect','Knight','Myr','Phyrexian','Saproling','Servo','Sliver','Soldier','Spider','Spirit','Thopter','Warrior','Wurm','Zombie','Elephant','Kithkin','Treefolk','Shaman','Snake','Merfolk','Wizard','Golem','Rogue','Scion','Spawn','Bat','Construct','Squirrel','Beast'}
TOKEN_IDENTITIES={'Phyrexian Myr':'Myr','Phyrexian Golem':'Golem'}
SOURCE_TOKEN_IDENTITIES={'Roc Egg':('Roc',18810)}
def token_identity(p):
 return SOURCE_TOKEN_IDENTITIES.get(p['source_name'],(TOKEN_IDENTITIES.get(p['token']['name'],p['token']['name']),None))[0]
WORDS={'a':1,'two':2,'three':3,'four':4,'seven':7}
# Whole exact inactive clauses are accepted as explicit component gaps. They do
# not change the exercised state; opponent-cast cases exclude the Tithe tax.
INACTIVE={
 'Thraben Doomsayer': ['Fateful hour — As long as you have 5 or less life, other creatures you control get +2/+2.'],
 'Seraph of the Scales': ['{W}: This creature gains vigilance until end of turn.','{B}: This creature gains deathtouch until end of turn.'],
 'Tithe Taker': ["During your turn, spells your opponents cast cost {1} more to cast and abilities your opponents activate cost {1} more to activate unless they're mana abilities."],
 'Mardu Strike Leader': ["Dash {3}{B} (You may cast this spell for its dash cost. If you do, it gains haste, and it's returned from the battlefield to its owner's hand at the beginning of the next end step.)"],
}

def creation(text,extended=False):
 pattern=r'Create (a|two|three|four|seven) ([1-8])/([1-9]|1[0-2]) (white|blue|black|red|green|colorless)(?: and (white|blue|black|red|green))? ([A-Za-z ]+?) (artifact )?creature tokens?(?: with ([a-z ,]+))?\.'
 if extended:pattern=pattern.replace('([1-8])','([0-8])')
 m=re.fullmatch(pattern,text)
 if not m:raise ValueError('one fixed ordinary creation clause required')
 count,power,toughness,color,second,subtypes,artifact,keywords=m.groups()
 types=subtypes.split();keys=[x.capitalize() for x in re.split(r' and |, ',keywords)] if keywords else []
 if any(t not in SUBTYPES for t in types) or len(types)>2 or any(k not in TOKEN_KEYWORDS for k in keys) or len(set(keys))!=len(keys):raise ValueError('unreviewed token subtype or keyword')
 return dict(name=' '.join(types),count=WORDS[count],power=int(power),toughness=int(toughness),colors=[COLORS[c] for c in (color,second) if c and c!='colorless'],subtypes=types,artifact=bool(artifact),keywords=keys)

def spec(d,r):
 if isinstance(d.get('params'),dict) and d['params'].get('profile')=='creator_wave6':return wave6_spec(d,r,'token_trigger_creation')
 if isinstance(d.get("params"),dict) and d["params"].get("profile")=="inspired_payment":return inspired_spec(d,r)
 if isinstance(d.get("params"),dict) and d["params"].get("profile")=="death_watch":return death_watch_spec(d,r)
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or 'Creature' not in r.get('type_line','') or r.get('type_line','').startswith('Token ') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('whole pinned normal token-maker creature required')
 p=d.get('params')
 if isinstance(p,dict) and 'profile' in p:return entry_component_spec(d,r)
 if not isinstance(p,dict) or set(p)!={'mode','keywords','token','activation','inactive_clauses'}:raise ValueError('complete explicit token component parameters required')
 if p['mode'] not in ('death','activation','attack','entry_attack','own_spell','opponent_spell'):raise ValueError('unreviewed token event')
 clauses=[];keys=[];inactive=[];canonical={k.lower():k for k in SOURCE_KEYWORDS};afterlife=False;activation=None
 for line in r['oracle_text'].splitlines():
  if line in INACTIVE.get(r['name'],[]):inactive.append(line);continue
  body=None
  if p['mode']=='death':
   m=re.fullmatch(r'When (?:this creature|'+re.escape(r['name'])+r') dies, (create .+)',line)
   a=re.fullmatch(r'Afterlife ([12]) \(When this creature dies, (create .+)\)',line)
   if m:body=m[1]
   if a:
    body=a[2];afterlife=True
    if creation(body[0].upper()+body[1:])['count']!=int(a[1]):raise ValueError('Afterlife amount disagrees with reminder')
  elif p['mode']=='activation':
   m=re.fullmatch(r'((?:\{[0-9WUBRGC]+\})*)(, \{T\})?(, Sacrifice this creature)?: (Create .+)',line)
   if line.startswith('{T}: '):activation=dict(mana=[0]*6,tap=True,sacrifice=False);body=line[5:]
   elif m:
    activation=dict(mana=cost_from_oracle(dict(mana_cost=m[1])),tap=bool(m[2]),sacrifice=bool(m[3]));body=m[4]
  elif p['mode']=='attack':
   m=re.fullmatch(r'Whenever (?:this creature|'+re.escape(r['name'])+r') attacks, (create .+)',line)
   if m:body=m[1]
  elif p['mode']=='entry_attack':
   m=re.fullmatch(r'Whenever this creature enters or attacks, (create .+)',line)
   if m:body=m[1]
  elif p['mode']=='own_spell':
   m=re.fullmatch(r'Whenever you cast an instant or sorcery spell, (create .+)',line)
   if m:body=m[1]
  elif p['mode']=='opponent_spell':
   m=re.fullmatch(r'Whenever an opponent casts a spell, (create .+)',line)
   if m:body=m[1]
  if body:
   clauses.append(creation(body[0].upper()+body[1:]));continue
  passive=line
  for reminder in re.findall(r'\([^\n]*?\)',line):
   if reminder not in REMINDERS:raise ValueError('unreviewed source reminder')
   passive=passive.replace(reminder,'')
  for k in passive.strip().split(','):
   if k.strip().lower() not in canonical:raise ValueError('unreviewed additional token-maker clause: '+line)
   keys.append(canonical[k.strip().lower()])
 if len(clauses)!=1 or clauses[0]!=p['token'] or sorted(keys)!=p['keywords'] or len(set(keys))!=len(keys) or (set(keys)|({'Afterlife'} if afterlife else set())|({'Dash'} if any(l.startswith('Dash ') for l in inactive) else set())|({'Fateful hour'} if r['name']=='Thraben Doomsayer' else set()))!=set(r.get('keywords',[])) or inactive!=p['inactive_clauses']:raise ValueError('token-maker whole-text parameters disagree')
 if activation is not None and (not isinstance(p['activation'],dict) or set(p['activation'])!={'mana','tap','sacrifice'}):raise ValueError('complete explicit activation costs required')
 if activation is not None and (any(type(n) is not int for n in p['activation'].get('mana',[])) or type(p['activation'].get('tap')) is not bool or type(p['activation'].get('sacrifice')) is not bool):raise ValueError('typed explicit activation costs required')
 if p['activation']!=activation:raise ValueError('activation costs disagree')
 for field in ('power','toughness'):
  if not re.fullmatch(r'\d+',r.get(field,'')) or not 0<=int(r[field])<=15:raise ValueError('bounded numeric source required')
 if not int(r['toughness']):raise ValueError('source must survive casting')
 if type(p['token']['artifact']) is not bool or any(type(p['token'][k]) is not int for k in ('count','power','toughness')):raise ValueError('integer token characteristics required')
 return dict(p,family='token_trigger_creation',source_name=r['name'],creature=True,mana=cost_from_oracle(r),power=int(r['power']),toughness=int(r['toughness']),gaps=['inactive: '+line for line in inactive])

def variants(p):
 if p.get('_wave6_component'):return wave6_variants(p)
 if p.get("_inspired_component"):return inspired_variants(p)
 if p.get("_death_watch_component"):return death_watch_variants(p)
 if p.get('_variable_component'):return variable_variants(p)
 if p.get('_event_component'):return event_component_variants(p)
 if p.get('_entry_component'):return entry_component_variants(p)
 common=['create','cast_only','payment','insufficient_mana','two_casts','cleanup','bare_zones','ready_snapshot','pending_snapshot','effect_snapshot','resolved_snapshot','countered','fresh_attack','ready_attack','block_ground','block_flying','destroy_token','bounce_token','exile_token','wrath','other_creature']
 if any(p['mana'][1:]):common+=['wrong_color']
 if p['mode']=='death':
  common+=['murder','bounce_only','bounce_recast','blink_only','blink_destroy','exile_source']
  if p['toughness']<=3:common+=['lethal_damage']
  if p['toughness']<=7 and not set(p['keywords'])&{'First strike','Double strike','Lifelink'}:common+=['combat_death']
 else:
  common+=['source_loss'];common.remove('two_casts')
 if p['mode']=='activation':
  common+=['activation_payment']
  if sum(p['activation']['mana']):common+=['activation_insufficient']
  if any(p['activation']['mana'][1:]):common+=['activation_wrong_color']
  if p['activation']['tap']:common+=['fresh_denied','tapped_denied','repeat_denied']
  elif not p['activation']['sacrifice']:common+=['repeat_activation']
 if 'Flying' in p['token']['keywords']:common+=['evasion','reach_blocker']
 if 'Trample' in p['token']['keywords']:common+=['trample']
 return [v+'_'+str(owner) for owner in (0,1) for v in common]

def validate_witnesses(o,rows,p):
 if p.get('_wave6_component'):return validate_wave6_witnesses(o,rows,p)
 if p.get("_inspired_component"):return validate_variable_witnesses(o,rows,p)
 if p.get("_death_watch_component"):return validate_variable_witnesses(o,rows,p)
 if p.get('_variable_component'):return validate_variable_witnesses(o,rows,p)
 pins=json.loads((Path(__file__).parent/'data/token_trigger_creation_witnesses.json').read_text(encoding='utf8'))['cards']
 for name,pin in pins.items():
  if any(o.get(name,{}).get(k)!=value for k,value in pin.items()):raise ValueError('complete token-trigger witness changed: '+name)
 for name,fields in dict(WITNESSES,**EXTRA_WITNESSES).items():
  r=o.get(name,{})
  if name not in rows or tuple(r.get(k) for k in ('mana_cost','oracle_text','power','toughness')[:len(fields)])!=fields:raise ValueError('token-trigger witness changed: '+name)
 identity=token_identity(p)
 if identity not in rows:raise ValueError('registered token identity required')
 if p['source_name'] in SOURCE_TOKEN_IDENTITIES:
  if rows[identity]['csvid']!=SOURCE_TOKEN_IDENTITIES[p['source_name']][1]:raise ValueError('legacy token identity changed')
 elif not o.get(identity,{}).get('type_line','').startswith('Token '):raise ValueError('pinned token identity required')


def entry_profiles():
 return json.loads((Path(__file__).parent/'data/token_trigger_creation_witnesses.json').read_text(encoding='utf8')).get('profiles',{})

STATIC_ENTRY_CLAUSES={
 'Golems you control get +1/+1.':dict(power=1,toughness=1,keywords=[]),
}

def component_creation(body):
 text=body[0].upper()+body[1:];mana=False
 for reminder in (' ({C} represents colorless mana.)'," (A creature with flying can't be blocked except by creatures with flying or reach.)"):
  if text.endswith(reminder):text=text[:-len(reminder)]
 m=re.fullmatch(r'(.+creature tokens?\.) (?:It has|They have) "Sacrifice this token: Add \{C\}."',text)
 if m:text=m[1];mana=True
 return creation(text,extended=True),mana

def entry_component_spec(d,r):
 from recipes import cost_from_oracle
 from creature_recipes import oracle_keywords
 p=d['params']
 if p.get('profile')=='variable_amount':return variable_spec(d,r)
 profile=entry_profiles().get(r['name'])
 if not isinstance(profile,dict) or any(r.get(k)!=value for k,value in profile['source'].items()):raise ValueError('reviewed entry source profile changed')
 if p.get('profile')=='grant_component':return grant_component_spec(d,r)
 if p.get('profile')=='event_component':return event_component_spec(d,r)
 if p.get('profile')=='followup_component':return followup_component_spec(d,r)
 allowed={'profile','keywords','token','inactive_clauses','fabricate'}
 if profile.get('active_static_clause'):allowed.add('active_static')
 if profile.get('token_mana'):allowed.add('token_mana')
 if profile.get('devoid_clause'):allowed.add('devoid')
 if set(p)!=allowed or p['profile']!='entry_component':raise ValueError('explicit entry component schema required')
 clause=profile['entry_clause'];fabricate=0;event=profile['mode']
 if event=='fabricate':
  m=re.fullmatch(r'Fabricate ([12]) \(When this creature enters, put (?:a|two) \+1/\+1 counters? on it or (create .+)\)',clause)
  if not m:raise ValueError('reviewed bounded Fabricate clause required')
  fabricate=int(m[1]);body=m[2]
 else:
  refs='this creature|'+re.escape(r['name'])+'|'+re.escape(r['name'].split(',')[0])
  word='dies' if event=='death' else 'enters'
  m=re.fullmatch(r'When (?:'+refs+r') '+word+r', (create .+)',clause)
  if not m:raise ValueError('reviewed fixed entry/death clause required')
  body=m[1]
 token,mana_ability=component_creation(body)
 if mana_ability!=bool(profile.get('token_mana')) or (mana_ability and p['token_mana'] is not True):raise ValueError('printed token mana ability required')
 if fabricate and token['count']!=fabricate:raise ValueError('Fabricate number and Servo reminder disagree')
 inactive=profile['inactive_clauses'];static=profile.get('active_static_clause');lines=r['oracle_text'].splitlines()
 if lines.count(clause)!=1 or any(lines.count(line)!=1 for line in inactive):raise ValueError('unique complete entry clauses required')
 effect=None
 if static:
  effect=STATIC_ENTRY_CLAUSES.get(static)
  if not isinstance(p.get('active_static'),dict) or set(p['active_static'])!={'power','toughness','keywords'} or any(type(p['active_static'].get(k)) is not int for k in ('power','toughness')):raise ValueError('typed explicit static token bonus required')
  if effect is None or lines.count(static)!=1 or p.get('active_static')!=effect or 'Golem' not in token['subtypes']:raise ValueError('explicit reviewed static token bonus required')
 devoid=profile.get('devoid_clause')
 if devoid and (devoid!='Devoid (This card has no color.)' or lines.count(devoid)!=1 or p.get('devoid') is not True or r['colors']!=[]):raise ValueError('printed mandatory colorless source property required')
 passive=[line for line in lines if line!=clause and line not in inactive and line!=static and line!=devoid]
 inactive_meta=profile.get('inactive_keywords',[])
 if any(not (k=='Regenerate' and any('Regenerate ' in line for line in inactive) or k=='Improvise' and any(line.startswith('Improvise (') for line in inactive)) for k in inactive_meta):raise ValueError('unreviewed inactive source keyword metadata')
 keys=oracle_keywords(dict(r,oracle_text='\n'.join(passive),keywords=[k for k in r['keywords'] if k!='Fabricate' and (k!='Devoid' or not devoid) and k not in inactive_meta]))
 if p['keywords']!=keys or p['token']!=token or p['inactive_clauses']!=inactive or type(p['fabricate']) is not int or p['fabricate']!=fabricate:raise ValueError('explicit entry component disagrees with creator Oracle')
 if type(p['token']['artifact']) is not bool or any(type(p['token'][k]) is not int for k in ('count','power','toughness')):raise ValueError('typed token definition required')
 return dict(p,_entry_component=True,family='token_trigger_creation',source_name=r['name'],mode='death' if event=='death' else 'entry',creature=True,activation=None,mana=cost_from_oracle(r),power=int(r['power']),toughness=int(r['toughness']),gaps=['inactive: '+line for line in inactive],static_effect=effect,token_mana_ability=mana_ability,devoid_source=bool(devoid),legendary='Legendary' in r['type_line'])

def entry_component_variants(p):
 if p.get('_followup_component'):return followup_component_variants(p)
 if p.get('_grant_component'):return grant_component_variants(p)
 from token_creation_recipes import variants as entry_variants
 result=variants(dict(p,_entry_component=False)) if p['mode']=='death' else entry_variants(p)
 if p['fabricate']:
  result=[v for v in result if not v.startswith('preloaded_source_')]
  result += [v+'_'+str(owner) for owner in (0,1) for v in ('fabricate_counters','fabricate_counters_cleanup','fabricate_counters_snapshot')]
 if p.get('legendary') or p['source_name']=='Maverick Thopterist':result=[v for v in result if not v.startswith('two_casts_')]
 if p.get('token_mana_ability'):result += [v+'_'+str(owner) for owner in (0,1) for v in ('token_mana','token_mana_all','token_mana_snapshot')]
 return result


GRANT_ENTRY_CLAUSES={
 'Golems you control have first strike.':dict(scope='golem',power=0,toughness=0,keywords=['First strike']),
 'Golem creatures you control have flying.':dict(scope='golem',power=0,toughness=0,keywords=['Flying']),
 'Golem creatures you control have trample.':dict(scope='golem',power=0,toughness=0,keywords=['Trample']),
 'Golem creatures you control have vigilance.':dict(scope='golem',power=0,toughness=0,keywords=['Vigilance']),
 "Artifact creatures you control have haste. (They can attack and {T} as soon as they come under your control.)":dict(scope='artifact',power=0,toughness=0,keywords=['Haste']),
 'Other creatures you control get +1/+1.':dict(scope='all_other',power=1,toughness=1,keywords=[]),
 'Other Soldier creatures you control get +1/+1 and have vigilance.':dict(scope='soldier_other',power=1,toughness=1,keywords=['Vigilance']),
 'Squirrel creatures get +1/+1.':dict(scope='squirrel_all',power=1,toughness=1,keywords=[]),
}

def grant_component_spec(d,r):
 from recipes import cost_from_oracle
 from creature_recipes import oracle_keywords
 profile=entry_profiles()[r['name']];p=d['params']
 if profile.get('profile')!='grant_component' or set(p)!=({'profile','keywords','token','inactive_clauses','fabricate','active_static'}|({'owner_return'} if profile.get('owner_return_clause') else set())):raise ValueError('explicit reviewed grant component required')
 clause=profile['entry_clause'];fabricate=0
 if profile['mode']=='fabricate':
  m=re.fullmatch(r'Fabricate ([12]) \(When this creature enters, put (?:a|two) \+1/\+1 counters? on it or (create .+)\)',clause)
  if not m:raise ValueError('bounded whole Fabricate clause required')
  fabricate=int(m[1]);body=m[2]
 else:
  refs='this creature|'+re.escape(r['name'])+'|'+re.escape(r['name'].split(',')[0])+('|Trostani' if r['name']=='Trostani Discordant' else '');m=re.fullmatch(r'When (?:'+refs+r') enters, (create .+)',clause)
  if not m:raise ValueError('whole fixed entry clause required')
  body=m[1]
 token,mana=component_creation(body)
 if mana or fabricate and token['count']!=fabricate:raise ValueError('reviewed grant-maker token body required')
 grant=profile['active_static_clause'];effect=GRANT_ENTRY_CLAUSES.get(grant);inactive=profile['inactive_clauses'];lines=r['oracle_text'].splitlines()
 if effect is None or lines.count(grant)!=1 or lines.count(clause)!=1 or any(lines.count(line)!=1 for line in inactive):raise ValueError('complete unique printed grant components required')
 declared=p['active_static']
 if not isinstance(declared,dict) or set(declared)!={'scope','power','toughness','keywords'} or any(type(declared.get(k)) is not int for k in ('power','toughness')) or declared!=effect:raise ValueError('typed explicit scope and grant required')
 if effect['scope']=='golem' and 'Golem' not in token['subtypes'] or effect['scope']=='artifact' and not token['artifact'] or effect['scope']=='soldier_other' and 'Soldier' not in token['subtypes'] or effect['scope']=='squirrel_all' and 'Squirrel' not in token['subtypes']:raise ValueError('printed grant predicate must match the reviewed token')
 owner_return=profile.get('owner_return_clause')
 if owner_return and (r['name']!='Trostani Discordant' or owner_return!='At the beginning of your end step, each player gains control of all creatures they own.' or lines.count(owner_return)!=1 or p['owner_return'] is not True):raise ValueError('whole active owner-return component required')
 passive=[line for line in lines if line!=clause and line!=grant and line not in inactive and line!=owner_return]
 inactive_meta=profile.get('inactive_keywords',[])
 if any(k!='Echo' or not any(line.startswith('Echo ') for line in inactive) for k in inactive_meta):raise ValueError('unreviewed inactive grant-maker metadata')
 keys=oracle_keywords(dict(r,oracle_text='\n'.join(passive),keywords=[k for k in r['keywords'] if k!='Fabricate' and k not in inactive_meta]))
 if p['keywords']!=keys or p['token']!=token or p['inactive_clauses']!=inactive or type(p['fabricate']) is not int or p['fabricate']!=fabricate:raise ValueError('explicit grant maker disagrees with creator Oracle')
 if type(p['token']['artifact']) is not bool or any(type(p['token'][k]) is not int for k in ('count','power','toughness')):raise ValueError('typed token characteristics required')
 return dict(p,_owner_return=bool(owner_return),_entry_component=True,_grant_component=True,family='token_trigger_creation',source_name=r['name'],mode='entry',creature=True,activation=None,mana=cost_from_oracle(r),power=int(r['power']),toughness=int(r['toughness']),static_effect={k:effect[k] for k in ('power','toughness','keywords')},grant_scope=effect['scope'],token_mana_ability=False,devoid_source=False,legendary='Legendary' in r['type_line'],gaps=['inactive: '+line for line in inactive])

def grant_component_variants(p):
 import copy
 effective=copy.deepcopy(p);effective['_grant_component']=False;effective['token']['keywords']+=p['static_effect']['keywords']
 result=entry_component_variants(effective)
 if p['static_effect']['keywords']:result += ['grant_lost_'+str(owner) for owner in (0,1)]
 if p.get('_owner_return'):result += [v+'_'+str(owner) for owner in (0,1) for v in ('owner_return','owner_return_snapshot','owner_return_wrong_turn','owner_return_source_lost')]
 if p['grant_scope'] in ('squirrel_all','soldier_other'):result += [v+'_'+str(owner) for owner in (0,1) for v in ('numeric_scope','numeric_scope_snapshot')]
 if p['grant_scope']=='golem':result += [v+'_'+str(owner) for owner in (0,1) for v in ('grant_scope','grant_scope_snapshot')]
 if p['grant_scope']=='artifact' and p['static_effect']['keywords']==['Haste']:result += [v+'_'+str(owner) for owner in (0,1) for v in ('haste_scope','haste_scope_snapshot')]
 return result


def event_component_spec(d,r):
 from recipes import cost_from_oracle
 from creature_recipes import oracle_keywords
 profile=entry_profiles()[r['name']];p=d['params'];event=profile['mode'];clause=profile['entry_clause'];inactive=profile['inactive_clauses'];lines=r['oracle_text'].splitlines()
 if profile.get('profile')!='event_component' or set(p)!=({'profile','keywords','token','inactive_clauses','activation','token_mana'}|({'optional'} if event=='landfall' else set())):raise ValueError('complete event component parameters required')
 if event=='entry_death':
  refs='this creature|'+re.escape(r['name']);m=re.fullmatch(r'When (?:'+refs+r') enters or dies, (create .+)',clause)
  if not m:raise ValueError('whole entry-or-death clause required')
  body=m[1];activation=None
 elif event=='landfall':
  m=re.fullmatch(r'Landfall \u2014 Whenever a land you control enters, (you may )?(create .+)',clause)
  if not m or type(p['optional']) is not bool or p['optional']!=bool(m[1]):raise ValueError('whole reviewed landfall clause required')
  body=m[2];activation=None
 elif event=='activation':
  m=re.fullmatch(r'((?:\{[0-9WUBRG]+\})*)(, \{T\})?: (Create .+)',clause)
  if not m:raise ValueError('whole fixed mana/tap activation required')
  body=m[3];activation=dict(mana=cost_from_oracle(dict(mana_cost=m[1])),tap=bool(m[2]),sacrifice=False)
 else:raise ValueError('reviewed event component required')
 functional_priority=profile.get('functional_trigger_priority',False)
 if type(functional_priority) is not bool or functional_priority and (r['name']!='Rampaging Baloths' or event!='landfall' or p.get('optional') is not False):raise ValueError('unreviewed mandatory-trigger presentation binding')
 token,mana_ability=component_creation(body)
 if lines.count(clause)!=1 or any(lines.count(line)!=1 for line in inactive):raise ValueError('complete unique event clauses required')
 inactive_meta=profile.get('inactive_keywords',[])
 if any(not (k=='Echo' and any(line.startswith('Echo ') for line in inactive) or k=='Annihilator' and any(line.startswith('Annihilator ') for line in inactive) or k=='Populate' and any(': Populate.' in line for line in inactive) or k=='Mill' and any(' mills ' in line for line in inactive)) for k in inactive_meta):raise ValueError('unreviewed inactive event metadata')
 keys=oracle_keywords(dict(r,oracle_text='\n'.join(line for line in lines if line!=clause and line not in inactive),keywords=[k for k in r['keywords'] if k not in inactive_meta and (k!='Landfall' or event!='landfall')]))
 if p['keywords']!=keys or p['token']!=token or p['inactive_clauses']!=inactive or p['activation']!=activation or type(p['token_mana']) is not bool or p['token_mana']!=mana_ability:raise ValueError('event parameters disagree with creator Oracle')
 if activation is not None and (not isinstance(p['activation'],dict) or set(p['activation'])!={'mana','tap','sacrifice'} or any(type(n) is not int for n in p['activation']['mana']) or type(p['activation']['tap']) is not bool or type(p['activation']['sacrifice']) is not bool):raise ValueError('typed activation cost required')
 if type(p['token']['artifact']) is not bool or any(type(p['token'][k]) is not int for k in ('count','power','toughness')):raise ValueError('typed event token required')
 return dict(p,_landfall_functional_priority=functional_priority,_landfall=event=='landfall',_event_component=True,_entry_death=event=='entry_death',family='token_trigger_creation',source_name=r['name'],mode='death' if event=='entry_death' else event,creature=True,mana=cost_from_oracle(r),power=int(r['power']),toughness=int(r['toughness']),token_mana_ability=mana_ability,legendary='Legendary' in r['type_line'],gaps=['inactive: '+line for line in inactive])


def event_component_variants(p):
 result=variants(dict(p,_event_component=False))
 # An alternative ability may remain legal even when the selected token ability
 # is short of one color or its source is tapped. Those probes require an
 # explicit mode-rejection interface rather than a card-wide CAN_ACTIVATE.
 excluded=['activation_wrong_color']
 if p['source_name']=='Lich Lord of Unx':excluded+=['fresh_denied','tapped_denied','repeat_denied']
 if p['legendary']:excluded+=['two_casts']
 result=[v for v in result if v.rsplit('_',1)[0] not in excluded]
 if p.get('token_mana_ability'):result += [v+'_'+str(owner) for owner in (0,1) for v in ('token_mana','token_mana_all','token_mana_snapshot')]
 if p.get('_landfall'):
  result += [v+'_'+str(owner) for owner in (0,1) for v in ('opponent_land','source_before_land')]
  if p['optional']:result+=['land_decline_0']
 return result


def followup_component_spec(d,r):
 from recipes import cost_from_oracle
 from creature_recipes import oracle_keywords
 profile=entry_profiles()[r['name']];p=d['params'];clause=profile['entry_clause'];lines=r['oracle_text'].splitlines()
 if profile.get('profile')!='followup_component' or set(p)!={'profile','keywords','token','inactive_clauses','fabricate','followup'}:raise ValueError('complete mandatory followup component required')
 if profile['mode']!='fabricate' or r['name']!='Marionette Master':raise ValueError('reviewed Fabricate followup source required')
 m=re.fullmatch(r'Fabricate (3) \(When this creature enters, put three \+1/\+1 counters on it or (create .+)\)',clause)
 followup="Whenever an artifact you control is put into a graveyard from the battlefield, target opponent loses life equal to this creature's power."
 if not m or profile.get('followup_clause')!=followup or p['followup']!='artifact_loss' or lines.count(clause)!=1 or lines.count(followup)!=1 or profile['inactive_clauses']!=[]:raise ValueError('whole mandatory artifact loss clause required')
 token,mana=component_creation(m[2]);keys=oracle_keywords(dict(r,oracle_text='\n'.join(l for l in lines if l not in (clause,followup)),keywords=[k for k in r['keywords'] if k!='Fabricate']))
 if mana or token['count']!=3 or p['token']!=token or p['keywords']!=keys or p['inactive_clauses']!=[] or type(p['fabricate']) is not int or p['fabricate']!=3 or any(type(p['token'][k]) is not int for k in ('count','power','toughness')) or type(p['token']['artifact']) is not bool:raise ValueError('Fabricate3 parameters disagree with creator Oracle')
 return dict(p,_entry_component=True,_followup_component=True,family='token_trigger_creation',source_name=r['name'],mode='entry',creature=True,activation=None,mana=cost_from_oracle(r),power=int(r['power']),toughness=int(r['toughness']),static_effect=None,token_mana_ability=False,devoid_source=False,legendary=False,gaps=[])


def followup_component_variants(p):
 result=entry_component_variants(dict(p,_followup_component=False))
 return result+[v+'_'+str(owner) for owner in (0,1) for v in ('own_artifact','power_artifact','power_artifact_snapshot','power_wrath','power_wrath_snapshot','opponent_artifact','source_lost_artifact','nonartifact_death','hexproof_opponent')]



# Variable quantity components, independently pinned and opt-in only. No old
# declaration receives these branches or additional runtime steps.
VARIABLE_COLOR_INDEX={'B':1,'U':2,'G':3,'R':4,'W':5}
VARIABLE_COLOR_WORD={'black':'B','blue':'U','green':'G','red':'R','white':'W'}
VARIABLE_SUBTYPES=SUBTYPES|{'Plant'}

def variable_data():
 data=json.loads((Path(__file__).parent/'data/token_trigger_creation_witnesses.json').read_text(encoding='utf8'))
 return dict(profiles=data.get('variable_profiles',{}),cards=data.get('variable_witnesses',{}),registry_identity_only=data.get('variable_registry_identity',{}),witness_token_components=data.get('variable_witness_tokens',{}))

def variable_exact(a,b):
 if type(a) is not type(b):return False
 if isinstance(a,dict):return a.keys()==b.keys() and all(variable_exact(a[k],b[k]) for k in a)
 if isinstance(a,list):return len(a)==len(b) and all(variable_exact(x,y) for x,y in zip(a,b))
 return a==b

def variable_cost(record):
 text=record.get('mana_cost','');symbols=re.findall(r'\{(\d+|[BUGRW])\}',text)
 if ''.join('{'+s+'}' for s in symbols)!=text:raise ValueError('unreviewed mana syntax')
 out=[0]*6
 for s in symbols:out[0 if s.isdigit() else VARIABLE_COLOR_INDEX[s]]+=int(s) if s.isdigit() else 1
 return out

def variable_token_from_creator(text):
 m=re.search(r'[Cc]reate (a number of|a|two|X) (?:tapped )?(\d+)/(\d+) (white|blue|black|red|green|colorless)(?: and (white|blue|black|red|green))? ([A-Za-z ]+?) (artifact )?creature tokens?(?: with ([a-z ,]+?))?(?=\.|, where| for each| equal to|$)',text)
 if not m:raise ValueError('fixed body with variable quantity required')
 _,power,toughness,c1,c2,types,artifact,keywords=m.groups();types=types.split();keys=[k.capitalize() for k in re.split(r', | and ',keywords)] if keywords else []
 if any(t not in VARIABLE_SUBTYPES for t in types) or any(k not in TOKEN_KEYWORDS for k in keys):raise ValueError('unreviewed token characteristics')
 return dict(name=' '.join(types),power=int(power),toughness=int(toughness),colors=[VARIABLE_COLOR_WORD[c] for c in (c1,c2) if c and c!='colorless'],subtypes=types,artifact=bool(artifact),keywords=keys)

def variable_spec(d,r,data=None,family='token_trigger_creation'):
 data=data or variable_data();profile=data['profiles'].get(r.get('name'))
 if not profile or not variable_exact(profile['source'],r) or r.get('layout')!='normal' or r.get('card_faces') or (family=='token_trigger_creation' and 'Creature' not in r.get('type_line','')) or (family=='token_spell_extension' and 'Creature' in r.get('type_line','')) or r['type_line'].startswith('Token '):raise ValueError('full pinned normal source required')
 if family not in ('token_trigger_creation','token_spell_extension') or d.get('family')!=family or d.get('version')!=2 or d.get('oracle_text')!=r['oracle_text'] or 'variants' not in d:raise ValueError('whole pinned variable component family required')
 expected={k:profile[k] for k in ('profile','basis','token','source_keywords','inactive_clauses','mandatory_followups')}
 if not variable_exact(d.get('params'),expected):raise ValueError('complete typed variable component declaration required')
 lines=r['oracle_text'].splitlines();creation=[l for l in lines if re.search(r'\bcreate ',l)]
 if len(creation)!=1 or not variable_exact(variable_token_from_creator(creation[0]),profile['token']):raise ValueError('token body disagrees with independent creator clause')
 clause=creation[0];basis=profile['basis']
 suffixes={'controlled_lands':'for each land you control.','own_grave_nontoken_deaths_this_turn':'for each nontoken creature put into your graveyard from the battlefield this turn.'}
 if basis in suffixes and not clause.endswith(suffixes[basis]):raise ValueError('quantity basis disagrees')
 if basis=='devotion_white' and not ('equal to your devotion to white.' in clause and '(Each {W} in the mana costs of permanents you control counts toward your devotion to white.)' in clause):raise ValueError('whole devotion rule required')
 if basis=='creature_cards_in_own_graveyard' and not (clause.endswith('for each creature card in your graveyard.') or clause.endswith('where X is the number of creature cards in your graveyard.')):raise ValueError('whole graveyard-card rule required')
 if basis not in set(suffixes)|{'devotion_white','creature_cards_in_own_graveyard'}:raise ValueError('reviewed count basis required')
 if (' dies,' in clause)!=(profile['event']=='death'):raise ValueError('event disagreement')
 accounted=[clause]+profile['source_keywords']+profile['inactive_clauses']
 if profile['mandatory_followups']:
  follow='Landfall \u2014 Whenever a land you control enters, you may put a +1/+1 counter on each Plant creature you control.'
  if r['name']!='Avenger of Zendikar' or not variable_exact(profile['mandatory_followups'],[dict(kind='landfall_counter_all_controlled_plants',amount=1,optional=True)]):raise ValueError('reviewed active followup required')
  accounted.append(follow)
 if sorted(accounted)!=sorted(lines):raise ValueError('unaccounted Oracle clause')
 tags={'Landfall'} if profile['mandatory_followups'] else {'Undergrowth'} if r['name']=='Izoni, Thousand-Eyed' else set()
 if set(r['keywords'])!=set(profile['source_keywords'])|tags:raise ValueError('keyword metadata disagreement')
 if any(not re.fullmatch(r'\d+',r[k]) for k in ('power','toughness')):raise ValueError('numeric source body required')
 from recipes import cost_from_oracle
 return dict(copy.deepcopy(profile),_variable_component=True,family=family,source_name=r['name'],creature='Creature' in r['type_line'],mana=cost_from_oracle(r),power=int(r.get('power') or 0),toughness=int(r.get('toughness') or 0))

def variable_variants(p):
 base=['create','minimum','larger_amount','opponent_state','ownership','tokens_do_not_contribute','payment','insufficient_mana','wrong_color','countered','cleanup','fresh_attack','ready_attack','block_ground','block_flying','destroy_token','bounce_token','exile_token','amount_locked','ready_snapshot','pending_snapshot','event_snapshot']
 base+=['bounce_source','exile_source','wrath_death'] if p['event']=='death' else ['source_removed','reentry_changed_input']
 if p['basis']=='own_grave_nontoken_deaths_this_turn':base+=['prior_turn_expired','purged_history','death_via_spell']
 if p['basis']=='creature_cards_in_own_graveyard':base+=['purged_input']
 if p['mandatory_followups']:base+=['landfall_scope','landfall_scope_snapshot','landfall_opponent','landfall_source_removed','landfall_cleanup','landfall_repeat','atlas_tapped_denied']
 out=[v+'_'+str(owner) for owner in (0,1) for v in base]
 if p['mandatory_followups']:out+=['landfall_decline_0']
 return out

def validate_variable_witnesses(oracle,rows,p,data=None):
 data=data or variable_data()
 for n,pin in data['cards'].items():
  if pin.get('layout')!='normal' or pin.get('type_line','').startswith('Token '):raise ValueError('normal physical witness required; token-name collisions are identity-only: '+n)
  if not variable_exact(oracle.get(n),pin):raise ValueError('complete witness changed: '+n)
 for n,component in data['witness_token_components'].items():
  if not variable_exact(variable_token_from_creator(oracle[n]['oracle_text']),component['token']) or not variable_exact(component['count'],2 if n=='Raise the Alarm' else 1):raise ValueError('independent witness token component changed: '+n)
 for n,pin in data['registry_identity_only'].items():
  if n not in rows or rows[n]['csvid']!=pin['csvid']:raise ValueError('registry identity changed: '+n)
 # Registry token characteristics/handlers intentionally do not supply rules.
 if not oracle[p['token']['name']]['type_line'].startswith('Token '):raise ValueError('registered token identity required')


def noncreature_data():
 return json.loads((Path(__file__).parent/'data/token_trigger_creation_witnesses.json').read_text(encoding='utf8')).get('noncreature_profiles',{})

# Exact noncreature source components, including mandatory entry/grant/replacement effects.
def noncreature_params(profile):
 keys=['profile','basis','token','inactive_clauses','entry_tapped','activation','fixed_count']+[k for k in ('entry_creation','active_static','empire_replacement') if k in profile]
 return {k:copy.deepcopy(profile[k]) for k in keys}

def noncreature_spec(d,r,data=None):
 profile=(noncreature_data() if data is None else data).get(r.get('name'))
 if not profile or not variable_exact(profile['source'],r) or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or 'Creature' in r.get('type_line','') or r['type_line'].startswith('Token '):raise ValueError('pinned normal noncreature source required')
 if d.get('family')!='token_spell_extension' or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r['oracle_text'] or 'variants' not in d or not variable_exact(d.get('params'),noncreature_params(profile)):raise ValueError('complete typed noncreature component required')
 lines=r['oracle_text'].splitlines();creation=[line for line in lines if re.search(r'\b[Cc]reate ',line)];entry=profile.get('entry_creation');static=profile.get('active_static');empire=profile.get('empire_replacement');activation=profile['activation'];basis=profile['basis'];accounted=[]
 if entry is not None:
  if not variable_exact(entry,dict(count=1)) or 'Artifact' not in r['type_line']:raise ValueError('bounded operative artifact entry required')
  entry_lines=[line for line in creation if line.startswith('When this artifact enters, ')];creation=[line for line in creation if line not in entry_lines]
  if len(entry_lines)!=1:raise ValueError('one mandatory artifact entry clause required')
  parsed,mana=component_creation(entry_lines[0][len('When this artifact enters, '):]);count=parsed.pop('count')
  if mana or count!=entry['count'] or not variable_exact(parsed,profile['token']):raise ValueError('independent complete entry body required')
  accounted+=entry_lines
 if len(creation)!=1 or not variable_exact(variable_token_from_creator(creation[0]),profile['token']):raise ValueError('independent creator token body changed')
 clause=creation[0];accounted+=[clause]
 if type(profile['entry_tapped']) is not bool or profile['entry_tapped']!=bool(re.search(r'[Cc]reate (?:a|two) tapped ',clause)):raise ValueError('printed tapped entry required')
 if activation is None:
  suffixes={'creature_cards_in_own_graveyard':'for each creature card in your graveyard.','instant_sorcery_cards_in_own_graveyard':'for each instant and sorcery card in your graveyard.','controlled_creatures':'for each creature you control.'}
  if basis not in suffixes or not clause.startswith('Create ') or not clause.endswith(suffixes[basis]) or profile['fixed_count'] is not None or not any(t in r['type_line'] for t in ('Instant','Sorcery')) or any(x is not None for x in (entry,static,empire)):raise ValueError('complete spell count basis required')
 else:
  if not isinstance(activation,dict) or set(activation)!={'mana','tap','sacrifice'} or len(activation['mana'])!=6 or any(type(x) is not int or x<0 for x in activation['mana']) or any(type(activation[x]) is not bool for x in ('tap','sacrifice')):raise ValueError('typed complete activation cost required')
  cost,body=clause.split(': ',1);terms=cost.split(', ');mana=''.join(re.findall(r'\{(?:\d+|[BUGRW])\}',cost));allowed=[mana] if mana else []
  if activation['tap']:allowed+=['{T}']
  if activation['sacrifice']:allowed+=['Sacrifice this '+('artifact' if 'Artifact' in r['type_line'] else 'enchantment')]
  if terms!=allowed or activation['mana']!=variable_cost(dict(mana_cost=mana)) or basis!='fixed' or type(profile['fixed_count']) is not int or profile['fixed_count'] not in (1,2):raise ValueError('printed activation cost and fixed count required')
  if empire is not None:
   if not variable_exact(empire,dict(required_artifacts=['Crown of Empires','Scepter of Empires'],count=5)) or profile['fixed_count']!=1:raise ValueError('whole mandatory empire replacement required')
   suffix=' Create five of those tokens instead if you control artifacts named Crown of Empires and Scepter of Empires.'
   if not body.endswith(suffix):raise ValueError('printed mandatory empire control condition required')
   body=body[:-len(suffix)]
  if r['name']=='Dragon Roost':body=body.removesuffix(" (It can't be blocked except by creatures with flying or reach.)")
  parsed,mana_ability=component_creation(body);number=parsed.pop('count')
  if mana_ability or number!=profile['fixed_count'] or not variable_exact(parsed,profile['token']):raise ValueError('whole operative activation effect required')
  if static is not None:
   if not variable_exact(static,dict(subtype='Soldier',keyword='Vigilance',controller='all')):raise ValueError('whole global Soldier vigilance grant required')
   accounted+=['Soldier creatures have vigilance.']
 if sorted(accounted+profile['inactive_clauses'])!=sorted(lines) or r['keywords']!=profile['keyword_metadata']:raise ValueError('unaccounted noncreature source text')
 from recipes import cost_from_oracle
 return dict(copy.deepcopy(profile),_noncreature_component=True,family='token_spell_extension',source_name=r['name'],creature=False,mana=cost_from_oracle(r),power=0,toughness=0)

def noncreature_variants(p):
 common=['create','payment','insufficient_mana','countered','cleanup','fresh_attack','ready_attack','block_ground','block_flying','destroy_token','bounce_token','exile_token','ready_snapshot','pending_snapshot','effect_snapshot']
 if any(p['mana'][i] for i in range(1,6)):common+=['wrong_color']
 if p['activation'] is None:common+=['minimum','larger_amount','opponent_state','ownership','token_input','purged_input','amount_locked','two_casts','response_count','response_snapshot']
 else:
  common+=['cast_only','activation_payment','repeat','activation_snapshot']
  # Keep the reviewed ordering identical for the previous six profiles.
  if sum(p['activation']['mana']):common.insert(common.index('repeat'),'activation_insufficient')
  if not p['activation']['sacrifice']:common[common.index('activation_snapshot'):common.index('activation_snapshot')]=['source_removed','source_removed_pending']
  if any(p['activation']['mana'][i] for i in range(1,6)):common+=['activation_wrong_color']
  if p['activation']['tap'] and not p['activation']['sacrifice']:common+=['tapped_denied','untap_repeat','untap_snapshot']
 if p['entry_tapped']:common+=['untap_block_ground','untap_block_flying','untap_snapshot']
 if 'Flying' in p['token']['keywords']:common+=['flying_evasion']
 if p.get('active_static'):common+=['grant_scope','grant_lost']
 if p.get('empire_replacement'):common+=['empire_pair','empire_crown_only','empire_scepter_only','empire_opponent','empire_foreign_controlled','empire_owned_opposing','empire_one_lost','empire_lost_pending','empire_pair_cleanup','empire_condition_snapshot']
 return [v+'_'+str(owner) for owner in (0,1) for v in common]


WATCH_PREFIXES={
 'owner_grave_nontoken_creature':'Whenever a nontoken creature is put into your graveyard from the battlefield, ',
 'controller_nontoken_creature':'Whenever a nontoken creature you control dies, ',
 'another_controller_nonspirit_creature':'Whenever another non-Spirit creature you control dies, ',
 'self_or_controller_nontoken_creature':'Whenever this creature or another nontoken creature you control dies, ',
 'another_controller_nontoken_creature':'Whenever another nontoken creature you control dies, ',
 'self_or_global_cleric':'Whenever this creature or another Cleric dies, ',
 'another_owner_grave_nontoken_elf':'Whenever another nontoken Elf is put into your graveyard from the battlefield, ',
}

def death_watch_data():
 return json.loads((Path(__file__).parent/'data/token_trigger_creation_witnesses.json').read_text(encoding='utf8')).get('death_watch_profiles',{})

def death_watch_spec(d,r,data=None):
 profile=(death_watch_data() if data is None else data).get(r.get('name'));creature='Creature' in r.get('type_line','');family='token_trigger_creation' if creature else 'token_spell_extension'
 if not profile or not variable_exact(profile['source'],r) or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r['type_line'].startswith('Token '):raise ValueError('whole pinned normal death watcher required')
 keys=('profile','token','predicate','optional','token_mana','source_keywords')
 if d.get('family')!=family or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r['oracle_text'] or 'variants' not in d or not variable_exact(d.get('params'),{k:profile[k] for k in keys}):raise ValueError('complete typed death watch declaration required')
 if profile['predicate'] not in WATCH_PREFIXES or any(type(profile[k]) is not bool for k in ('optional','token_mana')):raise ValueError('reviewed typed death predicate required')
 lines=r['oracle_text'].splitlines();creation=[line for line in lines if re.search(r'\bcreate ',line)]
 if len(creation)!=1 or not variable_exact(variable_token_from_creator(creation[0]),profile['token']):raise ValueError('independent creator body required')
 clause=creation[0];prefix=WATCH_PREFIXES[profile['predicate']]+('you may ' if profile['optional'] else '')+'create a '
 if not clause.startswith(prefix):raise ValueError('whole death predicate and optionality disagree')
 body=clause[len(WATCH_PREFIXES[profile['predicate']])+len('you may ' if profile['optional'] else ''):];parsed,printed_mana=component_creation(body);number=parsed.pop('count')
 if number!=1 or not variable_exact(parsed,profile['token']) or printed_mana!=profile['token_mana']:raise ValueError('complete death effect required; additional operative text rejected')
 mana='It has "Sacrifice this token: Add {C}."'
 if profile['token_mana']!=(mana in clause):raise ValueError('printed token mana ability disagrees')
 if sorted([clause]+profile['source_keywords'])!=sorted(lines) or r['keywords']!=profile['keyword_metadata']:raise ValueError('unaccounted operative source clause')
 if creature and any(not re.fullmatch(r'\d+',r[k]) for k in ('power','toughness')):raise ValueError('numeric source body required')
 from recipes import cost_from_oracle
 return dict(copy.deepcopy(profile),_death_watch_component=True,family=family,source_name=r['name'],creature=creature,mana=cost_from_oracle(r),power=int(r.get('power') or 0),toughness=int(r.get('toughness') or 0),event='watch')

def death_watch_variants(p):
 common=['create','cast_only','payment','insufficient_mana','wrong_color','countered','opponent_death','foreign_controlled','owned_opposing','noncreature_death','prior_death','repeat','murder','bounce_only','exile_only','wrath_death','source_removed','source_bounced','source_exiled','cleanup','fresh_attack','ready_attack','block_ground','block_flying','destroy_token','bounce_token','exile_token','ready_snapshot','pending_snapshot','event_snapshot','wrath_snapshot']
 if p['creature']:common+=['source_death']
 if p['token_mana']:common+=['token_mana','token_mana_snapshot']
 if p['predicate']=='another_controller_nonspirit_creature':common+=['independent_token_death','spirit_token_negative']
 if p['predicate']=='another_owner_grave_nontoken_elf':common+=['kindred_elf_death']
 if p['predicate'] in ('another_owner_grave_nontoken_elf','self_or_global_cleric','another_controller_nonspirit_creature'):common+=['other_type']
 if 'Flying' in p['source_keywords']:common+=['source_keyword_combat']
 out=[v+'_'+str(owner) for owner in (0,1) for v in common]
 if p['optional']:out+=['decline_0']
 return out


# Exact Inspired source pins and main-phase untap effect components.
def inspired_data():
 return json.loads((Path(__file__).parent/'data/token_trigger_creation_witnesses.json').read_text(encoding='utf8')).get('inspired_profiles',{})

def inspired_spec(d,r,data=None):
 profile=(inspired_data() if data is None else data).get(r.get('name'))
 if not profile or not variable_exact(profile['source'],r) or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or 'Creature' not in r.get('type_line','') or r['type_line'].startswith('Token '):raise ValueError('whole pinned normal inspired creature required')
 keys=('profile','token','count','payment','source_keywords')
 if d.get('family')!='token_trigger_creation' or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r['oracle_text'] or 'variants' not in d or not variable_exact(d.get('params'),{k:profile[k] for k in keys}):raise ValueError('complete typed inspired declaration required')
 lines=r['oracle_text'].splitlines();effects=[line for line in lines if line.startswith('Inspired — ')]
 if len(effects)!=1:raise ValueError('one whole inspired event required')
 m=re.fullmatch(r'Inspired — Whenever this creature becomes untapped, you may pay ((?:\{(?:\d+|[BUGRW])\})+)\. If you do, (create .+)',effects[0])
 if not m or variable_cost(dict(mana_cost=m[1]))!=profile['payment'] or any(type(v) is not int for v in profile['payment']):raise ValueError('printed complete inspired payment required')
 if m[2].count(' enchantment creature ')!=1 or profile['token'].get('enchantment') is not True or type(profile['token']['enchantment']) is not bool:raise ValueError('mandatory enchantment creature token type required')
 body=m[2].replace(' enchantment creature ',' creature ');token,mana=component_creation(body);count=token.pop('count');expected=dict(profile['token']);expected.pop('enchantment')
 if mana or type(profile['count']) is not int or count!=profile['count'] or not variable_exact(token,expected):raise ValueError('whole independent inspired token effect required')
 if sorted([effects[0]]+profile['source_keywords'])!=sorted(lines) or set(r['keywords'])!=set(profile['keyword_metadata']) or set(r['keywords'])!={'Inspired'}|{x.split(' (',1)[0] for x in profile['source_keywords']}:raise ValueError('unaccounted operative source rules')
 if any(not re.fullmatch(r'\d+',r[k]) for k in ('power','toughness')):raise ValueError('numeric source body required')
 from recipes import cost_from_oracle
 return dict(copy.deepcopy(profile),_inspired_component=True,family='token_trigger_creation',source_name=r['name'],creature=True,mana=cost_from_oracle(r),power=int(r['power']),toughness=int(r['toughness']),event='inspired')

def inspired_variants(p):
 base=['create','cast_only','source_payment','source_insufficient','source_wrong_color','countered','payment','payment_insufficient','payment_wrong_color','already_untapped','other_untaps','opponent_untap','untap_timing','repeat','source_destroyed','source_bounced','source_exiled','cleanup','fresh_attack','ready_attack','block_ground','block_flying','destroy_token','bounce_token','exile_token','enchantment_type','ready_snapshot','pending_snapshot','effect_snapshot','repeat_snapshot']
 if 'Flying' in p['token']['keywords']:base+=['flying_evasion']
 if 'Haste' in p['source_keywords']:base+=['source_haste']
 if any(k.startswith('Intimidate') for k in p['source_keywords']):base+=['source_intimidate_ground','source_intimidate_artifact','source_intimidate_black']
 return [v+'_'+str(owner) for owner in (0,1) for v in base]+['decline_0','decline_1']

# Wave 6 is opt-in: its private palette never changes older expanded bodies.
def wave6_data():
 return json.loads((Path(__file__).parent/'data/token_creator_wave6_witnesses.json').read_text(encoding='utf8'))

WAVE6_RULES={
 'Increasing Devotion':('flashback_create','Create five 1/1 white Human creature tokens. If this spell was cast from a graveyard, create ten of those tokens instead.\nFlashback {7}{W}{W} (You may cast this card from your graveyard for its flashback cost. Then exile it.)'),
 'Secure the Wastes':('x_create','Create X 1/1 white Warrior creature tokens.'),
 'Martial Coup':('x_wrath','Create X 1/1 white Soldier creature tokens. If X is 5 or more, destroy all other creatures.'),
 'Fresh Meat':('death_history','Create a 3/3 green Beast creature token for each creature put into your graveyard from the battlefield this turn.'),
 'Myrsmith':('artifact_cast','Whenever you cast an artifact spell, you may pay {1}. If you do, create a 1/1 colorless Myr artifact creature token.'),
 'Emmara, Soul of the Accord':('tap_trigger','Whenever Emmara becomes tapped, create a 1/1 white Soldier creature token with lifelink.'),
 'Vitu-Ghazi, the City-Tree':('land_activation','{T}: Add {C}.\n{2}{G}{W}, {T}: Create a 1/1 green Saproling creature token.'),
 'Master Trinketeer':('servo_lord','Servos and Thopters you control get +1/+1.\n{3}{W}: Create a 1/1 colorless Servo artifact creature token.'),
}

def wave6_cost(record,x=0):
 text=record.get('mana_cost','');symbols=re.findall(r'\{(\d+|X|[BUGRW](?:/[BUGRW])?)\}',text)
 if type(x) is not int or x<0 or ''.join('{'+s+'}' for s in symbols)!=text:raise ValueError('reviewed typed X mana cost required')
 out=[0]*6
 for s in symbols:out[0 if s.isdigit() or s=='X' else VARIABLE_COLOR_INDEX[s.split('/')[0]]]+=x if s=='X' else int(s) if s.isdigit() else 1
 return out

def wave6_params(record):
 if record['name'] in WAVE6_DAMAGE_RULES:return wave6_damage_params(record)
 if record['name'] in WAVE6_WOLF_RULES:return wave6_wolf_params(record)
 if record['name'] in WAVE6_ACTIVATION_RULES:return wave6_activation_params(record)
 if record['name'] in WAVE6_BREADTH_RULES:return wave6_breadth_params(record)
 if record['name'] in WAVE6_TRIGGER_RULES:return wave6_trigger_params(record)
 name=record['name'];kind,text=WAVE6_RULES.get(name,(None,None))
 if record['oracle_text']!=text:raise ValueError('whole reviewed Wave6 operative text required')
 # Characteristics are parsed from the creator, never the registered token row.
 body=text.replace('Create five ','Create a ')
 token=variable_token_from_creator(body)
 amount={'flashback_create':dict(normal=5,graveyard=10),'x_create':dict(basis='chosen_x'),'x_wrath':dict(basis='chosen_x',threshold=5),'death_history':dict(basis='all_owned_grave_creature_deaths_current_turn')}.get(kind,dict(fixed=1))
 activation=None;operative=[]
 if kind=='land_activation':activation=dict(mana=[2,0,0,1,0,1],tap=True,sacrifice=False);operative=[dict(kind='tap_add_colorless',amount=1)]
 if kind=='servo_lord':activation=dict(mana=[3,0,0,0,0,1],tap=False,sacrifice=False);operative=[dict(kind='own_servo_thopter_static',power=1,toughness=1)]
 if kind=='artifact_cast':operative=[dict(kind='optional_resolution_payment',mana=[1,0,0,0,0,0])]
 return dict(profile='creator_wave6',schema=1,mechanism=kind,token=token,amount=amount,activation=activation,operative=operative,source_keywords=[],inactive_clauses=[])

def wave6_spec(d,r,family):
 if not isinstance(d,dict) or not isinstance(r,dict):raise ValueError('typed Wave6 declaration and Oracle record required')
 chosen=d.get('variants')
 if type(d.get('version')) is not int or d['version']!=2 or not isinstance(chosen,list) or not chosen or any(type(v) is not str for v in chosen) or len(set(chosen))!=len(chosen):raise ValueError('version2 and explicit unique string variants required')
 data=wave6_data();profile=data['profiles'].get(r.get('name'))
 if not profile or not variable_exact(profile['source'],r) or r.get('layout')!='normal' or r.get('card_faces') or r['type_line'].startswith('Token '):raise ValueError('full pinned normal Wave6 source required')
 expected=wave6_params(r);wanted='token_trigger_creation' if 'Creature' in r['type_line'] else 'token_spell_extension'
 if family!=wanted or d.get('family')!=family or d.get('version')!=2 or d.get('oracle_text')!=r['oracle_text'] or 'variants' not in d or not variable_exact(d.get('params'),expected):raise ValueError('complete typed version2 Wave6 declaration required')
 tags=set(expected['source_keywords'])|({'Flashback'} if expected['mechanism']=='flashback_create' else set())|{'Miracle' for line in expected['inactive_clauses'] if line.startswith('Miracle ')}|{'Cycling' for line in expected['inactive_clauses'] if line.startswith('Cycling ')}
 tags|={'Partner' for line in expected['inactive_clauses'] if line.startswith('Partner ')}
 if set(r['keywords'])!=tags:raise ValueError('unaccounted Wave6 source keyword')
 return dict(copy.deepcopy(expected),_wave6_component=True,source_name=r['name'],family=family,creature='Creature' in r['type_line'],mana=wave6_cost(r),power=int(r.get('power') or 0),toughness=int(r.get('toughness') or 0))

def wave6_base_variants(p):
 if p['mechanism']=='damage_creator':return wave6_damage_variants(p)
 if p['mechanism']=='wolf_creator':return wave6_wolf_variants(p)
 if p['mechanism'] in ('land_creator','cost_creator'):return wave6_activation_variants(p)
 if p['mechanism'] in ('spell_count','spell_x','fixed_followup'):return wave6_breadth_variants(p)
 if p['mechanism'] in ('cast_predicate','tap_optional'):return wave6_trigger_variants(p)
 kind=p['mechanism'];modes=['create','payment','insufficient_mana','ready_snapshot','effect_snapshot','cleanup','destroy_token','bounce_token','exile_token','fresh_attack','ready_attack','block_ground','block_flying']
 if kind in ('x_create','x_wrath'):modes+=['zero','larger_x','pending_snapshot']
 if kind=='x_wrath':modes+=['below_threshold','threshold_ownership']
 if kind=='flashback_create':modes+=['flashback','flashback_payment','flashback_denied','flashback_snapshot']
 if kind=='death_history':modes+=['zero','token_deaths','ownership','opponent_deaths','prior_turn_expired','purged_history','pending_snapshot']
 if kind=='artifact_cast':modes+=['nonartifact','opponent_cast','no_payment','source_removed','repeat','pending_snapshot']
 if kind=='tap_trigger':modes+=['untap_only','other_tapped','source_attack','source_removed','repeat','lifelink']
 if kind=='land_activation':modes+=['land_entry','mana_ability','tapped_denied','source_removed','repeat','activation_snapshot']
 if kind=='servo_lord':modes+=['static_scope','static_removed','source_removed','repeat','activation_snapshot']
 if kind=='land_activation':modes=[m for m in modes if m!='insufficient_mana']
 if kind in ('artifact_cast','tap_trigger'):modes=[m for m in modes if m not in ('payment','insufficient_mana')]
 return [m+'_'+str(owner) for owner in (0,1) for m in modes]

def validate_wave6_witnesses(oracle,rows,p):
 data=wave6_data()
 for name,pin in data['cards'].items():
  if pin.get('layout')!='normal' or pin['type_line'].startswith('Token ') or not variable_exact(oracle.get(name),pin):raise ValueError('complete normal Wave6 witness changed: '+name)
 for name,pin in data['registry_identity_only'].items():
  if name not in rows or rows[name]['csvid']!=pin['csvid']:raise ValueError('Wave6 registry identity changed: '+name)
 for name,c in data['witness_token_components'].items():
  body=oracle[name]['oracle_text'].replace('Create three ','Create a ')
  if not variable_exact(variable_token_from_creator(body),c['token']):raise ValueError('Wave6 independent witness creator changed: '+name)
  number=re.search(r'[Cc]reate (a|two|three) ',oracle[name]['oracle_text'])
  if not number or type(c['count']) is not int or c['count']!={'a':1,'two':2,'three':3}[number[1]]:raise ValueError('Wave6 independent witness amount required')
 token_name=p['token']['name'];identity_only={('Nested Ghoul','Phyrexian Zombie'):22242}
 if (p['source_name'],token_name) in identity_only:
  if data['registry_identity_only'].get(token_name)!={'csvid':identity_only[(p['source_name'],token_name)]}:raise ValueError('exact reviewed creator/token engine identity required')
 elif not oracle.get(token_name,{}).get('type_line','').startswith('Token '):raise ValueError('Wave6 token identity required')

# Wave6 breadth specifications, isolated from earlier profile palettes.
WAVE6_BREADTH_RULES = {'Elvish Promenade': {'text': 'Create a 1/1 green Elf Warrior creature token for each Elf you control.',
                      'mechanism': 'spell_count',
                      'config': {'basis': 'controlled_elf_permanents'}},
 'Deploy to the Front': {'text': 'Create X 1/1 white Soldier creature tokens, where X is the number of '
                                 'creatures on the battlefield.',
                         'mechanism': 'spell_count',
                         'config': {'basis': 'all_creatures'}},
 'Fungal Sprouting': {'text': 'Create X 1/1 green Saproling creature tokens, where X is the greatest power '
                              'among creatures you control.',
                      'mechanism': 'spell_count',
                      'config': {'basis': 'greatest_controlled_creature_power'}},
 'Beacon of Creation': {'text': 'Create a 1/1 green Insect creature token for each Forest you control. '
                                "Shuffle Beacon of Creation into its owner's library.",
                        'mechanism': 'spell_count',
                        'config': {'basis': 'controlled_forests', 'destination': 'shuffle_owner_library'}},
 'Goblin Offensive': {'text': 'Create X 1/1 red Goblin creature tokens.',
                      'mechanism': 'spell_x',
                      'config': {'amount': 'x'}},
 'Entreat the Angels': {'text': 'Create X 4/4 white Angel creature tokens with flying.\n'
                                'Miracle {X}{W}{W} (You may cast this card for its miracle cost when you '
                                "draw it if it's the first card you drew this turn.)",
                        'mechanism': 'spell_x',
                        'config': {'amount': 'x', 'inactive_prefixes': ['Miracle ']}},
 'Gelatinous Genesis': {'text': 'Create X X/X green Ooze creature tokens.',
                        'mechanism': 'spell_x',
                        'config': {'amount': 'x', 'body': 'x'}},
 'Slime Molding': {'text': 'Create an X/X green Ooze creature token.',
                   'mechanism': 'spell_x',
                   'config': {'amount': 1, 'body': 'x'}},
 "White Sun's Zenith": {'text': "Create X 2/2 white Cat creature tokens. Shuffle White Sun's Zenith into its "
                                "owner's library.",
                        'mechanism': 'spell_x',
                        'config': {'amount': 'x', 'destination': 'shuffle_owner_library'}},
 'Decree of Justice': {'text': 'Create X 4/4 white Angel creature tokens with flying.\n'
                               'Cycling {2}{W} ({2}{W}, Discard this card: Draw a card.)\n'
                               'When you cycle this card, you may pay {X}. If you do, create X 1/1 white '
                               'Soldier creature tokens.',
                       'mechanism': 'spell_x',
                       'config': {'amount': 'x', 'inactive_prefixes': ['Cycling ', 'When you cycle ']}},
 'Skittering Invasion': {'text': 'Create five 0/1 colorless Eldrazi Spawn creature tokens. They have '
                                 '"Sacrifice this token: Add {C}."',
                         'mechanism': 'fixed_followup',
                         'config': {'amount': 5, 'token_sac_mana': True}},
 'Feral Lightning': {'text': 'Create three 3/1 red Elemental creature tokens with haste. Exile them at the '
                             'beginning of the next end step.',
                     'mechanism': 'fixed_followup',
                     'config': {'amount': 3, 'delayed': 'exile_next_end_step'}},
 'Thatcher Revolt': {'text': 'Create three 1/1 red Human creature tokens with haste. Sacrifice those tokens '
                             'at the beginning of the next end step.',
                     'mechanism': 'fixed_followup',
                     'config': {'amount': 3, 'delayed': 'sacrifice_next_end_step'}}}

def wave6_breadth_params(record):
 entry=WAVE6_BREADTH_RULES[record['name']];text=entry['text'];config=entry['config'];mechanism=entry['mechanism']
 if record['oracle_text']!=text:raise ValueError('whole reviewed Wave6 breadth text required')
 inactive=[line for line in text.splitlines() if any(line.startswith(prefix) for prefix in config.get('inactive_prefixes',[]))]
 # X body is normalized only for the independent token-characteristic parser;
 # the actual X and amount are carried separately and checked against full text.
 body=text.splitlines()[0]
 for word in ('five','three'):body=body.replace('Create '+word+' ','Create a ')
 body=body.replace('X/X','0/1').replace('an 0/1','a 0/1')
 # Ooze is an existing observer; do not extend the older parser's palette.
 match=re.search(r'[Cc]reate (?:a|an|X) (\d+)/(\d+) (white|blue|black|red|green|colorless)(?: and (white|blue|black|red|green))? ([A-Za-z ]+?) (artifact )?creature tokens?(?: with ([a-z ,]+?))?(?=\.|, where| for each|$)',body)
 if not match:raise ValueError('reviewed breadth creator required')
 power,toughness,c1,c2,types,artifact,keywords=match.groups();subtypes=types.split();keys=[k.capitalize() for k in re.split(r', | and ',keywords)] if keywords else []
 if any(t not in SUBTYPES|{'Plant','Ooze'} for t in subtypes) or any(k not in TOKEN_KEYWORDS for k in keys):raise ValueError('supported breadth token observer required')
 token=dict(name=' '.join(subtypes),power=0 if config.get('body')=='x' else int(power),toughness=0 if config.get('body')=='x' else int(toughness),colors=[VARIABLE_COLOR_WORD[c] for c in (c1,c2) if c and c!='colorless'],subtypes=subtypes,artifact=bool(artifact),keywords=keys)
 amount=dict(basis=config['basis']) if mechanism=='spell_count' else dict(basis='chosen_x',body_x=config.get('body')=='x',fixed_count=config.get('amount') if type(config.get('amount')) is int else None) if mechanism=='spell_x' else dict(fixed=config['amount'])
 operative=[]
 if 'destination' in config:operative.append(dict(kind=config['destination']))
 if 'delayed' in config:operative.append(dict(kind=config['delayed']))
 if config.get('token_sac_mana'):operative.append(dict(kind='token_sacrifice_colorless',amount=1))
 return copy.deepcopy(dict(profile='creator_wave6',schema=2,mechanism=mechanism,token=token,amount=amount,activation=None,operative=operative,source_keywords=[],inactive_clauses=inactive))


def wave6_breadth_variants(p):
 modes=['create','zero','larger_amount','payment','insufficient_mana','wrong_color','ready_snapshot','pending_snapshot','effect_snapshot','cleanup','fresh_attack','ready_attack','block_ground','block_flying','destroy_token','bounce_token','exile_token']
 if p['mechanism']=='fixed_followup':modes=[m for m in modes if m not in ('zero','larger_amount')]
 if not any(p['mana'][1:]):modes=[m for m in modes if m!='wrong_color']
 if p['mechanism']=='spell_count':modes+=['opponent_state','ownership','amount_locked']
 if p['amount'].get('basis')=='all_creatures':modes+=['preexisting_tokens']
 if p['amount'].get('basis')=='greatest_controlled_creature_power':modes+=['real_power_boost']
 if p['amount'].get('basis')=='controlled_elf_permanents':modes+=['token_input']
 if 'Flying' in p['token']['keywords']:modes+=['flying_evasion']
 if any(c['kind']=='token_sacrifice_colorless' for c in p['operative']):modes+=['token_mana','token_mana_all','token_mana_snapshot']
 if any(c['kind'].endswith('next_end_step') for c in p['operative']):modes=[m for m in modes if m not in ('ready_attack','block_ground','block_flying')]+['expiry_snapshot']
 return [v+'_'+str(owner) for owner in (0,1) for v in modes]

# Wave6 cast/tap trigger specifications.
WAVE6_TRIGGER_RULES = {'Goblinslide': {'text': 'Whenever you cast a noncreature spell, you may pay {1}. If you do, create a 1/1 '
                         'red Goblin creature token with haste.',
                 'mechanism': 'cast_predicate',
                 'config': {'predicate': 'noncreature', 'optional': True, 'payment': [1, 0, 0, 0, 0, 0]}},
 'Lys Alana Huntmaster': {'text': 'Whenever you cast an Elf spell, you may create a 1/1 green Elf Warrior '
                                  'creature token.',
                          'mechanism': 'cast_predicate',
                          'config': {'predicate': 'elf', 'optional': True, 'payment': None}},
 'Oyobi, Who Split the Heavens': {'text': 'Flying\n'
                                          'Whenever you cast a Spirit or Arcane spell, create a 3/3 white '
                                          'Spirit creature token with flying.',
                                  'mechanism': 'cast_predicate',
                                  'config': {'predicate': 'spirit_or_arcane',
                                             'optional': False,
                                             'payment': None,
                                             'source_keywords': ['Flying']}},
 'Sigil of the Empty Throne': {'text': 'Whenever you cast an enchantment spell, create a 4/4 white Angel '
                                       'creature token with flying.',
                               'mechanism': 'cast_predicate',
                               'config': {'predicate': 'enchantment', 'optional': False, 'payment': None}},
 'Efficient Construction': {'text': 'Whenever you cast an artifact spell, create a 1/1 colorless Thopter '
                                    'artifact creature token with flying.',
                            'mechanism': 'cast_predicate',
                            'config': {'predicate': 'artifact', 'optional': False, 'payment': None}},
 'Sai, Master Thopterist': {'text': 'Whenever you cast an artifact spell, create a 1/1 colorless Thopter '
                                    'artifact creature token with flying.\n'
                                    '{1}{U}, Sacrifice two artifacts: Draw a card.',
                            'mechanism': 'cast_predicate',
                            'config': {'predicate': 'artifact',
                                       'optional': False,
                                       'payment': None,
                                       'inactive_prefixes': ['{1}{U}, Sacrifice two artifacts:']}},
 'Hero of Precinct One': {'text': 'Whenever you cast a multicolored spell, create a 1/1 white Human creature '
                                  'token.',
                          'mechanism': 'cast_predicate',
                          'config': {'predicate': 'multicolor', 'optional': False, 'payment': None}},
 'Stonybrook Schoolmaster': {'text': 'Whenever this creature becomes tapped, you may create a 1/1 blue '
                                     'Merfolk Wizard creature token.',
                             'mechanism': 'tap_optional',
                             'config': {'optional': True}}}

def wave6_trigger_params(record):
 entry=WAVE6_TRIGGER_RULES[record['name']];text=entry['text'];c=entry['config']
 if record['oracle_text']!=text:raise ValueError('whole reviewed Wave6 trigger text required')
 token=variable_token_from_creator(text)
 inactive=[line for line in text.splitlines() if any(line.startswith(prefix) for prefix in c.get('inactive_prefixes',[]))]
 operative=[dict(kind='cast_predicate',predicate=c['predicate'],optional=c['optional'],payment=c['payment'])] if entry['mechanism']=='cast_predicate' else [dict(kind='optional_tap',optional=True)]
 return copy.deepcopy(dict(profile='creator_wave6',schema=2,mechanism=entry['mechanism'],token=token,amount=dict(fixed=1),activation=None,operative=operative,source_keywords=c.get('source_keywords',[]),inactive_clauses=inactive))

def wave6_trigger_variants(p):
 modes=['create','nonmatching','opponent_event','source_removed','repeat','ready_snapshot','event_snapshot','cleanup','fresh_attack','ready_attack','block_ground','block_flying','destroy_token','bounce_token','exile_token']
 if p['mechanism']=='cast_predicate':
  modes+=['pending_snapshot']
  if p['operative'][0]['predicate']!='enchantment':modes+=['predicate_second_type']
  if p['operative'][0]['payment'] is not None:modes+=['no_payment','exact_payment']
 else:modes+=['untap_only','source_attack']
 if 'Flying' in p['token']['keywords']:modes+=['flying_evasion']
 if 'Flying' in p['source_keywords']:modes+=['source_flying_evasion']
 out=[m+'_'+str(owner) for owner in (0,1) for m in modes]
 if p['operative'][0]['optional']:out+=['decline_0']
 return out

# Wave6 land and activation components.
WAVE6_ACTIVATION_RULES = {'Khalni Garden': {'text': 'This land enters tapped.\n'
                           'When this land enters, create a 0/1 green Plant creature token.\n'
                           '{T}: Add {G}.',
                   'config': {'kind': 'land_creator',
                              'entry_tapped': True,
                              'mana_color': 'G',
                              'entry_count': 1,
                              'cost': None,
                              'count': 1}},
 'Foundry of the Consuls': {'text': '{T}: Add {C}.\n'
                                    '{5}, {T}, Sacrifice this land: Create two 1/1 colorless Thopter artifact '
                                    'creature tokens with flying.',
                            'config': {'kind': 'land_creator',
                                       'mana_color': 'C',
                                       'cost': [5, 0, 0, 0, 0, 0],
                                       'tap': True,
                                       'sacrifice': True,
                                       'count': 2}},
 'Spawning Bed': {'text': '{T}: Add {C}.\n'
                          '{6}, {T}, Sacrifice this land: Create three 1/1 colorless Eldrazi Scion creature '
                          'tokens. They have "Sacrifice this token: Add {C}."',
                  'config': {'kind': 'land_creator',
                             'mana_color': 'C',
                             'cost': [6, 0, 0, 0, 0, 0],
                             'tap': True,
                             'sacrifice': True,
                             'count': 3,
                             'token_mana': True}},
 'Memorial to Glory': {'text': 'This land enters tapped.\n'
                               '{T}: Add {W}.\n'
                               '{3}{W}, {T}, Sacrifice this land: Create two 1/1 white Soldier creature tokens.',
                       'config': {'kind': 'land_creator',
                                  'entry_tapped': True,
                                  'mana_color': 'W',
                                  'cost': [3, 0, 0, 0, 0, 1],
                                  'tap': True,
                                  'sacrifice': True,
                                  'count': 2}},
 'Cradle of the Accursed': {'text': '{T}: Add {C}.\n'
                                    '{3}, {T}, Sacrifice this land: Create a 2/2 black Zombie creature token. '
                                    'Activate only as a sorcery.',
                            'config': {'kind': 'land_creator',
                                       'mana_color': 'C',
                                       'cost': [3, 0, 0, 0, 0, 0],
                                       'tap': True,
                                       'sacrifice': True,
                                       'count': 1,
                                       'only_sorcery': True}},
 'Patrol Signaler': {'text': '{1}{W}, {Q}: Create a 1/1 white Kithkin Soldier creature token. ({Q} is the untap '
                             'symbol.)',
                     'config': {'kind': 'cost_creator', 'cost': [1, 0, 0, 0, 0, 1], 'untap': True, 'count': 1}},
 'Selesnya Evangel': {'text': '{1}, {T}, Tap an untapped creature you control: Create a 1/1 green Saproling '
                              'creature token.',
                      'config': {'kind': 'cost_creator',
                                 'cost': [1, 0, 0, 0, 0, 0],
                                 'tap': True,
                                 'cost_kind': 'tap_other_creature',
                                 'cost_count': 1,
                                 'count': 1}},
 'Null Caller': {'text': '{3}{B}, Exile a creature card from your graveyard: Create a tapped 2/2 black Zombie '
                         'creature token.',
                 'config': {'kind': 'cost_creator',
                            'cost': [3, 1, 0, 0, 0, 0],
                            'cost_kind': 'exile_own_grave_creature',
                            'cost_count': 1,
                            'count': 1,
                            'token_tapped': True}},
 'Renowned Weaver': {'text': '{1}{G}, Sacrifice this creature: Create a 1/3 green Spider enchantment creature '
                             'token with reach. (It can block creatures with flying.)',
                     'config': {'kind': 'cost_creator',
                                'cost': [1, 0, 0, 1, 0, 0],
                                'sacrifice': True,
                                'count': 1,
                                'token_enchantment': True}},
 'Goblin Trenches': {'text': '{2}, Sacrifice a land: Create two 1/1 red and white Goblin Soldier creature tokens.',
                     'config': {'kind': 'cost_creator',
                                'cost': [2, 0, 0, 0, 0, 0],
                                'cost_kind': 'sacrifice_land',
                                'cost_count': 1,
                                'count': 2}},
 'Goblin Warrens': {'text': '{2}{R}, Sacrifice two Goblins: Create three 1/1 red Goblin creature tokens.',
                    'config': {'kind': 'cost_creator',
                               'cost': [2, 0, 0, 0, 1, 0],
                               'cost_kind': 'sacrifice_goblin',
                               'cost_count': 2,
                               'count': 3}},
 'Voice of the Woods': {'text': 'Tap five untapped Elves you control: Create a 7/7 green Elemental creature token '
                                'with trample. (It can deal excess combat damage to the player or planeswalker '
                                "it's attacking.)",
                        'config': {'kind': 'cost_creator',
                                   'cost': [0, 0, 0, 0, 0, 0],
                                   'cost_kind': 'tap_elf',
                                   'cost_count': 5,
                                   'count': 1}},
 'Slimefoot, the Stowaway': {'text': 'Whenever a Saproling you control dies, Slimefoot deals 1 damage to each '
                                     'opponent and you gain 1 life.\n'
                                     '{4}: Create a 1/1 green Saproling creature token.',
                             'config': {'kind': 'cost_creator',
                                        'cost': [4, 0, 0, 0, 0, 0],
                                        'count': 1,
                                        'followup': 'controlled_saproling_death_drain'}},
 'Dawn of Hope': {'text': 'Whenever you gain life, you may pay {2}. If you do, draw a card.\n'
                          '{3}{W}: Create a 1/1 white Soldier creature token with lifelink.',
                  'config': {'kind': 'cost_creator',
                             'cost': [3, 0, 0, 0, 0, 1],
                             'count': 1,
                             'followup': 'own_lifegain_optional_pay2_draw1'}},
 'Drogskol Cavalry': {'text': 'Flying\n'
                              'Whenever another Spirit you control enters, you gain 2 life.\n'
                              '{3}{W}: Create a 1/1 white Spirit creature token with flying.',
                      'config': {'kind': 'cost_creator',
                                 'cost': [3, 0, 0, 0, 0, 1],
                                 'count': 1,
                                 'followup': 'another_controlled_spirit_entry_gain2',
                                 'source_keywords': ['Flying']}}}

def wave6_activation_params(record):
 entry=WAVE6_ACTIVATION_RULES[record['name']];text=entry['text'];c=entry['config']
 if record['oracle_text']!=text:raise ValueError('whole reviewed activation text required')
 body=text.replace('enchantment creature','creature')
 for word in ('three','five'):body=body.replace('Create '+word+' ','Create a ')
 token=variable_token_from_creator(body)
 activation=None if c['cost'] is None else dict(mana=c['cost'],tap=c.get('tap',False),untap=c.get('untap',False),sacrifice=c.get('sacrifice',False),additional=c.get('cost_kind'),additional_count=c.get('cost_count',0),only_sorcery=c.get('only_sorcery',False))
 operative=[]
 if c.get('entry_tapped'):operative.append(dict(kind='source_enters_tapped'))
 if c.get('entry_count'):operative.append(dict(kind='entry_creation',count=c['entry_count']))
 if c.get('mana_color'):operative.append(dict(kind='tap_mana',color=c['mana_color'],amount=1))
 if c.get('token_tapped'):operative.append(dict(kind='tokens_enter_tapped'))
 if c.get('token_enchantment'):operative.append(dict(kind='token_enchantment'))
 if c.get('token_mana'):operative.append(dict(kind='token_sacrifice_colorless',amount=1))
 if c.get('followup'):operative.append(dict(kind=c['followup']))
 return copy.deepcopy(dict(profile='creator_wave6',schema=3,mechanism=c['kind'],token=token,amount=dict(fixed=c['count']),activation=activation,operative=operative,source_keywords=c.get('source_keywords',[]),inactive_clauses=[]))


def wave6_activation_variants(p):
 modes=['create','ready_snapshot','event_snapshot','cleanup','fresh_attack','ready_attack','block_ground','block_flying','destroy_token','bounce_token','exile_token']
 a=p['activation']
 if p['mechanism']=='land_creator':
  modes+=['mana_ability','land_entry']
  if any(c['kind']=='source_enters_tapped' for c in p['operative']):modes+=['entry_tapped_denied']
 if a:
  modes+=['payment','activation_snapshot']
  if any(a['mana']) and p['mechanism']!='land_creator':modes+=['insufficient_mana']
  if any(a['mana'][1:]) and p['mechanism']!='land_creator':modes+=['wrong_color']
  if not a['sacrifice']:modes+=['source_removed_pending','repeat']
  if a['tap'] or a['untap']:modes+=['tapped_or_untapped_denied','fresh_denied'] if p['creature'] else ['tapped_or_untapped_denied']
  if a['additional']:modes+=['missing_cost','opponent_cost','cost_receipt']
  if a['additional'] and a['additional']!='exile_own_grave_creature':modes+=['foreign_cost']
  if p['mechanism']=='land_creator' and a['sacrifice']:modes+=['foreign_source']
  if a['additional'] in ('tap_other_creature','tap_elf'):modes+=['fresh_other_cost']
 if any(c['kind']=='tokens_enter_tapped' for c in p['operative']):modes+=['token_untap_attack','tapped_token_block_denied']
 if any(c['kind']=='token_enchantment' for c in p['operative']):modes+=['enchantment_target']
 if any(c['kind']=='token_sacrifice_colorless' for c in p['operative']):modes+=['token_mana','token_mana_snapshot']
 if 'Flying' in p['token']['keywords']:modes+=['flying_evasion']
 if 'Trample' in p['token']['keywords']:modes+=['trample_damage']
 if 'Flying' in p['source_keywords']:modes+=['source_flying_evasion']
 if any(c['kind']=='controlled_saproling_death_drain' for c in p['operative']):modes+=['saproling_death','opponent_saproling_death','ordinary_death','source_removed_death','wrath_death','death_snapshot']
 if any(c['kind']=='another_controlled_spirit_entry_gain2' for c in p['operative']):modes+=['normal_spirit_entry','opponent_spirit_entry','ordinary_entry','source_removed_entry']
 if any(c['kind']=='own_lifegain_optional_pay2_draw1' for c in p['operative']):modes+=['lifelink_draw','lifelink_no_payment','normal_lifegain_draw','opponent_lifegain','source_removed_lifegain']
 return [m+'_'+str(owner) for owner in (0,1) for m in modes]

# Wave6 damage-trigger token creators.
WAVE6_DAMAGE_RULES = {'Precinct Captain': {'text': 'First strike\n'
                              'Whenever this creature deals combat damage to a player, create a 1/1 white '
                              'Soldier creature token.',
                      'config': {'predicate': 'self_combat_player',
                                 'quantity': 'events',
                                 'optional': False,
                                 'source_keywords': ['First strike']}},
 'Living Hive': {'text': 'Trample\n'
                         'Whenever this creature deals combat damage to a player, create that many 1/1 green '
                         'Insect creature tokens.',
                 'config': {'predicate': 'self_combat_player',
                            'quantity': 'damage',
                            'optional': False,
                            'source_keywords': ['Trample']}},
 'Rapacious One': {'text': 'Trample\n'
                           'Whenever this creature deals combat damage to a player, create that many 0/1 '
                           'colorless Eldrazi Spawn creature tokens. They have "Sacrifice this token: Add '
                           '{C}."',
                   'config': {'predicate': 'self_combat_player',
                              'quantity': 'damage',
                              'optional': False,
                              'source_keywords': ['Trample'],
                              'token_mana': True}},
 'Nested Ghoul': {'text': 'Whenever a source deals damage to this creature, create a 2/2 black Phyrexian '
                          'Zombie creature token.',
                  'config': {'predicate': 'source_dealt_damage',
                             'quantity': 'events',
                             'optional': False,
                             'source_keywords': []}},
 'Sharding Sphinx': {'text': 'Flying\n'
                             'Whenever an artifact creature you control deals combat damage to a player, you '
                             'may create a 1/1 blue Thopter artifact creature token with flying.',
                     'config': {'predicate': 'controlled_artifact_creature_combat_player',
                                'quantity': 'events',
                                'optional': True,
                                'source_keywords': ['Flying']}},
 'Brood Sliver': {'text': 'Whenever a Sliver deals combat damage to a player, its controller may create a 1/1 '
                          'colorless Sliver creature token.',
                  'config': {'predicate': 'global_sliver_combat_player',
                             'quantity': 'events',
                             'optional': True,
                             'source_keywords': []}},
 'Tana, the Bloodsower': {'text': 'Trample\n'
                                  'Whenever Tana deals combat damage to a player, create that many 1/1 green '
                                  'Saproling creature tokens.\n'
                                  'Partner (You can have two commanders if both have partner.)',
                          'config': {'predicate': 'self_combat_player',
                                     'quantity': 'damage',
                                     'optional': False,
                                     'source_keywords': ['Trample'],
                                     'inactive_prefixes': ['Partner ']}}}

def wave6_damage_params(record):
 entry=WAVE6_DAMAGE_RULES[record['name']];text=entry['text'];c=entry['config']
 if record['oracle_text']!=text:raise ValueError('whole reviewed damage-trigger source required')
 body=text.replace('create that many ','create a ');token=variable_token_from_creator(body)
 inactive=[line for line in text.splitlines() if any(line.startswith(prefix) for prefix in c.get('inactive_prefixes',[]))]
 operative=[dict(kind='damage_trigger',predicate=c['predicate'],optional=c['optional'],token_controller='damaging_creature_controller' if c['predicate']=='global_sliver_combat_player' else 'source_controller')]
 if c.get('token_mana'):operative.append(dict(kind='token_sacrifice_colorless',amount=1))
 return dict(profile='creator_wave6',schema=4,mechanism='damage_creator',token=token,amount=dict(basis=c['quantity']),activation=None,operative=operative,source_keywords=copy.deepcopy(c['source_keywords']),inactive_clauses=inactive)


def wave6_damage_variants(p):
 pred=p['operative'][0]['predicate'];modes=['create','blocked','other_creature','opponent_creature','source_removed','ready_snapshot','event_snapshot','damage_snapshot','cleanup','token_attack','destroy_token','bounce_token','exile_token']
 if pred!='source_dealt_damage':modes+=['fresh_denied']
 if 'Trample' in p['source_keywords']:modes+=['blocked_zero']
 if 'Flying' in p['source_keywords']:modes+=['flying_evasion']
 if pred=='source_dealt_damage':modes+=['multiple_damage_events','lethal_damage']
 if pred=='controlled_artifact_creature_combat_player':modes=[m for m in modes if m!='other_creature']+['artifact_creature','nonartifact_creature','noncombat_damage']
 if pred=='global_sliver_combat_player':modes+=['own_sliver','opponent_sliver','foreign_sliver']
 if any(c['kind']=='token_sacrifice_colorless' for c in p['operative']):modes+=['token_mana','token_mana_all','token_mana_snapshot']
 out=[m+'_'+str(owner) for owner in (0,1) for m in modes]
 if pred=='controlled_artifact_creature_combat_player':out+=['decline_0']
 return out

# Wave6 observer-unblocked Wolf creators.
WAVE6_WOLF_RULES = {'Howl of the Night Pack': {'text': 'Create a 2/2 green Wolf creature token for each Forest you control.',
                            'config': {'kind': 'forest_wolves', 'source_keywords': []}},
 'Bestial Menace': {'text': 'Create a 1/1 green Snake creature token, a 2/2 green Wolf creature token, and a '
                            '3/3 green Elephant creature token.',
                    'config': {'kind': 'three_bodies', 'source_keywords': []}},
 "Predator's Howl": {'text': 'Create a 2/2 green Wolf creature token.\n'
                             'Morbid — Create three 2/2 green Wolf creature tokens instead if a creature died '
                             'this turn.',
                     'config': {'kind': 'morbid_wolves', 'source_keywords': ['Morbid']}},
 'Fable of Wolf and Owl': {'text': 'Whenever you cast a green spell, you may create a 2/2 green Wolf creature '
                                   'token.\n'
                                   'Whenever you cast a blue spell, you may create a 1/1 blue Bird creature '
                                   'token with flying.',
                           'config': {'kind': 'two_color_cast',
                                      'source_keywords': [],
                                      'source_payment_choice': [3, 0, 0, 3, 0, 0]}},
 'Wolfbriar Elemental': {'text': 'Multikicker {G} (You may pay an additional {G} any number of times as you '
                                 'cast this spell.)\n'
                                 'When this creature enters, create a 2/2 green Wolf creature token for each '
                                 'time it was kicked.',
                         'config': {'kind': 'multikicker_wolves', 'source_keywords': ['Multikicker']}},
 'Raised by Wolves': {'text': 'Enchant creature\n'
                              'When this Aura enters, create two 2/2 green Wolf creature tokens.\n'
                              'Enchanted creature gets +1/+1 for each Wolf you control.',
                      'config': {'kind': 'aura_wolf_lord', 'source_keywords': ['Enchant']}}}

def wave6_wolf_tokens(text):
 pattern=r'(?:[Cc]reate |, |, and )(a|two|three) (\d+)/(\d+) (white|blue|black|red|green|colorless) ([A-Za-z ]+?) (artifact )?creature tokens?(?: with ([a-z ,]+?))?(?=\.|,| for each| instead|$)'
 out=[]
 for m in re.finditer(pattern,text):
  count,power,toughness,color,types,artifact,keywords=m.groups();subtypes=types.split();keys=[k.capitalize() for k in re.split(r', | and ',keywords)] if keywords else []
  if any(t not in SUBTYPES|{'Wolf','Plant','Ooze'} for t in subtypes) or any(k not in TOKEN_KEYWORDS for k in keys):raise ValueError('known observer token required')
  out.append(dict(token=dict(name=' '.join(subtypes),power=int(power),toughness=int(toughness),colors=[] if color=='colorless' else [VARIABLE_COLOR_WORD[color]],subtypes=subtypes,artifact=bool(artifact),keywords=keys),count={'a':1,'two':2,'three':3}[count]))
 if not out:raise ValueError('complete Wolf creator body required')
 return out


def wave6_wolf_params(record):
 entry=WAVE6_WOLF_RULES[record['name']];text=entry['text'];c=entry['config'];kind=c['kind']
 if record['oracle_text']!=text:raise ValueError('whole reviewed Wolf source required')
 components=wave6_wolf_tokens(text);wolf=next(t['token'] for t in components if t['token']['name']=='Wolf');operative=[]
 if kind=='forest_wolves':amount=dict(basis='controlled_forests')
 elif kind=='morbid_wolves':
  if len(components)!=2 or components[0]['token']!=components[1]['token']:raise ValueError('same morbid token body required')
  amount=dict(normal=1,morbid=3);operative=[dict(kind='any_creature_death_this_turn')]
 elif kind=='three_bodies':amount=dict(fixed=3)
 elif kind=='two_color_cast':amount=dict(basis='spell_colors');operative=[dict(kind='independent_green_blue_cast_creators',optional=True),dict(kind='legal_hybrid_payment_choice',mana=c['source_payment_choice'])]
 elif kind=='multikicker_wolves':amount=dict(basis='kicker_count');operative=[dict(kind='multikicker',mana=[0,0,0,1,0,0])]
 else:amount=dict(fixed=2);operative=[dict(kind='enchant_creature'),dict(kind='enchanted_creature_bonus_per_controlled_wolf',power=1,toughness=1)]
 return copy.deepcopy(dict(profile='creator_wave6',schema=4,mechanism='wolf_creator',wolf_kind=kind,token=wolf,tokens=components,amount=amount,activation=None,operative=operative,source_keywords=c['source_keywords'],inactive_clauses=[]))


def wave6_wolf_variants(p):
 kind=p['wolf_kind'];modes=['create','payment','insufficient_mana','ready_snapshot','event_snapshot','cleanup','fresh_attack','ready_attack','block_ground','block_flying','destroy_token','bounce_token','exile_token']
 if kind in ('forest_wolves','morbid_wolves'):modes+=['zero_input','opponent_input','ownership','prior_input_removed']
 if kind=='morbid_wolves':modes+=['token_death','opponent_token_death','noncreature_death','prior_turn_expired']
 if kind=='forest_wolves':modes+=['larger_amount','amount_locked']
 if kind=='three_bodies':modes+=['snake_attack','elephant_attack','elephant_block']
 if kind=='two_color_cast':modes=['create','green_spell','blue_spell','wrong_spell','opponent_spell','source_removed','repeat','payment','insufficient_mana','ready_snapshot','event_snapshot','bird_evasion','destroy_token','bounce_token','exile_token']
 if kind=='multikicker_wolves':modes+=['zero_kicker','larger_kicker','kicker_snapshot','bounce_recast']
 if kind=='aura_wolf_lord':modes+=['normal_wolf','opponent_wolf','foreign_wolf','opposing_bearer','wolf_removed','aura_removed','bearer_removed','bearer_attack','aura_lost_attack','grant_snapshot','bounce_recast']
 return [m+'_'+str(owner) for owner in (0,1) for m in modes]


def wave6_resolution_variants(p):
 kind=p['mechanism'];wolf=p.get('wolf_kind')
 supported=kind in ('flashback_create','x_create','x_wrath','death_history','spell_count','spell_x','fixed_followup') or kind=='wolf_creator' and wolf!='two_color_cast'
 modes=['countered'] if supported else []
 if wolf=='aura_wolf_lord':modes+=['lost_target']
 return [m+'_'+str(owner) for owner in (0,1) for m in modes]


def wave6_variants(p):
 return wave6_base_variants(p)+wave6_resolution_variants(p)
