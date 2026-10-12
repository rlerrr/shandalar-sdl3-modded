import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from passive_keyword_creature_recipes import split,spec,variants

class PassiveKeywordRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text())['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Air Servant'):
  r=self.oracle[name];abilities,_=split(r)
  return dict(family='passive_keyword_creature',version=2,params=dict(keywords=sorted(r['keywords']),activated_abilities=abilities),oracle_text=r['oracle_text'],variants=['cast_0'])
 def cases(self,name='Air Servant',d=None):return expand(name,self.rows[name]['csvid'],d or self.declaration(name),self.oracle[name],self.rows,oracle_context=self.oracle)
 def test_full_pin_and_explicit_component(self):
  for field,value in [('keywords',[]),('activated_abilities',[]),('extra',True)]:
   d=self.declaration();d['params'][field]=value
   with self.assertRaises(ValueError):self.cases(d=d)
  d=self.declaration();d['oracle_text']='different'
  with self.assertRaises(ValueError):self.cases(d=d)
 def test_conditional_keywords_and_other_triggers_rejected(self):
  for text in ('As long as you control an Island, this creature has flying.', 'When this creature enters, draw a card.', 'Other creatures you control have flying.'):
   r=dict(self.oracle['Air Servant'],oracle_text=text+'\n{U}: Draw a card.');d=self.declaration();d['oracle_text']=r['oracle_text']
   with self.assertRaises(ValueError):spec(d,r)
 def test_granted_keyword_not_promoted_to_unconditional(self):
  r=dict(self.oracle['Air Servant'],oracle_text='{U}: This creature gains flying until end of turn.');d=self.declaration();d['oracle_text']=r['oracle_text']
  with self.assertRaises(ValueError):spec(d,r)
 def test_plain_keywords_stay_in_existing_family(self):
  with self.assertRaises(ValueError):split(self.oracle['Air Elemental'])
 def test_activated_lines_pinned_verbatim_and_reported_untested(self):
  c=self.cases()[0];self.assertEqual(c['_recipe']['untested_abilities'],self.declaration()['params']['activated_abilities'])
  self.assertIn('activated abilities',c['_recipe']['untested_components'])
  self.assertFalse(any(s['op'] in ('activate','activate_hand','event') for s in c['steps']))
 def test_unknown_duplicate_and_empty_variants_rejected(self):
  for names in ([],['bad'],['cast_0','cast_0']):
   d=self.declaration();d['variants']=names
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_both_owners_include_real_payment_and_catches_wrong_zone(self):
  d=self.declaration();d['variants']=['payment_0','payment_1']
  for c,owner in zip(self.cases(d=d),(0,1)):
   self.assertEqual(next(s for s in c['setup'] if s['ref']=='source')['player'],owner)
   self.assertIn(dict(op='assert_mana_total',player=owner,value=0),c['steps'])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Air Servant']['csvid'],value=0),c['steps'])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Air Servant']['csvid'],value=0),c['steps'])
 def test_mirrored_defending_combat_keeps_attacker_opposite(self):
  d=self.declaration();d['variants']=['block_ground_1'];c=self.cases(d=d)[0]
  self.assertEqual(next(s for s in c['setup'] if s['ref']=='attacker')['player'],0)
  self.assertIn(dict(op='turn',player=0,value=0),c['steps'])
  self.assertLess(next(i for i,s in enumerate(c['steps']) if s['op']=='block'),next(i for i,s in enumerate(c['steps']) if s['op']=='advance_combat'))
 def test_lifelink_expected_damage_is_preserved(self):
  d=self.declaration('Griselbrand');d['variants']=['unblocked_1'];c=self.cases('Griselbrand',d)[0]
  self.assertIn(dict(op='assert_life',player=1,value=27),c['steps']);self.assertIn(dict(op='assert_life',player=0,value=13),c['steps'])
 def test_snapshot_checks_zones_before_record_and_after_restore(self):
  d=self.declaration();d['variants']=['snapshot_1'];steps=self.cases(d=d)[0]['steps'];record=next(i for i,s in enumerate(steps) if s['op']=='record_state');restore=next(i for i,s in enumerate(steps) if s['op']=='restore')
  self.assertTrue(any(s['op']=='assert_zone_multiplicity' for s in steps[:record]));self.assertTrue(any(s['op']=='assert_zone_multiplicity' for s in steps[restore:]))
  self.assertIn(dict(op='ai_roundtrip'),steps);self.assertIn(dict(op='assert_recorded_state'),steps)
  mana=[s for s in steps if s['op']=='assert_mana_total']
  self.assertEqual(len(mana),2);self.assertTrue(all(s['player']==1 for s in mana))
 def test_haste_expectation_keeps_fresh_attack_assertions(self):
  d=self.declaration('Ashen Ghoul');d['variants']=['fresh_attack_0'];steps=self.cases('Ashen Ghoul',d)[0]['steps']
  self.assertIn(dict(op='attack',ref='source',value=1),steps);self.assertIn(dict(op='assert_card',ref='source',property='attacking',value=1),steps)
 def test_legends_only_one_source_per_case(self):
  d=self.declaration('Griselbrand');d['variants']=variants(spec(d,self.oracle['Griselbrand']))
  for c in self.cases('Griselbrand',d):self.assertEqual(sum(s['name']=='Griselbrand' for s in c['setup']),1)
 def test_characteristics_and_layout_require_dedicated_scope(self):
  for change in (dict(layout='transform'),dict(toughness='0'),dict(power='*'),dict(keywords=[])):
   r=dict(self.oracle['Air Servant'],**change)
   with self.assertRaises(ValueError):spec(self.declaration(),r)

if __name__=='__main__':unittest.main()
