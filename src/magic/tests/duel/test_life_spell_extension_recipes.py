import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from life_spell_extension_recipes import PINS,spec,variants,expand,validate_witnesses

class LifeSpellExtensionTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Ancient Craving'):
  return dict(family='life_spell_extension',version=2,oracle_text=self.oracle[name]['oracle_text'],params=copy.deepcopy(PINS['cards'][name]['params']),variants=['effect_0'])
 def cases(self,name='Ancient Craving',vs=('effect_0',),oracle=None,rows=None):
  o=oracle or self.oracle;d=self.declaration(name);d['variants']=list(vs);p=spec(d,o[name])
  return expand(name,self.rows[name]['csvid'],d,p,list(vs),rows or self.rows,o)
 def test_whole_pin_and_typed_explicit_params_fail_closed(self):
  for key,value in (('oracle_text','Gain3 life.'),('mana_cost','{1}'),('layout','split'),('type_line','Enchantment'),('colors',['W'])):
   o=dict(self.oracle['Ancient Craving'],**{key:value});d=self.declaration();d['oracle_text']=o['oracle_text']
   with self.assertRaises(ValueError):spec(d,o)
  d=self.declaration();d['params']['operations'][0]['amount']=True
  with self.assertRaises(ValueError):spec(d,self.oracle['Ancient Craving'])
 def test_unknown_empty_duplicate_variants_rejected(self):
  for vs in ([],['missing_0'],['effect_0','effect_0']):
   with self.assertRaises(ValueError):self.cases(vs=vs)
 def test_registry_handlers_never_supply_expectations(self):
  rows=copy.deepcopy(self.rows)
  for row in rows.values():row['handler']='card_dummy';row['mana_cost']='{99}'
  for name in PINS['cards']:self.assertEqual(self.cases(name),self.cases(name,rows=rows))
 def test_both_controllers_and_variant_uniqueness(self):
  for name in PINS['cards']:
   vs=variants(spec(self.declaration(name),self.oracle[name]));self.assertEqual(len(vs),len(set(vs)))
   self.assertEqual({v[:-2] for v in vs if v.endswith('_0')},{v[:-2] for v in vs if v.endswith('_1')})
 def test_draw_loss_wait_for_resolution_and_exact_other_zones(self):
  s=self.cases()[0]['steps'];cut=s.index(dict(op='resolve'))
  self.assertIn(dict(op='assert_life',player=0,value=50),s[:cut]);self.assertIn(dict(op='assert_life',player=0,value=47),s[cut:])
  self.assertTrue(any(x['op']=='assert_zone_card' and x['zone']=='library' for x in s[cut:]))
  self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['zone']=='exile' for x in s[cut:]))
 def test_gain_and_draw_have_separate_recipients(self):
  s=self.cases('Healing Hands')[0]['steps']
  self.assertIn(dict(op='choose_player',player=0,value=1),s)
  self.assertIn(dict(op='assert_life',player=1,value=54),s)
  self.assertIn(dict(op='assert_life',player=0,value=50),s)
 def test_modal_choice_is_explicit_before_source(self):
  s=self.cases("Abuna's Chant")[0]['steps']
  self.assertLess(s.index(dict(op='choose_dialog',player=0,value=1)),s.index(dict(op='cast',ref='source',value=1)))
  self.assertIn(dict(op='assert_life',player=0,value=55),s)
 def test_zero_and_high_creature_quantity_use_real_boards(self):
  zero,high=self.cases('Festival of Trokin',['quantity_zero_0','quantity_high_1'])
  self.assertFalse(any(x['ref']=='friendly' for x in zero['setup']))
  self.assertIn(dict(op='assert_life',player=0,value=50),zero['steps'])
  self.assertEqual(sum(x['name']=='Grizzly Bears' and x['zone']=='battlefield' and x['player']==1 for x in high['setup']),3)
  self.assertIn(dict(op='assert_life',player=1,value=56),high['steps'])
  self.assertEqual(next(x['name'] for x in zero['setup'] if x['ref']=='quantity_type_control'),'Darksteel Relic')
  self.assertIn(dict(op='assert_zone',player=0,zone='battlefield',value=1),zero['steps'])
 def test_graveyard_count_uses_whole_graveyard_and_excludes_pending_source(self):
  effect,bare,extra=self.cases('Ancestral Tribute',['effect_0','bare_zones_1','grave_extra_0'])
  self.assertIn(dict(op='assert_life',player=0,value=58),effect['steps'])
  self.assertIn(dict(op='assert_life',player=1,value=50),bare['steps'])
  self.assertIn(dict(op='assert_life',player=0,value=64),extra['steps'])
 def test_real_shroud_response_drains_parent_in_response_context(self):
  s=self.cases('Healing Hands',['target_shrouded_pending_0'])[0]['steps'];at=s.index(dict(op='cast_response',ref='shield',value=1))
  self.assertEqual(s[at+1],dict(op='assert_stack',value=2));self.assertEqual(s[at+2],dict(op='resolve_response'));self.assertEqual(s[at+3],dict(op='assert_stack',value=0))
  self.assertNotIn(dict(op='resolve'),s[at+4:]);self.assertIn(dict(op='assert_life',player=1,value=50),s)
 def test_snapshots_replay_life_and_library_receipts(self):
  for v in ('ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0'):
   s=self.cases(vs=[v])[0]['steps'];cut=s.index(dict(op='restore'))
   self.assertIn(dict(op='assert_recorded_state'),s)
   self.assertTrue(any(x['op']=='assert_life' for x in s[cut:]));self.assertTrue(any(x['op']=='assert_zone_card' for x in s[cut:]))
 def test_witness_mutations_rejected(self):
  for name in ('Grizzly Bears','Hill Giant','Gilded Light'):
   o=copy.deepcopy(self.oracle);o[name]['oracle_text']='changed'
   with self.assertRaises(ValueError):validate_witnesses(o,self.rows)
 def test_countered_source_changes_no_life_or_draw(self):
  s=self.cases(vs=['countered_cast_0'])[0]['steps']
  self.assertIn(dict(op='choose_priority',ref='counter'),s);self.assertNotIn(dict(op='assert_life',player=0,value=47),s)
 def test_every_fixed_positive_case_changes_life(self):
  for name in PINS['cards']:
   s=self.cases(name)[0]['steps'];self.assertTrue(any(x['op']=='assert_life' and x['value']!=50 for x in s),name)
 def test_energy_and_poison_receipts_include_negative_controls(self):
  s=self.cases('Live Fast')[0]['steps'];self.assertIn(dict(op='assert_energy',player=0,value=2),s);self.assertIn(dict(op='assert_energy',player=1,value=0),s)
  s=self.cases('Live Fast',['countered_cast_1'])[0]['steps'];self.assertIn(dict(op='assert_energy',player=1,value=0),s)
  s=self.cases('Caress of Phyrexia',['own_target_1'])[0]['steps'];self.assertIn(dict(op='assert_poison',player=1,value=3),s);self.assertIn(dict(op='assert_poison',player=0,value=0),s)
  s=self.cases('Caress of Phyrexia',['target_shrouded_pending_0'])[0]['steps'];self.assertIn(dict(op='assert_poison',player=1,value=0),s)
 def test_hand_quantities_exclude_pending_source(self):
  for name in ("Gerrard's Wisdom",'Presence of the Wise'):
   for v,life in (('effect_0',58),('large_hand_0',74),('own_empty_0',50)):
    self.assertIn(dict(op='assert_life',player=0,value=life),self.cases(name,[v])[0]['steps'])
 def test_grave_creature_predicate_does_not_count_land_artifact_instant(self):
  for v,life in (('effect_0',52),('grave_extra_0',58),('bare_zones_0',50)):
   self.assertIn(dict(op='assert_life',player=0,value=life),self.cases('Gnaw to the Bone',[v])[0]['steps'])
 def test_land_quantities_have_tapped_mana_and_subtype_controls(self):
  for v,life in (('effect_0',54),('quantity_zero_0',50),('quantity_high_0',58)):
   c=self.cases('Landbind Ritual',[v])[0]
   self.assertIn(dict(op='assert_life',player=0,value=life),c['steps'])
   self.assertTrue(all('tapped' in x.get('state',[]) for x in c['setup'] if x['ref'].startswith('quantity_land_')))
   if v=='quantity_zero_0':self.assertEqual({x['name'] for x in c['setup'] if x['ref'].startswith('quantity_land_')},{'Forest','Island'})
 def test_land_snapshot_receipts_do_not_accumulate_alias_adjustments(self):
  s=self.cases('Landbind Ritual',['pending_snapshot_0'])[0]['steps']
  values=[x['value'] for x in s if x['op']=='assert_zone' and x.get('zone')=='battlefield' and x['player']==0]
  self.assertEqual(values,[5,5])
 def test_each_player_creature_quantity_uses_its_own_controller(self):
  s=self.cases('Stronghold Discipline',['quantity_high_0'])[0]['steps']
  self.assertIn(dict(op='assert_life',player=0,value=47),s);self.assertIn(dict(op='assert_life',player=1,value=49),s)
  s=self.cases('Stronghold Discipline',['quantity_zero_0'])[0]['steps']
  self.assertIn(dict(op='assert_life',player=0,value=50),s);self.assertIn(dict(op='assert_life',player=1,value=49),s)
 def test_normal_buyback_choice_follows_auto_bound_target(self):
  s=self.cases('Brush with Death')[0]['steps'];self.assertIn(dict(op='choose_dialog',player=0,value=1),s)
  self.assertFalse(any(x['op']=='choose_player' for x in s))
  self.assertFalse(any(x['op']=='choose_dialog' for x in self.cases('Brush with Death',['payment_0'])[0]['steps']))
  s=self.cases('Brush with Death',['countered_cast_0'])[0]['steps'];self.assertLess(s.index(dict(op='choose_dialog',player=0,value=1)),s.index(dict(op='choose_priority',ref='counter')))
 def test_expanded_dispatch_equals_direct_component(self):
  from recipes import expand as dispatch
  for name in ('Live Fast','Landbind Ritual','Brush with Death'):
   d=self.declaration(name);self.assertEqual(self.cases(name),dispatch(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,oracle_context=self.oracle))
 def test_reviewed_modal_and_normal_payment_bindings(self):
  for name,choice in (('Healing Salve',1),('Healing Leaves',1),('Heroes Remembered',0)):
   s=self.cases(name)[0]['steps'];at=s.index(dict(op='choose_dialog',player=0,value=choice))
   self.assertLess(at,s.index(dict(op='cast',ref='source',value=1)))
   if name!='Heroes Remembered':self.assertLess(at,s.index(dict(op='choose_player',player=0,value=1)))
  for owner in (0,1):
   s=self.cases("Dryad's Caress",[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='set_mana',player=owner,color=5,value=0),s)
   self.assertTrue(all(x['value']==0 for x in s if x['op']=='assert_mana' and x.get('player')==owner and x.get('color')==5))
   self.assertFalse(any(x['op']=='choose_dialog' for x in s))
 def test_exactly_unsafe_storm_responses_remain_outside_active_variants(self):
  d=self.declaration('Tendrils of Agony');vs=variants(spec(d,self.oracle['Tendrils of Agony']))
  for owner in (0,1):
   self.assertNotIn(f'target_shrouded_pending_{owner}',vs)
   self.assertIn(f'pending_snapshot_{owner}',vs)
   self.assertIn(f'own_target_{owner}',vs)

if __name__=='__main__':unittest.main()
