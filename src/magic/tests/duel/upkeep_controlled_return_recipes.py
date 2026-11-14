"""Whole upkeep return choices, owner destinations and actual life payments."""
import copy,json,re
from pathlib import Path
from upkeep_permanent_count_recipes import _CountWorld
from upkeep_value_trigger_recipes import _UpkeepWorld

FAMILY='upkeep_controlled_return'
def _profiles():return json.loads((Path(__file__).parent/'data/upkeep_controlled_return_wave8_profiles.json').read_text(encoding='utf8'))

def classify(r):
 from creature_recipes import oracle_keywords
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or 'card_faces' in r or 'selected_face' in r or r.get('type_line','').startswith('Token ') or not any(t in r.get('type_line','') for t in ('Creature','Artifact','Enchantment')):raise ValueError('whole normal standalone upkeep return permanent required')
 lines=r.get('oracle_text','').splitlines();phase=[s for s in lines if s.startswith('At the beginning of ')]
 if len(phase)!=1:raise ValueError('one complete upkeep return clause required')
 m=re.fullmatch(r"At the beginning of (your upkeep|each player's upkeep), (.+)",phase[0])
 if not m:raise ValueError('unreviewed phase or upkeep scope')
 gate='own' if m[1]=='your upkeep' else 'each';body=m[2];life=0
 if body=="that player may pay 2 life. If they don't, they return a permanent they control to its owner's hand.":body="that player returns a permanent they control to its owner's hand.";life=2
 if body=="that player returns a permanent they control to its owner's hand unless they pay 2 life.":body="that player returns a permanent they control to its owner's hand.";life=2
 pattern=r"return (?:a|an) (?:(white|red|green) )?(permanent|creature|artifact|enchantment) you control to its owner's hand\." if gate=='own' else r"that player returns (?:a|an) (?:(white|red|green) )?(permanent|creature|artifact|enchantment) they control to its owner's hand\."
 choice=re.fullmatch(pattern,body)
 if not choice or choice[1] and choice[2]!='creature' or life and gate!='each':raise ValueError('unsupported complete return/payment/selector clause')
 other=[s for s in lines if s!=phase[0]]
 if 'Creature' in r['type_line']:keys=oracle_keywords(dict(r,oracle_text='\n'.join(other)))
 elif not other and not r.get('keywords'):keys=[]
 else:raise ValueError('additional source callbacks/passives not calibrated')
 if set(keys)-{'Flying','Haste','Trample'}:raise ValueError('printed passive needs complete receipt extension')
 cost_from_oracle(r)
 return dict(gate=gate,kind=choice[2],color={None:'','white':'W','red':'R','green':'G'}[choice[1]],life_payment=life,keywords=keys)

def spec(d,r):
 from recipes import cost_from_oracle
 if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not r or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('pinned typed version2 upkeep-return declaration required')
 p=d.get('params');expected=classify(r)
 if type(p) is not dict or set(p)!=set(expected) or p!=expected or any(type(p[k]) is not str for k in ('gate','kind','color')) or type(p['life_payment']) is not int or type(p['keywords']) is not list or any(type(k) is not str for k in p['keywords']):raise ValueError('exact typed Oracle return/life/selector parameters required')
 pin=_profiles()['sources'].get(r['name'])
 if not pin or pin['oracle']!=r:raise ValueError('complete independent released source pin required')
 if pin['ui']!=dict(status='provisional_until_capture',chooser='active' if p['gate']=='each' else 'controller',pay_dialog=p['life_payment']!=0) or type(pin['ui'].get('pay_dialog')) is not bool:raise ValueError('reviewed chooser/menu annotation required')
 can_return=(p['kind']=='permanent' or p['kind'].title() in r['type_line']) and (not p['color'] or p['color'] in r.get('colors',[]))
 return dict(copy.deepcopy(p),family=FAMILY,effect='controlled_return',entry_energy=0,mana=cost_from_oracle(r),creature='Creature' in r['type_line'],legendary='Legendary' in r['type_line'],source_can_return=bool(can_return))

def variants(p):
 v=['cast','insufficient_mana','effect','opponent_upkeep','source_only','owner_hand','filter_controls','source_absent','source_in_hand','source_enters_after','source_destroyed_after','repeat','ready_snapshot','untap_snapshot','resolved_snapshot','bare_zones','mandatory_cancel']
 if any(p['mana'][1:]):v+=['wrong_color']
 if not p['legendary']:v+=['two_sources']
 if p['source_can_return']:v+=['return_source','recast_source']
 if p['gate']=='each':v+=['empty_opponent']
 if p['kind']=='permanent':v+=['return_land','return_artifact','return_enchantment']
 if p['kind']=='creature' and p['color'] in ('','G') or p['kind']=='permanent':v+=['shroud_choice']
 if not p['color'] and p['kind'] in ('permanent','creature','artifact'):v+=['token_return']
 if p['color'] in ('W','G'):v+=['multicolor_choice']
 if p['kind']=='enchantment':v+=['aura_return']
 if p['creature']:
  v+=['fresh_attack','unblocked']
  if not set(p['keywords'])&{'Flying','Trample'}:v+=['blocked']
 if 'Flying' in p['keywords']:v+=['ground_block','reach_block','flying_block','defend_flying']
 if 'Trample' in p['keywords']:v+=['trample_excess']
 if p['life_payment']:v+=['pay_all','pay_own','pay_opponent','pay_then_return','armor_pay']
 return [f'{x}_{pl}' for pl in (0,1) for x in v]+['cast_cancel_0']

def validate_witnesses(o,rows):
 for group in ('sources','witnesses'):
  for nm,pin in _profiles()[group].items():
   if o.get(nm)!=pin['oracle'] or nm not in rows or rows[nm]['handler']!=pin['handler'] or rows[nm].get('kind')!='card':raise ValueError('independent full Oracle/binding changed: '+nm)
   if rows[nm]['handler'] in ('NULL','card_dummy') and not pin.get('reviewed_native_witness'):raise ValueError('unimplemented/unreviewed native witness: '+nm)

class _ReturnWorld(_CountWorld):
 def __init__(self,*a):super().__init__(*a);self.owners={};self.preferred={};self.pay_players=set();self.refusal=False;self.auras={};self.forced_total=0
 def add(self,nm,ref,pl,zone='battlefield',source=False,other_owned=False):
  _UpkeepWorld.add(self,nm,ref,pl,zone,source=source);self.owners[ref]=1-pl if other_owned else pl
  if other_owned:self.setup[-1]['state']=['opponent_owned']
  if source:self.configs[ref]=classify(self.o[nm])
 def members(self):return dict(self.permanents,**{r:self.entities[r] for r in self.tokens})
 def eligible(self,ref,p,pl,available=None):
  e=self.members()[ref];r=self.o[e['name']]
  return e['zone']=='battlefield' and e['player']==pl and (available is None or ref in available) and (p['kind']=='permanent' or p['kind'].title() in r['type_line']) and (not p['color'] or p['color'] in r.get('colors',[]))
 def receipts(self):
  # The engine's evaluated Haste path clears this raw status bit. Haste's
  # Oracle receipt remains the actual legal fresh attack and combat damage.
  for ref,e in self.entities.items():
   if e['zone']=='battlefield' and self.has(ref,'Haste'):e['sick']=False
  super().receipts()
  for e in self.permanents.values():
   if e['zone']=='battlefield' and 'Artifact' in self.o[e['name']]['type_line'] and 'Creature' in self.o[e['name']]['type_line']:self.emit('assert_matching_permanents',player=e['player'],index=self.rows[e['name']]['csvid'],property='artifact',value=1)
 def expire(self):super().expire();self.apply_auras()
 def apply_auras(self):
  for aura,body in self.auras.items():
   if self.permanents[aura]['zone']=='battlefield' and self.entities[body]['zone']=='battlefield':self.entities[body]['power']+=2;self.entities[body]['toughness']+=1
 def move(self,ref,zone):
  e=self.members()[ref];pl=e['player'];owner=self.owners.get(ref,pl);nm=e['name'];self.zones[e['zone']][pl].remove(nm)
  token=ref in self.tokens;dest=owner if zone in ('hand','graveyard','exile') else pl
  if not token:self.zones[zone][dest].append(nm)
  for entry in ([self.permanents[ref]] if ref in self.permanents else [])+([self.entities[ref]] if ref in self.entities else []):
   entry['zone']='ceased' if token else zone;entry['player']=dest
   if 'attacking' in entry:entry['attacking']=False;entry['tapped']=False
  for aura,body in list(self.auras.items()):
   if ref==aura:
    if self.entities[body]['zone']=='battlefield':self.entities[body]['power']-=2;self.entities[body]['toughness']-=1
   elif ref==body and self.permanents[aura]['zone']=='battlefield':self.move(aura,'graveyard')
 def leave(self,ref,zone):self.move(ref,zone)
 def destroy(self,ref):self.emit('destroy',ref=ref);self.move(ref,'graveyard');self.receipts()
 def create_tokens(self,nm,player,amount):
  before=len(self.tokens);super().create_tokens(nm,player,amount)
  for ref in self.tokens[before:]:self.owners[ref]=player
 def visit(self):
  if any(any(v) for v in self.mana):raise ValueError('real upkeep requires spent pools')
  self.emit('advance_turn');self.expire();self.current=1-self.current;self.emit('untap_step',player=self.current)
  for e in self.entities.values():
   if e['zone']=='battlefield' and e['player']==self.current:e['sick']=False;e['tapped']=False
  self.receipts();cut=len(self.steps);available={ref for ref,e in self.members().items() if e['zone']=='battlefield'};plan=[];forced=0
  live=[ref for ref in self.sources if self.permanents[ref]['zone']=='battlefield' and (self.configs[ref]['gate']=='each' or self.permanents[ref]['player']==self.current)]
  for source in live:
   p=self.configs[source];pl=self.current if p['gate']=='each' else self.permanents[source]['player'];pay=pl in self.pay_players
   if p['life_payment']:
    if self.life[pl]<p['life_payment']:raise ValueError('low-life affordability/menu bug requires a separate bounded actual-offer observer; never queue nonexistent choices')
    self.emit('choose_dialog',player=pl,value=0 if pay else 1)
   if p['life_payment'] and pay:plan.append(('pay',pl,p['life_payment']));continue
   options=[ref for ref in self.members() if self.eligible(ref,p,pl,available)];preferred=self.preferred.get(pl);chosen=preferred if preferred in options else next((r for r in options if r.startswith('chosen_')),options[0] if options else None)
   if chosen is not None:
    self.emit('choose_optional_target' if self.refusal else 'choose_target',player=pl,ref=chosen);forced+=int(self.refusal);available.remove(chosen);plan.append(('return',chosen,0))
  self.emit('upkeep_step',player=self.current)
  self.forced_total+=forced
  if self.refusal and forced:self.emit('assert_forced_target_choices',value=self.forced_total)
  for action,target,n in plan:
   if action=='pay':self.life[target]-=n
   else:self.move(target,'hand')
  self.receipts();self.emit('turn',player=self.current,value=0);self.receipts();return cut
 def declare(self,refs):
  self.emit('begin_attackers');attackers=[]
  for ref in refs:
   e=self.entities[ref];legal=e['zone']=='battlefield' and e['player']==self.current and (not e['sick'] or self.has(ref,'Haste')) and not e['tapped'];self.emit('attack',ref=ref,value=int(legal))
   if legal:attackers.append(ref);e['tapped']=True;e['attacking']=True
  self.emit('finish_attackers');self.receipts();return attackers
 def combat(self,attackers,pairs=()):
  self.emit('begin_blockers');blocked={}
  for b,a in pairs:
   defender=self.entities[b];attacker=self.entities[a];legal=defender['zone']=='battlefield' and attacker['zone']=='battlefield' and defender['player']!=attacker['player'] and not defender['tapped'] and (not self.has(a,'Flying') or self.has(b,'Flying') or self.has(b,'Reach'))
   self.emit('block',ref=b,other=a,value=int(legal))
   if legal:blocked[a]=b;self.emit('assert_card',ref=b,property='blocking_count',value=1)
  self.emit('advance_combat');self.emit('combat_damage');self.emit('end_combat')
  for a in attackers:
   e=self.entities[a]
   if e['zone']!='battlefield':continue
   if a not in blocked:self.life[1-self.current]-=e['power'];continue
   b=blocked[a];d=self.entities[b];power=e['power'];bp=d['power'];lethal=max(0,d['toughness']-d['damage']);trample=self.has(a,'Trample');d['damage']+=min(power,lethal) if trample else power;e['damage']+=bp
   if trample:self.life[1-self.current]-=max(0,power-lethal)
   if e['damage']>=e['toughness']:self.move(a,'graveyard')
   if d['damage']>=d['toughness']:self.move(b,'graveyard')
  for e in self.entities.values():e['attacking']=False
  self.receipts()

def _choice_name(p):
 return {'W':'Savannah Lions','R':'Hill Giant','G':'Grizzly Bears'}.get(p['color'],{'artifact':'Memnite','enchantment':'Seal of Removal','creature':'Grizzly Bears','permanent':'Grizzly Bears'}[p['kind']])

def expand(name,csvid,d,p,selected,rows,o):
 from recipes import step
 if not selected or len(set(selected))!=len(selected) or any(v not in variants(p) for v in selected):raise ValueError('explicit unique released upkeep-return variants required')
 validate_witnesses(o,rows);out=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner;w=_ReturnWorld(name,p,owner,rows,o);w.add(name,'source',owner,'hand',source=True)
  chosen=_choice_name(p)
  if mode=='return_land':chosen='Forest'
  if mode=='return_artifact':chosen='Sol Ring'
  if mode=='return_enchantment':chosen='Seal of Removal'
  if mode=='shroud_choice':chosen='Deadly Insect'
  if mode=='multicolor_choice':chosen='Watchwolf'
  for pl in (owner,enemy):
   if mode=='source_only' and pl==owner or mode=='empty_opponent' and pl==enemy:continue
   w.add(chosen,f'chosen_{pl}',pl,other_owned=mode=='owner_hand' and pl==owner);w.preferred[pl]=f'chosen_{pl}'
  if mode=='two_sources':
   w.add(name,'copy',owner,'hand',source=True)
   for pl in (owner,enemy):w.add(chosen,f'chosen_extra_{pl}',pl)
  if mode in ('return_source','recast_source'):w.preferred[owner]='source'
  if mode=='filter_controls':
   w.add('Ivory Mask','white_noncreature',owner);w.add('Forest','land_control',owner);w.add('Air Elemental','blue_control',owner)
  if mode=='shroud_choice':w.add('Unsummon','probe_spell',owner,'hand')
  if mode=='token_return':w.add('Servo Exhibition','token_spell',owner,'hand')
  if mode=='aura_return':w.add('Grizzly Bears','aura_body',owner);w.add('Unholy Strength','aura',owner,'hand');w.preferred[owner]='aura'
  if mode=='armor_pay':w.add("Urza's Armor",'armor',owner,'hand')
  if mode in ('pay_all','pay_then_return','armor_pay'):w.pay_players={0,1}
  if mode=='pay_own':w.pay_players={owner}
  if mode=='pay_opponent':w.pay_players={enemy}
  w.refusal=mode=='mandatory_cancel'
  if mode in ('blocked','trample_excess','ground_block'):w.add('Grizzly Bears','blocker',enemy)
  if mode=='reach_block':w.add('Giant Spider','blocker',enemy)
  if mode=='flying_block':w.add('Air Elemental','blocker',enemy)
  if mode=='defend_flying':w.add('Air Elemental','attacker',enemy)
  if mode=='cast_cancel':
   for color,count in enumerate(p['mana']):
    for i in range(count):w.add(('Forest','Swamp','Island','Forest','Mountain','Plains')[color],f'payment_land_{color}_{i}',owner)
  for pl in (0,1):
   if mode!='bare_zones':w.add('Forest',f'hand_{pl}',pl,'hand');w.add('Island',f'grave_{pl}',pl,'graveyard');w.add('Swamp',f'exile_{pl}',pl,'exile')
   for i,nm in enumerate(('Forest','Island','Plains','Swamp','Mountain','Grizzly Bears')):w.add(nm,f'library_{pl}_{i}',pl,'library')
  w.emit('turn',player=owner,value=0);w.pool(0,[0]*6);w.pool(1,[0]*6);w.emit('refresh');w.receipts()
  if mode=='shroud_choice':w.emit('can_target_creature',ref='probe_spell',other=f'chosen_{owner}',value=0)
  if mode=='cast_cancel':
   for s in w.setup:
    if s['ref'].startswith('payment_land_'):w.emit('require_can_activate',ref=s['ref'],value=1)
   w.emit('choose_cancel_mana',player=0);w.emit('cast',ref='source',value=0);w.emit('assert_unused_choices',value=0);w.receipts()
  elif mode not in ('source_in_hand','source_enters_after'):w.cast_named('source',mode if mode in ('insufficient_mana','wrong_color') else None)
  if mode not in ('cast','insufficient_mana','wrong_color','cast_cancel'):
   if mode=='two_sources':w.cast_named('copy')
   if mode=='token_return':w.cast_card('Servo Exhibition','token_spell',owner);w.create_tokens('Servo',owner,2);w.receipts();w.preferred[owner]='token_0'
   if mode=='aura_return':w.cast_card('Unholy Strength','aura',owner,'battlefield',target='aura_body');w.permanents['aura']['zone']='battlefield';w.auras['aura']='aura_body';w.apply_auras();w.receipts()
   if mode=='armor_pay':w.cast_named('armor')
   if mode=='source_absent':w.destroy('source')
   if mode=='fresh_attack':
    attackers=w.declare(['source'])
    if attackers:w.combat(attackers)
   else:
    ready_cut=len(w.steps);untap_cut=w.visit()
    if mode not in ('opponent_upkeep','defend_flying'):untap_cut=w.visit()
    if mode=='source_enters_after':w.cast_named('source');w.visit();w.visit()
    if mode=='source_destroyed_after':w.destroy('source');w.visit();w.visit()
    if mode=='recast_source':
     w.emit('bind_hand_copy',player=owner,value=csvid,index=0,ref='source');w.cast_named('source');w.visit();w.visit()
    if mode=='repeat':w.visit();w.visit()
    if mode=='pay_then_return':w.pay_players=set();w.visit();w.visit()
    if mode in ('unblocked','blocked','trample_excess','ground_block','reach_block','flying_block','defend_flying'):
     attackers=w.declare(['attacker'] if mode=='defend_flying' else ['source']);pairs=[('source','attacker')] if mode=='defend_flying' else [] if mode=='unblocked' else [('blocker','source')];w.combat(attackers,pairs)
  w.receipts();seq=w.steps
  if mode in ('ready_snapshot','untap_snapshot'):
   cut=ready_cut if mode=='ready_snapshot' else untap_cut;tail=seq[cut:];seq=seq[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if mode=='resolved_snapshot':
   cut=len(w.steps);w.receipts();checks=w.steps[cut:];seq=w.steps[:cut]+[step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+checks+[step('assert_recorded_state')]
  out.append(dict(name=f'UpkeepControlledReturnRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],ui_annotations='provisional_until_capture',untested_components=['No invented pending-trigger priority or arbitrary rejected-selection introspection; TARGET UI is not Oracle targeting','Low-life/cross-player affordability offer defect held: no strict-unused or terminal-life baseline','Ownership fixture proves actual owner-hand destination; not a claim of a particular control-changing spell','Additional upkeeps, simultaneous source-return stacks, multiplayer and terminal game-loss observation','Four same-grammar followers remain unpinned/unhomed until pilot pair gate'])) )
 return out
