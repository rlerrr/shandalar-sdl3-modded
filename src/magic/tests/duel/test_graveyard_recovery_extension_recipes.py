import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import graveyard_recovery_extension_recipes as f

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]

class GraveyardRecoveryExtensionTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((HERE/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def d(self,name='Resourceful Return',variant='effect_0'):
  return dict(family='graveyard_recovery_extension',version=2,params=copy.deepcopy(f.PROFILES[name]['params']),oracle_text=self.o[name]['oracle_text'],variants=[variant])
 def cases(self,name='Resourceful Return',variant='effect_0',d=None):
  d=d or self.d(name,variant);p=f.spec(d,self.o[name]);return f.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.o)
 def test_requires_exact_profile_parameters_and_whole_source(self):
  for k,v in [('scope','artifact'),('mode','entry'),('conditional_draw',False),('self_destination','exile')]:
   d=self.d();d['params'][k]=v
   with self.assertRaises(ValueError):self.cases(d=d)
  d=self.d();d['params']['unknown']=True
  with self.assertRaises(ValueError):self.cases(d=d)
  d=self.d();d['oracle_text']='Return a creature.'
  with self.assertRaises(ValueError):self.cases(d=d)
 def test_changed_source_cost_type_stats_keyword_and_layout_rejected(self):
  for key,value in [('mana_cost','{B}'),('type_line','Instant'),('layout','split'),('keywords',['Storm']),('oracle_text','different')]:
   r=copy.deepcopy(self.o['Resourceful Return']);r[key]=value
   with self.assertRaises(ValueError):f.spec(self.d(),r)
  r=copy.deepcopy(self.o['Entomber Exarch']);r['power']='9'
  with self.assertRaises(ValueError):f.spec(self.d('Entomber Exarch'),r)
 def test_unknown_sources_and_alternate_faces_rejected(self):
  for name in ('Raise Dead','Regrowth'):
   with self.assertRaises(ValueError):f.spec(self.d(),self.o[name])
  for key in ('card_faces','selected_face'):
   r=copy.deepcopy(self.o['Resourceful Return']);r[key]=[{}] if key=='card_faces' else {'name':'face'}
   with self.assertRaises(ValueError):f.spec(self.d(),r)
 def test_unknown_empty_duplicate_and_nonstring_variants_rejected(self):
  for values in ([],['bad'],['effect_0','effect_0'],[{}]):
   d=self.d();d['variants']=values
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_independent_witness_characteristic_pins(self):
  f.validate_witnesses(self.o,self.rows)
  for key,value in [('type_line','Land'),('mana_cost','{0}'),('cmc',8),('oracle_text','different'),('colors',[])]:
   changed=copy.deepcopy(self.o);changed['Grizzly Bears'][key]=value
   with self.assertRaises(ValueError):f.validate_witnesses(changed,self.rows)
 def test_goblin_scope_includes_kindred_and_changeling_cards(self):
  p=self.d('Boggart Birth Rite')['params']
  for name in ("Mons's Goblin Raiders",'Goblin Piker','Tarfire','Nameless Inversion','Mothdust Changeling'):self.assertTrue(f.eligible(self.o[name],p))
  self.assertFalse(f.eligible(self.o['Grizzly Bears'],p))
 def test_instant_sorcery_union_excludes_creatures(self):
  p=self.d('Flood of Recollection')['params']
  for name in ('Giant Growth','Divination'):self.assertTrue(f.eligible(self.o[name],p))
  self.assertFalse(f.eligible(self.o['Grizzly Bears'],p))
 def test_both_controllers_use_real_casts_and_exact_cost_receipts(self):
  for owner in (0,1):
   c=self.cases(variant=f'payment_{owner}')[0]
   self.assertEqual(next(s['player'] for s in c['setup'] if s['ref']=='source'),owner)
   self.assertIn(dict(op='cast',ref='source',value=1),c['steps'])
   self.assertIn(dict(op='assert_mana_total',player=owner,value=0),c['steps'])
 def test_success_is_noop_sensitive_and_duplicates_conserved(self):
  c=self.cases()[0];bear=self.rows['Grizzly Bears']['csvid']
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=bear,value=2),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=bear,value=1),c['steps'])
 def test_pure_recovery_empty_and_wrong_type_do_not_force_illegal_choices(self):
  for name in ('Boggart Birth Rite','Resourceful Return','Flood of Recollection'):
   for variant in ('empty_graveyard_0','wrong_type_1','other_graveyard_0'):
    c=self.cases(name,variant)[0]
    self.assertIn(dict(op='can_cast',ref='source',value=0),c['steps'])
    self.assertFalse(any(s['op']=='choose_zone_card' for s in c['steps']))
 def test_buyback_decline_input_follows_the_target_and_is_omitted_at_exact_cost(self):
  c=self.cases('Disturbed Burial')[0]['steps'];zone=next(i for i,s in enumerate(c) if s['op']=='choose_zone_card');dialog=next(i for i,s in enumerate(c) if s['op']=='choose_dialog')
  self.assertLess(zone,dialog);self.assertEqual(c[dialog]['value'],1)
  self.assertFalse(any(s['op']=='choose_dialog' for s in self.cases('Disturbed Burial','payment_0')[0]['steps']))
 def test_countered_spell_selects_target_but_countered_entry_does_not_trigger(self):
  spell=self.cases('Resourceful Return','countered_1')[0]['steps'];entry=self.cases('Entomber Exarch','countered_1')[0]['steps']
  self.assertTrue(any(s['op']=='choose_zone_card' for s in spell));self.assertFalse(any(s['op']=='choose_zone_card' for s in entry));self.assertFalse(any(s['op']=='choose_dialog' for s in entry))
 def test_conditional_draw_uses_own_artifact_and_preserves_library_order(self):
  for variant,count in [('effect_0',7),('own_artifact_0',6),('opponent_artifact_0',7)]:
   steps=self.cases(variant=variant)[0]['steps'];self.assertIn(dict(op='assert_zone',player=0,zone='library',value=count),steps)
  c=self.cases(variant='own_artifact_1')[0];self.assertEqual(next(s['state'] for s in c['setup'] if s['ref']=='artifact_control'),['tapped'])
 def test_entry_component_has_mode_input_body_and_exact_return(self):
  c=self.cases('Entomber Exarch','effect_1')[0];steps=c['steps']
  self.assertIn(dict(op='choose_dialog',player=1,value=0),steps)
  self.assertIn(dict(op='assert_card',ref='source',property='power',value=2),steps)
  self.assertIn(dict(op='assert_zone',player=1,zone='battlefield',value=1),steps)
  self.assertFalse(any(s['op']=='require_can_cast' for s in steps))
  self.assertIn(dict(op='cast',ref='source',value=1),steps)
 def test_second_uncast_splice_card_gets_explicit_done_before_zone(self):
  for owner in (0,1):
   steps=self.cases('Soulless Revival',f'repeat_{owner}')[0]['steps']
   dialogs=[(i,s['value']) for i,s in enumerate(steps) if s['op']=='choose_dialog']
   if owner==0:
    self.assertEqual(len(dialogs),1);self.assertEqual(dialogs[0][1],0)
    self.assertLess(dialogs[0][0],next(i for i,s in enumerate(steps) if s['op']=='choose_zone_card'))
   else:self.assertEqual(dialogs,[])
 def test_modal_entry_omits_unreviewed_empty_graveyard_mode_legality(self):
  p=f.spec(self.d('Entomber Exarch'),self.o['Entomber Exarch'])
  self.assertNotIn('empty_graveyard_0',f.variants(p));self.assertTrue(any('other printed mode' in g for g in p['additional_components']))
 def test_source_exile_is_separate_from_returned_card_hand(self):
  c=self.cases('Flood of Recollection')[0]['steps'];source=self.rows['Flood of Recollection']['csvid']
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=source,value=1),c)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=source,value=0),c)
 def test_repeated_casts_recover_distinct_cards_and_have_mana_reserve(self):
  steps=self.cases(variant='repeat_0')[0]['steps']
  self.assertEqual(sum(s['op']=='cast' for s in steps),2)
  self.assertEqual(len({s['value'] for s in steps if s['op']=='choose_zone_card'}),2)
  self.assertEqual(sum(s['op']=='set_mana' and s['value']==30 for s in steps),6)
 def test_snapshots_replay_real_state_and_input_queue(self):
  for variant in ('ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0'):
   steps=self.cases(variant=variant)[0]['steps'];self.assertIn(dict(op='ai_roundtrip'),steps);self.assertEqual(steps[-1]['op'],'assert_recorded_state')
 def test_component_gaps_and_inputs_are_preserved(self):
  for name,fragment in [('Disturbed Burial','Buyback'),('Soulless Revival','Splice'),('Entomber Exarch','discard')]:
   self.assertTrue(any(fragment in s for s in self.cases(name)[0]['_recipe']['untested_components']))
  d=self.d();before=copy.deepcopy(d);self.cases(d=d);self.assertEqual(before,d)
 def test_activation_cost_pins_come_from_oracle_and_are_not_handler_values(self):
  p=f.spec(self.d('Corpse Hauler'),self.o['Corpse Hauler'])
  self.assertEqual(p['activation_mana'],[2,1,0,0,0,0])
  d=self.d('Corpse Hauler');d['params']['activation']['mana']='{1}{B}'
  with self.assertRaises(ValueError):self.cases('Corpse Hauler',d=d)
 def test_activated_artifact_value_boundary_and_wrong_predicates(self):
  p=self.d('Auriok Salvagers')['params']
  for n in ('Ornithopter','Sol Ring','Darksteel Relic'):self.assertTrue(f.eligible(self.o[n],p))
  self.assertFalse(f.eligible(self.o['Fellwar Stone'],p));self.assertFalse(f.eligible(self.o['Grizzly Bears'],p))
  steps=self.cases('Auriok Salvagers','value_boundary_1')[0]['steps']
  self.assertIn(dict(op='choose_zone_card',player=1,value=self.rows['Sol Ring']['csvid']),steps)
 def test_self_sacrifice_receipt_does_not_read_reused_hand_slot(self):
  steps=self.cases('Corpse Hauler','effect_0')[0]['steps']
  self.assertIn(dict(op='activate',ref='source',value=1),steps)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Corpse Hauler']['csvid'],value=1),steps)
  self.assertFalse(any(s['op']=='assert_card' and s['ref']=='source' and s['property']=='in_hand' for s in steps))
 def test_activated_life_cost_and_insufficient_life_are_real(self):
  steps=self.cases('Phyrexian Reclamation','effect_1')[0]['steps']
  self.assertIn(dict(op='assert_life',player=1,value=18),steps)
  steps=self.cases('Phyrexian Reclamation','insufficient_life_0')[0]['steps']
  self.assertIn(dict(op='set_life',player=0,value=1),steps)
  self.assertIn(dict(op='activate',ref='source',value=0),steps)
 def test_fresh_non_tap_activation_uses_actual_source_cast(self):
  c=self.cases('Hana Kami','fresh_activation_1')[0];steps=c['steps']
  self.assertEqual(next(s['zone'] for s in c['setup'] if s['ref']=='source'),'hand')
  self.assertLess(next(i for i,s in enumerate(steps) if s['op']=='cast'),next(i for i,s in enumerate(steps) if s['op']=='activate'))
 def test_pending_activation_snapshot_follows_payment_before_recovery(self):
  steps=self.cases('Auriok Salvagers','pending_snapshot_0')[0]['steps']
  self.assertLess(next(i for i,s in enumerate(steps) if s['op']=='activate'),next(i for i,s in enumerate(steps) if s['op']=='capture'))
  self.assertEqual(steps[-1]['op'],'assert_recorded_state')
 def test_choose_one_or_both_helpers_decline_the_other_card_type(self):
  for name in ('Grim Discovery','Remember the Fallen','Reviving Melody','Fortuitous Find'):
   steps=self.cases(name)[0]['steps']
   self.assertTrue(any(s['op']=='choose_optional_zone' for s in steps))
   self.assertFalse(any(s['op']=='choose_dialog' for s in steps))
   self.assertLess(next(i for i,s in enumerate(steps) if s['op']=='choose_zone_card'),next(i for i,s in enumerate(steps) if s['op']=='choose_optional_zone'))
 def test_ghoulcaller_menu_depends_on_actual_zombie_choices(self):
  ordinary=self.cases("Ghoulcaller's Chant")[0]['steps']
  bare=self.cases("Ghoulcaller's Chant",'bare_zones_0')[0]['steps']
  self.assertIn(dict(op='choose_dialog',player=0,value=0),ordinary)
  self.assertFalse(any(s['op']=='choose_dialog' for s in bare))
  ai=self.cases("Ghoulcaller's Chant",'bare_zones_1')[0]['steps']
  self.assertIn(dict(op='choose_dialog',player=1,value=0),ai)
 def test_grave_exchange_targets_real_player_before_grave_selection(self):
  steps=self.cases('Grave Exchange','effect_1')[0]['steps']
  self.assertIn(dict(op='choose_player',player=1,value=0),steps)
  self.assertLess(next(i for i,s in enumerate(steps) if s['op']=='choose_player'),next(i for i,s in enumerate(steps) if s['op']=='choose_zone_card'))
 def test_noncreature_goblin_and_changeling_cards_have_actual_return_variants(self):
  for variant,name in [('kindred_card_0','Tarfire'),('changeling_card_1','Nameless Inversion')]:
   owner=int(variant[-1]);steps=self.cases('Boggart Birth Rite',variant)[0]['steps']
   self.assertIn(dict(op='choose_zone_card',player=owner,value=self.rows[name]['csvid']),steps)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[name]['csvid'],value=1),steps)
 def test_fresh_tap_sources_cannot_activate_until_established(self):
  for name in ("Hanna, Ship's Navigator",'Scribe of the Mindful'):
   steps=self.cases(name,'fresh_activation_1')[0]['steps']
   self.assertIn(dict(op='cast',ref='source',value=1),steps)
   self.assertIn(dict(op='require_can_activate',ref='source',value=0),steps)
   self.assertIn(dict(op='activate',ref='source',value=0),steps)
   self.assertFalse(any(s['op']=='choose_zone_card' for s in steps))
 def test_generic_only_ability_has_no_wrong_color_scenario(self):
  p=f.spec(self.d('Scribe of the Mindful'),self.o['Scribe of the Mindful'])
  self.assertNotIn('wrong_color_0',f.variants(p));self.assertEqual(p['activation_mana'],[1,0,0,0,0,0])
 def test_exile_self_cost_has_separate_source_exile_receipt(self):
  steps=self.cases('Nyx Weaver','effect_0')[0]['steps'];source=self.rows['Nyx Weaver']['csvid']
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='exile',index=source,value=1),steps)
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=source,value=0),steps)
  p=f.spec(self.d('Nyx Weaver'),self.o['Nyx Weaver']);self.assertNotIn('wrong_type_0',f.variants(p))
 def test_basic_land_recovery_excludes_nonbasic_artifact_land(self):
  p=self.d('Groundskeeper')['params']
  self.assertTrue(f.eligible(self.o['Forest'],p));self.assertFalse(f.eligible(self.o['Darksteel Citadel'],p))
 def test_artifact_enchantment_union_has_real_both_type_variants(self):
  for variant,target in [('type_artifact_0','Ornithopter'),('type_enchantment_1','Seal of Cleansing')]:
   steps=self.cases("Hanna, Ship's Navigator",variant)[0]['steps']
   self.assertIn(dict(op='choose_zone_card',player=int(variant[-1]),value=self.rows[target]['csvid']),steps)

if __name__=='__main__':unittest.main()
