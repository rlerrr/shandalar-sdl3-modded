import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from self_keyword_activation_recipes import spec,variants

class SelfKeywordActivationRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Goblin Balloon Brigade',selected=None):
  params={'Goblin Balloon Brigade':([],['Flying'],'{R}'),
   'Disciple of the Old Ways':([],['First strike'],'{R}'),
   'Searchlight Geist':(['Flying'],['Deathtouch'],'{3}{B}'),
   'Stonefare Crocodile':([],['Lifelink'],'{2}{B}'),
   'Stonehorn Chanter':([],['Vigilance','Lifelink'],'{5}{W}'),
   'Wall of Tanglecord':(['Defender'],['Reach'],'{G}'),
   'Horror of the Dim':([],['Hexproof'],'{U}'),
   'Glimmering Angel':(['Flying'],['Shroud'],'{U}'),
   'Wily Bandar':([],['Indestructible'],'{2}{G}'),
   'Weldfast Monitor':([],['Menace'],'{R}')}[name]
  return dict(family='self_keyword_activation',version=2,params=dict(zip(('keywords','grants','mana_cost'),params)),oracle_text=self.o[name]['oracle_text'],variants=selected or ['effect_0'])
 def cases(self,name='Goblin Balloon Brigade',selected=None,d=None,o=None,rows=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.o[name],rows or self.rows,oracle_context=o or self.o)
 def test_explicit_cost_keywords_and_whole_pin(self):
  d=self.declaration()
  for change in ({'oracle_text':'changed'},{'params':{}},{'params':dict(d['params'],grants=['Persist'])},{'params':dict(d['params'],grants=['Flying','Flying'])},{'params':dict(d['params'],grants=['Vigilance'])},{'params':dict(d['params'],mana_cost='{X}')},{'params':dict(d['params'],keywords=['Flying'])},{'params':dict(d['params'],tap=True)}):
   with self.assertRaises(ValueError):self.cases(d=dict(d,**change))
 def test_unreviewed_clauses_fail_closed(self):
  for line in ('{T}: Draw a card.','When this creature dies, draw a card.','Other creatures you control get +1/+1.'):
   r=dict(self.o['Goblin Balloon Brigade'],oracle_text=self.o['Goblin Balloon Brigade']['oracle_text']+'\n'+line);d=self.declaration();d['oracle_text']=r['oracle_text']
   with self.assertRaises(ValueError):spec(d,r)
  r=dict(self.o['Goblin Balloon Brigade'],oracle_text='Double strike\n'+self.o['Goblin Balloon Brigade']['oracle_text'],keywords=['Double strike'])
  d=self.declaration();d['oracle_text']=r['oracle_text'];d['params']['keywords']=['Double strike']
  with self.assertRaises(ValueError):spec(d,r)
 def test_only_selected_unique_supported_variants(self):
  for vs in ([],['bad_0'],['effect_0','effect_0']):
   d=self.declaration();d['variants']=vs
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_real_stack_has_no_target_choice(self):
  for c in self.cases(selected=['effect_0','effect_1']):
   s=c['steps'];self.assertFalse(any(x['op']=='choose_target' for x in s));self.assertIn(dict(op='assert_stack',value=1),s);self.assertLess(s.index(dict(op='resolve')),s.index(dict(op='begin_attackers')))
 def test_flying_evasion_reach_and_expiry(self):
  live,expired,reach=self.cases(selected=['effect_0','expired_probe_1','reach_blocker_0'])
  self.assertIn(dict(op='block',ref='witness',other='source',value=0),live['steps']);self.assertIn(dict(op='block',ref='witness',other='source',value=1),expired['steps']);self.assertIn(dict(op='cleanup'),expired['steps']);self.assertIn(dict(op='block',ref='witness',other='source',value=1),reach['steps'])
 def test_first_strike_uses_return_damage_not_a_survival_shortcut(self):
  live,expired=self.cases('Disciple of the Old Ways',['effect_0','expired_probe_0'])
  self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),live['steps']);self.assertIn(dict(op='assert_card',ref='source',property='damage',value=1),expired['steps'])
 def test_deathtouch_tracks_both_deaths_and_exact_receipts(self):
  s=self.cases('Searchlight Geist')[0]['steps']
  for ref in ('source','witness'):self.assertIn(dict(op='assert_card',ref=ref,property='in_play',value=0),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Scaled Wurm']['csvid'],value=1),s)
 def test_lifelink_and_compound_vigilance(self):
  life=self.cases('Stonefare Crocodile')[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=23),life)
  compound=self.cases('Stonehorn Chanter')[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=24),compound);self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),compound)
 def test_reach_retains_defender_and_checks_flying_blockers(self):
  live,expired=self.cases('Wall of Tanglecord',['effect_1','expired_probe_1']);self.assertIn(dict(op='block',ref='source',other='witness',value=1),live['steps']);self.assertIn(dict(op='block',ref='source',other='witness',value=0),expired['steps'])
 def test_shroud_and_hexproof_distinguish_owner_targeting(self):
  for nm,owner_legal in (('Glimmering Angel',0),('Horror of the Dim',1)):
   s=self.cases(nm)[0]['steps'];self.assertIn(dict(op='can_target_creature',ref='owner_probe',other='source',value=owner_legal),s);self.assertIn(dict(op='can_target_creature',ref='enemy_probe',other='source',value=0),s)
 def test_indestructibility_damage_toughness_exile_and_bounce(self):
  live,damage,zero,exile,bounce=self.cases('Wily Bandar',['effect_0','lethal_damage_0','zero_toughness_0','exile_0','bounce_after_resolution_0'])
  self.assertIn(dict(op='destroy',ref='source'),live['steps']);self.assertIn(dict(op='assert_card',ref='source',property='damage',value=4),damage['steps'])
  for c,zone in ((zero,'graveyard'),(exile,'exile'),(bounce,'hand')):
   self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),c['steps']);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone=zone,index=self.rows['Wily Bandar']['csvid'],value=1),c['steps'])
 def test_menace_checks_completed_declaration(self):
  single,pair=self.cases('Weldfast Monitor',['effect_0','two_blockers_1'])
  self.assertIn(dict(op='block',ref='witness',other='source',value=0),single['steps'])
  self.assertIn(dict(op='block',ref='witness',other='source',value=1),pair['steps'])
  single,pair=self.cases('Weldfast Monitor',['effect_0','two_blockers_1']);self.assertIn(dict(op='advance_combat'),single['steps']);self.assertIn(dict(op='assert_card',ref='witness',property='blocking_count',value=0),single['steps']);self.assertIn(dict(op='assert_card',ref='source',property='blocked',value=1),pair['steps'])
 def test_costs_and_no_raw_sickness_expectation(self):
  paid,wrong,tax,fresh=self.cases(selected=['payment_0','wrong_color_1','tax_1','fresh_activation_0']);self.assertIn(dict(op='assert_mana',player=0,color=4,value=0),paid['steps']);self.assertIn(dict(op='require_can_activate',ref='source',value=0),wrong['steps']);self.assertIn(dict(op='set_mana',player=1,color=0,value=2),tax['steps']);self.assertIn(dict(op='require_can_activate',ref='source',value=1),fresh['steps']);self.assertFalse(any(s['op']=='assert_card' and s.get('property')=='sick' for s in fresh['steps']))
 def test_unaffected_creature_and_self_shroud_do_not_block_ability(self):
  unaffected,shroud=self.cases(selected=['unaffected_witness_0','shrouded_source_1']);self.assertIn(dict(op='block',ref='witness',other='unselected',value=1),unaffected['steps']);self.assertIn(dict(op='require_can_activate',ref='source',value=1),shroud['steps'])
  dead=self.cases('Searchlight Geist',['shrouded_source_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='robe',property='in_play',value=0),dead)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Robe of Mirrors']['csvid'],value=1),dead)
 def test_snapshots_include_zones_and_gameplay(self):
  for c in self.cases(selected=['ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0']):
   s=c['steps'];cut=s.index(dict(op='restore'));self.assertIn(dict(op='ai_roundtrip'),s);self.assertIn(dict(op='assert_recorded_state'),s);self.assertTrue(any(x['op']=='assert_zone_multiplicity' for x in s[cut:]));self.assertTrue(any(x['op']=='assert_zone_card' for x in s[cut:]));self.assertFalse(any(x['op']=='event' for x in s))
 def test_handler_hints_never_supply_expectations(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy';r['mana_cost']='{99}'
  self.assertEqual(self.cases(),self.cases(rows=rows))
 def test_witness_changes_and_custom_decisions_rejected(self):
  o=copy.deepcopy(self.o);o['Llanowar Elves']['toughness']='2'
  with self.assertRaises(ValueError):self.cases(o=o)
  with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),support_permanents=['Forest']))

if __name__=='__main__':unittest.main()
