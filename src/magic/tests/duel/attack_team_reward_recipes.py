"""Whole-Oracle attack-team scopes, temporary rewards and native combat clauses."""
import copy,json,re
from pathlib import Path
from battalion_trigger_recipes import _BattalionWorld

FAMILY='attack_team_reward'
PASSIVES={
 'Flying':('Flying',0),
 "Shadow (This creature can block or be blocked by only creatures with shadow.)":('Shadow',0),
 'Bushido 1 (Whenever this creature blocks or becomes blocked, it gets +1/+1 until end of turn.)':('Bushido',1),
 'Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)':('Flanking',0),
}

def _profiles():return json.loads((Path(__file__).parent/'data/attack_team_reward_wave8_profiles.json').read_text(encoding='utf8'))

def classify(r):
 from recipes import cost_from_oracle
 if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or 'Creature — ' not in r.get('type_line','') or r['type_line'].startswith('Token ')):
  raise ValueError('whole normal standalone source-attack creature required')
 lines=r.get('oracle_text','').splitlines();trigger=[];keys=[];bushido=0
 refs='(?:'+ '|'.join(re.escape(x) for x in sorted({'this creature',r['name'],r['name'].split(',')[0]}))+')'
 for line in lines:
  m=re.fullmatch(r'Whenever '+refs+r' attacks, (.+)',line)
  if m:trigger.append(m[1])
  elif line in PASSIVES:kw,n=PASSIVES[line];keys.append(kw);bushido+=n
  else:raise ValueError('active/passive source clause needs independent receipts')
 if len(trigger)!=1 or len(keys)!=len(set(keys)) or set(keys)!=set(r.get('keywords',[])):raise ValueError('one complete trigger and exact source passive keywords required')
 body=trigger[0];p=dict(scope='attacking',other=False,subtype='',required_keyword='',power=0,toughness=0,grants=[],keywords=sorted(keys),bushido=bushido)
 gain=re.fullmatch(r'attacking creatures gain (first strike|trample|flying and lifelink) until end of turn\.',body)
 pump=re.fullmatch(r'([A-Z][A-Za-z-]+) creatures you control get \+([0-3])/\+([0-3]) until end of turn\.',body)
 others=re.fullmatch(r'other creatures you control get \+([0-3])/\+([0-3]) until end of turn\.',body)
 flank=re.fullmatch(r'all attacking creatures with flanking get \+([0-3])/\+([0-3]) until end of turn\.',body)
 if gain:p['grants']={'first strike':['First strike'],'trample':['Trample'],'flying and lifelink':['Flying','Lifelink']}[gain[1]]
 elif body=="each other attacking creature gains fear until end of turn. (They can't be blocked except by artifact creatures and/or black creatures.)":p.update(other=True,grants=['Fear'])
 elif pump:p.update(scope='controlled',subtype=pump[1],power=int(pump[2]),toughness=int(pump[3]))
 elif others:p.update(scope='controlled',other=True,power=int(others[1]),toughness=int(others[2]))
 elif flank:p.update(required_keyword='Flanking',power=int(flank[1]),toughness=int(flank[2]))
 else:raise ValueError('complete typed team reward required')
 if p['subtype'] not in ('','Human','Samurai'):raise ValueError('independent complete subtype witness set required')
 for field in ('power','toughness'):
  if not re.fullmatch(r'[0-9]+',r.get(field,'')) or not 0<int(r[field])<=12:raise ValueError('bounded numeric source characteristics required')
 cost_from_oracle(r);return p

def spec(d,r):
 from recipes import cost_from_oracle
 if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not r or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('whole pinned version2 team declaration required')
 p=d.get('params');expected=classify(r)
 if (type(p) is not dict or p!=expected or set(p)!=set(expected) or any(type(p[k]) is not int for k in ('power','toughness','bushido')) or type(p['other']) is not bool
     or any(type(p[k]) is not str for k in ('scope','subtype','required_keyword')) or any(type(p[k]) is not list or any(type(x) is not str for x in p[k]) for k in ('grants','keywords'))):raise ValueError('exact typed full-Oracle team parameters required')
 return dict(copy.deepcopy(p),family=FAMILY,effect='team_reward',entry_energy=0,mana=cost_from_oracle(r),source_power=int(r['power']),source_toughness=int(r['toughness']),legendary='Legendary' in r['type_line'])

def variants(p):
 values=['entry','insufficient_mana','wrong_color','fresh_attack','effect','source_alone','only_others','without_source_combat','zero_attackers','tapped_source','source_absent','opponent_attack','source_destroyed_after','ally_destroyed_after','unblocked','blocked','granted_combat','cleanup','repeat','late_entry','expiry','ready_snapshot','declaration_snapshot','resolved_snapshot','bare_zones']
 if not p['legendary']:values+=['two_sources']
 if 'Trample' in p['grants']:values+=['trample_spill']
 if 'Fear' in p['grants']:values+=['fear_ground_denied','fear_black','fear_artifact','fear_source_excluded']
 if 'Flying' in p['grants']:values+=['flying_ground_denied','flying_reach','flying_blocker','prevented_combat','nonattacker_lifelink','expiry_flying']
 if 'Shadow' in p['keywords']:values+=['shadow_block','shadow_ground_denied','shadow_defend','source_blocks']
 if p['bushido']:values+=['bushido_attacker','bushido_defender','bushido_unblocked']
 if 'Flanking' in p['keywords']:values+=['flanking_penalty','flanking_immune','flanking_defender']
 return [f'{v}_{owner}' for owner in (0,1) for v in values]+['cast_cancel_0']

def validate_witnesses(o,rows):
 for name,pin in _profiles()['witnesses'].items():
  if o.get(name)!=pin['oracle'] or name not in rows or rows[name]['handler']!=pin['handler'] or rows[name].get('kind')!='card':raise ValueError('whole team witness or binding changed: '+name)
  if rows[name]['handler']=='NULL' or rows[name]['handler']=='card_dummy' and not pin.get('reviewed_native_witness'):raise ValueError('unimplemented/unreviewed team witness: '+name)
  if o[name].get('layout')!='normal' or o[name].get('card_faces') or o[name].get('selected_face'):raise ValueError('standalone complete witness required')

class _TeamWorld(_BattalionWorld):
 def cast_entity(self,ref):
  e=self.entities[ref];self.cast_card(e['name'],ref,e['player'],'battlefield');self.entities[ref]['zone']='battlefield';self.entities[ref]['sick']=not self.has(ref,'Haste');self.receipts()
 def matches(self,ref,source):
  e=self.entities[ref];p=self.p
  if e['zone']!='battlefield' or p['other'] and ref==source:return False
  if p['scope']=='attacking' and not e['attacking']:return False
  if p['scope']=='controlled' and e['player']!=self.entities[source]['player']:return False
  if p['subtype'] and p['subtype'] not in self.o[e['name']]['type_line'].split(' — ')[-1].split():return False
  return not p['required_keyword'] or self.has(ref,p['required_keyword'])
 def declaration(self,chosen):
  self.emit('begin_attackers');attackers=[]
  for ref in chosen:
   e=self.entities[ref];legal=e['zone']=='battlefield' and e['player']==self.current and not e['sick'] and not e['tapped']
   self.emit('attack',ref=ref,value=int(legal))
   if legal:attackers.append(ref);e['attacking']=True;e['tapped']=not self.has(ref,'Vigilance')
  self.receipts();cut=len(self.steps);self.emit('finish_attackers');self.emit('assert_choices_consumed')
  for source in [r for r in attackers if self.entities[r]['source']]:
   for ref,e in self.entities.items():
    if self.matches(ref,source):e['power']+=self.p['power'];e['toughness']+=self.p['toughness'];e['grants'].update(self.p['grants'])
  self.receipts();return attackers,cut
 def block_legal(self,b,a):
  d=self.entities[b];e=self.entities[a]
  return (d['zone']=='battlefield' and e['zone']=='battlefield' and d['player']!=e['player'] and not d['tapped']
   and self.has(a,'Shadow')==self.has(b,'Shadow')
   and (not self.has(a,'Flying') or self.has(b,'Flying') or self.has(b,'Reach'))
   and (not self.has(a,'Fear') or 'Artifact' in self.o[d['name']]['type_line'] or 'B' in self.o[d['name']]['colors']))
 def bushido(self,ref):
  m=re.search(r'Bushido ([1-9]) ',self.o[self.entities[ref]['name']].get('oracle_text',''));return int(m[1]) if m else 0
 def phase_damage(self,attackers,blocked,first):
  packets=[]
  def acts(ref):return self.entities[ref]['zone']=='battlefield' and (self.has(ref,'First strike') if first else not self.has(ref,'First strike'))
  for a in attackers:
   if self.entities[a]['zone']!='battlefield':continue
   b=blocked.get(a);live=b is not None and self.entities[b]['zone']=='battlefield'
   if acts(a):
    ap=self.entities[a]['power']
    if b is None:packets.append((a,1-self.current,ap,True))
    elif self.has(a,'Trample'):
     required=max(0,self.entities[b]['toughness']-self.entities[b]['damage']) if live else 0
     if live:packets.append((a,b,min(ap,required),False))
     if ap>required:packets.append((a,1-self.current,ap-required,True))
    elif live:packets.append((a,b,ap,False))
   if live and acts(b):packets.append((b,a,self.entities[b]['power'],False))
  # Compute the simultaneous packet set before any state-based deaths.
  for source,target,amount,player in packets:self.deal(source,target,amount,player=player)
 def combat(self,attackers,pairs=()):
  self.emit('begin_blockers');blocked={}
  for b,a in pairs:
   legal=self.block_legal(b,a);self.emit('block',ref=b,other=a,value=int(legal))
   if legal:blocked[a]=b;self.emit('assert_card',ref=b,property='blocking_count',value=1)
  self.emit('advance_combat')
  for a,b in blocked.items():
   if self.has(a,'Flanking') and not self.has(b,'Flanking'):
    self.entities[b]['power']-=1;self.entities[b]['toughness']-=1
    if self.entities[b]['toughness']<=0:self.leave(b,'graveyard')
   for ref in (a,b):
    if self.entities[ref]['zone']=='battlefield':n=self.bushido(ref);self.entities[ref]['power']+=n;self.entities[ref]['toughness']+=n
  self.receipts();self.emit('combat_damage');self.phase_damage(attackers,blocked,True);self.phase_damage(attackers,blocked,False);self.emit('end_combat')
  for e in self.entities.values():e['attacking']=False
  self.receipts()
 def tim_damage(self):
  self.emit('require_can_activate',ref='tim',value=1);self.emit('choose_player',player=self.owner,value=1-self.owner);self.emit('activate',ref='tim',value=1);self.emit('assert_stack',value=1);self.emit('resolve');self.emit('assert_choices_consumed');self.entities['tim']['tapped']=True;self.deal('tim',1-self.owner,1,player=True);self.receipts()

def expand(name,csvid,d,p,selected,rows,o):
 from recipes import step
 if not selected or len(set(selected))!=len(selected) or any(v not in variants(p) for v in selected):raise ValueError('explicit unique full-Oracle team variants required')
 validate_witnesses(o,rows);out=[]
 for variant in selected:
  value,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner;w=_TeamWorld(name,p,owner,rows,o)
  w.add(name,'source',owner,'hand',source=True)
  if value=='two_sources':w.add(name,'copy',owner,'hand',source=True)
  w.add('Grizzly Bears','ally',owner);w.add('Grizzly Bears','bench',owner);w.add('Grizzly Bears','enemy',enemy);w.add('Hill Giant','giant',enemy)
  crew='Devoted Retainer' if p['subtype']=='Samurai' else 'Benalish Cavalry' if p['required_keyword']=='Flanking' else 'Glory Seeker'
  zone='hand' if crew=='Devoted Retainer' else 'battlefield'
  w.add(crew,'crew',owner,zone);w.add(crew,'bench_match',owner,zone);w.add(crew,'other_match',enemy,zone)
  if 'Trample' in p['grants']:w.add("Mons's Goblin Raiders",'small',enemy)
  if 'Fear' in p['grants']:w.add('Walking Corpse','black',enemy);w.add('Memnite','artifact',enemy)
  if 'Flying' in p['keywords'] or 'Flying' in p['grants'] or value=='expiry':w.add('Giant Spider','reach',enemy);w.add('Air Elemental','flying',enemy)
  if 'Shadow' in p['keywords']:w.add('Soltari Foot Soldier','enemy_shadow',enemy)
  if value=='late_entry':w.add(crew,'late',owner,'hand')
  if value=='nonattacker_lifelink':w.add('Prodigal Sorcerer','tim',owner)
  if value in ('tapped_source','expiry_flying'):w.add('Twiddle','twiddle',owner,'hand')
  if value=='expiry_flying':w.add('Twiddle','source_untap',owner,'hand')
  if value=='prevented_combat':w.add('Safe Passage','shield',enemy,'hand')
  if value=='cast_cancel':
   for color,count in enumerate(p['mana']):
    for index in range(count):w.add(('Forest','Swamp','Island','Forest','Mountain','Plains')[color],f'payment_land_{color}_{index}',owner)
  if value!='bare_zones':
   for pl in (0,1):
    w.add('Forest',f'hand_{pl}',pl,'hand');w.add('Lightning Bolt',f'grave_{pl}',pl,'graveyard');w.add('Island',f'exile_{pl}',pl,'exile')
    for index,nm in enumerate(('Forest','Island','Plains','Swamp','Mountain','Grizzly Bears')):w.add(nm,f'library_{pl}_{index}',pl,'library')
  start=enemy if crew=='Devoted Retainer' else owner;w.current=start;w.emit('turn',player=start,value=0);w.pool(0,[0]*6);w.pool(1,[0]*6);w.emit('refresh');w.receipts()
  if crew=='Devoted Retainer':w.cast_entity('other_match');w.next_turn()
  if value=='cast_cancel':
   lands=[c['ref'] for c in w.setup if c['ref'].startswith('payment_land_')]
   for ref in lands:w.emit('require_can_activate',ref=ref,value=1);w.emit('assert_card',ref=ref,property='tapped',value=0)
   w.emit('choose_cancel_mana',player=0);w.emit('cast',ref='source',value=0);w.emit('assert_unused_choices',value=0)
   for ref in lands:w.emit('assert_card',ref=ref,property='tapped',value=0)
   w.receipts()
  else:w.cast_source('source',value if value in ('insufficient_mana','wrong_color') else None)
  ready_cut=declaration_cut=0
  if value not in ('entry','insufficient_mana','wrong_color','cast_cancel'):
   if crew=='Devoted Retainer':w.cast_entity('crew');w.cast_entity('bench_match')
   if value=='two_sources':w.cast_source('copy')
   if value!='fresh_attack':w.age()
   if value in ('opponent_attack','source_blocks','shadow_defend','bushido_defender','flanking_defender'):w.next_turn()
   if value=='tapped_source':w.spell('Twiddle','twiddle',owner,'source',dialog=0);w.entities['source']['tapped']=True;w.receipts()
   if value=='source_absent':w.emit('destroy',ref='source');w.leave('source','graveyard');w.receipts()
   chosen=[] if value=='zero_attackers' else ['source'] if value=='source_alone' else ['ally','crew'] if value in ('only_others','without_source_combat','source_absent') else ['enemy_shadow'] if value=='shadow_defend' else ['enemy'] if value in ('opponent_attack','source_blocks','bushido_defender','flanking_defender') else ['source','copy','ally'] if value=='two_sources' else ['source','ally'] if value in ('fresh_attack','trample_spill') else ['source','ally','crew']
   ready_cut=len(w.steps);attackers,declaration_cut=w.declaration(chosen)
   if value in ('source_destroyed_after','ally_destroyed_after'):
    ref='source' if value=='source_destroyed_after' else 'ally';w.emit('destroy',ref=ref);w.leave(ref,'graveyard');w.receipts()
   if value=='nonattacker_lifelink':w.tim_damage()
   if value=='prevented_combat':w.cast_card('Safe Passage','shield',enemy);w.prevented.add(enemy);w.receipts()
   fight=value in ('unblocked','blocked','granted_combat','without_source_combat','source_destroyed_after','ally_destroyed_after','cleanup','repeat','late_entry','expiry','trample_spill','fear_ground_denied','fear_black','fear_artifact','fear_source_excluded','flying_ground_denied','flying_reach','flying_blocker','prevented_combat','nonattacker_lifelink','expiry_flying','shadow_block','shadow_ground_denied','shadow_defend','source_blocks','bushido_attacker','bushido_defender','bushido_unblocked','flanking_penalty','flanking_immune','flanking_defender')
   if fight:
    pairs=[('enemy','source')] if value in ('blocked','fear_source_excluded','shadow_ground_denied','bushido_attacker') else [('enemy','ally')] if value in ('granted_combat','fear_ground_denied','flying_ground_denied') else [('black','ally')] if value=='fear_black' else [('artifact','ally')] if value=='fear_artifact' else [('reach','ally')] if value=='flying_reach' else [('flying','ally')] if value=='flying_blocker' else [('source','enemy_shadow')] if value=='shadow_defend' else [('source','enemy')] if value in ('source_blocks','bushido_defender','flanking_defender') else [('enemy_shadow','source')] if value=='shadow_block' else [('giant','source')] if value=='flanking_penalty' else [('other_match','source')] if value=='flanking_immune' else [('enemy','source'),('small','ally')] if value=='trample_spill' else []
    if value in ('without_source_combat','source_destroyed_after'):pairs=[('small' if 'Trample' in p['grants'] else 'enemy','ally')]
    if value=='ally_destroyed_after':pairs=[('enemy','source')]
    w.combat(attackers,pairs)
    if value=='late_entry':w.cast_entity('late')
    if value in ('cleanup','expiry','expiry_flying'):w.emit('cleanup');w.expire();w.receipts()
    if value in ('repeat','expiry'):
     w.age();attackers,_=w.declaration(['source','ally','crew'] if value=='repeat' else ['ally']);w.combat(attackers,[('enemy','ally')] if value=='expiry' else [])
    if value=='expiry_flying':
     w.spell('Twiddle','twiddle',owner,'ally',dialog=1);w.entities['ally']['tapped']=False;w.receipts()
     w.spell('Twiddle','source_untap',owner,'source',dialog=1);w.entities['source']['tapped']=False;w.receipts()
     w.next_turn();attackers,_=w.declaration(['flying']);w.combat(attackers,[('ally','flying'),('source','flying')])
  w.receipts();seq=w.steps
  if value in ('ready_snapshot','declaration_snapshot'):
   cut=ready_cut if value=='ready_snapshot' else declaration_cut;tail=seq[cut:];seq=seq[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if value=='resolved_snapshot':
   begin=len(w.steps);w.receipts();checks=w.steps[begin:];seq=w.steps[:begin]+[step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+checks+[step('assert_recorded_state')]
  out.append(dict(name=f'AttackTeamRewardRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['responses between attacker selection and inline reward resolution','multiplayer or attack-group control changes','legend-rule duplicate selection on legendary sources','direct keyword-bit introspection on noncombatants; measured combat/lifelink/late-entry scope receipts used instead','beginning-of-combat sources excluded; no fabricated phase hook','AI payment cancellation policy; only human source-mana cancellation authored'])))
 return out
