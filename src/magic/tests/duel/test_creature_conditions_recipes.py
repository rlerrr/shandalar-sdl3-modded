import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from creature_conditions_recipes import spec,variants

def declaration(o,name,vs=None):
 text=o[name]['oracle_text'];p=dict(keywords=sorted(o[name]['keywords']),enters_tapped='This creature enters tapped.' in text,attack_land=None)
 if 'defending player controls an Island.' in text:p['attack_land']=dict(type='Island',scope='defender')
 if 'there is a Mountain on the battlefield.' in text:p['attack_land']=dict(type='Mountain',scope='battlefield')
 return dict(family='creature_conditions',version=2,params=p,oracle_text=text,variants=vs or ['entry_0'])

class CreatureConditionsRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def cases(self,name='Hammerhead Shark',vs=None,d=None,rows=None,o=None):return expand(name,self.rows[name]['csvid'],d or declaration(self.o,name,vs),self.o[name],rows or self.rows,oracle_context=o or self.o)
 def attack(self,c):return [x['value'] for x in c['steps'] if x['op']=='attack' and x['ref']=='source']
 def test_parameters_and_oracle_pin_fail_closed(self):
  d=declaration(self.o,'Hammerhead Shark')
  for patch in (dict(params={}),dict(oracle_text='changed'),dict(params=dict(d['params'],enters_tapped=1)),dict(params=dict(d['params'],keywords=['Haste'])),dict(params=dict(d['params'],attack_land={'type':'Swamp','scope':'defender'})),dict(params=dict(d['params'],attack_land={'type':'Island','scope':'owner'}))):
   with self.assertRaises(ValueError):self.cases(d=dict(d,**patch))
 def test_extra_effects_faces_and_vanilla_rejected(self):
  d=declaration(self.o,'Hammerhead Shark');r=dict(self.o['Hammerhead Shark'],oracle_text=d['oracle_text']+'\nWhen this creature enters, draw a card.');d['oracle_text']=r['oracle_text']
  with self.assertRaises(ValueError):spec(d,r)
  with self.assertRaises(ValueError):spec(declaration(self.o,'Hammerhead Shark'),dict(self.o['Hammerhead Shark'],layout='transform'))
  with self.assertRaises(ValueError):self.cases('Grizzly Bears')
 def test_invalid_variant_lists_rejected(self):
  for vs in ([],['unknown_0'],['entry_0','entry_0']):
   d=declaration(self.o,'Hammerhead Shark');d['variants']=vs
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_both_controllers_have_unique_variants(self):
  p=spec(declaration(self.o,'Hammerhead Shark'),self.o['Hammerhead Shark']);v=variants(p);self.assertEqual(len(v),len(set(v)));self.assertEqual({x[:-2] for x in v if x.endswith('_0')},{x[:-2] for x in v if x.endswith('_1')})
 def test_own_island_does_not_satisfy_defender_condition(self):self.assertEqual(self.attack(self.cases(vs=['own_only_0'])[0]),[0])
 def test_enemy_island_satisfies_defender_condition(self):self.assertEqual(self.attack(self.cases(vs=['enemy_only_1'])[0]),[1])
 def test_battlefield_condition_accepts_either_owner(self):
  for c in self.cases('Glacial Crasher',['own_only_0','enemy_only_1']):self.assertEqual(self.attack(c),[1])
 def test_missing_wrong_and_offboard_lands_do_not_qualify(self):
  for c in self.cases(vs=['no_land_0','wrong_type_0','offboard_0']):self.assertEqual(self.attack(c),[0])
 def test_tapped_snow_and_dual_land_types_qualify(self):
  for c in self.cases(vs=['tapped_land_0','snow_land_0','dual_land_0']):self.assertEqual(self.attack(c),[1])
 def test_land_bounce_before_attack_blocks_declaration(self):
  c=self.cases(vs=['remove_before_attack_0'])[0];self.assertEqual(self.attack(c),[0]);self.assertIn(dict(op='cast',ref='remove_land',value=1),c['steps'])
 def test_land_bounce_after_attack_does_not_remove_attacker(self):
  c=self.cases(vs=['remove_after_attack_0'])[0];self.assertEqual(self.attack(c),[1]);self.assertIn(dict(op='assert_life',player=1,value=18),c['steps'])
 def test_spreading_seas_legally_adds_type_and_draws(self):
  c=self.cases(vs=['add_before_attack_0'])[0];self.assertEqual(self.attack(c),[1]);self.assertTrue(any(x['name']=='Spreading Seas' for x in c['setup']));self.assertIn(dict(op='cast',ref='change_type',value=1),c['steps']);self.assertIn(dict(op='assert_zone',player=0,zone='library',value=6),c['steps'])
 def test_blood_moon_legally_adds_mountain_type(self):
  c=self.cases('Glacial Crasher',['add_before_attack_1'])[0];self.assertEqual(self.attack(c),[1]);self.assertTrue(any(x['name']=='Blood Moon' for x in c['setup']))
 def test_type_changed_qualifier_uses_stable_reference_not_printed_zone_id(self):
  c=self.cases(vs=['add_before_attack_0'])[0];s=c['steps']
  self.assertIn(dict(op='assert_card',ref='qualifier',property='in_play',value=1),s)
  self.assertFalse(any(x['op']=='assert_zone_multiplicity' and x['player']==1 and x['zone']=='battlefield' and x['index'] in (self.rows['Forest']['csvid'],self.rows['Island']['csvid']) for x in s))
  self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['zone']=='hand' and x['index']==self.rows['Forest']['csvid'] for x in s))
 def test_entry_tapping_is_not_a_fixture_override(self):
  c=self.cases('Diregraf Ghoul',['entry_0'])[0];self.assertNotIn('state',next(x for x in c['setup'] if x['ref']=='source'));self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),c['steps'])
 def test_cast_creature_enters_tapped(self):
  c=self.cases('Diregraf Ghoul',['cast_0'])[0];self.assertIn(dict(op='cast',ref='source',value=1),c['steps']);self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),c['steps'])
 def test_attack_restriction_does_not_prevent_casting(self):
  s=self.cases(vs=['cast_no_land_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s)
 def test_tapped_or_sick_creature_cannot_attack(self):
  for c in self.cases(vs=['tapped_0','sick_0']):self.assertEqual(self.attack(c),[0])
 def test_real_untap_allows_ready_entry_tapped_creature(self):
  c=self.cases('Diregraf Ghoul',['untap_attack_0'])[0];self.assertEqual(self.attack(c),[1]);self.assertIn(dict(op='cast',ref='untap',value=1),c['steps'])
 def test_untap_does_not_clear_fresh_creatures_sickness(self):
  c=self.cases('Diregraf Ghoul',['fresh_untapped_attack_0'])[0];self.assertEqual(self.attack(c),[0]);self.assertIn(dict(op='cast',ref='untap',value=1),c['steps'])
 def test_no_attack_land_is_needed_to_block(self):
  s=self.cases(vs=['block_ground_0'])[0]['steps'];self.assertIn(dict(op='block',ref='source',other='attacker',value=1),s)
 def test_flying_block_legality(self):
  for nm,allowed in [('Hammerhead Shark',0),('Ethereal Whiskergill',1)]:self.assertIn(dict(op='block',ref='source',other='attacker',value=allowed),self.cases(nm,['block_flying_0'])[0]['steps'])
 def test_entry_tapping_prevents_blocks_until_real_untap(self):
  for v,allowed in [('block_ground_0',0),('untapped_block_ground_0',1)]:self.assertIn(dict(op='block',ref='source',other='attacker',value=allowed),self.cases('Diregraf Ghoul',[v])[0]['steps'])
 def test_cleanup_does_not_untap(self):self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),self.cases('Diregraf Ghoul',['cleanup_0'])[0]['steps'])
 def test_casting_exact_and_insufficient_mana(self):
  a,b=self.cases(vs=['cast_payment_0','insufficient_mana_0']);self.assertIn(dict(op='cast',ref='source',value=1),a['steps']);self.assertIn(dict(op='cast',ref='source',value=0),b['steps'])
 def test_snapshots_repeat_combat_and_casts_after_ai(self):
  for v in ('cast_snapshot_0','ready_snapshot_0','restricted_snapshot_0'):
   s=self.cases(vs=[v])[0]['steps']
   for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertIn(dict(op=op),s)
 def test_populated_zones_have_exact_ownership_and_order(self):
  c=self.cases()[0];self.assertEqual(c['fixture'],'stocked_zones');s=c['steps'];self.assertEqual({x['zone'] for x in s if x['op']=='assert_zone'},{'battlefield','hand','graveyard','exile','library'});self.assertTrue(any(x['op']=='assert_zone_card' for x in s))
 def test_bare_zones_are_explicit(self):self.assertEqual(self.cases(vs=['bare_zones_0'])[0]['fixture'],'empty_main')
 def test_handlers_are_not_used_for_expectations_and_pins_are_checked(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy';r['mana_cost']='{99}'
  self.assertEqual(self.cases(),self.cases(rows=rows));o=copy.deepcopy(self.o);o['Spreading Seas']['oracle_text']='Enchant land\nEnchanted land is a Mountain.'
  with self.assertRaises(ValueError):self.cases(o=o)

if __name__=='__main__':unittest.main()
