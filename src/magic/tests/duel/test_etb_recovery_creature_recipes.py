import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from etb_recovery_creature_recipes import eligible,validate_witnesses

class EntryRecoveryRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text())['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Gravedigger'):
  types=['instant','sorcery'] if name=='Archaeomancer' else ['card'] if name=='Eternal Witness' else ['creature','enchantment'] if name=='Odunos River Trawler' else ['creature']
  return dict(family='etb_recovery_creature',version=2,params=dict(targets=types,type_match='all' if name=='Odunos River Trawler' else 'any',exclude_types=[],optional=name in ('Gravedigger','Eternal Witness'),keywords=sorted(self.oracle[name]['keywords'])),oracle_text=self.oracle[name]['oracle_text'],variants=['entry_0'])
 def cases(self,name='Gravedigger',d=None):return expand(name,self.rows[name]['csvid'],d or self.declaration(name),self.oracle[name],self.rows,oracle_context=self.oracle)
 def test_full_pin_required(self):
  d=self.declaration();d['oracle_text']='different'
  with self.assertRaises(ValueError):self.cases(d=d)
 def test_type_and_optional_scope_is_reviewed(self):
  for k,v in [('optional',False),('optional',1),('targets',['artifact']),('targets',['creature','creature']),('targets',[{}]),('type_match','all')]:
   d=self.declaration();d['params'][k]=v
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_no_unknown_parameters(self):
  d=self.declaration();d['params']['extra']=True
  with self.assertRaises(ValueError):self.cases(d=d)
 def test_unknown_duplicate_empty_variants_rejected(self):
  for values in ([],['bad'],['entry_0','entry_0']):
   d=self.declaration();d['variants']=values
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_intersection_differs_from_union(self):
  p=self.declaration('Odunos River Trawler')['params']
  self.assertTrue(eligible(self.oracle['Nyx-Fleece Ram'],p));self.assertFalse(eligible(self.oracle['Grizzly Bears'],p));self.assertFalse(eligible(self.oracle['Seal of Cleansing'],p))
 def test_all_cards_include_every_zone_card_type(self):
  p=self.declaration('Eternal Witness')['params']
  for name in ('Forest','Grizzly Bears','Sol Ring','Divination','Giant Growth','Seal of Cleansing','Jace Beleren'):self.assertTrue(eligible(self.oracle[name],p))
 def test_exclusions_are_independent_of_union(self):
  p=dict(self.declaration('Eternal Witness')['params'],exclude_types=['creature','land'])
  self.assertFalse(eligible(self.oracle['Ornithopter'],p));self.assertFalse(eligible(self.oracle['Forest'],p));self.assertTrue(eligible(self.oracle['Sol Ring'],p))
 def test_multiple_or_conditional_triggers_rejected(self):
  for name in ('Nucklavee','Pit Keeper','Greenwarden of Murasa','Warren Pilferers'):
   d=self.declaration();d['oracle_text']=self.oracle[name]['oracle_text'];d['params']['keywords']=sorted(self.oracle[name]['keywords'])
   with self.assertRaises(ValueError):self.cases(name,d)
 def test_zone_counts_include_duplicates_and_absence(self):
  s=self.cases()[0]['steps'];counts=[a for a in s if a['op']=='assert_zone_multiplicity']
  bear=self.rows['Grizzly Bears']['csvid'];giant=self.rows['Hill Giant']['csvid']
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=bear,value=2),counts)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=bear,value=1),counts)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=giant,value=0),counts)
 def test_other_graveyard_not_a_legal_fallback(self):
  d=self.declaration();d['variants']=['other_graveyard_1'];c=self.cases(d=d)[0]
  self.assertFalse(any(a['op']=='choose_zone_card' for a in c['steps']));self.assertTrue(any(a['zone']=='graveyard' and a['player']==0 for a in c['setup']))
 def test_countered_source_never_selects_graveyard(self):
  d=self.declaration();d['variants']=['countered_0'];s=self.cases(d=d)[0]['steps'];self.assertFalse(any(a['op']=='choose_zone_card' for a in s))
 def test_real_blink_recovers_two_distinct_cards(self):
  d=self.declaration();d['variants']=['blink_1'];s=self.cases(d=d)[0]['steps']
  self.assertEqual([a['value'] for a in s if a['op']=='choose_zone_card'],[self.rows['Grizzly Bears']['csvid'],self.rows['Hill Giant']['csvid']]);self.assertEqual(sum(a['op']=='cast' for a in s),2)
 def test_ready_and_pending_snapshots_replay_after_ai(self):
  for variant in ('ready_snapshot_0','pending_snapshot_1','blink_snapshot_0'):
   d=self.declaration();d['variants']=[variant];s=self.cases(d=d)[0]['steps'];self.assertIn(dict(op='ai_roundtrip'),s);self.assertEqual(s[-1]['op'],'assert_recorded_state')
 def test_optional_trigger_priority_annotation(self):
  d=self.declaration('Eternal Witness');d['trigger_priority']=True;d['variants']=['decline_0'];s=self.cases('Eternal Witness',d)[0]['steps'];self.assertFalse(any(a['op'].startswith('choose_') for a in s))
  for name,value in [('Archaeomancer',True),('Gravedigger',1)]:
   d=self.declaration(name);d['trigger_priority']=value
   with self.assertRaises(ValueError):self.cases(name,d)
 def test_protection_witness_pin_and_no_mutation(self):
  d=self.declaration();before=copy.deepcopy((d,self.oracle));self.cases(d=d);self.assertEqual(before,(d,self.oracle))
  changed=copy.deepcopy(self.oracle);changed['Ground Seal']['oracle_text']='changed'
  with self.assertRaises(ValueError):validate_witnesses(changed,self.rows)
 def test_activated_components_are_reported_uncovered(self):
  c=self.cases('Odunos River Trawler')[0];self.assertTrue(any('Sacrifice' in t for t in c['_recipe']['untested_components']))
 def test_decline_asserts_optional_policy_and_unchanged_zones(self):
  d=self.declaration();d['variants']=['decline_1'];s=self.cases(d=d)[0]['steps']
  self.assertIn(dict(op='choose_optional_zone',player=1,value=self.rows['Grizzly Bears']['csvid']),s)
  self.assertIn(dict(op='assert_forced_zone_choices',value=0),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=self.rows['Grizzly Bears']['csvid'],value=1),s)

if __name__=='__main__':unittest.main()
