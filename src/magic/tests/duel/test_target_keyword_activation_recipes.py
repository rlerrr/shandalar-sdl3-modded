import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from target_keyword_activation_recipes import spec,variants

class TargetKeywordActivationRecipesTest(unittest.TestCase):
 def test_pending_shroud_response_has_flash(self):
  for owner in (0,1):
   c=self.cases(selected=[f'target_shrouded_pending_{owner}'])[0]
   self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='response'),"Alexi's Cloak")
   self.assertIn('Flash',self.o["Alexi's Cloak"]['oracle_text'].splitlines())
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Caller of Gales',selected=None):
  params={'Caller of Gales':([], 'Flying','creature','{1}{U}',True),
   'Advance Scout':(['First strike'],'First strike','creature','{W}',False),
   'Alabaster Mage':([], 'Lifelink','own_creature','{1}{W}',False),
   'Onyx Mage':([], 'Deathtouch','own_creature','{1}{B}',False),
   'Deathless Angel':(['Flying'],'Indestructible','creature','{W}{W}',False),
   'Shell Skulkin':([], 'Shroud','blue_creature','{3}',False),
   'Akki Drillmaster':([], 'Haste','creature','',True),
   'Bloodthorn Taunter':(['Haste'],'Haste','power_five','',True),
   'Beacon Behemoth':([], 'Vigilance','power_five','{1}',False),
   'Veteran Cathar':([], 'Double strike','human','{3}{W}',False),
   'Elvish Herder':([], 'Trample','creature','{G}',False)}[name]
  return dict(family='target_keyword_activation',version=2,params=dict(zip(('keywords','grant','targets','mana_cost','tap'),params)),oracle_text=self.o[name]['oracle_text'],variants=selected or ['effect_0'])
 def cases(self,name='Caller of Gales',selected=None,d=None,o=None,rows=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.o[name],rows or self.rows,oracle_context=o or self.o)
 def test_schema_and_whole_pin_fail_closed(self):
  d=self.declaration()
  for change in ({'oracle_text':'changed'},{'params':{}},{'params':dict(d['params'],grant='Persist')},{'params':dict(d['params'],tap=1)},{'params':dict(d['params'],targets='player')},{'params':dict(d['params'],mana_cost='{X}')},{'params':dict(d['params'],grant='Trample')},{'params':dict(d['params'],keywords=['Flying'])}):
   with self.assertRaises(ValueError):self.cases(d=dict(d,**change))
 def test_extra_rules_not_certified(self):
  for line in ('When this creature dies, draw a card.','{T}: Draw a card.','Other creatures you control get +1/+1.'):
   r=dict(self.o['Caller of Gales'],oracle_text=self.o['Caller of Gales']['oracle_text']+'\n'+line);d=self.declaration();d['oracle_text']=r['oracle_text']
   with self.assertRaises(ValueError):spec(d,r)
 def test_variants_must_be_explicit_and_supported(self):
  for selected in ([],['bad_0'],['effect_0','effect_0']):
   d=self.declaration();d['variants']=selected
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_real_stack_and_cost_before_combat(self):
  for c,owner in zip(self.cases(selected=['effect_0','effect_1']),(0,1)):
   s=c['steps'];resolve=s.index(dict(op='resolve'));attack=s.index(dict(op='begin_attackers'))
   self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s[:resolve]);self.assertIn(dict(op='assert_stack',value=1),s[:resolve]);self.assertLess(resolve,attack)
   self.assertIn(dict(op='choose_target',player=owner,ref='recipient'),s)
 def test_flying_probes_block_evasion_and_expiry(self):
  live,expired=self.cases(selected=['effect_0','expired_probe_1'])
  self.assertIn(dict(op='block',ref='blocker',other='recipient',value=0),live['steps']);self.assertIn(dict(op='block',ref='blocker',other='recipient',value=1),expired['steps']);self.assertIn(dict(op='cleanup'),expired['steps'])
 def test_first_strike_life_and_trample_have_consequences(self):
  first=self.cases('Advance Scout')[0]['steps'];self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),first)
  life=self.cases('Alabaster Mage')[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=22),life)
  trample=self.cases('Elvish Herder')[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=19),trample)
 def test_deathtouch_tracks_both_deaths(self):
  s=self.cases('Onyx Mage')[0]['steps']
  for ref in ('recipient','blocker'):self.assertIn(dict(op='assert_card',ref=ref,property='in_play',value=0),s)
 def test_indestructible_uses_destroy_not_stats(self):
  live,expired=self.cases('Deathless Angel',['effect_0','expired_probe_0'])
  self.assertIn(dict(op='destroy',ref='recipient'),live['steps']);self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),live['steps']);self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=0),expired['steps'])
 def test_shroud_tests_specific_targets_from_both_players(self):
  live,expired=self.cases('Shell Skulkin',['effect_0','expired_probe_1'])
  for ref in ('owner_probe','enemy_probe'):
   self.assertIn(dict(op='can_target_creature',ref=ref,other='recipient',value=0),live['steps']);self.assertIn(dict(op='can_target_creature',ref=ref,other='recipient',value=1),expired['steps'])
 def test_haste_fresh_recipient_and_no_grant_control(self):
  live,control=self.cases('Akki Drillmaster',['effect_0','fresh_without_grant_1'])
  self.assertEqual(next(c for c in live['setup'] if c['ref']=='recipient')['state'],['sick']);self.assertIn(dict(op='attack',ref='recipient',value=1),live['steps']);self.assertIn(dict(op='attack',ref='recipient',value=0),control['steps']);self.assertFalse(any(s['op']=='activate' for s in control['steps']))
 def test_vigilance_and_double_strike_have_observable_damage(self):
  vigilance=self.cases('Beacon Behemoth')[0]['steps'];self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=0),vigilance)
  double=self.cases('Veteran Cathar')[0]['steps'];self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),double)
 def test_no_aggregate_cast_or_synthetic_combat_events(self):
  for nm in ('Caller of Gales','Alabaster Mage','Onyx Mage','Akki Drillmaster','Veteran Cathar'):
   s=self.cases(nm)[0]['steps'];self.assertIn(dict(op='advance_combat'),s);self.assertFalse(any(x['op']=='event' for x in s))
 def test_unselected_target_is_not_granted_effect(self):
  for nm in ('Caller of Gales','Akki Drillmaster'):
   s=self.cases(nm,['unselected_probe_0'])[0]['steps'];self.assertIn(dict(op='attack',ref='unselected',value=int(nm!='Akki Drillmaster')),s)
  s=self.cases('Shell Skulkin',['unselected_probe_0'])[0]['steps'];self.assertIn(dict(op='can_target_creature',ref='enemy_probe',other='unselected',value=1),s)
 def test_cost_sickness_shroud_and_illegal_targets_are_not_forced(self):
  for variant in ('sick_0','wrong_color_1','no_targets_1','tax_insufficient_0','shrouded_targets_0'):
   s=self.cases(selected=[variant])[0]['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=0),s);self.assertFalse(any(x['op']=='choose_target' for x in s[s.index(dict(op='require_can_activate',ref='source',value=0)):]))
 def test_snapshots_replay_combat_and_exact_zones(self):
  for c in self.cases(selected=['ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0']):
   s=c['steps'];cut=s.index(dict(op='restore'));self.assertIn(dict(op='ai_roundtrip'),s);self.assertIn(dict(op='assert_recorded_state'),s);self.assertTrue(any(x['op']=='assert_zone_multiplicity' for x in s[cut:]));self.assertTrue(any(x['op']=='assert_zone_card' for x in s[cut:]))
 def test_repeat_removes_alternatives_and_checks_legality_first(self):
  s=self.cases(selected=['untap_repeat_1'])[0]['steps']
  self.assertLess(s.index(dict(op='destroy',ref='unselected')),s.index(dict(op='activate',ref='source',value=1)))
  last_choose=max(i for i,x in enumerate(s) if x['op']=='choose_target' and x['ref']=='recipient')
  self.assertEqual(s[last_choose-1],dict(op='require_can_activate',ref='source',value=1))
 def test_haste_is_action_permission_not_cleared_raw_sickness(self):
  for c in self.cases('Bloodthorn Taunter',['cast_0','cast_payment_1','fresh_activation_0']):
   self.assertFalse(any(s['op']=='assert_card' and s.get('property')=='sick' for s in c['steps']))
  s=self.cases('Bloodthorn Taunter',['fresh_activation_0'])[0]['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=1),s);self.assertIn(dict(op='attack',ref='recipient',value=1),s)
 def test_identity_only_registry_and_witness_pins(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy';r['mana_cost']='{99}'
  self.assertEqual(self.cases(),self.cases(rows=rows))
  o=copy.deepcopy(self.o);o['Grizzly Bears']['power']='3'
  with self.assertRaises(ValueError):self.cases(o=o)
  with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),support_permanents=['Forest']))

if __name__=='__main__':unittest.main()
