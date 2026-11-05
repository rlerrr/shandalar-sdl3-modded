import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from hand_exchange_recipes import variants


def op(kind,amount,scope='recipient',random=False):return dict(kind=kind,amount=amount,scope=scope,random=random)


class HandExchangeRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

 def declaration(self,name='Careful Study',selected=None):
  configs={
   'Careful Study':dict(operations=[op('draw',2),op('discard',2)]),
   'Mind Rot':dict(recipient='any',operations=[op('discard',2)]),
   'Hymn to Tourach':dict(recipient='any',operations=[op('discard',2,random=True)]),
   'Wistful Thinking':dict(recipient='any',operations=[op('draw',2),op('discard',4)]),
   'Tolarian Winds':dict(operations=[op('discard','all'),op('draw','discarded')]),
   'Windfall':dict(recipient='both',operations=[op('discard','all'),op('draw','max_discarded')]),
   'Dark Deal':dict(recipient='both',operations=[op('discard','all'),op('draw','discarded_minus_one')]),
   'Forget':dict(recipient='any',operations=[op('discard',2),op('draw','discarded')]),
   'Purge the Profane':dict(recipient='opponent',operations=[op('discard',2),op('gain_life',2,'self')]),
   'Ravenous Rats':dict(mode='entry',recipient='opponent',operations=[op('discard',1)]),
   'Miasmic Mummy':dict(mode='entry',recipient='both',operations=[op('discard',1)]),
   'Mindslicer':dict(mode='death',recipient='both',operations=[op('discard','all')]),
   'Black Cat':dict(mode='death',recipient='opponent',operations=[op('discard',1,random=True)]),
   'Thought Courier':dict(mode='activation',operations=[op('draw',1),op('discard',1)],tap=True),
   'Magus of the Bazaar':dict(mode='activation',operations=[op('draw',2),op('discard',3)],tap=True),
   'Bonded Fetch':dict(mode='activation',operations=[op('draw',1),op('discard',1)],tap=True,keywords=['Defender','Haste']),
   'Rummaging Goblin':dict(mode='activation',operations=[op('draw',1)],tap=True,discard=1),
   'Unfulfilled Desires':dict(mode='activation',operations=[op('draw',1),op('discard',1)],mana='{1}',pay_life=1),
   'Hapless Researcher':dict(mode='activation',operations=[op('draw',1),op('discard',1)],sacrifice=True),
   'Tormenting Voice':dict(operations=[op('draw',2)],discard=1),
   'Geier Reach Sanitarium':dict(mode='activation',recipient='both',operations=[op('draw',1),op('discard',1)],mana='{2}',tap=True,mana_ability=True),
  }
  p=dict(mode='spell',recipient='self',operations=[],cost=dict(mana='',tap=False,sacrifice=False,pay_life=0,discard=0),keywords=[],mana_ability=False)
  for k,v in configs[name].items():
   if k in p['cost']:p['cost'][k]=v
   else:p[k]=v
  return dict(family='hand_exchange',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['effect_0'])

 def cases(self,name='Careful Study',selected=None,d=None,oracle=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,oracle_context=oracle or self.oracle)

 def has(self,steps,**data):return any(all(s.get(k)==v for k,v in data.items()) for s in steps)

 def test_target_presentation_annotations_cannot_change_oracle_recipients(self):
  for n in (-1,3,True,'1'):
   d=self.declaration('Mind Rot');d['target_choices']=n
   with self.assertRaises(ValueError):self.cases('Mind Rot',d=d)
  d=self.declaration();d['target_choices']=0
  with self.assertRaises(ValueError):self.cases(d=d)
  for n in (0,2):
   d=self.declaration('Mind Rot');d['target_choices']=n
   s=self.cases('Mind Rot',d=d)[0]['steps']
   self.assertEqual(sum(x['op']=='choose_player' for x in s),n)
   self.assertTrue(self.has(s,op='assert_zone',player=1,zone='hand',value=2))

 def test_two_pending_activations_keep_both_stack_entries_and_pay_each_cost(self):
  s=self.cases('Unfulfilled Desires',['stacked_activations_0'])[0]['steps']
  self.assertEqual([x['value'] for x in s if x['op']=='assert_stack'],[1,2,0])
  self.assertTrue(self.has(s,op='assert_life',player=0,value=18))
  self.assertTrue(self.has(s,op='assert_mana',player=0,color=0,value=18))
  self.assertTrue(self.has(s,op='assert_zone',player=0,zone='library',value=26))

 def test_complete_ordered_effect_and_cost_pins_fail_closed(self):
  for patch in (dict(operations=[op('discard',2),op('draw',2)]),dict(recipient='opponent'),dict(keywords=['Flying'])):
   d=self.declaration();d['params'].update(patch)
   with self.assertRaises(ValueError):self.cases(d=d)
  for n in (True,0,8,'all',{},'max_discarded'):
   d=self.declaration();d['params']['operations'][0]['amount']=n
   with self.assertRaises(ValueError):self.cases(d=d)
  for patch in (dict(oracle_text='Draw a card.'),dict(variants=[]),dict(variants=['unknown_0'])):
   with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),**patch))

 def test_lowest_identity_policy_can_discard_a_newly_drawn_card(self):
  s=self.cases()[0]['steps']
  self.assertIn(dict(op='discard_policy',value=1),s)
  for n in ('Forest','Giant Growth'):
   self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows[n]['csvid'],value=0))
  self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Plains']['csvid'],value=2))

 def test_highest_identity_policy_exercises_a_distinct_legal_choice(self):
  s=self.cases(selected=['last_choice_0'])[0]['steps']
  self.assertIn(dict(op='discard_policy',value=2),s)
  self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Plains']['csvid'],value=0))
  self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Forest']['csvid'],value=1))

 def test_random_receipts_bound_the_legal_hand_pool_and_preserve_every_identity(self):
  s=self.cases('Hymn to Tourach')[0]['steps']
  self.assertTrue(self.has(s,op='assert_zone',player=1,zone='hand',value=2))
  self.assertTrue(self.has(s,op='assert_zone_multiplicity_max',player=1,zone='hand',index=self.rows['Grizzly Bears']['csvid'],value=1))
  self.assertTrue(self.has(s,op='assert_hand_graveyard_total',player=1,index=self.rows['Grizzly Bears']['csvid'],value=2))
  self.assertTrue(self.has(s,op='assert_zone_multiplicity_max',player=1,zone='hand',index=self.rows['Sol Ring']['csvid'],value=0))
  self.assertFalse(self.has(s,op='choose_zone'))

 def test_too_small_hands_discard_only_available_cards(self):
  s=self.cases('Wistful Thinking',['own_empty_0'])[0]['steps']
  self.assertTrue(self.has(s,op='assert_zone',player=0,zone='hand',value=0))
  s=self.cases('Mind Rot',['opponent_empty_0'])[0]['steps']
  self.assertTrue(self.has(s,op='require_can_cast',ref='source',value=1))
  self.assertTrue(self.has(s,op='assert_zone',player=1,zone='hand',value=0))

 def test_windfall_uses_the_largest_actual_discard_count_for_both_players(self):
  s=self.cases('Windfall',['uneven_hands_0'])[0]['steps']
  for pl in (0,1):
   self.assertTrue(self.has(s,op='assert_zone',player=pl,zone='hand',value=8))
   self.assertTrue(self.has(s,op='assert_zone',player=pl,zone='library',value=20))

 def test_discard_derived_draw_and_minus_one_handle_empty_hands(self):
  s=self.cases('Dark Deal',['own_empty_0'])[0]['steps']
  self.assertTrue(self.has(s,op='assert_zone',player=0,zone='hand',value=0))
  self.assertTrue(self.has(s,op='assert_zone',player=1,zone='hand',value=3))
  s=self.cases('Forget',['opponent_empty_0'])[0]['steps']
  self.assertTrue(self.has(s,op='assert_zone',player=1,zone='library',value=28))
  self.assertTrue(self.has(s,op='assert_zone',player=1,zone='hand',value=0))

 def test_discard_cost_is_paid_before_drawing_and_survives_a_counter(self):
  s=self.cases('Tormenting Voice',['payment_0'])[0]['steps']
  resolve=next(i for i,x in enumerate(s) if x['op']=='resolve')
  # Four physical slots remain: three hand cards and the pending spell.
  self.assertTrue(self.has(s[:resolve],op='assert_zone',player=0,zone='hand',value=4))
  self.assertTrue(self.has(s[:resolve],op='assert_zone',player=0,zone='graveyard',value=5))
  self.assertTrue(self.has(s,op='assert_zone',player=0,zone='hand',value=5))
  s=self.cases('Tormenting Voice',['countered_cast_0'])[0]['steps']
  self.assertTrue(self.has(s,op='assert_zone',player=0,zone='hand',value=3))
  self.assertTrue(self.has(s,op='assert_zone',player=0,zone='library',value=28))
  self.assertTrue(self.has(s,op='choose_priority',ref='counter'))

 def test_no_discard_cost_can_borrow_cards_from_its_later_draw(self):
  s=self.cases('Rummaging Goblin',['empty_hand_cost_0'])[0]['steps']
  self.assertIn(dict(op='activate',ref='source',value=0),s)
  self.assertTrue(self.has(s,op='assert_zone',player=0,zone='library',value=28))
  s=self.cases('Tormenting Voice',['bare_zones_0'])[0]['steps']
  self.assertIn(dict(op='cast',ref='source',value=0),s)

 def test_tap_sickness_haste_and_nontap_costs_have_distinct_permissions(self):
  for name,variant,expected in [('Thought Courier','sick_0',0),('Bonded Fetch','sick_0',1),('Bonded Fetch','fresh_activation_0',1),('Unfulfilled Desires','tapped_source_0',1)]:
   s=self.cases(name,[variant])[0]['steps'];self.assertIn(dict(op='activate',ref='source',value=expected),s)

 def test_untap_and_bounce_helpers_cannot_be_discarded_before_they_are_used(self):
  for name,variant,witness in [('Thought Courier','untap_repeat_1',"Kiora's Follower"),('Ravenous Rats','bounce_recast_1','Temporal Adept')]:
   c=self.cases(name,[variant])[0]
   self.assertTrue(any(x['name']==witness and x['zone']=='battlefield' for x in c['setup']))
   self.assertFalse(any(x['name']==witness and x['zone']=='hand' for x in c['setup']))

 def test_entry_and_death_triggers_use_casting_and_different_source_exits(self):
  s=self.cases('Ravenous Rats')[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s)
  s=self.cases('Mindslicer',['source_exiled_0'])[0]['steps']
  self.assertTrue(self.has(s,op='assert_zone',player=1,zone='hand',value=4))
  self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Mindslicer']['csvid'],value=1))

 def test_self_sacrifice_is_visible_before_resolution(self):
  s=self.cases('Hapless Researcher')[0]['steps'];at=next(i for i,x in enumerate(s) if x['op']=='resolve')
  self.assertTrue(self.has(s[:at],op='assert_zone',player=0,zone='graveyard',value=5))
  p=explicit_spec(self.declaration('Hapless Researcher'),self.oracle['Hapless Researcher']);self.assertNotIn('repeat_0',variants(p))

 def test_life_payments_and_taxes_are_separate_from_resolution_receipts(self):
  s=self.cases('Unfulfilled Desires',['payment_0'])[0]['steps'];at=next(i for i,x in enumerate(s) if x['op']=='resolve')
  self.assertTrue(self.has(s[:at],op='assert_life',player=0,value=19))
  s=self.cases('Unfulfilled Desires',['tax_0'])[0]['steps'];self.assertTrue(self.has(s,op='set_mana',player=0,color=0,value=3))
  p=explicit_spec(self.declaration('Unfulfilled Desires'),self.oracle['Unfulfilled Desires']);self.assertNotIn('insufficient_life_0',variants(p))

 def test_each_player_discards_without_a_target_and_shroud_fizzles_targeted_spells(self):
  s=self.cases('Miasmic Mummy')[0]['steps'];self.assertFalse(any(x['op']=='choose_player' for x in s))
  s=self.cases('Purge the Profane',['target_shrouded_pending_0'])[0]['steps']
  self.assertIn(dict(op='cast',ref='shield',value=1),s)
  self.assertTrue(self.has(s,op='assert_life',player=0,value=20))
  self.assertTrue(self.has(s,op='assert_zone',player=1,zone='hand',value=4))

 def test_legendary_sources_do_not_get_illegal_simultaneous_duplicates(self):
  p=explicit_spec(self.declaration('Geier Reach Sanitarium'),self.oracle['Geier Reach Sanitarium']);self.assertNotIn('two_sources_0',variants(p));self.assertNotIn('insufficient_mana_0',variants(p))

 def test_snapshots_replay_actual_choices_and_pipeline_receipts(self):
  s=self.cases('Hymn to Tourach',['pending_snapshot_1'])[0]['steps']
  for op_name in ('capture','record_state','restore','ai_roundtrip','assert_recorded_state'):self.assertTrue(any(x['op']==op_name for x in s))
  c=self.cases('Mindslicer',['pending_snapshot_0'])[0];s=c['steps'];at=next(i for i,x in enumerate(s) if x['op']=='capture')
  self.assertIn(dict(op='cast',ref='removal',value=1),s[:at]);self.assertIn(dict(op='assert_stack',value=1),s[:at])
  self.assertIn(dict(op='resolve'),s[at:])

 def test_witness_pins_and_inputs_are_immutable(self):
  d=self.declaration();before=copy.deepcopy(d);self.cases(d=d);self.assertEqual(d,before)
  o=copy.deepcopy(self.oracle);o['Temporal Adept']['oracle_text']='Draw a card.'
  with self.assertRaises(ValueError):self.cases(oracle=o)


if __name__=='__main__':unittest.main()
