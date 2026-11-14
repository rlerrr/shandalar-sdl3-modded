"""Whole-Oracle Echo costs and history through real casts, turns and control."""
import copy,json,re
from pathlib import Path
from upkeep_value_trigger_recipes import _UpkeepWorld
from attack_value_trigger_recipes import _AttackResourceWorld

FAMILY='upkeep_echo_payment'
REMINDER=' (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)'

def _profiles():return json.loads((Path(__file__).parent/'data/upkeep_echo_payment_wave8_profiles.json').read_text(encoding='utf8'))

def classify(r):
 from recipes import cost_from_oracle
 from creature_recipes import oracle_keywords
 if not r or r.get('layout')!='normal' or 'card_faces' in r or 'selected_face' in r or not r.get('type_line','').startswith('Creature') or 'Legendary' in r['type_line']:raise ValueError('whole normal nonlegendary Echo creature required')
 lines=r.get('oracle_text','').splitlines();echo=[s for s in lines if s.startswith('Echo ')]
 if len(echo)!=1 or r.get('keywords',[]).count('Echo')!=1:raise ValueError('one complete Echo clause required')
 m=re.fullmatch(r'Echo ((?:\{[0-9WUBRG]+\})+)'+re.escape(REMINDER),echo[0])
 if not m or m[1]=='{0}':raise ValueError('unreviewed Echo cost/reminder or zero-cost paid rider')
 other=[s for s in lines if s!=echo[0]];entry='none';amount=0
 for text,kind,n in [('When this creature enters, draw a card.','draw',1),('When this creature enters, you gain 5 life.','gain_life',5)]:
  if text in other:
   if entry!='none':raise ValueError('coupled multiple entry instructions need a separate calibrated composition')
   other.remove(text);entry=kind;amount=n
 keys=oracle_keywords(dict(r,oracle_text='\n'.join(other),keywords=[k for k in r.get('keywords',[]) if k!='Echo']))
 if set(keys)-{'Flying','Vigilance','Haste','First strike','Trample','Flash'}:raise ValueError('additional native passive needs complete receipts')
 cost_from_oracle(r);fee=cost_from_oracle(dict(r,mana_cost=m[1]))
 if not 1<=sum(fee)<=8 or not any(fee[1:]):raise ValueError('bounded colored nonzero Echo payment required')
 return dict(echo_mana=fee,entry_effect=entry,entry_amount=amount,keywords=keys)

def spec(d,r):
 from recipes import cost_from_oracle
 if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not r or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('typed pinned version2 Echo declaration required')
 p=d.get('params');expected=classify(r)
 if type(p) is not dict or set(p)!=set(expected) or p!=expected or type(p['echo_mana']) is not list or any(type(n) is not int for n in p['echo_mana']) or type(p['entry_effect']) is not str or type(p['entry_amount']) is not int or type(p['keywords']) is not list or any(type(k) is not str for k in p['keywords']):raise ValueError('exact typed complete Oracle Echo parameters required')
 pin=_profiles()['sources'].get(r['name']);ui=dict(status='provisional_until_capture',dialog_players=[0,1],manual_land_payment_players=[0],multiple_trigger_order_players=[0])
 if not pin or pin['oracle']!=r or pin['ui']!=ui or any(type(x) is not int for k in ('dialog_players','manual_land_payment_players','multiple_trigger_order_players') for x in pin['ui'][k]):raise ValueError('independent released full source and provisional input pin required')
 return dict(copy.deepcopy(p),family=FAMILY,effect='echo',entry_energy=0,mana=cost_from_oracle(r))

def variants(p):
 modes=['cast','cast_insufficient','cast_wrong_color','pay','decline','echo_insufficient','echo_wrong_color','opponent_upkeep','paid_history','source_absent','source_in_hand','source_enters_after','source_destroyed_after','two_sources','recast','control_rearm','controller_decline','control_return','ready_snapshot','untap_snapshot','paid_snapshot','bare_zones','fresh_attack','unblocked','blocked']
 if 'Flying' in p['keywords']:modes+=['ground_block','reach_block','flying_block','defend_flying']
 if 'Trample' in p['keywords']:modes+=['trample_excess']
 if 'First strike' in p['keywords']:modes+=['first_strike']
 if 'Vigilance' in p['keywords']:modes+=['vigilance']
 if 'Flash' in p['keywords']:modes+=['flash_response']
 if p['entry_effect']=='draw':modes+=['exact_entry_library']
 return [m+'_'+str(pl) for pl in (0,1) for m in modes]+['cast_cancel_0','echo_cancel_0']

def validate_witnesses(o,rows):
 for group in ('sources','witnesses'):
  for nm,pin in _profiles()[group].items():
   if o.get(nm)!=pin['oracle'] or nm not in rows or rows[nm]['kind']!='card' or rows[nm]['handler']!=pin['handler'] or o[nm].get('layout')!='normal':raise ValueError('whole source/witness identity or handler changed: '+nm)
   if rows[nm]['handler'] in ('NULL','card_dummy') and (group=='sources' or not pin.get('reviewed_native_witness')):raise ValueError('unimplemented primary or unreviewed native witness: '+nm)

class _EchoWorld(_UpkeepWorld):
 def __init__(self,*args):super().__init__(*args);self.owners={};self.due={};self.lands={};self.control_auras={};self.retired_receipt_names=set()
 def add(self,nm,ref,pl,zone='battlefield',source=False):
  super().add(nm,ref,pl,zone,source=source);self.owners[ref]=pl
  if ref in self.permanents:self.permanents[ref]['tapped']=False
  if source:self.due[ref]=False
  if ref in self.permanents and 'Land' in self.o[nm]['type_line']:self.lands[ref]=dict(color={'Wastes':0,'Swamp':1,'Island':2,'Forest':3,'Mountain':4,'Plains':5}[nm])
 def receipts(self):
  for ref,e in self.entities.items():
   if e['zone']=='battlefield' and self.has(ref,'Haste'):e['sick']=False
  _AttackResourceWorld.receipts(self)
  # Replacing generic payment fixtures must retain their absence observers,
  # rather than dropping the old Wastes zone assertions with the setup name.
  for nm in sorted(self.retired_receipt_names):
   for pl in (0,1):
    for zone in ('battlefield','hand','graveyard','exile'):self.emit('assert_zone_multiplicity',player=pl,zone=zone,index=self.rows[nm]['csvid'],value=self.zones[zone][pl].count(nm))
  for ref,e in self.permanents.items():
   if ref not in self.entities and e['zone']=='battlefield':self.emit('assert_card',ref=ref,property='in_play',value=1);self.emit('assert_card',ref=ref,property='tapped',value=int(e['tapped']))
 def move(self,ref,zone):
  e=self.permanents[ref];pl=e['player'];dest=self.owners[ref] if zone in ('hand','graveyard','exile') else pl;nm=e['name'];self.zones[e['zone']][pl].remove(nm);self.zones[zone][dest].append(nm)
  for obj in [e]+([self.entities[ref]] if ref in self.entities else []):
   obj['zone']=zone;obj['player']=dest;obj['tapped']=False
   if 'attacking' in obj:obj['attacking']=False
  if ref in self.due:self.due[ref]=False
  if ref in self.control_auras:
   body,previous=self.control_auras.pop(ref)
   if self.permanents[body]['zone']=='battlefield':self.transfer(body,previous)
  for aura,(body,_) in list(self.control_auras.items()):
   if body==ref and self.permanents[aura]['zone']=='battlefield':self.move(aura,'graveyard')
 def transfer(self,ref,pl):
  e=self.entities[ref];old=e['player'];self.zones['battlefield'][old].remove(e['name']);self.zones['battlefield'][pl].append(e['name']);e['player']=pl;e['sick']=True;self.permanents[ref]['player']=pl;self.due[ref]=True
  self.emit('bind_permanent_copy',player=pl,value=self.rows[e['name']]['csvid'],index=0,ref=ref)
 def entry(self,ref):
  pl=self.permanents[ref]['player'];n=self.p['entry_amount']
  if self.p['entry_effect']=='draw':
   if len(self.zones['library'][pl])<n:raise ValueError('terminal entry draw is outside pilot')
   self.zones['hand'][pl]+=self.zones['library'][pl][:n];del self.zones['library'][pl][:n]
  elif self.p['entry_effect']=='gain_life':self.life[pl]+=n
 def cast_echo(self,ref='source',denied=None):
  from recipes import cost_from_oracle
  e=self.permanents[ref];pl=e['player'];fee=cost_from_oracle(self.o[e['name']])
  if denied=='cast_insufficient':fee=[0]*6
  elif denied=='cast_wrong_color':k=next(i for i in range(1,6) if fee[i]);fee[k]-=1;fee[0]+=1
  self.pool(pl,fee);self.emit('cast',ref=ref,value=int(denied is None))
  if denied:self.emit('assert_unused_choices',value=0);self.receipts();return
  self.emit('assert_stack',value=1);self.emit('assert_card',ref=ref,property='in_hand',value=0);self.emit('resolve');self.mana[pl]=[0]*6;self.move(ref,'battlefield');self.entities[ref]['sick']=True;self.due[ref]=True;self.entry(ref);self.receipts()
 def destroy(self,ref):self.emit('destroy',ref=ref);self.move(ref,'graveyard');self.receipts()
 def payment_refs(self,pl):
  available=[r for r in self.lands if self.permanents[r]['player']==pl and self.permanents[r]['zone']=='battlefield' and not self.permanents[r]['tapped']];chosen=[]
  # Reserve mandatory colors first. Generic costs may use any remaining
  # mana; prefer Wastes when supplied so calibrated human input order stays
  # exact. Duplicate AI fixtures use matching basics to avoid prescribing
  # which color its legitimate first generic payment must consume.
  for color,n in enumerate(self.p['echo_mana'][1:],1):
   refs=[r for r in available if r not in chosen and self.lands[r]['color']==color]
   if len(refs)<n:return None
   chosen+=refs[:n]
  generic=[r for r in available if r not in chosen];generic.sort(key=lambda r:self.lands[r]['color']!=0)
  if len(generic)<self.p['echo_mana'][0]:return None
  return generic[:self.p['echo_mana'][0]]+chosen
 def visit(self,choice='pay'):
  if any(any(x) for x in self.mana):raise ValueError('real upkeep entry requires genuinely spent pools')
  self.emit('advance_turn');self.expire();self.current=1-self.current;self.emit('untap_step',player=self.current)
  for e in self.permanents.values():
   if e['zone']=='battlefield' and e['player']==self.current:e['tapped']=False
  for e in self.entities.values():
   if e['zone']=='battlefield' and e['player']==self.current:e['tapped']=False;e['sick']=False
  self.receipts();cut=len(self.steps);due=[r for r in self.sources if self.permanents[r]['zone']=='battlefield' and self.permanents[r]['player']==self.current and self.due[r]]
  for i,ref in enumerate(due):
   if len(due)>1 and i==0 and self.current==0:self.emit('choose_trigger_source',player=self.current,ref=ref)
   refs=self.payment_refs(self.current);paid=False
   if refs is not None:
    self.emit('choose_dialog',player=self.current,value=int(choice=='decline'))
    if choice=='cancel':
     if self.current!=0:raise ValueError('AI autotap cancellation is a strategy/input gap')
     self.emit('choose_cancel_mana',player=0)
    elif choice!='decline':
     if self.current==0:
      for land in refs:self.emit('choose_mana_source',ref=land)
     for land in refs:self.permanents[land]['tapped']=True
     paid=True
   self.due[ref]=False
   if not paid:self.move(ref,'graveyard')
  self.emit('upkeep_step',player=self.current);self.emit('assert_unused_choices',value=0);self.receipts();self.emit('turn',player=self.current,value=0);self.receipts();return cut
 def bounce_recast(self):
  from recipes import cost_from_oracle
  ref='source';pl=self.entities[ref]['player'];self.pool(pl,cost_from_oracle(self.o['Unsummon']));self.emit('choose_target',player=pl,ref=ref);self.emit('cast',ref='bounce_spell',value=1);self.emit('assert_stack',value=1);self.emit('resolve');self.mana[pl]=[0]*6;self.zones['hand'][pl].remove('Unsummon');self.zones['graveyard'][pl].append('Unsummon');self.move(ref,'hand');self.emit('bind_hand_copy',player=pl,value=self.rows[self.name]['csvid'],index=0,ref=ref);self.receipts();self.cast_echo(ref)
 def control(self):
  from recipes import cost_from_oracle
  pl=1-self.owner;previous=self.entities['source']['player'];self.pool(pl,cost_from_oracle(self.o['Control Magic']));self.emit('choose_target',player=pl,ref='source');self.emit('cast',ref='control_aura',value=1);self.emit('assert_stack',value=1);self.emit('resolve');self.mana[pl]=[0]*6;self.move('control_aura','battlefield');self.control_auras['control_aura']=('source',previous);self.transfer('source',pl);self.receipts()
 def remove_control(self):
  from recipes import cost_from_oracle
  pl=self.owner;self.pool(pl,cost_from_oracle(self.o['Disenchant']));self.emit('choose_target',player=pl,ref='control_aura');self.emit('cast',ref='aura_removal',value=1);self.emit('assert_stack',value=1);self.emit('resolve');self.mana[pl]=[0]*6;self.zones['hand'][pl].remove('Disenchant');self.zones['graveyard'][pl].append('Disenchant');self.move('control_aura','graveyard');self.receipts()
 def flash_response(self):
  from recipes import cost_from_oracle
  pl=self.owner;enemy=1-pl
  if 'Flash' not in self.p['keywords']:raise ValueError('non-Flash cannot be forced into a response menu')
  self.pool(pl,self.p['mana']);self.pool(enemy,cost_from_oracle(self.o['Grizzly Bears']));self.emit('cast',ref='flash_parent',value=1)
  # Legacy menu discovery filters this source by act_phases. Use the existing
  # real pending-spell bridge for the Oracle-legal Flash action; do not force
  # an unoffered candidate or claim that this proves response-menu discovery.
  self.emit('assert_stack',value=1);self.emit('announce_spell',ref='flash_parent');self.emit('cast_response',ref='source',value=1);self.emit('assert_stack',value=2);self.emit('assert_card',ref='source',property='in_hand',value=0);self.emit('assert_card',ref='source',property='in_play',value=0);self.emit('announce_spell',ref='source');self.emit('resolve_response');self.emit('assert_card',ref='source',property='in_play',value=1);self.emit('assert_unused_choices',value=0);self.mana[pl]=self.mana[enemy]=[0]*6;self.move('source','battlefield');self.entities['source']['sick']=True;self.due['source']=True;self.entry('source');self.move('flash_parent','battlefield');self.entities['flash_parent']['sick']=True;self.receipts()
 def declare(self,refs):
  self.emit('begin_attackers');chosen=[]
  for ref in refs:
   e=self.entities[ref];legal=e['zone']=='battlefield' and e['player']==self.current and not e['tapped'] and (not e['sick'] or self.has(ref,'Haste'));self.emit('attack',ref=ref,value=int(legal))
   if legal:e['attacking']=True;e['tapped']=not self.has(ref,'Vigilance');chosen.append(ref)
  self.emit('finish_attackers');self.receipts();return chosen
 def combat(self,attackers,pairs=()):
  self.emit('begin_blockers');blocked={}
  for b,a in pairs:
   d=self.entities[b];e=self.entities[a];legal=d['zone']=='battlefield' and e['zone']=='battlefield' and d['player']!=e['player'] and not d['tapped'] and (not self.has(a,'Flying') or self.has(b,'Flying') or self.has(b,'Reach'));self.emit('block',ref=b,other=a,value=int(legal))
   if legal:blocked[a]=b;self.emit('assert_card',ref=b,property='blocking_count',value=1)
  self.emit('advance_combat');self.emit('combat_damage');self.emit('end_combat')
  for a in attackers:
   e=self.entities[a]
   if a not in blocked:self.life[1-self.current]-=e['power'];continue
   b=blocked[a];d=self.entities[b];lethal=max(0,d['toughness']-d['damage']);power=e['power'];d['damage']+=min(power,lethal) if self.has(a,'Trample') else power
   if self.has(a,'Trample'):self.life[1-self.current]-=max(0,power-lethal)
   if not self.has(a,'First strike') or d['damage']<d['toughness']:e['damage']+=d['power']
   if e['damage']>=e['toughness']:self.move(a,'graveyard')
   if d['damage']>=d['toughness']:self.move(b,'graveyard')
  for e in self.entities.values():e['attacking']=False
  self.receipts()

def expand(name,csvid,d,p,selected,rows,o):
 from recipes import step
 if not selected or len(set(selected))!=len(selected) or any(v not in variants(p) for v in selected):raise ValueError('explicit unique released Echo variants required')
 validate_witnesses(o,rows);out=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner;w=_EchoWorld(name,p,owner,rows,o);w.add(name,'source',owner,'hand',source=True)
  if mode=='two_sources' and owner==1 and p['echo_mana'][0]:w.retired_receipt_names.add('Wastes')
  if mode=='two_sources':w.add(name,'copy',owner,'hand',source=True)
  if mode=='recast':w.add('Unsummon','bounce_spell',owner,'hand')
  if mode in ('control_rearm','controller_decline','control_return'):
   w.add('Control Magic','control_aura',enemy,'hand')
   if mode=='control_return':w.add('Disenchant','aura_removal',owner,'hand')
  if mode=='flash_response':w.add('Grizzly Bears','flash_parent',enemy,'hand')
  if mode in ('blocked','ground_block','trample_excess'):w.add('Grizzly Bears','blocker',enemy)
  if mode=='reach_block':w.add('Giant Spider','blocker',enemy)
  if mode in ('flying_block','defend_flying'):w.add('Air Elemental','blocker',enemy)
  if mode=='first_strike':w.add('Hill Giant','blocker',enemy)
  cast_modes=('cast','cast_insufficient','cast_wrong_color','cast_cancel')
  if mode not in cast_modes+('fresh_attack',):
   for pl in (owner,enemy) if mode in ('control_rearm','controller_decline','control_return') else (owner,):
    fee=list(p['echo_mana']);mult=2 if mode=='two_sources' else 1
    if mode=='echo_insufficient':k=next(i for i,n in enumerate(fee) if n);fee[k]-=1
    if mode=='echo_wrong_color':k=next(i for i in range(1,6) if fee[i]);fee[k]-=1;fee[4 if k!=4 else 5]+=1
    for color,n in enumerate(fee):
     landcolor=next(i for i in range(1,6) if fee[i]) if mode=='two_sources' and owner==1 and color==0 else color
     for i in range(n*mult):w.add(('Wastes','Swamp','Island','Forest','Mountain','Plains')[landcolor],f'echo_land_{pl}_{color}_{i}',pl)
  if mode=='cast_cancel':
   for color,n in enumerate(p['mana']):
    for i in range(n):w.add(('Wastes','Swamp','Island','Forest','Mountain','Plains')[color],f'cast_land_{color}_{i}',owner)
  for pl in (0,1):
   if mode!='bare_zones':w.add('Island',f'grave_{pl}',pl,'graveyard');w.add('Mountain',f'exile_{pl}',pl,'exile')
   count=1 if mode=='exact_entry_library' and pl==owner else 8
   for i in range(count):w.add(('Forest','Island','Plains','Swamp')[i%4],f'library_{pl}_{i}',pl,'library')
  w.current=enemy if mode=='flash_response' else owner;w.emit('turn',player=w.current,value=0);w.pool(0,[0]*6);w.pool(1,[0]*6);w.emit('refresh');w.receipts()
  if mode=='cast_cancel':
   for ref in w.lands:w.emit('require_can_activate',ref=ref,value=1)
   w.emit('choose_cancel_mana',player=0);w.emit('cast',ref='source',value=0);w.emit('assert_unused_choices',value=0);w.receipts()
  elif mode=='flash_response':w.flash_response()
  elif mode not in ('source_in_hand','source_enters_after'):w.cast_echo(denied=mode if mode in ('cast_insufficient','cast_wrong_color') else None)
  ready_cut=len(w.steps);untap_cut=paid_cut=0
  if mode not in cast_modes:
   if mode=='two_sources':w.cast_echo('copy')
   if mode=='source_absent':w.destroy('source')
   if mode=='fresh_attack':
    attackers=w.declare(['source'])
    if attackers:w.combat(attackers)
   elif mode=='flash_response':w.visit();w.visit();w.visit()
   elif mode in ('controller_decline',):w.visit();w.control();w.visit();w.visit('decline')
   else:
    w.visit()
    if mode!='opponent_upkeep':untap_cut=w.visit('decline' if mode=='decline' else 'cancel' if mode=='echo_cancel' else 'pay')
    if mode=='paid_snapshot':paid_cut=len(w.steps);w.visit();w.visit()
    if mode=='source_enters_after':w.cast_echo();w.visit();w.visit()
    if mode=='paid_history':
     for _ in range(4):w.visit()
    if mode=='source_destroyed_after':w.destroy('source');w.visit();w.visit()
    if mode=='recast':w.bounce_recast();w.visit();w.visit()
    if mode in ('control_rearm','control_return'):
     w.visit();w.control();w.visit();w.visit()
     if mode=='control_return':w.visit();w.remove_control();w.visit();w.visit()
    if mode in ('unblocked','blocked','ground_block','reach_block','flying_block','trample_excess','first_strike','vigilance'):
     attackers=w.declare(['source']);w.combat(attackers,[] if mode in ('unblocked','vigilance') else [('blocker','source')])
    if mode=='defend_flying':w.visit();attackers=w.declare(['blocker']);w.combat(attackers,[('source','blocker')])
  w.receipts();seq=w.steps
  if mode in ('ready_snapshot','untap_snapshot','paid_snapshot'):
   cut=ready_cut if mode=='ready_snapshot' else untap_cut if mode=='untap_snapshot' else paid_cut;tail=seq[cut:];seq=seq[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  out.append(dict(name=f'UpkeepEchoPaymentRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],ui_annotations='provisional_until_capture',untested_components=['AI mana cancellation strategy (human actual payment cancellation only)','legacy response-menu discovery is constrained by act_phases; Flash is exercised through the real pending-spell response bridge, not menu eligibility' if mode=='flash_response' else 'negative arbitrary response-menu introspection; Flash positive uses actual validated priority selection','noncast blink/reanimation reentry Echo arming','extra/skipped upkeeps and inter-trigger response actions','multiplayer and terminal life/deck states'])))
 return out
