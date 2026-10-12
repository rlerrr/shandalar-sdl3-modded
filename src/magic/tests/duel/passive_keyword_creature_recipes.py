"""Unconditional keyword components beside explicitly pinned inactive abilities."""
import copy,re
from collections import Counter
from creature_recipes import classify, variants as keyword_variants, expand as keyword_cases
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

def split(r):
 abilities=[];passives=[]
 for line in r.get('oracle_text','').splitlines():
  if re.match(r'^(?:\{[^}]+\}|Sacrifice |Discard |Remove |Exile |Pay |Tap |Untap |Return )',line) and ':' in line:abilities.append(line)
  else:passives.append(line)
 if not abilities:raise ValueError('passive keyword component needs explicit additional activated abilities')
 return abilities,dict(r,oracle_text='\n'.join(passives))

def spec(d,r):
 if not r or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('whole Oracle pin and explicit keyword component variants required')
 abilities,plain=split(r);p=classify(plain)
 if not p['keywords']:raise ValueError('keyword component must test at least one unconditional keyword')
 if d.get('params')!={'keywords':p['keywords'],'activated_abilities':abilities}:raise ValueError('explicit keywords and untested activated clauses must match whole Oracle text')
 return dict(p,family='passive_keyword_creature',remaining_abilities=abilities)

def variants(p):
 return [v+'_'+str(owner) for owner in (0,1) for v in keyword_variants(p)]

def expand(name,csvid,d,p,selected,registry,oracle):
 from recipes import step
 base=list(dict.fromkeys(v.rsplit('_',1)[0] for v in selected))
 cases=keyword_cases(name,csvid,d,p,base,registry);result=[]
 for requested in selected:
  v,owner=requested.rsplit('_',1);owner=int(owner);c=copy.deepcopy(next(c for c in cases if c['_recipe']['variant']==v))
  hands=[list(HAND),list(HAND)];graves=[list(GRAVE),list(GRAVE)]
  if v=='insufficient_mana':hands[0].append(name)
  if v in ('first_strike','deathtouch'):
   graves[1].append('Llanowar Elves' if v=='first_strike' else 'Scaled Wurm')
   if v=='deathtouch' and not (set(p['keywords'])&{'First strike','Double strike'}) and p['toughness']<=7:graves[0].append(name)
  if v=='trample' and p['power']*(2 if 'Double strike' in p['keywords'] else 1)>=(1 if 'Deathtouch' in p['keywords'] else 2):graves[1].append('Ornithopter')
  # Check receipt and absence separately, rather than letting a wrong-zone card
  # satisfy a combined hand/graveyard count.
  universe=set(HAND+GRAVE+EXILE+LIBRARY+(name,'Llanowar Elves','Scaled Wurm','Ornithopter','Suntail Hawk','Grizzly Bears','Giant Spider'))
  checks=[]
  for player in (0,1):
   for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',EXILE),('library',LIBRARY)):
    checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
    if zone=='library':checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(names)]
    else:
     counts=Counter(names);checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=registry[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
  # Include zone checks inside each replay so snapshot tests verify them twice.
  if v=='snapshot':
   cut=next(i for i,s in enumerate(c['steps']) if s['op']=='record_state');c['steps'][cut:cut]=copy.deepcopy(checks)
   cut=next(i for i,s in enumerate(c['steps']) if s['op']=='assert_recorded_state');c['steps'][cut:cut]=copy.deepcopy(checks)
  else:c['steps']+=checks
  if owner:
   # Snapshot replays share some dictionaries in the base recipe. Give each
   # occurrence its own record so mirroring does not flip one object twice.
   c['steps']=[dict(s) for s in c['steps']]
   for item in c['setup']:item['player']=1-item['player']
   for action in c['steps']:
    if 'player' in action:action['player']=1-action['player']
   c['steps'].insert(0,step('turn',player=1,value=0))
  c['name']=f'PassiveKeywordCreatureRecipe_{csvid}_{requested}';c['_recipe'].update(variant=requested,untested_abilities=p['remaining_abilities'],untested_components=['activated abilities','Flash timing','keyword removal/granting','control changes','additional combat interactions'])
  result.append(c)
 return result
