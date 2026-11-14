import copy,json,unittest
from pathlib import Path
from generate_cases import registry,validate_upkeep_step
from upkeep_echo_payment_recipes import FAMILY,classify,spec,variants,expand,validate_witnesses

NAMES=['Acridian','Cradle Guard','Herald of Serra','Shivan Raptor',"Multani's Acolyte","Radiant's Dragoons",'Simian Grunts','Uktabi Drake']
class UpkeepEchoPaymentRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,n):return dict(family=FAMILY,version=2,oracle_text=self.o[n]['oracle_text'],params=classify(self.o[n]),variants=['pay_0'])
 def case(self,n='Acridian',mode='pay',owner=0):
  d=self.declaration(n);return expand(n,self.rows[n]['csvid'],d,spec(d,self.o[n]),[mode+'_'+str(owner)],self.rows,self.o)[0]
 def final(self,c):return c['steps'][max(i for i,s in enumerate(c['steps']) if s['op']=='upkeep_step')+1:]
 def zone(self,c,n,pl,z,amount):self.assertIn(dict(op='assert_zone_multiplicity',player=pl,zone=z,index=self.rows[n]['csvid'],value=amount),self.final(c))
 def test_whole_oracle_echo_and_all_printed_callbacks_fail_closed(self):
  for n in NAMES:
   self.assertEqual(len(classify(self.o[n])['echo_mana']),6)
   with self.assertRaises(ValueError):classify(dict(self.o[n],oracle_text=self.o[n]['oracle_text']+'\nDraw a card.'))
  for n in ('Albino Troll','Crater Hellion','Ghitu Slinger','Hunting Moa','Karmic Guide','Shah of Naar Isle','Thran War Machine','Ticking Gnomes'):
   with self.assertRaises(ValueError):classify(self.o[n])
  bad=dict(self.o["Multani's Acolyte"],oracle_text=self.o["Multani's Acolyte"]['oracle_text']+'\nWhen this creature enters, you gain 5 life.')
  with self.assertRaises(ValueError):classify(bad)
  self.assertEqual(classify(self.o['Uktabi Drake'])['echo_mana'],[1,0,0,2,0,0]);self.assertEqual(self.o['Uktabi Drake']['mana_cost'],'{G}')
 def test_exact_json_types_source_pins_and_released_followers(self):
  r=self.o['Acridian'];d=self.declaration('Acridian')
  for k,v in [('family','upkeep_controlled_return'),('version',2.0),('version',True),('oracle_text','Echo {G}')]:
   with self.assertRaises(ValueError):spec(dict(d,**{k:v}),r)
  for k,v in [('echo_mana',[1.0,0,0,1,0,0]),('echo_mana',[True,0,0,1,0,0]),('entry_amount',False),('entry_effect',[]),('keywords',()),('extra',0)]:
   bad=copy.deepcopy(d);bad['params'][k]=v
   with self.assertRaises(ValueError):spec(bad,r)
  with self.assertRaises(ValueError):spec(d,dict(r,power='3'))
  for n in ('Goblin Patrol','Goblin War Buggy','Pouncing Jaguar'):
   self.assertEqual(spec(self.declaration(n),self.o[n])['echo_mana'],classify(self.o[n])['echo_mana'])
 def test_released_follow3_pure_colored_costs_haste_and_exact_AI_duplicate_funding(self):
  for n,color,amount in [('Goblin Patrol',4,1),('Goblin War Buggy',4,2),('Pouncing Jaguar',3,1)]:
   for owner in (0,1):
    c=self.case(n,'pay',owner);self.zone(c,n,owner,'battlefield',1);self.assertEqual(sum(x['op']=='choose_dialog' for x in c['steps']),1);self.assertEqual([x['player'] for x in c['steps'] if x['op']=='upkeep_step'],[1-owner,owner]);self.assertEqual(sum(x['ref'].startswith('echo_land_') for x in c['setup']),amount)
    if owner==0:
     self.assertIn(dict(op='choose_mana_source',ref=f'echo_land_0_{color}_0'),c['steps']);self.assertEqual(sum(x['op']=='choose_mana_source' for x in c['steps']),amount)
    self.assertIn(dict(op='assert_card',ref='source',property='power',value=2),self.final(c))
   c=self.case(n,'two_sources',1);self.zone(c,n,1,'battlefield',2);self.assertEqual(sum(x['op']=='choose_dialog' for x in c['steps']),2);self.assertTrue(all(x['name']==('Mountain' if color==4 else 'Forest') for x in c['setup'] if x['ref'].startswith('echo_land_')))
  for owner in (0,1):
   s=self.case('Goblin War Buggy','fresh_attack',owner)['steps'];self.assertIn(dict(op='attack',ref='source',value=1),s);self.assertIn(dict(op='assert_life',player=1-owner,value=18),s)
   s=self.case('Goblin Patrol','fresh_attack',owner)['steps'];self.assertIn(dict(op='attack',ref='source',value=0),s)
 def test_independent_full_witness_pins_and_native_dummy_honesty(self):
  validate_witnesses(self.o,self.rows);self.assertEqual(self.rows['Grizzly Bears']['handler'],'card_dummy')
  bad=copy.deepcopy(self.o);bad['Wastes']['oracle_text']='{T}: Add {G}.'
  with self.assertRaises(ValueError):validate_witnesses(bad,self.rows)
  bad=copy.deepcopy(self.rows);bad['Simian Grunts']['handler']='card_dummy'
  with self.assertRaises(ValueError):validate_witnesses(self.o,bad)
 def test_human_payment_uses_real_live_lands_and_zero_pool_upkeep(self):
  c=self.case();s=c['steps'];self.assertEqual([x['player'] for x in s if x['op']=='upkeep_step'],[1,0]);up=next(i for i,x in enumerate(s) if x['op']=='upkeep_step' and x['player']==0)
  self.assertEqual(s[up-3:up],[dict(op='choose_dialog',player=0,value=0),dict(op='choose_mana_source',ref='echo_land_0_0_0'),dict(op='choose_mana_source',ref='echo_land_0_3_0')])
  setup={x['ref']:x for x in c['setup']};self.assertEqual(setup['echo_land_0_0_0']['name'],'Wastes');self.assertEqual(setup['echo_land_0_3_0']['name'],'Forest')
  for ref in ('echo_land_0_0_0','echo_land_0_3_0'):self.assertIn(dict(op='assert_card',ref=ref,property='tapped',value=1),self.final(c))
  self.assertEqual(s[up+1],dict(op='assert_unused_choices',value=0))
 def test_decline_insufficient_wrong_color_and_actual_human_cancellation(self):
  for owner in (0,1):
   for mode in ('decline','echo_insufficient','echo_wrong_color'):
    c=self.case(mode=mode,owner=owner);self.zone(c,'Acridian',owner,'graveyard',1);self.zone(c,'Acridian',owner,'battlefield',0)
    self.assertFalse(any(x['op']=='choose_mana_source' for x in c['steps']))
    self.assertEqual(sum(x['op']=='choose_dialog' for x in c['steps']),int(mode=='decline'))
   c=self.case(mode='opponent_upkeep',owner=owner);self.zone(c,'Acridian',owner,'battlefield',1);self.assertFalse(any(x['op']=='choose_dialog' for x in c['steps']))
  c=self.case(mode='echo_cancel');self.zone(c,'Acridian',0,'graveyard',1);self.assertIn(dict(op='choose_cancel_mana',player=0),c['steps']);self.assertNotIn('echo_cancel_1',variants(spec(self.declaration('Acridian'),self.o['Acridian'])))
 def test_paid_echo_is_history_not_recurrent_tax_and_removed_source_has_no_legacy_cost(self):
  for owner in (0,1):
   c=self.case(mode='paid_history',owner=owner);self.assertEqual([x['player'] for x in c['steps'] if x['op']=='upkeep_step'],[1-owner,owner]*3);self.assertEqual(sum(x['op']=='choose_dialog' for x in c['steps']),1);self.zone(c,'Acridian',owner,'battlefield',1)
   for mode in ('source_absent','source_in_hand'):
    c=self.case(mode=mode,owner=owner);self.assertFalse(any(x['op']=='choose_dialog' for x in c['steps']))
   c=self.case(mode='source_destroyed_after',owner=owner);i=c['steps'].index(dict(op='destroy',ref='source'));self.assertFalse(any(x['op'].startswith('choose_') for x in c['steps'][i+1:]));self.zone(c,'Acridian',owner,'graveyard',1)
 def test_real_control_acquisition_rearms_and_decline_keeps_owner_graveyard(self):
  for owner in (0,1):
   c=self.case(mode='controller_decline',owner=owner);s=c['steps'];self.assertIn(dict(op='cast',ref='control_aura',value=1),s);self.assertIn(dict(op='bind_permanent_copy',player=1-owner,value=3690,index=0,ref='source'),s);self.zone(c,'Acridian',owner,'graveyard',1);self.zone(c,'Acridian',1-owner,'graveyard',0);self.zone(c,'Control Magic',1-owner,'graveyard',1)
   c=self.case(mode='control_rearm',owner=owner);self.assertEqual([x['player'] for x in c['steps'] if x['op']=='choose_dialog'],[owner,1-owner]);self.zone(c,'Acridian',1-owner,'battlefield',1)
   c=self.case(mode='control_return',owner=owner);self.assertEqual([x['player'] for x in c['steps'] if x['op']=='choose_dialog'],[owner,1-owner,owner]);self.assertIn(dict(op='cast',ref='aura_removal',value=1),c['steps']);self.zone(c,'Acridian',owner,'battlefield',1)
 def test_actual_bounce_recast_rearms_and_entry_callbacks_repeat(self):
  for owner in (0,1):
   for n in ("Multani's Acolyte","Radiant's Dragoons"):
    c=self.case(n,'recast',owner);s=c['steps'];self.assertEqual(sum(x==dict(op='cast',ref='source',value=1) for x in s),2);self.assertIn(dict(op='cast',ref='bounce_spell',value=1),s);self.assertIn(dict(op='bind_hand_copy',player=owner,value=self.rows[n]['csvid'],index=0,ref='source'),s);self.assertEqual(sum(x['op']=='choose_dialog' for x in s),2)
    if n=="Radiant's Dragoons":self.assertIn(dict(op='assert_life',player=owner,value=30),self.final(c))
    else:self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=6),self.final(c))
   c=self.case("Multani's Acolyte",'exact_entry_library',owner);self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=0),self.final(c));self.zone(c,'Forest',owner,'hand',1)
 def test_two_real_sources_each_pay_once_with_cumulative_entry_effects(self):
  for owner in (0,1):
   c=self.case("Multani's Acolyte",'two_sources',owner);self.assertEqual(sum(x['op']=='cast' for x in c['steps']),2);self.assertEqual(sum(x['op']=='choose_dialog' for x in c['steps']),2);self.zone(c,"Multani's Acolyte",owner,'battlefield',2);self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=6),self.final(c));self.assertEqual(sum(x['op']=='choose_trigger_source' for x in c['steps']),int(owner==0))
 def test_flash_uses_actual_pending_bridge_and_next_own_echo_without_forcing_menu(self):
  for owner in (0,1):
   c=self.case('Simian Grunts','flash_response',owner);s=c['steps'];self.assertEqual(s[0],dict(op='turn',player=1-owner,value=0));self.assertFalse(any(x['op']=='choose_priority' or x.get('ref')=='source' and x['op']=='cast' for x in s));self.assertIn(dict(op='cast_response',ref='source',value=1),s);self.assertEqual([x['player'] for x in s if x['op']=='upkeep_step'],[owner,1-owner,owner]);self.assertEqual(sum(x['op']=='choose_dialog' for x in s),1);self.zone(c,'Simian Grunts',owner,'battlefield',1);self.zone(c,'Grizzly Bears',1-owner,'battlefield',1);self.assertIn('not menu eligibility',c['_recipe']['untested_components'][1])
 def test_explicit_flash_bridge_announces_real_pending_spells_and_drains_both(self):
  for owner in (0,1):
   s=self.case('Simian Grunts','flash_response',owner)['steps'];parent=s.index(dict(op='cast',ref='flash_parent',value=1));self.assertEqual(s[parent:parent+11],[dict(op='cast',ref='flash_parent',value=1),dict(op='assert_stack',value=1),dict(op='announce_spell',ref='flash_parent'),dict(op='cast_response',ref='source',value=1),dict(op='assert_stack',value=2),dict(op='assert_card',ref='source',property='in_hand',value=0),dict(op='assert_card',ref='source',property='in_play',value=0),dict(op='announce_spell',ref='source'),dict(op='resolve_response'),dict(op='assert_card',ref='source',property='in_play',value=1),dict(op='assert_unused_choices',value=0)]);self.assertEqual(s[parent+11],dict(op='assert_stack',value=0));self.assertFalse(any(x['op']=='set_mana' for x in s[parent:parent+11]))
 def test_correction_ai_duplicate_generic_funding_does_not_require_autotap_strategy(self):
  for n in NAMES:
   fee=classify(self.o[n])['echo_mana'];c=self.case(n,'two_sources',1);lands=[x for x in c['setup'] if x['ref'].startswith('echo_land_1_')];self.assertEqual(len(lands),2*sum(fee));self.assertTrue(all(x['name']!='Wastes' for x in lands));self.assertEqual(len({x['name'] for x in lands}),1);self.assertEqual(sum(x['op']=='choose_dialog' for x in c['steps']),2);self.zone(c,n,1,'battlefield',2)
   # Any first payment uses exactly half the same-colored one-mana lands,
   # so the rest can still pay every colored symbol and generic component.
   remainder=len(lands)-sum(fee);self.assertEqual(remainder,sum(fee));self.assertGreaterEqual(remainder,sum(fee[1:]))
   if fee[0]:
    self.assertTrue(any(x['name']=='Wastes' and x['ref'].startswith('echo_land_0_') for x in self.case(n,'two_sources',0)['setup']))
    for z in ('battlefield','hand','graveyard','exile'):self.zone(c,'Wastes',1,z,0)
 def test_complete_fresh_haste_vigilance_first_strike_trample_and_evasion_receipts(self):
  for owner in (0,1):
   for n,legal in [('Acridian',0),('Shivan Raptor',1),('Uktabi Drake',1)]:
    c=self.case(n,'fresh_attack',owner);self.assertIn(dict(op='attack',ref='source',value=legal),c['steps']);self.assertFalse(any(x['op']=='upkeep_step' for x in c['steps']))
   c=self.case('Herald of Serra','vigilance',owner);i=c['steps'].index(dict(op='finish_attackers'));self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),c['steps'][i:])
   c=self.case('Shivan Raptor','first_strike',owner);self.zone(c,'Hill Giant',1-owner,'graveyard',1);self.zone(c,'Shivan Raptor',owner,'battlefield',1);self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),c['steps'][c['steps'].index(dict(op='end_combat')):])
   c=self.case('Cradle Guard','trample_excess',owner);self.assertIn(dict(op='assert_life',player=1-owner,value=18),c['steps']);self.assertIn(dict(op='assert_card',ref='source',property='damage',value=2),c['steps'])
   for mode,legal in [('ground_block',0),('reach_block',1),('flying_block',1)]:self.assertIn(dict(op='block',ref='blocker',other='source',value=legal),self.case('Herald of Serra',mode,owner)['steps'])
 def test_cast_failures_do_not_confuse_raw_predicate_with_real_cost(self):
  for mode in ('cast_insufficient','cast_wrong_color'):
   c=self.case(mode=mode);self.assertIn(dict(op='cast',ref='source',value=0),c['steps']);self.assertFalse(any('Land' in self.o[x['name']]['type_line'] and x['zone']=='battlefield' for x in c['setup']));self.assertFalse(any(x['op'] in ('can_cast','require_can_cast') for x in c['steps']))
  c=self.case(mode='cast_cancel');self.assertIn(dict(op='choose_cancel_mana',player=0),c['steps']);self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])
 def test_all_new_cases_have_real_phase_provenance_and_snapshot_history(self):
  for n in NAMES:
   d=self.declaration(n);p=spec(d,self.o[n])
   for c in expand(n,self.rows[n]['csvid'],d,p,variants(p),self.rows,self.o):
    self.assertTrue(all(x.get('state') is None for x in c['setup']));self.assertEqual(next(x for x in c['setup'] if x['ref']=='source')['zone'],'hand');ready=saved=None;last_turn=False
    for s in c['steps']:
     self.assertNotIn(s['op'],('set_life','set_counter','set_card_available','reset_death_history'))
     if s['op']=='advance_turn':last_turn=True;ready=None
     elif s['op']=='untap_step':self.assertTrue(last_turn);ready=s['player'];last_turn=False
     elif s['op']=='capture':saved=(ready,last_turn)
     elif s['op']=='restore':ready,last_turn=saved
     elif s['op']=='upkeep_step':validate_upkeep_step(s);self.assertEqual(ready,s['player']);ready=None
     elif s['op'] in ('cast','activate','turn','cleanup'):ready=None
     elif s['op']=='set_mana':self.assertIsNone(ready)
     if s['op']=='assert_life':self.assertGreater(s['value'],0)
  for mode in ('ready_snapshot','untap_snapshot','paid_snapshot'):
   s=self.case(mode=mode)['steps'];self.assertEqual(sum(x['op']=='capture' for x in s),1);self.assertIn(dict(op='ai_roundtrip'),s);self.assertIn(dict(op='assert_recorded_state'),s)
   if mode=='paid_snapshot':
    cut=s.index(dict(op='capture'));self.assertEqual(sum(x['op']=='upkeep_step' for x in s[cut:]),4);self.assertFalse(any(x['op'].startswith('choose_') for x in s[cut:]));self.assertEqual(sum(x['op']=='choose_dialog' for x in s[:cut]),1)

if __name__=='__main__':unittest.main()
