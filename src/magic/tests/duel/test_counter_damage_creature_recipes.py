import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from counter_damage_creature_recipes import variants,validate_witnesses

class CounterDamageRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text())['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Sickle Ripper'):
  r=self.oracle[name];mechanic='Infect' if 'Infect' in r['keywords'] else 'Wither'
  d=dict(family='counter_damage_creature',version=2,params=dict(mechanic=mechanic,keywords=sorted(set(r['keywords'])-{mechanic})),oracle_text=r['oracle_text'],variants=['unblocked_0'])
  return d
 def cases(self,name='Sickle Ripper',d=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name),self.oracle[name],self.rows,oracle_context=self.oracle)
 def test_full_text_and_explicit_mechanic(self):
  for change in (dict(oracle_text='different'),dict(params=dict(mechanic='Infect',keywords=[]))):
   with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),**change))
 def test_parameters_and_variant_guards(self):
  for change in (dict(params=dict(mechanic='Wither',keywords=[],amount=2)),dict(variants=[]),dict(variants=['bad']),dict(variants=['unblocked_0','unblocked_0'])):
   with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),**change))
 def test_conditional_and_triggered_cards_rejected(self):
  for name in ('Hand of the Praetors','Ichor Rats','Rot Wolf','Phyrexian Hydra','Chained Throatseeker'):
   with self.assertRaises(ValueError):self.cases(name)
 def test_infect_poison_and_life_are_separate(self):
  c=self.cases('Flensermite')[0];s=c['steps']
  self.assertIn(dict(op='assert_poison',player=1,value=1),s)
  self.assertIn(dict(op='assert_life',player=1,value=20),s)
  self.assertIn(dict(op='assert_life',player=0,value=21),s)
 def test_wither_damage_to_player_is_ordinary(self):
  s=self.cases()[0]['steps'];self.assertIn(dict(op='assert_poison',player=1,value=0),s);self.assertIn(dict(op='assert_life',player=1,value=18),s)
 def test_counters_persist_through_cleanup(self):
  d=self.declaration();d['variants']=['cleanup_0'];s=self.cases(d=d)[0]['steps'];cut=next(i for i,a in enumerate(s) if a['op']=='cleanup')
  expected=dict(op='assert_card',ref='witness',property='minus1_counters',value=2)
  self.assertIn(expected,s[:cut]);self.assertIn(expected,s[cut+1:]);self.assertIn(dict(op='assert_card',ref='witness',property='damage',value=0),s[cut+1:])
 def test_snapshots_and_ai_roundtrip(self):
  d=self.declaration();d['variants']=['combat_snapshot_1'];s=self.cases(d=d)[0]['steps']
  self.assertEqual(sum(a['op']=='combat_damage' for a in s),2);self.assertTrue(any(a['op']=='ai_roundtrip' for a in s));self.assertTrue(any(a['op']=='assert_recorded_state' for a in s))
 def test_haste_and_defender_attack_rules(self):
  for name,expected in [('Blackcleave Goblin',1),('Sickle Ripper',0)]:
   d=self.declaration(name);d['variants']=['fresh_attack_0'];self.assertIn(dict(op='attack',ref='source',value=expected),self.cases(name,d)[0]['steps'])
 def test_unused_abilities_and_no_mutation(self):
  d=self.declaration('Fallen Ferromancer');before=copy.deepcopy((d,self.oracle));c=self.cases('Fallen Ferromancer',d)[0]
  self.assertTrue(any('deals 1 damage' in x for x in c['_recipe']['untested_components']));self.assertEqual(before,(d,self.oracle))
 def test_witnesses_are_independently_pinned(self):
  changed=copy.deepcopy(self.oracle);changed['Wall of Denial']['power']='1'
  with self.assertRaises(ValueError):validate_witnesses(changed,self.rows)
 def test_hybrid_negative_excludes_both_colors(self):
  name='Harvest Gwyllion';d=self.declaration(name);d['params']['payment_colors']=['W','W'];d['variants']=['wrong_color_0','wrong_color_1']
  for case in self.cases(name,d):
   owner=case['setup'][0]['player'];pools={s['color']:s['value'] for s in case['steps'] if s['op']=='set_mana' and s['player']==owner}
   self.assertEqual(pools[1],0);self.assertEqual(pools[5],0);self.assertEqual(sum(pools.values()),4)

 def test_ai_cast_uses_exact_pools_without_payment_policy(self):
  name='Boggart Ram-Gang';d=self.declaration(name);d['params']['payment_colors']=['R']*3;d['variants']=['cast_1'];case=self.cases(name,d)[0]
  pools={s['color']:s['value'] for s in case['steps'] if s['op']=='set_mana' and s['player']==1}
  self.assertEqual(pools,{0:0,1:0,2:0,3:0,4:3,5:0});self.assertFalse(any(s['op']=='choose_payment' for s in case['steps']))

if __name__=='__main__':unittest.main()
