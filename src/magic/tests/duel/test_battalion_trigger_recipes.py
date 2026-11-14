import copy,json
from pathlib import Path
import unittest
from battalion_trigger_recipes import classify,spec,variants,validate_witnesses,expand
from generate_cases import registry


class BattalionTriggerRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):return dict(family='battalion_trigger',version=2,oracle_text=self.oracle[name]['oracle_text'],params=classify(self.oracle[name]),variants=['effect_0'])
 def cases(self,name,selected=None):
  d=self.declaration(name);p=spec(d,self.oracle[name]);vs=selected or d['variants']
  return expand(name,self.rows[name]['csvid'],d,p,vs,self.rows,self.oracle)
 def card(self,ref,key,value):return dict(op='assert_card',ref=ref,property=key,value=value)

 def test_complete_oracle_grammar_has_ten_matches_and_preserves_other_three_for_real_extensions(self):
  names=['Bomber Corps','Boros Elite','Boros Mastiff','Daring Skyjek','Firefist Striker','Firemane Avenger','Nav Squad Commandos','Ordruun Veteran','Warmind Infantry','Wojek Halberdiers']
  for name in names:
   self.assertEqual(classify(self.oracle[name])['threshold'],3)
   with self.subTest(name=name),self.assertRaises(ValueError):classify(dict(self.oracle[name],oracle_text=self.oracle[name]['oracle_text']+'\nDraw a card.'))
  for name in ('Frontline Medic','Legion Loyalist','Tajic, Blade of the Legion'):
   with self.subTest(name=name),self.assertRaises(ValueError):classify(self.oracle[name])
  clone=dict(self.oracle['Boros Elite'],name='Unlisted Soldier')
  self.assertEqual(classify(clone),classify(self.oracle['Boros Elite']))

 def test_exact_json_types_full_record_and_cost_truth_fail_closed(self):
  name='Boros Elite';d=self.declaration(name)
  for key,value in (('version',2.0),('version',True),('family','attack_value_trigger')):
   with self.subTest(key=key),self.assertRaises(ValueError):spec(dict(d,**{key:value}),self.oracle[name])
  for key,value in (('threshold',3.0),('power',True),('untap',0),('keyword',[]),('keywords',()),('unmodeled',1)):
   bad=copy.deepcopy(d);bad['params'][key]=value
   with self.subTest(key=key),self.assertRaises(ValueError):spec(bad,self.oracle[name])
  for fields in (dict(layout='transform'),dict(card_faces=[{}]),dict(selected_face='front'),dict(type_line='Token Creature — Soldier')):
   with self.subTest(fields=fields),self.assertRaises(ValueError):classify(dict(self.oracle[name],**fields))
  self.assertEqual(spec(d,self.oracle[name])['mana'],[0,0,0,0,0,1])

 def test_witnesses_pin_complete_oracle_and_bindings_and_review_native_dummy_explicitly(self):
  validate_witnesses(self.oracle,self.rows)
  for name,change in (('Hill Giant',dict(power='4')),('Servo',dict(colors=['W'])),('Safe Passage',dict(oracle_text='Prevent damage.'))):
   o=dict(self.oracle,**{name:dict(self.oracle[name],**change)})
   with self.subTest(name=name),self.assertRaises(ValueError):validate_witnesses(o,self.rows)
  rows=copy.deepcopy(self.rows);rows['Grizzly Bears']['handler']='NULL'
  with self.assertRaises(ValueError):validate_witnesses(self.oracle,rows)
  self.assertNotIn('Raging Bull',[c['name'] for c in self.cases('Boros Elite')[0]['setup']])

 def test_three_attackers_must_include_source_not_three_other_creatures(self):
  for owner in (0,1):
   for value,power in (('effect',3),('four_attackers',3),('one_attacker',1),('two_attackers',1),('three_without_source',1),('opponent_attack',1)):
    steps=self.cases('Boros Elite',[f'{value}_{owner}'])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
    self.assertIn(self.card('source','power',power),steps[finish:])
   case=self.cases('Boros Elite',[f'three_without_source_{owner}'])[0]
   self.assertEqual([s['ref'] for s in case['steps'] if s['op']=='attack'],['ally','crew','reserve'])

 def test_no_gameplay_priority_or_fabricated_removal_between_selection_and_inline_finish(self):
  for name in ('Boros Elite','Firemane Avenger','Firefist Striker'):
   steps=self.cases(name)[0]['steps'];last=max(i for i,s in enumerate(steps) if s['op']=='attack');finish=steps.index(dict(op='finish_attackers'))
   self.assertFalse(any(s['op'] in ('cast','cast_response','announce_spell','resolve','destroy','turn','advance_turn','choose_priority','choose_trigger_source') for s in steps[last+1:finish]))
   steps=self.cases(name,['source_destroyed_after_0'])[0]['steps']
   self.assertLess(steps.index(dict(op='finish_attackers')),steps.index(dict(op='destroy',ref='source')))
   after=steps.index(dict(op='destroy',ref='source'))
   self.assertFalse(any(s['op']=='assert_card' and s['ref']=='source' for s in steps[after:]))

 def test_primary_cast_aging_and_sickness_use_actual_cost_and_turn_gates(self):
  for owner in (0,1):
   case=self.cases('Nav Squad Commandos',[f'effect_{owner}'])[0];steps=case['steps'];attack=steps.index(dict(op='attack',ref='source',value=1))
   self.assertEqual(next(c['zone'] for c in case['setup'] if c['ref']=='source'),'hand');self.assertFalse(any(c.get('state') for c in case['setup']))
   self.assertIn(dict(op='cast',ref='source',value=1),steps[:attack]);self.assertEqual(sum(s['op']=='advance_turn' for s in steps[:attack]),2)
   for i,s in enumerate(steps):
    if s['op']=='untap_step':self.assertEqual(steps[i-1]['op'],'advance_turn');self.assertEqual(steps[i+1],dict(op='turn',player=s['player'],value=0))
   fresh=self.cases('Boros Elite',[f'fresh_attack_{owner}'])[0]['steps'];self.assertIn(dict(op='attack',ref='source',value=0),fresh)
   for value in ('insufficient_mana','wrong_color'):
    denied=self.cases('Nav Squad Commandos',[f'{value}_{owner}'])[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=0),denied)

 def test_untap_reward_preserves_attack_and_does_not_give_allies_vigilance(self):
  steps=self.cases('Nav Squad Commandos')[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
  for ref,key,value in (('source','power',4),('source','toughness',6),('source','tapped',0),('source','attacking',1),('ally','tapped',1),('crew','tapped',1)):
   self.assertIn(self.card(ref,key,value),steps[finish:])
  steps=self.cases('Nav Squad Commandos',['two_attackers_0'])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
  self.assertIn(self.card('source','power',3),steps[finish:]);self.assertIn(self.card('source','tapped',1),steps[finish:])

 def test_lifelink_counts_only_damage_dealt_by_source_and_prevention_stops_gain(self):
  steps=self.cases('Boros Mastiff',['unblocked_0'])[0]['steps'];damage=steps.index(dict(op='combat_damage'))
  self.assertIn(dict(op='assert_life',player=0,value=22),steps[damage:]);self.assertIn(dict(op='assert_life',player=1,value=13),steps[damage:])
  steps=self.cases('Boros Mastiff',['prevented_combat_0'])[0]['steps'];damage=steps.index(dict(op='combat_damage'))
  self.assertIn(dict(op='cast',ref='shield',value=1),steps)
  for player in (0,1):self.assertIn(dict(op='assert_life',player=player,value=20),steps[damage:])

 def test_temporary_flying_has_real_ground_denial_reach_acceptance_and_expiry(self):
  for value,ref,legal in (('blocked','giant',0),('reach_block','reach',1),('flying_block','flying',1)):
   steps=self.cases('Daring Skyjek',[value+'_0'])[0]['steps'];self.assertIn(dict(op='block',ref=ref,other='source',value=legal),steps)
  steps=self.cases('Daring Skyjek',['expiry_0'])[0]['steps'];self.assertIn(dict(op='block',ref='enemy',other='source',value=1),steps)

 def test_first_and_double_strike_use_real_combat_and_regular_expiry(self):
  for name in ('Wojek Halberdiers','Ordruun Veteran'):
   steps=self.cases(name,['blocked_0'])[0]['steps'];damage=steps.index(dict(op='combat_damage'))
   self.assertIn(self.card('source','damage',0),steps[damage:]);self.assertIn(self.card('source','in_play',1),steps[damage:])
  steps=self.cases('Ordruun Veteran',['unblocked_0'])[0]['steps'];damage=steps.index(dict(op='combat_damage'))
  self.assertIn(dict(op='assert_life',player=1,value=9),steps[damage:])
  steps=self.cases('Ordruun Veteran',['cleanup_0'])[0]['steps'];cleanup=steps.index(dict(op='cleanup'))
  self.assertIn(self.card('source','power',3),steps[cleanup:])

 def test_damage_and_independent_lifegain_keep_prevention_and_own_player_truth(self):
  steps=self.cases('Firemane Avenger',['prevented_damage_0'])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
  self.assertIn(dict(op='assert_life',player=1,value=20),steps[finish:]);self.assertIn(dict(op='assert_life',player=0,value=23),steps[finish:])
  steps=self.cases('Firemane Avenger',['target_own_player_0'])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
  self.assertIn(dict(op='choose_player',player=0,value=0),steps[:finish]);self.assertIn(dict(op='assert_life',player=0,value=20),steps[finish:])
  steps=self.cases('Firemane Avenger',['target_self_0'])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
  self.assertIn(dict(op='assert_life',player=0,value=23),steps[finish:])

 def test_any_target_uses_legally_cast_planeswalker_and_loyalty_zone_receipts(self):
  for owner in (0,1):
   steps=self.cases('Firemane Avenger',[f'target_planeswalker_{owner}'])[0]['steps'];walker=steps.index(dict(op='cast',ref='walker',value=1));source=steps.index(dict(op='cast',ref='source',value=1))
   self.assertLess(walker,source);self.assertIn(dict(op='advance_turn'),steps[walker:source])
   self.assertIn(dict(op='assert_counter',ref='walker',counter_kind='loyalty',value=2),steps)
   finish=steps.index(dict(op='finish_attackers'))
   self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Tibalt, the Fiend-Blooded']['csvid'],value=1),steps[finish:])
   self.assertFalse(any(s['op']=='assert_card' and s['ref']=='walker' for s in steps[finish:]))

 def test_mandatory_target_cancellation_uses_legal_fallback_without_declining_reward(self):
  for name in ('Firemane Avenger','Firefist Striker'):
   steps=self.cases(name,['mandatory_cancel_0'])[0]['steps'];finish=steps.index(dict(op='finish_attackers'))
   self.assertEqual(steps[finish-1],dict(op='choose_optional_target',player=0,ref='enemy'))
   self.assertIn(dict(op='assert_forced_target_choices',value=1),steps[finish:])
   if name=='Firemane Avenger':self.assertIn(dict(op='assert_life',player=0,value=23),steps[finish:])

 def test_real_servo_witnesses_are_cast_aged_declared_and_replayed_without_fake_tokens(self):
  case=self.cases('Boros Elite',['token_crew_0'])[0];steps=case['steps'];self.assertNotIn('Servo',[c['name'] for c in case['setup']])
  self.assertIn(dict(op='cast',ref='servo_spell',value=1),steps)
  self.assertIn(dict(op='attack',ref='servo_0',value=1),steps);self.assertIn(dict(op='attack',ref='servo_1',value=1),steps)
  finish=steps.index(dict(op='finish_attackers'));self.assertIn(self.card('source','power',3),steps[finish:])
  for value in ('ready_snapshot_0','declaration_snapshot_0','resolved_snapshot_0'):
   steps=self.cases('Boros Elite',[value])[0]['steps'];self.assertIn(dict(op='restore'),steps);self.assertIn(dict(op='assert_recorded_state'),steps)
   self.assertEqual(len(steps),len({id(s) for s in steps}))

 def test_all_eight_pilot_variants_construct_without_invented_initial_states(self):
  total=0
  for name in ('Boros Elite','Boros Mastiff','Daring Skyjek','Firefist Striker','Firemane Avenger','Nav Squad Commandos','Ordruun Veteran','Wojek Halberdiers'):
   d=self.declaration(name);p=spec(d,self.oracle[name]);cases=self.cases(name,variants(p));total+=len(cases)
   for case in cases:
    self.assertFalse(any(c.get('state') for c in case['setup']))
    for i,s in enumerate(case['steps']):
     if s['op']=='untap_step':self.assertEqual(case['steps'][i-1]['op'],'advance_turn');self.assertEqual(case['steps'][i+1],dict(op='turn',player=s['player'],value=0))
  self.assertGreater(total,390)

 def test_cancel_checks_real_funding_and_consumed_attempt_not_raw_creature_handler_query(self):
  for name in ('Boros Elite','Boros Mastiff','Daring Skyjek','Firefist Striker','Firemane Avenger','Nav Squad Commandos','Ordruun Veteran','Wojek Halberdiers'):
   case=self.cases(name,['cast_cancel_0'])[0];steps=case['steps'];p=spec(self.declaration(name),self.oracle[name])
   lands=[c for c in case['setup'] if c['ref'].startswith('cast_land_')]
   self.assertEqual(len(lands),sum(p['mana']))
   expected=[('Forest','Swamp','Island','Forest','Mountain','Plains')[color] for color,n in enumerate(p['mana']) for _ in range(n)]
   self.assertEqual([c['name'] for c in lands],expected)
   cast=steps.index(dict(op='cast',ref='source',value=0));self.assertNotIn('require_can_cast',[s['op'] for s in steps])
   self.assertEqual(steps[cast-1],dict(op='choose_cancel_mana',player=0));self.assertEqual(steps[cast+1],dict(op='assert_unused_choices',value=0))
   for land in lands:
    self.assertIn(dict(op='require_can_activate',ref=land['ref'],value=1),steps[:cast])
    self.assertIn(self.card(land['ref'],'tapped',0),steps[cast:])
   self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows[name]['csvid'],value=1),steps[cast:])
   self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows[name]['csvid'],value=0),steps[cast:])


if __name__=='__main__':unittest.main()
