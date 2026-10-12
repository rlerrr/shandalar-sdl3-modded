import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from conditional_stats_recipes import spec,variants

class ConditionalStatsRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Ardent Recruit',selected=None):
  p=dict(keywords=[],condition='metalcraft',power=2,toughness=2)
  if name in ('Krosan Beast','Nimble Mongoose','Springing Tiger'):p['condition']='threshold'
  if name=='Krosan Beast':p.update(power=7,toughness=7)
  if name=='Nimble Mongoose':p['keywords']=['Shroud']
  return dict(family='conditional_stats',version=2,params=p,oracle_text=self.o[name]['oracle_text'],variants=selected or ['effect_0_0'])
 def cases(self,name='Ardent Recruit',selected=None,d=None,rows=None,o=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.o[name],rows or self.rows,oracle_context=o or self.o)
 def test_pin_and_parameter_types_fail_closed(self):
  d=self.declaration()
  for change in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],power=True)),dict(params=dict(d['params'],power=8)),dict(params=dict(d['params'],toughness=-1)),dict(params=dict(d['params'],keywords=[{}])),dict(params=dict(d['params'],condition='unreviewed'))):
   with self.assertRaises(ValueError):self.cases(d=dict(d,**change))
 def test_extra_clauses_and_faces_rejected(self):
  for line in ('\n{T}: Add {G}.','\nWhen this creature enters, draw a card.'):
   r=dict(self.o['Ardent Recruit'],oracle_text=self.o['Ardent Recruit']['oracle_text']+line);d=self.declaration();d['oracle_text']=r['oracle_text']
   with self.assertRaises(ValueError):spec(d,r)
  with self.assertRaises(ValueError):spec(self.declaration(),dict(self.o['Ardent Recruit'],layout='transform'))
 def test_unknown_empty_and_duplicate_variants_rejected(self):
  for vs in ([],['unknown_0'],['effect_0_0','effect_0_0']):
   d=self.declaration();d['variants']=vs
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_artifact_creature_counts_itself(self):
  s=self.cases('Chrome Steed',['effect_2_0'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),s)
 def test_nonartifact_host_needs_three_other_artifacts(self):
  low,high=self.cases(selected=['effect_2_0','effect_3_0']);self.assertIn(dict(op='assert_card',ref='source',property='power',value=1),low['steps']);self.assertIn(dict(op='assert_card',ref='source',property='power',value=3),high['steps'])
 def test_threshold_six_and_seven_boundaries(self):
  low,high=self.cases('Krosan Beast',['effect_6_0','effect_7_0']);self.assertIn(dict(op='assert_card',ref='source',property='power',value=1),low['steps']);self.assertIn(dict(op='assert_card',ref='source',property='power',value=8),high['steps'])
 def test_initial_graveyard_is_loaded_as_real_zone_cards(self):
  c=self.cases('Krosan Beast',['effect_7_0'])[0];self.assertEqual(c['fixture'],'empty_main');self.assertEqual(sum(x['player']==0 and x['zone']=='graveyard' for x in c['setup']),7);self.assertNotIn('graveyard_override',c)
 def test_opponent_cards_do_not_satisfy_condition(self):
  for nm in ('Ardent Recruit','Krosan Beast'):
   s=self.cases(nm,['opponent_only_1'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='power',value=1),s)
 def test_duplicates_count_as_multiple_cards(self):
  for nm in ('Ardent Recruit','Krosan Beast'):
   c=self.cases(nm,['duplicates_0'])[0];self.assertIn(dict(op='assert_card',ref='source',property='power',value=3 if nm=='Ardent Recruit' else 8),c['steps'])
 def test_offboard_artifacts_do_not_count(self):
  c=self.cases(selected=['offboard_0'])[0];self.assertEqual({x['zone'] for x in c['setup'] if x['ref'].startswith('outside_')},{'hand','graveyard','exile','library'});self.assertIn(dict(op='assert_card',ref='source',property='power',value=1),c['steps'])
 def test_artifact_transition_casts_real_permanent(self):
  s=self.cases(selected=['increase_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='new_artifact',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='power',value=3),s)
 def test_graveyard_transition_uses_resolving_spell(self):
  s=self.cases('Krosan Beast',['increase_1'])[0]['steps'];self.assertIn(dict(op='cast',ref='new_card',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='power',value=8),s)
 def test_death_also_crosses_threshold(self):
  s=self.cases('Krosan Beast',['death_increase_0'])[0]['steps'];self.assertIn(dict(op='destroy',ref='qualifier'),s);self.assertIn(dict(op='assert_card',ref='source',property='power',value=8),s)
 def test_removing_artifact_removes_bonus(self):
  s=self.cases(selected=['decrease_0'])[0]['steps'];self.assertIn(dict(op='destroy',ref='artifact_0'),s);self.assertIn(dict(op='assert_card',ref='source',property='power',value=1),s)
 def test_graveyard_reset_uses_real_morningtide(self):
  s=self.cases('Krosan Beast',['decrease_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='clear_graves',value=1),s);self.assertIn(dict(op='assert_zone',player=0,zone='graveyard',value=1),s)
 def test_loss_of_bonus_checks_state_based_death(self):
  for nm in ('Ardent Recruit','Nimble Mongoose'):
   s=self.cases(nm,['damage_then_decrease_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='damage',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s)
 def test_shroud_host_uses_untargeted_damage_and_removal(self):
  c=self.cases('Nimble Mongoose',['damage_then_decrease_0'])[0];self.assertFalse(any(x['op']=='choose_target' and x.get('ref')=='source' for x in c['steps']))
 def test_recount_does_not_stack_continuous_bonus(self):
  s=self.cases(selected=['recount_3_0'])[0]['steps'];self.assertEqual(sum(x['op']=='refresh' for x in s),3);self.assertFalse(any(x['op']=='assert_card' and x.get('ref')=='source' and x.get('property')=='power' and x['value']>3 for x in s))
 def test_combat_uses_actual_attacks_and_effective_power(self):
  s=self.cases('Krosan Beast',['combat_7_1'])[0]['steps'];self.assertIn(dict(op='attack',ref='source',value=1),s);self.assertIn(dict(op='combat_damage'),s);self.assertIn(dict(op='assert_life',player=0,value=12),s)
 def test_cleanup_does_not_expire_continuous_bonus(self):
  s=self.cases(selected=['cleanup_0'])[0]['steps'];self.assertIn(dict(op='cleanup'),s);self.assertIn(dict(op='assert_card',ref='source',property='power',value=3),s)
 def test_snapshots_restore_count_and_zones_after_ai_work(self):
  c=self.cases('Krosan Beast',['snapshot_7_0'])[0]
  for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertIn(dict(op=op),c['steps'])
 def test_all_counts_and_controllers_explicit(self):
  p=spec(self.declaration('Krosan Beast'),self.o['Krosan Beast']);vs=variants(p);self.assertEqual(len(vs),len(set(vs)));self.assertTrue(all(f'effect_{n}_{o}' in vs for n in (6,7,8) for o in (0,1)))
 def test_exact_zone_checks_include_battlefield(self):
  s=self.cases(selected=['duplicates_0'])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Sol Ring']['csvid'],value=3),s)
 def test_handler_independence_and_witness_mutations(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy';r['mana_cost']='{99}'
  self.assertEqual(self.cases(),self.cases(rows=rows));o=copy.deepcopy(self.o);o['Morningtide']['oracle_text']='Draw a card.'
  with self.assertRaises(ValueError):self.cases(o=o)

if __name__=='__main__':unittest.main()
