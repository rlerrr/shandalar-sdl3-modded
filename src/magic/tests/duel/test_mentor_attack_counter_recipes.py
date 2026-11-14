import copy,json,unittest
from pathlib import Path
from mentor_attack_counter_recipes import classify,spec,variants,expand,validate_witnesses
from generate_cases import registry

NAMES=['Barging Sergeant','Blade Instructor','Hammer Dropper','Parhelion Patrol','Sunhome Stalwart','Boros Challenger']

class MentorAttackCounterRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):return dict(family='mentor_attack_counter',version=2,oracle_text=self.o[name]['oracle_text'],params=classify(self.o[name]),variants=['effect_0'])
 def case(self,name,value='effect',owner=0):
  d=self.declaration(name);return expand(name,self.rows[name]['csvid'],d,spec(d,self.o[name]),[f'{value}_{owner}'],self.rows,self.o)[0]
 def card(self,ref,key,value):return dict(op='assert_card',ref=ref,property=key,value=value)
 def after(self,c):
  s=c['steps'];return s[s.index(dict(op='finish_attackers')):]
 def test_whole_oracle_grammar_scales_without_source_names_and_rejects_active_clauses(self):
  for name in NAMES:
   self.assertEqual(classify(self.o[name]),classify(dict(self.o[name],name='Another Mentor')))
   with self.subTest(name=name),self.assertRaises(ValueError):classify(dict(self.o[name],oracle_text=self.o[name]['oracle_text']+'\nDraw a card.'))
  for name in ('Aurelia, Exemplar of Justice','Legion Warboss'):
   with self.assertRaises(ValueError):classify(self.o[name])
  self.assertEqual(classify(self.o['Boros Challenger'])['activation_mana'],[2,0,0,0,1,1])
 def test_exact_types_and_full_pinned_record_fail_closed(self):
  name='Boros Challenger';d=self.declaration(name)
  for key,value in [('version',2.0),('version',True),('family','battalion_trigger'),('oracle_text','Mentor')]:
   with self.assertRaises(ValueError):spec(dict(d,**{key:value}),self.o[name])
  for key,value in [('pump_power',1.0),('pump_toughness',True),('activation_mana',[2.0,0,0,0,1,1]),('keywords',()),('extra',1)]:
   bad=copy.deepcopy(d);bad['params'][key]=value
   with self.assertRaises(ValueError):spec(bad,self.o[name])
  for change in [dict(layout='transform'),dict(card_faces=[{}]),dict(selected_face='front'),dict(type_line='Token Creature — Human')]:
   with self.assertRaises(ValueError):classify(dict(self.o[name],**change))
 def test_witnesses_pin_full_oracle_and_binding_and_review_native_controls(self):
  validate_witnesses(self.o,self.rows)
  for name,change in [('Water Elemental',dict(power='4')),('Giant Growth',dict(oracle_text='Target creature gets +1/+1.'))]:
   with self.assertRaises(ValueError):validate_witnesses(dict(self.o,**{name:dict(self.o[name],**change)}),self.rows)
  bad=copy.deepcopy(self.rows);bad["Mons's Goblin Raiders"]['handler']='NULL'
  with self.assertRaises(ValueError):validate_witnesses(self.o,bad)
 def test_all371_cases_expand_with_finite_nonlethal_receipts_and_no_seeded_effects(self):
  total=0
  for name in NAMES:
   d=self.declaration(name);p=spec(d,self.o[name]);cases=expand(name,self.rows[name]['csvid'],d,p,variants(p),self.rows,self.o);total+=len(cases)
   for c in cases:
    self.assertFalse(any(x.get('state') for x in c['setup']))
    self.assertTrue(all(x['value']>0 for x in c['steps'] if x['op']=='assert_life'))
    self.assertFalse(any(x['op']=='set_counter' for x in c['steps']))
  self.assertEqual(total,371)
 def test_strict_lesser_attacking_target_and_selected_recipient_both_controllers(self):
  for name in NAMES:
   for owner in (0,1):
    for value in ('effect','target_other'):
     c=self.case(name,value,owner);s=c['steps'];finish=s.index(dict(op='finish_attackers'));target='other_lower' if value=='target_other' else 'lower'
     self.assertIn(dict(op='choose_target',player=owner,ref=target),s[:finish]);self.assertIn(self.card(target,'plus1_counters',1),s[finish:]);self.assertIn(self.card('source','plus1_counters',0),s[finish:])
    for value in ('equal_only','greater_only','nonattacking_lower','source_alone','only_others','zero_attackers','source_absent','opponent_attack'):
     c=self.case(name,value,owner);self.assertFalse(any(x['op']=='choose_target' for x in c['steps']));self.assertFalse(any(x['op']=='assert_card' and x.get('property')=='plus1_counters' and x['value'] for x in c['steps']))
 def test_real_giant_growth_changes_comparison_without_fabricated_resolution_priority(self):
  for owner in (0,1):
   for value,target,counter in [('source_pump','equal',1),('target_pump','lower',0)]:
    c=self.case('Blade Instructor',value,owner);s=c['steps'];cast=s.index(dict(op='cast',ref='growth',value=1));begin=s.index(dict(op='begin_attackers'))
    self.assertLess(cast,begin);self.assertIn(dict(op='choose_target',player=owner,ref='source' if value=='source_pump' else 'lower'),s[:cast]);self.assertIn(self.card(target,'plus1_counters',counter),self.after(c))
   s=self.case('Blade Instructor')['steps'];last=max(i for i,x in enumerate(s) if x['op']=='attack');finish=s.index(dict(op='finish_attackers'))
   self.assertFalse(any(x['op'] in ('cast','activate','destroy','turn','choose_priority','resolve') for x in s[last+1:finish]))
 def test_counters_survive_cleanup_turns_and_source_death_but_equality_stops_next_counter(self):
  for owner in (0,1):
   s=self.case('Blade Instructor','cleanup',owner)['steps'];cut=s.index(dict(op='cleanup'));self.assertIn(self.card('lower','plus1_counters',1),s[cut:]);self.assertIn(self.card('lower','power',3),s[cut:])
   s=self.case('Blade Instructor','source_destroyed_after',owner)['steps'];cut=s.index(dict(op='destroy',ref='source'));self.assertIn(self.card('lower','plus1_counters',1),s[cut:]);self.assertFalse(any(x['op']=='assert_card' and x['ref']=='source' for x in s[cut+1:]))
   s=self.case('Blade Instructor','repeat_equal',owner)['steps'];cuts=[i for i,x in enumerate(s) if x['op']=='finish_attackers'];self.assertEqual(len(cuts),2);self.assertIn(self.card('lower','plus1_counters',1),s[cuts[1]:]);self.assertEqual(sum(x['op']=='choose_target' and x['ref']=='lower' for x in s),1)
   s=self.case('Blade Instructor','repeat_source_pump',owner)['steps'];cuts=[i for i,x in enumerate(s) if x['op']=='finish_attackers'];self.assertIn(self.card('lower','plus1_counters',2),s[cuts[1]:])
 def test_fresh_source_haste_is_legal_and_other_sources_age_through_real_turns(self):
  for name in NAMES:
   for owner in (0,1):
    c=self.case(name,'fresh_attack',owner);haste='Haste' in classify(self.o[name])['keywords'];self.assertIn(dict(op='attack',ref='source',value=int(haste)),c['steps']);self.assertIn(self.card('source','sick',int(not haste)),c['steps']);self.assertIn(self.card('lower','plus1_counters',int(haste)),self.after(c))
    s=self.case(name,'effect',owner)['steps'];cast=s.index(dict(op='cast',ref='source',value=1));begin=s.index(dict(op='begin_attackers'));self.assertEqual(sum(x['op']=='advance_turn' for x in s[cast:begin]),2)
    for i,x in enumerate(s):
     if x['op']=='untap_step':self.assertEqual(s[i+1],dict(op='turn',player=x['player'],value=0))
 def test_parhelion_flying_vigilance_and_sunhome_first_strike_have_real_combat_receipts(self):
  for owner in (0,1):
   c=self.case('Parhelion Patrol','blocked',owner);self.assertIn(dict(op='block',ref='enemy',other='source',value=0),c['steps']);self.assertIn(self.card('source','tapped',0),self.after(c));self.assertIn(self.card('source','attacking',1),self.after(c))
   for value,blocker in [('reach_block','reach'),('flying_block','flying')]:self.assertIn(dict(op='block',ref=blocker,other='source',value=1),self.case('Parhelion Patrol',value,owner)['steps'])
   s=self.case('Sunhome Stalwart','blocked',owner)['steps'];damage=s.index(dict(op='combat_damage'));self.assertIn(self.card('source','in_play',1),s[damage:]);self.assertIn(self.card('source','damage',0),s[damage:]);self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=1),s[damage:])
 def test_mandatory_cancellation_uses_legal_fallback(self):
  for owner in (0,1):
   s=self.case('Blade Instructor','mandatory_cancel',owner)['steps'];cut=s.index(dict(op='finish_attackers'));self.assertEqual(s[cut-1],dict(op='choose_optional_target',player=owner,ref='lower'));self.assertIn(dict(op='assert_forced_target_choices',value=1),s[cut:]);self.assertIn(self.card('lower','plus1_counters',1),s[cut:])
 def test_challenger_activation_is_real_paid_pending_then_resolved_and_expires_separately(self):
  for owner in (0,1):
   s=self.case('Boros Challenger','activation_cleanup',owner)['steps'];activate=s.index(dict(op='activate',ref='source',value=1));resolve=next(i for i in range(activate+1,len(s)) if s[i]['op']=='resolve');begin=s.index(dict(op='begin_attackers'));cleanup=s.index(dict(op='cleanup'))
   self.assertLess(resolve,begin);self.assertIn(dict(op='assert_stack',value=1),s[activate:resolve]);self.assertIn(self.card('source','power',2),s[activate:resolve]);self.assertIn(self.card('source','power',3),s[resolve:begin]);self.assertIn(self.card('equal','plus1_counters',1),s[begin:cleanup]);self.assertIn(self.card('source','power',2),s[cleanup:]);self.assertIn(self.card('equal','power',3),s[cleanup:])
   for value in ('activation_insufficient','activation_wrong_color'):
    s=self.case('Boros Challenger',value,owner)['steps'];self.assertIn(dict(op='activate',ref='source',value=0),s);self.assertFalse(any(x['op']=='begin_attackers' for x in s))
 def test_human_cancel_uses_actual_printed_cost_producers_and_consumes_intent(self):
  for name,value in [('Blade Instructor','cast_cancel'),('Boros Challenger','activation_cancel')]:
   c=self.case(name,value);s=c['steps'];lands=[x for x in c['setup'] if x['ref'].startswith('payment_land_')];self.assertEqual(len(lands),3 if value=='cast_cancel' else 4);cancel=s.index(dict(op='choose_cancel_mana',player=0));self.assertEqual(s[cancel+1],dict(op='cast' if value=='cast_cancel' else 'activate',ref='source',value=0));self.assertEqual(s[cancel+2],dict(op='assert_unused_choices',value=0))
   for land in lands:self.assertIn(self.card(land['ref'],'tapped',0),s[cancel:])
 def test_replay_and_empty_zones_keep_real_actions_and_decisive_receipts(self):
  for value in ('ready_snapshot','declaration_snapshot','resolved_snapshot'):
   s=self.case('Blade Instructor',value)['steps'];self.assertEqual(sum(x['op']=='capture' for x in s),1);self.assertIn(dict(op='ai_roundtrip'),s);self.assertIn(dict(op='assert_recorded_state'),s)
  s=self.case('Boros Challenger','activation_snapshot')['steps'];self.assertEqual(sum(x['op']=='activate' for x in s),2)
  c=self.case('Blade Instructor','bare_zones');self.assertFalse(any(x['zone'] in ('library','graveyard','exile') for x in c['setup']));self.assertIn(self.card('lower','plus1_counters',1),self.after(c))

if __name__=='__main__':unittest.main()
