"""Whole-Oracle mentor declarations, permanent counters and paid power comparisons."""
import copy,json,re
from pathlib import Path
from battalion_trigger_recipes import _BattalionWorld

FAMILY='mentor_attack_counter'
MENTOR='Mentor (Whenever this creature attacks, put a +1/+1 counter on target attacking creature with lesser power.)'

def _profiles():
 return json.loads((Path(__file__).parent/'data/mentor_attack_counter_wave8_profiles.json').read_text(encoding='utf8'))

def classify(r):
 from creature_recipes import oracle_keywords
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or not r.get('type_line','').startswith('Creature — '):
  raise ValueError('whole normal nonlegendary standalone mentor creature required')
 lines=r.get('oracle_text','').splitlines()
 if lines.count(MENTOR)!=1:raise ValueError('complete mentor clause required')
 other=[line for line in lines if line!=MENTOR];pump=[]
 for line in other:
  match=re.fullmatch(r'((?:\{[0-9WUBRG]+\})+): (?:This creature|'+re.escape(r['name'])+r') gets \+([1-5])/\+([1-5]) until end of turn\.',line)
  if match:pump.append((line,match))
 if len(pump)>1:raise ValueError('only one complete ordinary self-pump activation supported')
 mana=[];power=toughness=0
 if pump:
  line,match=pump[0];other.remove(line);mana=cost_from_oracle(dict(r,mana_cost=match[1]));power=int(match[2]);toughness=int(match[3])
 passive=oracle_keywords(dict(r,oracle_text='\n'.join(other),keywords=[k for k in r.get('keywords',[]) if k!='Mentor']))
 if set(passive)-{'Haste','Flying','Vigilance','First strike'}:raise ValueError('mentor passive requires independent combat receipts')
 cost_from_oracle(r)
 return dict(keywords=passive,activation_mana=mana,pump_power=power,pump_toughness=toughness)

def spec(d,r):
 from recipes import cost_from_oracle
 if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not r or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:
  raise ValueError('whole pinned version2 mentor declaration required')
 expected=classify(r);p=d.get('params')
 if (type(p) is not dict or p!=expected or set(p)!=set(expected) or any(type(p[k]) is not int for k in ('pump_power','pump_toughness'))
     or type(p['activation_mana']) is not list or any(type(x) is not int for x in p['activation_mana'])
     or type(p['keywords']) is not list or any(type(x) is not str for x in p['keywords'])):
  raise ValueError('exact typed mentor parameters must derive from whole Oracle')
 return dict(copy.deepcopy(p),family=FAMILY,effect='mentor_counter',entry_energy=0,mana=cost_from_oracle(r),source_power=int(r['power']),source_toughness=int(r['toughness']))

def variants(p):
 values=['entry','insufficient_mana','wrong_color','fresh_attack','effect','target_other','equal_only','greater_only','nonattacking_lower','source_alone','only_others','zero_attackers',
         'opponent_attack','tapped_source','source_absent','source_destroyed_after','target_destroyed_after','source_pump','target_pump','mandatory_cancel','unblocked','blocked','cleanup','repeat_equal','repeat_source_pump',
         'ready_snapshot','declaration_snapshot','resolved_snapshot','bare_zones']
 if 'Flying' in p['keywords']:values+=['reach_block','flying_block']
 if p['activation_mana']:values+=['activation','activation_twice','activation_insufficient','activation_wrong_color','activation_cleanup','activation_snapshot']
 return [f'{v}_{owner}' for owner in (0,1) for v in values]+['cast_cancel_0']+(['activation_cancel_0'] if p['activation_mana'] else [])

def validate_witnesses(o,rows):
 for name,pin in _profiles()['witnesses'].items():
  if o.get(name)!=pin['oracle'] or name not in rows or rows[name]['handler']!=pin['handler'] or rows[name].get('kind')!='card':raise ValueError('whole mentor witness/binding changed: '+name)
  if rows[name]['handler']=='NULL' or rows[name]['handler']=='card_dummy' and not pin.get('reviewed_native_witness'):raise ValueError('unreviewed/unimplemented mentor witness: '+name)
  if o[name].get('layout')!='normal' or o[name].get('card_faces') or o[name].get('selected_face'):raise ValueError('standalone mentor witness required')

class _MentorWorld(_BattalionWorld):
 def entity(self,*args,**kwargs):
  super().entity(*args,**kwargs);self.entities[args[1]]['counters']=0
 def arrive(self,ref):
  super().arrive(ref);self.entities[ref]['sick']=not self.has(ref,'Haste')
 def receipts(self):
  start=len(self.steps);super().receipts()
  for s in self.steps[start:]:
   if s['op']=='assert_card' and s.get('property')=='plus1_counters':s['value']=self.entities[s['ref']]['counters']
 def expire(self):
  super().expire()
  for e in self.entities.values():
   e['power']+=e['counters'];e['toughness']+=e['counters']
 def growth(self,ref):
  self.spell('Giant Growth','growth',self.owner,ref);self.entities[ref]['power']+=3;self.entities[ref]['toughness']+=3;self.receipts()
 def declaration(self,chosen,target='lower',cancel=False):
  self.emit('begin_attackers');attackers=[]
  for ref in chosen:
   e=self.entities[ref];legal=e['zone']=='battlefield' and e['player']==self.current and not e['sick'] and not e['tapped']
   self.emit('attack',ref=ref,value=int(legal))
   if legal:attackers.append(ref);e['attacking']=True;e['tapped']=not self.has(ref,'Vigilance')
  self.receipts();cut=len(self.steps)
  active='source' in attackers
  candidates=[ref for ref in attackers if self.entities[ref]['power']<self.entities['source']['power']] if active else []
  # No gameplay priority is fabricated between declaration and inline resolution.
  if candidates:
   if target not in candidates:raise ValueError('mentor target must be a real lesser-power attacker')
   self.emit('choose_optional_target' if cancel else 'choose_target',player=self.owner,ref=target)
  self.emit('finish_attackers');self.emit('assert_choices_consumed')
  if cancel:self.emit('assert_forced_target_choices',value=int(bool(candidates)))
  if candidates:
   e=self.entities[target];e['counters']+=1;e['power']+=1;e['toughness']+=1
  self.receipts();return attackers,cut
 def activate_pump(self,mode='activation'):
  price=list(self.p['activation_mana']);denied=mode in ('activation_insufficient','activation_wrong_color');cancel=mode=='activation_cancel'
  if mode=='activation_insufficient':price=[0]*6
  if mode=='activation_wrong_color':price=[sum(price),0,0,0,0,0]
  if cancel:price=[0]*6
  self.pool(self.owner,price);self.emit('require_can_activate',ref='source',value=int(not denied))
  if cancel:self.emit('choose_cancel_mana',player=0)
  self.emit('activate',ref='source',value=int(not denied and not cancel))
  if denied or cancel:
   if cancel:self.emit('assert_unused_choices',value=0)
   self.receipts();return
  self.mana[self.owner]=[0]*6;self.emit('assert_stack',value=1)
  for key in ('power','toughness'):self.emit('assert_card',ref='source',property=key,value=self.entities['source'][key])
  for color in range(6):self.emit('assert_mana',player=self.owner,color=color,value=0)
  self.emit('resolve');self.emit('assert_choices_consumed');self.entities['source']['power']+=self.p['pump_power'];self.entities['source']['toughness']+=self.p['pump_toughness'];self.receipts()

def expand(name,csvid,d,p,selected,rows,o):
 from recipes import step
 if not selected or len(set(selected))!=len(selected) or any(v not in variants(p) for v in selected):raise ValueError('explicit unique mentor variants required')
 validate_witnesses(o,rows);out=[]
 boundary={1:"Mons's Goblin Raiders",2:'Grizzly Bears',3:'Hill Giant',4:'Air Elemental',5:'Water Elemental',6:'Craw Wurm'}
 if not 2<=p['source_power']<=5:raise ValueError('independently pinned mentor power boundary witnesses required')
 for variant in selected:
  value,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner;w=_MentorWorld(name,p,owner,rows,o)
  w.add(name,'source',owner,'hand',source=True)
  for ref,power in [('lower',p['source_power']-1),('other_lower',p['source_power']-1),('equal',p['source_power']),('greater',p['source_power']+1)]:w.add(boundary[power],ref,owner)
  w.add('Grizzly Bears','enemy',enemy);w.add('Hill Giant','giant',enemy);w.add('Giant Spider','reach',enemy);w.add('Air Elemental','flying',enemy);w.add('Sol Ring','artifact',owner)
  if value in ('source_pump','target_pump','repeat_source_pump'):w.add('Giant Growth','growth',owner,'hand')
  if value in ('repeat_equal','repeat_source_pump'):w.add('Safe Passage','shield',enemy,'hand')
  if value=='tapped_source':w.add('Twiddle','twiddle',owner,'hand')
  if value in ('cast_cancel','activation_cancel'):
   for color,count in enumerate(p['mana'] if value=='cast_cancel' else p['activation_mana']):
    for index in range(count):w.add(('Forest','Swamp','Island','Forest','Mountain','Plains')[color],f'payment_land_{color}_{index}',owner)
  if value!='bare_zones':
   for pl in (0,1):
    w.add('Forest',f'hand_{pl}',pl,'hand');w.add('Lightning Bolt',f'grave_{pl}',pl,'graveyard');w.add('Island',f'exile_{pl}',pl,'exile')
    for index,nm in enumerate(('Forest','Island','Plains','Swamp','Mountain','Grizzly Bears')):w.add(nm,f'library_{pl}_{index}',pl,'library')
  w.emit('turn',player=owner,value=0);w.pool(0,[0]*6);w.pool(1,[0]*6);w.emit('refresh');w.receipts()
  lands=[c['ref'] for c in w.setup if c['ref'].startswith('payment_land_')]
  if value=='cast_cancel':
   for ref in lands:w.emit('require_can_activate',ref=ref,value=1);w.emit('assert_card',ref=ref,property='tapped',value=0)
   w.emit('choose_cancel_mana',player=0);w.emit('cast',ref='source',value=0);w.emit('assert_unused_choices',value=0)
   for ref in lands:w.emit('assert_card',ref=ref,property='tapped',value=0)
   w.receipts()
  else:w.cast_source('source',value if value in ('insufficient_mana','wrong_color') else None)
  ready_cut=declaration_cut=activation_cut=0
  if value not in ('entry','insufficient_mana','wrong_color','cast_cancel'):
   if value!='fresh_attack':w.age()
   if value=='opponent_attack':w.next_turn()
   if value=='tapped_source':w.spell('Twiddle','twiddle',owner,'source',dialog=0);w.entities['source']['tapped']=True;w.receipts()
   if value=='source_absent':w.emit('destroy',ref='source');w.leave('source','graveyard');w.receipts()
   if value=='source_pump':w.growth('source')
   if value=='target_pump':w.growth('lower')
   if value.startswith('activation'):
    activation_cut=len(w.steps)
    for ref in lands:w.emit('require_can_activate',ref=ref,value=1);w.emit('assert_card',ref=ref,property='tapped',value=0)
    w.activate_pump(value)
    if value=='activation_twice':w.activate_pump()
    for ref in lands:w.emit('assert_card',ref=ref,property='tapped',value=0)
   if value not in ('activation_insufficient','activation_wrong_color','activation_cancel'):
    chosen=[] if value=='zero_attackers' else ['source'] if value in ('source_alone','nonattacking_lower') else ['lower','other_lower'] if value in ('only_others','source_absent') else ['enemy','giant'] if value=='opponent_attack' else ['source','equal'] if value in ('equal_only','source_pump') or value.startswith('activation') else ['source','greater'] if value=='greater_only' else ['source','lower','other_lower'] if value in ('effect','target_other','bare_zones') else ['source','lower']
    target='other_lower' if value=='target_other' else 'equal' if value=='source_pump' or value.startswith('activation') else 'lower'
    ready_cut=len(w.steps);attackers,declaration_cut=w.declaration(chosen,target,cancel=value=='mandatory_cancel')
    if value in ('source_destroyed_after','target_destroyed_after'):
     ref='source' if value=='source_destroyed_after' else 'lower';w.emit('destroy',ref=ref);w.leave(ref,'graveyard');w.receipts()
    if value in ('unblocked','blocked','cleanup','repeat_equal','repeat_source_pump','reach_block','flying_block','activation_cleanup'):
     # Keep repeated attack comparisons nonlethal through an actual paid instant.
     if value in ('repeat_equal','repeat_source_pump'):w.cast_card('Safe Passage','shield',enemy);w.prevented.add(enemy);w.receipts()
     pairs=[('enemy','source')] if value=='blocked' else [('reach','source')] if value=='reach_block' else [('flying','source')] if value=='flying_block' else []
     w.combat(attackers,pairs)
     if value in ('cleanup','activation_cleanup'):w.emit('cleanup');w.expire();w.receipts()
     if value in ('repeat_equal','repeat_source_pump'):
      w.age()
      if value=='repeat_source_pump':w.growth('source')
      attackers,_=w.declaration(['source','lower']);w.combat(attackers)
  w.receipts();seq=w.steps
  if value in ('ready_snapshot','declaration_snapshot','activation_snapshot'):
   cut={'ready_snapshot':ready_cut,'declaration_snapshot':declaration_cut,'activation_snapshot':activation_cut}[value];tail=seq[cut:]
   seq=seq[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if value=='resolved_snapshot':
   begin=len(w.steps);w.receipts();checks=w.steps[begin:];seq=w.steps[:begin]+[step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+checks+[step('assert_recorded_state')]
  out.append(dict(name=f'MentorAttackCounterRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=seq,
   _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['gameplay responses between declaration target selection and inline trigger resolution','arbitrary card-specific rejected-target-filter introspection','AI source/activation payment cancellation policy','multiplayer','beginning-of-combat compound Mentor sources; no fabricated phase hooks','multiple Mentor trigger ordering and target revalidation during a response remain separate deltas'])))
 return out
