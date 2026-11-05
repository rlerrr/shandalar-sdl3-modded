import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from value_flow_recipes import variants


def operation(kind,amount,scope='recipient'):
 return dict(kind=kind,amount=amount,scope=scope,random=False)


class ValueFlowRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
  cls.rows={r['name']:r for r in registry()}

 def declaration(self,name,selected=None):
  configs={
   'Sign in Blood':dict(recipient='any',operations=[operation('draw',2),operation('lose_life',2)]),
   "Heroes' Reunion":dict(recipient='any',operations=[operation('gain_life',7)]),
   'Soul Feast':dict(recipient='any',operations=[operation('lose_life',4),operation('gain_life',4,'self')]),
   'Vision Skeins':dict(recipient='both',operations=[operation('draw',2)]),
   'Cloudblazer':dict(mode='entry',operations=[operation('gain_life',2),operation('draw',2)],keywords=['Flying']),
   'Baleful Strix':dict(mode='entry',operations=[operation('draw',1)],keywords=['Deathtouch','Flying']),
   'Runed Servitor':dict(mode='death',recipient='both',operations=[operation('draw',1)]),
   'Mindeye Drake':dict(mode='death',recipient='any',operations=[operation('mill',5)],keywords=['Flying']),
   'Vampire Neonate':dict(mode='activation',recipient='opponents',operations=[operation('lose_life',1),operation('gain_life',1,'self')],mana='{2}',tap=True),
   'Bile Urchin':dict(mode='activation',recipient='any',operations=[operation('lose_life',1)],sacrifice=True),
   'Armistice':dict(mode='activation',recipient='opponent',operations=[operation('draw',1,'self'),operation('gain_life',3)],mana='{3}{W}{W}'),
   'Nephalia Drownyard':dict(mode='activation',recipient='any',operations=[operation('mill',3)],mana='{1}{U}{B}',tap=True,mana_ability=True),
  }
  p=dict(mode='spell',recipient='self',operations=[],cost=dict(mana='',tap=False,sacrifice=False,pay_life=0,discard=0),keywords=[],mana_ability=False)
  for k,v in configs[name].items():
   if k in p['cost']:p['cost'][k]=v
   else:p[k]=v
  return dict(family='value_flow',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['effect_0'])

 def cases(self,name,selected=None,d=None,o=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,oracle_context=o or self.oracle)

 def has(self,steps,**fields):
  return any(all(s.get(k)==v for k,v in fields.items()) for s in steps)

 def test_whole_text_order_recipient_and_amount_must_match(self):
  for patch in (dict(recipient='self'),dict(operations=[operation('lose_life',2),operation('draw',2)]),dict(operations=[operation('draw',3),operation('lose_life',2)])):
   d=self.declaration('Sign in Blood');d['params'].update(patch)
   with self.assertRaises(ValueError):self.cases('Sign in Blood',d=d)

 def test_discard_effects_and_costs_belong_to_the_separate_family(self):
  for cost in (False,True):
   d=self.declaration('Sign in Blood')
   if cost:d['params']['cost']['discard']=1
   else:d['params']['operations'].append(dict(kind='discard',amount=1,scope='recipient',random=False))
   with self.assertRaises(ValueError):self.cases('Sign in Blood',d=d)

 def test_targeted_draw_and_life_loss_change_only_the_target(self):
  s=self.cases('Sign in Blood')[0]['steps']
  for pl,life,hand,library in ((0,50,4,28),(1,48,6,26)):
   for op,extra,value in (('assert_life',{},life),('assert_zone',dict(zone='hand'),hand),('assert_zone',dict(zone='library'),library)):
    self.assertTrue(self.has(s,op=op,player=pl,value=value,**extra))
  s=self.cases('Sign in Blood',['own_target_1'])[0]['steps']
  self.assertTrue(self.has(s,op='assert_life',player=1,value=48))
  self.assertTrue(self.has(s,op='choose_player',player=1,value=1))

 def test_symmetric_draw_has_separate_exact_library_boundaries(self):
  case=self.cases('Vision Skeins',['exact_library_0'])[0]
  for pl in (0,1):
   self.assertEqual(sum(c['player']==pl and c['zone']=='library' for c in case['setup']),2)
   self.assertTrue(self.has(case['steps'],op='assert_zone',player=pl,zone='library',value=0))
   self.assertTrue(self.has(case['steps'],op='assert_zone',player=pl,zone='hand',value=6))

 def test_mill_stops_at_the_end_of_a_short_or_empty_library(self):
  for v,n in (('short_library_0',1),('empty_library_0',0)):
   case=self.cases('Mindeye Drake',[v])[0];s=case['steps']
   self.assertEqual(sum(c['player']==1 and c['zone']=='library' for c in case['setup']),n)
   self.assertTrue(self.has(s,op='assert_zone',player=1,zone='library',value=0))
   self.assertTrue(self.has(s,op='assert_zone',player=1,zone='graveyard',value=4+n))
   self.assertTrue(self.has(s,op='assert_zone',player=0,zone='library',value=28))

 def test_drain_separates_recipient_loss_from_controller_gain(self):
  for owner in (0,1):
   s=self.cases('Vampire Neonate',[f'effect_{owner}'])[0]['steps']
   self.assertTrue(self.has(s,op='assert_life',player=owner,value=51))
   self.assertTrue(self.has(s,op='assert_life',player=1-owner,value=49))
   self.assertTrue(self.has(s,op='assert_mana',player=owner,color=0,value=18))

 def test_targeted_self_sacrifice_remains_paid_while_effect_is_pending(self):
  s=self.cases('Bile Urchin')[0]['steps'];resolve=next(i for i,x in enumerate(s) if x['op']=='resolve')
  self.assertTrue(self.has(s[:resolve],op='assert_zone',player=0,zone='graveyard',value=5))
  self.assertTrue(self.has(s[:resolve],op='assert_life',player=1,value=50))
  self.assertTrue(self.has(s[resolve:],op='assert_life',player=1,value=49))

 def test_two_activations_pay_separately_and_resolve_twice(self):
  s=self.cases('Armistice',['stacked_activations_0'])[0]['steps']
  self.assertEqual([x['value'] for x in s if x['op']=='assert_stack'],[1,2,0])
  self.assertTrue(self.has(s,op='assert_life',player=1,value=56))
  self.assertTrue(self.has(s,op='assert_zone',player=0,zone='hand',value=6))
  self.assertTrue(self.has(s,op='assert_mana',player=0,color=5,value=16))

 def test_entry_receipts_survive_blink_and_bounce_recast(self):
  for variant in ('blink_source_0','bounce_recast_0'):
   s=self.cases('Cloudblazer',[variant])[0]['steps']
   self.assertTrue(self.has(s,op='assert_life',player=0,value=54))
   self.assertTrue(self.has(s,op='assert_zone',player=0,zone='library',value=24))
   self.assertTrue(self.has(s,op='assert_zone',player=0,zone='hand',value=8))

 def test_death_snapshot_contains_real_pending_removal_and_replays_it(self):
  s=self.cases('Runed Servitor',['pending_snapshot_0'])[0]['steps'];capture=next(i for i,x in enumerate(s) if x['op']=='capture')
  self.assertTrue(self.has(s[:capture],op='cast',ref='removal',value=1))
  self.assertTrue(self.has(s[:capture],op='assert_stack',value=1))
  self.assertTrue(self.has(s[capture:],op='restore'))
  self.assertTrue(self.has(s[capture:],op='assert_recorded_state'))

 def test_disabled_variants_never_attempt_drawing_from_an_empty_library(self):
  d=self.declaration('Sign in Blood');p=explicit_spec(d,self.oracle['Sign in Blood']);vs=variants(p)
  self.assertIn('exact_library_0',vs);self.assertNotIn('empty_library_0',vs);self.assertNotIn('short_library_0',vs)

 def test_land_mode_input_precedes_its_target_and_preserves_both_controllers(self):
  d=self.declaration('Nephalia Drownyard');d['activation_dialogs']={'0':[1],'1':[1]}
  s=self.cases('Nephalia Drownyard',d=d)[0]['steps'];mode=next(i for i,x in enumerate(s) if x['op']=='choose_dialog');target=next(i for i,x in enumerate(s) if x['op']=='choose_player')
  self.assertLess(mode,target);self.assertTrue(self.has(s,op='present_ai_dialogs',value=1))

 def test_additional_source_clause_or_changed_witness_requires_review(self):
  d=self.declaration('Baleful Strix');d['oracle_text']+='\nDraw a card.'
  with self.assertRaises(ValueError):self.cases('Baleful Strix',d=d)
  o=copy.deepcopy(self.oracle);o['Murder']['oracle_text']='Draw a card.'
  with self.assertRaises(ValueError):self.cases('Runed Servitor',o=o)

 def test_existing_keywords_are_pinned_but_combat_is_a_named_gap(self):
  c=self.cases('Baleful Strix')[0]
  self.assertEqual(c['_recipe']['untested_keywords'],['Deathtouch','Flying'])
  self.assertEqual(c['_recipe']['family'],'value_flow')
  self.assertTrue(c['name'].startswith('ValueFlowRecipe_'))


if __name__=='__main__':unittest.main()
