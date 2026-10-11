import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import explicit_spec,expand
from mill_recipes import variants,validate_witnesses

class FixedMillRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text())['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Millstone',selected=None,**changes):
  p=dict(mode='activation',amount=2,recipient='any',draw=0,draw_first=False,mana_cost='{2}',tap=True,sacrifice=False);p.update(changes)
  return dict(family='fixed_mill',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['effect_0'])
 def cases(self,name='Millstone',selected=None,**changes):
  return expand(name,self.rows[name]['csvid'],self.declaration(name,selected,**changes),self.oracle[name],self.rows,oracle_context=self.oracle)
 def test_explicit_keys(self):
  for p in ({},dict(self.declaration()['params'],extra=1)):
   with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),params=p),self.oracle['Millstone'])
 def test_exact_effect_cost_and_target(self):
  for changes in (dict(amount=3),dict(recipient='self'),dict(mana_cost='{1}'),dict(tap=False),dict(sacrifice=True),dict(draw=1)):
   with self.assertRaises(ValueError):self.cases(**changes)
 def test_bounds_and_bool_types(self):
  for changes in (dict(amount=True),dict(amount=11),dict(draw=4),dict(tap=1),dict(draw_first=True)):
   with self.assertRaises(ValueError):self.cases(**changes)
 def test_nonstandard_costs(self):
  for cost in ('{X}','{C}','{W/U}','{11}'):
   with self.assertRaises(ValueError):self.cases(mana_cost=cost)
 def test_full_text_pin(self):
  with self.assertRaises(ValueError):explicit_spec(dict(self.declaration(),oracle_text='changed'),self.oracle['Millstone'])
 def test_unsupported_record(self):
  for changes in (dict(layout='transform'),dict(card_faces=[{}]),dict(type_line='Token Artifact'),dict(type_line='Artifact — Equipment'),dict(keywords=['Mill','Storm'])):
   with self.assertRaises(ValueError):explicit_spec(self.declaration(),dict(self.oracle['Millstone'],**changes))
 def test_variable_conditional_and_additional_abilities_rejected(self):
  for name in ('Grindstone','Codex Shredder','Sands of Delirium','Keening Stone','Dream Twist','Dampen Thought'):
   with self.assertRaises(ValueError):self.cases(name)
 def test_witness_refresh(self):
  for name in ('Counterspell','Boomerang','Twiddle'):
   o=copy.deepcopy(self.oracle);o[name]['oracle_text']='changed'
   with self.assertRaises(ValueError):validate_witnesses(o,self.rows)
 def test_real_activation_then_resolution(self):
  s=self.cases()[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='activate');j=next(i for i,a in enumerate(s) if a['op']=='resolve')
  self.assertLess(i,j);self.assertIn(dict(op='assert_zone',player=1,zone='library',value=28),s[i:j]);self.assertIn(dict(op='assert_zone',player=1,zone='library',value=26),s[j:])
 def test_exact_mill_order_and_graveyard_order(self):
  s=self.cases()[0]['steps']
  self.assertIn(dict(op='assert_zone_card',player=1,zone='library',index=0,value=self.rows['Grizzly Bears']['csvid']),s)
  self.assertIn(dict(op='assert_zone_card',player=1,zone='graveyard',index=4,value=self.rows['Forest']['csvid']),s)
 def test_self_target(self):
  s=self.cases(selected=['self_target_0'])[0]['steps'];self.assertIn(dict(op='choose_player',player=0,value=0),s)
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=28),s);self.assertIn(dict(op='assert_zone',player=0,zone='library',value=26),s)
 def test_both_players_mill(self):
  s=self.cases('Whetstone',mana_cost='{3}',tap=False,recipient='both')[0]['steps']
  self.assertFalse(any(a['op']=='choose_player' for a in s));self.assertIn(dict(op='assert_zone',player=0,zone='library',value=26),s);self.assertIn(dict(op='assert_zone',player=1,zone='library',value=26),s)
 def test_short_library_is_not_a_draw(self):
  c=self.cases(selected=['short_library_0'])[0];s=c['steps']
  self.assertEqual(sum(a['zone']=='library' and a['player']==1 for a in c['setup']),1)
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=0),s);self.assertIn(dict(op='assert_life',player=1,value=20),s)
 def test_empty_library_no_loss(self):
  s=self.cases(selected=['empty_library_0'])[0]['steps'];self.assertIn(dict(op='assert_zone',player=1,zone='library',value=0),s)
 def test_exact_payment(self):
  s=self.cases(selected=['payment_1'])[0]['steps'];self.assertIn(dict(op='set_mana',player=1,color=0,value=2),s);self.assertIn(dict(op='assert_mana',player=1,color=0,value=0),s)
 def test_sacrifice_is_paid_before_effect(self):
  s=self.cases('Morgue Thrull',amount=3,recipient='self',mana_cost='',tap=False,sacrifice=True)[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='activate');j=next(i for i,a in enumerate(s) if a['op']=='resolve')
  self.assertIn(dict(op='assert_zone_contains',player=0,zone='hand',value=self.rows['Giant Growth']['csvid']),s[i:j])
  self.assertIn(dict(op='assert_zone_card',player=0,zone='graveyard',index=4,value=self.rows['Morgue Thrull']['csvid']),s[i:j])
 def test_sacrificed_source_not_destroyed_twice(self):
  p=explicit_spec(self.declaration('Morgue Thrull',amount=3,recipient='self',mana_cost='',tap=False,sacrifice=True),self.oracle['Morgue Thrull'])
  self.assertNotIn('source_destroyed_pending_0',variants(p));self.assertNotIn('insufficient_mana_0',variants(p))
 def test_source_bounce_does_not_cancel_ability(self):
  s=self.cases(selected=['source_bounced_pending_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='response',value=1),s);self.assertIn(dict(op='assert_zone',player=1,zone='library',value=26),s)
 def test_entry_target_choice_at_resolution(self):
  s=self.cases('Merrow Witsniper',mode='entry',amount=1,mana_cost='',tap=False)[0]['steps'];ops=[a['op'] for a in s]
  self.assertLess(ops.index('cast'),ops.index('choose_player'));self.assertLess(ops.index('choose_player'),ops.index('resolve'))
 def test_entry_countered_has_no_target_choice(self):
  s=self.cases('Merrow Witsniper',['countered_0'],mode='entry',amount=1,mana_cost='',tap=False)[0]['steps'];self.assertFalse(any(a['op']=='choose_player' for a in s))
 def test_draw_follows_self_mill(self):
  s=self.cases('Mental Note',mode='spell',recipient='self',mana_cost='',tap=False,draw=1)[0]['steps']
  self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=0,value=self.rows['Forest']['csvid']),s)
  self.assertIn(dict(op='assert_hand_graveyard_total',player=0,index=self.rows['Grizzly Bears']['csvid'],value=3),s)
 def test_draw_before_mill(self):
  s=self.cases("Thassa's Bounty",mode='spell',amount=3,mana_cost='',tap=False,draw=3,draw_first=True,selected=['self_target_0'])[0]['steps']
  self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=0,value=self.rows['Lightning Bolt']['csvid']),s)
 def test_pending_snapshot_preserves_decisions(self):
  s=self.cases(selected=['pending_snapshot_1'])[0]['steps'];ops=[a['op'] for a in s]
  self.assertLess(ops.index('activate'),ops.index('capture'));self.assertLess(ops.index('capture'),ops.index('resolve'));self.assertEqual(ops.count('activate'),1);self.assertEqual(ops.count('resolve'),2);self.assertIn('ai_roundtrip',ops)
 def test_ready_snapshot_replays_payment(self):
  ops=[a['op'] for a in self.cases(selected=['ready_snapshot_0'])[0]['steps']];self.assertLess(ops.index('capture'),ops.index('activate'));self.assertEqual(ops.count('activate'),2)
 def test_empty_graveyard_keeps_other_zones_populated(self):
  c=self.cases(selected=['empty_graveyard_0'])[0];self.assertFalse(any(a['zone']=='graveyard' for a in c['setup']));self.assertTrue(all(any(a['zone']==z for a in c['setup']) for z in ('hand','library','exile')))
 def test_cleanup_does_not_repeat_mill(self):
  s=self.cases(selected=['cleanup_once_0'])[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='cleanup');self.assertIn(dict(op='assert_zone',player=1,zone='library',value=26),s[:i]);self.assertIn(dict(op='assert_zone',player=1,zone='library',value=26),s[i:])
 def test_invalid_variant_rejected(self):
  with self.assertRaises(ValueError):self.cases(selected=['made_up_0'])
 def test_ai_policy_refusal_is_not_a_rules_baseline(self):
  p=explicit_spec(self.declaration(),self.oracle['Millstone']);self.assertIn('self_target_0',variants(p));self.assertNotIn('self_target_1',variants(p))
 def test_pending_hand_inspector_is_not_semantic_hand_zone(self):
  s=self.cases('Thought Scour',mode='spell',mana_cost='',tap=False,draw=1)[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='cast');j=next(i for i,a in enumerate(s) if a['op']=='resolve')
  self.assertFalse(any(a['op']=='assert_zone' and a['zone']=='hand' for a in s[i:j]));self.assertTrue(any(a['op']=='assert_zone' and a['zone']=='hand' for a in s[j:]))
 def test_stacked_activations_are_on_stack_together(self):
  s=self.cases('Whetstone',['stacked_activations_1'],mana_cost='{3}',tap=False,recipient='both')[0]['steps'];j=next(i for i,a in enumerate(s) if a['op']=='resolve')
  self.assertEqual(sum(a['op']=='activate' for a in s[:j]),2);self.assertIn(dict(op='assert_stack',value=2),s[:j]);self.assertIn(dict(op='assert_zone',player=1,zone='library',value=24),s[j:])
 def test_legendary_fixture_does_not_require_unscripted_legend_choice(self):
  p=explicit_spec(self.declaration('Ambassador Laquatus',amount=3,mana_cost='{3}',tap=False),self.oracle['Ambassador Laquatus']);self.assertNotIn('two_copies_0',variants(p));self.assertIn('stacked_activations_1',variants(p))
 def test_forced_opponent_choice_is_not_a_menu(self):
  s=self.cases('Mind Sculpt',mode='spell',amount=7,recipient='opponent',mana_cost='',tap=False)[0]['steps'];self.assertFalse(any(a['op']=='choose_player' for a in s));self.assertIn(dict(op='assert_zone',player=1,zone='library',value=21),s)

if __name__=='__main__':unittest.main()
