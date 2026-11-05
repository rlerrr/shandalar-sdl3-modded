"""Whole-Oracle continuous creature stats, independent of the implementing handlers."""
import json,re
from collections import Counter
from pathlib import Path
from creature_recipes import KEYWORDS,REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS,reviewed_payment
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY

PINS=json.loads((Path(__file__).parent/'data/static_stats_witnesses.json').read_text(encoding='utf8'))['cards']
CORE=('Savannah Lions','Merfolk of the Pearl Trident','Scathe Zombies',"Mons's Goblin Raiders",'Grizzly Bears','Watchwolf','Hovermyr','Phyrexian Hulk','Air Elemental','Ornithopter','Wall of Stone','Isamaru, Hound of Konda','Dryad Arbor','Nyxborn Shieldmate','Metallic Sliver','Mothdust Changeling','Avian Changeling','Hill Giant','Scaled Wurm','Wasp Lancer','Zealous Guardian','Woolly Thoctar','Storm Fleet Sprinter')
COLORS={'white':'W','blue':'U','black':'B','red':'R','green':'G'}
PREDICATES={'all':'','artifact':'artifact ','colorless':'colorless ','multicolored':'multicolored ','legendary':'legendary ','nonlegendary':'nonlegendary ','token':'token ','nontoken':'nontoken '}
STATES={'all':'','attacking':'attacking ','blocking':'blocking ','tapped':'tapped ','untapped':'untapped '}


def phrase(q):
 if q['predicate']=='color':return q['value']+' '
 if q['predicate']=='not_color':return 'non'+q['value']+' '
 if q['predicate']=='subtypes':return ' '.join(q['value'])+' '
 return PREDICATES.get(q['predicate'],'')


def matches(q,r,owner,controller,ref,source,*,token=False,tapped=False,attacking=False,blocking=False):
 if 'Creature' not in r['type_line'].partition(' — ')[0].split():return False
 if q['other'] and ref==source or q['scope']=='own' and controller!=owner or q['scope']=='opponents' and controller==owner:return False
 if not {'all':True,'attacking':attacking,'blocking':blocking,'tapped':tapped,'untapped':not tapped}[q['state']]:return False
 k=q['predicate'];v=q['value'];types=r['type_line'].partition(' — ')[0].split();subs=r['type_line'].partition(' — ')[2].split();colors=r.get('colors',[])
 if k=='all':return True
 if k=='color':return COLORS[v] in colors
 if k=='not_color':return COLORS[v] not in colors
 if k=='subtypes':return 'Changeling' in r.get('keywords',[]) or set(v)<=set(subs)
 if k=='artifact':return 'Artifact' in types
 if k=='colorless':return not colors
 if k=='multicolored':return len(colors)>1
 if k=='legendary':return 'Legendary' in types
 if k=='nonlegendary':return 'Legendary' not in types
 if k=='token':return token
 if k=='nontoken':return not token
 if k=='flying':return 'Flying' in r.get('keywords',[])
 return False


def spec(d,r):
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('pinned normal whole static source and explicit variants required')
 types=r['type_line'].partition(' — ')[0].split();creature='Creature' in types
 if not {'Artifact','Enchantment','Creature'}&set(types) or {'Aura','Equipment'}&set(r['type_line'].partition(' — ')[2].split()):raise ValueError('standalone static permanent required')
 p=d.get('params')
 if not isinstance(p,dict) or not {'clauses','keywords','activated_abilities'}<=set(p) or set(p)-{'clauses','keywords','activated_abilities','payment_colors'}:raise ValueError('explicit continuous clauses, source keywords and inactive abilities required')
 if not isinstance(p['clauses'],list) or not 1<=len(p['clauses'])<=3:raise ValueError('one to three ordinary continuous clauses required')
 expected=[]
 for q in p['clauses']:
  if not isinstance(q,dict) or set(q)!={'scope','other','state','predicate','value','power','toughness'} or q['scope'] not in ('own','opponents','both') or type(q['other']) is not bool or q['state'] not in STATES or any(type(q[k]) is not int or not -3<=q[k]<=3 for k in ('power','toughness')) or not (q['power'] or q['toughness']):raise ValueError('bounded signed stats and explicit creature predicate required')
  k=q['predicate'];v=q['value']
  if not (k in PREDICATES and v=='' or k in ('color','not_color') and v in COLORS or k=='subtypes' and isinstance(v,list) and 1<=len(v)<=2 and len(set(v))==len(v) and all(isinstance(t,str) and re.fullmatch('[A-Z][A-Za-z-]+',t) for t in v) or k=='flying' and v==''):raise ValueError('reviewed creature predicate required')
  noun=STATES[q['state']]+phrase(q)+'creatures'+{'own':' you control','opponents':' your opponents control','both':''}[q['scope']]+(' with flying' if k=='flying' else '')
  stems=[('Other ' if q['other'] else '')+noun]
  if not q['other']:stems.append('All '+noun)
  powers=('-0','+0') if q['power']==0 else (f"{q['power']:+d}",);toughs=('-0','+0') if q['toughness']==0 else (f"{q['toughness']:+d}",)
  forms={s[0].upper()+s[1:]+f' get {a}/{b}.' for s in stems for a in powers for b in toughs}
  if k=='subtypes' and len(v)==1:
   plural={'Elf':'Elves','Merfolk':'Merfolk','Myr':'Myr','Faerie':'Faeries'}.get(v[0],v[0]+'s');forms|={s.replace(v[0]+' creatures',plural) for s in tuple(forms)}
  expected.append(forms)
 lines=r['oracle_text'].splitlines();actual=[]
 for forms in expected:
  found=[l for l in lines if l in forms]
  if len(found)!=1:raise ValueError('continuous clause must match complete printed effect')
  actual.append(found[0]);lines.remove(found[0])
 if len(actual)!=len(set(actual)):raise ValueError('duplicate continuous clause')
 abilities=[];keys=[];canonical={k.lower():k for k in KEYWORDS+('Hexproof','Indestructible','Devoid','Split second','Ingest')}
 for line in lines:
  if ':' in line and re.match(r'^(?:\{|Sacrifice |Discard |Remove |Exile |Pay |Tap |Untap |Return )',line):abilities.append(line);continue
  for reminder in re.findall(r'\([^\n]*?\)',line):
   if reminder not in REMINDERS|EXTRA_REMINDERS|{'(This card has no color.)',"(As long as this spell is on the stack, players can't cast spells or activate abilities that aren't mana abilities.)",'(Whenever this creature deals combat damage to a player, that player exiles the top card of their library.)'}:raise ValueError('unreviewed source reminder')
   line=line.replace(reminder,'')
  if not line.strip():continue
  for word in line.strip().split(','):
   if word.strip().lower() not in canonical:raise ValueError('additional passive/triggered clause needs separate coverage')
   keys.append(canonical[word.strip().lower()])
 if p['keywords']!=sorted(keys) or len(keys)!=len(set(keys)) or set(keys)!=set(r.get('keywords',[])) or p['activated_abilities']!=abilities:raise ValueError('whole passive and inactive-ability scope must match')
 if creature and (any(not re.fullmatch(r'\d+',r.get(k,'')) or not 0<=int(r[k])<=20 for k in ('power','toughness')) or not int(r['toughness'])):raise ValueError('bounded surviving numeric source required')
 mana,hybrid=reviewed_payment(r,p)
 return dict(p,family='static_stats',mana=mana,creature=creature,legendary='Legendary' in types,world='World' in types,hybrid=hybrid)


def army(p):
 names=list(CORE)
 for q in p['clauses']:
  if q['predicate']!='subtypes':continue
  choices=[n for n,r in PINS.items() if n not in names and 'Creature' in r['type_line'] and set(q['value'])<=set(r['type_line'].partition(' — ')[2].split())]
  if choices:names.append(min(choices,key=lambda n:(len(PINS[n]['oracle_text']),n)))
 return names


def variants(p):
 vs=['static','cast','payment','insufficient_mana','bare_zones','cleanup','tapped_source','source_bounced','source_destroyed','bounce_recast','late_arrival','blink_witness','pump','change_tapping','attackers','blockers','combat','ready_snapshot','pending_snapshot','resolved_snapshot']
 if not p['legendary'] and not p['world']:vs+=['two_sources','remove_one_source']
 if not p['world']:vs+=['opposing_source']
 if p['creature']:vs+=['blink_source','sick_source']
 if any(p['mana'][1:]):vs+=['wrong_color']
 if p['hybrid']:vs+=['hybrid_other']+(['hybrid_mixed'] if len(p['hybrid'])>1 else [])
 if any(q['toughness']>0 and q['state'] in ('all','untapped') for q in p['clauses']):vs+=['damage_then_bounce']
 return [v+'_'+str(owner) for owner in (0,1) for v in vs]


def validate_witnesses(o,rows,p):
 for n,r in PINS.items():
  if n not in rows or any(o.get(n,{}).get(k)!=v for k,v in r.items()):raise ValueError('static stats witness changed: '+n)
 for q in p['clauses']:
  if q['predicate']=='subtypes' and not any(set(q['value'])<=set(o[n]['type_line'].partition(' — ')[2].split()) for n in army(p)):raise ValueError('printed subtype needs a real nonchangeling witness')


def expand(name,csvid,d,p,selected,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 out=[]
 for variant in selected:
  v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';casting=v in ('cast','payment','insufficient_mana','wrong_color','pending_snapshot','hybrid_other','hybrid_mixed')
  cast_cost=p['mana']
  if v in ('hybrid_other','hybrid_mixed'):
   colors=[(b if c==a else a) if v=='hybrid_other' or i%2 else c for i,((a,b),c) in enumerate(zip(p['hybrid'],p['payment_colors']))]
   cast_cost,_=reviewed_payment(o[name],dict(p,payment_colors=colors))
  zones={z:[[] if bare else list(ns),[] if bare else list(ns)] for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY))}
  mana=[[20]*6,[20]*6];life=[20,20];entities={};setup=[];actions=[];sources=[];extra={};cut=0
  for pl in (0,1):
   for z,ns in zones.items():setup.extend(card(n,f'p{pl}_{z}_{i}',pl,z) for i,n in enumerate(ns[pl]))
  def add(n,ref,pl,zone='battlefield',*,token=False,tapped=False,sick=False):
   x=card(n,ref,pl,zone)
   if zone=='battlefield':x['state']=(['tapped'] if tapped else ['untapped'])+(['token'] if token else [])+(['sick'] if sick else [])
   setup.append(x);entities[ref]=dict(name=n,player=pl,alive=zone=='battlefield',token=token,tapped=tapped,attacking=False,blocking=False,damage=0)
   if zone=='hand':zones['hand'][pl].append(n)
  def move(ref,dest):
   e=entities[ref];e['alive']=False;e['damage']=0;e['attacking']=e['blocking']=False
   if not e['token']:zones[dest][e['player']].append(e['name'])
   if ref in sources:sources.remove(ref)
  def value(ref,k):
   e=entities[ref];r=o[e['name']];bonus=sum(q[k] for sr in sources for q in p['clauses'] if matches(q,r,entities[sr]['player'],e['player'],ref,sr,token=e['token'],tapped=e['tapped'],attacking=e['attacking'],blocking=e['blocking']))
   return int(r[k])+bonus+extra.get(ref,0)
  def sba():
   while True:
    dead=[ref for ref,e in entities.items() if e['alive'] and 'Creature' in o[e['name']]['type_line'] and (value(ref,'toughness')<=0 or e['damage']>=value(ref,'toughness') and 'Indestructible' not in o[e['name']].get('keywords',[]))]
    if not dead:return
    for ref in dead:move(ref,'graveyard')
  def stats():
   checks=[]
   for ref,e in entities.items():
    if not e['alive']:continue
    checks+=[observe(ref,'in_play',1),observe(ref,'tapped',int(e['tapped']))]
    if 'Creature' in o[e['name']]['type_line']:checks += [observe(ref,k,value(ref,k)) for k in ('power','toughness')]+[observe(ref,'damage',e['damage'])]
   for pl in (0,1):checks.append(step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values())))
   return checks
  def spell(n,ref,caster,target,dialog=None):
   add(n,ref,caster,'hand');actions.append(step('turn',player=caster,value=0));actions.extend([step('choose_target',player=caster,ref=target),step('cast',ref=ref,value=1)])
   if dialog is not None:actions.append(step('choose_dialog',player=caster,value=dialog))
   mana[caster]=[a-b for a,b in zip(mana[caster],cost_from_oracle(o[n]))];zones['hand'][caster].remove(n);zones['graveyard'][caster].append(n);actions.extend([step('resolve'),step('assert_choices_consumed')])
  add(name,'source',owner,'hand' if casting else 'battlefield',tapped=v=='tapped_source',sick=v=='sick_source')
  if not casting:sources.append('source')
  if v in ('two_sources','remove_one_source','opposing_source'):
   add(name,'second',1-owner if v=='opposing_source' else owner);sources.append('second')
  for pl in (0,1):
   for i,n in enumerate(army(p)):add(n,f'army_{pl}_{i}',pl,tapped='Land' in o[n]['type_line'])
   add('Grizzly Bears',f'token_{pl}',pl,token=True)
   add('Darksteel Relic',f'artifact_{pl}',pl);add('Seal of Fire',f'enchantment_{pl}',pl)
  pre=[step('turn',player=owner,value=0)]
  if v in ('payment','insufficient_mana','wrong_color','hybrid_other','hybrid_mixed'):
   mana[owner]=list(cast_cost) if v!='insufficient_mana' else [0]*6
   if v=='wrong_color':
    i=next(i for i in range(1,6) if mana[owner][i]);mana[owner][0]+=mana[owner][i];mana[owner][i]=0
  for pl in (0,1):pre.extend(step('set_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
  pre.append(step('refresh'));sba();actions+=stats()
  if casting:
   legal=v not in ('insufficient_mana','wrong_color')
   if p['hybrid'] and owner==0 and v in ('cast','pending_snapshot'):actions.append(step('choose_payment',player=owner,value=sum(n<<(4*i) for i,n in enumerate(cast_cost))))
   actions.append(step('cast',ref='source',value=int(legal)))
   if legal:
    zones['hand'][owner].remove(name);mana[owner]=[a-b for a,b in zip(mana[owner],cast_cost)];actions.append(step('assert_stack',value=1));actions+=stats();cut=len(actions)
    actions.extend([step('resolve'),step('assert_choices_consumed')]);entities['source']['alive']=True;sources.append('source');sba();actions+=stats()
  if v in ('source_bounced','source_destroyed','bounce_recast','remove_one_source','damage_then_bounce'):
   if v=='damage_then_bounce':
    candidates=[(ref,n,amount) for n,amount in (('Shock',2),('Lightning Bolt',3),('Flame Slash',4)) for ref,e in entities.items() if e['alive'] and ref not in sources and 'Creature' in o[e['name']]['type_line'] and int(o[e['name']]['toughness'])<=amount<value(ref,'toughness')]
    if not candidates:raise ValueError('toughness-loss fixture needs a surviving damaged witness')
    ref,n,amount=candidates[0];spell(n,'damage',owner,ref);entities[ref]['damage']=amount;sba();actions+=stats()
   if v=='source_destroyed':
    actions.append(step('destroy',ref='source'))
    if 'Indestructible' not in p['keywords']:move('source','graveyard')
   else:
    spell('Boomerang','bounce',owner,'source');move('source','hand')
   sba();actions+=stats()
   if v=='bounce_recast':
    if p['hybrid'] and owner==0:actions.append(step('choose_payment',player=owner,value=sum(n<<(4*i) for i,n in enumerate(cast_cost))))
    actions.extend([step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'),step('cast',ref='source',value=1),step('resolve'),step('assert_choices_consumed')]);zones['hand'][owner].remove(name);mana[owner]=[a-b for a,b in zip(mana[owner],cast_cost)];entities['source']['alive']=True;entities['source']['tapped']=False;sources.append('source');sba();actions+=stats()
  if v in ('blink_source','blink_witness'):
   ref='source' if v=='blink_source' else next(ref for ref,e in entities.items() if ref.startswith('army_') and e['player']==owner and e['alive'] and 'Land' not in o[e['name']]['type_line'])
   spell('Cloudshift','blink',owner,ref);e=entities[ref];e['tapped']=False;e['damage']=0;actions.append(step('bind_permanent_copy',player=e['player'],value=rows[e['name']]['csvid'],index=0,ref=ref));sba();actions+=stats()
  if v=='late_arrival':
   choices=[(n,pl) for n in army(p)+['Jasmine Boreal'] for pl in (owner,1-owner) if '/' not in o[n]['mana_cost'] and 'Bestow' not in o[n].get('keywords',[]) and not ('Legendary' in o[n]['type_line'] and any(e['alive'] and e['player']==pl and e['name']==n for e in entities.values())) and any(matches(q,o[n],owner,pl,'late','source') for q in p['clauses'])]
   n,pl=choices[0] if choices else ('Grizzly Bears',owner)
   add(n,'late',pl,'hand');actions.extend([step('turn',player=pl,value=0),step('cast',ref='late',value=1),step('resolve'),step('assert_choices_consumed')]);zones['hand'][pl].remove(n);mana[pl]=[a-b for a,b in zip(mana[pl],cost_from_oracle(o[n]))];entities['late']['alive']=True;sba();actions+=stats()
  if v in ('pump','change_tapping'):
   ref=next(ref for ref,e in entities.items() if ref.startswith('army_') and e['player']==owner and e['alive'] and 'Land' not in o[e['name']]['type_line'])
   if v=='pump':spell('Giant Growth','pump',owner,ref);extra[ref]=3
   else:spell('Twiddle','twiddle',owner,ref,0 if not entities[ref]['tapped'] else 1);entities[ref]['tapped']=not entities[ref]['tapped']
   sba();actions+=stats()
  if v in ('attackers','blockers','combat'):
   pl=1-owner if v=='blockers' else owner
   eligible_refs=[ref for ref,e in entities.items() if ref.startswith('army_') and e['player']==pl and e['alive'] and not e['tapped'] and o[e['name']]['oracle_text']=='']
   ref=max(eligible_refs,key=lambda ref:value(ref,'toughness'));actions.extend([step('turn',player=pl,value=0),step('begin_attackers'),step('attack',ref=ref,value=1),step('finish_attackers'),step('begin_blockers')]);entities[ref]['attacking']=True;entities[ref]['tapped']=True
   if v=='blockers':
    blocker=max((ref for ref,e in entities.items() if ref.startswith('army_') and e['player']==owner and e['alive'] and not e['tapped'] and o[e['name']]['oracle_text']==''),key=lambda ref:value(ref,'toughness'));actions.append(step('block',ref=blocker,other=ref,value=1));entities[blocker]['blocking']=True
   actions.append(step('advance_combat'));sba();actions+=stats()
   if v=='combat':
    life[1-pl]-=max(0,value(ref,'power'));actions.extend([step('combat_damage'),step('assert_life',player=1-pl,value=life[1-pl]),step('end_combat')])
    for e in entities.values():e['attacking']=e['blocking']=False
    sba();actions+=stats()
  if v in ('cleanup','pump','attackers','blockers','combat'):
   if v in ('attackers','blockers'):actions.extend(step('remove_from_combat',ref=ref) for ref,e in entities.items() if e['alive'] and (e['attacking'] or e['blocking']))
   actions.append(step('cleanup'));extra.clear()
   for e in entities.values():e['attacking']=e['blocking']=False;e['damage']=0
   sba();actions+=stats()
  checks=[step('assert_stack',value=0),step('assert_choices_consumed')];names={x['name'] for x in setup}
  for pl in (0,1):
   checks.extend([step('assert_life',player=pl,value=life[pl])]);checks.append(step('assert_mana_total',player=pl,value=sum(mana[pl]))) if p['hybrid'] and pl==owner and owner==1 and v in ('cast','pending_snapshot','bounce_recast') else checks.extend(step('assert_mana',player=pl,color=i,value=n) for i,n in enumerate(mana[pl]))
   checks.append(step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values())))
   checks.extend(step('assert_zone_multiplicity',player=pl,zone='battlefield',index=rows[n]['csvid'],value=sum(e['alive'] and e['player']==pl and e['name']==n for e in entities.values())) for n in sorted(names))
   for z,ns in zones.items():
    checks.append(step('assert_zone',player=pl,zone=z,value=len(ns[pl])))
    checks.extend(step('assert_zone_card',player=pl,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(ns[pl])) if z=='library' else checks.extend(step('assert_zone_multiplicity',player=pl,zone=z,index=rows[n]['csvid'],value=ns[pl].count(n)) for n in sorted(names))
   checks.extend(step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=zones['hand'][pl].count(n)+zones['graveyard'][pl].count(n)) for n in sorted(names))
  steps=pre+actions+checks
  if v in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
   at=0 if v=='ready_snapshot' else cut if v=='pending_snapshot' else len(actions)
   steps=pre+actions[:at]+[step('capture')]+actions[at:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[at:]+checks+[step('assert_recorded_state')]
  out.append(dict(name=f'StaticStatsRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family='static_stats',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_abilities=p['activated_abilities'],untested_components=['source keyword combat','inactive activated abilities','replacement effects','live color/type/control changes','multiplayer'])))
 return out
