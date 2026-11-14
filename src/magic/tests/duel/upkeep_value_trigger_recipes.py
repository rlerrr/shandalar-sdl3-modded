"""Fixed whole-Oracle upkeep resources through the complete production phase step."""
import copy,json,re
from pathlib import Path
from battalion_trigger_recipes import _BattalionWorld

FAMILY='upkeep_value_trigger'
WORDS={'a':1,'one':1,'two':2,'three':3}
def _profiles():return json.loads((Path(__file__).parent/'data/upkeep_value_trigger_wave8_profiles.json').read_text(encoding='utf8'))

def classify(r):
 from creature_recipes import oracle_keywords
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r.get('type_line','').startswith('Token '):raise ValueError('whole normal standalone upkeep permanent required')
 if not any(k in r.get('type_line','') for k in ('Creature','Enchantment','Artifact')) or 'Aura' in r['type_line']:raise ValueError('unsupported upkeep permanent/attachment')
 lines=r.get('oracle_text','').splitlines();phase=[x for x in lines if re.match(r'^At the beginning of (?:your upkeep|each upkeep|each player\'s upkeep|each opponent\'s upkeep), ',x)]
 if len(phase)!=1:raise ValueError('one complete fixed-value upkeep clause required')
 line=phase[0];gate,body=re.fullmatch(r'At the beginning of ([^,]+), (.+)',line).groups();gate='own' if gate=='your upkeep' else 'opponent' if gate=="each opponent's upkeep" else 'each'
 other=[x for x in lines if x!=line]
 if 'Creature' in r['type_line']:keys=oracle_keywords(dict(r,oracle_text='\n'.join(other)))
 elif not other and not r.get('keywords'):keys=[]
 else:raise ValueError('additional whole-Oracle source clauses not calibrated')
 if set(keys)-{'Flying'}:raise ValueError('source passive needs full independent receipt branch')
 actions=[];optional=False
 def action(kind,recipient,n):actions.append(dict(kind=kind,recipient=recipient,amount=n))
 m=re.fullmatch(r'you (may )?gain ([1-3]) life\.',body)
 if m:optional=bool(m[1]);action('gain','controller',int(m[2]))
 else:
  m=re.fullmatch(r'you lose ([1-3]) life\.',body)
  if m:action('loss','controller',int(m[1]))
  else:
   m=re.fullmatch(r'you draw (a|one|two|three) cards? and (?:you )?lose ([1-3]) life\.',body)
   if m:action('draw','controller',WORDS[m[1]]);action('loss','controller',int(m[2]))
   else:
    m=re.fullmatch(r'that player loses ([1-3]) life and draws (a|one|two|three) cards?\.',body)
    if m:action('loss','active',int(m[1]));action('draw','active',WORDS[m[2]])
    else:
     m=re.fullmatch(r'target player draws (a|one|two|three) cards? and loses ([1-3]) life\.',body)
     if m:action('draw','target',WORDS[m[1]]);action('loss','target',int(m[2]))
     else:
      m=re.fullmatch(r'each opponent loses ([1-3]) life and you gain ([1-3]) life\.',body)
      if m:action('loss','opponent',int(m[1]));action('gain','controller',int(m[2]))
      else:
       m=re.fullmatch(r'(?:this creature|this artifact|this enchantment) deals ([1-3]) damage to (you|that player|each player)\.',body)
       if m:action('damage',{'you':'controller','that player':'active','each player':'each'}[m[2]],int(m[1]))
       else:
        m=re.fullmatch(r'each opponent draws (a|one|two|three) cards?\.',body)
        if m:action('draw','opponent',WORDS[m[1]])
 if not actions:raise ValueError('retain nonfixed/token/counter/maintenance upkeep for adjacent models')
 cost_from_oracle(r);return dict(gate=gate,optional=optional,actions=actions,keywords=keys)

def spec(d,r):
 from recipes import cost_from_oracle
 if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not r or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('pinned version2 upkeep declaration required')
 p=d.get('params');expected=classify(r)
 if (type(p) is not dict or p!=expected or set(p)!=set(expected) or type(p['gate']) is not str or type(p['optional']) is not bool or type(p['keywords']) is not list or any(type(x) is not str for x in p['keywords'])
     or type(p['actions']) is not list or any(type(a) is not dict or set(a)!={'kind','recipient','amount'} or type(a['kind']) is not str or type(a['recipient']) is not str or type(a['amount']) is not int for a in p['actions'])):raise ValueError('exact typed Oracle action order/recipients required')
 pin=_profiles()['sources'].get(r['name'])
 if pin is None or pin['oracle']!=r:raise ValueError('independent complete source pin required')
 ui=pin['ui'];target=any(a['recipient']=='target' for a in p['actions'])
 if (set(ui)!={'status','trigger_accept_players','target_choice_players','ai_target_policy'} or ui['status'] not in ('provisional_until_capture','calibrated')
     or any(type(ui[k]) is not list or any(type(v) is not int or v not in (0,1) for v in ui[k]) or len(set(ui[k]))!=len(ui[k]) for k in ('trigger_accept_players','target_choice_players'))
     or ui['ai_target_policy']!=('own_unless_opponent_library_short' if target else '') or not target and ui['target_choice_players']):raise ValueError('reviewed presentation contract required')
 return dict(copy.deepcopy(p),family=FAMILY,effect='upkeep_value',entry_energy=0,mana=cost_from_oracle(r),legendary='Legendary' in r['type_line'],creature='Creature' in r['type_line'],ui=copy.deepcopy(ui))

def variants(p):
 values=['cast','insufficient_mana','effect','opponent_upkeep','source_absent','source_enters_after','source_destroyed_after','repeat','armor','ready_snapshot','untap_snapshot','resolved_snapshot','bare_zones']
 if any(p['mana'][1:]):values+=['wrong_color']
 if not p['legendary']:values+=['two_sources']
 if any(a['kind']=='draw' for a in p['actions']):values+=['short_library']
 if any(a['recipient']=='target' for a in p['actions']):values+=['target_opponent']
 if p['creature']:values+=['unblocked']
 if 'Flying' in p['keywords']:values+=['flying_ground','flying_reach','flying_blocker','defend_flying']
 return [f'{v}_{owner}' for owner in (0,1) for v in values]+(['decline_0'] if p['optional'] else [])+['cast_cancel_0']

def validate_witnesses(o,rows):
 for name,pin in _profiles()['witnesses'].items():
  if o.get(name)!=pin['oracle'] or name not in rows or rows[name]['handler']!=pin['handler'] or rows[name].get('kind')!='card':raise ValueError('whole upkeep witness/binding changed: '+name)
  if rows[name]['handler']=='NULL' or rows[name]['handler']=='card_dummy' and not pin.get('reviewed_native_witness'):raise ValueError('unimplemented/unreviewed upkeep witness: '+name)

class _UpkeepWorld(_BattalionWorld):
 def __init__(self,*args):super().__init__(*args);self.permanents={};self.sources=[];self.armored=set()
 def add(self,nm,ref,pl,zone='battlefield',source=False):
  super().add(nm,ref,pl,zone,source=source)
  if zone in ('hand','battlefield') and any(t in self.o[nm]['type_line'] for t in ('Creature','Artifact','Enchantment','Land')):self.permanents[ref]=dict(name=nm,player=pl,zone=zone)
  if source:self.sources.append(ref)
 def receipts(self):
  super().receipts()
  for ref,e in self.permanents.items():
   if ref in self.entities:continue
   if e['zone']=='battlefield':self.emit('assert_card',ref=ref,property='in_play',value=1);self.emit('assert_card',ref=ref,property='tapped',value=0)
 def cast_named(self,ref,denied=None):
  from recipes import cost_from_oracle
  e=self.permanents[ref];pl=e['player'];price=cost_from_oracle(self.o[e['name']])
  if denied:
   if denied=='insufficient_mana':price=[0]*6
   else:color=next(i for i in range(1,6) if price[i]);price[color]-=1;price[0]+=1
   self.pool(pl,price);self.emit('cast',ref=ref,value=0);self.emit('assert_choices_consumed');self.receipts();return
  self.cast_card(e['name'],ref,pl,'battlefield');e['zone']='battlefield'
  if ref in self.entities:self.entities[ref]['zone']='battlefield';self.entities[ref]['sick']=True
  self.receipts()
 def leave(self,ref,zone):
  super().leave(ref,zone)
  if ref in self.permanents:self.permanents[ref]['zone']=zone
 def destroy(self,ref):
  self.emit('destroy',ref=ref)
  if ref in self.entities:self.leave(ref,'graveyard')
  else:
   e=self.permanents[ref];self.zones['battlefield'][e['player']].remove(e['name']);self.zones['graveyard'][e['player']].append(e['name']);e['zone']='graveyard'
  self.receipts()
 def active(self):return self.p['gate']=='each' or (self.current==self.owner)==(self.p['gate']=='own')
 def recipient(self,action,target):return [self.owner] if action['recipient']=='controller' else [self.current] if action['recipient']=='active' else [target] if action['recipient']=='target' else [1-self.owner] if action['recipient']=='opponent' else [0,1]
 def visit(self,mode='effect',target=None):
  if any(any(v) for v in self.mana):raise ValueError('real upkeep requires spent pools')
  self.emit('advance_turn');self.expire();self.current=1-self.current;self.emit('untap_step',player=self.current)
  for e in self.entities.values():
   if e['zone']=='battlefield' and e['player']==self.current:e['sick']=False;e['tapped']=False
  self.receipts();cut=len(self.steps);live=[ref for ref in self.sources if self.permanents[ref]['zone']=='battlefield'] if self.active() else []
  target=self.owner if target is None else target
  for ref in live:
   if self.p['optional'] and mode=='decline':continue
   if self.owner in self.p['ui']['trigger_accept_players']:self.emit('choose_trigger_source',player=self.owner,ref=ref)
   if self.owner in self.p['ui']['target_choice_players']:self.emit('choose_player',player=self.owner,value=target)
  self.emit('upkeep_step',player=self.current)
  for ref in live:
   if self.p['optional'] and mode=='decline':continue
   for action in self.p['actions']:
    for pl in self.recipient(action,target):
     n=action['amount']
     if action['kind']=='draw':
      if len(self.zones['library'][pl])<n:raise ValueError('terminal empty-library draw requires separate game-loss support')
      self.zones['hand'][pl].extend(self.zones['library'][pl][:n]);del self.zones['library'][pl][:n]
     elif action['kind']=='gain':self.life[pl]+=n
     elif action['kind']=='loss':self.life[pl]-=n
     else:self.life[pl]-=max(0,n-int(pl in self.armored))
  self.receipts();self.emit('turn',player=self.current,value=0);self.receipts();return cut
 def declare(self,refs):
  self.emit('begin_attackers');attackers=[]
  for ref in refs:
   e=self.entities[ref];legal=e['zone']=='battlefield' and e['player']==self.current and not e['sick'] and not e['tapped'];self.emit('attack',ref=ref,value=int(legal))
   if legal:attackers.append(ref);e['tapped']=True;e['attacking']=True
  self.emit('finish_attackers');self.emit('assert_choices_consumed');self.receipts();return attackers

def expand(name,csvid,d,p,selected,rows,o):
 from recipes import step
 if not selected or len(set(selected))!=len(selected) or any(v not in variants(p) for v in selected):raise ValueError('explicit unique upkeep variants required')
 validate_witnesses(o,rows);out=[]
 for variant in selected:
  mode,owner=variant.rsplit('_',1);owner=int(owner);enemy=1-owner;w=_UpkeepWorld(name,p,owner,rows,o);w.add(name,'source',owner,'hand',source=True)
  if mode=='two_sources':w.add(name,'copy',owner,'hand',source=True)
  if mode=='armor':w.add("Urza's Armor",'armor',owner,'hand')
  if mode.startswith('flying') or mode in ('unblocked','defend_flying'):
   w.add('Grizzly Bears','ground',enemy);w.add('Giant Spider','reach',enemy);w.add('Air Elemental','flying',enemy)
  if mode=='cast_cancel':
   for color,count in enumerate(p['mana']):
    for i in range(count):w.add(('Forest','Swamp','Island','Forest','Mountain','Plains')[color],f'payment_land_{color}_{i}',owner)
  for pl in (0,1):
   if mode!='bare_zones':w.add('Forest',f'hand_{pl}',pl,'hand');w.add('Lightning Bolt',f'grave_{pl}',pl,'graveyard');w.add('Island',f'exile_{pl}',pl,'exile')
   library=['Forest','Island','Plains','Swamp','Mountain','Grizzly Bears']*2
   if mode=='short_library':library=library[:2]
   if mode=='target_opponent' and owner==1 and pl==enemy:library=library[:1]
   for i,nm in enumerate(library):w.add(nm,f'library_{pl}_{i}',pl,'library')
  w.emit('turn',player=owner,value=0);w.pool(0,[0]*6);w.pool(1,[0]*6);w.emit('refresh');w.receipts()
  if mode=='armor':w.cast_named('armor');w.armored.add(owner)
  if mode=='cast_cancel':
   refs=[x['ref'] for x in w.setup if x['ref'].startswith('payment_land_')]
   for ref in refs:w.emit('require_can_activate',ref=ref,value=1)
   w.emit('choose_cancel_mana',player=0);w.emit('cast',ref='source',value=0);w.emit('assert_unused_choices',value=0);w.receipts()
  elif mode!='source_enters_after':w.cast_named('source',mode if mode in ('insufficient_mana','wrong_color') else None)
  ready_cut=len(w.steps);untap_cut=0
  if mode not in ('cast','insufficient_mana','wrong_color','cast_cancel'):
   if mode=='two_sources':w.cast_named('copy')
   if mode=='source_absent':w.destroy('source')
   target=enemy if mode=='target_opponent' else owner
   w.visit(mode,target)
   if mode not in ('opponent_upkeep','defend_flying'):untap_cut=w.visit(mode,target)
   if mode=='source_enters_after':w.cast_named('source')
   if mode=='source_destroyed_after':w.destroy('source');w.visit();w.visit()
   if mode=='repeat':w.visit(mode,target);w.visit(mode,target)
   if mode in ('unblocked','flying_ground','flying_reach','flying_blocker','defend_flying'):
    attackers=w.declare(['flying'] if mode=='defend_flying' else ['source']);pairs=[('source','flying')] if mode=='defend_flying' else [('ground','source')] if mode=='flying_ground' else [('reach','source')] if mode=='flying_reach' else [('flying','source')] if mode=='flying_blocker' else [];w.combat(attackers,pairs)
  w.receipts();seq=w.steps
  if mode in ('ready_snapshot','untap_snapshot'):
   cut=ready_cut if mode=='ready_snapshot' else untap_cut;tail=seq[cut:];seq=seq[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
  if mode=='resolved_snapshot':
   cut=len(w.steps);w.receipts();checks=w.steps[cut:];seq=w.steps[:cut]+[step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+checks+[step('assert_recorded_state')]
  out.append(dict(name=f'UpkeepValueTriggerRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=seq,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],ui_annotations='provisional_until_capture',untested_components=['terminal empty-library/life-zero game-loss observation','additional-upkeep generators and arbitrary phase reentry','multiplayer','AI optional decline policy; human actual offers only','Bloodgift AI legal target strategy is provisional presentation evidence, not an Oracle target preference','legend-rule duplicate selection on legendary sources'])) )
 return out
