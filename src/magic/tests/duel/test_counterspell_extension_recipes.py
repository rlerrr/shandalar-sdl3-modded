"""Fail-closed authoring checks for the counter extension recipe."""
import copy
import json
import unittest
import hashlib
from pathlib import Path
from generate_cases import registry
import counterspell_extension_recipes as extension

class CounterExtensionTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
  cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):
  profile=extension.PROFILES.get(name) or extension.WAVE7_PROFILES[name]
  return dict(family='counterspell_extension',version=2,params=copy.deepcopy(profile['params']),oracle_text=self.oracle[name]['oracle_text'],variants=[])
 def cases(self,name,variants):
  d=self.declaration(name);d['variants']=variants;p=extension.spec(d,self.oracle[name]);return extension.expand(name,self.rows[name]['csvid'],d,p,variants,self.rows,self.oracle)
 def test_every_profile_whole_source_and_explicit_params(self):
  for name in extension.PROFILES:
   d=self.declaration(name);d['variants']=['opponent_0'];extension.spec(d,self.oracle[name])
   for changed in (dict(oracle_text=d['oracle_text']+' '),dict(params=dict(d['params'],ignored=True))):
    with self.assertRaises(ValueError):extension.spec(dict(d,**changed),self.oracle[name])
   changed=copy.deepcopy(self.oracle[name]);changed['keywords']=changed['keywords']+['Flash']
   with self.assertRaises(ValueError):extension.spec(d,changed)
 def test_unknown_empty_duplicate_and_wrong_family_rejected(self):
  for variants in ([],['invented_0'],['opponent_0','opponent_0']):
   d=self.declaration('Ionize');d['variants']=variants
   with self.assertRaises(ValueError):extension.spec(d,self.oracle['Ionize'])
  d=self.declaration('Ionize');d.update(family='counter_spell',variants=['opponent_0'])
  with self.assertRaises(ValueError):extension.spec(d,self.oracle['Ionize'])
 def test_full_witness_pin(self):
  extension.validate_witnesses(self.oracle,self.rows)
  changed=copy.deepcopy(self.oracle);changed['Scaled Wurm']['power']='6'
  with self.assertRaises(ValueError):extension.validate_witnesses(changed,self.rows)
 def test_both_controllers_and_token_receipts(self):
  for owner in (0,1):
   c=self.cases('Geist Snatch',['opponent_'+str(owner)])[0]
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Spirit']['csvid'],value=1),c['steps'])
   self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='source'),owner)
   self.assertTrue(any(s['op']=='assert_matching_permanents' and s.get('property')=='power' and s['value']==1 for s in c['steps']))
 def test_opponent_controller_casts_both_real_spells_on_stack(self):
  c=self.cases('Geist Snatch',['opponent_1'])[0]
  self.assertIn(dict(op='cast',ref='spell',value=1),c['steps'])
  self.assertIn(dict(op='cast_response',ref='source',value=1),c['steps'])
  self.assertIn(dict(op='assert_stack',value=2),c['steps'])
  index=c['steps'].index(dict(op='cast_response',ref='source',value=1))
  self.assertEqual(c['steps'][index-1],dict(op='assert_stack',value=1))
  self.assertEqual(c['steps'][index+1],dict(op='assert_stack',value=2))
  self.assertEqual(c['steps'][index+2],dict(op='resolve_response'))
  negative=self.cases('Geist Snatch',['insufficient_mana_1'])[0]
  self.assertIn(dict(op='resolve'),negative['steps'])
  self.assertFalse(any(s['op']=='resolve_response' for s in negative['steps']))
  self.assertFalse(any(s['op']=='choose_priority' for s in c['steps']))
 def test_snapshot_player_swap_handles_shared_replay_steps_once(self):
  c=self.cases('Geist Snatch',['snapshot_1'])[0]
  receipts=[s for s in c['steps'] if s['op']=='assert_mana_total']
  self.assertEqual([s['value'] for s in receipts if s['player']==1],[56,56])
  self.assertEqual([s['value'] for s in receipts if s['player']==0],[58,58])
  c=self.cases('Metallic Rebuke',['snapshot_1'])[0]
  self.assertEqual([s['player'] for s in c['steps'] if s['op']=='choose_dialog'],[0,0])
 def test_token_recipient_and_mana_value_are_printed(self):
  c=self.cases('Swan Song',['opponent_0'])[0]
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Bird']['csvid'],value=1),c['steps'])
  c=self.cases('Mystic Genesis',['value_high_0'])[0]
  self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows['Ooze']['csvid'],property='power',value=4),c['steps'])
  c=self.cases('Spell Swindle',['value_high_1'])[0]
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Treasure']['csvid'],value=4),c['steps'])
 def test_legacy_token_alias_preserves_printed_characteristics(self):
  c=self.cases("Summoner's Bane",['opponent_0'])[0];token_id=self.rows['Illusion Token']['csvid']
  self.assertEqual(token_id,18769)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=token_id,value=1),c['steps'])
  for prop,value in (('power',2),('toughness',2),('color',4),('subtype_Illusion',1),('token',1)):
   self.assertIn(dict(op='assert_matching_permanents',player=0,index=token_id,property=prop,value=value),c['steps'])
  changed=copy.deepcopy(self.rows);changed['Illusion Token']['csvid']=19208
  with self.assertRaises(ValueError):extension.validate_witnesses(self.oracle,changed)
 def test_life_cost_and_failure_are_separate(self):
  c=self.cases('Withering Boon',['opponent_1','insufficient_life_1'])
  self.assertIn(dict(op='assert_life',player=1,value=17),c[0]['steps'])
  self.assertIn(dict(op='assert_life',player=1,value=2),c[1]['steps'])
  self.assertIn(dict(op='cast_response',ref='source',value=0),c[1]['steps'])
  self.assertIn(dict(op='assert_mana_total',player=1,value=60),c[1]['steps'])
 def test_relationship_positive_and_negative_keep_real_spell(self):
  c=self.cases('Turn Aside',['opponent_0','wrong_relationship_0'])
  self.assertEqual([next(x['player'] for x in a['setup'] if x['ref']=='recipient') for a in c],[0,1])
  self.assertIn(dict(op='can_cast',ref='source',value=0),c[1]['steps'])
  self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=10),c[1]['steps'])
 def test_dynamic_draw_and_snapshot_token_receipts(self):
  c=self.cases('Overwhelming Intellect',['value_high_0'])[0]
  self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=4),c['steps'])
  c=self.cases('Confirm Suspicions',['snapshot_0'])[0]
  self.assertEqual(sum(s['op']=='cast' and s.get('ref')=='spell' for s in c['steps']),2)
  self.assertGreaterEqual(sum(s['op']=='assert_zone_multiplicity' and s.get('player')==0 and s.get('value')==3 for s in c['steps']),2)
 def test_no_mutation_and_every_supported_expansion(self):
  for name in extension.PROFILES:
   d=self.declaration(name);d['variants']=['opponent_0'];p=extension.spec(d,self.oracle[name]);d['variants']=extension.variants(p)
   before=copy.deepcopy((d,p,self.oracle));cases=extension.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)
   self.assertEqual(len(cases),len(d['variants']));self.assertEqual(before,(d,p,self.oracle))
   self.assertTrue(all(c['_recipe']['untested_components'] for c in cases))
 def test_prior_532_case_bodies_unchanged(self):
  cases=[]
  for name,profile in extension.PROFILES.items():
   if 'extension' in profile['params']:continue
   d=self.declaration(name);d['variants']=['opponent_0'];p=extension.spec(d,self.oracle[name]);d['variants']=extension.variants(p)
   cases+=extension.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)
  self.assertEqual(len(cases),532)
  body=json.dumps(cases,sort_keys=True,separators=(',',':')).encode()
  self.assertEqual(hashlib.sha256(body).hexdigest(),'c8454759e7f31108a33f242cf10716ff08d594282bd00359b3994ce43a7dfe3c')
 def test_new_derived_tax_and_zero_count_control(self):
  c=self.cases('Rethink',['tax_exact_1'])[0]
  self.assertIn(dict(op='assert_mana_total',player=0,value=0),c['steps'])
  c=self.cases('Override',['count_none_0'])[0]
  self.assertIn(dict(op='assert_card',ref='spell',property='in_play',value=1),c['steps'])
  self.assertFalse(any(s['op']=='choose_dialog' for s in c['steps']))
 def test_new_cost_payment_and_missing_resource_are_distinct(self):
  c=self.cases('Abjure',['payment_0','no_cost_resource_1'])
  self.assertIn(dict(op='choose_target',player=0,ref='cost_resource'),c[0]['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Wind Drake']['csvid'],value=1),c[0]['steps'])
  self.assertIn(dict(op='cast_response',ref='source',value=0),c[1]['steps'])
  self.assertFalse(any(x['ref']=='cost_resource' for x in c[1]['setup']))
  c=self.cases('Abjure',['opponent_1'])[0]
  self.assertFalse(any(s['op']=='choose_target' and s.get('ref')=='cost_resource' for s in c['steps']))
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Wind Drake']['csvid'],value=1),c['steps'])
  self.assertIn(dict(op='assert_mana_total',player=1,value=59),c['steps'])
 def test_new_untap_and_modal_inputs_use_real_response(self):
  c=self.cases('Rewind',['opponent_1'])[0]
  self.assertIn(dict(op='assert_stack',value=2),c['steps'])
  for i in range(4):self.assertIn(dict(op='assert_card',ref='untap_'+str(i),property='tapped',value=0),c['steps'])
  self.assertIn(dict(op='assert_card',ref='untap_unselected',property='tapped',value=1),c['steps'])
  c=self.cases('Steel Sabotage',['opponent_0'])[0]
  self.assertIn(dict(op='choose_dialog',player=0,value=0),c['steps'])
  name='Steel Sabotage';d=self.declaration(name);d['variants']=['opponent_0'];p=extension.spec(d,self.oracle[name])
  self.assertNotIn('no_stack_0',extension.variants(p))
 def test_new_scope_counts_and_colored_payment_denial(self):
  c=self.cases('Oppressive Will',['snapshot_0'])[0]
  self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=6),c['steps'])
  c=self.cases('Rune Snag',['snapshot_1'])[0]
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Rune Snag']['csvid'],value=3),c['steps'])
  c=self.cases('Evasive Action',['wrong_mana_0'])[0]
  self.assertIn(dict(op='cast_response',ref='source',value=0),c['steps'])
  for item in c['setup']:
   if item['zone']=='battlefield' and 'Land' in self.oracle[item['name']]['type_line']:self.assertIn('tapped',item['state'])
 def test_new_bounce_cost_rebind_and_controlled_modal_player_target(self):
  c=self.cases('Deprive',['opponent_1'])[0]
  self.assertIn(dict(op='bind_hand',player=1,value=self.rows['Forest']['csvid'],ref='cost_resource'),c['steps'])
  c=self.cases('Dawn Charm',['insufficient_mana_1'])[0]
  self.assertIn(dict(op='choose_player',player=0,value=1),c['steps'])
  self.assertIn(dict(op='assert_zone',player=1,zone='hand',value=4),c['steps'])
 def test_prior_57_homes_1314_case_bodies_unchanged(self):
  cases=[]
  for name,profile in extension.PROFILES.items():
   if profile['params'].get('extension',{}).get('wave') is not None:continue
   d=self.declaration(name);d['variants']=['opponent_0'];p=extension.spec(d,self.oracle[name]);d['variants']=extension.variants(p)
   cases += extension.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)
  self.assertEqual(len(cases),1314)
  self.assertEqual(hashlib.sha256(json.dumps(cases,sort_keys=True,separators=(',',':')).encode()).hexdigest(),'6084f5a703f14fc6d6b478a3fe834cdabc5fb453aa1bb68bb58e38e17fb67785')
 def test_optional_controller_draw_and_discard_conservation(self):
  c=self.cases('Vex',['opponent_1','draw_declined_1'])
  self.assertIn(dict(op='choose_dialog',player=0,value=0),c[0]['steps']);self.assertIn(dict(op='choose_dialog',player=0,value=1),c[1]['steps'])
  self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=1),c[0]['steps']);self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=0),c[1]['steps'])
  c=self.cases('Failed Inspection',['snapshot_0'])[0]
  self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=4),c['steps'])
  self.assertIn(dict(op='assert_hand_graveyard_total',player=0,index=self.rows['Forest']['csvid'],value=2),c['steps'])
  self.assertTrue(any(s['op']=='assert_zone_multiplicity_max' for s in c['steps']))
 def test_ancillary_effects_apply_when_parent_tax_paid(self):
  c=self.cases('Frightful Delusion',['tax_exact_0'])[0]
  self.assertFalse(any(s['op']=='assert_card' and s.get('ref')=='discard_fodder' and s.get('property')=='in_hand' for s in c['steps']))
  self.assertIn(dict(op='assert_hand_graveyard_total',player=1,index=self.rows['Forest']['csvid'],value=1),c['steps'])
  self.assertIn(dict(op='assert_zone',player=1,zone='hand',value=0),c['steps'])
  self.assertIn(dict(op='assert_zone',player=1,zone='graveyard',value=1),c['steps'])
  c=self.cases('Abstruse Interference',['tax_paid_0'])[0]
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Eldrazi Scion']['csvid'],value=1),c['steps'])
  self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows['Eldrazi Scion']['csvid'],property='subtype_Scion',value=1),c['steps'])
 def test_bonus_counter_targets_persist_and_rejected_cast_keeps_choice(self):
  c=self.cases('Trap Essence',['bonus_opponent_0'])[0]
  self.assertIn(dict(op='choose_target',player=0,ref='bonus_other'),c['steps'])
  self.assertIn(dict(op='assert_card',ref='bonus_other',property='plus1_counters',value=2),c['steps'])
  c=self.cases('Essence Capture',['bonus_cleanup_1'])[0]
  at=next(i for i,s in enumerate(c['steps']) if s['op']=='cleanup')
  self.assertIn(dict(op='assert_card',ref='bonus_recipient',property='plus1_counters',value=1),c['steps'][at:])
  c=self.cases('Trap Essence',['wrong_type_0'])[0]
  self.assertIn(dict(op='cast_response',ref='source',value=0),c['steps']);self.assertIn(dict(op='assert_unused_choices',value=1),c['steps'])
  c=self.cases('Essence Capture',['bonus_no_owned_1'])[0]
  self.assertTrue(all(x['player']==0 for x in c['setup'] if x['ref'] in ('recipient','bonus_recipient','bonus_other')))
  self.assertIn(dict(op='cast_response',ref='source',value=1),c['steps']);self.assertIn(dict(op='assert_unused_choices',value=1),c['steps'])
  self.assertIn(dict(op='assert_card',ref='bonus_recipient',property='plus1_counters',value=0),c['steps'])
 def test_created_scion_real_sacrifice_mana_activation(self):
  c=self.cases('Abstruse Interference',['token_mana_1'])[0]
  self.assertIn(dict(op='activate',ref='created_token',value=1),c['steps'])
  self.assertIn(dict(op='assert_mana_total',player=1,value=58),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Eldrazi Scion']['csvid'],value=0),c['steps'])
 def test_conditional_ancillary_effects_use_printed_controller_state(self):
  c=self.cases('Statute of Denial',['opponent_0','condition_opponent_0'])
  self.assertIn(dict(op='assert_hand_graveyard_total',player=0,index=self.rows['Forest']['csvid'],value=1),c[0]['steps'])
  self.assertFalse(any(s['op']=='assert_hand_graveyard_total' for s in c[1]['steps']))
  c=self.cases('Calculated Dismissal',['tax_paid_1','condition_none_1','condition_opponent_1','snapshot_1','mastery_snapshot_1','mastery_snapshot_0'])
  self.assertEqual([sum(s['op']=='choose_zone' for s in a['steps']) for a in c],[1,0,0,0,2,4])
  self.assertTrue(all(a['player']==0 for a in c[2]['setup'] if a['ref'].startswith('condition_grave')))
  self.assertTrue(any(s['op']=='assert_zone_card' and s.get('player')==1 and s.get('index')==2 for s in c[4]['steps']))
  self.assertFalse(any(s['op']=='assert_zone_card' and s.get('player')==1 and s.get('index',99)<2 for s in c[4]['steps']))
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='library',index=self.rows['Forest']['csvid'],value=2),c[4]['steps'])
 def test_mandatory_secondary_effects_and_control_receipts(self):
  c=self.cases('Lost in the Mist',['secondary_land_1'])[0]
  self.assertIn(dict(op='choose_target',player=1,ref='secondary'),c['steps'])
  self.assertIn(dict(op='bind_hand',player=0,value=self.rows['Forest']['csvid'],ref='secondary'),c['steps'])
  self.assertIn(dict(op='assert_card',ref='secondary',property='in_hand',value=1),c['steps'])
  c=self.cases('Suffocating Blast',['opponent_0','secondary_lethal_0','secondary_cleanup_1'])
  self.assertIn(dict(op='assert_card',ref='secondary',property='damage',value=3),c[0]['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Wind Drake']['csvid'],value=1),c[1]['steps'])
  self.assertEqual(c[2]['steps'][-2:], [dict(op='assert_card',ref='secondary',property='damage',value=0),dict(op='assert_card',ref='secondary',property='in_play',value=1)])
 def test_variable_counter_x_and_zero_tax_parent_resolution(self):
  for name in ('Condescend','Clash of Wills'):
   c=self.cases(name,['x_0_1','x_8_1','payment_0'])
   self.assertIn(dict(op='choose_x',player=1,value=0),c[0]['steps'])
   self.assertIn(dict(op='assert_card',ref='spell',property='in_play',value=1),c[0]['steps'])
   self.assertIn(dict(op='assert_mana_total',player=1,value=59),c[0]['steps'])
   self.assertIn(dict(op='choose_x',player=1,value=8),c[1]['steps'])
   self.assertIn(dict(op='assert_mana_total',player=1,value=51),c[1]['steps'])
   self.assertIn(dict(op='assert_mana_total',player=0,value=0),c[2]['steps'])
   self.assertIn(dict(op='set_mana',player=0,color=0,value=3),c[2]['steps'])

 def case(self,name,v):return self.cases(name,[v])[0]
 def test_reused_x_ancillary_effects_even_when_paid(self):
  c=self.case('Overrule','x_8_1')
  self.assertIn(dict(op='assert_life',player=1,value=28),c['steps'])
  c=self.case('Mindswipe','tax_paid_0')
  self.assertIn(dict(op='assert_life',player=1,value=17),c['steps'])
  c=self.case('Mindswipe','x_0_0')
  self.assertIn(dict(op='assert_life',player=1,value=20),c['steps'])
  c=self.case('Syncopate','opponent_0')
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=1),c['steps'])
 def test_explicit_command_modes_and_draw_receipts(self):
  for name,choices,draw in [('Cryptic Command',[0,3],1),("Ojutai's Command",[2,3],1),('Mystic Confluence',[0,2,2],2)]:
   c=self.case(name,'opponent_1')
   self.assertEqual([s['value'] for s in c['steps'] if s['op']=='choose_dialog'][:len(choices)],choices)
   self.assertIn(dict(op='present_ai_dialogs',value=1),c['steps'])
   self.assertIn(dict(op='assert_zone',player=1,zone='hand',value=draw),c['steps'])
 def test_color_is_resolution_condition(self):
  for name in ('Hydroblast','Pyroblast'):
   for v,alive in [('opponent_0',1),('color_match_0',0)]:
    c=self.case(name,v)
    self.assertIn(dict(op='cast_response',ref='source',value=1),c['steps'])
    self.assertIn(dict(op='assert_card',ref='spell',property='in_play',value=alive),c['steps'])
 def test_high_value_is_legal_but_not_countered(self):
  for name in ('Dispersal Shield','Prohibit'):
   c=self.case(name,'value_high_1')
   self.assertIn(dict(op='cast_response',ref='source',value=1),c['steps'])
   self.assertIn(dict(op='assert_card',ref='spell',property='in_play',value=1),c['steps'])
 def test_only_own_permanents_contribute(self):
  c=self.case('Dispersal Shield','condition_opponent_1')
  self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='condition_opponent'),0)
  self.assertIn(dict(op='assert_card',ref='spell',property='in_play',value=1),c['steps'])
 def test_ferocious_and_dragon_tax_scopes(self):
  for name in ('Stubborn Denial',"Silumgar's Scorn"):
   a=self.case(name,'opponent_0');b=self.case(name,'tax_paid_0');c=self.case(name,'condition_opponent_1')
   self.assertFalse(any(s['op']=='choose_dialog' for s in a['steps']))
   self.assertIn(dict(op='choose_dialog',player=1,value=0),b['steps'])
   self.assertIn(dict(op='choose_dialog',player=0,value=1),c['steps'])
 def test_kicker_changes_threshold_and_printed_payment(self):
  c=self.case('Prohibit','kicked_high_0')
  self.assertIn(dict(op='choose_dialog',player=0,value=0),c['steps'])
  self.assertIn(dict(op='assert_card',ref='spell',property='in_play',value=0),c['steps'])
  c=self.case('Prohibit','kicked_payment_1')
  self.assertIn(dict(op='set_mana',player=1,color=0,value=3),c['steps'])
  self.assertIn(dict(op='assert_mana_total',player=1,value=0),c['steps'])
 def test_dedicated_colorless_is_not_colored_payment(self):
  c=self.case('Warping Wail','wrong_mana_0')
  self.assertIn(dict(op='set_mana',player=0,color=0,value=0),c['steps'])
  self.assertIn(dict(op='set_mana',player=0,color=1,value=2),c['steps'])
  self.assertIn(dict(op='cast_response',ref='source',value=0),c['steps'])

 def test_target_value_x_has_exact_payment_and_no_free_x_input(self):
  for name in ('Spell Blast','Spell Burst'):
   c=self.case(name,'payment_1')
   self.assertFalse(any(s['op']=='choose_x' for s in c['steps']))
   self.assertIn(dict(op='set_mana',player=1,color=0,value=2),c['steps'])
   self.assertIn(dict(op='assert_mana_total',player=1,value=0),c['steps'])
 def test_buyback_pays_three_and_returns_bound_source(self):
  c=self.case('Spell Burst','buyback_payment_0')
  self.assertIn(dict(op='choose_dialog',player=0,value=0),c['steps'])
  self.assertIn(dict(op='set_mana',player=0,color=0,value=5),c['steps'])
  self.assertIn(dict(op='bind_hand',player=0,value=self.rows['Spell Burst']['csvid'],ref='source'),c['steps'])
  self.assertIn(dict(op='assert_card',ref='source',property='in_hand',value=1),c['steps'])
 def test_hinder_bottom_placement(self):
  c=self.case('Hinder','bottom_1')
  self.assertIn(dict(op='choose_dialog',player=1,value=1),c['steps'])
  self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=7,value=self.rows['Grizzly Bears']['csvid']),c['steps'])
 def test_cast_denial_expires_and_other_controller_can_cast(self):
  c=self.case('Render Silent','denial_expiry_1')
  at=next(i for i,s in enumerate(c['steps']) if s['op']=='cleanup')
  self.assertIn(dict(op='cast',ref='followup',value=0),c['steps'][:at])
  self.assertIn(dict(op='cast',ref='followup',value=1),c['steps'][at:])
  self.assertIn(dict(op='set_mana',player=0,color=3,value=1),c['steps'][at:])
  c=self.case('Render Silent','snapshot_1')
  self.assertEqual(sum(s==dict(op='cast',ref='followup',value=0) for s in c['steps']),2)
  c=self.case('Render Silent','other_player_cast_0')
  self.assertIn(dict(op='cast',ref='unaffected_spell',value=1),c['steps'])
  self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=10),c['steps'])
 def test_black_payment_mills_parent_value_and_owner_ai_omits_dialog(self):
  a=self.case('Induce Paranoia','black_high_0');b=self.case('Induce Paranoia','black_high_1')
  self.assertIn(dict(op='choose_dialog',player=0,value=1),a['steps'])
  self.assertFalse(any(s['op']=='choose_dialog' for s in b['steps']))
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=3),a['steps'])
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=3),b['steps'])
  c=self.case('Induce Paranoia','black_own_spell_0')
  self.assertIn(dict(op='set_mana',player=0,color=3,value=1),c['steps'])
  self.assertIn(dict(op='assert_mana_total',player=0,value=0),c['steps'])
 def test_library_reorder_has_functional_human_choice_and_ai_permutation(self):
  a=self.case('Discombobulate','opponent_0');b=self.case('Discombobulate','opponent_1')
  self.assertIn(dict(op='choose_zone',player=0,value=2),a['steps'])
  self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=0,value=self.rows['Grizzly Bears']['csvid']),a['steps'])
  self.assertFalse(any(s['op']=='assert_zone_card' and s.get('player')==1 and s.get('index',99)<4 for s in b['steps']))
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='library',index=self.rows['Forest']['csvid'],value=2),b['steps'])
 def test_above_four_kicker_permission_drain_scope_explicit(self):
  a=self.case('Prohibit','kicked_above_0');b=self.case('Prohibit','kicked_above_1')
  self.assertEqual(next(c['name'] for c in a['setup'] if c['ref']=='spell'),'Scaled Wurm')
  self.assertIn(dict(op='resolve_response'),a['steps']);self.assertIn(dict(op='resolve'),b['steps'])
  self.assertIn(dict(op='cast_response',ref='source',value=1),b['steps'])
  self.assertTrue(any('permission' in s for s in b['_recipe']['untested_components']))
 def test_first_25_wave4_homes_and_all_prior_bodies_unchanged(self):
  cases=[];later={'Bind','Squelch','Interdict','Rust','Spell Blast','Spell Burst','Hinder','Render Silent','Induce Paranoia','Discombobulate','Molten Influence'}
  for name in extension.PROFILES:
   if name in later or extension.PROFILES[name]['params'].get('extension',{}).get('wave')==5:continue
   d=self.declaration(name);d['variants']=['opponent_0'];p=extension.spec(d,self.oracle[name]);d['variants']=[v for v in extension.variants(p) if not v.startswith('kicked_above_')]
   cases += extension.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)
  self.assertEqual(len(cases),2044)
  self.assertEqual(hashlib.sha256(json.dumps(cases,sort_keys=True,separators=(',',':')).encode()).hexdigest(),'dd6f8d8a2d1f5fc71fc24a2da21579708505277f0c71c2cc90750d313534000c')
 def test_optional_damage_belongs_to_original_spell_controller(self):
  a=self.case('Molten Influence','damage_taken_1');b=self.case('Molten Influence','damage_own_spell_0')
  self.assertIn(dict(op='choose_dialog',player=0,value=0),a['steps'])
  self.assertIn(dict(op='assert_life',player=0,value=16),a['steps'])
  self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=10),a['steps'])
  self.assertIn(dict(op='choose_dialog',player=0,value=0),b['steps'])
  self.assertIn(dict(op='assert_life',player=0,value=16),b['steps'])
  c=self.case('Molten Influence','wrong_type_1')
  self.assertIn(dict(op='cast_response',ref='source',value=0),c['steps'])
  self.assertIn(dict(op='assert_unused_choices',value=1),c['steps'])

 def test_real_whelp_pending_cost_and_parent_effect(self):
  a=self.cases('Bind',['opponent_0'])[0];b=self.cases('Bind',['insufficient_mana_0'])[0]
  self.assertIn(dict(op='activate',ref='whelp',value=1),a['steps'])
  self.assertIn(dict(op='assert_mana',player=1,color=4,value=9),a['steps'])
  self.assertIn(dict(op='cast',ref='source',value=1),a['steps'])
  self.assertIn(dict(op='assert_stack',value=2),a['steps'])
  self.assertIn(dict(op='assert_card',ref='whelp',property='power',value=3),b['steps'])
  self.assertIn(dict(op='cast',ref='source',value=0),b['steps'])
 def test_unrelated_activation_control_has_printed_red_mana(self):
  for name in ('Bind','Squelch','Interdict','Rust'):
   for v in ('payment_0','insufficient_mana_0','wrong_mana_0'):
    c=self.cases(name,[v])[0]
    self.assertIn(dict(op='can_activate',ref='other_whelp',value=0),c['steps'])
   c=self.cases(name,['opponent_0'])[0]
   self.assertIn(dict(op='can_activate',ref='other_whelp',value=1),c['steps'])
 def test_oracle_controls_squelch_and_interdict_independently_of_alias(self):
  self.assertEqual(self.rows['Squelch']['handler'],self.rows['Interdict']['handler'])
  a=self.cases('Squelch',['repeat_parent_0'])[0];b=self.cases('Interdict',['repeat_parent_0'])[0]
  self.assertIn(dict(op='activate',ref='whelp',value=1),a['steps'][-16:])
  self.assertIn(dict(op='activate',ref='whelp',value=0),b['steps'][-16:])
  self.assertIn(dict(op='assert_card',ref='whelp',property='power',value=3),a['steps'][-16:])
  self.assertIn(dict(op='assert_card',ref='whelp',property='power',value=2),b['steps'][-16:])
 def test_mana_ability_is_real_immediate_and_has_no_counter_target(self):
  c=self.cases('Squelch',['mana_ability_1'])[0]
  self.assertIn(dict(op='activate',ref='mana_source',value=1),c['steps'])
  self.assertIn(dict(op='assert_mana_total',player=0,value=62),c['steps'])
  self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])
  self.assertFalse(any(s['op']=='assert_stack' and s['value']>0 for s in c['steps']))
 def test_rust_artifact_positive_and_creature_rejection(self):
  a,b=self.cases('Rust',['opponent_0','wrong_source_0'])
  self.assertIn(dict(op='activate',ref='icy',value=1),a['steps'])
  self.assertIn(dict(op='assert_card',ref='icy',property='tapped',value=1),a['steps'])
  self.assertIn(dict(op='cast',ref='source',value=1),a['steps'])
  self.assertIn(dict(op='activate',ref='whelp',value=1),b['steps'])
  self.assertIn(dict(op='cast',ref='source',value=0),b['steps'])
  self.assertIn(dict(op='assert_card',ref='whelp',property='power',value=3),b['steps'])
 def test_interdict_expiry_and_unrelated_source_control(self):
  c=self.cases('Interdict',['expiry_1'])[0];at=next(i for i,s in enumerate(c['steps']) if s['op']=='advance_turn')
  self.assertIn(dict(op='can_activate',ref='whelp',value=0),c['steps'][:at])
  self.assertIn(dict(op='activate',ref='whelp',value=1),c['steps'][at:])
  c=self.cases('Interdict',['control_activation_1'])[0]
  self.assertIn(dict(op='activate',ref='other_whelp',value=1),c['steps'])
  self.assertIn(dict(op='assert_card',ref='other_whelp',property='power',value=3),c['steps'])
 def test_snapshot_inputs_and_both_controllers(self):
  for name in ('Bind','Squelch','Interdict','Rust'):
   c=self.cases(name,['snapshot_1'])[0]
   self.assertEqual(sum(s['op']=='cast' and s.get('ref')=='source' for s in c['steps']),2)
   self.assertTrue(all(x['player']==1 for x in c['setup'] if x['ref']=='source'))
   self.assertEqual(sum(s==dict(op='assert_zone_multiplicity',player=1,zone='hand',index=self.rows[name]['csvid'],value=1) for s in c['steps']),2)
   self.assertEqual(len({id(s) for s in c['steps']}),len(c['steps']))

 def test_prior_89_homes_2238_bodies_unchanged(self):
  cases=[]
  for name,profile in extension.PROFILES.items():
   if profile['params'].get('extension',{}).get('wave')==5:continue
   d=self.declaration(name);d['variants']=['opponent_0'];p=extension.spec(d,self.oracle[name]);d['variants']=extension.variants(p)
   cases+=extension.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)
  self.assertEqual(len(cases),2238)
  self.assertEqual(hashlib.sha256(json.dumps(cases,sort_keys=True,separators=(',',':')).encode()).hexdigest(),'08e1f417f31eed9991e9e9edd715daaf129cab82937838c9df366ad7d12f374d')
 def test_search_exiles_matching_names_only_in_three_target_zones(self):
  c=self.cases('Counterbore',['opponent_0'])[0]
  self.assertEqual(sum(s['op']=='choose_zone_card' for s in c['steps']),6)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=6),c['steps'])
  self.assertIn(dict(op='assert_card',ref='search_battlefield_control',property='in_play',value=1),c['steps'])
  self.assertIn(dict(op='assert_card',ref='search_other_hand',property='in_hand',value=1),c['steps'])
  self.assertFalse(any(s['op']=='assert_zone_card' and s.get('player')==1 for s in c['steps']))
 def test_delirium_uses_grave_after_parent_before_source_finishes(self):
  a,b,c=self.cases('Invasive Surgery',['delirium_three_0','delirium_opponent_0','delirium_own_parent_0'])
  self.assertFalse(any(s['op']=='choose_zone_card' for s in a['steps']+b['steps']))
  self.assertTrue(any(s['op']=='choose_zone_card' for s in c['steps']))
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Divination']['csvid'],value=5),c['steps'])
 def test_optional_search_declines_and_partial_choice(self):
  a,b=self.cases('Invasive Surgery',['search_declined_1','search_partial_1'])
  self.assertEqual(sum(s['op']=='choose_zone' for s in a['steps']),3)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Divination']['csvid'],value=0),a['steps'])
  self.assertEqual(sum(s['op']=='choose_zone_card' for s in b['steps']),1)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Divination']['csvid'],value=1),b['steps'])

 def test_first_eight_wave5_gated_bodies_unchanged(self):
  cases=[]
  for name in ['Bind', 'Squelch', 'Interdict', 'Rust', 'Counterbore', 'Quash', 'Invasive Surgery', "Silumgar's Command"]:
   d=self.declaration(name);d['variants']=['opponent_0'];p=extension.spec(d,self.oracle[name]);d['variants']=extension.variants(p)
   cases+=extension.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)
  self.assertEqual(len(cases),204)
  self.assertEqual(hashlib.sha256(json.dumps(cases,sort_keys=True,separators=(',',':')).encode()).hexdigest(),'5502afef2cc2dddb21b8afc0fad0d4aa3d895d69e41ee1d5f2719e23bd3a0237')
 def test_silumgar_counter_bounce_printed_mode_pair_and_controllers(self):
  for owner in (0,1):
   c=self.cases("Silumgar's Command",['secondary_land_'+str(owner)])[0]
   dialogs=[s['value'] for s in c['steps'] if s['op']=='choose_dialog']
   self.assertEqual(dialogs,[0,1])
   self.assertIn(dict(op='choose_target',player=owner,ref='secondary'),c['steps'])
   self.assertIn(dict(op='assert_card',ref='secondary',property='in_hand',value=1),c['steps'])
   self.assertIn(dict(op='cast_response',ref='source',value=1),c['steps'])

 def test_resource_expand_rejects_invalid_selected_variants(self):
  for name in ('Scent of Brine','Rites of Refusal','Logic Knot','Mental Misstep','Force Void'):
   d=self.declaration(name);d['variants']=['opponent_0'];p=extension.spec(d,self.oracle[name])
   for selected in ([],['opponent_0','opponent_0'],['invented_0'],[True]):
    with self.assertRaises(ValueError):extension.expand(name,self.rows[name]['csvid'],d,p,selected,self.rows,self.oracle)
 def test_phyrexian_payment_is_cast_time_mana_or_two_life(self):
  blue,life,denied=self.cases('Mental Misstep',['payment_0','life_payment_0','insufficient_mana_0'])
  self.assertIn(dict(op='choose_number',player=0,value=0),blue['steps'])
  self.assertIn(dict(op='choose_number',player=0,value=1),life['steps'])
  at=next(i for i,x in enumerate(life['steps']) if x['op']=='resolve_response')
  self.assertIn(dict(op='assert_life',player=0,value=18),life['steps'][:at])
  self.assertIn(dict(op='assert_mana_total',player=0,value=0),life['steps'][:at])
  self.assertIn(dict(op='set_life',player=0,value=1),denied['steps'])
  self.assertIn(dict(op='cast_response',ref='source',value=0),denied['steps'])
  self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=5),denied['steps'])
 def test_delve_cash_and_exile_define_independent_printed_x(self):
  a,b=self.cases('Logic Knot',['opponent_0','cash_only_1'])
  self.assertIn(dict(op='choose_x',player=0,value=1),a['steps'])
  at=next(i for i,x in enumerate(a['steps']) if x['op']=='resolve_response')
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Forest']['csvid'],value=1),a['steps'][:at])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Island']['csvid'],value=1),a['steps'][:at])
  self.assertIn(dict(op='choose_x',player=1,value=3),b['steps'])
  self.assertFalse(any(x['op']=='choose_zone_card' for x in b['steps']))
 def test_resolution_discard_is_not_an_additional_cost(self):
  c=self.cases('Rites of Refusal',['tax_exact_0'])[0];at=next(i for i,x in enumerate(c['steps']) if x['op']=='resolve_response')
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Forest']['csvid'],value=1),c['steps'][:at])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Forest']['csvid'],value=0),c['steps'][:at])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Forest']['csvid'],value=1),c['steps'][at:])
  self.assertIn(dict(op='assert_mana_total',player=1,value=0),c['steps'][at:])
 def test_reveal_is_nondestructive_and_has_actual_player_zero_viewer(self):
  c=self.cases('Scent of Brine',['resource_many_1'])[0]
  self.assertIn(dict(op='choose_zone',player=0,value=-1),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=self.rows['Wind Drake']['csvid'],value=2),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=self.rows['Counterspell']['csvid'],value=1),c['steps'])
 def test_delayed_draw_real_phases_and_first_upkeep_expiry(self):
  for owner,expected in ((0,[0,2,3]),(1,[0,1,1])):
   c=self.cases('Force Void',['delayed_once_'+str(owner)])[0]
   self.assertEqual(sum(x['op']=='ai_turns' and x['value']==1 for x in c['steps']),2)
   self.assertEqual([x['value'] for x in c['steps'] if x['op']=='assert_zone' and x['zone']=='hand' and x['player']==owner],expected)
   self.assertFalse(any(x['zone']=='battlefield' for x in c['setup']))
  c=self.cases('Force Void',['snapshot_1'])[0]
  self.assertEqual(sum(x['op']=='ai_turns' for x in c['steps']),2)
  self.assertEqual(len({id(x) for x in c['steps']}),len(c['steps']))

 def test_first_thirteen_wave5_bodies_except_approved_rites_policy(self):
  cases=[]
  for name in ['Bind', 'Squelch', 'Interdict', 'Rust', 'Counterbore', 'Quash', 'Invasive Surgery', "Silumgar's Command", 'Scent of Brine', 'Rites of Refusal', 'Logic Knot', 'Mental Misstep', 'Force Void']:
   d=self.declaration(name);d['variants']=['opponent_0'];p=extension.spec(d,self.oracle[name]);d['variants']=extension.variants(p)
   cases+=extension.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)
  self.assertEqual(len(cases),338)
  self.assertEqual(hashlib.sha256(json.dumps(cases,sort_keys=True,separators=(',',':')).encode()).hexdigest(),'105465f5a6fd480f4ba8faa71c67c30d2cb17f745982c1e3d0cea41fc3b55b3a')

 def test_desertion_control_and_original_ownership_use_real_actions(self):
  c=self.cases('Desertion',['acquired_owner_grave_1'])[0]
  self.assertIn(dict(op='bind_permanent_copy',player=1,value=self.rows['Sol Ring']['csvid'],index=0,ref='spell'),c['steps'])
  self.assertIn(dict(op='cast',ref='owner_test_removal',value=1),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Sol Ring']['csvid'],value=1),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Sol Ring']['csvid'],value=0),c['steps'])
  c=self.cases('Desertion',['acquired_mana_0'])[0]
  self.assertIn(dict(op='activate',ref='spell',value=1),c['steps'])
  self.assertIn(dict(op='assert_card',ref='spell',property='tapped',value=1),c['steps'])
 def test_counterlash_waives_larger_matching_creature_cost(self):
  c=self.cases('Counterlash',['payment_0'])[0]
  self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='spell'),'Grizzly Bears')
  self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='free_spell'),'Hill Giant')
  self.assertIn(dict(op='assert_mana_total',player=0,value=0),c['steps'])
  self.assertIn(dict(op='assert_card',ref='free_spell',property='in_play',value=1),c['steps'])
  for v in ('free_declined_0','free_nonmatching_0'):
   c=self.cases('Counterlash',[v])[0]
   self.assertIn(dict(op='assert_card',ref='free_spell',property='in_hand',value=1),c['steps'])
 def test_put_away_cast_target_timing_has_safe_unused_fallback(self):
  c=self.cases('Put Away',['grave_timing_0'])[0];steps=c['steps'];at=next(i for i,x in enumerate(steps) if x['op']=='cast_response')
  self.assertEqual(steps[at-1],dict(op='choose_zone_card',player=0,value=self.rows['Forest']['csvid']))
  self.assertEqual(steps[at+2],dict(op='assert_choices_consumed'))
  self.assertEqual(steps[at+3],dict(op='choose_zone_card',player=0,value=self.rows['Forest']['csvid']))
  self.assertIn(dict(op='assert_unused_choices',value=1),steps)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='library',index=self.rows['Forest']['csvid'],value=3),steps)
  self.assertFalse(any(x['op']=='assert_zone_card' and x.get('player')==0 for x in steps))
 def test_land_response_matches_land_controller_and_parent_kind(self):
  a,b,c=self.cases("Teferi's Response",['opponent_0','wrong_land_0','spell_land_1'])
  self.assertIn(dict(op='activate',ref='icy',value=1),a['steps'])
  self.assertIn(dict(op='cast',ref='source',value=1),a['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Icy Manipulator']['csvid'],value=1),a['steps'])
  self.assertIn(dict(op='cast',ref='source',value=0),b['steps'])
  self.assertIn(dict(op='assert_card',ref='opposing_land',property='tapped',value=1),b['steps'])
  self.assertIn(dict(op='assert_card',ref='own_land',property='tapped',value=0),b['steps'])
  self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='parent_spell'),'Stone Rain')
  self.assertIn(dict(op='cast_response',ref='source',value=1),c['steps'])
  self.assertIn(dict(op='resolve_response'),c['steps'])
 def test_corrupted_uses_real_poison_and_preserves_unpoisoned_permission(self):
  a,b=self.cases('Corrupted Resolve',['opponent_0','condition_opponent_0'])
  self.assertIn(dict(op='cast',ref='poison_spell',value=1),a['steps'])
  self.assertIn(dict(op='assert_poison',player=1,value=1),a['steps'])
  self.assertIn(dict(op='resolve_response'),a['steps'])
  self.assertIn(dict(op='assert_poison',player=0,value=1),b['steps'])
  self.assertIn(dict(op='assert_poison',player=1,value=0),b['steps'])
  self.assertIn(dict(op='cast_response',ref='source',value=1),b['steps'])
  self.assertIn(dict(op='resolve'),b['steps'])
  self.assertFalse(any(x['op']=='resolve_response' for x in b['steps']))
  self.assertTrue(any('permission' in x for x in b['_recipe']['untested_components']))

 def test_first_eighteen_wave5_bodies_except_approved_rites_policy(self):
  cases=[]
  for name in ['Bind', 'Squelch', 'Interdict', 'Rust', 'Counterbore', 'Quash', 'Invasive Surgery', "Silumgar's Command", 'Scent of Brine', 'Rites of Refusal', 'Logic Knot', 'Mental Misstep', 'Force Void', 'Desertion', 'Counterlash', 'Put Away', "Teferi's Response", 'Corrupted Resolve']:
   d=self.declaration(name);d['variants']=['opponent_0'];p=extension.spec(d,self.oracle[name]);d['variants']=extension.variants(p)
   cases+=extension.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)
  self.assertEqual(len(cases),476)
  self.assertEqual(hashlib.sha256(json.dumps(cases,sort_keys=True,separators=(',',':')).encode()).hexdigest(),'ee8a6b91aa288b96d12094da62d46b56da4b21092d9ab0ed88e2461c281779b9')

 def test_proliferate_real_seeds_partial_selection_and_persistence(self):
  c=self.cases('Fuel for the Cause',['proliferate_partial_0'])[0];steps=c['steps'];at=next(i for i,x in enumerate(steps) if x['op']=='cast_response')
  self.assertEqual(steps[0],dict(op='reset_death_history'))
  self.assertIn(dict(op='cast',ref='proliferate_seed_plus',value=1),steps[:at])
  self.assertIn(dict(op='cast',ref='proliferate_seed_minus',value=1),steps[:at])
  self.assertIn(dict(op='assert_card',ref='proliferate_positive',property='plus1_counters',value=2),steps[at:])
  self.assertIn(dict(op='assert_card',ref='proliferate_negative',property='minus1_counters',value=1),steps[at:])
  c=self.cases('Fuel for the Cause',['proliferate_cleanup_1'])[0];at=next(i for i,x in enumerate(c['steps']) if x['op']=='cleanup')
  self.assertIn(dict(op='assert_card',ref='proliferate_negative',property='minus1_counters',value=2),c['steps'][at:])
  c=self.cases('Fuel for the Cause',['proliferate_none_0'])[0]
  self.assertFalse(any(x['op']=='cast' and x.get('ref','').startswith('proliferate_seed') for x in c['steps']))
 def test_mastery_copy_excludes_source_and_includes_countered_own_parent(self):
  a,b=self.cases('Psychic Rebuttal',['mastery_one_0','mastery_own_parent_0'])
  self.assertIn(dict(op='assert_unused_choices',value=2),a['steps'])
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=7),a['steps'])
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=4),b['steps'])
  c=self.cases('Psychic Rebuttal',['copy_opponent_1'])[0]
  self.assertIn(dict(op='choose_player',player=1,value=0),c['steps'])
  self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=3),c['steps'])
 def test_power_sink_drains_only_original_controller_and_mana_lands(self):
  c=self.cases('Power Sink',['own_spell_0'])[0]
  self.assertIn(dict(op='assert_mana_total',player=0,value=0),c['steps'])
  self.assertIn(dict(op='assert_card',ref='drain_own_land',property='tapped',value=1),c['steps'])
  self.assertIn(dict(op='assert_card',ref='drain_other_land',property='tapped',value=0),c['steps'])
  self.assertIn(dict(op='assert_card',ref='drain_nonmana_land',property='tapped',value=0),c['steps'])
  c=self.cases('Power Sink',['x_0_0'])[0]
  self.assertIn(dict(op='assert_card',ref='drain_other_land',property='tapped',value=0),c['steps'])
  self.assertIn(dict(op='assert_mana_total',player=1,value=58),c['steps'])
 def test_clash_uses_revealed_values_before_bottom_and_mills_spell_controller(self):
  c=self.cases('Broken Ambitions',['clash_bottom_source_0'])[0]
  self.assertIn(dict(op='choose_zone_card',player=0,value=self.rows['Scaled Wurm']['csvid']),c['steps'])
  self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=6,value=self.rows['Scaled Wurm']['csvid']),c['steps'])
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=3),c['steps'])
  c=self.cases('Broken Ambitions',['own_spell_0'])[0]
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Scaled Wurm']['csvid'],value=1),c['steps'])
  c=self.cases('Broken Ambitions',['clash_tie_1'])[0]
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=7),c['steps'])
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=7),c['steps'])

 def test_rites_ai_cap_retains_third_forest_without_trailing_cancel(self):
  human,ai=self.cases('Rites of Refusal',['resource_many_0','resource_many_1'])
  for c,owner,chosen,remaining in ((human,0,3,0),(ai,1,2,1)):
   self.assertEqual(sum(x['name']=='Forest' and x['player']==owner and x['zone']=='hand' for x in c['setup']),3)
   self.assertEqual(sum(x['op']=='choose_zone_card' and x['player']==owner and x['value']==self.rows['Forest']['csvid'] for x in c['steps']),chosen)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Forest']['csvid'],value=remaining),c['steps'])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Forest']['csvid'],value=chosen),c['steps'])
  self.assertIn(dict(op='choose_zone',player=0,value=-1),human['steps'])
  self.assertFalse(any(x['op']=='choose_zone' and x['player']==1 and x['value']==-1 for x in ai['steps']))

 def test_wave7_wizard_draw_is_independent_of_counter_tax_payment(self):
  for variant in ('tax_paid_0','tax_exact_1','two_wizards_0'):
   c=self.cases("Sage's Dousing",[variant])[0];owner=int(variant[-1])
   self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=6),c['steps'])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Forest']['csvid'],value=2),c['steps'])
  for variant in ('tax_paid_no_wizard_0','no_wizard_1','opposing_wizard_0','departed_wizard_1'):
   c=self.cases("Sage's Dousing",[variant])[0]
   self.assertIn(dict(op='assert_zone',player=int(variant[-1]),zone='library',value=7),c['steps'])
  c=self.cases("Sage's Dousing",['departed_wizard_0'])[0]
  self.assertLess(c['steps'].index(dict(op='destroy',ref='wizard_0')),c['steps'].index(dict(op='resolve_response')))
 def test_wave7_non_faerie_exiles_only_the_countered_spell(self):
  positive,negative=self.cases('Faerie Trickery',['opponent_1','forbidden_faerie_1'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=1),positive['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=0),positive['steps'])
  self.assertIn(dict(op='cast_response',ref='source',value=0),negative['steps'])
  self.assertIn(dict(op='assert_card',ref='spell',property='in_play',value=1),negative['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Faerie Miscreant']['csvid'],value=0),negative['steps'])
 def test_wave7_second_spell_uses_cast_history_and_isolates_illegal_casts(self):
  for variant,count in (('opponent_0',1),('opponent_1',1),('first_spell_0',0),('first_spell_1',0),('third_spell_0',2),('third_spell_1',2),('split_history_0',1)):
   c=self.cases('Second Guess',[variant])[0]
   self.assertEqual(sum(s['op']=='cast' and s.get('ref','').startswith('precursor_') for s in c['steps']),count)
   denied=variant.startswith(('first_spell','third_spell'))
   parent=c['steps'].index(dict(op='cast',ref='spell',value=1));announce=c['steps'].index(dict(op='announce_spell',ref='spell'))
   response=c['steps'].index(dict(op='cast_response',ref='source',value=int(not denied)))
   self.assertLess(parent,announce);self.assertLess(announce,response)
   self.assertFalse(any(s['op']=='choose_priority' for s in c['steps']))
   if variant.startswith(('first_spell','third_spell')):
    self.assertIn(dict(op='assert_card',ref='source',property='in_hand',value=1),c['steps'])
    self.assertIn(dict(op='assert_card',ref='spell',property='in_play',value=1),c['steps'])
    self.assertIn(dict(op='assert_mana_total',player=int(variant[-1]),value=60),c['steps'])
    self.assertIn(dict(op='resolve'),c['steps'])
  c=self.cases('Second Guess',['snapshot_0'])[0]
  self.assertEqual(sum(s['op']=='cast' and s.get('ref')=='precursor_0' for s in c['steps']),2)
  self.assertEqual(sum(s['op']=='cast_response' and s.get('ref')=='source' for s in c['steps']),2)
  self.assertEqual(sum(s['op']=='announce_spell' for s in c['steps']),2)
  self.assertIn(dict(op='assert_recorded_state'),c['steps'])
  self.assertFalse(any('deferred' in gap for gap in c['_recipe']['untested_components']))
  d=self.declaration('Second Guess');d['variants']=['first_spell_1'];p=extension.spec(d,self.oracle['Second Guess'])
  self.assertEqual(len(extension.variants(p)),32)
 def test_wave7_snapshot_restores_an_already_announced_parent(self):
  for owner in (0,1):
   c=self.cases('Second Guess',[f'announced_snapshot_{owner}'])[0]
   self.assertEqual(sum(s['op']=='cast' and s.get('ref')=='spell' for s in c['steps']),1)
   self.assertEqual(sum(s['op']=='announce_spell' for s in c['steps']),1)
   self.assertEqual(sum(s['op']=='cast_response' for s in c['steps']),2)
   self.assertLess(c['steps'].index(dict(op='announce_spell',ref='spell')),c['steps'].index(dict(op='capture')))
   self.assertLess(c['steps'].index(dict(op='capture')),c['steps'].index(dict(op='cast_response',ref='source',value=1)))
   self.assertIn(dict(op='assert_recorded_state'),c['steps'])
 def test_wave7_split_history_uses_a_legal_off_turn_instant_without_turn_flip(self):
  for owner in (0,1):
   c=self.cases('Second Guess',[f'split_history_{owner}'])[0]
   turns=[s for s in c['steps'] if s['op']=='turn']
   self.assertEqual(turns,[dict(op='turn',player=1-owner,value=0)])
   self.assertFalse(any(s['op']=='advance_turn' for s in c['steps']))
   precursor=next(x for x in c['setup'] if x['ref']=='precursor_0')
   self.assertEqual((precursor['name'],precursor['player']),('Reach Through Mists',owner))
   self.assertIn('Instant',self.oracle[precursor['name']]['type_line'])
   self.assertEqual(self.oracle[precursor['name']]['mana_cost'],'{U}')
   self.assertEqual(self.oracle[precursor['name']]['oracle_text'],'Draw a card.')
   before_parent=c['steps'][:c['steps'].index(dict(op='cast',ref='spell',value=1))]
   self.assertIn(dict(op='assert_mana',player=owner,color=2,value=9),before_parent)
   self.assertIn(dict(op='assert_mana_total',player=owner,value=59),before_parent)
   self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=6),before_parent)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Forest']['csvid'],value=2),before_parent)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Reach Through Mists']['csvid'],value=1),before_parent)
   for i,name in enumerate(extension.ordinary.LIBRARY[1:]):
    self.assertIn(dict(op='assert_zone_card',player=owner,zone='library',index=i,value=self.rows[name]['csvid']),before_parent)
   self.assertIn(dict(op='assert_mana_total',player=owner,value=57),c['steps'])
   ordinary=self.cases('Second Guess',[f'opponent_{owner}'])[0]
   self.assertEqual(next(x['name'] for x in ordinary['setup'] if x['ref']=='precursor_0'),'Ornithopter')
   self.assertTrue(all(s['player']==1-owner for s in ordinary['steps'] if s['op']=='turn'))
 def test_wave7_split_history_draw_witness_is_fully_pinned(self):
  for field,value in (('mana_cost','{1}'),('oracle_text','Draw two cards.'),('type_line','Sorcery — Arcane')):
   changed=copy.deepcopy(self.oracle);changed['Reach Through Mists'][field]=value
   with self.assertRaisesRegex(ValueError,'wave-seven counter witness'):extension.validate_witnesses(changed,self.rows)
 def test_wave7_source_and_target_subtype_pins_fail_closed(self):
  d=self.declaration('Faerie Trickery');d['variants']=['opponent_0']
  for key,value in (('schema',True),('extension',dict(wave=True)),('kind','any_spell')):
   changed=copy.deepcopy(d);changed['params'][key]=value
   with self.assertRaises(ValueError):extension.spec(changed,self.oracle['Faerie Trickery'])
  oracle=copy.deepcopy(self.oracle);oracle['Faerie Miscreant']['type_line']='Creature — Human Rogue'
  with self.assertRaisesRegex(ValueError,'wave-seven counter witness'):extension.validate_witnesses(oracle,self.rows)
  c=self.cases('Lay Bare',['snapshot_0'])[0]
  self.assertTrue(any('hand-inspection' in s for s in c['_recipe']['untested_components']))
 def test_wave7_full_sources_selected_variants_and_inputs_are_preserved(self):
  for name in extension.WAVE7_PROFILES:
   d=self.declaration(name);d['variants']=['opponent_0'];p=extension.spec(d,self.oracle[name]);d['variants']=extension.variants(p)
   before=copy.deepcopy((d,p,self.oracle));cases=extension.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)
   self.assertEqual(before,(d,p,self.oracle));self.assertEqual(len(cases),len(d['variants']))
   changed=copy.deepcopy(self.oracle[name]);changed['oracle_text']+=' '
   with self.assertRaises(ValueError):extension.spec(d,changed)
   for variants in ([],['opponent_0','opponent_0'],['unknown_0']):
    with self.assertRaises(ValueError):extension.spec(dict(d,variants=variants),self.oracle[name])
 def test_wave7_payment_receipt_precedes_resolution_for_both_controllers(self):
  for owner in (0,1):
   c=self.cases('Faerie Trickery',[f'payment_{owner}'])[0];resolved=c['steps'].index(dict(op='resolve_response'))
   cast=c['steps'].index(dict(op='cast_response',ref='source',value=1))
   self.assertIn(dict(op='assert_mana_total',player=owner,value=0),c['steps'][cast:resolved])
   self.assertIn(dict(op='assert_card',ref='source',property='in_hand',value=0),c['steps'][cast:resolved])
   self.assertIn(dict(op='assert_card',ref='source',property='in_hand',value=0),c['steps'][resolved:])

if __name__=='__main__':unittest.main()
