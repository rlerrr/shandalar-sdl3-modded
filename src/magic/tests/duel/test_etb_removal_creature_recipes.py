import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from etb_removal_creature_recipes import variants,validate_witnesses

class EtbRemovalCreatureRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text())['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Manic Vandal'):
  effect='bounce' if name in ('Aether Adept','Voidwielder') else 'destroy';targets=['creature'] if effect=='bounce' else ['artifact','enchantment'] if name=='Reclamation Sage' else ['artifact']
  return dict(family='etb_removal_creature',version=2,params=dict(effect=effect,targets=targets,optional=name in ('Reclamation Sage','Voidwielder'),keywords=sorted(self.oracle[name]['keywords'])),oracle_text=self.oracle[name]['oracle_text'],variants=['entry_0'])
 def cases(self,name='Manic Vandal',d=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name),self.oracle[name],self.rows,oracle_context=self.oracle)
 def test_full_oracle_pin_and_amount_scope(self):
  for change in (dict(oracle_text='different'),dict(params=dict(effect='bounce',targets=['artifact'],optional=False,keywords=[]))):
   with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),**change))
 def test_optional_and_targets_require_explicit_review(self):
  for key,value in [('optional',True),('optional',1),('targets',['enchantment']),('targets',['artifact','artifact'])]:
   d=self.declaration();d['params'][key]=value
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_unknown_parameters_and_variants_fail_closed(self):
  for change in (dict(extra=True),dict(variants=[]),dict(variants=['bad']),dict(variants=['entry_0','entry_0'])):
   with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),**change))
 def test_conditional_and_extra_triggers_rejected(self):
  for name in ('Nikko-Onna','Kor Sanctifiers','Duergar Hedge-Mage'):
   with self.assertRaises(ValueError):self.cases(name)
 def test_mandatory_creature_return_can_target_itself(self):
  d=self.declaration('Aether Adept');d['variants']=['no_other_targets_0'];s=self.cases('Aether Adept',d)[0]['steps']
  self.assertIn(dict(op='choose_target',player=0,ref='source'),s);self.assertIn(dict(op='assert_card',ref='source',property='in_hand',value=1),s)
 def test_optional_creature_return_can_decline_self_target(self):
  d=self.declaration('Voidwielder');d['variants']=['no_other_targets_1'];s=self.cases('Voidwielder',d)[0]['steps']
  self.assertIn(dict(op='choose_cancel_target',player=1),s);self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s)
 def test_blink_has_two_distinct_real_target_choices(self):
  d=self.declaration();d['variants']=['blink_0'];s=self.cases(d=d)[0]['steps']
  self.assertEqual([a['ref'] for a in s if a['op']=='choose_target'],['recipient','source','second_target']);self.assertEqual(sum(a['op']=='cast' for a in s),2)
 def test_pending_snapshot_replays_after_ai(self):
  d=self.declaration();d['variants']=['pending_snapshot_1'];s=self.cases(d=d)[0]['steps']
  self.assertEqual(sum(a['op']=='cast' for a in s),1);self.assertEqual(sum(a['op']=='resolve' for a in s),2);self.assertTrue(any(a['op']=='ai_roundtrip' for a in s));self.assertEqual(s[-1]['op'],'assert_recorded_state')
 def test_zone_multiplicity_and_order_are_checked(self):
  s=self.cases()[0]['steps'];self.assertTrue(any(a['op']=='assert_hand_graveyard_total' for a in s));self.assertTrue(any(a['op']=='assert_zone_card' and a['zone']=='library' for a in s))
 def test_countered_creature_never_queues_entry_choice(self):
  d=self.declaration();d['variants']=['countered_0'];s=self.cases(d=d)[0]['steps'];self.assertFalse(any(a['op']=='choose_target' for a in s));self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),s)
 def test_no_mutation_and_witness_guards(self):
  d=self.declaration();before=copy.deepcopy((d,self.oracle));self.cases(d=d);self.assertEqual(before,(d,self.oracle))
  changed=copy.deepcopy(self.oracle);changed['Seal of Cleansing']['oracle_text']='changed'
  with self.assertRaises(ValueError):validate_witnesses(changed,self.rows)
 def test_priority_trigger_is_selected_after_casting(self):
  d=self.declaration('Reclamation Sage');d['trigger_priority']=True;s=self.cases('Reclamation Sage',d)[0]['steps']
  self.assertLess(next(i for i,a in enumerate(s) if a['op']=='cast'),next(i for i,a in enumerate(s) if a['op']=='choose_trigger'))
  d['variants']=['decline_0'];s=self.cases('Reclamation Sage',d)[0]['steps'];self.assertFalse(any(a['op'].startswith('choose_') for a in s));self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),s)
 def test_returned_instances_rebind_to_unique_names(self):
  d=self.declaration('Aether Adept');d['variants']=['blink_0'];c=self.cases('Aether Adept',d)[0]
  self.assertEqual([a['ref'] for a in c['steps'] if a['op']=='bind_hand'],['recipient','second_target']);self.assertEqual(next(a['name'] for a in c['setup'] if a['ref']=='second_target'),'Craw Wurm')
 def test_priority_annotation_guard(self):
  for name,value in [('Manic Vandal',True),('Reclamation Sage',1)]:
   d=self.declaration(name);d['trigger_priority']=value
   with self.assertRaises(ValueError):self.cases(name,d)

if __name__=='__main__':unittest.main()
