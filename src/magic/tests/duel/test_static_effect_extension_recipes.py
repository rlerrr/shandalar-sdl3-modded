import copy,json,sys,unittest
from pathlib import Path
from generate_cases import registry,OPS,PROPERTIES,TOKEN_PROPERTIES
from recipes import expand as dispatch
from static_effect_extension_recipes import REVIEWS,FAMILY,spec,variants,expand

class StaticEffectExtensionTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name,selected):return dict(family=FAMILY,version=2,oracle_text=self.oracle[name]['oracle_text'],params=copy.deepcopy(REVIEWS['sources'][name]['params']),variants=list(selected))
 def cases(self,name,selected):
  d=self.declaration(name,selected);p=spec(d,self.oracle[name]);return expand(name,self.rows[name]['csvid'],d,p,selected,self.rows,self.oracle)
 def test_full_source_and_explicit_profile_fail_closed(self):
  for name in REVIEWS['sources']:
   d=self.declaration(name,['effect_0'])
   for key,value in [('oracle_text','changed'),('mana_cost','{0}'),('cmc',99),('keywords',['Unreviewed keyword'])]:
    r=copy.deepcopy(self.oracle[name]);r[key]=value
    with self.assertRaises(ValueError):spec(d,r)
   for version in (1,True,'2'):
    bad=copy.deepcopy(d);bad['version']=version
    with self.assertRaises(ValueError):spec(bad,self.oracle[name])
   bad=copy.deepcopy(d);bad['params']['scope']='both' if bad['params']['scope']!='both' else 'own'
   with self.assertRaises(ValueError):spec(bad,self.oracle[name])
 def test_public_dispatch_and_strict_distinct_variants(self):
  for name in REVIEWS['sources']:
   d=self.declaration(name,['effect_0']);p=spec(d,self.oracle[name]);direct=expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle);self.assertEqual(direct,dispatch(name,self.rows[name]['csvid'],d,self.oracle[name],self.rows,oracle_context=self.oracle))
   for selected in ([],['effect_0','effect_0'],['invented_0']):
    with self.assertRaises(ValueError):expand(name,self.rows[name]['csvid'],d,p,selected,self.rows,self.oracle)
 def test_all_private_witnesses_pinned_and_no_identity_collisions(self):
  from static_effect_extension_recipes import validate_witnesses
  bad=copy.deepcopy(self.oracle);bad['Elvish Mystic']['cmc']=0
  with self.assertRaises(ValueError):validate_witnesses(bad,self.rows)
  self.assertNotIn('Llanowar Elves',REVIEWS['witnesses']);self.assertEqual(self.oracle['Elvish Mystic']['cmc'],1)
 def test_token_record_cannot_be_used_as_castable_physical_sliver(self):
  from static_effect_extension_recipes import _World
  self.assertEqual(self.oracle['Metallic Sliver']['layout'],'token');self.assertEqual(self.oracle['Sliver Construct']['layout'],'normal');self.assertEqual(self.oracle['Sliver Construct']['mana_cost'],'{3}');self.assertNotIn('Metallic Sliver',REVIEWS['witnesses'])
  world=_World(REVIEWS['sources']['Fury Sliver']['params'],0,self.rows,self.oracle,False)
  with self.assertRaises(ValueError):world.add('Metallic Sliver','invalid',0,'hand')
  with self.assertRaises(ValueError):world.add('Metallic Sliver','invalid',0,'battlefield')
  c=self.cases('Fury Sliver',['late_arrival_0'])[0];self.assertIn(dict(op='assert_mana',player=0,color=0,value=17),c['steps']);self.assertTrue(any(x['name']=='Sliver Construct' and x['zone']=='hand' for x in c['setup']))
 def test_all_generated_ops_and_refs_have_existing_schema(self):
  collision={'Llanowar Elves','Nightmare','Mutavault',"Ajani's Pridemate",'Delver of Secrets','Anointer Priest'}
  for name in REVIEWS['sources']:
   d=self.declaration(name,['placeholder']);p=spec(d,self.oracle[name])
   for c in self.cases(name,variants(p)):
    self.assertFalse({x['name'] for x in c['setup']}&collision);refs={x['ref'] for x in c['setup']}
    for step in c['steps']:
     self.assertIn(step['op'],OPS)
     if 'property' in step:self.assertIn(step['property'],PROPERTIES|(TOKEN_PROPERTIES if step['op']=='assert_matching_permanents' else set()))
     if 'ref' in step:self.assertIn(step['ref'],refs)
 def test_source_actual_payment_receipt_precedes_refill(self):
  from recipes import cost_from_oracle
  for name in REVIEWS['sources']:
   for owner in (0,1):
    c=self.cases(name,[f'payment_{owner}'])[0];s=c['steps'];cast=s.index(dict(op='cast',ref='source',value=1));refill=next(i for i,x in enumerate(s[cast+1:],cast+1) if x['op']=='set_mana');cost=cost_from_oracle(self.oracle[name])
    for color,amount in enumerate(cost):self.assertIn(dict(op='set_mana',player=owner,color=color,value=amount),s[:cast])
    for color in range(6):self.assertIn(dict(op='assert_mana',player=owner,color=color,value=0),s[cast:refill])
 def test_failed_payment_does_not_continue_to_refill_or_resolve(self):
  for name in REVIEWS['sources']:
   d=self.declaration(name,['placeholder']);supported=variants(spec(d,self.oracle[name]))
   for v in [x for x in ('insufficient_mana_0','wrong_color_1') if x in supported]:
    c=self.cases(name,[v])[0];s=c['steps'];self.assertIn(dict(op='cast',ref='source',value=0),s);self.assertFalse(any(x['op']=='resolve' for x in s))
    for item in c['setup']:
     if item['zone']=='battlefield' and item['name']=='Forest':self.assertIn('tapped',item['state'])
 def test_pending_spell_physical_hand_and_card_status_are_separate(self):
  s=self.cases('Levitation',['pending_snapshot_0'])[0]['steps'];cast=s.index(dict(op='cast',ref='source',value=1));capture=s.index(dict(op='capture'));self.assertIn(dict(op='assert_card',ref='source',property='in_hand',value=0),s[cast:capture]);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Levitation']['csvid'],value=1),s[cast:capture])
 def test_haste_is_real_fresh_cast_attack_and_mana_activation(self):
  for owner in (0,1):
   s=self.cases('Fervor',[f'effect_{owner}'])[0]['steps'];self.assertIn(dict(op='cast',ref=f'late_{owner}',value=1),s);self.assertIn(dict(op='attack',ref=f'late_{owner}',value=1),s);self.assertIn(dict(op='cast',ref=f'mana_elf_{owner}',value=1),s);self.assertIn(dict(op='activate',ref=f'mana_elf_{owner}',value=1),s)
   s=self.cases('Fervor',[f'countered_{owner}'])[0]['steps'];self.assertIn(dict(op='attack',ref=f'late_{owner}',value=0),s);self.assertIn(dict(op='activate',ref=f'mana_elf_{owner}',value=0),s)
 def test_tribal_nonmatching_and_changeling_are_actual_haste_controls(self):
  s=self.cases('Goblin Chieftain',['nonmatching_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='late_nonmatch_0',value=1),s);self.assertIn(dict(op='attack',ref='late_nonmatch_0',value=0),s)
  s=self.cases('Goblin Chieftain',['changeling_1'])[0]['steps'];self.assertIn(dict(op='cast',ref='late_changeling_1',value=1),s);self.assertIn(dict(op='attack',ref='late_changeling_1',value=1),s)
 def test_global_double_strike_and_source_inclusion_have_real_damage(self):
  s=self.cases('Fury Sliver',['effect_0'])[0]['steps'];self.assertIn(dict(op='assert_life',player=1,value=16),s)
  c=self.cases('Fury Sliver',['source_recipient_1'])[0];self.assertEqual(next(x['zone'] for x in c['setup'] if x['ref']=='source'),'battlefield');self.assertIn(dict(op='attack',ref='source',value=1),c['steps']);self.assertIn(dict(op='assert_life',player=0,value=14),c['steps'])
 def test_source_loss_checks_live_effect_before_restore_then_absence(self):
  for name in ('Fervor','Levitation','Bedlam'):
   s=self.cases(name,['source_destroyed_0'])[0]['steps'];capture=s.index(dict(op='capture'));restore=s.index(dict(op='restore'));destroy=s.index(dict(op='destroy',ref='source'));self.assertLess(capture,restore);self.assertLess(restore,destroy);self.assertTrue(any(x['op']=='combat_damage' for x in s[capture:restore]));self.assertTrue(any(x['op']=='combat_damage' for x in s[destroy:]))
 def test_counter_response_is_real_stack_two_then_drained(self):
  for name in REVIEWS['sources']:
   s=self.cases(name,['countered_1'])[0]['steps'];a=s.index(dict(op='cast_response',ref='counter',value=1));b=s.index(dict(op='resolve_response'));self.assertIn(dict(op='assert_stack',value=2),s[a:b]);self.assertIn(dict(op='assert_stack',value=0),s[b:])
 def test_flying_block_denial_and_block_permission_removal(self):
  s=self.cases('Levitation',['effect_0'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker_1',other='match_0',value=0),s)
  s=self.cases('Bedlam',['source_destroyed_1'])[0]['steps'];destroy=s.index(dict(op='destroy',ref='source'));self.assertIn(dict(op='block',ref='blocker_0',other='match_1',value=0),s[:destroy]);self.assertIn(dict(op='block',ref='blocker_0',other='match_1',value=1),s[destroy:])
 def test_turn_bonus_changes_in_real_blocking_and_damage_leaves_toughness(self):
  s=self.cases('Glory of Warfare',['damaged_turn_transition_0'])[0]['steps'];self.assertIn(dict(op='block',ref='match_0',other='blocker_1',value=1),s);self.assertIn(dict(op='assert_card',ref='match_0',property='toughness',value=4),s);self.assertIn(dict(op='assert_card',ref='match_0',property='damage',value=3),s);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=1),s)
 def test_token_negative_stats_and_zero_toughness_conserve_non_tokens(self):
  c=self.cases('Illness in the Ranks',['effect_0'])[0];s=c['steps'];self.assertIn('token',next(x['state'] for x in c['setup'] if x['ref']=='match_0'));self.assertNotIn('token',next(x['state'] for x in c['setup'] if x['ref']=='nonmatch_0'));self.assertIn(dict(op='assert_card',ref='match_0',property='power',value=1),s);self.assertIn(dict(op='assert_card',ref='nonmatch_0',property='power',value=2),s);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Suntail Hawk']['csvid'],value=0),s)
 def test_entry_type_and_controller_predicates_use_actual_casts(self):
  for mode,name,tapped in [('effect','Raging Goblin',1),('own_entry','Raging Goblin',0),('entry_artifact','Sol Ring',1),('entry_land','Forest',1),('entry_enchantment','Seal of Fire',0),('entry_artifact_creature','Ornithopter',1)]:
   c=self.cases('Kismet',[mode+'_0'])[0];actor=0 if mode=='own_entry' else 1;s=c['steps'];self.assertIn(dict(op='activate',ref=f'atlas_{actor}',value=1) if name=='Forest' else dict(op='cast',ref=f'entry_{actor}',value=1),s);self.assertIn(dict(op='assert_card',ref=f'entry_{actor}',property='tapped',value=tapped),s)
   if name in ('Sol Ring','Forest'):self.assertIn(dict(op='activate',ref=f'entry_{actor}',value=0),s)
 def test_land_entry_uses_real_atlas_activation_not_a_land_spell(self):
  for owner in (0,1):
   c=self.cases('Kismet',[f'entry_land_{owner}'])[0];s=c['steps'];actor=1-owner;self.assertIn('untapped',next(x['state'] for x in c['setup'] if x['ref']==f'land_{actor}'));self.assertIn(dict(op='assert_card',ref=f'land_{actor}',property='tapped',value=0),s);self.assertFalse(any(x['op']=='cast' and x.get('ref')==f'entry_{actor}' for x in s));i=s.index(dict(op='activate',ref=f'atlas_{actor}',value=1));self.assertEqual(s[i+1],dict(op='assert_stack',value=1));self.assertIn(dict(op='choose_zone_card',player=actor,value=self.rows['Forest']['csvid']),s[i:]);self.assertIn(dict(op='bind_permanent_copy',player=actor,value=self.rows['Forest']['csvid'],index=1,ref=f'entry_{actor}'),s[i:]);self.assertIn(dict(op='assert_card',ref=f'atlas_{actor}',property='tapped',value=1),s[i:])
 def test_foreign_controller_applies_and_destroy_returns_to_original_owner(self):
  for owner in (0,1):
   c=self.cases('Levitation',[f'foreign_destroyed_{owner}'])[0];item=next(x for x in c['setup'] if x['ref']==f'match_{owner}');self.assertEqual(item['player'],owner);self.assertIn('opponent_owned',item['state']);self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=1),c['steps'])
 def test_snapshot_tail_is_equal_but_not_aliased(self):
  for name in REVIEWS['sources']:
   s=self.cases(name,['pending_snapshot_1'])[0]['steps'];ops=[x['op'] for x in s];a=ops.index('capture');b=ops.index('record_state');r=ops.index('restore');self.assertEqual(s[a+1:b],s[r+1:-1]);self.assertIsNot(s[a+1],s[r+1])

 def test_trample_uses_real_pump_blocker_and_excess_damage(self):
  s=self.cases('Horned Sliver',['effect_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='combat_pump_0',value=1),s);self.assertIn(dict(op='block',ref='blocker_1',other='match_0',value=1),s);self.assertIn(dict(op='assert_life',player=1,value=17),s)
  s=self.cases('Horned Sliver',['countered_0'])[0]['steps'];self.assertIn(dict(op='assert_life',player=1,value=20),s)
 def test_first_strike_prevents_actual_return_damage(self):
  s=self.cases('Talon Sliver',['effect_0'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker_1',other='match_0',value=1),s);self.assertIn(dict(op='assert_card',ref='match_0',property='damage',value=0),s);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Savannah Lions']['csvid'],value=1),s)
  s=self.cases('Talon Sliver',['countered_0'])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Sliver Construct']['csvid'],value=1),s)
 def test_late_trample_and_vigilance_use_real_temporary_haste_spell(self):
  for name in ('Horned Sliver',"Serra's Blessing"):
   c=self.cases(name,['late_arrival_1'])[0];s=c['steps'];self.assertIn(dict(op='cast',ref='late_1',value=1),s);self.assertIn(dict(op='cast',ref='late_haste',value=1),s);self.assertIn(dict(op='attack',ref='late_1',value=1),s);self.assertTrue(any(x['name']=='Burst of Speed' and x['zone']=='hand' for x in c['setup']))
  s=self.cases("Serra's Blessing",['late_arrival_1'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='late_1',property='tapped',value=0),s)
 def test_vigilance_token_and_nontoken_predicates_are_distinct_status_flags(self):
  a=self.cases('Intangible Virtue',['effect_0'])[0];b=self.cases('Always Watching',['nonmatching_0'])[0]
  self.assertIn('token',next(x['state'] for x in a['setup'] if x['ref']=='match_0'));self.assertNotIn('token',next(x['state'] for x in a['setup'] if x['ref']=='nonmatch_0'));self.assertIn(dict(op='assert_card',ref='match_0',property='tapped',value=0),a['steps'])
  self.assertIn('token',next(x['state'] for x in b['setup'] if x['ref']=='nonmatch_0'));self.assertIn(dict(op='assert_card',ref='nonmatch_0',property='tapped',value=1),b['steps'])
 def test_true_conviction_checks_both_double_damage_and_lifelink(self):
  s=self.cases('True Conviction',['effect_1'])[0]['steps'];self.assertIn(dict(op='assert_life',player=0,value=16),s);self.assertIn(dict(op='assert_life',player=1,value=24),s)
 def test_entry_types_creature_only_do_not_tap_artifact_land_controls(self):
  for mode in ('entry_artifact_0','entry_land_0'):
   s=self.cases('Uphill Battle',[mode])[0]['steps'];self.assertIn(dict(op='assert_card',ref='entry_1',property='tapped',value=0),s);self.assertIn(dict(op='activate',ref='entry_1',value=1),s)
 def test_virulent_plague_preserves_surviving_positive_token_receipt(self):
  c=self.cases('Virulent Plague',['effect_0'])[0];self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='match_0'),'Hill Giant');self.assertIn(dict(op='assert_card',ref='match_0',property='power',value=1),c['steps']);self.assertIn(dict(op='assert_card',ref='nonmatch_0',property='power',value=2),c['steps'])

 def test_numeric_source_witness_and_parameter_pins_reject_boolean_aliases(self):
  from static_effect_extension_recipes import validate_witnesses
  r=copy.deepcopy(self.oracle['Mass Hysteria']);r['cmc']=True
  with self.assertRaises(ValueError):spec(self.declaration('Mass Hysteria',['effect_0']),r)
  bad=copy.deepcopy(self.oracle);bad['Ornithopter']['cmc']=False
  with self.assertRaises(ValueError):validate_witnesses(bad,self.rows)
  d=self.declaration('Phantom General',['effect_0']);d['params']['power']=True
  with self.assertRaises(ValueError):spec(d,self.oracle['Phantom General'])
 def test_nonflying_and_protected_player_attack_restrictions(self):
  s=self.cases('Moat',['effect_0'])[0]['steps'];self.assertIn(dict(op='attack',ref='match_0',value=0),s)
  s=self.cases('Moat',['nonmatching_1'])[0]['steps'];self.assertIn(dict(op='attack',ref='nonmatch_1',value=1),s)
  s=self.cases('Blazing Archon',['effect_0'])[0]['steps'];self.assertIn(dict(op='attack',ref='match_1',value=0),s)
  s=self.cases('Blazing Archon',['own_attacker_0'])[0]['steps'];self.assertIn(dict(op='attack',ref='match_0',value=1),s)
 def test_power_attack_boundary_changes_with_real_growth(self):
  s=self.cases('Reverence',['effect_0'])[0]['steps'];self.assertIn(dict(op='attack',ref='match_1',value=0),s)
  s=self.cases('Reverence',['pumped_attacker_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='permission_pump_1',value=1),s);self.assertIn(dict(op='assert_card',ref='match_1',property='power',value=5),s);self.assertIn(dict(op='attack',ref='match_1',value=1),s)
 def test_block_rules_isolate_color_and_attacking_controller(self):
  s=self.cases('Razorjaw Oni',['effect_0'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker_1',other='match_0',value=0),s)
  s=self.cases('Razorjaw Oni',['ground_blocker_0'])[0]['steps'];self.assertIn(dict(op='block',ref='ground_blocker_1',other='match_0',value=1),s)
  s=self.cases('Razorjaw Oni',['source_blocker_0'])[0]['steps'];self.assertIn(dict(op='block',ref='source',other='nonmatch_1',value=0),s)
  s=self.cases('Bower Passage',['opponent_recipient_0'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker_0',other='match_1',value=1),s)
 def test_wall_permission_retains_defender_and_nonwall_negative(self):
  s=self.cases('Rolling Stones',['effect_0'])[0]['steps'];self.assertIn(dict(op='attack',ref='match_0',value=1),s)
  s=self.cases('Rolling Stones',['nonmatching_0'])[0]['steps'];self.assertIn(dict(op='attack',ref='nonmatch_0',value=0),s)
  s=self.cases('Rolling Stones',['late_arrival_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='late_haste',value=1),s);self.assertIn(dict(op='attack',ref='late_0',value=1),s)
 def test_color_replacement_observes_exact_masks_and_real_nonblack_target_filter(self):
  s=self.cases('Darkest Hour',['effect_0'])[0]['steps'];self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows['Savannah Lions']['csvid'],property='color',value=2),s);self.assertIn(dict(op='assert_matching_permanents',player=1,index=self.rows['Sliver Construct']['csvid'],property='color',value=2),s);self.assertIn(dict(op='assert_matching_permanents',player=0,index=self.rows['Seal of Fire']['csvid'],property='color',value=16),s);self.assertIn(dict(op='can_cast',ref='color_probe_0',value=0),s)
  s=self.cases('Darkest Hour',['countered_0'])[0]['steps'];self.assertIn(dict(op='cast',ref='color_probe_0',value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Savannah Lions']['csvid'],value=1),s)
 def test_color_filter_uses_handler_not_unrestricted_generic_target_query(self):
  d=self.declaration('Darkest Hour',['placeholder']);p=spec(d,self.oracle['Darkest Hour'])
  for c in self.cases('Darkest Hour',variants(p)):
   self.assertFalse(any(s['op']=='can_target_creature' for s in c['steps']))
   if not any(s['op']=='cast' and s.get('value')==0 for s in c['steps']):self.assertTrue(any(s['op'] in ('can_cast','cast') and s.get('ref','').startswith('color_probe_') for s in c['steps']))
 def test_flying_and_islandwalk_losses_use_actual_block_legality(self):
  s=self.cases('Gravity Sphere',['effect_0'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker_1',other='match_0',value=1),s)
  s=self.cases('Gravity Sphere',['countered_0'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker_1',other='match_0',value=0),s)
  s=self.cases('Mystic Decree',['islandwalk_0'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker_1',other='walker_0',value=1),s);self.assertIn(dict(op='remove_from_combat',ref='walker_0'),s);self.assertIn(dict(op='assert_life',player=1,value=20),s)
 def test_named_restriction_covers_both_attack_and_block_without_nested_snapshots(self):
  for name in ('Paper Tiger','Rock Lobster','Scissors Lizard'):
   s=self.cases(name,['effect_0'])[0]['steps'];self.assertIn(dict(op='attack',ref='match_0',value=0),s)
   s=self.cases(name,['blocked_receiver_0'])[0]['steps'];self.assertIn(dict(op='block',ref='match_0',other='nonmatch_1',value=0),s)
   s=self.cases(name,['block_countered_1'])[0]['steps'];self.assertIn(dict(op='block',ref='match_1',other='nonmatch_0',value=1),s);self.assertIn(dict(op='cast_response',ref='counter',value=1),s)

 def test_conditional_source_boosts_follow_real_arrival_and_departure(self):
  for owner in (0,1):
   s=self.cases('Angelic Voices',[f'condition_appears_{owner}'])[0]['steps'];i=s.index(dict(op='cast',ref='condition_control',value=1));self.assertIn(dict(op='assert_card',ref='match_'+str(owner),property='power',value=3),s[:i]);self.assertIn(dict(op='assert_card',ref='match_'+str(owner),property='power',value=2),s[i:])
   s=self.cases('Angelic Voices',[f'condition_removed_{owner}'])[0]['steps'];i=s.index(dict(op='destroy',ref='condition_control'));self.assertIn(dict(op='assert_card',ref='match_'+str(owner),property='power',value=2),s[:i]);self.assertIn(dict(op='assert_card',ref='match_'+str(owner),property='power',value=3),s[i:])
 def test_common_color_condition_intersects_all_nonartifact_creatures(self):
  for owner in (0,1):
   c=self.cases('Common Cause',[f'condition_overlap_{owner}'])[0];names={x['name'] for x in c['setup'] if x['zone']=='battlefield' and 'Creature' in self.oracle[x['name']]['type_line'] and 'Artifact' not in self.oracle[x['name']]['type_line']};self.assertEqual(names,{'Goblin Legionnaire','Watchwolf','Scab-Clan Mauler'});self.assertFalse(set.intersection(*(set(self.oracle[n]['colors']) for n in names)));self.assertIn(dict(op='assert_card',ref='match_'+str(owner),property='power',value=2),c['steps'])
 def test_threshold_counts_actual_graveyard_and_morningtide_returns_itself(self):
  for owner in (0,1):
   for name in ('Divine Sacrament','Silver Seraph'):
    s=self.cases(name,[f'threshold_increase_{owner}'])[0]['steps'];i=s.index(dict(op='destroy',ref='threshold_victim'));self.assertIn(dict(op='assert_zone',player=owner,zone='graveyard',value=6),s[:i]);self.assertIn(dict(op='assert_zone',player=owner,zone='graveyard',value=7),s[i:])
    s=self.cases(name,[f'threshold_decrease_{owner}'])[0]['steps'];i=s.index(dict(op='cast',ref='grave_reset',value=1));self.assertIn(dict(op='assert_zone',player=owner,zone='graveyard',value=7),s[:i]);self.assertIn(dict(op='assert_zone',player=owner,zone='graveyard',value=1),s[i:]);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Morningtide']['csvid'],value=1),s[i:])
 def test_gate_count_preserves_zero_gate_vigilance_and_real_land_entry(self):
  for owner in (0,1):
   s=self.cases('Hold the Gates',[f'gates0_{owner}'])[0]['steps'];self.assertIn(dict(op='attack',ref='match_'+str(owner),value=1),s);self.assertIn(dict(op='assert_card',ref='match_'+str(owner),property='tapped',value=0),s)
   s=self.cases('Hold the Gates',[f'gate_enters_{owner}'])[0]['steps'];self.assertIn(dict(op='activate',ref='atlas_'+str(owner),value=1),s);self.assertIn(dict(op='assert_card',ref='new_gate',property='tapped',value=1),s);self.assertFalse(any(x['op']=='cast' and x.get('ref')=='new_gate' for x in s));self.assertIn(dict(op='assert_card',ref='match_'+str(owner),property='toughness',value=4),s)
 def test_artifact_cda_counts_source_and_other_master_bonus_separately(self):
  for owner in (0,1):
   s=self.cases('Master of Etherium',[f'effect_{owner}'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),s)
   s=self.cases('Master of Etherium',[f'artifact_enters_{owner}'])[0]['steps'];i=s.index(dict(op='cast',ref='new_artifact',value=1));self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),s[:i]);self.assertIn(dict(op='assert_card',ref='source',property='power',value=5),s[i:])
   s=self.cases('Master of Etherium',[f'two_sources_{owner}'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='power',value=6),s);self.assertIn(dict(op='assert_card',ref='second',property='power',value=6),s)
 def test_equipment_condition_uses_unattached_owned_equipment_not_other_artifacts(self):
  for owner in (0,1):
   c=self.cases('Weapons Trainer',[f'equipment_opponent_{owner}'])[0];self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='equipment'),1-owner);self.assertIn(dict(op='assert_card',ref='match_'+str(owner),property='power',value=2),c['steps'])
   s=self.cases('Weapons Trainer',[f'equipment_appears_{owner}'])[0]['steps'];i=s.index(dict(op='cast',ref='equipment',value=1));self.assertIn(dict(op='assert_card',ref='match_'+str(owner),property='power',value=2),s[:i]);self.assertIn(dict(op='assert_card',ref='match_'+str(owner),property='power',value=3),s[i:]);self.assertFalse(any(x['op']=='activate' for x in s))
 def test_obelisk_normal_cost_choice_and_actual_untap_are_independent(self):
  for owner in (0,1):
   c=self.cases('Obelisk of Urd',[f'payment_{owner}'])[0];self.assertTrue(all('tapped' in x.get('state',[]) for x in c['setup'] if x['zone']=='battlefield' and x['player']==owner and 'Creature' in self.oracle[x['name']]['type_line']));s=c['steps'];a=s.index(dict(op='cast',ref='source',value=1));b=s.index(dict(op='choose_dialog',player=owner,value=2));r=s.index(dict(op='resolve'));self.assertLess(a,b);self.assertLess(b,r);self.assertIn(dict(op='choose_target',player=owner,ref='match_'+str(owner)),s[b:r]);self.assertIn(dict(op='assert_mana',player=owner,color=0,value=0),s[a:b]);self.assertIn(dict(op='cast',ref='untapper_'+str(owner),value=1),s);self.assertIn(dict(op='choose_dialog',player=owner,value=1),s)
   self.assertIn(dict(op='choose_dialog',player=owner,value=0),s[b:r])
 def test_reach_is_real_flying_block_permission_with_source_absence_control(self):
  for owner in (0,1):
   for name in ('Spinneret Sliver','Spidersilk Armor'):
    s=self.cases(name,[f'effect_{owner}'])[0]['steps'];self.assertIn(dict(op='block',ref='match_'+str(owner),other='air_'+str(1-owner),value=1),s)
    s=self.cases(name,[f'countered_{owner}'])[0]['steps'];self.assertIn(dict(op='block',ref='match_'+str(owner),other='air_'+str(1-owner),value=0),s)
 def test_landwalk_uses_defending_land_type_and_actual_block_refusal(self):
  for owner in (0,1):
   for name in ('Elvish Champion','Master of the Pearl Trident','Eladamri, Lord of Leaves'):
    s=self.cases(name,[f'effect_{owner}'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker_'+str(1-owner),other='match_'+str(owner),value=0),s)
    for mode in ('no_walk_land','other_walk_land'):
     s=self.cases(name,[f'{mode}_{owner}'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker_'+str(1-owner),other='match_'+str(owner),value=1),s)
 def test_other_elf_shroud_uses_supported_unsummon_query_and_late_entrant(self):
  for owner in (0,1):
   s=self.cases('Eladamri, Lord of Leaves',[f'late_arrival_{owner}'])[0]['steps'];self.assertIn(dict(op='can_target_creature',ref='shroud_probe_'+str(owner),other='late_'+str(owner),value=0),s);self.assertIn(dict(op='can_target_creature',ref='shroud_probe_'+str(owner),other='source',value=1),s);self.assertIn(dict(op='can_target_creature',ref='shroud_probe_'+str(owner),other='nonmatch_'+str(owner),value=1),s)
   s=self.cases('Eladamri, Lord of Leaves',[f'countered_{owner}'])[0]['steps'];self.assertIn(dict(op='cast',ref='shroud_probe_'+str(owner),value=1),s)

 def test_distinct_shroud_transition_and_real_artifact_token_departure(self):
  for owner in (0,1):
   c=self.cases('Eladamri, Lord of Leaves',[f'shroud_{owner}'])[0];s=c['steps'];a=s.index(dict(op='cast',ref='shroud_removal',value=1));b=s.index(dict(op='cast',ref='shroud_probe_'+str(owner),value=1));self.assertLess(a,b);self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='shroud_removal'),1-owner);self.assertIn(dict(op='can_target_creature',ref='shroud_probe_'+str(owner),other='match_'+str(1-owner),value=0),s[:a]);self.assertIn(dict(op='can_target_creature',ref='shroud_probe_'+str(owner),other='match_'+str(1-owner),value=1),s[a:b]);self.assertTrue(any(x['op']=='combat_damage' for x in s[:a]));self.assertIn('noncreature Kindred Elf permanents are not represented',REVIEWS['sources']['Eladamri, Lord of Leaves']['params']['gaps'])
   c=self.cases('Master of Etherium',[f'artifact_token_{owner}'])[0];s=c['steps'];a=s.index(dict(op='destroy',ref='artifact_token'));self.assertIn(dict(op='assert_matching_permanents',player=owner,index=self.rows['Ornithopter']['csvid'],property='token',value=1),s[:a]);self.assertIn(dict(op='assert_card',ref='source',property='power',value=5),s[:a]);self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),s[a:]);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Ornithopter']['csvid'],value=0),s[a:])

 def test_reuse3_keeps_self_foreign_and_defending_forest_controls(self):
  for owner in (0,1):
   s=self.cases('Knighthood',[f'effect_{owner}'])[0]['steps'];self.assertIn(dict(op='assert_card',ref='match_'+str(owner),property='damage',value=0),s)
   s=self.cases('Primal Rage',[f'effect_{owner}'])[0]['steps'];self.assertIn(dict(op='assert_life',player=1-owner,value=17),s)
   s=self.cases('Lumbering Satyr',[f'source_recipient_{owner}'])[0]['steps'];self.assertIn(dict(op='attack',ref='source',value=1),s);self.assertIn(dict(op='assert_life',player=1-owner,value=15),s)
   c=self.cases('Lumbering Satyr',[f'foreign_destroyed_{owner}'])[0];self.assertIn('opponent_owned',next(x['state'] for x in c['setup'] if x['ref']=='match_'+str(owner)));self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=1),c['steps'])
   s=self.cases('Lumbering Satyr',[f'no_walk_land_{owner}'])[0]['steps'];self.assertIn(dict(op='block',ref='blocker_'+str(1-owner),other='match_'+str(owner),value=1),s)

if __name__=='__main__':unittest.main()
