"""Oracle-derived Battalion declarations and actual reward/combat receipts."""
import copy
import json
from pathlib import Path
import re

from attack_value_trigger_recipes import _AttackResourceWorld

FAMILY='battalion_trigger'


def _profiles():
 return json.loads((Path(__file__).parent/'data/battalion_trigger_wave8_profiles.json').read_text(encoding='utf8'))


def classify(r):
 from creature_recipes import oracle_keywords
 from recipes import cost_from_oracle
 if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
     or 'Creature' not in r.get('type_line','') or r['type_line'].startswith('Token ')):
  raise ValueError('whole normal standalone Battalion creature required')
 lines=r.get('oracle_text','').splitlines();clauses=[s for s in lines if s.startswith('Battalion — ')]
 if len(clauses)!=1:raise ValueError('one complete Battalion clause required')
 refs={'this creature',r['name'],r['name'].split(',')[0]};source='(?:'+'|'.join(re.escape(x) for x in sorted(refs))+')'
 match=re.fullmatch(r'Battalion — Whenever '+source+r' and at least two other creatures attack, (.+)',clauses[0])
 if not match:raise ValueError('printed source plus two other declared attackers required')
 body=match[1];kind=None;power=toughness=damage=gain=0;untap=False;keyword=''
 pump=re.fullmatch(source+r' gets \+([1-5])/\+([0-5]) until end of turn\.( Untap it\.)?',body)
 keys=re.fullmatch(source+r' gains (flying|lifelink|first strike|double strike) until end of turn\.(.*)',body)
 hit=re.fullmatch(source+r' deals ([1-9]) damage to any target(?: and you gain ([1-9]) life)?\.',body)
 if pump:kind='self_pump';power=int(pump[1]);toughness=int(pump[2]);untap=bool(pump[3])
 elif keys:
  reminders={'lifelink':' (Damage dealt by a creature with lifelink also causes its controller to gain that much life.)','double strike':' (It deals both first-strike and regular combat damage.)'}
  if keys[2] not in ('',reminders.get(keys[1],'')):raise ValueError('whole reviewed temporary-keyword reminder required')
  kind='self_keyword';keyword=keys[1]
 elif hit:kind='target_damage';damage=int(hit[1]);gain=int(hit[2] or 0)
 elif body=="target creature can't block this turn.":kind='target_cannot_block'
 if kind is None:raise ValueError('retain compound Battalion reward for adjacent receipt delta')
 stripped=dict(r,oracle_text='\n'.join(s for s in lines if s!=clauses[0]),keywords=[k for k in r.get('keywords',[]) if k!='Battalion'])
 passive=oracle_keywords(stripped)
 if set(passive)-{'Flying'}:raise ValueError('source passive needs independent whole-card receipt extension')
 cost_from_oracle(r)
 return dict(threshold=3,reward=kind,power=power,toughness=toughness,untap=untap,keyword=keyword,damage=damage,gain_life=gain,keywords=passive)


def spec(d,r):
 from recipes import cost_from_oracle
 if (d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2
     or not r or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
  raise ValueError('whole pinned version2 Battalion declaration required')
 expected=classify(r);p=d.get('params')
 if (type(p) is not dict or p!=expected or set(p)!=set(expected)
     or any(type(p[k]) is not int for k in ('threshold','power','toughness','damage','gain_life'))
     or type(p['untap']) is not bool or type(p['reward']) is not str or type(p['keyword']) is not str
     or type(p['keywords']) is not list or any(type(k) is not str for k in p['keywords'])):
  raise ValueError('exact typed parameters must derive from the complete Oracle clause')
 return dict(copy.deepcopy(p),family=FAMILY,effect='battalion_reward',entry_energy=0,mana=cost_from_oracle(r),
             legendary='Legendary' in r['type_line'],source_power=int(r['power']),source_toughness=int(r['toughness']))


def variants(p):
 values=['entry','insufficient_mana','wrong_color','fresh_attack','effect','zero_attackers','one_attacker','two_attackers',
         'three_without_source','four_attackers','tapped_source','source_absent','opponent_attack','unblocked','blocked',
         'cleanup','expiry','source_destroyed_after','two_sources','token_crew','ready_snapshot','declaration_snapshot','resolved_snapshot','bare_zones']
 if p['reward']=='target_damage':values+=['target_creature','target_own_creature','target_self','target_own_player','target_planeswalker','prevented_damage','mandatory_cancel']
 if p['reward']=='target_cannot_block':values+=['target_own','target_self','mandatory_cancel']
 if p['keyword']=='flying':values+=['reach_block','flying_block']
 if p['keyword']=='lifelink':values+=['prevented_combat']
 return [v+'_'+str(owner) for owner in (0,1) for v in values if not (v=='two_sources' and p['legendary'])]+['cast_cancel_0']


def validate_witnesses(o,rows):
 profiles=_profiles()
 for nm,pin in profiles['witnesses'].items():
  r=o.get(nm,{})
  if r!=pin['oracle'] or nm not in rows or rows[nm]['handler']!=pin['handler'] or rows[nm].get('kind')!='card':
   raise ValueError('independent whole Oracle/binding witness changed: '+nm)
  if rows[nm]['handler']=='NULL':raise ValueError('unimplemented witness: '+nm)
  if rows[nm]['handler']=='card_dummy' and not pin.get('reviewed_native_witness'):
   raise ValueError('unreviewed dummy witness: '+nm)
  if r.get('layout') not in ('normal','token') or r.get('card_faces') or r.get('selected_face'):
   raise ValueError('standalone normal or independently pinned token witness required')


class _BattalionWorld(_AttackResourceWorld):
 def __init__(self,name,p,owner,rows,o):
  super().__init__(name,p,owner,rows,o);self.prevented=set();self.walkers={}
 def receipts(self):
  super().receipts()
  for ref,e in self.walkers.items():
   if e['zone']=='battlefield':self.emit('assert_card',ref=ref,property='in_play',value=1);self.emit('assert_counter',ref=ref,counter_kind='loyalty',value=e['loyalty'])
  if any(self.entities[t]['zone']=='battlefield' for t in self.tokens):
   for prop,value in (('token',1),('artifact',1),('color',0),('subtype_Servo',1)):
    self.emit('assert_matching_permanents',player=self.owner,index=self.rows['Servo']['csvid'],property=prop,value=value)
 def expire(self):
  super().expire();self.prevented=set()
  for e in self.entities.values():e['power']=int(self.o[e['name']]['power']);e['toughness']=int(self.o[e['name']]['toughness'])
 def cast_card(self,nm,ref,pl,zone='graveyard',dialog=None,target=None):
  from recipes import cost_from_oracle
  self.pool(pl,cost_from_oracle(self.o[nm]))
  if target:self.emit('choose_target',player=pl,ref=target)
  self.emit('cast',ref=ref,value=1);self.emit('assert_stack',value=1)
  if dialog is not None:self.emit('choose_dialog',player=pl,value=dialog)
  self.emit('resolve');self.emit('assert_choices_consumed');self.mana[pl]=[0]*6
  self.zones['hand'][pl].remove(nm);self.zones[zone][pl].append(nm)
 def cast_walker(self,ref):
  e=self.walkers[ref];self.cast_card(e['name'],ref,e['player'],'battlefield');e['zone']='battlefield';e['loyalty']=int(self.o[e['name']]['loyalty']);self.receipts()
 def cast_servos(self):
  self.cast_card('Servo Exhibition','servo_spell',self.owner)
  for _ in range(2):
   ref=f'servo_{len(self.tokens)}';self.tokens.append(ref);self.entity('Servo',ref,self.owner,'battlefield',token=True);self.zones['battlefield'][self.owner].append('Servo')
  self.receipts()
 def deal(self,source,target,amount,player=False):
  receiver=target if player else self.entities[target]['player']
  actual=0 if receiver in self.prevented else amount
  if player:self.life[receiver]-=actual
  else:
   e=self.entities[target];e['damage']+=actual
   if e['damage']>=e['toughness'] and not self.has(target,'indestructible'):self.leave(target,'graveyard')
  if self.has(source,'Lifelink'):self.life[self.entities[source]['player']]+=actual
 def hit_walker(self,ref,amount):
  e=self.walkers[ref];e['loyalty']-=amount
  if e['loyalty']<=0:
   self.zones['battlefield'][e['player']].remove(e['name']);self.zones['graveyard'][e['player']].append(e['name']);e['zone']='graveyard'
 def declaration(self,chosen,target='enemy',target_player=None,cancel=False):
  self.emit('begin_attackers');attackers=[]
  for ref in chosen:
   e=self.entities[ref];legal=e['zone']=='battlefield' and e['player']==self.current and not e['sick'] and not e['tapped']
   self.emit('attack',ref=ref,value=int(legal))
   if legal:attackers.append(ref);e['attacking']=True;e['tapped']=not self.has(ref,'Vigilance')
  self.receipts();cut=len(self.steps)
  active=[ref for ref in attackers if self.entities[ref]['source'] and len(attackers)>=self.p['threshold']]
  # Queue only required inputs. No gameplay action/priority window is inserted
  # between declaration selection and the inline finish_attackers dispatch.
  for ref in active:
   if self.p['reward'] in ('target_damage','target_cannot_block'):
    if cancel:self.emit('choose_optional_target',player=self.owner,ref=target)
    elif target_player is not None:self.emit('choose_player',player=self.owner,value=target_player)
    else:self.emit('choose_target',player=self.owner,ref=target)
  self.emit('finish_attackers');self.emit('assert_choices_consumed')
  if cancel:self.emit('assert_forced_target_choices',value=len(active))
  for ref in active:
   e=self.entities[ref]
   if self.p['reward']=='self_pump':
    e['power']+=self.p['power'];e['toughness']+=self.p['toughness']
    if self.p['untap']:e['tapped']=False
   elif self.p['reward']=='self_keyword':e['grants'].add(self.p['keyword'])
   elif self.p['reward']=='target_cannot_block':self.entities[target]['grants'].add('cannot_block')
   elif target in self.walkers and target_player is None:self.hit_walker(target,self.p['damage']);self.life[self.owner]+=self.p['gain_life']
   else:self.deal(ref,target_player if target_player is not None else target,self.p['damage'],player=target_player is not None);self.life[self.owner]+=self.p['gain_life']
  self.receipts();return [ref for ref in attackers if self.entities[ref]['zone']=='battlefield'],cut
 def combat(self,attackers,pairs=()):
  self.emit('begin_blockers');blocked={}
  for b,a in pairs:
   defender=self.entities[b];attacker=self.entities[a]
   legal=defender['zone']=='battlefield' and attacker['zone']=='battlefield' and defender['player']!=attacker['player'] and not defender['tapped'] and not self.has(b,'cannot_block') and (not self.has(a,'Flying') or self.has(b,'Flying') or self.has(b,'Reach'))
   self.emit('block',ref=b,other=a,value=int(legal))
   if legal:blocked[a]=b;self.emit('assert_card',ref=b,property='blocking_count',value=1)
  self.emit('advance_combat');self.emit('combat_damage');self.emit('end_combat')
  for a in attackers:
   attacker=self.entities[a]
   if attacker['zone']!='battlefield':continue
   ap=attacker['power'];double=self.has(a,'Double strike');first=double or self.has(a,'First strike')
   if a not in blocked:self.deal(a,1-self.current,ap*(2 if double else 1),player=True);continue
   b=blocked[a];bp=self.entities[b]['power']
   if first:
    self.deal(a,b,ap)
    if self.entities[b]['zone']=='battlefield':
     if double:self.deal(a,b,ap)
     self.deal(b,a,bp)
   else:self.deal(a,b,ap);self.deal(b,a,bp)
  for e in self.entities.values():e['attacking']=False
  self.receipts()


def expand(name,csvid,d,p,selected,rows,o):
 from recipes import step
 if not selected or len(set(selected))!=len(selected) or any(v not in variants(p) for v in selected):raise ValueError('explicit unique Battalion variants required')
 validate_witnesses(o,rows);out=[]
 for variant in selected:
  value,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner;w=_BattalionWorld(name,p,owner,rows,o)
  w.add(name,'source',owner,'hand',source=True)
  if value=='two_sources':w.add(name,'copy',owner,'hand',source=True)
  w.add('Grizzly Bears','ally',owner);w.add('Hill Giant','crew',owner);w.add('Craw Wurm','reserve',owner)
  w.add('Grizzly Bears','enemy',enemy);w.add('Hill Giant','giant',enemy);w.add('Giant Spider','reach',enemy);w.add('Air Elemental','flying',enemy);w.add('Sol Ring','artifact_control',owner)
  if value=='token_crew':w.add('Servo Exhibition','servo_spell',owner,'hand')
  if value=='tapped_source':w.add('Twiddle','tap_spell',owner,'hand')
  if value in ('prevented_damage','prevented_combat'):w.add('Safe Passage','shield',enemy,'hand')
  if value=='cast_cancel':
   for color,count in enumerate(p['mana']):
    for index in range(count):w.add(('Forest','Swamp','Island','Forest','Mountain','Plains')[color],f'cast_land_{color}_{index}',owner)
  if value=='target_planeswalker':
   w.add('Tibalt, the Fiend-Blooded','walker',enemy,'hand');w.walkers['walker']=dict(name='Tibalt, the Fiend-Blooded',player=enemy,zone='hand',loyalty=0)
  if value!='bare_zones':
   for pl in (0,1):
    w.add('Forest',f'hand_{pl}',pl,'hand');w.add('Lightning Bolt',f'grave_{pl}',pl,'graveyard');w.add('Island',f'exile_{pl}',pl,'exile')
    for index,nm in enumerate(('Forest','Island','Plains','Swamp','Mountain','Grizzly Bears')):w.add(nm,f'library_{pl}_{index}',pl,'library')
  start=enemy if value=='target_planeswalker' else owner;w.current=start;w.emit('turn',player=start,value=0);w.pool(0,[0]*6);w.pool(1,[0]*6);w.emit('refresh');w.receipts()
  if value=='target_planeswalker':w.cast_walker('walker');w.next_turn()
  if value=='cast_cancel':
   # EVENT_CAN_CAST is a raw card-handler query, not creature casting legality.
   # Ordinary creature handlers need not return true there. Verify actual mana
   # producers, then require the real casting path to consume cancellation.
   lands=[c['ref'] for c in w.setup if c['ref'].startswith('cast_land_')]
   for ref in lands:w.emit('require_can_activate',ref=ref,value=1);w.emit('assert_card',ref=ref,property='tapped',value=0)
   w.emit('choose_cancel_mana',player=owner);w.emit('cast',ref='source',value=0);w.emit('assert_unused_choices',value=0)
   for ref in lands:w.emit('assert_card',ref=ref,property='tapped',value=0)
   w.receipts()
  else:w.cast_source('source',value if value in ('insufficient_mana','wrong_color') else None)
  ready_cut=declaration_cut=0
  if value not in ('entry','insufficient_mana','wrong_color','cast_cancel'):
   if value=='two_sources':w.cast_source('copy')
   if value=='token_crew':w.cast_servos()
   if value!='fresh_attack':w.age()
   if value=='opponent_attack':w.next_turn()
   if value=='tapped_source':w.spell('Twiddle','tap_spell',owner,'source',dialog=0);w.entities['source']['tapped']=True;w.receipts()
   if value=='source_absent':w.emit('destroy',ref='source');w.leave('source','graveyard');w.receipts()
   if value in ('prevented_damage','prevented_combat'):w.cast_card('Safe Passage','shield',enemy);w.prevented.add(enemy);w.receipts()
   chosen=[] if value=='zero_attackers' else ['source'] if value=='one_attacker' else ['source','ally'] if value=='two_attackers' else ['ally','crew','reserve'] if value in ('three_without_source','source_absent') else ['enemy','giant','reach'] if value=='opponent_attack' else ['source','copy','ally'] if value=='two_sources' else ['source','servo_0','servo_1'] if value=='token_crew' else ['source','ally','crew']
   if value=='four_attackers':chosen.append('reserve')
   target='source' if value in ('target_self',) else 'ally' if value in ('target_own','target_own_creature') else 'walker' if value=='target_planeswalker' else 'enemy'
   target_player=owner if value=='target_own_player' else enemy if p['reward']=='target_damage' and value not in ('target_creature','target_own_creature','target_self','target_planeswalker','mandatory_cancel') else None
   ready_cut=len(w.steps);attackers,declaration_cut=w.declaration(chosen,target,target_player,cancel=value=='mandatory_cancel')
   if value=='source_destroyed_after':w.emit('destroy',ref='source');w.leave('source','graveyard');w.receipts()
   if value in ('unblocked','blocked','cleanup','expiry','reach_block','flying_block','prevented_combat'):
    pairs=[('giant','source')] if value=='blocked' else [('reach','source')] if value=='reach_block' else [('flying','source')] if value=='flying_block' else []
    if value=='expiry' and p['reward']=='target_cannot_block':pairs=[('enemy','source')]
    w.combat(attackers,pairs)
    if value=='cleanup':w.emit('cleanup');w.expire();w.receipts()
    if value=='expiry':w.age();attackers,_=w.declaration(['source','ally']);w.combat(attackers,[('enemy','source')])
  w.receipts();seq=w.steps
  if value in ('ready_snapshot','declaration_snapshot'):
   cut=ready_cut if value=='ready_snapshot' else declaration_cut;tail=seq[cut:]
   seq=seq[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if value=='resolved_snapshot':
   begin=len(w.steps);w.receipts();checks=w.steps[begin:];seq=w.steps[:begin]+[step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+checks+[step('assert_recorded_state')]
  out.append(dict(name=f'BattalionTriggerRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=seq,
   _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],
    untested_components=['gameplay responses between declaration and inline finish_attackers resolution','arbitrary rejected-target-filter introspection','AI source-payment cancellation policy','multiplayer','entering-attacking tokens are not fabricated or treated as declared attackers','compound Battalion/source-protection/extra activated-clause cards remain active later deltas'])))
 return out
