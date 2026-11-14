import copy,json,unittest
from pathlib import Path
from generate_cases import registry,validate_upkeep_step
from upkeep_controlled_return_recipes import FAMILY,_ReturnWorld,classify,spec,variants,expand,validate_witnesses

NAMES=['Sunken Hope','Cache Raiders','Esperzoa','Eiganjo Free-Riders','Oni of Wild Places','Stampeding Serow','Riptide Chimera','Blood Clock']
class UpkeepControlledReturnRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,n):return dict(family=FAMILY,version=2,oracle_text=self.o[n]['oracle_text'],params=classify(self.o[n]),variants=['effect_0'])
 def case(self,n,m='effect',pl=0):
  d=self.declaration(n);return expand(n,self.rows[n]['csvid'],d,spec(d,self.o[n]),[f'{m}_{pl}'],self.rows,self.o)[0]
 def last(self,c):s=c['steps'];return s[max(i for i,x in enumerate(s) if x['op']=='upkeep_step'):]
 def zone(self,c,pl,zone,nm,value):self.assertIn(dict(op='assert_zone_multiplicity',player=pl,zone=zone,index=self.rows[nm]['csvid'],value=value),self.last(c))
 def life(self,c,pl,value):self.assertIn(dict(op='assert_life',player=pl,value=value),self.last(c))
 def test_whole_oracle_selectors_scope_and_complete_native_passives(self):
  for n in NAMES:
   p=spec(self.declaration(n),self.o[n]);self.assertIn(p['kind'],('permanent','creature','artifact','enchantment'))
   with self.assertRaises(ValueError):classify(dict(self.o[n],oracle_text=self.o[n]['oracle_text']+'\nDraw a card.'))
  self.assertEqual(classify(self.o['Oni of Wild Places'])['keywords'],['Haste']);self.assertEqual(classify(self.o['Stampeding Serow'])['keywords'],['Trample'])
  self.assertEqual(classify(dict(self.o['Cache Raiders'],name='Another Raider'))['kind'],'permanent')
  for n in ('Imaginary Pet','Complex Automaton','Noetic Scales','Skull Collector','Trusted Advisor','Living Tsunami','Waterspout Djinn'):
   with self.assertRaises(ValueError):classify(self.o[n])
 def test_exact_json_types_source_pins_and_released_followers(self):
  d=self.declaration('Sunken Hope');r=self.o['Sunken Hope']
  for k,v in [('version',2.0),('version',True),('family','upkeep_hand_value'),('oracle_text','Return a creature.')]:
   with self.assertRaises(ValueError):spec(dict(d,**{k:v}),r)
  for k,v in [('life_payment',0.0),('life_payment',False),('kind',[]),('keywords',()),('extra',1)]:
   bad=copy.deepcopy(d);bad['params'][k]=v
   with self.assertRaises(ValueError):spec(bad,r)
  with self.assertRaises(ValueError):spec(d,dict(r,colors=['G']))
  for n in ('Roaring Primadox','Species Gorger','Stampeding Wildebeests','Umbilicus'):
   p=spec(self.declaration(n),self.o[n]);self.assertEqual(p['kind'],classify(self.o[n])['kind'])
 def test_native_witness_honesty_complete_pins_and_no_callback_substitution(self):
  validate_witnesses(self.o,self.rows)
  self.assertEqual(self.rows['Deadly Insect']['handler'],'card_dummy')
  bad=copy.deepcopy(self.o);bad['Deadly Insect']['oracle_text']=''
  with self.assertRaises(ValueError):validate_witnesses(bad,self.rows)
  bad=copy.deepcopy(self.rows);bad['Seal of Removal']['handler']='NULL'
  with self.assertRaises(ValueError):validate_witnesses(self.o,bad)
 def test_all440_real_upkeep_provenance_and_only_ownership_fixture_flag(self):
  total=0
  for n in NAMES:
   d=self.declaration(n);p=spec(d,self.o[n]);cs=expand(n,self.rows[n]['csvid'],d,p,variants(p),self.rows,self.o);total+=len(cs)
   for c in cs:
    ready=saved=None
    for x in c['setup']:
     if x.get('state'):self.assertEqual(x['state'],['opponent_owned']);self.assertIn('_owner_hand_',c['name'])
    for s in c['steps']:
     self.assertNotIn(s['op'],('set_life','set_counter','set_hand_count','draw'))
     if s['op']=='advance_turn':ready=None
     elif s['op']=='untap_step':ready=s['player']
     elif s['op']=='capture':saved=ready
     elif s['op']=='restore':ready=saved
     elif s['op']=='upkeep_step':validate_upkeep_step(s);self.assertEqual(ready,s['player']);ready=None
     elif s['op'] in ('turn','cast','activate','cleanup'):ready=None
     if s['op']=='assert_life':self.assertGreater(s['value'],2)
  self.assertEqual(total,440)
 def test_each_and_own_scope_with_exact_owner_hand_destinations(self):
  for pl in (0,1):
   c=self.case('Sunken Hope',pl=pl)
   for who in (0,1):self.zone(c,who,'hand','Grizzly Bears',1);self.zone(c,who,'battlefield','Grizzly Bears',0)
   c=self.case('Cache Raiders',pl=pl);self.zone(c,pl,'hand','Grizzly Bears',1);self.zone(c,1-pl,'battlefield','Grizzly Bears',1)
   c=self.case('Cache Raiders','owner_hand',pl);self.zone(c,pl,'hand','Grizzly Bears',0);self.zone(c,1-pl,'hand','Grizzly Bears',1)
   c=self.case('Sunken Hope','owner_hand',pl);self.zone(c,pl,'hand','Grizzly Bears',0);self.zone(c,1-pl,'hand','Grizzly Bears',2)
   self.assertEqual(sum(x.get('state')==['opponent_owned'] for x in c['setup']),1)
 def test_mandatory_choice_uses_legal_fallback_and_no_fake_optional_decline(self):
  for pl in (0,1):
   for n in NAMES:
    c=self.case(n,'mandatory_cancel',pl);s=c['steps'];self.assertTrue(any(x['op']=='choose_optional_target' for x in s));self.assertIn(dict(op='assert_forced_target_choices',value=1),s)
    self.assertFalse(any(x['op']=='choose_dialog' for x in s) if n!='Blood Clock' else False)
   for n in ('Sunken Hope','Blood Clock'):
    s=self.case(n,'mandatory_cancel',pl)['steps'];self.assertEqual([x['value'] for x in s if x['op']=='assert_forced_target_choices'],[1,2])
 def test_source_itself_qualifies_and_recast_rebinds_real_hand(self):
  for pl in (0,1):
   for n in NAMES[1:]:
    c=self.case(n,'return_source',pl);self.zone(c,pl,'hand',n,1);self.zone(c,pl,'battlefield',n,0)
    c=self.case(n,'recast_source',pl);s=c['steps'];self.assertEqual(sum(x==dict(op='cast',ref='source',value=1) for x in s),2)
    bind=dict(op='bind_hand_copy',player=pl,value=self.rows[n]['csvid'],index=0,ref='source');self.assertIn(bind,s);self.zone(c,pl,'hand',n,1)
   self.assertNotIn('return_source_0',variants(spec(self.declaration('Sunken Hope'),self.o['Sunken Hope'])))
 def test_empty_active_field_is_real_only_when_source_not_qualifying(self):
  for pl in (0,1):
   for n in ('Sunken Hope','Blood Clock'):
    c=self.case(n,'empty_opponent',pl);s=c['steps'];self.assertFalse(any(x['op'] in ('choose_target','choose_optional_target') and x['player']==1-pl for x in s));self.life(c,1-pl,20)
   c=self.case('Esperzoa','source_only',pl);self.zone(c,pl,'hand','Esperzoa',1)
 def test_type_color_multicolor_and_shroud_selection_proofs(self):
  for pl in (0,1):
   for n,witness in [('Esperzoa','Memnite'),('Riptide Chimera','Seal of Removal'),('Eiganjo Free-Riders','Savannah Lions'),('Oni of Wild Places','Hill Giant'),('Stampeding Serow','Grizzly Bears')]:
    c=self.case(n,'filter_controls',pl);self.zone(c,pl,'hand',witness,1);self.zone(c,pl,'battlefield','Ivory Mask',1);self.zone(c,pl,'battlefield','Air Elemental',1)
   for n in ('Eiganjo Free-Riders','Stampeding Serow'):self.zone(self.case(n,'multicolor_choice',pl),pl,'hand','Watchwolf',1)
   for n in ('Sunken Hope','Cache Raiders','Stampeding Serow','Blood Clock'):
    c=self.case(n,'shroud_choice',pl);self.assertIn(dict(op='can_target_creature',ref='probe_spell',other=f'chosen_{pl}',value=0),c['steps']);self.zone(c,pl,'hand','Deadly Insect',1)
 def test_returned_aura_is_real_and_modifier_expires(self):
  for pl in (0,1):
   c=self.case('Riptide Chimera','aura_return',pl);s=c['steps'];self.assertIn(dict(op='cast',ref='aura',value=1),s);self.assertIn(dict(op='choose_target',player=pl,ref='aura_body'),s)
   self.assertIn(dict(op='assert_card',ref='aura_body',property='power',value=4),s);self.assertIn(dict(op='assert_card',ref='aura_body',property='power',value=2),self.last(c));self.zone(c,pl,'hand','Unholy Strength',1)
 def test_token_return_ceases_with_no_hand_or_grave_card(self):
  for pl in (0,1):
   for n in ('Sunken Hope','Cache Raiders','Esperzoa','Blood Clock'):
    c=self.case(n,'token_return',pl);self.assertIn(dict(op='cast',ref='token_spell',value=1),c['steps']);self.zone(c,pl,'battlefield','Servo',1)
    for z in ('hand','graveyard','exile'):self.zone(c,pl,z,'Servo',0)
 def test_pay2_is_actual_active_player_cost_unprevented_by_armor(self):
  for pl in (0,1):
   for m,life_own,life_enemy in [('pay_all',18,18),('pay_own',18,20),('pay_opponent',20,18),('pay_then_return',18,18),('armor_pay',18,18)]:
    c=self.case('Blood Clock',m,pl);self.life(c,pl,life_own);self.life(c,1-pl,life_enemy)
    self.assertTrue(any(x['op']=='choose_dialog' and x['value']==0 for x in c['steps']))
    if m=='armor_pay':self.assertIn(dict(op='cast',ref='armor',value=1),c['steps'])
   c=self.case('Blood Clock',pl=pl);self.life(c,pl,20);self.life(c,1-pl,20);self.assertEqual([x['value'] for x in c['steps'] if x['op']=='choose_dialog'],[1,1])
 def test_low_life_menu_composition_fails_closed_instead_of_fake_dialog(self):
  n='Blood Clock';d=self.declaration(n);w=_ReturnWorld(n,spec(d,self.o[n]),0,self.rows,self.o);w.add(n,'source',0,source=True);w.life[1]=1
  with self.assertRaises(ValueError):w.visit()
  self.assertFalse(any(x['op']=='choose_dialog' for x in w.steps))
 def test_printed_flying_haste_and_trample_have_real_combat_receipts(self):
  for pl in (0,1):
   c=self.case('Oni of Wild Places','fresh_attack',pl);self.assertIn(dict(op='attack',ref='source',value=1),c['steps']);self.assertIn(dict(op='assert_life',player=1-pl,value=14),c['steps'])
   self.assertTrue(all(x['value']==0 for x in c['steps'] if x['op']=='assert_card' and x['ref']=='source' and x['property']=='sick'))
   c=self.case('Cache Raiders','fresh_attack',pl);self.assertIn(dict(op='attack',ref='source',value=0),c['steps']);self.assertFalse(any(x['op']=='combat_damage' for x in c['steps']))
   for m,blocker,legal in [('ground_block','blocker',0),('reach_block','blocker',1),('flying_block','blocker',1)]:self.assertIn(dict(op='block',ref=blocker,other='source',value=legal),self.case('Esperzoa',m,pl)['steps'])
   c=self.case('Stampeding Serow','trample_excess',pl);self.assertIn(dict(op='block',ref='blocker',other='source',value=1),c['steps']);self.life(c,1-pl,17);self.assertIn(dict(op='assert_card',ref='source',property='damage',value=2),self.last(c))
 def test_two_sources_reserve_distinct_choices_and_history_after_removal(self):
  for pl in (0,1):
   c=self.case('Sunken Hope','two_sources',pl)
   for who in (0,1):self.zone(c,who,'hand','Grizzly Bears',2)
   c=self.case('Cache Raiders','source_destroyed_after',pl);s=c['steps'];i=s.index(dict(op='destroy',ref='source'));self.assertFalse(any(x['op']=='assert_card' and x['ref']=='source' for x in s[i+1:]));self.zone(c,pl,'hand','Grizzly Bears',1)
 def test_snapshot_readiness_and_actual_cast_mana_cancellation(self):
  for m in ('ready_snapshot','untap_snapshot','resolved_snapshot'):
   s=self.case('Cache Raiders',m)['steps'];self.assertEqual(sum(x['op']=='capture' for x in s),1);self.assertIn(dict(op='assert_recorded_state'),s)
   if m=='untap_snapshot':i=s.index(dict(op='capture'));self.assertEqual(next(x for x in reversed(s[:i]) if x['op'] in ('turn','untap_step','upkeep_step')),dict(op='untap_step',player=0))
  c=self.case('Oni of Wild Places','cast_cancel');s=c['steps'];self.assertEqual(sum(x['ref'].startswith('payment_land_') for x in c['setup']),6);i=s.index(dict(op='choose_cancel_mana',player=0));self.assertEqual(s[i+1],dict(op='cast',ref='source',value=0));self.assertEqual(s[i+2],dict(op='assert_unused_choices',value=0))
 def test_released_follow4_same_receipt_models_and_source_native_stats(self):
  for pl in (0,1):
   for n,power in [('Roaring Primadox',4),('Species Gorger',6)]:
    c=self.case(n,pl=pl);self.zone(c,pl,'hand','Grizzly Bears',1);self.zone(c,1-pl,'battlefield','Grizzly Bears',1);self.assertIn(dict(op='assert_card',ref='source',property='power',value=power),self.last(c))
   c=self.case('Stampeding Wildebeests','trample_excess',pl);self.life(c,1-pl,17)
   c=self.case('Umbilicus','armor_pay',pl);self.life(c,pl,18);self.life(c,1-pl,18)
   c=self.case('Umbilicus','empty_opponent',pl);self.life(c,1-pl,20);self.assertFalse(any(x['op']=='choose_target' and x['player']==1-pl for x in c['steps']))
 def test_released_follow4_all222_keep_phase_owner_and_payment_contracts(self):
  total=0
  for n in ('Roaring Primadox','Species Gorger','Stampeding Wildebeests','Umbilicus'):
   d=self.declaration(n);p=spec(d,self.o[n]);cs=expand(n,self.rows[n]['csvid'],d,p,variants(p),self.rows,self.o);total+=len(cs)
   for c in cs:
    ready=saved=None
    for s in c['steps']:
     self.assertNotIn(s['op'],('set_life','set_counter','set_hand_count'))
     if s['op']=='advance_turn':ready=None
     elif s['op']=='untap_step':ready=s['player']
     elif s['op']=='capture':saved=ready
     elif s['op']=='restore':ready=saved
     elif s['op']=='upkeep_step':validate_upkeep_step(s);self.assertEqual(ready,s['player']);ready=None
     elif s['op'] in ('turn','cast','activate','cleanup'):ready=None
  self.assertEqual(total,222)

if __name__=='__main__':unittest.main()
