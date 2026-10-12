import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand
from damage_activation_recipes import spec,variants

class DamageActivationRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Prodigal Sorcerer',selected=None):
  profiles={'Prodigal Sorcerer':dict(damage=1,targets='any',tap=True),
   'Cunning Sparkmage':dict(damage=1,targets='any',tap=True),
   'Mogg Fanatic':dict(damage=1,targets='any',sacrifice=True),
   'Centaur Archer':dict(damage=1,targets='flying_creature',tap=True),
   'Crossbow Infantry':dict(damage=1,targets='combat_creature',tap=True),
   'Brothers of Fire':dict(damage=1,targets='any',mana_cost='{1}{R}{R}',damage_you=1),
   'Psionic Entity':dict(damage=2,targets='any',tap=True,damage_self=3),
   'Reckless Assault':dict(damage=1,targets='any',mana_cost='{1}',pay_life=2),
   'Tower of Calamities':dict(damage=12,targets='creature',mana_cost='{8}',tap=True)}
  p=dict(keywords=sorted(self.oracle[name]['keywords']),damage=1,targets='any',mana_cost='',tap=False,sacrifice=False,pay_life=0,damage_you=0,damage_self=0,controller_life_loss=0,tap_target=False);p.update(profiles[name])
  return dict(family='damage_activation',version=2,oracle_text=self.oracle[name]['oracle_text'],params=p,variants=selected or ['effect_0'])
 def cases(self,name='Prodigal Sorcerer',selected=None,d=None,rows=None,oracle=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],rows or self.rows,oracle_context=oracle or self.oracle)
 def test_pin_cost_scope_and_types_fail_closed(self):
  d=self.declaration()
  for change in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],damage=True)),dict(params=dict(d['params'],damage=13)),dict(params=dict(d['params'],targets=[])),dict(params=dict(d['params'],targets='land')),dict(params=dict(d['params'],tap=1)),dict(params=dict(d['params'],mana_cost='{X}'))):
   with self.assertRaises(ValueError):self.cases(d=dict(d,**change))
 def test_additional_abilities_and_unreviewed_costs_rejected(self):
  for line in ('\n{T}: Add {U}.',' Draw a card.','\nWhen this creature enters, draw a card.'):
   r=dict(self.oracle['Prodigal Sorcerer'],oracle_text=self.oracle['Prodigal Sorcerer']['oracle_text']+line);d=self.declaration();d['oracle_text']=r['oracle_text']
   with self.assertRaises(ValueError):spec(d,r)
 def test_variants_unknown_empty_duplicates_rejected(self):
  for vs in ([],['bad_0'],['effect_0','effect_0']):
   d=self.declaration();d['variants']=vs
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_damage_waits_for_resolution_and_tapping_is_cost(self):
  s=self.cases()[0]['steps'];cut=s.index(dict(op='resolve'));self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),s[cut:])
 def test_sacrifice_happens_before_damage(self):
  s=self.cases('Mogg Fanatic')[0]['steps'];cut=s.index(dict(op='resolve'));self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),s[cut:]);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Mogg Fanatic']['csvid'],value=1),s)
 def test_player_choices_cover_both_controllers(self):
  for owner in (0,1):
   s=self.cases(selected=[f'opponent_player_{owner}'])[0]['steps'];self.assertIn(dict(op='choose_player',player=owner,value=1-owner),s);self.assertIn(dict(op='assert_life',player=1-owner,value=19),s)
 def test_haste_is_activation_legality_not_cleared_raw_flags(self):
  s=self.cases('Cunning Sparkmage',['fresh_activation_0'])[0]['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=1),s);self.assertFalse(any(x['op']=='assert_card' and x.get('property')=='sick' for x in s))
 def test_nontapping_sick_source_can_activate(self):
  s=self.cases('Brothers of Fire',['sick_0'])[0]['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=1),s)
 def test_exact_payment_and_tax(self):
  payment,tax=self.cases('Brothers of Fire',['payment_1','tax_1']);self.assertIn(dict(op='set_mana',player=1,color=4,value=2),payment['steps']);self.assertIn(dict(op='set_mana',player=1,color=0,value=3),tax['steps'])
 def test_life_payment_is_a_cost_not_delayed_damage(self):
  s=self.cases('Reckless Assault')[0]['steps'];cut=s.index(dict(op='resolve'));self.assertIn(dict(op='assert_life',player=0,value=18),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s[:cut]);s=self.cases('Reckless Assault',['insufficient_life_0'])[0]['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
 def test_collateral_player_and_source_damage(self):
  s=self.cases('Brothers of Fire')[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=19),s);s=self.cases('Psionic Entity')[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s)
 def test_high_damage_has_surviving_and_lethal_witnesses(self):
  c=self.cases('Tower of Calamities')[0];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='recipient'),'Krosan Cloudscraper');self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=12),c['steps'])
 def test_combat_restrictions_use_legal_declarations(self):
  s=self.cases('Crossbow Infantry')[0]['steps'];self.assertLess(s.index(dict(op='advance_combat')),s.index(dict(op='activate',ref='source',value=1)));self.assertFalse(any(x['op']=='event' for x in s))
 def test_dead_declared_blocker_leaves_attacker_blocked(self):
  s=self.cases('Crossbow Infantry',['declared_blocker_death_0'])[0]['steps'];self.assertIn(dict(op='block',ref='recipient',other='attacker',value=1),s);self.assertIn(dict(op='assert_life',player=1,value=20),s);self.assertIn(dict(op='combat_damage'),s)
 def test_flying_indestructible_target_is_actually_flying(self):
  c=self.cases('Centaur Archer',['indestructible_0'])[0];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='recipient'),'Darksteel Gargoyle')
 def test_prevention_and_player_shroud_responses_are_instants(self):
  for v,nm in (('prevention_0','Safe Passage'),('player_shrouded_pending_1','Gilded Light')):
   c=self.cases(selected=[v])[0];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='response'),nm);self.assertIn(dict(op='cast',ref='response',value=1),c['steps'])
 def test_pending_creature_shroud_response_has_flash(self):
  for owner in (0,1):
   c=self.cases(selected=[f'target_shrouded_pending_{owner}'])[0]
   self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='response'),"Alexi's Cloak")
   self.assertIn('Flash',self.oracle["Alexi's Cloak"]['oracle_text'].splitlines())
   self.assertIn(dict(op='cast',ref='response',value=1),c['steps'])
 def test_fizzle_does_not_apply_collateral_effect(self):
  s=self.cases('Brothers of Fire',['target_bounced_0'])[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=20),s)
 def test_cleanup_clears_damage_but_not_tapping(self):
  s=self.cases(selected=['cleanup_0'])[0]['steps'];cut=s.index(dict(op='cleanup'));self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),s[:cut]);self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s[cut:]);self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s[cut:])
 def test_snapshots_restore_all_zones_and_decisions(self):
  for c in self.cases(selected=['ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0']):
   s=c['steps'];cut=s.index(dict(op='restore'));self.assertIn(dict(op='ai_roundtrip'),s);self.assertIn(dict(op='assert_recorded_state'),s);self.assertTrue(any(x['op']=='assert_zone_multiplicity' for x in s[cut:]));self.assertTrue(any(x['op']=='assert_zone_card' for x in s[cut:]))
 def test_handlers_do_not_supply_damage_expectations(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy';r['mana_cost']='{99}'
  self.assertEqual(self.cases(),self.cases(rows=rows))
 def test_witness_mutation_and_custom_setup_rejected(self):
  o=copy.deepcopy(self.oracle);o['Safe Passage']['oracle_text']='Prevent 1 damage.'
  with self.assertRaises(ValueError):self.cases(oracle=o)
  with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),support_permanents=['Forest']))
 def test_variants_unique_and_self_sacrificing_source_not_reused(self):
  for nm in ('Prodigal Sorcerer','Mogg Fanatic','Crossbow Infantry'):
   vs=variants(spec(self.declaration(nm),self.oracle[nm]));self.assertEqual(len(vs),len(set(vs)))
  self.assertNotIn('untap_repeat_0',variants(spec(self.declaration('Mogg Fanatic'),self.oracle['Mogg Fanatic'])))

if __name__=='__main__':unittest.main()
