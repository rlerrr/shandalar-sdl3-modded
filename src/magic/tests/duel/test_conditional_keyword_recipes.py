import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from conditional_keyword_recipes import spec,variants

class ConditionalKeywordRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Auriok Edgewright',vs=None):
  p=dict(keywords=[],condition='metalcraft',power=0,toughness=0,grant='Double strike')
  if name in ('Snapsail Glider','Auriok Sunchaser','Mystic Visionary','Mystic Penitent','Mystic Zealot'):p['grant']='Flying'
  if name.startswith('Mystic'):p['condition']='threshold'
  if name=='Auriok Sunchaser':p.update(power=2,toughness=2)
  if name=='Mystic Penitent':p.update(power=1,toughness=1,keywords=['Vigilance'])
  if name=="Ezuri's Brigade":p.update(power=4,toughness=4,grant='Trample')
  return dict(family='conditional_keyword',version=2,params=p,oracle_text=self.o[name]['oracle_text'],variants=vs or ['effect_0_0'])
 def cases(self,name='Auriok Edgewright',vs=None,d=None,rows=None,o=None):return expand(name,self.rows[name]['csvid'],d or self.declaration(name,vs),self.o[name],rows or self.rows,oracle_context=o or self.o)
 def test_parameters_and_pin_fail_closed(self):
  d=self.declaration()
  for patch in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],grant='Haste')),dict(params=dict(d['params'],grant={})),dict(params=dict(d['params'],power=True)),dict(params=dict(d['params'],power=8)),dict(params=dict(d['params'],toughness=-1)),dict(params=dict(d['params'],keywords=[{}])),dict(params=dict(d['params'],condition='unreviewed'))):
   with self.assertRaises(ValueError):self.cases(d=dict(d,**patch))
 def test_additional_clauses_and_layout_rejected(self):
  for extra in ('\n{T}: Add {G}.','\nWhen this creature enters, draw a card.'):
   r=dict(self.o['Auriok Edgewright'],oracle_text=self.o['Auriok Edgewright']['oracle_text']+extra);d=self.declaration();d['oracle_text']=r['oracle_text']
   with self.assertRaises(ValueError):spec(d,r)
  with self.assertRaises(ValueError):spec(self.declaration(),dict(self.o['Auriok Edgewright'],layout='transform'))
 def test_incomplete_bonus_and_wrong_keyword_rejected(self):
  d=self.declaration('Auriok Sunchaser')
  for p in (dict(d['params'],power=0),dict(d['params'],grant='Trample')):
   with self.assertRaises(ValueError):self.cases('Auriok Sunchaser',d=dict(d,params=p))
 def test_variants_are_explicit_unique_and_controller_symmetric(self):
  p=spec(self.declaration(),self.o['Auriok Edgewright']);v=variants(p);self.assertEqual(len(v),len(set(v)));self.assertEqual({x.rsplit('_',1)[0] for x in v if x.endswith('_0')},{x.rsplit('_',1)[0] for x in v if x.endswith('_1')})
 def test_invalid_variant_lists_rejected(self):
  for v in ([],['unknown_0'],['effect_0_0','effect_0_0']):
   d=self.declaration();d['variants']=v
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_double_strike_unblocked_damage_uses_both_steps(self):
  low,high=self.cases(vs=['unblocked_2_0','unblocked_3_0']);self.assertIn(dict(op='assert_life',player=1,value=18),low['steps']);self.assertIn(dict(op='assert_life',player=1,value=16),high['steps'])
 def test_double_strike_kills_small_blocker_before_retaliation(self):
  s=self.cases(vs=['effect_3_0'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),s);self.assertIn(dict(op='assert_card',ref='combat_witness',property='in_play',value=0),s)
 def test_large_blocker_receives_two_damage_steps(self):
  s=self.cases(vs=['large_block_3_0'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s);self.assertIn(dict(op='assert_card',ref='combat_witness',property='damage',value=4),s)
 def test_first_strike_can_kill_duelist_before_second_step(self):
  s=self.cases('Spiraling Duelist',['first_strike_block_3_0'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s);self.assertIn(dict(op='assert_card',ref='combat_witness',property='in_play',value=0),s)
 def test_flying_changes_ground_block_legality(self):
  low,high=self.cases('Auriok Sunchaser',['effect_2_0','effect_3_0']);self.assertIn(dict(op='block',ref='combat_witness',other='source',value=1),low['steps']);self.assertIn(dict(op='block',ref='combat_witness',other='source',value=0),high['steps'])
 def test_artifact_source_counts_itself(self):
  s=self.cases('Snapsail Glider',['effect_2_0'])[0]['steps'];self.assertIn(dict(op='block',ref='combat_witness',other='source',value=0),s)
 def test_reach_and_flying_blockers_remain_legal(self):
  for c in self.cases('Auriok Sunchaser',['reach_block_3_0','flying_block_3_0']):self.assertIn(dict(op='block',ref='combat_witness',other='source',value=1),c['steps'])
 def test_threshold_requires_own_seventh_card(self):
  for v in ('effect_6_0','opponent_only_0','offboard_0'):
   s=self.cases('Mystic Visionary',[v])[0]['steps'];self.assertIn(dict(op='block',ref='combat_witness',other='source',value=1),s)
  s=self.cases('Mystic Visionary',['effect_7_0'])[0]['steps'];self.assertIn(dict(op='block',ref='combat_witness',other='source',value=0),s)
 def test_vigilance_survives_with_and_without_threshold(self):
  for c in self.cases('Mystic Penitent',['effect_6_0','effect_7_0']):self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),c['steps'])
 def test_trample_overflow_is_only_active_above_condition(self):
  low,high=self.cases("Ezuri's Brigade",['effect_2_0','effect_3_0']);self.assertIn(dict(op='assert_life',player=1,value=20),low['steps']);self.assertIn(dict(op='assert_life',player=1,value=14),high['steps'])
 def test_count_transitions_probe_actual_combat_after_real_actions(self):
  for c in self.cases('Auriok Sunchaser',['increase_0','decrease_0']):
   s=c['steps'];op='cast' if c['_recipe']['variant']=='increase_0' else 'destroy';self.assertLess(next(i for i,x in enumerate(s) if x['op']==op),next(i for i,x in enumerate(s) if x['op']=='attack'));self.assertIn(dict(op='combat_damage'),s)
 def test_graveyard_clear_is_a_real_cast_before_keyword_probe(self):
  s=self.cases('Mystic Visionary',['decrease_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='clear_graves',value=1),s);self.assertIn(dict(op='block',ref='combat_witness',other='source',value=1),s)
 def test_snapshot_replays_combat_after_ai_restore(self):
  s=self.cases(vs=['snapshot_3_0'])[0]['steps'];self.assertEqual(sum(x['op']=='attack' for x in s),2);self.assertEqual(sum(x['op']=='combat_damage' for x in s),2)
  for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertIn(dict(op=op),s)
 def test_populated_zones_and_ordered_libraries_preserved(self):
  c=self.cases()[0];self.assertEqual(c['fixture'],'empty_main');self.assertEqual({x['zone'] for x in c['setup']},{'battlefield','hand','graveyard','exile','library'});self.assertTrue(any(x['op']=='assert_zone_card' and x['zone']=='library' for x in c['steps']))
 def test_bare_zones_are_deliberate_variant(self):
  c=self.cases(vs=['bare_zones_0'])[0];self.assertEqual({x['zone'] for x in c['setup']},{'battlefield'})
 def test_source_cast_payment_and_insufficient_mana(self):
   a,b=self.cases(vs=['payment_0','insufficient_mana_0']);self.assertIn(dict(op='cast',ref='source',value=1),a['steps']);self.assertIn(dict(op='cast',ref='source',value=0),b['steps']);self.assertFalse(any(x['op']=='attack' for x in b['steps']))
 def test_cast_source_waits_for_real_turn_gate_before_attacking(self):
  s=self.cases(vs=['cast_0_0'])[0]['steps'];self.assertIn(dict(op='attack',ref='source',value=0),s);self.assertEqual(sum(x['op']=='advance_turn' for x in s),2);self.assertIn(dict(op='attack',ref='source',value=1),s)
 def test_exact_ownership_and_witness_zones(self):
   s=self.cases(vs=['effect_3_1'])[0]['steps'];ident=self.rows['Grizzly Bears']['csvid'];self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=ident,value=2),s);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=ident,value=0),s)
 def test_no_raw_keyword_or_event_injection(self):
  for c in self.cases(vs=['effect_0_0','effect_3_0']):self.assertFalse(any(x['op'] in ('event','get_abilities') for x in c['steps']));self.assertIn(dict(op='begin_blockers'),c['steps'])
 def test_handler_independence_and_witness_pin_mutation(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy';r['mana_cost']='{99}'
  self.assertEqual(self.cases(),self.cases(rows=rows));o=copy.deepcopy(self.o);o['Tundra Wolves']['oracle_text']='Double strike'
  with self.assertRaises(ValueError):self.cases(o=o)

if __name__=='__main__':unittest.main()
