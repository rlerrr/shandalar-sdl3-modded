import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from target_stats_activation_recipes import spec,variants

class TargetStatsActivationRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Ghost Warden',selected=None):
  params={'Ghost Warden':dict(power=1,toughness=1,targets='creature',mana_cost='',tap=True,sacrifice=False),
   'Grandmother Sengir':dict(power=-1,toughness=-1,targets='creature',mana_cost='{1}{B}',tap=True,sacrifice=False),
   'Flowstone Overseer':dict(power=1,toughness=-1,targets='creature',mana_cost='{R}{R}',tap=False,sacrifice=False),
   'Child of Thorns':dict(power=1,toughness=1,targets='creature',mana_cost='',tap=False,sacrifice=True),
   'Infantry Veteran':dict(power=1,toughness=1,targets='attacking_creature',mana_cost='',tap=True,sacrifice=False),
   'Angelic Page':dict(power=1,toughness=1,targets='combat_creature',mana_cost='',tap=True,sacrifice=False),
   'Crossroads Consecrator':dict(power=1,toughness=1,targets='attacking_human',mana_cost='{G}',tap=True,sacrifice=False)}[name]
  return dict(family='target_stats_activation',version=2,oracle_text=self.oracle[name]['oracle_text'],params=dict(params,keywords=sorted(self.oracle[name]['keywords'])),variants=selected or ['effect_0'])
 def cases(self,name='Ghost Warden',selected=None,d=None,rows=None,oracle=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],rows or self.rows,oracle_context=oracle or self.oracle)
 def test_pending_shroud_response_has_flash(self):
  for owner in (0,1):
   c=self.cases(selected=[f'target_shrouded_pending_{owner}'])[0]
   self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='response'),"Alexi's Cloak")
   self.assertIn('Flash',self.oracle["Alexi's Cloak"]['oracle_text'].splitlines())
 def test_explicit_scope_pin_parameters_fail_closed(self):
  base=self.declaration()
  for change in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(base['params'],power=True)),dict(params=dict(base['params'],power=5)),dict(params=dict(base['params'],targets='player')),dict(params=dict(base['params'],tap=1)),dict(params=dict(base['params'],mana_cost='{X}')),dict(params=dict(base['params'],toughness=-1)),dict(params=dict(base['params'],keywords=['Flying']))):
   with self.assertRaises(ValueError):self.cases(d=dict(base,**change))
 def test_additional_abilities_and_triggers_not_certified(self):
  for line in ('{T}: Draw a card.','When this creature dies, draw a card.','Other creatures you control get +1/+1.'):
   r=dict(self.oracle['Ghost Warden'],oracle_text=self.oracle['Ghost Warden']['oracle_text']+'\n'+line);d=self.declaration();d['oracle_text']=r['oracle_text']
   with self.assertRaises(ValueError):spec(d,r)
 def test_selected_variants_unknown_duplicate_empty_rejected(self):
  for selected in ([],['bad_0'],['effect_0','effect_0']):
   d=self.declaration();d['variants']=selected
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_pending_both_owners_preserve_base_until_resolution(self):
  for c,owner in zip(self.cases(selected=['effect_0','effect_1']),(0,1)):
   s=c['steps'];cut=s.index(dict(op='resolve'));self.assertIn(dict(op='choose_target',player=owner,ref='recipient'),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=7),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=8),s[cut:])
   self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s[:cut])
 def test_sacrifice_is_a_cost_and_ability_survives_source(self):
  s=self.cases('Child of Thorns')[0]['steps'];cut=s.index(dict(op='resolve'))
  self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=8),s[cut:]);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Child of Thorns']['csvid'],value=1),s)
 def test_mana_payment_tax_wrong_color_are_distinct(self):
  payment,tax,wrong=self.cases('Flowstone Overseer',['payment_1','tax_1','wrong_color_1'])
  for c in (payment,tax):self.assertIn(dict(op='assert_mana',player=1,color=4,value=0),c['steps'])
  self.assertIn(dict(op='set_mana',player=1,color=0,value=2),tax['steps']);self.assertIn(dict(op='require_can_activate',ref='source',value=0),wrong['steps'])
 def test_tap_cost_sickness_differs_from_non_tap_ability(self):
  for nm,legal in (('Ghost Warden',0),('Flowstone Overseer',1)):
   s=self.cases(nm,['fresh_activation_1'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='sick',value=1),s);self.assertIn(dict(op='require_can_activate',ref='source',value=legal),s)
 def test_restrictions_follow_actual_combat_phase_progression(self):
  for nm,chosen in (('Infantry Veteran','effect_1'),('Angelic Page','blocking_target_0')):
   s=self.cases(nm,[chosen])[0]['steps'];self.assertLess(next(i for i,x in enumerate(s) if x['op']=='advance_combat'),next(i for i,x in enumerate(s) if x['op']=='activate'));self.assertFalse(any(x['op']=='event' for x in s))
  s=self.cases('Crossroads Consecrator',['wrong_target_0'])[0]['steps'];self.assertIn(dict(op='attack',ref='recipient',value=1),s);self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
 def test_wrong_role_and_removed_role_do_not_redirect(self):
  wrong,removed=self.cases('Infantry Veteran',['wrong_role_0','removed_from_combat_1']);self.assertIn(dict(op='require_can_activate',ref='source',value=0),wrong['steps']);s=removed['steps'];self.assertLess(s.index(dict(op='remove_from_combat',ref='recipient')),s.index(dict(op='resolve')));self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=7),s[s.index(dict(op='resolve')):])
 def test_negative_power_and_zero_toughness_keep_rules_expectations(self):
  negative,lethal=self.cases('Grandmother Sengir',['negative_power_1','lethal_0']);self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=-1),negative['steps']);self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=0),lethal['steps']);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Muck Rats']['csvid'],value=1),lethal['steps'])
 def test_cleanup_and_stacked_changes_are_independent(self):
  cleanup,stacked=self.cases('Flowstone Overseer',['cleanup_0','stacked_1']);s=cleanup['steps'];cut=s.index(dict(op='cleanup'));self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=8),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=7),s[cut:]);self.assertIn(dict(op='assert_stack',value=2),stacked['steps']);self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=9),stacked['steps'])
 def test_no_targets_does_not_treat_the_source_as_absent(self):
  s=self.cases(selected=['no_targets_0'])[0]['steps'];self.assertIn(dict(op='choose_target',player=0,ref='source'),s);self.assertIn(dict(op='require_can_activate',ref='source',value=0),s);self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s)
 def test_zone_multiplicity_order_and_snapshot_replays(self):
  for c in self.cases(selected=['ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0']):
   s=c['steps'];restore=s.index(dict(op='restore'));self.assertIn(dict(op='ai_roundtrip'),s);self.assertIn(dict(op='assert_recorded_state'),s);self.assertTrue(any(x['op']=='assert_zone_multiplicity' for x in s[restore:]));self.assertTrue(any(x['op']=='assert_zone_card' and x['zone']=='library' for x in s[restore:]))
 def test_handler_registry_does_not_supply_expected_behavior(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy';r['mana_cost']='{99}'
  self.assertEqual(self.cases(),self.cases(rows=rows))
 def test_witness_change_and_custom_setup_rejected(self):
  o=copy.deepcopy(self.oracle);o['Scaled Wurm']['power']='8'
  with self.assertRaises(ValueError):self.cases(oracle=o)
  with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),support_permanents=['Forest']))
 def test_variants_are_unique_and_multiple_combat_targets_deferred(self):
  for nm in ('Ghost Warden','Child of Thorns','Infantry Veteran'):
   d=self.declaration(nm);ns=variants(spec(d,self.oracle[nm]));self.assertEqual(len(ns),len(set(ns)))
  self.assertNotIn('two_targets_0',variants(spec(self.declaration('Infantry Veteran'),self.oracle['Infantry Veteran'])))

if __name__=='__main__':unittest.main()
