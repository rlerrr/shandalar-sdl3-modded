import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from token_spell_extension_recipes import creation,spec,variants,validate_witnesses,expand,INACTIVE,SOURCE_KEYWORDS

class TokenSpellExtensionTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Acorn Harvest'):
  r=self.oracle[name];effect=next(l for l in r['oracle_text'].splitlines() if l not in INACTIVE[name]);token,gain,tapped=creation(effect)
  return dict(family='token_spell_extension',version=2,params=dict(token=token,inactive_clauses=INACTIVE[name],gain_life=gain,tapped=tapped,delayed_cleanup=None,convoke_no_creatures='Convoke' in SOURCE_KEYWORDS[name]),oracle_text=r['oracle_text'],variants=['create_0'])
 def cases(self,name='Acorn Harvest',selected=None,d=None,o=None,rows=None):
  d=d or self.declaration(name);s=spec(d,self.oracle[name]);return expand(name,self.rows[name]['csvid'],d,s,selected or d['variants'],rows or self.rows,o or self.oracle)
 def has(self,c,op,**fields):self.assertIn(dict(op=op,**fields),c['steps'])
 def test_whole_source_pin_and_exact_schema(self):
  for change in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(self.declaration()['params'],other=True))):
   with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),**change))
 def test_creation_characteristics_cannot_come_from_catalog(self):
  for fields in ({'power':2},{'count':True},{'artifact':1},{'colors':['B']},{'subtypes':['Spirit']},{'name':'Spirit'}):
   d=self.declaration();d['params']['token'].update(fields)
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_normal_whole_spell_required(self):
  for change in (dict(layout='transform'),dict(selected_face='front'),dict(card_faces=[{}]),dict(type_line='Creature — Elf')):
   with self.assertRaises(ValueError):spec(self.declaration(),dict(self.oracle['Acorn Harvest'],**change))
 def test_inactive_clauses_are_exact_and_named_gaps(self):
  c=self.cases()[0];self.assertIn('inactive: '+INACTIVE['Acorn Harvest'][0],c['_recipe']['untested_components'])
  d=self.declaration();d['params']['inactive_clauses']=[]
  with self.assertRaises(ValueError):self.cases(d=d)
 def test_new_additional_rules_clause_rejects(self):
  r=dict(self.oracle['Acorn Harvest']);r['oracle_text']+='\nDraw a card.';d=self.declaration();d['oracle_text']=r['oracle_text']
  with self.assertRaises(ValueError):spec(d,r)
 def test_explicit_unique_variants_and_witnesses_guarded(self):
  for selected in (['bad_0'],['create_0','create_0']):
   with self.assertRaises(ValueError):self.cases(selected=selected)
  o=copy.deepcopy(self.oracle);o['Grizzly Bears']['colors']=['R']
  with self.assertRaises(ValueError):self.cases(o=o)
 def test_real_cast_and_both_controller_receipts(self):
  for c in self.cases(selected=['create_0','create_1']):
   owner=int(c['_recipe']['variant'][-1]);self.has(c,'cast',ref='source',value=1);self.has(c,'assert_stack',value=1);self.has(c,'assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Squirrel']['csvid'],value=2)
 def test_fixed_life_gain_and_two_casts_are_not_gaps(self):
  for c,expected in zip(self.cases('Spectral Reserves',['create_0','two_casts_1']),(22,24)):
   self.has(c,'assert_life',player=int(c['_recipe']['variant'][-1]),value=expected)
 def test_life_component_mismatch_rejects(self):
  d=self.declaration('Spectral Reserves');d['params']['gain_life']=3
  with self.assertRaises(ValueError):self.cases('Spectral Reserves',d=d)
 def test_normal_cost_convoke_taps_all_controlled_creatures_before_cast(self):
  c=self.cases('Scatter the Seeds')[0];self.assertEqual(next(c for c in c['setup'] if c['ref']=='unselected')['state'],['tapped']);self.has(c,'assert_card',ref='unselected',property='tapped',value=1);self.assertNotIn('two_casts_0',variants(spec(self.declaration('Scatter the Seeds'),self.oracle['Scatter the Seeds'])))
 def test_positive_effect_precedes_cleanup(self):
  c=self.cases(selected=['cleanup_0'])[0];steps=c['steps'];self.assertTrue(any(s['op']=='assert_matching_permanents' for s in steps[:steps.index(dict(op='cleanup'))]))
 def test_token_catalog_and_handler_names_are_not_expected_rules(self):
  o=copy.deepcopy(self.oracle);o['Squirrel'].update(power='99',toughness='99',colors=['B'],oracle_text='Flying');rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy'
  self.assertEqual(self.cases(),self.cases(o=o,rows=rows))
 def test_snapshots_replay_positive_checks_and_life(self):
  for c in self.cases('Spectral Reserves',['ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0']):
   self.has(c,'ai_roundtrip');self.has(c,'assert_recorded_state');steps=c['steps'];tail=steps[steps.index(dict(op='restore'))+1:];self.assertTrue(any(s['op']=='assert_matching_permanents' for s in tail));self.assertIn(dict(op='assert_life',player=int(c['_recipe']['variant'][-1]),value=22),tail)
 def test_all_reviewed_source_profiles_expand_public_variants(self):
  for name in INACTIVE:
   d=self.declaration(name);p=spec(d,self.oracle[name]);cases=self.cases(name,variants(p));self.assertTrue(cases);self.assertTrue(all(c['_recipe']['family']=='token_spell_extension' for c in cases))
 def test_reviewed_source_metadata_cannot_change_even_with_new_text_pin(self):
  r=dict(self.oracle['Beast Attack'],mana_cost='{1}{G}');d=self.declaration('Beast Attack')
  with self.assertRaises(ValueError):spec(d,r)
 def test_tapped_army_tokens_and_bounded_copy_bindings(self):
  d=self.declaration('Army of the Damned');p=spec(d,self.oracle['Army of the Damned']);self.assertFalse(any(v.startswith(('two_casts_','ready_attack_','fresh_attack_')) for v in variants(p)))
  c=self.cases('Army of the Damned')[0];self.has(c,'bind_permanent_copy',player=0,value=self.rows['Zombie']['csvid'],index=12,ref='expected_token_12');self.has(c,'assert_card',ref='expected_token_12',property='tapped',value=1)
 def test_first_spell_storm_profiles_omit_repeated_casts(self):
  for name in ('Empty the Warrens','Hunting Pack'):
   p=spec(self.declaration(name),self.oracle[name]);self.assertNotIn('two_casts_0',variants(p));self.assertTrue(any('first spell' in gap for gap in self.cases(name)[0]['_recipe']['untested_components']))
 def test_splice_profile_has_no_other_arcane_card_in_hand(self):
  p=spec(self.declaration('Spiritual Visit'),self.oracle['Spiritual Visit']);self.assertNotIn('two_casts_0',variants(p))
 def test_high_generic_cost_source_omits_two_cast_fixture(self):
  p=spec(self.declaration('Crush of Wurms'),self.oracle['Crush of Wurms']);self.assertNotIn('two_casts_0',variants(p));self.has(self.cases('Crush of Wurms')[0],'assert_matching_permanents',player=0,index=self.rows['Wurm']['csvid'],property='power',value=6)
 def test_threshold_and_fateful_replacements_are_named_gaps(self):
  for name,word in (('Grizzly Fate','threshold'),('Gather the Townsfolk','fateful-hour')):self.assertTrue(any(word in gap for gap in self.cases(name)[0]['_recipe']['untested_components']))
 def test_new_observer_profiles_assert_every_printed_subtype(self):
  expected={'Call of the Herd':['Elephant'],"Cenn's Enlistment":['Kithkin','Soldier'],'Reach of Branches':['Treefolk','Shaman'],"Sosuke's Summons":['Snake'],'Summon the School':['Merfolk','Wizard']}
  for name,subtypes in expected.items():
   c=self.cases(name)[0];tid=self.rows[' '.join(subtypes)]['csvid']
   for subtype in subtypes:self.has(c,'assert_matching_permanents',player=0,index=tid,property='subtype_'+subtype,value=1)
 def test_elephant_and_snake_characteristics_come_from_creator_text(self):
  c=self.cases('Call of the Herd')[0];self.has(c,'assert_matching_permanents',player=0,index=self.rows['Elephant']['csvid'],property='power',value=3)
  o=copy.deepcopy(self.oracle);o['Elephant'].update(power='99',toughness='99');self.assertEqual(c,self.cases('Call of the Herd',o=o)[0])
  p=spec(self.declaration("Sosuke's Summons"),self.oracle["Sosuke's Summons"]);self.assertEqual(p['token']['keywords'],[])
 def test_declined_resources_do_not_gain_life(self):
  for c in self.cases('Spectral Reserves',['insufficient_mana_0','wrong_color_1','countered_0']):self.has(c,'assert_life',player=int(c['_recipe']['variant'][-1]),value=20)

class NoncreatureAmountTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):
  from token_trigger_creation_spec import noncreature_data,noncreature_params
  p=noncreature_data()[name];return dict(family='token_spell_extension',version=2,params=noncreature_params(p),oracle_text=self.oracle[name]['oracle_text'],variants=['create_0'])
 def cases(self,name,selected=None,d=None,o=None,rows=None):
  d=d or self.declaration(name);p=spec(d,self.oracle[name]);return expand(name,self.rows[name]['csvid'],d,p,selected or d['variants'],rows or self.rows,o or self.oracle)
 def count(self,name,variant):
  c=self.cases(name,[variant])[0];token=self.declaration(name)['params']['token']['name'];return [a['value'] for a in c['steps'] if a['op']=='assert_zone_multiplicity' and a['player']==int(variant[-1]) and a['zone']=='battlefield' and a['index']==self.rows[token]['csvid']][-1]
 def has(self,c,op,**fields):self.assertIn(dict(op=op,**fields),c['steps'])
 def test_whole_sources_and_exact_typed_costs(self):
  for name in ('Spider Spawning','Whirlermaker','Centaur Glade'):
   for key,value in [('entry_tapped',1),('fixed_count',True),('inactive_clauses',['unreviewed'])]:
    d=self.declaration(name);d['params'][key]=value
    with self.assertRaises(ValueError):self.cases(name,d=d)
  d=self.declaration('Whirlermaker');d['params']['activation']['tap']=1
  with self.assertRaises(ValueError):self.cases('Whirlermaker',d=d)
 def test_token_body_and_count_basis_are_independently_parsed(self):
  from token_trigger_creation_spec import noncreature_data,noncreature_spec
  name='Spider Spawning';data=noncreature_data();data[name]['token']['keywords']=[];d=self.declaration(name);d['params']['token']['keywords']=[]
  with self.assertRaises(ValueError):noncreature_spec(d,self.oracle[name],data)
  name='Rise from the Tides';data=noncreature_data();data[name]['basis']='creature_cards_in_own_graveyard';d=self.declaration(name);d['params']['basis']=data[name]['basis']
  with self.assertRaises(ValueError):noncreature_spec(d,self.oracle[name],data)
 def test_source_spell_excluded_until_after_creation_but_included_on_recast(self):
  self.assertEqual(self.count('Rise from the Tides','minimum_0'),0);self.assertEqual(self.count('Rise from the Tides','create_0'),2);self.assertEqual(self.count('Rise from the Tides','two_casts_1'),5)
  self.assertEqual(self.count('Saproling Symbiosis','two_casts_0'),6);self.assertEqual(self.count('Spider Spawning','two_casts_1'),4)
 def test_real_response_changes_resolution_time_count(self):
  for name,expected in [('Spider Spawning',3),('Rise from the Tides',3),('Saproling Symbiosis',5)]:
   for owner in (0,1):
    c=self.cases(name,['response_count_'+str(owner)])[0];self.has(c,'cast_response',ref='response',value=1);self.has(c,'assert_stack',value=2);self.has(c,'resolve_response');self.assertEqual(self.count(name,'response_count_'+str(owner)),expected)
 def test_foreign_ownership_and_opposing_inputs(self):
  for name,expected in [('Spider Spawning',3),('Rise from the Tides',2),('Saproling Symbiosis',1)]:self.assertEqual(self.count(name,'ownership_0'),expected);self.assertEqual(self.count(name,'opponent_state_1'),0)
 def test_tokens_contribute_only_to_live_creature_count(self):
  for name,expected in [('Spider Spawning',2),('Rise from the Tides',4),('Saproling Symbiosis',4)]:self.assertEqual(self.count(name,'token_input_0'),expected)
 def test_source_cast_and_activation_costs_have_separate_receipts(self):
  for name in ('Centaur Glade','Dragon Roost','Whirlermaker'):
   self.assertEqual(self.count(name,'payment_0'),0);self.assertEqual(self.count(name,'cast_only_1'),0)
   c=self.cases(name,['activation_payment_1'])[0];self.has(c,'cast',ref='source',value=1);self.has(c,'activate',ref='source',value=1)
   for color in range(6):self.has(c,'assert_mana',player=1,color=color,value=0)
 def test_insufficient_activation_and_tap_costs_are_not_spells(self):
  for name in ('Centaur Glade','Dragon Roost','Whirlermaker'):
   c=self.cases(name,['activation_insufficient_0'])[0];self.has(c,'cast',ref='source',value=1);self.has(c,'activate',ref='source',value=0);self.assertEqual(self.count(name,'activation_insufficient_0'),0)
  c=self.cases('Whirlermaker',['tapped_denied_0'])[0];self.has(c,'assert_card',ref='source',property='tapped',value=1);self.has(c,'activate',ref='source',value=0)
 def test_tapped_tokens_cannot_block_then_real_untap_enables_block(self):
  name='Rise from the Tides';c=self.cases(name,['block_ground_0'])[0];self.has(c,'block',ref='test_token',other='combat_witness',value=0);self.has(c,'assert_life',player=0,value=18)
  c=self.cases(name,['untap_block_ground_0'])[0];self.has(c,'untap_step',player=0);self.has(c,'block',ref='test_token',other='combat_witness',value=1)
 def test_untap_has_zero_both_pools_and_no_intervening_turn_setter(self):
  for name,variant in [('Rise from the Tides','untap_snapshot_1'),('Whirlermaker','untap_repeat_0')]:
   c=self.cases(name,[variant])[0];s=c['steps'];idx=next(i for i,a in enumerate(s) if a['op']=='untap_step');self.assertEqual(s[idx-1],dict(op='advance_turn'));self.assertEqual(s[idx-2],dict(op='advance_turn'));self.assertEqual(s[idx-14:idx-2],[dict(op='set_mana',player=player,color=color,value=0) for player in (0,1) for color in range(6)])
  self.assertEqual(self.count('Whirlermaker','untap_repeat_0'),2)
 def test_reach_and_flying_have_actual_legal_blockers(self):
  for name in ('Spider Spawning','Dragon Roost','Whirlermaker'):
   c=self.cases(name,['block_flying_1'])[0];self.has(c,'block',ref='test_token',other='combat_witness',value=1);self.has(c,'combat_damage')
  c=self.cases('Saproling Symbiosis',['block_flying_1'])[0];self.has(c,'block',ref='test_token',other='combat_witness',value=0)
 def test_printed_flash_surcharge_is_inactive_but_normal_ui_is_explicit(self):
  c=self.cases('Saproling Symbiosis')[0];self.has(c,'choose_dialog',player=0,value=0);self.assertTrue(any('inactive: You may cast' in g for g in c['_recipe']['untested_components']));c=self.cases('Saproling Symbiosis',['payment_0'])[0];self.assertFalse(any(a['op']=='choose_dialog' for a in c['steps']))
 def test_source_removed_pending_keeps_activated_effect(self):
  for name in ('Centaur Glade','Dragon Roost','Whirlermaker'):
   c=self.cases(name,['source_removed_pending_0'])[0];s=c['steps'];act=next(i for i,a in enumerate(s) if a['op']=='activate');death=s.index(dict(op='destroy',ref='source'));self.assertLess(act,death);self.assertEqual(self.count(name,'source_removed_pending_0'),1)
 def test_snapshots_replay_real_payments_responses_and_untaps(self):
  for name,v in [('Spider Spawning','response_snapshot_0'),('Dragon Roost','activation_snapshot_1'),('Rise from the Tides','untap_snapshot_0'),('Whirlermaker','pending_snapshot_1')]:
   c=self.cases(name,[v])[0];s=c['steps'];self.has(c,'assert_recorded_state');tail=s[s.index(dict(op='restore'))+1:];self.assertTrue(any(a['op'] in ('cast_response','resolve','untap_step') for a in tail));self.assertTrue(any(a['op']=='assert_zone_multiplicity' for a in tail))
 def test_new_profiles_ignore_handler_and_token_catalog_rules(self):
  from token_trigger_creation_spec import noncreature_data
  o=copy.deepcopy(self.oracle);rows=copy.deepcopy(self.rows)
  for n in ('Spider','Zombie','Saproling','Centaur','Dragon','Thopter'):o[n].update(power='99',toughness='99',colors=['W'],oracle_text='Deathtouch')
  for row in rows.values():row['handler']='card_dummy'
  for name in noncreature_data():self.assertEqual(self.cases(name),self.cases(name,o=o,rows=rows))
 def test_nonlive_refs_are_zone_offsets_with_complete_zone_receipts(self):
  c=self.cases('Spider Spawning')[0];refs={e['ref'] for e in c['setup'] if e['zone'] in ('library','graveyard','exile')};self.assertFalse(any(a['op']=='assert_card' and a.get('ref') in refs for a in c['steps']));self.assertTrue(any(a['op']=='assert_zone_card' for a in c['steps']))
 def test_all_six_profiles_have_explicit_unique_catalogs(self):
  total=0
  for name in ('Spider Spawning','Rise from the Tides','Saproling Symbiosis','Centaur Glade','Dragon Roost','Whirlermaker'):
   d=self.declaration(name);p=spec(d,self.oracle[name]);vv=variants(p);self.assertEqual(len(vv),len(set(vv)));total+=len(self.cases(name,vv))
  self.assertEqual(total,312)
 def test_flying_attack_denies_ground_block_and_lands_actual_damage(self):
  for name,power in [('Dragon Roost',5),('Whirlermaker',1)]:
   c=self.cases(name,['flying_evasion_1'])[0];self.has(c,'untap_step',player=1);self.has(c,'attack',ref='test_token',value=1);self.has(c,'block',ref='combat_witness',other='test_token',value=0);self.has(c,'assert_life',player=0,value=20-power)
 def test_repeat_activation_waits_for_main_priority_after_actual_untap(self):
  c=self.cases('Whirlermaker',['untap_repeat_1'])[0];s=c['steps'];u=s.index(dict(op='untap_step',player=1));self.assertEqual(s[u+1],dict(op='turn',player=1,value=0));self.assertTrue(any(a==dict(op='activate',ref='source',value=1) for a in s[u+2:]))

class PermanentTokenBreadthTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 declaration=NoncreatureAmountTest.declaration
 cases=NoncreatureAmountTest.cases
 count=NoncreatureAmountTest.count
 has=NoncreatureAmountTest.has
 def test_complete_typed_operative_entry_static_and_replacement_parameters(self):
  for name,field,value in [("Cogworker's Puzzleknot",'entry_creation',None),('Mobilization','active_static',dict(subtype='Soldier',keyword='Vigilance',controller='own')),('Throne of Empires','empire_replacement',dict(required_artifacts=['Crown of Empires'],count=5)),('Vessel of Ephemera','fixed_count',True)]:
   d=self.declaration(name);d['params'][field]=value
   with self.assertRaises(ValueError):self.cases(name,d=d)
 def test_independent_whole_activation_effect_rejects_extra_same_line_rules(self):
  from token_trigger_creation_spec import noncreature_data,noncreature_spec
  name='Vessel of Ephemera';data=noncreature_data();r=copy.deepcopy(self.oracle[name]);r['oracle_text']+=' Draw a card.';data[name]['source']=r;d=self.declaration(name);d['oracle_text']=r['oracle_text']
  with self.assertRaises(ValueError):noncreature_spec(d,r,data)
 def test_cogworker_entry_and_sacrifice_activation_are_separate_mandatory_effects(self):
  name="Cogworker's Puzzleknot"
  for owner in (0,1):
   self.assertEqual(self.count(name,'cast_only_'+str(owner)),1);self.assertEqual(self.count(name,'create_'+str(owner)),2);c=self.cases(name,['create_'+str(owner)])[0];s=c['steps'];act=next(i for i,a in enumerate(s) if a['op']=='activate');self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Servo']['csvid'],value=1),s[:act])
 def test_vessel_produces_exact_two_printed_flying_spirits(self):
  for owner in (0,1):
   self.assertEqual(self.count('Vessel of Ephemera','create_'+str(owner)),2);c=self.cases('Vessel of Ephemera',['block_flying_'+str(owner)])[0];self.has(c,'block',ref='test_token',other='combat_witness',value=1)
 def test_sacrifice_cost_is_observed_before_ability_resolution(self):
  for name in ('Vessel of Ephemera',"Cogworker's Puzzleknot"):
   c=self.cases(name)[0];s=c['steps'];act=next(i for i,a in enumerate(s) if a['op']=='activate');res=next(i for i in range(act+1,len(s)) if s[i]['op']=='resolve');self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s[act:res]);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[name]['csvid'],value=1),s[act:res])
 def test_myr_turbine_is_an_immediate_zero_mana_artifact_tap_ability(self):
  c=self.cases('Myr Turbine')[0];self.has(c,'require_can_activate',ref='source',value=1);self.has(c,'activate',ref='source',value=1);self.has(c,'assert_card',ref='source',property='tapped',value=1);self.assertEqual(self.count('Myr Turbine','untap_repeat_1'),2)
  from token_trigger_creation_spec import noncreature_data
  p=spec(self.declaration('Myr Turbine'),self.oracle['Myr Turbine']);self.assertNotIn('activation_insufficient_0',variants(p));self.assertTrue(any('Tap five untapped Myr' in g for g in c['_recipe']['untested_components']))
 def test_presented_ai_myr_create_menu_is_explicit_per_actual_activation(self):
  for variant in ('create_1','activation_snapshot_1','untap_snapshot_1'):
   c=self.cases('Myr Turbine',[variant])[0];self.assertEqual(sum(a==dict(op='choose_dialog',player=1,value=0) for a in c['steps']),sum(a==dict(op='activate',ref='source',value=1) for a in c['steps']))
  c=self.cases('Myr Turbine',['create_0'])[0];self.assertFalse(any(a['op']=='choose_dialog' for a in c['steps']))
 def test_empire_control_and_ownership_replacement_matrix(self):
  for owner in (0,1):
   for v,n in [('create',1),('empire_pair',5),('empire_crown_only',1),('empire_scepter_only',1),('empire_opponent',1),('empire_foreign_controlled',5),('empire_owned_opposing',1),('empire_one_lost',1)]:self.assertEqual(self.count('Throne of Empires',v+'_'+str(owner)),n)
 def test_empire_pieces_use_actual_casts_and_resolution_time_condition(self):
  c=self.cases('Throne of Empires',['empire_pair_1'])[0];self.has(c,'cast',ref='crown_piece',value=1);self.has(c,'cast',ref='scepter_piece',value=1)
  c=self.cases('Throne of Empires',['empire_lost_pending_0'])[0];s=c['steps'];act=next(i for i,a in enumerate(s) if a['op']=='activate');death=s.index(dict(op='destroy',ref='crown_piece'));res=next(i for i in range(act+1,len(s)) if s[i]['op']=='resolve');self.assertLess(act,death);self.assertLess(death,res);self.assertEqual(self.count('Throne of Empires','empire_lost_pending_0'),1);self.assertEqual(self.count('Throne of Empires','empire_pair_cleanup_0'),5)
 def test_mobilization_global_vigilance_has_normal_tokens_opponent_and_bear_controls(self):
  for v,tapped in [('grant_scope',0),('grant_lost',1)]:
   c=self.cases('Mobilization',[v+'_0'])[0]
   for player in (0,1):self.has(c,'assert_card',ref='normal_soldier_'+str(player),property='tapped',value=tapped)
   self.has(c,'assert_card',ref='ordinary_attacker',property='tapped',value=1);self.has(c,'assert_life',player=0,value=18);self.has(c,'assert_life',player=1,value=16)
 def test_mobilization_loss_pair_has_identical_setup_and_live_positive_receipts(self):
  live=self.cases('Mobilization',['grant_scope_0'])[0];lost=self.cases('Mobilization',['grant_lost_0'])[0];self.assertEqual(live['setup'],lost['setup']);self.has(lost,'cast',ref='remove_grant',value=1);s=lost['steps'];remove=next(i for i,a in enumerate(s) if a==dict(op='cast',ref='remove_grant',value=1));self.assertTrue(any(a['op']=='assert_matching_permanents' for a in s[:remove]));self.has(live,'attack',ref='normal_soldier_1',value=1)
 def test_actual_source_normal_witnesses_and_token_catalog_independence(self):
  o=copy.deepcopy(self.oracle);rows=copy.deepcopy(self.rows)
  for name in ('Myr','Spirit','Servo','Soldier'):o[name].update(power='99',toughness='99',colors=['B'],oracle_text='Deathtouch')
  for row in rows.values():row['handler']='card_dummy'
  for name in ('Myr Turbine','Vessel of Ephemera',"Cogworker's Puzzleknot",'Mobilization','Throne of Empires'):self.assertEqual(self.cases(name),self.cases(name,o=o,rows=rows))
 def test_all_five_maker_catalogs_have_supported_complete_observations(self):
  from generate_cases import OPS,PROPERTIES,TOKEN_PROPERTIES
  total=0
  for name in ('Myr Turbine','Vessel of Ephemera',"Cogworker's Puzzleknot",'Mobilization','Throne of Empires'):
   d=self.declaration(name);p=spec(d,self.oracle[name]);cases=self.cases(name,variants(p));total+=len(cases)
   for c in cases:
    for a in c['steps']:self.assertIn(a['op'],OPS);self.assertIn(a.get('property','power'),PROPERTIES|(TOKEN_PROPERTIES if a['op']=='assert_matching_permanents' else set()))
  self.assertEqual(total,258)
 def test_two_ordinary_normal_cost_spell_components_keep_alternate_components_explicit(self):
  for name,number in [('Roar of the Wurm',1),('Stir the Sands',3)]:
   d=TokenSpellExtensionTest.declaration(self,name);p=spec(d,self.oracle[name]);cases=expand(name,self.rows[name]['csvid'],d,p,variants(p),self.rows,self.oracle);self.assertEqual(p['token']['count'],number);self.assertTrue(any('inactive:' in g for g in cases[0]['_recipe']['untested_components']))

if __name__=='__main__':unittest.main()

class Wave6SpellDispatchTest(unittest.TestCase):
 def test_wave6_public_spell_branch_uses_private_palette(self):
  from recipes import expand as public_expand
  from token_trigger_creation_spec import wave6_params
  o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];rows={r['name']:r for r in registry()}
  for name in ('Increasing Devotion','Secure the Wastes','Martial Coup','Fresh Meat','Vitu-Ghazi, the City-Tree'):
   d=dict(family='token_spell_extension',version=2,params=wave6_params(o[name]),oracle_text=o[name]['oracle_text'],variants=['create_0','create_1']);cases=public_expand(name,rows[name]['csvid'],d,o[name],rows,oracle_context=o);self.assertEqual(len(cases),2)
   self.assertTrue(all(c['_recipe']['family']=='token_spell_extension' for c in cases));self.assertTrue(all(c['_recipe']['version']==2 for c in cases))
 def test_wave6_spell_declaration_rejects_internal_schema_bool(self):
  from token_trigger_creation_spec import wave6_params
  o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];name='Secure the Wastes';d=dict(family='token_spell_extension',version=2,params=wave6_params(o[name]),oracle_text=o[name]['oracle_text'],variants=['create_0']);d['params']['schema']=True
  with self.assertRaises(ValueError):spec(d,o[name])
