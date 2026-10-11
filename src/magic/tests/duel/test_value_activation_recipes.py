import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec,expand
from value_activation_recipes import variants,validate_witnesses

class ValueActivationRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text())['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Jayemdae Tome',selected=None,**changes):
  p=dict(effect='draw',amount=1,mana_cost='{4}',tap=True,sacrifice=False,pay_life=0,mana_outputs=[]);p.update(changes)
  return dict(family='value_activation',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['value_0'])
 def cases(self,name='Jayemdae Tome',selected=None,**changes):
  return expand(name,self.rows[name]['csvid'],self.declaration(name,selected,**changes),self.oracle[name],self.rows,oracle_context=self.oracle)
 def test_complete_text_and_explicit_keys(self):
  d=self.declaration()
  for changes in (dict(params={}),dict(params=dict(d['params'],extra=1)),dict(oracle_text='changed')):
   with self.assertRaises(ValueError):explicit_spec(dict(d,**changes),self.oracle['Jayemdae Tome'])
 def test_permanent_scope(self):
  for changes in (dict(type_line='Instant'),dict(type_line='Artifact — Equipment'),dict(type_line='Token Artifact'),dict(layout='transform'),dict(card_faces=[{}])):
   with self.assertRaises(ValueError):explicit_spec(self.declaration(),dict(self.oracle['Jayemdae Tome'],**changes))
 def test_exact_effect_and_cost(self):
  for changes in (dict(effect='gain_life'),dict(amount=2),dict(mana_cost='{3}'),dict(tap=False),dict(sacrifice=True),dict(pay_life=2),dict(mana_outputs=['{C}'])):
   with self.assertRaises(ValueError):self.cases(**changes)
 def test_integer_bool_and_amount_bounds(self):
  for changes in (dict(amount=True),dict(amount=4),dict(tap=1),dict(sacrifice=0),dict(pay_life=True),dict(pay_life=6)):
   with self.assertRaises(ValueError):self.cases(**changes)
 def test_nonstandard_cost_rejected(self):
  for cost in ('{X}','{W/U}','{C}','{11}'):
   with self.assertRaises(ValueError):self.cases(mana_cost=cost)
 def test_added_restrictions_rejected(self):
  for name in ("Fool's Tome",'Loreseeker\'s Stone','Humble Defector',"Yawgmoth's Bargain",'Arcanis the Omnipotent','Compulsion'):
   with self.assertRaises(ValueError):self.cases(name)
 def test_zero_cast_has_no_false_negative_variant(self):
  p=explicit_spec(self.declaration('Fountain of Youth',effect='gain_life',mana_cost='{2}'),self.oracle['Fountain of Youth'])
  self.assertNotIn('insufficient_cast_mana_0',variants(p))
 def test_witness_refresh_fails_closed(self):
  for name in ('Twiddle','Boomerang','Suppression Field','Grizzly Bears','Hill Giant'):
   oracle=copy.deepcopy(self.oracle);oracle[name]['oracle_text']='changed'
   with self.assertRaises(ValueError):validate_witnesses(oracle,self.rows)
 def test_costs_before_real_resolution(self):
  s=self.cases()[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='activate');j=next(i for i,a in enumerate(s) if a['op']=='resolve')
  self.assertLess(i,j)
  self.assertIn(dict(op='require_card',ref='source',property='tapped',value=1),s[i:j])
  self.assertIn(dict(op='assert_stack',value=1),s[i:j])
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=7),s[i:j])
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=6),s[j:])
 def test_both_controllers_draw_from_their_library(self):
  s=self.cases(selected=['value_1'])[0]['steps']
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=6),s)
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=7),s)
 def test_multi_draw_order(self):
  s=self.cases('Courier\'s Capsule',amount=2,mana_cost='{1}{U}',sacrifice=True)[0]['steps']
  self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=0,value=self.rows['Grizzly Bears']['csvid']),s)
  self.assertIn(dict(op='assert_hand_graveyard_total',player=0,index=self.rows['Plains']['csvid'],value=2),s)
 def test_self_sacrifice_paid_before_draw(self):
  s=self.cases("Alchemist's Apprentice",mana_cost='',tap=False,sacrifice=True)[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='activate');j=next(i for i,a in enumerate(s) if a['op']=='resolve')
  self.assertIn(dict(op='require_card',ref='source',property='in_play',value=0),s[i:j])
  self.assertIn(dict(op='assert_zone_contains',player=0,zone='graveyard',value=self.rows["Alchemist's Apprentice"]['csvid']),s[j:])
 def test_life_payment_separate_from_draw(self):
  s=self.cases('Greed',mana_cost='{B}',tap=False,pay_life=2)[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='activate');j=next(i for i,a in enumerate(s) if a['op']=='resolve')
  self.assertIn(dict(op='assert_life',player=0,value=18),s[i:j])
 def test_low_life_stays_alive(self):
  s=self.cases('Greed',['low_life_1'],mana_cost='{B}',tap=False,pay_life=2)[0]['steps']
  self.assertIn(dict(op='set_life',player=1,value=3),s);self.assertIn(dict(op='assert_life',player=1,value=1),s)
 def test_life_gain_occurs_only_on_resolution(self):
  s=self.cases('Soulmender',effect='gain_life',mana_cost='')[0]['steps'];j=next(i for i,a in enumerate(s) if a['op']=='resolve')
  self.assertIn(dict(op='assert_life',player=0,value=20),s[:j]);self.assertIn(dict(op='assert_life',player=0,value=21),s[j:])
 def test_generic_red_payment_explicit_human_choice(self):
  for owner in (0,1):
   s=self.cases(selected=['red_payment_'+str(owner)])[0]['steps']
   self.assertEqual(sum(a['op']=='choose_mana' for a in s),4 if owner==0 else 0)
   self.assertIn(dict(op='assert_mana',player=owner,color=4,value=0),s)
 def test_refresh_after_isolated_pool(self):
  s=self.cases(selected=['insufficient_mana_0'])[0]['steps'];ops=[a['op'] for a in s]
  self.assertLess(max(i for i,a in enumerate(s) if a['op']=='set_mana'),ops.index('refresh'));self.assertLess(ops.index('refresh'),ops.index('require_can_activate'))
 def test_tax_is_legally_cast_then_turn_restored(self):
  s=self.cases(selected=['tax_0'])[0]['steps'];i=s.index(dict(op='cast',ref='taxer',value=1))
  self.assertEqual(s[i-1],dict(op='turn',player=1,value=0));self.assertIn(dict(op='turn',player=0,value=0),s[i:])
  self.assertIn(dict(op='set_mana',player=0,color=0,value=6),s)
 def test_pending_snapshot_replays_after_nested_ai(self):
  s=self.cases(selected=['pending_snapshot_1'])[0]['steps'];ops=[a['op'] for a in s]
  self.assertLess(ops.index('activate'),ops.index('capture'));self.assertLess(ops.index('capture'),ops.index('resolve'));self.assertEqual(ops.count('activate'),1);self.assertEqual(ops.count('resolve'),2);self.assertIn('ai_roundtrip',ops)
 def test_ready_snapshot_replays_costs(self):
  s=self.cases(selected=['ready_snapshot_0'])[0]['steps'];ops=[a['op'] for a in s]
  self.assertLess(ops.index('capture'),ops.index('activate'));self.assertEqual(ops.count('activate'),2)
 def test_bare_zones_deliberate_nonempty_library(self):
  c=self.cases(selected=['bare_zones_0'])[0];self.assertEqual(c['fixture'],'empty_main');self.assertEqual(sum(a['zone']=='library' for a in c['setup']),14)
 def test_pending_bounce_does_not_cancel_nontargeted_ability(self):
  c=self.cases(selected=['source_bounced_pending_0'])[0];s=c['steps']
  self.assertEqual(next(a['zone'] for a in c['setup'] if a['ref']=='response'),'hand');self.assertIn(dict(op='cast',ref='response',value=1),s)
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=6),s)
 def test_two_abilities_accumulate_draws(self):
  s=self.cases('Azure Mage',['stacked_activations_1'],mana_cost='{3}{U}',tap=False)[0]['steps']
  self.assertEqual(sum(a['op']=='activate' for a in s),2);self.assertIn(dict(op='assert_stack',value=2),s);self.assertIn(dict(op='assert_zone',player=1,zone='library',value=5),s)
 def test_no_tap_sacrifice_works_fresh_creature(self):
  s=self.cases("Alchemist's Apprentice",['fresh_activation_0'],mana_cost='',tap=False,sacrifice=True)[0]['steps']
  self.assertIn(dict(op='require_can_activate',ref='source',value=1),s)
 def test_fresh_tap_creature_denied(self):
  s=self.cases('Archivist',['fresh_activation_0'],mana_cost='')[0]['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
 def test_alternate_mana_mode_has_no_stack_or_draw(self):
  s=self.cases('Mind Stone',['mana_0_1'],mana_cost='{1}',sacrifice=True,mana_outputs=['{C}'])[0]['steps']
  self.assertFalse(any(a['op']=='choose_dialog' for a in s));self.assertNotIn('resolve',[a['op'] for a in s]);self.assertIn(dict(op='assert_mana',player=1,color=0,value=1),s)
 def test_unaffordable_value_uses_legal_mana_alternative(self):
  s=self.cases('Azorius Cluestone',['wrong_color_0'],mana_cost='{W}{U}',sacrifice=True,mana_outputs=['{W}','{U}'])[0]['steps']
  self.assertIn(dict(op='require_can_activate',ref='source',value=1),s);self.assertIn(dict(op='choose_dialog',player=0,value=0),s);self.assertNotIn(dict(op='choose_dialog',player=0,value=1),s)
 def test_all_mode_colors_expanded(self):
  p=explicit_spec(self.declaration('Abzan Banner',mana_cost='{W}{B}{G}',sacrifice=True,mana_outputs=['{W}','{B}','{G}']),self.oracle['Abzan Banner'])
  self.assertEqual(len([v for v in variants(p) if v.startswith('mana_')]),6)
 def test_land_has_no_synthetic_cast_cases(self):
  p=explicit_spec(self.declaration('Blighted Cataract',amount=2,mana_cost='{5}{U}',sacrifice=True,mana_outputs=['{C}']),self.oracle['Blighted Cataract'])
  self.assertFalse(any(v.startswith(('cast_','fresh_activation_','insufficient_cast')) for v in variants(p)))
 def test_unknown_variant_and_custom_choices_rejected(self):
  with self.assertRaises(ValueError):self.cases(selected=['nonsense_0'])
  d=self.declaration();d['resolve_dialogs']=[1]
  with self.assertRaises(ValueError):expand('Jayemdae Tome',self.rows['Jayemdae Tome']['csvid'],d,self.oracle['Jayemdae Tome'],self.rows,oracle_context=self.oracle)
 def test_cast_checks_entry_without_illegal_fresh_tap(self):
  s=self.cases('Soulmender',['cast_1'],effect='gain_life',mana_cost='')[0]['steps']
  self.assertIn(dict(op='cast',ref='source',value=1),s);self.assertFalse(any(a['op']=='activate' for a in s))
 def test_cleanup_checks_payment_before_cleanup_only(self):
  s=self.cases(selected=['cleanup_0'])[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='cleanup')
  self.assertIn(dict(op='assert_mana',player=0,color=0,value=6),s[:i]);self.assertFalse(any(a['op']=='assert_mana' for a in s[i:]))
 def test_only_mana_color_choice_when_value_unaffordable(self):
  s=self.cases('Azorius Cluestone',['insufficient_mana_1'],mana_cost='{W}{U}',sacrifice=True,mana_outputs=['{W}','{U}'])[0]['steps']
  self.assertIn(dict(op='choose_color',player=1,value=5),s);self.assertFalse(any(a['op']=='choose_dialog' for a in s))
 def test_expensive_repeats_have_enough_generic_mana(self):
  for variant in ('two_sources_0','two_sources_1','untap_repeat_0','untap_repeat_1'):
   s=self.cases('Tower of Eons',[variant],effect='gain_life',amount=10,mana_cost='{8}')[0]['steps']
   self.assertTrue(any(a['op']=='set_mana' and a['color']==0 and a['value']==16 for a in s))
   self.assertFalse(any(a['op']=='assert_mana' and a['value']<0 for a in s))

if __name__=='__main__':unittest.main()
