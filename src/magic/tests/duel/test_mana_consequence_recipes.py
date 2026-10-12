import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from mana_consequence_recipes import spec,variants
from mana_creature_recipes import ANY_COLOR

class ManaConsequenceRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Adarkar Wastes',selected=None):
  p=dict(production='colorless_choice',outputs=['{C}','{W}','{U}'],colored_damage=1,fixed_damage=0,life_payment=0,opponent_life=0,enters_tapped=False)
  if name=='Ancient Tomb':p.update(production='fixed',outputs=['{C}{C}'],colored_damage=0,fixed_damage=2)
  if name in ('Mana Confluence','Phyrexian Lens'):p.update(production='any_color',outputs=ANY_COLOR,colored_damage=0,life_payment=1)
  if name=='Grove of the Burnwillows':p.update(outputs=['{C}','{R}','{G}'],colored_damage=0,opponent_life=1)
  if name=='Talisman of Progress':pass
  if name=='Grand Coliseum':p.update(production='colorless_any',outputs=['{C}']+ANY_COLOR,enters_tapped=True)
  if name=='Salt Flats':p.update(outputs=['{C}','{W}','{B}'],enters_tapped=True)
  return dict(family='mana_consequence',version=2,params=p,oracle_text=self.o[name]['oracle_text'],variants=selected or ['production_1_0'])
 def cases(self,name='Adarkar Wastes',selected=None,d=None,rows=None,o=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.o[name],rows or self.rows,oracle_context=o or self.o)
 def test_whole_pin_and_parameter_types_fail_closed(self):
  d=self.declaration()
  for change in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],colored_damage=True)),dict(params=dict(d['params'],colored_damage=4)),dict(params=dict(d['params'],outputs=['{X}'])),dict(params=dict(d['params'],outputs=['{C}','{W}','{W}'])),dict(params=dict(d['params'],production='unreviewed')),dict(params=dict(d['params'],enters_tapped=1))):
   with self.assertRaises(ValueError):self.cases(d=dict(d,**change))
 def test_additional_rules_and_keywords_not_certified(self):
  for line in ('\nWhen this land enters, draw a card.','\n{T}: Add {R}.'):
   r=dict(self.o['Adarkar Wastes'],oracle_text=self.o['Adarkar Wastes']['oracle_text']+line);d=self.declaration();d['oracle_text']=r['oracle_text']
   with self.assertRaises(ValueError):spec(d,r)
  for change in (dict(keywords=['Indestructible']),dict(type_line='Artifact Creature — Golem'),dict(layout='transform')):
   with self.assertRaises(ValueError):spec(self.declaration(),dict(self.o['Adarkar Wastes'],**change))
 def test_variants_unknown_empty_and_duplicates_rejected(self):
  for vs in ([],['wrong_0'],['production_1_0','production_1_0']):
   d=self.declaration();d['variants']=vs
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_every_output_and_both_players_have_production(self):
  vs=variants(spec(self.declaration(),self.o['Adarkar Wastes']))
  self.assertTrue(all(f'production_{i}_{owner}' in vs for i in range(3) for owner in (0,1)))
 def test_colorless_mode_avoids_pain(self):
  s=self.cases(selected=['production_0_0'])[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=20),s);self.assertIn(dict(op='choose_color',player=0,value=0),s)
 def test_colored_production_deals_damage_to_controller(self):
  for owner in (0,1):
   s=self.cases(selected=[f'production_1_{owner}'])[0]['steps'];self.assertIn(dict(op='assert_life',player=owner,value=19),s);self.assertIn(dict(op='assert_life',player=1-owner,value=20),s)
 def test_fixed_two_mana_and_damage_are_independent(self):
  s=self.cases('Ancient Tomb',['production_0_1'])[0]['steps'];self.assertIn(dict(op='assert_mana',player=1,color=0,value=22),s);self.assertIn(dict(op='assert_life',player=1,value=18),s);self.assertFalse(any(x['op']=='choose_color' for x in s))
 def test_payment_pool_starts_empty_and_mana_ability_has_no_stack(self):
  s=self.cases(selected=['payment_2_1'])[0]['steps'];cut=s.index(dict(op='activate',ref='source',value=1));self.assertIn(dict(op='set_mana',player=1,color=2,value=0),s[:cut]);self.assertIn(dict(op='assert_mana',player=1,color=2,value=1),s[cut:]);self.assertIn(dict(op='assert_stack',value=0),s[cut:])
 def test_damage_is_prevented_by_safe_passage(self):
  s=self.cases(selected=['prevention_1_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='prevent',value=1),s);self.assertIn(dict(op='assert_life',player=0,value=20),s)
 def test_life_cost_is_not_prevented(self):
  s=self.cases('Mana Confluence',['prevention_0_0'])[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=19),s)
 def test_opponent_life_gain_is_not_prevented(self):
  s=self.cases('Grove of the Burnwillows',['prevention_1_1'])[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=21),s);self.assertIn(dict(op='assert_life',player=1,value=20),s)
 def test_opponent_gain_does_not_apply_to_colorless_mode(self):
  s=self.cases('Grove of the Burnwillows',['production_0_0'])[0]['steps'];self.assertIn(dict(op='assert_life',player=1,value=20),s)
 def test_tapped_entry_checked_before_legal_untap(self):
  entry=self.cases('Salt Flats',['entry_0'])[0]['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=0),entry)
  s=self.cases('Salt Flats',['production_1_0'])[0]['steps'];self.assertLess(s.index(dict(op='cast',ref='entry_untap',value=1)),s.index(dict(op='activate',ref='source',value=1)))
 def test_incidental_tap_has_no_mana_or_life_consequence(self):
  s=self.cases(selected=['incidental_tap_0'])[0]['steps'];self.assertFalse(any(x['op']=='activate' for x in s));self.assertIn(dict(op='assert_life',player=0,value=20),s);self.assertIn(dict(op='assert_mana',player=0,color=5,value=20),s)
 def test_retap_does_not_repeat_colored_pain(self):
  s=self.cases(selected=['retap_without_mana_1_0'])[0]['steps'];self.assertEqual(sum(x['op']=='activate' for x in s),1);self.assertIn(dict(op='assert_life',player=0,value=19),s)
 def test_repeat_needs_untap_and_charges_consequence_twice(self):
  s=self.cases(selected=['untap_repeat_1_0'])[0]['steps'];self.assertEqual(sum(x['op']=='activate' and x.get('value')==1 for x in s),2);self.assertIn(dict(op='assert_life',player=0,value=18),s)
 def test_two_sources_and_taxes_cover_every_mode(self):
  vs=variants(spec(self.declaration(),self.o['Adarkar Wastes']));self.assertTrue(all(f'tax_{i}_0' in vs and f'two_sources_{i}_1' in vs for i in range(3)))
 def test_produced_mana_pays_real_spell_cost(self):
  s=self.cases(selected=['spend_colored_1_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='spender_5_0',value=1),s);self.assertIn(dict(op='assert_mana',player=0,color=5,value=0),s)
 def test_generic_payment_consumes_the_whole_mixed_pool(self):
  for owner in (0,1):
   s=self.cases(selected=[f'spend_generic_1_{owner}'])[0]['steps'];cut=s.index(dict(op='cast',ref='spender',value=1))
   final=[x for x in s[cut:] if x['op']=='assert_mana' and x['player']==owner]
   self.assertEqual(len(final),6);self.assertTrue(all(x['value']==0 for x in final))
 def test_noncreature_fresh_cast_can_tap(self):
  s=self.cases('Talisman of Progress',['fresh_activation_0'])[0]['steps'];self.assertLess(s.index(dict(op='cast',ref='source',value=1)),s.index(dict(op='require_can_activate',ref='source',value=1)))
 def test_source_removal_does_not_remove_produced_mana(self):
  s=self.cases('Ancient Tomb',['source_destroyed_0'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s);self.assertIn(dict(op='assert_mana',player=0,color=0,value=22),s)
 def test_both_players_zones_and_order_are_checked(self):
  s=self.cases()[0]['steps']
  for owner in (0,1):
   self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['player']==owner and x['zone']=='exile' for x in s));self.assertTrue(any(x['op']=='assert_zone_card' and x['player']==owner and x['zone']=='library' for x in s))
 def test_snapshots_are_replayed_after_ai_work(self):
  for c in self.cases(selected=['ready_snapshot_1_0','mana_snapshot_2_1','resolved_snapshot_0']):
   s=c['steps'];self.assertIn(dict(op='ai_roundtrip'),s);self.assertIn(dict(op='restore'),s);self.assertIn(dict(op='assert_recorded_state'),s)
 def test_handlers_do_not_supply_expectations_and_witnesses_are_pinned(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy';r['mana_cost']='{99}'
  self.assertEqual(self.cases(),self.cases(rows=rows))
  o=copy.deepcopy(self.o);o['Safe Passage']['oracle_text']='Prevent 1 damage.'
  with self.assertRaises(ValueError):self.cases(o=o)

if __name__=='__main__':unittest.main()
