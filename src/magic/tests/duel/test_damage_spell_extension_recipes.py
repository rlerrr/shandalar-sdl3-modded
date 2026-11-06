"""Fixed damage extensions preserve Oracle pins and real-cast receipts."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
import damage_spell_extension_recipes as damage

class DamageSpellExtensionRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Fiery Finish',selected=('survivor_0',)):
  return dict(family='damage_spell_extension',version=2,params=copy.deepcopy(damage.PROFILES[name]['params']),oracle_text=self.oracle[name]['oracle_text'],variants=list(selected))
 def cases(self,name='Fiery Finish',selected=('survivor_0',)):
  d=self.declaration(name,selected);p=damage.spec(d,self.oracle[name]);return damage.expand(name,self.rows[name]['csvid'],d,p,list(selected),self.rows,self.oracle)
 def test_full_source_identity_characteristics_cost_and_text_pinned(self):
  d=self.declaration();r=self.oracle['Fiery Finish']
  for key,value in (('oracle_text','Deal 7 damage.'),('mana_cost','{R}'),('oracle_id','changed'),('type_line','Instant'),('layout','transform'),('card_faces',[{}]),('selected_face',{})):
   if key=='selected_face':value={'name':'other'}
   with self.assertRaises(ValueError):damage.spec(d,dict(r,**{key:value}))
 def test_explicit_profile_params_and_version_required(self):
  d=self.declaration()
  for change in (dict(params={}),dict(params=dict(d['params'],extra=True)),dict(version=1),dict(family='damage_spell'),dict(oracle_text='changed'),dict(variants=[])):
   with self.assertRaises(ValueError):damage.spec(dict(d,**change),self.oracle['Fiery Finish'])
  p=copy.deepcopy(d['params']);p['effect']['amount']=6
  with self.assertRaises(ValueError):damage.spec(dict(d,params=p),self.oracle['Fiery Finish'])
 def test_unknown_duplicate_or_unsafe_modal_variants_rejected(self):
  for selected in (['invented_0'],['survivor_0','survivor_0']):
   with self.assertRaises(ValueError):self.cases(selected=selected)
  with self.assertRaises(ValueError):self.cases('Abrade',['wrong_type_0'])
 def test_unreviewed_source_not_enrolled_by_matching_one_clause(self):
  with self.assertRaises(ValueError):damage.spec(self.declaration(),self.oracle['Lightning Bolt'])
 def test_witness_changes_rejected(self):
  damage.validate_witnesses(self.oracle,self.rows)
  for name,key,value in (('Scaled Wurm','toughness','7'),('Unsummon','oracle_text','Exile target creature.'),('Darksteel Myr','keywords',[]),('Sol Ring','mana_cost','{0}')):
   o=copy.deepcopy(self.oracle);o[name][key]=value
   with self.assertRaises(ValueError):damage.validate_witnesses(o,self.rows)
 def test_positive_damage_observed_before_cleanup(self):
  s=self.cases()[0]['steps'];cut=next(i for i,a in enumerate(s) if a['op']=='cleanup')
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=7),s[:cut])
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s[cut:])
  self.assertIn(dict(op='assert_card',ref='unselected',property='damage',value=0),s)
 def test_abrade_witness_is_undamaged_before_real_cast(self):
  s=self.cases('Abrade')[0]['steps'];cast=next(i for i,a in enumerate(s) if a['op']=='cast' and a['ref']=='source')
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s[:cast])
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=3),s[cast:])
 def test_exact_payment_and_wrong_color_have_all_color_receipts(self):
  for c in self.cases(selected=['payment_1','wrong_payment_color_1']):
   self.assertEqual({s['color'] for s in c['steps'] if s['op']=='assert_mana' and s['player']==1},set(range(6)))
  c=self.cases(selected=['wrong_payment_color_0'])[0]
  self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])
  self.assertIn(dict(op='assert_mana_total',player=0,value=6),c['steps'])
 def test_both_controllers_and_snapshot_replay_swap_once(self):
  c=self.cases(selected=['snapshot_1'])[0];s=c['steps'];source=next(a for a in c['setup'] if a['ref']=='source')
  self.assertEqual(source['player'],1)
  self.assertEqual([a['player'] for a in s if a['op']=='choose_target' and a['ref']=='recipient'],[1,1])
  self.assertEqual([a['player'] for a in s if a['op']=='assert_mana_total' and a['value']==54],[1,1])
 def test_pending_snapshot_does_not_recast_and_retains_target_queue(self):
  s=self.cases(selected=['pending_snapshot_1'])[0]['steps'];ops=[a['op'] for a in s];cut=ops.index('capture')
  self.assertEqual(sum(a['op']=='cast' and a['ref']=='source' for a in s),1)
  self.assertIn('cast',ops[:cut]);self.assertIn('resolve',ops[cut:]);self.assertEqual(ops[-1],'assert_recorded_state')
  self.assertEqual([a['player'] for a in s if a['op']=='choose_target' and a['ref']=='recipient'],[1])
  self.assertEqual([a['player'] for a in s if a['op']=='assert_mana_total' and a['value']==54],[1,1])
 def test_pending_departure_casts_actual_response_with_stack_receipt(self):
  s=self.cases(selected=['lost_target_0'])[0]['steps']
  self.assertIn(dict(op='cast_response',ref='response',value=1),s)
  self.assertIn(dict(op='assert_stack',value=2),s)
  self.assertIn(dict(op='resolve_response'),s)
  self.assertNotIn('choose_priority',[a['op'] for a in s])
 def test_modal_choice_precedes_target_selection(self):
  s=self.cases('Abrade')[0]['steps'];choice=next(i for i,a in enumerate(s) if a['op']=='choose_dialog');target=next(i for i,a in enumerate(s) if a['op']=='choose_target')
  self.assertLess(choice,target);self.assertEqual(s[choice]['value'],0)
 def test_declined_kicker_follows_target_and_exact_payment_has_no_kicker_prompt(self):
  s=self.cases('Burst Lightning')[0]['steps'];choice=next(i for i,a in enumerate(s) if a['op']=='choose_dialog');target=next(i for i,a in enumerate(s) if a['op']=='choose_target');cast=next(i for i,a in enumerate(s) if a['op']=='cast')
  self.assertLess(target,choice);self.assertLess(choice,cast);self.assertEqual(s[choice]['value'],1)
  self.assertNotIn('choose_dialog',[a['op'] for a in self.cases('Burst Lightning',['payment_0'])[0]['steps']])
 def test_exile_replacement_has_lethal_later_and_expiry_receipts(self):
  cs=self.cases('Lava Coil',['lethal_0','later_death_0','expired_death_0'])
  for c in cs:self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=0),c['steps'])
  self.assertIn('cleanup',[a['op'] for a in cs[2]['steps']])
  self.assertNotIn('cleanup',[a['op'] for a in cs[1]['steps']])
 def test_other_printed_components_remain_explicit_gaps(self):
  for name,word in (('Deem Worthy','Cycling'),('Thunderous Wrath','Miracle'),('Burst Lightning','Kicker'),('Abrade','artifact')):
   self.assertTrue(any(word in s for s in self.cases(name)[0]['_recipe']['untested_components']))
 def test_kicker_before_target_has_distinct_exact_payment_path(self):
  s=self.cases('Fight with Fire')[0]['steps'];choice=next(i for i,a in enumerate(s) if a['op']=='choose_dialog');target=next(i for i,a in enumerate(s) if a['op']=='choose_target')
  self.assertLess(choice,target);self.assertEqual(s[choice]['value'],1)
  self.assertNotIn('choose_dialog',[a['op'] for a in self.cases('Fight with Fire',['payment_0'])[0]['steps']])
 def test_sweep_keeps_controller_predicate_and_pending_snapshot(self):
  c=self.cases('Blazing Volley',['pending_snapshot_1'])[0];s=c['steps'];ops=[a['op'] for a in s]
  self.assertEqual(sum(a['op']=='cast' and a['ref']=='source' for a in s),1)
  self.assertEqual([a['player'] for a in s if a['op']=='assert_mana_total' and a['value']==59],[1,1])
  self.assertIn(dict(op='assert_card',ref='permanent_0_2',property='damage',value=0),s)
  self.assertIn(dict(op='assert_card',ref='permanent_1_2',property='damage',value=1),s)
 def test_fixed_printed_life_gain_is_active_and_does_not_change_pending_life(self):
  c=self.cases('Creeping Chill',['snapshot_1'])[0];s=c['steps']
  self.assertEqual([a['value'] for a in s if a['op']=='assert_life' and a['player']==1],[20,23,20,23])
  c=self.cases('Creeping Chill',['insufficient_mana_1'])[0]
  self.assertIn(dict(op='assert_life',player=1,value=20),c['steps'])
  self.assertNotIn(dict(op='assert_life',player=1,value=23),c['steps'])
 def test_player_target_snapshot_ownership_is_mirrored_on_both_replays(self):
  s=self.cases('Boros Charm',['snapshot_1'])[0]['steps']
  self.assertEqual([a['player'] for a in s if a['op']=='choose_player'],[1,1])
  self.assertEqual([a['value'] for a in s if a['op']=='choose_player'],[0,0])
 def test_alternate_modes_do_not_create_false_whole_card_denials(self):
  for name,variant in (('Jeskai Charm','self_player_0'),('Take Down','wrong_flying_0'),('Mizzium Mortars','wrong_controller_0')):
   with self.assertRaises(ValueError):self.cases(name,[variant])
 def test_suspend_normal_choice_is_present_when_exact_printed_payment_also_affords_suspend(self):
  for c in self.cases('Shivan Meteor',['survivor_0','payment_0','payment_1']):
   s=c['steps'];choice=next(i for i,a in enumerate(s) if a['op']=='choose_dialog');target=next(i for i,a in enumerate(s) if a['op']=='choose_target');cast=next(i for i,a in enumerate(s) if a['op']=='cast' and a['ref']=='source')
   self.assertLess(choice,target);self.assertLess(target,cast);self.assertEqual(s[choice]['value'],0)
   self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=13),s)
  for c in self.cases('Shivan Meteor',['insufficient_mana_0','wrong_payment_color_1']):
   self.assertNotIn('choose_dialog',[s['op'] for s in c['steps']])

if __name__=='__main__':unittest.main()
