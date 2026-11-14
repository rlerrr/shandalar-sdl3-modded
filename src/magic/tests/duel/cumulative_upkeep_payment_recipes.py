"""Whole cumulative upkeep payments, real age history and complete source clauses."""
import copy,json,re
from pathlib import Path
from upkeep_value_trigger_recipes import _UpkeepWorld
from attack_value_trigger_recipes import _AttackResourceWorld

FAMILY='cumulative_upkeep_payment'
REMINDER=' (At the beginning of your upkeep, put an age counter on this permanent, then sacrifice it unless you pay its upkeep cost for each age counter on it.)'
FEAR_REMINDER=" (This creature can't be blocked except by artifact creatures and/or black creatures.)"
NATIVE={'flying':'Flying','trample':'Trample','haste':'Haste','first strike':'First strike','defender':'Defender','fear':'Fear'}

def _profiles():return json.loads((Path(__file__).parent/'data/cumulative_upkeep_payment_wave8_profiles.json').read_text(encoding='utf8'))

def classify(r):
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or not any(k in r.get('type_line','') for k in ('Creature','Enchantment')) or r['type_line'].startswith('Token '):raise ValueError('whole normal cumulative-upkeep creature/enchantment required')
 lines=r.get('oracle_text','').splitlines();cu=[s for s in lines if s.startswith('Cumulative upkeep')]
 if len(cu)!=1 or r.get('keywords',[]).count('Cumulative upkeep')!=1:raise ValueError('one complete cumulative upkeep clause required')
 mana=re.fullmatch(r'Cumulative upkeep ((?:\{[0-9WUBRG]+\})+)'+re.escape(REMINDER),cu[0]);life=re.fullmatch(r'Cumulative upkeep—Pay ([1-2]) life\.'+re.escape(REMINDER),cu[0])
 if not mana and not life:raise ValueError('unreviewed complete mana/life cumulative upkeep cost')
 fee=cost_from_oracle(dict(r,mana_cost=mana[1])) if mana else [0]*6
 if mana and not 1<=sum(fee)<=4:raise ValueError('bounded nonzero upkeep mana vector required')
 other=[s for s in lines if s!=cu[0]];entry_gain=leave_loss=leave_age_gain=0;blocking='ordinary';entry='When this enchantment enters, you gain 20 life.';leave='When this enchantment leaves the battlefield, you lose 20 life.'
 if entry in other or leave in other:
  if entry not in other or leave not in other or 'Enchantment' not in r['type_line']:raise ValueError('complete coupled entry/leave life20 required')
  other.remove(entry);other.remove(leave);entry_gain=leave_loss=20
 age_leave='When this creature leaves the battlefield, you gain life equal to the number of age counters on it.'
 if age_leave in other:
  if entry_gain or 'Creature' not in r['type_line']:raise ValueError('unpiloted coupled leave composition')
  other.remove(age_leave);leave_age_gain=1
 wall="This creature can't be blocked by non-Wall creatures."
 if wall in other:other.remove(wall);blocking='wall_only'
 text='\n'.join(other).replace(FEAR_REMINDER,'');tokens=[s.strip().lower() for s in re.split(r'[,;\n]',text) if s.strip()]
 if any(t not in NATIVE for t in tokens) or len(set(tokens))!=len(tokens):raise ValueError('unreviewed whole source clause/passive/activation')
 keys=sorted(NATIVE[t] for t in tokens)
 if set(keys)!=set(r.get('keywords',[]))-{'Cumulative upkeep'} or keys and 'Creature' not in r['type_line']:raise ValueError('source text and structured native keyword pins disagree')
 if 'Fear' in keys:
  if FEAR_REMINDER not in r['oracle_text'] or blocking!='ordinary':raise ValueError('complete independent Fear clause required')
  blocking='artifact_or_black'
 if 'Creature' in r['type_line'] and (not re.fullmatch(r'[0-9]+',r.get('power','')) or not re.fullmatch(r'[1-9][0-9]*',r.get('toughness',''))):raise ValueError('fixed bounded creature stats required')
 cost_from_oracle(r)
 return dict(cost_kind='mana' if mana else 'life',mana_per_age=fee,life_per_age=int(life[1]) if life else 0,keywords=keys,blocker_requirement=blocking,entry_gain=entry_gain,leave_loss=leave_loss,leave_age_gain=leave_age_gain)

def spec(d,r):
 from recipes import cost_from_oracle
 if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not r or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('typed pinned version2 cumulative-upkeep declaration required')
 p=d.get('params');expected=classify(r)
 if type(p) is not dict or set(p)!=set(expected) or p!=expected or any(type(p[k]) is not str for k in ('cost_kind','blocker_requirement')) or any(type(p[k]) is not int for k in ('life_per_age','entry_gain','leave_loss','leave_age_gain')) or type(p['mana_per_age']) is not list or any(type(n) is not int for n in p['mana_per_age']) or type(p['keywords']) is not list or any(type(k) is not str for k in p['keywords']):raise ValueError('exact typed complete Oracle cost/passive/leave parameters required')
 pin=_profiles()['sources'].get(r['name']);ui=dict(status='provisional_until_capture',payable_dialog_players=[0,1],unaffordable_dialog_players=[1],manual_mana_players=[0],single_mandatory_trigger_selection=[])
 if not pin or pin['oracle']!=r or pin['ui']!=ui or any(type(x) is not int for k,v in ui.items() if type(v) is list for x in pin['ui'][k]):raise ValueError('complete independent released source/menu pins required')
 return dict(copy.deepcopy(p),family=FAMILY,effect='cumulative_upkeep',entry_energy=0,mana=cost_from_oracle(r),creature='Creature' in r['type_line'],legendary='Legendary' in r['type_line'])

def validate_witnesses(o,rows):
 for group in ('sources','witnesses'):
  for n,pin in _profiles()[group].items():
   if o.get(n)!=pin['oracle'] or n not in rows or rows[n]['kind']!='card' or rows[n]['handler']!=pin['handler'] or o[n].get('layout')!='normal':raise ValueError('whole normal source/witness Oracle or handler changed: '+n)
   if rows[n]['handler'] in ('NULL','card_dummy') and (group=='sources' or not pin.get('reviewed_native_witness')):raise ValueError('unimplemented primary or unreviewed native witness: '+n)

def variants(p):
 modes=['cast','cast_insufficient','cast_wrong_color','cast_countered','age_one','age_two','age_three','decline_one','decline_after_one','opponent_upkeep','source_in_hand','source_absent','late_entry','destroy_age2','bounce_age2','recast','ready_snapshot','untap_snapshot','paid_snapshot','bare_zones']
 if p['cost_kind']=='mana':
  modes+=['unaffordable_one','unaffordable_age3']
  if any(p['mana_per_age'][1:]):modes+=['wrong_color_one','wrong_color_age3']
 else:modes+=['life_short','armor_life']
 if p['creature']:
  modes+=['fresh_attack','unblocked','blocked','control_age','control_return','controller_decline']
  if 'Flying' in p['keywords']:modes+=['ground_block','reach_block','flying_block','defend_flying']
  if 'Trample' in p['keywords']:modes+=['trample_excess']
  if 'Defender' in p['keywords']:modes+=['defend_ground']
  if p['blocker_requirement']=='artifact_or_black':modes+=['fear_black_flyer','fear_artifact_flyer','fear_nonmatching_flyer','fear_ground_artifact']
  if p['blocker_requirement']=='wall_only':modes+=['wall_block','wall_changeling','wall_nonmatching']
  if p['leave_age_gain']:modes+=['combat_leave']
 return [m+'_'+str(pl) for pl in (0,1) for m in modes]+['cast_cancel_0']+(['pay_cancel_0'] if p['cost_kind']=='mana' else [])

class _CumulativeWorld(_UpkeepWorld):
 """Age and complete life/owner ledgers; every counter change is real upkeep."""
 def __init__(self,*a):super().__init__(*a);self.owners={};self.age={};self.lands={};self.control_auras={}
 def add(self,nm,ref,pl,zone='battlefield',source=False):
  super().add(nm,ref,pl,zone,source=source);self.owners[ref]=pl
  if ref in self.permanents:self.permanents[ref]['tapped']=False
  if source:self.age[ref]=0
  if ref in self.permanents and 'Land' in self.o[nm]['type_line']:self.lands[ref]={'Wastes':0,'Swamp':1,'Island':2,'Forest':3,'Mountain':4,'Plains':5}[nm]
 def receipts(self):
  for ref,e in self.entities.items():
   if e['zone']=='battlefield' and self.has(ref,'Haste'):e['sick']=False
  _AttackResourceWorld.receipts(self)
  for ref,e in self.permanents.items():
   if ref not in self.entities and e['zone']=='battlefield':
    self.emit('assert_card',ref=ref,property='in_play',value=1)
    if e['tapped'] is not None:self.emit('assert_card',ref=ref,property='tapped',value=int(e['tapped']))
  for ref,n in self.age.items():
   if self.permanents[ref]['zone']=='battlefield':self.emit('assert_counter',ref=ref,counter_kind='age',value=n)
 def move(self,ref,zone):
  e=self.permanents[ref];pl=e['player'];was=e['zone']=='battlefield';nm=e['name'];dest=self.owners[ref] if zone in ('hand','graveyard','exile') else pl
  self.zones[e['zone']][pl].remove(nm);self.zones[zone][dest].append(nm)
  if ref in self.age and was:
   self.life[pl]+=self.p['leave_age_gain']*self.age[ref]-self.p['leave_loss']
   if self.life[pl]<=0:raise ValueError('terminal leave-loss path is outside this bounded fixture')
  for obj in [e]+([self.entities[ref]] if ref in self.entities else []):
   obj['zone']=zone;obj['player']=dest;obj['tapped']=False
   if 'attacking' in obj:obj['attacking']=False
  if ref in self.age:self.age[ref]=0
  if ref in self.control_auras:
   body,previous=self.control_auras.pop(ref)
   if self.permanents[body]['zone']=='battlefield':self.transfer(body,previous)
  for aura,(body,_) in list(self.control_auras.items()):
   if body==ref and self.permanents[aura]['zone']=='battlefield':self.move(aura,'graveyard')
 def transfer(self,ref,pl):
  e=self.entities[ref];old=e['player'];self.zones['battlefield'][old].remove(e['name']);self.zones['battlefield'][pl].append(e['name']);e['player']=pl;e['sick']=True;self.permanents[ref]['player']=pl
  # Control changes retain counters; only a real zone change creates a new object.
  self.emit('bind_permanent_copy',player=pl,value=self.rows[e['name']]['csvid'],index=0,ref=ref)
 def cast_source(self,denied=None,countered=False):
  from recipes import cost_from_oracle
  pl=self.owner;fee=list(self.p['mana'])
  if denied=='cast_insufficient':fee=[0]*6
  elif denied=='cast_wrong_color':color=next(i for i in range(1,6) if fee[i]);fee[color]-=1;fee[0]+=1
  self.pool(pl,fee)
  if countered:self.pool(1-pl,cost_from_oracle(self.o['Counterspell']))
  self.emit('cast',ref='source',value=int(not denied))
  if denied:self.emit('assert_unused_choices',value=0);self.receipts();return
  self.emit('assert_stack',value=1);self.emit('assert_card',ref='source',property='in_hand',value=0)
  if countered:
   self.emit('announce_spell',ref='source');self.emit('cast_response',ref='counter',value=1);self.emit('assert_stack',value=2);self.emit('announce_spell',ref='counter');self.emit('resolve_response');self.mana[pl]=self.mana[1-pl]=[0]*6;self.move('source','graveyard');self.zones['hand'][1-pl].remove('Counterspell');self.zones['graveyard'][1-pl].append('Counterspell')
  else:
   self.emit('resolve');self.mana[pl]=[0]*6;self.move('source','battlefield');self.age['source']=0;self.life[pl]+=self.p['entry_gain']
   if self.p['creature']:self.entities['source']['sick']=True
  self.emit('assert_unused_choices',value=0);self.receipts()
 def destroy(self,ref):self.emit('destroy',ref=ref);self.move(ref,'graveyard');self.receipts()
 def available_lands(self,pl):return [r for r in self.lands if self.permanents[r]['zone']=='battlefield' and self.permanents[r]['player']==pl and self.permanents[r]['tapped'] is False]
 def payment_refs(self,pl,fee):
  available=self.available_lands(pl);chosen=[]
  for color,n in enumerate(fee[1:],1):
   refs=[r for r in available if r not in chosen and self.lands[r]==color]
   if len(refs)<n:return None
   chosen+=refs[:n]
  generic=[r for r in available if r not in chosen];generic.sort(key=lambda r:self.lands[r]!=0)
  return None if len(generic)<fee[0] else generic[:fee[0]]+chosen
 def mark_payment(self,pl,fee,refs):
  available=self.available_lands(pl)
  if pl==0:
   for ref in refs:self.emit('choose_mana_source',ref=ref);self.permanents[ref]['tapped']=True
   return
  # AI chooses equivalent surplus basics freely. Observe exact all-paid
  # capacities and mandatory-color totals, never require a particular slot.
  for color,n in enumerate(fee):
   typed=[r for r in available if self.lands[r]==color]
   if n:
    for ref in typed:self.permanents[ref]['tapped']=True if len(typed)==n else None
 def visit(self,choice='pay'):
  if any(any(v) for v in self.mana):raise ValueError('real upkeep entry requires genuinely spent pools')
  self.emit('advance_turn');self.expire();self.current=1-self.current;self.emit('untap_step',player=self.current)
  for e in self.permanents.values():
   if e['zone']=='battlefield' and e['player']==self.current:e['tapped']=False
  for e in self.entities.values():
   if e['zone']=='battlefield' and e['player']==self.current:e['tapped']=False;e['sick']=False
  self.receipts();cut=len(self.steps)
  live=[r for r in self.sources if self.permanents[r]['zone']=='battlefield' and self.permanents[r]['player']==self.current]
  if len(live)>1:raise ValueError('simultaneous cumulative sources require a separate captured ordering delta')
  for ref in live:
   self.age[ref]+=1;n=self.age[ref];fee=[x*n for x in self.p['mana_per_age']];loss=n*self.p['life_per_age'];refs=self.payment_refs(self.current,fee) if self.p['cost_kind']=='mana' else []
   affordable=refs is not None if self.p['cost_kind']=='mana' else self.life[self.current]>=loss
   if affordable:self.emit('choose_dialog',player=self.current,value=int(choice=='decline'))
   elif self.current==1:self.emit('choose_dialog',player=1,value=1)
   paid=affordable and choice!='decline' and choice!='cancel'
   if affordable and choice=='cancel':
    if self.current!=0 or self.p['cost_kind']!='mana':raise ValueError('only actual human early mana cancellation is calibrated')
    self.emit('choose_cancel_mana',player=0)
   if paid:
    if self.p['cost_kind']=='mana':self.mark_payment(self.current,fee,refs)
    else:
     if self.life[self.current]==loss:raise ValueError('legal but terminal exact-life payment requires separate game-loss support')
     self.life[self.current]-=loss
   else:self.move(ref,'graveyard')
  self.emit('upkeep_step',player=self.current);self.emit('assert_unused_choices',value=0);self.receipts();self.emit('turn',player=self.current,value=0);self.receipts();return cut
 def bounce(self,recast=False):
  from recipes import cost_from_oracle
  pl=self.owner;self.pool(pl,cost_from_oracle(self.o['Boomerang']));self.emit('choose_target',player=pl,ref='source');self.emit('cast',ref='bounce_spell',value=1);self.emit('assert_stack',value=1);self.emit('resolve');self.mana[pl]=[0]*6;self.zones['hand'][pl].remove('Boomerang');self.zones['graveyard'][pl].append('Boomerang');self.move('source','hand');self.emit('bind_hand_copy',player=pl,value=self.rows[self.name]['csvid'],index=0,ref='source');self.receipts()
  if recast:self.cast_source()
 def control(self):
  from recipes import cost_from_oracle
  pl=1-self.owner;previous=self.entities['source']['player'];self.pool(pl,cost_from_oracle(self.o['Control Magic']));self.emit('choose_target',player=pl,ref='source');self.emit('cast',ref='control_aura',value=1);self.emit('assert_stack',value=1);self.emit('resolve');self.mana[pl]=[0]*6;self.move('control_aura','battlefield');self.control_auras['control_aura']=('source',previous);self.transfer('source',pl);self.receipts()
 def remove_control(self):
  from recipes import cost_from_oracle
  pl=self.owner;self.pool(pl,cost_from_oracle(self.o['Disenchant']));self.emit('choose_target',player=pl,ref='control_aura');self.emit('cast',ref='aura_removal',value=1);self.emit('assert_stack',value=1);self.emit('resolve');self.mana[pl]=[0]*6;self.zones['hand'][pl].remove('Disenchant');self.zones['graveyard'][pl].append('Disenchant');self.move('control_aura','graveyard');self.receipts()
 def lower_life(self):
  from recipes import cost_from_oracle
  for i in range(5):
   self.pool(self.owner,cost_from_oracle(self.o['Lightning Bolt']));self.emit('choose_player',player=self.owner,value=self.owner);self.emit('cast',ref=f'bolt_{i}',value=1);self.emit('assert_stack',value=1);self.emit('resolve');self.mana[self.owner]=[0]*6;self.zones['hand'][self.owner].remove('Lightning Bolt');self.zones['graveyard'][self.owner].append('Lightning Bolt');self.life[self.owner]-=3
   if self.life[self.owner]<=0:raise ValueError('real damage lowering must remain nonterminal')
   self.receipts()
 def armor(self):
  self.cast_card("Urza's Armor",'armor',self.owner,'battlefield');self.permanents['armor']['zone']='battlefield';self.receipts()
 def wall(self,nm):return 'Wall' in self.o[nm]['type_line'].split(' — ')[-1].split() or 'Changeling' in self.o[nm].get('keywords',[])
 def can_block(self,b,a):
  d=self.entities[b];e=self.entities[a]
  if d['zone']!='battlefield' or e['zone']!='battlefield' or d['player']==e['player'] or d['tapped'] or self.has(a,'Flying') and not(self.has(b,'Flying') or self.has(b,'Reach')):return False
  if a in self.sources:
   if self.p['blocker_requirement']=='wall_only' and not self.wall(d['name']):return False
   if self.p['blocker_requirement']=='artifact_or_black' and not('Artifact' in self.o[d['name']]['type_line'] or 'B' in self.o[d['name']].get('colors',[])):return False
  return True
 def declare(self,refs):
  self.emit('begin_attackers');chosen=[]
  for ref in refs:
   e=self.entities[ref];legal=e['zone']=='battlefield' and e['player']==self.current and not e['tapped'] and (not e['sick'] or self.has(ref,'Haste')) and not self.has(ref,'Defender');self.emit('attack',ref=ref,value=int(legal))
   if legal:e['attacking']=True;e['tapped']=True;chosen.append(ref)
  self.emit('finish_attackers');self.receipts();return chosen
 def combat(self,attackers,pairs=()):
  self.emit('begin_blockers');blocked={}
  for b,a in pairs:
   legal=self.can_block(b,a);self.emit('block',ref=b,other=a,value=int(legal))
   if legal:blocked[a]=b;self.emit('assert_card',ref=b,property='blocking_count',value=1)
  self.emit('advance_combat');self.emit('combat_damage');self.emit('end_combat')
  for a in attackers:
   if a not in blocked:self.life[1-self.current]-=self.entities[a]['power'];continue
   b=blocked[a]
   for early in (True,False):
    ae=self.entities[a];de=self.entities[b]
    if ae['zone']!='battlefield':break
    a_strike=self.has(a,'First strike')==early;d_strike=de['zone']=='battlefield' and self.has(b,'First strike')==early
    if a_strike:
     if de['zone']=='battlefield':
      lethal=max(0,de['toughness']-de['damage']);hit=min(ae['power'],lethal) if self.has(a,'Trample') else ae['power'];de['damage']+=hit
      if self.has(a,'Trample'):self.life[1-self.current]-=max(0,ae['power']-lethal)
     elif self.has(a,'Trample'):self.life[1-self.current]-=ae['power']
    if d_strike:ae['damage']+=de['power']
    if ae['damage']>=ae['toughness']:self.move(a,'graveyard')
    if de['zone']=='battlefield' and de['damage']>=de['toughness']:self.move(b,'graveyard')
  for e in self.entities.values():e['attacking']=False
  self.receipts()

def expand(name,csvid,d,p,selected,rows,o):
 from recipes import step
 if not selected or len(set(selected))!=len(selected) or any(v not in variants(p) for v in selected):raise ValueError('explicit unique released cumulative-upkeep variants required')
 validate_witnesses(o,rows);out=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner;w=_CumulativeWorld(name,p,owner,rows,o);w.add(name,'source',owner,'hand',source=True)
  if mode=='cast_countered':w.add('Counterspell','counter',enemy,'hand')
  if mode in ('bounce_age2','recast'):w.add('Boomerang','bounce_spell',owner,'hand')
  if mode in ('control_age','control_return','controller_decline'):
   w.add('Control Magic','control_aura',enemy,'hand')
   if mode=='control_return':w.add('Disenchant','aura_removal',owner,'hand')
  if mode=='life_short':
   for i in range(5):w.add('Lightning Bolt',f'bolt_{i}',owner,'hand')
  if mode=='armor_life':w.add("Urza's Armor",'armor',owner,'hand')
  blockers={'blocked':'Grizzly Bears','ground_block':'Grizzly Bears','reach_block':'Giant Spider','flying_block':'Air Elemental','defend_flying':'Air Elemental','defend_ground':'Hill Giant','trample_excess':'Grizzly Bears','fear_black_flyer':'Bog Imp','fear_artifact_flyer':'Ornithopter','fear_nonmatching_flyer':'Air Elemental','fear_ground_artifact':'Memnite','wall_block':'Wall of Wood','wall_changeling':'Woodland Changeling','wall_nonmatching':'Grizzly Bears','combat_leave':'Hill Giant'}
  if mode in blockers:w.add(blockers[mode],'blocker',enemy)
  cast_only=('cast','cast_insufficient','cast_wrong_color','cast_countered','cast_cancel');capacity=1 if mode in ('age_one','decline_one','opponent_upkeep','wrong_color_one','pay_cancel') else 3 if mode in ('age_three','control_age','controller_decline') else 4 if mode=='control_return' else 2
  if mode not in cast_only+('fresh_attack',) and p['cost_kind']=='mana':
   for pl in (owner,enemy) if mode in ('control_age','control_return','controller_decline') else (owner,):
    fee=[n*capacity for n in p['mana_per_age']]
    if mode=='unaffordable_one':fee=[0]*6
    if mode in ('wrong_color_one','wrong_color_age3'):
     if mode=='wrong_color_age3':fee=[n*3 for n in p['mana_per_age']]
     color=next(i for i in range(1,6) if fee[i]);fee[color]-=1;fee[4 if color!=4 else 3]+=1
    for color,n in enumerate(fee):
     for i in range(n):w.add(('Wastes','Swamp','Island','Forest','Mountain','Plains')[color],f'pay_land_{pl}_{color}_{i}',pl)
  if mode=='cast_cancel':
   for color,n in enumerate(p['mana']):
    for i in range(n):w.add(('Wastes','Swamp','Island','Forest','Mountain','Plains')[color],f'cast_land_{color}_{i}',owner)
  for pl in (0,1):
   if mode!='bare_zones':w.add('Island',f'grave_{pl}',pl,'graveyard');w.add('Mountain',f'exile_{pl}',pl,'exile')
   for i,nm in enumerate(('Forest','Island','Plains','Swamp')):w.add(nm,f'library_{pl}_{i}',pl,'library')
  w.emit('turn',player=owner,value=0);w.pool(0,[0]*6);w.pool(1,[0]*6);w.emit('refresh');w.receipts()
  if mode=='armor_life':w.armor()
  if mode=='cast_cancel':
   for ref in w.lands:w.emit('require_can_activate',ref=ref,value=1)
   if not p['creature']:w.emit('require_can_cast',ref='source',value=1)
   w.emit('choose_cancel_mana',player=0);w.emit('cast',ref='source',value=0);w.emit('assert_unused_choices',value=0);w.receipts()
  elif mode not in ('source_in_hand','late_entry'):w.cast_source(denied=mode if mode in ('cast_insufficient','cast_wrong_color') else None,countered=mode=='cast_countered')
  ready_cut=len(w.steps);untap_cut=paid_cut=0
  if mode not in cast_only:
   if mode=='source_absent':w.destroy('source')
   if mode=='fresh_attack':
    attackers=w.declare(['source'])
    if attackers:w.combat(attackers)
   else:
    w.visit()
    if mode!='opponent_upkeep':
     untap_cut=w.visit('decline' if mode=='decline_one' else 'cancel' if mode=='pay_cancel' else 'pay')
     if mode=='paid_snapshot':paid_cut=len(w.steps)
     if mode=='life_short':w.lower_life()
     if mode not in ('age_one','decline_one','unaffordable_one','wrong_color_one','pay_cancel'):
      w.visit();untap_cut=w.visit('decline' if mode=='decline_after_one' else 'pay')
     if mode in ('age_three','unaffordable_age3','wrong_color_age3','life_short'):w.visit();w.visit()
    if mode=='late_entry':w.cast_source();w.visit();w.visit()
    if mode=='destroy_age2':w.destroy('source');w.visit();w.visit()
    if mode in ('bounce_age2','recast'):
     w.bounce(recast=mode=='recast')
     if mode=='recast':w.visit();w.visit()
    if mode in ('control_age','control_return','controller_decline'):
     w.visit();w.control();w.visit();w.visit('decline' if mode=='controller_decline' else 'pay')
     if mode=='control_return':w.remove_control();w.visit()
    if mode in blockers or mode=='unblocked':
     if mode in ('defend_flying','defend_ground'):w.visit();attackers=w.declare(['blocker']);pairs=[('source','blocker')]
     else:attackers=w.declare(['source']);pairs=[('blocker','source')] if mode in blockers and attackers else []
     if attackers:w.combat(attackers,pairs)
  w.receipts();seq=w.steps
  if mode in ('ready_snapshot','untap_snapshot','paid_snapshot'):
   cut=ready_cut if mode=='ready_snapshot' else untap_cut if mode=='untap_snapshot' else paid_cut;tail=seq[cut:];seq=seq[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  out.append(dict(name=f'CumulativeUpkeepPaymentRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],ui_annotations='single-source payable/decline/unaffordable protocol captured; full-source callbacks/combat provisional',untested_components=['AI equivalent surplus mana-source identity and payment cancellation strategy','simultaneous cumulative-source ordering/extra or skipped upkeep generators','partial mana cancellation/refund and end-upkeep residual mana beyond early zero-production cancel','legendary duplicates/legend-rule selection','enchantment donation terminal leave-loss paths','direct responses inside bounded production upkeep wrapper','multiplayer and life0/deck-loss terminals'])))
 return out
