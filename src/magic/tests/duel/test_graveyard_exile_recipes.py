import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import graveyard_exile_recipes as f

class GraveyardExileTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def d(self,name='Cremate',variant='effect_0'):
  return dict(family='graveyard_exile',version=2,params=copy.deepcopy(f.PROFILES[name]['params']),oracle_text=self.o[name]['oracle_text'],variants=[variant])
 def cases(self,name='Cremate',variant='effect_0',d=None):
  d=d or self.d(name,variant);p=f.spec(d,self.o[name]);return f.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.o)
 def test_whole_source_identity_cost_text_and_layout_required(self):
  for k,v in [('oracle_text','different'),('mana_cost','{2}{B}'),('type_line','Sorcery'),('layout','split'),('oracle_id','changed')]:
   r=copy.deepcopy(self.o['Cremate']);r[k]=v
   with self.assertRaises(ValueError):f.spec(self.d(),r)
  d=self.d();d['oracle_text']='different'
  with self.assertRaises(ValueError):self.cases(d=d)
 def test_unknown_parameters_and_boolean_number_coercion_rejected(self):
  for k,v in [('scope','creature'),('graveyard','self'),('draw',0),('setup_tapped',0),('quantity','all'),('unknown',True)]:
   d=self.d();d['params'][k]=v
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_version_and_variants_fail_closed(self):
  for version in (1,2.0,True):
   d=self.d();d['version']=version
   with self.assertRaises(ValueError):self.cases(d=d)
  for values in ([],['bad'],['effect_0','effect_0'],[{}]):
   d=self.d();d['variants']=values
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_unknown_sources_and_faces_rejected(self):
  with self.assertRaises(ValueError):f.spec(self.d(),self.o['Tormod\'s Crypt'])
  r=copy.deepcopy(self.o['Cremate']);r['card_faces']=[{}]
  with self.assertRaises(ValueError):f.spec(self.d(),r)
 def test_all_witnesses_including_stock_and_responses_are_pinned(self):
  f.validate_witnesses(self.o,self.rows)
  for name,key,value in [('Grizzly Bears','power','9'),('Reclaim','oracle_text','different'),('Llanowar Elves','mana_cost','{2}{G}'),('Forest','type_line','Land'),('Counterspell','colors',[])]:
   changed=copy.deepcopy(self.o);changed[name][key]=value
   with self.assertRaises(ValueError):f.validate_witnesses(changed,self.rows)
 def test_card_and_creature_predicates_differ(self):
  p=self.d('Crypt Incursion')['params'];self.assertTrue(f.eligible(self.o['Ornithopter'],p));self.assertFalse(f.eligible(self.o['Sol Ring'],p));self.assertFalse(f.eligible(self.o['Forest'],p))
  p=self.d()['params'];self.assertTrue(f.eligible(self.o['Sol Ring'],p));self.assertTrue(f.eligible(self.o['Forest'],p))
 def test_source_controller_and_target_owner_are_independent(self):
  for owner in (0,1):
   for mode,recipient in [('effect',1-owner),('own_target',owner)]:
    c=self.cases(variant=f'{mode}_{owner}')[0]
    self.assertEqual(next(s['player'] for s in c['setup'] if s['ref']=='source'),owner)
    self.assertIn(dict(op='choose_player',player=owner,value=recipient),c['steps'])
    self.assertIn(dict(op='assert_zone_multiplicity',player=recipient,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=1),c['steps'])
 def test_single_eligible_graveyard_skips_player_menu(self):
  for owner in (0,1):
   for variant in ('sole_own_graveyard','sole_opponent_graveyard','bare_zones'):
    steps=self.cases(variant=f'{variant}_{owner}')[0]['steps'];self.assertFalse(any(s['op']=='choose_player' for s in steps));self.assertTrue(any(s['op']=='choose_zone_card' for s in steps))
 def test_spell_not_in_graveyard_until_after_its_exile_effect(self):
  source=self.rows['Morningtide']['csvid'];steps=self.cases('Morningtide')[0]['steps']
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=source,value=1),steps)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=source,value=0),steps)
 def test_repeated_all_exile_can_exile_prior_source_spell(self):
  source=self.rows['Morningtide']['csvid'];steps=self.cases('Morningtide','repeat_1')[0]['steps']
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=source,value=1),steps)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=source,value=1),steps)
 def test_sacrificed_source_is_paid_before_selected_card_exile(self):
  source=self.rows['Crypt Creeper']['csvid'];steps=self.cases('Crypt Creeper','own_target_0')[0]['steps']
  paid=next(i for i,s in enumerate(steps) if s==dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=source,value=1))
  self.assertLess(next(i for i,s in enumerate(steps) if s['op']=='activate'),paid)
  self.assertLess(paid,next(i for i,s in enumerate(steps) if s['op']=='resolve'))
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=source,value=0),steps)
 def test_self_exile_cost_is_not_duplicated_by_all_graveyard_exile(self):
  source=self.rows['Relic of Progenitus']['csvid'];steps=self.cases('Relic of Progenitus')[0]['steps']
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=source,value=1),steps)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=source,value=0),steps)
 def test_alternative_free_ability_is_disabled_for_denied_relic_payment(self):
  c=self.cases('Relic of Progenitus','insufficient_mana_1')[0]
  self.assertEqual(next(s['state'] for s in c['setup'] if s['ref']=='source'),['tapped'])
  self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
 def test_creature_casting_uses_real_core_path_not_raw_handler_guard(self):
  steps=self.cases('Rotfeaster Maggot')[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=1),steps);self.assertFalse(any(s['op']=='require_can_cast' for s in steps))
 def test_fixed_payment_is_actual_and_free_abilities_are_not_denied_for_no_mana(self):
  for name in ('Cremate','Crypt Creeper','Relic of Progenitus'):
   steps=self.cases(name,'payment_1')[0]['steps'];self.assertIn(dict(op='assert_mana_total',player=1,value=0),steps)
  p=f.spec(self.d('Crypt Creeper'),self.o['Crypt Creeper']);self.assertNotIn('insufficient_mana_0',f.variants(p))
 def test_draw_and_life_receipts_use_real_eligible_card_count(self):
  steps=self.cases()[0]['steps'];self.assertIn(dict(op='assert_zone',player=0,zone='library',value=6),steps)
  steps=self.cases('Crypt Incursion')[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=35),steps)
  steps=self.cases('Honor the Fallen')[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=30),steps)
 def test_toughness_life_receipt_changes_with_selected_creature(self):
  for variant,value in [('effect_0',22),('alternate_0',24)]:self.assertIn(dict(op='assert_life',player=0,value=value),self.cases('Rotfeaster Maggot',variant)[0]['steps'])
 def test_lost_target_is_a_real_response_and_prevents_draw(self):
  for owner in (0,1):
   steps=self.cases(variant=f'lost_target_{owner}')[0]['steps']
   self.assertIn(dict(op='cast_response',ref='response',value=1),steps);self.assertIn(dict(op='assert_stack',value=2),steps);self.assertIn(dict(op='resolve_response'),steps)
   resolved=next(i for i,s in enumerate(steps) if s['op']=='resolve_response');self.assertEqual(steps[resolved+1],dict(op='assert_stack',value=0))
   self.assertFalse(any(s['op']=='resolve' for s in steps))
   self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=7),steps)
 def test_empty_single_target_does_not_queue_illegal_selection(self):
  steps=self.cases(variant='empty_graveyards_0')[0]['steps'];self.assertIn(dict(op='can_cast',ref='source',value=0),steps);self.assertFalse(any(s['op']=='choose_zone_card' for s in steps))
 def test_all_graveyards_empty_is_still_a_legal_spell(self):
  steps=self.cases('Morningtide','empty_graveyards_1')[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=1),steps)
 def test_bare_draw_case_keeps_a_real_ordered_library(self):
  c=self.cases(variant='bare_zones_1')[0];self.assertTrue(any(s['zone']=='library' and s['player']==1 for s in c['setup']))
 def test_all_three_snapshot_positions_replay_observable_state(self):
  for name in ('Cremate','Crypt Creeper','Relic of Progenitus','Rotfeaster Maggot'):
   for v in ('ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0'):
    steps=self.cases(name,v)[0]['steps'];self.assertIn(dict(op='ai_roundtrip'),steps);self.assertEqual(steps[-1]['op'],'assert_recorded_state')
 def test_input_declarations_and_oracle_records_are_not_mutated(self):
  d=self.d();before=copy.deepcopy(d);self.cases(d=d);self.assertEqual(d,before)

 def test_new_dialog_branches_follow_live_hand_availability(self):
  for owner in (0,1):
   full=self.cases('Mortiphobia',f'effect_{owner}')[0]['steps'];bare=self.cases('Mortiphobia',f'bare_zones_{owner}')[0]['steps']
   self.assertIn(dict(op='choose_dialog',player=owner,value=1),full)
   self.assertFalse(any(x['op']=='choose_dialog' for x in bare))
 def test_silent_source_exile_is_effect_after_tap_cost(self):
  steps=self.cases('Silent Gravestone')[0]['steps'];source=self.rows['Silent Gravestone']['csvid'];resolve=next(i for i,x in enumerate(steps) if x['op']=='resolve')
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=source,value=0),steps[:resolve])
  self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),steps[:resolve])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=source,value=1),steps[resolve:])
 def test_tap_creature_is_sick_after_real_cast(self):
  steps=self.cases('Thraben Heretic','fresh_activation_1')[0]['steps']
  self.assertIn(dict(op='cast',ref='source',value=1),steps)
  self.assertIn(dict(op='require_can_activate',ref='source',value=0),steps)
 def test_rest_in_peace_replaces_both_owners_real_deaths(self):
  for owner in (0,1):
   for variant,pl in [('replacement_own_death',owner),('replacement_opponent_death',1-owner)]:
    steps=self.cases('Rest in Peace',f'{variant}_{owner}')[0]['steps']
    death=next(i for i,x in enumerate(steps) if x==dict(op='destroy',ref='replacement_control'))
    self.assertGreater(death,next(i for i,x in enumerate(steps) if x['op']=='resolve'))
    self.assertIn(dict(op='assert_zone_multiplicity',player=pl,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=3),steps[death:])
 def test_rest_in_peace_removed_stops_replacing_control_death(self):
  steps=self.cases('Rest in Peace','replacement_removed_0')[0]['steps']
  self.assertLess(steps.index(dict(op='destroy',ref='source')),steps.index(dict(op='destroy',ref='replacement_control')))
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=1),steps)
 def test_rest_in_peace_real_spell_draws_and_exiles_itself(self):
  steps=self.cases('Rest in Peace','replacement_spell_1')[0]['steps']
  self.assertIn(dict(op='cast',ref='replacement_spell',value=1),steps)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows['Divination']['csvid'],value=2),steps)
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=5),steps)
 def test_skaab_cost_is_own_creatures_paid_before_resolution(self):
  for name,count in [('Headless Skaab',1),('Skaab Goliath',2),('Skaab Ruinator',3)]:
   for owner in (0,1):
    steps=self.cases(name,f'effect_{owner}')[0]['steps'];cast=next(i for i,x in enumerate(steps) if x['op']=='cast');resolve=next(i for i,x in enumerate(steps) if x['op']=='resolve')
    selections=[x for x in steps[:cast] if x['op']=='choose_zone_card'];self.assertEqual(len(selections),count);self.assertTrue(all(x['player']==owner for x in selections))
    self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['zone']=='exile' and x['value']>0 for x in steps[cast:resolve]))
    self.assertFalse(any(x['op']=='require_can_cast' for x in steps))
 def test_skaab_short_cost_and_opponent_graveyard_are_illegal(self):
  for variant in ('short_creature_cost_1','opponent_only_cost_0','empty_graveyards_1','wrong_type_0'):
   steps=self.cases('Skaab Ruinator',variant)[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=0),steps);self.assertFalse(any(x['op']=='choose_zone_card' for x in steps))
 def test_countered_skaab_keeps_exile_cost_paid(self):
  steps=self.cases('Skaab Ruinator','countered_1')[0]['steps'];self.assertEqual(len([x for x in steps if x['op']=='choose_zone_card']),3)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Skaab Ruinator']['csvid'],value=1),steps)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows['Hill Giant']['csvid'],value=1),steps)
 def test_headless_tapped_entry_and_undying_gap_are_explicit(self):
  steps=self.cases('Headless Skaab')[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),steps)
  p=f.spec(self.d('Relentless Skaabs'),self.o['Relentless Skaabs']);self.assertNotIn('destroy_source_0',f.variants(p));self.assertTrue(any('Undying' in x for x in p['additional_components']))

 def test_exile_token_is_controlled_by_caster_not_exiled_owner(self):
  for owner in (0,1):
   steps=self.cases('Vile Rebirth',f'effect_{owner}')[0]['steps'];token=self.rows['Zombie']['csvid']
   self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=1),steps)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=token,value=1),steps)
   self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='battlefield',index=token,value=0),steps)
 def test_fizzled_token_spell_neither_creates_nor_gains_life(self):
  for owner in (0,1):
   steps=self.cases('Shamble Back',f'lost_target_{owner}')[0]['steps']
   self.assertIn(dict(op='cast_response',ref='response',value=1),steps)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Zombie']['csvid'],value=0),steps)
   self.assertIn(dict(op='assert_life',player=owner,value=20),steps)
 def test_exile_life_token_combines_all_printed_effects(self):
  steps=self.cases('Shamble Back','effect_1')[0]['steps'];self.assertIn(dict(op='assert_life',player=1,value=22),steps)
  for prop,val in [('power',2),('toughness',2)]:self.assertIn(dict(op='assert_card',ref='created_token_0',property=prop,value=val),steps)
  for prop,val in [('token',1),('color',2),('subtype_Zombie',1)]:self.assertIn(dict(op='assert_matching_permanents',player=1,index=self.rows['Zombie']['csvid'],property=prop,value=val),steps)
 def test_ritual_token_statistics_follow_selected_creature(self):
  for variant,power,toughness in [('effect_0',2,2),('alternate_0',5,4)]:
   steps=self.cases('Ritual of the Returned',variant)[0]['steps']
   for prop,value in [('power',power),('toughness',toughness)]:self.assertIn(dict(op='assert_card',ref='created_token_0',property=prop,value=value),steps)
 def test_repeated_token_ability_keeps_two_independent_tokens(self):
  steps=self.cases('Necrogenesis','repeat_1')[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Saproling']['csvid'],value=2),steps)
  self.assertIn(dict(op='bind_permanent_copy',player=1,value=self.rows['Saproling']['csvid'],index=1,ref='created_token_1'),steps)
 def test_token_destroy_bounce_and_sickness_are_real_actions(self):
  for variant in ('destroy_token_0','bounce_token_1'):
   steps=self.cases('Vile Rebirth',variant)[0]['steps'];who=int(variant[-1]);self.assertIn(dict(op='assert_zone_multiplicity',player=who,zone='battlefield',index=self.rows['Zombie']['csvid'],value=0),steps)
   self.assertTrue(any(x['op']=='destroy' or x.get('ref')=='token_bounce' and x['op']=='cast' for x in steps))
  self.assertIn(dict(op='attack',ref='action_token',value=0),self.cases('Vile Rebirth','fresh_token_attack_0')[0]['steps'])
 def test_new_witness_pins_fail_closed_without_altering_old_zone_universe(self):
  self.assertNotIn('Zombie',f.PINS['cards']);self.assertIn('Zombie',f.PINS['extra_cards'])
  changed=copy.deepcopy(self.o);changed['Zombie']['type_line']='Creature';
  with self.assertRaises(ValueError):f.validate_witnesses(changed,self.rows)
 def test_new_optional_permissions_and_additional_entry_triggers_are_gaps(self):
  for name in ('Mardu Woe-Reaper','Dire Fleet Daredevil','Myr Welder'):self.assertTrue(self.d(name)['params']['additional_components'])
  p=f.spec(self.d('Mardu Woe-Reaper'),self.o['Mardu Woe-Reaper']);self.assertNotIn('repeat_0',f.variants(p))

 def test_every_emitted_property_uses_supported_observer_schema(self):
  from generate_cases import PROPERTIES,TOKEN_PROPERTIES,OPS
  for name in f.PROFILES:
   d=self.d(name);p=f.spec(d,self.o[name]);d['variants']=f.variants(p)
   for c in f.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.o):
    for x in c['steps']:
     self.assertIn(x['op'],OPS)
     if 'property' in x:self.assertIn(x['property'],PROPERTIES | (TOKEN_PROPERTIES if x['op']=='assert_matching_permanents' else set()))

 def test_activation_exile_cost_precedes_token_effect(self):
  for name,count in [('Bearscape',2),('Graf Harvest',1),('Ongoing Investigation',1)]:
   for owner in (0,1):
    steps=self.cases(name,f'effect_{owner}')[0]['steps'];activate=next(i for i,x in enumerate(steps) if x['op']=='activate');resolve=next(i for i,x in enumerate(steps) if x['op']=='resolve')
    self.assertEqual(len([x for x in steps[:activate] if x['op']=='choose_zone_card']),count)
    self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['zone']=='exile' and x['value']>0 for x in steps[activate:resolve]))
    self.assertFalse(any(x['op']=='choose_player' for x in steps))
 def test_only_own_grave_can_pay_activation_exile_cost(self):
  for name in ('Bearscape','Graf Harvest','Corpseweft','Ongoing Investigation'):
   for v in ('opponent_only_cost_0','short_exile_cost_1'):
    steps=self.cases(name,v)[0]['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=0),steps);self.assertFalse(any(x['op']=='choose_zone_card' for x in steps))
 def test_variable_exile_cost_creates_twice_count_tapped_token(self):
  for variant,count in [('exile_one_0',1),('exile_two_0',2),('effect_0',3)]:
   steps=self.cases('Corpseweft',variant)[0]['steps'];activate=next(i for i,x in enumerate(steps) if x['op']=='activate')
   self.assertEqual(len([x for x in steps[:activate] if x['op']=='choose_zone_card']),count)
   self.assertTrue(any(x['op']=='choose_optional_zone' for x in steps[:activate]))
   for prop,value in [('power',2*count),('toughness',2*count),('tapped',1)]:self.assertIn(dict(op='assert_card',ref='created_token_0',property=prop,value=value),steps)
 def test_clue_is_an_artifact_and_life_gain_is_mandatory(self):
  steps=self.cases('Ongoing Investigation','effect_1')[0]['steps'];self.assertIn(dict(op='assert_life',player=1,value=22),steps)
  for prop,value in [('artifact',1),('color',0),('subtype_Clue',1)]:self.assertIn(dict(op='assert_matching_permanents',player=1,index=self.rows['Clue']['csvid'],property=prop,value=value),steps)
  self.assertFalse(any(x.get('ref')=='created_token_0' and x.get('property') in ('power','toughness') for x in steps))
  p=f.spec(self.d('Ongoing Investigation'),self.o['Ongoing Investigation']);self.assertNotIn('bounce_token_0',f.variants(p));self.assertNotIn('fresh_token_attack_0',f.variants(p))

 def test_up_to_zero_targets_still_casts_or_activates_legally(self):
  for name in ('Decompose','Shred Memory','Carrion Beetles'):
   for variant in ('zero_targets_0','empty_graveyards_1'):
    steps=self.cases(name,variant)[0]['steps'];activation=name=='Carrion Beetles'
    self.assertIn(dict(op='activate' if activation else 'cast',ref='source',value=1),steps)
    self.assertIn(dict(op='require_can_activate' if activation else 'require_can_cast',ref='source',value=1),steps)
    self.assertFalse(any(x['op']=='choose_zone_card' for x in steps))
    self.assertEqual(any(x['op']=='choose_optional_zone' for x in steps),variant.startswith('zero'))
 def test_up_to_player_selector_matches_displayed_owner_menu(self):
  for owner in (0,1):
   for name,required in [('Decompose',True),('Carrion Beetles',True),('Shred Memory',False)]:
    steps=self.cases(name,f'sole_own_graveyard_{owner}')[0]['steps']
    self.assertEqual(dict(op='choose_player',player=owner,value=owner) in steps,required)
 def test_up_to_limits_and_two_equal_named_copies(self):
  for name,maximum in [('Decompose',3),('Shred Memory',4)]:
   for variant,amount in [('one_target_0',1),('max_targets_0',maximum),('duplicate_targets_0',2)]:
    steps=self.cases(name,variant)[0]['steps'];picks=[x for x in steps if x['op']=='choose_zone_card'];self.assertEqual(len(picks),amount)
    if variant.startswith('duplicate'):
     self.assertEqual(len({x['value'] for x in picks}),1)
     self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=2),steps)
 def test_partial_target_loss_preserves_other_selected_cards(self):
  steps=self.cases('Decompose','partial_lost_target_1')[0]['steps'];cast=next(i for i,x in enumerate(steps) if x['op']=='cast');self.assertEqual(len([x for x in steps[:cast] if x['op']=='choose_zone_card']),3)
  self.assertIn(dict(op='cast_response',ref='response',value=1),steps);self.assertFalse(any(x['op']=='resolve' for x in steps))
  for name in ('Giant Growth','Forest'):self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows[name]['csvid'],value=1),steps)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=0),steps)
 def test_up_to_all_selected_lost_and_source_not_exiled_from_stack(self):
  steps=self.cases('Decompose','lost_target_0')[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=0),steps)
  steps=self.cases('Decompose','own_target_0')[0]['steps'];source=self.rows['Decompose']['csvid']
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=source,value=1),steps)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=source,value=0),steps)

 def test_identity_crisis_exiles_chosen_hand_and_grave_only(self):
  steps=self.cases('Identity Crisis','own_target_1')[0]['steps']
  for zone,value in [('hand',0),('graveyard',1)]:self.assertIn(dict(op='assert_zone',player=1,zone=zone,value=value),steps)
  self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=4),steps)
  for owner in (0,1):self.assertIn(dict(op='assert_zone',player=owner,zone='battlefield',value=4),steps)
 def test_global_zone_wipes_keep_current_spell_out_of_exile(self):
  for name in ('Worldfire','Decree of Annihilation'):
   for owner in (0,1):
    steps=self.cases(name,f'effect_{owner}')[0]['steps'];source=self.rows[name]['csvid']
    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=source,value=1),steps)
    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='exile',index=source,value=0),steps)
    for who in (0,1):self.assertIn(dict(op='assert_zone',player=who,zone='hand',value=0),steps)
 def test_worldfire_sets_both_life_and_decree_spares_enchantments(self):
  steps=self.cases('Worldfire')[0]['steps']
  for owner in (0,1):
   self.assertIn(dict(op='assert_life',player=owner,value=1),steps);self.assertIn(dict(op='assert_zone',player=owner,zone='battlefield',value=0),steps)
  steps=self.cases('Decree of Annihilation')[0]['steps']
  for owner in (0,1):
   self.assertIn(dict(op='assert_zone',player=owner,zone='battlefield',value=1),steps)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='exile',index=self.rows['Seal of Cleansing']['csvid'],value=1),steps)
   self.assertIn(dict(op='assert_card',ref=f'zone_control_{owner}_2',property='in_play',value=1),steps)
   for name in ('Forest','Sol Ring','Grizzly Bears'):self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['player']==owner and x['zone']=='exile' and x['index']==self.rows[name]['csvid'] and x['value']>0 for x in steps))
 def test_zone_wipe_payment_denial_has_no_independent_mana_sources(self):
  for name in ('Worldfire','Identity Crisis','Decree of Annihilation'):
   c=self.cases(name,'insufficient_mana_0')[0]
   for obj in c['setup']:
    if obj['zone']=='battlefield' and obj['player']==0:self.assertEqual(obj['state'],['tapped'])
   self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])

 def test_unearth_source_is_seeded_in_grave_and_uses_real_helper(self):
  for owner in (0,1):
   c=self.cases('Dregscape Zombie',f'effect_{owner}')[0]
   self.assertEqual(next(x['zone'] for x in c['setup'] if x['ref']=='source'),'graveyard')
   self.assertEqual(c['steps'][0],dict(op='prepare_graveyard_abilities'))
   self.assertIn(dict(op='activate_graveyard',ref='source',value=1),c['steps'])
   self.assertIn(dict(op='bind_permanent',player=owner,value=self.rows['Dregscape Zombie']['csvid'],ref='returned'),c['steps'])
 def test_unearth_pending_source_stays_in_grave_because_cost_is_mana_only(self):
  for name in ('Dregscape Zombie','Viscera Dragger'):
   for owner in (0,1):
    steps=self.cases(name,f'payment_{owner}')[0]['steps'];activate=next(i for i,x in enumerate(steps) if x['op']=='activate_graveyard');resolve=next(i for i,x in enumerate(steps) if x['op']=='resolve');between=steps[activate:resolve]
    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[name]['csvid'],value=1),between)
    self.assertIn(dict(op='assert_zone',player=owner,zone='battlefield',value=0),between)
    self.assertIn(dict(op='assert_mana_total',player=owner,value=0),between)
    self.assertNotIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[name]['csvid'],value=0),between)
 def test_unearth_real_early_leaves_and_cleanup_expect_owned_exile(self):
  for variant in ('cleanup_0','destroy_source_1','bounce_source_0','exile_source_1'):
   steps=self.cases('Dregscape Zombie',variant)[0]['steps'];owner=int(variant[-1]);source=self.rows['Dregscape Zombie']['csvid']
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='exile',index=source,value=1),steps)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=source,value=0),steps)
   self.assertIn(dict(op='assert_zone',player=owner,zone='battlefield',value=0),steps)
   if variant.startswith('exile'):self.assertIn(dict(op='assert_life',player=owner,value=22),steps)
 def test_unearth_haste_attack_and_sorcery_timing_are_actual_actions(self):
  steps=self.cases('Dregscape Zombie','haste_attack_1')[0]['steps'];self.assertIn(dict(op='attack',ref='returned',value=1),steps);self.assertIn(dict(op='remove_from_combat',ref='returned'),steps)
  for variant in ('opposing_turn_0','combat_timing_1','stack_timing_0'):
   steps=self.cases('Dregscape Zombie',variant)[0]['steps'];self.assertIn(dict(op='require_can_activate_graveyard',ref='source',value=0),steps);self.assertIn(dict(op='activate_graveyard',ref='source',value=0),steps)
 def test_unearth_normal_hand_cast_is_distinct_from_grave_activation(self):
  steps=self.cases('Viscera Dragger','cast_payment_1')[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=1),steps);self.assertFalse(any(x['op']=='activate_graveyard' for x in steps));self.assertIn(dict(op='assert_mana_total',player=1,value=0),steps)
 def test_unearth_snapshot_replay_and_no_unsafe_removed_source_inspection(self):
  for variant in ('ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0'):
   steps=self.cases('Viscera Dragger',variant)[0]['steps'];self.assertIn(dict(op='ai_roundtrip'),steps);self.assertEqual(steps[-1]['op'],'assert_recorded_state')
   self.assertFalse(any(x['op']=='assert_card' and x.get('ref')=='source' for x in steps))

 def test_unearth_combat_timing_negative_completes_normal_combat_damage(self):
  for owner in (0,1):
   steps=self.cases('Dregscape Zombie',f'combat_timing_{owner}')[0]['steps'];deny=steps.index(dict(op='require_can_activate_graveyard',ref='source',value=0));damage=steps.index(dict(op='combat_damage'));end=steps.index(dict(op='end_combat'))
   self.assertLess(deny,damage);self.assertLess(damage,end)

 def test_each_required_unearth_color_has_an_isolated_absence_control(self):
  for owner in (0,1):
   steps=self.cases('Fire-Field Ogre',f'missing_color_2_{owner}')[0]['steps']
   self.assertIn(dict(op='set_mana',player=owner,color=0,value=1),steps)
   for color,value in [(1,1),(2,0),(4,1)]:self.assertIn(dict(op='set_mana',player=owner,color=color,value=value),steps)
   self.assertIn(dict(op='require_can_activate_graveyard',ref='source',value=0),steps)
   p=f.spec(self.d('Fire-Field Ogre'),self.o['Fire-Field Ogre'])
   for color in (1,2,4):self.assertIn(f'missing_color_{color}_{owner}',f.variants(p))
 def test_pure_colored_unearth_cost_has_exact_color_receipts(self):
  steps=self.cases('Fire-Field Ogre','effect_0')[0]['steps']
  for color,value in [(0,10),(1,9),(2,9),(4,9)]:self.assertIn(dict(op='assert_mana',player=0,color=color,value=value),steps)
 def test_generic_unearth_payment_preserves_legal_color_allocation_freedom(self):
  steps=self.cases('Viscera Dragger','effect_0')[0]['steps']
  self.assertIn(dict(op='assert_mana_total',player=0,value=58),steps)
  self.assertFalse(any(x['op']=='assert_mana' for x in steps))
 def test_unearth_companion_scope_never_discards_or_mills_unmodeled_cards(self):
  for name in ('Sedraxis Specter','Extractor Demon'):
   p=f.spec(self.d(name),self.o[name]);self.assertTrue(p['additional_components'])
   steps=self.cases(name,'haste_attack_0')[0]['steps'];self.assertLess(steps.index(dict(op='remove_from_combat',ref='returned')),steps.index(dict(op='combat_damage')))
  self.assertEqual(f.PROFILES['Corpse Connoisseur']['params']['entry_effect'],'tutor_creature')
  for name,effect in [('Anathemancer','nonbasic_damage'),('Kederekt Leviathan','bounce_nonlands'),('Rotting Rats','both_discard')]:self.assertEqual(f.PROFILES[name]['params']['entry_effect'],effect)

 def test_extractor_self_leave_target_sentinel_is_oracle_unused_and_drained(self):
  for owner in (0,1):
   for mode in ('destroy_source','bounce_source','exile_source','cleanup'):
    steps=self.cases('Extractor Demon',f'{mode}_{owner}')[0]['steps'];choice=steps.index(dict(op='choose_player',player=owner,value=owner));unused=steps.index(dict(op='assert_unused_choices',value=8));self.assertEqual(len([x for x in steps if x==dict(op='choose_player',player=owner,value=owner)]),8)
    self.assertIn(dict(op='assert_zone',player=owner,zone='battlefield',value=1),steps[:choice]);self.assertIn(dict(op='assert_zone',player=1-owner,zone='battlefield',value=0),steps[:choice])
    self.assertLess(choice,unused)
    self.assertTrue(any(x['op'] in ('destroy','cleanup','resolve') for x in steps[choice+1:unused]))
    self.assertIn(dict(op='assert_choices_consumed'),steps[unused+1:])
    self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=7),steps)
 def test_fire_field_payment_reserve_changes_funding_not_printed_cost(self):
  for owner in (0,1):
   steps=self.cases('Fire-Field Ogre',f'payment_{owner}')[0]['steps']
   self.assertIn(dict(op='set_mana',player=owner,color=0,value=1),steps)
   self.assertIn(dict(op='assert_mana',player=owner,color=0,value=1),steps)
   for color in (1,2,4):self.assertIn(dict(op='assert_mana',player=owner,color=color,value=0),steps)
   self.assertIn(dict(op='assert_mana_total',player=owner,value=1),steps)

 def test_delve_exiles_own_mixed_cards_as_cost_before_resolution(self):
  for owner in (0,1):
   steps=self.cases('Gurmag Angler',f'effect_{owner}')[0]['steps'];cast=next(i for i,x in enumerate(steps) if x['op']=='cast');resolve=next(i for i,x in enumerate(steps) if x['op']=='resolve')
   choices=[x for x in steps[:cast] if x['op']=='choose_zone_card']
   self.assertEqual([x['value'] for x in choices],[self.rows[n]['csvid'] for n in ('Giant Growth','Forest')]);self.assertTrue(all(x['player']==owner for x in choices))
   self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['zone']=='exile' and x['value']>0 for x in steps[cast:resolve]))
   self.assertTrue(any(x['op']=='assert_zone' and x.get('zone')=='hand' and x.get('player')==owner for x in steps[cast:resolve]))
 def test_delve_zero_exile_and_normal_full_payment_remain_legal(self):
  for variant in ('zero_cost_0','normal_payment_1','no_own_graveyard_0','empty_graveyards_1'):
   steps=self.cases('Tombstalker',variant)[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=1),steps);self.assertFalse(any(x['op']=='choose_zone_card' for x in steps))
   self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),steps)
 def test_delve_cannot_use_opponents_cards_or_replace_colored_mana(self):
  for variant in ('opponent_only_cost_0','insufficient_exile_cost_1','wrong_color_0'):
   steps=self.cases('Shambling Attendants',variant)[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=0),steps);self.assertFalse(any(x['op']=='choose_zone_card' for x in steps))
 def test_delve_exact_payment_preserves_printed_colored_cost(self):
  for owner in (0,1):
   steps=self.cases('Tombstalker',f'payment_{owner}')[0]['steps'];self.assertIn(dict(op='set_mana',player=owner,color=1,value=2),steps);self.assertIn(dict(op='assert_mana_total',player=owner,value=0),steps)
   self.assertEqual(len([x for x in steps if x['op']=='choose_zone_card']),6)
 def test_delve_countering_does_not_refund_exiled_cost(self):
  steps=self.cases('Gurmag Angler','countered_1')[0]['steps'];self.assertIn(dict(op='choose_priority',player=0,ref='counter'),steps)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Gurmag Angler']['csvid'],value=1),steps)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows['Giant Growth']['csvid'],value=1),steps)
 def test_soulflayer_no_keyword_exile_still_requires_real_legal_cast(self):
  steps=self.cases('Soulflayer','effect_1')[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=1),steps);self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),steps)
  self.assertTrue(self.d('Soulflayer')['params']['additional_components'])
 def test_delve_sources_pin_entire_cost_text_and_keyword_identity(self):
  r=copy.deepcopy(self.o['Soulflayer']);r['keywords']=[]
  with self.assertRaises(ValueError):f.spec(self.d('Soulflayer'),r)
 def test_delve_snapshot_replay_keeps_decision_controller(self):
  for variant in ('ready_snapshot_1','pending_snapshot_1','resolved_snapshot_1'):
   steps=self.cases('Gurmag Angler',variant)[0]['steps'];self.assertEqual(steps[-1]['op'],'assert_recorded_state')
   self.assertTrue(all(x['player']==1 for x in steps if x['op'].startswith('choose_')))

 def capacity_d(self):
  profile=f.PINS['components']['myr_capacity']
  return dict(family='graveyard_exile',version=2,params=copy.deepcopy(profile['params']),oracle_text=self.o['Myr Welder']['oracle_text'],variants=[f'slot{slot}_{who}' for who in (0,1) for slot in (14,15)])
 def test_myr_capacity_component_is_exactly_the_four_validated_raw_bodies(self):
  import hashlib
  # Independent root-authored raw controls passed both runtimes before enrollment.
  expected=['d6b3d4fe7f53998a64a0259aef7c9fdee973c4645571ede482020af14f2c4b3c','f7509e1dcbcea78575ac1edac484183508a67d702c1d5c5c121f2af25d30cc61','f217e90e4710ce78a469323c00eebc5924d7bee3d62ad7fc164952d81ca64acf','3129b9eaa38a6f45cc842a79c626738108b245b4b5dea961a0448f90b4f67400']
  d=self.capacity_d();cases=self.cases('Myr Welder',d=d)
  self.assertEqual(len(cases),4)
  for c,digest in zip(cases,expected):
   body={k:v for k,v in c.items() if k!='_recipe'}
   self.assertEqual(hashlib.sha256(json.dumps(body,sort_keys=True,separators=(',',':'),ensure_ascii=True).encode('utf8')).hexdigest(),digest)
 def test_myr_capacity_parameters_and_slots_fail_closed(self):
  for key,value in [('imprints',15),('imprints',True),('slots',[13,15]),('menu_page_size',13),('witness','Sol Ring')]:
   d=self.capacity_d();d['params'][key]=value
   with self.assertRaises(ValueError):self.cases('Myr Welder',d=d)
  for selected in ([],['slot16_0'],['slot14_0','slot14_0']):
   d=self.capacity_d();d['variants']=selected
   with self.assertRaises(ValueError):self.cases('Myr Welder',d=d)
 def test_myr_capacity_independent_full_source_and_witness_pins(self):
  for name,key,value in [('Myr Welder','power','9'),('Sol Ring','oracle_text','{T}: Add {C}.'),('Mana Vault','oracle_text','{T}: Add {C}{C}.')]:
   changed=copy.deepcopy(self.o);changed[name][key]=value
   with self.assertRaises(ValueError):f.validate_witnesses(changed,self.rows)
  d=self.capacity_d();r=copy.deepcopy(self.o['Myr Welder']);r['color_identity']=['U']
  with self.assertRaises(ValueError):f.spec(d,r)
 def test_myr_capacity_mana_receipt_is_derived_from_independent_printed_ability(self):
  d=self.capacity_d();p=f.spec(d,self.o['Myr Welder']);changed=copy.deepcopy(self.o)
  changed['Mana Vault']['oracle_text']=changed['Mana Vault']['oracle_text'].rsplit('\n',1)[0]+'\n{T}: Add {C}{C}{C}{C}.'
  cases=f.expand('Myr Welder',self.rows['Myr Welder']['csvid'],d,p,d['variants'],self.rows,changed)
  for c in cases:self.assertIn(dict(op='assert_mana',player=c['setup'][0]['player'],color=0,value=4),c['steps'])
 def test_myr_capacity_requires_actual_sixteen_imprints_and_normal_untaps(self):
  for c in self.cases('Myr Welder',d=self.capacity_d()):
   owner=c['setup'][0]['player'];steps=c['steps']
   self.assertEqual(len([x for x in steps if x['op']=='choose_zone_card']),16)
   self.assertEqual(len([x for x in steps if x['op']=='untap_step']),32)
   self.assertIn(dict(op='assert_zone',player=owner,zone='exile',value=16),steps)
   self.assertEqual([x['value'] for x in steps if x['op']=='choose_dialog'][-2:], [14,int(c['name'].split('_')[1].removeprefix('slot'))-14])

 def test_anathemancer_entry_counts_nonbasic_lands_and_keeps_basics(self):
  for owner in (0,1):
   steps=self.cases('Anathemancer',f'effect_{owner}')[0]['steps']
   self.assertIn(dict(op='assert_life',player=1-owner,value=18),steps)
   self.assertIn(dict(op='assert_zone',player=owner,zone='battlefield',value=4),steps)
   self.assertIn(dict(op='assert_zone',player=1-owner,zone='battlefield',value=3),steps)
   zero=self.cases('Anathemancer',f'zero_nonbasic_{owner}')[0]['steps'];self.assertIn(dict(op='assert_life',player=1-owner,value=20),zero)
 def test_anathemancer_selection_inputs_do_not_forge_ai_self_targeting(self):
  steps=self.cases('Anathemancer','own_target_0')[0]['steps'];self.assertIn(dict(op='choose_player',player=0,value=0),steps);self.assertIn(dict(op='assert_life',player=0,value=18),steps)
  self.assertFalse(any(x['op']=='choose_player' for x in self.cases('Anathemancer','effect_1')[0]['steps']))
  p=f.spec(self.d('Anathemancer'),self.o['Anathemancer']);self.assertNotIn('own_target_1',f.variants(p));self.assertTrue(p['additional_components'])
 def test_unearth_entry_mana_denials_leave_independent_sources_tapped(self):
  for name in ('Anathemancer','Kederekt Leviathan'):
   c=self.cases(name,'insufficient_mana_1')[0]
   for x in c['setup']:
    if x['zone']=='battlefield' and 'Land' in self.o[x['name']]['type_line']:self.assertIn('tapped',x['state'])
   self.assertIn(dict(op='activate_graveyard',ref='source',value=0),c['steps'])
 def test_rotting_rats_discards_both_players_cards_after_real_return(self):
  for owner in (0,1):
   c=self.cases('Rotting Rats',f'effect_{owner}')[0];steps=c['steps']
   self.assertIn(dict(op='assert_discard_choices',value=1),steps)
   for who in (0,1):self.assertIn(dict(op='assert_zone_multiplicity',player=who,zone='graveyard',index=self.rows['Giant Growth']['csvid'],value=2),steps)
   self.assertEqual(set(x['name'] for x in c['setup'] if x['zone']=='hand' and x['player']==1),{'Giant Growth'})
 def test_rotting_rats_lifecycle_responses_remain_in_human_legal_hand(self):
  for variant,n in [('bounce_source_1','Unsummon'),('exile_source_1','Swords to Plowshares')]:
   c=self.cases('Rotting Rats',variant)[0]
   self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='response'),0)
   self.assertIn(dict(op='choose_target',player=0,ref='returned'),c['steps'])
   self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[n]['csvid'],value=1),c['steps'])
 def test_rotting_rats_empty_hands_do_not_create_discard_choices(self):
  for owner in (0,1):self.assertIn(dict(op='assert_discard_choices',value=0),self.cases('Rotting Rats',f'empty_hands_{owner}')[0]['steps'])
 def test_rotting_rats_ai_discard_notification_is_an_actual_separate_input(self):
  for owner in (0,1):
   steps=self.cases('Rotting Rats',f'effect_{owner}')[0]['steps'];resolve=next(i for i,x in enumerate(steps) if x['op']=='resolve')
   self.assertEqual(steps[resolve-1],dict(op='choose_dialog',player=1,value=0))
   steps=self.cases('Rotting Rats',f'empty_hands_{owner}')[0]['steps'];self.assertNotIn(dict(op='choose_dialog',player=1,value=0),steps)
 def test_leviathan_returns_other_nonlands_to_both_original_owners(self):
  for owner in (0,1):
   steps=self.cases('Kederekt Leviathan',f'effect_{owner}')[0]['steps']
   for who in (0,1):
    for n in ('Grizzly Bears','Sol Ring','Seal of Cleansing'):self.assertIn(dict(op='assert_zone_multiplicity',player=who,zone='hand',index=self.rows[n]['csvid'],value=2 if n=='Grizzly Bears' else 1),steps)
    self.assertIn(dict(op='assert_zone',player=who,zone='battlefield',value=1+int(who==owner)),steps)
   self.assertIn(dict(op='assert_card',ref='returned',property='in_play',value=1),steps)
 def test_ue_entry_companions_also_run_on_ordinary_paid_cast(self):
  self.assertIn(dict(op='assert_life',player=1,value=18),self.cases('Anathemancer','cast_source_0')[0]['steps'])
  self.assertIn(dict(op='assert_discard_choices',value=1),self.cases('Rotting Rats','cast_source_0')[0]['steps'])
  self.assertIn(dict(op='assert_zone',player=0,zone='battlefield',value=2),self.cases('Kederekt Leviathan','cast_source_0')[0]['steps'])

 def test_delve_postcast_receipt_accounts_real_counterspell_same_controller(self):
  for owner in (0,1):
   steps=self.cases('Gurmag Angler',f'countered_{owner}')[0]['steps'];cast=next(i for i,x in enumerate(steps) if x['op']=='cast');resolve=next(i for i,x in enumerate(steps) if x['op']=='resolve')
   self.assertIn(dict(op='assert_mana_total',player=owner,value=57 if owner==0 else 59),steps[cast:resolve])
 def test_flashback_cost_life_and_effect_are_derived_from_pinned_oracle(self):
  p=f.spec(self.d('Deep Analysis'),self.o['Deep Analysis'])
  self.assertEqual(p['mana'],[3,0,1,0,0,0]);self.assertEqual(p['flashback_mana'],[1,0,1,0,0,0]);self.assertEqual(p['flashback_life'],3);self.assertEqual(p['amount'],2)
  p=f.spec(self.d('Saving Grasp'),self.o['Saving Grasp']);self.assertEqual(p['flashback_mana'],[0,0,0,0,0,1]);self.assertEqual(p['flashback_life'],0)
 def test_flashback_uses_actual_atomic_cast_without_outer_menu_or_pending_boundary(self):
  for name in ('Deep Analysis','Dream Twist','Chill of Foreboding','Saving Grasp','Forbidden Alchemy'):
   for owner in (0,1):
    steps=self.cases(name,f'effect_{owner}')[0]['steps'];cast=next(i for i,x in enumerate(steps) if x['op']=='cast_graveyard')
    self.assertEqual(steps[cast+1],dict(op='assert_stack',value=0));self.assertFalse(any(x['op'] in ('choose_dialog','resolve') for x in steps))
    self.assertFalse(any(x['op']=='assert_card' and x.get('ref')=='source' for x in steps))
 def test_flashback_actual_paid_cost_and_life_are_not_replaced_by_normal_cost(self):
  for owner in (0,1):
   steps=self.cases('Deep Analysis',f'payment_{owner}')[0]['steps'];self.assertIn(dict(op='assert_mana_total',player=owner,value=0),steps);self.assertIn(dict(op='assert_life',player=owner,value=17),steps)
   steps=self.cases('Deep Analysis',f'cast_payment_{owner}')[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=1),steps);self.assertIn(dict(op='assert_life',player=owner,value=20),steps)
 def test_flashback_life_denial_keeps_actual_source_in_graveyard(self):
  for owner in (0,1):
   steps=self.cases('Deep Analysis',f'insufficient_life_{owner}')[0]['steps'];self.assertIn(dict(op='require_can_cast_graveyard',ref='source',value=0),steps);self.assertIn(dict(op='cast_graveyard',ref='source',value=0),steps)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Deep Analysis']['csvid'],value=1),steps)
   self.assertIn(dict(op='assert_life',player=owner,value=2),steps)
 def test_flashback_draw_target_is_independent_of_cost_payer(self):
  for owner in (0,1):
   steps=self.cases('Deep Analysis',f'opponent_target_{owner}')[0]['steps'];self.assertIn(dict(op='choose_player',player=owner,value=1-owner),steps)
   self.assertIn(dict(op='assert_life',player=owner,value=17),steps);self.assertIn(dict(op='assert_life',player=1-owner,value=20),steps)
   self.assertIn(dict(op='assert_zone',player=1-owner,zone='hand',value=6),steps);self.assertIn(dict(op='assert_zone',player=1-owner,zone='library',value=5),steps)
 def test_flashback_grasp_returns_owned_opposing_control_to_original_owner(self):
  for owner in (0,1):
   c=self.cases('Saving Grasp',f'owned_opponent_control_{owner}')[0];obj=next(x for x in c['setup'] if x['ref']=='target');self.assertEqual(obj['player'],1-owner);self.assertIn('opponent_owned',obj['state'])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Grizzly Bears']['csvid'],value=2),c['steps'])
   self.assertIn(dict(op='assert_zone',player=1-owner,zone='battlefield',value=0),c['steps'])
 def test_flashback_repeat_grave_refs_remain_valid_after_first_source_removal(self):
  for owner in (0,1):
   c=self.cases('Dream Twist',f'repeat_{owner}')[0];refs=[x['ref'] for x in c['setup'] if x['player']==owner and x['zone']=='graveyard'];self.assertLess(refs.index('source2'),refs.index('source'))
   self.assertEqual([x['ref'] for x in c['steps'] if x['op']=='cast_graveyard'],['source','source2'])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='exile',index=self.rows['Dream Twist']['csvid'],value=2),c['steps'])
 def test_flashback_replays_use_only_real_observable_atomic_boundaries(self):
  p=f.spec(self.d('Deep Analysis'),self.o['Deep Analysis']);self.assertNotIn('pending_snapshot_0',f.variants(p))
  for variant in ('ready_snapshot_1','resolved_snapshot_1'):
   steps=self.cases('Deep Analysis',variant)[0]['steps'];self.assertEqual(steps[-1]['op'],'assert_recorded_state');self.assertTrue(all(x['player']==1 for x in steps if x['op']=='choose_player'))
 def test_flashback_mandatory_mill_and_look_components_are_complete(self):
  steps=self.cases('Chill of Foreboding')[0]['steps']
  for who in (0,1):self.assertIn(dict(op='assert_zone',player=who,zone='graveyard',value=9),steps)
  steps=self.cases('Forbidden Alchemy')[0]['steps'];self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=5),steps);self.assertIn(dict(op='assert_zone',player=0,zone='graveyard',value=7),steps);self.assertIn(dict(op='assert_zone',player=0,zone='library',value=3),steps)
 def test_flashback_full_source_grammar_and_identity_fail_closed(self):
  for key,value in [('oracle_text','Target player draws three cards.'),('mana_cost','{2}{U}'),('color_identity',[]),('keywords',[])]:
   r=copy.deepcopy(self.o['Deep Analysis']);r[key]=value
   with self.assertRaises(ValueError):f.spec(self.d('Deep Analysis'),r)

 def test_delve_spell_models_complete_printed_draw_and_disposition(self):
  for owner in (0,1):
   steps=self.cases('Treasure Cruise',f'effect_{owner}')[0]['steps'];self.assertIn(dict(op='assert_zone',player=owner,zone='hand',value=7),steps);self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=4),steps)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Treasure Cruise']['csvid'],value=1),steps);self.assertFalse(any(x.get('ref')=='source' and x['op']=='assert_card' for x in steps))
 def test_targeted_delve_queues_actual_payment_before_creature_selection(self):
  for name in ('Death Rattle','Murderous Cut','Become Immense'):
   steps=self.cases(name,'effect_1')[0]['steps'];target=next(i for i,x in enumerate(steps) if x['op']=='choose_target');cost=[i for i,x in enumerate(steps) if x['op']=='choose_zone_card'];self.assertTrue(cost);self.assertTrue(all(i<target for i in cost));self.assertLess(target,next(i for i,x in enumerate(steps) if x['op']=='cast'))
 def test_delve_nongreen_predicate_and_no_targets_deny_real_cast(self):
  for name,variant in [('Death Rattle','wrong_type_0'),('Murderous Cut','no_targets_1'),('Become Immense','no_targets_0')]:
   steps=self.cases(name,variant)[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=0),steps);self.assertFalse(any(x['op']=='choose_target' for x in steps))
 def test_delve_pump_positive_and_cleanup_have_distinct_printed_statistics(self):
  for variant,power in [('effect_0',8),('cleanup_0',2)]:
   steps=self.cases('Become Immense',variant)[0]['steps'];self.assertIn(dict(op='assert_card',ref='target_1',property='power',value=power),steps);self.assertIn(dict(op='assert_card',ref='target_1',property='toughness',value=power),steps)
 def test_targeted_delve_real_response_preserves_cost_and_removes_only_target(self):
  for name in ('Death Rattle','Murderous Cut','Become Immense'):
   for owner in (0,1):
    steps=self.cases(name,f'lost_target_{owner}')[0]['steps'];self.assertIn(dict(op='cast_response',ref='response',value=1),steps);self.assertIn(dict(op='resolve_response'),steps);self.assertFalse(any(x['op']=='resolve' for x in steps))
    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[name]['csvid'],value=1),steps);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Unsummon']['csvid'],value=1),steps)
 def test_delve_legendary_component_cannot_accidentally_cast_second_copy(self):
  p=f.spec(self.d('Tasigur, the Golden Fang'),self.o['Tasigur, the Golden Fang']);self.assertNotIn('repeat_0',f.variants(p));self.assertTrue(p['additional_components'])
  p=f.spec(self.d('Necropolis Fiend'),self.o['Necropolis Fiend']);self.assertIn('repeat_1',f.variants(p));self.assertTrue(p['additional_components'])
 def test_delve_spell_whole_effect_and_parameters_fail_closed(self):
  d=self.d('Murderous Cut');d['params']['spell_effect']='draw_three'
  with self.assertRaises(ValueError):self.cases('Murderous Cut',d=d)
  r=copy.deepcopy(self.o['Treasure Cruise']);r['oracle_text']='Draw two cards.'
  with self.assertRaises(ValueError):f.spec(self.d('Treasure Cruise'),r)
 def test_teachings_instant_or_flash_selection_and_independent_predicate_receipts(self):
  for variant,target in [('effect_0','Lightning Bolt'),('alternate_1','Ashcoat Bear')]:
   c=self.cases('Mystical Teachings',variant)[0];steps=c['steps'];self.assertIn(dict(op='choose_zone_card',player=int(variant[-1]),value=self.rows[target]['csvid']),steps)
   self.assertIn(dict(op='assert_offered_legal_card',index=self.rows[target]['csvid'],value=1),steps);self.assertIn(dict(op='assert_offered_legal_card',index=self.rows['Grizzly Bears']['csvid'],value=0),steps);self.assertIn(dict(op='assert_offered_selection_required',value=0),steps)
 def test_typed_teachings_may_legally_fail_and_rejects_wrong_library_types(self):
  for variant in ('decline_search_0','wrong_type_library_1'):
   steps=self.cases('Mystical Teachings',variant)[0]['steps'];self.assertTrue(any(x['op']=='choose_optional_zone' for x in steps));self.assertFalse(any(x['op']=='choose_zone_card' for x in steps));self.assertIn(dict(op='assert_zone',player=int(variant[-1]),zone='hand',value=4),steps)
 def test_ambition_finds_one_normally_two_from_grave_and_observes_required_any_search(self):
  for variant,count in [('effect_0',2),('cast_source_1',1),('one_card_library_0',1)]:
   steps=self.cases('Increasing Ambition',variant)[0]['steps'];self.assertEqual(len([x for x in steps if x['op']=='choose_zone_card']),count);self.assertIn(dict(op='assert_offered_selection_required',value=1),steps)
  steps=self.cases('Increasing Ambition','empty_library_0')[0]['steps'];self.assertFalse(any(x['op']=='assert_offered_selection_required' for x in steps))
 def test_grave_shuffle_zero_targets_still_targets_player_and_resolves_legally(self):
  for name in ('Krosan Reclamation',"Memory's Journey"):
   steps=self.cases(name,'zero_targets_1')[0]['steps'];self.assertIn(dict(op='choose_player',player=1,value=1),steps);self.assertIn(dict(op='cast_graveyard',ref='source',value=1),steps);self.assertFalse(any(x['op']=='choose_zone_card' for x in steps))
   self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows[name]['csvid'],value=1),steps)
 def test_grave_shuffle_duplicate_physical_copies_and_target_owner_are_independent(self):
  for name,count in [('Krosan Reclamation',2),("Memory's Journey",3)]:
   steps=self.cases(name,'duplicate_targets_0')[0]['steps'];self.assertEqual(len([x for x in steps if x['op']=='choose_zone_card' and x['value']==self.rows['Grizzly Bears']['csvid']]),count)
   steps=self.cases(name,'opponent_target_1')[0]['steps'];self.assertIn(dict(op='choose_player',player=1,value=0),steps);self.assertIn(dict(op='assert_zone',player=0,zone='library',value=7+count),steps)
 def test_grave_removing_effects_keep_second_real_grave_ref_below_every_removed_card(self):
  for name in ('Krosan Reclamation',"Memory's Journey",'Creeping Renaissance'):
   c=self.cases(name,'repeat_1')[0];grave=[x for x in c['setup'] if x['player']==1 and x['zone']=='graveyard'];self.assertEqual(grave[0]['ref'],'source2');self.assertEqual(grave[-1]['ref'],'source')
   self.assertEqual([x['ref'] for x in c['steps'] if x['op']=='cast_graveyard'],['source','source2'])
 def test_renaissance_permanent_type_overlap_and_original_owner_hand(self):
  for variant,names in [('effect_0',['Grizzly Bears','Ornithopter','Phyrexian Hulk']),('type_artifact_1',['Sol Ring','Ornithopter','Phyrexian Hulk','Darksteel Citadel']),('type_land_0',['Forest','Darksteel Citadel']),('type_planeswalker_1',['Jace Beleren'])]:
   steps=self.cases('Creeping Renaissance',variant)[0]['steps'];who=int(variant[-1])
   for name in names:self.assertIn(dict(op='assert_zone_multiplicity',player=who,zone='graveyard',index=self.rows[name]['csvid'],value=0),steps)
   self.assertTrue(self.d('Creeping Renaissance')['params']['additional_components'])
 def test_new_library_witnesses_are_full_pinned_without_expanding_old_universe(self):
  for name in ('Ashcoat Bear','Jace Beleren'):
   self.assertIn(name,f.PINS['extra_cards']);self.assertNotIn(name,f.PINS['cards']);changed=copy.deepcopy(self.o);changed[name]['oracle_text']='different'
   with self.assertRaises(ValueError):f.validate_witnesses(changed,self.rows)

 def test_connoisseur_uses_real_hand_identity_only_for_actual_offered_trigger(self):
  for owner in (0,1):
   c=self.cases('Corpse Connoisseur',f'effect_{owner}')[0];carrier=next(x for x in c['setup'] if x['ref']=='trigger_identity');self.assertEqual((carrier['name'],carrier['zone'],carrier['player']),('Corpse Connoisseur','hand',owner))
   self.assertFalse(any(x['op'] in ('cast','activate') and x.get('ref')=='trigger_identity' for x in c['steps']))
   self.assertEqual(any(x['op']=='choose_trigger' and x['ref']=='trigger_identity' for x in c['steps']),owner==0)
   self.assertFalse(any(x['op']=='assert_card' and x.get('ref')=='source' for x in c['steps']))
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Corpse Connoisseur']['csvid'],value=1),c['steps'])
 def test_connoisseur_search_is_real_typed_library_to_grave_with_conservation(self):
  for owner in (0,1):
   steps=self.cases('Corpse Connoisseur',f'effect_{owner}')[0]['steps'];self.assertIn(dict(op='choose_zone_card',player=owner,value=self.rows['Grizzly Bears']['csvid']),steps)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2),steps);self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=6),steps)
   self.assertIn(dict(op='assert_offered_legal_card',index=self.rows['Grizzly Bears']['csvid'],value=1),steps);self.assertIn(dict(op='assert_offered_legal_card',index=self.rows['Forest']['csvid'],value=0),steps)
 def test_connoisseur_decline_still_chooses_actual_trigger_and_may_fail_find(self):
  steps=self.cases('Corpse Connoisseur','decline_tutor_0')[0]['steps'];self.assertIn(dict(op='choose_trigger',player=0,ref='trigger_identity'),steps);self.assertTrue(any(x['op']=='choose_optional_zone' for x in steps));self.assertFalse(any(x['op']=='choose_zone_card' for x in steps))
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=7),steps);self.assertIn(dict(op='assert_offered_selection_required',value=0),steps)
 def test_blink_exercises_real_original_owner_return_and_tap_reset(self):
  for owner in (0,1):
   c=self.cases('Momentary Blink',f'owned_opponent_{owner}')[0];obj=next(x for x in c['setup'] if x['ref']=='target');self.assertEqual(obj['player'],owner);self.assertIn('opponent_owned',obj['state']);self.assertIn('tapped',obj['state'])
   self.assertIn(dict(op='bind_permanent',player=1-owner,value=self.rows['Grizzly Bears']['csvid'],ref='blink_returned'),c['steps']);self.assertIn(dict(op='assert_card',ref='blink_returned',property='tapped',value=0),c['steps']);self.assertIn(dict(op='assert_card',ref='blink_returned',property='sick',value=1),c['steps'])
 def test_blink_new_object_removes_actual_temporary_growth_and_can_die_normally(self):
  steps=self.cases('Momentary Blink','preexisting_pump_1')[0]['steps'];self.assertIn(dict(op='cast',ref='p1_hand_1',value=1),steps);self.assertIn(dict(op='assert_card',ref='blink_returned',property='power',value=2),steps)
  steps=self.cases('Momentary Blink','destroy_returned_0')[0]['steps'];self.assertIn(dict(op='destroy',ref='blink_returned'),steps);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2),steps)
 def test_blink_control_and_creature_predicates_use_real_cast_denials(self):
  for variant in ('wrong_controller_0','wrong_type_1','no_targets_0'):
   steps=self.cases('Momentary Blink',variant)[0]['steps'];self.assertIn(dict(op='require_can_cast_graveyard',ref='source',value=0),steps);self.assertIn(dict(op='cast_graveyard',ref='source',value=0),steps);self.assertFalse(any(x['op']=='choose_target' for x in steps))
 def test_artful_dodge_blocking_and_cleanup_are_functional(self):
  for owner in (0,1):
   steps=self.cases('Artful Dodge',f'effect_{owner}')[0]['steps'];self.assertIn(dict(op='block',ref='blocker0',other='attacker',value=0),steps);self.assertIn(dict(op='assert_life',player=1-owner,value=18),steps)
   steps=self.cases('Artful Dodge',f'cleanup_{owner}')[0]['steps'];self.assertIn(dict(op='block',ref='blocker0',other='attacker',value=1),steps);self.assertIn(dict(op='assert_life',player=1-owner,value=20),steps)
 def test_nightbird_zero_one_two_distinct_targets_have_real_blocking_effects(self):
  for variant,count in [('zero_targets_0',0),('one_target_0',1),('effect_0',2)]:
   steps=self.cases("Nightbird's Clutches",variant)[0]['steps'];self.assertEqual(len([x for x in steps if x['op']=='choose_target']),count)
   if count<2:self.assertIn(dict(op='choose_cancel_target',player=0),steps)
   self.assertIn(dict(op='block',ref='blocker0',other='attacker',value=int(count==0)),steps)
   if count==1:self.assertIn(dict(op='block',ref='blocker1',other='attacker',value=1),steps)
 def test_sever_exiles_all_matching_creatures_by_original_owner_only(self):
  for owner in (0,1):
   steps=self.cases('Sever the Bloodline',f'owned_opponent_copy_{owner}')[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=3),steps)
   for who in (0,1):self.assertIn(dict(op='assert_zone',player=who,zone='battlefield',value=1),steps)
   self.assertIn(dict(op='assert_card',ref='control0',property='in_play',value=1),steps)
 def test_board_flashback_has_only_true_atomic_or_ordinary_hand_boundaries(self):
  for name in ('Momentary Blink','Artful Dodge',"Nightbird's Clutches",'Sever the Bloodline'):
   steps=self.cases(name,'effect_1')[0]['steps'];cast=next(i for i,x in enumerate(steps) if x['op']=='cast_graveyard');self.assertEqual(steps[cast+1],dict(op='assert_stack',value=0));self.assertFalse(any(x['op']=='assert_card' and x.get('ref')=='source' for x in steps))
   for mode in ('ready_snapshot_1','resolved_snapshot_1'):self.assertEqual(self.cases(name,mode)[0]['steps'][-1]['op'],'assert_recorded_state')
 def test_teachings_is_instant_or_flash_and_explicitly_excludes_sorcery(self):
  steps=self.cases('Mystical Teachings','predicate_library_1')[0]['steps']
  for name,value in [('Lightning Bolt',1),('Ashcoat Bear',1),('Divination',0),('Grizzly Bears',0),('Forest',0),('Sol Ring',0)]:self.assertIn(dict(op='assert_offered_legal_card',index=self.rows[name]['csvid'],value=value),steps)

 def test_repeated_delve_spell_can_exile_prior_resolved_copy_before_second_resolution(self):
  for name in ('Become Immense','Murderous Cut','Death Rattle','Treasure Cruise'):
   for owner in (0,1):
    steps=self.cases(name,f'repeat_{owner}')[0]['steps'];casts=[i for i,x in enumerate(steps) if x['op']=='cast']
    self.assertEqual(len(casts),2)
    for number,start in enumerate(casts):
     end=next(i for i in range(start+1,len(steps)) if steps[i]['op']=='resolve')
     receipts=[x for x in steps[start:end] if x['op']=='assert_zone_multiplicity' and x.get('zone')=='exile' and x.get('index')==self.rows[name]['csvid']]
     self.assertTrue(receipts);self.assertTrue(all(x['value']==number for x in receipts))

class GraveyardExileWave7Test(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def d(self,name,variant='effect_0'):
  return dict(family='graveyard_exile',version=2,params=copy.deepcopy(f.WAVE7_PROFILES[name]['params']),oracle_text=self.o[name]['oracle_text'],variants=[variant])
 def cases(self,name,variant='effect_0',d=None):
  d=d or self.d(name,variant);p=f.spec(d,self.o[name]);return f.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.o)
 def test_wave7_requires_complete_identity_and_exact_component_params(self):
  for name in f.WAVE7_PROFILES:
   for key,value in [('oracle_text','Exile each graveyard.'),('mana_cost','{7}'),('oracle_id','other'),('layout','transform')]:
    r=copy.deepcopy(self.o[name]);r[key]=value
    with self.assertRaises(ValueError):f.spec(self.d(name),r)
   for key,value in [('schema',7.0),('draw',1),('quantity','one'),('graveyard','all'),('battlefield_controls',1)]:
    d=self.d(name);d['params'][key]=value
    with self.assertRaises(ValueError):f.spec(d,self.o[name])
 def test_wave7_grammar_rejects_mutually_consistent_bad_source_pins(self):
  for name in f.WAVE7_PROFILES:
   source=copy.deepcopy(self.o[name]);source['oracle_text']+=' Draw a card.'
   with self.assertRaises(ValueError):f._validate_wave7_profile(source,copy.deepcopy(f.WAVE7_PROFILES[name]['params']))
 def test_free_crypt_cast_is_not_denied_for_zero_mana(self):
  d=self.d("Tormod's Crypt");p=f.spec(d,self.o["Tormod's Crypt"])
  self.assertNotIn('insufficient_cast_mana_0',f.variants(p))
  for owner in (0,1):
   steps=self.cases("Tormod's Crypt",f'cast_payment_{owner}')[0]['steps']
   self.assertIn(dict(op='cast',ref='source',value=1),steps)
   self.assertIn(dict(op='assert_mana_total',player=owner,value=0),steps)
 def test_crypt_sacrifice_cost_precedes_own_graveyard_exile(self):
  csvid=self.rows["Tormod's Crypt"]['csvid']
  for owner in (0,1):
   steps=self.cases("Tormod's Crypt",f'own_target_{owner}')[0]['steps'];resolve=next(i for i,s in enumerate(steps) if s['op']=='resolve')
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=csvid,value=1),steps[:resolve])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=csvid,value=0),steps[resolve:])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='exile',index=csvid,value=1),steps[resolve:])
 def test_modal_selection_is_explicit_for_both_controllers(self):
  for owner in (0,1):
   steps=self.cases('Rakdos Charm',f'effect_{owner}')[0]['steps']
   self.assertIn(dict(op='present_ai_dialogs',value=1),steps)
   self.assertIn(dict(op='choose_dialog',player=owner,value=0),steps)
   self.assertIn(dict(op='choose_player',player=owner,value=1-owner),steps)
   self.assertIn(dict(op='assert_choices_consumed'),steps)
 def test_entry_and_modal_effects_keep_battlefield_controls(self):
  for name in f.WAVE7_PROFILES:
   steps=self.cases(name)[0]['steps']
   for who in (0,1):
    for index in range(4):self.assertIn(dict(op='assert_card',ref=f'zone_control_{who}_{index}',property='in_play',value=1),steps)
 def test_empty_player_graveyard_remains_a_legal_target(self):
  for name in f.WAVE7_PROFILES:
   for owner in (0,1):
    steps=self.cases(name,f'empty_graveyards_{owner}')[0]['steps']
    self.assertIn(dict(op='choose_player',player=owner,value=1-owner),steps)
    self.assertIn(dict(op='activate' if name=="Tormod's Crypt" else 'cast',ref='source',value=1),steps)
 def test_constellation_scope_avoids_unmodeled_second_source_triggers(self):
  d=self.d('Agent of Erebos');p=f.spec(d,self.o['Agent of Erebos'])
  self.assertNotIn('repeat_0',f.variants(p))
  self.assertTrue(any('other enchantments' in gap for gap in p['additional_components']))
 def test_trap_component_checks_printed_cost_without_claiming_alternative_cost(self):
  for owner in (0,1):
   steps=self.cases('Ravenous Trap',f'payment_{owner}')[0]['steps']
   self.assertIn(dict(op='set_mana',player=owner,color=0,value=2),steps)
   self.assertIn(dict(op='set_mana',player=owner,color=1,value=2),steps)
   self.assertIn(dict(op='assert_mana_total',player=owner,value=0),steps)
   self.assertFalse(any(s['op'] in ('destroy','advance_turn') for s in steps))
  self.assertTrue(any('alternative cost' in gap for gap in f.WAVE7_PROFILES['Ravenous Trap']['params']['additional_components']))
 def test_trap_printed_cost_is_an_independent_grammar_requirement(self):
  source=copy.deepcopy(self.o['Ravenous Trap']);source['mana_cost']='{0}'
  with self.assertRaises(ValueError):f._validate_wave7_profile(source,copy.deepcopy(f.WAVE7_PROFILES['Ravenous Trap']['params']))

if __name__=='__main__':unittest.main()
