"""Independent Oracle pin, semantic rejection, and complete generated receipts."""
import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
import growth_counter_activation_recipes as growth

ROOT=Path(__file__).parent


class GrowthCounterActivationTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((ROOT/'data/oracle.json').read_text(encoding='utf8'))['cards']
  cls.rows={r['name']:r for r in registry()}

 def declaration(self,name='Chronomaton',**updates):
  p=dict(mechanic='ordinary',amount=1,mana_cost='{1}',tap=True,sacrifice='none',pay_life=0,gain_life=0,scope='self',keywords=[],entry=0,inactive=[]);p.update(updates)
  return dict(family=growth.FAMILY,version=2,params=p,oracle_text=self.o[name]['oracle_text'],variants=['effect_0'])

 def cases(self,n,d,vs):
  p=growth.spec(d,self.o[n]);return growth.expand(n,self.rows[n]['csvid'],d,p,vs,self.rows,self.o)

 def test_parameters_whole_pin_and_types_fail_closed(self):
  d=self.declaration()
  for changed in (dict(params={}),dict(oracle_text='changed'),dict(params=dict(d['params'],amount=True)),dict(params=dict(d['params'],tap=1)),dict(params=dict(d['params'],extra=0))):
   with self.assertRaises(ValueError):growth.spec(dict(d,**changed),self.o['Chronomaton'])
  for changed in (dict(layout='transform'),dict(card_faces=[{}]),dict(power='*'),dict(toughness='0'),dict(mana_cost='{X}')):
   with self.assertRaises(ValueError):growth.spec(d,dict(self.o['Chronomaton'],**changed))

 def test_wrong_amount_cost_target_or_passive_rejected(self):
  for fields in (dict(amount=2),dict(mana_cost='{2}'),dict(scope='target'),dict(keywords=['Flying']),dict(inactive=['Invented clause'])):
   with self.assertRaises(ValueError):growth.spec(self.declaration(**fields),self.o['Chronomaton'])
  r=dict(self.o['Chronomaton'],oracle_text=self.o['Chronomaton']['oracle_text']+'\nWhenever this creature attacks, draw a card.')
  with self.assertRaises(ValueError):growth.spec(dict(self.declaration(),oracle_text=r['oracle_text']),r)

 def test_witness_pins_cover_stats_rules_types_and_costs(self):
  growth.validate_witnesses(self.o,self.rows)
  for n,k,v in [('Grizzly Bears','power','3'),('Unsummon','mana_cost','{B}'),('Hunger of the Howlpack','oracle_text','Changed'),('Ornithopter','type_line','Creature')]:
   o=copy.deepcopy(self.o);o[n][k]=v
   with self.assertRaises(ValueError):growth.validate_witnesses(o,self.rows)

 def test_cost_paid_before_persistent_effect_and_cleanup(self):
  for c in self.cases('Chronomaton',self.declaration(),['payment_0','cleanup_1']):
   s=c['steps'];activation=next(i for i,x in enumerate(s) if x['op']=='activate');resolution=next(i for i,x in enumerate(s) if x['op']=='resolve')
   self.assertLess(activation,resolution)
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s[activation:resolution])
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s[resolution:])
   self.assertIn(dict(op='assert_card',ref='source',property='power',value=2),s[resolution:])
   self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s)
   self.assertEqual(c['_recipe']['untested_abilities'],[])

 def test_both_controllers_cast_and_check_exact_payment(self):
  for owner,c in enumerate(self.cases('Chronomaton',self.declaration(),['cast_payment_0','cast_payment_1'])):
   self.assertIn(dict(op='cast',ref='source',value=1),c['steps'])
   for color in range(6):self.assertIn(dict(op='assert_mana',player=owner,color=color,value=0),c['steps'])
   self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='source'),owner)

 def test_tapped_fresh_and_resource_negatives(self):
  for v in ('tapped_source_0','sick_source_1','fresh_activation_0','insufficient_mana_1'):
   s=self.cases('Chronomaton',self.declaration(),[v])[0]['steps']
   self.assertIn(dict(op='activate',ref='source',value=0),s)
   self.assertNotIn(dict(op='activate',ref='source',value=1),s)

 def test_sorcery_growth_rejects_opposing_turn_combat_and_stack(self):
  d=self.declaration('Disowned Ancestor',mechanic='outlast',mana_cost='{1}{B}')
  for c in self.cases('Disowned Ancestor',d,['opposing_turn_0','combat_timing_1','stack_timing_0']):
   self.assertIn(dict(op='activate',ref='source',value=0),c['steps'])
  s=self.cases('Disowned Ancestor',d,['stack_timing_0'])[0]['steps']
  self.assertIn(dict(op='cast',ref='pending',value=1),s)
  self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=5),s)

 def test_team_counters_include_both_own_artifacts_and_exclude_others(self):
  d=self.declaration('Steel Overseer',mana_cost='',scope='artifact_team')
  s=self.cases('Steel Overseer',d,['effect_1'])[0]['steps']
  for ref,n in [('source',1),('artifact',1),('artifact_second',1),('enemy_artifact',0),('recipient',0),('control',0)]:
   self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=n),s)

 def test_sacrifice_cost_life_gain_and_missing_fodder(self):
  d=self.declaration('Blood Host',mana_cost='{1}{B}',tap=False,sacrifice='another creature',gain_life=2)
  c=self.cases('Blood Host',d,['effect_1'])[0];s=c['steps']
  self.assertNotIn(dict(op='choose_target',player=1,ref='fodder'),s)
  self.assertIn(dict(op='assert_life',player=1,value=22),s)
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Ornithopter']['csvid'],value=1),s)
  for c in self.cases('Blood Host',d,['no_fodder_0','enemy_fodder_1']):
   self.assertIn(dict(op='activate',ref='source',value=0),c['steps'])
   self.assertFalse(any(x['name']=='Ornithopter' and x['player']==next(y['player'] for y in c['setup'] if y['ref']=='source') for x in c['setup']))

 def test_ai_sacrifice_has_no_queued_ui_choice_and_only_declared_fodder(self):
  d=self.declaration('Bloodflow Connoisseur',mana_cost='',tap=False,sacrifice='creature')
  for v,refs in [('effect_1',{'source','fodder'}),('repeat_1',{'source','fodder','fodder_second'}),('sacrifice_source_1',{'source'})]:
   c=self.cases('Bloodflow Connoisseur',d,[v])[0]
   self.assertEqual({a['ref'] for a in c['setup'] if a['zone']=='battlefield' and a['player']==1},refs)
   self.assertFalse(any(s['op']=='choose_target' and s.get('ref') in ('source','fodder','fodder_second') for s in c['steps']))
   for a in c['setup']:
    if a['ref'].startswith('fodder'):self.assertEqual(a['name'],'Ornithopter')

 def test_noncreature_source_bounce_uses_permanent_spell(self):
  d=self.declaration('Dragon Blood',mana_cost='{3}',scope='target')
  c=self.cases('Dragon Blood',d,['source_bounced_pending_1'])[0]
  self.assertEqual(next(a['name'] for a in c['setup'] if a['ref']=='response'),'Boomerang')
  self.assertIn(dict(op='assert_stack',value=2),c['steps'])

 def test_insufficient_life_keeps_counters_and_life_unchanged(self):
  d=self.declaration('Unspeakable Symbol',mana_cost='',tap=False,pay_life=3,scope='target')
  self.assertIn('insufficient_life_1',growth.variants(growth.spec(d,self.o['Unspeakable Symbol'])))
  for owner,c in enumerate(self.cases('Unspeakable Symbol',d,['insufficient_life_0','insufficient_life_1'])):
   self.assertIn(dict(op='set_life',player=owner,value=2),c['steps'])
   self.assertIn(dict(op='activate',ref='source',value=0),c['steps'])
   self.assertIn(dict(op='assert_life',player=owner,value=2),c['steps'])
   self.assertIn(dict(op='assert_card',ref='recipient',property='plus1_counters',value=0),c['steps'])

 def test_sorcery_ability_cannot_stack_a_second_activation(self):
  d=self.declaration('Voracious Null',mana_cost='{1}{B}',tap=False,sacrifice='another creature',amount=2)
  s=self.cases('Voracious Null',d,['stacked_activations_0'])[0]['steps']
  self.assertEqual(sum(a['op']=='activate' and a['value']==1 for a in s),1)
  self.assertIn(dict(op='activate',ref='source',value=0),s)
  self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=2),s)

 def test_adapt_repetition_and_preexisting_counters(self):
  d=self.declaration('Skitter Eel',mechanic='adapt',amount=2,mana_cost='{2}{U}',tap=False)
  for v,n in [('repeat_0',2),('stacked_activations_1',2),('preexisting_counter_0',1),('bounce_recast_1',2)]:
   s=self.cases('Skitter Eel',d,[v])[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=n),s)
   if v.startswith('stacked'):self.assertIn(dict(op='assert_stack',value=2),s)

 def test_monstrosity_preexisting_counter_is_not_monstrous(self):
  d=self.declaration('Gluttonous Cyclops',mechanic='monstrosity',amount=3,mana_cost='{5}{R}{R}',tap=False)
  for v,n in [('repeat_0',3),('preexisting_counter_1',4)]:self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=n),self.cases('Gluttonous Cyclops',d,[v])[0]['steps'])

 def test_recast_rebinds_hand_after_bounce_and_resets_counters(self):
  d=self.declaration('Jungle Delver',mana_cost='{3}{G}',tap=False)
  s=self.cases('Jungle Delver',d,['bounce_recast_1'])[0]['steps']
  rebound=next(i for i,a in enumerate(s) if a['op']=='bind_hand')
  self.assertEqual(s[rebound],dict(op='bind_hand',player=1,value=self.rows['Jungle Delver']['csvid'],ref='source'))
  self.assertEqual(s[rebound+1],dict(op='cast',ref='source',value=1))
  self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s)

 def test_bare_zones_are_empty_and_populated_zones_are_exact(self):
  for mode,size in [('bare_zones_0',0),('effect_1',7)]:
   s=self.cases('Chronomaton',self.declaration(),[mode])[0]['steps']
   for player in (0,1):self.assertIn(dict(op='assert_zone',player=player,zone='library',value=size),s)

 def test_snapshots_capture_pending_effect_and_replay(self):
  for c in self.cases('Chronomaton',self.declaration(),['ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0']):
   ops=[s['op'] for s in c['steps']]
   self.assertIn('capture',ops);self.assertIn('ai_roundtrip',ops);self.assertIn('restore',ops);self.assertEqual(ops[-1],'assert_recorded_state')

 def test_target_departure_and_source_sacrifice(self):
  d=self.declaration('Shambling Shell',mana_cost='',tap=False,sacrifice='self',scope='target',inactive=growth.PINS['inactive']['Shambling Shell'])
  for c in self.cases('Shambling Shell',d,['effect_0','target_bounced_1','target_destroyed_0']):
   self.assertIn(dict(op='activate',ref='source',value=1),c['steps'])
   self.assertIn(dict(op='assert_zone_multiplicity',player=int(c['_recipe']['variant'][-1]),zone='graveyard',index=self.rows['Shambling Shell']['csvid'],value=1),c['steps'])


class GrowthCounterExtensionTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((ROOT/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

 def declaration(self,n):return dict(family=growth.FAMILY,version=2,params=copy.deepcopy(growth.PINS['profiles'][n]['params']),oracle_text=self.o[n]['oracle_text'],variants=['effect_0'])

 def cases(self,n,selected):
  d=self.declaration(n);p=growth.spec(d,self.o[n]);return growth.expand(n,self.rows[n]['csvid'],d,p,selected,self.rows,self.o)

 def test_source_identity_profile_and_all_parameters_are_pinned(self):
  n='Consumptive Goo';d=self.declaration(n)
  for change in (dict(power='2'),dict(oracle_id='changed'),dict(oracle_text='changed'),dict(mana_cost='{B}'),dict(layout='token')):
   with self.assertRaises(ValueError):growth.spec(d,dict(self.o[n],**change))
  bad=copy.deepcopy(d);bad['params']['amount']=True
  with self.assertRaises(ValueError):growth.spec(bad,self.o[n])
  bad=copy.deepcopy(d);bad['params']['extras']=[]
  with self.assertRaises(ValueError):growth.spec(bad,self.o[n])

 def test_reinforce_discards_source_and_resolves_real_counters(self):
  for owner,c in enumerate(self.cases('Mosquito Guard',['effect_0','effect_1'])):
   s=c['steps'];self.assertIn(dict(op='activate_hand',ref='source',value=1),s)
   self.assertIn(dict(op='choose_target',player=owner,ref='recipient'),s)
   self.assertIn(dict(op='assert_card',ref='recipient',property='plus1_counters',value=1),s)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Mosquito Guard']['csvid'],value=1),s)

 def test_reinforce_denials_query_whole_hand_ability_with_no_other_creatures(self):
  for c in self.cases('Mosquito Guard',['no_creatures_0','shroud_only_1']):
   self.assertIn(dict(op='require_can_activate_hand',ref='source',value=0),c['steps'])
   self.assertFalse(any(x['zone']=='battlefield' and x['name']=='Scaled Wurm' for x in c['setup']))

 def test_human_sacrifice_keeps_nonsacrificed_permanents_and_grows_target(self):
  s=self.cases('Deranged Outcast',['effect_0'])[0]['steps']
  self.assertLess(s.index(dict(op='choose_target',player=0,ref='recipient')),s.index(dict(op='choose_target',player=0,ref='fodder')))
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Eager Cadet']['csvid'],value=1),s)
  self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=4),s)

 def test_companion_target_shrink_and_growth_are_both_checked(self):
  s=self.cases('Consumptive Goo',['cleanup_1'])[0]['steps'];at=next(i for i,a in enumerate(s) if a['op']=='cleanup')
  self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=1),s[:at])
  self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s)
  self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=2),s[at:])

 def test_lost_target_fizzles_growth_and_shrink_together(self):
  s=self.cases('Consumptive Goo',['target_bounced_0'])[0]['steps']
  self.assertIn(dict(op='assert_stack',value=2),s)
  self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s)

 def test_monstrous_damage_counts_actual_opponent_hand(self):
  for owner in (0,1):
   for mode,count in [('effect',4),('empty_opponent_hand',0),('large_opponent_hand',8)]:
    s=self.cases('Stormbreath Dragon',[mode+'_'+str(owner)])[0]['steps']
    self.assertIn(dict(op='assert_life',player=1-owner,value=20-count),s)
    self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=3),s)

 def test_extension_variants_and_snapshots_fail_closed(self):
  for bad in ([],['effect_0','effect_0'],['invented_0']):
   with self.assertRaises(ValueError):self.cases('Consumptive Goo',bad)
  for c in self.cases('Consumptive Goo',['ready_snapshot_0','pending_snapshot_1','resolved_snapshot_0']):
   ops=[x['op'] for x in c['steps']];self.assertIn('ai_roundtrip',ops);self.assertEqual(ops[-1],'assert_recorded_state')

 def test_scavenge_uses_real_graveyard_proxy_and_exile_cost(self):
  for owner,c in enumerate(self.cases('Deadbridge Goliath',['effect_0','effect_1'])):
   s=c['steps'];activate=next(i for i,a in enumerate(s) if a['op']=='activate_graveyard');resolve=next(i for i,a in enumerate(s) if a['op']=='resolve')
   self.assertEqual(s[0],dict(op='prepare_graveyard_abilities'))
   self.assertIn(dict(op='choose_target',player=owner,ref='recipient'),s[:activate])
   self.assertNotIn(dict(op='assert_choices_consumed'),s[activate:resolve])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='exile',index=self.rows['Deadbridge Goliath']['csvid'],value=1),s)
   self.assertIn(dict(op='assert_card',ref='recipient',property='plus1_counters',value=5),s)
   self.assertFalse(any(a['op']=='activate' for a in s))

 def test_scavenge_pending_target_receipt_exposes_timing_without_reselection(self):
  for c in self.cases('Deadbridge Goliath',['targeting_pending_0','targeting_pending_1']):
   s=c['steps'];activation=next(i for i,a in enumerate(s) if a['op']=='activate_graveyard')
   self.assertIn(dict(op='assert_choices_consumed'),s[activation:])
   self.assertNotIn(dict(op='resolve'),s)
   self.assertIn(dict(op='assert_stack',value=1),s)
   self.assertEqual(sum(a['op']=='choose_target' for a in s),1)

 def test_fresh_counter_target_is_actually_cast(self):
  for n in ('Mosquito Guard','Deadbridge Goliath'):
   s=self.cases(n,['fresh_recipient_1'])[0]['steps']
   self.assertIn(dict(op='cast',ref='recipient',value=1),s)
   self.assertIn(dict(op='assert_card',ref='recipient',property='sick',value=1),s)

 def test_helper_query_purity_keeps_real_queued_menu_and_target(self):
  s=self.cases('Deadbridge Goliath',['query_purity_1'])[0]['steps']
  queued=next(i for i,a in enumerate(s) if a['op']=='choose_target');activate=next(i for i,a in enumerate(s) if a['op']=='activate_graveyard')
  self.assertEqual(sum(a['op']=='require_can_activate_graveyard' for a in s[queued:activate]),2)
  self.assertNotIn(dict(op='assert_choices_consumed'),s[queued:activate])
  self.assertIn(dict(op='assert_stack',value=0),s[queued:activate])

 def test_outlast_grant_uses_counter_and_controller_predicates(self):
  for mode,legal in [('grant_source',0),('grant_ally',0),('grant_counterless',1),('grant_opponent',1)]:
   s=self.cases('Abzan Falconer',[mode+'_1'])[0]['steps']
   self.assertIn(dict(op='block',ref='grant_fighter',other='source' if mode=='grant_source' else 'recipient',value=legal),s)
  s=self.cases('Abzan Falconer',['grant_removed_0'])[0]['steps'];restore=next(i for i,a in enumerate(s) if a['op']=='restore')
  self.assertIn(dict(op='block',ref='grant_fighter',other='recipient',value=0),s[:restore])
  self.assertIn(dict(op='block',ref='grant_fighter',other='recipient',value=1),s[restore:])

 def test_outlast_tap_repetition_really_untaps_before_repaying(self):
  s=self.cases('Ainok Bond-Kin',['untap_repeat_1'])[0]['steps']
  self.assertIn(dict(op='activate',ref='untapper',value=1),s)
  self.assertEqual(sum(a['op']=='activate' and a['ref']=='source' and a['value']==1 for a in s),2)
  self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=2),s)

 def test_energy_cost_is_real_entry_resource_and_exhausts(self):
  for owner,c in enumerate(self.cases('Bristling Hydra',['repeat_0','repeat_1'])):
   s=c['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s)
   self.assertIn(dict(op='assert_energy',player=owner,value=0),s)
   self.assertIn(dict(op='activate',ref='source',value=0),s)
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s)
  s=self.cases('Bristling Hydra',['cleanup_0'])[0]['steps'];at=next(i for i,a in enumerate(s) if a['op']=='cleanup')
  self.assertIn(dict(op='can_target_creature',ref='protection_probe',other='source',value=0),s[:at])
  self.assertIn(dict(op='can_target_creature',ref='protection_probe',other='source',value=1),s[at:])

 def test_elf_amount_counts_own_elves_and_changelings_at_resolution(self):
  for mode,amount in [('effect',1),('two_extra_elves',3),('changeling_elf',2),('source_bounced_pending',0)]:
   s=self.cases('Immaculate Magistrate',[mode+'_0'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='recipient',property='plus1_counters',value=amount),s)

 def test_high_sentinels_counts_countered_creatures_not_counter_amount(self):
  for mode,source_power in [('countered_ally',4),('many_counters_one_ally',4),('self_target',4)]:
   s=self.cases('High Sentinels of Arashin',[mode+'_0'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='source',property='power',value=source_power),s)

 def test_monstrous_keyword_checks_have_before_and_after_controls(self):
  s=self.cases('Fleecemane Lion',['monstrous_indestructible_1'])[0]['steps'];restore=next(i for i,a in enumerate(s) if a['op']=='restore')
  self.assertIn(dict(op='require_card',ref='source',property='in_play',value=0),s[:restore])
  self.assertIn(dict(op='require_card',ref='source',property='in_play',value=1),s[restore:])
  s=self.cases('Sinuous Vermin',['monstrous_menace_0'])[0]['steps'];restore=next(i for i,a in enumerate(s) if a['op']=='restore')
  self.assertIn(dict(op='block',ref='menace_one',other='source',value=1),s[:restore])
  self.assertIn(dict(op='block',ref='menace_one',other='source',value=0),s[restore:])

 def test_land_reinforce_has_no_fake_cast_or_battlefield_context(self):
  d=self.declaration('Rustic Clachan');p=growth.spec(d,self.o['Rustic Clachan']);self.assertIsNone(p['mana'])
  for c in growth.expand('Rustic Clachan',self.rows['Rustic Clachan']['csvid'],d,p,growth.variants(p),self.rows,self.o):
   self.assertFalse(any(a['op']=='cast' and a['ref']=='source' for a in c['steps']))
   self.assertEqual(next(x['zone'] for x in c['setup'] if x['ref']=='source'),'hand')

 def test_every_frozen_profile_expands_all_variants(self):
  for name in growth.PINS['profiles']:
   d=self.declaration(name);p=growth.spec(d,self.o[name]);vs=growth.variants(p)
   cases=growth.expand(name,self.rows[name]['csvid'],d,p,vs,self.rows,self.o)
   self.assertEqual(len(cases),len(vs));self.assertEqual(len(set(c['name'] for c in cases)),len(cases))

 def test_kraj_real_counter_mode_is_selected_for_probe_and_activation(self):
  for owner,c in enumerate(self.cases('Experiment Kraj',['effect_0','effect_1'])):
   s=c['steps'];at=next(i for i,a in enumerate(s) if a['op']=='activate' and a.get('ref')=='source')
   self.assertIn(dict(op='probe_activation_target',ref='source',other='recipient',index=0,value=1),s[:at])
   self.assertEqual(s[at-2:at],[dict(op='choose_dialog',player=owner,value=0),dict(op='choose_target',player=owner,ref='recipient')])
  s=self.cases('Experiment Kraj',['shroud_target_0'])[0]['steps']
  self.assertIn(dict(op='probe_activation_target',ref='source',other='recipient',index=0,value=0),s)

 def test_monstrous_destroy_has_gorgon_and_indestructible_survivor_controls(self):
  for owner,c in enumerate(self.cases('Hythonia the Cruel',['effect_0','effect_1'])):
   s=c['steps']
   for pl in (0,1):
    self.assertIn(dict(op='assert_card',ref='gorgon_'+str(pl),property='in_play',value=1),s)
    self.assertIn(dict(op='assert_card',ref='indestructible_'+str(pl),property='in_play',value=1),s)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2),s)
   self.assertFalse(any(a['op']=='destroy' for a in s))

 def test_monstrous_exile_includes_enchantments_both_players_and_indestructible_artifacts(self):
  for c in self.cases("Kalemne's Captain",['effect_0','effect_1']):
   for pl in (0,1):
    for n in ('Ivory Mask','Darksteel Myr'):
     self.assertIn(dict(op='assert_zone_multiplicity',player=pl,zone='exile',index=self.rows[n]['csvid'],value=1),c['steps'])
   self.assertFalse(any(a['op']=='destroy' for a in c['steps']))

 def test_monstrous_damage_separates_enemy_death_survival_and_friendly_control(self):
  for owner,c in enumerate(self.cases('Wildfire Cerberus',['effect_0','effect_1'])):
   s=c['steps']
   self.assertIn(dict(op='assert_life',player=owner,value=20),s)
   self.assertIn(dict(op='assert_life',player=1-owner,value=18),s)
   self.assertIn(dict(op='assert_card',ref='unselected',property='damage',value=2),s)
   self.assertIn(dict(op='assert_card',ref='friendly_creature',property='in_play',value=1),s)
   self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2),s)
  s=self.cases('Wildfire Cerberus',['cleanup_0'])[0]['steps'];cleanup=next(i for i,a in enumerate(s) if a['op']=='cleanup')
  self.assertIn(dict(op='assert_card',ref='unselected',property='damage',value=2),s[:cleanup])
  self.assertNotIn(dict(op='assert_card',ref='unselected',property='damage',value=2),s[cleanup:])

 def test_cruel_sadist_life_is_a_real_cost_and_second_growth_selects_counter_mode(self):
  for owner,c in enumerate(self.cases('Cruel Sadist',['untap_repeat_0','untap_repeat_1'])):
   s=c['steps'];self.assertIn(dict(op='assert_life',player=owner,value=18),s)
   self.assertEqual(sum(a['op']=='choose_dialog' for a in s),1)
   self.assertIn(dict(op='choose_dialog',player=owner,value=0),s)
  s=self.cases('Cruel Sadist',['insufficient_life_1'])[0]['steps']
  self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
  self.assertNotIn(dict(op='activate',ref='source',value=1),s)

 def test_incubation_adapt_menu_pin_is_owned_param_and_has_no_land_mana_path(self):
  from recipes import expand as public_expand
  n='Incubation Druid';d=self.declaration(n);p=growth.spec(d,self.o[n]);d['variants']=growth.variants(p)
  cases=public_expand(n,self.rows[n]['csvid'],d,self.o[n],self.rows,self.o)
  for c in cases:
   owner=int(c['_recipe']['variant'][-1])
   self.assertFalse(any(x['player']==owner and x['zone']=='battlefield' and 'Land' in self.o[x['name']]['type_line'] for x in c['setup']))
  s=self.cases(n,['effect_0'])[0]['steps'];self.assertIn(dict(op='choose_dialog',player=0,value=1),s)
  s=self.cases(n,['effect_1'])[0]['steps'];self.assertFalse(any(a['op']=='choose_dialog' for a in s))
  for c in self.cases(n,['insufficient_mana_0','wrong_color_1']):
   self.assertIn('tapped',next(x['state'] for x in c['setup'] if x['ref']=='source'))
   self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])

 def test_public_recipe_boundary_accepts_every_owned_profile(self):
  from recipes import expand as public_expand
  for n in growth.PINS['profiles']:
   d=self.declaration(n);p=growth.spec(d,self.o[n]);d['variants']=growth.variants(p)
   self.assertEqual(len(public_expand(n,self.rows[n]['csvid'],d,self.o[n],self.rows,self.o)),len(d['variants']))

 def test_targeted_monster_selects_trigger_target_after_untargeted_activation(self):
  for n in ('Arbor Colossus','Keepsake Gorgon'):
   for owner,c in enumerate(self.cases(n,['effect_0','effect_1'])):
    s=c['steps'];activate=next(i for i,a in enumerate(s) if a['op']=='activate');target=next(i for i,a in enumerate(s) if a['op']=='choose_target');resolve=next(i for i,a in enumerate(s) if a['op']=='resolve')
    self.assertLess(activate,target);self.assertLess(target,resolve)
    self.assertEqual(s[target],dict(op='choose_target',player=owner,ref='monster_victim'))
    self.assertFalse(any(a['op']=='probe_activation_target' for a in s))
    self.assertIn(dict(op='assert_card',ref='friendly_matching',property='in_play',value=1),s)
    self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Suntail Hawk' if n=='Arbor Colossus' else 'Hill Giant']['csvid'],value=1),s)

 def test_targeted_monster_no_eligible_and_protected_boards_need_no_target_input(self):
  for n in ('Arbor Colossus','Keepsake Gorgon'):
   for c in self.cases(n,['trigger_no_targets_0','trigger_shroud_only_1','trigger_hexproof_only_0']):
    self.assertFalse(any(a['op']=='choose_target' for a in c['steps']))
    self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=3 if n=='Arbor Colossus' else 1),c['steps'])
   for owner,c in enumerate(self.cases(n,['trigger_indestructible_0','trigger_indestructible_1'])):
    self.assertIn(dict(op='choose_target',player=owner,ref='monster_victim'),c['steps'])
    self.assertIn(dict(op='assert_card',ref='monster_victim',property='in_play',value=1),c['steps'])

 def test_keepsake_reentry_selects_a_second_remaining_legal_creature(self):
  for owner,c in enumerate(self.cases('Keepsake Gorgon',['bounce_recast_0','bounce_recast_1'])):
   self.assertEqual([a['ref'] for a in c['steps'] if a['op']=='choose_target' and a['ref']!='source'],['monster_victim','recipient'])
   self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=2),c['steps'])
  s=self.cases('Arbor Colossus',['bounce_recast_0'])[0]['steps']
  self.assertEqual(sum(a['op']=='choose_target' and a['ref']!='source' for a in s),1)

 def test_discount_counts_own_matching_grave_cards_and_preserves_colored_cost(self):
  for n,color,colored in [('Nemesis of Mortals',3,18),('Pteramander',2,19)]:
   for owner in (0,1):
    for mode,generic in [('effect',34),('discount_empty',33),('discount_nonmatching',33),('discount_opponent',33),('discount_mixed',36),('discount_zero_generic',40)]:
     s=self.cases(n,[mode+'_'+str(owner)])[0]['steps']
     self.assertIn(dict(op='assert_mana',player=owner,color=0,value=generic),s)
     self.assertIn(dict(op='assert_mana',player=owner,color=color,value=colored),s)

 def test_excess_discount_has_exact_colored_payment_and_asserts_x_sentinel_unconsumed(self):
  for n in ('Nemesis of Mortals','Pteramander'):
   for owner,c in enumerate(self.cases(n,['discount_excess_0','discount_excess_1'])):
    s=c['steps'];activate=next(i for i,a in enumerate(s) if a['op']=='activate')
    self.assertEqual(s[activate-1],dict(op='choose_x',player=owner,value=0))
    self.assertEqual(s[activate+1],dict(op='assert_unused_choices',value=1))
    for color in range(6):self.assertIn(dict(op='assert_mana',player=owner,color=color,value=0),s[activate:])
    self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=5 if n=='Nemesis of Mortals' else 4),s)

 def test_discount_is_recomputed_after_real_spells_change_graveyard(self):
  s=self.cases('Pteramander',['bounce_recast_0'])[0]['steps']
  self.assertIn(dict(op='cast',ref='bounce',value=1),s)
  self.assertIn(dict(op='assert_mana',player=0,color=0,value=29),s)
  self.assertIn(dict(op='assert_mana',player=0,color=2,value=16),s)
  s=self.cases('Nemesis of Mortals',['cast_discount_zero_generic_1'])[0]['steps']
  self.assertIn(dict(op='cast',ref='source',value=1),s)
  self.assertFalse(any(a['op']=='activate' for a in s))
  for color in range(6):self.assertIn(dict(op='assert_mana',player=1,color=color,value=0),s)

 def test_fixed_discounted_ability_does_not_consume_nonzero_x_or_extra_mana(self):
  for n in ('Nemesis of Mortals','Pteramander'):
   for owner,c in enumerate(self.cases(n,['discount_excess_overpay_0','discount_excess_overpay_1'])):
    s=c['steps'];activate=next(i for i,a in enumerate(s) if a['op']=='activate')
    self.assertEqual(s[activate-1],dict(op='choose_x',player=owner,value=3))
    self.assertEqual(s[activate+1],dict(op='assert_unused_choices',value=1))
    self.assertIn(dict(op='assert_mana',player=owner,color=0,value=3),s[activate:])

 def test_colossus_requires_monstrosity_to_attack_and_deals_actual_trample_excess(self):
  for owner,c in enumerate(self.cases('Colossus of Akros',['monstrous_trample_0','monstrous_trample_cleanup_1'])):
   s=c['steps'];restore=next(i for i,a in enumerate(s) if a['op']=='restore')
   self.assertIn(dict(op='attack',ref='source',value=0),s[:restore])
   self.assertIn(dict(op='attack',ref='source',value=1),s[restore:])
   self.assertIn(dict(op='combat_damage'),s[restore:])
   self.assertIn(dict(op='assert_life',player=1-owner,value=2),s)
  s=self.cases('Colossus of Akros',['source_destroyed_pending_0'])[0]['steps']
  self.assertIn(dict(op='destroy',ref='source'),s)
  self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=10),s)

 def test_swarmborn_counter_alone_lacks_reach_and_player_damage_really_sacrifices(self):
  for owner,c in enumerate(self.cases('Swarmborn Giant',['monstrous_reach_0','monstrous_reach_cleanup_1'])):
   s=c['steps'];restore=next(i for i,a in enumerate(s) if a['op']=='restore')
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s[:restore])
   self.assertIn(dict(op='block',ref='source',other='reach_attacker',value=0),s[:restore])
   self.assertIn(dict(op='require_card',ref='source',property='in_play',value=0),s[:restore])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Swarmborn Giant']['csvid'],value=1),s[:restore])
   self.assertIn(dict(op='assert_life',player=owner,value=19),s[:restore])
   self.assertIn(dict(op='block',ref='source',other='reach_attacker',value=1),s[restore:])
   self.assertIn(dict(op='assert_life',player=owner,value=20),s[restore:])

 def test_stoneshock_real_rule_controls_include_cleanup_controller_and_new_flash_creature(self):
  s=self.cases('Stoneshock Giant',['monstrous_block_cleanup_0'])[0]['steps']
  restores=[i for i,a in enumerate(s) if a['op']=='restore'];self.assertEqual(len(restores),2)
  self.assertIn(dict(op='block',ref='rule_blocker',other='source',value=0),s[restores[0]:restores[1]])
  self.assertIn(dict(op='block',ref='rule_blocker',other='source',value=1),s[restores[1]:])
  s=self.cases('Stoneshock Giant',['monstrous_friendly_block_1'])[0]['steps'];at=next(i for i,a in enumerate(s) if a['op']=='activate')
  self.assertEqual(next(a['player'] for a in reversed(s[:at]) if a['op']=='turn'),0)
  self.assertFalse(any(a['op']=='turn' for a in s[at:]))
  s=self.cases('Stoneshock Giant',['monstrous_fresh_block_0'])[0]['steps']
  self.assertIn(dict(op='cast',ref='rule_blocker',value=1),s)
  self.assertIn(dict(op='assert_card',ref='rule_blocker',property='sick',value=1),s)
  self.assertIn(dict(op='block',ref='rule_blocker',other='source',value=0),s)
  self.assertFalse(any(a['op']=='combat_damage' for a in s))




from types import SimpleNamespace
import itertools
import re
m=SimpleNamespace(FAMILY=growth.FAMILY,PINS=growth.TRANSFER_PINS,World=growth._TransferWorld,spec=growth._transfer_spec,variants=growth._transfer_variants,expand=growth._transfer_expand,step=growth._transfer_step,observe=growth._transfer_observe)
class GrowthCounterTransferTests(unittest.TestCase):
 def test_pending_response_is_in_initial_hand_receipts(self):
  for n in m.PINS['profiles']:
   s=self.cases(n,['source_bounced_pending_0'])[0]['steps']
   at=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='response')
   self.assertIn(m.step('assert_zone_multiplicity',player=1,zone='hand',index=m.PINS['registry']['Unsummon']['csvid'],value=1),s[:at])
 def decl(self,n):return dict(family=m.FAMILY,version=2,params=copy.deepcopy(m.PINS['profiles'][n]['params']),oracle_text=m.PINS['witnesses'][n]['oracle_text'],variants=['effect_0'])
 def cases(self,n,vs):
  d=self.decl(n);p=m.spec(d,m.PINS['witnesses'][n]);return m.expand(n,m.PINS['profiles'][n]['csvid'],d,p,vs,m.PINS['registry'],m.PINS['witnesses'])
 def test_whole_pins_and_parameters_fail_closed(self):
  n='Simic Fluxmage';d=self.decl(n);o=copy.deepcopy(m.PINS['witnesses'][n])
  for k,v in [('oracle_text','changed'),('power','2'),('layout','transform'),('oracle_id','wrong')]:
   changed=dict(o,**{k:v})
   with self.assertRaises(ValueError):m.spec(d,changed)
  for fields in [dict(transfer=True),dict(tap=False),dict(entry=1),dict(extra=0)]:
   bad=copy.deepcopy(d);bad['params'].update(fields)
   with self.assertRaises(ValueError):m.spec(bad,o)
 def test_source_cast_and_activation_vectors_independently_match_printed_symbols(self):
  def vectors(text):
   out={(0,0,0,0,0,0)}
   for token in re.findall(r'\{([^}]+)\}',text):
    if token=='T':continue
    choices=[]
    for part in token.split('/'):
     v=[0]*6
     if part.isdigit():v[0]=int(part)
     else:v['BUGRW'.index(part)+1]=1
     choices.append(v)
    out={tuple(a+b for a,b in zip(left,right)) for left in out for right in choices}
   return out
  for n,pin in m.PINS['profiles'].items():
   self.assertEqual(set(map(tuple,pin['params']['cast_vectors'])),vectors(pin['oracle']['mana_cost']))
   printed=next(line.split(':',1)[0] for line in pin['oracle']['oracle_text'].splitlines() if ': Move a +1/+1 counter' in line)
   self.assertEqual(set(map(tuple,pin['params']['activation_vectors'])),vectors(printed));self.assertEqual(pin['params']['tap'],'{T}' in printed)
 def test_transfer_is_atomic_conservative_and_zero_safe(self):
  for amount,a_zone,b_zone,same in itertools.product((0,1,3),('battlefield','hand'),('battlefield','graveyard'),(False,True)):
   w=m.World(0);w.add('Grizzly Bears','a',0);w.add('Hill Giant','b',1);w.entities['a'].update(counters=amount,zone=a_zone);w.entities['b'].update(counters=2,zone=b_zone)
   before=copy.deepcopy(w.entities);w.transfer('a','a' if same else 'b')
   move=not same and a_zone==b_zone=='battlefield' and amount>0
   self.assertEqual(w.entities['a']['counters'],amount-int(move));self.assertEqual(w.entities['b']['counters'],2+int(move))
 def test_pending_flux_counter_is_not_paid_as_cost(self):
  for owner,c in enumerate(self.cases('Simic Fluxmage',['activation_pending_0','activation_pending_1'])):
   s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate')
   self.assertIn(m.observe('source','plus1_counters',1),s[a:]);self.assertIn(m.step('assert_stack',value=1),s[a:]);self.assertFalse(any(x['op']=='resolve' for x in s[a:]))
   self.assertIn(m.step('assert_mana',player=owner,color=0,value=29),s[a:])
 def test_flux_target_fizzle_preserves_source_counter(self):
  for c in self.cases('Simic Fluxmage',['recipient_bounced_pending_0','recipient_bounced_pending_1']):
   self.assertIn(m.observe('source','plus1_counters',1),c['steps'])
   self.assertEqual([x['value'] for x in c['steps'] if x['op']=='assert_stack' and x['value']==2],[2])
 def test_flux_source_loss_does_not_create_counter_on_recipient(self):
  for c in self.cases('Simic Fluxmage',['source_bounced_pending_0','source_destroyed_pending_1']):
   self.assertIn(m.observe('recipient','plus1_counters',0),c['steps'][-250:])
 def test_bounced_hand_observations_rebind_unique_actual_identity_after_resolution(self):
  for n,mode,ref in [('Simic Fluxmage','source_bounced_pending','source'),('Cytoplast Root-Kin','origin_bounced_pending','origin'),('Simic Guildmage','recipient_bounced_pending','recipient'),('Daghatar the Adamant','source_bounced_pending','source')]:
   s=self.cases(n,[mode+'_0'])[0]['steps'];bind=next(i for i,a in enumerate(s) if a['op']=='bind_hand')
   self.assertEqual(s[bind]['ref'],ref);self.assertEqual(s[bind-2]['op'],'resolve')
   self.assertIn(m.observe(ref,'in_hand',1),s[bind:])
 def test_root_kin_uses_real_cast_and_qualified_mandatory_entry_bonus(self):
  c=self.cases('Cytoplast Root-Kin',['cast_payment_0'])[0];s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source')
  self.assertEqual(next(x['zone'] for x in c['setup'] if x['ref']=='source'),'hand')
  for ref,count in [('source',4),('origin',2),('own_control',2),('enemy_control',1)]:self.assertIn(m.observe(ref,'plus1_counters',count),s[a:])
  c=self.cases('Cytoplast Root-Kin',['entry_controls_1'])[0]
  self.assertIn(m.observe('own_control','plus1_counters',0),c['steps']);self.assertIn(m.observe('enemy_control','plus1_counters',2),c['steps'])
 def test_graft_dormancy_and_evolve_real_entry_are_explicit(self):
  for c in self.cases('Cytoplast Root-Kin',['effect_0','repeat_1']):
   self.assertFalse(any(x['op']=='cast' and x['ref']=='new_creature' for x in c['steps']))
  c=self.cases('Simic Fluxmage',['evolve_qualifying_0'])[0];s=c['steps'];at=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='new_creature')
  self.assertIn(m.observe('source','plus1_counters',1),s[at:]);self.assertIn(m.observe('new_creature','sick',1),s[at:])
 def test_hybrid_casts_and_activations_use_real_unique_color_pools(self):
  for mode,colors in [('cast_blue',{2:2}),('cast_mixed',{2:1,3:1})]:
   s=self.cases('Simic Guildmage',[mode+'_1'])[0]['steps'];at=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source')
   pool={x['color']:x['value'] for x in s[:at] if x['op']=='set_mana' and x['player']==1}
   self.assertEqual(pool,{i:colors.get(i,0) for i in range(6)});self.assertFalse(any(x['op']=='choose_dialog' for x in s))
  for mode,v in [('payment_black',[1,2,0,0,0,0]),('payment_mixed',[1,1,0,1,0,0])]:
   s=self.cases('Daghatar the Adamant',[mode+'_0'])[0]['steps'];at=next(i for i,x in enumerate(s) if x['op']=='activate')
   self.assertEqual({x['color']:x['value'] for x in s[:at] if x['op']=='set_mana' and x['player']==0},dict(enumerate(v)))
   for i in range(6):self.assertIn(m.step('assert_mana',player=0,color=i,value=0),s[at:])
 def test_two_target_positive_relations_and_no_ambiguous_probes(self):
  for n,mode in [('Simic Guildmage','opponent_pair'),('Daghatar the Adamant','opponent_origin')]:
   c=self.cases(n,[mode+'_0'])[0];e={x['ref']:x for x in c['setup']}
   self.assertEqual(e['origin']['player'],1);self.assertEqual(e['recipient']['player'],1 if n=='Simic Guildmage' else 0)
   self.assertFalse(any(x['op']=='probe_activation_target' for x in c['steps']))
 def test_unused_aura_mode_has_no_aura_or_blue_activation_resources(self):
  n='Simic Guildmage';d=self.decl(n);p=m.spec(d,m.PINS['witnesses'][n])
  for c in self.cases(n,m.variants(p)):
   self.assertFalse(any('Aura' in m.PINS['witnesses'][x['name']]['type_line'] for x in c['setup']))
   blue=0
   for a in c['steps']:
    if a['op']=='set_mana' and a['player']==int(c['_recipe']['variant'][-1]) and a['color']==2:blue=a['value']
    if a['op']=='activate' and a['ref']=='source' and a['value']==1:self.assertEqual(blue,0)
 def test_zero_and_incomplete_pairs_retain_real_queries(self):
  s=self.cases('Simic Guildmage',['zero_counter_0'])[0]['steps'];self.assertIn(m.step('require_can_activate',ref='source',value=1),s)
  for n in ('Simic Guildmage','Daghatar the Adamant'):
   s=self.cases(n,['one_creature_1'])[0]['steps'];self.assertIn(m.step('require_can_activate',ref='source',value=0),s);self.assertNotIn(m.step('activate',ref='source',value=1),s)
 def test_every_staged_shape_has_valid_refs_and_no_counter_setter(self):
  for n in m.PINS['profiles']:
   d=self.decl(n);p=m.spec(d,m.PINS['witnesses'][n]);cs=self.cases(n,m.variants(p))
   for c in cs:
    refs={x['ref'] for x in c['setup']};self.assertEqual(len(refs),len(c['setup']))
    for a in c['steps']:
     for k in ('ref','other'):
      if k in a:self.assertIn(a[k],refs)
     self.assertNotIn(a['op'],('set_counters','set_power','set_toughness'))
    self.assertEqual(c['_recipe']['transfer_model'],'atomic resolution movement; printed costs')




resource_namespace=vars(growth)
resource_pins=growth.PINS['resources']
resource_rows={r['name']:r for r in registry()}
resource_oracle=json.loads((ROOT/'data/oracle.json').read_text(encoding='utf8'))['cards']
from generate_cases import OPS
class GrowthResourceActivationTests(unittest.TestCase):
 def test_uniform_duplicate_discard_rebinds_actual_remaining_card(self):
  for n in ('Amok','Flowstone Sculpture',"Nature's Blessing",'Lotleth Troll'):
   for owner in (0,1):
    c=self.case(n,'repeat_'+str(owner));s=c['steps'];first=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source');bind=next(i for i,x in enumerate(s) if x['op']=='bind_hand' and x['ref']=='cost_1');second=next(i for i in range(first+1,len(s)) if s[i]['op']=='activate' and s[i]['ref']=='source')
    self.assertLess(first,bind);self.assertLess(bind,second);self.assertEqual(s[bind],dict(op='bind_hand',ref='cost_1',player=owner,value=resource_rows['Grizzly Bears']['csvid']))
 def test_discard_and_impostor_display_inputs_match_actual_ui(self):
  for owner in (0,1):
   for n in ('Flowstone Sculpture',"Nature's Blessing",'Lotleth Troll'):
    s=self.case(n,'effect_'+str(owner))['steps'];choices=[x for x in s if x['op']=='choose_zone_card']
    self.assertEqual(len(choices),int(owner==0 or n=='Lotleth Troll'))
   s=self.case('Dark Impostor','effect_'+str(owner))['steps'];at=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source')
   self.assertIn(dict(op='choose_dialog',player=owner,value=0),s[:at])
 def test_public_dispatch_and_strict_real_untap_fields(self):
  import recipes
  for n,pin in resource_pins['profiles'].items():
   d=dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=resource_oracle[n]['oracle_text'],variants=['effect_0','effect_1'])
   self.assertEqual(len(recipes.expand(n,pin['csvid'],d,resource_oracle[n],resource_rows,resource_oracle)),2)
  for owner in (0,1):
   s=self.case('Shauku, Endbringer','cast_untap_activation_'+str(owner))['steps']
   for i,a in enumerate(s):
    if a['op']=='untap_step':self.assertEqual(set(a),{'op','player'});self.assertEqual(s[i-1],dict(op='advance_turn'))
   last=max(i for i,a in enumerate(s) if a['op']=='untap_step')
   self.assertEqual(s[last+1],dict(op='turn',player=owner,value=0))
 def case(self,n,v):
  pin=resource_pins['profiles'][n];d=dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=resource_oracle[n]['oracle_text'],variants=[v]);p=resource_namespace['_resource_spec'](d,resource_oracle[n]);return resource_namespace['_resource_expand'](n,pin['csvid'],d,p,[v],resource_rows,resource_oracle)[0]
 def test_all_shapes_and_resource_pins(self):
  resource_namespace['_resource_validate_witnesses'](resource_oracle,resource_rows)
  for n,pin in resource_pins['profiles'].items():
   p=resource_namespace['_resource_spec'](dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=resource_oracle[n]['oracle_text'],variants=['effect_0']),resource_oracle[n])
   for v in resource_namespace['_resource_variants'](p):
    c=self.case(n,v);refs={x['ref'] for x in c['setup']}
    for s in c['steps']:
     self.assertIn(s['op'],OPS,(n,v,s))
     for k in ('ref','other'):
      if k in s:self.assertIn(s[k],refs)
 def test_random_discard_remains_uniform(self):
  for mode in ('effect','repeat','land_discard'):
   c=self.case('Amok',mode+'_0');hand=[x['name'] for x in c['setup'] if x['zone']=='hand' and x['player']==0 and x['ref']!='source']
   self.assertEqual(len(set(hand)),1);self.assertFalse(any(s['op']=='choose_zone_card' for s in c['steps']))
 def test_cost_paid_before_counter_effect(self):
  for n in ('Amok','Flowstone Sculpture',"Nature's Blessing",'Lotleth Troll'):
   c=self.case(n,'effect_1');s=c['steps'];at=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source')
   self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['zone']=='graveyard' and x['player']==1 and x['index']==resource_rows['Grizzly Bears']['csvid'] and x['value']==2 for x in s[at:next(i for i in range(at,len(s)) if s[i]['op']=='resolve')]))
 def test_ooze_source_loss_still_gains_life(self):
  for mode in ('source_destroyed_pending','source_bounced_pending'):
   s=self.case('Scavenging Ooze',mode+'_0')['steps'];self.assertIn(dict(op='assert_life',player=0,value=21),s)
 def test_noncreature_exiles_no_reward(self):
  for mode in ('land_exile','instant_exile'):
   s=self.case('Scavenging Ooze',mode+'_1')['steps'];self.assertIn(dict(op='assert_life',player=1,value=20),s);self.assertNotIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s)
 def test_shauku_fresh_denies_but_nontap_fresh_activates(self):
  for n in ('Shauku, Endbringer','Dark Impostor','Lotleth Troll'):
   s=self.case(n,'fresh_source_0' if n=='Shauku, Endbringer' else 'effect_0')['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s);self.assertIn(dict(op='require_can_activate',ref='source',value=int(n!='Shauku, Endbringer')),s)
 def test_noncreature_source_bounce_uses_real_permanent_removal(self):
  for n in ('Amok',"Nature's Blessing"):
   c=self.case(n,'source_bounced_pending_0');self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='response'),'Boomerang')
 def test_all_resource_oracle_and_parameter_mutations_rejected(self):
  for n,pin in resource_pins['profiles'].items():
   d=dict(family=growth.FAMILY,version=2,params=copy.deepcopy(pin['params']),oracle_text=resource_oracle[n]['oracle_text'],variants=['effect_0'])
   for k in pin['params']:
    broken=copy.deepcopy(d);broken['params'][k]=None
    with self.assertRaises(ValueError):resource_namespace['_resource_spec'](broken,resource_oracle[n])
   changed=dict(resource_oracle[n],oracle_id='changed')
   with self.assertRaises(ValueError):resource_namespace['_resource_spec'](d,changed)




graft_namespace=vars(growth)
graft_pins=growth.PINS['graft_grants']
graft_rows={r['name']:r for r in registry()}
graft_oracle=json.loads((ROOT/'data/oracle.json').read_text(encoding='utf8'))['cards']
class GrowthGraftActivationTests(unittest.TestCase):
 def test_public_dispatch_and_independent_printed_cost_entry_pins(self):
  import recipes
  for n,pin in graft_pins['profiles'].items():
   text=graft_oracle[n]['oracle_text'];self.assertEqual(int(re.search(r'Graft (\d+)',text)[1]),pin['params']['entry'])
   cost=next(line.split(':',1)[0] for line in text.splitlines() if line.startswith('{'))
   self.assertEqual(recipes.cost_from_oracle(dict(mana_cost=cost)),pin['params']['cost'])
   self.assertIn(pin['params']['effect'].lower(),text.lower())
   d=dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=text,variants=['effect_0','effect_1'])
   self.assertEqual(len(recipes.expand(n,pin['csvid'],d,graft_oracle[n],graft_rows,graft_oracle)),2)
   for key in pin['params']:
    broken=copy.deepcopy(d);broken['params'][key]=None
    with self.assertRaises(ValueError):growth.spec(broken,graft_oracle[n])
 def case(self,n,v):
  pin=graft_pins['profiles'][n];d=dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=graft_oracle[n]['oracle_text'],variants=[v]);p=graft_namespace['_graft_spec'](d,graft_oracle[n]);return graft_namespace['_graft_expand'](n,pin['csvid'],d,p,[v],graft_rows,graft_oracle)[0]
 def test_all_pinned_shapes(self):
  graft_namespace['_graft_validate_witnesses'](graft_oracle,graft_rows)
  for n,pin in graft_pins['profiles'].items():
   p=graft_namespace['_graft_spec'](dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=graft_oracle[n]['oracle_text'],variants=['effect_0']),graft_oracle[n])
   for v in graft_namespace['_graft_variants'](p):
    c=self.case(n,v);refs={e['ref'] for e in c['setup']};self.assertEqual(len(refs),len(c['setup']))
    self.assertEqual(next(x['zone'] for x in c['setup'] if x['ref']=='source'),'hand')
    for s in c['steps']:
     self.assertIn(s['op'],OPS)
     for k in ('ref','other'):
      if k in s:self.assertIn(s[k],refs)
 def test_protected_predicate_has_real_counter(self):
  for n in graft_pins['profiles']:
   s=self.case(n,'shroud_probe_0')['steps'];probe=next(i for i,x in enumerate(s) if x['op']=='probe_activation_target');self.assertTrue(any(x['op']=='cast' and x['ref']=='counter_seed' for x in s[:probe]));self.assertIn(dict(op='assert_card',ref='recipient',property='plus1_counters',value=1),s[:probe])
 def test_counter_loss_uses_real_paid_response(self):
  for n in graft_pins['profiles']:
   c=self.case(n,'counter_removed_pending_1');s=c['steps'];self.assertIn(dict(op='activate',ref='hexmage',value=1),s);self.assertIn(dict(op='assert_stack',value=2),s)
   self.assertLess(next(i for i,x in enumerate(c['setup']) if x['ref']=='hexmage'),next(i for i,x in enumerate(c['setup']) if x['ref']=='source'))
 def test_keyword_cleanup_proves_positive_on_same_board(self):
  for n,pin in graft_pins['profiles'].items():
   if pin['params']['effect']=='Untap':continue
   s=self.case(n,'cleanup_0')['steps'];self.assertEqual(s[0],dict(op='capture'));restore=next(i for i,x in enumerate(s) if x['op']=='restore');cleanup=next(i for i,x in enumerate(s) if x['op']=='cleanup');self.assertLess(restore,cleanup)
   self.assertTrue(any(x['op'] in ('combat_damage','can_target_creature') for x in s[:restore]))
 def test_every_cast_is_source_or_noncreature(self):
  for n in graft_pins['profiles']:
   c=self.case(n,'effect_1');names={e['ref']:e['name'] for e in c['setup']}
   for s in c['steps']:
    if s['op']=='cast' and s['ref']!='source':self.assertNotIn('Creature',graft_oracle[names[s['ref']]]['type_line'])
 def test_gift_combat_resources(self):
  for n,expected in [('Helium Squirter',16),('Cytospawn Shambler',18)]:
   s=self.case(n,'effect_0')['steps'];self.assertIn(dict(op='assert_life',player=1,value=expected),s);self.assertTrue(any(x['op']=='combat_damage' for x in s))
  s=self.case('Aquastrand Spider','effect_0')['steps'];self.assertIn(dict(op='block',ref='recipient',other='fighter',value=1),s)
 def test_shroud_has_two_controller_probes_and_real_control_cast(self):
  s=self.case('Plaxcaster Frogling','effect_1')['steps']
  for ref in ('probe_own','probe_enemy'):self.assertIn(dict(op='can_target_creature',ref=ref,other='recipient',value=0),s)
  self.assertIn(dict(op='cast',ref='probe_enemy',value=1),s)
 def test_untap_checks_pending_tapped_and_resolved_ready(self):
  s=self.case('Vigean Graftmage','effect_0')['steps'];at=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source');resolve=next(i for i in range(at,len(s)) if s[i]['op']=='resolve');self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=1),s[at:resolve]);self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=0),s[resolve:])




draw_namespace=vars(growth)
draw_pins=growth.PINS['counter_draw']
draw_rows={r['name']:r for r in registry()}
draw_oracle=json.loads((ROOT/'data/oracle.json').read_text(encoding='utf8'))['cards']
class GrowthCounterCostDrawTests(unittest.TestCase):
 def test_public_dispatch_and_independent_printed_counter_costs(self):
  import recipes
  for n,pin in draw_pins['profiles'].items():
   text=draw_oracle[n]['oracle_text'];cost,amount=re.search(r'^((?:\{[^}]+\})+), Remove (a|two) \+1/\+1 counter',text,re.M).groups()
   self.assertEqual(recipes.cost_from_oracle(dict(mana_cost=cost)),pin['params']['cost']);self.assertEqual({'a':1,'two':2}[amount],pin['params']['remove'])
   self.assertEqual(int('then discard a card' in text),pin['params']['discard'])
   d=dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=text,variants=['effect_0','effect_1'])
   self.assertEqual(len(recipes.expand(n,pin['csvid'],d,draw_oracle[n],draw_rows,draw_oracle)),2)
 def test_entry_mode_controller_control_has_its_own_turn_and_positive(self):
  for owner in (0,1):
   c=self.case('Zameck Guildmage','entry_mode_enemy_'+str(owner));s=c['steps'];self.assertEqual(s[0],dict(op='capture'));restore=next(i for i,x in enumerate(s) if x['op']=='restore');tail=s[restore+1:]
   self.assertIn(dict(op='assert_card',ref='entrant',property='plus1_counters',value=1),s[:restore]);self.assertIn(dict(op='assert_card',ref='enemy_entrant',property='plus1_counters',value=0),tail)
   act=next(i for i,x in enumerate(tail) if x['op']=='activate' and x['ref']=='source');turns=[x for x in tail[:act] if x['op']=='turn'];self.assertEqual(turns[-1],dict(op='turn',player=1-owner,value=0))
 def case(self,n,v):
  pin=draw_pins['profiles'][n];d=dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=draw_oracle[n]['oracle_text'],variants=[v]);p=draw_namespace['_counter_draw_spec'](d,draw_oracle[n]);return draw_namespace['_counter_draw_expand'](n,pin['csvid'],d,p,[v],draw_rows,draw_oracle)[0]
 def test_all_shapes_and_pins(self):
  draw_namespace['_counter_draw_validate_witnesses'](draw_oracle,draw_rows)
  for n,pin in draw_pins['profiles'].items():
   p=draw_namespace['_counter_draw_spec'](dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=draw_oracle[n]['oracle_text'],variants=['effect_0']),draw_oracle[n])
   for v in draw_namespace['_counter_draw_variants'](p):
    c=self.case(n,v);refs={x['ref'] for x in c['setup']};self.assertEqual(len(refs),len(c['setup']))
    for s in c['steps']:
     self.assertIn(s['op'],OPS)
     for k in ('ref','other'):
      if k in s:self.assertIn(s[k],refs)
 def test_actual_source_cast_all_variants(self):
  for n in draw_pins['profiles']:
   self.assertIn(dict(op='cast',ref='source',value=1),self.case(n,'effect_1')['steps'])
 def test_draw_depletes_ordered_library_and_persists(self):
  for n,pin in draw_pins['profiles'].items():
   s=self.case(n,'effect_0')['steps'];self.assertIn(dict(op='assert_zone',player=0,zone='library',value=3),s);self.assertIn(dict(op='assert_zone_card',player=0,zone='library',index=0,value=draw_rows['Plains']['csvid']),s)
   self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard' if pin['params']['discard'] else 'hand',index=draw_rows['Forest']['csvid'],value=2 if pin['params']['discard'] else 1),s)
 def test_cost_removed_while_draw_pending(self):
  for n,ref,value in [('Academy Elite','source',1),('Novijen Sages','source',3),('Sage of Fables','payer',0),('Zameck Guildmage','payer',0)]:
   s=self.case(n,'effect_0')['steps'];at=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source');resolve=next(i for i in range(at,len(s)) if s[i]['op']=='resolve');self.assertIn(dict(op='assert_card',ref=ref,property='plus1_counters',value=value),s[at:resolve]);self.assertIn(dict(op='assert_zone',player=0,zone='library',value=4),s[at:resolve])
 def test_operative_wizard_entry_and_first_mode_both_modeled(self):
  for mode,value in [('wizard_entry',1),('nonwizard_entry',0),('enemy_wizard_entry',0)]:
   s=self.case('Sage of Fables',mode+'_0')['steps'];self.assertIn(dict(op='assert_card',ref='entrant',property='plus1_counters',value=value),s)
  for mode,value in [('entry_mode',1),('entry_mode_cleanup',0),('entry_mode_twice',2),('entry_mode_source_loss',1)]:
   s=self.case('Zameck Guildmage',mode+'_1')['steps'];self.assertIn(dict(op='assert_card',ref='entrant',property='plus1_counters',value=value),s)
 def test_empty_entry_is_a_real_cast_death(self):
  s=self.case('Academy Elite','entry_zero_0')['steps'];self.assertIn(dict(op='cast',ref='source',value=1),s);self.assertNotIn(dict(op='activate',ref='source',value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=draw_rows['Academy Elite']['csvid'],value=1),s)
 def test_whole_oracle_and_parameters_fail_closed(self):
  for n,pin in draw_pins['profiles'].items():
   d=dict(family=growth.FAMILY,version=2,params=copy.deepcopy(pin['params']),oracle_text=draw_oracle[n]['oracle_text'],variants=['effect_0'])
   for key in pin['params']:
    broken=copy.deepcopy(d);broken['params'][key]=None if pin['params'][key] is not None else 'changed'
    with self.assertRaises(ValueError):draw_namespace['_counter_draw_spec'](broken,draw_oracle[n])
   with self.assertRaises(ValueError):draw_namespace['_counter_draw_spec'](d,dict(draw_oracle[n],oracle_id='changed'))




followup_namespace=vars(growth)
followup_pins=growth.PINS['resources']
followup_delta=dict(profiles={n:followup_pins['profiles'][n] for n in ('Molten Hydra','Licia, Sanguine Tribune')})
followup_rows={r['name']:r for r in registry()}
followup_oracle=json.loads((ROOT/'data/oracle.json').read_text(encoding='utf8'))['cards']
class GrowthResourceFollowupTests(unittest.TestCase):
 def test_printed_life_cost_amount_and_turn_limits_are_independent(self):
  p=followup_pins['profiles']['Licia, Sanguine Tribune']['params'];text=followup_oracle['Licia, Sanguine Tribune']['oracle_text']
  self.assertIn('Pay 5 life: Put three +1/+1 counters on Licia.',text);self.assertEqual((p['pay_life'],p['amount']),(5,3));self.assertIn('only during your turn and only once each turn',text);self.assertTrue(p['own_turn_only'] and p['once_per_turn'])
  import recipes
  p=followup_pins['profiles']['Molten Hydra']['params'];cost=followup_oracle['Molten Hydra']['oracle_text'].split(':',1)[0];self.assertEqual(recipes.cost_from_oracle(dict(mana_cost=cost)),p['cost'])
 def decl(self,n):
  pin=followup_pins['profiles'][n];return dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=followup_oracle[n]['oracle_text'],variants=['effect_0'])
 def case(self,n,v):
  d=self.decl(n);p=followup_namespace['_resource_spec'](d,followup_oracle[n]);return followup_namespace['_resource_expand'](n,followup_pins['profiles'][n]['csvid'],d,p,[v],followup_rows,followup_oracle)[0]

 def test_all_new_shapes_and_whole_pins(self):
  followup_namespace['_resource_validate_witnesses'](followup_oracle,followup_rows)
  for n in followup_delta['profiles']:
   d=self.decl(n);p=followup_namespace['_resource_spec'](d,followup_oracle[n])
   for v in followup_namespace['_resource_variants'](p):
    c=self.case(n,v);refs={x['ref'] for x in c['setup']}
    for s in c['steps']:
     self.assertIn(s['op'],OPS)
     for key in ('ref','other'):
      if key in s:self.assertIn(s[key],refs)
 def test_life_receipts_and_boundaries(self):
  for mode,life,counters in [('effect',15,3),('low_life',1,3),('insufficient_life',4,0),('opposing_turn',20,0),('next_turn',10,6)]:
   s=self.case('Licia, Sanguine Tribune',mode+'_0')['steps'];self.assertIn(dict(op='assert_life',player=0,value=life),s);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=counters),s)
 def test_once_denials_do_not_pay_second_life_cost(self):
  for mode in ('once_used','stacked_denied'):
   s=self.case('Licia, Sanguine Tribune',mode+'_1')['steps'];self.assertEqual(sum(x==dict(op='activate',ref='source',value=1) for x in s),1);self.assertIn(dict(op='activate',ref='source',value=0),s);self.assertIn(dict(op='assert_life',player=1,value=15),s)
 def test_next_turn_actual_untap_then_main_priority(self):
  s=self.case('Licia, Sanguine Tribune','next_turn_1')['steps']
  for i,x in enumerate(s):
   if x['op']=='untap_step':self.assertEqual(set(x),{'op','player'});self.assertEqual(s[i-1],dict(op='advance_turn'));self.assertEqual(s[i+1],dict(op='turn',player=x['player'],value=0))
 def test_hydra_autochoose_human_vs_presented_ai(self):
  for owner in (0,1):
   s=self.case('Molten Hydra','repeat_'+str(owner))['steps'];self.assertEqual([x['value'] for x in s if x['op']=='choose_dialog' and x['player']==owner],[0,0] if owner else [])
 def test_source_bounce_counter_clear_is_explicit(self):
  for n in followup_delta['profiles']:
   s=self.case(n,'source_bounced_pending_0')['steps'];self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s)



selection_namespace=vars(growth)
selection_pins=growth.PINS['selection_growth']
selection_rows={r['name']:r for r in registry()}
selection_oracle=json.loads((ROOT/'data/oracle.json').read_text(encoding='utf8'))['cards']
class GrowthSelectionActivationTests(unittest.TestCase):
 def test_enchantments_use_permanent_tapper_and_real_white_cost(self):
  for n in ('Echoes of the Kin Tree','Durable Handicraft'):
   for owner in (0,1):
    c=self.case(n,'tapped_source_'+str(owner));s=c['steps'];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='tapper'),'Scepter of Dominance')
    self.assertIn(dict(op='activate',ref='tapper',value=1),s);self.assertIn(dict(op='assert_mana',player=1-owner,color=5,value=39),s);self.assertIn(dict(op='assert_mana',player=1-owner,color=0,value=40),s)
  self.assertEqual(next(x['name'] for x in self.case('Joraga Auxiliary','tapped_source_0')['setup'] if x['ref']=='tapper'),'Icy Manipulator')
 def test_public_dispatch_and_independent_printed_costs(self):
  import recipes
  for n,pin in selection_pins['profiles'].items():
   line=next(line for line in selection_oracle[n]['oracle_text'].splitlines() if line.startswith('{'));cost=re.match(r'((?:\{[^}]+\})+)',line)[1]
   self.assertEqual(recipes.cost_from_oracle(dict(mana_cost=cost)),pin['params']['cost']);self.assertEqual('Sacrifice' in line,pin['params']['sacrifice'])
   if pin['params']['kind']=='support':self.assertIn('Support 2.',line);self.assertEqual(pin['params']['limit'],2)
   if pin['params']['kind']=='bolster':self.assertIn('Bolster 1.',line);self.assertEqual(pin['params']['amount'],1)
   d=dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=selection_oracle[n]['oracle_text'],variants=['effect_0','effect_1'])
   self.assertEqual(len(recipes.expand(n,pin['csvid'],d,selection_oracle[n],selection_rows,selection_oracle)),2)
 def case(self,n,v):
  pin=selection_pins['profiles'][n];d=dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=selection_oracle[n]['oracle_text'],variants=[v]);p=selection_namespace['_selection_spec'](d,selection_oracle[n]);return selection_namespace['_selection_expand'](n,pin['csvid'],d,p,[v],selection_rows,selection_oracle)[0]
 def test_all_shapes_and_pins(self):
  selection_namespace['_selection_validate_witnesses'](selection_oracle,selection_rows)
  for n,pin in selection_pins['profiles'].items():
   p=selection_namespace['_selection_spec'](dict(family=growth.FAMILY,version=2,params=pin['params'],oracle_text=selection_oracle[n]['oracle_text'],variants=['effect_0']),selection_oracle[n])
   for v in selection_namespace['_selection_variants'](p):
    c=self.case(n,v);refs={e['ref'] for e in c['setup']};self.assertEqual(len(refs),len(c['setup']))
    for a in c['steps']:
     self.assertIn(a['op'],OPS)
     for key in ('ref','other'):
      if key in a:self.assertIn(a[key],refs)
 def test_bolster_choice_is_resolution_time_and_nontargeted(self):
  for mode,ref in [('tie','control'),('protected_choice','recipient')]:
   c=self.case('Echoes of the Kin Tree',mode+'_0');s=c['steps'];act=next(i for i,a in enumerate(s) if a['op']=='activate' and a['ref']=='source');choice=next(i for i,a in enumerate(s) if a['op']=='choose_target' and a['ref']==ref);self.assertLess(act,choice)
  s=self.case('Echoes of the Kin Tree','effect_0')['steps'];self.assertFalse(any(a['op']=='choose_target' for a in s))
 def test_bolster_recomputes_after_real_response(self):
  s=self.case('Echoes of the Kin Tree','response_changes_minimum_1')['steps'];self.assertIn(dict(op='cast',ref='response',value=1),s);self.assertIn(dict(op='assert_stack',value=2),s);self.assertIn(dict(op='assert_card',ref='control',property='plus1_counters',value=1),s);self.assertIn(dict(op='bind_hand',ref='recipient',player=1,value=selection_rows['Grizzly Bears']['csvid']),s)
 def test_support_oracle_time_queue_not_cleared_or_requeued(self):
  for mode in ('effect','pending_snapshot','targeting_pending'):
   s=self.case('Joraga Auxiliary',mode+'_0')['steps'];act=next(i for i,a in enumerate(s) if a['op']=='activate' and a['ref']=='source');choices=[i for i,a in enumerate(s) if a['op']=='choose_target'];self.assertEqual(len(choices),2);self.assertTrue(all(i<act for i in choices))
   if mode=='targeting_pending':self.assertFalse(any(a['op']=='resolve' for a in s[act:]));self.assertIn(dict(op='assert_stack',value=1),s[act:]);self.assertEqual(sum(a['op']=='assert_choices_consumed' for a in s[act:]),1)
 def test_optional_support_and_controller_controls(self):
  s=self.case('Joraga Auxiliary','one_target_1')['steps'];self.assertIn(dict(op='choose_cancel_target',player=1),s)
  s=self.case('Joraga Auxiliary','opponent_targets_0')['steps'];self.assertIn(dict(op='choose_target',player=0,ref='enemy_large'),s)
 def test_team_sacrifice_is_a_pending_cost(self):
  s=self.case('Durable Handicraft','effect_0')['steps'];act=next(i for i,a in enumerate(s) if a['op']=='activate' and a['ref']=='source');resolve=next(i for i in range(act,len(s)) if s[i]['op']=='resolve');self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=selection_rows['Durable Handicraft']['csvid'],value=1),s[act:resolve]);self.assertIn(dict(op='assert_card',ref='artifact_creature',property='plus1_counters',value=0),s[act:resolve]);self.assertIn(dict(op='assert_card',ref='artifact_creature',property='plus1_counters',value=1),s[resolve:])
 def test_operative_entry_pay_decline_and_controller(self):
  for mode,counters in [('entry_pay',1),('entry_decline',0),('entry_no_mana',0),('entry_opponent',0),('entry_source_removed',0)]:
   s=self.case('Durable Handicraft',mode+'_0')['steps'];self.assertIn(dict(op='assert_card',ref='entrant',property='plus1_counters',value=counters),s)
   if mode in ('entry_pay','entry_decline'):self.assertIn(dict(op='choose_trigger',player=0,ref='source'),s)
   if mode=='entry_source_removed':self.assertEqual(s[0],dict(op='capture'));restore=next(i for i,a in enumerate(s) if a['op']=='restore');self.assertIn(dict(op='assert_card',ref='entrant',property='plus1_counters',value=1),s[:restore])
 def test_whole_oracle_and_parameter_mutations_rejected(self):
  for n,pin in selection_pins['profiles'].items():
   d=dict(family=growth.FAMILY,version=2,params=copy.deepcopy(pin['params']),oracle_text=selection_oracle[n]['oracle_text'],variants=['effect_0'])
   for key in pin['params']:
    broken=copy.deepcopy(d);broken['params'][key]=None if pin['params'][key] is not None else 'changed'
    with self.assertRaises(ValueError):selection_namespace['_selection_spec'](broken,selection_oracle[n])
   with self.assertRaises(ValueError):selection_namespace['_selection_spec'](d,dict(selection_oracle[n],oracle_id='changed'))



class Wave6GrowthTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((ROOT/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name,variant):
  pin=growth.WAVE6_PINS['profiles'][name]
  return dict(family=growth.FAMILY,version=2,params=copy.deepcopy(pin['params']),oracle_text=self.o[name]['oracle_text'],variants=[variant])
 def case(self,name,variant):
  from recipes import expand
  return expand(name,self.rows[name]['csvid'],self.declaration(name,variant),self.o[name],self.rows,self.o)[0]
 def test_all_staged_variants_use_public_expansion_and_whole_pins(self):
  for name,pin in growth.WAVE6_PINS['profiles'].items():
   for variant in growth.variants(pin['params']):
    c=self.case(name,variant);self.assertEqual(c['_recipe']['oracle_text'],self.o[name]['oracle_text']);self.assertTrue(c['steps']);self.assertEqual(next(e['player'] for e in c['setup'] if e['ref']=='source'),int(variant.rsplit('_',1)[1]))
 def test_profile_and_oracle_mutations_fail_closed(self):
  for name,pin in growth.WAVE6_PINS['profiles'].items():
   d=self.declaration(name,'effect_0')
   for key in pin['params']:
    broken=copy.deepcopy(d);broken['params'][key]=None
    with self.assertRaises(ValueError):growth.spec(broken,self.o[name])
   with self.assertRaises(ValueError):growth.spec(d,dict(self.o[name],oracle_text='Changed'))
 def test_recursive_types_reject_bool_integer_and_list_aliases(self):
  name='Fabrication Module';d=self.declaration(name,'effect_0')
  for mutate in ('tap','zero_cost','integer_cost','modes_tuple'):
   bad=copy.deepcopy(d)
   if mutate=='tap':bad['params']['tap']=1
   elif mutate=='zero_cost':bad['params']['cost'][1]=False
   elif mutate=='integer_cost':bad['params']['cost'][0]=4.0
   else:bad['params']['modes']=tuple(bad['params']['modes'])
   with self.assertRaises(ValueError):growth.spec(bad,self.o[name])
 def test_implement_independent_death_draw_and_printed_sorcery_cost(self):
  s=self.case('Implement of Ferocity','effect_0')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');r=next(i for i in range(a,len(s)) if s[i]['op']=='resolve')
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=16729,value=1),s[a:r]);self.assertIn(dict(op='assert_card',ref='recipient',property='plus1_counters',value=0),s[a:r]);self.assertIn(dict(op='assert_card',ref='recipient',property='plus1_counters',value=1),s[r:])
  death=self.case('Implement of Ferocity','destroy_draw_1')['steps'];self.assertFalse(any(x['op']=='activate' for x in death));self.assertIn(dict(op='destroy',ref='source'),death)
 def test_countered_nonsoldier_positive_and_uncountered_soldier_negative(self):
  yes=self.case("Cenn's Tactician",'grant_nonsoldier_0');self.assertEqual(next(e['name'] for e in yes['setup'] if e['ref']=='control'),'Hill Giant');self.assertIn(dict(op='block',ref='control',other='attacker1',value=1),yes['steps'])
  no=self.case("Cenn's Tactician",'grant_counterless_soldier_1');self.assertEqual(next(e['name'] for e in no['setup'] if e['ref']=='counterless_soldier'),'Eager Cadet');self.assertIn(dict(op='block',ref='counterless_soldier',other='attacker1',value=0),no['steps'])
 def test_hand_count_excludes_resolving_source_and_search_spends_counter_before_resolution(self):
  for variant,expected in [('effect_0',6),('entry_fewer_hands_1',2),('entry_more_hands_0',9)]:
   s=self.case('Realm Seekers',variant)['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');r=next(i for i in range(a,len(s)) if s[i]['op']=='resolve')
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=expected),s[:a]);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=expected-1),s[a:r]);self.assertTrue(any(x['op']=='choose_zone_card' for x in s[a:r]))
 def test_energy_earned_by_real_combat_and_paid_before_growth(self):
  s=self.case('Longtusk Cub','effect_1')['steps'];damage=next(i for i,x in enumerate(s) if x['op']=='combat_damage');a=next(i for i,x in enumerate(s) if x['op']=='activate');self.assertLess(damage,a);self.assertIn(dict(op='assert_energy',player=1,value=2),s[damage:a]);self.assertIn(dict(op='assert_energy',player=1,value=0),s[a:]);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s[a:])
 def test_module_trigger_selection_follows_actual_energy_activation(self):
  s=self.case('Fabrication Module','effect_0')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');t=next(i for i,x in enumerate(s) if x['op']=='choose_target');self.assertLess(a,t);self.assertIn(dict(op='assert_energy',player=0,value=1),s[t:]);self.assertIn(dict(op='assert_card',ref='recipient',property='plus1_counters',value=1),s[t:])
 def test_zegana_another_and_controller_entry_scopes(self):
  for mode,hand in [('entry_qualified',4),('entry_counterless',3),('entry_opponent',3)]:
   s=self.case('Zegana, Utopian Speaker',mode+'_0')['steps'];self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=hand),s)
 def test_source_removal_grants_have_positive_same_board_capture(self):
  for name in ("Cenn's Tactician",'Zegana, Utopian Speaker'):
   s=self.case(name,'grant_removed_0')['steps'];capture=next(i for i,x in enumerate(s) if x['op']=='capture');restore=next(i for i,x in enumerate(s) if x['op']=='restore');self.assertTrue(any(x['op']=='block' for x in s[capture:restore]));self.assertTrue(any(x['op']=='cast' and x.get('ref')=='response' for x in s[restore:]))
 def test_pending_snapshot_replays_real_resolution(self):
  s=self.case('Realm Seekers','pending_snapshot_1')['steps'];self.assertEqual(sum(x['op']=='activate' for x in s),1);self.assertEqual(sum(x['op']=='resolve' for x in s),3);self.assertEqual(s[-1],dict(op='assert_recorded_state'))
 def test_artifact_bounce_and_actual_zone_change_rebind(self):
  c=self.case('Fabrication Module','source_bounced_pending_0');self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='response'),'Boomerang');self.assertIn(dict(op='bind_hand',ref='source',player=0,value=16370),c['steps'])
  self.assertIn(dict(op='bind_hand',ref='recipient',player=1,value=self.rows['Eager Cadet']['csvid']),self.case("Cenn's Tactician",'target_bounced_pending_1')['steps'])
 def test_cleanup_keeps_pretransition_payment_and_has_no_unrun_pool_clear_claim(self):
  s=self.case('Realm Seekers','cleanup_0')['steps'];c=next(i for i,x in enumerate(s) if x['op']=='cleanup');self.assertTrue(any(x['op']=='assert_mana' for x in s[:c]));self.assertFalse(any(x['op']=='assert_mana' for x in s[c:]));self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=5),s[c:])
 def test_fresh_artifact_flag_does_not_invent_creature_tap_restriction(self):
  s=self.case('Fabrication Module','fresh_source_1')['steps'];self.assertIn(dict(op='assert_card',ref='source',property='sick',value=1),s);self.assertIn(dict(op='activate',ref='source',value=1),s)
 def test_hydra_actual_x_threshold_cast_and_zero_entry_death(self):
  for mode,x,counters in [('entry_x4',4,4),('entry_x5',5,10)]:
   s=self.case('Apocalypse Hydra',mode+'_0')['steps'];self.assertIn(dict(op='choose_x',player=0,value=x),s);self.assertIn(dict(op='cast',ref='source',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=counters),s)
  s=self.case('Apocalypse Hydra','entry_x0_1')['steps'];self.assertIn(dict(op='choose_x',player=1,value=0),s);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=10108,value=1),s);self.assertFalse(any(x['op']=='activate' for x in s))
 def test_hydra_last_counter_cost_death_precedes_retained_damage(self):
  s=self.case('Apocalypse Hydra','last_counter_0')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');r=next(i for i in range(a,len(s)) if s[i]['op']=='resolve');self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=10108,value=1),s[a:r]);self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s[a:r]);self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),s[r:])
 def test_regeneration_uses_actual_destroy_window_and_target(self):
  s=self.case('Sporeback Troll','effect_1')['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='removal');choice=next(i for i,x in enumerate(s) if x['op']=='choose_regeneration');self.assertLess(cast,choice);self.assertIn(dict(op='choose_target',player=1,ref='recipient'),s[choice:]);self.assertIn(dict(op='assert_card',ref='recipient',property='tapped',value=1),s[choice:]);self.assertFalse(any(x['op']=='activate' for x in s))
 def test_graft_stays_dormant_and_second_destruction_is_unprotected(self):
  c=self.case('Sporeback Troll','second_destruction_0');s=c['steps'];sourcecast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source');self.assertEqual(sum(x['op']=='choose_regeneration' for x in s),1);self.assertTrue(any(x['op']=='cast' and x.get('ref')=='second_removal' for x in s));self.assertFalse(any(x['op']=='cast' and x.get('ref') not in ('removal','second_removal') for x in s[sourcecast+1:]));self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Hill Giant']['csvid'],value=1),s)
 def test_denied_regeneration_cost_retains_real_unavailable_response(self):
  for mode in ('insufficient_mana','wrong_color','counterless_target','cannot_regenerate'):
   s=self.case('Sporeback Troll',mode+'_0')['steps'];self.assertIn(dict(op='choose_regeneration',ref='source'),s);self.assertIn(dict(op='assert_unused_choices',value=1),s);r=next(i for i,x in enumerate(s) if x['op']=='choose_regeneration');self.assertFalse(any(x['op']=='choose_target' and x.get('player')==0 for x in s[r:]))
 def test_life_events_are_not_life_amounts(self):
  for name,count in [('Karlov of the Ghost Council',2),('Wall of Limbs',1)]:
   mode='one_life_event_0' if name.startswith('Karlov') else 'effect_0';s=self.case(name,mode)['steps'];n=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='nectar_0');self.assertIn(dict(op='assert_life',player=0,value=24),s[n:]);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=count),s[n:])
 def test_opponent_life_gain_does_not_fund_own_trigger(self):
  for name in ('Karlov of the Ghost Council','Wall of Limbs'):
   s=self.case(name,'opponent_gain_1')['steps'];self.assertIn(dict(op='assert_life',player=0,value=24),s);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s)
 def test_wall_sacrifice_uses_counter_and_growth_lki_power(self):
  s=self.case('Wall of Limbs','boosted_power_0')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');self.assertIn(dict(op='cast',ref='boost',value=1),s[:a]);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=14198,value=1),s[a:]);self.assertIn(dict(op='assert_life',player=1,value=16),s[a:])
 def test_wall_printed_target_player_includes_self(self):
  s=self.case('Wall of Limbs','own_player_1')['steps'];self.assertIn(dict(op='choose_player',player=1,value=1),s);self.assertIn(dict(op='assert_life',player=1,value=23),s)
 def test_drakestown_counts_only_graves_and_shrink_expires(self):
  for mode,entry in [('effect',2),('opponent_graves_only',1)]:
   s=self.case('Drakestown Forgotten',mode+'_0')['steps'];self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=entry),s)
  s=self.case('Drakestown Forgotten','cleanup_0')['steps'];c=next(i for i,x in enumerate(s) if x['op']=='cleanup');self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=6),s[:c]);self.assertIn(dict(op='assert_card',ref='recipient',property='power',value=7),s[c:])
 def test_sunburst_counts_distinct_colors_not_total_mana(self):
  for mode,counters in [('effect',4),('three_colors',3),('two_colors',2)]:
   s=self.case('Etched Oracle',mode+'_0')['steps'];self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=counters),s)
   if counters<4:self.assertIn(dict(op='activate',ref='source',value=0),s)
 def test_sunburst_uses_actual_human_colored_mana_clicks(self):
  for mode,colors in [('effect',[1,2,3,4]),('three_colors',[1,1,2,3]),('two_colors',[1,1,2,2]),('zero_colors',[])]:
   s=self.case('Etched Oracle',mode+'_0')['steps'];self.assertEqual([x['color'] for x in s if x['op']=='choose_mana'],colors)
  self.assertFalse(any(x['op']=='choose_mana' for x in self.case('Etched Oracle','effect_1')['steps']))
 def test_etched_last_four_counters_are_cost_before_draw(self):
  s=self.case('Etched Oracle','effect_0')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate');r=next(i for i in range(a,len(s)) if s[i]['op']=='resolve');self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=7049,value=1),s[a:r]);self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=3),s[a:r]);self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=6),s[r:])
 def test_damage_growth_tap_freshness_and_real_counter_effect(self):
  s=self.case('Blood Cultist','fresh_source_0')['steps'];self.assertIn(dict(op='activate',ref='source',value=0),s)
  s=self.case('Blood Cultist','effect_1')['steps'];self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Eager Cadet']['csvid'],value=1),s)
 def test_flying_only_negative_has_no_other_flying_creature(self):
  c=self.case('Trophy Hunter','no_flying_0');self.assertIn(dict(op='activate',ref='source',value=0),c['steps']);self.assertFalse(any('Flying' in self.o[x['name']]['keywords'] for x in c['setup'] if x['zone']=='battlefield'))
 def test_kazarov_counts_damage_events_not_amount_or_death(self):
  s=self.case('Kazarov, Sengir Pureblood','effect_0')['steps'];self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=2),s);self.assertIn(dict(op='assert_card',ref='recipient',property='in_play',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s)
  own=self.case('Kazarov, Sengir Pureblood','own_target_1')['steps'];self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),own)
 def test_delayed_death_uses_same_turn_and_actual_next_turn_control(self):
  s=self.case('Blood Cultist','kill_later_0')['steps'];self.assertIn(dict(op='cast',ref='removal',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s)
  s=self.case('Trophy Hunter','after_turn_kill_1')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='advance_turn');self.assertEqual(s[a+1],dict(op='untap_step',player=0));self.assertEqual(s[a+2],dict(op='turn',player=0,value=0));self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s[a:])
 def test_undamaged_same_name_copy_does_not_give_counter(self):
  c=self.case('Blood Cultist','undamaged_death_0');names={x['ref']:x['name'] for x in c['setup']};self.assertEqual(names['recipient'],names['enemy_control']);self.assertIn(dict(op='choose_target',player=1,ref='enemy_control'),c['steps']);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),c['steps'])
 def test_absorb_prevents_real_damage_before_later_death(self):
  for n,count in [('Blood Cultist',1),('Trophy Hunter',1),('Kazarov, Sengir Pureblood',2)]:
   c=self.case(n,'prevented_then_kill_0');self.assertEqual(sum(x['name']=='Lymph Sliver' and x['zone']=='battlefield' for x in c['setup']),count);self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='recipient'),'Winged Sliver');self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),c['steps']);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),c['steps'])
 def test_source_recast_does_not_inherit_prior_object_damage_history(self):
  s=self.case('Trophy Hunter','source_recast_before_death_0')['steps'];b=next(i for i,x in enumerate(s) if x['op']=='bind_hand' and x['ref']=='source');self.assertIn(dict(op='cast',ref='source',value=1),s[b:]);self.assertIn(dict(op='assert_card',ref='source',property='sick',value=1),s[b:]);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s[b:])
 def test_kazarov_two_damaged_opposing_creatures_give_two_counters(self):
  c=self.case('Kazarov, Sengir Pureblood','simultaneous_damage_0');self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='other_damage'),'Pyroclasm');self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=2),c['steps']);self.assertIn(dict(op='assert_card',ref='source',property='damage',value=2),c['steps']);self.assertFalse(any(x['op']=='activate' for x in c['steps']))

 def test_self_regeneration_uses_actual_entry_and_destruction_response(self):
  for name in ('Krakilin','Golgari Grave-Troll','Experiment One'):
   s=self.case(name,'effect_0')['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source');murder=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='removal');regen=next(i for i,x in enumerate(s) if x['op']=='choose_regeneration');self.assertLess(cast,murder);self.assertLess(murder,regen);self.assertFalse(any(x['op']=='activate' for x in s));self.assertFalse(any(x['op']=='choose_target' for x in s[regen:]));self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s[regen:])
 def test_experiment_real_evolve_requires_greater_owned_entry(self):
  for mode,count in [('effect',2),('one_evolve',1),('equal_evolve',1),('opponent_entry',1),('no_evolve',0)]:
   s=self.case('Experiment One',mode+'_1')['steps'];m=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='removal');self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=count),s[:m]);self.assertEqual(sum(x['op']=='cast' and x.get('ref','').startswith('entrant') for x in s),0 if mode=='no_evolve' else 1 if mode=='one_evolve' else 2)
 def test_grave_troll_last_counter_is_paid_without_saving_zero_toughness(self):
  s=self.case('Golgari Grave-Troll','last_counter_0')['steps'];r=next(i for i,x in enumerate(s) if x['op']=='choose_regeneration');self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s[:r]);self.assertIn(dict(op='assert_mana',player=0,color=0,value=25),s[r:]);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=7862,value=1),s[r:])
 def test_regeneration_heals_real_nonlethal_damage_after_counter_cost(self):
  for name in ('Krakilin','Golgari Grave-Troll','Experiment One'):
   s=self.case(name,'damaged_regen_0')['steps'];r=next(i for i,x in enumerate(s) if x['op']=='choose_regeneration');self.assertIn(dict(op='cast',ref='warmup',value=1),s[:r]);self.assertIn(dict(op='assert_card',ref='source',property='damage',value=1),s[:r]);self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),s[r:]);self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s[r:])
 def test_regeneration_second_destruction_has_live_positive_control(self):
  for name in ('Krakilin','Golgari Grave-Troll','Experiment One'):
   s=self.case(name,'second_destruction_1')['steps'];second=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='second_removal');self.assertEqual(sum(x['op']=='choose_regeneration' for x in s),1);self.assertIn(dict(op='require_card',ref='source',property='in_play',value=1),s[:second]);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows[name]['csvid'],value=1),s[second:])
 def test_self_regeneration_denials_keep_unconsumed_real_response(self):
  for name,mode in [('Krakilin','wrong_color'),('Golgari Grave-Troll','insufficient_mana'),('Experiment One','one_evolve')]:
   s=self.case(name,mode+'_1')['steps'];self.assertIn(dict(op='choose_regeneration',ref='source'),s);self.assertIn(dict(op='assert_unused_choices',value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows[name]['csvid'],value=1),s)
 def test_vampiric_non_tap_cost_allows_real_fresh_and_tapped_activations(self):
  for mode in ('fresh_source','tapped_source'):
   s=self.case('Vampiric Dragon',mode+'_0')['steps'];self.assertIn(dict(op='activate',ref='source',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s)
  for mode in ('insufficient_mana','wrong_color'):
   self.assertIn(dict(op='activate',ref='source',value=0),self.case('Vampiric Dragon',mode+'_1')['steps'])

 def test_regeneration_combat_growth_uses_real_cast_and_turn_untap(self):
  for name in ('Slith Bloodletter','Undercity Troll','Spiritmonger'):
   s=self.case(name,'effect_0')['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source');untap=next(i for i,x in enumerate(s) if x['op']=='untap_step');attack=next(i for i,x in enumerate(s) if x['op']=='attack' and x['ref']=='source');regen=next(i for i,x in enumerate(s) if x['op']=='choose_regeneration');self.assertLess(cast,untap);self.assertLess(untap,attack);self.assertLess(attack,regen);self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s[attack:regen])
 def test_each_combat_damage_and_renown_once_have_distinct_counters(self):
  for name,counters in [('Slith Bloodletter',2),('Undercity Troll',1)]:
   s=self.case(name,'repeat_combat_1')['steps'];self.assertEqual(sum(x['op']=='combat_damage' for x in s),2);self.assertEqual(sum(x['op']=='untap_step' for x in s),4);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=counters),s)
 def test_unleash_decline_and_real_counter_loss_restore_blocking(self):
  for mode,allowed in [('block_countered',0),('block_decline',1),('block_counter_removed',1)]:
   s=self.case('Grim Roustabout',mode+'_0')['steps'];self.assertIn(dict(op='block',ref='source',other='attacker',value=allowed),s);self.assertFalse(any(x['op']=='combat_damage' for x in s))
  s=self.case('Grim Roustabout','block_counter_removed_0')['steps'];self.assertIn(dict(op='activate',ref='counter_remover',value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Vampire Hexmage']['csvid'],value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s)
 def test_spiritmonger_counts_real_creature_damage_events(self):
  for mode,count in [('effect',1),('unblocked_combat',0),('prevented_combat',0)]:
   s=self.case('Spiritmonger',mode+'_0')['steps'];self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=count),s)
  s=self.case('Spiritmonger','effect_0')['steps'];self.assertIn(dict(op='block',ref='blocker',other='source',value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Wall of Tanglecord']['csvid'],value=1),s)
  self.assertIn(dict(op='cast',ref='preventer',value=1),self.case('Spiritmonger','prevented_combat_0')['steps']);self.assertIn(dict(op='assert_life',player=1,value=14),self.case('Spiritmonger','unblocked_combat_0')['steps'])
 def test_spiritmonger_regeneration_does_not_offer_optional_green_mode(self):
  s=self.case('Spiritmonger','effect_1')['steps'];r=next(i for i,x in enumerate(s) if x['op']=='choose_regeneration');self.assertIn(dict(op='set_mana',player=1,color=3,value=0),s[:r]);self.assertFalse(any(x['op']=='choose_color' for x in s));self.assertIn(dict(op='assert_mana',player=1,color=1,value=9),s[r:])
 def test_regeneration_followups_heal_real_damage_without_tap_cost(self):
  for name in ('Grim Roustabout','Slith Bloodletter','Undercity Troll','Spiritmonger'):
   s=self.case(name,'damaged_regen_1')['steps'];r=next(i for i,x in enumerate(s) if x['op']=='choose_regeneration');self.assertIn(dict(op='cast',ref='warmup',value=1),s[:r]);self.assertIn(dict(op='assert_card',ref='source',property='damage',value=1),s[:r]);self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),s[r:]);self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s[r:])

 def test_solarion_actual_sunburst_payment_and_zero_entry_death(self):
  s=self.case('Solarion','effect_0')['steps'];self.assertEqual([x['color'] for x in s if x['op']=='choose_mana'],[1,2,3,4]);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=4),s)
  s=self.case('Solarion','zero_colors_1')['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=7123,value=1),s);self.assertFalse(any(x['op']=='activate' for x in s))
 def test_solarion_doubles_current_counters_after_real_growth_response(self):
  s=self.case('Solarion','more_counters_pending_1')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source');g=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='growth');self.assertLess(a,g);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=4),s[a:g]);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=10),s[g:]);self.assertFalse(any(x['op']=='resolve_response' for x in s));self.assertFalse(any(x.get('property')=='plus1_counters' and x.get('ref')=='source' and x.get('value')==5 for x in s[g:]))
 def test_solarion_counter_removal_causes_real_zero_toughness_death(self):
  s=self.case('Solarion','counter_removed_pending_0')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='counter_remover');self.assertIn(dict(op='assert_stack',value=2),s[a:]);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Vampire Hexmage']['csvid'],value=1),s[a:]);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=7123,value=1),s[a:])
 def test_solarion_repeat_uses_real_untapper_after_first_resolution(self):
  s=self.case('Solarion','repeat_0')['steps'];self.assertEqual(sum(x['op']=='activate' and x['ref']=='source' for x in s),2);self.assertIn(dict(op='activate',ref='untapper',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=16),s);self.assertNotIn('stacked',self.declaration('Solarion','effect_0')['params']['modes'])
 def test_cohort_actual_cost_taps_distinct_fresh_ally_and_source(self):
  s=self.case("Munda's Vanguard",'fresh_ally_0')['steps'];c=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='ally');a=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source');r=next(i for i in range(a,len(s)) if s[i]['op']=='resolve');self.assertLess(c,a);self.assertIn(dict(op='assert_card',ref='ally',property='sick',value=1),s[c:a]);self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s[a:r]);self.assertIn(dict(op='assert_card',ref='ally',property='tapped',value=1),s[a:r]);self.assertIn(dict(op='assert_card',ref='artifact_control',property='plus1_counters',value=1),s[r:]);self.assertIn(dict(op='assert_card',ref='enemy_control',property='plus1_counters',value=0),s[r:])
 def test_cohort_unavailable_costs_deny_actual_activation(self):
  for mode in ('source_alone','opponent_ally_only','tapped_ally','tapped_source','fresh_source'):
   s=self.case("Munda's Vanguard",mode+'_1')['steps'];self.assertIn(dict(op='activate',ref='source',value=0),s);self.assertFalse(any(x['op']=='choose_target' and x['player']==1 for x in s))
 def test_transmogrant_pays_self_sacrifice_and_keeps_creature_subtypes(self):
  s=self.case("Ashnod's Transmogrant",'effect_1')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source');r=next(i for i in range(a,len(s)) if s[i]['op']=='resolve');self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=476,value=1),s[a:r]);self.assertIn(dict(op='assert_card',ref='recipient',property='plus1_counters',value=0),s[a:r]);self.assertIn(dict(op='assert_card',ref='recipient',property='plus1_counters',value=1),s[r:])
  for prop in ('artifact','subtype_Human','subtype_Soldier'):self.assertIn(dict(op='assert_matching_permanents',player=1,index=self.rows['Eager Cadet']['csvid'],property=prop,value=1),s[r:])
 def test_transmogrant_new_artifact_has_actual_shatter_control(self):
  c=self.case("Ashnod's Transmogrant",'artifact_destroy_probe_0');s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='artifact_removal');self.assertIn(dict(op='require_can_cast',ref='artifact_removal',value=1),s[:cast]);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Eager Cadet']['csvid'],value=1),s[cast:]);self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='artifact_removal'),'Shatter')
 def test_next_three_responses_use_only_supported_whole_drain(self):
  for name in ('Solarion',"Munda's Vanguard","Ashnod's Transmogrant"):
   for variant in growth.variants(growth.WAVE6_PINS['profiles'][name]['params']):self.assertFalse(any(s['op']=='resolve_response' for s in self.case(name,variant)['steps']))

 def test_gyre_evolve_axes_are_separate_and_zero_output_still_pays_tap(self):
  for mode in ('power_axis','toughness_axis'):
   s=self.case('Gyre Sage',mode+'_0')['steps'];self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s);self.assertIn(dict(op='assert_mana',player=0,color=3,value=1),s)
  s=self.case('Gyre Sage','no_growth_1')['steps'];self.assertIn(dict(op='require_can_activate',ref='source',value=1),s);self.assertIn(dict(op='activate',ref='source',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s);self.assertIn(dict(op='assert_mana',player=1,color=3,value=0),s)
 def test_marwyn_counts_actual_owned_elf_and_changeling_entries(self):
  c=self.case('Marwyn, the Nurturer','effect_1');self.assertEqual([e['name'] for e in c['setup'] if e['ref'].startswith('entrant')],['Elvish Mystic','Woodland Changeling']);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=2),c['steps']);self.assertIn(dict(op='assert_mana',player=1,color=3,value=3),c['steps'])
  for mode in ('nonmatching_entry','opponent_entry'):
   s=self.case('Marwyn, the Nurturer',mode+'_1')['steps'];self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s);self.assertIn(dict(op='assert_mana',player=1,color=3,value=1),s)
 def test_mana_growth_uses_counter_count_or_current_power_as_printed(self):
  for name,output in [('Gyre Sage',2),('Marwyn, the Nurturer',6)]:
   s=self.case(name,'power_boost_0')['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source');self.assertIn(dict(op='cast',ref='boost',value=1),s[:a]);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=2),s[a:]);self.assertIn(dict(op='assert_mana',player=0,color=3,value=output),s[a:]);self.assertEqual(s[a+1],dict(op='assert_stack',value=0))
 def test_hierarch_real_renown_then_vigilance_and_all_color_choices(self):
  s=self.case('Honored Hierarch','second_attack_0')['steps'];attacks=[i for i,x in enumerate(s) if x['op']=='attack' and x['ref']=='source'];self.assertEqual(len(attacks),2);self.assertEqual(s[attacks[0]+1],dict(op='assert_card',ref='source',property='tapped',value=1));self.assertEqual(s[attacks[1]+1],dict(op='assert_card',ref='source',property='tapped',value=0));self.assertIn(dict(op='assert_life',player=1,value=17),s);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s)
  for mode,color in [('effect',3),('color_B',1),('color_U',2),('color_R',4),('color_W',5)]:
   s=self.case('Honored Hierarch',mode+'_1')['steps'];self.assertIn(dict(op='choose_color',player=1,value=color),s);self.assertIn(dict(op='assert_mana',player=1,color=color,value=1),s)
 def test_hierarch_counter_without_renown_has_no_granted_mana_ability(self):
  s=self.case('Honored Hierarch','counter_is_not_renown_0')['steps'];self.assertIn(dict(op='cast',ref='growth',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s);self.assertIn(dict(op='activate',ref='source',value=0),s);self.assertFalse(any(x['op']=='choose_color' for x in s))
 def test_hierarch_remains_renowned_after_actual_counter_removal(self):
  s=self.case('Honored Hierarch','counter_removed_1')['steps'];r=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='counter_remover');self.assertIn(dict(op='combat_damage'),s[:r]);self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s[r:]);self.assertIn(dict(op='activate',ref='source',value=1),s[r:]);self.assertIn(dict(op='assert_mana',player=1,color=3,value=1),s[r:])
 def test_produced_mana_is_spent_and_persists_after_actual_source_loss(self):
  for name,output in [('Gyre Sage',2),('Marwyn, the Nurturer',3),('Honored Hierarch',1)]:
   s=self.case(name,'spend_output_0')['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='spender');self.assertIn(dict(op='assert_mana',player=0,color=3,value=output-1),s[cast:]);s=self.case(name,'source_destroyed_after_mana_0')['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='response');self.assertIn(dict(op='assert_mana',player=0,color=3,value=output),s[cast:]);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[name]['csvid'],value=1),s[cast:])

if __name__=='__main__':unittest.main()
