import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from removal_recipes import eligible,variants,PINS,TARGETS


class RemovalRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

 def declaration(self,name='Final Reward',selected=None):
  configs={
   'Final Reward':dict(effect='exile'),
   'Vindicate':dict(scope='permanent'),
   'Annihilate':dict(scope='nonblack_creature',no_regenerate=True,draw=1),
   'Last Breath':dict(effect='exile',limit=dict(field='power',op='le',value=2),target_gain_life=4),
   'Pillar of Light':dict(effect='exile',limit=dict(field='toughness',op='ge',value=4)),
   'Cast Down':dict(scope='nonlegendary_creature'),
   'Human Frailty':dict(scope='human_creature'),
   'Urgent Exorcism':dict(scope='spirit_or_enchantment'),
   'Divine Verdict':dict(state='attacking_or_blocking'),
   'Excoriate':dict(effect='exile',state='tapped'),
   'Liturgy of Blood':dict(add_mana='{B}{B}{B}'),
   'Deconstruct':dict(scope='artifact',add_mana='{G}{G}{G}'),
   'Rend Flesh':dict(scope='nonspirit_creature'),
  }
  p=dict(effect='destroy',scope='creature',state='any',controller='any',limit=None,no_regenerate=False,draw=0,gain_life=0,target_gain_life=0,target_lose_life=0,target_damage=0,add_mana='');p.update(configs[name])
  return dict(family='target_removal',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['effect_0'])

 def cases(self,name='Final Reward',selected=None,d=None,oracle=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,oracle_context=oracle or self.oracle)

 def test_parameters_and_whole_effect_pin_fail_closed(self):
  d=self.declaration();d['params']['scope']='artifact'
  with self.assertRaises(ValueError):self.cases(d=d)
  for patch in (dict(oracle_text='Destroy target creature.'),dict(params={}),dict(variants=[]),dict(variants=['unknown_0'])):
   with self.assertRaises(ValueError):self.cases(d=dict(self.declaration(),**patch))
  for patch in (dict(draw=True),dict(target_gain_life=-1),dict(no_regenerate=1),dict(add_mana='{X}'),dict(limit=dict(field='power',op='le',value=True))):
   d=self.declaration();d['params'].update(patch)
   with self.assertRaises(ValueError):self.cases(d=d)

 def test_type_intersections_and_exclusions_are_independent(self):
  p=self.declaration()['params']
  for scope,expected in [('artifact_creature',{'Ornithopter','Phyrexian Hulk'}),('nonartifact_creature',{'Grizzly Bears','Nyxborn Shieldmate'}),('artifact_or_enchantment',{'Ornithopter','Nyxborn Shieldmate','Seal of Cleansing'}),('creature_or_planeswalker',{'Grizzly Bears','Jace Beleren'})]:
   for n in expected:self.assertTrue(eligible(PINS[n],dict(p,scope=scope)),(scope,n))
  self.assertFalse(eligible(PINS['Forest'],dict(p,scope='nonland_permanent')))
  self.assertTrue(eligible(PINS['Darksteel Citadel'],dict(p,scope='artifact')))
  self.assertFalse(eligible(PINS['Ornithopter'],dict(p,scope='nonartifact_creature')))
  for n in ('Fervor',"Executioner's Capsule"):
   self.assertTrue(eligible(PINS[n],dict(p,scope='black_or_red_permanent')))
  self.assertTrue(eligible(PINS['Ajani Goldmane'],dict(p,scope='white_permanent')))

 def test_changeling_counts_as_human_and_spirit_but_not_equipment(self):
  p=self.declaration()['params']
  for nm in ('Mothdust Changeling','Avian Changeling'):
   self.assertTrue(eligible(PINS[nm],dict(p,scope='human_creature')))
   self.assertTrue(eligible(PINS[nm],dict(p,scope='spirit_or_enchantment')))
   self.assertFalse(eligible(PINS[nm],dict(p,scope='nonspirit_creature')))
   self.assertFalse(eligible(PINS[nm],dict(p,scope='equipment')))

 def test_power_and_toughness_boundaries_use_printed_characteristics(self):
  power=self.declaration('Last Breath')['params'];tough=self.declaration('Pillar of Light')['params']
  self.assertFalse(eligible(PINS['Wasp Lancer'],power));self.assertTrue(eligible(PINS['Ornithopter'],power))
  self.assertTrue(eligible(PINS['Craw Wurm'],tough));self.assertFalse(eligible(PINS['Grizzly Bears'],tough))

 def test_legendary_and_monocolored_are_distinct_from_color_identity(self):
  p=self.declaration('Cast Down')['params']
  self.assertFalse(eligible(PINS['Isamaru, Hound of Konda'],p));self.assertTrue(eligible(PINS['Savannah Lions'],p))
  p=dict(p,scope='monocolored_creature')
  self.assertFalse(eligible(PINS['Ornithopter'],p));self.assertFalse(eligible(PINS['Watchwolf'],p));self.assertTrue(eligible(PINS['Scathe Zombies'],p))

 def test_one_candidate_negative_scope_queries_do_not_add_another_legal_target(self):
  i=TARGETS.index('Sol Ring');c=self.cases(selected=[f'card_{i}_0'])[0]
  battle=[x for x in c['setup'] if x['zone']=='battlefield'];self.assertEqual([x['name'] for x in battle],['Sol Ring'])
  self.assertIn(dict(op='can_cast',ref='source',value=0),c['steps'])
  self.assertFalse(any(x['op']=='cast' and x['ref']=='source' for x in c['steps']))

 def test_positive_cast_uses_real_cost_and_exile_receipt(self):
  s=self.cases(selected=['payment_1'])[0]['steps']
  self.assertIn(dict(op='require_can_cast',ref='source',value=1),s)
  self.assertIn(dict(op='cast',ref='source',value=1),s)
  self.assertIn(dict(op='assert_mana',player=1,color=1,value=0),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=self.rows['Savannah Lions']['csvid'],value=1),s)

 def test_indestructible_is_targetable_but_survives_destruction(self):
  i=TARGETS.index('Darksteel Relic');c=self.cases('Vindicate',[f'card_{i}_0'])[0]
  self.assertIn(dict(op='require_can_cast',ref='source',value=1),c['steps'])
  self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Darksteel Relic']['csvid'],value=0),c['steps'])

 def test_combat_restrictions_use_real_declarations_and_recheck_tapping(self):
  s=self.cases('Divine Verdict',['blocking_target_0'])[0]['steps']
  for op in ('begin_attackers','finish_attackers','begin_blockers','block','advance_combat'):self.assertTrue(any(x['op']==op for x in s))
  s=self.cases('Excoriate',['state_changed_pending_0'])[0]['steps']
  self.assertIn(dict(op='choose_dialog',player=0,value=1),s)
  self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),s)

 def test_fizzled_cantrip_does_not_draw_and_mana_receipts_are_resolution_effects(self):
  s=self.cases('Annihilate',['lost_target_0'])[0]['steps']
  self.assertIn(dict(op='assert_zone',player=0,zone='library',value=7),s)
  s=self.cases('Liturgy of Blood',['payment_0'])[0]['steps']
  self.assertIn(dict(op='assert_mana',player=0,color=1,value=3),s)
  s=self.cases('Liturgy of Blood',['countered_0'])[0]['steps']
  self.assertIn(dict(op='assert_mana',player=0,color=1,value=18),s)
  choices=[x['op'] for x in s if x['op'] in ('choose_target','choose_priority')]
  self.assertEqual(choices[:2],['choose_target','choose_priority'])

 def test_namesake_receipts_check_only_one_of_two_identical_targets_moved(self):
  s=self.cases(selected=['two_targets_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='unselected',property='in_play',value=1),s)
  self.assertIn(dict(op='assert_zone',player=1,zone='battlefield',value=1),s)

 def test_regeneration_window_is_consumed_only_when_the_spell_allows_it(self):
  s=self.cases('Vindicate',['regeneration_0'])[0]['steps']
  self.assertIn(dict(op='choose_regeneration',ref='recipient'),s)
  self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),s)
  for name in ('Annihilate','Final Reward'):
   s=self.cases(name,['regeneration_0'])[0]['steps']
   self.assertIn(dict(op='choose_regeneration',ref='recipient'),s)
   self.assertIn(dict(op='assert_unused_choices',value=1),s)
   self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=0),s)

 def test_bounced_target_rebinds_and_snapshot_replays_real_pending_stack(self):
  s=self.cases(selected=['bounced_target_0'])[0]['steps']
  self.assertTrue(any(x['op']=='bind_hand_copy' and x['ref']=='recipient' for x in s))
  s=self.cases(selected=['pending_snapshot_1'])[0]['steps']
  for op in ('capture','restore','ai_roundtrip','record_state','assert_recorded_state'):self.assertTrue(any(x['op']==op for x in s))

 def test_full_source_pin_and_witnesses_are_immutable(self):
  d=self.declaration('Rend Flesh');before=copy.deepcopy(d);self.cases('Rend Flesh',d=d);self.assertEqual(d,before)
  o=copy.deepcopy(self.oracle);o['Mothdust Changeling']['keywords']=[]
  with self.assertRaises(ValueError):self.cases(oracle=o)


if __name__=='__main__':unittest.main()
