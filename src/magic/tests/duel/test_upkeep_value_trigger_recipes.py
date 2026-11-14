import copy,json,unittest
from pathlib import Path
from upkeep_value_trigger_recipes import classify,spec,variants,expand,validate_witnesses
from generate_cases import registry,validate_upkeep_step

NAMES=['Phyrexian Arena','Baleful Force','Seizan, Perverter of Truth','Bloodgift Demon',"Drana's Emissary","Ajani's Mantra",'Copper Tablet','Moroii']
class UpkeepValueTriggerRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):return dict(family='upkeep_value_trigger',version=2,oracle_text=self.o[name]['oracle_text'],params=classify(self.o[name]),variants=['effect_0'])
 def case(self,name,value='effect',owner=0):
  d=self.declaration(name);return expand(name,self.rows[name]['csvid'],d,spec(d,self.o[name]),[f'{value}_{owner}'],self.rows,self.o)[0]
 def last(self,c):
  s=c['steps'];return s[max(i for i,x in enumerate(s) if x['op']=='upkeep_step'):]
 def card(self,ref,key,value):return dict(op='assert_card',ref=ref,property=key,value=value)
 def test_full_oracle_actions_order_recipients_and_no_name_enum(self):
  self.assertEqual(classify(self.o['Phyrexian Arena'])['actions'],[dict(kind='draw',recipient='controller',amount=1),dict(kind='loss',recipient='controller',amount=1)])
  self.assertEqual(classify(self.o['Seizan, Perverter of Truth'])['actions'],[dict(kind='loss',recipient='active',amount=2),dict(kind='draw',recipient='active',amount=2)])
  self.assertEqual(classify(dict(self.o['Phyrexian Arena'],name='Another Arena')),classify(self.o['Phyrexian Arena']))
  for name in NAMES:
   with self.assertRaises(ValueError):classify(dict(self.o[name],oracle_text=self.o[name]['oracle_text']+'\nDraw a card.'))
  for name in ('Bitterblossom','Honden of Cleansing Fire','Avaricious Dragon'):
   with self.assertRaises(ValueError):classify(self.o[name])
 def test_exact_json_types_and_source_pins_fail_closed(self):
  name='Phyrexian Arena';d=self.declaration(name)
  for key,value in [('version',2.0),('version',True),('family','attack_team_reward'),('oracle_text','At your upkeep draw.')]:
   with self.assertRaises(ValueError):spec(dict(d,**{key:value}),self.o[name])
  for key,value in [('optional',0),('gate',[]),('keywords',()),('actions',()),('extra',1)]:
   bad=copy.deepcopy(d);bad['params'][key]=value
   with self.assertRaises(ValueError):spec(bad,self.o[name])
  bad=copy.deepcopy(d);bad['params']['actions'][0]['amount']=1.0
  with self.assertRaises(ValueError):spec(bad,self.o[name])
  with self.assertRaises(ValueError):spec(d,dict(self.o[name],colors=['W']))
  for change in [dict(layout='transform'),dict(card_faces=[{}]),dict(selected_face='front'),dict(type_line='Token Creature — Spirit')]:
   with self.assertRaises(ValueError):classify(dict(self.o[name],**change))
 def test_exact_released_follow7_have_full_pins_and_calibrated_scalar_parameters(self):
  for name in ('Serendib Efreet','Fledgling Djinn','Nettletooth Djinn','Rumbling Slum','Celestial Force','Master of the Feast','Nyx-Fleece Ram'):
   d=self.declaration(name);p=spec(d,self.o[name]);self.assertFalse(p['optional']);self.assertEqual(p['ui']['trigger_accept_players'],[]);self.assertEqual(p['ui']['target_choice_players'],[])
   self.assertTrue(all(a['kind'] in ('gain','damage','draw') and a['recipient'] in ('controller','each','opponent') for a in p['actions']))
  self.assertEqual(classify(self.o['Rumbling Slum'])['actions'],[dict(kind='damage',recipient='each',amount=1)])
  for name in ('Honden of Cleansing Fire','Honden of Seeing Winds'):
   with self.assertRaises(ValueError):classify(self.o[name])
 def test_full_witness_prevention_truth_and_bindings(self):
  validate_witnesses(self.o,self.rows)
  with self.assertRaises(ValueError):validate_witnesses(dict(self.o,**{"Urza's Armor":dict(self.o["Urza's Armor"],oracle_text='Prevent 2 damage.')}),self.rows)
  bad=copy.deepcopy(self.rows);bad['Grizzly Bears']['handler']='NULL'
  with self.assertRaises(ValueError):validate_witnesses(self.o,bad)
 def test_all289_are_nonlethal_real_cast_and_exact_upkeep_schema(self):
  total=0
  for name in NAMES:
   d=self.declaration(name);p=spec(d,self.o[name]);cs=expand(name,self.rows[name]['csvid'],d,p,variants(p),self.rows,self.o);total+=len(cs)
   for c in cs:
    self.assertFalse(any(x.get('state') for x in c['setup']));self.assertFalse(any(x['op'] in ('set_life','set_counter','set_power') for x in c['steps']));self.assertTrue(all(x['value']>0 for x in c['steps'] if x['op']=='assert_life'))
    ready=saved=None
    for s in c['steps']:
     if s['op']=='advance_turn':ready=None
     elif s['op']=='untap_step':ready=s['player']
     elif s['op']=='capture':saved=ready
     elif s['op']=='restore':ready=saved
     elif s['op']=='upkeep_step':self.assertEqual(set(s),{'op','player'});self.assertEqual(ready,s['player']);validate_upkeep_step(s);ready=None
     elif s['op'] in ('turn','cast','activate','cleanup'):ready=None
  self.assertEqual(total,289)
  for bad in [dict(op='upkeep_step',player=0,value=1),dict(op='upkeep_step',player=True),dict(op='upkeep_step',player=2)]:
   with self.assertRaises(ValueError):validate_upkeep_step(bad)
 def test_own_each_controller_and_active_recipients_both_owners(self):
  for owner in (0,1):
   s=self.last(self.case('Phyrexian Arena',owner=owner));self.assertIn(dict(op='assert_life',player=owner,value=19),s);self.assertIn(dict(op='assert_life',player=1-owner,value=20),s);self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=11),s)
   s=self.last(self.case('Baleful Force','opponent_upkeep',owner));self.assertIn(dict(op='upkeep_step',player=1-owner),s);self.assertIn(dict(op='assert_life',player=owner,value=19),s);self.assertIn(dict(op='assert_life',player=1-owner,value=20),s);self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=11),s)
   s=self.last(self.case('Seizan, Perverter of Truth',owner=owner))
   for pl in (0,1):self.assertIn(dict(op='assert_life',player=pl,value=18),s);self.assertIn(dict(op='assert_zone',player=pl,zone='library',value=10),s)
 def test_real_turn_visits_repeat_once_and_never_run_draw_step(self):
  for owner in (0,1):
   c=self.case('Baleful Force','repeat',owner);s=c['steps'];self.assertEqual([x['player'] for x in s if x['op']=='upkeep_step'],[1-owner,owner,1-owner,owner]);self.assertEqual(sum(x['op']=='advance_turn' for x in s),4);self.assertIn(dict(op='assert_life',player=owner,value=16),self.last(c));self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=8),self.last(c));self.assertFalse(any(x['op']=='draw' for x in s))
   c=self.case('Phyrexian Arena','opponent_upkeep',owner);self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=12),self.last(c));self.assertIn(dict(op='assert_life',player=owner,value=20),self.last(c))
 def test_actual_armor_distinguishes_damage_from_oracle_lifeloss(self):
  for owner in (0,1):
   for name,life in [('Copper Tablet',20),('Moroii',19),('Phyrexian Arena',19)]:
    c=self.case(name,'armor',owner);s=c['steps'];self.assertLess(s.index(dict(op='cast',ref='armor',value=1)),s.index(dict(op='advance_turn')));self.assertIn(dict(op='assert_life',player=owner,value=life),self.last(c))
   self.assertIn(dict(op='assert_life',player=1-owner,value=19),self.last(self.case('Copper Tablet','armor',owner)))
 def test_player_target_inputs_optional_decline_and_ai_policy_annotations(self):
  c=self.case('Bloodgift Demon','target_opponent',0);s=c['steps'];self.assertIn(dict(op='choose_player',player=0,value=1),s);self.assertIn(dict(op='assert_life',player=1,value=19),self.last(c));self.assertIn(dict(op='assert_zone',player=1,zone='library',value=11),self.last(c))
  c=self.case('Bloodgift Demon','target_opponent',1);self.assertFalse(any(x['op']=='choose_player' for x in c['steps']));self.assertIn(dict(op='assert_zone',player=0,zone='library',value=0),self.last(c));self.assertEqual(sum(x['op']=='upkeep_step' for x in c['steps']),2)
  c=self.case("Ajani's Mantra",'decline');self.assertFalse(any(x['op']=='choose_trigger_source' for x in c['steps']));self.assertIn(dict(op='assert_life',player=0,value=20),self.last(c))
  c=self.case("Ajani's Mantra");self.assertIn(dict(op='choose_trigger_source',player=0,ref='source'),c['steps']);self.assertIn(dict(op='assert_life',player=0,value=21),self.last(c))
 def test_source_absence_after_entry_and_after_destruction_keep_exact_zones(self):
  for owner in (0,1):
   c=self.case('Phyrexian Arena','source_absent',owner);self.assertIn(dict(op='assert_life',player=owner,value=20),self.last(c));self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Phyrexian Arena']['csvid'],value=1),self.last(c))
   c=self.case('Phyrexian Arena','source_enters_after',owner);s=c['steps'];self.assertGreater(s.index(dict(op='cast',ref='source',value=1)),max(i for i,x in enumerate(s) if x['op']=='upkeep_step'));self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=12),s[s.index(dict(op='cast',ref='source',value=1)):])
   c=self.case('Phyrexian Arena','source_destroyed_after',owner);s=c['steps'];destroy=s.index(dict(op='destroy',ref='source'));self.assertIn(dict(op='assert_life',player=owner,value=19),self.last(c));self.assertFalse(any(x['op']=='assert_card' and x['ref']=='source' for x in s[destroy+1:]))
 def test_native_flying_has_real_ground_reach_and_defending_receipts(self):
  for owner in (0,1):
   for value,blocker in [('flying_ground','ground'),('flying_reach','reach'),('flying_blocker','flying')]:self.assertIn(dict(op='block',ref=blocker,other='source',value=int(value!='flying_ground')),self.case('Bloodgift Demon',value,owner)['steps'])
   c=self.case('Bloodgift Demon','defend_flying',owner);self.assertEqual(sum(x['op']=='upkeep_step' for x in c['steps']),1);self.assertIn(dict(op='block',ref='source',other='flying',value=1),c['steps'])
 def test_snapshots_include_real_untap_readiness_and_paid_cancel_is_consumed(self):
  for value in ('ready_snapshot','untap_snapshot','resolved_snapshot'):
   s=self.case('Phyrexian Arena',value)['steps'];self.assertEqual(sum(x['op']=='capture' for x in s),1);self.assertIn(dict(op='ai_roundtrip'),s);self.assertIn(dict(op='assert_recorded_state'),s)
  s=self.case('Phyrexian Arena','untap_snapshot')['steps'];capture=s.index(dict(op='capture'));self.assertEqual(next(x for x in reversed(s[:capture]) if x['op'] in ('untap_step','upkeep_step','turn')),dict(op='untap_step',player=0))
  c=self.case('Baleful Force','cast_cancel');s=c['steps'];self.assertEqual(len([x for x in c['setup'] if x['ref'].startswith('payment_land_')]),8);cancel=s.index(dict(op='choose_cancel_mana',player=0));self.assertEqual(s[cancel+1],dict(op='cast',ref='source',value=0));self.assertEqual(s[cancel+2],dict(op='assert_unused_choices',value=0));self.assertFalse(any(x['op']=='upkeep_step' for x in s))

if __name__=='__main__':unittest.main()
