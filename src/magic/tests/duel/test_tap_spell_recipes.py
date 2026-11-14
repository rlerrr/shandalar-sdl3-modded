"""Independent recipe guards for reviewed fixed tap and untap spells."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from tap_spell_recipes import REVIEW,spec,variants,expand,validate_witnesses

class TapSpellRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
  cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Pressure Point'):
  return dict(family='tap_spell',version=2,oracle_text=self.oracle[name]['oracle_text'],params=copy.deepcopy(REVIEW['cards'][name]['params']),variants=['effect_0'])
 def cases(self,name='Pressure Point',selected=('effect_0',),oracle=None,rows=None):
  o=oracle or self.oracle;d=self.declaration(name);d['variants']=list(selected);p=spec(d,o[name])
  return expand(name,self.rows[name]['csvid'],d,p,list(selected),rows or self.rows,o)
 def test_all_reviews_expand_without_handlers(self):
  rows=copy.deepcopy(self.rows)
  for row in rows.values():row['handler']='card_dummy';row['mana_cost']='{99}'
  for name in REVIEW['cards']:
   self.assertEqual(self.cases(name),self.cases(name,rows=rows))
 def test_dispatch_matches_direct_and_rejects_custom_setup(self):
  from recipes import expand as dispatch
  name='Pressure Point';d=self.declaration(name);csvid=self.rows[name]['csvid']
  self.assertEqual(self.cases(name),dispatch(name,csvid,d,self.oracle[name],self.rows,oracle_context=self.oracle))
  with self.assertRaises(ValueError):dispatch(name,csvid,dict(d,support_permanents=['Forest']),self.oracle[name],self.rows,oracle_context=self.oracle)
 def test_whole_source_pin_and_params_fail_closed(self):
  for key,value in (('oracle_text','Tap target creature.'),('mana_cost','{9}'),('type_line','Enchantment'),('layout','split'),('colors',['R'])):
   record=dict(self.oracle['Pressure Point'],**{key:value});d=self.declaration();d['oracle_text']=record['oracle_text']
   with self.assertRaises(ValueError):spec(d,record)
  for params in ({},dict(self.declaration()['params'],count=True),dict(self.declaration()['params'],draw=2),dict(self.declaration()['params'],gaps=['invented'])):
   d=self.declaration();d['params']=params
   with self.assertRaises(ValueError):spec(d,self.oracle['Pressure Point'])
 def test_unreviewed_card_and_extra_clause_rejected(self):
  d=self.declaration();record=dict(self.oracle['Pressure Point'],oracle_text=d['oracle_text']+' Gain 3 life.');d['oracle_text']=record['oracle_text']
  with self.assertRaises(ValueError):spec(d,record)
  with self.assertRaises(ValueError):spec(d,self.oracle['Twiddle'])
 def test_variants_are_explicit_unique_and_controller_paired(self):
  for name in REVIEW['cards']:
   p=spec(self.declaration(name),self.oracle[name]);vs=variants(p)
   self.assertEqual(len(vs),len(set(vs)));self.assertEqual({v[:-2] for v in vs if v.endswith('_0')},{v[:-2] for v in vs if v.endswith('_1')})
  for selected in ([],['missing_0'],['effect_0','effect_0']):
   with self.assertRaises(ValueError):self.cases(selected=selected)
 def test_positive_receipts_precede_cleanup(self):
  c=self.cases(selected=['cleanup_0'])[0];s=c['steps'];cut=s.index(dict(op='cleanup'))
  self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=1),s[:cut])
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=5),s[cut:])
 def test_every_source_has_a_primary_noop_sensitive_tap_receipt(self):
  for name in REVIEW['cards']:
   s=self.cases(name)[0]['steps'];cut=s.index(dict(op='resolve'))
   before={x['ref']:x['value'] for x in s[:cut] if x['op']=='assert_card' and x.get('property')=='tapped'}
   after={x['ref']:x['value'] for x in s[cut:] if x['op']=='assert_card' and x.get('property')=='tapped'}
   self.assertTrue(any(after.get(ref)!=value for ref,value in before.items()),name)
 def test_cantrip_draw_fizzles_when_only_target_leaves(self):
  for v in ('lost_target_0','bounced_target_1'):
   c=self.cases(selected=[v])[0];s=c['steps'];owner=int(v[-1])
   self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=6),s)
   self.assertIn(dict(op='assert_stack',value=1),s)
 def test_multiple_targets_and_unchosen_controls(self):
  c=self.cases('Downpour')[0];s=c['steps']
  for ref in ('recipient','recipient_1','recipient_2'):
   self.assertIn(dict(op='choose_target',player=0,ref=ref),s)
   self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=1),s)
  self.assertIn(dict(op='assert_card',ref='unselected',property='tapped',value=0),s)
 def test_optional_zero_and_partial_target_choices_are_real(self):
  for v,count in (('zero_targets_0',0),('one_target_0',1)):
   c=self.cases('Downpour',[v])[0]
   self.assertEqual(sum(x['op']=='choose_target' for x in c['steps']),count)
   self.assertIn(dict(op='choose_cancel_target',player=0),c['steps'])
  s=self.cases('Downpour',['wrong_type_0'])[0]['steps']
  self.assertIn(dict(op='require_can_cast',ref='source',value=1),s)
  self.assertIn(dict(op='cast',ref='source',value=1),s)
  self.assertFalse(any(x['op'] in ('choose_target','choose_cancel_target') for x in s))
 def test_own_untap_and_opponent_controls(self):
  s=self.cases('Vitalize')[0]['steps']
  self.assertIn(dict(op='assert_card',ref='army_0_0',property='tapped',value=0),s)
  self.assertIn(dict(op='assert_card',ref='army_1_0',property='tapped',value=0),s)
  self.assertIn(dict(op='assert_card',ref='army_1_1',property='tapped',value=1),s)
  s=self.cases('Rude Awakening')[0]['steps']
  self.assertIn(dict(op='assert_card',ref='army_1_6',property='tapped',value=1),s)
  self.assertIn(dict(op='assert_card',ref='army_0_6',property='tapped',value=0),s)
 def test_modal_and_alternate_cost_choices_explicit(self):
  for name,choice in (('Ivory Charm',1),('Rude Awakening',0),('Naya Charm',2),('Aether Shockwave',1)):
   self.assertIn(dict(op='choose_dialog',player=0,value=choice),self.cases(name)[0]['steps'])
  self.assertNotIn(dict(op='choose_dialog',player=0,value=1),self.cases('Aether Shockwave',['countered_0'])[0]['steps'])
  for name in ('Mind Games','Blustersquall'):
   self.assertIn(dict(op='choose_dialog',player=0,value=1),self.cases(name)[0]['steps'])
   self.assertFalse(any(x['op']=='choose_dialog' for x in self.cases(name,['payment_0'])[0]['steps']))
 def test_source_countered_retains_targets_and_library(self):
  c=self.cases(selected=['countered_1'])[0];s=c['steps']
  self.assertIn(dict(op='choose_priority',ref='counter'),s);self.assertIn(dict(op='assert_zone',player=1,zone='library',value=6),s)
  self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=0),s)
 def test_repeated_cast_has_positive_reset_between_resolutions(self):
  s=self.cases('Refocus',['two_copies_0'])[0]['steps']
  first=s.index(dict(op='cast',ref='source',value=1));second=s.index(dict(op='cast',ref='second',value=1));reset=s.index(dict(op='cast',ref='reset',value=1))
  self.assertLess(first,reset);self.assertLess(reset,second)
  self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=1),s[reset:second])
 def test_later_untap_recipient_enters_tapped_and_stays_tapped(self):
  c=self.cases('Vitalize',['late_entry_0'])[0]
  self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='late'),'Diregraf Ghoul')
  self.assertIn(dict(op='assert_card',ref='late',property='tapped',value=1),c['steps'])
 def test_snapshot_replays_maintain_exact_other_zones(self):
  for v in ('ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0'):
   s=self.cases(selected=[v])[0]['steps'];cut=s.index(dict(op='restore'))
   self.assertIn(dict(op='assert_recorded_state'),s)
   self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['zone']=='exile' for x in s[cut:]))
   self.assertTrue(any(x['op']=='assert_zone_card' and x['zone']=='library' for x in s[cut:]))
 def test_real_combat_attack_permission_and_no_sorcery_offturn(self):
  self.assertIn(dict(op='attack',ref='recipient',value=0),self.cases(selected=['combat_attack_0'])[0]['steps'])
  self.assertIn(dict(op='attack',ref='recipient',value=1),self.cases('Refocus',['combat_attack_0'])[0]['steps'])
  p=spec(self.declaration('Mobilize'),self.oracle['Mobilize']);self.assertNotIn('opposing_turn_0',variants(p))
  p=spec(self.declaration('Early Frost'),self.oracle['Early Frost']);self.assertNotIn('combat_attack_0',variants(p))
 def test_payment_receipts_disable_battlefield_mana_sources(self):
  for name in ('Riptide','Ensnare','Dramatic Reversal',"Dream's Grip"):
   for v in ('payment_0','insufficient_mana_0','wrong_color_1'):
    c=self.cases(name,[v])[0];owner=int(v[-1])
    self.assertTrue(all('tapped' in x.get('state',[]) for x in c['setup'] if x['player']==owner and x['zone']=='battlefield' and x['name'] in ('Forest','Island','Sol Ring')))
  s=self.cases("Dream's Grip",['payment_0'])[0]['steps']
  self.assertIn(dict(op='set_mana',player=0,color=0,value=1),s)
  self.assertIn(dict(op='assert_mana',player=0,color=0,value=1),s)
  self.assertFalse(any(x['op']=='choose_cancel_mana' for x in s))
 def test_witness_mutations_rejected(self):
  for name in ('Forest','Grizzly Bears','Counterspell','Twiddle'):
   oracle=copy.deepcopy(self.oracle);oracle[name]['oracle_text']='changed'
   with self.assertRaises(ValueError):validate_witnesses(oracle,self.rows)
 def test_pending_shroud_uses_flash_and_suppresses_cantrip(self):
  c=self.cases(selected=['target_shrouded_pending_1'])[0]
  self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='response'),"Alexi's Cloak")
  self.assertIn('Flash',self.oracle["Alexi's Cloak"]['keywords'])
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=6),c['steps'])
  self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=0),c['steps'])
 def test_extra_mechanics_are_pinned_and_inactive_fixtures_safe(self):
  for name in ('Dispatch','Send to Sleep','Call to Glory','Battle Cry'):
   c=self.cases(name)[0]
   self.assertTrue(c['_recipe']['inactive_conditions']);self.assertTrue(c['_recipe']['untested_components'])
  p=spec(self.declaration('Blustersquall'),self.oracle['Blustersquall']);self.assertNotIn('own_target_0',variants(p));self.assertNotIn('wrong_type_0',variants(p))

class TapSpell3RecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):
  from tap_spell_recipes import REVIEW
  return dict(family='tap_spell',version=2,oracle_text=self.oracle[name]['oracle_text'],params=copy.deepcopy(REVIEW['advanced_cards'][name]['params']),variants=['effect_0'])
 def cases(self,name,vs=None,rows=None):
  from tap_spell_recipes import expand as extend
  d=self.declaration(name);p=spec(d,self.oracle[name]);vs=vs or [('effect_x2_0' if p['variable'] else 'effect_0')];d['variants']=vs
  return extend(name,self.rows[name]['csvid'],d,p,vs,rows or self.rows,self.oracle)
 def test_schema3_requires_whole_pin_and_typed_review(self):
  d=self.declaration('Backlash');d['params']['stats']=[True,0]
  with self.assertRaises(ValueError):spec(d,self.oracle['Backlash'])
  o=dict(self.oracle['Backlash'],oracle_text=self.oracle['Backlash']['oracle_text']+' Draw a card.');d=self.declaration('Backlash');d['oracle_text']=o['oracle_text']
  with self.assertRaises(ValueError):spec(d,o)
 def test_prior_schema_is_still_distinct(self):
  from tap_spell_recipes import REVIEW
  self.assertEqual(len(REVIEW['cards']),33)
  for name,entry in REVIEW['cards'].items():
   d=dict(family='tap_spell',version=2,oracle_text=entry['oracle']['oracle_text'],params=entry['params'],variants=['effect_0'])
   self.assertFalse(spec(d,self.oracle[name]).get('advanced3',False))
 def test_control_change_is_rebound_and_expires_at_cleanup(self):
  s=self.cases('Act of Treason',['cleanup_0'])[0]['steps']
  self.assertIn(dict(op='bind_permanent_copy',ref='recipient',player=0,value=106,index=0),s)
  cut=s.index(dict(op='cleanup'));self.assertIn(dict(op='bind_permanent_copy',ref='recipient',player=1,value=106,index=0),s[cut:])
  self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=0),s[:cut])
 def test_theft_haste_has_real_combat_damage(self):
  c=self.cases('Act of Treason',['combat_attack_1'])[0];s=c['steps']
  self.assertIn('sick',next(x['state'] for x in c['setup'] if x['ref']=='recipient'))
  self.assertIn(dict(op='attack',ref='recipient',value=1),s)
  self.assertIn(dict(op='combat_damage'),s);self.assertIn(dict(op='assert_life',player=0,value=18),s)
 def test_stolen_death_uses_owner_not_new_controller(self):
  s=self.cases('Act of Treason',['stolen_destroyed_0'])[0]['steps']
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=106,value=2),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=106,value=1),s)
 def test_power_and_mana_value_receipts_are_independent(self):
  s=self.cases('Backlash',['large_target_0'])[0]['steps'];self.assertIn(dict(op='assert_life',player=1,value=13),s)
  s=self.cases('Energy Tap',['large_target_0'])[0]['steps'];self.assertIn(dict(op='assert_mana',player=0,color=0,value=68),s)
 def test_untapped_and_controller_predicates_are_negative_permissions(self):
  for name,vs in (('Backlash',['wrong_state_0']),('Energy Tap',['wrong_controller_1'])):
   s=self.cases(name,vs)[0]['steps'];self.assertIn(dict(op='can_cast',ref='source',value=0),s)
   self.assertFalse(any(x['op']=='choose_target' for x in s))
 def test_x_has_real_payment_and_exact_target_counts(self):
  for x in (0,1,2,3):
   s=self.cases('Word of Binding',[f'payment_x{x}_0'])[0]['steps']
   self.assertFalse(any(z['op']=='choose_x' for z in s))
   self.assertEqual(sum(x['op']=='choose_target' for x in s),x)
   self.assertIn(dict(op='assert_mana',player=0,color=1,value=0),s)
 def test_x_zero_with_no_targets_is_legally_castable(self):
  s=self.cases('Word of Binding',['no_targets_x0_1'])[0]['steps']
  self.assertIn(dict(op='require_can_cast',ref='source',value=1),s);self.assertIn(dict(op='cast',ref='source',value=1),s)
  self.assertFalse(any(x['op']=='choose_target' for x in s))
 def test_target_count_x_ui_has_a_shrouded_unselected_control(self):
  c=self.cases('Word of Binding')[0]
  self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='unselected'),'Deadly Insect')
  self.assertIn('Shroud',self.oracle['Deadly Insect']['keywords'])
 def test_stats_observed_before_cleanup_and_reset_afterward(self):
  s=self.cases('Hope and Glory',['cleanup_0'])[0]['steps'];cut=s.index(dict(op='cleanup'))
  for ref in ('recipient','recipient_1'):
   self.assertIn(dict(op='assert_card',ref=ref,property='power',value=3),s[:cut])
   self.assertIn(dict(op='assert_card',ref=ref,property='power',value=2),s[cut:])
 def test_mass_inversion_is_simultaneous_for_mixed_states(self):
  s=self.cases('Breaking Wave')[0]['steps']
  for owner in (0,1):
   self.assertIn(dict(op='assert_card',ref=f'army_{owner}_0',property='tapped',value=0),s)
   self.assertIn(dict(op='assert_card',ref=f'army_{owner}_1',property='tapped',value=1),s)
   self.assertIn(dict(op='assert_card',ref=f'artifact_{owner}',property='tapped',value=1),s)
 def test_breaking_wave_menu_depends_on_owner_and_extra_affordability(self):
  self.assertIn(dict(op='choose_dialog',player=0,value=0),self.cases('Breaking Wave')[0]['steps'])
  for v in ('effect_1','payment_0'):
   self.assertFalse(any(s['op']=='choose_dialog' for s in self.cases('Breaking Wave',[v])[0]['steps']))
 def test_response_uses_parent_context_and_fully_drains_stack(self):
  s=self.cases('Act of Treason',['bounced_target_0'])[0]['steps'];i=s.index(dict(op='cast_response',ref='response',value=1))
  self.assertEqual(s[i+1],dict(op='assert_stack',value=2));self.assertEqual(s[i+2],dict(op='resolve_response'));self.assertEqual(s[i+3],dict(op='assert_stack',value=0))
 def test_real_repeated_cast_has_a_positive_reset(self):
  s=self.cases('Hope and Glory',['two_copies_0'])[0]['steps']
  first=s.index(dict(op='cast',ref='source',value=1));reset=s.index(dict(op='cast',ref='reset',value=1));second=s.index(dict(op='cast',ref='second',value=1))
  self.assertLess(first,reset);self.assertLess(reset,second);self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=4),s[second:])
 def test_new_guards_ignore_handlers_and_replay_decisions(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy';r['mana_cost']='{99}'
  self.assertEqual(self.cases('Act of Treason'),self.cases('Act of Treason',rows=rows))
  for c in self.cases('Word of Binding',['ready_snapshot_x2_0','pending_snapshot_x2_1','resolved_snapshot_x2_0']):
   s=c['steps'];cut=s.index(dict(op='restore'));self.assertIn(dict(op='assert_recorded_state'),s)
   self.assertTrue(any(x['op']=='assert_zone_multiplicity' for x in s[cut:]))
 def test_optional_zero_targets_are_legal_and_mandatory_tokens_still_create(self):
  for name in ('Synchronized Strike','Triton Tactics','Adverse Conditions','Expose Evil'):
   p=spec(self.declaration(name),self.oracle[name]);self.assertNotIn('no_targets_0',variants(p))
   c=self.cases(name,['zero_targets_0'])[0];self.assertIn(dict(op='cast',ref='source',value=1),c['steps'])
   self.assertFalse(any(s['op']=='choose_target' for s in c['steps']))
  s=self.cases('Expose Evil',['zero_targets_0'])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Clue']['csvid'],value=1),s)
 def test_all_targets_lost_suppresses_investigate(self):
  s=self.cases('Press for Answers',['lost_target_1'])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Clue']['csvid'],value=0),s)
  s=self.cases('Expose Evil',['lost_target_1'])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Clue']['csvid'],value=1),s)
  for name,token in (('Expose Evil','Clue'),('Adverse Conditions','Eldrazi Scion')):
   s=self.cases(name,['only_target_lost_0'])[0]['steps'];self.assertEqual(sum(x['op']=='choose_target' for x in s),1);self.assertIn(dict(op='destroy',ref='recipient'),s);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows[token]['csvid'],value=0),s)
 def test_x_cost_and_target_count_are_independent(self):
  p=spec(self.declaration('Unwilling Recruit'),self.oracle['Unwilling Recruit']);self.assertNotIn('no_targets_x0_0',variants(p));self.assertIn('no_targets_x2_0',variants(p))
  s=self.cases('Unwilling Recruit',['effect_x0_0'])[0]['steps'];self.assertIn(dict(op='choose_x',player=0,value=0),s);self.assertEqual(sum(z['op']=='choose_target' for z in s),1)
  s=self.cases('Unwilling Recruit',['effect_x3_1'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=5),s)
 def test_power_limit_negative_has_no_eligible_control(self):
  c=self.cases('Wrangle',['wrong_power_0'])[0];self.assertIn(dict(op='can_cast',ref='source',value=0),c['steps'])
  self.assertEqual([x['name'] for x in c['setup'] if x['zone']=='battlefield'],['Scaled Wurm'])
 def test_flying_damage_is_visible_before_cleanup(self):
  s=self.cases('Winter Blast',['flying_cleanup_x2_1'])[0]['steps'];cut=s.index(dict(op='cleanup'))
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=2),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s[cut:])
 def test_rebound_exile_is_conditional_on_successful_resolution(self):
  for v,count in (('effect_0',2),('lost_target_0',1),('countered_0',1)):
   s=self.cases("Ojutai's Breath",[v])[0]['steps'];self.assertIn(dict(op='assert_zone',player=0,zone='exile',value=count),s)
 def test_trample_real_block_and_mana_theft_have_functional_receipts(self):
  s=self.cases('Traitorous Blood',['trample_block_0'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker',other='recipient',value=1),s);self.assertIn(dict(op='assert_life',player=1,value=19),s)
  s=self.cases('Metallic Mastery',['mana_source_1'])[0]['steps'];self.assertIn(dict(op='activate',ref='recipient',value=1),s);self.assertIn(dict(op='assert_mana',player=1,color=0,value=60),s)
 def test_flash_conscription_never_spends_white_in_these_fixtures(self):
  for v in ('effect_0','payment_1','combat_attack_0'):
   s=self.cases('Flash Conscription',[v])[0]['steps'];self.assertIn(dict(op='set_mana',player=int(v[-1]),color=5,value=0),s);self.assertIn(dict(op='assert_mana',player=int(v[-1]),color=5,value=0),s)
 def test_reviewed_target_ui_and_repeat_controller_rebinding(self):
  for v,count in (('wrong_type_0',1),('zero_targets_1',2),('one_target_0',1)):
   s=self.cases('Synchronized Strike',[v])[0]['steps'];self.assertEqual(sum(x['op']=='choose_cancel_target' for x in s),count)
  c=self.cases('Ray of Command',['two_copies_0'])[0];s=c['steps'];second=s.index(dict(op='cast',ref='second',value=1));cleanup=s.index(dict(op='cleanup'));self.assertLess(cleanup,second)
  self.assertFalse(any(x['name']=='Twiddle' for x in c['setup']));self.assertIn(dict(op='bind_permanent_copy',ref='recipient',player=1,value=106,index=0),s[cleanup:second])
  c=self.cases('Metallic Mastery',['cleanup_0'])[0];self.assertEqual(sum(x['name']=='Phyrexian Hulk' and x['zone']=='battlefield' for x in c['setup']),1)

class TapSpell4RecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):
  from tap_spell_recipes import REVIEW
  return dict(family='tap_spell',version=2,oracle_text=self.oracle[name]['oracle_text'],params=copy.deepcopy(REVIEW['extension_cards'][name]['params']),variants=['effect_0'])
 def cases(self,name,vs):
  from tap_spell_recipes import expand as extend
  d=self.declaration(name);p=spec(d,self.oracle[name]);d['variants']=vs
  return extend(name,self.rows[name]['csvid'],d,p,vs,self.rows,self.oracle)
 def test_prior_review_tables_and_full_new_pins_remain_distinct(self):
  from tap_spell_recipes import REVIEW
  self.assertEqual(len(REVIEW['cards'])+len(REVIEW['advanced_cards']),64)
  d=self.declaration('Sudden Storm');d['params']['scry']=True
  with self.assertRaises(ValueError):spec(d,self.oracle['Sudden Storm'])
 def test_scry_keep_bottom_and_all_target_fizzle_have_ordered_receipts(self):
  for name in ('Portent of Betrayal','Sudden Storm'):
   s=self.cases(name,['scry_bottom_0'])[0]['steps'];self.assertIn(dict(op='choose_zone',player=0,value=0),s)
   self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=0,value=self.rows['Plains']['csvid']),s)
  s=self.cases('Sudden Storm',['only_target_lost_0'])[0]['steps'];self.assertFalse(any(x['op'] in ('choose_zone','choose_optional_zone') for x in s))
  s=self.cases('Sudden Storm',['lost_target_0'])[0]['steps'];self.assertIn(dict(op='choose_zone',player=0,value=-1),s)
 def test_x_menu_uses_fixed_x_targets_and_zero_still_scries(self):
  s=self.cases('Glimpse the Sun God',['effect_x0_0'])[0]['steps'];self.assertIn(dict(op='choose_x',player=0,value=0),s)
  self.assertFalse(any(x['op']=='choose_target' for x in s));self.assertIn(dict(op='choose_zone',player=0,value=-1),s)
 def test_reset_timing_land_scope_and_functional_mana(self):
  s=self.cases('Reset',['effect_0'])[0]['steps'];self.assertEqual(s[0],dict(op='turn',player=1,value=0))
  self.assertIn(dict(op='assert_card',ref='land_0',property='tapped',value=0),s);self.assertIn(dict(op='assert_card',ref='land_1',property='tapped',value=1),s)
  s=self.cases('Reset',['own_turn_denied_0'])[0]['steps'];self.assertIn(dict(op='can_cast',ref='source',value=0),s)
  s=self.cases('Reset',['mana_source_0'])[0]['steps'];self.assertIn(dict(op='activate',ref='land_0',value=1),s);self.assertIn(dict(op='assert_mana',player=0,color=3,value=21),s)
 def test_thoughtweft_two_sides_and_hybrid_payment_are_explicit(self):
  s=self.cases('Thoughtweft Gambit',['effect_1'])[0]['steps'];self.assertIn(dict(op='set_mana',player=1,color=5,value=0),s)
  for ref,value in (('army_0_1',1),('army_1_0',0)):
   self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=value),s)
 def test_turnabout_modes_select_player_type_and_direction(self):
  s=self.cases('Turnabout',['mode_land_untap_0'])[0]['steps'];self.assertIn(dict(op='choose_player',player=0,value=1),s)
  self.assertIn(dict(op='choose_dialog',player=0,value=0),s);self.assertIn(dict(op='choose_dialog',player=0,value=1),s)
  self.assertIn(dict(op='assert_card',ref='land_1',property='tapped',value=0),s);self.assertIn(dict(op='assert_card',ref='land_0',property='tapped',value=1),s)
 def test_optional_opponent_only_cast_remains_legal_with_zero_targets(self):
  s=self.cases('Mass Mutiny',['wrong_controller_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s);self.assertFalse(any(x['op']=='choose_target' for x in s))
  c=self.cases('Mass Mutiny',['two_copies_1'])[0];s=c['steps'];self.assertIn(dict(op='choose_target',player=1,ref='unselected'),s)
  self.assertFalse(any(x['name']=='Twiddle' for x in c['setup']))
 def test_scry_replay_and_response_choices_follow_actual_resolution(self):
  s=self.cases('Sudden Storm',['bounced_target_0'])[0]['steps'];a=s.index(dict(op='cast_response',ref='response',value=1));b=s.index(dict(op='resolve_response'))
  self.assertIn(dict(op='choose_zone',player=0,value=-1),s[a:b])
  s=self.cases('Portent of Betrayal',['pending_snapshot_1'])[0]['steps'];r=s.index(dict(op='restore'));self.assertIn(dict(op='choose_optional_zone',player=1,value=self.rows['Forest']['csvid']),s[r:])
 def test_mass_control_uses_unique_creatures_and_owner_aware_cleanup(self):
  c=self.cases('Insurrection',['cleanup_0'])[0];creatures=[x['name'] for x in c['setup'] if x['zone']=='battlefield' and 'Creature' in self.oracle[x['name']]['type_line']]
  self.assertEqual(len(creatures),len(set(creatures)));s=c['steps'];cut=s.index(dict(op='cleanup'))
  self.assertIn(dict(op='bind_permanent_copy',ref='army_1_0',player=0,value=self.rows['Runeclaw Bear']['csvid'],index=0),s[:cut]);self.assertIn(dict(op='bind_permanent_copy',ref='army_1_0',player=1,value=self.rows['Runeclaw Bear']['csvid'],index=0),s[cut:])
  s=self.cases('Reins of Power',['effect_0'])[0]['steps'];self.assertIn(dict(op='bind_permanent_copy',ref='army_0_0',player=1,value=self.rows['Grizzly Bears']['csvid'],index=0),s)
 def test_modal_power_boundary_and_radiance_colorless_control(self):
  s=self.cases('Mob Rule',['small_creatures_0'])[0]['steps'];self.assertIn(dict(op='choose_dialog',player=0,value=1),s)
  self.assertIn(dict(op='assert_card',ref='army_1_2',property='tapped',value=1),s);self.assertIn(dict(op='assert_card',ref='army_1_0',property='tapped',value=0),s)
  s=self.cases('Rally the Righteous',['colorless_target_0'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=7),s);self.assertIn(dict(op='assert_card',ref='radiance_colorless',property='power',value=5),s)
 def test_mass_draw_and_flying_double_strike_have_functional_receipts(self):
  s=self.cases('Intellectual Offering',['two_copies_0'])[0]['steps']
  for pl in (0,1):self.assertIn(dict(op='assert_zone',player=pl,zone='library',value=12),s)
  s=self.cases('Flying Crane Technique',['flying_block_0'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker',other='army_0_0',value=0),s);self.assertIn(dict(op='assert_life',player=1,value=16),s)
  s=self.cases('Flying Crane Technique',['keyword_cleanup_0'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker',other='army_0_0',value=1),s);self.assertIn(dict(op='assert_card',ref='army_0_0',property='damage',value=1),s)
 def test_targeted_prevention_distinguishes_by_from_to_and_by(self):
  s=self.cases('Telekinesis',['prevent_blocked_0'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),s)
  s=self.cases('Foxfire',['prevent_blocked_0'])[0]['steps'];self.assertFalse(any(x['op']=='assert_card' and x.get('ref')=='recipient' and x.get('property')=='damage' and x['value']!=0 for x in s))
  self.assertIn(dict(op='attack',ref='recipient',value=1),s);self.assertIn(dict(op='block',ref='fog_blocker',other='recipient',value=1),s)
 def test_blocker_only_tap_preserves_a_vigilant_attacker(self):
  c=self.cases('Spore Cloud',['prevent_blocked_0'])[0];s=c['steps'];self.assertIn('Vigilance',self.oracle[next(x['name'] for x in c['setup'] if x['ref']=='fog_attacker')]['keywords'])
  self.assertIn(dict(op='assert_card',ref='fog_attacker',property='tapped',value=0),s);self.assertIn(dict(op='assert_card',ref='fog_blocker',property='tapped',value=1),s)
 def test_prevention_expiry_first_proves_damage_was_prevented(self):
  s=self.cases('Tangle',['prevent_expired_0'])[0]['steps'];cut=s.index(dict(op='cleanup'))
  self.assertIn(dict(op='assert_life',player=0,value=20),s[:cut]);self.assertIn(dict(op='assert_life',player=0,value=16),s[cut:])
  self.assertIn(dict(op='cast',ref='expiry_reset',value=1),s);self.assertEqual(sum(x['op']=='combat_damage' for x in s),2)
 def test_any_number_selection_has_real_zero_and_three_target_casts(self):
  s=self.cases('Energy Arc',['three_targets_0'])[0]['steps'];self.assertEqual(sum(x['op']=='choose_target' for x in s),3);self.assertIn(dict(op='choose_cancel_target',player=0),s)
  self.assertFalse(any(x['op']=='choose_x' for x in s))
  s=self.cases('Energy Arc',['zero_targets_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s);self.assertFalse(any(x['op']=='choose_target' for x in s))
 def test_auto_opponent_and_filtered_target_loop_do_not_queue_unused_choices(self):
  s=self.cases('Reins of Power',['effect_0'])[0]['steps'];self.assertFalse(any(x['op']=='choose_player' for x in s))
  s=self.cases('Energy Arc',['prevent_blocked_1'])[0]['steps'];self.assertFalse(any(x['op']=='choose_cancel_target' for x in s))
  s=self.cases('Energy Arc',['prevent_blocked_0'])[0]['steps'];self.assertIn(dict(op='choose_cancel_target',player=0),s)
  s=self.cases('Flying Crane Technique',['keyword_cleanup_0'])[0]['steps'];cleanup=s.index(dict(op='cleanup'));attack=s.index(dict(op='begin_attackers'));self.assertIn(dict(op='turn',player=0,value=0),s[cleanup:attack])
 def test_normal_convoke_spends_printed_cost_and_disables_creature_funding_in_negatives(self):
  s=self.cases('Overwhelm',['payment_0'])[0]['steps'];self.assertIn(dict(op='choose_dialog',player=0,value=1),s);self.assertIn(dict(op='assert_mana',player=0,color=3,value=0),s)
  c=self.cases('Overwhelm',['insufficient_mana_0'])[0]
  self.assertTrue(all('tapped' in x.get('state',[]) for x in c['setup'] if x['zone']=='battlefield' and x['player']==0 and 'Creature' in self.oracle[x['name']]['type_line']))
 def test_fixed_life_draw_and_hybrid_components_have_real_receipts(self):
  s=self.cases('Meditation Puzzle',['effect_1'])[0]['steps'];self.assertIn(dict(op='assert_life',player=1,value=28),s)
  s=self.cases('Reverse Engineer',['effect_1'])[0]['steps'];self.assertIn(dict(op='assert_zone',player=1,zone='library',value=15),s)
  s=self.cases('Barkshell Blessing',['combat_attack_0'])[0]['steps'];self.assertIn(dict(op='set_mana',player=0,color=5,value=0),s);self.assertIn(dict(op='assert_life',player=1,value=16),s)
 def test_shared_discovery_pays_with_sick_shrouded_creatures_before_resolution(self):
  c=self.cases('Shared Discovery',['effect_0'])[0];s=c['steps'];cast=s.index(dict(op='cast',ref='source',value=1));resolve=s.index(dict(op='resolve'))
  resources=[x for x in c['setup'] if x['ref'].startswith('cost_') and x['ref'][-1].isdigit()]
  self.assertEqual(len(resources),4);self.assertTrue(all('sick' in x['state'] for x in resources));self.assertIn('Deadly Insect',[x['name'] for x in resources])
  for x in resources:self.assertIn(dict(op='assert_card',ref=x['ref'],property='tapped',value=1),s[cast:resolve])
  s=self.cases('Shared Discovery',['countered_0'])[0]['steps']
  for i in range(4):self.assertIn(dict(op='assert_card',ref='cost_'+str(i),property='tapped',value=1),s)
 def test_shared_discovery_cost_restrictions_and_non_target_reset(self):
  for v in ('cost_tapped_0','cost_wrong_controller_1','cost_noncreature_0','cost_too_few_1','empty_battlefield_0'):
   s=self.cases('Shared Discovery',[v])[0]['steps'];self.assertIn(dict(op='can_cast',ref='source',value=0),s);self.assertFalse(any(x['op']=='choose_target' for x in s))
  c=self.cases('Shared Discovery',['two_copies_0'])[0];s=c['steps'];self.assertIn('Mobilize',[x['name'] for x in c['setup']]);self.assertEqual(sum(x['op']=='choose_target' for x in s),8)
  self.assertIn(dict(op='cast',ref='cost_reset',value=1),s);self.assertIn(dict(op='assert_mana',player=0,color=2,value=18),s);self.assertIn(dict(op='assert_mana',player=0,color=3,value=19),s)
 def test_sweeper_repeat_casts_a_fresh_creature_and_preserves_owner_zones(self):
  c=self.cases("Bontu's Last Reckoning",['two_copies_1'])[0];s=c['steps'];first=s.index(dict(op='cast',ref='source',value=1));fresh=s.index(dict(op='cast',ref='sweep_later',value=1));second=s.index(dict(op='cast',ref='second',value=1))
  self.assertLess(first,fresh);self.assertLess(fresh,second);self.assertFalse(any(x['name']=='Twiddle' for x in c['setup']))
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=3),s)
 def test_starting_life_assignment_is_not_gain_and_repeat_resets_only_after_positive(self):
  s=self.cases("Oketra's Last Mercy",['two_copies_0'])[0]['steps'];first=s.index(dict(op='cast',ref='source',value=1));reset=next(i for i,x in enumerate(s) if i>first and x==dict(op='set_life',player=0,value=7))
  self.assertIn(dict(op='assert_life',player=0,value=20),s[first:reset]);self.assertEqual(sum(x==dict(op='cast',ref='second',value=1) for x in s),1)
  s=self.cases("Oketra's Last Mercy",['countered_1'])[0]['steps'];self.assertIn(dict(op='assert_life',player=1,value=7),s)
 def test_snake_characteristics_and_fateful_hour_inactivity_are_explicit(self):
  c=self.cases("Rhonas's Last Stand",['effect_0'])[0]
  for key,value in (('power',5),('toughness',4),('color',8),('subtype_Snake',1)):
   self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows['Snake']['csvid'],property=key,value=value),c['steps'])
  c=self.cases('Clinging Mists',['prevent_blocked_0'])[0];self.assertIn('life above 5 at every resolution',c['_recipe']['inactive_conditions']);self.assertFalse(any(s['op']=='set_life' for s in c['steps']))

class Wave5TapMechanicsTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):
  review={**REVIEW['next_untap_cards'],**REVIEW['mechanic_cards']}[name]
  return dict(family='tap_spell',version=2,oracle_text=self.oracle[name]['oracle_text'],params=copy.deepcopy(review['params']),variants=['next_step_0' if name in REVIEW['next_untap_cards'] else 'effect_x2_0' if review['params']['schema']==6 else 'effect_0'])
 def cases(self,name,vs):
  d=self.declaration(name);d['variants']=list(vs);p=spec(d,self.oracle[name]);return expand(name,self.rows[name]['csvid'],d,p,vs,self.rows,self.oracle)
 def test_explicit_review_types_and_whole_pin_fail_closed(self):
  for name in (*REVIEW['next_untap_cards'],*REVIEW['mechanic_cards']):
   d=self.declaration(name)
   for value in (True,1,3):
    bad=copy.deepcopy(d);bad['params']['schema']=value
    with self.assertRaises(ValueError):spec(bad,self.oracle[name])
   with self.assertRaises(ValueError):spec(d,dict(self.oracle[name],oracle_text=d['oracle_text']+' Draw a card.'))
 def test_selected_variants_fail_closed_through_public_dispatch(self):
  from recipes import expand as dispatch
  for name in (*REVIEW['next_untap_cards'],*REVIEW['mechanic_cards']):
   d=self.declaration(name);self.assertEqual(self.cases(name,d['variants']),dispatch(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,oracle_context=self.oracle))
   for vs in ([],['invented_0'],d['variants']*2):
    with self.assertRaises(ValueError):self.cases(name,vs)
 def test_untap_player_lock_controller_type_and_ownership_controls(self):
  for name in ('Exhaustion','Misstep'):
   for owner in (0,1):
    c=self.cases(name,[f'next_step_{owner}'])[0];p=c['_recipe']['untap_model_checkpoints'][0];target=1-owner;states=p['expected_tapped']
    self.assertEqual(p['player'],target)
    for label in ('creature','artifact_creature','foreign_creature'):self.assertEqual(states[f'{label}_{target}'],1)
    self.assertEqual(states[f'land_{target}'],int(name=='Exhaustion'));self.assertEqual(states[f'artifact_{target}'],0);self.assertEqual(states[f'artifact_{owner}'],1)
    self.assertIn('opponent_owned',next(x['state'] for x in c['setup'] if x['ref']==f'foreign_creature_{target}'))
 def test_later_real_untap_expires_and_no_direct_state_emulation(self):
  for name in ('Exhaustion','Misstep'):
   c=self.cases(name,['next_step_0'])[0];s=c['steps'];points=c['_recipe']['untap_model_checkpoints']
   self.assertEqual(points[-1]['expected_tapped']['creature_1'],0)
   self.assertTrue(any(x['blocked'] for x in points));self.assertEqual(sum(x['op']=='advance_turn' for x in s),sum(x['op']=='untap_step' for x in s))
   self.assertFalse(any(x.get('property')=='tapped' and x['op']!='assert_card' for x in s))
 def test_payment_denials_tap_all_independent_resources_and_preserve_pool(self):
  for name in ('Exhaustion','Misstep'):
   for owner in (0,1):
    c=self.cases(name,[f'wrong_color_{owner}'])[0]
    self.assertFalse(any(x['op'] in ('untap_step','advance_turn') for x in c['steps']))
    for x in c['setup']:
     if x['zone']=='battlefield' and x['player']==owner and x['name'] in ('Sol Ring','Forest','Island'):self.assertIn('tapped',x['state'])
    self.assertIn(dict(op='assert_mana',player=owner,color=1,value=1),c['steps'])
 def test_untap_replay_restores_real_tail_and_pending_source(self):
  for name in ('Exhaustion','Misstep'):
   s=self.cases(name,['pending_snapshot_1'])[0]['steps'];ops=[x['op'] for x in s];a=ops.index('capture');b=ops.index('record_state');r=ops.index('restore')
   self.assertEqual(s[a+1:b],s[r+1:-1]);self.assertIn(dict(op='assert_stack',value=1),s[:a]);self.assertIn(dict(op='assert_card',ref='source',property='in_hand',value=0),s[:a])
 def test_functional_unaffected_artifact_actual_activation_and_payment(self):
  s=self.cases('Exhaustion',['artifact_function_1'])[0]['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');b=s.index(dict(op='cast',ref='functional_stone',value=1))
  self.assertIn(dict(op='assert_mana',player=0,color=0,value=2),s[a:b]);self.assertIn(dict(op='assert_mana',player=0,color=0,value=0),s[b:])
 def test_strive_derives_payment_from_target_count_without_x_menu(self):
  for name in ('Colossal Heroics','Harness by Force'):
   for count in (1,2,3):
    c=self.cases(name,[f'payment_x{count}_0'])[0];s=c['steps'];self.assertEqual(sum(x['op']=='choose_target' for x in s),count);self.assertFalse(any(x['op']=='choose_x' for x in s))
    extra=REVIEW['mechanic_cards'][name]['params']['strive_cost'];from recipes import cost_from_oracle
    expected=[a+max(0,count-1)*b for a,b in zip(cost_from_oracle(self.oracle[name]),extra)]
    for color,value in enumerate(expected):self.assertIn(dict(op='set_mana',player=0,color=color,value=value),s)
    for color in range(6):self.assertIn(dict(op='assert_mana',player=0,color=color,value=0),s)
 def test_multi_theft_names_distinct_and_real_attack_cleanup_receipts(self):
  c=self.cases('Harness by Force',['combat_attack_x2_0'])[0];targets=[x['name'] for x in c['setup'] if x['ref'] in ('recipient','recipient_1')]
  self.assertEqual(targets,['Grizzly Bears','Hill Giant']);self.assertIn(dict(op='attack',ref='recipient',value=1),c['steps']);self.assertIn(dict(op='assert_life',player=1,value=18),c['steps'])
  s=self.cases('Colossal Heroics',['cleanup_x2_0'])[0]['steps'];cut=s.index(dict(op='cleanup'));self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=4),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=2),s[cut:])
 def test_optional_zero_targets_remain_legal_with_no_targetable_creature(self):
  for name in ('Colossal Heroics','Harness by Force'):
   c=self.cases(name,['effect_x0_1'])[0];s=c['steps'];self.assertIn(dict(op='require_can_cast',ref='source',value=1),s);self.assertFalse(any(x['op']=='choose_target' for x in s))
   self.assertEqual([x['name'] for x in c['setup'] if x['zone']=='battlefield' and 'Creature' in self.oracle[x['name']]['type_line']],['Deadly Insect'])

 def test_fixed_damage_has_actual_player_lethal_survivor_and_life_receipts(self):
  for name in ('Covenant of Blood','Stoke the Flames'):
   c=self.cases(name,['effect_0'])[0];s=c['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2),s)
   c=self.cases(name,['opponent_player_1'])[0];self.assertIn(dict(op='choose_player',player=1,value=0),c['steps']);self.assertIn(dict(op='assert_life',player=0,value=16),c['steps'])
   self.assertIn(dict(op='assert_life',player=1,value=24 if name=='Covenant of Blood' else 20),c['steps'])
   c=self.cases(name,['large_target_0'])[0];self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=4),c['steps'])
 def test_damage_all_targets_illegal_suppresses_mandatory_gain(self):
  s=self.cases('Covenant of Blood',['lost_target_0'])[0]['steps'];self.assertIn(dict(op='destroy',ref='recipient'),s);self.assertIn(dict(op='assert_life',player=0,value=20),s);self.assertFalse(any(x==dict(op='assert_life',player=0,value=24) for x in s))
 def test_removal_checks_artifact_enchantment_land_and_creature_predicates(self):
  for name in ('Sundering Vitae','Gleeful Sabotage'):
   for mode,nm in (('artifact_target','Sol Ring'),('enchantment_target','Omniscience')):
    c=self.cases(name,[mode+'_0'])[0];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='recipient'),nm);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows[nm]['csvid'],value=1),c['steps'])
   for mode in ('wrong_type','wrong_creature'):
    s=self.cases(name,[mode+'_1'])[0]['steps'];self.assertIn(dict(op='can_cast',ref='source',value=0),s)
 def test_combat_removal_uses_real_attackers_or_blockers(self):
  for name in ('Devouring Light','Aethertow'):
   for mode in ('effect','blocking_target'):
    c=self.cases(name,[mode+'_0'])[0];s=c['steps'];cast=s.index(dict(op='cast',ref='source',value=1));self.assertIn(dict(op='advance_combat'),s[:cast]);self.assertTrue(any(x['op']==('block' if mode=='blocking_target' else 'attack') and x['ref']=='recipient' for x in s[:cast]))
   s=self.cases(name,['wrong_state_0'])[0]['steps'];self.assertIn(dict(op='can_cast',ref='source',value=0),s);self.assertFalse(any(x['op']=='attack' for x in s))
 def test_aethertow_owner_library_order_and_repeated_real_target(self):
  c=self.cases('Aethertow',['two_copies_0'])[0];s=c['steps'];self.assertIn(dict(op='assert_zone_card',player=1,zone='library',index=0,value=self.rows['Hill Giant']['csvid']),s)
  self.assertIn(dict(op='assert_zone_card',player=1,zone='library',index=1,value=self.rows['Grizzly Bears']['csvid']),s);self.assertEqual(sum(x['op']=='cast' and x['ref'] in ('source','second') for x in s),2)
 def test_indestructible_is_proved_by_destruction_damage_and_expiry(self):
  c=self.cases('Ephemeral Shields',['protected_destroy_0'])[0];s=c['steps'];destroy=s.index(dict(op='destroy',ref='recipient'));self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),s[destroy:]);self.assertIn(dict(op='destroy',ref='unselected'),s)
  s=self.cases('Ephemeral Shields',['expired_destroy_1'])[0]['steps'];self.assertEqual(sum(x==dict(op='destroy',ref='recipient') for x in s),2);self.assertIn(dict(op='cleanup'),s)
  s=self.cases('Ephemeral Shields',['lethal_damage_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='protection_probe',value=1),s);self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=3),s)
 def test_nontoken_sweep_preserves_actual_token_and_indestructible_controls(self):
  c=self.cases('Hour of Reckoning',['effect_1'])[0]
  for player in (0,1):
   item=next(x for x in c['setup'] if x['ref']==f'kept_token_{player}');self.assertIn('token',item['state'])
   self.assertIn(dict(op='assert_matching_permanents',player=player,index=self.rows['Eldrazi Scion']['csvid'],property='token',value=1),c['steps']);self.assertIn(dict(op='assert_card',ref=f'indestructible_{player}',property='in_play',value=1),c['steps'])
   self.assertIn('opponent_owned',next(x['state'] for x in c['setup'] if x['ref']==f'foreign_{player}'))
 def test_normal_convoke_negatives_disable_all_owned_creature_resources(self):
  for name in ('Covenant of Blood','Stoke the Flames','Sundering Vitae','Devouring Light','Ephemeral Shields','Hour of Reckoning'):
   c=self.cases(name,['wrong_color_0'])[0]
   for x in c['setup']:
    if x['zone']=='battlefield' and x['player']==0 and 'Creature' in self.oracle[x['name']]['type_line']:self.assertIn('tapped',x['state'])

 def test_every_emitted_opcode_and_property_is_generator_schema_supported(self):
  from generate_cases import OPS,PROPERTIES,TOKEN_PROPERTIES,validate_untap_step
  for name in (*REVIEW['next_untap_cards'],*REVIEW['mechanic_cards']):
   d=self.declaration(name);p=spec(d,self.oracle[name]);vs=variants(p)
   for c in self.cases(name,vs):
    for step in c['steps']:
     self.assertIn(step['op'],OPS,(name,c['name'],step))
     if 'property' in step:self.assertIn(step['property'],PROPERTIES | (TOKEN_PROPERTIES if step['op']=='assert_matching_permanents' else set()),(name,c['name'],step))
     if step['op']=='untap_step':validate_untap_step(step)

 def test_denied_convoke_preserves_token_and_foreign_owner_setup_flags(self):
  for owner in (0,1):
   for mode in ('insufficient_mana','wrong_color'):
    c=self.cases('Hour of Reckoning',[f'{mode}_{owner}'])[0]
    token=next(x for x in c['setup'] if x['ref']==f'kept_token_{owner}');foreign=next(x for x in c['setup'] if x['ref']==f'foreign_{owner}')
    self.assertIn('tapped',token['state']);self.assertIn('token',token['state']);self.assertIn('opponent_owned',foreign['state']);self.assertIn('tapped',foreign['state'])

 def test_regeneration_consumes_once_and_changes_tap_only_on_destruction(self):
  for name in ('Regenerate','Necrobite','Dark Dabbling'):
   s=self.cases(name,['shield_consumed_0'])[0]['steps'];source=s.index(dict(op='cast',ref='source',value=1));destroy=s.index(dict(op='destroy',ref='recipient'))
   self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=0),s[source:destroy]);self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=1),s[destroy:]);self.assertEqual(sum(x==dict(op='destroy',ref='recipient') for x in s),2)
   self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2),s)
 def test_regeneration_expiry_installs_fresh_shield_after_positive_proof(self):
  for name in ('Regenerate','Necrobite','Dark Dabbling'):
   s=self.cases(name,['expired_destroy_1'])[0]['steps'];first=s.index(dict(op='destroy',ref='recipient'));second=s.index(dict(op='cast',ref='expiry_second',value=1));cleanup=s.index(dict(op='cleanup'))
   self.assertLess(first,second);self.assertLess(second,cleanup);self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=1),s[first:second]);self.assertIn(dict(op='destroy',ref='recipient'),s[cleanup:])
 def test_shield_creation_preserves_existing_damage_until_used(self):
  for name in ('Regenerate','Necrobite','Dark Dabbling'):
   s=self.cases(name,['damaged_shield_0'])[0]['steps'];bolt=s.index(dict(op='cast',ref='regeneration_probe',value=1));source=s.index(dict(op='cast',ref='source',value=1));destroy=s.index(dict(op='destroy',ref='recipient'))
   self.assertLess(bolt,source);self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=3),s[source:destroy]);self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s[destroy:]);self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=1),s[destroy:])
 def test_no_regeneration_and_zero_toughness_use_actual_spells(self):
  for name in ('Regenerate','Necrobite','Dark Dabbling'):
   for mode,witness in (('zero_toughness','Last Gasp'),('cannot_regenerate','Terminate')):
    c=self.cases(name,[mode+'_1'])[0];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='regeneration_probe'),witness);self.assertIn(dict(op='cast',ref='regeneration_probe',value=1),c['steps']);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2),c['steps'])
 def test_necrobite_deathtouch_and_regeneration_have_distinct_combat_effects(self):
  s=self.cases('Necrobite',['deathtouch_combat_0'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='recipient',property='attacking',value=0),s);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Scaled Wurm']['csvid'],value=1),s)
  s=self.cases('Regenerate',['regeneration_combat_0'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='regeneration_blocker',property='damage',value=2),s)
  s=self.cases('Necrobite',['expired_deathtouch_1'])[0]['steps'];cleanup=s.index(dict(op='cleanup'));self.assertIn(dict(op='cast',ref='regen_reset',value=1),s[cleanup:]);self.assertIn(dict(op='assert_card',ref='expiry_blocker',property='damage',value=2),s[cleanup:])
 def test_active_mastery_uses_own_grave_and_controller_not_owner(self):
  for owner in (0,1):
   c=self.cases('Dark Dabbling',[f'mastery_active_{owner}'])[0];s=c['steps'];grave=[x['name'] for x in c['setup'] if x['zone']=='graveyard' and x['player']==owner]
   self.assertIn('Lightning Bolt',grave);self.assertIn('Giant Growth',grave);self.assertIn('opponent_owned',next(x['state'] for x in c['setup'] if x['ref']=='mastery_foreign'))
   for ref in ('mastery_own','mastery_foreign'):
    destroy=s.index(dict(op='destroy',ref=ref));self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=1),s[destroy:])
   c=self.cases('Dark Dabbling',[f'opponent_mastery_{owner}'])[0];s=c['steps'];cut=s.index(dict(op='destroy',ref='mastery_own'));self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Hill Giant']['csvid'],value=1),s[cut:])
 def test_mastery_all_targets_lost_suppresses_draw_and_other_shields(self):
  s=self.cases('Dark Dabbling',['mastery_all_targets_lost_0'])[0]['steps'];self.assertIn(dict(op='destroy',ref='recipient'),s);self.assertIn(dict(op='assert_zone',player=0,zone='library',value=18),s);self.assertFalse(any(x==dict(op='assert_zone',player=0,zone='library',value=17) for x in s));self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Hill Giant']['csvid'],value=1),s)
 def test_countered_regeneration_has_permission_gate_and_actual_no_shield_probe(self):
  for name in ('Regenerate','Necrobite','Dark Dabbling'):
   s=self.cases(name,['countered_1'])[0]['steps'];query=s.index(dict(op='require_can_cast',ref='source',value=1));priority=s.index(dict(op='choose_priority',ref='counter'));destroy=s.index(dict(op='destroy',ref='recipient'));self.assertLess(query,priority);self.assertLess(priority,destroy)

class ConvokeCostFollowupTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def cases(self,selected,name='Stoke the Flames'):
  r=REVIEW['cost_followups'][name];d=dict(family='tap_spell',version=2,oracle_text=self.oracle[name]['oracle_text'],params=copy.deepcopy(r['params']),variants=list(selected));p=spec(d,self.oracle[name]);return expand(name,self.rows[name]['csvid'],d,p,selected,self.rows,self.oracle)
 def test_additive_namespace_and_original_recipe_stay_distinct(self):
  r=REVIEW['cost_followups']['Stoke the Flames'];self.assertEqual(r['params']['schema'],9);self.assertEqual(REVIEW['mechanic_cards']['Stoke the Flames']['params']['schema'],7)
  c=self.cases(['convoke_payment_0'])[0];self.assertEqual(c['_recipe']['params'],r['params'])
  bad=dict(family='tap_spell',version=2,oracle_text=self.oracle['Stoke the Flames']['oracle_text'],params=dict(r['params'],schema=7),variants=['convoke_payment_0'])
  with self.assertRaises(ValueError):spec(bad,self.oracle['Stoke the Flames'])
 def test_colored_credit_uses_exact_reduced_pool_and_sick_creatures(self):
  for owner in (0,1):
   c=self.cases([f'convoke_payment_{owner}'])[0];s=c['steps'];cast=s.index(dict(op='cast',ref='source',value=1));resolve=s.index(dict(op='resolve'))
   self.assertIn(dict(op='set_mana',player=owner,color=0,value=2),s);self.assertIn(dict(op='set_mana',player=owner,color=4,value=0),s)
   for i in (0,1):
    ref=f'convoke_resource_{i}';self.assertIn('sick',next(x['state'] for x in c['setup'] if x['ref']==ref));self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=1),s[cast:resolve]);self.assertIn(dict(op='assert_card',ref=ref,property='sick',value=1),s[cast:resolve])
   for color in range(6):self.assertIn(dict(op='assert_mana',player=owner,color=color,value=0),s)
 def test_human_cost_choices_and_ai_auto_selection_have_separate_contracts(self):
  for owner,expected in ((0,['convoke_resource_0','convoke_resource_1','recipient']),(1,['recipient'])):
   s=self.cases([f'convoke_payment_{owner}'])[0]['steps'];self.assertEqual([x['ref'] for x in s if x['op']=='choose_target'],expected);self.assertFalse(any(x['op']=='choose_dialog' for x in s))
 def test_cost_negative_changes_only_critical_resource_predicate(self):
  for mode in ('tapped','wrong_controller','noncreature'):
   c=self.cases([f'convoke_{mode}_0'])[0];first=next(x for x in c['setup'] if x['ref']=='convoke_resource_0');second=next(x for x in c['setup'] if x['ref']=='convoke_resource_1')
   self.assertIn('untapped',second['state']);self.assertIn('sick',second['state']);self.assertIn(dict(op='set_mana',player=0,color=0,value=2),c['steps']);self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])
   if mode=='tapped':self.assertIn('tapped',first['state'])
   if mode=='wrong_controller':self.assertEqual(first['player'],1)
   if mode=='noncreature':self.assertEqual(first['name'],'Seal of Fire');self.assertEqual(self.oracle[first['name']]['colors'],['R']);self.assertNotIn('Creature',self.oracle[first['name']]['type_line'])
 def test_foreign_owned_resources_are_controlled_by_caster_and_eligible(self):
  for owner in (0,1):
   c=self.cases([f'convoke_foreign_owned_{owner}'])[0]
   for i in (0,1):
    item=next(x for x in c['setup'] if x['ref']==f'convoke_resource_{i}');self.assertEqual(item['player'],owner);self.assertIn('opponent_owned',item['state'])
   self.assertIn(dict(op='cast',ref='source',value=1),c['steps'])
 def test_countered_spell_still_pays_tapping_and_remaining_mana(self):
  c=self.cases(['convoke_countered_0'])[0];s=c['steps'];self.assertIn(dict(op='cast',ref='source',value=0),s);self.assertIn(dict(op='set_mana',player=0,color=2,value=2),s)
  for i in (0,1):self.assertIn(dict(op='assert_card',ref=f'convoke_resource_{i}',property='tapped',value=1),s)
  for color in range(6):self.assertIn(dict(op='assert_mana',player=0,color=color,value=0),s)
 def test_cost_replay_has_identical_tail_and_valid_generator_schema(self):
  from generate_cases import OPS,PROPERTIES,TOKEN_PROPERTIES
  p=spec(dict(family='tap_spell',version=2,oracle_text=self.oracle['Stoke the Flames']['oracle_text'],params=REVIEW['cost_followups']['Stoke the Flames']['params'],variants=['convoke_payment_0']),self.oracle['Stoke the Flames'])
  for c in self.cases(variants(p)):
   for step in c['steps']:
    self.assertIn(step['op'],OPS)
    if 'property' in step:self.assertIn(step['property'],PROPERTIES|(TOKEN_PROPERTIES if step['op']=='assert_matching_permanents' else set()))
  s=self.cases(['convoke_pending_snapshot_1'])[0]['steps'];ops=[x['op'] for x in s];a=ops.index('capture');b=ops.index('record_state');r=ops.index('restore');self.assertEqual(s[a+1:b],s[r+1:-1])

class LastLandLockTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def case(self,name,variant):
  r=REVIEW['next_untap_cards'][name];d=dict(family='tap_spell',version=2,oracle_text=self.oracle[name]['oracle_text'],params=copy.deepcopy(r['params']),variants=[variant]);p=spec(d,self.oracle[name]);return expand(name,self.rows[name]['csvid'],d,p,[variant],self.rows,self.oracle)[0]
 def test_hazoret_exiles_actual_top_four_or_available_fewer_lands(self):
  for owner in (0,1):
   for mode,n in [('next_step',4),('small_library',2),('empty_library',0)]:
    c=self.case("Hazoret's Undying Fury",f'{mode}_{owner}');library=[x['name'] for x in c['setup'] if x['player']==owner and x['zone']=='library'];self.assertEqual(len(library),n);self.assertTrue(all('Land' in self.oracle[nm]['type_line'] for nm in library));self.assertIn(dict(op='assert_zone',player=owner,zone='exile',value=1+n),c['steps']);self.assertTrue(any('free-cast' in g for g in c['_recipe']['untested_components']))
 def test_lock_uses_full_actual_turns_and_positive_other_type_controls(self):
  for name in ("Kefnet's Last Word","Hazoret's Undying Fury"):
   c=self.case(name,'next_step_0');self.assertTrue(any(x['name']=='Eon Hub' and x['zone']=='battlefield' for x in c['setup']));s=c['steps'];self.assertGreaterEqual(sum(x['op']=='untap_step' and x['player']==0 for x in s),2)
   for i,x in enumerate(s):
    if x['op']=='untap_step':self.assertEqual(s[i-1],dict(op='advance_turn'))
   checkpoints=c['_recipe']['untap_model_checkpoints'];own=[x for x in checkpoints if x['player']==0];self.assertIn('land_0',own[0]['blocked']);self.assertNotIn('creature_0',own[0]['blocked']);self.assertEqual(own[0]['expected_tapped']['artifact_0'],0);self.assertEqual(own[1]['expected_tapped']['land_0'],0)
 def test_all_new_future_lock_boards_have_reachable_skipped_upkeep_context(self):
  for name in ("Kefnet's Last Word","Hazoret's Undying Fury"):
   entry=REVIEW['next_untap_cards'][name];d=dict(family='tap_spell',version=2,oracle_text=self.oracle[name]['oracle_text'],params=entry['params'],variants=['next_step_0']);p=spec(d,self.oracle[name])
   for mode in variants(p):
    c=self.case(name,mode)
    if any(x['op']=='untap_step' for x in c['steps']):
     self.assertEqual(sum(x['name']=='Eon Hub' and x['zone']=='battlefield' for x in c['setup']),1);self.assertTrue(any('normal intervening-upkeep' in g for g in c['_recipe']['untested_components']))
 def test_hazoret_acknowledges_nonempty_display_for_actual_viewer_zero(self):
  for owner in (0,1):
   c=self.case("Hazoret's Undying Fury",f'next_step_{owner}');s=c['steps'];at=s.index(dict(op='resolve'));self.assertEqual(s[at-1],dict(op='choose_zone',player=0,value=-1))
   c=self.case("Hazoret's Undying Fury",f'empty_library_{owner}');self.assertFalse(any(x['op']=='choose_zone' for x in c['steps']))
 def test_kefnet_theft_never_adds_untap_or_haste(self):
  c=self.case("Kefnet's Last Word",'next_step_0');s=c['steps'];cast=s.index(dict(op='cast',ref='source',value=1));advance=s.index(dict(op='advance_turn'));self.assertIn(dict(op='assert_card',ref='theft_target',property='tapped',value=1),s[cast:advance]);self.assertIn(dict(op='bind_permanent_copy',ref='theft_target',player=0,value=self.rows['Air Elemental']['csvid'],index=0),s)
  s=self.case("Kefnet's Last Word",'stolen_sick_1')['steps'];self.assertIn(dict(op='attack',ref='theft_target',value=0),s)
 def test_whole_spell_target_fizzle_suppresses_own_land_lock(self):
  for mode in ('lost_target','bounced_target'):
   c=self.case("Kefnet's Last Word",mode+'_0');self.assertTrue(all(not x['blocked'] for x in c['_recipe']['untap_model_checkpoints']))
   if mode=='bounced_target':self.assertIn(dict(op='resolve_response'),c['steps'])
 def test_kefnet_restricts_targets_to_printed_three_types(self):
  for mode in ('wrong_land','wrong_planeswalker'):
   c=self.case("Kefnet's Last Word",mode+'_1');self.assertIn(dict(op='can_cast',ref='source',value=0),c['steps']);self.assertFalse(any(x['op']=='untap_step' for x in c['steps']))

class QuantitySearchRecoveryTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def cases(self,name,selected):
  r=REVIEW['quantity_cards'][name];d=dict(family='tap_spell',version=2,oracle_text=self.oracle[name]['oracle_text'],params=copy.deepcopy(r['params']),variants=list(selected));p=spec(d,self.oracle[name]);return expand(name,self.rows[name]['csvid'],d,p,selected,self.rows,self.oracle)
 def test_quantity_pins_are_fail_closed_at_public_spec_and_dispatch(self):
  from recipes import expand as dispatch
  for name,entry in REVIEW['quantity_cards'].items():
   variant='effect_x2_0' if entry['params']['variable'] else 'effect_0';d=dict(family='tap_spell',version=2,oracle_text=self.oracle[name]['oracle_text'],params=copy.deepcopy(entry['params']),variants=[variant]);p=spec(d,self.oracle[name]);self.assertEqual(expand(name,self.rows[name]['csvid'],d,p,[variant],self.rows,self.oracle),dispatch(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,oracle_context=self.oracle))
   for key,value in [('count',3),('destination','graveyard'),('schema',7)]:
    bad=copy.deepcopy(d);bad['params'][key]=value
    with self.assertRaises(ValueError):spec(bad,self.oracle[name])
   bad=copy.deepcopy(self.oracle[name]);bad['oracle_text']+=' Extra.'
   with self.assertRaises(ValueError):spec(d,bad)
 def test_value_filter_is_oracle_based_including_artifact_land_and_zero_creature(self):
  from tap_spell_recipes import _quantity_eligible8
  for name in ('Chord of Calling','Whir of Invention'):
   p=REVIEW['quantity_cards'][name]['params']
   for x in range(4):
    s=self.cases(name,[f'effect_x{x}_0'])[0]['steps'];self.assertIn(dict(op='set_mana',player=0,color=0,value=17+x),s)
    for nm in ('Ornithopter','Merfolk of the Pearl Trident','Grizzly Bears','Scathe Zombies','Darksteel Citadel','Sol Ring','Mind Stone','Darksteel Ingot','Seal of Fire'):
     self.assertIn(dict(op='assert_offered_legal_card',index=self.rows[nm]['csvid'],value=int(_quantity_eligible8(self.oracle[nm],p,x))),s)
  self.assertTrue(_quantity_eligible8(self.oracle['Darksteel Citadel'],REVIEW['quantity_cards']['Whir of Invention']['params'],0));self.assertTrue(_quantity_eligible8(self.oracle['Ornithopter'],REVIEW['quantity_cards']['Chord of Calling']['params'],0))
 def test_nissa_basic_is_supertype_and_actual_entry_is_tapped(self):
  from tap_spell_recipes import _quantity_eligible8
  p=REVIEW['quantity_cards']["Nissa's Expedition"]['params'];self.assertTrue(_quantity_eligible8(self.oracle['Snow-Covered Forest'],p,0));self.assertFalse(_quantity_eligible8(self.oracle['Tundra'],p,0));self.assertFalse(_quantity_eligible8(self.oracle['Darksteel Citadel'],p,0))
  c=self.cases("Nissa's Expedition",['effect_1'])[0];s=c['steps'];bound=[x['ref'] for x in s if x['op']=='bind_permanent_copy'];self.assertEqual(len(bound),2)
  for ref in bound:self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=1),s)
  # The second picker sees the library after the first selected card left.
  self.assertIn(dict(op='assert_offered_count',value=12),s)
 def test_zero_selection_is_still_legal_with_populated_other_graveyard(self):
  for mode in ('decline','empty_zone','no_match'):
   c=self.cases('Unmake the Graves',[mode+'_1'])[0];s=c['steps'];self.assertIn(dict(op='require_can_cast',ref='source',value=1),s);self.assertIn(dict(op='cast',ref='source',value=1),s)
   self.assertIn(dict(op='assert_zone',player=0,zone='graveyard',value=5),s)
 def test_pending_uses_physical_hand_receipts_but_stack_source_not_in_hand(self):
  c=self.cases('Chord of Calling',['effect_x2_0'])[0];s=c['steps'];a=s.index(dict(op='cast',ref='source',value=1));b=s.index(dict(op='resolve'));self.assertIn(dict(op='assert_card',ref='source',property='in_hand',value=0),s[a:b]);self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=2),s[a:b]);self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=1),s[b:])
 def test_recovery_target_loss_uses_real_response_and_partial_survives(self):
  for owner in (0,1):
   all_lost=self.cases('Unmake the Graves',[f'only_target_lost_{owner}'])[0]['steps'];partial=self.cases('Unmake the Graves',[f'partial_target_lost_{owner}'])[0]['steps']
   for seq in (all_lost,partial):self.assertIn(dict(op='cast_response',ref='response',value=1),seq);self.assertIn(dict(op='resolve_response'),seq);self.assertFalse(any(x['op']=='resolve' for x in seq))
   self.assertIn(dict(op='assert_zone',player=owner,zone='hand',value=1),all_lost);self.assertIn(dict(op='assert_zone',player=owner,zone='hand',value=2),partial)
 def test_repeat_cast_taps_new_convoke_improvise_resource_with_real_spell(self):
  for name in ('Chord of Calling','Whir of Invention'):
   s=self.cases(name,['two_copies_x2_0'])[0]['steps'];first=s.index(dict(op='cast',ref='source',value=1));tap=s.index(dict(op='cast',ref='response',value=1));second=s.index(dict(op='cast',ref='second',value=1));self.assertLess(first,tap);self.assertLess(tap,second);self.assertIn(dict(op='choose_dialog',player=0,value=0),s[tap:second])
 def test_resources_unavailable_for_normal_payment_and_negatives(self):
  for name,entry in REVIEW['quantity_cards'].items():
   suffix='_x2_0' if entry['params']['variable'] else '_0'
   for mode in ('effect','insufficient_mana','wrong_color'):
    c=self.cases(name,[mode+suffix])[0]
    for item in c['setup']:
     if item['zone']=='battlefield' and item['player']==0:self.assertIn('tapped',item['state'])
 def test_effect_rich_reserve_and_exact_payment_are_distinct(self):
  for name,entry in REVIEW['quantity_cards'].items():
   for owner in (0,1):
    suffix=f'_x2_{owner}' if entry['params']['variable'] else f'_{owner}';rich=self.cases(name,['effect'+suffix])[0]['steps'];exact=self.cases(name,['payment'+suffix])[0]['steps'];self.assertIn(dict(op='assert_mana',player=owner,color=0,value=17),rich);self.assertIn(dict(op='assert_mana',player=owner,color=0,value=0),exact)
 def test_tapped_basic_entry_has_real_unavailable_mana_activation(self):
  for owner in (0,1):
   c=self.cases("Nissa's Expedition",[f'tapped_entry_{owner}'])[0];s=c['steps'];found=[x['ref'] for x in s if x['op']=='bind_permanent_copy'];self.assertEqual(len(found),2)
   for ref in found:self.assertIn(dict(op='activate',ref=ref,value=0),s)
 def test_qualified_search_declines_display_even_without_remaining_legal_match(self):
  for owner in (0,1):
   s=self.cases("Nissa's Expedition",[f'snow_basic_{owner}'])[0]['steps'];resolve=s.index(dict(op='resolve'));self.assertEqual(s[resolve-1],dict(op='choose_optional_zone',player=owner,value=self.rows['Tundra']['csvid']))
   s=self.cases('Chord of Calling',[f'no_match_x2_{owner}'])[0]['steps'];self.assertTrue(any(x['op']=='choose_optional_zone' for x in s))
 def test_whir_no_resource_cast_does_not_queue_unoffered_improvise_dialog(self):
  for owner in (0,1):
   s=self.cases('Whir of Invention',[f'effect_x2_{owner}'])[0]['steps'];self.assertFalse(any(x['op']=='choose_dialog' for x in s));self.assertIn(dict(op='choose_x',player=owner,value=2),s);self.assertIn(dict(op='set_mana',player=owner,color=2,value=3),s)
 def test_all_emitted_ops_supported_and_replay_tails_independent(self):
  from generate_cases import OPS,PROPERTIES
  for name,entry in REVIEW['quantity_cards'].items():
   d=dict(family='tap_spell',version=2,oracle_text=self.oracle[name]['oracle_text'],params=entry['params'],variants=['placeholder']);p=spec(d,self.oracle[name])
   for c in self.cases(name,variants(p)):
    for step in c['steps']:
     self.assertIn(step['op'],OPS)
     if 'property' in step:self.assertIn(step['property'],PROPERTIES)
   suffix='_x2_1' if entry['params']['variable'] else '_1';s=self.cases(name,['pending_snapshot'+suffix])[0]['steps'];ops=[x['op'] for x in s];a=ops.index('capture');b=ops.index('record_state');r=ops.index('restore');self.assertEqual(s[a+1:b],s[r+1:-1]);self.assertIsNot(s[a+1],s[r+1])

class Wave7TapSpellTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  from tap_spell_recipes import WAVE7
  cls.reviews=WAVE7['cards']
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
  cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):
  return dict(family='tap_spell',version=2,oracle_text=self.oracle[name]['oracle_text'],params=copy.deepcopy(self.reviews[name]['params']),variants=[])
 def cases(self,name,selected):
  d=self.declaration(name);d['variants']=selected
  return expand(name,self.rows[name]['csvid'],d,spec(d,self.oracle[name]),selected,self.rows,self.oracle)
 def test_complete_source_and_component_contract_fail_closed(self):
  for name in self.reviews:
   d=self.declaration(name)
   for key,value in [('oracle_text',self.oracle[name]['oracle_text']+' Draw a card.'),('mana_cost','{99}'),('colors',[]),('type_line','Enchantment')]:
    changed=dict(self.oracle[name],**{key:value});changed_d=dict(d,oracle_text=changed['oracle_text'])
    with self.assertRaises(ValueError):spec(changed_d,changed)
   changed=copy.deepcopy(d);changed['params']['base']['draw']+=1
   with self.assertRaises(ValueError):spec(changed,self.oracle[name])
 def test_all_actions_and_controllers_have_noop_sensitive_receipts(self):
  for name,entry in self.reviews.items():
   for effect in entry['params']['effects']:
    for owner in (0,1):
     c=self.cases(name,[f'{effect}_effect_{owner}'])[0];steps=c['steps'];cut=next(i for i,s in enumerate(steps) if s['op']=='resolve')
     before=[s['value'] for s in steps[:cut] if s.get('ref')=='recipient' and s.get('property')=='tapped']
     after=[s['value'] for s in steps[cut:] if s.get('ref')=='recipient' and s.get('property')=='tapped']
     self.assertEqual(before[-1],int(effect=='untap'),name)
     self.assertEqual(after[-1],int(effect=='tap'),name)
     self.assertTrue(any(s.get('ref')=='unselected' and s.get('property')=='tapped' and s['value']==before[-1] for s in steps[cut:]),name)
 def test_cipher_inactive_profiles_keep_controlled_creatures_out_of_play(self):
  d=self.declaration('Hidden Strings');p=spec(d,self.oracle['Hidden Strings'])
  self.assertFalse(any('_own_target_' in v for v in variants(p)))
  for c in self.cases('Hidden Strings',variants(p)):
   owner=int(c['_recipe']['variant'][-1])
   self.assertFalse(any(item['player']==owner and item['zone']=='battlefield' and 'Creature' in self.oracle[item['name']]['type_line'] for item in c['setup']))
 def test_partial_target_loss_queues_only_valid_twiddle_actions(self):
  for name in ('Psychic Puppetry','Hidden Strings'):
   count=self.reviews[name]['params']['base']['count']
   for effect in ('tap','untap'):
    for owner in (0,1):
     steps=self.cases(name,[f'{effect}_lost_target_{owner}'])[0]['steps']
     self.assertEqual(sum(s['op']=='choose_dialog' for s in steps),count-1)
     self.assertIn(dict(op='assert_choices_consumed'),steps)
 def test_provoke_rejects_own_creature_and_draws_only_after_valid_resolution(self):
  for owner in (0,1):
   c=self.cases('Provoke',[f'untap_wrong_controller_{owner}'])[0]
   target=next(x for x in c['setup'] if x['ref']=='recipient')
   self.assertEqual(target['player'],owner)
   self.assertIn(dict(op='can_cast',ref='source',value=0),c['steps'])
   self.assertFalse(any(x['op']=='choose_target' for x in c['steps']))
   lost=self.cases('Provoke',[f'untap_lost_target_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=6),lost)
   effect=self.cases('Provoke',[f'untap_effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=5),effect)
 def test_action_variants_and_snapshot_replay_are_strict(self):
  for name,entry in self.reviews.items():
   d=self.declaration(name);p=spec(d,self.oracle[name]);supported=variants(p)
   self.assertEqual(len(supported),len(set(supported)))
   for effect in entry['params']['effects']:
    s=self.cases(name,[f'{effect}_pending_snapshot_1'])[0]['steps'];ops=[x['op'] for x in s];a=ops.index('capture');b=ops.index('record_state');r=ops.index('restore')
    self.assertEqual(s[a+1:b],s[r+1:-1]);self.assertIsNot(s[a+1],s[r+1])
   for bad in ([],[supported[0],supported[0]],['invented_effect_0']):
    with self.assertRaises(ValueError):self.cases(name,bad)
 def test_hidden_strings_response_retains_the_pending_multitarget_parent(self):
  for effect in ('tap','untap'):
   for owner in (0,1):
    s=self.cases('Hidden Strings',[f'{effect}_bounced_target_{owner}'])[0]['steps']
    response=next(i for i,x in enumerate(s) if x['op']=='cast_response')
    self.assertEqual(s[response]['ref'],'response')
    self.assertEqual(s[response+1],dict(op='assert_stack',value=2))
    drain=next(i for i,x in enumerate(s) if x['op']=='resolve_response')
    self.assertEqual(s[drain+1],dict(op='assert_stack',value=0))
    dialogs=[i for i,x in enumerate(s) if x['op']=='choose_dialog']
    self.assertEqual(len(dialogs),1)
    self.assertLess(response,dialogs[0]);self.assertLess(dialogs[0],drain)
    response_target=max(i for i,x in enumerate(s[:response]) if x['op']=='choose_target')
    self.assertLess(response_target,response)
    self.assertTrue(any(x.get('ref')=='recipient_1' and x.get('property')=='tapped' and x['value']==int(effect=='tap') for x in s[drain:]))
 def test_two_arcane_copies_explicitly_decline_human_splice_menu(self):
  for effect in ('tap','untap'):
   for owner in (0,1):
    s=self.cases('Psychic Puppetry',[f'{effect}_two_copies_{owner}'])[0]['steps']
    cast=next(i for i,x in enumerate(s) if x['op']=='cast')
    dialogs=[x['value'] for x in s[:cast] if x['op']=='choose_dialog']
    self.assertEqual(dialogs,([0] if owner==0 else [])+[int(effect=='untap')])
    queued=[x for x in s[:cast] if x['op'] in ('choose_target','choose_dialog')]
    if owner==0:
     self.assertEqual(queued[0],dict(op='choose_dialog',player=0,value=0))
     self.assertEqual(queued[1]['op'],'choose_target')
     self.assertEqual(queued[2],dict(op='choose_dialog',player=0,value=int(effect=='untap')))
    else:self.assertEqual(queued[0]['op'],'choose_target')
 def test_ruthless_nonattacking_only_fixtures_choose_the_first_mode_implicitly(self):
  name='Ruthless Instincts'
  home=Path(__file__).parent/'data/cases/cards/146/14614-ruthless-instincts.json'
  selected=json.loads(home.read_text(encoding='utf8'))['recipes'][0]['variants']
  for c in self.cases(name,selected):
   owner=int(c['_recipe']['variant'][-1]);s=c['steps']
   # The actual handler offers a modal menu only if both attacking and
   # nonattacking targets exist. These boards have no attacking creature.
   self.assertFalse(any(x['zone']=='battlefield' and 'attacking' in x.get('state',[]) for x in c['setup']))
   casts=[i for i,x in enumerate(s) if x['op']=='cast' and x.get('ref') in ('source','second')]
   for cast in casts:
    self.assertFalse(any(x['op']=='attack' for x in s[:cast]))
    previous=max((i for i,x in enumerate(s[:cast]) if x['op']=='assert_choices_consumed'),default=-1)
    queued=[x for x in s[previous+1:cast] if x['op'] in ('choose_dialog','choose_target')]
    self.assertFalse(any(x['op']=='choose_dialog' for x in queued))
    if queued:self.assertEqual(queued[0],dict(op='choose_target',player=owner,ref='recipient'))
 def test_mark_for_death_untaps_opposing_target_without_a_draw(self):
  for owner in (0,1):
   c=self.cases('Mark for Death',[f'untap_effect_{owner}'])[0]
   target=next(x for x in c['setup'] if x['ref']=='recipient')
   self.assertEqual(target['player'],1-owner)
   cut=next(i for i,x in enumerate(c['steps']) if x['op']=='resolve')
   after=c['steps'][cut:]
   self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=0),after)
   self.assertIn(dict(op='assert_card',ref='unselected',property='tapped',value=1),after)
   self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=6),after)

if __name__=='__main__':unittest.main()
