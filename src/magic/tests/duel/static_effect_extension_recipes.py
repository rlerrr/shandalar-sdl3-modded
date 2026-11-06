"""Whole-Oracle wave6 static effects with private palettes and real actions.

This family deliberately does not extend any prior family's global witnesses.
"""
import copy
import json
from pathlib import Path

FAMILY='static_effect_extension'
REVIEWS=json.loads((Path(__file__).parent/'data/static_effect_extension_witnesses.json').read_text(encoding='utf8'))


def _same_pin(actual,expected):
 if type(actual) is not type(expected):return False
 if isinstance(expected,dict):return actual.keys()==expected.keys() and all(_same_pin(actual[k],v) for k,v in expected.items())
 if isinstance(expected,list):return len(actual)==len(expected) and all(_same_pin(a,b) for a,b in zip(actual,expected))
 return actual==expected


def spec(d,r):
 entry=REVIEWS['sources'].get(r.get('name')) if isinstance(r,dict) else None
 if (not entry or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or not _same_pin(r,entry['oracle'])
     or d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r['oracle_text']
     or not _same_pin(d.get('params'),entry['params']) or 'variants' not in d):
  raise ValueError('whole pinned source and explicit reviewed version2 static-effect parameters required')
 p=copy.deepcopy(entry['params'])
 if type(p['schema']) is not int or p['schema'] not in (1,2,3) or p['kind'] not in ('grant','stats','turn_stats','block_permission','entry_tapped','attack_permission','defender_permission','color','flying_loss','named_restriction','conditional_stats'):raise ValueError('unreviewed static-effect mechanism')
 from recipes import cost_from_oracle
 return dict(p,mana=cost_from_oracle(r),family=FAMILY,source_name=r['name'],creature='Creature' in r['type_line'],legendary='Legendary' in r['type_line'],world='World' in r['type_line'])


def variants(p):
 if p['schema']==3:return _variants3(p)
 modes=['effect','payment','insufficient_mana','wrong_color','countered','bare_zones','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot','source_bounced','source_destroyed','bounce_recast','late_arrival','opponent_recipient','foreign_recipient','foreign_destroyed']
 if not p['legendary'] and not p['world']:modes+=['two_sources','remove_one_source','opposing_source']
 if p['creature']:
  modes+=['tapped_source']
  if p['kind']!='entry_tapped':modes+=['source_recipient']
 if p['kind']=='grant' and p['predicate']!='all':modes+=['nonmatching','changeling']
 if p['kind']=='turn_stats':modes+=['opposing_turn','turn_transition','damaged_turn_transition']
 if p['kind']=='stats':modes+=['nonmatching']
 if p['kind']=='entry_tapped':
  modes=[v for v in modes if v not in ('late_arrival','opponent_recipient','foreign_recipient','foreign_destroyed')];modes+=['own_entry','entry_artifact','entry_land','entry_enchantment','entry_artifact_creature']
 if p['kind']=='attack_permission':
  if p['rule'] in ('you','power_le2'):modes.remove('opponent_recipient')
  if p['rule'] in ('nonflying','power_le2'):modes+=['nonmatching']
  if p['rule'] in ('you','power_le2'):modes+=['own_attacker']
  if p['rule']=='power_le2':modes+=['pumped_attacker']
 if p['kind']=='defender_permission':modes+=['nonmatching']
 if p.get('alternate_blocker'):modes+=['ground_blocker']
 if p.get('self_blocker'):modes+=['source_blocker']
 if p['kind']=='named_restriction':modes+=['blocked_receiver','block_countered']
 if p['kind']=='flying_loss' and 'Islandwalk' in p['removed_keywords']:modes+=['islandwalk']
 if p['primary_keyword']=='Haste':modes.remove('late_arrival')
 if not any(p['mana'][1:]):modes.remove('wrong_color')
 return [f'{v}_{owner}' for owner in (0,1) for v in modes]


def validate_witnesses(oracle,rows):
 for name,pin in REVIEWS['witnesses'].items():
  if name not in rows or not _same_pin(oracle.get(name,{}),pin):raise ValueError('static-effect witness changed: '+name)


def expand(name,csvid,d,p,selected,rows,o):
 validate_witnesses(o,rows)
 if not isinstance(selected,list) or not selected or len(selected)!=len(set(selected)) or set(selected)-set(variants(p)):raise ValueError('explicit distinct supported static-effect variants required')
 return [_case(name,csvid,d,p,v,rows,o) for v in selected]


class _World:
 def __init__(self,p,owner,rows,o,bare):
  self.p=p;self.owner=owner;self.rows=rows;self.o=o;self.setup=[];self.entities={};self.actions=[];self.mana=[[20]*6,[20]*6];self.life=[20,20];self.turn=owner
  self.zones={z:[list(names) if not bare else [],list(names) if not bare else []] for z,names in [('hand',['Plains']),('graveyard',['Forest']),('exile',['Island']),('library',['Forest','Plains','Hill Giant','Island'])]}
  for z,players in self.zones.items():
   for pl,names in enumerate(players):
    for i,n in enumerate(names):self.setup.append(dict(name=n,ref=f'{z}_{pl}_{i}',player=pl,zone=z))
 def add(self,name,ref,pl,zone='battlefield',*,token=False,tapped=False,sick=False,source=False,foreign=False):
  record=self.o[name]
  if record.get('layout')=='token' and (zone!='battlefield' or not token):raise ValueError('token Oracle record requires a token battlefield fixture; it is not a castable physical card: '+name)
  item=dict(name=name,ref=ref,player=pl,zone=zone)
  if zone=='battlefield':item['state']=['tapped' if tapped else 'untapped']+(['token'] if token else [])+(['sick'] if sick else [])+(['opponent_owned'] if foreign else [])
  self.setup.append(item);self.entities[ref]=dict(name=name,player=pl,owner=1-pl if foreign else pl,zone=zone,token=token,tapped=tapped,sick=sick,damage=0,extra=0,source=source)
  if zone!='battlefield':self.zones[zone][pl].append(name)
 def sources(self):return [ref for ref,e in self.entities.items() if e['source'] and e['zone']=='battlefield']
 def matches(self,ref,sr):
  e=self.entities[ref];s=self.entities[sr];r=self.o[e['name']];p=self.p
  if 'Creature' not in r['type_line'] or p['other'] and ref==sr:return False
  if p['scope']=='own' and e['player']!=s['player'] or p['scope']=='opponents' and e['player']==s['player']:return False
  pred=p['predicate'];v=p['value']
  return pred=='all' or pred=='token' and e['token'] or pred=='nontoken' and not e['token'] or pred=='subtype' and (v in r['type_line'].partition(' — ')[2].split() or 'Changeling' in r['keywords']) or pred=='color' and v in r['colors']
 def keys(self,ref):
  e=self.entities[ref];keys=set(self.p['source_keywords'] if e['source'] else self.o[e['name']].get('keywords',[]));keys.update(e.get('temporary_keys',[]))
  for sr in self.sources():
   if self.matches(ref,sr):keys.update(self.p['granted_keywords'])
  if self.p['kind']=='flying_loss' and self.sources() and 'Creature' in self.o[e['name']]['type_line']:keys.difference_update(self.p['removed_keywords'])
  return keys
 def colors(self,ref):
  e=self.entities[ref]
  if self.p['kind']=='color' and self.sources() and 'Creature' in self.o[e['name']]['type_line']:return {self.p['value']}
  return set(self.o[e['name']]['colors'])
 def has_land(self,pl,subtype):return any(e['zone']=='battlefield' and e['player']==pl and 'Land' in self.o[e['name']]['type_line'].partition(' — ')[0].split() and subtype in self.o[e['name']]['type_line'].partition(' — ')[2].split() for e in self.entities.values())
 def can_attack(self,ref):
  e=self.entities[ref];ks=self.keys(ref);types=self.o[e['name']]['type_line'].partition(' — ')[2].split()
  wall_override=self.p['kind']=='defender_permission' and self.sources() and ('Wall' in types or 'Changeling' in ks)
  if e['tapped'] or e['sick'] and 'Haste' not in ks or 'Defender' in ks and not wall_override:return False
  for sr in self.sources():
   rule=self.p.get('rule');source=self.entities[sr]
   if self.p['kind']=='attack_permission':
    if rule=='all' or rule=='nonflying' and 'Flying' not in ks:return False
    if e['player']!=source['player'] and (rule=='you' or rule=='power_le2' and self.stat(ref,'power')<=2):return False
   if self.p['kind']=='named_restriction' and e['name']==self.p['value']:return False
  return True
 def can_block(self,blocker,attacker):
  b=self.entities[blocker];a=self.entities[attacker];bk=self.keys(blocker);ak=self.keys(attacker)
  if b['tapped'] or 'Flying' in ak and not bk&{'Flying','Reach'}:return False
  for keyword,land in [('Islandwalk','Island'),('Forestwalk','Forest'),('Swampwalk','Swamp'),('Mountainwalk','Mountain'),('Plainswalk','Plains')]:
   if keyword in ak and self.has_land(b['player'],land):return False
  for sr in self.sources():
   if self.p['kind']=='block_permission':
    rule=self.p.get('rule','all')
    if rule=='all' or rule=='black' and 'B' in self.colors(blocker) or rule=='flying_vs_own' and 'Flying' in bk and a['player']==self.entities[sr]['player']:return False
   if self.p['kind']=='named_restriction' and b['name']==self.p['value']:return False
  return True
 def stat(self,ref,field):
  e=self.entities[ref];value=int(self.o[e['name']][field])+e['extra']
  for sr in self.sources():
   if self.matches(ref,sr):
    if self.p['kind']=='turn_stats':value+=self.p['own_turn'][0 if field=='power' else 1] if self.turn==self.entities[sr]['player'] else self.p['other_turn'][0 if field=='power' else 1]
    else:value+=self.p[field]
  return value
 def move(self,ref,zone):
  e=self.entities[ref];assert e['zone']=='battlefield';e['zone']='gone' if e['token'] else zone;e['damage']=0
  if not e['token']:self.zones[zone][e['owner']].append(e['name'])
 def sba(self):
  while True:
   dead=[ref for ref,e in self.entities.items() if e['zone']=='battlefield' and 'Creature' in self.o[e['name']]['type_line'] and (self.stat(ref,'toughness')<=0 or e['damage']>=self.stat(ref,'toughness') and 'Indestructible' not in self.keys(ref))]
   if not dead:return
   for ref in dead:self.move(ref,'graveyard')
 def pay(self,name,pl):
  from recipes import cost_from_oracle
  cost=cost_from_oracle(self.o[name]) if self.o[name]['mana_cost'] not in ('','{0}') else [0]*6;self.mana[pl]=[a-b for a,b in zip(self.mana[pl],cost)];assert min(self.mana[pl])>=0
 def active_entry(self,ref):
  e=self.entities[ref]
  return self.p['kind']=='entry_tapped' and any((self.p['scope']=='both' or (e['player']!=self.entities[sr]['player'] if self.p['scope']=='opponents' else e['player']==self.entities[sr]['player'])) and bool(set(self.o[e['name']]['type_line'].partition(' — ')[0].split())&set(self.p['entry_types'])) for sr in self.sources())
 def enter(self,ref):
  e=self.entities[ref];self.zones['hand'][e['player']].remove(e['name']);e['zone']='battlefield';e['tapped']=self.active_entry(ref);e['sick']='Creature' in self.o[e['name']]['type_line'];self.sba()
 def cast(self,ref,target=None,dialog=None):
  from recipes import step
  e=self.entities[ref];pl=e['player']
  if self.o[e['name']].get('layout')!='normal' or self.o[e['name']]['type_line'].startswith('Token '):raise ValueError('only a pinned normal physical card can be cast or entered from hand')
  self.actions.append(step('turn',player=pl,value=0));self.turn=pl
  if target:self.actions.append(step('choose_target',player=pl,ref=target))
  land='Land' in self.o[e['name']]['type_line'].partition(' — ')[0].split()
  if land:
   # A land is not a spell. Use the established real Atlas hand-to-play path.
   atlas='atlas_'+str(pl);copy_index=sum(x['zone']=='battlefield' and x['player']==pl and x['name']==e['name'] for x in self.entities.values())
   self.actions += [step('require_can_activate',ref=atlas,value=1),step('activate',ref=atlas,value=1),step('assert_stack',value=1),step('choose_zone_card',player=pl,value=self.rows[e['name']]['csvid']),step('resolve'),step('assert_choices_consumed'),step('assert_stack',value=0)]
   self.entities[atlas]['tapped']=True;self.enter(ref);self.actions.append(step('bind_permanent_copy',player=pl,value=self.rows[e['name']]['csvid'],index=copy_index,ref=ref));return
  self.actions += [step('cast',ref=ref,value=1),step('assert_stack',value=1)]
  self.pay(e['name'],pl)
  if dialog is not None:self.actions.append(step('choose_dialog',player=pl,value=dialog))
  self.actions += [step('resolve'),step('assert_choices_consumed'),step('assert_stack',value=0)]
  if set(self.o[e['name']]['type_line'].partition(' — ')[0].split())&{'Instant','Sorcery'}:
   self.zones['hand'][pl].remove(e['name']);self.zones['graveyard'][pl].append(e['name']);e['zone']='graveyard'
  else:self.enter(ref)
 def inspect(self):
  from recipes import observe
  out=[]
  for ref,e in self.entities.items():
   if e['zone']!='battlefield':continue
   out += [observe(ref,'in_play',1),observe(ref,'tapped',int(e['tapped']))]
   if 'Creature' in self.o[e['name']]['type_line']:out += [observe(ref,k,self.stat(ref,k)) for k in ('power','toughness')]+[observe(ref,'damage',e['damage'])]
  if self.p['kind']=='color':
   color_bits={'B':2,'U':4,'G':8,'R':16,'W':32};seen=set()
   for ref,e in self.entities.items():
    if e['zone']!='battlefield' or (e['player'],e['name']) in seen:continue
    seen.add((e['player'],e['name']));out.append(__import__('recipes').step('assert_matching_permanents',player=e['player'],index=self.rows[e['name']]['csvid'],property='color',value=sum(color_bits[c] for c in self.colors(ref))))
  return out
 def receipts(self):
  from recipes import step
  names={x['name'] for x in self.setup};out=[]
  for pl in (0,1):
   alive=[e['name'] for e in self.entities.values() if e['zone']=='battlefield' and e['player']==pl]
   out += [step('assert_life',player=pl,value=self.life[pl]),step('assert_poison',player=pl,value=0),step('assert_zone',player=pl,zone='battlefield',value=len(alive))]
   out += [step('assert_zone_multiplicity',player=pl,zone='battlefield',index=self.rows[n]['csvid'],value=alive.count(n)) for n in sorted(names)]
   out += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(self.mana[pl])]
   for z,players in self.zones.items():
    ns=players[pl];out.append(step('assert_zone',player=pl,zone=z,value=len(ns)));out += [step('assert_zone_multiplicity',player=pl,zone=z,index=self.rows[n]['csvid'],value=ns.count(n)) for n in sorted(names)]
    if z=='library':out += [step('assert_zone_card',player=pl,zone=z,index=i,value=self.rows[n]['csvid']) for i,n in enumerate(ns)]
  return out
 def checks(self):
  from recipes import step
  return [step('assert_stack',value=0),step('assert_choices_consumed')]+self.inspect()+self.receipts()
 def refill(self):
  from recipes import step
  self.mana=[[20]*6,[20]*6];self.actions += [step('set_mana',player=pl,color=c,value=20) for pl in (0,1) for c in range(6)]
 def remember(self):return copy.deepcopy((self.entities,self.zones,self.mana,self.life,self.turn))
 def recall(self,state):self.entities,self.zones,self.mana,self.life,self.turn=copy.deepcopy(state)
 def combat(self,attacker,blocker=None,legality_only=False):
  from recipes import step,observe
  a=self.entities[attacker];actor=a['player'];self.turn=actor;ak=self.keys(attacker);attack=self.can_attack(attacker)
  self.actions += [step('turn',player=actor,value=0),step('begin_attackers'),step('attack',ref=attacker,value=int(attack)),observe(attacker,'attacking',int(attack))]
  if attack:a['tapped']='Vigilance' not in ak
  self.actions += [observe(attacker,'tapped',int(a['tapped'])),step('finish_attackers'),step('begin_blockers')];blocked=False
  if blocker and attack:
   b=self.entities[blocker];bk=self.keys(blocker);blocked=self.can_block(blocker,attacker)
   self.actions.append(step('block',ref=blocker,other=attacker,value=int(blocked)))
  if legality_only:
   self.actions.append(step('remove_from_combat',ref=attacker))
   if blocked:self.actions.append(step('remove_from_combat',ref=blocker))
  self.actions += [step('advance_combat'),step('combat_damage')]
  if attack and not legality_only:
   if not blocked:
    damage=max(0,self.stat(attacker,'power'))*(2 if 'Double strike' in ak else 1);self.life[1-actor]-=damage
    if 'Lifelink' in ak:self.life[actor]+=damage
   else:
    # First and normal combat damage are simultaneous within each strike step.
    for first in (True,False):
     if a['zone']!='battlefield' or self.entities[blocker]['zone']!='battlefield':break
     b=self.entities[blocker];bk=self.keys(blocker);ap=max(0,self.stat(attacker,'power'));bp=max(0,self.stat(blocker,'power'));at=self.stat(attacker,'toughness');bt=self.stat(blocker,'toughness')
     ad=ap if (bool(ak&{'First strike','Double strike'}) if first else 'First strike' not in ak or 'Double strike' in ak) else 0
     bd=bp if (bool(bk&{'First strike','Double strike'}) if first else 'First strike' not in bk or 'Double strike' in bk) else 0
     assigned=min(ad,max(0,bt-b['damage'])) if 'Trample' in ak else ad
     if 'Trample' in ak:self.life[1-actor]-=ad-assigned
     b['damage']+=assigned;a['damage']+=bd
     if 'Lifelink' in ak:self.life[actor]+=ad
     if 'Lifelink' in bk:self.life[1-actor]+=bd
     if assigned and 'Deathtouch' in ak and 'Indestructible' not in bk:self.move(blocker,'graveyard')
     if bd and 'Deathtouch' in bk and 'Indestructible' not in ak:self.move(attacker,'graveyard')
     self.sba()
  self.actions.append(step('end_combat'));self.sba();self.actions+=self.checks()


def _functional(w,mode):
 from recipes import step,observe
 p=w.p;owner=w.owner;actor=1-owner if mode=='opponent_recipient' else owner;target=('nonmatch_' if mode=='nonmatching' else 'changeling_' if mode=='changeling' else 'match_')+str(actor)
 if mode=='source_recipient':target='source';actor=owner
 if p['kind'] in ('attack_permission','defender_permission','named_restriction'):
  if p['kind']=='attack_permission' and p['rule'] in ('you','power_le2') and mode not in ('own_attacker','source_recipient','foreign_recipient','foreign_destroyed'):actor=1-owner
  else:actor=1-owner if mode=='opponent_recipient' else owner
  target=('nonmatch_' if mode=='nonmatching' else 'match_')+str(actor)
  if mode=='source_recipient':target='source'
  if mode in ('blocked_receiver','block_countered'):
   w.combat('nonmatch_'+str(1-owner),'match_'+str(owner));return
  if mode=='late_arrival':
   target='late_'+str(actor);w.cast(target);w.cast('late_haste')
   for e in w.entities.values():
    if e['zone']=='battlefield' and e['player']==actor and 'Creature' in w.o[e['name']]['type_line']:e.setdefault('temporary_keys',[]).append('Haste')
  if mode=='pumped_attacker':w.cast('permission_pump_'+str(actor),target);w.entities[target]['extra']+=3;w.actions+=w.checks()
  w.combat(target);return
 if p['kind']=='flying_loss':
  target=('walker_' if mode=='islandwalk' else 'match_')+str(actor);w.combat(target,'blocker_'+str(1-actor),legality_only=mode=='islandwalk');return
 if p['kind']=='color':
  probe='color_probe_'+str(actor)
  if w.sources():w.actions.append(step('can_cast',ref=probe,value=0))
  else:
   target='match_'+str(1-actor);w.cast(probe,target);w.move(target,'graveyard');w.sba()
  w.actions+=w.checks();return
 if p['kind']=='entry_tapped':
  actor=owner if mode=='own_entry' else 1-owner;ref='entry_'+str(actor);w.cast(ref);e=w.entities[ref];w.actions+=w.checks()
  if e['name']=='Raging Goblin':w.combat(ref)
  if e['name'] in ('Sol Ring','Forest'):
   legal=not e['tapped'];w.actions += [step('activate',ref=ref,value=int(legal)),step('assert_stack',value=0),step('assert_choices_consumed')]
   if legal:e['tapped']=True;w.mana[actor][0 if e['name']=='Sol Ring' else 3]+=2 if e['name']=='Sol Ring' else 1
   w.actions+=w.checks()
  return
 if p['kind']=='grant':
  keyword=p['primary_keyword']
  if keyword=='Haste':
   if mode not in ('source_recipient','foreign_recipient','foreign_destroyed'):target=('late_nonmatch_' if mode=='nonmatching' else 'late_changeling_' if mode=='changeling' else 'late_')+str(actor);w.cast(target)
   w.combat(target)
   if p['predicate']=='all':
    elf='mana_elf_'+str(actor);w.cast(elf);active='Haste' in w.keys(elf);w.actions += [step('activate',ref=elf,value=int(active)),step('assert_stack',value=0),step('assert_choices_consumed')]
    if active:w.entities[elf]['tapped']=True;w.mana[actor][3]+=1
    w.actions+=w.checks()
  else:
   if mode=='late_arrival':
    target='late_'+str(actor);w.cast(target)
    if p.get('late_attack'):
     w.cast('late_haste')
     for e in w.entities.values():
      if e['zone']=='battlefield' and e['player']==actor and 'Creature' in w.o[e['name']]['type_line']:e.setdefault('temporary_keys',[]).append('Haste')
    else:
     w.combat(('flying_enemy_' if keyword=='Flying' else 'blocker_')+str(1-actor),target);return
   if keyword=='Trample':w.cast('combat_pump_'+str(actor),target);w.entities[target]['extra']+=3;w.actions+=w.checks()
   w.combat(target,'blocker_'+str(1-actor) if keyword in ('Flying','Trample','First strike') else None)
  return
 if p['kind']=='turn_stats' and mode in ('opposing_turn','turn_transition','damaged_turn_transition'):
  w.combat('blocker_'+str(1-owner),'match_'+str(owner))
  if mode in ('turn_transition','damaged_turn_transition'):
   if mode=='turn_transition':
    w.actions += [step('cleanup'),step('refresh')]
    for e in w.entities.values():e['damage']=0;e['extra']=0;e['temporary_keys']=[]
   w.turn=owner;w.actions += [step('turn',player=owner,value=0),step('refresh')];w.sba();w.actions+=w.checks()
  return
 if mode=='late_arrival':w.cast('late_'+str(actor));target='late_'+str(actor)
 if p['kind']=='block_permission':
  if mode=='source_blocker':w.combat('nonmatch_'+str(1-owner),'source')
  else:w.combat(target,('ground_blocker_' if mode=='ground_blocker' else 'blocker_')+str(1-actor))
 elif w.entities[target]['zone']=='battlefield' and not w.entities[target]['sick']:w.combat(target)


def _case(name,csvid,d,p,variant,rows,o):
 if p['schema']==3:return _case3(name,csvid,d,p,variant,rows,o)
 from recipes import step,cost_from_oracle
 mode,owner=variant.rsplit('_',1);owner=int(owner);w=_World(p,owner,rows,o,mode=='bare_zones');starting=mode in ('tapped_source','source_blocker') or mode=='source_recipient' and p['primary_keyword']!='Haste';cut=None;countered=mode in ('countered','block_countered')
 w.add(name,'source',owner,'battlefield' if starting else 'hand',tapped=mode=='tapped_source',source=True)
 if mode in ('two_sources','remove_one_source','opposing_source'):w.add(name,'second',1-owner if mode=='opposing_source' else owner,'hand',source=True)
 for pl in (0,1):
  w.add(p['matching'],'match_'+str(pl),pl,token=p['predicate']=='token',foreign=mode in ('foreign_recipient','foreign_destroyed') and pl==owner,sick=mode in ('foreign_recipient','foreign_destroyed') and pl==owner and p['primary_keyword']=='Haste');w.add(p['nonmatching'],'nonmatch_'+str(pl),pl,token=p.get('nonmatching_token',False));w.add('Mothdust Changeling','changeling_'+str(pl),pl)
  w.add(p.get('blocker') or ('Ornithopter' if p['primary_keyword']=='Trample' else 'Savannah Lions' if p['primary_keyword']=='First strike' else 'Hill Giant'),'blocker_'+str(pl),pl);
  if p.get('alternate_blocker'):w.add('Hill Giant','ground_blocker_'+str(pl),pl)
  w.add('Air Elemental','flying_enemy_'+str(pl),pl);w.add('Darksteel Relic','artifact_'+str(pl),pl);w.add('Forest','land_'+str(pl),pl,tapped=not (p['kind']=='entry_tapped' and mode=='entry_land' and pl==1-owner))
  w.add(p['matching'],'late_'+str(pl),pl,'hand')
  if p['primary_keyword']=='Haste':
   w.add(p['nonmatching'],'late_nonmatch_'+str(pl),pl,'hand');w.add('Mothdust Changeling','late_changeling_'+str(pl),pl,'hand')
  if p['primary_keyword']=='Haste' and p['predicate']=='all':w.add('Elvish Mystic','mana_elf_'+str(pl),pl,'hand')
  if p['kind']=='stats':w.add('Suntail Hawk','small_token_'+str(pl),pl,token=True)
 if p['kind']=='attack_permission' and mode=='pumped_attacker':
  for pl in (0,1):w.add('Giant Growth','permission_pump_'+str(pl),pl,'hand')
 if p['kind']=='color':
  for pl in (0,1):
   w.add('Doom Blade','color_probe_'+str(pl),pl,'hand');w.add('Sliver Construct','colored_artifact_'+str(pl),pl);w.add('Seal of Fire','red_control_'+str(pl),pl)
 if p['kind']=='flying_loss' and 'Islandwalk' in p['removed_keywords']:
  for pl in (0,1):w.add('River Boa','walker_'+str(pl),pl);w.add('Island','walk_land_'+str(pl),pl,tapped=True)
 if p['primary_keyword']=='Trample':
  for pl in (0,1):w.add('Giant Growth','combat_pump_'+str(pl),pl,'hand')
 if p.get('late_attack') and mode=='late_arrival':w.add('Burst of Speed','late_haste',1-owner if p['kind']=='attack_permission' and p['rule'] in ('you','power_le2') else owner,'hand')
 if p['kind']=='entry_tapped':
  en={'entry_artifact':'Sol Ring','entry_land':'Forest','entry_enchantment':'Seal of Fire','entry_artifact_creature':'Ornithopter'}.get(mode,'Raging Goblin')
  for pl in (0,1):w.add(en,'entry_'+str(pl),pl,'hand')
  if en=='Forest':w.add('Walking Atlas','atlas_'+str(1-owner),1-owner)
 if mode in ('source_bounced','bounce_recast'):w.add('Boomerang','response',owner,'hand')
 if countered:w.add('Counterspell','counter',0,'hand')
 if p['kind']=='named_restriction':assert not any(e['name']==p['witness_forbids'] for e in w.entities.values()),'named witness restriction must remain inactive in this bounded scene'
 cost=cost_from_oracle(o[name]);denied=mode in ('insufficient_mana','wrong_color')
 if mode in ('payment','insufficient_mana','wrong_color'):
  w.mana[owner]=list(cost) if mode!='insufficient_mana' else [0]*6
  if mode=='wrong_color':
   c=next(c for c in range(1,6) if cost[c]);w.mana[owner][0]+=w.mana[owner][c];w.mana[owner][c]=0
 pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=pl,color=c,value=n) for pl in (0,1) for c,n in enumerate(w.mana[pl])]+[step('refresh')];w.sba();w.actions+=w.checks()
 if not starting:
  w.actions.append(step('cast',ref='source',value=int(not denied)))
  if denied:w.actions += [step('assert_stack',value=0),step('assert_choices_consumed')]+w.checks()
  else:
   w.pay(name,owner);w.actions += [step('assert_stack',value=1),step('assert_card',ref='source',property='in_hand',value=0)]+w.checks()[2:];cut=len(w.actions)
   if countered:
    w.actions += [step('cast_response',ref='counter',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_stack',value=0),step('assert_choices_consumed')];w.pay('Counterspell',0)
    for ref in ('source','counter'):
     e=w.entities[ref];w.zones['hand'][e['player']].remove(e['name']);w.zones['graveyard'][e['owner']].append(e['name']);e['zone']='graveyard'
   else:
    w.actions += [step('resolve'),step('assert_choices_consumed'),step('assert_stack',value=0)];w.enter('source')
   w.actions += [step('refresh')]+w.checks()
 if not denied:
  if mode in ('two_sources','remove_one_source','opposing_source'):w.cast('second');w.actions+=w.checks()
  w.refill()
  if mode in ('source_bounced','source_destroyed','remove_one_source','bounce_recast'):
   state=w.remember();w.actions.append(step('capture'));_functional(w,'effect');w.actions.append(step('restore'));w.recall(state)
   if mode in ('source_bounced','bounce_recast'):
    w.cast('response','source');w.move('source','hand')
   else:w.actions.append(step('destroy',ref='source'));w.move('source','graveyard')
   w.sba();w.actions+=w.checks()
   if mode=='bounce_recast':
    w.actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'));w.cast('source');w.actions+=w.checks()
  _functional(w,mode)
  if mode=='foreign_destroyed':
   ref='match_'+str(owner);w.actions.append(step('destroy',ref=ref));w.move(ref,'graveyard');w.sba();w.actions+=w.checks()
  if mode=='cleanup':
   w.actions += [step('cleanup'),step('refresh')]
   for e in w.entities.values():e['damage']=0;e['extra']=0;e['temporary_keys']=[]
   w.sba();w.actions+=w.checks()
 checks=w.checks();seq=pre+w.actions+checks
 if mode in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
  at=0 if mode=='ready_snapshot' else cut if mode=='pending_snapshot' else len(w.actions);assert at is not None;tail=w.actions[at:]+checks;seq=pre+w.actions[:at]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
 return dict(name=f'StaticEffectExtensionRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps']))


"""Schema3 static conditions, type choice, reach, landwalk and shroud."""

def _variants3(p):
 modes=['effect','payment','insufficient_mana','countered','bare_zones','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot','source_bounced','source_destroyed','bounce_recast','late_arrival','opponent_recipient','foreign_recipient','foreign_destroyed']
 if p['predicate']!='all':modes+=['nonmatching']
 if any(p['mana'][1:]):modes+=['wrong_color']
 if not p['legendary']:modes+=['two_sources','remove_one_source','opposing_source']
 if p['creature']:modes+=['tapped_source','source_recipient']
 condition=p.get('condition')
 if condition in ('no_own_nonartifact_nonwhite','all_nonartifact_creatures_share_color'):modes+=['condition_off','condition_appears','condition_removed']
 if condition=='no_own_nonartifact_nonwhite':modes+=['condition_multicolor']
 if condition=='all_nonartifact_creatures_share_color':modes+=['condition_colorless','condition_overlap']
 if condition in ('own_grave_threshold','own_grave_threshold_extra'):modes+=['grave6','grave8','opponent_threshold','threshold_increase','threshold_decrease']
 if condition=='own_equipment_exists':modes+=['equipment_absent','equipment_opponent','equipment_appears','equipment_removed']
 if condition=='own_gate_count':modes+=['gates0','gates2','gates_opponent','gate_enters','gate_removed']
 if condition=='own_artifact_count_cda_plus_other_anthem':modes+=['artifact_enters','artifact_removed','artifact_token']
 if p['predicate']=='subtype':modes+=['changeling']
 if condition=='chosen_creature_type':modes+=['chosen_type_other','two_types']
 if p['primary_keyword'] in ('Forestwalk','Islandwalk'):modes+=['no_walk_land','other_walk_land']
 if 'Shroud' in p['granted_keywords']:modes+=['shroud']
 return [f'{v}_{owner}' for owner in (0,1) for v in modes]


class _World3(_World):
 def __init__(self,*args,**kw):super().__init__(*args,**kw);self.chosen={}
 def battlefield(self,player=None):return [(ref,e,self.o[e['name']]) for ref,e in self.entities.items() if e['zone']=='battlefield' and (player is None or e['player']==player)]
 def count_artifacts(self,player):return sum('Artifact' in r['type_line'].partition(' — ')[0].split() for _,_,r in self.battlefield(player))
 def count_gates(self,player):return sum('Land' in r['type_line'].partition(' — ')[0].split() and 'Gate' in r['type_line'].partition(' — ')[2].split() for _,_,r in self.battlefield(player))
 def condition_active(self,sr):
  player=self.entities[sr]['player'];condition=self.p.get('condition')
  if condition=='no_own_nonartifact_nonwhite':return not any('Creature' in r['type_line'] and 'Artifact' not in r['type_line'] and 'W' not in self.colors(ref) for ref,_,r in self.battlefield(player))
  if condition=='all_nonartifact_creatures_share_color':
   colors=[self.colors(ref) for ref,_,r in self.battlefield() if 'Creature' in r['type_line'] and 'Artifact' not in r['type_line']];return not colors or bool(set.intersection(*colors))
  if condition=='own_grave_threshold':return len(self.zones['graveyard'][player])>=7
  if condition=='own_equipment_exists':return any('Equipment' in r['type_line'].partition(' — ')[2].split() for _,_,r in self.battlefield(player))
  return True
 def matches(self,ref,sr):
  if self.p['kind']!='conditional_stats':return super().matches(ref,sr)
  e=self.entities[ref];s=self.entities[sr];r=self.o[e['name']];p=self.p
  if 'Creature' not in r['type_line'] or p['other'] and ref==sr or p['scope']=='own' and e['player']!=s['player'] or not self.condition_active(sr):return False
  pred=p['predicate'];value=self.chosen.get(sr,p['value'])
  return pred=='all' or pred=='nonartifact' and 'Artifact' not in r['type_line'] or pred=='artifact_creature' and 'Artifact' in r['type_line'] or pred=='color' and value in self.colors(ref) or pred=='subtype' and (value in r['type_line'].partition(' — ')[2].split() or 'Changeling' in r.get('keywords',[]))
 def stat(self,ref,field):
  if self.p['kind']!='conditional_stats':return super().stat(ref,field)
  e=self.entities[ref];p=self.p
  value=self.count_artifacts(e['player']) if p.get('base_formula')=='own_artifact_count_including_source' and e['source'] else int(self.o[e['name']][field]);value+=e['extra']
  for sr in self.sources():
   if self.matches(ref,sr):
    player=self.entities[sr]['player'];condition=p.get('condition')
    if condition=='own_gate_count':value+=self.count_gates(player) if field=='toughness' else 0
    else:
     value+=p[field]
     if condition=='own_grave_threshold_extra' and len(self.zones['graveyard'][player])>=7:value+=p['threshold_'+field]
  return value
 def enter(self,ref):
  super().enter(ref)
  if self.o[self.entities[ref]['name']]['oracle_text'].startswith('This land enters tapped.'):self.entities[ref]['tapped']=True
 def source_choices(self,ref):
  from recipes import step
  pl=self.entities[ref]['player'];value=self.chosen[ref];target=('nonmatch_' if value=='Cat' else 'match_')+str(pl)
  return [step('choose_dialog',player=pl,value=2),step('choose_target',player=pl,ref=target),step('choose_dialog',player=pl,value=0)]
 def cast(self,ref,target=None,dialog=None):
  from recipes import step
  e=self.entities[ref]
  if e['source'] and self.p.get('condition')=='chosen_creature_type':
   pl=e['player'];self.turn=pl;self.actions += [step('turn',player=pl,value=0),step('cast',ref=ref,value=1),step('assert_stack',value=1)];self.pay(e['name'],pl);self.actions+=self.source_choices(ref)+[step('resolve'),step('assert_choices_consumed'),step('assert_stack',value=0)];self.enter(ref)
  else:super().cast(ref,target,dialog)


def _functional3(w,mode):
 from recipes import step
 p=w.p;owner=w.owner;actor=1-owner if mode=='opponent_recipient' else owner;target=('nonmatch_' if mode in ('nonmatching','chosen_type_other') else 'changeling_' if mode=='changeling' else 'match_')+str(actor)
 if mode=='source_recipient':target='source'
 if mode=='late_arrival':
  target='late_'+str(actor);w.cast(target)
  if p['primary_keyword']!='Reach':
   w.cast('late_haste')
   for e in w.entities.values():
    if e['zone']=='battlefield' and e['player']==actor and 'Creature' in w.o[e['name']]['type_line']:e.setdefault('temporary_keys',[]).append('Haste')
  w.actions+=w.checks()
 if p.get('condition')=='chosen_creature_type':
  w.cast('untapper_'+str(actor),target,dialog=1);w.entities[target]['tapped']=False;w.actions+=w.checks()
 if p['primary_keyword']=='Reach':w.combat('air_'+str(1-actor),target);return
 if p['primary_keyword'] in ('Forestwalk','Islandwalk'):
  if 'Shroud' in p['granted_keywords']:
   for pl in (0,1):
    for ref in dict.fromkeys(['match_'+str(owner),'match_'+str(1-owner),'nonmatch_'+str(owner),'source',target]):
     if w.entities[ref]['zone']=='battlefield':w.actions.append(step('can_target_creature',ref='shroud_probe_'+str(pl),other=ref,value=int('Shroud' not in w.keys(ref))))
   if not w.sources():
    ref='match_'+str(1-actor);w.cast('shroud_probe_'+str(actor),ref);w.move(ref,'hand');w.actions+=w.checks()
   elif mode=='shroud':
    state=w.remember();w.actions.append(step('capture'));w.combat(target,'blocker_'+str(1-actor));w.actions.append(step('restore'));w.recall(state)
    w.cast('shroud_removal','source');w.move('source','hand');w.sba();w.actions+=w.checks()
    for pl in (0,1):
     for ref in ['match_'+str(owner),'match_'+str(1-owner)]:w.actions.append(step('can_target_creature',ref='shroud_probe_'+str(pl),other=ref,value=1))
    ref='match_'+str(1-actor);w.cast('shroud_probe_'+str(actor),ref);w.move(ref,'hand');w.actions+=w.checks()
  w.combat(target,'blocker_'+str(1-actor));return
 if p['primary_keyword']=='Vigilance':w.combat(target);return
 w.combat(target,'blocker_'+str(1-actor) if mode=='source_recipient' and 'Flying' in w.keys(target) else None)


def _case3(name,csvid,d,p,variant,rows,o):
 from recipes import step,cost_from_oracle
 mode,owner=variant.rsplit('_',1);owner=int(owner);condition=p.get('condition');starting=mode in ('tapped_source','source_recipient');w=_World3(p,owner,rows,o,mode=='bare_zones');countered=mode=='countered';cut=None
 w.add(name,'source',owner,'battlefield' if starting else 'hand',tapped=mode=='tapped_source',source=True)
 if mode in ('two_sources','remove_one_source','opposing_source','two_types'):w.add(name,'second',1-owner if mode=='opposing_source' else owner,'hand',source=True)
 if condition=='chosen_creature_type':
  w.chosen['source']='Cat' if mode=='chosen_type_other' else 'Bear';w.chosen['second']='Cat' if mode=='two_types' else w.chosen['source']
 common=condition=='all_nonartifact_creatures_share_color';angel=condition=='no_own_nonartifact_nonwhite';overlap=common and mode=='condition_overlap'
 for pl in (0,1):
  matching='Goblin Legionnaire' if overlap else p['matching'];nonmatching='Watchwolf' if overlap else 'Samite Healer' if angel else 'Sliver Construct' if common else p['nonmatching']
  w.add(matching,'match_'+str(pl),pl,foreign=mode in ('foreign_recipient','foreign_destroyed') and pl==owner);w.add(nonmatching,'nonmatch_'+str(pl),pl)
  w.add('Sliver Construct','artifact_creature_'+str(pl),pl);w.add('Darksteel Relic','artifact_'+str(pl),pl);w.add('Forest','land_'+str(pl),pl,tapped=True)
  if p['predicate']=='subtype':w.add('Mothdust Changeling','changeling_'+str(pl),pl)
  w.add(p['matching'],'late_'+str(pl),pl,'hand')
  if p['primary_keyword']=='Reach':w.add('Suntail Hawk','air_'+str(pl),pl)
  elif not (angel or common):w.add('Hill Giant','blocker_'+str(pl),pl)
  if p['primary_keyword'] in ('Forestwalk','Islandwalk'):
   walk='Forest' if p['primary_keyword']=='Forestwalk' else 'Island'
   # The ordinary Forest is replaced so no-walk controls are unambiguous.
   land='land_'+str(pl);item=next(x for x in w.setup if x['ref']==land);chosen='Plains' if mode=='no_walk_land' else ('Island' if walk=='Forest' else 'Forest') if mode=='other_walk_land' else walk;item['name']=chosen;w.entities[land]['name']=chosen
  if 'Shroud' in p['granted_keywords']:w.add('Unsummon','shroud_probe_'+str(pl),pl,'hand')
  if condition=='chosen_creature_type':w.add('Twiddle','untapper_'+str(pl),pl,'hand')
 if overlap:
  for pl in (0,1):w.add('Scab-Clan Mauler','third_color_'+str(pl),pl)
 if angel and mode=='condition_multicolor':w.add('Watchwolf','white_multicolor',owner)
 if angel:w.add('Grizzly Bears','opponent_nonwhite',1-owner)
 if angel or common:
  if mode in ('condition_off','condition_removed'):w.add('Grizzly Bears','condition_control',owner)
  if mode=='condition_appears':w.add('Grizzly Bears','condition_control',owner,'hand')
  if common and mode=='condition_colorless':w.add('Scion of Ugin','colorless_control',owner)
 if condition in ('own_grave_threshold','own_grave_threshold_extra'):
  amount=0 if mode=='bare_zones' else 6 if mode in ('grave6','threshold_increase','opponent_threshold') else 8 if mode=='grave8' else 7
  for pl in (0,1):
   desired=7 if mode=='opponent_threshold' and pl==1-owner else amount if pl==owner else len(w.zones['graveyard'][pl])
   for i in range(desired-len(w.zones['graveyard'][pl])):w.add('Plains',f'threshold_{pl}_{i}',pl,'graveyard')
  if mode=='threshold_increase':w.add('Savannah Lions','threshold_victim',owner)
  if mode=='threshold_decrease':w.add('Morningtide','grave_reset',owner,'hand')
 if condition=='own_equipment_exists':
  if mode not in ('equipment_absent','equipment_appears'):w.add('Bone Saw','equipment',1-owner if mode=='equipment_opponent' else owner)
  if mode=='equipment_appears':w.add('Bone Saw','equipment',owner,'hand')
 if condition=='own_gate_count':
  count=0 if mode in ('gates0','gates_opponent') else 2 if mode in ('gates2','gate_removed') else 1
  for pl in (0,1):
   for i in range(2 if mode=='gates_opponent' and pl==1-owner else count if pl==owner else 1):w.add('Selesnya Guildgate',f'gate_{pl}_{i}',pl,tapped=True)
  if mode=='gate_enters':w.add('Golgari Guildgate','new_gate',owner,'hand');w.add('Walking Atlas','atlas_'+str(owner),owner)
 if condition=='own_artifact_count_cda_plus_other_anthem':
  if mode=='artifact_enters':w.add('Darksteel Relic','new_artifact',owner,'hand')
  if mode=='artifact_token':w.add('Ornithopter','artifact_token',owner,token=True)
 if condition=='chosen_creature_type':
  # Exact printed6 normal payment: no creature is available to convoke.
  for ref,e in w.entities.items():
   if e['zone']=='battlefield' and 'Creature' in o[e['name']]['type_line']:
    e['tapped']=True;next(x for x in w.setup if x['ref']==ref)['state'][0]='tapped'
 if mode=='late_arrival' and p['primary_keyword']!='Reach':w.add('Burst of Speed','late_haste',owner,'hand')
 if mode in ('source_bounced','bounce_recast'):w.add('Boomerang','response',owner,'hand')
 if mode=='shroud':w.add('Boomerang','shroud_removal',1-owner,'hand')
 if countered:w.add('Counterspell','counter',0,'hand')
 cost=cost_from_oracle(o[name]);denied=mode in ('insufficient_mana','wrong_color')
 if mode in ('payment','insufficient_mana','wrong_color'):
  w.mana[owner]=list(cost) if mode!='insufficient_mana' else [0]*6
  if mode=='wrong_color':c=next(c for c in range(1,6) if cost[c]);w.mana[owner][0]+=w.mana[owner][c];w.mana[owner][c]=0
 pre=[step('turn',player=owner,value=0)]+[step('set_mana',player=pl,color=c,value=n) for pl in (0,1) for c,n in enumerate(w.mana[pl])]+[step('refresh')];w.sba();w.actions+=w.checks()
 if mode=='artifact_token':w.actions.append(step('assert_matching_permanents',player=owner,index=rows['Ornithopter']['csvid'],property='token',value=1))
 if not starting:
  w.actions.append(step('cast',ref='source',value=int(not denied)))
  if denied:w.actions += [step('assert_stack',value=0),step('assert_choices_consumed')]+w.checks()
  else:
   w.pay(name,owner);w.actions += [step('assert_stack',value=1),step('assert_card',ref='source',property='in_hand',value=0)]+w.checks()[2:];cut=len(w.actions)
   if countered:
    w.actions += [step('cast_response',ref='counter',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_stack',value=0),step('assert_choices_consumed')];w.pay('Counterspell',0)
    for ref in ('source','counter'):
     e=w.entities[ref];w.zones['hand'][e['player']].remove(e['name']);w.zones['graveyard'][e['owner']].append(e['name']);e['zone']='graveyard'
   else:
    if condition=='chosen_creature_type':w.actions+=w.source_choices('source')
    w.actions += [step('resolve'),step('assert_choices_consumed'),step('assert_stack',value=0)];w.enter('source')
   w.actions += [step('refresh')]+w.checks()
 if not denied:
  if mode in ('two_sources','remove_one_source','opposing_source','two_types'):w.cast('second');w.actions+=w.checks()
  w.refill()
  if mode=='condition_appears':w.cast('condition_control');w.sba();w.actions+=w.checks()
  if mode in ('condition_removed','threshold_increase','equipment_removed','gate_removed','artifact_removed'):
   target={'condition_removed':'condition_control','threshold_increase':'threshold_victim','equipment_removed':'equipment','gate_removed':f'gate_{owner}_0','artifact_removed':f'artifact_creature_{owner}'}[mode];w.actions.append(step('destroy',ref=target));w.move(target,'graveyard');w.sba();w.actions+=w.checks()
  if mode in ('equipment_appears','gate_enters','artifact_enters'):
   target={'equipment_appears':'equipment','gate_enters':'new_gate','artifact_enters':'new_artifact'}[mode];w.cast(target);w.sba();w.actions+=w.checks()
  if mode=='threshold_decrease':
   old=copy.deepcopy(w.zones['graveyard']);w.cast('grave_reset')
   for pl in (0,1):w.zones['exile'][pl].extend(old[pl]);w.zones['graveyard'][pl]=['Morningtide'] if pl==owner else []
   for ref,e in w.entities.items():
    if e['zone']=='graveyard' and ref!='grave_reset':e['zone']='exile'
   w.sba();w.actions+=w.checks()
  if mode in ('source_bounced','source_destroyed','remove_one_source','bounce_recast'):
   state=w.remember();w.actions.append(step('capture'));_functional3(w,'effect');w.actions.append(step('restore'));w.recall(state)
   if mode in ('source_bounced','bounce_recast'):w.cast('response','source');w.move('source','hand')
   else:w.actions.append(step('destroy',ref='source'));w.move('source','graveyard')
   w.sba();w.actions+=w.checks()
   if mode=='bounce_recast':w.actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'));w.cast('source');w.actions+=w.checks()
  _functional3(w,mode)
  if mode=='artifact_token':w.actions.append(step('destroy',ref='artifact_token'));w.move('artifact_token','graveyard');w.sba();w.actions+=w.checks()
  if mode=='foreign_destroyed':
   ref='match_'+str(owner)
   if w.entities[ref]['zone']=='battlefield':w.actions.append(step('destroy',ref=ref));w.move(ref,'graveyard');w.sba();w.actions+=w.checks()
  if mode=='cleanup':
   w.actions += [step('cleanup'),step('refresh')]
   for e in w.entities.values():e['damage']=0;e['extra']=0;e['temporary_keys']=[]
   w.sba();w.actions+=w.checks()
 checks=w.checks();seq=pre+w.actions+checks
 if mode in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
  at=0 if mode=='ready_snapshot' else cut if mode=='pending_snapshot' else len(w.actions);assert at is not None;tail=w.actions[at:]+checks;seq=pre+w.actions[:at]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
 return dict(name=f'StaticEffectExtensionRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps']))
