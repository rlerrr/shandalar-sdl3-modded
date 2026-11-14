import copy,json,unittest
from pathlib import Path
from generate_cases import registry,validate_upkeep_step
from upkeep_hand_value_recipes import FAMILY,_HandWorld,classify,spec,variants,expand,validate_witnesses

NAMES=['Ivory Tower','Bulwark','Dark Suspicions','Dreamborn Muse','Storm World','Scalding Tongs','Ivory Crane Netsuke','Skullcage']
class UpkeepHandValueRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,n):return dict(family=FAMILY,version=2,oracle_text=self.o[n]['oracle_text'],params=classify(self.o[n]),variants=['effect_0'])
 def case(self,n,m='effect',pl=0):
  d=self.declaration(n);return expand(n,self.rows[n]['csvid'],d,spec(d,self.o[n]),[f'{m}_{pl}'],self.rows,self.o)[0]
 def last(self,c):s=c['steps'];return s[max(i for i,x in enumerate(s) if x['op']=='upkeep_step'):]
 def life(self,c,pl,n):self.assertIn(dict(op='assert_life',player=pl,value=n),self.last(c))
 def test_full_oracle_arithmetic_predicates_and_independent_pins(self):
  for n in NAMES:
   d=self.declaration(n);self.assertTrue(spec(d,self.o[n]))
   with self.assertRaises(ValueError):classify(dict(self.o[n],oracle_text=self.o[n]['oracle_text']+'\nDraw a card.'))
  self.assertEqual(classify(dict(self.o['Ivory Tower'],name='Another Tower'))['formula'],[1,0,-4])
  self.assertEqual(classify(self.o['Dark Suspicions'])['formula'],[-1,1,0])
  self.assertEqual(classify(self.o['Skullcage'])['predicate'],dict(kind='outside',basis='active',low=3,high=4))
  for n in ("Wolfcaller's Howl","Venser's Journal",'The Rack','Black Vise','Fevered Visions','Brink of Madness'):
   with self.assertRaises(ValueError):classify(self.o[n])
  with self.assertRaises(ValueError):classify(dict(self.o['Ivory Tower'],oracle_text='At the beginning of your upkeep, you gain 4 life, where X is the number of cards in your hand minus 4.'))
 def test_exact_json_types_whole_source_and_released_exact_follow11(self):
  d=self.declaration('Ivory Tower');r=self.o['Ivory Tower']
  for k,v in [('version',2.0),('version',True),('family','upkeep_permanent_count'),('oracle_text','Gain life.')]:
   with self.assertRaises(ValueError):spec(dict(d,**{k:v}),r)
  for k,v in [('formula',[1.0,0,-4]),('world',0),('keywords',()),('gate',[]),('extra',1)]:
   bad=copy.deepcopy(d);bad['params'][k]=v
   with self.assertRaises(ValueError):spec(bad,r)
  bad=copy.deepcopy(d);bad['params']['predicate']['low']=0.0
  with self.assertRaises(ValueError):spec(bad,r)
  with self.assertRaises(ValueError):spec(d,dict(r,colors=['U']))
  for n in ('Iron Maiden','Rackling','Viseling','Wheel of Torture','Ebony Owl Netsuke','Hellfire Mongrel','Lavaborn Muse',"Misers' Cage","Paupers' Cage",'Shrieking Affliction','Thumbscrews'):
   p=spec(self.declaration(n),self.o[n]);self.assertEqual(p['predicate'],classify(self.o[n])['predicate'])
  with self.assertRaises(ValueError):spec(self.declaration('Ivory Tower'),dict(self.o['Ivory Tower'],oracle_text=self.o['Ivory Tower']['oracle_text']+'\nDraw a card.'))
 def test_witness_complete_cost_prevention_and_binding_pins(self):
  validate_witnesses(self.o,self.rows)
  bad=copy.deepcopy(self.o);bad['Ancestral Recall']['oracle_text']='Draw three cards.'
  with self.assertRaises(ValueError):validate_witnesses(bad,self.rows)
  bad=copy.deepcopy(self.rows);bad['Ivory Mask']['handler']='NULL'
  with self.assertRaises(ValueError):validate_witnesses(self.o,bad)
 def test_all468_real_turn_provenance_and_no_fake_hand_life_actions(self):
  total=0
  for n in NAMES:
   d=self.declaration(n);p=spec(d,self.o[n]);cs=expand(n,self.rows[n]['csvid'],d,p,variants(p),self.rows,self.o);total+=len(cs)
   for c in cs:
    self.assertFalse(any(x.get('state') for x in c['setup']));ready=saved=None
    for s in c['steps']:
     self.assertNotIn(s['op'],('set_life','set_counter','set_hand_count','draw'))
     if s['op']=='advance_turn':ready=None
     elif s['op']=='untap_step':ready=s['player']
     elif s['op']=='capture':saved=ready
     elif s['op']=='restore':ready=saved
     elif s['op']=='upkeep_step':validate_upkeep_step(s);self.assertEqual(ready,s['player']);ready=None
     elif s['op'] in ('turn','cast','activate','cleanup'):ready=None
     if s['op']=='assert_life':self.assertGreater(s['value'],0)
  self.assertEqual(total,468)
 def test_linear_count_differences_clamp_negative_and_controller_scopes(self):
  for pl in (0,1):
   for mode,value in [('hand_zero',20),('hand_three',20),('hand_four',20),('hand_five',21),('hand_seven',23)]:self.life(self.case('Ivory Tower',mode,pl),pl,value)
   for mode,value in [('own_more',17),('equal_hands',20),('opponent_more',20)]:self.life(self.case('Bulwark',mode,pl),1-pl,value)
   for mode,value in [('own_more',20),('equal_hands',20),('opponent_more',17)]:self.life(self.case('Dark Suspicions',mode,pl),1-pl,value)
   self.life(self.case('Ivory Tower','opposite_visit',pl),pl,20)
   self.life(self.case('Dark Suspicions','opposite_visit',pl),1-pl,17)
 def test_threshold_edges_and_unless_is_not_intervening_if(self):
  for pl in (0,1):
   for m,life in [('hand_zero',18),('hand_three',20),('hand_four',20),('hand_five',18),('hand_seven',18)]:self.life(self.case('Skullcage',m,pl),1-pl,life)
   self.life(self.case('Ivory Crane Netsuke','hand_five',pl),pl,20);self.life(self.case('Ivory Crane Netsuke','hand_seven',pl),pl,24)
   for m,life in [('hand_three',19),('hand_four',20)]:
    c=self.case('Scalding Tongs',m,pl);self.life(c,1-pl,life)
    self.assertEqual(sum(x['op']=='choose_player' for x in c['steps']),int(m=='hand_three'))
 def test_mill_exact_short_empty_and_both_players_prefix(self):
  for pl in (0,1):
   for m,remaining,milled in [('effect',22,2),('short_library',0,1),('empty_library',0,0),('exact_library',0,2)]:
    c=self.case('Dreamborn Muse',m,pl)
    for target in (0,1):
     self.assertIn(dict(op='assert_zone',player=target,zone='library',value=remaining),self.last(c));self.life(c,target,20)
     if milled:self.assertIn(dict(op='assert_zone_multiplicity',player=target,zone='graveyard',index=self.rows['Forest']['csvid'],value=1),self.last(c))
 def test_armor_is_actually_paid_and_cannot_prevent_life_loss(self):
  for pl in (0,1):
   c=self.case('Dark Suspicions','armor',pl);self.life(c,1-pl,17);self.assertIn(dict(op='cast',ref='armor',value=1),c['steps'])
   self.life(self.case('Bulwark','armor',pl),1-pl,18)
   c=self.case('Storm World','armor',pl);self.life(c,pl,19);self.life(c,1-pl,18)
 def test_real_world_replacement_both_controllers_and_source_refs(self):
  for pl in (0,1):
   for m in ('world_own','world_enemy'):
    c=self.case('Storm World',m,pl);s=c['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s);i=s.index(dict(op='cast',ref='copy',value=1))
    self.assertFalse(any(x['op']=='destroy' for x in s));self.assertFalse(any(x['op']=='assert_card' and x['ref']=='source' for x in s[i+3:]))
    self.assertIn(dict(op='assert_zone_multiplicity',player=pl,zone='graveyard',index=self.rows['Storm World']['csvid'],value=1),self.last(c));self.life(c,pl,18)
 def test_real_offturn_draw_crosses_above_seven_without_fake_maximum(self):
  for pl in (0,1):
   c=self.case('Ivory Crane Netsuke','draw_own',pl);s=c['steps'];i=s.index(dict(op='cast',ref='draw_spell',value=1));before=s[:i]
   self.assertEqual(next(x for x in reversed(before) if x['op']=='turn'),dict(op='turn',player=1-pl,value=0));self.life(c,pl,24)
   self.assertIn(dict(op='assert_zone_multiplicity',player=pl,zone='hand',index=self.rows['Ancestral Recall']['csvid'],value=0),self.last(c));self.assertIn(dict(op='assert_zone',player=pl,zone='hand',value=8),self.last(c))
   c=self.case('Dark Suspicions','draw_opponent',pl);self.life(c,1-pl,12)
   self.assertEqual(sum(s['op']=='advance_turn' for s in c['steps']),1)
 def test_real_discard_changes_hand_difference_and_exact_destinations(self):
  for pl in (0,1):
   c=self.case('Bulwark','discard_before',pl);self.assertIn(dict(op='cast',ref='discard_spell',value=1),c['steps']);self.life(c,1-pl,16)
   self.assertIn(dict(op='assert_zone_multiplicity',player=1-pl,zone='graveyard',index=self.rows['Forest']['csvid'],value=2),self.last(c))
 def test_real_planeswalkers_legal_on_either_side_and_mandatory_fallback(self):
  for pl in (0,1):
   for m in ('target_own_walker','target_enemy_walker','mandatory_cancel'):
    c=self.case('Scalding Tongs',m,pl);s=c['steps'];self.assertIn(dict(op='cast',ref='walker',value=1),s);self.assertIn(dict(op='assert_counter',ref='walker',counter_kind='loyalty',value=1),self.last(c))
    self.assertIn(dict(op='choose_optional_target' if m=='mandatory_cancel' else 'choose_target',player=pl,ref='walker'),s)
    if m=='mandatory_cancel':self.assertIn(dict(op='assert_forced_target_choices',value=1),s)
   c=self.case('Bulwark','protected_opponent',pl);self.assertIn(dict(op='cast',ref='mask',value=1),c['steps']);self.life(c,1-pl,20)
 def test_source_history_zero_then_late_entry_and_after_resolution(self):
  for pl in (0,1):
   for m,n in [('source_absent',20),('source_in_hand',20),('source_enters_after',21),('source_destroyed_after',21),('repeat',22)]:self.life(self.case('Ivory Tower',m,pl),pl,n)
   c=self.case('Dreamborn Muse','blocked',pl);self.assertIn(dict(op='block',ref='blocker',other='source',value=1),c['steps'])
 def test_snapshots_phase_ready_and_actual_cancel_mana(self):
  for m in ('ready_snapshot','untap_snapshot','resolved_snapshot'):
   s=self.case('Ivory Tower',m)['steps'];self.assertEqual(sum(x['op']=='capture' for x in s),1);self.assertIn(dict(op='assert_recorded_state'),s)
   if m=='untap_snapshot':i=s.index(dict(op='capture'));self.assertEqual(next(x for x in reversed(s[:i]) if x['op'] in ('turn','untap_step','upkeep_step')),dict(op='untap_step',player=0))
  s=self.case('Bulwark','cast_cancel')['steps'];i=s.index(dict(op='choose_cancel_mana',player=0));self.assertEqual(s[i+1],dict(op='cast',ref='source',value=0));self.assertEqual(s[i+2],dict(op='assert_unused_choices',value=0));self.assertFalse(any(x['op']=='upkeep_step' for x in s))
 def test_follow11_default_positive_controls_keep_oracle_scope_and_prevention(self):
  for pl in (0,1):
   for n,damage in [('Iron Maiden',1),('Rackling',1),('Viseling',1),('Wheel of Torture',1),('Ebony Owl Netsuke',4),('Hellfire Mongrel',2),('Lavaborn Muse',3),("Misers' Cage",2),("Paupers' Cage",2),('Shrieking Affliction',3),('Thumbscrews',1)]:
    c=self.case(n,pl=pl);self.life(c,1-pl,20-damage);self.life(c,pl,20)
    c=self.case(n,'armor',pl);self.life(c,1-pl,20-(damage if n=='Shrieking Affliction' else max(0,damage-1)))
 def test_follow11_hand_boundaries_and_ordinary_signed_zero(self):
  for pl in (0,1):
   for n,modes in [('Iron Maiden',[('hand_three',20),('hand_four',20),('hand_five',19)]),('Rackling',[('hand_zero',17),('hand_three',20),('hand_five',20)]),('Ebony Owl Netsuke',[('hand_five',20),('hand_seven',16)]),('Shrieking Affliction',[('hand_one',17),('hand_three',20)]),('Thumbscrews',[('hand_four',20),('hand_five',19)])]:
    for m,life in modes:self.life(self.case(n,m,pl),1-pl,life)
 def test_follow11_high_hand_plane_targets_and_native_artifact_combat_receipts(self):
  for pl in (0,1):
   for m in ('target_own_walker','target_enemy_walker','mandatory_cancel'):
    c=self.case('Thumbscrews',m,pl);self.assertIn(dict(op='assert_counter',ref='walker',counter_kind='loyalty',value=1),self.last(c));self.assertIn(dict(op='choose_optional_target' if m=='mandatory_cancel' else 'choose_target',player=pl,ref='walker'),c['steps'])
   for n in ('Rackling','Viseling'):
    c=self.case(n,'blocked',pl);self.assertIn(dict(op='assert_matching_permanents',player=pl,index=self.rows[n]['csvid'],property='artifact',value=1),c['steps']);self.assertIn(dict(op='assert_card',ref='source',property='power',value=2),c['steps']);self.assertIn(dict(op='block',ref='blocker',other='source',value=1),c['steps'])
 def test_all633_follow11_phase_schema_no_fake_hand_actions(self):
  total=0
  for n in ('Iron Maiden','Rackling','Viseling','Wheel of Torture','Ebony Owl Netsuke','Hellfire Mongrel','Lavaborn Muse',"Misers' Cage","Paupers' Cage",'Shrieking Affliction','Thumbscrews'):
   d=self.declaration(n);p=spec(d,self.o[n]);cs=expand(n,self.rows[n]['csvid'],d,p,variants(p),self.rows,self.o);total+=len(cs)
   for c in cs:
    ready=saved=None
    for s in c['steps']:
     self.assertNotIn(s['op'],('set_life','set_counter','set_hand_count','draw'))
     if s['op']=='advance_turn':ready=None
     elif s['op']=='untap_step':ready=s['player']
     elif s['op']=='capture':saved=ready
     elif s['op']=='restore':ready=saved
     elif s['op']=='upkeep_step':validate_upkeep_step(s);self.assertEqual(ready,s['player']);ready=None
     elif s['op'] in ('turn','cast','activate','cleanup'):ready=None
  self.assertEqual(total,633)

if __name__=='__main__':unittest.main()
