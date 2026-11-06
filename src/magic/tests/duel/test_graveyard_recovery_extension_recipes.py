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

class Wave6RecoveryTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((HERE/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()};cls.pins=f.wave6_pins()
 def d(self,name='Dutiful Return',v='effect_0'):
  p=self.pins['profiles'][name]
  return dict(family=p['family'],version=2,oracle_text=self.o[name]['oracle_text'],params=copy.deepcopy(p['params']),variants=[v])
 def case(self,name='Dutiful Return',v='effect_0',d=None):
  from recipes import expand
  return expand(name,self.rows[name]['csvid'],d or self.d(name,v),self.o[name],self.rows,oracle_context=self.o)[0]
 def test_opt_in_keeps_top_level_version_two_and_exact_complete_pins(self):
  for name in self.pins['profiles']:
   d=self.d(name);self.assertEqual(d['version'],2);self.assertEqual(d['params']['schema'],3)
   self.case(name,d=d)
   for key,value in [('mana_cost','{0}'),('oracle_text','Return a card.'),('colors',[]),('layout','split')]:
    if self.o[name].get(key)==value:continue
    r=copy.deepcopy(self.o[name]);r[key]=value
    with self.assertRaises(ValueError):f.wave6_spec(d,r,d['family'])
   d['version']=3
   with self.assertRaises(ValueError):f.wave6_spec(d,self.o[name],d['family'])
 def test_independent_whole_witness_pins_and_mutation_free_expansion(self):
  f.wave6_validate_witnesses(self.o,self.rows)
  for name,key,value in [('Sol Ring','cmc',2),('Howling Mine','type_line','Land'),('Grizzly Bears','power','9')]:
   changed=dict(self.o);changed[name]=dict(self.o[name],**{key:value})
   with self.assertRaises(ValueError):f.wave6_validate_witnesses(changed,self.rows)
  d=self.d();before=copy.deepcopy(d);self.case(d=d);self.assertEqual(before,d)
 def test_bool_integer_parameter_aliases_are_rejected_on_public_path(self):
  for name in self.pins['profiles']:
   for key,value in self.d(name)['params'].items():
    if type(value) is int and value in (0,1):
     d=self.d(name);d['params'][key]=bool(value)
     with self.assertRaises(ValueError):self.case(name,d=d)
    if type(value) is bool:
     d=self.d(name);d['params'][key]=int(value)
     with self.assertRaises(ValueError):self.case(name,d=d)
 def test_added_private_witnesses_do_not_rewrite_the_initial_universe(self):
  from unittest.mock import patch
  before=self.case('Roar of Reclamation')
  pins=copy.deepcopy(self.pins);pins['cards']['Elvish Mystic']=self.o['Elvish Mystic']
  with patch.object(f,'wave6_pins',return_value=pins):self.assertEqual(before,self.case('Roar of Reclamation'))
 def test_exact_two_and_up_to_zero_have_distinct_availability(self):
  for owner in (0,1):
   s=self.case("Death's Duet",f'one_available_{owner}')['steps'];self.assertIn(dict(op='cast',ref='source',value=0),s)
   s=self.case('Dutiful Return',f'empty_{owner}')['steps'];self.assertIn(dict(op='require_can_cast',ref='source',value=1),s);self.assertIn(dict(op='cast',ref='source',value=1),s)
   s=self.case('Dutiful Return',f'zero_{owner}')['steps'];self.assertIn(dict(op='choose_optional_zone',player=owner,value=self.rows['Grizzly Bears']['csvid']),s)
 def test_up_to_selector_gets_done_when_fewer_legal_targets_remain(self):
  for owner in (0,1):
   for mode in ('wrong_type','one_available','all_lost','repeat'):
    s=self.case('Dutiful Return',f'{mode}_{owner}')['steps'];self.assertTrue(any(x['op']=='choose_optional_zone' for x in s))
   self.assertFalse(any(x['op']=='choose_optional_zone' for x in self.case('Dutiful Return',f'empty_{owner}')['steps']))
 def test_repeated_exact_two_has_four_original_eligible_cards(self):
  for owner in (0,1):
   c=self.case("Death's Duet",f'repeat_{owner}');n=sum(x['zone']=='graveyard' and x['player']==owner and 'Creature' in self.o[x['name']]['type_line'] for x in c['setup'])
   self.assertGreaterEqual(n,4);self.assertEqual(sum(x['op']=='cast' for x in c['steps']),2)
 def test_life_uses_selected_artifact_mana_value(self):
  for owner in (0,1):
   c=self.case('Razor Hippogriff',f'effect_{owner}');selected=next(s['value'] for s in c['steps'] if s['op']=='choose_zone_card');name=next(n for n,r in self.rows.items() if r['csvid']==selected)
   self.assertGreater(self.o[name]['cmc'],0);self.assertIn(dict(op='assert_life',player=owner,value=20+int(self.o[name]['cmc'])),c['steps'])
 def test_artifact_value_boundary_has_two_independent_legal_witnesses(self):
  p=f.wave6_spec(self.d('Leonin Squire'),self.o['Leonin Squire'],'etb_recovery_creature')
  self.assertTrue(f.wave6_eligible(self.o['Sol Ring'],p));self.assertTrue(f.wave6_eligible(self.o['Ornithopter'],p));self.assertFalse(f.wave6_eligible(self.o['Howling Mine'],p))
  self.assertIn(dict(op='choose_zone_card',player=0,value=self.rows['Sol Ring']['csvid']),self.case('Leonin Squire','value_boundary_0')['steps'])
 def test_colored_union_requires_a_creature_and_either_printed_color(self):
  p=self.d('Crypt Angel')['params']
  for n in ('Coral Merfolk','Goblin Piker','Hill Giant'):self.assertTrue(f.wave6_eligible(self.o[n],p))
  for n in ('Grizzly Bears','Ornithopter','Lightning Bolt','Divination'):self.assertFalse(f.wave6_eligible(self.o[n],p))
 def test_equipment_aura_union_and_permanent_types_are_independent(self):
  p=self.d('Ironclad Slayer')['params']
  for n in ('Holy Strength','Bonesplitter'):self.assertTrue(f.wave6_eligible(self.o[n],p))
  for n in ('Sol Ring','Seal of Cleansing','Grizzly Bears'):self.assertFalse(f.wave6_eligible(self.o[n],p))
  p=self.d('Golgari Findbroker')['params']
  for n in ('Sol Ring','Grizzly Bears','Seal of Cleansing','Forest','Jace Beleren'):self.assertTrue(f.wave6_eligible(self.o[n],p))
  for n in ('Giant Growth','Divination'):self.assertFalse(f.wave6_eligible(self.o[n],p))
 def test_soulshift_is_spirit_card_not_just_spirit_creature(self):
  p=self.d('Nightsoil Kami')['params']
  for n in ('Lantern Kami','Kami of Ancient Law','Nameless Inversion','Mothdust Changeling','Raging Poltergeist'):self.assertTrue(f.wave6_eligible(self.o[n],p))
  for n in ('Grizzly Bears','Giant Growth','Kami of Lunacy'):self.assertFalse(f.wave6_eligible(self.o[n],p))
  self.assertIn(dict(op='choose_zone_card',player=1,value=self.rows['Nameless Inversion']['csvid']),self.case('Nightsoil Kami','kindred_card_1')['steps'])
 def test_pit_keeper_three_four_boundary_counts_graveyard_only(self):
  for mode,count in [('condition_below',3),('condition_boundary',4)]:
   c=self.case('Pit Keeper',mode+'_0');actual=sum(x['zone']=='graveyard' and x['player']==0 and 'Creature' in self.o[x['name']]['type_line'] for x in c['setup'])
   self.assertEqual(actual,count);self.assertEqual(any(s['op']=='choose_zone_card' for s in c['steps']),count==4)
 def test_pit_rechecks_condition_after_recovery_changes_the_graveyard(self):
  for owner in (0,1):
   for mode,initial,returned in [('repeat',5,2),('condition_depleted',4,1)]:
    c=self.case('Pit Keeper',f'{mode}_{owner}');count=sum(x['zone']=='graveyard' and x['player']==owner and 'Creature' in self.o[x['name']]['type_line'] for x in c['setup'])
    self.assertEqual(count,initial);self.assertEqual(sum(s['op']=='cast' for s in c['steps']),2);self.assertEqual(sum(s['op']=='choose_zone_card' for s in c['steps']),returned)
 def test_pinned_profile_cannot_change_quantity_or_threshold_away_from_text(self):
  from unittest.mock import patch
  for name,key,value in [("Death's Duet",'minimum',1),('Pit Keeper','minimum_grave_creatures',5),('Pit Keeper','optional',1)]:
   pins=copy.deepcopy(self.pins);pins['profiles'][name]['params'][key]=value;d=self.d(name);d['params']=copy.deepcopy(pins['profiles'][name]['params'])
   with patch.object(f,'wave6_pins',return_value=pins):
    with self.assertRaises(ValueError):self.case(name,d=d)
 def test_random_return_uses_homogeneous_eligible_copies_and_no_forced_pick(self):
  names=[]
  for mode in ('effect','alternate'):
   c=self.case('Ghoulraiser',mode+'_1');p=self.d('Ghoulraiser')['params'];eligible=[x['name'] for x in c['setup'] if x['zone']=='graveyard' and x['player']==1 and f.wave6_eligible(self.o[x['name']],p)]
   self.assertEqual(len(eligible),2);self.assertEqual(len(set(eligible)),1);names.append(eligible[0]);self.assertFalse(any(s['op'].startswith('choose_') for s in c['steps']))
  self.assertNotEqual(names[0],names[1])
 def test_possessed_skaab_death_replacement_preserves_original_graveyard(self):
  for owner in (0,1):
   s=self.case('Possessed Skaab',f'destroy_source_{owner}')['steps'];sid=self.rows['Possessed Skaab']['csvid']
   self.assertIn(dict(op='destroy',ref='source'),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='exile',index=sid,value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=sid,value=0),s)
 def test_restock_resolved_exiles_but_countered_goes_to_grave(self):
  for mode,zone in [('effect','exile'),('countered','graveyard')]:
   self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone=zone,index=self.rows['Restock']['csvid'],value=1),self.case('Restock',mode+'_0')['steps'])
 def test_optional_entry_human_trigger_input_precedes_actual_zone_selection(self):
  s=self.case('Ironclad Slayer')['steps'];self.assertLess(next(i for i,x in enumerate(s) if x['op']=='choose_trigger'),next(i for i,x in enumerate(s) if x['op']=='choose_zone_card'))
  self.assertFalse(any(x['op'].startswith('choose_') for x in self.case('Ironclad Slayer','decline_0')['steps']))
 def test_ai_empty_eligibility_optional_selector_declines_without_forcing_card(self):
  c=self.case('Ironclad Slayer','wrong_type_1');p=self.d('Ironclad Slayer')['params']
  self.assertFalse(any(x['zone']=='graveyard' and x['player']==1 and f.wave6_eligible(self.o[x['name']],p) for x in c['setup']))
  self.assertTrue(any(x['op']=='choose_optional_zone' for x in c['steps']));self.assertFalse(any(x['op']=='choose_zone_card' for x in c['steps']))
  self.assertFalse(any(x['op'].startswith('choose_') for x in self.case('Ironclad Slayer','wrong_type_0')['steps']))
 def test_two_card_entry_positive_selects_both_and_land_boundary_is_really_one(self):
  for name in ('Tilling Treefolk','Baloth Null'):
   for owner in (0,1):self.assertEqual(sum(x['op']=='choose_zone_card' for x in self.case(name,f'effect_{owner}')['steps']),2)
  c=self.case('Tilling Treefolk','one_available_1');p=self.d('Tilling Treefolk')['params']
  self.assertEqual(sum(x['zone']=='graveyard' and x['player']==1 and f.wave6_eligible(self.o[x['name']],p) for x in c['setup']),1)
  self.assertIn(dict(op='choose_optional_zone',player=1,value=self.rows['Forest']['csvid']),c['steps'])
  c=self.case('Restock','one_available_0');self.assertEqual(sum(x['zone']=='graveyard' and x['player']==0 for x in c['setup']),1)
  self.assertIn(dict(op='require_can_cast',ref='source',value=0),c['steps'])
 def test_normal_speed_menu_is_human_only_after_affording_base_and_two_extra(self):
  self.assertIn(dict(op='choose_dialog',player=0,value=0),self.case("Twilight's Call")['steps'])
  for v in ('payment_0','effect_1'):self.assertFalse(any(s['op']=='choose_dialog' for s in self.case("Twilight's Call",v)['steps']))
  s=self.case("Twilight's Call",'countered_0')['steps'];self.assertLess(next(i for i,x in enumerate(s) if x['op']=='choose_dialog'),next(i for i,x in enumerate(s) if x['op']=='choose_priority'))
 def test_death_is_real_and_other_source_events_do_not_recover(self):
  for owner in (0,1):
   s=self.case('Myr Retriever',f'effect_{owner}')['steps'];self.assertLess(next(i for i,x in enumerate(s) if x['op']=='cast'),next(i for i,x in enumerate(s) if x['op']=='destroy'))
   for mode in ('cast_only','source_only','bounce_source','exile_source','other_death'):
    s=self.case('Myr Retriever',f'{mode}_{owner}')['steps'];self.assertFalse(any(x['op']=='choose_zone_card' for x in s))
 def test_roar_initial_graves_are_asymmetric_and_return_to_each_owner(self):
  for owner in (0,1):
   c=self.case('Roar of Reclamation',f'effect_{owner}');graves=[[x['name'] for x in c['setup'] if x['zone']=='graveyard' and x['player']==pl] for pl in (0,1)]
   self.assertNotEqual(graves[0],graves[1])
   for pl,names in enumerate(graves):
    for name in names:
     if 'Artifact' in self.o[name]['type_line']:
      self.assertTrue(any(s['op']=='bind_permanent_copy' and s['player']==pl and s['value']==self.rows[name]['csvid'] for s in c['steps']))
 def test_actual_response_precedes_parent_completion_and_preserves_other_target(self):
  c=self.case('Dutiful Return','partial_loss_1');s=c['steps'];r=next(i for i,x in enumerate(s) if x['op']=='resolve_response')
  self.assertEqual(s[r+1],dict(op='assert_stack',value=0));self.assertFalse(any(x['op']=='resolve' for x in s[r+1:]))
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=self.rows['Hill Giant']['csvid'],value=1),s)
 def test_every_public_case_has_valid_actions_observations_and_unique_refs(self):
  import generate_cases as g
  from recipes import expand
  count=0
  for name,profile in self.pins['profiles'].items():
   d=self.d(name);p=f.wave6_spec(d,self.o[name],d['family']);d['variants']=f.wave6_variants(p)
   for c in expand(name,self.rows[name]['csvid'],d,self.o[name],self.rows,oracle_context=self.o):
    count+=1;refs=[x['ref'] for x in c['setup']];self.assertEqual(len(refs),len(set(refs)))
    for s in c['steps']:
     self.assertIn(s['op'],g.OPS);self.assertIn(s.get('property','power'),g.PROPERTIES)
     if s['op'] in ('bind_permanent','bind_permanent_copy','bind_hand'):refs.append(s['ref'])
     if s['op'] in g.REF_OPS:self.assertIn(s['ref'],refs)
  self.assertEqual(count,2027)
 def test_activation_response_uses_real_normal_cast_above_pending_ability(self):
  for name,mode in [('Undertaker','lost_target'),('Font of Return','partial_target_loss'),('Crystal Chimes','grave_changed'),('Restoration Specialist','partial_target_loss')]:
   for owner in (0,1):
    s=self.case(name,f'{mode}_{owner}')['steps'];i=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='actual_response')
    self.assertEqual(s[i+1],dict(op='assert_stack',value=2));self.assertEqual(s[i+2],dict(op='resolve'));self.assertEqual(s[i+3],dict(op='assert_stack',value=0))
    self.assertFalse(any(x['op']=='cast_response' for x in s))
 def test_activation_response_history_is_real_and_target_loss_goes_to_library(self):
  for owner in (0,1):
   c=self.case('Undertaker',f'lost_target_{owner}');s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');self.assertTrue(any(x['op']=='destroy' for x in s[:a]));self.assertTrue(any(x['op']=='cast' for x in s[:a]))
   self.assertIn(dict(op='assert_zone_card',player=owner,zone='library',index=0,value=self.rows['Grizzly Bears']['csvid']),s)
 def test_borrowed_surviving_activated_source_bounces_to_original_owner(self):
  for name in ('Tortured Existence','Undertaker','Malevolent Awakening'):
   for owner in (0,1):
    c=self.case(name,f'source_bounced_pending_{owner}');self.assertEqual(next(x for x in c['setup'] if x['ref']=='source')['state'],['opponent_owned'])
    self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='hand',index=self.rows[name]['csvid'],value=1),c['steps'])
 def test_untargeted_all_enchantment_recovery_recomputes_after_grave_change(self):
  c=self.case('Crystal Chimes','grave_changed_1');s=c['steps'];self.assertIn(dict(op='assert_zone_card',player=1,zone='library',index=0,value=self.rows['Seal of Cleansing']['csvid']),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=self.rows['Seal of Cleansing']['csvid'],value=0),s)
 def test_lost_selected_target_does_not_retarget_same_name_sacrificed_cost(self):
  for owner in (0,1):
   s=self.case('Malevolent Awakening',f'lost_target_{owner}')['steps']
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=1),s)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Grizzly Bears']['csvid'],value=0),s)
 def test_extended_spell_zero_still_draws_and_target_loss_suppresses_draw(self):
  self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=3),self.case('Urborg Uprising','zero_0')['steps'])
  self.assertIn(dict(op='assert_zone',player=1,zone='library',value=5),self.case('Urborg Uprising','all_lost_1')['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=self.rows['Grizzly Bears']['csvid'],value=1),self.case('Urborg Uprising','partial_loss_1')['steps'])
 def test_extended_spell_discard_is_mandatory_but_all_lost_plan_stays_unused(self):
  self.assertIn(dict(op='choose_zone_card',player=0,value=self.rows['Grizzly Bears']['csvid']),self.case('Macabre Waltz','discard_returned_0')['steps'])
  for name in ('Macabre Waltz','Shreds of Sanity'):
   self.assertIn(dict(op='assert_unused_choices',value=1),self.case(name,'all_lost_1')['steps'])
 def test_extended_spell_ai_discard_observes_conservation_without_guessed_identity(self):
  s=self.case('Shreds of Sanity','effect_1')['steps']
  for name,total in [('Plains',2),('Giant Growth',1),('Divination',1)]:self.assertIn(dict(op='assert_hand_graveyard_total',player=1,index=self.rows[name]['csvid'],value=total),s)
 def test_extended_spell_x_cost_and_life_derive_from_printed_symbols(self):
  for owner in (0,1):
   self.assertIn(dict(op='assert_life',player=owner,value=18),self.case('Shattered Crypt',f'effect_{owner}')['steps'])
   self.assertIn(dict(op='assert_life',player=owner,value=20),self.case('Shattered Crypt',f'all_lost_{owner}')['steps'])
  self.assertIn(dict(op='choose_x',player=0,value=2),self.case('Wildest Dreams','effect_0')['steps'])
  self.assertIn(dict(op='assert_mana_total',player=0,value=55),self.case('Wildest Dreams','effect_0')['steps'])
 def test_extended_spell_instant_sorcery_union_excludes_creature_and_land(self):
  p=self.d('Pull from the Deep')['params']
  for name in ('Giant Growth','Divination'):self.assertTrue(f.wave6_eligible(self.o[name],p))
  for name in ('Grizzly Bears','Forest'):self.assertFalse(f.wave6_eligible(self.o[name],p))
 def test_activation_printed_cost_is_paid_before_target_recovery(self):
  for name in ('Font of Return','Crystal Chimes','Tortured Existence','Undertaker','Malevolent Awakening','Restoration Specialist'):
   for owner in (0,1):
    c=self.case(name,f'payment_{owner}');s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');r=next(i for i,x in enumerate(s) if x['op']=='resolve');self.assertLess(a,r)
    self.assertIn(dict(op='assert_mana_total',player=owner,value=0),s[a:r])
    if self.pins['profiles'][name]['params']['activation']['self_sacrifice']:
     self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[name]['csvid'],value=1),s[a:r])
 def test_activated_source_has_real_cast_and_summoning_sickness_controls(self):
  for owner in (0,1):
   s=self.case('Undertaker',f'fresh_activation_{owner}')['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s);self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
   s=self.case('Restoration Specialist',f'fresh_activation_{owner}')['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s);self.assertIn(dict(op='require_can_activate',ref='source',value=1),s)
   s=self.case('Crystal Chimes',f'fresh_activation_{owner}')['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=1),s)
 def test_ai_discard_identity_is_uniform_and_uses_actual_acknowledgement(self):
  c=self.case('Undertaker','effect_1');self.assertEqual([x['name'] for x in c['setup'] if x['zone']=='hand' and x['player']==1],['Grizzly Bears','Grizzly Bears'])
  self.assertIn(dict(op='choose_dialog',player=1,value=0),c['steps']);self.assertEqual(sum(s['op']=='choose_zone_card' for s in c['steps']),1)
  self.assertFalse(any(s['op']=='choose_dialog' for s in self.case('Tortured Existence','effect_1')['steps']))
 def test_cost_creature_control_and_original_owner_are_separate(self):
  for owner in (0,1):
   c=self.case('Malevolent Awakening',f'stolen_cost_{owner}');cost=next(x for x in c['setup'] if x['ref']=='cost_creature_0');self.assertEqual(cost['player'],owner);self.assertEqual(cost['state'],['opponent_owned'])
   source_gy=[s for s in c['steps'] if s['op']=='assert_zone_multiplicity' and s['player']==1-owner and s.get('zone')=='graveyard' and s['index']==self.rows['Grizzly Bears']['csvid']]
   self.assertTrue(any(s['value']==2 for s in source_gy));self.assertIn(dict(op='require_can_activate',ref='source',value=0),self.case('Malevolent Awakening',f'opponent_cost_{owner}')['steps'])
 def test_ai_sacrifice_choice_uses_native_policy_without_a_nonexistent_prompt(self):
  self.assertTrue(any(x['op']=='choose_target' for x in self.case('Malevolent Awakening','effect_0')['steps']))
  self.assertFalse(any(x['op']=='choose_target' for x in self.case('Malevolent Awakening','effect_1')['steps']))
 def test_borrowed_source_sacrifice_goes_to_original_owner_grave(self):
  for name in ('Font of Return','Crystal Chimes','Restoration Specialist'):
   for owner in (0,1):
    c=self.case(name,f'stolen_source_{owner}');source=next(x for x in c['setup'] if x['ref']=='source');self.assertEqual(source['player'],owner);self.assertEqual(source['state'],['opponent_owned'])
    self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows[name]['csvid'],value=1),c['steps'])
 def test_targetless_zero_and_all_recovery_still_pay_sacrifice_costs(self):
  for name,v in [('Font of Return','zero_0'),('Crystal Chimes','empty_1'),('Restoration Specialist','zero_0')]:
   s=self.case(name,v)['steps'];self.assertIn(dict(op='activate',ref='source',value=1),s);self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x.get('index')==self.rows[name]['csvid'] and x.get('zone')=='graveyard' and x['value']==1 for x in s))
 def test_actual_grave_selector_offers_creatures_but_not_kindred_instant(self):
  s=self.case('Font of Return','predicate_offer_1')['steps'];self.assertIn(dict(op='observe_search_filters',value=1),s)
  for n,value in [('Grizzly Bears',1),('Ornithopter',1),('Nameless Inversion',0),('Jace Beleren',0)]:self.assertIn(dict(op='assert_offered_legal_card',index=self.rows[n]['csvid'],value=value),s)
  self.assertIn(dict(op='assert_offered_selection_required',value=0),s)
 def test_paired_activation_has_independent_optional_type_choices(self):
  s=self.case('Restoration Specialist')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');self.assertEqual(sum(x['op']=='choose_zone_card' for x in s[:a]),2)
  s=self.case('Restoration Specialist','zero_1')['steps'];self.assertEqual(sum(x['op']=='choose_optional_zone' for x in s),2)
  for v in ('artifact_only_0','enchantment_only_1'):
   s=self.case('Restoration Specialist',v)['steps'];self.assertEqual(sum(x['op']=='choose_zone_card' for x in s),1);self.assertEqual(sum(x['op']=='choose_optional_zone' for x in s),1)

class RecoveryRecipeDeterminismTest(unittest.TestCase):
 def test_receipt_order_is_stable_across_python_hash_seeds(self):
  import os,subprocess,sys
  program="import hashlib,json;from test_graveyard_recovery_extension_recipes import Wave6RecoveryTest as T;T.setUpClass();t=T();print(hashlib.sha256(json.dumps([t.case('Font of Return','alternate_0'),t.case('Shreds of Sanity','pending_snapshot_1')],sort_keys=True,ensure_ascii=False).encode('utf8')).hexdigest())"
  outputs=[]
  for seed in ('1','2','7'):
   result=subprocess.run([sys.executable,'-c',program],cwd=HERE,env=dict(os.environ,PYTHONHASHSEED=seed,PYTHONUTF8='1'),capture_output=True,text=True,encoding='utf8',check=True)
   value=result.stdout.strip();self.assertRegex(value,r'^[0-9a-f]{64}$');outputs.append(value)
  self.assertEqual(len(set(outputs)),1)

if __name__=='__main__':unittest.main()
