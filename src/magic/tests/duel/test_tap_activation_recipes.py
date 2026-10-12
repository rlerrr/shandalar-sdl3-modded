import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from tap_activation_recipes import spec,variants

class TapActivationRecipesTest(unittest.TestCase):
 def test_pending_shroud_response_has_flash(self):
  for owner in (0,1):
   c=self.cases(selected=[f'target_shrouded_pending_{owner}'])[0]
   self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='response'),"Alexi's Cloak")
   self.assertIn('Flash',self.oracle["Alexi's Cloak"]['oracle_text'].splitlines())
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Blinding Mage',selected=None):
  fields={'Blinding Mage':('tap','creature','{W}',True),
   'Air Servant':('tap','flying_creature','{2}{U}',False),
   'Seeker of Skybreak':('untap','creature','',True),
   'Arbor Elf':('untap','forest','',True),
   'Greenside Watcher':('untap','gate','',True),
   'Voltaic Key':('untap','artifact','{1}',True),
   'Voltaic Construct':('untap','artifact_creature','{2}',False),
   'Icy Manipulator':('tap','artifact_creature_land','{1}',True),
   'Touchstone':('tap','enemy_artifact','',True),
   'Ali Baba':('tap','wall','{R}',False),
   'Ephara\'s Warden':('tap','small_power_3','',True)}[name]
  return dict(family='tap_activation',version=2,oracle_text=self.oracle[name]['oracle_text'],
   params=dict(zip(('effect','targets','mana_cost','tap'),fields),keywords=sorted(self.oracle[name]['keywords'])),variants=selected or ['effect_0'])
 def cases(self,name='Blinding Mage',selected=None,d=None,rows=None,oracle=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],rows or self.rows,oracle_context=oracle or self.oracle)
 def test_parameters_and_whole_pin_fail_closed(self):
  d=self.declaration()
  for change in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],tap=1)),dict(params=dict(d['params'],effect='destroy')),dict(params=dict(d['params'],targets='player')),dict(params=dict(d['params'],mana_cost='{X}')),dict(params=dict(d['params'],keywords=['Flying']))):
   with self.assertRaises(ValueError):self.cases(d=dict(d,**change))
 def test_extra_effects_and_mana_modes_rejected(self):
  for line in ('\n{T}: Add {W}.',' This creature deals 1 damage to that creature.','\nWhen this creature dies, draw a card.'):
   r=dict(self.oracle['Blinding Mage'],oracle_text=self.oracle['Blinding Mage']['oracle_text']+line);d=self.declaration();d['oracle_text']=r['oracle_text']
   with self.assertRaises(ValueError):spec(d,r)
 def test_unreviewed_haste_source_and_nonstrings_fail_closed(self):
  r=dict(self.oracle['Blinding Mage'],oracle_text='Haste\n'+self.oracle['Blinding Mage']['oracle_text'],keywords=['Haste']);d=self.declaration();d['oracle_text']=r['oracle_text'];d['params']['keywords']=['Haste']
  with self.assertRaises(ValueError):spec(d,r)
  for k,value in (('effect',None),('targets',[])):
   d=self.declaration();d['params'][k]=value
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_unknown_empty_duplicate_variants_rejected(self):
  for vs in ([],['bad_0'],['effect_0','effect_0']):
   d=self.declaration();d['variants']=vs
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_tap_cost_precedes_target_effect(self):
  s=self.cases()[0]['steps'];cut=s.index(dict(op='resolve'))
  self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=0),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=1),s[cut:])
 def test_untap_waits_for_resolution(self):
  s=self.cases('Seeker of Skybreak')[0]['steps'];cut=s.index(dict(op='resolve'))
  self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=1),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=0),s[cut:])
 def test_both_controllers_and_friendly_targets(self):
  for owner in (0,1):
   c=self.cases(selected=[f'friendly_{owner}'])[0];self.assertIn(dict(op='choose_target',player=owner,ref='recipient'),c['steps']);self.assertEqual(next(x for x in c['setup'] if x['ref']=='recipient')['player'],owner)
 def test_redundant_tap_untap_is_rules_legal(self):
  for nm in ('Blinding Mage','Seeker of Skybreak'):
   c=self.cases(nm,['already_in_state_0'])[0];self.assertIn(dict(op='require_can_activate',ref='source',value=1),c['steps'])
 def test_source_sickness_only_matters_for_creature_tap_cost(self):
  for nm,legal in (('Blinding Mage',0),('Air Servant',1)):
   s=self.cases(nm,['fresh_activation_1'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='sick',value=1),s);self.assertIn(dict(op='require_can_activate',ref='source',value=legal),s)
 def test_source_tapped_without_tap_cost_is_legal(self):
  s=self.cases('Air Servant',['tapped_0'])[0]['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=1),s)
 def test_exact_mana_and_tax_include_costs(self):
  payment,tax,wrong=self.cases(selected=['payment_1','tax_1','wrong_color_1']);self.assertIn(dict(op='assert_mana',player=1,color=5,value=0),payment['steps']);self.assertIn(dict(op='set_mana',player=1,color=0,value=2),tax['steps']);self.assertIn(dict(op='require_can_activate',ref='source',value=0),wrong['steps'])
 def test_wrong_restricted_target_is_not_forced(self):
  s=self.cases('Air Servant',['wrong_target_0'])[0]['steps'];cut=s.index(dict(op='require_can_activate',ref='source',value=0));self.assertFalse(any(x['op']=='choose_target' for x in s[cut:]));self.assertIn(dict(op='choose_target',player=0,ref='source'),s[:cut])
 def test_source_is_not_ignored_in_negative_target_boards(self):
  s=self.cases(selected=['no_targets_0'])[0]['steps'];self.assertIn(dict(op='choose_target',player=0,ref='source'),s);self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
  for nm in ('Voltaic Key','Icy Manipulator'):self.assertNotIn('no_targets_0',variants(spec(self.declaration(nm),self.oracle[nm])))
 def test_self_untap_pays_tap_cost_before_untapping_source(self):
  s=self.cases('Seeker of Skybreak',['self_target_0'])[0]['steps'];cut=s.index(dict(op='resolve'));self.assertIn(dict(op='choose_target',player=0,ref='source'),s[:cut]);self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s[:cut]);self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),s[cut:])
 def test_combat_uses_declarations_and_tapped_legality(self):
  for nm,value in (('Blinding Mage',0),('Seeker of Skybreak',1)):
   s=self.cases(nm,['combat_attack_0'])[0]['steps'];self.assertIn(dict(op='attack',ref='recipient',value=value),s);self.assertFalse(any(x['op']=='event' for x in s))
 def test_tapping_declared_attacker_does_not_remove_it(self):
  s=self.cases(selected=['declared_attacker_0'])[0]['steps'];self.assertLess(s.index(dict(op='attack',ref='recipient',value=1)),s.index(dict(op='activate',ref='source',value=1)));self.assertIn(dict(op='assert_life',player=0,value=18),s)
 def test_tapping_declared_blocker_still_blocks_and_deals_damage(self):
  s=self.cases(selected=['declared_blocker_0'])[0]['steps'];self.assertIn(dict(op='block',ref='recipient',other='attacker',value=1),s);self.assertIn(dict(op='assert_life',player=1,value=20),s);self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=0),s)
 def test_defender_target_only_gets_blocking_scenarios(self):
  vs=variants(spec(self.declaration('Ali Baba'),self.oracle['Ali Baba']));self.assertIn('combat_block_0',vs);self.assertNotIn('combat_attack_0',vs)
 def test_land_mana_activation_is_real_and_immediate(self):
  s=self.cases('Arbor Elf',['mana_source_1'])[0]['steps'];self.assertIn(dict(op='activate',ref='recipient',value=1),s);self.assertIn(dict(op='assert_mana',player=1,color=3,value=11),s)
 def test_illegal_pending_target_does_not_redirect(self):
  s=self.cases(selected=['target_shrouded_pending_0'])[0]['steps'];self.assertIn(dict(op='choose_target',player=0,ref='recipient'),s);self.assertEqual(s[-1].get('op'),'assert_zone_card');self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=0),s)
 def test_snapshot_replays_keep_zones_and_decisions(self):
  for c in self.cases(selected=['ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0']):
   s=c['steps'];cut=s.index(dict(op='restore'));self.assertIn(dict(op='ai_roundtrip'),s);self.assertIn(dict(op='assert_recorded_state'),s);self.assertTrue(any(x['op']=='assert_zone_multiplicity' for x in s[cut:]));self.assertTrue(any(x['op']=='assert_zone_card' and x['zone']=='library' for x in s[cut:]))
 def test_handlers_and_database_costs_do_not_supply_expectations(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy';r['mana_cost']='{99}'
  self.assertEqual(self.cases(),self.cases(rows=rows))
 def test_witness_mutations_and_custom_setup_rejected(self):
  o=copy.deepcopy(self.oracle);o['Forest']['oracle_text']='{T}: Add {R}.'
  with self.assertRaises(ValueError):self.cases(oracle=o)
  with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),support_permanents=['Forest']))
 def test_gate_readiness_and_repeated_targets_use_legal_twiddles(self):
  ready,repeat=self.cases('Greenside Watcher',['already_in_state_0','untap_repeat_1'])
  s=ready['steps'];first=s.index(dict(op='activate',ref='source',value=1))
  for ref in ('ready_recipient','ready_unselected'):self.assertIn(dict(op='cast',ref=ref,value=1),s[:first])
  s=repeat['steps'];casts=[i for i,x in enumerate(s) if x==dict(op='activate',ref='source',value=1)]
  self.assertIn(dict(op='cast',ref='retap_target',value=1),s[casts[0]:casts[1]]);self.assertIn(dict(op='choose_dialog',player=1,value=0),s[casts[0]:casts[1]])
 def test_variant_names_are_unique(self):
  for nm in ('Blinding Mage','Arbor Elf','Icy Manipulator','Touchstone'):
   vs=variants(spec(self.declaration(nm),self.oracle[nm]));self.assertEqual(len(vs),len(set(vs)))

if __name__=='__main__':unittest.main()
