import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from death_value_creature_recipes import variants,validate_witnesses

class DeathValueRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text())['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Darkslick Drake'):
  return dict(family='death_value_creature',version=2,params=dict(draw=2 if name=='Youthful Scholar' else 0 if name=='Highland Game' else 1,gain_life=2 if name=='Highland Game' else 0,optional=name=='Aven Fisher',keywords=sorted(self.oracle[name]['keywords'])),oracle_text=self.oracle[name]['oracle_text'],variants=['destroy_0'])
 def cases(self,name='Darkslick Drake',d=None):return expand(name,self.rows[name]['csvid'],d or self.declaration(name),self.oracle[name],self.rows,oracle_context=self.oracle)
 def test_pin_and_explicit_amounts(self):
  for field,value in [('draw',2),('draw',True),('gain_life',1),('optional',1),('keywords',[])]:
   d=self.declaration();d['params'][field]=value
   with self.assertRaises(ValueError):self.cases(d=d)
  d=self.declaration();d['oracle_text']='different'
  with self.assertRaises(ValueError):self.cases(d=d)
 def test_reject_additional_triggers_and_variable_receipts(self):
  for name in ('Pelakka Wurm','Filigree Familiar','Dusk Urchins','Solemn Simulacrum','Floating-Dream Zubera','Corpse Augur'):
   with self.assertRaises(ValueError):explicit_spec(self.declaration(name),self.oracle[name])
 def test_variants_fail_closed(self):
  for values in ([],['bad'],['destroy_0','destroy_0']):
   d=self.declaration();d['variants']=values
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_short_library_order_and_draw_multiplicity(self):
  d=self.declaration('Youthful Scholar');d['variants']=['short_library_1'];c=self.cases('Youthful Scholar',d)[0]
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=0),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=self.rows['Plains']['csvid'],value=2),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Youthful Scholar']['csvid'],value=1),c['steps'])
 def test_countering_and_exile_never_receive_death_value(self):
  d=self.declaration('Highland Game');d['variants']=['countered_0','exile_1']
  for c,owner,expected in zip(self.cases('Highland Game',d),(0,1),(20,22)):
   self.assertIn(dict(op='assert_life',player=owner,value=expected),c['steps'])
   self.assertNotIn(dict(op='destroy',ref='source'),c['steps'])
 def test_bounce_and_blink_use_actual_spells_and_rebind(self):
  d=self.declaration();d['variants']=['bounce_0','blink_only_1','blink_destroy_1'];cs=self.cases(d=d)
  self.assertIn(dict(op='bind_hand',player=0,value=self.rows['Darkslick Drake']['csvid'],ref='source'),cs[0]['steps'])
  self.assertIn(dict(op='bind_permanent',player=1,value=self.rows['Darkslick Drake']['csvid'],ref='source'),cs[1]['steps'])
  self.assertNotIn(dict(op='destroy',ref='source'),cs[1]['steps']);self.assertIn(dict(op='destroy',ref='source'),cs[2]['steps'])
 def test_combat_advances_before_damage(self):
  d=self.declaration();d['variants']=['combat_death_1'];steps=self.cases(d=d)[0]['steps']
  self.assertLess(steps.index(dict(op='advance_combat')),steps.index(dict(op='combat_damage')))
  self.assertIn(dict(op='block',ref='source',other='attacker',value=1),steps)
 def test_optional_decline_and_receipt_are_distinct(self):
  d=self.declaration('Aven Fisher');d['variants']=['destroy_0','decline_0'];cs=self.cases('Aven Fisher',d)
  self.assertIn(dict(op='choose_trigger',player=0,ref='source'),cs[0]['steps'])
  self.assertFalse(any(s['op']=='choose_trigger' for s in cs[1]['steps']))
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=7),cs[1]['steps'])
 def test_snapshots_replay_pending_real_removal(self):
  d=self.declaration();d['variants']=['death_snapshot_0','ready_snapshot_1','resolved_snapshot_0']
  for c in self.cases(d=d):
   for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertTrue(any(s['op']==op for s in c['steps']))
  steps=self.cases(d=d)[0]['steps'];cap=steps.index(dict(op='capture'))
  self.assertEqual(steps[cap-1],dict(op='assert_stack',value=1));self.assertEqual(steps[cap+1],dict(op='resolve'))
 def test_each_controller_and_real_exact_payment(self):
  d=self.declaration();d['variants']=['payment_0','payment_1']
  for c,owner in zip(self.cases(d=d),(0,1)):
   self.assertEqual(sum(s['op']=='set_mana' and s['player']==owner for s in c['steps']),6)
   self.assertIn(dict(op='assert_mana_total',player=owner,value=0),c['steps'])
 def test_witness_pin_and_card_identity(self):
  validate_witnesses(self.oracle,self.rows);o=copy.deepcopy(self.oracle);o['Murder']['oracle_text']='Exile target creature.'
  with self.assertRaises(ValueError):validate_witnesses(o,self.rows)
 def test_bare_zones_and_negative_absence_counts(self):
  d=self.declaration();d['variants']=['bare_zones_0'];c=self.cases(d=d)[0]
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Darkslick Drake']['csvid'],value=0),c['steps'])
  self.assertIn(dict(op='assert_zone',player=1,zone='hand',value=0),c['steps'])
 def test_no_event_injection_or_engine_derived_expected_results(self):
  d=self.declaration();d['variants']=variants(explicit_spec(d,self.oracle['Darkslick Drake']))
  for c in self.cases(d=d):
   self.assertFalse(any(s['op'] in ('event','call_handler') for s in c['steps']))
   self.assertIn('sacrifice costs',c['_recipe']['untested_components'])

if __name__=='__main__':unittest.main()
