import copy
import json
from pathlib import Path
import unittest
from unittest.mock import patch

from attack_value_trigger_recipes import classify,expand,spec,validate_witnesses,variants
from generate_cases import registry


class AttackValueTriggerRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
  cls.rows={row['name']:row for row in registry()}

 def declaration(self,name,selected=None):
  return dict(family='attack_value_trigger',version=2,oracle_text=self.oracle[name]['oracle_text'],params=classify(self.oracle[name]),variants=selected or ['effect_0'])

 def cases(self,name,selected=None,oracle=None,d=None):
  o=self.oracle if oracle is None else oracle;declaration=self.declaration(name,selected) if d is None else d
  return expand(name,self.rows[name]['csvid'],declaration,spec(declaration,o[name]),declaration['variants'],self.rows,o)

 def assertion(self,ref,key,value):return dict(op='assert_card',ref=ref,property=key,value=value)

 def test_general_wording_is_derived_without_source_name_enums(self):
  expected={'Infectious Horror':('self','life_loss',2),'Hellrider':('own','damage',1),'Nemesis of Reason':('self','mill',10),'Riddlekeeper':('defense','mill',2),'Accorder Paladin':('self','pump',0),'Master of Diversion':('self','tap',0)}
  for name,(trigger,effect,amount) in expected.items():
   with self.subTest(name=name):
    p=classify(self.oracle[name]);self.assertEqual((p['trigger'],p['effect'],p['amount']),(trigger,effect,amount))
  r=dict(self.oracle['Infectious Horror'],name='Unlisted Test Creature',oracle_text='Whenever Unlisted Test Creature attacks, defending player loses 3 life.')
  self.assertEqual(classify(r)['amount'],3)

 def test_complete_source_and_extra_clauses_are_validated(self):
  name='Infectious Horror';d=self.declaration(name)
  for fields in (dict(layout='transform'),dict(card_faces=[{}]),dict(selected_face='front'),dict(type_line='Token Creature — Horror'),dict(oracle_text=self.oracle[name]['oracle_text']+'\nDraw a card.')):
   r=dict(self.oracle[name],**fields);bad=dict(d,oracle_text=r['oracle_text'])
   with self.subTest(fields=fields),self.assertRaises(ValueError):spec(bad,r)
  with self.assertRaises(ValueError):spec(dict(d,oracle_text='Whenever this creature attacks, each opponent loses 3 life.'),self.oracle[name])

 def test_exact_json_types_and_complete_parameters_fail_closed(self):
  name='Infectious Horror';d=self.declaration(name)
  for key,value in (('version',2.0),('version',True),('family','attack_pump_creature')):
   with self.subTest(key=key,value=value),self.assertRaises(ValueError):spec(dict(d,**{key:value}),self.oracle[name])
  for key,value in (('amount',2.0),('power',False),('other',0),('trigger','own'),('keywords',()),('unknown',1)):
   bad=copy.deepcopy(d);bad['params'][key]=value
   with self.subTest(key=key,value=value),self.assertRaises(ValueError):spec(bad,self.oracle[name])

 def test_source_ui_pin_and_witnesses_are_independent_rules_inputs(self):
  r=dict(self.oracle['Riddlekeeper'],mana_cost='{U}')
  with self.assertRaises(ValueError):spec(self.declaration('Riddlekeeper'),r)
  for name,key,value in (('Safe Passage','oracle_text','Prevent all damage.'),('Grizzly Bears','power','3'),('Forest','type_line','Land')):
   o=dict(self.oracle,**{name:dict(self.oracle[name],**{key:value})})
   with self.subTest(name=name),self.assertRaises(ValueError):validate_witnesses(o,self.rows)

 def test_subtype_witness_cannot_deck_players_during_real_battlefield_entry(self):
  import attack_value_trigger_recipes as module
  pins=module._pins();bad=copy.deepcopy(pins)
  bad['witnesses']['Kavu Climber']=dict(self.oracle['Kavu Climber'])
  bad['subtype_witnesses']['Kavu']='Kavu Climber'
  with patch.object(module,'_pins',return_value=bad),self.assertRaisesRegex(ValueError,'trigger-free ordinary combat'):
   validate_witnesses(self.oracle,self.rows)
  for owner in (0,1):
   for value in ('effect','bare_zones'):
    case=self.cases('Kavu Mauler',[f'{value}_{owner}'])[0]
    matching=[x for x in case['setup'] if x['ref'] in ('ally','enemy')]
    self.assertEqual([x['name'] for x in matching],['Kavu Chameleon']*2)
    self.assertEqual(self.oracle['Kavu Chameleon']['oracle_text'],"This spell can't be countered.\n{G}: This creature becomes the color of your choice until end of turn.")
    for player in (0,1):self.assertIn(dict(op='assert_life',player=player,value=20),case['steps'])
    self.assertNotIn('activate',[x['op'] for x in case['steps']])

 def test_real_complete_declarations_and_pre_damage_attack_effects_for_both_players(self):
  for owner in (0,1):
   case=self.cases('Infectious Horror',[f'effect_{owner}'])[0];steps=case['steps']
   attack=steps.index(dict(op='attack',ref='source',value=1));finish=steps.index(dict(op='finish_attackers'))
   self.assertLess(attack,finish);self.assertNotIn('combat_damage',[x['op'] for x in steps])
   self.assertIn(dict(op='assert_life',player=1-owner,value=20),steps[:finish])
   self.assertIn(dict(op='assert_life',player=1-owner,value=18),steps[finish:])
   self.assertEqual(next(x['player'] for x in case['setup'] if x['ref']=='source'),owner)

 def test_trigger_scope_differentiates_self_other_and_opposing_attackers(self):
  for name,pair,other,opposing in (('Infectious Horror',18,20,20),('Hellrider',18,19,20)):
   cases=self.cases(name,['attack_pair_0','other_attack_0','opponent_attack_0'])
   for case,total in zip(cases,(pair,other,opposing)):
    self.assertIn(dict(op='assert_life',player=1,value=total),case['steps'])
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=2),self.cases('Nemesis of Reason')[0]['steps'])
  defensive=self.cases('Riddlekeeper',['attack_pair_0'])[0]
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=8),defensive['steps'])
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=12),defensive['steps'])

 def test_battle_cry_must_pump_real_allies_but_never_source_or_idle_control(self):
  case=self.cases('Accorder Paladin')[0];steps=case['steps'];finish=steps.index(dict(op='finish_attackers'))
  self.assertIn(dict(op='attack',ref='ally',value=1),steps[:finish])
  self.assertIn(self.assertion('ally','power',3),steps[finish:])
  self.assertIn(self.assertion('source','power',3),steps[finish:])
  self.assertIn(self.assertion('idle','power',3),steps[finish:])
  pair=self.cases('Accorder Paladin',['attack_pair_0'])[0]
  self.assertIn(self.assertion('idle','power',4),pair['steps'])
  doubled=self.cases('Accorder Paladin',['two_sources_0'])[0]
  self.assertIn(self.assertion('source','power',4),doubled['steps']);self.assertIn(self.assertion('copy','power',4),doubled['steps'])
  self.assertIn(self.assertion('ally','power',4),doubled['steps'])

 def test_life_loss_and_damage_have_a_real_prevention_contrast(self):
  loss=self.cases('Infectious Horror',['prevention_control_0'])[0]['steps']
  damage=self.cases('Hellrider',['prevention_control_0'])[0]['steps']
  for steps in (loss,damage):self.assertIn(dict(op='cast',ref='shield',value=1),steps)
  self.assertIn(dict(op='assert_life',player=1,value=18),loss)
  self.assertNotIn(dict(op='assert_life',player=1,value=19),damage)
  self.assertIn(dict(op='assert_life',player=1,value=20),damage)

 def test_targeted_tap_selects_one_real_eligible_creature_and_keeps_controls(self):
  case=self.cases('Master of Diversion')[0];steps=case['steps'];finish=steps.index(dict(op='finish_attackers'))
  self.assertEqual(steps[finish-1],dict(op='choose_target',player=0,ref='enemy'))
  self.assertIn(self.assertion('enemy','tapped',1),steps[finish:])
  self.assertIn(self.assertion('enemy_idle','tapped',0),steps[finish:])
  empty=self.cases('Master of Diversion',['no_eligible_target_0'])[0]
  self.assertNotIn('choose_target',[x['op'] for x in empty['steps']])
  self.assertNotIn('enemy',[x['ref'] for x in empty['setup']])
  self.assertIn('artifact_control',[x['ref'] for x in empty['setup']])

 def test_costs_freshness_tapping_and_sickness_use_real_cast_attack_paths(self):
  for name in ('Infectious Horror','Hellrider'):
   paid,denied,fresh=self.cases(name,['payment_0','insufficient_mana_0','fresh_attack_0'])
   self.assertIn(dict(op='cast',ref='source',value=1),paid['steps'])
   self.assertIn(self.assertion('source','in_hand',0),paid['steps'])
   self.assertIn(dict(op='cast',ref='source',value=0),denied['steps'])
   self.assertIn(dict(op='attack',ref='source',value=int(name=='Hellrider')),fresh['steps'])
   for value in ('tapped','sick'):
    self.assertIn(dict(op='attack',ref='source',value=int(name=='Hellrider' and value=='sick')),self.cases(name,[value+'_0'])[0]['steps'])

 def test_mill_is_bounded_and_keeps_exact_top_card_identity_receipts(self):
  short,empty=self.cases('Nemesis of Reason',['short_library_0','empty_library_0'])
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=0),short['steps'])
  self.assertIn(dict(op='assert_zone',player=1,zone='graveyard',value=4),short['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Plains']['csvid'],value=1),short['steps'])
  self.assertIn(dict(op='assert_zone',player=1,zone='graveyard',value=1),empty['steps'])

 def test_cleanup_expires_pump_after_actual_combat_and_not_life_loss(self):
  case=self.cases('Accorder Paladin',['cleanup_0'])[0];steps=case['steps'];at=steps.index(dict(op='cleanup'))
  self.assertIn('combat_damage',[x['op'] for x in steps[:at]])
  self.assertIn(self.assertion('ally','power',3),steps[:at]);self.assertIn(self.assertion('ally','power',2),steps[at:])
  self.assertIn(dict(op='assert_life',player=1,value=14),steps[at:])
  loss=self.cases('Infectious Horror',['cleanup_0'])[0]['steps']
  self.assertIn(dict(op='assert_life',player=1,value=16),loss)

 def test_snapshots_replay_real_declaration_and_keep_distinct_step_objects(self):
  for name in ('Infectious Horror','Accorder Paladin','Master of Diversion','Riddlekeeper'):
   case=self.cases(name,['declaration_snapshot_0'])[0];steps=case['steps'];at=steps.index(dict(op='capture'))
   self.assertIn('attack',[x['op'] for x in steps[:at]])
   self.assertEqual(sum(x['op']=='finish_attackers' for x in steps),2)
   self.assertIn(dict(op='assert_recorded_state'),steps)
   self.assertEqual(len({id(x) for x in steps}),len(steps))

 def test_reviewed_mandatory_trigger_uses_presented_human_acceptance_only_when_triggered(self):
  case=self.cases('Riddlekeeper',['effect_0'])[0]
  self.assertIn(dict(op='choose_trigger',player=0,ref='source'),case['steps'])
  for value in ('other_attack','no_attack','source_absent'):
   self.assertNotIn('choose_trigger',[x['op'] for x in self.cases('Riddlekeeper',[value+'_0'])[0]['steps']])
  self.assertNotIn('choose_trigger',[x['op'] for x in self.cases('Riddlekeeper',['effect_1'])[0]['steps']])
  pair=self.cases('Riddlekeeper',['attack_pair_0'])[0]['steps']
  self.assertEqual(sum(x['op']=='choose_trigger' for x in pair),1)
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=8),pair)

 def test_graveyard_ref_is_verified_by_zones_without_live_instance_aliasing(self):
  case=self.cases('Infectious Horror',['source_absent_0'])[0]
  self.assertFalse(any(x['op'] in ('assert_card','require_card') and x.get('ref')=='source' for x in case['steps']))
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Infectious Horror']['csvid'],value=1),case['steps'])

 def test_surviving_blocker_assignment_is_observed_during_live_combat(self):
  steps=self.cases('Nemesis of Reason',['blocked_0'])[0]['steps']
  damage=steps.index(dict(op='combat_damage'));end=steps.index(dict(op='end_combat'))
  self.assertIn(self.assertion('blocker','blocking_count',1),steps[:damage])
  self.assertNotIn(self.assertion('blocker','blocking_count',0),steps[end:])
  self.assertIn(self.assertion('source','damage',4),steps[end:])
  self.assertIn(self.assertion('blocker','damage',3),steps[end:])

 def test_rejects_unknown_empty_duplicate_variants(self):
  for selected in ([],['unknown_0'],['effect_0','effect_0']):
   d=self.declaration('Infectious Horror');d['variants']=selected
   with self.assertRaises(ValueError):self.cases('Infectious Horror',d=d)

 def test_equivalent_source_reference_and_each_other_wording_are_general(self):
  pianna=classify(self.oracle['Pianna, Nomad Captain']);renegade=classify(self.oracle['Renegade Warlord'])
  self.assertEqual((pianna['effect'],pianna['power'],pianna['toughness'],pianna['other']),('pump',1,1,False))
  self.assertEqual((renegade['effect'],renegade['power'],renegade['toughness'],renegade['other']),('pump',1,0,True))
  r=dict(self.oracle['Pianna, Nomad Captain'],name='Oracle Alias, Test Legend',oracle_text='Whenever Oracle Alias attacks, attacking creatures get +1/+1 until end of turn.')
  self.assertEqual(classify(r)['effect'],'pump')
  with self.assertRaises(ValueError):
   classify(dict(r,oracle_text='Whenever Oracle Alias attacks, attacking creatures get +0/+0 until end of turn.'))

 def test_attacker_count_and_untapped_creature_count_use_different_boards(self):
  for name,expected in (('Akroan Hoplite',(2,3,3)),('Knotvine Paladin',(4,3,5))):
   for variant,power in zip(('effect_0','attack_pair_0','extra_qualifier_0'),expected):
    steps=self.cases(name,[variant])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
    self.assertIn(self.assertion('source','power',power),steps[finish:])
  steps=self.cases('Knotvine Paladin',['no_qualifiers_0'])[0]['steps']
  self.assertIn(self.assertion('source','power',2),steps)
  self.assertFalse(any(x.get('ref') in ('ally','idle') for x in self.cases('Knotvine Paladin',['no_qualifiers_0'])[0]['setup']))

 def test_land_count_requires_owned_battlefield_lands_and_ignores_stocked_zones(self):
  for variant,power in (('effect_0',9),('no_qualifiers_0',7),('extra_qualifier_0',10),('opponent_qualifier_0',9)):
   case=self.cases('Rampaging Brontodon',[variant])[0];finish=case['steps'].index(dict(op='finish_attackers'))
   self.assertIn(self.assertion('source','power',power),case['steps'][finish:])
  case=self.cases('Rampaging Brontodon',['opponent_qualifier_0'])[0]
  self.assertEqual(sum('land' in x['ref'] and x['zone']=='battlefield' and x['player']==1 for x in case['setup']),2)

 def test_other_attacking_subtypes_include_changeling_but_exclude_source_and_opponent(self):
  for name,base in (("Cenn's Heir",1),('Bull Aurochs',2)):
   for variant,bonus in (('effect_0',1),('no_qualifiers_0',0),('extra_qualifier_0',2),('opponent_qualifier_0',1)):
    case=self.cases(name,[variant])[0];finish=case['steps'].index(dict(op='finish_attackers'))
    self.assertIn(self.assertion('source','power',base+bonus),case['steps'][finish:])
   case=self.cases(name,['extra_qualifier_0'])[0]
   self.assertEqual(next(x['name'] for x in case['setup'] if x['ref']=='extra_qualifier'),'Woodland Changeling')
  with self.assertRaises(ValueError):
   o=dict(self.oracle,**{'Woodland Changeling':dict(self.oracle['Woodland Changeling'],oracle_text='This card is not every creature type.')})
   validate_witnesses(o,self.rows)

 def test_tribal_team_pump_includes_idle_owned_members_and_excludes_opponents(self):
  case=self.cases('Goblin General')[0];steps=case['steps'];finish=steps.index(dict(op='finish_attackers'))
  self.assertIn(self.assertion('ally','power',3),steps[finish:]);self.assertIn(self.assertion('idle','power',3),steps[finish:])
  self.assertIn(self.assertion('nonmatching','power',3),steps[finish:]);self.assertIn(self.assertion('enemy','power',2),steps[finish:])
  self.assertIn(self.assertion('idle','attacking',0),steps[finish:])

 def test_untap_other_retains_source_tap_and_opponent_controls(self):
  case=self.cases('Copperhorn Scout')[0];steps=case['steps'];finish=steps.index(dict(op='finish_attackers'))
  for ref,value in (('source',1),('ally',0),('idle',0),('enemy_idle',1)):
   self.assertIn(self.assertion(ref,'tapped',value),steps[finish:])
  self.assertIn(self.assertion('idle','tapped',1),steps[:finish])
  two=self.cases('Copperhorn Scout',['two_sources_0'])[0]['steps'];finish=two.index(dict(op='finish_attackers'))
  self.assertIn(self.assertion('source','tapped',0),two[finish:]);self.assertIn(self.assertion('copy','tapped',0),two[finish:])
  empty=self.cases('Copperhorn Scout',['no_qualifiers_0'])[0]
  self.assertFalse(any(x['ref'] in ('ally','idle') for x in empty['setup']))

 def test_attack_counters_are_permanent_and_stack_for_each_source(self):
  case=self.cases('Fangren Firstborn',['cleanup_0'])[0];steps=case['steps'];cleanup=steps.index(dict(op='cleanup'))
  self.assertIn(self.assertion('source','plus1_counters',1),steps[cleanup:])
  self.assertIn(self.assertion('source','power',5),steps[cleanup:]);self.assertIn(self.assertion('source','toughness',3),steps[cleanup:])
  self.assertIn(self.assertion('ally','plus1_counters',1),steps[cleanup:]);self.assertIn(self.assertion('idle','plus1_counters',0),steps[cleanup:])
  two=self.cases('Fangren Firstborn',['two_sources_0'])[0]['steps'];finish=two.index(dict(op='finish_attackers'))
  for ref in ('source','copy','ally'):self.assertIn(self.assertion(ref,'plus1_counters',2),two[finish:])

 def test_count_pump_expiry_and_trample_use_real_combat_receipts(self):
  case=self.cases('Rampaging Brontodon',['blocked_0'])[0];steps=case['steps']
  self.assertIn(dict(op='combat_damage'),steps)
  self.assertIn(dict(op='assert_life',player=1,value=15),steps)
  self.assertIn(self.assertion('source','damage',4),steps)
  case=self.cases('Knotvine Paladin',['cleanup_0'])[0];steps=case['steps'];at=steps.index(dict(op='cleanup'))
  self.assertIn(self.assertion('source','power',4),steps[:at]);self.assertIn(self.assertion('source','power',2),steps[at:])

 def test_all_owned_pump_includes_source_and_nonattacking_creatures(self):
  case=self.cases('Inspiring Unicorn')[0];steps=case['steps'];finish=steps.index(dict(op='finish_attackers'))
  for ref,power in (('source',3),('ally',3),('idle',4),('own_control',4),('enemy',2)):
   self.assertIn(self.assertion(ref,'power',power),steps[finish:])
  empty=self.cases('Inspiring Unicorn',['no_qualifiers_0'])[0]
  self.assertFalse(any(x['ref'] in ('ally','idle','own_control') for x in empty['setup']))
  self.assertIn(self.assertion('source','power',3),empty['steps'])

 def test_printed_devoid_has_real_color_and_payment_receipts(self):
  p=classify(self.oracle['Silent Skimmer']);self.assertIs(p['devoid'],True)
  self.assertEqual(p['keywords'],['Flying'])
  with self.assertRaises(ValueError):classify(dict(self.oracle['Silent Skimmer'],colors=['B']))
  case=self.cases('Silent Skimmer',['payment_1'])[0];steps=case['steps']
  self.assertIn(dict(op='set_mana',player=1,color=1,value=1),steps)
  self.assertIn(dict(op='assert_mana',player=1,color=1,value=0),steps)
  self.assertIn(dict(op='assert_matching_permanents',player=1,index=self.rows['Silent Skimmer']['csvid'],property='color',value=0),steps)
  self.assertIn(dict(op='assert_matching_permanents',player=1,index=self.rows['Hill Giant']['csvid'],property='color',value=16),steps)

 def test_evasion_has_real_ground_denial_and_flying_reach_acceptance(self):
  for name in ('Signal Pest','Silent Skimmer'):
   ground=self.cases(name,['ground_block_denied_0'])[0]
   self.assertIn(dict(op='block',ref='enemy',other='source',value=0),ground['steps'])
   flying=self.cases(name,['blocked_0'])[0]
   self.assertEqual(next(x['name'] for x in flying['setup'] if x['ref']=='blocker'),'Air Elemental')
   self.assertIn(dict(op='block',ref='blocker',other='source',value=1),flying['steps'])
   reach=self.cases(name,['reach_block_allowed_0'])[0]
   self.assertEqual(next(x['name'] for x in reach['setup'] if x['ref']=='blocker'),'Giant Spider')
   self.assertIn(dict(op='block',ref='blocker',other='source',value=1),reach['steps'])
  case=self.cases('Signal Pest')[0]
  self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows['Signal Pest']['csvid'],property='artifact',value=1),case['steps'])

 def test_mandatory_attacker_controls_are_legally_ineligible_and_gap_is_explicit(self):
  for value in ('no_attack','other_attack'):
   case=self.cases('Kuldotha Ringleader',[value+'_0'])[0]
   source=next(x for x in case['setup'] if x['ref']=='source')
   self.assertEqual(source['state'],['tapped'])
   self.assertNotIn(dict(op='attack',ref='source',value=1),case['steps'])
   self.assertIn('forced attack-declaration UI enforcement; authored declarations are legal',case['_recipe']['untested_components'])
  case=self.cases('Kuldotha Ringleader',['effect_0'])[0]
  self.assertIn(dict(op='attack',ref='source',value=1),case['steps'])
  self.assertIn(self.assertion('ally','power',3),case['steps'])

 def test_filtered_own_attacker_triggers_keep_matching_nonmatching_and_control_separate(self):
  expected=(('effect_0',19),('attack_pair_0',18),('other_attack_0',19),('nonmatching_attacker_0',20),('opponent_attack_0',20),('two_sources_0',16),('extra_qualifier_0',18))
  for value,total in expected:
   case=self.cases('Leeching Sliver',[value])[0];steps=case['steps'];finish=steps.index(dict(op='finish_attackers'))
   self.assertIn(dict(op='assert_life',player=1,value=total),steps[finish:])
  case=self.cases('Leeching Sliver',['effect_0'])[0]
  self.assertEqual(next(x['name'] for x in case['setup'] if x['ref']=='ally'),"Venser's Sliver")
  self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows["Venser's Sliver"]['csvid'],property='subtype_Sliver',value=1),case['steps'])
  self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows['Hill Giant']['csvid'],property='subtype_Sliver',value=0),case['steps'])
  case=self.cases('Leeching Sliver',['extra_qualifier_0'])[0]
  self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows['Woodland Changeling']['csvid'],property='subtype_Sliver',value=1),case['steps'])

 def test_passive_and_filtered_parameter_types_are_exact_and_extra_text_is_not_dropped(self):
  for name,key,value in (('Silent Skimmer','devoid',1),('Kuldotha Ringleader','mandatory_attack',1),('Signal Pest','evasion',True),('Leeching Sliver','subtype',[])):
   d=self.declaration(name);d['params'][key]=value
   with self.subTest(name=name,key=key),self.assertRaises(ValueError):spec(d,self.oracle[name])
  for name in ('Silent Skimmer','Signal Pest','Kuldotha Ringleader','Leeching Sliver'):
   r=dict(self.oracle[name],oracle_text=self.oracle[name]['oracle_text']+'\nDraw a card.')
   with self.subTest(name=name),self.assertRaises(ValueError):classify(r)

 def test_energy_cohort_full_entry_attack_and_passive_text_is_derived(self):
  names=['Riparian Tiger','Thriving Grubs','Thriving Ibex','Thriving Rats','Thriving Rhino','Thriving Turtle','Voltaic Brawler','Scrapper Champion']
  for name in names:
   p=classify(self.oracle[name]);self.assertEqual(p['entry_energy'],2)
   self.assertEqual(p['energy_cost'],1 if name=='Voltaic Brawler' else 2)
   self.assertEqual(p['effect'],'energy_pump' if name in ('Riparian Tiger','Voltaic Brawler') else 'energy_counter')
   for extra in ('\nWhen this creature dies, draw a card.','\nFlying'):
    with self.subTest(name=name,extra=extra),self.assertRaises(ValueError):classify(dict(self.oracle[name],oracle_text=self.oracle[name]['oracle_text']+extra))
  self.assertEqual(classify(self.oracle['Scrapper Champion'])['keywords'],['Double strike'])
  for name in ('Aetherwind Basker','Aetherstorm Roc','Glint-Sleeve Siphoner'):
   with self.subTest(name=name),self.assertRaises(ValueError):classify(self.oracle[name])

 def test_energy_parameter_types_are_exact_and_optional_decline_is_not_claimed_for_ai(self):
  for key,value in (('entry_energy',2.0),('energy_cost',True),('grants_trample',0)):
   d=self.declaration('Voltaic Brawler');d['params'][key]=value
   with self.subTest(key=key),self.assertRaises(ValueError):spec(d,self.oracle['Voltaic Brawler'])
  p=spec(self.declaration('Thriving Grubs'),self.oracle['Thriving Grubs'])
  self.assertIn('decline_0',variants(p));self.assertNotIn('decline_1',variants(p))

 def test_energy_is_obtained_by_real_paid_cast_entry_and_aged_with_actual_turns(self):
  for name in ('Riparian Tiger','Thriving Grubs','Thriving Turtle','Voltaic Brawler','Scrapper Champion'):
   for owner in (0,1):
    case=self.cases(name,[f'effect_{owner}'])[0];steps=case['steps']
    self.assertEqual(next(x['zone'] for x in case['setup'] if x['ref']=='source'),'hand')
    self.assertFalse(any(x.get('state') for x in case['setup']))
    cast=steps.index(dict(op='cast',ref='source',value=1));resolve=steps.index(dict(op='resolve'));attack=steps.index(dict(op='attack',ref='source',value=1))
    self.assertLess(cast,resolve);self.assertLess(resolve,attack)
    self.assertIn(dict(op='assert_energy',player=owner,value=0),steps[:cast])
    self.assertIn(dict(op='assert_energy',player=owner,value=2),steps[resolve:attack])
    turns=[i for i,x in enumerate(steps[:attack]) if x['op']=='advance_turn'];self.assertEqual(len(turns),2)
    for i,pl in zip(turns,(1-owner,owner)):self.assertEqual(steps[i+1],dict(op='untap_step',player=pl))
    self.assertIn(self.assertion('source','sick',1),steps[resolve:attack]);self.assertIn(self.assertion('source','sick',0),steps[resolve:attack])
    self.assertFalse(any(x['op']=='set_energy' for x in steps))

 def test_energy_accept_decline_and_fresh_or_other_declarations_keep_real_resource(self):
  paid=self.cases('Thriving Grubs',['effect_0'])[0]['steps'];finish=paid.index(dict(op='finish_attackers'))
  self.assertEqual(paid[finish-1],dict(op='choose_trigger_source',player=0,ref='source'))
  self.assertIn(dict(op='assert_energy',player=0,value=0),paid[finish:])
  for value in ('decline_0','fresh_attack_0','other_attack_0','no_attack_0','opponent_attack_0'):
   steps=self.cases('Thriving Grubs',[value])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
   self.assertNotIn('choose_trigger_source',[x['op'] for x in steps])
   self.assertIn(dict(op='assert_energy',player=0,value=2),steps[finish:]);self.assertIn(self.assertion('source','plus1_counters',0),steps[finish:])
  steps=self.cases('Thriving Grubs',['fresh_attack_0'])[0]['steps'];self.assertIn(dict(op='attack',ref='source',value=0),steps)
  for value in ('insufficient_mana_0','wrong_color_0'):
   steps=self.cases('Voltaic Brawler',[value])[0]['steps']
   self.assertIn(dict(op='cast',ref='source',value=0),steps);self.assertNotIn('resolve',[x['op'] for x in steps])
   self.assertTrue(all(x['value']==0 for x in steps if x['op']=='assert_energy'))

 def test_energy_counter_permanence_and_temporary_pump_expiry_have_cleanup_receipts(self):
  for name,power,toughness,counters in (('Thriving Grubs',3,2,1),('Thriving Turtle',1,4,1),('Riparian Tiger',4,4,0),('Voltaic Brawler',3,2,0)):
   steps=self.cases(name,['cleanup_0'])[0]['steps'];cleanup=steps.index(dict(op='cleanup'))
   self.assertIn(self.assertion('source','power',power),steps[cleanup:]);self.assertIn(self.assertion('source','toughness',toughness),steps[cleanup:])
   self.assertIn(self.assertion('source','plus1_counters',counters),steps[cleanup:])
  steps=self.cases('Thriving Grubs',['depleted_0'])[0]['steps'];last=max(i for i,x in enumerate(steps) if x['op']=='finish_attackers')
  self.assertEqual(sum(x['op']=='choose_trigger_source' for x in steps),1)
  self.assertIn(self.assertion('source','plus1_counters',1),steps[last:]);self.assertIn(dict(op='assert_energy',player=0,value=0),steps[last:])

 def test_energy_trample_and_its_expiry_use_real_block_damage_and_grave_receipts(self):
  for name,life in (('Riparian Tiger',16),('Voltaic Brawler',18)):
   steps=self.cases(name,['blocked_0'])[0]['steps']
   self.assertIn(dict(op='block',ref='enemy',other='source',value=1),steps)
   self.assertIn(dict(op='assert_life',player=1,value=life),steps)
   self.assertIn(self.assertion('source','damage',2),steps)
  steps=self.cases('Voltaic Brawler',['trample_expiry_0'])[0]['steps'];last=max(i for i,x in enumerate(steps) if x['op']=='combat_damage')
  self.assertEqual(sum(x['op']=='choose_trigger_source' for x in steps),2)
  self.assertIn(dict(op='assert_life',player=1,value=12),steps[last:])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Voltaic Brawler']['csvid'],value=1),steps[last:])

 def test_scrapper_double_strike_is_observed_in_unblocked_and_first_strike_combat(self):
  for owner in (0,1):
   steps=self.cases('Scrapper Champion',[f'unblocked_{owner}'])[0]['steps'];damage=steps.index(dict(op='combat_damage'))
   self.assertIn(dict(op='assert_life',player=1-owner,value=14),steps[damage:])
   steps=self.cases('Scrapper Champion',[f'blocked_{owner}'])[0]['steps'];damage=steps.index(dict(op='combat_damage'))
   self.assertIn(self.assertion('source','damage',0),steps[damage:]);self.assertIn(dict(op='assert_life',player=1-owner,value=20),steps[damage:])
   self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Hill Giant']['csvid'],value=1),steps[damage:])

 def test_energy_duplicate_sources_share_pool_and_replay_has_no_ambiguous_card_choice(self):
  steps=self.cases('Thriving Grubs',['two_sources_0'])[0]['steps']
  self.assertEqual([x['ref'] for x in steps if x['op']=='choose_trigger_source'],['source','copy'])
  self.assertIn(dict(op='assert_energy',player=0,value=4),steps)
  last=steps.index(dict(op='finish_attackers'));self.assertIn(dict(op='assert_energy',player=0,value=0),steps[last:])
  for ref in ('source','copy'):self.assertIn(self.assertion(ref,'plus1_counters',1),steps[last:])
  for value in ('ready_snapshot_0','declaration_snapshot_0','resolved_snapshot_0'):
   steps=self.cases('Riparian Tiger',[value])[0]['steps']
   self.assertIn(dict(op='restore'),steps);self.assertIn(dict(op='assert_recorded_state'),steps)
   self.assertEqual(len(steps),len({id(x) for x in steps}))

 def test_energy_survives_source_removal_without_inspecting_grave_index_as_instance(self):
  for value in ('source_removed_before_0','source_destroyed_after_0'):
   steps=self.cases('Thriving Grubs',[value])[0]['steps'];destroy=steps.index(dict(op='destroy',ref='source'))
   self.assertFalse(any(x['op']=='assert_card' and x['ref']=='source' for x in steps[destroy:]))
   self.assertIn(dict(op='assert_energy',player=0,value=2 if value.startswith('source_removed_before') else 0),steps[destroy:])

 def test_energy_recharge_is_a_second_actual_entry_and_preserves_first_sources_counters(self):
  steps=self.cases('Thriving Grubs',['repeat_0'])[0]['steps']
  self.assertEqual([x['ref'] for x in steps if x['op']=='cast'],['source','copy'])
  self.assertEqual(sum(x['op']=='advance_turn' for x in steps),4)
  last=max(i for i,x in enumerate(steps) if x['op']=='finish_attackers')
  self.assertIn(self.assertion('source','plus1_counters',2),steps[last:]);self.assertIn(self.assertion('copy','plus1_counters',0),steps[last:])
  self.assertIn(dict(op='assert_energy',player=0,value=0),steps[last:])

 def test_tapped_energy_source_uses_paid_twiddle_and_retains_unspent_energy(self):
  for owner in (0,1):
   steps=self.cases('Thriving Turtle',[f'tapped_{owner}'])[0]['steps']
   self.assertIn(dict(op='cast',ref='tap_spell',value=1),steps)
   self.assertIn(dict(op='choose_dialog',player=owner,value=0),steps)
   self.assertIn(dict(op='attack',ref='source',value=0),steps)
   finish=steps.index(dict(op='finish_attackers'))
   self.assertIn(dict(op='assert_energy',player=owner,value=2),steps[finish:])
   self.assertNotIn('choose_trigger_source',[x['op'] for x in steps])
  o=dict(self.oracle,Twiddle=dict(self.oracle['Twiddle'],mana_cost='{2}{U}'))
  with self.assertRaises(ValueError):validate_witnesses(o,self.rows)

 def test_all_energy_cases_keep_real_aging_and_restore_main_phase_after_untap(self):
  count=0;boundaries=0
  for name in ('Riparian Tiger','Thriving Grubs','Thriving Turtle','Voltaic Brawler','Scrapper Champion'):
   p=spec(self.declaration(name),self.oracle[name])
   for case in self.cases(name,variants(p)):
    count+=1;phase='main1';current=0;saved=None
    steps=case['steps']
    for i,x in enumerate(steps):
     op=x['op']
     if op=='advance_turn':current=1-current;phase='main1'
     elif op=='untap_step':
      boundaries+=1;self.assertEqual(steps[i-1]['op'],'advance_turn')
      self.assertEqual(x['player'],current)
      self.assertEqual(steps[i+1],dict(op='turn',player=current,value=0))
      phase='untap'
     elif op=='turn':current=x['player'];phase='main1'
     elif op=='begin_attackers':
      with self.subTest(case=case['name'],step=i):self.assertEqual(phase,'main1')
      phase='attackers'
     elif op=='finish_attackers':phase='before_blocking'
     elif op=='begin_blockers':phase='blockers'
     elif op=='advance_combat':phase='after_blocking'
     elif op=='combat_damage':phase='normal_damage'
     elif op=='end_combat':phase='main2'
     elif op=='cleanup':phase='cleanup'
     elif op=='capture':saved=(phase,current)
     elif op=='restore':self.assertIsNotNone(saved);phase,current=saved
  self.assertEqual(count,222);self.assertGreater(boundaries,350)

 def test_exact_thriving_followons_reuse_counter_model_with_their_printed_combat_stats(self):
  for name,power,toughness in (('Thriving Ibex',3,5),('Thriving Rats',2,3),('Thriving Rhino',3,4)):
   self.assertEqual(classify(self.oracle[name]),classify(self.oracle['Thriving Grubs']))
   for owner in (0,1):
    for value in ('cleanup','blocked','unblocked'):
     case=self.cases(name,[f'{value}_{owner}'])[0];steps=case['steps'];finish=steps.index(dict(op='finish_attackers'))
     self.assertIn(self.assertion('source','power',power),steps[finish:]);self.assertIn(self.assertion('source','toughness',toughness),steps[finish:])
     self.assertIn(self.assertion('source','plus1_counters',1),steps[finish:]);self.assertIn(dict(op='assert_energy',player=owner,value=0),steps[finish:])
     damage=steps.index(dict(op='combat_damage'))
     self.assertIn(dict(op='assert_life',player=1-owner,value=20 if value=='blocked' else 20-power),steps[damage:])
     if value=='blocked':self.assertIn(self.assertion('source','damage',2),steps[damage:])
     if value=='cleanup':
      cleanup=steps.index(dict(op='cleanup'));self.assertIn(self.assertion('source','power',power),steps[cleanup:]);self.assertIn(self.assertion('source','plus1_counters',1),steps[cleanup:])

 def test_resource_six_complete_oracle_clauses_and_exact_target_scope_are_derived(self):
  for name,effect,cost in (('Aether Herder','energy_servo',2),('Aether Inspector','energy_servo',2),('Aether Swooper','energy_servo',2),("Consul's Shieldguard",'energy_target',1),('Eddytrail Hawk','energy_target',1),('Maulfist Doorbuster','energy_target',1)):
   p=classify(self.oracle[name]);self.assertEqual((p['effect'],p['energy_cost'],p['entry_energy']),(effect,cost,2))
   with self.subTest(name=name),self.assertRaises(ValueError):classify(dict(self.oracle[name],oracle_text=self.oracle[name]['oracle_text']+'\nDraw a card.'))
  self.assertEqual(classify(self.oracle['Maulfist Doorbuster'])['target_scope'],'creature')
  self.assertEqual(classify(self.oracle['Eddytrail Hawk'])['target_scope'],'other_attacking')
  d=self.declaration('Eddytrail Hawk');d['params']['target_effect']=1
  with self.assertRaises(ValueError):spec(d,self.oracle['Eddytrail Hawk'])
  clone=dict(self.oracle['Aether Herder'],name='Another Unlisted Artificer')
  self.assertEqual(classify(clone),classify(self.oracle['Aether Herder']))

 def test_servo_witness_is_independently_pinned_token_not_a_normal_primary(self):
  for fields in (dict(power='2'),dict(colors=['W']),dict(type_line='Token Creature — Servo'),dict(layout='normal'),dict(selected_face='front')):
   o=dict(self.oracle,Servo=dict(self.oracle['Servo'],**fields))
   with self.subTest(fields=fields),self.assertRaises(ValueError):self.cases('Aether Herder',oracle=o)
  self.assertEqual(self.oracle['Servo']['type_line'],'Token Artifact Creature — Servo')
  self.assertEqual(self.rows['Servo']['csvid'],18816)

 def test_created_servo_has_real_type_color_body_and_nonattacking_fresh_entry(self):
  for name in ('Aether Herder','Aether Inspector','Aether Swooper'):
   for owner in (0,1):
    case=self.cases(name,[f'effect_{owner}'])[0];steps=case['steps'];finish=steps.index(dict(op='finish_attackers'))
    self.assertNotIn('Servo',[c['name'] for c in case['setup']])
    self.assertIn(dict(op='bind_permanent_copy',player=owner,value=18816,index=0,ref='servo_0'),steps[finish:])
    for prop,value in (('power',1),('toughness',1),('sick',1),('tapped',0),('attacking',0)):
     self.assertIn(self.assertion('servo_0',prop,value),steps[finish:])
    for prop,value in (('token',1),('artifact',1),('color',0),('subtype_Servo',1)):
     self.assertIn(dict(op='assert_matching_permanents',player=owner,index=18816,property=prop,value=value),steps[finish:])
    self.assertIn(dict(op='assert_energy',player=owner,value=0),steps[finish:])
    self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='battlefield',index=18816,value=0),steps[finish:])

 def test_servo_real_later_attack_and_fresh_opposing_turn_block_do_not_inherit_source_keywords(self):
  for owner in (0,1):
   steps=self.cases('Aether Inspector',[f'token_ready_attack_{owner}'])[0]['steps']
   attack=steps.index(dict(op='attack',ref='servo_0',value=1))
   self.assertEqual(sum(x['op']=='advance_turn' for x in steps[:attack]),4)
   self.assertIn(self.assertion('source','tapped',0),steps[:attack]);self.assertIn(self.assertion('servo_0','tapped',1),steps[attack:])
   self.assertIn(dict(op='block',ref='enemy',other='servo_0',value=1),steps[attack:])
   steps=self.cases('Aether Swooper',[f'token_block_{owner}'])[0]['steps'];block=steps.index(dict(op='block',ref='servo_0',other='enemy',value=1))
   self.assertIn(self.assertion('servo_0','sick',1),steps[:block])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=18816,value=0),steps[block:])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=18816,value=0),steps[block:])
   steps=self.cases('Aether Swooper',[f'token_ready_attack_{owner}'])[0]['steps']
   self.assertIn(dict(op='block',ref='enemy',other='servo_0',value=1),steps)

 def test_servo_removal_ceases_token_and_keeps_normal_destination_ledgers(self):
  for value,operation,ref in (('token_destroy','destroy','servo_0'),('token_bounce','cast','bounce'),('token_exile','cast','exile_spell')):
   steps=self.cases('Aether Herder',[value+'_0'])[0]['steps'];remove=next(i for i,x in enumerate(steps) if x['op']==operation and x['ref']==ref)
   last=max(i for i,x in enumerate(steps) if x['op']=='assert_zone' and x['zone']=='battlefield' and x['player']==0)
   self.assertFalse(any(x['op']=='assert_card' and x['ref']=='servo_0' for x in steps[last:]))
   for zone in ('hand','graveyard','exile'):
    self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone=zone,index=18816,value=0),steps[remove:])
   if value=='token_exile':self.assertIn(dict(op='assert_life',player=0,value=21),steps[remove:])

 def test_no_legal_other_attacker_preserves_oracle_energy_and_measures_human_offer(self):
  for name in ("Consul's Shieldguard",'Eddytrail Hawk'):
   for owner in (0,1):
    steps=self.cases(name,[f'no_eligible_target_{owner}'])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
    self.assertEqual([x['ref'] for x in steps if x['op']=='attack'],['source'])
    self.assertNotIn('choose_target',[x['op'] for x in steps])
    self.assertIn(dict(op='assert_energy',player=owner,value=2),steps[finish:])
    if owner==0:self.assertEqual(steps[finish+1],dict(op='assert_unused_choices',value=1))
    else:self.assertNotIn('choose_trigger_source',[x['op'] for x in steps])

 def test_targeted_flying_grant_is_real_block_legality_and_expires_on_actual_turns(self):
  for owner in (0,1):
   steps=self.cases('Eddytrail Hawk',[f'effect_blocked_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_target',player=owner,ref='ally'),steps)
   self.assertIn(dict(op='block',ref='enemy',other='ally',value=0),steps)
   steps=self.cases('Eddytrail Hawk',[f'expiry_{owner}'])[0]['steps']
   self.assertIn(dict(op='block',ref='enemy',other='ally',value=0),steps)
   self.assertIn(dict(op='block',ref='enemy',other='ally',value=1),steps)
   self.assertEqual(sum(x['op']=='advance_turn' for x in steps),4)

 def test_targeted_indestructible_survives_lethal_then_loses_protection_before_paid_bolt(self):
  for owner in (0,1):
   steps=self.cases("Consul's Shieldguard",[f'expiry_{owner}'])[0]['steps']
   combat=steps.index(dict(op='combat_damage'));bolt=steps.index(dict(op='cast',ref='expiry_bolt',value=1))
   self.assertIn(self.assertion('ally','damage',3),steps[combat:bolt]);self.assertIn(self.assertion('ally','in_play',1),steps[combat:bolt])
   self.assertIn(dict(op='advance_turn'),steps[combat:bolt])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=1),steps[bolt:])
   self.assertIn(dict(op='assert_energy',player=owner,value=1),steps[bolt:])

 def test_cannot_block_target_allows_own_creature_and_expiry_does_not_hide_behind_new_payment(self):
  for owner in (0,1):
   steps=self.cases('Maulfist Doorbuster',[f'effect_blocked_{owner}'])[0]['steps']
   self.assertIn(dict(op='block',ref='enemy',other='source',value=0),steps)
   self.assertIn(dict(op='block',ref='enemy_idle',other='source',value=1),steps)
   steps=self.cases('Maulfist Doorbuster',[f'target_own_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_target',player=owner,ref='ally'),steps)
   self.assertIn(dict(op='block',ref='enemy',other='source',value=1),steps)
   steps=self.cases('Maulfist Doorbuster',[f'expiry_{owner}'])[0]['steps']
   self.assertEqual([x['ref'] for x in steps if x['op']=='choose_target'],['enemy','enemy_idle'])
   self.assertIn(dict(op='block',ref='enemy',other='source',value=0),steps)
   self.assertIn(dict(op='block',ref='enemy',other='source',value=1),steps)

 def test_token_target_duplicate_sources_and_replay_keep_individual_bindings(self):
  steps=self.cases('Aether Herder',['two_sources_0'])[0]['steps']
  self.assertEqual([x['ref'] for x in steps if x['op']=='choose_trigger_source'],['source','copy'])
  self.assertEqual([x['index'] for x in steps if x['op']=='bind_permanent_copy'],[0,1])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=18816,value=2),steps)
  for name in ('Aether Herder','Eddytrail Hawk'):
   for value in ('ready_snapshot_0','declaration_snapshot_0','resolved_snapshot_0'):
    steps=self.cases(name,[value])[0]['steps'];self.assertIn(dict(op='restore'),steps);self.assertIn(dict(op='assert_recorded_state'),steps)
    self.assertEqual(len(steps),len({id(x) for x in steps}))

 def test_resource_six_all_variants_use_paid_entry_and_real_untap_main_boundaries(self):
  total=0
  for name in ('Aether Herder','Aether Inspector','Aether Swooper',"Consul's Shieldguard",'Eddytrail Hawk','Maulfist Doorbuster'):
   p=spec(self.declaration(name),self.oracle[name])
   for case in self.cases(name,variants(p)):
    total+=1;steps=case['steps'];source=next(c for c in case['setup'] if c['ref']=='source')
    self.assertEqual(source['zone'],'hand');self.assertFalse(any(x.get('state') for x in case['setup']))
    self.assertNotIn('set_energy',[x['op'] for x in steps])
    for i,x in enumerate(steps):
     if x['op']=='untap_step':self.assertEqual(steps[i-1]['op'],'advance_turn');self.assertEqual(steps[i+1],dict(op='turn',player=x['player'],value=0))
  self.assertGreater(total,300)

 def test_paid_target_full_oracle_cost_scope_and_devoid_are_derived(self):
  for name,symbol,effect,scope in (('Frenzied Goblin','R','cannot_block','creature'),('Haazda Snare Squad','W','tap','opponent_creature'),('Gravity Negator','C','flying','another_creature')):
   p=classify(self.oracle[name]);self.assertEqual((p['payment_symbol'],p['target_effect'],p['target_scope']),(symbol,effect,scope))
   self.assertEqual(p['requires_colorless'],symbol=='C')
   with self.subTest(name=name),self.assertRaises(ValueError):classify(dict(self.oracle[name],oracle_text=self.oracle[name]['oracle_text']+'\nDraw a card.'))
  self.assertIs(classify(self.oracle['Gravity Negator'])['devoid'],True)
  with self.assertRaises(ValueError):classify(dict(self.oracle['Gravity Negator'],colors=['U']))
  for key,value in (('payment_amount',1.0),('requires_colorless',1),('payment_symbol',None)):
   d=self.declaration('Gravity Negator');d['params'][key]=value
   with self.subTest(key=key),self.assertRaises(ValueError):spec(d,self.oracle['Gravity Negator'])

 def test_resolving_payment_presentation_is_pinned_separately_from_oracle_rules(self):
  import attack_value_trigger_recipes as module
  for name in ('Frenzied Goblin','Haazda Snare Squad','Gravity Negator'):
   with self.assertRaises(ValueError):spec(self.declaration(name),dict(self.oracle[name],mana_cost='{9}'))
   bad=module._pins();bad['source_ui'][name]['cancellation_players']=[1]
   with patch.object(module,'_pins',return_value=bad),self.assertRaises(ValueError):spec(self.declaration(name),self.oracle[name])

 def test_paid_attack_uses_real_casts_and_age_but_never_grants_energy(self):
  for name in ('Frenzied Goblin','Haazda Snare Squad','Gravity Negator'):
   for owner in (0,1):
    case=self.cases(name,[f'effect_{owner}'])[0];steps=case['steps'];cast=steps.index(dict(op='cast',ref='source',value=1));attack=steps.index(dict(op='attack',ref='source',value=1))
    self.assertEqual(next(c['zone'] for c in case['setup'] if c['ref']=='source'),'hand')
    self.assertEqual(sum(x['op']=='advance_turn' for x in steps[cast:attack]),2)
    self.assertTrue(all(x['value']==0 for x in steps if x['op']=='assert_energy'))
    for i,x in enumerate(steps):
     if x['op']=='untap_step':self.assertEqual(steps[i-1]['op'],'advance_turn');self.assertEqual(steps[i+1],dict(op='turn',player=x['player'],value=0))

 def test_r_and_w_attack_payments_spend_exact_pool_and_keep_excess(self):
  for name,color in (('Frenzied Goblin',4),('Haazda Snare Squad',5)):
   for owner in (0,1):
    steps=self.cases(name,[f'effect_{owner}'])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
    self.assertIn(dict(op='set_mana',player=owner,color=color,value=1),steps[:finish])
    self.assertIn(dict(op='choose_target',player=owner,ref='enemy'),steps[:finish])
    self.assertIn(dict(op='assert_mana',player=owner,color=color,value=0),steps[finish:])
    steps=self.cases(name,[f'payment_extra_{owner}'])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
    self.assertIn(dict(op='assert_mana',player=owner,color=color,value=1),steps[finish:])

 def test_actual_human_cancellation_has_real_untapped_mana_source_and_no_fake_ai_choice(self):
  for name,witness in (('Frenzied Goblin','Mountain'),('Haazda Snare Squad','Plains'),('Gravity Negator','Sol Ring')):
   case=self.cases(name,['payment_cancel_0'])[0];steps=case['steps'];source=next(c for c in case['setup'] if c['ref']=='payment_source')
   self.assertEqual((source['name'],source['zone'],source['player']),(witness,'battlefield',0))
   self.assertIn(dict(op='require_can_activate',ref='payment_source',value=1),steps)
   cancel=steps.index(dict(op='choose_cancel_mana',player=0));finish=steps.index(dict(op='finish_attackers'))
   self.assertLess(cancel,finish);self.assertNotIn('choose_target',[x['op'] for x in steps])
   self.assertIn(self.assertion('payment_source','tapped',0),steps[finish:])
   p=spec(self.declaration(name),self.oracle[name]);self.assertNotIn('payment_cancel_1',variants(p))

 def test_insufficient_and_wrong_colored_payment_preserve_pool_and_no_effect(self):
  for name in ('Frenzied Goblin','Haazda Snare Squad'):
   for owner in (0,1):
    for value in ('insufficient_payment','wrong_payment_color'):
     steps=self.cases(name,[f'{value}_{owner}'])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
     self.assertNotIn('choose_target',[x['op'] for x in steps])
     if name=='Frenzied Goblin':self.assertNotIn('choose_cancel_mana',[x['op'] for x in steps])
     elif owner==0:self.assertIn(dict(op='choose_cancel_mana',player=owner),steps)
     if value=='wrong_payment_color':self.assertIn(dict(op='assert_mana',player=owner,color=5 if name=='Frenzied Goblin' else 4,value=1),steps[finish:])

 def test_true_c_keeps_colored_mana_but_accepts_outer_trigger_and_selection_input(self):
  for owner in (0,1):
   steps=self.cases('Gravity Negator',[f'colored_instead_C_{owner}'])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
   self.assertIn(dict(op='assert_mana',player=owner,color=4,value=1),steps[finish:])
   self.assertNotIn('choose_target',[x['op'] for x in steps])
   if owner==0:
    self.assertIn(dict(op='choose_mana',player=owner,color=4),steps)
    self.assertIn(dict(op='choose_trigger_source',player=owner,ref='source'),steps)
    self.assertEqual(steps[finish+1],dict(op='assert_choices_consumed'))
   else:self.assertNotIn('choose_mana',[x['op'] for x in steps])
   paid=self.cases('Gravity Negator',[f'effect_{owner}'])[0]['steps'];finish=paid.index(dict(op='finish_attackers'))
   self.assertIn(dict(op='assert_mana',player=owner,color=0,value=0),paid[finish:])
   self.assertIn(dict(op='assert_matching_permanents',player=owner,index=15607,property='color',value=0),paid)

 def test_gravity_legal_opposing_recipient_gets_flying_despite_own_preference(self):
  for owner in (0,1):
   steps=self.cases('Gravity Negator',[f'opponent_recipient_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_target',player=owner,ref='enemy'),steps)
   self.assertIn(dict(op='block',ref='enemy',other='source',value=1),steps)
   self.assertIn(self.assertion('source','damage',2),steps)
   steps=self.cases('Gravity Negator',[f'idle_recipient_{owner}'])[0]['steps']
   self.assertEqual([x['ref'] for x in steps if x['op']=='attack'],['source'])
   self.assertIn(dict(op='choose_target',player=owner,ref='ally'),steps)

 def test_paid_recipient_expiry_distinguishes_temporary_grants_from_tap_until_untap(self):
  for owner in (0,1):
   for name,ref,other in (('Frenzied Goblin','enemy','source'),('Gravity Negator','enemy','ally')):
    steps=self.cases(name,[f'expiry_{owner}'])[0]['steps']
    self.assertIn(dict(op='block',ref=ref,other=other,value=0),steps)
    self.assertIn(dict(op='block',ref=ref,other=other,value=1),steps)
   steps=self.cases('Haazda Snare Squad',[f'expiry_{owner}'])[0]['steps'];cleanup=steps.index(dict(op='cleanup'));turn=next(i for i in range(cleanup+1,len(steps)) if steps[i]['op']=='advance_turn')
   self.assertIn(self.assertion('enemy','tapped',1),steps[cleanup:turn])
   self.assertIn(self.assertion('enemy','tapped',0),steps[turn:])
   self.assertIn(dict(op='attack',ref='enemy',value=1),steps[turn:])

 def test_paid_creature_permissions_include_frenzied_self_and_own_targets(self):
  for owner in (0,1):
   for value,target in (('own_recipient','ally'),('self_recipient','source')):
    steps=self.cases('Frenzied Goblin',[f'{value}_{owner}'])[0]['steps']
    self.assertIn(dict(op='choose_target',player=owner,ref=target),steps)
    self.assertIn(dict(op='block',ref='enemy',other='source',value=1),steps)
   steps=self.cases('Gravity Negator',[f'no_eligible_target_{owner}'])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
   self.assertNotIn('choose_target',[x['op'] for x in steps])
   self.assertIn(dict(op='assert_mana',player=owner,color=0,value=1),steps[finish:])
   if owner==0:self.assertEqual(steps[finish+1],dict(op='assert_unused_choices',value=1))

 def test_paid_all_variants_replay_without_mutating_cast_sickness_or_turn_history(self):
  count=0
  for name in ('Frenzied Goblin','Haazda Snare Squad','Gravity Negator'):
   p=spec(self.declaration(name),self.oracle[name])
   for case in self.cases(name,variants(p)):
    count+=1;self.assertFalse(any(c.get('state') for c in case['setup']))
    steps=case['steps'];self.assertEqual(len(steps),len({id(x) for x in steps}))
    for i,x in enumerate(steps):
     if x['op']=='untap_step':self.assertEqual(steps[i-1]['op'],'advance_turn');self.assertEqual(steps[i+1],dict(op='turn',player=x['player'],value=0))
    if 'snapshot' in case['_recipe']['variant']:self.assertIn(dict(op='restore'),steps);self.assertIn(dict(op='assert_recorded_state'),steps)
  self.assertEqual(count,160)


if __name__=='__main__':unittest.main()
