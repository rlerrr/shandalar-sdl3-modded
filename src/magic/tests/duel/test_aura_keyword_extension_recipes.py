import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from aura_keyword_extension_recipes import spec,variants,expand,pins,validate_witnesses


class AuraKeywordExtensionRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
  cls.rows={r['name']:r for r in registry()}

 def declaration(self,name='Buoyancy',vs=None):
  return dict(family='aura_keyword_extension',version=2,params=copy.deepcopy(pins()['sources'][name]['params']),
    oracle_text=self.oracle[name]['oracle_text'],variants=vs or ['unblocked_0'])

 def cases(self,name='Buoyancy',vs=None,d=None,o=None,rows=None):
  d=d or self.declaration(name,vs);o=o or self.oracle
  return expand(name,self.rows[name]['csvid'],d,spec(d,o[name]),d['variants'],rows or self.rows,o)

 def test_full_source_identity_and_rules_are_pinned(self):
  d=self.declaration()
  for k,v in (('oracle_text','Enchant creature'),('mana_cost','{9}'),('oracle_id','changed'),('type_line','Token Enchantment — Aura'),('layout','transform'),('keywords',[])):
   with self.subTest(field=k),self.assertRaises(ValueError):spec(d,dict(self.oracle['Buoyancy'],**{k:v}))

 def test_params_and_explicit_gaps_cannot_be_weakened(self):
  for key,val in (('power',1),('keywords',[]),('gaps',[]),('draw',1),('return_aura',True)):
   d=self.declaration();d['params'][key]=val
   with self.subTest(key=key),self.assertRaises(ValueError):self.cases(d=d)

 def test_variants_empty_duplicate_unknown_fail_closed(self):
  for vs in ([],['bad_0'],['unblocked_0','unblocked_0']):
   d=self.declaration();d['variants']=vs
   with self.assertRaises(ValueError):self.cases(d=d)

 def test_real_flying_evasion_has_ground_block_and_damage(self):
  for owner in (0,1):
   s=self.cases(vs=['flying_evasion_'+str(owner)])[0]['steps']
   self.assertIn(dict(op='choose_target',player=owner,ref='recipient'),s)
   self.assertIn(dict(op='block',ref='blocker',other='recipient',value=0),s)
   self.assertIn(dict(op='combat_damage'),s)
   self.assertIn(dict(op='assert_life',player=1-owner,value=18),s)

 def test_removal_restores_ground_blocking(self):
  s=self.cases(vs=['expired_flying_0'])[0]['steps']
  self.assertIn(dict(op='cast',ref='removal',value=1),s)
  self.assertIn(dict(op='block',ref='blocker',other='recipient',value=1),s)

 def test_trample_positive_and_removed_damage(self):
  pos,neg=self.cases('Epic Proportions',['trample_0','expired_trample_0'])
  self.assertIn(dict(op='assert_life',player=1,value=14),pos['steps'])
  self.assertIn(dict(op='assert_life',player=1,value=20),neg['steps'])

 def test_first_strike_prevents_return_damage(self):
  pos,neg=self.cases('Spiteful Motives',['first_strike_0','expired_first_strike_0'])
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),pos['steps'])
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),neg['steps'])

 def test_reach_uses_real_flying_attacker_and_block(self):
  c=self.cases('Aether Web',['block_flying_0'])[0]
  self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='attacker'),'Suntail Hawk')
  self.assertIn(dict(op='block',ref='recipient',other='attacker',value=1),c['steps'])
  self.assertIn('Blocking creatures with shadow as though they did not have shadow',c['_recipe']['untested_components'])

 def test_pending_aura_does_not_apply_before_resolve(self):
  s=self.cases('Epic Proportions')[0]['steps'];cut=s.index(dict(op='resolve'))
  self.assertIn(dict(op='assert_stack',value=1),s[:cut])
  self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=2),s[:cut])
  self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=7),s[cut:])

 def test_both_casters_snapshots_restore_ordered_zones(self):
  for owner in (0,1):
   c=self.cases(vs=['snapshot_'+str(owner)])[0];s=c['steps'];cut=s.index(dict(op='restore'))
   self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='source'),owner)
   self.assertIn(dict(op='turn',player=owner,value=0),s)
   self.assertIn(dict(op='ai_roundtrip'),s)
   self.assertIn(dict(op='assert_recorded_state'),s)
   self.assertTrue(any(x['op']=='assert_zone_card' for x in s[cut:]))

 def test_owner_one_snapshot_remaps_each_shared_step_only_once(self):
  s=self.cases(vs=['snapshot_1'])[0]['steps']
  choices=[x for x in s if x['op']=='choose_target']
  self.assertEqual(len(choices),2)
  self.assertTrue(all(x['player']==1 for x in choices))
  receipts=[x for x in s if x['op']=='assert_mana_total' and x['player']==1]
  self.assertEqual(len(receipts),2)
  self.assertTrue(all(x['value']==58 for x in receipts))

 def test_lost_target_has_an_explicit_paid_response_for_both_casters(self):
  for owner in (0,1):
   s=self.cases(vs=['lost_target_'+str(owner)])[0]['steps']
   cast=s.index(dict(op='cast',ref='source',value=1));response=s.index(dict(op='cast_response',ref='response',value=1))
   self.assertLess(cast,response)
   self.assertIn(dict(op='assert_stack',value=1),s[cast:response])
   self.assertIn(dict(op='choose_target',player=owner,ref='recipient'),s[cast:response])
   self.assertIn(dict(op='assert_stack',value=2),s[response:])
   self.assertIn(dict(op='resolve_response'),s[response:])
   self.assertFalse(any(x['op']=='choose_priority' for x in s))
   self.assertIn(dict(op='assert_mana_total',player=owner,value=57),s)

 def test_complete_witness_mutations_fail(self):
  for name,k,v in (('Grizzly Bears','mana_cost','{9}'),('Suntail Hawk','oracle_text',''),('Disenchant','oracle_id','changed')):
   o=copy.deepcopy(self.oracle);o[name][k]=v
   with self.assertRaises(ValueError):validate_witnesses(o,self.rows)

 def test_handler_changes_do_not_supply_expectations(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy'
  self.assertEqual(self.cases(),self.cases(rows=rows))

 def test_integer_and_boolean_profile_fields_do_not_alias(self):
  for name,key,value in (('Aether Web','power',True),('Aether Web','power',1.0),('Buoyancy','draw',False),('Buoyancy','return_aura',0)):
   d=self.declaration(name);d['params'][key]=value
   with self.subTest(name=name,key=key),self.assertRaises(ValueError):self.cases(name,d=d)

 def test_umbra_replacement_and_return_triggers_are_honest_gaps(self):
  for name in ('Boar Umbra','Drake Umbra','Felidar Umbra','Hyena Umbra','Indrik Umbra','Mammoth Umbra','Spider Umbra','Eel Umbra'):
   d=self.declaration(name);p=spec(d,self.oracle[name])
   self.assertIn('Umbra armor replacement',p['gaps'])
   self.assertNotIn('host_destroyed_0',variants(p))
  p=spec(self.declaration('Angelic Destiny'),self.oracle['Angelic Destiny'])
  self.assertIn('Aura return when enchanted creature dies',p['gaps'])
  self.assertNotIn('host_destroyed_0',variants(p))

 def test_vows_exclude_attacks_against_aura_controller(self):
  for name in ('Vow of Duty','Vow of Flight','Vow of Lightning','Vow of Wildness'):
   p=spec(self.declaration(name),self.oracle[name])
   self.assertIn('opponent_cleanup_0',variants(p))
   self.assertNotIn('opponent_unblocked_0',variants(p))
   self.assertIn('unblocked_1',variants(p))

 def test_source_shroud_is_not_removed_by_an_illegal_spell(self):
  p=spec(self.declaration('Diplomatic Immunity'),self.oracle['Diplomatic Immunity'])
  for v in ('aura_removed_0','aura_bounced_0','expired_targeting_0'):self.assertNotIn(v,variants(p))

 def test_lifelink_gains_the_damaging_creatures_controller_life(self):
  for owner in (0,1):
   s=self.cases('Eternal Thirst',['unblocked_'+str(owner)])[0]['steps']
   self.assertIn(dict(op='combat_damage'),s)
   self.assertIn(dict(op='assert_life',player=owner,value=22),s)
   self.assertIn(dict(op='assert_life',player=1-owner,value=18),s)

 def test_all_enrolled_profiles_pin_whole_source_and_have_real_effect_variant(self):
  for name in pins()['sources']:
   d=self.declaration(name);p=spec(d,self.oracle[name]);selected=variants(p)
   self.assertIn('unblocked_0',selected)
   for owner in (0,1):
    c=self.cases(name,['unblocked_'+str(owner)])[0]
    self.assertIn(dict(op='cast',ref='source',value=1),c['steps'])
    self.assertIn(dict(op='combat_damage'),c['steps'])

 def test_indrik_opposing_host_has_no_available_defending_blocker(self):
  for caster in (0,1):
   c=self.cases('Indrik Umbra',['opponent_unblocked_'+str(caster)])[0]
   control=next(x for x in c['setup'] if x['ref']=='unselected')
   self.assertEqual(control['player'],caster)
   self.assertIn('tapped',control['state'])
   self.assertIn(dict(op='assert_card',ref='unselected',property='tapped',value=1),c['steps'])
   self.assertIn(dict(op='attack',ref='recipient',value=1),c['steps'])
   self.assertIn(dict(op='assert_life',player=caster,value=14),c['steps'])


if __name__=='__main__':unittest.main()
