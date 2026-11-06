"""Growth declarations fail closed and assert earned, persistent counters."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
import growth_counter_spell_recipes as growth

class GrowthCounterSpellRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
  cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Burst of Strength',**changes):
  p=dict(scope='single',amount=1,draw=0,untap=True,condition='none',followup='none',tail='none');p.update(changes)
  return dict(family='growth_counter_spell',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=['effect_0'])
 def cases(self,name='Burst of Strength',selected=('effect_0',),**changes):
  d=self.declaration(name,**changes);p=growth.spec(d,self.oracle[name]);d['variants']=list(selected)
  return growth.expand(name,self.rows[name]['csvid'],d,p,list(selected),self.rows,self.oracle)
 def test_complete_pin_and_explicit_parameters_required(self):
  d=self.declaration();r=self.oracle['Burst of Strength']
  for changed in (dict(d,params={}),dict(d,oracle_text='changed'),dict(d,params=dict(d['params'],extra=True)),{k:v for k,v in d.items() if k!='variants'}):
   with self.assertRaises(ValueError):growth.spec(changed,r)
  for key,value in (('amount',True),('amount',0),('amount',6),('draw',True),('untap',1),('scope','distribute'),('tail','unknown'),('condition','threshold')):
   with self.assertRaises(ValueError):growth.spec(self.declaration(**{key:value}),r)
 def test_no_additional_oracle_rules_can_silently_enter(self):
  for suffix in ('\nDraw a card.','\nCycling {2}','\nYou gain 2 life.'):
   r=dict(self.oracle['Burst of Strength']);r['oracle_text']+=suffix
   with self.assertRaises(ValueError):growth.spec(dict(self.declaration(),oracle_text=r['oracle_text']),r)
 def test_face_type_and_variable_cost_rejected(self):
  for change in (dict(layout='transform'),dict(card_faces=[{}]),dict(type_line='Enchantment'),dict(mana_cost='{X}{G}')):
   with self.assertRaises(ValueError):growth.spec(self.declaration(),dict(self.oracle['Burst of Strength'],**change))
 def test_unknown_variant_rejected(self):
  with self.assertRaises(ValueError):self.cases(selected=['invented_0'])
 def test_positive_assertions_require_counter_and_stats_not_only_resolution(self):
  s=self.cases()[0]['steps']
  for prop,value in (('plus1_counters',1),('power',3),('toughness',2),('tapped',0)):
   self.assertIn(dict(op='assert_card',ref='first',property=prop,value=value),s)
  self.assertIn(dict(op='assert_card',ref='unselected',property='plus1_counters',value=0),s)
 def test_cleanup_preserves_counters_but_pump_expires(self):
  s=self.cases(selected=['pump_0'])[0]['steps'];cut=next(i for i,a in enumerate(s) if a['op']=='cleanup')
  self.assertIn(dict(op='cast',ref='pump',value=1),s[:cut])
  self.assertIn(dict(op='assert_card',ref='first',property='power',value=6),s[:cut])
  for prop,value in (('power',3),('plus1_counters',1)):
   self.assertIn(dict(op='assert_card',ref='first',property=prop,value=value),s[cut:])
 def test_both_controllers_and_opponent_targets(self):
  c=self.cases(selected=['opponent_1'])[0]
  self.assertIn(dict(op='turn',player=1,value=0),c['steps'])
  self.assertIn(dict(op='choose_target',player=1,ref='first'),c['steps'])
  self.assertEqual(next(a['player'] for a in c['setup'] if a['ref']=='first'),0)
 def test_countered_spell_has_no_effect_and_pays(self):
  s=self.cases(selected=['countered_0'])[0]['steps']
  self.assertIn(dict(op='choose_priority',ref='response'),s)
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=0),s)
  self.assertIn(dict(op='assert_mana_total',player=0,value=57),s)
 def test_pending_target_departure_is_real_response_stack(self):
  s=self.cases(selected=['bounce_target_1'])[0]['steps']
  self.assertIn(dict(op='cast',ref='response',value=1),s)
  self.assertIn(dict(op='assert_stack',value=2),s)
  self.assertIn(dict(op='assert_card',ref='first',property='in_play',value=0),s)
 def test_bounce_discards_counters(self):
  s=self.cases(selected=['bounce_after_0'])[0]['steps']
  self.assertIn(dict(op='bind_hand',player=0,value=self.rows['Savannah Lions']['csvid'],ref='returned'),s)
  self.assertIn(dict(op='assert_card',ref='returned',property='plus1_counters',value=0),s)
 def test_pending_hexproof_changes_target_legality(self):
  s=self.cases(selected=['protected_target_1'])[0]['steps']
  self.assertIn(dict(op='cast',ref='response',value=1),s)
  self.assertIn(dict(op='assert_stack',value=2),s)
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=0),s)
  self.assertIn(dict(op='assert_card',ref='first',property='power',value=3),s)
 def test_snapshot_replays_pending_resolution(self):
  s=self.cases(selected=['pending_snapshot_1'])[0]['steps'];ops=[a['op'] for a in s]
  cut=ops.index('capture');self.assertIn('cast',ops[:cut]);self.assertEqual(ops[cut+1],'resolve')
  self.assertEqual(ops.count('resolve'),2);self.assertEqual(ops[-1],'assert_recorded_state')
 def test_support_draw_with_targets_and_with_empty_board(self):
  cs=self.cases('Shoulder to Shoulder',['effect_0','empty_0'],scope='support',untap=False,draw=1)
  for c in cs:
   self.assertIn(dict(op='assert_zone',player=0,zone='library',value=6),c['steps'])
  self.assertNotIn('choose_cancel_target',[a['op'] for a in cs[1]['steps']])
 def test_pending_target_selection_receipt_requires_choices_at_cast_time(self):
  c=self.cases('Lead by Example',['selection_pending_0'],scope='support',untap=False)[0]
  ops=[a['op'] for a in c['steps']]
  self.assertIn('cast',ops);self.assertNotIn('resolve',ops)
  self.assertIn(dict(op='assert_stack',value=1),c['steps'])
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=0),c['steps'])
  self.assertIn('cast-time binding',c['_recipe']['untested_components'][0])
 def test_sequential_counter_clause_uses_updated_counter_set(self):
  s=self.cases("Lifecrafter's Gift",['preexisting_0'],untap=False,followup='countered_team')[0]['steps']
  self.assertIn(dict(op='cast',ref='seed',value=1),s)
  for ref in ('first','unselected'):self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=2),s)
 def test_team_untap_includes_preexisting_unselected_counter_holder(self):
  s=self.cases('Unity of Purpose',['preexisting_0'],scope='support',untap=False,followup='untap_countered_team')[0]['steps']
  self.assertIn(dict(op='assert_card',ref='unselected',property='tapped',value=1),s)
  self.assertIn(dict(op='assert_card',ref='unselected',property='tapped',value=0),s)
  self.assertIn(dict(op='assert_card',ref='opposing',property='tapped',value=1),s)
 def test_repeated_bolster_rechecks_least_toughness_without_spurious_choice(self):
  s=self.cases('Cached Defenses',['two_copies_0'],scope='bolster',amount=3,untap=False)[0]['steps']
  self.assertNotIn('choose_target',[a['op'] for a in s])
  for ref in ('first','second_target'):self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=3),s)
 def test_real_death_sets_morbid_and_delirium_uses_four_types(self):
  c=self.cases('Hunger of the Howlpack',['condition_met_0'],untap=False,condition='morbid')[0]
  self.assertIn(dict(op='destroy',ref='opposing'),c['steps'])
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=3),c['steps'])
  c=self.cases('Might Beyond Reason',['condition_met_1'],amount=2,untap=False,condition='delirium')[0]
  self.assertEqual({a['name'] for a in c['setup'] if a['player']==1 and a.get('zone')=='graveyard'}, {'Grizzly Bears','Giant Growth','Forest','Sol Ring'})
 def test_inactive_alternative_components_are_explicit_gaps(self):
  c=self.cases('Increasing Savagery',amount=5,untap=False,tail='graveyard_double')[0]
  self.assertIn('Flashback',c['_recipe']['untested_components'][0]);self.assertIn('ten',c['_recipe']['untested_components'][0])
 def test_exact_payment_asserts_every_color(self):
  s=self.cases(selected=['payment_1'])[0]['steps']
  for color in range(6):self.assertIn(dict(op='assert_mana',player=1,color=color,value=0),s)

 def extension_declaration(self,name,selected=('effect_0',)):
  return dict(family='growth_counter_spell',version=2,params=copy.deepcopy(growth.EXTENSION_PINS['profiles'][name]['params']),oracle_text=self.oracle[name]['oracle_text'],variants=list(selected))
 def extension_cases(self,name,selected=('effect_0',)):
  d=self.extension_declaration(name,selected);p=growth.spec(d,self.oracle[name]);growth.validate_witnesses(self.oracle,self.rows)
  return growth.expand(name,self.rows[name]['csvid'],d,p,list(selected),self.rows,self.oracle)
 def test_extension_is_exact_and_does_not_loosen_prior_schema(self):
  name='Reap What Is Sown';d=self.extension_declaration(name)
  for change in (dict(version=1),dict(params={}),dict(oracle_text='changed'),dict(variants=[]),dict(variants=['effect_0','effect_0'])):
   with self.assertRaises(ValueError):growth.spec(dict(d,**change),self.oracle[name])
  p=copy.deepcopy(d['params']);p['amounts'][0]=True
  with self.assertRaises(ValueError):growth.spec(dict(d,params=p),self.oracle[name])
  with self.assertRaises(ValueError):growth.spec(dict(self.declaration(),params=d['params']),self.oracle['Burst of Strength'])
 def test_every_reviewed_profile_satisfies_the_public_expansion_contract(self):
  from recipes import expand as public_expand
  for name in growth.EXTENSION_PINS['profiles']:
   d=self.extension_declaration(name);p=growth.spec(d,self.oracle[name])
   self.assertEqual(p['family'],'growth_counter_spell')
   cases=public_expand(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,oracle_context=self.oracle)
   self.assertEqual(len(cases),1)
   self.assertEqual(cases[0]['_recipe']['family'],'growth_counter_spell')
   self.assertEqual(cases[0]['_recipe']['version'],2)
 def test_extension_identity_and_full_oracle_pin(self):
  name='Incremental Growth';d=self.extension_declaration(name)
  for change in (dict(mana_cost='{G}'),dict(oracle_text=d['oracle_text']+'\nDraw a card.'),dict(card_faces=[{}]),dict(oracle_id='changed')):
   with self.assertRaises(ValueError):growth.spec(d,dict(self.oracle[name],**change))
 def test_incremental_distinct_targets_receive_printed_amounts(self):
  s=self.extension_cases('Incremental Growth')[0]['steps']
  for ref,n in (('first',1),('second',2),('third',3),('unselected',0)):
   self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=n),s)
 def test_mill_counter_expectation_includes_preexisting_graveyard_creatures(self):
  s=self.extension_cases('Grave Strength')[0]['steps']
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=2),s)
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=4),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2),s)
 def test_fight_has_real_damage_and_deaths_not_a_gap(self):
  c=self.extension_cases("Mutant's Prey")[0];s=c['steps']
  self.assertIn(dict(op='assert_card',ref='first',property='damage',value=2),s)
  self.assertIn(dict(op='assert_card',ref='first',property='power',value=8),s)
  self.assertIn(dict(op='assert_card',ref='enemy',property='in_play',value=0),s)
  self.assertFalse(any('fight' in gap.lower() for gap in c['_recipe']['untested_components']))
  for c in self.extension_cases("Mutant's Prey",['fight_both_die_0','fight_first_dies_1']):
   self.assertIn(dict(op='assert_card',ref='first',property='in_play',value=0),c['steps'])
 def test_fight_predicates_are_card_queries_with_no_other_eligible_recipient(self):
  for c in self.extension_cases("Mutant's Prey",['no_counter_0','wrong_controller_1']):
   self.assertIn(dict(op='can_cast',ref='source',value=0),c['steps'])
   self.assertNotIn('can_target_creature',[s['op'] for s in c['steps']])
 def test_repeated_fights_use_a_second_live_opponent(self):
  s=self.extension_cases("Mutant's Prey",['two_copies_0'])[0]['steps']
  self.assertIn(dict(op='choose_target',player=0,ref='enemy_second'),s)
  self.assertIn(dict(op='assert_card',ref='first',property='damage',value=5),s)
 def test_strive_costs_are_real_and_stop_at_affordable_target_count(self):
  c=self.extension_cases('Solidarity of Heroes',['multi_target_1'])[0];s=c['steps']
  for color,value in ((0,2),(3,2)):self.assertIn(dict(op='set_mana',player=1,color=color,value=value),s)
  for ref in ('first','second'):self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=2),s)
  self.assertNotIn('choose_cancel_target',[a['op'] for a in s])
 def test_only_ai_counterless_control_is_pending(self):
  name='Solidarity of Heroes';p=growth.spec(self.extension_declaration(name),self.oracle[name]);available=growth.variants(p)
  self.assertIn('counterless_0',available);self.assertNotIn('counterless_1',available)
  for v in ('effect_1','multi_target_1','payment_1','ready_snapshot_1','pending_snapshot_1'):self.assertIn(v,available)
 def test_inspiring_call_draw_and_indestructibility_are_both_active(self):
  s=self.extension_cases('Inspiring Call')[0]['steps']
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=5),s)
  for ref in ('murder_marked','murder_unmarked','murder_enemy'):self.assertIn(dict(op='cast',ref=ref,value=1),s)
  self.assertIn(dict(op='require_card',ref='first',property='in_play',value=1),s)
  self.assertIn(dict(op='assert_card',ref='unselected',property='in_play',value=0),s)
  self.assertIn(dict(op='assert_card',ref='enemy',property='in_play',value=0),s)
 def test_inspiring_call_counts_creatures_not_counters_and_expires(self):
  c=self.extension_cases('Inspiring Call',['multiple_counters_0'])[0]
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=5),c['steps'])
  c=self.extension_cases('Inspiring Call',['protection_expiry_0'])[0];ops=[s['op'] for s in c['steps']]
  self.assertIn('cleanup',ops);self.assertIn(dict(op='cast',ref='murder_expired',value=1),c['steps'])
  self.assertIn(dict(op='assert_card',ref='first',property='in_play',value=0),c['steps'])
 def test_new_snapshot_replay_decisions_and_receipts_keep_controller(self):
  s=self.extension_cases('Subtle Strike',['ready_snapshot_1'])[0]['steps']
  self.assertEqual([a['player'] for a in s if a['op']=='choose_dialog'],[1,1])
  self.assertEqual([a['player'] for a in s if a['op']=='choose_target' and a['ref']=='first'],[1,1])
  self.assertEqual(sum(a==dict(op='assert_mana',player=1,color=1,value=9) for a in s),4)
 def test_new_pending_snapshot_does_not_repeat_prep_or_source_cast(self):
  s=self.extension_cases("Mutant's Prey",['pending_snapshot_1'])[0]['steps']
  self.assertEqual(sum(a['op']=='cast' and a['ref']=='source' for a in s),1)
  self.assertEqual(sum(a['op']=='cast' and a['ref']=='seed_0' for a in s),1)
  self.assertEqual(s[-1]['op'],'assert_recorded_state')

 def test_blessings_allocates_all_four_and_same_recipient_stacks(self):
  for variant,expected in (('effect_0',{'first':2,'second':1,'third':1}),('one_target_1',{'first':4,'second':0,'third':0}),('two_copies_0',{'first':4,'second':2,'third':2})):
   s=self.extension_cases('Blessings of Nature',[variant])[0]['steps']
   for ref,n in expected.items():self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=n),s)
  s=self.extension_cases('Blessings of Nature',['cleanup_0'])[0]['steps'];cut=next(i for i,a in enumerate(s) if a['op']=='cleanup')
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=2),s[cut:])
 def test_confluence_selects_three_counter_modes_and_can_repeat_recipient(self):
  c=self.extension_cases('Verdant Confluence',['one_target_1'])[0]
  self.assertEqual([a['value'] for a in c['steps'] if a['op']=='choose_dialog'],[0,0,0])
  self.assertEqual([a['ref'] for a in c['steps'] if a['op']=='choose_target'],['first']*3)
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=6),c['steps'])
 def test_panoply_additional_target_is_paid_and_gets_its_counter(self):
  s=self.extension_cases("Nature's Panoply",['multi_target_1'])[0]['steps']
  for color,n in enumerate([2,0,0,2,0,0]):self.assertIn(dict(op='set_mana',player=1,color=color,value=n),s)
  for ref in ('first','second'):self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=1),s)
  for color in range(6):self.assertIn(dict(op='assert_mana',player=1,color=color,value=0),s)
 def test_collective_chosen_team_has_controls_and_explicit_declined_escalate(self):
  s=self.extension_cases('Collective Effort')[0]['steps']
  self.assertEqual([a['value'] for a in s if a['op']=='choose_dialog'],[2,1])
  self.assertIn(dict(op='choose_player',player=0,value=0),s)
  for ref in ('first','second','third','unselected'):self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=1),s)
  for ref in ('enemy','artifact'):self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=0),s)
  s=self.extension_cases('Collective Effort',['opponent_1'])[0]['steps']
  self.assertIn(dict(op='choose_player',player=1,value=0),s)
  self.assertIn(dict(op='assert_card',ref='enemy',property='plus1_counters',value=1),s)
 def test_awaken_normal_components_keep_whole_source_and_no_land_target(self):
  for name in ('Clutch of Currents','Coastal Discovery','Sheer Drop'):
   c=self.extension_cases(name)[0]
   self.assertEqual(c['_recipe']['oracle_text'],self.oracle[name]['oracle_text'])
   self.assertTrue(any('Awaken' in gap for gap in c['_recipe']['untested_components']))
   self.assertFalse(any('Land' in self.oracle[a['name']]['type_line'] and a.get('zone','battlefield')=='battlefield' and a['player']==0 for a in c['setup']))
  s=self.extension_cases('Coastal Discovery')[0]['steps']
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=5),s)
  s=self.extension_cases('Sheer Drop')[0]
  self.assertEqual(next(a['state'] for a in s['setup'] if a['ref']=='recipient'),['tapped'])
  self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=0),s['steps'])
 def test_awaken_snapshot_occurrences_keep_both_decisions_and_payments_owner1(self):
  from recipes import cost_from_oracle
  for name in ('Clutch of Currents','Coastal Discovery','Sheer Drop'):
   for variant,cast_count,target_count in (('ready_snapshot_1',2,2),('pending_snapshot_1',1,1)):
    s=self.extension_cases(name,[variant])[0]['steps'];receipt=dict(op='assert_mana_total',player=1,value=60-sum(cost_from_oracle(self.oracle[name])))
    self.assertEqual(sum(a['op']=='cast' and a.get('ref')=='source' for a in s),cast_count)
    if name!='Coastal Discovery':self.assertEqual([a['player'] for a in s if a['op']=='choose_target'],[1]*target_count)
    self.assertGreaterEqual(sum(a==receipt for a in s),2)
    self.assertEqual(s[-1]['op'],'assert_recorded_state')
    self.assertFalse(any(a==dict(receipt,player=0) for a in s))

 def test_linked_bolster_sacrifice_is_real_and_empty_enchantment_still_bolsters(self):
  s=self.extension_cases('Abzan Advantage',['effect_1'])[0]['steps']
  self.assertIn(dict(op='choose_player',player=1,value=0),s)
  self.assertIn(dict(op='choose_target',player=0,ref='offered_enchantment'),s)
  self.assertIn(dict(op='assert_card',ref='offered_enchantment',property='in_play',value=0),s)
  for c in self.extension_cases('Abzan Advantage',['no_enchantment_0','two_copies_1']):
   self.assertIn(dict(op='assert_card',ref='second',property='plus1_counters',value=1),c['steps'])
  s=self.extension_cases('Abzan Advantage',['two_copies_1'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=1),s)
 def test_linked_bolster_recalculates_after_destroy_and_between_resolutions(self):
  s=self.extension_cases('Pinion Feast',['two_copies_0'])[0]['steps']
  for ref in ('enemy','enemy_second'):self.assertIn(dict(op='assert_card',ref=ref,property='in_play',value=0),s)
  for ref in ('first','second'):self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=2),s)
  s=self.extension_cases('Pinion Feast',['friendly_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='second',property='plus1_counters',value=2),s)
 def test_linked_map_checks_real_tapped_entry_and_offered_basic_predicate(self):
  s=self.extension_cases('Map the Wastes')[0]['steps']
  self.assertIn(dict(op='observe_search_filters',value=1),s)
  self.assertIn(dict(op='choose_zone_card',player=0,value=self.rows['Forest']['csvid']),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Forest']['csvid'],value=1),s)
  self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows['Forest']['csvid'],property='tapped',value=1),s)
  for name,valid in (('Forest',1),('Grizzly Bears',0),('Wasteland',0)):
   self.assertIn(dict(op='assert_offered_legal_card',index=self.rows[name]['csvid'],value=valid),s)
  self.assertFalse(any(a['op']=='assert_zone_card' and a.get('zone')=='library' and a['player']==0 for a in s))
 def test_linked_map_empty_or_declined_search_still_bolsters(self):
  for c in self.extension_cases('Map the Wastes',['empty_library_0','no_basic_land_1','fail_to_find_0']):
   self.assertIn(dict(op='assert_card',ref='second',property='plus1_counters',value=1),c['steps'])
   self.assertIn(dict(op='assert_zone_multiplicity',player=int(c['_recipe']['variant'][-1]),zone='battlefield',index=self.rows['Forest']['csvid'],value=0),c['steps'])
   self.assertIn(dict(op='assert_forced_zone_choices',value=0),c['steps'])
 def test_linked_destroy_predicates_use_actual_combat_and_card_queries(self):
  s=self.extension_cases('Enduring Victory')[0]['steps'];ops=[a['op'] for a in s]
  self.assertLess(ops.index('begin_attackers'),ops.index('cast'))
  self.assertIn(dict(op='attack',ref='enemy',value=1),s)
  self.assertIn(dict(op='assert_card',ref='enemy',property='in_play',value=0),s)
  for name in ('Enduring Victory','Pinion Feast'):
   s=self.extension_cases(name,['wrong_predicate_1'])[0]['steps']
   self.assertIn(dict(op='can_cast',ref='source',value=0),s)
   self.assertNotIn('can_target_creature',[a['op'] for a in s])
 def test_linked_dromoka_counter_precedes_real_fight_and_damage(self):
  s=self.extension_cases("Dromoka's Command")[0]['steps']
  self.assertEqual([a['value'] for a in s if a['op']=='choose_dialog'],[2,3])
  self.assertEqual([a['ref'] for a in s if a['op']=='choose_target'],['first','first','enemy'])
  for prop,n in (('plus1_counters',1),('power',8),('damage',2)):
   self.assertIn(dict(op='assert_card',ref='first',property=prop,value=n),s)
  self.assertIn(dict(op='assert_card',ref='enemy',property='in_play',value=0),s)
  c=self.extension_cases("Dromoka's Command",['fight_both_die_1'])[0]
  for ref in ('first','enemy'):self.assertIn(dict(op='assert_card',ref=ref,property='in_play',value=0),c['steps'])
 def test_linked_dromoka_missing_fighter_retains_separate_legal_counter(self):
  s=self.extension_cases("Dromoka's Command",['lost_target_0'])[0]['steps']
  self.assertIn(dict(op='cast_response',ref='response',value=1),s)
  self.assertIn(dict(op='assert_stack',value=2),s)
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=1),s)
  self.assertIn(dict(op='assert_card',ref='first',property='damage',value=0),s)
 def test_linked_misfortune_opponent_chooses_and_caster_gets_counters_and_life(self):
  for owner in (0,1):
   s=self.extension_cases('Misfortune',['two_copies_'+str(owner)])[0]['steps']
   self.assertEqual([a['player'] for a in s if a['op']=='choose_dialog'],[1-owner,1-owner])
   self.assertIn(dict(op='assert_life',player=owner,value=28),s)
   self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=2),s)
   self.assertIn(dict(op='assert_card',ref='enemy',property='plus1_counters',value=0),s)
 def test_linked_owner1_snapshots_replay_decisions_and_exact_source_receipts(self):
  for name in ('Abzan Advantage','Map the Wastes','Pinion Feast','Enduring Victory',"Dromoka's Command",'Misfortune'):
   for variant,expected_casts in (('ready_snapshot_1',2),('pending_snapshot_1',1)):
    s=self.extension_cases(name,[variant])[0]['steps']
    self.assertEqual(sum(a['op']=='cast' and a.get('ref')=='source' for a in s),expected_casts)
    receipt=dict(op='assert_mana',player=1,color=0,value=10-int(self.oracle[name]['mana_cost'][1]) if self.oracle[name]['mana_cost'][1].isdigit() else 10)
    self.assertGreaterEqual(sum(a==receipt for a in s),2)
    self.assertEqual(s[-1]['op'],'assert_recorded_state')
    if name=="Dromoka's Command":self.assertEqual([a['player'] for a in s if a['op']=='choose_dialog'],[1]*(4 if expected_casts==2 else 2))
 def test_tempting_offer_accept_and_decline_change_both_teams_correctly(self):
  for variant,own,enemy in (('effect_1',2,1),('decline_0',1,0)):
   s=self.extension_cases('Tempt with Glory',[variant])[0]['steps']
   self.assertIn(dict(op='choose_dialog',player=0 if variant.endswith('1') else 1,value=0 if enemy else 1),s)
   for ref,n in (('first',own),('enemy',enemy),('artifact',0)):
    self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=n),s)
 def test_support_damage_uses_actual_power_and_earlier_marked_unselected_creatures(self):
  s=self.extension_cases("Nissa's Judgment")[0]['steps']
  self.assertIn(dict(op='assert_card',ref='enemy',property='damage',value=6),s)
  for ref in ('first','second'):self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=1),s)
  self.assertIn(dict(op='assert_card',ref='first',property='damage',value=0),s)
  s=self.extension_cases("Nissa's Judgment",['preexisting_0'])[0]['steps']
  self.assertIn(dict(op='choose_target',player=0,ref='unselected'),s)
  self.assertIn(dict(op='assert_card',ref='enemy',property='in_play',value=0),s)
  s=self.extension_cases("Nissa's Judgment",['selection_pending_1'])[0]['steps']
  self.assertEqual([a['ref'] for a in s if a['op']=='choose_target'],['enemy','first','second'])
  self.assertNotIn('resolve',[a['op'] for a in s]);self.assertIn(dict(op='assert_choices_consumed'),s)
 def test_bounty_checks_normal_payment_then_removes_only_its_own_counters(self):
  s=self.extension_cases('Bounty of the Hunt',['preexisting_cleanup_0'])[0]['steps']
  cast=next(i for i,a in enumerate(s) if a['op']=='cast' and a['ref']=='source')
  self.assertEqual(s[cast-1],dict(op='choose_dialog',player=0,value=1))
  cleanup=next(i for i,a in enumerate(s) if a['op']=='cleanup')
  for ref,n in (('first',2),('second',2)):self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=n),s[:cleanup])
  for ref,n in (('first',0),('second',1)):self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=n),s[cleanup:])
  for color in range(6):self.assertIn(dict(op='assert_mana',player=0,color=color,value=0),s)
 def test_conditional_reanimation_uses_real_grave_targets_and_spent_green(self):
  s=self.extension_cases('Vigor Mortis',['green_payment_0'])[0]['steps']
  self.assertIn(dict(op='choose_dialog',player=0,value=1),s)
  self.assertIn(dict(op='choose_zone_card',player=0,value=self.rows['Elvish Warrior']['csvid']),s)
  self.assertIn(dict(op='assert_card',ref='returned_0',property='plus1_counters',value=1),s)
  self.assertIn(dict(op='assert_card',ref='returned_0',property='power',value=3),s)
  for color,n in enumerate([1,2,0,1,0,0]):self.assertIn(dict(op='set_mana',player=0,color=color,value=n),s)
  for color in range(6):self.assertIn(dict(op='assert_mana',player=0,color=color,value=0),s)
  s=self.extension_cases('Vigor Mortis',['wrong_owner_1'])[0]['steps']
  self.assertIn(dict(op='can_cast',ref='source',value=0),s)
 def test_spell_mastery_is_counted_before_resolving_source_enters_grave(self):
  s=self.extension_cases('Necromantic Summons',['two_copies_1'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='returned_0',property='plus1_counters',value=0),s)
  self.assertIn(dict(op='assert_card',ref='returned_1',property='plus1_counters',value=2),s)
  s=self.extension_cases('Necromantic Summons',['condition_met_0'])[0]['steps']
  self.assertIn(dict(op='cast',ref='seed',value=1),s)
  self.assertIn(dict(op='assert_card',ref='returned_0',property='plus1_counters',value=2),s)
 def test_reanimation_checks_control_and_original_owner_on_death(self):
  c=self.extension_cases('Necromantic Summons',['opponent_graveyard_1'])[0];s=c['steps']
  self.assertIn(dict(op='choose_player',player=1,value=0),s)
  self.assertIn(dict(op='bind_permanent',player=1,value=self.rows['Elvish Warrior']['csvid'],ref='returned_0'),s)
  s=self.extension_cases('Vigor Mortis',['death_owner_0'])[0]['steps']
  self.assertIn(dict(op='cast',ref='murder',value=1),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Elvish Warrior']['csvid'],value=1),s)
  s=self.extension_cases('Necromantic Summons',['opponent_death_0'])[0]['steps']
  self.assertIn(dict(op='bind_permanent',player=0,value=self.rows['Elvish Warrior']['csvid'],ref='returned_0'),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Elvish Warrior']['csvid'],value=1),s)
 def test_green_reanimation_snapshot_keeps_both_human_decisions_and_payments(self):
  s=self.extension_cases('Vigor Mortis',['green_snapshot_0'])[0]['steps']
  self.assertEqual([a['player'] for a in s if a['op']=='choose_dialog'],[0,0])
  self.assertEqual([a['player'] for a in s if a['op']=='choose_zone_card'],[0,0])
  self.assertEqual(sum(a['op']=='cast' and a.get('ref')=='source' for a in s),2)
  for color in range(6):self.assertGreaterEqual(sum(a==dict(op='assert_mana',player=0,color=color,value=0) for a in s),2)
 def test_cannibalize_exiles_one_and_gives_counters_to_the_other(self):
  s=self.extension_cases('Cannibalize')[0]['steps']
  self.assertEqual([a['ref'] for a in s if a['op']=='choose_target'],['first','second'])
  self.assertIn(dict(op='assert_card',ref='first',property='in_play',value=0),s)
  self.assertIn(dict(op='assert_card',ref='second',property='plus1_counters',value=2),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=1),s)
  s=self.extension_cases('Cannibalize',['two_copies_1'])[0]['steps']
  self.assertIn(dict(op='choose_target',player=1,ref='third'),s)
  self.assertIn(dict(op='assert_card',ref='second',property='plus1_counters',value=4),s)
 def test_cannibalize_checks_same_controller_and_each_partial_target(self):
  c=self.extension_cases('Cannibalize',['wrong_predicate_0'])[0]
  self.assertEqual([(a['ref'],a['player']) for a in c['setup'] if a.get('zone','battlefield')=='battlefield' and 'Creature' in self.oracle[a['name']]['type_line']],[('first',0),('second',1)])
  self.assertIn(dict(op='can_cast',ref='source',value=0),c['steps'])
  s=self.extension_cases('Cannibalize',['lost_target_1'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='second',property='plus1_counters',value=2),s)
  s=self.extension_cases('Cannibalize',['lost_own_0'])[0]['steps']
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=1),s)
  c=self.extension_cases('Cannibalize',['own_only_1'])[0]
  self.assertFalse(any(a.get('zone','battlefield')=='battlefield' and a['player']==0 and 'Creature' in self.oracle[a['name']]['type_line'] for a in c['setup']))
  self.assertIn(dict(op='require_can_cast',ref='source',value=1),c['steps'])
  self.assertLess(next(i for i,a in enumerate(c['steps']) if a['op']=='require_can_cast'),next(i for i,a in enumerate(c['steps']) if a['op']=='choose_target'))
 def test_manifest_recipient_is_a_new_two_two_with_printed_counters(self):
  for name,n in (('Fierce Invocation',2),('Formless Nurturing',1),('Wildcall',3)):
   s=self.extension_cases(name)[0]['steps']
   self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=13252,value=1),s)
   for prop,value in (('plus1_counters',n),('power',2+n),('toughness',2+n)):
    self.assertIn(dict(op='assert_card',ref='manifested_0',property=prop,value=value),s)
   self.assertIn(dict(op='assert_card',ref='kept',property='plus1_counters',value=0),s)
   self.assertIn(dict(op='assert_zone',player=0,zone='library',value=6),s)
 def test_manifest_creature_top_uses_facedown_stats_and_empty_library_still_pays(self):
  c=self.extension_cases('Fierce Invocation',['creature_top_1'])[0]
  self.assertIn(dict(op='assert_card',ref='manifested_0',property='toughness',value=4),c['steps'])
  self.assertEqual(next(a['name'] for a in c['setup'] if a['ref']=='library_1_0'),'Elvish Warrior')
  s=self.extension_cases('Formless Nurturing',['empty_library_0'])[0]['steps']
  self.assertNotIn('bind_permanent_copy',[a['op'] for a in s])
  self.assertIn(dict(op='assert_zone',player=0,zone='battlefield',value=2),s)
  self.assertIn(dict(op='assert_mana',player=0,color=0,value=7),s)
 def test_variable_manifest_zero_and_five_pay_one_x_and_keep_recipient_counters(self):
  for variant,x in (('zero_1',0),('large_x_0',5)):
   s=self.extension_cases('Wildcall',[variant])[0]['steps'];owner=int(variant[-1])
   self.assertEqual([a['value'] for a in s if a['op']=='choose_x'],[x])
   self.assertIn(dict(op='assert_card',ref='manifested_0',property='plus1_counters',value=x),s)
   self.assertIn(dict(op='assert_mana',player=owner,color=0,value=10-x),s)
   self.assertIn(dict(op='assert_mana',player=owner,color=3,value=8),s)
  s=self.extension_cases('Wildcall',['pending_snapshot_1'])[0]['steps']
  self.assertEqual(sum(a['op']=='choose_x' for a in s),1)
  self.assertEqual(sum(a['op']=='cast' and a.get('ref')=='source' for a in s),1)
  self.assertEqual(s[-1]['op'],'assert_recorded_state')
 def test_manifest_characteristics_do_not_inherit_underlying_artifact_or_color(self):
  for name in ('Fierce Invocation','Formless Nurturing','Wildcall'):
   c=self.extension_cases(name,['artifact_top_1'])[0]
   self.assertEqual(next(a['name'] for a in c['setup'] if a['ref']=='library_1_0'),'Sol Ring')
   for prop in ('color','artifact','token'):
    self.assertIn(dict(op='assert_matching_permanents',player=1,index=13252,property=prop,value=0),c['steps'])
 def test_reanimation_response_gets_funded_after_source_pays_no_green(self):
  for name in ('Vigor Mortis','Necromantic Summons'):
   s=self.extension_cases(name,['lost_target_0'])[0]['steps']
   source=next(i for i,a in enumerate(s) if a['op']=='cast' and a['ref']=='source')
   refill=next(i for i,a in enumerate(s) if a==dict(op='set_mana',player=0,color=3,value=1))
   response=next(i for i,a in enumerate(s) if a['op']=='cast_response')
   self.assertLess(source,refill);self.assertLess(refill,response)
   self.assertIn(dict(op='assert_mana',player=0,color=3,value=0),s[source:refill])
   self.assertIn(dict(op='assert_mana',player=0,color=3,value=0),s[response:])

class GrowthLifecycleIntegratedTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.g = growth
        cls.o = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry()}

    def test_every_lifecycle_profile_expands_through_public_dispatcher(self):
        from recipes import expand as public_expand
        for name in self.g.LIFECYCLE_PINS['profiles']:
            d = self.declaration(name)
            p = self.g.spec(d, self.o[name])
            d['variants'] = self.g.variants(p)
            cases = public_expand(name, self.rows[name]['csvid'], d, self.o[name], self.rows, oracle_context=self.o)
            self.assertEqual(len(cases), len(d['variants']))
            self.assertTrue(all((c['_recipe']['family'] == 'growth_counter_spell' and 'draft_status' not in c['_recipe'] for c in cases)))

    def declaration(self, name, selected=('effect_0',)):
        return dict(family='growth_counter_spell', version=2, params=copy.deepcopy(self.g.LIFECYCLE_PINS['profiles'][name]['params']), oracle_text=self.o[name]['oracle_text'], variants=list(selected))

    def cases(self, name, selected=('effect_0',)):
        d = self.declaration(name, selected)
        p = self.g.spec(d, self.o[name])
        return self.g.expand(name, self.rows[name]['csvid'], d, p, list(selected), self.rows, self.o)

    def test_whole_source_identity_and_parameters_fail_closed(self):
        name = 'Feat of Resistance'
        d = self.declaration(name)
        for change in (dict(oracle_text='Put a counter on target creature.'), dict(mana_cost='{W}'), dict(layout='transform'), dict(card_faces=[{}]), dict(oracle_id='changed')):
            with self.assertRaises(ValueError):
                self.g.spec(d, dict(self.o[name], **change))
        for change in (dict(version=1), dict(params={}), dict(extra=True), dict(variants=[]), dict(variants=['effect_0', 'effect_0'])):
            with self.assertRaises(ValueError):
                self.g.spec(dict(d, **change), self.o[name])
        params = copy.deepcopy(d['params'])
        params['protection_color'] = True
        with self.assertRaises(ValueError):
            self.g.spec(dict(d, params=params), self.o[name])

    def test_full_witness_pins_include_spirit_type_cost_and_stats(self):
        self.g.validate_witnesses(self.o, self.rows)
        for name, key, value in (('Kami of Ancient Law', 'type_line', 'Creature — Human'), ('Murder', 'mana_cost', '{B}'), ('Hill Giant', 'power', '4'), ('Arbor Armament', 'oracle_text', 'changed')):
            oracle = copy.deepcopy(self.o)
            oracle[name][key] = value
            with self.assertRaises(ValueError):
                self.g.validate_witnesses(oracle, self.rows)

    def test_every_declared_profile_expands_without_unknown_or_duplicate_names(self):
        names = []
        count = 0
        for name in self.g.LIFECYCLE_PINS['profiles']:
            d = self.declaration(name)
            p = self.g.spec(d, self.o[name])
            selected = self.g.variants(p)
            d['variants'] = selected
            for c in self.g.expand(name, self.rows[name]['csvid'], d, p, selected, self.rows, self.o):
                names.append(c['name'])
                count += 1
                self.assertEqual(c['_recipe']['family'], 'growth_counter_spell')
                self.assertFalse(any((a['op'] in ('add_counters', 'set_counters', 'reanimate', 'put_into_play', 'set_card_state') for a in c['steps'])))
                self.assertTrue(all((a['value'] >= 0 for a in c['steps'] if a['op'] == 'assert_mana')))
        self.assertEqual(count, 140)
        self.assertEqual(len(names), len(set(names)))

    def test_emitted_ref_dependencies_survive_both_restore_tails(self):
        ref_ops = {'choose_target', 'require_card', 'cast', 'cast_response', 'assert_card', 'attack', 'block', 'can_cast', 'require_can_cast', 'can_target_creature'}
        for name in self.g.LIFECYCLE_PINS['profiles']:
            d = self.declaration(name)
            p = self.g.spec(d, self.o[name])
            d['variants'] = self.g.variants(p)
            for c in self.g.expand(name, self.rows[name]['csvid'], d, p, d['variants'], self.rows, self.o):
                refs = {a['ref'] for a in c['setup']}
                self.assertEqual(len(refs), len(c['setup']))
                saved = None
                for a in c['steps']:
                    if a['op'] == 'capture':
                        saved = set(refs)
                    elif a['op'] == 'restore':
                        self.assertIsNotNone(saved)
                        refs = set(saved)
                    elif a['op'] == 'bind_permanent':
                        refs.add(a['ref'])
                    elif a['op'] in ref_ops:
                        self.assertIn(a['ref'], refs, (c['name'], a))
                        if 'other' in a:
                            self.assertIn(a['other'], refs, (c['name'], a))

    def test_blink_is_new_object_with_spirit_counter_and_unselected_controls(self):
        c = self.cases('Essence Flux')[0]
        s = c['steps']
        self.assertEqual(next((a['state'] for a in c['setup'] if a['ref'] == 'first')), ['tapped'])
        for prop, n in (('plus1_counters', 1), ('power', 3), ('toughness', 3), ('tapped', 0), ('sick', 1)):
            self.assertIn(dict(op='assert_card', ref='returned_1', property=prop, value=n), s)
        self.assertIn(dict(op='assert_card', ref='kept', property='plus1_counters', value=0), s)
        self.assertNotIn('returned_1', [a['ref'] for a in c['setup']])

    def test_non_spirit_blink_resets_real_old_counter_to_zero(self):
        s = self.cases('Essence Flux', ['non_spirit_preexisting_1'])[0]['steps']
        self.assertIn(dict(op='cast', ref='seed', value=1), s)
        self.assertIn(dict(op='assert_card', ref='first', property='plus1_counters', value=1), s)
        self.assertIn(dict(op='assert_card', ref='returned_1', property='plus1_counters', value=0), s)
        self.assertIn(dict(op='assert_card', ref='returned_1', property='toughness', value=2), s)

    def test_repeat_blink_clears_real_pump_instead_of_noop_second_cast(self):
        s = self.cases('Essence Flux', ['repeat_0'])[0]['steps']
        pump = next((i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'pump'))
        second = next((i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'source_second'))
        self.assertLess(pump, second)
        self.assertIn(dict(op='assert_card', ref='returned_1', property='power', value=6), s[pump:second])
        self.assertIn(dict(op='assert_card', ref='returned_2', property='power', value=3), s[second:])

    def test_undying_requires_real_death_and_returns_one_counter(self):
        s = self.cases('Undying Evil')[0]['steps']
        death = next((i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'murder'))
        bound = next((i for i, a in enumerate(s) if a['op'] == 'bind_permanent'))
        self.assertLess(death, bound)
        self.assertIn(dict(op='assert_card', ref='returned_1', property='plus1_counters', value=1), s[bound:])
        self.assertIn(dict(op='assert_card', ref='returned_1', property='in_play', value=1), s[bound:])

    def test_undying_preexisting_counter_and_cleanup_prevent_return(self):
        for variant in ('preexisting_0', 'cleanup_1'):
            s = self.cases('Undying Evil', [variant])[0]['steps']
            self.assertNotIn('bind_permanent', [a['op'] for a in s])
            self.assertIn(dict(op='assert_zone_multiplicity', player=int(variant[-1]), zone='graveyard', index=self.rows['Grizzly Bears']['csvid'], value=1), s)
        s = self.cases('Undying Evil', ['cleanup_0'])[0]['steps']
        self.assertLess(next((i for i, a in enumerate(s) if a['op'] == 'cleanup')), next((i for i, a in enumerate(s) if a['op'] == 'cast' and a['ref'] == 'murder')))

    def test_undying_returned_counter_persists_through_later_cleanup(self):
        s = self.cases('Undying Evil', ['returned_cleanup_1'])[0]['steps']
        cut = next((i for i, a in enumerate(s) if a['op'] == 'cleanup'))
        self.assertIn(dict(op='bind_permanent', player=1, value=self.rows['Grizzly Bears']['csvid'], ref='returned_1'), s[:cut])
        for prop, n in (('plus1_counters', 1), ('power', 3), ('toughness', 3)):
            self.assertIn(dict(op='assert_card', ref='returned_1', property=prop, value=n), s[cut:])

    def test_undying_opposing_target_returns_to_its_owner_not_caster(self):
        s = self.cases('Undying Evil', ['opponent_0'])[0]['steps']
        self.assertIn(dict(op='bind_permanent', player=1, value=self.rows['Grizzly Bears']['csvid'], ref='returned_1'), s)
        self.assertIn(dict(op='assert_mana', player=0, color=1, value=0), s)

    def test_foreign_owner_fixture_returns_to_owner_in_all_three_lifecycles(self):
        for name, target in (('Essence Flux', 'Kami of Ancient Law'), ('Undying Evil', 'Grizzly Bears'), ('Long Road Home', 'Grizzly Bears')):
            for owner in (0, 1):
                c = self.cases(name, ['foreign_owner_' + str(owner)])[0]
                first = next((a for a in c['setup'] if a['ref'] == 'first'))
                self.assertEqual(first['player'], owner)
                self.assertIn('opponent_owned', first['state'])
                self.assertIn(dict(op='bind_permanent', player=1 - owner, value=self.rows[target]['csvid'], ref='returned_1'), c['steps'])

    def test_bounce_instead_of_death_never_synthesizes_an_undying_return(self):
        s = self.cases('Undying Evil', ['bounce_instead_1'])[0]['steps']
        self.assertIn(dict(op='cast', ref='response', value=1), s)
        self.assertNotIn('bind_permanent', [a['op'] for a in s])
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='hand', index=self.rows['Grizzly Bears']['csvid'], value=1), s)

    def test_delayed_return_checks_actual_exile_before_endturn_and_binds_after(self):
        c = self.cases('Long Road Home', ['opponent_0'])[0]
        s = c['steps']
        boundary = next((i for i, a in enumerate(s) if a['op'] == 'cleanup'))
        bound = next((i for i, a in enumerate(s) if a['op'] == 'bind_permanent'))
        self.assertLess(boundary, bound)
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='exile', index=self.rows['Grizzly Bears']['csvid'], value=1), s[:boundary])
        self.assertIn(dict(op='assert_card', ref='returned_1', property='plus1_counters', value=1), s[bound:])
        self.assertTrue(any(('cannot separately prove' in gap for gap in c['_recipe']['untested_components'])))

    def test_protection_uses_real_color_targeting_and_evasion_combat(self):
        s = self.cases('Feat of Resistance')[0]['steps']
        for a in (dict(op='choose_color', player=0, value=4), dict(op='can_target_creature', ref='red_probe', other='first', value=0), dict(op='can_target_creature', ref='black_probe', other='first', value=1), dict(op='attack', ref='first', value=1), dict(op='block', ref='enemy', other='first', value=0), dict(op='assert_life', player=1, value=17)):
            self.assertIn(a, s)
        self.assertIn('combat_damage', [a['op'] for a in s])

    def test_protection_reverse_combat_prevents_damage_and_cleanup_retains_counter(self):
        s = self.cases('Feat of Resistance', ['opposing_turn_1'])[0]['steps']
        self.assertIn(dict(op='block', ref='first', other='enemy', value=1), s)
        self.assertIn(dict(op='assert_card', ref='first', property='damage', value=0), s)
        s = self.cases('Feat of Resistance', ['cleanup_0'])[0]['steps']
        cut = next((i for i, a in enumerate(s) if a['op'] == 'cleanup'))
        self.assertIn(dict(op='assert_card', ref='first', property='plus1_counters', value=1), s[cut:])
        self.assertIn(dict(op='can_target_creature', ref='red_probe', other='first', value=1), s[cut:])
        self.assertIn(dict(op='block', ref='enemy', other='first', value=1), s[cut:])
        self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='graveyard', index=self.rows['Grizzly Bears']['csvid'], value=1), s[cut:])

    def test_controller_and_type_denials_are_actual_card_queries(self):
        for name in ('Essence Flux', 'Feat of Resistance'):
            c = self.cases(name, ['wrong_controller_1'])[0]
            self.assertFalse(any((a['player'] == 1 and a.get('zone') == 'battlefield' and ('Creature' in self.o[a['name']]['type_line']) for a in c['setup'])))
            self.assertIn(dict(op='can_cast', ref='source', value=0), c['steps'])
        for name in self.g.LIFECYCLE_PINS['profiles']:
            self.assertIn(dict(op='can_cast', ref='source', value=0), self.cases(name, ['wrong_type_0'])[0]['steps'])

    def test_both_snapshot_tails_keep_source_target_and_payment_owner(self):
        for name in self.g.LIFECYCLE_PINS['profiles']:
            for variant, n in (('ready_snapshot_1', 2), ('pending_snapshot_1', 1)):
                s = self.cases(name, [variant])[0]['steps']
                self.assertEqual(sum((a['op'] == 'cast' and a.get('ref') == 'source' for a in s)), n)
                self.assertEqual(sum((a['op'] == 'choose_target' and a.get('player') == 1 and (a.get('ref') == 'first') for a in s)), n)
                for color in range(6):
                    self.assertGreaterEqual(sum((a == dict(op='assert_mana', player=1, color=color, value=0) for a in s)), 2)
                if name == 'Feat of Resistance':
                    self.assertEqual([a['player'] for a in s if a['op'] == 'choose_color'], [1, 1])
                self.assertEqual(s[-1]['op'], 'assert_recorded_state')

class GrowthCombatIntegratedTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.g=growth;cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name,selected=('effect_0',)):
  return dict(family='growth_counter_spell',version=2,params=copy.deepcopy(self.g.COMBAT_PINS[name]['params']),oracle_text=self.o[name]['oracle_text'],variants=list(selected))
 def cases(self,name,selected=('effect_0',)):
  from recipes import expand as public_expand
  d=self.declaration(name,selected);return public_expand(name,self.rows[name]['csvid'],d,self.o[name],self.rows,oracle_context=self.o)
 def test_public_all_combat_profiles(self):
  for name in self.g.COMBAT_PINS:
   d=self.declaration(name);p=self.g.spec(d,self.o[name]);d['variants']=self.g.variants(p)
   self.assertEqual(len(self.cases(name,d['variants'])),len(d['variants']))
 def test_complete_pins_and_no_extra_parameters(self):
  for name in self.g.COMBAT_PINS:
   d=self.declaration(name)
   for changed in (dict(self.o[name],mana_cost='{0}'),dict(self.o[name],oracle_id='bad'),dict(self.o[name],oracle_text='')):
    with self.assertRaises(ValueError):self.g.spec(d,changed)
   with self.assertRaises(ValueError):self.g.spec(dict(d,params=dict(d['params'],extra=1)),self.o[name])
 def test_converge_exact_four_color_payment_and_positive_counter(self):
  s=self.cases('Infuse with the Elements',['four_colors_1'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=4),s)
  self.assertEqual([a['value'] for a in s if a['op']=='set_mana' and a['player']==1][:6],[0,1,1,1,1,0])
  self.assertTrue(any(a['op']=='assert_life' and a['player']==0 and a['value']<20 for a in s))
  for variant,colors in [('green_only_0',[3,3,3]),('two_colors_0',[3,4,4]),('three_colors_0',[2,3,4]),('four_colors_0',[1,2,4])]:
   s=self.cases('Infuse with the Elements',[variant])[0]['steps']
   self.assertEqual([a['color'] for a in s if a['op']=='choose_mana'],colors)
 def test_trample_expiry_does_not_erase_counters(self):
  s=self.cases('Infuse with the Elements',['cleanup_0'])[0]['steps'];cut=next(i for i,a in enumerate(s) if a['op']=='cleanup')
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=1),s[cut:])
  self.assertIn(dict(op='assert_life',player=1,value=20),s[cut:])
 def test_sun_hand_count_and_second_live_bolster_recipient(self):
  s=self.cases("Sunbringer's Touch",['two_copies_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='second',property='plus1_counters',value=4),s)
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=3),s)
  s=self.cases("Sunbringer's Touch",['empty_hand_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='second',property='plus1_counters',value=0),s)
 def test_formation_main_phase_and_offturn_distinct(self):
  for v,amount in [('effect_0',1),('combat_0',0),('opposing_turn_0',0)]:
   s=self.cases('Unbreakable Formation',[v])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=amount),s)
   self.assertIn(dict(op='cast',ref='murder_first',value=1),s)
   self.assertIn(dict(op='assert_card',ref='first',property='in_play',value=1),s)
 def test_testfaith_real_damage_and_finite_shield(self):
  s=self.cases('Test of Faith')[0]['steps']
  for ref in ('bolt','shock','control_bolt'):self.assertIn(dict(op='cast',ref=ref,value=1),s)
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=3),s)
  self.assertIn(dict(op='assert_card',ref='first',property='damage',value=2),s)
  self.assertIn(dict(op='assert_card',ref='unselected',property='damage',value=3),s)
 def test_testfaith_simultaneous_combat_assignment(self):
  s=self.cases('Test of Faith',['combat_1'])[0]['steps']
  self.assertIn(dict(op='combat_damage'),s)
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=3),s)
  self.assertIn(dict(op='assert_card',ref='enemy',property='damage',value=2),s)
 def test_fog_mixed_counter_predicate_and_noncombat_control(self):
  s=self.cases('Hindervines')[0]['steps']
  self.assertIn(dict(op='assert_card',ref='block_b',property='damage',value=4),s)
  self.assertIn(dict(op='assert_card',ref='attack_b',property='damage',value=0),s)
  for name in ('Hindervines','Encircling Fissure'):
   s=self.cases(name,['noncombat_1'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='block_b',property='damage',value=3),s)
 def test_opponent_fog_both_directions_and_normal_awaken_gap(self):
  c=self.cases('Encircling Fissure',['reverse_combat_1'])[0]
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Hill Giant']['csvid'],value=0),c['steps'])
  self.assertTrue(any('Awaken' in gap for gap in c['_recipe']['untested_components']))
  self.assertFalse(any(a['op']=='choose_player' for a in c['steps']))
 def test_owner_one_snapshot_payment_and_target_binding(self):
  for name in self.g.COMBAT_PINS:
   for variant in ('ready_snapshot_1','pending_snapshot_1'):
    s=self.cases(name,[variant])[0]['steps'];self.assertEqual(s[-1]['op'],'assert_recorded_state')
    for i in range(6):self.assertGreaterEqual(s.count(dict(op='assert_mana',player=1,color=i,value=0)),2)
    if name in ('Infuse with the Elements','Test of Faith'):
     source_casts=[i for i,a in enumerate(s) if a==dict(op='cast',ref='source',value=1)]
     self.assertEqual(len(source_casts),2 if variant.startswith('ready') else 1)
     self.assertTrue(all(s[i-1]==dict(op='choose_target',player=1,ref='first') for i in source_casts))

class GrowthActionIntegratedTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.g=growth;cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name,selected=None):
  return dict(family='growth_counter_spell',version=2,params=copy.deepcopy(self.g.ACTION_PINS[name]['params']),oracle_text=self.o[name]['oracle_text'],variants=list(selected or ['next_step_0' if name=='Rush of Ice' else 'effect_0']))
 def cases(self,name,selected=None):
  from recipes import expand as public_expand
  d=self.declaration(name,selected);return public_expand(name,self.rows[name]['csvid'],d,self.o[name],self.rows,oracle_context=self.o)
 def test_public_all_action_profiles(self):
  for name in self.g.ACTION_PINS:
   d=self.declaration(name);d['variants']=self.g.variants(self.g.spec(d,self.o[name]));self.assertEqual(len(self.cases(name,d['variants'])),len(d['variants']))
 def test_complete_source_and_exact_parameters(self):
  for name in self.g.ACTION_PINS:
   d=self.declaration(name)
   for record in (dict(self.o[name],oracle_text=''),dict(self.o[name],mana_cost='{0}'),dict(self.o[name],oracle_id='changed'),dict(self.o[name],card_faces=[{}])):
    with self.assertRaises(ValueError):self.g.spec(d,record)
   with self.assertRaises(ValueError):self.g.spec(dict(d,params=dict(d['params'],unexpected=True)),self.o[name])
 def test_transfer_conserves_counters_and_uses_real_seeds(self):
  s=self.cases('Bioshift')[0]['steps']
  self.assertEqual(sum(a['op']=='cast' and a['ref'].startswith('seed_') for a in s),4)
  for ref,n in [('first',0),('second',4)]:self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=n),s)
  self.assertIn(dict(op='choose_number',player=0,value=3),s)
 def test_transfer_partial_and_blue_cost(self):
  s=self.cases('Bioshift',['partial_amount_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=1),s)
  self.assertIn(dict(op='assert_card',ref='second',property='plus1_counters',value=3),s)
  s=self.cases('Bioshift',['blue_payment_1'])[0]['steps']
  self.assertIn(dict(op='set_mana',player=1,color=2,value=1),s)
  self.assertFalse(any(a['op']=='choose_number' for a in s))
 def test_transfer_legality_board_has_no_same_controller_pair(self):
  for owner in (0,1):
   c=self.cases('Bioshift',['wrong_controller_'+str(owner)])[0]
   creatures=[a for a in c['setup'] if a['zone']=='battlefield' and 'Creature' in self.o[a['name']]['type_line']]
   self.assertEqual([a['player'] for a in creatures],[owner,1-owner]);self.assertIn(dict(op='can_cast',ref='source',value=0),c['steps'])
 def test_thrive_targets_determine_x_and_partial_retains_assigned_one(self):
  s=self.cases('Thrive',['three_targets_1'])[0]['steps']
  self.assertFalse(any(a['op']=='choose_x' for a in s))
  for ref in ('first','second','third'):self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=1),s)
  s=self.cases('Thrive',['partial_target_0'])[0]['steps'];self.assertIn(dict(op='destroy',ref='second'),s);self.assertIn(dict(op='assert_card',ref='first',property='plus1_counters',value=1),s)
 def test_multikicker_exact_cost_and_separate_target(self):
  s=self.cases('Strength of the Tajuru',['kicked_1'])[0]['steps']
  for a in (dict(op='choose_x',player=1,value=3),dict(op='choose_dialog',player=1,value=0),dict(op='choose_target',player=1,ref='second'),dict(op='set_mana',player=1,color=0,value=4),dict(op='set_mana',player=1,color=3,value=2),dict(op='assert_card',ref='second',property='plus1_counters',value=3)):self.assertIn(a,s)
 def test_animation_changes_body_and_preserves_artifact_traits(self):
  s=self.cases('Lifecraft Awakening')[0]['steps']
  for prop,n in [('power',3),('toughness',3),('plus1_counters',3)]:self.assertIn(dict(op='assert_card',ref='first',property=prop,value=n),s)
  for prop,n in [('artifact',1),('token',0),('subtype_Construct',1)]:self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows['Darksteel Relic']['csvid'],property=prop,value=n),s)
  self.assertIn(dict(op='assert_life',player=1,value=17),s)
 def test_artifact_creature_base_and_zero_toughness_death(self):
  s=self.cases('Lifecraft Awakening',['artifact_creature_1'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='first',property='power',value=8),s)
  s=self.cases('Lifecraft Awakening',['x_zero_0'])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Darksteel Relic']['csvid'],value=1),s)
 def test_rush_real_untap_order_skip_and_expiry(self):
  for variant in ('next_step_0','target_self_1','locked_snapshot_1'):
   c=self.cases('Rush of Ice',[variant])[0];s=c['steps']
   self.assertFalse(any(a['zone']=='battlefield' and 'Land' in self.o[a['name']]['type_line'] for a in c['setup']))
   self.assertTrue(all(i>0 and s[i-1]['op']=='advance_turn' and set(a)=={'op','player'} for i,a in enumerate(s) if a['op']=='untap_step'))
   self.assertIn(dict(op='assert_card',ref='first',property='tapped',value=1),s)
   self.assertIn(dict(op='assert_card',ref='first',property='tapped',value=0),s[-100:])
 def test_temporary_control_real_haste_return_and_owner_death(self):
  for name in ('Mark of Mutiny','Press into Service'):
   s=self.cases(name,['foreign_owner_1'])[0]['steps']
   for a in (dict(op='bind_permanent',player=1,value=self.rows['Hill Giant']['csvid'],ref='stolen'),dict(op='attack',ref='stolen',value=1),dict(op='bind_permanent',player=0,value=self.rows['Hill Giant']['csvid'],ref='returned')):self.assertIn(a,s)
   s=self.cases(name,['death_before_return_1'])[0]['steps'];self.assertIn(dict(op='cast',ref='murder',value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Hill Giant']['csvid'],value=1),s)
 def test_support_pending_targets_and_replay_owner(self):
  s=self.cases('Press into Service',['selection_pending_1'])[0]['steps']
  self.assertEqual([a['ref'] for a in s if a['op']=='choose_target'],['first','second','third'])
  self.assertNotIn('resolve',[a['op'] for a in s]);self.assertIn(dict(op='assert_choices_consumed'),s)
  for name in self.g.ACTION_PINS:
   for variant,n in [('ready_snapshot_1',2),('pending_snapshot_1',1)]:
    s=self.cases(name,[variant])[0]['steps'];self.assertEqual(sum(a==dict(op='cast',ref='source',value=1) for a in s),n)
    self.assertEqual(s[-1]['op'],'assert_recorded_state')
    for color in range(6):self.assertGreaterEqual(s.count(dict(op='assert_mana',player=1,color=color,value=0)),2)

class GrowthFinalFollowupTest(unittest.TestCase):
    setUpClass = classmethod(GrowthActionIntegratedTest.setUpClass.__func__)
    declaration = GrowthActionIntegratedTest.declaration
    cases = GrowthActionIntegratedTest.cases

    def test_spoils_union_forced_allocation_and_exact_cost(self):
        c = self.cases('Spoils of War', ['effect_1'])[0]
        s = c['steps']
        eligible = [a for a in c['setup'] if a['player'] == 0 and a['zone'] == 'graveyard' and ('Creature' in self.o[a['name']]['type_line'] or 'Artifact' in self.o[a['name']]['type_line'])]
        self.assertEqual(len(eligible), 3)
        self.assertEqual([a['name'] for a in eligible].count('Phyrexian Hulk'), 1)
        for a in (dict(op='set_mana', player=1, color=0, value=3), dict(op='set_mana', player=1, color=1, value=1), dict(op='assert_card', ref='first', property='plus1_counters', value=3)):
            self.assertIn(a, s)
        self.assertFalse(any((a['op'] == 'choose_x' for a in s)))
        cast = next((i for i, a in enumerate(s) if a == dict(op='cast', ref='source', value=1)))
        dialog = next((i for i, a in enumerate(s) if a['op'] == 'choose_dialog'))
        self.assertLess(cast, dialog)

    def test_spoils_cast_time_x_survives_grave_changing_response(self):
        s = self.cases('Spoils of War', ['grave_response_0'])[0]['steps']
        for a in (dict(op='cast_response', ref='response', value=1), dict(op='resolve_response'), dict(op='assert_card', ref='first', property='plus1_counters', value=3), dict(op='assert_zone_multiplicity', player=1, zone='graveyard', index=self.rows['Hill Giant']['csvid'], value=1)):
            self.assertIn(a, s)

    def test_attack_trigger_lifelink_uses_actual_combat_and_one_gain(self):
        for variant, life in [('effect_0', 22), ('unblocked_0', 29), ('two_copies_0', 22), ('countered_0', 20), ('cleanup_0', 20)]:
            s = self.cases('Ondu Rising', [variant])[0]['steps']
            self.assertIn(dict(op='assert_life', player=0, value=life), s)
            self.assertIn(dict(op='finish_attackers'), s)
            self.assertIn(dict(op='combat_damage'), s)
            finish = next((i for i, a in enumerate(s) if a['op'] == 'finish_attackers'))
            blockers = next((i for i, a in enumerate(s) if a['op'] == 'begin_blockers'))
            self.assertIn(dict(op='resolve'), s[finish:blockers])

    def test_lifelink_noncombat_and_nonattacking_blocker_controls(self):
        s = self.cases('Ondu Rising', ['noncombat_1'])[0]['steps']
        self.assertIn(dict(op='cast', ref='shock', value=1), s)
        self.assertNotIn('combat_damage', [a['op'] for a in s])
        self.assertIn(dict(op='assert_life', player=1, value=20), s)
        s = self.cases('Ondu Rising', ['effect_1'])[0]['steps']
        self.assertIn(dict(op='assert_life', player=0, value=20), s)

    def test_next_creature_real_cast_entry_consumption_and_intervening_spell(self):
        s = self.cases('Savage Summoning', ['intervening_1'])[0]['steps']
        for ref, n in [('entered_first', 1), ('entered_second', 0)]:
            self.assertIn(dict(op='assert_card', ref=ref, property='plus1_counters', value=n), s)
        self.assertIn(dict(op='cast', ref='creature_first', value=1), s)
        self.assertIn(dict(op='cast', ref='creature_second', value=1), s)
        shock = next((i for i, a in enumerate(s) if a == dict(op='cast', ref='shock', value=1)))
        creature = next((i for i, a in enumerate(s) if a == dict(op='cast', ref='creature_first', value=1)))
        self.assertLess(shock, creature)
        self.assertFalse(any((a['op'] == 'require_can_cast' and a.get('ref', '').startswith('creature') for a in s)))

    def test_next_creature_expiry_and_permanent_counter_cleanup(self):
        s = self.cases('Savage Summoning', ['expired_0'])[0]['steps']
        cut = next((i for i, a in enumerate(s) if a['op'] == 'cleanup'))
        cast = next((i for i, a in enumerate(s) if a == dict(op='cast', ref='creature_first', value=1)))
        self.assertLess(cut, cast)
        self.assertIn(dict(op='assert_card', ref='entered_first', property='plus1_counters', value=0), s)
        s = self.cases('Savage Summoning', ['cleanup_1'])[0]['steps']
        cut = next((i for i, a in enumerate(s) if a['op'] == 'cleanup'))
        self.assertIn(dict(op='assert_card', ref='entered_first', property='plus1_counters', value=1), s[cut:])

    def test_source_resists_counter_and_protected_creature_targeting_ends_pending(self):
        s = self.cases('Savage Summoning', ['countered_1'])[0]['steps']
        self.assertIn(dict(op='cast_response', ref='counter', value=1), s)
        self.assertIn(dict(op='resolve_response'), s)
        self.assertIn(dict(op='assert_card', ref='entered_first', property='plus1_counters', value=1), s)
        s = self.cases('Savage Summoning', ['creature_counter_pending_0'])[0]['steps']
        response = next((i for i, a in enumerate(s) if a['op'] == 'cast_response'))
        self.assertNotIn('resolve_response', [a['op'] for a in s[response:]])
        self.assertIn(dict(op='assert_stack', value=2), s[response:])

    def test_thrive_zero_target_is_terminal_legal_permission_probe(self):
        c = self.cases('Thrive', ['zero_targets_1'])[0]
        s = c['steps']
        self.assertFalse(any((a['zone'] == 'battlefield' and 'Creature' in self.o[a['name']]['type_line'] for a in c['setup'])))
        self.assertIn(dict(op='can_cast', ref='source', value=1), s)
        self.assertNotIn('cast', [a['op'] for a in s])
        self.assertFalse(any((a['op'].startswith('choose_') for a in s)))
        self.assertIn(dict(op='assert_mana', player=1, color=3, value=1), s)

    def test_vehicle_functionally_remains_noncreature(self):
        s = self.cases('Lifecraft Awakening', ['vehicle_1'])[0]['steps']
        self.assertIn(dict(op='begin_attackers'), s)
        self.assertIn(dict(op='attack', ref='first', value=0), s)
        self.assertIn(dict(op='assert_matching_permanents', player=1, index=self.rows['Sky Skiff']['csvid'], property='subtype_Construct', value=0), s)
        self.assertIn(dict(op='assert_card', ref='first', property='plus1_counters', value=3), s)

if __name__=='__main__':unittest.main()
