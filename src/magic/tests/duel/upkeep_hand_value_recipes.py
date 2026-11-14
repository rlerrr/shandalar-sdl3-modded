"""Complete upkeep hand-size clauses, real hand changes and World lifecycle."""
import copy,json,re
from pathlib import Path
from upkeep_permanent_count_recipes import _CountWorld
from upkeep_value_trigger_recipes import _UpkeepWorld

FAMILY='upkeep_hand_value'
def _profiles():return json.loads((Path(__file__).parent/'data/upkeep_hand_value_wave8_profiles.json').read_text(encoding='utf8'))

def classify(r):
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or 'card_faces' in r or 'selected_face' in r or r.get('type_line','').startswith('Token ') or not any(t in r.get('type_line','') for t in ('Creature','Artifact','Enchantment')):raise ValueError('whole normal standalone hand-upkeep permanent required')
 m=re.fullmatch(r"At the beginning of (your upkeep|each player's upkeep|each opponent's upkeep), (.+)",r.get('oracle_text',''))
 if not m:raise ValueError('one complete hand-size upkeep clause required')
 gate={'your upkeep':'own',"each player's upkeep":'each',"each opponent's upkeep":'opponent'}[m[1]];body=m[2]
 source='(?:this artifact|this creature|this enchantment|'+re.escape(r['name'])+')'
 pred=dict(kind='always',basis='active',low=0,high=0);effect=recipient=None;formula=None
 cond=re.fullmatch(r'if (you|that player) (?:have|has) (one|two|three|five|seven) or (fewer|more) cards in hand, (.+)',body)
 if cond:
  n={'one':1,'two':2,'three':3,'five':5,'seven':7}[cond[2]];pred=dict(kind='le' if cond[3]=='fewer' else 'ge',basis='controller' if cond[1]=='you' else 'active',low=n,high=n);body=cond[4]
 gain=re.fullmatch(r'you gain (X|[1-9]) life(?:, where X is the number of cards in your hand minus 4)?\.',body)
 loss=re.fullmatch(r'(that player|they) (?:loses|lose) (X|[1-9]) life(?:, where X is the number of cards in that player\'s hand minus the number of cards in your hand)?\.',body)
 mill=re.fullmatch(r'that player mills X cards, where X is the number of cards in their hand\.',body)
 damage=re.fullmatch(source+r' deals (X|[1-9]) damage to (target opponent or planeswalker|target opponent|that player)(.*)',body)
 if gain:effect='gain';recipient='controller';formula=[1,0,-4] if gain[1]=='X' and body.endswith('minus 4.') else [0,0,int(gain[1])] if gain[1]!='X' and body==f'you gain {gain[1]} life.' else None
 elif loss:effect='loss';recipient='active';formula=[-1,1,0] if loss[2]=='X' and 'minus the number of cards in your hand' in body else [0,0,int(loss[2])] if loss[2]!='X' and body==f'{loss[1]} '+('loses' if loss[1]=='that player' else 'lose')+f' {loss[2]} life.' else None
 elif mill:effect='mill';recipient='active';formula=[0,1,0]
 elif damage:
  effect='damage';recipient={'that player':'active','target opponent':'opponent','target opponent or planeswalker':'opponent_or_planeswalker'}[damage[2]];tail=damage[3]
  if damage[1]!='X' and tail=='.':formula=[0,0,int(damage[1])]
  elif damage[1]!='X' and tail==' unless they have exactly three or exactly four cards in hand.':formula=[0,0,int(damage[1])];pred=dict(kind='outside',basis='active',low=3,high=4)
  elif damage[1]=='X':
   formulas={", where X is the number of cards in your hand minus the number of cards in that player's hand.":[1,-1,0],', where X is the number of cards in their hand minus 4.':[0,1,-4],', where X is 3 minus the number of cards in their hand.':[0,-1,3],', where X is 4 minus the number of cards in their hand.':[0,-1,4]}
   formula=formulas.get(tail)
 if formula is None or set(r.get('keywords',[]))-({'Mill'} if effect=='mill' else set()):raise ValueError('unsupported output, arithmetic, passive or compound hand clause')
 if recipient=='controller' and gate!='own' or recipient in ('opponent','opponent_or_planeswalker') and gate!='own':raise ValueError('unreviewed recipient/upkeep composition')
 if pred['kind']!='always' and (pred['basis']=='controller')!=(gate=='own'):raise ValueError('predicate controller must match printed upkeep gate')
 if 'Creature' in r['type_line']:int(r['power']);int(r['toughness'])
 cost_from_oracle(r)
 return dict(gate=gate,effect=effect,recipient=recipient,formula=formula,predicate=pred,keywords=[],world='World' in r['type_line'].split())

def spec(d,r):
 from recipes import cost_from_oracle
 if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not r or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('typed pinned version2 hand-value declaration required')
 p=d.get('params');expected=classify(r)
 if (type(p) is not dict or set(p)!=set(expected) or p!=expected or any(type(p[k]) is not str for k in ('gate','effect','recipient')) or type(p['formula']) is not list or any(type(n) is not int for n in p['formula']) or type(p['world']) is not bool or type(p['keywords']) is not list or p['keywords'] or type(p['predicate']) is not dict or any(type(p['predicate'][k]) is not str for k in ('kind','basis')) or any(type(p['predicate'][k]) is not int for k in ('low','high'))):raise ValueError('exact typed Oracle hand arithmetic/predicate required')
 pin=_profiles()['sources'].get(r['name'])
 if not pin or pin['oracle']!=r:raise ValueError('complete independent released source pin required')
 ui=pin['ui']
 if ui!=dict(status='provisional_until_capture',target_choices=p['recipient']=='opponent_or_planeswalker'):raise ValueError('reviewed target-offer annotation required')
 if type(ui.get('target_choices')) is not bool:raise ValueError('Boolean target annotation required')
 return dict(copy.deepcopy(p),family=FAMILY,entry_energy=0,mana=cost_from_oracle(r),creature='Creature' in r['type_line'],ui=copy.deepcopy(ui))

def variants(p):
 v=['cast','insufficient_mana','effect','opposite_visit','hand_zero','hand_one','hand_three','hand_four','hand_five','hand_seven','own_more','opponent_more','equal_hands','source_absent','source_in_hand','source_enters_after','source_destroyed_after','repeat','armor','ready_snapshot','untap_snapshot','resolved_snapshot','bare_zones','draw_own','draw_opponent','discard_before']
 if any(p['mana'][1:]):v+=['wrong_color']
 v+=['world_own','world_enemy'] if p['world'] else ['two_sources']
 if p['recipient']=='opponent_or_planeswalker':v+=['target_own_walker','target_enemy_walker','mandatory_cancel']
 if p['recipient']=='opponent':v+=['protected_opponent']
 if p['effect']=='mill':v+=['empty_library','short_library','exact_library']
 if p['creature']:v+=['unblocked','blocked']
 return [f'{x}_{pl}' for pl in (0,1) for x in v]+['cast_cancel_0']

def validate_witnesses(o,rows):
 for group in ('sources','witnesses'):
  for nm,pin in _profiles()[group].items():
   if o.get(nm)!=pin['oracle'] or nm not in rows or rows[nm]['handler']!=pin['handler'] or rows[nm].get('kind')!='card':raise ValueError('independent complete Oracle/binding changed: '+nm)
   if rows[nm]['handler'] in ('NULL','card_dummy') and not pin.get('reviewed_native_witness'):raise ValueError('unimplemented/unreviewed witness: '+nm)

class _HandWorld(_CountWorld):
 def __init__(self,*a):super().__init__(*a);self.masked=set()
 def receipts(self):
  super().receipts()
  for e in self.permanents.values():
   if e['zone']=='battlefield' and 'Artifact' in self.o[e['name']]['type_line'] and 'Creature' in self.o[e['name']]['type_line']:
    self.emit('assert_matching_permanents',player=e['player'],index=self.rows[e['name']]['csvid'],property='artifact',value=1)
 def add(self,nm,ref,pl,zone='battlefield',source=False):
  _UpkeepWorld.add(self,nm,ref,pl,zone,source=source)
  if source:self.configs[ref]=classify(self.o[nm])
 def cast_card(self,nm,ref,pl,zone='graveyard',dialog=None,target=None):
  old=[x for x,e in self.permanents.items() if e['zone']=='battlefield' and 'World' in self.o[e['name']]['type_line'].split()] if 'World' in self.o[nm]['type_line'].split() else []
  super().cast_card(nm,ref,pl,zone,dialog,target)
  for x in old:
   e=self.permanents[x];self.zones['battlefield'][e['player']].remove(e['name']);self.zones['graveyard'][e['player']].append(e['name']);e['zone']='graveyard'
 def predicate(self,p,pl):
  d=p['predicate'];n=len(self.zones['hand'][pl if d['basis']=='controller' else self.current])
  return d['kind']=='always' or d['kind']=='le' and n<=d['low'] or d['kind']=='ge' and n>=d['low'] or d['kind']=='outside' and not d['low']<=n<=d['high']
 def amount(self,p,pl,recipient):
  a,b,c=p['formula'];return max(0,a*len(self.zones['hand'][pl])+b*len(self.zones['hand'][recipient])+c)
 def visit(self):
  if any(any(v) for v in self.mana):raise ValueError('real upkeep requires spent pools')
  self.emit('advance_turn');self.expire();self.current=1-self.current;self.emit('untap_step',player=self.current)
  for e in self.entities.values():
   if e['zone']=='battlefield' and e['player']==self.current:e['sick']=False;e['tapped']=False
  self.receipts();cut=len(self.steps);live=[]
  for ref in self.sources:
   e=self.permanents[ref];p=self.configs[ref];pl=e['player']
   if e['zone']=='battlefield' and (p['gate']=='each' or (self.current==pl)==(p['gate']=='own')) and self.predicate(p,pl):live.append(ref)
  for ref in live:
   p=self.configs[ref];pl=self.permanents[ref]['player']
   if p['recipient']=='opponent_or_planeswalker':
    if self.target is None:self.emit('choose_player',player=pl,value=1-pl)
    elif self.cancel_target:self.emit('choose_optional_target',player=pl,ref=self.target)
    else:self.emit('choose_target',player=pl,ref=self.target)
  self.emit('upkeep_step',player=self.current)
  if self.cancel_target and live:self.emit('assert_forced_target_choices',value=1)
  for ref in live:
   p=self.configs[ref];pl=self.permanents[ref]['player'];recipient=pl if p['recipient']=='controller' else self.current if p['recipient']=='active' else 1-pl;n=self.amount(p,pl,recipient)
   if p['effect']=='gain':self.life[recipient]+=n
   elif p['effect']=='loss':self.life[recipient]-=n
   elif p['effect']=='mill':
    n=min(n,len(self.zones['library'][recipient]));self.zones['graveyard'][recipient].extend(self.zones['library'][recipient][:n]);del self.zones['library'][recipient][:n]
   elif p['recipient']=='opponent' and recipient in self.masked:pass
   elif p['recipient']=='opponent_or_planeswalker' and self.target is not None:self.hit_walker(self.target,n)
   else:self.life[recipient]-=max(0,n-int(recipient in self.armored))
  self.receipts();self.emit('turn',player=self.current,value=0);self.receipts();return cut
 def instant_draw(self,ref,recipient):
  e=self.permanents.get(ref);nm='Ancestral Recall';pl=self.owner
  from recipes import cost_from_oracle
  self.pool(pl,cost_from_oracle(self.o[nm]));self.emit('choose_player',player=pl,value=recipient);self.emit('cast',ref=ref,value=1);self.emit('assert_stack',value=1);self.emit('resolve');self.mana[pl]=[0]*6
  self.zones['hand'][pl].remove(nm);self.zones['graveyard'][pl].append(nm);self.draw(recipient,3);self.receipts()
 def discard_spell(self):
  from recipes import cost_from_oracle
  self.pool(self.owner,cost_from_oracle(self.o['Mind Rot']));self.emit('choose_player',player=self.owner,value=1-self.owner);self.emit('cast',ref='discard_spell',value=1);self.emit('assert_stack',value=1);self.emit('resolve');self.mana[self.owner]=[0]*6
  self.zones['hand'][self.owner].remove('Mind Rot');self.zones['graveyard'][self.owner].append('Mind Rot');hand=self.zones['hand'][1-self.owner]
  for _ in range(min(2,len(hand))):
   if set(hand)!={'Forest'}:raise ValueError('discard policy needs homogeneous ordinary control')
   self.zones['graveyard'][1-self.owner].append(hand.pop())
  self.receipts()

def expand(name,csvid,d,p,selected,rows,o):
 from recipes import step
 if not selected or len(set(selected))!=len(selected) or any(v not in variants(p) for v in selected):raise ValueError('explicit unique released hand-value variants required')
 validate_witnesses(o,rows);out=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner;w=_HandWorld(name,p,owner,rows,o);w.add(name,'source',owner,'hand',source=True)
  if mode in ('two_sources','world_own','world_enemy'):w.add(name,'copy',enemy if mode=='world_enemy' else owner,'hand',source=True)
  if mode in ('draw_own','draw_opponent'):w.add('Ancestral Recall','draw_spell',owner,'hand')
  if mode=='discard_before':w.add('Mind Rot','discard_spell',owner,'hand')
  if mode=='blocked':w.add('Grizzly Bears','blocker',enemy)
  armor_recipient=owner if p['recipient']=='controller' or p['world'] else enemy
  if mode=='armor':w.add("Urza's Armor",'armor',armor_recipient,'hand')
  if mode=='protected_opponent':w.add('Ivory Mask','mask',enemy,'hand')
  walker_player=owner if mode=='target_own_walker' else enemy
  if mode in ('target_own_walker','target_enemy_walker','mandatory_cancel'):
   w.add('Tibalt, the Fiend-Blooded','walker',walker_player,'hand');w.walkers['walker']=dict(name='Tibalt, the Fiend-Blooded',player=walker_player,zone='hand',loyalty=0);w.target='walker';w.cancel_target=mode=='mandatory_cancel'
  desired=[2,2]
  if p['effect']=='gain':desired[owner]=7 if p['predicate']['kind']!='always' else 5
  if p['recipient']=='opponent':desired[owner]=5
  if p['effect']=='loss':desired[owner]=2;desired[enemy]=5
  # A released profile's ordinary positive control must satisfy its printed
  # hand gate. Existing pilot defaults already do; contrary variants below
  # still override these counts independently.
  predicate=p['predicate'];subject=owner if predicate['basis']=='controller' else enemy
  if predicate['kind']=='ge':desired[subject]=max(desired[subject],predicate['low'])
  if predicate['kind']=='le':desired[subject]=min(desired[subject],predicate['low'])
  a,b,c=p['formula'];recipient=owner if p['recipient']=='controller' else enemy
  if predicate['kind']=='always' and a*desired[owner]+b*desired[recipient]+c<=0:
   choices=[(x,y) for x in range(8) for y in range(8) if a*x+b*(x if recipient==owner else y)+c>0]
   if not choices:raise ValueError('positive hand fixture needs an explicit new composition review')
   x,y=min(choices,key=lambda xy:(abs(xy[0]-desired[owner])+abs(xy[1]-desired[enemy]),xy))
   desired[owner]=x;desired[enemy]=y
  counts={'hand_zero':0,'hand_one':1,'hand_three':3,'hand_four':4,'hand_five':5,'hand_seven':7,'equal_hands':3}
  if mode in counts:desired=[counts[mode]]*2
  if mode=='own_more':desired[owner]=5;desired[enemy]=2
  if mode=='opponent_more':desired[owner]=2;desired[enemy]=5
  if mode=='draw_own':desired[owner]=6
  if mode=='draw_opponent':desired[enemy]=6
  if mode in ('source_in_hand','source_enters_after'):desired[owner]=min(desired[owner],6)
  for pl in (0,1):
   leftover=1 if pl==owner and mode in ('draw_own','draw_opponent','discard_before') else 0
   for i in range(desired[pl]-leftover):w.add('Forest',f'hand_{pl}_{i}',pl,'hand')
   if mode!='bare_zones':w.add('Island',f'grave_{pl}',pl,'graveyard');w.add('Swamp',f'exile_{pl}',pl,'exile')
   library=['Forest','Island','Plains','Swamp','Mountain','Grizzly Bears']*4
   if mode in ('empty_library','short_library','exact_library'):library=library[:0 if mode=='empty_library' else 1 if mode=='short_library' else desired[pl]]
   for i,nm in enumerate(library):w.add(nm,f'library_{pl}_{i}',pl,'library')
  if mode=='cast_cancel':
   for color,count in enumerate(p['mana']):
    for i in range(count):w.add(('Forest','Swamp','Island','Forest','Mountain','Plains')[color],f'payment_land_{color}_{i}',owner)
  prep_enemy=mode=='protected_opponent' or mode=='armor' and armor_recipient==enemy or mode in ('target_enemy_walker','mandatory_cancel')
  w.current=enemy if prep_enemy else owner;w.emit('turn',player=w.current,value=0);w.emit('discard_policy',value=1);w.pool(0,[0]*6);w.pool(1,[0]*6);w.emit('refresh');w.receipts()
  if mode=='armor':w.cast_named('armor');w.armored.add(armor_recipient)
  if mode=='protected_opponent':w.cast_named('mask');w.masked.add(enemy)
  if mode in ('target_own_walker','target_enemy_walker','mandatory_cancel'):w.cast_walker('walker')
  if prep_enemy:w.visit()
  if mode=='cast_cancel':
   for c in w.setup:
    if c['ref'].startswith('payment_land_'):w.emit('require_can_activate',ref=c['ref'],value=1)
   w.emit('choose_cancel_mana',player=0);w.emit('cast',ref='source',value=0);w.emit('assert_unused_choices',value=0);w.receipts()
  elif mode not in ('source_in_hand','source_enters_after'):w.cast_named('source',mode if mode in ('insufficient_mana','wrong_color') else None)
  if mode not in ('cast','insufficient_mana','wrong_color','cast_cancel'):
   if mode in ('two_sources','world_own'):w.cast_named('copy')
   if mode=='source_absent':w.destroy('source')
   if mode=='draw_opponent':w.instant_draw('draw_spell',enemy)
   if mode=='discard_before':w.discard_spell()
   ready_cut=len(w.steps);untap_cut=w.visit()
   if mode=='world_enemy':w.cast_named('copy')
   if mode=='draw_own':w.instant_draw('draw_spell',owner)
   if mode not in ('opposite_visit','draw_opponent'):untap_cut=w.visit()
   if mode=='source_enters_after':w.cast_named('source');w.visit();w.visit()
   if mode=='source_destroyed_after':w.destroy('source');w.visit();w.visit()
   if mode=='repeat':w.visit();w.visit()
   if mode in ('unblocked','blocked'):w.combat(w.declare(['source']),[('blocker','source')] if mode=='blocked' else [])
  w.receipts();seq=w.steps
  if mode in ('ready_snapshot','untap_snapshot'):
   cut=ready_cut if mode=='ready_snapshot' else untap_cut;tail=seq[cut:];seq=seq[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if mode=='resolved_snapshot':
   cut=len(w.steps);w.receipts();checks=w.steps[cut:];seq=w.steps[:cut]+[step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+checks+[step('assert_recorded_state')]
  out.append(dict(name=f'UpkeepHandValueRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],ui_annotations='provisional_until_capture',untested_components=['Pending-trigger hand changes and target announcement timing: no invented inline priority','Arbitrary rejected-target introspection, simultaneous World entries, multiplayer and terminal life loss','Each real upkeep visit included; above-seven own hand uses legal off-turn instant draw','Follow11 profiles/homes held until pilot paired calibration'])) )
 return out
