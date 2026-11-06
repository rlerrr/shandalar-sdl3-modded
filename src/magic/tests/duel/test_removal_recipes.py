import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from removal_recipes import eligible,variants,PINS,TARGETS


class RemovalRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

 def declaration(self,name='Final Reward',selected=None):
  configs={
   'Final Reward':dict(effect='exile'),
   'Vindicate':dict(scope='permanent'),
   'Annihilate':dict(scope='nonblack_creature',no_regenerate=True,draw=1),
   'Last Breath':dict(effect='exile',limit=dict(field='power',op='le',value=2),target_gain_life=4),
   'Pillar of Light':dict(effect='exile',limit=dict(field='toughness',op='ge',value=4)),
   'Cast Down':dict(scope='nonlegendary_creature'),
   'Human Frailty':dict(scope='human_creature'),
   'Urgent Exorcism':dict(scope='spirit_or_enchantment'),
   'Divine Verdict':dict(state='attacking_or_blocking'),
   'Excoriate':dict(effect='exile',state='tapped'),
   'Liturgy of Blood':dict(add_mana='{B}{B}{B}'),
   'Deconstruct':dict(scope='artifact',add_mana='{G}{G}{G}'),
   'Rend Flesh':dict(scope='nonspirit_creature'),
  }
  p=dict(effect='destroy',scope='creature',state='any',controller='any',limit=None,no_regenerate=False,draw=0,gain_life=0,target_gain_life=0,target_lose_life=0,target_damage=0,add_mana='');p.update(configs[name])
  return dict(family='target_removal',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['effect_0'])

 def cases(self,name='Final Reward',selected=None,d=None,oracle=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,oracle_context=oracle or self.oracle)

 def test_parameters_and_whole_effect_pin_fail_closed(self):
  d=self.declaration();d['params']['scope']='artifact'
  with self.assertRaises(ValueError):self.cases(d=d)
  for patch in (dict(oracle_text='Destroy target creature.'),dict(params={}),dict(variants=[]),dict(variants=['unknown_0'])):
   with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),**patch))
  for patch in (dict(draw=True),dict(target_gain_life=-1),dict(no_regenerate=1),dict(add_mana='{X}'),dict(limit=dict(field='power',op='le',value=True))):
   d=self.declaration();d['params'].update(patch)
   with self.assertRaises(ValueError):self.cases(d=d)

 def test_type_intersections_and_exclusions_are_independent(self):
  p=self.declaration()['params']
  for scope,expected in [('artifact_creature',{'Ornithopter','Phyrexian Hulk'}),('nonartifact_creature',{'Grizzly Bears','Nyxborn Shieldmate'}),('artifact_or_enchantment',{'Ornithopter','Nyxborn Shieldmate','Seal of Cleansing'}),('creature_or_planeswalker',{'Grizzly Bears','Jace Beleren'})]:
   for n in expected:self.assertTrue(eligible(PINS[n],dict(p,scope=scope)),(scope,n))
  self.assertFalse(eligible(PINS['Forest'],dict(p,scope='nonland_permanent')))
  self.assertTrue(eligible(PINS['Darksteel Citadel'],dict(p,scope='artifact')))
  self.assertFalse(eligible(PINS['Ornithopter'],dict(p,scope='nonartifact_creature')))
  for n in ('Fervor',"Executioner's Capsule"):
   self.assertTrue(eligible(PINS[n],dict(p,scope='black_or_red_permanent')))
  self.assertTrue(eligible(PINS['Ajani Goldmane'],dict(p,scope='white_permanent')))

 def test_changeling_counts_as_human_and_spirit_but_not_equipment(self):
  p=self.declaration()['params']
  for nm in ('Mothdust Changeling','Avian Changeling'):
   self.assertTrue(eligible(PINS[nm],dict(p,scope='human_creature')))
   self.assertTrue(eligible(PINS[nm],dict(p,scope='spirit_or_enchantment')))
   self.assertFalse(eligible(PINS[nm],dict(p,scope='nonspirit_creature')))
   self.assertFalse(eligible(PINS[nm],dict(p,scope='equipment')))

 def test_power_and_toughness_boundaries_use_printed_characteristics(self):
  power=self.declaration('Last Breath')['params'];tough=self.declaration('Pillar of Light')['params']
  self.assertFalse(eligible(PINS['Wasp Lancer'],power));self.assertTrue(eligible(PINS['Ornithopter'],power))
  self.assertTrue(eligible(PINS['Craw Wurm'],tough));self.assertFalse(eligible(PINS['Grizzly Bears'],tough))

 def test_legendary_and_monocolored_are_distinct_from_color_identity(self):
  p=self.declaration('Cast Down')['params']
  self.assertFalse(eligible(PINS['Isamaru, Hound of Konda'],p));self.assertTrue(eligible(PINS['Savannah Lions'],p))
  p=dict(p,scope='monocolored_creature')
  self.assertFalse(eligible(PINS['Ornithopter'],p));self.assertFalse(eligible(PINS['Watchwolf'],p));self.assertTrue(eligible(PINS['Scathe Zombies'],p))

 def test_one_candidate_negative_scope_queries_do_not_add_another_legal_target(self):
  i=TARGETS.index('Sol Ring');c=self.cases(selected=[f'card_{i}_0'])[0]
  battle=[x for x in c['setup'] if x['zone']=='battlefield'];self.assertEqual([x['name'] for x in battle],['Sol Ring'])
  self.assertIn(dict(op='can_cast',ref='source',value=0),c['steps'])
  self.assertFalse(any(x['op']=='cast' and x['ref']=='source' for x in c['steps']))

 def test_positive_cast_uses_real_cost_and_exile_receipt(self):
  s=self.cases(selected=['payment_1'])[0]['steps']
  self.assertIn(dict(op='require_can_cast',ref='source',value=1),s)
  self.assertIn(dict(op='cast',ref='source',value=1),s)
  self.assertIn(dict(op='assert_mana',player=1,color=1,value=0),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Savannah Lions']['csvid'],value=1),s)

 def test_indestructible_is_targetable_but_survives_destruction(self):
  i=TARGETS.index('Darksteel Relic');c=self.cases('Vindicate',[f'card_{i}_0'])[0]
  self.assertIn(dict(op='require_can_cast',ref='source',value=1),c['steps'])
  self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Darksteel Relic']['csvid'],value=0),c['steps'])

 def test_combat_restrictions_use_real_declarations_and_recheck_tapping(self):
  s=self.cases('Divine Verdict',['blocking_target_0'])[0]['steps']
  for op in ('begin_attackers','finish_attackers','begin_blockers','block','advance_combat'):self.assertTrue(any(x['op']==op for x in s))
  s=self.cases('Excoriate',['state_changed_pending_0'])[0]['steps']
  self.assertIn(dict(op='choose_dialog',player=0,value=1),s)
  self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),s)

 def test_fizzled_cantrip_does_not_draw_and_mana_receipts_are_resolution_effects(self):
  s=self.cases('Annihilate',['lost_target_0'])[0]['steps']
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=7),s)
  s=self.cases('Liturgy of Blood',['payment_0'])[0]['steps']
  self.assertIn(dict(op='assert_mana',player=0,color=1,value=3),s)
  s=self.cases('Liturgy of Blood',['countered_0'])[0]['steps']
  self.assertIn(dict(op='assert_mana',player=0,color=1,value=18),s)
  choices=[x['op'] for x in s if x['op'] in ('choose_target','choose_priority')]
  self.assertEqual(choices[:2],['choose_target','choose_priority'])

 def test_namesake_receipts_check_only_one_of_two_identical_targets_moved(self):
  s=self.cases(selected=['two_targets_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='unselected',property='in_play',value=1),s)
  self.assertIn(dict(op='assert_zone',player=1,zone='battlefield',value=1),s)

 def test_regeneration_window_is_consumed_only_when_the_spell_allows_it(self):
  s=self.cases('Vindicate',['regeneration_0'])[0]['steps']
  self.assertIn(dict(op='choose_regeneration',ref='recipient'),s)
  self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),s)
  for name in ('Annihilate','Final Reward'):
   s=self.cases(name,['regeneration_0'])[0]['steps']
   self.assertIn(dict(op='choose_regeneration',ref='recipient'),s)
   self.assertIn(dict(op='assert_unused_choices',value=1),s)
   self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=0),s)

 def test_bounced_target_rebinds_and_snapshot_replays_real_pending_stack(self):
  s=self.cases(selected=['bounced_target_0'])[0]['steps']
  self.assertTrue(any(x['op']=='bind_hand_copy' and x['ref']=='recipient' for x in s))
  s=self.cases(selected=['pending_snapshot_1'])[0]['steps']
  for op in ('capture','restore','ai_roundtrip','record_state','assert_recorded_state'):self.assertTrue(any(x['op']==op for x in s))

 def test_full_source_pin_and_witnesses_are_immutable(self):
  d=self.declaration('Rend Flesh');before=copy.deepcopy(d);self.cases('Rend Flesh',d=d);self.assertEqual(d,before)
  o=copy.deepcopy(self.oracle);o['Mothdust Changeling']['keywords']=[]
  with self.assertRaises(ValueError):self.cases(oracle=o)


class Wave6RemovalRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  import removal_recipes as module
  cls.module=module;cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Afterlife',selected=None):
  p=copy.deepcopy(self.module.WAVE6_PINS['profiles'][name]['params'])
  return dict(family='target_removal',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['effect_0'])
 def cases(self,name='Afterlife',selected=None,d=None,o=None):return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,o or self.oracle)
 def has(self,steps,**fields):return any(all(s.get(k)==v for k,v in fields.items()) for s in steps)
 def test_wave6_full_source_and_all_parameter_pins_fail_closed(self):
  for key,value in [('effect','exile'),('count',2),('no_regenerate',False),('secondary',{}),('scope','permanent')]:
   d=self.declaration();d['params'][key]=value
   with self.assertRaises(ValueError):self.cases(d=d)
  for key,value in [('oracle_text','Destroy target creature.'),('version',True),('variants',[]),('variants',['effect_0','effect_0']),('variants',['unknown_0'])]:
   d=self.declaration();d[key]=value
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_wave6_public_expand_guard_rejects_different_invalid_selection(self):
  d=self.declaration();p=self.module.spec(d,self.oracle['Afterlife'])
  for selected in ([],['effect_0','effect_0'],['unknown_0']):
   with self.assertRaises(ValueError):self.module.expand('Afterlife',self.rows['Afterlife']['csvid'],d,p,selected,self.rows,self.oracle)
  with self.assertRaises(ValueError):self.module.expand('Afterlife',1,d,p,['effect_0'],self.rows,self.oracle)
 def test_wave6_numeric_and_boolean_types_cannot_alias_pinned_values(self):
  for k,value in [('count',True),('variable',0),('no_regenerate',1)]:
   d=self.declaration();d['params'][k]=value
   with self.assertRaises(ValueError):self.cases(d=d)
  d=self.declaration();d['params']['secondary']['token']['count']=True
  with self.assertRaises(ValueError):self.cases(d=d)
  d=self.declaration();d['variants']=[{}]
  with self.assertRaises(ValueError):self.cases(d=d)
 def test_wave6_private_witness_full_pin_is_validated_publicly(self):
  for n,k,value in [('Spirit','type_line','Token Creature — Changed'),('Grizzly Bears','toughness','3')]:
   o=copy.deepcopy(self.oracle);o[n][k]=value
   with self.assertRaises(ValueError):self.cases(o=o)
 def test_wave6_indestructible_still_receives_unconditional_token(self):
  for n in ('Afterlife','Beast Within'):
   c=self.cases(n,['indestructible_target_1'])[0];s=c['steps'];token=self.module.WAVE6_PINS['profiles'][n]['params']['secondary']['token']
   self.assertTrue(self.has(s,op='assert_card',ref='victim0',property='in_play',value=1))
   self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows[token['name']]['csvid'],value=1))
 def test_wave6_all_lost_suppresses_token_scry_and_life(self):
  for n in ('Afterlife',"Artisan's Sorrow",'Anguished Unmaking'):
   s=self.cases(n,['lost_all_0'])[0]['steps'];self.assertFalse(any(x['op']=='choose_zone' for x in s));self.assertTrue(self.has(s,op='assert_life',player=0,value=20))
   if n=='Afterlife':self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Spirit']['csvid'],value=0))
 def test_wave6_distinct_targets_partial_and_all_lost_are_separate(self):
  for n in ('Peace and Quiet','Into the Core'):
   s=self.cases(n,['partial_lost_1'])[0]['steps'];self.assertEqual([x['ref'] for x in s if x['op']=='choose_target'],['victim0','victim1'])
   self.assertTrue(self.has(s,op='cast',ref='source',value=1));self.assertFalse(self.has(s,op='assert_card',ref='victim1',property='in_play',value=1))
   s=self.cases(n,['short_targets_1'])[0]['steps'];self.assertTrue(self.has(s,op='require_can_cast',ref='source',value=0));self.assertFalse(self.has(s,op='cast',ref='source'))
 def test_wave6_original_owner_zone_is_distinct_from_controller(self):
  s=self.cases('Anguished Unmaking',['stolen_target_0'])[0]['steps']
  self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=1))
  self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=0))
 def test_wave6_normal_cast_cost_and_pending_physical_hand_receipt(self):
  for pl in (0,1):
   s=self.cases('Anguished Unmaking',[f'payment_{pl}'])[0]['steps'];self.assertTrue(self.has(s,op='assert_stack',value=1));self.assertTrue(self.has(s,op='assert_card',ref='source',property='in_hand',value=0));self.assertTrue(self.has(s,op='assert_zone',player=pl,zone='hand',value=5))
   self.assertTrue(self.has(s,op='assert_life',player=pl,value=17))
   for color in range(6):self.assertTrue(self.has(s,op='assert_mana',player=pl,color=color,value=0))
 def test_wave6_real_counter_and_target_responses_use_exact_stack(self):
  for v in ('countered_1','bounced_target_1','target_hexproof_1'):
   s=self.cases('Afterlife',[v])[0]['steps'];self.assertTrue(self.has(s,op='cast_response',ref='response',value=1));self.assertTrue(self.has(s,op='assert_stack',value=2));self.assertTrue(self.has(s,op='resolve_response'))
 def test_wave6_flying_token_has_actual_flying_block(self):
  s=self.cases(selected=['token_flying_1'])[0]['steps'];self.assertTrue(self.has(s,op='block',ref='created_token',other='flyer',value=1));self.assertTrue(self.has(s,op='assert_card',ref='created_token',property='blocking_count',value=1))
 def test_wave6_ai_scry_allows_only_top_window_permutation(self):
  s=self.cases("Artisan's Sorrow",['effect_1'])[0]['steps'];self.assertEqual(len([x for x in s if x['op']=='choose_zone']),1)
  self.assertFalse(self.has(s,op='assert_zone_card',player=1,zone='library',index=0));self.assertTrue(self.has(s,op='assert_zone_card',player=1,zone='library',index=2))
 def test_wave6_empty_library_scry_has_no_input_prompt(self):
  for pl in (0,1):
   s=self.cases("Artisan's Sorrow",[f'bare_zones_{pl}'])[0]['steps'];self.assertFalse(any(x['op']=='choose_zone' for x in s));self.assertTrue(self.has(s,op='assert_choices_consumed'))
 def test_wave6_replay_tails_are_deepcopied_and_owner_native(self):
  for pl in (0,1):
   c=self.cases('Afterlife',[f'pending_snapshot_{pl}'])[0];s=c['steps'];self.assertEqual(len({id(x) for x in s}),len(s));self.assertTrue(self.has(s,op='capture'));self.assertTrue(self.has(s,op='assert_recorded_state'))
   self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='source'),pl)
 def test_wave6_additional_life_is_paid_before_resolve_and_not_refunded(self):
  for pl in (0,1):
   s=self.cases('Fumarole',[f'countered_{pl}'])[0]['steps'];i=next(i for i,x in enumerate(s) if x['op']=='assert_life' and x['player']==pl);self.assertLess(i,next(i for i,x in enumerate(s) if x['op']=='cast_response'))
   self.assertEqual(s[i]['value'],17)
   s=self.cases('Fumarole',[f'insufficient_life_{pl}'])[0]['steps'];self.assertTrue(self.has(s,op='cast',ref='source',value=0));self.assertTrue(self.has(s,op='set_life',player=pl,value=2))
 def test_wave6_differently_worded_target_predicates_allow_same_object(self):
  c=self.cases('Fumarole',['overlap_target_0'])[0];s=c['steps'];self.assertEqual([x['ref'] for x in s if x['op']=='choose_target'],['overlap','overlap'])
  self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Dryad Arbor']['csvid'],value=1))
 def test_wave6_target_chosen_x_ui_has_no_extra_generic_x_choice(self):
  for pl in (0,1):
   s=self.cases('By Force',[f'payment_x3_{pl}'])[0]['steps'];self.assertFalse(any(x['op']=='choose_x' for x in s));self.assertEqual(len([x for x in s if x['op']=='choose_target']),3)
   self.assertTrue(self.has(s,op='set_mana',player=pl,color=0,value=3));self.assertTrue(self.has(s,op='set_mana',player=pl,color=4,value=1))
 def test_wave6_zero_target_x_is_legal_with_no_artifacts(self):
  c=self.cases('By Force',['zero_empty_1'])[0];self.assertFalse(any('Artifact' in self.oracle[x['name']]['type_line'] for x in c['setup'] if x['zone']=='battlefield'))
  self.assertTrue(self.has(c['steps'],op='can_cast',ref='source',value=1));self.assertTrue(self.has(c['steps'],op='cast',ref='source',value=1));self.assertFalse(any(x['op']=='choose_cancel_target' for x in c['steps']))
 def test_wave6_poison_follows_controller_not_original_owner(self):
  s=self.cases('Pistus Strike',['stolen_target_0'])[0]['steps'];self.assertTrue(self.has(s,op='assert_poison',player=1,value=1));self.assertTrue(self.has(s,op='assert_poison',player=0,value=0))
  self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Suntail Hawk']['csvid'],value=1))
 def test_wave6_poison_still_occurs_on_indestructible_but_not_lost_target(self):
  s=self.cases('Pistus Strike',['indestructible_target_1'])[0]['steps'];self.assertTrue(self.has(s,op='assert_card',ref='victim0',property='in_play',value=1));self.assertTrue(self.has(s,op='assert_poison',player=0,value=1))
  s=self.cases('Pistus Strike',['lost_all_1'])[0]['steps'];self.assertTrue(self.has(s,op='assert_poison',player=0,value=0))
 def test_wave6_power_mill_reads_actual_pending_pump_and_short_library(self):
  s=self.cases('Grisly Spectacle',['power_pumped_0'])[0]['steps'];self.assertTrue(self.has(s,op='cast_response',ref='response',value=1));self.assertTrue(self.has(s,op='assert_zone',player=1,zone='graveyard',value=11))
  s=self.cases('Grisly Spectacle',['short_library_0'])[0]['steps'];self.assertTrue(self.has(s,op='assert_zone',player=1,zone='library',value=0))
 def test_wave6_flying_evasion_rejects_a_ground_blocker_after_actual_turn_gates(self):
  s=self.cases('Afterlife',['token_evasion_0'])[0]['steps'];self.assertEqual(len([x for x in s if x['op']=='advance_turn']),2);self.assertTrue(self.has(s,op='attack',ref='created_token',value=1));self.assertTrue(self.has(s,op='block',ref='ground_blocker',other='created_token',value=0))
 def test_wave6_regeneration_scry_binds_observed_ui_order_without_changing_final_effect(self):
  for pl in (0,1):
   s=self.cases("Artisan's Sorrow",[f'regeneration_{pl}'])[0]['steps'];regen=next(i for i,x in enumerate(s) if x['op']=='choose_regeneration');self.assertTrue(all(i<regen for i,x in enumerate(s) if x['op']=='choose_zone'))
   self.assertTrue(self.has(s,op='assert_card',ref='victim0',property='in_play',value=1));self.assertTrue(self.has(s,op='assert_mana',player=1-pl,color=0,value=28))
 def test_wave6_heterogeneous_regeneration_uses_actual_regenerating_creature(self):
  c=self.cases('Fumarole',['regeneration_1'])[0];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='victim0'),'Drudge Skeletons');self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='victim1'),'Forest')
 def test_wave6_variable_short_and_wrong_type_boards_still_allow_legal_x(self):
  for pl in (0,1):
   c=self.cases('By Force',[f'short_targets_{pl}'])[0];s=c['steps'];self.assertTrue(self.has(s,op='can_cast',ref='source',value=1));self.assertTrue(self.has(s,op='cast',ref='source',value=1));self.assertEqual(len([x for x in s if x['op']=='choose_target']),1)
   c=self.cases('By Force',[f'wrong_type_{pl}'])[0];s=c['steps'];self.assertTrue(self.has(s,op='cast',ref='source',value=1));self.assertFalse(any(x['op'] in ('choose_target','choose_cancel_target') for x in s));self.assertEqual([x['name'] for x in c['setup'] if x['zone']=='battlefield'],['Grizzly Bears','Grizzly Bears'])
 def test_wave6_distinct_target_group_can_span_both_controllers(self):
  c=self.cases('Into the Core',['mixed_controller_0'])[0];s=c['steps'];self.assertEqual({x['player'] for x in c['setup'] if x['ref'] in ('victim0','victim1')},{0,1})
  for pl in (0,1):self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=pl,zone='exile',index=self.rows['Wall of Spears']['csvid'],value=1))
 def test_wave6_optional_secondary_target_short_board_and_decline_are_legal(self):
  for pl in (0,1):
   s=self.cases('Relic Crush',[f'short_targets_{pl}'])[0]['steps'];self.assertTrue(self.has(s,op='require_can_cast',ref='source',value=1));self.assertTrue(self.has(s,op='cast',ref='source',value=1));self.assertFalse(any(x['op']=='choose_cancel_target' for x in s))
   s=self.cases('Relic Crush',[f'decline_second_{pl}'])[0]['steps'];self.assertEqual(len([x for x in s if x['op']=='choose_target']),1);self.assertTrue(self.has(s,op='choose_cancel_target',player=pl));self.assertTrue(self.has(s,op='assert_card',ref='unselected',property='in_play',value=1))
 def test_wave6_decimate_overlap_keeps_other_distinct_predicate_slots(self):
  c=self.cases('Decimate',['overlap_target_0'])[0];s=c['steps'];self.assertEqual([x['ref'] for x in s if x['op']=='choose_target'],['overlap','overlap','victim2','victim3']);self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='overlap'),'Wall of Spears')
  self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Wall of Spears']['csvid'],value=1))
 def test_wave6_excluded_artifact_and_black_predicates_have_creature_controls(self):
  for n,v,w in [('Grisly Spectacle','predicate_artifact_creature','Wall of Spears'),('Wicked Pact','predicate_black','Scathe Zombies')]:
   c=self.cases(n,[v+'_0'])[0];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='victim0'),w);self.assertTrue(self.has(c['steps'],op='require_can_cast',ref='source',value=0))
 def test_wave6_power_limit_boundary_and_devoid_protection_are_real_casts(self):
  for n,v,expected in [('Complete Disregard','limit_at',1),('Complete Disregard','limit_above',0),("Skywhaler's Shot",'limit_below',0),("Skywhaler's Shot",'limit_at',1)]:
   s=self.cases(n,[v+'_1'])[0]['steps'];self.assertTrue(self.has(s,op='require_can_cast',ref='source',value=expected))
  for n in ('Complete Disregard','Oblivion Strike','Grip of Desolation'):
   c=self.cases(n,['devoid_protection_0'])[0];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='victim0'),'White Knight');self.assertTrue(self.has(c['steps'],op='cast',ref='source',value=1))
 def test_wave6_union_and_multicolor_predicates_keep_type_and_color_independent(self):
  for n,v,expected in [('Return to the Earth','predicate_artifact',1),('Return to the Earth','predicate_ground',0),('Radiant Purge','predicate_enchantment',1),('Radiant Purge','predicate_multicolor_artifact',0),('Radiant Purge','predicate_colorless',0)]:
   self.assertTrue(self.has(self.cases(n,[v+'_0'])[0]['steps'],op='require_can_cast',ref='source',value=expected))
 def test_wave6_mandatory_discard_uses_controller_and_conserves_all_ai_hand_identities(self):
  s=self.cases("Assassin's Strike",['stolen_target_0'])[0]['steps'];self.assertTrue(self.has(s,op='assert_zone',player=1,zone='hand',value=3));self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2))
  self.assertTrue(any(x['op']=='assert_hand_graveyard_total' and x['player']==1 for x in s));self.assertTrue(any(x['op']=='assert_zone_multiplicity_max' and x['player']==1 and x['zone']=='graveyard' for x in s))
 def test_wave6_discard_empty_and_single_hand_preserve_mandatory_effect(self):
  for n in ("Assassin's Strike",'Desecrated Earth'):
   s=self.cases(n,['one_discard_card_0'])[0]['steps'];self.assertTrue(self.has(s,op='assert_zone',player=1,zone='hand',value=0));self.assertTrue(self.has(s,op='cast',ref='source',value=1))
   s=self.cases(n,['empty_discard_hand_0'])[0]['steps'];self.assertTrue(self.has(s,op='assert_zone',player=1,zone='hand',value=0));self.assertTrue(self.has(s,op='assert_choices_consumed'))
 def test_wave6_pongify_and_rapid_have_complete_actual_subtype_observations(self):
  for n,token,types in [('Pongify','Ape',['Ape']),('Rapid Hybridization','Frog Lizard',['Frog','Lizard'])]:
   for pl in (0,1):
    s=self.cases(n,[f'effect_{pl}'])[0]['steps'];tid=self.rows[token]['csvid']
    for subtype in types:self.assertTrue(self.has(s,op='assert_matching_permanents',player=1-pl,index=tid,property='subtype_'+subtype,value=1))
    for k,value in [('power',3),('toughness',3),('color',8),('token',1)]:self.assertTrue(self.has(s,op='assert_matching_permanents',player=1-pl,index=tid,property=k,value=value))
   self.assertFalse(any('subtype' in gap.lower() for gap in self.declaration(n)['params']['gaps']))
 def test_wave6_pongify_and_rapid_token_not_conditional_on_successful_destruction(self):
  for n,token in [('Pongify','Ape'),('Rapid Hybridization','Frog Lizard')]:
   s=self.cases(n,['indestructible_target_1'])[0]['steps'];self.assertTrue(self.has(s,op='assert_card',ref='victim0',property='in_play',value=1));self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows[token]['csvid'],value=1))
   s=self.cases(n,['lost_all_1'])[0]['steps'];self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows[token]['csvid'],value=0))


if __name__=='__main__':unittest.main()
