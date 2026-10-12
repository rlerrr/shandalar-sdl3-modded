import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from conditional_mana_land_recipes import spec,boards,ready,variants

class ConditionalManaLandRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Seachrome Coast',selected=None):
  p=dict(outputs=['{W}','{U}'],condition='at_most_other_lands',limit=2,types=[])
  if name=='Glacial Fortress':p.update(condition='land_types',limit=0,types=['Plains','Island'])
  if name=='Canopy Vista':p.update(outputs=['{G}','{W}'],condition='at_least_basics')
  return dict(family='conditional_mana_land',version=2,oracle_text=self.o[name]['oracle_text'],params=p,variants=selected or ['entry_none_0'])
 def cases(self,name='Seachrome Coast',selected=None,d=None,rows=None,o=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.o[name],rows or self.rows,oracle_context=o or self.o)
 def test_explicit_pin_and_types_fail_closed(self):
  d=self.declaration()
  for change in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],limit=True)),dict(params=dict(d['params'],limit=3)),dict(params=dict(d['params'],outputs=['{W}','{W}'])),dict(params=dict(d['params'],outputs=['{W}',{}])),dict(params=dict(d['params'],outputs=['{W}','{C}'])),dict(params=dict(d['params'],types=[{}])),dict(params=dict(d['params'],condition='unreviewed'))):
   with self.assertRaises(ValueError):self.cases(d=dict(d,**change))
 def test_extra_rules_and_faces_not_certified(self):
  for line in ('\nWhen this land enters, draw a card.','\n{T}: Add {G}.'):
   r=dict(self.o['Seachrome Coast'],oracle_text=self.o['Seachrome Coast']['oracle_text']+line);d=self.declaration();d['oracle_text']=r['oracle_text']
   with self.assertRaises(ValueError):spec(d,r)
  for change in (dict(keywords=['Indestructible']),dict(layout='transform'),dict(type_line='Artifact')):
   with self.assertRaises(ValueError):spec(self.declaration(),dict(self.o['Seachrome Coast'],**change))
 def test_selected_variants_unknown_empty_duplicates_rejected(self):
  for vs in ([],['bad_0'],['entry_none_0','entry_none_0']):
   d=self.declaration();d['variants']=vs
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_fast_land_threshold_excludes_entering_land(self):
  p=spec(self.declaration(),self.o['Seachrome Coast']);self.assertTrue(ready(p,['Wasteland']*2,self.o));self.assertFalse(ready(p,['Wasteland']*3,self.o))
 def test_nonlands_do_not_count_as_lands(self):
  p=spec(self.declaration(),self.o['Seachrome Coast']);self.assertTrue(ready(p,['Sol Ring','Grizzly Bears','Phyrexian Hulk'],self.o))
 def test_battle_land_counts_basic_supertype_not_land_subtypes(self):
  p=spec(self.declaration('Canopy Vista'),self.o['Canopy Vista']);self.assertTrue(ready(p,['Forest','Island'],self.o));self.assertFalse(ready(p,['Tundra','Bayou'],self.o));self.assertFalse(ready(p,['Forest','Tundra'],self.o))
 def test_duplicate_basic_names_still_count_twice(self):
  p=spec(self.declaration('Canopy Vista'),self.o['Canopy Vista']);self.assertTrue(ready(p,['Forest','Forest'],self.o))
 def test_check_land_accepts_nonbasic_with_qualifying_types(self):
  p=spec(self.declaration('Glacial Fortress'),self.o['Glacial Fortress']);self.assertTrue(ready(p,['Tundra'],self.o));self.assertFalse(ready(p,['Forest','Wasteland'],self.o))
 def test_both_qualifying_types_are_independent_alternatives(self):
  p=spec(self.declaration('Glacial Fortress'),self.o['Glacial Fortress']);self.assertTrue(ready(p,['Plains'],self.o));self.assertTrue(ready(p,['Island'],self.o))
 def test_entry_state_is_not_overridden_in_fixture(self):
  for c in self.cases(selected=['entry_lands_2_0','entry_lands_3_0']):
   source=next(x for x in c['setup'] if x['ref']=='source');self.assertNotIn('state',source)
 def test_qualifiers_enter_before_source(self):
  c=self.cases('Canopy Vista',['entry_two_basics_1'])[0];refs=[x['ref'] for x in c['setup']];self.assertGreater(refs.index('source'),refs.index('board_1_1'))
 def test_late_qualifiers_enter_after_source_and_do_not_untap_it(self):
  c=self.cases('Canopy Vista',['entry_late_basics_0'])[0];refs=[x['ref'] for x in c['setup']];self.assertLess(refs.index('source'),refs.index('board_0_0'));self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),c['steps'])
 def test_opponent_qualifiers_do_not_count(self):
  c=self.cases('Glacial Fortress',['entry_opponent_only_1'])[0];self.assertTrue(any(x['player']==0 and x['name']=='Plains' for x in c['setup']));self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
 def test_matching_cards_outside_battlefield_do_not_count(self):
  c=self.cases('Glacial Fortress',['entry_offboard_0'])[0];self.assertEqual({x['zone'] for x in c['setup'] if x['ref'].startswith('outside_')},{'hand','graveyard','exile','library'});self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
 def test_removing_qualifiers_does_not_recheck_ready_entry(self):
  s=self.cases('Canopy Vista',['qualifiers_removed_0'])[0]['steps'];self.assertEqual(sum(x['op']=='destroy' for x in s),2);self.assertIn(dict(op='require_can_activate',ref='source',value=1),s)
 def test_reducing_land_count_does_not_untap_existing_fast_land(self):
  s=self.cases(selected=['remove_excess_0'])[0]['steps'];self.assertIn(dict(op='destroy',ref='board_0_0'),s);self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
 def test_sequential_entries_check_changed_land_count(self):
  s=self.cases(selected=['sequential_boundary_0'])[0]['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=1),s);self.assertIn(dict(op='require_can_activate',ref='second',value=0),s)
 def test_production_from_tapped_entry_uses_legal_untap(self):
  s=self.cases('Glacial Fortress',['production_none_0_0'])[0]['steps'];self.assertLess(s.index(dict(op='cast',ref='entry_untap',value=1)),s.index(dict(op='activate',ref='source',value=1)))
 def test_every_board_output_and_controller_is_selected(self):
  p=spec(self.declaration(),self.o['Seachrome Coast']);vs=variants(p);self.assertEqual(len(vs),len(set(vs)))
  self.assertTrue(all(f'production_{b}_{i}_{owner}' in vs for b in boards(p) for i in (0,1) for owner in (0,1)))
 def test_generated_mana_pays_colored_and_generic_spells(self):
  for v in ('spend_colored_0_0','spend_generic_1_1'):
   s=self.cases(selected=[v])[0]['steps'];self.assertIn(dict(op='cast',ref='spender',value=1),s);self.assertTrue(all(x['value']==0 for x in s if x['op']=='assert_mana' and x['player']==int(v[-1]) and x in s[s.index(dict(op='cast',ref='spender',value=1)):]))
 def test_final_zones_use_exact_multiplicities_including_battlefield(self):
  s=self.cases('Canopy Vista',['entry_same_basics_0'])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Forest']['csvid'],value=2),s);self.assertTrue(any(x['op']=='assert_zone_card' and x['zone']=='library' for x in s))
 def test_snapshots_cover_entry_ready_and_mana_states(self):
  for c in self.cases(selected=['entry_snapshot_lands_3_0','ready_snapshot_0_0','mana_snapshot_1_1']):
   for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertIn(dict(op=op),c['steps'])
 def test_handlers_do_not_supply_expectations(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy';r['mana_cost']='{99}'
  self.assertEqual(self.cases(),self.cases(rows=rows))
 def test_mutated_witnesses_and_custom_setup_are_rejected(self):
  o=copy.deepcopy(self.o);o['Tundra']['type_line']='Basic Land — Plains Island'
  with self.assertRaises(ValueError):self.cases(o=o)
  with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),support_permanents=['Forest']))

if __name__=='__main__':unittest.main()
