"""Whole-Oracle single-X targeted damage, with exact colored payment receipts."""
import copy,re
from damage_recipes import expand as damage,validate_witnesses
VALUES=(0,1,2,3,5,8)


def spec(d,r):
 from recipes import cost_from_oracle
 if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r.get('type_line') not in ('Instant','Sorcery') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d or r.get('keywords'):raise ValueError('ordinary whole-Oracle variable damage spell required')
 p=d.get('params')
 if not isinstance(p,dict) or set(p)!={'targets','gain_life','x_color'} or p['targets'] not in (['creature'],['creature','player']) or type(p['gain_life']) is not bool or p['x_color'] not in ('generic','black'):raise ValueError('explicit target scope, X life receipt and X mana restriction required')
 noun='target creature' if p['targets']==['creature'] else 'any target';text=r['name']+' deals X damage to '+noun+'.'
 if p['gain_life']:texts={text+' You gain X life.',text[:-1]+' and you gain X life.'}
 else:texts={text}
 if p['x_color']=='black':texts={'Spend only black mana on X. '+s for s in texts}
 if ' '.join(r['oracle_text'].splitlines()) not in texts:raise ValueError('variable damage parameters must match the complete printed effect')
 cost=r['mana_cost']
 if cost.count('{X}')!=1 or not re.fullmatch(r'(?:\{(?:X|[1-9][0-9]?|[WUBRGC])\})+',cost):raise ValueError('one X and ordinary fixed base mana required')
 base=cost_from_oracle(dict(mana_cost=cost.replace('{X}','')))
 return dict(p,family='variable_damage',base_mana=base,instant=r['type_line']=='Instant')


def variants(p):
 vs=[(v,x) for x in VALUES for v in ('survivor','payment')]
 vs += [('lethal',x) for x in VALUES if x]
 if 'player' in p['targets']:vs += [(v,x) for x in VALUES for v in ('opponent_player','self_player')]
 vs += [('snapshot',x) for x in (0,5)]
 extra=['friendly','lost_target','indestructible','artifact','insufficient_mana','regenerate']
 if 'player' not in p['targets']:extra+=['empty','wrong_type']
 if p['instant']:extra+=['other_turn']
 vs += [(v,3) for v in extra]
 return [f'{v}_x{x}_{owner}' for owner in (0,1) for v,x in vs]


def expand(name,csvid,d,p,selected,rows,o):
 from recipes import step
 out=[]
 for variant in selected:
  v,tail=variant.rsplit('_x',1);x,owner=map(int,tail.split('_'));mana=list(p['base_mana']);mana[1 if p['x_color']=='black' else 0]+=x
  q=dict(family='damage_spell',mana=mana,amount=x,targets=p['targets'],gain_life=x if p['gain_life'] else 0)
  c=damage(name,csvid,d,q,[v],rows,o)[0];negative=v in ('empty','wrong_type','insufficient_mana');steps=[]
  raw=c['steps']
  if v=='lost_target':
   # Cast the real instant onto the pending spell. The legacy automatic
   # response selector cannot nominate this same-controller response here.
   raw=[s for s in raw if s['op']!='choose_priority' and not (s['op']=='choose_target' and s.get('player')==-1)]
   at=next(i for i,s in enumerate(raw) if s['op']=='assert_stack' and s['value']==1)+1
   raw[at:at]=[step('choose_target',player=0,ref='recipient'),step('cast',ref='response',value=1),step('assert_stack',value=2)]
  for s in raw:
   if v=='regenerate' and s['op']=='choose_priority':
    s=step('choose_regeneration',ref=s['ref'])
   if s['op']=='cast' and s.get('ref')=='source' and not negative:
    at=len(steps)
    while at and steps[at-1]['op'].startswith('choose_'):at-=1
    steps.insert(at,step('choose_x',player=0,value=x))
   steps.append(s)
  pool=[0]*6 if v=='insufficient_mana' else list(mana) if v=='payment' else [10]*6
  if not negative:
   pool=[a-b for a,b in zip(pool,mana)]
   if v=='lost_target':pool[2]-=1
   if v=='regenerate':pool[1]-=1
  receipt=[step('assert_mana',player=0,color=i,value=n) for i,n in enumerate(pool)];checked=[]
  for s in steps:
   if s['op'] in ('record_state','assert_recorded_state'):checked.extend(copy.deepcopy(receipt))
   checked.append(s)
  if v!='snapshot':checked.extend(receipt)
  c['steps']=checked
  if owner:
   c['steps']=[dict(s) for s in c['steps']]
   for item in c['setup']+c['steps']:
    if item.get('player') in (0,1):item['player']=1-item['player']
    if item.get('op')=='choose_player':item['value']=1-item['value']
   c['steps'].insert(0,step('turn',player=1,value=0))
  c['name']=f'VariableDamageRecipe_{csvid}_{variant}'
  c['_recipe']=dict(family='variable_damage',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],x=x,untested_components=['planeswalkers','alternative costs','damage replacement/prevention and triggered effects','multiplayer'])
  out.append(c)
 return out
