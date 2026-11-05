"""Guard Oracle-derived X amounts, real payment inputs and mirrored recipients."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from variable_value_recipes import variants,amounts


class VariableValueRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

 def declaration(self,name='Braingeyser',selected=None):
  def op(kind,amount='X',scope='recipient',random=False):return dict(kind=kind,amount=amount,scope=scope,random=random)
  configs={'Braingeyser':('any',[op('draw')]),'Prosperity':('both',[op('draw')]),
   'Vitalizing Cascade':('self',[op('gain_life','X+3')]),'Damnable Pact':('any',[op('draw'),op('lose_life')]),
   'Debt to the Deathless':('opponents',[op('lose_life','2X'),op('gain_life','life_lost','self')]),
   'Pull from Tomorrow':('self',[op('draw'),op('discard',1)]),'Psychic Drain':('any',[op('mill'),op('gain_life',scope='self')])}
  recipient,ops=configs[name]
  return dict(family='variable_value',version=2,params=dict(recipient=recipient,operations=ops),oracle_text=self.o[name]['oracle_text'],variants=selected or ['effect_x0_0'])

 def cases(self,name='Braingeyser',selected=None,d=None,oracle=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.o[name],self.rows,oracle_context=oracle or self.o)

 def test_whole_variable_text_and_order_fail_closed(self):
  d=self.declaration();d['params']['operations'][0]['amount']=3
  with self.assertRaises(ValueError):self.cases(d=d)
  d=self.declaration('Damnable Pact');d['params']['operations'].reverse()
  with self.assertRaises(ValueError):self.cases('Damnable Pact',d=d)
  for value in (True,-1,0,'unknown',[]):
   d=self.declaration();d['params']['operations'][0]['amount']=value
   with self.assertRaises(ValueError):self.cases(d=d)
  for patch in (dict(oracle_text='Draw X cards.'),dict(variants=[]),dict(variants=['effect_x20_0'])):
   with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),**patch))

 def test_zero_draw_pays_fixed_cost_but_keeps_every_library_card(self):
  c=self.cases()[0];s=c['steps']
  self.assertIn(dict(op='choose_x',player=0,value=0),s)
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=28),s)
  self.assertIn(dict(op='assert_mana',player=0,color=2,value=18),s)

 def test_exact_payment_includes_x_and_precedes_target_choice(self):
  s=self.cases(selected=['payment_x5_1'])[0]['steps']
  self.assertIn(dict(op='set_mana',player=1,color=0,value=5),s)
  self.assertIn(dict(op='set_mana',player=1,color=2,value=2),s)
  self.assertIn(dict(op='assert_mana',player=1,color=0,value=0),s)
  x=next(i for i,a in enumerate(s) if a['op']=='choose_x');target=next(i for i,a in enumerate(s) if a['op']=='choose_player');cast=next(i for i,a in enumerate(s) if a['op']=='cast')
  self.assertLess(x,target);self.assertLess(target,cast)

 def test_affine_and_life_lost_amounts_come_from_oracle_parameters(self):
  p=explicit_spec(self.declaration('Debt to the Deathless'),self.o['Debt to the Deathless'])
  self.assertEqual([o['amount'] for o in amounts(p,5)],[10,10])
  s=self.cases('Debt to the Deathless',['effect_x5_1'])[0]['steps']
  self.assertIn(dict(op='assert_life',player=0,value=40),s);self.assertIn(dict(op='assert_life',player=1,value=60),s)
  s=self.cases('Vitalizing Cascade',['effect_x0_0'])[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=53),s)

 def test_zero_draw_still_discards_when_oracle_requires_it(self):
  s=self.cases('Pull from Tomorrow',['effect_x0_0'])[0]['steps']
  self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=3),s)
  self.assertIn(dict(op='assert_zone',player=0,zone='graveyard',value=6),s)

 def test_pending_snapshot_replays_resolution_without_repaying_x(self):
  s=self.cases(selected=['pending_snapshot_x5_0'])[0]['steps']
  self.assertEqual(sum(a['op']=='choose_x' for a in s),1)
  self.assertEqual(sum(a['op']=='cast' for a in s),1)
  for op in ('capture','restore','record_state','ai_roundtrip','assert_recorded_state'):self.assertTrue(any(a['op']==op for a in s))
  s=self.cases(selected=['ready_snapshot_x5_0'])[0]['steps'];self.assertEqual(sum(a['op']=='choose_x' for a in s),2)

 def test_illegal_fixed_payment_does_not_queue_a_phantom_x_input(self):
  for v in ('insufficient_mana_x3_0','wrong_color_x3_1'):
   s=self.cases(selected=[v])[0]['steps'];self.assertFalse(any(a['op']=='choose_x' for a in s));self.assertIn(dict(op='cast',ref='source',value=0),s)

 def test_mill_short_libraries_and_zero_preserve_exact_zone_receipts(self):
  p=explicit_spec(self.declaration('Psychic Drain'),self.o['Psychic Drain']);self.assertIn('empty_library_x3_0',variants(p))
  s=self.cases('Psychic Drain',['empty_library_x3_0'])[0]['steps']
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=0),s);self.assertIn(dict(op='assert_life',player=0,value=53),s)

 def test_source_and_witness_pins_are_immutable_and_handler_independent(self):
  d=self.declaration();before=copy.deepcopy(d);expected=self.cases(d=d);self.assertEqual(d,before)
  rows=copy.deepcopy(self.rows);rows['Braingeyser']['handler']='unrelated';rows['Braingeyser']['mana_cost']='{19}'
  self.assertEqual(expected,expand('Braingeyser',self.rows['Braingeyser']['csvid'],d,self.o['Braingeyser'],rows,oracle_context=self.o))
  o=copy.deepcopy(self.o);o['Gilded Light']['oracle_text']='You gain 2 life.'
  with self.assertRaises(ValueError):self.cases(oracle=o)


class VariableDamageRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=VariableValueRecipesTest.o if hasattr(VariableValueRecipesTest,'o') else json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

 def cases(self,name='Blaze',variant='survivor_x0_0',patch=None):
  p=dict(targets=['creature'] if name=='Heat Ray' else ['creature','player'],gain_life=name in ('Death Grasp','Consume Spirit'),x_color='black' if name=='Consume Spirit' else 'generic');p.update(patch or {})
  d=dict(family='variable_damage',version=2,params=p,oracle_text=self.o[name]['oracle_text'],variants=[variant])
  return expand(name,self.rows[name]['csvid'],d,self.o[name],self.rows,oracle_context=self.o)

 def test_target_scope_and_payment_restrictions_are_full_text_pinned(self):
  with self.assertRaises(ValueError):self.cases('Heat Ray',patch=dict(targets=['creature','player']))
  with self.assertRaises(ValueError):self.cases('Consume Spirit',patch=dict(x_color='generic'))
  with self.assertRaises(ValueError):self.cases('Blaze',patch=dict(gain_life=True))

 def test_mirror_flips_target_player_as_well_as_caster(self):
  s=self.cases(variant='opponent_player_x5_1')[0]['steps']
  self.assertIn(dict(op='choose_x',player=1,value=5),s);self.assertIn(dict(op='choose_player',player=1,value=0),s)
  self.assertIn(dict(op='assert_life',player=0,value=15),s)
  s=self.cases(variant='lost_target_x3_1')[0]['steps']
  self.assertNotIn('choose_priority',[a['op'] for a in s])
  casts=[a['ref'] for a in s if a['op']=='cast'];self.assertEqual(casts,['source','response'])
  self.assertIn(dict(op='assert_stack',value=2),s)
  self.assertEqual([a['player'] for a in s if a['op']=='choose_target'],[1,1])
  s=self.cases(variant='regenerate_x3_1')[0]['steps']
  self.assertIn(dict(op='choose_regeneration',ref='recipient'),s)
  self.assertNotIn('choose_priority',[a['op'] for a in s])

 def test_zero_damage_retains_legal_target_and_spends_only_base_cost(self):
  s=self.cases()[0]['steps'];self.assertIn(dict(op='choose_target',player=0,ref='recipient'),s)
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s)
  self.assertIn(dict(op='assert_mana',player=0,color=0,value=10),s);self.assertIn(dict(op='assert_mana',player=0,color=4,value=9),s)

 def test_black_only_x_is_checked_per_color_not_only_total(self):
  s=self.cases('Consume Spirit','payment_x5_1')[0]['steps']
  self.assertIn(dict(op='set_mana',player=1,color=1,value=6),s)
  self.assertIn(dict(op='set_mana',player=1,color=0,value=1),s)
  self.assertIn(dict(op='assert_mana',player=1,color=1,value=0),s)


if __name__=='__main__':unittest.main()
