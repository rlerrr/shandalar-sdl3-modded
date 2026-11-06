import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import etb_removal_extension_recipes as extension
class EtbRemovalExtensionTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):return dict(family='etb_removal_extension',version=2,params=copy.deepcopy(extension.PROFILES[name]['params']),oracle_text=self.oracle[name]['oracle_text'],variants=['entry_0'])
 def cases(self,name,variants):
  d=self.declaration(name);d['variants']=variants;p=extension.spec(d,self.oracle[name]);return extension.expand(name,self.rows[name]['csvid'],d,p,variants,self.rows,self.oracle)
 def test_source_pin_and_parameter_guards(self):
  for name in extension.PROFILES:
   d=self.declaration(name);extension.spec(d,self.oracle[name])
   for change in (dict(oracle_text=d['oracle_text']+' '),dict(params=dict(d['params'],extra=True)),dict(variants=[]),dict(variants=['entry_0','entry_0']),dict(variants=['bad'])):
    with self.assertRaises(ValueError):extension.spec(dict(d,**change),self.oracle[name])
   changed=copy.deepcopy(self.oracle[name]);changed['power']='9'
   with self.assertRaises(ValueError):extension.spec(d,changed)
 def test_witness_pins(self):
  extension.validate_witnesses(self.oracle,self.rows);changed=copy.deepcopy(self.oracle);changed['Bog Imp']['colors']=['G']
  with self.assertRaises(ValueError):extension.validate_witnesses(changed,self.rows)
 def test_expand_selected_variants_guard(self):
  name='Goblin Settler';d=self.declaration(name);p=extension.spec(d,self.oracle[name])
  for selected in ([],['entry_0','entry_0'],['unknown_0']):
   with self.assertRaises(ValueError):extension.expand(name,self.rows[name]['csvid'],d,p,selected,self.rows,self.oracle)
 def test_both_controllers_cast_normal_evoke_mode(self):
  for owner in (0,1):
   c=self.cases('Ingot Chewer',['entry_'+str(owner)])[0];steps=c['steps'];i=next(i for i,s in enumerate(steps) if s['op']=='cast')
   self.assertEqual(steps[i-1],dict(op='choose_dialog',player=owner,value=0))
   self.assertIn(dict(op='assert_mana_total',player=owner,value=55),steps)
 def test_exact_normal_cost_omits_unaffordable_alternate_menu(self):
  for owner in (0,1):
   c=self.cases('Aethersnipe',['payment_'+str(owner)])[0]
   self.assertFalse(any(s['op']=='choose_dialog' for s in c['steps']))
   self.assertIn(dict(op='assert_mana_total',player=owner,value=0),c['steps'])
 def test_land_scope_and_full_zone_receipts(self):
  c=self.cases('Goblin Settler',['entry_0'])[0]
  self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='recipient'),'Forest')
  self.assertTrue(any(s['op']=='assert_zone_card' for s in c['steps']))
 def test_predicate_negatives_have_real_cast_and_unchanged_witness(self):
  c=self.cases('Bone Shredder',['invalid_black_0'])[0]
  self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='invalid'),'Bog Imp')
  self.assertIn(dict(op='cast',ref='source',value=1),c['steps']);self.assertIn(dict(op='assert_card',ref='invalid',property='in_play',value=1),c['steps'])
  self.assertFalse(any(s['op']=='choose_target' for s in c['steps']))
 def test_opponent_control_negative_and_reentry(self):
  c=self.cases('Stingscourger',['own_only_1'])[0]
  self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='invalid'),1)
  self.assertIn(dict(op='assert_card',ref='invalid',property='in_play',value=1),c['steps'])
  c=self.cases('Stingscourger',['blink_1'])[0];self.assertEqual([s['ref'] for s in c['steps'] if s['op']=='choose_target'],['recipient','source','second_target'])
  self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='second_target'),0)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Craw Wurm']['csvid'],value=1),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=self.rows['Craw Wurm']['csvid'],value=0),c['steps'])
  c=self.cases('Stingscourger',['wrong_type_0'])[0]
  invalid=next(x['name'] for x in c['setup'] if x['ref']=='invalid')
  self.assertNotIn('Creature',self.oracle[invalid]['type_line'])
 def test_snapshots_replay_real_actions_and_no_mutation(self):
  for name in extension.PROFILES:
   d=self.declaration(name);p=extension.spec(d,self.oracle[name]);all_variants=extension.variants(p);before=copy.deepcopy((d,p,self.oracle));cases=extension.expand(name,self.rows[name]['csvid'],d,p,all_variants,self.rows,self.oracle)
   self.assertEqual(before,(d,p,self.oracle));self.assertEqual(len(cases),len(all_variants))
  c=self.cases('Bone Shredder',['pending_snapshot_1'])[0]
  self.assertEqual(sum(s['op']=='resolve' for s in c['steps']),2);self.assertEqual(c['steps'][-1]['op'],'assert_recorded_state')
 def test_new_predicate_witnesses_keep_exact_identities(self):
  c=self.cases('Stingerfling Spider',['entry_0'])[0]
  self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='recipient'),'Suntail Hawk')
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Suntail Hawk']['csvid'],value=1),c['steps'])
  self.assertFalse(any(x['name']=='Hill Giant' for x in c['setup']))
  c=self.cases('Gilt-Leaf Winnower',['entry_1'])[0]
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Craw Wurm']['csvid'],value=1),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Wall of Air']['csvid'],value=1),c['steps'])
 def test_persist_source_death_is_excluded_and_predicates_independent(self):
  name='Woodfall Primus';d=self.declaration(name);p=extension.spec(d,self.oracle[name])
  self.assertNotIn('destroy_source_0',extension.variants(p));self.assertNotIn('destroy_source_1',extension.variants(p))
  p=extension.PROFILES['Gilt-Leaf Winnower']['params']
  self.assertFalse(extension._eligible('Elvish Warrior',p,self.oracle));self.assertFalse(extension._eligible('Hill Giant',p,self.oracle));self.assertTrue(extension._eligible('Craw Wurm',p,self.oracle))
 def test_suspend_menu_noncreature_overlap_and_generic_reserve(self):
  c=self.cases('Riftwing Cloudskate',['entry_0'])[0];cast=next(i for i,s in enumerate(c['steps']) if s['op']=='cast')
  self.assertEqual(c['steps'][cast-1],dict(op='choose_dialog',player=0,value=0))
  p=extension.PROFILES['Woodfall Primus']['params'];self.assertFalse(extension._eligible('Ornithopter',p,self.oracle));self.assertTrue(extension._eligible('Bonesplitter',p,self.oracle))
  c=self.cases('Meteor Golem',['bounce_recast_0'])[0]
  self.assertIn(dict(op='set_mana',player=0,color=0,value=14),c['steps']);self.assertIn(dict(op='assert_mana_total',player=0,value=49),c['steps'])
if __name__=='__main__':unittest.main()
